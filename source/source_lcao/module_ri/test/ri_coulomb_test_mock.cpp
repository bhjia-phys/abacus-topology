// Minimal stubs so module_ri unit tests can construct a UnitCell without
// linking the full cell library. Mirrors the approach of
// source/source_lcao/test/tmp_mocks.cpp.
#include "source_cell/unitcell.h"

// constructor of Atom
Atom::Atom() {}
Atom::~Atom() {}

Atom_pseudo::Atom_pseudo() {}
Atom_pseudo::~Atom_pseudo() {}

Magnetism::Magnetism() {}
Magnetism::~Magnetism() {}

pseudo::pseudo() {}
pseudo::~pseudo() {}
SepPot::SepPot() {}
SepPot::~SepPot() {}
Sep_Cell::Sep_Cell() noexcept {}
Sep_Cell::~Sep_Cell() noexcept {}

// constructor of UnitCell
UnitCell::UnitCell() {}
UnitCell::~UnitCell() {}
