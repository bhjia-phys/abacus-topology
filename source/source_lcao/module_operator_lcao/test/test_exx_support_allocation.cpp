#include "gtest/gtest.h"
#include "../op_exx_lcao.hpp"

#include <memory>

namespace
{
template<typename T>
using Reference = std::vector<std::map<int,std::map<hamilt::TAC,RI::Tensor<T>>>>;

std::unique_ptr<Parallel_Orbitals> parallel(const int npol, const int* start)
{
    std::unique_ptr<Parallel_Orbitals> pv(new Parallel_Orbitals);
#ifdef __MPI
    pv->init(2*npol,2*npol,npol,MPI_COMM_WORLD);
#else
    pv->set_serial(2*npol,2*npol);
#endif
    pv->set_atomic_trace(start,1,2*npol);
    return pv;
}

template<typename T>
RI::Tensor<T> matrix()
{
    RI::Tensor<T> t({2,2});
    for (int i=0;i<2;++i)
        for (int j=0;j<2;++j) t(i,j)=T{};
    return t;
}

template<typename T>
void expect_support(const Reference<T>& ref, const int npol)
{
    const int start[1]={0};
    auto pv=parallel(npol,start);
    hamilt::HContainer<T> h(pv.get());
    h.insert_pair(hamilt::AtomPair<T>(0,0,0,0,0,pv.get()));
    h.allocate(nullptr,true);
    // This is structural allocation before Veff/EXX contribution, not a test
    // that reallocating a populated numerical matrix must preserve its values.
    hamilt::reallocate_hcontainer(ref,&h,nullptr);
    EXPECT_NE(h.find_matrix(0,0,0,0,0),nullptr);
    EXPECT_NE(h.find_matrix(0,0,-1,0,0),nullptr);
    EXPECT_NE(h.find_matrix(0,0,1,0,0),nullptr);
}
}

TEST(ExxSupportAllocation, CollinearEmptyFirstChannelStillAllocatesSecondChannel)
{
    Reference<double> ref(2);
    auto local=matrix<double>(),nonlocal=matrix<double>();
    local(0,0)=local(1,1)=-.8;
    nonlocal(0,0)=nonlocal(1,1)=-.2;
    ref[1][0][{0,{{0,0,0}}}]=local;
    ref[1][0][{0,{{-1,0,0}}}]=nonlocal;
    ref[1][0][{0,{{1,0,0}}}]=nonlocal;
    expect_support(ref,1);
}

TEST(ExxSupportAllocation, SpinorOffDiagonalSupportCanDifferFromDiagonalSupport)
{
    using C=std::complex<double>;
    Reference<C> ref(4);
    auto uu=matrix<C>(),dd=matrix<C>(),ud=matrix<C>(),du=matrix<C>();
    // K(k)=-x x^dagger for x=(.8,0,0,.6i exp(ikR)) is Hermitian
    // negative semidefinite. Its diagonal maps have only R=0 support.
    uu(0,0)=-.64;
    dd(1,1)=-.36;
    ud(0,1)=C(0,.48);
    du(1,0)=C(0,-.48);
    ref[0][0][{0,{{0,0,0}}}]=uu;
    ref[3][0][{0,{{0,0,0}}}]=dd;
    ref[1][0][{0,{{-1,0,0}}}]=ud;
    ref[2][0][{0,{{1,0,0}}}]=du;
    expect_support(ref,2);
}

TEST(ExxSupportAllocation, ComputedZeroChannelsNeedNoAdditionalCells)
{
    const int start[1]={0};
    auto pv=parallel(2,start);
    Reference<std::complex<double>> ref(4);
    hamilt::HContainer<std::complex<double>> h(pv.get());
    h.insert_pair(hamilt::AtomPair<std::complex<double>>(0,0,0,0,0,pv.get()));
    h.allocate(nullptr,true);
    const auto count=h.get_nnr();
    hamilt::reallocate_hcontainer(ref,&h,nullptr);
    EXPECT_EQ(h.get_nnr(),count);
    EXPECT_EQ(h.find_matrix(0,0,-1,0,0),nullptr);
}

int main(int argc,char** argv)
{
#ifdef __MPI
    MPI_Init(&argc,&argv);
#endif
    testing::InitGoogleTest(&argc,argv);
    const int result=RUN_ALL_TESTS();
#ifdef __MPI
    MPI_Finalize();
#endif
    return result;
}
