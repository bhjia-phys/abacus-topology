# HSE cross-feature calibration record v3 (2026-07-31)

Workspace: /home/bhjia/physics/GW_librpa
Binary: build-merge/abacus_std_para @ HEAD 269ac8a8ec48237797d1929142ececbc7d2c6dbc
Binary SHA-256: d82ce36ea23c5a14511d425dfca734b791d3f9ba0976cf27d746551cba815525
LibXC: local 5.1.7 (ENABLE_LIBXC=ON, Libxc_DIR=/usr/share/cmake/Libxc)
Scratch: /tmp/abacus-hse-calib2.iUMasS

## ARCHITECTURE FINDING (critical, verified against master_ghj parent)

In BOTH the merged tree and the pinned master_ghj parent (dd4216653):

- HSE (dft_functional=hse) sets ccp_type=Erfc. In
  `RI_Util::update_coulomb_settings`, Coulomb_Type::Erfc is ALWAYS routed to
  the Center2 method ("Erfc is always calculated with Center2 method"). It
  never produces Coulomb_Method::Ewald.
- `Exx_LRI::use_rotated_n0_long_range = rotate_abfs && coul_moment && (Ewald
  present in coulomb_settings)`. For HSE the Ewald method is never present,
  so use_rotated_n0_long_range = false => **split Ewald is NOT active for
  HSE**, regardless of exx_singularity_correction=massidda.
