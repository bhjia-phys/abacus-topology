#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <complex>
#include <vector>

#include "../symmetry.h"
#include "../symmetry_rotation_spin.h"
#include "source_cell/unitcell.h"

/************************************************
 *  unit test of Symmetry::rhog_symmetry_nspin4
 *  (nspin=4 / SOC reciprocal-space spin-density symmetrization)
 *
 *  The operator is driven with a MANUALLY built symmetry group (no analy_sys),
 *  so the group, grid and spin rotations are fully controlled. We use the proper
 *  point group D_4 = {E, C4z, C2z, C4z^3, C2x, C2y, C2[110], C2[1-10]} on a cubic
 *  lattice (a=1 => gmatc = kgmatrix = gmatrix = R^T). D_4 is NON-ABELIAN, so the
 *  test is sensitive to spin-rotation representation/handedness bugs that an
 *  abelian group (e.g. C_6h) would hide.
 *
 *  Checks:
 *   - Idempotence: symmetrizing an already-symmetric density is a no-op.
 *   - Invariance:  the symmetrized density satisfies m(R_g G) = W(g) m(G) for
 *                  every group operation g, verified by an independent oracle.
***********************************************/

// mock the unused constructors pulled in by linking the symmetry library
pseudo::pseudo() {}
pseudo::~pseudo() {}
Atom::Atom() {}
Atom::~Atom() {}
Atom_pseudo::Atom_pseudo() {}
Atom_pseudo::~Atom_pseudo() {}
UnitCell::UnitCell() {}
UnitCell::~UnitCell() {}
Magnetism::Magnetism() {}
Magnetism::~Magnetism() {}
SepPot::SepPot() {}
SepPot::~SepPot() {}
Sep_Cell::Sep_Cell() noexcept {}
Sep_Cell::~Sep_Cell() noexcept {}

