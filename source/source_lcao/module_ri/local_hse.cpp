#include "local_hse.h"

#include "Exx_LRI.h"
#include "source_basis/module_nao/two_center_integrator.h"
#include "source_cell/unitcell.h"
#include <RI/physics/LocalInteraction.h>

#include <climits>
#include <cmath>
#include <fstream>
#include <functional>
#include <memory>
#include <set>
#include <sstream>
#include <stdexcept>

namespace LocalHse
{
namespace
{
using Cell = std::array<int, 3>;
using AtomCell = std::pair<int, Cell>;
using OrbitalMap = std::map<int, std::map<Cell, RI::Tensor<std::complex<double>>>>;
using BlockMap = std::map<int, std::map<AtomCell, RI::Tensor<double>>>;

void broadcast_string(MPI_Comm comm, const int root, std::string& message)
{
    int rank = 0;
    MPI_Comm_rank(comm, &rank);
    int length = rank == root ? static_cast<int>(message.size()) : 0;
    MPI_Bcast(&length, 1, MPI_INT, root, comm);
    message.resize(length);
    if (length)
    {
        MPI_Bcast(&message[0], length, MPI_CHAR, root, comm);
    }
}

void collective_stage(MPI_Comm comm, const std::string& name, const std::function<void()>& action)
{
    int rank = 0;
    MPI_Comm_rank(comm, &rank);
    std::string error;
    try
    {
        action();
    }
    catch (const std::exception& exception)
    {
        error = exception.what();
    }
    catch (...)
    {
        error = "unknown exception";
    }
    const int source = error.empty() ? INT_MAX : rank;
    int first_source = INT_MAX;
    MPI_Allreduce(&source, &first_source, 1, MPI_INT, MPI_MIN, comm);
    if (first_source != INT_MAX)
    {
        broadcast_string(comm, first_source, error);
        throw std::runtime_error("Local HSE " + name + ": " + error);
    }
}

Cell difference(const Cell& right, const Cell& left)
{
    Cell result;
    for (std::size_t d = 0; d < 3; ++d)
    {
        const long long value = static_cast<long long>(right[d]) - left[d];
        if (value < INT_MIN || value > INT_MAX)
        {
            throw std::invalid_argument("Local HSE support cell difference overflows an integer");
        }
        result[d] = static_cast<int>(value);
    }
    return result;
}

OrbitalMap materialize(const Projectors& projectors, const UnitCell& ucell)
{
    OrbitalMap result;
    for (const auto& entry: projectors.entries)
    {
        if (entry.atom >= ucell.nat)
        {
            throw std::invalid_argument("Local HSE projector atom is outside STRU");
        }
        const int n_ao = ucell.atoms[ucell.iat2it[entry.atom]].nw;
        if (entry.ao >= n_ao)
        {
            throw std::invalid_argument("Local HSE projector spatial AO is outside its atom basis");
        }
        auto& block = result[entry.atom][entry.cell];
        if (block.empty())
        {
            block = RI::Tensor<std::complex<double>>({static_cast<std::size_t>(n_ao),
                                                     projectors.n_orbitals});
        }
        block(entry.ao, entry.orbital) = entry.coefficient;
    }
    return result;
}

double check_overlap(const Projectors& projectors,
                     const UnitCell& ucell,
                     const TwoCenterIntegrator& overlap)
{
    RI::Tensor<std::complex<double>> gram({projectors.n_orbitals, projectors.n_orbitals});
    for (const auto& left: projectors.entries)
    {
        const int it = ucell.iat2it[left.atom];
        const auto& atom_left = ucell.atoms[it];
        const int l_left = atom_left.iw2l[left.ao];
        const int n_left = atom_left.iw2n[left.ao];
        const int m_left = atom_left.iw2m[left.ao];
        const int signed_left = m_left % 2 == 0 ? -m_left / 2 : (m_left + 1) / 2;
        for (const auto& right: projectors.entries)
        {
            const int jt = ucell.iat2it[right.atom];
            const auto& atom_right = ucell.atoms[jt];
            const int m_right = atom_right.iw2m[right.ao];
            const int signed_right = m_right % 2 == 0 ? -m_right / 2 : (m_right + 1) / 2;
            const auto delta = difference(right.cell, left.cell);
            const ModuleBase::Vector3<double> translation(delta[0], delta[1], delta[2]);
            const auto displacement = (atom_right.tau[ucell.iat2ia[right.atom]]
                                       - atom_left.tau[ucell.iat2ia[left.atom]]
                                       + translation * ucell.latvec) * ucell.lat0;
            double value = 0;
            overlap.calculate(it, l_left, n_left, signed_left,
                              jt, atom_right.iw2l[right.ao], atom_right.iw2n[right.ao], signed_right,
                              displacement, &value);
            gram(left.orbital, right.orbital)
                += std::conj(left.coefficient) * right.coefficient * value;
        }
    }
    return validate_overlap(gram, 1e-6);
}

std::map<int, std::set<AtomCell>> requested_blocks(const OrbitalMap& orbitals)
{
    std::map<int, std::set<AtomCell>> result;
    for (const auto& left_atom: orbitals)
        for (const auto& left: left_atom.second)
            for (const auto& right_atom: orbitals)
                for (const auto& right: right_atom.second)
                {
                    result[left_atom.first].insert(
                        {right_atom.first, difference(right.first, left.first)});
                }
    return result;
}

Exx_Info_RI short_range_info(const Exx_Info_RI& base,
                            const std::string& orbital_directory,
                            const double omega)
{
    Exx_Info_RI info(base);
    info.coulomb_param.clear();
    std::ostringstream value;
    value.precision(17);
    value << omega;
    info.coulomb_param[Conv_Coulomb_Pot_K::Coulomb_Type::Erfc]
        = {{{"alpha", "1"}, {"omega", value.str()}, {"singularity_correction", "limits"}}};
    info.real_number = true;
    info.coul_moment = false;
    info.rotate_abfs = false;
    for (auto& path: info.files_abfs)
    {
        if (!path.empty() && path[0] != '/' && !orbital_directory.empty())
        {
            path = orbital_directory + (orbital_directory.back() == '/' ? "" : "/") + path;
        }
    }
    return info;
}
} // namespace

void run(MPI_Comm comm,
         const UnitCell& ucell,
         const K_Vectors& kv,
         const LCAO_Orbitals& orbitals,
         const TwoCenterIntegrator& overlap,
         const Exx_Info_RI& base_info,
         const std::string& orbital_directory,
         const std::string& projector_path,
         const std::string& output_path,
         const double omega,
         const double alpha)
{
    int rank = 0;
    MPI_Comm_rank(comm, &rank);
    std::string text;
    collective_stage(comm, "projector read", [&]()
    {
        if (!std::isfinite(omega) || omega <= 0 || !std::isfinite(alpha) || alpha < 0 || alpha > 1)
        {
            throw std::invalid_argument("omega must be positive and alpha must be in [0,1]");
        }
        if (rank == 0)
        {
            std::ifstream input(projector_path.c_str());
            if (!input)
            {
                throw std::runtime_error("Cannot open projector file " + projector_path);
            }
            std::ostringstream buffer;
            buffer << input.rdbuf();
            if (input.bad() || buffer.str().size() > static_cast<std::size_t>(INT_MAX))
            {
                throw std::runtime_error("Cannot read or broadcast local HSE projector file");
            }
            text = buffer.str();
        }
    });
    broadcast_string(comm, 0, text);
    Projectors projectors;
    OrbitalMap localized;
    double overlap_error = 0;
    collective_stage(comm, "projector validation", [&]()
    {
        std::istringstream input(text);
        projectors = read_projectors(input);
        localized = materialize(projectors, ucell);
        overlap_error = check_overlap(projectors, ucell, overlap);
    });

    Exx_Info_RI info = short_range_info(base_info, orbital_directory, omega);
    std::unique_ptr<Exx_LRI<double>> engine;
    collective_stage(comm, "SR integral initialization", [&]()
    {
        engine.reset(new Exx_LRI<double>(info));
        engine->init(comm, ucell, kv, orbitals);
    });
    Result result;
    collective_stage(comm, "finite-support contraction", [&]()
    {
        BlockMap coefficients;
        BlockMap metric;
        engine->cal_local_ri_blocks(ucell, requested_blocks(localized), coefficients, metric);
        const auto pairs = RI::Local_Interaction::build_lri_pair_vertices(
            coefficients, localized, projectors.n_orbitals);
        const auto tensor = RI::Local_Interaction::contract_real_space_pair_vertices(
            pairs, metric, projectors.n_orbitals);
        // CV uses the 4*pi/k^2 Coulomb kernel in atomic units. The -2
        // exchange/Ry conversion in post_process_Hexx is deliberately absent.
        result = summarize(tensor, alpha, overlap_error);
    });
    collective_stage(comm, "output", [&]()
    {
        if (rank == 0)
        {
            std::ofstream output(output_path.c_str());
            if (!output)
            {
                throw std::runtime_error("Cannot open result file " + output_path);
            }
            write_result(output, projector_path, projectors, result, omega, alpha, info);
            output.close();
            if (!output)
            {
                throw std::runtime_error("Cannot finish result file " + output_path);
            }
        }
    });
}
} // namespace LocalHse
