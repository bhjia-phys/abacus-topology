# Merge audit: master_ghj x soc-sym-mag-final

Audit date: 2026-07-30

Verdict: `LOCAL_PASS_FISH_PENDING` under the corrected scope in
`MERGE_EXECUTION_PLAN.md`.

The original draft Prompt B was not executable as written. The blockers were
removed by separating existing force/stress preservation from unsupported
split-Ewald force/stress, requiring the exact split-Ewald trigger, correcting
case selection, and expanding review to all actual conflict modules.

The exact local checkpoint covered by the latest evidence is
`872a46c8b34ed7b6a8f1582986141798f29435f8`. Its parent is the real two-parent
merge commit `2c462275e25b05bde4f520913d99774d1fe41ad5`, whose parents, in order,
are the pinned SOC chassis and `master_ghj` revisions listed below. No remote
branch has been updated.

## Pinned revisions

| Role | Revision |
|---|---|
| master_ghj | `dd4216653386d32f79e3219f3ea5dd2d229c1c5a` |
| soc-sym-mag-final | `4aa46ed65caf72fca593bf1a99a4e1705526f1a8` |
| merge base | `b4945b633dc24f6ff407ca561d31393229702dc5` |
| master-only commits | 49 |
| SOC-only commits | 316 |

Remote `ls-remote` was repeated before creating the integration clone and
matched these tips.

`git log --left-right --cherry-pick` found no commit-level patch-equivalent
pair. There are 121 paths changed on both sides. Patch-equivalence therefore
must be established at function and behavior level.

## Classification

| Subsystem | Class | Decision and evidence |
|---|---:|---|
| EXX configuration | E | Keep standalone `Exx_Info_RI` and by-value `Exx_LRI::info` from the chassis; add missing master fields and their complete parser-to-runtime chain. |
| Ewald short/long x SOC | E | Master has split channels but restores each spin independently; chassis has correct four-component `restore_HR_nspin4`. Implement one four-map restore per Coulomb channel. |
| Full-k MPI | C | Chassis allocates/broadcasts `nkstot_full` but fills the buffer inside the reduced-mesh loop; port master's independent `nkstot_full` fill and reconstruction loops. |
| DM mixing | D | Keep target `Mix_DMk_2D<T>` explicit instantiations and lifetime/restart design; do not restore legacy `Mix_Matrix` dependency without an independent caller. |
| ABF symmetry | E | Both tips have ABF rotation pieces. Master uniquely has `restore_HR_abf`; adapt it to chassis `nanti_` indexing and define/test scalar antiunitary conjugation. |
| RPA/LibRPA v1 output | C/E | Port master v1 output and validators, but remove the header-defined global `Exx_LRI<double> exx_lri_rpa` and use explicit ownership. |
| Gaussian/singular/shrink | B/D | Chassis already has modern equivalents in several files; preserve its module boundaries and port only demonstrably missing weighted/Ewald behavior. |
| Basis permutation | C | Port `IndexPermutation` overload with identity, nontrivial, size, duplicate, and range tests. |
| CMake/test macros | D/E | Keep chassis feature-disable conventions, union the live tests, and add only missing source/test targets. |
| Hsolver/H-S/restart/operators | E | These glue layers are real conflicts and must preserve both SOC/spinor formats and master RPA/EXX interfaces. |

## Function-level conflict evidence

- Chassis `Exx_LRI.h` accepts standalone `Exx_Info_RI` and stores
  `Exx_Info_RI info`; master accepts `Exx_Info::Exx_Info_RI` and stores a
  reference.
- Chassis contains `nanti_`, `magnetic_nspin4_`, `spin_U_`, and
  `restore_HR_nspin4`; master contains active `TRS_first_`.
- Master `cal_exx_elec` invokes ordinary `restore_HR` in a per-spin
  `run_exx_channel` lambda for both short and long suffixes.
- Master activates split Ewald only when `rotate_abfs && coul_moment &&
  Ewald`; it explicitly throws for force/stress in this path.
