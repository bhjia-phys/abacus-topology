# master_ghj x soc-sym-mag-final execution plan

Date: 2026-07-30

This plan is the reviewed execution contract for the semantic merge. It
supersedes the unsafe or untestable commands in the external draft plan, but
does not change the scientific invariants in that plan.

## 1. Pinned inputs and output

- Chassis / first parent:
  `maki49/soc-sym-mag-final@4aa46ed65caf72fca593bf1a99a4e1705526f1a8`
- Feature line / second parent:
  `AroundPeking/master_ghj@dd4216653386d32f79e3219f3ea5dd2d229c1c5a`
- Merge base:
  `b4945b633dc24f6ff407ca561d31393229702dc5`
- Unique commits: 49 on master_ghj, 316 on soc-sym-mag-final
- Output branch: `integration/master-ghj-soc-sym-mag`
- Local integration clone:
  `/home/bhjia/physics/GW_librpa/abacus-master-ghj-soc-integration`

The remote tips were queried again on 2026-07-30 before this clone was
created. The original dirty worktree
`/home/bhjia/physics/repo/abacus-mag-group` is not used for the merge.

No push, formal-branch update, release, remote deletion, or force operation is
authorized by this plan.

## 2. Corrected safety procedure

1. Resolve remotes by URL, not by assumed names.
2. Preserve backup refs for both pinned tips.
3. Keep `rerere.enabled=true` and `rerere.autoupdate=false`.
4. Run `git merge --no-ff --no-commit <MASTER_SHA>` and distinguish:
   - clean merge;
   - expected conflict exit with `MERGE_HEAD` and unmerged index entries;
   - other errors, which stop the merge.
5. Never select `ours` or `theirs` for the whole RI tree.
6. Stage each conflict only after three-way and caller review.
7. Create one real two-parent merge commit, followed by focused semantic-fix
   and test commits when needed.

## 3. Scope boundary

The merge must preserve every supported behavior of both pinned parents. It
must not claim to implement a capability absent from both.

In particular, `master_ghj` deliberately rejects force/stress when the
rotated-ABF split-Ewald path is active. The active condition is:

```text
exx_rotate_abfs
&& exx_coul_moment
&& an Ewald Coulomb setting (massidda or carrier)
```

Therefore:

- the existing non-split SOC HSE force/stress case remains a mandatory
  regression;
- the new split-Ewald SOC cross-case is an energy/SCF and H(R) test;
- a focused negative test must preserve the explicit force/stress rejection;
- long-range force/stress derivatives are a separate feature, not part of
  this merge.

## 4. Architecture invariants

The final tree must retain:

- standalone, modular `Exx_Info_RI`, held by value in `Exx_LRI`;
- templated `Mix_DMk_2D<T>` and current mixing lifetime/restart semantics;
- `nanti_`, `magnetic_nspin4_`, `spin_U_`, the target Shubnikov indexing
  convention, and `restore_HR_nspin4`;
- `dHexx` APIs and the existing non-split force/stress path;
- target CMake feature-disable macros and generated-document workflow;
- complete `kvec_c_full[0:nkstot_full)` data on every rank;
- master_ghj Ewald, ABF permutation, weighted-short controls, RPA/LibRPA v1
  output, and validator behavior that is genuinely absent from the chassis.

The final tree must not contain active:

- `Exx_Info::Exx_Info_RI`;
- `TRS_first_`;
- the old untemplated `Mix_DMk_2D` path;
- a non-inline header-defined `exx_lri_rpa` global object.

## 5. Semantic integration order

Each phase has a stop gate. Evidence from one commit or build is not valid for
a different tree.

### Phase A: configuration and build graph

- Merge modular EXX configuration fields and parser/conversion wiring.
- Keep target CMake conventions.
- Port basis-index permutation and validation.
- Resolve affected IO, hsolver, Hamiltonian, operator, restart, and CSR glue.

Gate: affected targets compile, parser/permutation tests pass, and static
architecture checks pass.

### Phase B: low-level RI/Ewald/ABF

- Integrate only missing Ewald, Gaussian, singular-value, weighted-short,
  ABF-permutation, and RPA-output helpers.
- Prefer the target helper/module boundaries over template-header duplication.

Gate: complete `MODULE_RI` low-level set, including MPI Ewald distribution.

### Phase C: full-k MPI

