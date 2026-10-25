#include "two_center_position_integrator.h"

#include "radial_collection.h"
#include "real_gaunt_table.h"
#include "two_center_integrator.h"
#include "source_base/constants.h"

#include <algorithm>
#include <cmath>
#include <map>
#include <stdexcept>
#include <tuple>
#include <vector>

namespace
{
using Channel = std::tuple<int,int,int,int>; // type, source l, source zeta, moment L
class MomentRadials : public RadialSet
{
  public:
    MomentRadials(const RadialSet& source, int type, std::map<Channel,int>& channels)
    {
        symbol_=source.symbol(); itype_=type; lmax_=source.lmax()+1;
        nzeta_=new int[lmax_+1]();
        for(int l=0;l<=source.lmax();++l)
            for(int z=0;z<source.nzeta(l);++z)
                for(int L=l-1;L<=l+1;L+=2) if(L>=0) ++nzeta_[L];
        nzeta_max_=*std::max_element(nzeta_,nzeta_+lmax_+1);
        for(int L=0;L<=lmax_;++L) nchi_+=nzeta_[L];
        indexing(); chi_=new NumericalRadial[nchi_];
        std::vector<int> next(lmax_+1,0);
        for(int l=0;l<=source.lmax();++l)
            for(int z=0;z<source.nzeta(l);++z)
            {
                const auto& original=source.chi(l,z);
                if(original.pr()!=0.0 || !original.is_fft_compliant())
                    throw std::invalid_argument("Coordinate moments require explicit pr=0 active FFT radials");
                std::vector<double> values(original.nr());
                for(int ir=0;ir<original.nr();++ir)
                    values[ir]=original.rgrid(ir)*original.rvalue(ir);
                for(int L=l-1;L<=l+1;L+=2)
                {
                    if(L<0) continue;
                    const int mz=next[L]++;
                    auto& moment=chi_[index(L,mz)];
                    moment.set_transformer(original.sbt());
                    moment.build(L,true,original.nr(),original.rgrid(),values.data(),
                                 0,mz,symbol_,type,false);
                    moment.set_grid(false,original.nk(),original.kgrid(),'t');
                    // No normalization: this is r*R, not a replacement orbital.
                    channels.emplace(Channel(type,l,z,L),mz);
                }
            }
        set_rcut_max();
    }
    MomentRadials(const MomentRadials& other):RadialSet(other) {}
    MomentRadials* clone() const override { return new MomentRadials(*this); }
};
void check_orbital(const RadialCollection& basis,int type,int l,int zeta,int m)
{
    if(type<0 || type>=basis.ntype() || l<0 || l>basis.lmax(type)
       || zeta<0 || zeta>=basis.nzeta(type,l) || std::abs(m)>l)
        throw std::invalid_argument("Invalid active position orbital index");
}
void check_vector(const ModuleBase::Vector3<double>& v)
{
    if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z))
        throw std::invalid_argument("Nonfinite active position geometry");
}
}

struct TwoCenterPositionIntegrator::Impl
{
    Impl(const RadialCollection& b,const TwoCenterIntegrator& s):basis(b),overlap(s) {}
    const RadialCollection& basis;
    const TwoCenterIntegrator& overlap;
    RadialCollection moments;
    std::map<Channel,int> channels;
    TwoCenterIntegrator mixed;
};

TwoCenterPositionIntegrator::TwoCenterPositionIntegrator(
    const RadialCollection& basis,const TwoCenterIntegrator& overlap):impl_(new Impl(basis,overlap))
{
    if(basis.ntype()<=0 || basis.nchi()<=0 || overlap.operation()!='S'
       || overlap.radial_grid_size()<3 || overlap.radial_grid_cutoff()<=0)
        throw std::invalid_argument("Active position needs a nonempty matching overlap/basis");
    const auto& first=**basis.cbegin();
    for(auto it=basis.cbegin();it!=basis.cend();++it)
    {
        const auto& radial=**it;
        if(radial.pr()!=0.0 || !radial.is_fft_compliant() || radial.nr()!=first.nr()
           || radial.nk()!=first.nk()
           || !std::equal(radial.rgrid(),radial.rgrid()+radial.nr(),first.rgrid())
           || !std::equal(radial.kgrid(),radial.kgrid()+radial.nk(),first.kgrid()))
            throw std::invalid_argument("Active position requires common explicit FFT radial grids");
    }
    auto& moments=impl_->moments;
    // Keep a destructible empty collection if the pointer-array allocation fails.
    moments.radset_=new RadialSet*[basis.ntype()](); moments.ntype_=basis.ntype();
    for(int type=0;type<basis.ntype();++type)
    {
        moments.radset_[type]=new MomentRadials(basis(type),type,impl_->channels);
        const auto& set=*moments.radset_[type];
        moments.lmax_=std::max(moments.lmax_,set.lmax());
        moments.nzeta_max_=std::max(moments.nzeta_max_,set.nzeta_max());
        moments.nchi_+=set.nchi();
    }
    moments.set_rcut_max(); moments.iter_build();
    impl_->mixed.tabulate(moments,basis,'S',overlap.radial_grid_size(),overlap.radial_grid_cutoff());
}
TwoCenterPositionIntegrator::~TwoCenterPositionIntegrator()=default;

ModuleBase::Vector3<double> TwoCenterPositionIntegrator::relative_to_bra(
    int type1,int l1,int zeta1,int m1,int type2,int l2,int zeta2,int m2,
    const ModuleBase::Vector3<double>& d) const
{
    check_orbital(impl_->basis,type1,l1,zeta1,m1);
    check_orbital(impl_->basis,type2,l2,zeta2,m2); check_vector(d);
    ModuleBase::Vector3<double> result(0,0,0);
    const int component_m[3]={1,-1,0};
    const double signs[3]={-1,-1,1};
    const double factor=std::sqrt(ModuleBase::FOUR_PI/3.0);
    for(int L=l1-1;L<=l1+1;L+=2)
    {
        if(L<0) continue;
        const int mz=impl_->channels.at(Channel(type1,l1,zeta1,L));
        for(int M=-L;M<=L;++M)
        {
            double overlap=0;
            impl_->mixed.calculate(type1,L,mz,M,type2,l2,zeta2,m2,d,&overlap);
            for(int axis=0;axis<3;++axis)
                result[axis]+=signs[axis]*factor*
                    RealGauntTable::instance()(l1,1,L,m1,component_m[axis],M)*overlap;
        }
    }
    return result;
}
ModuleBase::Vector3<double> TwoCenterPositionIntegrator::absolute(
    int type1,int l1,int zeta1,int m1,int type2,int l2,int zeta2,int m2,
    const ModuleBase::Vector3<double>& a,const ModuleBase::Vector3<double>& b) const
{
    check_vector(a); check_vector(b);
    const auto d=b-a;
    auto result=relative_to_bra(type1,l1,zeta1,m1,type2,l2,zeta2,m2,d);
    double overlap=0;
    impl_->overlap.calculate(type1,l1,zeta1,m1,type2,l2,zeta2,m2,d,&overlap);
    return result+a*overlap;
}