namespace
{
constexpr int N = 4;                // grid dimension (even, so -i mod N stays on-grid)
constexpr int NXYZ = N * N * N;
constexpr double TOL = 1e-10;

// the 8 proper rotations of D_4, as textbook column-vector matrices R (r'=R r)
const std::array<std::array<std::array<int, 3>, 3>, 8> Rcol = {{
    {{{ 1, 0, 0}, { 0, 1, 0}, { 0, 0, 1}}},   // E
    {{{ 0,-1, 0}, { 1, 0, 0}, { 0, 0, 1}}},   // C4z
    {{{-1, 0, 0}, { 0,-1, 0}, { 0, 0, 1}}},   // C2z
    {{{ 0, 1, 0}, {-1, 0, 0}, { 0, 0, 1}}},   // C4z^3
    {{{ 1, 0, 0}, { 0,-1, 0}, { 0, 0,-1}}},   // C2x
    {{{-1, 0, 0}, { 0, 1, 0}, { 0, 0,-1}}},   // C2y
    {{{ 0, 1, 0}, { 1, 0, 0}, { 0, 0,-1}}},   // C2[110]
    {{{ 0,-1, 0}, {-1, 0, 0}, { 0, 0,-1}}},   // C2[1-10]
}};

// ABACUS stores the cartesian rotation in the row-vector convention: gmatc = R^T.
ModuleBase::Matrix3 gmatc_of(int g)
{
    const auto& R = Rcol[g];
    return ModuleBase::Matrix3(R[0][0], R[1][0], R[2][0],
                               R[0][1], R[1][1], R[2][1],
                               R[0][2], R[1][2], R[2][2]);
}

// mirror of the internal rotate_recip: G' index components from kgmatrix (=gmatc)
void rotate_index(const ModuleBase::Matrix3& g, int i, int j, int k, int& ii, int& jj, int& kk)
{
    ii = int(g.e11 * i + g.e21 * j + g.e31 * k); if (ii < 0) ii += 10 * N; ii %= N;
    jj = int(g.e12 * i + g.e22 * j + g.e32 * k); if (jj < 0) jj += 10 * N; jj %= N;
    kk = int(g.e13 * i + g.e23 * j + g.e33 * k); if (kk < 0) kk += 10 * N; kk %= N;
}

// build a Symmetry object carrying the D_4 group on a cubic grid
void build_group(ModuleSymmetry::Symmetry& symm, std::vector<ModuleBase::Matrix3>& wspin)
{
    symm.epsilon = 1e-6;
    symm.nrot = 8;
    symm.nrotk = 8;
    symm.ncell = 1;
    symm.ptrans = {ModuleBase::Vector3<double>(0.0, 0.0, 0.0)};
    ModuleSymmetry::Symmetry::pricell_loop = false;
    wspin.resize(8);
    for (int g = 0; g < 8; ++g)
    {
        const ModuleBase::Matrix3 gc = gmatc_of(g);
        symm.gmatrix[g] = gc;      // cubic a=1: direct == cartesian
        symm.kgmatrix[g] = gc;     // orthogonal rotation: reciprocal == direct
        symm.gtrans[g] = ModuleBase::Vector3<double>(0.0, 0.0, 0.0);
        wspin[g] = ModuleSymmetry::SpinRotation::spin_so3(gc);
    }
}

// a fixed, non-symmetric complex spin density on the grid
void fill_density(std::vector<std::complex<double>>& x,
                  std::vector<std::complex<double>>& y,
                  std::vector<std::complex<double>>& z)
{
    for (int idx = 0; idx < NXYZ; ++idx)
    {
        x[idx] = std::complex<double>(0.3 * idx - 1.0, 0.7 * ((idx * 13) % 5) - 1.5);
        y[idx] = std::complex<double>(-0.5 * ((idx * 7) % 4) + 0.9, 0.2 * idx - 2.0);
        z[idx] = std::complex<double>(0.11 * ((idx * 3) % 6), -0.4 * ((idx * 5) % 7) + 1.0);
    }
}

// ---------------------------------------------------------------------
// Hexagonal C3z (120-degree rotation) group, exercised in the exact
// geometry used by psymmg_soc(): integer direct-lattice matrices gmatrix,
// reciprocal reduced matrices kgmatrix = (gmatrix^-1)^T, and the cartesian
// spin rotation W = spin_so3(ilatvec * gmatrix * latvec).
// ---------------------------------------------------------------------
constexpr double SQ3 = 1.7320508075688772;

// hexagonal direct lattice (rows = a1, a2, a3), a1=(1,0,0), a2=(-1/2,sqrt3/2,0)
ModuleBase::Matrix3 latvec_hex()
{
    return ModuleBase::Matrix3(1.0, 0.0, 0.0,
                               -0.5, SQ3 / 2.0, 0.0,
                               0.0, 0.0, 1.0);
}

// 120-degree rotation about z in the direct (row-vector) basis: a1->a2, a2->-a1-a2
ModuleBase::Matrix3 gdirect_c3z()
{
    return ModuleBase::Matrix3(0.0, 1.0, 0.0,
                               -1.0, -1.0, 0.0,
                               0.0, 0.0, 1.0);
}

// build the C3z = {E, C3z, C3z^2} group (hexagonal direct basis) and the
// per-operation spin-rotation matrices exactly as psymmg_soc() does
void build_group_c3z(ModuleSymmetry::Symmetry& symm, std::vector<ModuleBase::Matrix3>& wspin)
{
    symm.epsilon = 1e-6;
    symm.nrot = 3;
    symm.nrotk = 3;
    symm.ncell = 1;
    symm.ptrans = {ModuleBase::Vector3<double>(0.0, 0.0, 0.0)};
    ModuleSymmetry::Symmetry::pricell_loop = false;
    wspin.resize(3);
    const ModuleBase::Matrix3 E(1.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 1.0);
    const ModuleBase::Matrix3 G = gdirect_c3z();
    const ModuleBase::Matrix3 G2 = G * G;
    const ModuleBase::Matrix3 gmat[3] = {E, G, G2};
    const ModuleBase::Matrix3 lat = latvec_hex();
    const ModuleBase::Matrix3 ilat = lat.Inverse();
    for (int g = 0; g < 3; ++g)
    {
        symm.gmatrix[g] = gmat[g];
        symm.kgmatrix[g] = gmat[g].Inverse().Transpose();
        symm.gtrans[g] = ModuleBase::Vector3<double>(0.0, 0.0, 0.0);
        // direct -> cartesian conversion, same formula as psymmg_soc()
        const ModuleBase::Matrix3 gmatc = ilat * gmat[g] * lat;
        wspin[g] = ModuleSymmetry::SpinRotation::spin_so3(gmatc);
    }
}
} // namespace

