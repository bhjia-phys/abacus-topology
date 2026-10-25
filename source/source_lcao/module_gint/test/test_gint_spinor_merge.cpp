#include "gtest/gtest.h"

#include "../gint_common.h"
#include "../gint_info.h"

#include <array>
#include <cmath>
#include <complex>
#include <vector>

// Parameter's existing friend seam keeps test setup out of production APIs.
class TestParameters
{
  public:
    static void set_domag(const bool value) { PARAM.sys.domag = value; }
};

namespace
{
using Complex = std::complex<double>;
using SpinBlock = std::array<Complex, 4>;
using PauliBlock = std::array<double, 4>;

SpinBlock spinor(const PauliBlock& a)
{
    return {{a[0] + a[3], Complex(a[1], -a[2]),
             Complex(a[1], a[2]), a[0] - a[3]}};
}

SpinBlock rotate(const SpinBlock& a, const SpinBlock& u)
{
    SpinBlock result{};
    for (int s = 0; s < 2; ++s)
        for (int t = 0; t < 2; ++t)
            for (int v = 0; v < 2; ++v)
                for (int w = 0; w < 2; ++w)
                    result[2*s+t] += u[2*s+v] * a[2*v+w] * std::conj(u[2*t+w]);
    return result;
}

PauliBlock rotated_pauli(const PauliBlock& a)
{
    const double angle = 0.73;
    const double n[3] = {1/std::sqrt(14.0), 2/std::sqrt(14.0), 3/std::sqrt(14.0)};
    const double dot = n[0]*a[1] + n[1]*a[2] + n[2]*a[3];
    const double cross[3] = {n[1]*a[3]-n[2]*a[2], n[2]*a[1]-n[0]*a[3], n[0]*a[2]-n[1]*a[1]};
    PauliBlock result = a;
    for (int i = 0; i < 3; ++i)
        result[i+1] = std::cos(angle)*a[i+1] + std::sin(angle)*cross[i]
                      + (1-std::cos(angle))*n[i]*dot;
    return result;
}

SpinBlock spin_rotation()
{
    const double c = std::cos(0.73/2), s = std::sin(0.73/2);
    const double n[3] = {1/std::sqrt(14.0), 2/std::sqrt(14.0), 3/std::sqrt(14.0)};
    return {{Complex(c,-s*n[2]), Complex(-s*n[1],-s*n[0]),
             Complex(s*n[1],-s*n[0]), Complex(c,s*n[2])}};
}
}

class GintSpinorMergeTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        old_domag_ = PARAM.globalv.domag;
        TestParameters::set_domag(true);
        ucell_.ntype = 1;
        ucell_.nat = 2;
        ucell_.atoms = new Atom[1];
        ucell_.atoms[0].na = 2;
        ucell_.atoms[0].nw = 2;
        ucell_.iat2it = new int[2]{0,0};
        ucell_.iat2ia = new int[2]{0,1};
        ucell_.set_iat2iwt(2);
#ifdef __MPI
        pv_.init(8, 8, 2, MPI_COMM_WORLD);
#else
        pv_.set_serial(8, 8);
#endif
        pv_.set_atomic_trace(ucell_.get_iat2iwt(), 2, 8);
        ijrs_ = {4, 0,0,1,0,0,0, 0,1,1,1,-1,0, 1,0,1,-1,1,0, 1,1,1,0,0,0};
        info_ = ModuleGint::GintInfo::make_test_instance_ptr(ucell_, ijrs_);
    }

    void TearDown() override
    {
        delete info_;
        TestParameters::set_domag(old_domag_);
    }

    PauliBlock reference(const int a, const int b, const int i, const int j) const
    {
        if (a > b) return reference(b, a, j, i);
        PauliBlock result{};
        for (int mu = 0; mu < 4; ++mu)
            result[mu] = 0.13*(mu+1) + 0.07*a + 0.02*b
                         + (a == b ? 0.03*(i+j) : 0.02*i-0.04*j);
        return result;
    }

    hamilt::HContainer<Complex> merged(const bool rotated) const
    {
        std::vector<hamilt::HContainer<double>> channels;
        for (int mu = 0; mu < 4; ++mu) channels.push_back(info_->get_hr<double>());
        for (int a = 0; a < 2; ++a)
            for (int b = 0; b < 2; ++b)
            {
                const ModuleBase::Vector3<int> r(b-a, a-b, 0);
                for (int i = 0; i < 2; ++i)
                    for (int j = 0; j < 2; ++j)
                    {
                        PauliBlock value = reference(a,b,i,j);
                        if (rotated) value = rotated_pauli(value);
                        for (int mu = 0; mu < 4; ++mu)
                            channels[mu].find_matrix(a,b,r)->get_value(i,j) = value[mu];
                    }
            }
        hamilt::HContainer<Complex> result(&pv_, nullptr, &ijrs_);
        result.allocate(nullptr, true);
        ModuleGint::merge_hr_part_to_hR(channels, &result, *info_);
        return result;
    }

    SpinBlock block(const hamilt::HContainer<Complex>& h, const int a, const int b,
                    const int i, const int j) const
    {
        const ModuleBase::Vector3<int> r(b-a, a-b, 0);
        const auto* matrix = h.find_matrix(a,b,r);
        SpinBlock result{};
        for (int s = 0; s < 2; ++s)
            for (int t = 0; t < 2; ++t)
                result[2*s+t] = matrix->get_value(2*i+s,2*j+t);
        return result;
    }

    UnitCell ucell_;
    Parallel_Orbitals pv_;
    std::vector<int> ijrs_;
    ModuleGint::GintInfo* info_ = nullptr;
    bool old_domag_ = false;
};

TEST_F(GintSpinorMergeTest, FullComplexPauliReferenceAndHermiticity)
{
    const auto h = merged(false);
    for (int a = 0; a < 2; ++a)
        for (int b = 0; b < 2; ++b)
            for (int i = 0; i < 2; ++i)
                for (int j = 0; j < 2; ++j)
                {
                    const auto got = block(h,a,b,i,j);
                    const auto expected = spinor(reference(a,b,i,j));
                    const auto reverse = block(h,b,a,j,i);
                    for (int s = 0; s < 2; ++s)
                        for (int t = 0; t < 2; ++t)
                        {
                            EXPECT_NEAR(std::abs(got[2*s+t]-expected[2*s+t]),0,1e-12);
                            EXPECT_NEAR(std::abs(got[2*s+t]-std::conj(reverse[2*t+s])),0,1e-12);
                        }
                }
}

TEST_F(GintSpinorMergeTest, FullComplexSpinRotationCovariance)
{
    const auto h = merged(false);
    const auto hr = merged(true);
    const auto u = spin_rotation();
    for (int a = 0; a < 2; ++a)
        for (int b = 0; b < 2; ++b)
            for (int i = 0; i < 2; ++i)
                for (int j = 0; j < 2; ++j)
                {
                    const auto got = block(hr,a,b,i,j);
                    const auto expected = rotate(block(h,a,b,i,j),u);
                    for (int st = 0; st < 4; ++st)
                        EXPECT_NEAR(std::abs(got[st]-expected[st]),0,1e-12);
                }
}
