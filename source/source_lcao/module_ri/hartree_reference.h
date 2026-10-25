#ifndef HARTREE_REFERENCE_H
#define HARTREE_REFERENCE_H

#include "source_base/element_basis_index.h"
#include <vector>

class UnitCell;
class Numerical_Orbital_Lm;
namespace ModuleBase { class matrix; }
namespace ModulePW { class PW_Basis; }

namespace HartreeReference
{
// 4*pi integral r^2 dr R_l(r) j_l(Gr), before the angular phase.
double radial_fourier(const Numerical_Orbital_Lm& orbital, const double gnorm);

// Bare periodic auxiliary metric in Hartree atomic units. The reciprocal
// vectors and pool communicator belong to the charge-density PW basis.
ModuleBase::matrix coulomb_q0(
    const UnitCell& cell,
    const ModulePW::PW_Basis& basis,
    const std::vector<std::vector<std::vector<Numerical_Orbital_Lm>>>& abfs,
    const ModuleBase::Element_Basis_Index::IndexLNM& index);
}

#endif