TEST(RhogSymmetrySoc, Idempotence)
{
    ModuleSymmetry::Symmetry symm;
    std::vector<ModuleBase::Matrix3> wspin;
    build_group(symm, wspin);

    std::vector<int> ixyz2ipw(NXYZ);
    for (int i = 0; i < NXYZ; ++i) { ixyz2ipw[i] = i; } // every FFT point is a plane wave

    std::vector<std::complex<double>> x(NXYZ), y(NXYZ), z(NXYZ);
    fill_density(x, y, z);

    symm.rhog_symmetry_nspin4(x.data(), y.data(), z.data(), wspin.data(), ixyz2ipw.data(), N, N, N, N, N, N, nullptr, nullptr, nullptr, -1);
    std::vector<std::complex<double>> x1 = x, y1 = y, z1 = z;
    symm.rhog_symmetry_nspin4(x.data(), y.data(), z.data(), wspin.data(), ixyz2ipw.data(), N, N, N, N, N, N, nullptr, nullptr, nullptr, -1);

    for (int i = 0; i < NXYZ; ++i)
    {
        EXPECT_NEAR(x[i].real(), x1[i].real(), TOL); EXPECT_NEAR(x[i].imag(), x1[i].imag(), TOL);
        EXPECT_NEAR(y[i].real(), y1[i].real(), TOL); EXPECT_NEAR(y[i].imag(), y1[i].imag(), TOL);
        EXPECT_NEAR(z[i].real(), z1[i].real(), TOL); EXPECT_NEAR(z[i].imag(), z1[i].imag(), TOL);
    }
}

TEST(RhogSymmetrySoc, GroupInvariance)
{
    ModuleSymmetry::Symmetry symm;
    std::vector<ModuleBase::Matrix3> wspin;
    build_group(symm, wspin);

    std::vector<int> ixyz2ipw(NXYZ);
    for (int i = 0; i < NXYZ; ++i) { ixyz2ipw[i] = i; }

    std::vector<std::complex<double>> x(NXYZ), y(NXYZ), z(NXYZ);
    fill_density(x, y, z);
    symm.rhog_symmetry_nspin4(x.data(), y.data(), z.data(), wspin.data(), ixyz2ipw.data(), N, N, N, N, N, N, nullptr, nullptr, nullptr, -1);

    // non-triviality guard: the symmetrized density must not be all-zero, otherwise
    // invariance would hold trivially and the test would be meaningless.
    double maxabs = 0.0;
    for (int i = 0; i < NXYZ; ++i)
    {
        maxabs = std::max(maxabs, std::abs(x[i]));
        maxabs = std::max(maxabs, std::abs(y[i]));
        maxabs = std::max(maxabs, std::abs(z[i]));
    }
    EXPECT_GT(maxabs, 0.1);

    // independent oracle: the symmetrized density must obey m(R_g G) = W(g) m(G) for all g, G.
    for (int g = 0; g < 8; ++g)
    {
        const ModuleBase::Matrix3& W = wspin[g];
        for (int i = 0; i < N; ++i)
        {
            for (int j = 0; j < N; ++j)
            {
                for (int k = 0; k < N; ++k)
                {
                    const int idx = (i * N + j) * N + k;
                    int ii, jj, kk;
                    rotate_index(symm.kgmatrix[g], i, j, k, ii, jj, kk);
                    const int idx2 = (ii * N + jj) * N + kk;
                    const std::complex<double> ex = W.e11 * x[idx] + W.e12 * y[idx] + W.e13 * z[idx];
                    const std::complex<double> ey = W.e21 * x[idx] + W.e22 * y[idx] + W.e23 * z[idx];
                    const std::complex<double> ez = W.e31 * x[idx] + W.e32 * y[idx] + W.e33 * z[idx];
                    EXPECT_NEAR(x[idx2].real(), ex.real(), TOL) << "g=" << g << " idx=" << idx;
                    EXPECT_NEAR(x[idx2].imag(), ex.imag(), TOL) << "g=" << g << " idx=" << idx;
                    EXPECT_NEAR(y[idx2].real(), ey.real(), TOL) << "g=" << g << " idx=" << idx;
                    EXPECT_NEAR(y[idx2].imag(), ey.imag(), TOL) << "g=" << g << " idx=" << idx;
                    EXPECT_NEAR(z[idx2].real(), ez.real(), TOL) << "g=" << g << " idx=" << idx;
                    EXPECT_NEAR(z[idx2].imag(), ez.imag(), TOL) << "g=" << g << " idx=" << idx;
                }
            }
        }
    }
}

// ---------------------------------------------------------------------
// Hexagonal C3z (120-degree rotation) cases with nonzero m_y.
// ---------------------------------------------------------------------

