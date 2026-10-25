# Periodic Hartree reference output

For LCAO calculations with one charge-density grid (the norm-conserving route), `out_mat_h_vh 1` writes the Hartree-only AO operator
from the current charge density. The real-space `vh*nao.csr` files and dense
complex `vhk*nao.txt` files use Ry. The Fourier convention is

\[
 V^H_{ij}(k)=\sum_R e^{+2\pi i k\cdot R} V^H_{ij}(R).
\]

The PW Hartree builder sums the physical spin densities, applies
`8*pi/G^2` in Ry for nonzero reciprocal vectors, and sets the `G=0`
potential to zero. Its forward FFT is `sum_r rho(r) exp(-iG.r)/Ngrid`.
The AO matrix is obtained by native grid integration of this potential.
This output contains the complete AO operator, rather than band expectation
values. Spin-collinear channels receive the same Hartree potential; this
output option does not support `nspin=4` or separate smooth/dense charge grids.

When this reference option is enabled, any requested H(R), S(R), and r(R)
CSR exports retain every nonzero native matrix entry. The same zero threshold
also applies to any concurrently requested T(R), dH/dR, and dS/dR exports
through the shared sparse exporter. Other calculations keep the existing
`1e-10` CSR sparsity threshold. This is an output precision
choice and does not change the Hamiltonian or its eigenvectors. Small AO
entries can matter after projection through an ill-conditioned NAO basis;
requesting more printed digits alone does not prevent their removal. Use
`out_mat_hs2 1 16` and `out_mat_r 1 16` for full-precision reference text.

A tested producer setup uses the following INPUT options together with a
Gamma-centered k mesh and the desired RI fitting settings:

```text
calculation scf
basis_type lcao
symmetry -1
rpa 1
out_librpa_reader_version 1
out_mat_h_vh 1
exx_singularity_correction massidda
```

Disabling symmetry here exports the complete regular SCF grid required by
the first LibRPA Hartree input path, including Gamma. The tested route is
scalar or collinear LCAO with norm-conserving pseudopotentials.

With `rpa 1`, this option additionally writes
`v1_coulomb_hartree_iq_<Gamma index>_rank0.dat` in the current directory.
Only the Gamma block is written. It uses the existing LibRPA reader-v1
binary Coulomb format, including per-atom auxiliary sizes and upper atom-pair
blocks. A Gamma-centered k mesh is required. All PW ranks in each pool
participate in the metric reduction; the world root writes one complete file,
so replicated k-point pools do not multiply the result.

It also writes `v1_Cs_hartree_shrinked_data_<rank>.txt` for the selected
shrink auxiliary basis, and `v1_Cs_hartree_data_<rank>.txt` for the parent
basis. Without auxiliary shrinkage, only the latter is written. Despite
the `.txt` suffix, these are full-precision LibRPA reader-v1 binary C files.
They retain each physical lattice displacement before Born--von Karman
folding and before the EXX `post_2D` redistribution. The original unique
real-space task ownership is retained across the rank files. Use the
selected-basis file together with the matching dedicated auxiliary metric
when evaluating Hartree operators away from the SCF k mesh. The ordinary
folded C files retain their existing definitions and remain available for GW.

This dedicated auxiliary metric uses Hartree atomic units and the same PW
reciprocal set as the charge-density Hartree calculation:

\[
 V^{H,\mathrm{aux}}_{\mu\nu}(0)
 =\frac{1}{\Omega}\sum_{G\ne0}\frac{4\pi}{G^2}
       \widetilde P_\mu(G)^*\widetilde P_\nu(G).
\]

The full Fourier transform of an atom-centered real auxiliary orbital is

\[
 \widetilde P_{a l n m}(G)
 =4\pi(-i)^l e^{-iG\cdot\tau_a}Y_{lm}(\widehat G)
    \int_0^\infty r^2dr\,P_{aln}(r)j_l(Gr).
\]

For ABACUS's symmetric radial Bessel transform this is equivalently
`(2*pi)^(3/2) (-i)^l psif(G) Y_lm(Ghat) exp(-iG.tau)`.
The implementation evaluates the radial integral directly with the native
Bessel and Simpson routines. It uses the actual auxiliary orbitals supplied to the Spencer C fit, in
canonical `(l,n,m)` order, including the selected shrink basis. A separate
Ewald long-prefix permutation is not applied to this metric; the C export
does not use that permutation. For a Gamma-only FFT,
an omitted `-G` partner contributes the conjugate of its stored `+G` partner;
the sum uses twice the real part away from the unreduced reciprocal plane.

The existing `v1_coulomb_full*` and `v1_coulomb_cut*` files retain their
previous definitions. The full Ewald route contains an EXX singularity
correction, and the cut route uses a spherical Coulomb truncation. Neither
defines this reference's explicit `G=0` omission. No gauge adjustment or
fitted constant is used in the dedicated reference.

For a same-density comparison, use a converged SCF result. A stopped,
unconverged SCF calculation can contain a mixed grid density and an unmixed
density matrix. Even at convergence, ABACUS rescales its grid density by
`nelec / sum_rho` after constructing it from the density matrix; the
pre-normalization charge is printed in `warning.log`. An RI calculation
using exported eigenvectors and occupations must account for this factor
or quantify its residual. Native AO reference values must be divided by two
to compare in Ha; raw RI Coulomb exports already use Ha. RPA KS eigenvectors
are exported as their actual complex coefficients without conjugation.

Agreement remains limited by RI fitting, auxiliary shrinkage, reciprocal and
real-space grids, and native AO integration. Increasing the PW cutoff tests
one of these errors; it does not establish convergence of all the others.
