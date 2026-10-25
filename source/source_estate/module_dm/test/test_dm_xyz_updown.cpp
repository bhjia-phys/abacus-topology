#include <cmath>
#include <complex>

#include "gtest/gtest.h"

#include "source_estate/module_dm/density_matrix.h"
#include "source_estate/module_dm/cal_dm_psi.h"
#include "source_io/module_parameter/parameter.h"
#include <array>

/************************************************
 * The actual cal_dm_psi caller stores Q_ab=sum_n f_n C_an* C_bn,
 * the transpose of the physical density D=C f C^dagger. cal_DMR only
 * repacks this Q from column-major to row-major storage.
 * With sigma_y=[[0,-i],[i,0]], the physical Pauli traces therefore are
 *   rho_0=Q_uu+Q_dd, rho_x=Q_ud+Q_du,
 *   rho_y=-i*(Q_ud-Q_du), rho_z=Q_uu-Q_dd.
 * The real template takes their real parts. Testing an externally supplied
 * physical D instead of the actual stored Q would enforce the wrong contract.
 ***********************************************/

namespace
{
constexpr double TOL = 1e-12;
// contiguous 2x2 spin block at icol = 0 (as in cal_DMR for a single orbital pair)
const int step_trace[4] = {0, 1, 2, 3};

void expect_dvec_near(const double* got, const double r0, const double rx, const double ry, const double rz)
{
    EXPECT_NEAR(got[0], r0, TOL);
    EXPECT_NEAR(got[1], rx, TOL);
    EXPECT_NEAR(got[2], ry, TOL);
    EXPECT_NEAR(got[3], rz, TOL);
}

void expect_cvec_near(const std::complex<double>* got, const std::complex<double>* ref)
{
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_NEAR(got[i].real(), ref[i].real(), TOL) << "elem " << i;
        EXPECT_NEAR(got[i].imag(), ref[i].imag(), TOL) << "elem " << i;
    }
}
} // namespace

// Pure spinor psi = (1+i, 2-0.5i): stored Q = psi* psi^T is Hermitian, so the
// complex-template output must be purely real and identical to the double
// template. Reference values from m_i = Tr(sigma_i rho):
//   m_x = 2 Re(psi_up^* psi_dn) = 3, m_y = 2 Im(psi_up^* psi_dn) = -5,
//   m_z = |psi_up|^2 - |psi_dn|^2 = -2.25, charge = |psi|^2 = 6.25.
TEST(FuncXyzToUpdown, HermitianSpinor)
{
    const std::complex<double> up(1.0, 1.0);
    const std::complex<double> dn(2.0, -0.5);
    const std::complex<double> tmp[4] = {std::norm(up), std::conj(up) * dn, up * std::conj(dn), std::norm(dn)};
    // Q_ud = (1-i)(2-0.5i) = 1.5 - 2.5i, Q_du = conj(Q_ud)
    EXPECT_NEAR(tmp[1].real(), 1.5, TOL);
    EXPECT_NEAR(tmp[1].imag(), -2.5, TOL);

    double out_d[4];
    std::complex<double> out_c[4];
    elecstate::DensityMatrix_Tools::func_xyz_to_updown<double>(tmp, 0, step_trace, out_d);
    elecstate::DensityMatrix_Tools::func_xyz_to_updown<std::complex<double>>(tmp, 0, step_trace, out_c);

    expect_dvec_near(out_d, 6.25, 3.0, -5.0, -2.25);
    const std::complex<double> ref[4] = {6.25, 3.0, -5.0, -2.25};
    expect_cvec_near(out_c, ref);
}

// Magnetization along the axes, with actual stored-Q inputs. Both templates agree.
TEST(FuncXyzToUpdown, AxisMagnetizations)
{
    // spin +x: psi = (1,1)/sqrt2
    {
        const std::complex<double> up = std::complex<double>(1.0, 0.0) / std::sqrt(2.0);
        const std::complex<double> dn = std::complex<double>(1.0, 0.0) / std::sqrt(2.0);
        const std::complex<double> tmp[4] = {std::norm(up), std::conj(up) * dn, up * std::conj(dn), std::norm(dn)};
        double out_d[4];
        std::complex<double> out_c[4];
        elecstate::DensityMatrix_Tools::func_xyz_to_updown<double>(tmp, 0, step_trace, out_d);
        elecstate::DensityMatrix_Tools::func_xyz_to_updown<std::complex<double>>(tmp, 0, step_trace, out_c);
        expect_dvec_near(out_d, 1.0, 1.0, 0.0, 0.0);
        const std::complex<double> ref[4] = {1.0, 1.0, 0.0, 0.0};
        expect_cvec_near(out_c, ref);
    }
    // spin +y: psi=(1,i)/sqrt2, Q_ud=+i/2, so physical m_y=+1.
    {
        const std::complex<double> up = std::complex<double>(1.0, 0.0) / std::sqrt(2.0);
        const std::complex<double> dn = std::complex<double>(0.0, 1.0) / std::sqrt(2.0);
        const std::complex<double> tmp[4] = {std::norm(up), std::conj(up) * dn, up * std::conj(dn), std::norm(dn)};
        double out_d[4];
        std::complex<double> out_c[4];
        elecstate::DensityMatrix_Tools::func_xyz_to_updown<double>(tmp, 0, step_trace, out_d);
        elecstate::DensityMatrix_Tools::func_xyz_to_updown<std::complex<double>>(tmp, 0, step_trace, out_c);
        expect_dvec_near(out_d, 1.0, 0.0, 1.0, 0.0);
        const std::complex<double> ref[4] = {1.0, 0.0, 1.0, 0.0};
        expect_cvec_near(out_c, ref);
    }
    // spin -y: psi = (1,-i)/sqrt2 ->  rho_y = -1
    {
        const std::complex<double> up = std::complex<double>(1.0, 0.0) / std::sqrt(2.0);
        const std::complex<double> dn = std::complex<double>(0.0, -1.0) / std::sqrt(2.0);
        const std::complex<double> tmp[4] = {std::norm(up), std::conj(up) * dn, up * std::conj(dn), std::norm(dn)};
        double out_d[4];
        std::complex<double> out_c[4];
        elecstate::DensityMatrix_Tools::func_xyz_to_updown<double>(tmp, 0, step_trace, out_d);
        elecstate::DensityMatrix_Tools::func_xyz_to_updown<std::complex<double>>(tmp, 0, step_trace, out_c);
        expect_dvec_near(out_d, 1.0, 0.0, -1.0, 0.0);
        const std::complex<double> ref[4] = {1.0, 0.0, -1.0, 0.0};
        expect_cvec_near(out_c, ref);
    }
}