- Fill and reconstruct full-k arrays with `nkstot_full` bounds.
- Test the reduced/full mesh case at 1, 2, and 4 ranks.

Gate: every rank has byte/numerically identical complete full-k data.

### Phase D: symmetry

- Keep the target magnetic operation table and spinor rotation.
- Add only missing ABF matrix restoration.
- For antiunitary scalar ABF tensors, apply the spatial representation and
  complex conjugation when the tensor type is complex; never apply spin
  `sigma_y` to scalar ABFs.

Gate: spin, SOC density, spinor EXX, ABF unitary, and ABF antiunitary tests.

### Phase E: Exx_LRI channels

- For nspin 1/2, retain scalar full or short+long execution.
- For nspin 4 with symmetry, construct all four irreducible maps for each
  full/short/long channel, then call `restore_HR_nspin4` once per channel.
- Add short and long componentwise and post-process H and energy once.

Gate: scalar EXX, existing SOC EXX, four-channel rotation, split-Ewald
energy/SCF, and explicit force/stress rejection tests.

### Phase F: interface and RPA

- Preserve the target interface and mixer ownership.
- Port v1 output through an object with explicit lifetime.
- Run output validators, not just energy references.

Gate: cases 53, 56, and 57 plus their generated-file validators.

### Phase G: generated documentation and final reports

- Generate parameters YAML from the built binary.
- Generate input documentation from that YAML.
- Explain any checked-in generated-file change.

Gate: generated files reproduce exactly and all reports contain commands,
commits, logs, tolerances, and unresolved limits.

## 6. Regression matrix

### Static and build

- `git diff --check` and conflict-marker scan.
- Main LibRI build matching the available oneAPI environment.
- Minimal LibRI build.
- Focused Debug ASan/UBSan build where the MPI/toolchain combination supports
  it.

The nonexistent generic command `source toolchain/install/setup` is not an
acceptance command. The exact local and fish environment scripts must be
recorded from the working toolchains.

### Unit/module tests

- Complete `MODULE_RI`.
- `MODULE_RI_EXX_SYMMETRY_rotation`.
- `MODULE_CELL_SYMMETRY_rotation_spin`.
- `MODULE_CELL_SYMMETRY_rho_soc`.
- Affected `MODULE_CELL`, `MODULE_ESTATE`, `MODULE_IO`, `MODULE_LCAO`,
  `MODULE_HSOLVER`, `MODULE_HAMILT`, and `MODULE_BASE` tests.

### Integration tests

CTest registers suites, not individual 08_EXX cases. Targeted cases must use:

```text
tests/integrate/Autotest.sh -r '<anchored-case-regex>' ...
```

Mandatory parent-preservation cases:

- SOC parent: 03, 08, and 15 HSE/SOC cases.
- master_ghj parent: 53, 56, and 57 RI/RPA cases.
- The final registered-test matrix is the union of affected tests from both
  pinned parents, not merely the merged branch's list.

Each numerical comparison uses its case-specific threshold. Reference files
are not changed solely to obtain a pass.

### New cross-feature tests

1. nspin=4 + SOC + symmetry + truly active rotated-ABF split Ewald:
   energy, EXX energy, four H(R) spin blocks, Hermiticity, and symmetry
   on/off comparison.
2. The same configuration with force/stress requested: exact, intentional
   unsupported-path rejection.
3. Four-channel nonzero off-diagonal spin rotation: SU(2), antiunitary
   conjugation and `sigma_y K`, and H(R) Hermiticity.
4. Full-k reduced/full mesh at MPI ranks 1, 2, and 4.
5. Round-trip and invalid-value tests for every newly ported parameter.
6. LibRPA v1 output schema/content validator for cases 56 and 57.

## 7. fish validation

Fish is used only through Slurm for compilation/tests/calculations. Login-node
operations are limited to inspection, immutable-directory preparation,
submission, and result collection.

The existing dirty fish mirror `/home/bhj/abacus-mag-group` is preserved.
Validated commits will be transferred as Git objects/patches into a new
immutable mirror. Each submission uses a new directory under:

```text
/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260730-<commit>/
```

For every job, record:

- job ID and dependency chain;
- submitted script and immutable run directory;
- exact source commit and both merge parents;
- compiler, MPI, CMake flags, LibRI revision, and executable SHA-256;
- commands, rank/thread counts, exit status, logs, thresholds, and outputs.

Fish gates:

