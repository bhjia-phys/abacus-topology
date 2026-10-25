#ifndef TWO_CENTER_POSITION_INTEGRATOR_H
#define TWO_CENTER_POSITION_INTEGRATOR_H

#include <memory>
#include "source_base/vector3.h"

class RadialCollection;
class TwoCenterIntegrator;

// Absolute position in the same real NAO representation as the supplied overlap.
// The active basis and overlap must outlive this local, noncopyable helper.
// Coordinate-multiplied moment radials remain unnormalized.
class TwoCenterPositionIntegrator
{
  public:
    TwoCenterPositionIntegrator(const RadialCollection& basis, const TwoCenterIntegrator& overlap);
    ~TwoCenterPositionIntegrator();
    TwoCenterPositionIntegrator(const TwoCenterPositionIntegrator&) = delete;
    TwoCenterPositionIntegrator& operator=(const TwoCenterPositionIntegrator&) = delete;

    ModuleBase::Vector3<double> relative_to_bra(
        int type1, int l1, int zeta1, int m1, int type2, int l2, int zeta2, int m2,
        const ModuleBase::Vector3<double>& ket_minus_bra_bohr) const;

    ModuleBase::Vector3<double> absolute(
        int type1, int l1, int zeta1, int m1, int type2, int l2, int zeta2, int m2,
        const ModuleBase::Vector3<double>& bra_position_bohr,
        const ModuleBase::Vector3<double>& ket_position_bohr) const;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
#endif