- HF/PBE0 (ccp_type=Hf/Fock) with massidda/carrier DOES produce the Ewald
  method => split Ewald IS active (markers "Rotated ABFS long-prefix sizes
  by type:" and "Construct Ewald bare Coulomb blocks directly..." appear).
- Verified identical logic in master_ghj parent source
  (source/source_lcao/module_ri/RI_Util.hpp and Exx_LRI.hpp). This is an
  inherited master_ghj behavior, NOT a merge regression.

Consequences:

- handoff section 9.3's marker-bearing cross case was HF (dft_functional=hf),
  not HSE. Reproduced exactly: E_exx=-22.6133091022 Ry,
  total=-3296.003281913835 eV (handoff recorded ...857, float noise),
  markers T0=25 present, CSR validator passes, C_4h + 8 antiunitary, 3 IBZ.
- handoff section 12.2's HSE case (old scratch) has NO split markers, i.e. it
  was a non-split HSE run. My HSE calibrations likewise have no markers.
- An HSE force/stress request does NOT hit the intentional rejection (since
  split Ewald is not active for HSE). The rejection message is only reachable
  with HF/PBE0 (Fock channel) + massidda + coul_moment + rotate_abfs.
  Verified: HF force-reject exits 134 with exact message
  "Rotated-ABFS split Ewald currently supports energy/SCF only."
- handoff 12/13's premise (HSE + active split Ewald) is not satisfiable in
  this tree; the split-Ewald cross gate must use HF or PBE0 for the positive
  markers, while HSE remains valid for the HSE+LibXC+SOC+magnetic-symmetry
  convergence/H(R) requirements.

## Fe metallic chassis (ecutwfc=5, 2x2x1, smearing 0.02, nspin=4, SOC)

### HSE symm (58_KP_HSE_SOC_EWALD_symm, INPUT 5153a2e5...)
- CONVERGED, final drho=4.06345e-06; E_exx=-7.3803607575 Ry;
  E_tot=-3418.325412172493 eV; mag=[0,0,1.49056]
- 3 IBZ k (0.25/0.5/0.25), C_4h, 8 antiunitary, 8 magnetic space ops
- CSR: dim 54, 89 R-blocks, 4 spin blocks nonzero, folded hermit 4.0e-9,
  k-mesh hermit 4.7e-9
- NO split-Ewald markers (architecture: HSE Erfc -> Center2)

### HSE nosymm (symmetry=-1)
- CONVERGED, final drho=5.88064e-06; E_exx=-7.3850804949 Ry;
  E_tot=-3427.404805103829 eV; mag=[~0,~0,1.49128]
- CSR passes (1.0e-9 / 3.0e-9); NO split markers

### Gap analysis (9.08 eV, E_exx gap 0.0047 Ry) => INHERITED, not regression
1. PBE control (no EXX at all): symm -3420.408048142737 vs
   nosymm -3426.376735130781 (~6 eV gap). No Exx_LRI init at all (1.41s).
2. Independent build (repo/build/abacus-mag-group-oneapi2026.1/abacus_3p,
   non-merge maki-style port): reproduces PBE results bitwise
   (-3420.408048142737 / -3426.376735130779).
3. Fixed-density nscf at the same density: symm=1 vs symm=-1 total energy
   identical to 1e-9 eV; E_band/Hartree/XC/Ewald bitwise identical; S(R)
   byte-identical; H(R) max diff 1.0e-15 (float noise); R-block sets equal.
   => H(k)/H(R) construction is fully equivalent under symmetry on/off.
4. No-split HSE control (coul_moment=0, rotate_abfs=0, massidda kept):
   symm -3418.327613028233, nosymm -3427.404805125151 (gap persists);
   E_exx split-vs-nosplit agree to 1e-5 Ry; E_Ewald bitwise identical.
5. 4x4x1 PBE: symm NOT converged (mag collapses 0.35), nosymm 0.18 => input
   ill-conditioned.
6. 2x2x2 PBE: symm mag +1.23194, nosymm -0.434481 (state flip), gap ~6 eV.

Conclusion: ~9 eV symm/nosymm gap on Fe metal at coarse params = inherited
multi-fixed-point SCF behavior (PROJECT_MEMORY: metals at 4^3 k already show
2-3 meV sym-vs-nosym fixed-point differences intrinsic to the maki-style
implementation). Not a merge regression. H(R) construction proven equivalent.

## NiO AFM chassis (insulating; Dojo-NC-FR Ni/O pseudopotentials)

- PBE symm -10027.64612866746 eV, nosymm -10027.64498467752 eV
  (fish records -10027.646129 / -10027.644985; delta 1.1436 meV known
  dual-minimum, documented in PROJECT_MEMORY).
- HSE (massidda + coul_moment + rotate_abfs): EXX update blows up magnetic
  moment (7-8 uB) and energy jumps ~2800 eV; NOT convergent. Same in no-split
  (rotate_abfs=0). NiO HSE is not a viable oracle chassis on this machine;
  also its STRU moments are collinear AFM (0,0,+-1.8), not the explicit
  noncollinear moment required by section 8.
- NiO abandoned as oracle chassis.

## Positive split-Ewald evidence (HF / PBE0, Fock channel)

### HF probe (dft_functional=hf, massidda, coul_moment=1, rotate_abfs=1,
### scf_thr=1, scf_nmax=4 -- same as handoff 9.3)
- markers: "Rotated ABFS long-prefix sizes by type: T0=25" and
  "Construct Ewald bare Coulomb blocks directly..." (present)
- E_exx=-22.6133091022 Ry; total=-3296.003281913835 eV (handoff: ...857)
- CSR validator passes (4 spin blocks, folded hermit 1e-13, k hermit 2e-13)
- C_4h + 8 antiunitary + 3 IBZ (magnetic group intact)
- loose-threshold run only; handoff 12.1 records strict HF instability.

### HF force-reject (cal_force=1, cal_stress=1)
- exits 134 (SIGABRT) with exact message
  "Rotated-ABFS split Ewald currently supports energy/SCF only." (verified)

### PBE0 (dft_functional=pbe0, separate_loop=1, hybrid_step=2, massidda,
### coul_moment=1, rotate_abfs=1, scf_thr=1e-5)
- markers present; CONVERGED; E_exx=-6.1482294301 Ry;
  E_tot=-3387.234771366142 eV; C_4h + 8 antiunitary; CSR passes.
- BUT magnetization collapses to ~1e-4 uB during the EXX loop (both symm and
  nosymm). Not usable as the magnetic cross chassis.

## Design decision for the cross gate

- Positive HSE/SOC/magnetic-symmetry + convergence + four-spinor + H(R):
  HSE case (section 12 requirements), documented as non-split (split inactive
  for HSE by inherited architecture). HSE symm/nosymm both converge; CSR
  validators pass; magnetic group active; H(R) equivalence across symmetry
  proven at fixed density (1e-9 eV).
- Positive split-Ewald energy/SCF/H(R) markers + negative force rejection:
  HF case (matches handoff 9.3 exactly). This is the only functional that
  activates the split path while preserving the magnetic moment.
- The cross archive must accurately name cases: HSE cases must NOT be labeled
  "EWALD"; the HF case carries the split-Ewald markers.