1. build and focused unit tests;
2. parent-preservation integration cases;
3. cross-feature MPI and SOC/Ewald tests;
4. only after those pass, the larger union suite.

No queue success alone is a scientific conclusion. Build compatibility,
test-oracle agreement, numerical convergence, and physical validity are
reported separately.

## 8. Completion rule

The branch is `PASS` only if:

1. the history contains the pinned tips as the two parents of a real merge;
2. every high-risk semantic conflict has an implementation and a focused test;
3. both pinned-parent behavior sets are preserved within their own tolerances;
4. the cross-feature tests pass on the committed tree locally and on fish, or
   a precisely bounded residual coverage gap is explicitly accepted by the
   maintainer without being relabeled as validated physics;
5. generated documentation is reproducible;
6. no sanitizer, MPI, unexplained-reference, or output-validator failure
   remains.

Otherwise the committed branch and reports may be delivered, but the verdict
must remain `BLOCKED` with the exact failed or unavailable gate.

## 9. Execution status (2026-08-03)

Implementation and acceptance are tracked separately:

- **Semantic merge: complete.** Commit
  `2c462275e25b05bde4f520913d99774d1fe41ad5` is a real merge whose parents
  are exactly the pinned SOC chassis and `master_ghj` revisions.  All 48
  textual conflicts are resolved and the unmerged index is empty.
- **Current built source: complete.** Fish job 1094 freshly materialized and
  built checkpoint `7ef8506a8bd4890e34d3e027660670bff3477ffb` plus both parents.
  Merge and SOC built fully; master had exactly the predeclared upstream
  `MODULE_RI_ri_cv_io_test` target defect.  Registration counts are
  merge/SOC/master = 313/310/287.  Evidence is committed under
  `validation-evidence/job-1094/`.
- **Focused correctness work: complete at the stated coverage boundary.**
  The single_R isolation, scalar-ABF antiunitary math, complex nspin4 restore,
  strict v1 consumer, focused MPI, parent-preservation and serial ASan/UBSan
  gates are recorded in `MERGE_AUDIT.md`.
- **Three-tree union acceptance: complete.** Jobs 1060/1061 and 1091 remain
  retracted because their harnesses could produce false conclusions.  Job
  1095 is preserved as diagnostic failure evidence for orphaned MPI steps.
  Job 1096 then produced a clean, manifest-verified 313-row raw comparison;
  its critical 330-second timeout removed the old step before the next test.
  The 22 conservative UNKNOWN rows were reviewed without overriding any
  regression/missing/not-run merge test.  Immutable reclassification job 1097
  verified source and target manifests and passed with zero blocking classes:
  203 PASS_ALL, 59 MERGE_PASS_PARENT_NONPASS, 28 INHERITED_BOTH,
  16 INHERITED_SOC, and 7 ENVIRONMENT_REPRODUCED_ALL.  Evidence is under
  `validation-evidence/job-1095/`, `job-1096/`, and `job-1097/`.
- **No remote update:** all implementation, harness and evidence commits are
  local; nothing has been pushed.

The two remaining acceptance decisions were resolved explicitly on 2026-08-03:

- the maintainer accepted the formally `UNVALIDATED` combined cross-feature
  contract as a documented scientific/architectural coverage limitation of
  this merge, not as a passing physics result; and
- the maintainer approved `GOVERNANCE_EXCEPTION_DRAFT.md`, including its
  reason, scope, risk, ownership, and cleanup milestones.

The pinned merge checkpoint therefore has verdict `PASS WITH DOCUMENTED
LIMITATIONS`.  Neither decision changes source code, waives a failed executable
gate, converts historical invalid harness evidence into PASS, or authorizes a
push/formal-branch update.

## 10. Accepted limitations and follow-up work

These items are tracked after merge acceptance and are not blockers for the
pinned checkpoint:

1. Design a bounded, pre-registered case (or revise the inherited HSE/split-
   Ewald architecture) before claiming the combined nspin4 + SOC + magnetic
   symmetry + active split-Ewald + converged non-trivial-magnetization contract
   is physically validated.
2. Introduce an Exx_Info_RI-style configuration object for the Exx/RPA input
   block, targeting at least 30 fewer global references.
3. Move the 42 RPA_LRI.hpp runtime-flag findings to explicit configuration or
   Input_Parameter accessors, then rerun `agent_governance_check.py` until the
   block threshold is reached.
