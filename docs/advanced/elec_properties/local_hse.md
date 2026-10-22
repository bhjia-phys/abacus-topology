# Local short-range interaction integrals

This opt-in LCAO postprocessing calculates the unscaled short-range interaction
of explicitly supplied orthonormal localized orbitals. It reuses the EXX radial
integral machinery and LibRI pair contractions. It runs after SCF/NSCF and
does not change the electronic Hamiltonian, occupations or SCF functional.
No response function or screened interaction is computed.

## Build and input

Build with `ENABLE_LCAO=ON`, `ENABLE_MPI=ON`, `ENABLE_LIBRI=ON`, and `LIBRI_DIR`
pointing to a LibRI checkout containing `include/RI/physics/LocalInteraction.h`.
Older LibRI checkouts retain their existing functionality; requesting these
outputs with such a build fails with an explicit dependency message.
The default automatically downloaded LibRI does not contain this extension.
The corresponding published dependency is
[bhjia-phys/LibRI at c493f439](https://github.com/bhjia-phys/LibRI/tree/c493f439eefcbc4f2bce345735b5f77ddfdc7610).
With the [standard build prerequisites](../../quick_start/easy_install.md)
available, an example source/build sequence is:

```bash
git clone --branch feature/local-hse-uj-20261021 https://github.com/bhjia-phys/LibRI.git LibRI-local-hse
git -C LibRI-local-hse checkout c493f439eefcbc4f2bce345735b5f77ddfdc7610
git clone --branch feature/local-hse-uj-20261021 https://github.com/bhjia-phys/abacus-topology.git abacus-local-hse
cmake -S abacus-local-hse -B abacus-local-hse/build \
  -DENABLE_LCAO=ON -DENABLE_MPI=ON -DENABLE_LIBRI=ON \
  -DLIBRI_DIR="$PWD/LibRI-local-hse"
cmake --build abacus-local-hse/build --target abacus_std_para -j4
```

Supply any compiler and dependency paths required by your normal ABACUS build.
Configuration should print `LibRI local-interaction integrals available`.

Add these optional parameters to a valid SCF or NSCF LCAO `INPUT`:

```text
local_hse_projectors  local-projectors.dat
local_hse_omega       0.11
local_hse_alpha       0.25
```

The projector path enables the calculation. `local_hse_omega` is positive and
finite, in Bohr$^{-1}$, and `local_hse_alpha` lies in $[0,1]$. They are independent
of the SCF functional: for example, the postprocessing can run on a PBE SCF
calculation without enabling HSE exchange in that SCF calculation.

## Explicit local orbitals

The projector file supplies a finite expansion in the actual spatial NAOs:

$$
w_m(\mathbf r)=\sum_{I\mathbf R i}
T_{I\mathbf R i,m}\,\chi_{Ii}(\mathbf r-\mathbf R).
$$

Its text format is:

```text
ABACUS_LOCAL_HSE_PROJECTORS 1
n_orbitals 2
n_entries 2
# orbital atom Rx Ry Rz local_ao real imag
0 0 0 0 0 0 1.0 0.0
1 0 0 0 0 1 1.0 0.0
```

All orbital, atom and local spatial AO indices are zero-based. Atoms follow
the internal species/atom ordering of `STRU`; local AOs follow the NAO ordering
on that atom. Integer cell labels are unwrapped translations of the three
lattice vectors. A line gives one nonzero complex coefficient. Omitted
coefficients are zero; duplicate entries are rejected. `#` introduces a comment.
The example selects two home-cell NAOs and is valid only if those particular
orbitals are orthonormal. It is a format example, not a general prescription
for selecting a correlated subspace.

The supplied coefficients define the final orbital frame. The native overlap
integrator checks $T^\dagger S_{\mathrm{support}}T=\mathbf1$, with maximum
elementwise error at most $10^{-6}$; it does not change
the frame by an automatic orthogonalization. Multiple zetas, neighboring sites
and translations generally have nonzero overlap. Provide the coefficients of
the intended normalized orbitals, with adequate finite support, rather than
assuming that an arbitrary selection of NAOs is orthonormal.

Each file defines one local orbital set. Its orbitals may have tails on
different atoms and explicit cells. They are spatial orbitals; the integral
calculation does not infer a spin-dependent or spinor projector from an SCF
wavefunction. Matching a particular Wannier or DMFT projector is the caller's
responsibility.
The parser accepts at most 1024 orbitals and 1,000,000 sparse entries. The
complete output tensor contains $M^4$ entries, so keep the selected local set
small enough for the available memory.

## Runnable hydrogen p-shell example

The [example inputs](../../../examples/local_hse/INPUT) use the repository's
`H_ONCV_PBE-1.0.upf` and `H_gga_6au_60Ry_2s1p.orb` in `tests/PP_ORB`, one H
atom in a 20-Bohr cubic cell, and a PBE SCF calculation. In that particular
basis, local AO indices 2, 3 and 4 are the three normalized p functions.
Its [projector file](../../../examples/local_hse/projectors.dat) is:

```text
ABACUS_LOCAL_HSE_PROJECTORS 1
n_orbitals 3
n_entries 3
# orbital atom Rx Ry Rz local_ao real imag
0 0 0 0 0 2 1 0
1 0 0 0 0 3 1 0
2 0 0 0 0 4 1 0
```

Run from the example directory so the relative basis paths resolve:

```bash
cd abacus-local-hse/examples/local_hse
OMP_NUM_THREADS=1 MKL_NUM_THREADS=1 mpirun -np 2 ../../build/abacus_std_para
```

For the supplied basis and settings, a native implementation control gives
approximately `Ubar_SR = 0.4948028608`, `Jbar_SR = 0.0369011110`, and
`C_HSE = 0.1144754374`, all in Hartree. These unoccupied atomic p functions
provide a small usage/control example, not material interaction parameters.

## Interaction and output conventions

The kernel is a fresh pure $v_{\mathrm{SR}}(r)=\mathrm{erfc}(\omega r)/r$
with unit prefactor. It uses the finite short-range zero-mode limit and does
not inherit a mixture of full/SR kernels or the SCF hybrid fraction.
The RI coefficients and auxiliary metric are contracted in explicit real
space, including both LRI endpoints and unwrapped auxiliary cells. No Gamma
image sum or density-matrix contraction is substituted for the local integral.
The native builder evaluates matching SR three-center and two-center
integrals, so its coefficients are an SR-metric fit of the AO products.
They are not assumed identical to coefficients fitted with the full Coulomb
metric. The calculation uses the EXX auxiliary-generation and inverse-metric
settings, including `exx_pca_threshold`, `exx_cs_inv_thr` and
`exx_ccp_rmesh_times`, but does not apply the EXX C/V screening thresholds
to the requested raw blocks.
The native integral builder still applies radial support cutoffs. In
particular, `exx_ccp_rmesh_times` controls the range of the convolved auxiliary
potential; increase and converge it for distant support cells. Its existing
default depends on the SCF functional, so set it explicitly when comparing
different functional starts at a fixed local SR kernel. Bypassing amplitude
screening does not remove these radial-cutoff errors.
The default unregularized inverse also assumes a well-conditioned auxiliary
fit. A nearly linearly dependent auxiliary set can yield very large finite
integrals without a Cholesky failure. Check the basis and converge an explicit
positive `exx_cs_inv_thr` when needed; that setting selects the native
eigenvalue-filtered inverse. A successful exit and finite output alone do not
establish fit accuracy. The local service preserves this explicit native
precision choice rather than silently adding a regularization threshold.

The integral convention is bilinear in the two pair densities:

$$
I^{\mathrm{SR}}_{mn,pq}=\iint
w_m^*(\mathbf r)w_n(\mathbf r)\,
v_{\mathrm{SR}}(\mathbf r-\mathbf r')\,
w_p^*(\mathbf r')w_q(\mathbf r')\,d\mathbf r\,d\mathbf r'.
$$

The reported direct and exchange matrices are
$U_{mn}^{\mathrm{SR}}=I^{\mathrm{SR}}_{mm,nn}$ and
$J_{mn}^{\mathrm{SR}}=I^{\mathrm{SR}}_{mn,nm}$, in Hartree. For $M>1$,

$$
\overline U^{\mathrm{SR}}=\frac1{M^2}\sum_{mn}U_{mn}^{\mathrm{SR}},\qquad
\overline J^{\mathrm{SR}}=\frac1{M(M-1)}\sum_{m\ne n}J_{mn}^{\mathrm{SR}},
\qquad C_{\mathrm{HSE}}=\alpha(\overline U^{\mathrm{SR}}-\overline J^{\mathrm{SR}}).
$$

The hybrid fraction scales only $C_{\mathrm{HSE}}$; the tensor and U/J remain
unscaled. For a one-orbital set, the off-diagonal exchange average and this
definition of $C_{\mathrm{HSE}}$ are undefined; the direct/exchange integrals
are still meaningful. The diagonal direct average is a separate quantity
from $\overline U$.

Results are written to `OUT.<suffix>/local_hse.dat`. The text header records
the kernel, units, range parameter, hybrid fraction, RI precision settings
and maximum overlap error. Sections `I_SR` and `U_J_SR` contain zero-based
orbital indices and complex values. `Ubar_SR`, `Jbar_SR` and `C_HSE` appear
when the orbital set has more than one member. The exact supplied projector
coefficients are included in the `PROJECTORS` section.

These are finite-support RI interaction integrals. Their numerical accuracy
depends on the AO/auxiliary bases, radial meshes and retained orbital support.
They do not supply a full projected HSE potential, a density-functional
double-counting choice, or a DMFT calculation.