- Chassis already contains `set_rotation_matrix_abf`, but master additionally
  contains `restore_HR_abf` and `rotate_atompair_serial_abf`.
- Chassis `Mix_DMk_2D.cpp` explicitly instantiates real and complex templates;
  master routes through `Mix_Matrix`.
- Master `RPA_LRI.h` defines `Exx_LRI<double> exx_lri_rpa(...)` at namespace
  scope in the header.
- Master uses `nkstot_full` for the complete full-k fill and reconstruction
  loops. The chassis fills the auxiliary full-k buffer in the reduced loop and
  later broadcasts `nkstot_full * 3`.

## Predicted textual conflicts

The pinned three-way `git merge-tree` contains 48 paths with conflict markers:

```text
source/source_cell/klist.h
source/source_hamilt/module_xc/exx_info.h
source/source_hamilt/module_xc/xc_functional.cpp
source/source_hsolver/hsolver_lcao.cpp
source/source_io/module_hs/single_R_io.cpp
source/source_io/module_hs/write_HS_sparse.cpp
source/source_io/module_hs/write_vxc_r.hpp
source/source_io/module_output/csr_reader.cpp
source/source_io/module_parameter/input_conv.cpp
source/source_io/module_parameter/input_parameter.h
source/source_io/module_restart/restart_exx_csr.hpp
source/source_io/test/for_testing_input_conv.h
source/source_io/test/support/INPUT
source/source_lcao/hamilt_lcao.cpp
source/source_lcao/module_operator_lcao/ekinetic.cpp
source/source_lcao/module_operator_lcao/op_exx_lcao.hpp
source/source_lcao/module_operator_lcao/overlap.cpp
source/source_lcao/module_ri/CMakeLists.txt
source/source_lcao/module_ri/Exx_LRI.h
source/source_lcao/module_ri/Exx_LRI.hpp
source/source_lcao/module_ri/Exx_LRI_interface.hpp
source/source_lcao/module_ri/LRI_CV.h
source/source_lcao/module_ri/LRI_CV.hpp
source/source_lcao/module_ri/LRI_CV_Tools.h
source/source_lcao/module_ri/LRI_CV_Tools.hpp
source/source_lcao/module_ri/Matrix_Orbs11.cpp
source/source_lcao/module_ri/Matrix_Orbs21.cpp
source/source_lcao/module_ri/Matrix_Orbs22.cpp
source/source_lcao/module_ri/Mix_DMk_2D.cpp
source/source_lcao/module_ri/RI_2D_Comm.h
source/source_lcao/module_ri/RI_2D_Comm.hpp
source/source_lcao/module_ri/RI_Util.hpp
source/source_lcao/module_ri/RPA_LRI.h
source/source_lcao/module_ri/RPA_LRI.hpp
source/source_lcao/module_ri/ewald_Vq.h
source/source_lcao/module_ri/ewald_Vq.hpp
source/source_lcao/module_ri/exx_rotate_abfs.h
source/source_lcao/module_ri/exx_rotate_abfs.hpp
source/source_lcao/module_ri/gaussian_abfs.cpp
source/source_lcao/module_ri/gaussian_abfs.h
source/source_lcao/module_ri/module_exx_symmetry/symmetry_rotation.h
source/source_lcao/module_ri/module_exx_symmetry/symmetry_rotation_output.cpp
source/source_lcao/module_ri/singular_value.cpp
source/source_lcao/module_ri/singular_value.h
source/source_lcao/module_ri/test/CMakeLists.txt
source/source_lcao/module_ri/test/ri_cv_io_test.cpp
source/source_lcao/spar_hsr.cpp
tests/08_EXX/CASES_CPU.txt
```

Files changed on both sides without textual markers remain mandatory semantic
review targets, including `k_vector_utils.cpp`, `klist.cpp`,
`read_input_item_exx_dftu.cpp`, `Exx_LRI_interface.h`, `RI_2D_Comm.cpp`,
`conv_coulomb_pot_k.*`, `Inverse_Matrix.hpp`, and the affected tests/docs.

## Review corrections to the two prompts

