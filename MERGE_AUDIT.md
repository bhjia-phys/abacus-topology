# Merge audit: master_ghj x soc-sym-mag-final

Audit date: 2026-07-31

Verdict: `BLOCKED` (revised 2026-08-01 after independent Codex review; see
the `Codex-review remediation` section for the exact blocking gates). The
original draft Prompt B was not executable as written. The blockers were
removed by separating existing force/stress preservation from unsupported
split-Ewald force/stress, requiring the exact split-Ewald trigger, correcting
case selection, and expanding review to all actual conflict modules.

Checkpoint lineage (distinct roles, do not conflate):

- merge commit: `2c462275e25b05bde4f520913d99774d1fe41ad5` (real two-parent
  merge; parents in order are the pinned SOC chassis and `master_ghj`
  revisions listed below).
- early validated code checkpoint: `872a46c8` (timer fix; the first local
  evidence set) and `269ac8a8` (the first fish validation run directory).
- **current validated code checkpoint: `c4c076ca2`** (Codex-review
  remediation: audit truthfulness, single_R temp isolation, ABF and
  nspin4 tests, error-handling hardening; fish jobs 1048/1056/1060/1061).
- report commit (local, not pushed): `0e93393d1` (this audit's verdict
  statement) and earlier documentation commits; report commits add no
  validated-code changes beyond c4c076ca2.

No remote branch has been updated.

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

## Fish Slurm validation (jobs 1005-1023 on commit `269ac8a8`; jobs 1048/1056/1060/1061 on checkpoint `c4c076ca2` in the remediation section below)

Run directory (new, immutable):
`/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260731-269ac8a8-v3`
on `Fisherd-Server`. All compilation, MPI and numerical work ran exclusively
through Slurm. Bundle SHA-256 `b7637a8a...959076`, cross-case archive
`3c4a3f47...e048f`, LibRI new-API tarball `238e9021...cda5`.

| Gate | Job | Result | Evidence |
|---|---|---|---|
| Full LibXC build | 1005 | PASS | IntelLLVM 2025.2.1, Intel MPI 2021.16, LibXC 6.2.2, LibRI new API (libri-new-api-2.1.1), ELPA, `ENABLE_LIBXC=ON`, `ENABLE_MLALGO=OFF`, `CMAKE_BUILD_TYPE=Release`. Exe SHA-256 `a52a7f1e7c...6f9f90`, `ABACUS v3.11.0-beta7`. Build required the fish LibRI `Flag_Finish::Cs` vs `::C` compatibility pin, recorded in `provenance/`. |
| Focused unit + MPI | 1006 | 27/30 | Non-MPI focused matrix green; klist/Ewald rank-1/2/4 all pass. 3 fails: two parallel driver fixtures, `MODULE_IO_single_R_test`. |
| Focused fix | 1007 | **FAIL** (env-blocked) | Fixture fix made the two parallel driver tests pass, but `MODULE_IO_single_R_test` still fails on fish (shared `/tmp/0temp_sparse_indices.dat`, owner `fisherd`; passes locally). Script exited 1, marker `FOCUSED_FIX_FAIL`, **no PASS file written**. Audit previously listed this gate as PASS - corrected: this gate did NOT pass. Root cause is the test hard-coding `/tmp/<rank>temp_sparse_indices.dat` (`source/source_io/test/single_R_io_test.cpp:195`); fix = unique temp dir per run, then re-run. |
| Pinned-parent regression | 1008 | PASS | 58 `08_EXX` cases (03/08/15/53/56/57 family) run with merge, soc-parent `4aa46ed6` and master-parent `dd421665` executables; inputs/`result.ref` pinned from each parent and recorded by SHA-256. |
| LibRPA reader-v1 | 1012 | PASS | Cases 56/57 with `overlay-from-soc-parent` inputs; output schema/content validator green (binary-format outputs, so validator is the gate, not text diff). |
| Cross-feature (positive + negative) | 1013 | PASS | `58_KP_HSE_SOC_symm` converged (final drho 4.06e-6), `58_KP_HSE_SOC_nosymm` converged (drho 5.88e-6); `58_KP_HF_SOC_EWALD_symm` carries the split-Ewald markers and E_exx = -22.6133091022 Ry; `negative-force-reject` exits with the exact message `Rotated-ABFS split Ewald currently supports energy/SCF only.` |
| Performance benchmark | 1022 | PASS | Per repetition/statistics protocol vs soc-parent (`d41c3cb6...`) and master-parent (`18715e9c...`): cases 03 +1.2%, 08 +5.8%, 15 +0.6%, 53 +12%. Case 53 is micro-benchmark noise: merge 2.745 s vs master-parent median 2.74 s, and merge-vs-soc-parent on case 03/08/15 is the larger representative suite. No evidence of regression. |
| Affected union suite | 1023 | **RAW_RESULT, NOT PASS** | Raw full CTest run of the affected registered-test union: 311 tests, 50 failed (84% pass). The job script unconditionally wrote a `PASS` marker (`ctest ... || true` swallowed the exit code, `printf ... > PASS` at script end) - **marker is invalid; it records raw results only**. Corrected in the Codex-review remediation below; parent-symmetric baseline for all 50 failures is a mandatory remaining gate. |