// Generic (not necessarily Hermitian) 4-tuple: pins the exact formulas of both
// templates, including that the double template equals the real part of the
// complex template componentwise.
TEST(FuncXyzToUpdown, GenericTuple)
{
    const std::complex<double> tmp[4] = {std::complex<double>(1.0, 2.0),
                                         std::complex<double>(0.5, 0.3),
                                         std::complex<double>(-0.7, 0.1),
                                         std::complex<double>(0.3, -0.4)};
    double out_d[4];
    std::complex<double> out_c[4];
    elecstate::DensityMatrix_Tools::func_xyz_to_updown<double>(tmp, 0, step_trace, out_d);
    elecstate::DensityMatrix_Tools::func_xyz_to_updown<std::complex<double>>(tmp, 0, step_trace, out_c);

    // double: rho_y=Im(Q_ud)-Im(Q_du)=0.3-0.1.
    expect_dvec_near(out_d, 1.3, -0.2, 0.2, 0.7);
    // complex: rho_y=-i*(Q_ud-Q_du)=0.2-1.2i.
    const std::complex<double> ref[4] = {std::complex<double>(1.3, 1.6),
                                         std::complex<double>(-0.2, 0.4),
                                         std::complex<double>(0.2, -1.2),
                                         std::complex<double>(0.7, 2.4)};
    expect_cvec_near(out_c, ref);
    // cross-consistency: the double template is the real part of the complex one
    for (int i = 0; i < 4; ++i)
    {
        EXPECT_NEAR(out_d[i], out_c[i].real(), TOL) << "elem " << i;
    }
}

// Pauli -> stored Q -> Pauli: Q_ud=(rho_x+i*rho_y)/2 because Q=D^T.
TEST(FuncXyzToUpdown, PauliRoundTrip)
{
    const double r0 = 2.0, rx = -1.5, ry = 0.75, rz = 0.25;
    const std::complex<double> tmp[4] = {std::complex<double>(0.5 * (r0 + rz), 0.0),
                                         std::complex<double>(0.5 * rx, 0.5 * ry),
                                         std::complex<double>(0.5 * rx, -0.5 * ry),
                                         std::complex<double>(0.5 * (r0 - rz), 0.0)};
    double out_d[4];
    std::complex<double> out_c[4];
    elecstate::DensityMatrix_Tools::func_xyz_to_updown<double>(tmp, 0, step_trace, out_d);
    elecstate::DensityMatrix_Tools::func_xyz_to_updown<std::complex<double>>(tmp, 0, step_trace, out_c);
    expect_dvec_near(out_d, r0, rx, ry, rz);
    const std::complex<double> ref[4] = {r0, rx, ry, rz};
    expect_cvec_near(out_c, ref);
}

// icol/step_trace offset handling: with a nonzero base column the outputs land
// at target[icol + step_trace[k]] (the same indexing used by cal_DMR).
TEST(FuncXyzToUpdown, IndexOffset)
{
    const std::complex<double> tmp[4] = {std::complex<double>(1.0, 0.0),
                                         std::complex<double>(0.0, 1.0),
                                         std::complex<double>(0.0, -1.0),
                                         std::complex<double>(2.0, 0.0)};
    const int col_size = 8; // arbitrary column stride of the atom-pair block
    const int step[4] = {0, 1, col_size, col_size + 1};
    double out[2 * col_size + 2] = {0.0};
    elecstate::DensityMatrix_Tools::func_xyz_to_updown<double>(tmp, col_size / 2, step, out);
    // rho_0=3, rho_x=0, rho_y=1-(-1)=2, rho_z=-1 at icol=4.
    EXPECT_NEAR(out[col_size / 2 + 0], 3.0, TOL);
    EXPECT_NEAR(out[col_size / 2 + 1], 0.0, TOL);
    EXPECT_NEAR(out[col_size / 2 + col_size], 2.0, TOL);
    EXPECT_NEAR(out[col_size / 2 + col_size + 1], -1.0, TOL);
}

