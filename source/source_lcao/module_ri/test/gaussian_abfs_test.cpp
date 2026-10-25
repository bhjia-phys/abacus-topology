// Focused unit tests for Gaussian_Abfs::init, Gaussian_Abfs::get_lattice_sum, and Gaussian_Abfs::get_Vq.
//
// The quantity under test is the 3D reciprocal-space Gaussian lattice sum
//
//   S_lm(q) = sum_G k^(power+L) * exp(-exponent * k^2) * Y_lm(k_hat)
//             * exp(-2 pi i (q+G) . tau),   k = |q + G| * tpiba,
//
// where q, G and tau are in crystal (reciprocal-lattice) units and the phase
// convention follows the implementation in gaussian_abfs.cpp, which stores
// vec = -(q + G) and uses phase = exp(2 pi i vec . tau).  These tests lock
// that convention, the shell truncation selected by Gaussian_Abfs::init, the
// exact q -> -q conjugation symmetry and the exclude_Gamma bookkeeping,
// against an independent direct-sum oracle that never calls the code under
// test.
//
// Scope and limitations: independent explicit harmonic oracles cover the L = 0
// channel (Y00 = 1/sqrt(4 pi)) and the selected L = 6, m = 0 channel (Y60).
// Other L >= 1 components are covered by the parity relation
// S_lm(-q) = (-1)^L conj(S_lm(q)), which is exact for real spherical harmonics
// on a centrosymmetric G shell and therefore needs no explicit Y_lm oracle. The
// 2D and derivative variants (get_lattice_sum_2d and get_d_lattice_sum) remain
// out of scope; get_Vq is covered below for the l = 0 by l = 0 and selected
// l = 0 by l = 6 channels.

#include <gtest/gtest.h>

#include "source_base/constants.h"
#include "source_base/vector3.h"
#include "source_cell/unitcell.h"
#include "source_lcao/module_ri/gaussian_abfs.h"

#include <cmath>
#include <complex>
#include <vector>