### Union-suite failure classification (job 1023)

`84% tests passed, 50 tests failed out of 311`. The classification table below
is a **provisional hypothesis** based on error signatures read from
`ctest-full.log`; it is NOT a parent-symmetric baseline. Two corrections apply
versus the earlier audit text:

1. **The claim "none of the failing modules is touched by this merge" is
   WRONG.** The merge conflict/change list explicitly includes
   `source/source_io/module_hs/single_R_io.cpp` (source of the failing
   `MODULE_IO_single_R_test`), `source/source_hsolver/hsolver_lcao.cpp`
   (failing HSOLVER tests), `source/source_lcao/hamilt_lcao.cpp` and
   `source/source_lcao/module_operator_lcao/*` (failing LCAO tests). The error
   signatures currently point to baseline/environment causes, but **a
   per-test, three-tree (merge / SOC parent / master parent) comparison in the
   identical environment is the mandatory remaining gate** before any
   failure can be called inherited.
2. **The class counts sum to 52, not 50**: classes overlap (e.g.
   `MODULE_IO_single_R_test` appears in both "Release death-test" and the PMI
   env list; #263 and #208 carry multiple signatures). A unique per-test
   mapping is required and is part of the remediation gate.

| Class | Count | Tests | Basis |
|---|---|---|---|
| Release death-test | 16 | matrix, complexarray, complexmatrix, integral, bspline, container, ndarray, relax allocate* (4), IO_single_R death subtest | MERGE_AUDIT 9.3 "Release/death-test" baseline |
| Intel MPI PMI env | 15 | NAO/AO/IO/PSI tests reporting `MPI startup(): PMI server not found` | fish environment; local sandbox has the same class of MPI limitation (audit 9.3: "Local MPI execution is not counted as a failure") |
| Missing build-tree .sh fixtures | 10 | `parallel_*_test.sh` etc. `No such file or directory` | 9.3 "missing build-tree-fixture" baseline |
| Shared SEGFAULT | 2 | MODULE_BASE_cubic_spline, MODULE_AO_ORB_atomic_lm_test | 9.3 "two shared segfaulting tests" baseline |
| Numerical/float | 3 | blas_connector.Axpy, sphbes precision, clebsch_gordan | 9.3 "exact-floating-comparison" baseline |
| Config-disabled / no-device integration | 4 | 07_OFDFT (needs `ENABLE_MLALGO`), 11_PW_GPU (no GPU), 01_PW CHG mismatch, 17_DS_DFTU deviation | build configuration and hardware, not merge code |
| Other fixture/state | 2 | 03_NAO_multik, 263 orb_io parallel | same environment class |

The 22 shared local baseline failures (9.3) are a subset of this union set.
No failure has yet been traced to the merged RI/SOC/Ewald/symmetry code, but
that conclusion awaits the three-tree per-test comparison (see remediation).

### Environmental limitations recorded

- `MODULE_IO_single_R_test`: blocked on fish by `/tmp/0temp_sparse_indices.dat`
  ownership (`fisherd`), passes locally. Environment residue.
- Parallel wrapper scripts for 10 legacy tests are not copied into the fish
  build tree; identical class is baseline.
- Intel MPI `PMI server not found` for tests invoked without a wrapper; the
  full-k and Ewald MPI gates at ranks 1/2/4 pass under `srun`, which is the
  gated path.
- `ENABLE_MLALGO=OFF` and no CUDA device explain 07_OFDFT and 11_PW_GPU.

### Final verdict

**BLOCKED** (status as of 2026-08-02, checkpoint `c4c076ca2`). The earlier
`PASS` was withdrawn because the following gates were not truthfully closed;
each is now resolved except the four blockers below. The exact remaining
blockers are:

1. **Combined cross-feature gate UNVALIDATED** - no single case satisfies
   nspin=4 + SOC + magnetic symmetry + active split-Ewald + convergence +
   non-trivial magnetization (HSE never activates split-Ewald; HF probe is
   scf_thr=1; PBE0 magnetization collapses). Formally recorded as a
   coverage gap; acceptance requires either a qualifying case or an
   explicit maintainer acceptance of the gap.
2. **Master-parent union column NA** - master-parent-build has no test
   binaries (0 registered tests); a test-enabled rebuild (~1-2 h, ~20 GB,
   fish disk at 96% use) or an explicit maintainer exemption is required
   before the union gate can claim three-tree coverage.
3. **Complex antiunitary coverage** - CLOSED (commit a53bd8c6e): new
   AntiunitarySigmaYComplexInputs test with complex channels verifies
   conj+remap+sign explicitly (nspin4 suite now 7/7 PASS). Coverage
   boundary stated honestly: Exx_LRI::cal_exx_elec_soc short/long call
   chain is helper-level covered only (SCF-environment dependent), noted in
   the test header and here.
4. **Sanitizer status** - pending: a serial focused ASan/UBSan run is
   attempted; if it cannot run (toolchain/MPI incompatibility), the exact
   error will be recorded and an exemption requested.
5. **Governance exception approval** - net +94 GlobalV/GlobalC/PARAM growth
   needs maintainer approval of `GOVERNANCE_EXCEPTION_DRAFT.md` (or a
   refactor milestone).

The earlier `PASS` was withdrawn because the following gates were not
truthfully closed; each is now resolved except the blockers above:

1. Focused-fix gate (job 1007) actually FAILED (`FOCUSED_FIX_FAIL`,
   `MODULE_IO_single_R_test`); the audit table previously marked it PASS.
2. Union-suite `PASS` marker (job 1023) was written unconditionally despite
   50/311 failures (`ctest ... || true`); it is a raw record, not a PASS.
3. Union failures have no parent-symmetric three-tree baseline; the audit's
   claim that failing modules are untouched by the merge is contradicted by
   its own conflict list (`single_R_io.cpp`, `hsolver_lcao.cpp`,
   `hamilt_lcao.cpp`, `module_operator_lcao/*`).
4. Scalar-ABF antiunitary restore has no focused mathematical unit test.
5. The original cross-feature contract (single case with nspin=4 + SOC +
   magnetic symmetry + active split-Ewald + four spin blocks + on/off) is not
   satisfied by any single case; the split HSE/HF/PBE0 evidence is recorded in
   `HSE_CALIBRATION_RECORD_20260731.md` but the original contract was never
   formally revised.
6. Governance check reports 168 findings (105 errors / 63 warnings) with a
   net +94 GlobalV/GlobalC/PARAM references; no exception was recorded.

Still valid positive evidence (not re-opened by this review): build job 1005;
focused unit+MPI job 1006 (27/30, both fixture failures then fixed);
parent-regression job 1008 (58 cases); LibRPA reader-v1 job 1012; magnetic
matrix 18/18 bitwise vs soc-parent (job 1027); ABACUS output merge vs
master_ghj 273/277 bitwise identical with all LibRPA v1 inputs identical
(job 1031); LibRPA standalone regression 22/22 PASS (job 1044). The
ABACUS->LibRPA v1 consumer comparison job 1045 is RETRACTED as a
stale-output false positive (see claim #8 in the remediation table); a
correct consumer run is an open gate. The 9 eV Fe symm/nosymm gap and the
HSE-never-activates-split-Ewald architecture finding remain inherited
master_ghj behavior.

The exact blocking gates and the remediation plan are enumerated in the
`Codex-review remediation` section below.

## Codex-review remediation (2026-08-01)

An independent review (Codex agent) audited the merge evidence. Every claim
was re-verified against the raw fish logs, slurm scripts, source tree and
locally re-run tools before being accepted.

| # | Codex claim | Verdict after re-verification | Evidence |
|---|---|---|---|
| 1 | Focused-fix (job 1007) failed but was reported PASS | **CONFIRMED (audit-level)** | `validation/focused-fix/console.log` ends `FOCUSED_FIX_FAIL`, `failures=1`, no PASS file (script exits 1 correctly); the audit table row was wrong. |
| 2 | Union-suite PASS marker invalid | **CONFIRMED** | `jobs/abacus-union-suite-269ac8a8-v3.slurm` line 43-44 `ctest ... \|\| true`, line 63 unconditional `printf > PASS`; 50/311 failed. |
| 2b | Classification counts 52 != 50 | **CONFIRMED** | Overlapping classes; per-test unique mapping required. |
| 3 | "Failing modules untouched by merge" is false | **CONFIRMED** | Conflict list includes `source/source_io/module_hs/single_R_io.cpp`, `source/source_hsolver/hsolver_lcao.cpp`, `source/source_lcao/hamilt_lcao.cpp`, `source/source_lcao/module_operator_lcao/*`; failing tests MODULE_IO_single_R (241), HSOLVER (194-196), LCAO (141,158) map onto them. |
| 4 | No scalar-ABF antiunitary math unit test | **CONFIRMED** | `module_exx_symmetry` tests contain no direct `rotate_atompair_serial_abf`/`restore_HR_abf` test; audit itself flags this (line 270). |
| 5 | Original cross-feature contract not satisfied by any single case | **CONFIRMED** | HSE never activates split-Ewald (Erfc->Center2); HF probe is scf_thr=1; PBE0 magnetization collapses ~1e-4 uB. Contract split without formal revision. |
| 6 | No parent-symmetric union baseline | **CONFIRMED at review time; NOW CLOSED (jobs 1060+1061, checkpoint c4c076ca2)** | Three-tree per-test table produced: 308 registered tests of the checkpoint build, each run identically on the merge exe (23fc8201) and the soc-parent build (d41c3cb6, 211 registered). Classification: 14 PASS_ALL; 248 merge-PASS (soc registration differs); 46 merge-FAIL; **0 MERGE_REGRESSION**. All 46 merge-failed tests were re-verified on the soc-parent build (job 1061): 44 `Error` + 2 `Error;SEGFAULT`, **per-test signatures identical to merge (mismatch=0)** - inherited failures, not regressions. The master-parent column is NA: master-parent-build has no test binaries (0 registered tests); rebuilding it with BUILD_TESTING would need ~1-2 h and ~20 GB on a fish disk at 96% use - recorded as a limitation, not silently skipped. |
| 7 | Governance 168 findings / 105 errors / 63 warnings / net +94 | **CONFIRMED** | Re-ran `agent_governance_check.py --base 4aa46ed6 --head 269ac8a8 --format json` locally: 168 items, 105 error / 63 warning, GlobalV/GlobalC/PARAM added=124 removed=30 net=+94. |
| 8 | ABACUS->LibRPA v1 E2E not verified | **CONFIRMED at review time; NOW PASSED with strict methodology (job 1056, checkpoint c4c076ca2)** | The original claim stands: job 1045 was a stale-output false positive (copied old librpa.out with mtime < job start, broken 1-rank MPI, swallowed exit codes) and was retracted. Job 1056 re-ran the consumer correctly: fresh immutable dirs, dataset assembled from the framework-verified 1044 workspace layout with only the ABACUS-produced v1 files (15 per side) swapped in from merge (exe 23fc8201) and master_ghj (exe 18715e9c) runs; both chi0_main consumers exited 0 under real 4-rank MPI (`Total number of tasks: 4`); librpa.out mtime (02:47:25 / 02:47:27) later than job start (02:47:23); sizes 37810 B each. Physics result **identical**: `Total EcRPA: -0.375934152` on both sides. Remaining librpa.out diffs are only the init timestamp, memory measurement, per-step timing fields and MPI print ordering - no numeric/physics difference. Input provenance recorded: ABACUS v1 files from the merge/master runs; band_out/KS_eigenvector/velocity_matrix/k_path_info kept from the LibRPA baseline (pyatb head/wing meanfield aux), documented in the job console. |

### Remediation plan (ordered)

1. Test-harness truthfulness: rewrite union-suite and focused-fix scripts so
   PASS is written only when the mandatory exit codes are zero; otherwise
   write `RECORDED`/`FAIL`. Never `ctest ... || true` into a PASS marker.
2. Fix `MODULE_IO_single_R_test` temp-file hygiene: use a unique temp
   directory per run instead of fixed `/tmp/0temp_sparse_indices.dat`
   (`source/source_io/test/single_R_io_test.cpp:195`), then re-run the exact
   focused gate on fish.
3. Three-tree union baseline: run the identical 311-test list with merge,
   SOC parent and master_ghj parent builds in the identical environment;
   produce the per-test table
   `test | merge | soc_parent | master_parent | exit codes | error signature | classification`
   (classes: PASS_ALL / INHERITED_BOTH / INHERITED_SOC / INHERITED_MASTER /
   MERGE_REGRESSION / ENVIRONMENT_REPRODUCED_ALL / UNKNOWN).
4. Add scalar-ABF unitary/antiunitary math unit tests (complex non-real
   tensor, non-trivial T1/T2, unitary vs antiunitary contrast, explicit
   T1^dag A* T2 under the documented convention, restore_HR_abf atom-pair/R
   star mapping, invalid-shape and duplicate-key behavior; replace the silent
   `continue` on invalid shape with a detectable error).
5. Add four-spinor short/long channel restore focused tests (all four
   channels nonzero, off-diagonal channels, SU(2) rotation, sigma_y K,
   per-channel single restore, no channel cross-talk, Hermiticity).
6. Cross-feature: **formally marked UNVALIDATED (2026-08-01/02)**. Three
   candidates were tested with pre-registered reasoning (HSE_CALIBRATION_RECORD):
   HSE converges with non-trivial magnetization but never activates split-Ewald
   (Erfc->Center2 by inherited architecture); HF activates split-Ewald but is
   only a scf_thr=1 probe (not a convergence gate); PBE0 activates split-Ewald
   and converges but its magnetization collapses to ~1e-4 uB. No single case
   satisfies the original combined contract; the gap is recorded as
   implementation/architecture coverage gap (HSE path) and stability gap
   (HF/PBE0 magnetization), not as a merge regression. No further unbounded
   search will be performed.
7. Record a governance exception for the net +94 GlobalV/GlobalC/PARAM
   growth (reason: master_ghj semantic port; scope; risk; why unavoidable;
   follow-up cleanup plan), or eliminate a defensible subset of the new
   references.
8. Re-run build -> focused -> MPI -> parent preservation -> cross-feature ->
   union -> validators on a single immutable commit, and only then re-assess
   PASS.

Items 1-2 are executed in the next commits; items 3-8 are open gates tracked
in MERGE_EXECUTION_PLAN.md status.
