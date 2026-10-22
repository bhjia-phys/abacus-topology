#include "local_hse.h"

#include "source_hamilt/module_xc/exx_info_ri.h"
#include <RI/physics/LocalInteraction.h>

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <istream>
#include <limits>
#include <ostream>
#include <set>
#include <sstream>
#include <stdexcept>
#include <tuple>

namespace LocalHse
{
namespace
{
bool finite(const std::complex<double> value)
{
    return std::isfinite(value.real()) && std::isfinite(value.imag());
}

double checked_real(const std::complex<double> value)
{
    if (!finite(value) || std::abs(value.imag()) > 1e-9 * std::max(1.0, std::abs(value.real())))
    {
        throw std::invalid_argument("Local HSE U/J is non-finite or has a non-real component");
    }
    return value.real();
}
} // namespace

Projectors read_projectors(std::istream& input)
{
    std::ostringstream cleaned;
    std::string line;
    while (std::getline(input, line))
    {
        cleaned << line.substr(0, line.find('#')) << '\n';
    }
    if (input.bad())
    {
        throw std::runtime_error("Failed to read the local HSE projector stream");
    }
    std::istringstream tokens(cleaned.str());
    std::string label;
    int version = 0;
    long long n_orbitals = 0;
    long long n_entries = 0;
    if (!(tokens >> label >> version) || label != "ABACUS_LOCAL_HSE_PROJECTORS" || version != 1
        || !(tokens >> label >> n_orbitals) || label != "n_orbitals" || n_orbitals <= 0
        || n_orbitals > 1024 || !(tokens >> label >> n_entries) || label != "n_entries"
        || n_entries <= 0 || n_entries > 1000000)
    {
        throw std::invalid_argument("Invalid local HSE projector header/counts (expected version 1)");
    }
    Projectors result;
    result.n_orbitals = static_cast<std::size_t>(n_orbitals);
    std::set<std::tuple<std::size_t, int, std::array<int, 3>, int>> seen;
    std::vector<bool> has_support(result.n_orbitals, false);
    for (long long i = 0; i < n_entries; ++i)
    {
        long long orbital = -1;
        ProjectorEntry entry;
        double real = 0;
        double imag = 0;
        if (!(tokens >> orbital >> entry.atom >> entry.cell[0] >> entry.cell[1] >> entry.cell[2]
                     >> entry.ao >> real >> imag)
            || orbital < 0 || orbital >= n_orbitals || entry.atom < 0 || entry.ao < 0
            || !std::isfinite(real) || !std::isfinite(imag))
        {
            throw std::invalid_argument("Invalid local HSE projector entry " + std::to_string(i));
        }
        entry.orbital = static_cast<std::size_t>(orbital);
        entry.coefficient = {real, imag};
        if (!seen.insert(std::make_tuple(entry.orbital, entry.atom, entry.cell, entry.ao)).second)
        {
            throw std::invalid_argument("Duplicate local HSE projector coefficient");
        }
        has_support[entry.orbital] = has_support[entry.orbital] || std::abs(entry.coefficient) > 0;
        result.entries.push_back(entry);
    }
    if (tokens >> label)
    {
        throw std::invalid_argument("Unexpected data after local HSE projector entries");
    }
    for (const bool supported: has_support)
    {
        if (!supported)
        {
            throw std::invalid_argument("Every local HSE orbital must have nonzero support");
        }
    }
    return result;
}

double validate_overlap(const RI::Tensor<std::complex<double>>& gram, const double tolerance)
{
    if (!std::isfinite(tolerance) || tolerance <= 0 || gram.shape.size() != 2
        || gram.shape[0] == 0 || gram.shape[0] != gram.shape[1] || !gram.data
        || gram.data->size() != gram.shape[0] * gram.shape[1])
    {
        throw std::invalid_argument("Invalid local HSE orbital overlap matrix");
    }
    double error = 0;
    for (std::size_t m = 0; m < gram.shape[0]; ++m)
    {
        for (std::size_t n = 0; n < gram.shape[1]; ++n)
        {
            if (!finite(gram(m, n)))
            {
                throw std::invalid_argument("Non-finite local HSE orbital overlap");
            }
            error = std::max(error, std::abs(gram(m, n) - (m == n ? 1.0 : 0.0)));
        }
    }
    if (error > tolerance)
    {
        std::ostringstream message;
        message << "Local HSE final orbitals are not orthonormal: max |T^dagger S T - 1|="
                << error << " exceeds " << tolerance << "; no automatic orthogonalization is performed";
        throw std::invalid_argument(message.str());
    }
    return error;
}

Result summarize(const RI::Tensor<std::complex<double>>& integrals,
                 const double alpha,
                 const double overlap_error)
{
    if (!std::isfinite(alpha) || alpha < 0 || alpha > 1
        || !std::isfinite(overlap_error) || overlap_error < 0)
    {
        throw std::invalid_argument("Invalid local HSE fraction or overlap error");
    }
    const auto uj = RI::Local_Interaction::extract_direct_exchange(integrals);
    Result result;
    result.integrals = integrals;
    result.direct = uj.direct;
    result.exchange = uj.exchange;
    result.overlap_error = overlap_error;
    const std::size_t n = uj.direct.shape[0];
    result.has_averages = n > 1;
    result.ubar = 0;
    result.jbar = 0;
    result.c_hse = 0;
    for (std::size_t m = 0; m < n; ++m)
    {
        for (std::size_t p = 0; p < n; ++p)
        {
            const double direct = checked_real(uj.direct(m, p));
            const double exchange = checked_real(uj.exchange(m, p));
            if (result.has_averages)
            {
                result.ubar += direct / static_cast<double>(n * n);
                if (m != p)
                {
                    result.jbar += exchange / static_cast<double>(n * (n - 1));
                }
            }
        }
    }
    if (result.has_averages)
    {
        result.c_hse = alpha * (result.ubar - result.jbar);
    }
    return result;
}

void write_result(std::ostream& output,
                  const std::string& projector_path,
                  const Projectors& projectors,
                  const Result& result,
                  const double omega,
                  const double alpha,
                  const Exx_Info_RI& info)
{
    output << std::scientific << std::setprecision(17)
           << "ABACUS_LOCAL_HSE 1\n"
           << "# Final finite-support spatial orbitals; no occupied-density or Hamiltonian update.\n"
           << "# projectors_file " << projector_path << '\n'
           << "energy_unit Hartree\nlength_unit bohr\nkernel erfc\nmetric_alpha 1\n"
           << "ri_fitting_kernel erfc\n"
           << "omega " << omega << "\nhybrid_alpha " << alpha << '\n'
           << "n_orbitals " << projectors.n_orbitals << '\n'
           << "overlap_max_error " << result.overlap_error << '\n'
           << "ri_pca_threshold " << info.pca_threshold << '\n'
           << "ri_inverse_threshold " << info.Cs_inv_thr << '\n'
           << "ri_ccp_rmesh_times " << info.ccp_rmesh_times << '\n'
           << "ri_kmesh_times " << info.kmesh_times << '\n'
           << "# Unwrapped real-space CV blocks; no periodic image sum or BvK folding.\n";
    for (const auto& path: info.files_abfs)
    {
        output << "# auxiliary_file " << path << '\n';
    }
    if (result.has_averages)
    {
        output << "averages defined\nUbar_SR " << result.ubar << "\nJbar_SR " << result.jbar
               << "\nC_HSE " << result.c_hse << '\n';
    }
    else
    {
        output << "averages undefined_for_one_orbital\n"
               << "# Off-diagonal exchange average and C_HSE are not defined for M=1.\n";
    }
    const std::size_t n = projectors.n_orbitals;
    output << "# I_SR: m n p q real imag; integral w_m* w_n v_SR w_p* w_q\nI_SR\n";
    for (std::size_t m = 0; m < n; ++m)
        for (std::size_t k = 0; k < n; ++k)
            for (std::size_t p = 0; p < n; ++p)
                for (std::size_t q = 0; q < n; ++q)
                {
                    const auto value = result.integrals(m, k, p, q);
                    output << m << ' ' << k << ' ' << p << ' ' << q << ' '
                           << value.real() << ' ' << value.imag() << '\n';
                }
    output << "# U_SR=I_mm,nn; J_SR=I_mn,nm; m n Ureal Uimag Jreal Jimag\nU_J_SR\n";
    for (std::size_t m = 0; m < n; ++m)
        for (std::size_t p = 0; p < n; ++p)
        {
            const auto u = result.direct(m, p);
            const auto j = result.exchange(m, p);
            output << m << ' ' << p << ' ' << u.real() << ' ' << u.imag() << ' '
                   << j.real() << ' ' << j.imag() << '\n';
        }
    output << "# Exact supplied coefficients: orbital atom Rx Ry Rz local_ao real imag\n"
           << "n_entries " << projectors.entries.size() << "\nPROJECTORS\n";
    for (const auto& entry: projectors.entries)
    {
        output << entry.orbital << ' ' << entry.atom << ' ' << entry.cell[0] << ' '
               << entry.cell[1] << ' ' << entry.cell[2] << ' ' << entry.ao << ' '
               << entry.coefficient.real() << ' ' << entry.coefficient.imag() << '\n';
    }
    if (!output)
    {
        throw std::runtime_error("Failed to write local HSE result");
    }
}
} // namespace LocalHse