namespace
{

// Simple cubic cell: lattice constant a = 4 Bohr, reciprocal lattice vectors
// e_i (in units of 2 pi / a), so G is the 3x3 identity.
const double kLat0 = 4.0;
const double kTpiba = ModuleBase::TWO_PI / kLat0;
const double kLambda = 1.0; // Bohr^2; exponent = 1/lambda matches get_Vq usage.
const double kExponent = 1.0 / kLambda;
const ModuleBase::Vector3<double> kQ0(0.1, 0.2, 0.3);      // non-Gamma, crystal units
const ModuleBase::Vector3<double> kTau(0.25, 0.35, 0.45);  // nonzero, crystal units
const int kLmaxInit = 2; // must be >= the largest lmax passed to get_lattice_sum

void expect_complex_near(const std::complex<double>& actual,
                         const std::complex<double>& expected,
                         const double tol)
{
    EXPECT_NEAR(actual.real(), expected.real(), tol);
    EXPECT_NEAR(actual.imag(), expected.imag(), tol);
}

// Independent direct-sum oracle for the L = 0 channel of get_lattice_sum.
// It re-implements the mathematical definition from first principles (its own
// loop, its own phase convention, the standard Y00 = 1/sqrt(4 pi)) and never
// calls the code under test.  exclude_g0 drops the G = (0,0,0) term, matching
// the exclude_Gamma flag of the production function.
std::complex<double> oracle_l0(const ModuleBase::Vector3<double>& q,
                               const double power,
                               const double exponent,
                               const bool exclude_g0,
                               const ModuleBase::Vector3<double>& tau,
                               const int n_shell)
{
    const double y00 = 1.0 / std::sqrt(ModuleBase::FOUR_PI);
    std::complex<double> sum(0.0, 0.0);
    for (int i = -n_shell; i <= n_shell; ++i)
    {
        for (int j = -n_shell; j <= n_shell; ++j)
        {
            for (int k = -n_shell; k <= n_shell; ++k)
            {
                if (exclude_g0 && i == 0 && j == 0 && k == 0)
                {
                    continue;
                }
                const ModuleBase::Vector3<double> qg(q.x + i, q.y + j, q.z + k);
                const double ksq = qg.norm2() * kTpiba * kTpiba;
                const double kabs = std::sqrt(ksq);
                const double radial = std::exp(-exponent * ksq) * std::pow(kabs, power);
                const std::complex<double> phase
                    = std::exp(-ModuleBase::TWO_PI * ModuleBase::IMAG_UNIT * (qg * tau));
                sum += radial * y00 * phase;
            }
        }
    }
    return sum;
}

// Independent direct-sum oracle for the l = 0 by l = 0 channel of get_Vq.
// The non-Gamma q makes every k_G nonzero, so no singularity correction is
// needed in this direct evaluation.
std::complex<double> oracle_v00(const ModuleBase::Vector3<double>& q,
                                const ModuleBase::Vector3<double>& tau,
                                const UnitCell& ucell,
                                const int n_shell)
{
    const double y00 = 1.0 / std::sqrt(ModuleBase::FOUR_PI);
    std::complex<double> sum(0.0, 0.0);
    for (int i = -n_shell; i <= n_shell; ++i)
    {
        for (int j = -n_shell; j <= n_shell; ++j)
        {
            for (int k = -n_shell; k <= n_shell; ++k)
            {
                const ModuleBase::Vector3<double> qg(q.x + i, q.y + j, q.z + k);
                const double k_g = kTpiba * std::sqrt(qg.norm2());
                const double radial = std::exp(-k_g * k_g / kLambda) / (k_g * k_g);
                const std::complex<double> phase
                    = std::exp(-ModuleBase::TWO_PI * ModuleBase::IMAG_UNIT * (qg * tau));
                sum += radial * phase;
            }
        }
    }
    return 8.0 * std::pow(ModuleBase::TWO_PI, 3) / ucell.omega * y00 * y00 * sum;
}

// Explicit real spherical harmonic used only by the high-l direct-sum oracle.
double y60(const double u)
{
    const double u2 = u * u;
    return std::sqrt(13.0 / ModuleBase::FOUR_PI) / 16.0
           * (231.0 * u2 * u2 * u2 - 315.0 * u2 * u2 + 105.0 * u2 - 5.0);
}

// Independent direct-sum oracle for the l = 0 by l = 6, L = 6, m = 0 channel.
// Its radial factor includes both the Coulomb k^-2 and G_LM's k^L factor;
// for L = 6 this is exp(-k^2/lambda) * k^6 / k^2. The non-Gamma q makes
// every k_G nonzero, so no singularity correction is needed in this direct
// evaluation.
std::complex<double> oracle_v06(const ModuleBase::Vector3<double>& q,
                                const ModuleBase::Vector3<double>& tau,
                                const UnitCell& ucell)
{
    std::complex<double> sum(0.0, 0.0);
    for (int i = -6; i <= 6; ++i)
    {
        for (int j = -6; j <= 6; ++j)
        {
            for (int k = -6; k <= 6; ++k)
            {
                const ModuleBase::Vector3<double> qg(q.x + i, q.y + j, q.z + k);
                const double ksq = qg.norm2() * kTpiba * kTpiba;
                const double k_g = std::sqrt(ksq);
                const double u = qg.z / std::sqrt(qg.norm2());
                const double radial
                    = std::exp(-k_g * k_g / kLambda) * std::pow(k_g, 6) / (k_g * k_g);
                const std::complex<double> phase
                    = std::exp(-ModuleBase::TWO_PI * ModuleBase::IMAG_UNIT * (qg * tau));
                sum += radial * y60(u) * phase;
            }
        }
    }

    const double n0 = std::sqrt(ModuleBase::PI * 0.5);
    const double n6 = 11.0 * 9.0 * 7.0 * 5.0 * 3.0 * n0;
    const double gaunt_006 = 1.0 / std::sqrt(ModuleBase::FOUR_PI);
    const double prefactor = 4.0 * ModuleBase::PI * (-1.0) * std::pow(ModuleBase::TWO_PI, 3)
                             / (ucell.omega * n0 * n6) * gaunt_006;
    return prefactor * sum;
}

// Independent Gamma-point direct-sum oracle for the l = 0 by l = 0 channel.
// It omits only G = (0,0,0) from the explicit reciprocal-cell loop and includes
// the analytic chi - 1/lambda term required by the Gamma-point correction.
std::complex<double> oracle_v00_gamma(const double chi,
                                      const ModuleBase::Vector3<double>& tau,
                                      const UnitCell& ucell)
{
    const double y00 = 1.0 / std::sqrt(ModuleBase::FOUR_PI);
    std::complex<double> sum(0.0, 0.0);
    for (int i = -6; i <= 6; ++i)
    {
        for (int j = -6; j <= 6; ++j)
        {
            for (int k = -6; k <= 6; ++k)
            {
                if (i == 0 && j == 0 && k == 0)
                {
                    continue;
                }
                const ModuleBase::Vector3<double> G(static_cast<double>(i),
                                                    static_cast<double>(j),
                                                    static_cast<double>(k));
                const double k_g = kTpiba * std::sqrt(G.norm2());
                const double radial = std::exp(-k_g * k_g / kLambda) / (k_g * k_g);
                const std::complex<double> phase
                    = std::exp(-ModuleBase::TWO_PI * ModuleBase::IMAG_UNIT * (G * tau));
                sum += radial * phase;
            }
        }
    }
    return 8.0 * std::pow(ModuleBase::TWO_PI, 3) / ucell.omega * y00 * y00
           * (sum + chi - 1.0 / kLambda);
}

class GaussianAbfsLatticeSumTest : public ::testing::Test
{
  protected:
    GaussianAbfsLatticeSumTest()
    {
        ucell.lat0 = kLat0;
        ucell.tpiba = kTpiba;
        ucell.omega = kLat0 * kLat0 * kLat0;
        ucell.a1 = ModuleBase::Vector3<double>(1.0, 0.0, 0.0);
        ucell.a2 = ModuleBase::Vector3<double>(0.0, 1.0, 0.0);
        ucell.a3 = ModuleBase::Vector3<double>(0.0, 0.0, 1.0);
        G.Identity(); // default ctor already yields the identity; kept explicit
    }