1. Prompt A cannot simultaneously prohibit all writes and require
   `MERGE_AUDIT.md`; the audit report is its only allowed tracked write.
2. The AroundPeking remote is selected by URL, not assumed to be `origin`.
3. `rerere.autoupdate` is false for manual staging of semantic conflicts.
4. `git merge ... || true` is replaced by explicit exit/MERGE_HEAD/index
   checks.
5. Individual 08_EXX cases are selected with `Autotest.sh -r`; they are not
   separate CTest registrations.
6. A Massidda input alone does not prove the split path. The test must also
   enable ABF rotation and Coulomb moments.
7. Existing split Ewald is energy/SCF only. Existing non-split force/stress
   remains mandatory, while split force/stress is an explicit negative test.
8. RPA cases 56 and 57 require output-schema/content validation in addition to
   `result.ref`.
9. The final suite is the affected registered-test union from both parents.
10. Fish compilation and numerical work is submitted via Slurm in a new
    immutable directory; the dirty historical fish mirror is untouched.

## Required implementation sequence

1. Real merge and conflict inventory.
2. Configuration/build graph and glue-layer compatibility.
3. Low-level RI/Ewald/ABF and full-k MPI.
4. Magnetic/SOC and scalar-ABF symmetry integration.
5. Four-channel EXX full/short/long execution.
6. Interface/mixer and RPA output ownership.
7. Generated docs and focused tests.
8. Identical-toolchain parent comparisons.
9. Local union suite, then fish Slurm gates.

## Implemented semantic resolutions

- Resolved all 48 predicted textual conflicts without a tree-wide
  `ours`/`theirs` selection; the unmerged index is empty.
- Kept the standalone `Exx_Info_RI` and added the missing long-V, weighted
  short-screen, Ewald-dimension/lambda, shrink and LibRPA reader-v1 controls
  through `Input_Parameter`, `Input_Item`, `Input_Conv`, runtime configuration,
  generated YAML and generated Markdown.
- Filled, broadcast and reconstructed `kvec_c_full` over
  `[0,nkstot_full)`, independently of the irreducible `nkstot` loop.
- Kept `Mix_DMk_2D<T>` and adapted RPA callers to the templated current API;
  no header-defined global RPA EXX object remains.
- Kept the chassis Shubnikov indices, `nanti_`, `magnetic_nspin4_`,
  `spin_U_`, and four-channel spinor restoration. For split Ewald, the four
  irreducible H maps are restored together once for the short channel and once
  for the long channel, then accumulated componentwise.
- Ported scalar ABF restoration onto the chassis symmetry model. Complex
  antiunitary scalar tensors receive spatial rotation plus complex
  conjugation; spinor `sigma_y` is confined to `restore_HR_nspin4`.
- Preserved non-split `dHexx` force/stress behavior and retained the explicit
  exception for rotated-ABF split-Ewald force/stress.
- Ported LibRPA reader-v1 output behind explicit object lifetime and added the
  schema/content validator.
- Preserved target CMake feature-disable conventions and fixed two
  build-tree-only test fixture issues without removing their install rules.
- The first active split-Ewald/SOC execution exposed mismatched timer labels in
  `Gaussian_Abfs::{get_Vq,get_dVq}`. Follow-up commit `872a46c8` pairs each
  timer start and finish with its own function label; the active path then ran
  to completion.

## Local verification evidence

Configuration:

```text
CMake Release, IntelLLVM 2026.1.0
MPI: Intel MPI 4.1
LCAO: ON
LibRI: ON, v2.1.1
ELPA: ON
BUILD_TESTING: ON
```

Results on the committed tree:

- `ninja -C build-merge -j4`: 2056/2056 build actions completed.
- `abacus_std_para --version`: `v3.11.0-beta7`.
- The exact `872a46c8` executable SHA-256 is
  `3f7fd254743372c7dc8d232c5a11332f738baf0f14a3b13d5d6b171110b51181`.