class TestParameters
{
  public:
    static void set_spinor_caller(const int nspin, const std::string& solver)
    {
        PARAM.input.nspin = nspin;
        PARAM.input.ks_solver = solver;
    }
};

template<typename TR>
void actual_density_caller(const std::complex<double> up, const std::complex<double> dn)
{
    Parallel_Orbitals pv;
#ifdef __MPI
    pv.init(2,2,2,MPI_COMM_WORLD);
    std::ofstream quiet;
    EXPECT_EQ(pv.set_nloc_wfc_Eij(2,quiet,quiet),0);
    pv.set_desc_wfc_Eij(2,2,pv.nrow);
#else
    pv.set_serial(2,2);
#endif
    const int first[1] = {0};
    pv.set_atomic_trace(first,1,2);
    psi::Psi<std::complex<double>> wfc(1,2,2,2,true);
    for (int i=0;i<4;++i) wfc.get_pointer()[i]=0;
    wfc(0,0,0)=up;
    wfc(0,0,1)=dn;
    ModuleBase::matrix wg(1,2);
    wg(0,0)=1;
    wg(0,1)=0;
    const std::vector<ModuleBase::Vector3<double>> mesh{{0,0,0}};
    elecstate::DensityMatrix<std::complex<double>,TR> dm(&pv,1,mesh,1);
    elecstate::cal_dm_psi(&pv,wg,wfc,dm);
    const std::complex<double> c[2] = {up,dn};
    for (int i=0;i<2;++i)
        for (int j=0;j<2;++j)
            {
                // cal_DMR reads column-major Q directly; get_DMK intentionally
                // provides the transposed row-major public view instead.
                EXPECT_NEAR(std::abs(dm.get_DMK_pointer(0)[i+j*pv.nrow]-std::conj(c[i])*c[j]),0,TOL);
                EXPECT_NEAR(std::abs(dm.get_DMK(1,0,i,j)-c[i]*std::conj(c[j])),0,TOL);
            }
    hamilt::HContainer<TR> layout(&pv);
    layout.insert_pair(hamilt::AtomPair<TR>(0,0,0,0,0,&pv));
    layout.allocate(nullptr,true);
    dm.init_DMR(layout);
    // Explicit core path covers both native output types, independent of LR wrappers.
    elecstate::DensityMatrix_Tools::cal_DMR(dm,dm.get_DMR_vector(),-1);
    const auto* got = dm.get_DMR_pointer(1)->find_matrix(0,0,0,0,0);
    const std::complex<double> cross = std::conj(up)*dn;
    const double expected[4] = {std::norm(up)+std::norm(dn),2*cross.real(),
                                2*cross.imag(),std::norm(up)-std::norm(dn)};
    std::cout << "raw-column-major-Qud=" << dm.get_DMK_pointer(0)[pv.nrow]
              << " physical-Dud=" << up*std::conj(dn)
              << " native-My=" << got->get_value(1,0)
              << " expected-My=" << expected[2] << std::endl;
    for (int i=0;i<2;++i)
        for (int j=0;j<2;++j)
            EXPECT_NEAR(std::abs(got->get_value(i,j)-expected[2*i+j]),0,TOL);
}

TEST(CalDmPsiToPauli, ActualStoredTransposeCallerBothOutputTypes)
{
#ifdef __MPI
    if (GlobalV::NPROC!=1) GTEST_SKIP() << "The minimal complete caller uses one MPI rank.";
#endif
    const int old_spin=PARAM.inp.nspin;
    const std::string old_solver=PARAM.inp.ks_solver;
    TestParameters::set_spinor_caller(4,"scalapack_gvx");
    const std::array<std::array<std::complex<double>,2>,3> spinors{{
        {{1/std::sqrt(2.0),std::complex<double>(0,1/std::sqrt(2.0))}},
        {{1/std::sqrt(2.0),std::complex<double>(0,-1/std::sqrt(2.0))}},
        {{std::complex<double>(1,1)/2.5,std::complex<double>(2,-0.5)/2.5}}}};
    for (const auto& p:spinors)
    {
        actual_density_caller<double>(p[0],p[1]);
        actual_density_caller<std::complex<double>>(p[0],p[1]);
    }
    TestParameters::set_spinor_caller(old_spin,old_solver);
}

int main(int argc, char** argv)
{
#ifdef __MPI
    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&GlobalV::MY_RANK);
    MPI_Comm_size(MPI_COMM_WORLD,&GlobalV::NPROC);
    PARAM.set_pal_param(GlobalV::MY_RANK,GlobalV::NPROC,1);
#endif
    testing::InitGoogleTest(&argc,argv);
    const int result=RUN_ALL_TESTS();
#ifdef __MPI
    MPI_Finalize();
#endif
    return result;
}