    UnitCell ucell;
    ModuleBase::Matrix3 G;
};

TEST_F(GaussianAbfsLatticeSumTest, L0MatchesDirectSumOracle)
{
    Gaussian_Abfs abfs;
    abfs.init(ucell, kLmaxInit, std::vector<ModuleBase::Vector3<double>>{kQ0}, G, kLambda);

    for (const double power : {0.0, -2.0})
    {
        const std::vector<std::complex<double>> result
            = abfs.get_lattice_sum(kTpiba, 0, power, kExponent, false, 0, kTau);
        ASSERT_EQ(result.size(), 1u);
        const std::complex<double> ref = oracle_l0(kQ0, power, kExponent, false, kTau, 6);
        expect_complex_near(result[0], ref, 2e-11 * (1.0 + std::abs(ref)));
    }

    // The L = 0 channel must not depend on the caller's lmax truncation.
    const std::vector<std::complex<double>> ref_l0
        = abfs.get_lattice_sum(kTpiba, 0, 0.0, kExponent, false, 0, kTau);
    for (const int lmax : {1, 2})
    {
        const std::vector<std::complex<double>> bigger
            = abfs.get_lattice_sum(kTpiba, 0, 0.0, kExponent, false, lmax, kTau);
        ASSERT_EQ(bigger.size(), static_cast<size_t>((lmax + 1) * (lmax + 1)));
        expect_complex_near(bigger[0], ref_l0[0], 1e-14 * (1.0 + std::abs(ref_l0[0])));
    }
}

TEST_F(GaussianAbfsLatticeSumTest, ShellTruncationConverged)
{
    Gaussian_Abfs abfs;
    abfs.init(ucell, kLmaxInit, std::vector<ModuleBase::Vector3<double>>{kQ0}, G, kLambda);
    const std::vector<std::complex<double>> result
        = abfs.get_lattice_sum(kTpiba, 0, 0.0, kExponent, false, 0, kTau);
    ASSERT_EQ(result.size(), 1u);

    // The oracle itself must be converged well inside the production shell.
    const std::complex<double> ref_inner = oracle_l0(kQ0, 0.0, kExponent, false, kTau, 4);
    const std::complex<double> ref_outer = oracle_l0(kQ0, 0.0, kExponent, false, kTau, 6);
    expect_complex_near(ref_inner, ref_outer, 1e-12 * (1.0 + std::abs(ref_outer)));

    // init's eta-based shell truncation must reproduce the converged sum.
    expect_complex_near(result[0], ref_outer, 2e-11 * (1.0 + std::abs(ref_outer)));

    // Sanity check: the result is a true lattice sum, not just the G = 0 term.
    const std::complex<double> ref_g0_only = oracle_l0(kQ0, 0.0, kExponent, false, kTau, 0);
    EXPECT_GT(std::abs(result[0] - ref_g0_only), 1e-4);
}

TEST_F(GaussianAbfsLatticeSumTest, QReversalConjugation)
{
    Gaussian_Abfs abfs;
    abfs.init(ucell,
              kLmaxInit,
              std::vector<ModuleBase::Vector3<double>>{
                  kQ0,
                  ModuleBase::Vector3<double>(-kQ0.x, -kQ0.y, -kQ0.z)},
              G,
              kLambda);

    for (const int lmax : {0, 1, 2})
    {
        const std::vector<std::complex<double>> s_plus
            = abfs.get_lattice_sum(kTpiba, 0, 0.0, kExponent, false, lmax, kTau);
        const std::vector<std::complex<double>> s_minus
            = abfs.get_lattice_sum(kTpiba, 1, 0.0, kExponent, false, lmax, kTau);
        ASSERT_EQ(s_plus.size(), static_cast<size_t>((lmax + 1) * (lmax + 1)));
        ASSERT_EQ(s_minus.size(), s_plus.size());
        for (size_t lm = 0; lm < s_plus.size(); ++lm)
        {
            const int L = static_cast<int>(std::floor(std::sqrt(static_cast<double>(lm))));
            const std::complex<double> expected = (L % 2 == 0 ? 1.0 : -1.0) * std::conj(s_plus[lm]);
            expect_complex_near(s_minus[lm], expected, 2e-11 * (1.0 + std::abs(s_plus[lm])));
        }
    }
}

TEST_F(GaussianAbfsLatticeSumTest, ExcludeGammaSkipsGZeroTerm)
{
    Gaussian_Abfs abfs;
    abfs.init(ucell, kLmaxInit, std::vector<ModuleBase::Vector3<double>>{kQ0}, G, kLambda);

    // For a non-Gamma q the exclude_Gamma flag must still drop the single
    // G = (0,0,0) cell from the sum.
    for (const double power : {0.0, -2.0})
    {
        const std::vector<std::complex<double>> result
            = abfs.get_lattice_sum(kTpiba, 0, power, kExponent, true, 0, kTau);
        ASSERT_EQ(result.size(), 1u);
        const std::complex<double> ref = oracle_l0(kQ0, power, kExponent, true, kTau, 6);
        expect_complex_near(result[0], ref, 2e-11 * (1.0 + std::abs(ref)));
    }
}

TEST_F(GaussianAbfsLatticeSumTest, VqL0MatchesIndependentOracleAndIgnoresChi)
{
    Gaussian_Abfs abfs;
    abfs.init(ucell, kLmaxInit, std::vector<ModuleBase::Vector3<double>>{kQ0}, G, kLambda);

    const double y00 = 1.0 / std::sqrt(ModuleBase::FOUR_PI);
    ModuleBase::realArray gaunt(1, 1, 1);
    gaunt(0, 0, 0) = y00;

    const RI::Tensor<std::complex<double>> vq_first
        = abfs.get_Vq(0, 0, 0, -37.0, kTau, gaunt);
    ASSERT_EQ(vq_first.shape.size(), 2u);
    ASSERT_EQ(vq_first.shape[0], 1u);
    ASSERT_EQ(vq_first.shape[1], 1u);

    const RI::Tensor<std::complex<double>> vq_second
        = abfs.get_Vq(0, 0, 0, 91.0, kTau, gaunt);
    ASSERT_EQ(vq_second.shape.size(), 2u);
    ASSERT_EQ(vq_second.shape[0], 1u);
    ASSERT_EQ(vq_second.shape[1], 1u);

    const std::complex<double> expected = oracle_v00(kQ0, kTau, ucell, 6);
    const double tolerance = 2e-11 * (1.0 + std::abs(expected));
    expect_complex_near(vq_first(0, 0), expected, tolerance);
    expect_complex_near(vq_second(0, 0), expected, tolerance);
    expect_complex_near(vq_first(0, 0), vq_second(0, 0), tolerance);
}

TEST_F(GaussianAbfsLatticeSumTest, VqL0L6MatchesIndependentOracleAndIgnoresChi)
{
    Gaussian_Abfs abfs;
    abfs.init(ucell, 6, std::vector<ModuleBase::Vector3<double>>{kQ0}, G, kLambda);

    ModuleBase::realArray gaunt(49, 49, 49);
    gaunt = 0.0;
    gaunt(0, 36, 36) = 1.0 / std::sqrt(ModuleBase::FOUR_PI);

    const double chi_a = -37.0;
    const double chi_b = 91.0;
    const RI::Tensor<std::complex<double>> vq_a
        = abfs.get_Vq(0, 6, 0, chi_a, kTau, gaunt);
    const RI::Tensor<std::complex<double>> vq_b
        = abfs.get_Vq(0, 6, 0, chi_b, kTau, gaunt);
    ASSERT_EQ(vq_a.shape.size(), 2u);
    ASSERT_EQ(vq_a.shape[0], 1u);
    ASSERT_EQ(vq_a.shape[1], 49u);
    ASSERT_EQ(vq_b.shape.size(), 2u);
    ASSERT_EQ(vq_b.shape[0], 1u);
    ASSERT_EQ(vq_b.shape[1], 49u);

    const std::complex<double> expected = oracle_v06(kQ0, kTau, ucell);
    const double tolerance = 2e-11 * (1.0 + std::abs(expected));
    expect_complex_near(vq_a(0, 36), expected, tolerance);
    expect_complex_near(vq_b(0, 36), expected, tolerance);
    expect_complex_near(vq_a(0, 36), vq_b(0, 36), tolerance);

    expect_complex_near(vq_a(0, 0), std::complex<double>(0.0, 0.0), 1e-14);
    expect_complex_near(vq_a(0, 37), std::complex<double>(0.0, 0.0), 1e-14);
}

TEST_F(GaussianAbfsLatticeSumTest, GammaVqL0MatchesIndependentOracleAndChiDifference)
{
    Gaussian_Abfs abfs;
    abfs.init(ucell,
              kLmaxInit,
              std::vector<ModuleBase::Vector3<double>>{
                  ModuleBase::Vector3<double>(0.0, 0.0, 0.0)},
              G,
              kLambda);

    const double y00 = 1.0 / std::sqrt(ModuleBase::FOUR_PI);
    ModuleBase::realArray gaunt(1, 1, 1);
    gaunt(0, 0, 0) = y00;

    const double chi_a = -0.37;
    const double chi_b = 0.91;
    const RI::Tensor<std::complex<double>> vq_a
        = abfs.get_Vq(0, 0, 0, chi_a, kTau, gaunt);
    const RI::Tensor<std::complex<double>> vq_b
        = abfs.get_Vq(0, 0, 0, chi_b, kTau, gaunt);
    ASSERT_EQ(vq_a.shape.size(), 2u);
    ASSERT_EQ(vq_a.shape[0], 1u);
    ASSERT_EQ(vq_a.shape[1], 1u);
    ASSERT_EQ(vq_b.shape.size(), 2u);
    ASSERT_EQ(vq_b.shape[0], 1u);
    ASSERT_EQ(vq_b.shape[1], 1u);

    const std::complex<double> expected_a = oracle_v00_gamma(chi_a, kTau, ucell);
    const std::complex<double> expected_b = oracle_v00_gamma(chi_b, kTau, ucell);
    expect_complex_near(vq_a(0, 0), expected_a, 2e-11 * (1.0 + std::abs(expected_a)));
    expect_complex_near(vq_b(0, 0), expected_b, 2e-11 * (1.0 + std::abs(expected_b)));

    const std::complex<double> expected_difference
        = 8.0 * std::pow(ModuleBase::TWO_PI, 3) / ucell.omega * y00 * y00 * (chi_b - chi_a);
    expect_complex_near(vq_b(0, 0) - vq_a(0, 0),
                       expected_difference,
                       2e-11 * (1.0 + std::abs(expected_difference)));
}

} // namespace