- Focused affected non-MPI CTest matrix: 32/32 passed. This includes basis
  permutation, spherical Bessel, single-rank full-k, SOC/spin symmetry,
  charge mixing, Ewald helpers, Hsolver LCAO, H/S and EXX restart IO,
  parser/item checks, LCAO operators, RI symmetry/mixer/CV IO and ABF order.
- After the timer correction, the focused RI/Ewald subset passed 6/6.
- `MODULE_HSOLVER_LCAO` initially exposed missing build-tree fixtures; after
  copying the existing source fixtures at configure time, all six diagonalizer
  cases passed.
- Parameter metadata generated 532 documented parameters. Regenerated
  `docs/parameters.yaml` and `input-main.md` compare byte-for-byte with fresh
  output from the built executable and generator.
- `git diff --check`, unmerged-index check, conflict-marker scan, and forbidden
  architecture scan passed.

The broader local Release run selected 257 registered tests whose commands do
not launch an external MPI/integration driver. It initially reported 234
passes and 23 failures. This is not reported as a clean full-suite pass.
Identical-toolchain comparison established the boundary:

- Sequentially rerunning those 23 tests on `872a46c8` gives one pass and 22
  failures.
- Building and sequentially running the same 23 tests on the pinned SOC parent
  `4aa46ed6` gives the identical one-pass/22-failure set.
- The apparent additional parallel failure, `MODULE_BASE_memory`, passes alone
  on both revisions. It races with `MODULE_BASE_tool_check` because both tests
  use the fixed build-directory filename `tmp`.
- The 22 shared failures are pre-existing Release/death-test, missing
  build-tree-fixture, exact-floating-comparison, and test-undefined-behavior
  failures. They include two shared segfaulting tests. They are retained as
  baseline defects and are not hidden or repaired as part of the RI/SOC merge.
- The pinned `master_ghj` parent cannot build the relax comparison targets with
  IntelLLVM 2026.1 and the system RapidJSON: compilation stops in
  `rapidjson/document.h` at assignment to const `GenericStringRef::length`.
  The merged tree inherits the modern SOC-side build compatibility and builds
  those targets.

An active cross-feature case derived from 08_EXX case 15 was run directly on
the exact `872a46c8` executable with nspin=4, SOC, symmetry, a 2x2x1 k mesh,
HF, Massidda correction, `exx_coul_moment=1`, `exx_rotate_abfs=1`, and
short/long Ewald thresholds. It completed in 17.5 seconds and logged:

- `Rotated ABFS long-prefix sizes by type: T0=25`;
- construction of Ewald bare Coulomb blocks in the current ABFS basis;
- `E_exx = -22.6133091022 Ry = -307.6698544252 eV`;
- total energy `-3296.003281913857 eV`;
- generated `hrs1_nao.csr` and `srs1_nao.csr`.

The paired `cal_force=1` run exited nonzero with the exact intentional message
`Rotated-ABFS split Ewald currently supports energy/SCF only.` A symmetry-off
sibling reproduced the EXX energy, but its coarsely converged total energy
differed. That total-energy comparison is therefore not accepted as numerical
or physical equivalence; a converged fish comparison remains mandatory.

Local MPI execution is not counted as a failure: Intel Hydra cannot open its
listener port in the local sandbox. The full-k rank-4 and Ewald-distribution
tests therefore remain mandatory fish Slurm gates rather than being marked
passed locally.

## Remaining implementation and validation risks

- A merge that compiles can still mix spinor channels incorrectly.
- Scalar antiunitary ABF behavior needs a mathematical test, not just a
  successful call.
- Parent result files may encode different architecture-era output; schema
  validation must distinguish intentional versioning from numerical drift.
- ASan with Intel MPI may need a serial/focused boundary; any unavailable
  sanitizer configuration is reported, not silently skipped.
- The fish mirror has an environmental LibRI compatibility revert. Parent and
  merged builds must use the same pinned fish LibRI or explicitly record a
  commit-local compatibility patch before comparing results.
- The new active split-Ewald + SOC + magnetic-symmetry cross-case must be run
  on fish before a stable reference is checked in; its paired force/stress run
  must fail with the intentional unsupported-path message.
