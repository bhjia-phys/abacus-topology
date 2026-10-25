#include "hartree_reference.h"

#include "source_base/constants.h"
#include "source_base/math_integral.h"
#include "source_base/math_sphbes.h"
#include "source_base/matrix.h"
#include "source_base/ylm.h"
#include "source_basis/module_ao/ORB_atomic_lm.h"
#include "source_basis/module_pw/pw_basis.h"
#include "source_cell/unitcell.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <limits>
#include <stdexcept>

namespace HartreeReference
{
double radial_fourier(const Numerical_Orbital_Lm& orbital, const double gnorm)
{
    std::vector<double> integrand(orbital.getNr());
    ModuleBase::Sphbes::Spherical_Bessel(orbital.getNr(), orbital.get_r_radial().data(),
                                       gnorm, orbital.getL(), integrand.data());
    for (int ir = 0; ir < orbital.getNr(); ++ir)
    {
        const double radius = orbital.getRadial(ir);
        integrand[ir] *= orbital.getPsi(ir) * radius * radius;
    }
    double value = 0.0;
    ModuleBase::Integral::Simpson_Integral(orbital.getNr(), integrand.data(), orbital.getRab(), value);
    return ModuleBase::FOUR_PI * value;
}

ModuleBase::matrix coulomb_q0(
    const UnitCell& cell,
    const ModulePW::PW_Basis& basis,
    const std::vector<std::vector<std::vector<Numerical_Orbital_Lm>>>& abfs,
    const ModuleBase::Element_Basis_Index::IndexLNM& index)
{
    std::vector<int> offsets(cell.nat + 1, 0);
    int lmax = 0;
    for (int atom = 0; atom < cell.nat; ++atom)
    {
        const int type = cell.iat2it[atom];
        offsets[atom + 1] = offsets[atom] + index.at(type).count_size;
        lmax = std::max(lmax, static_cast<int>(abfs.at(type).size()) - 1);
    }
    const int naux = offsets.back();
    if (naux <= 0 || static_cast<long long>(naux) * naux > std::numeric_limits<int>::max())
    {
        throw std::invalid_argument("Hartree reference auxiliary matrix size is invalid");
    }
    ModuleBase::matrix metric(naux, naux);
    std::vector<std::complex<double>> fourier(naux);
    std::vector<double> harmonics((lmax + 1) * (lmax + 1));
    for (int ig = 0; ig < basis.npw; ++ig)
    {
        const double g2 = cell.tpiba2 * basis.gg[ig];
        if (ig == basis.ig_gge0 || g2 < 1e-20)
        {
            continue;
        }
        const double gnorm = std::sqrt(g2);
        ModuleBase::Ylm::get_ylm_real(lmax + 1, basis.gcar[ig], harmonics.data());
        std::vector<std::vector<std::vector<double>>> radial(abfs.size());
        for (std::size_t type = 0; type < abfs.size(); ++type)
        {
            radial[type].resize(abfs[type].size());
            for (std::size_t l = 0; l < abfs[type].size(); ++l)
            {
                for (const auto& orbital : abfs[type][l])
                {
                    radial[type][l].push_back(radial_fourier(orbital, gnorm));
                }
            }
        }
        for (int atom = 0; atom < cell.nat; ++atom)
        {
            const int type = cell.iat2it[atom];
            const auto tau = cell.atoms[type].tau[cell.iat2ia[atom]];
            const double argument = -ModuleBase::TWO_PI * (basis.gcar[ig] * tau);
            const std::complex<double> atom_phase(std::cos(argument), std::sin(argument));
            for (std::size_t l = 0; l < abfs[type].size(); ++l)
            {
                const auto angular_phase = std::pow(std::complex<double>(0, -1), static_cast<int>(l));
                for (std::size_t n = 0; n < abfs[type][l].size(); ++n)
                {
                    for (std::size_t m = 0; m < 2 * l + 1; ++m)
                    {
                        // Full Fourier transform: 4*pi*(-i)^l Y_lm(Ghat)
                        // integral r^2 dr R_l(r) j_l(Gr). Equivalently,
                        // (2*pi)^(3/2)*(-i)^l*psif(G)*Y_lm(Ghat).
                        fourier[offsets[atom] + index[type][l][n][m]]
                            = radial[type][l][n] * harmonics[l * l + m]
                              * angular_phase * atom_phase;
                    }
                }
            }
        }
        // A gamma-only FFT stores one of G,-G off its unreduced plane.
        // The omitted partner is the complex conjugate, so add twice the
        // real part; G on the unreduced plane already has both partners.
        const double reduced_component = basis.xprime ? basis.gdirect[ig].x : basis.gdirect[ig].y;
        const double multiplicity = basis.gamma_only && std::abs(reduced_component) > 1e-12 ? 2.0 : 1.0;
        const double factor = multiplicity * ModuleBase::FOUR_PI / (cell.omega * g2);
        for (int i = 0; i < naux; ++i)
        {
            for (int j = 0; j < naux; ++j)
            {
                metric(i, j) += factor * std::real(std::conj(fourier[i]) * fourier[j]);
            }
        }
    }
#ifdef __MPI
    if (basis.pool_world != MPI_COMM_NULL)
    {
        MPI_Allreduce(MPI_IN_PLACE, metric.c, naux * naux, MPI_DOUBLE, MPI_SUM, basis.pool_world);
    }
#endif
    return metric;
}
}
