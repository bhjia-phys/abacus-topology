#ifndef ABACUS_LOCAL_HSE_H
#define ABACUS_LOCAL_HSE_H

#include <RI/global/Tensor.h>

#include <array>
#include <complex>
#include <cstddef>
#include <iosfwd>
#include <string>
#include <vector>
#include <mpi.h>

class UnitCell;
class K_Vectors;
class LCAO_Orbitals;
class TwoCenterIntegrator;
struct Exx_Info_RI;

namespace LocalHse
{

// All indices are zero based. The cell is an unwrapped physical translation.
struct ProjectorEntry
{
    std::size_t orbital;
    int atom;
    std::array<int, 3> cell;
    int ao;
    std::complex<double> coefficient;
};

struct Projectors
{
    std::size_t n_orbitals;
    std::vector<ProjectorEntry> entries;
};

struct Result
{
    RI::Tensor<std::complex<double>> integrals;
    RI::Tensor<std::complex<double>> direct;
    RI::Tensor<std::complex<double>> exchange;
    bool has_averages;
    double ubar;
    double jbar;
    double c_hse;
    double overlap_error;
};

Projectors read_projectors(std::istream& input);
double validate_overlap(const RI::Tensor<std::complex<double>>& gram, double tolerance);
Result summarize(const RI::Tensor<std::complex<double>>& integrals,
                 double alpha,
                 double overlap_error);
void write_result(std::ostream& output,
                  const std::string& projector_path,
                  const Projectors& projectors,
                  const Result& result,
                  double omega,
                  double alpha,
                  const Exx_Info_RI& info);

// Replicated local calculation; only communicator rank zero writes the result.
// No occupied density, SCF Hamiltonian, or global XC setting is modified.
void run(MPI_Comm comm,
         const UnitCell& ucell,
         const K_Vectors& kv,
         const LCAO_Orbitals& orbitals,
         const TwoCenterIntegrator& overlap,
         const Exx_Info_RI& base_info,
         const std::string& orbital_directory,
         const std::string& projector_path,
         const std::string& output_path,
         double omega,
         double alpha);

} // namespace LocalHse

#endif