// The 120-degree spin rotation of a pure y-magnetization:
// W(C3z) * (0,1,0) = (-sqrt3/2, -1/2, 0): m_y stays nonzero and mixes into
// m_x. Also pins the direct->cartesian conversion used by psymmg_soc()
// (gmatc = ilatvec * gmatrix * latvec) and the reciprocal relation
// kgmatrix = (gmatrix^-1)^T for a non-orthogonal lattice.
TEST(RhogSymmetrySoc, C3zHexSpinRotation)
{
    ModuleSymmetry::Symmetry symm;
    std::vector<ModuleBase::Matrix3> wspin;
    build_group_c3z(symm, wspin);

    // W(C3z) is the column-vector +120deg rotation about z
    const ModuleBase::Matrix3& W = wspin[1];
    EXPECT_NEAR(W.e11, -0.5, 1e-12);
    EXPECT_NEAR(W.e12, -SQ3 / 2.0, 1e-12);
    EXPECT_NEAR(W.e21, SQ3 / 2.0, 1e-12);
    EXPECT_NEAR(W.e22, -0.5, 1e-12);
    EXPECT_NEAR(W.e33, 1.0, 1e-12);
    // W * (0,1,0) = (W12, W22, W32): nonzero m_y under a 120-degree rotation
    EXPECT_NEAR(W.e12, -SQ3 / 2.0, 1e-12);
    EXPECT_NEAR(W.e22, -0.5, 1e-12);
    EXPECT_NEAR(std::abs(W.e32), 0.0, 1e-12);

    // kgmatrix(C3z) = (gmatrix^-1)^T for the hexagonal direct basis
    EXPECT_NEAR(symm.kgmatrix[1].e11, -1.0, 1e-12);
    EXPECT_NEAR(symm.kgmatrix[1].e12, 1.0, 1e-12);
    EXPECT_NEAR(symm.kgmatrix[1].e21, -1.0, 1e-12);
    EXPECT_NEAR(symm.kgmatrix[1].e22, 0.0, 1e-12);

    // gmatc(C3z) = ilatvec * gmatrix * latvec is the row-vector +120deg matrix
    const ModuleBase::Matrix3 lat = latvec_hex();
    const ModuleBase::Matrix3 gmatc = lat.Inverse() * gdirect_c3z() * lat;
    EXPECT_NEAR(gmatc.e11, -0.5, 1e-12);
    EXPECT_NEAR(gmatc.e12, SQ3 / 2.0, 1e-12);
    EXPECT_NEAR(gmatc.e21, -SQ3 / 2.0, 1e-12);
    EXPECT_NEAR(gmatc.e22, -0.5, 1e-12);
}

// Group invariance of rhog_symmetry_nspin4 under the hexagonal C3z group:
// the symmetrized density must satisfy m(G * kg(g)) = W(g) m(G) for all g,
// verified with the independent oracle. The D4 test above covers 90-degree
// rotations; this locks the 120-degree case (spin mixing with nonzero m_y)
// on a non-orthogonal lattice.
TEST(RhogSymmetrySoc, C3zHexGroupInvariance)
{
    ModuleSymmetry::Symmetry symm;
    std::vector<ModuleBase::Matrix3> wspin;
    build_group_c3z(symm, wspin);

    std::vector<int> ixyz2ipw(NXYZ);
    for (int i = 0; i < NXYZ; ++i) { ixyz2ipw[i] = i; }

    std::vector<std::complex<double>> x(NXYZ), y(NXYZ), z(NXYZ);
    fill_density(x, y, z);

    // non-triviality guard on the INPUT: nonzero m_y present
    double max_my_in = 0.0;
    for (int i = 0; i < NXYZ; ++i) { max_my_in = std::max(max_my_in, std::abs(y[i])); }
    EXPECT_GT(max_my_in, 0.1);

    symm.rhog_symmetry_nspin4(x.data(), y.data(), z.data(), wspin.data(), ixyz2ipw.data(), N, N, N, N, N, N, nullptr, nullptr, nullptr, -1);
    std::vector<std::complex<double>> x1 = x, y1 = y, z1 = z;
    symm.rhog_symmetry_nspin4(x.data(), y.data(), z.data(), wspin.data(), ixyz2ipw.data(), N, N, N, N, N, N, nullptr, nullptr, nullptr, -1);

    // idempotence
    for (int i = 0; i < NXYZ; ++i)
    {
        EXPECT_NEAR(x[i].real(), x1[i].real(), TOL); EXPECT_NEAR(x[i].imag(), x1[i].imag(), TOL);
        EXPECT_NEAR(y[i].real(), y1[i].real(), TOL); EXPECT_NEAR(y[i].imag(), y1[i].imag(), TOL);
        EXPECT_NEAR(z[i].real(), z1[i].real(), TOL); EXPECT_NEAR(z[i].imag(), z1[i].imag(), TOL);
    }

    // non-triviality guard on the OUTPUT
    double maxabs = 0.0;
    for (int i = 0; i < NXYZ; ++i)
    {
        maxabs = std::max(maxabs, std::abs(x[i]));
        maxabs = std::max(maxabs, std::abs(y[i]));
        maxabs = std::max(maxabs, std::abs(z[i]));
    }
    EXPECT_GT(maxabs, 0.1);

    // independent oracle: m(G * kg(g)) = W(g) m(G) for all g, G
    for (int g = 0; g < 3; ++g)
    {
        const ModuleBase::Matrix3& W = wspin[g];
        for (int i = 0; i < N; ++i)
        {
            for (int j = 0; j < N; ++j)
            {
                for (int k = 0; k < N; ++k)
                {
                    const int idx = (i * N + j) * N + k;
                    int ii, jj, kk;
                    rotate_index(symm.kgmatrix[g], i, j, k, ii, jj, kk);
                    const int idx2 = (ii * N + jj) * N + kk;
                    const std::complex<double> ex = W.e11 * x[idx] + W.e12 * y[idx] + W.e13 * z[idx];
                    const std::complex<double> ey = W.e21 * x[idx] + W.e22 * y[idx] + W.e23 * z[idx];
                    const std::complex<double> ez = W.e31 * x[idx] + W.e32 * y[idx] + W.e33 * z[idx];
                    EXPECT_NEAR(x[idx2].real(), ex.real(), TOL) << "g=" << g << " idx=" << idx;
                    EXPECT_NEAR(x[idx2].imag(), ex.imag(), TOL) << "g=" << g << " idx=" << idx;
                    EXPECT_NEAR(y[idx2].real(), ey.real(), TOL) << "g=" << g << " idx=" << idx;
                    EXPECT_NEAR(y[idx2].imag(), ey.imag(), TOL) << "g=" << g << " idx=" << idx;
                    EXPECT_NEAR(z[idx2].real(), ez.real(), TOL) << "g=" << g << " idx=" << idx;
                    EXPECT_NEAR(z[idx2].imag(), ez.imag(), TOL) << "g=" << g << " idx=" << idx;
                }
            }
        }
    }
}

// Physical 120-degree covariance with nonzero m_y: a constant in-plane
// magnetization m = (0,1,0) is incompatible with the threefold rotation and
// must be symmetrized away to zero, while the out-of-plane moment (0,0,1)
// is invariant and must survive unchanged.
TEST(RhogSymmetrySoc, C3zAnnihilatesNetInPlaneMoment)
{
    ModuleSymmetry::Symmetry symm;
    std::vector<ModuleBase::Matrix3> wspin;
    build_group_c3z(symm, wspin);

    std::vector<int> ixyz2ipw(NXYZ);
    for (int i = 0; i < NXYZ; ++i) { ixyz2ipw[i] = i; }

    {
        std::vector<std::complex<double>> x(NXYZ, 0.0), y(NXYZ, 1.0), z(NXYZ, 0.0);
        symm.rhog_symmetry_nspin4(x.data(), y.data(), z.data(), wspin.data(), ixyz2ipw.data(), N, N, N, N, N, N, nullptr, nullptr, nullptr, -1);
        for (int i = 0; i < NXYZ; ++i)
        {
            EXPECT_NEAR(x[i].real(), 0.0, TOL);
            EXPECT_NEAR(y[i].real(), 0.0, TOL);
            EXPECT_NEAR(z[i].real(), 0.0, TOL);
        }
    }
    {
        std::vector<std::complex<double>> x(NXYZ, 0.0), y(NXYZ, 0.0), z(NXYZ, 1.0);
        symm.rhog_symmetry_nspin4(x.data(), y.data(), z.data(), wspin.data(), ixyz2ipw.data(), N, N, N, N, N, N, nullptr, nullptr, nullptr, -1);
        for (int i = 0; i < NXYZ; ++i)
        {
            EXPECT_NEAR(x[i].real(), 0.0, TOL);
            EXPECT_NEAR(y[i].real(), 0.0, TOL);
            EXPECT_NEAR(z[i].real(), 1.0, TOL);
        }
    }
}

int main(int argc, char** argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}
