# Governance exception (APPROVED)

Status: **APPROVED on 2026-08-03**. The workspace maintainer explicitly
accepted the reason, scope, risk, and cleanup plan for the global-state growth
reported by `agent_governance_check.py`.  The historical filename is retained
so existing audit and evidence references remain stable.

## Evidence

Re-run on the exact source checkpoint used by fish build job 1094 (base
`4aa46ed6`, head `7ef8506a8`, 2026-08-02). The committed raw JSON and checksum
are under `validation-evidence/governance-7ef8506a8/`. Counts are identical to
the 269ac8a8/c4c076ca2 runs because the later commits change tests, harnesses,
and evidence rather than global-state references:

```text
python3 tools/03_code_analysis/agent_governance_check.py \
  --base 4aa46ed65caf72fca593bf1a99a4e1705526f1a8 \
  --head 7ef8506a8bd4890e34d3e027660670bff3477ffb --format json
```

- total findings: 168 (105 errors, 63 warnings)
- Global dependency budget (error): 105 findings
- GlobalV/GlobalC/PARAM references: added = 124, removed = 30,
  **net_delta = +94**
- other categories: "No new default parameters" (6),
  "Avoid new .hpp propagation" (1), "Header dependency review" (56)

Per-file Global-budget findings (all contributors, from the 7ef8506a8 run):

| File | Findings |
|---|---|
| source/source_lcao/module_ri/RPA_LRI.hpp | 42 |
| source/source_lcao/module_ri/Exx_LRI.hpp | 39 |
| source/source_lcao/module_operator_lcao/op_exx_lcao.hpp | 7 |
| source/source_io/module_parameter/input_conv.cpp | 6 |
| source/source_lcao/hamilt_lcao.cpp | 3 |
| source/source_hsolver/hsolver_lcao.cpp | 2 |
| source/source_lcao/module_operator_lcao/operator_lcao.cpp | 2 |
| source/source_lcao/module_ri/module_exx_symmetry/symmetry_rotation_output.cpp | 2 |
| source/source_hamilt/module_xc/xc_functional.cpp | 1 |
| source/source_lcao/module_ri/ewald_Vq.hpp | 1 |

Per `docs/developers_guide/agent_governance.md`, a net increase is a
high/block item unless an exception is recorded.

## Reason (why unavoidable)

The merge semantically ports the AroundPeking `master_ghj` line's
EXX/RPA/LibRPA-reader features onto the SOC magnetic-symmetry chassis. The
porting surface is:

1. input parameter chain: master fields (`rpa_ccp_rmesh_times`,
   `exx_ccp_rmesh_times`, `out_librpa_reader_version`, Ewald/lambda
   controls, ...) flow through `Input_Parameter` -> runtime config ->
   Exx/RPA code, following the existing chassis convention of reading
   `PARAM`/`GlobalV` at the use site.
2. RPA_LRI / Exx_LRI / Ewald / ABF-symmetry implementations were taken
   from master_ghj nearly verbatim to keep behavior bit-compatible
   (validated: merge vs master_ghj ABACUS outputs are bitwise identical,
   273/277 files, and LibRPA v1 inputs all identical).
3. The chassis itself already uses the same global-state convention, so
   the port does not introduce a *new* architectural pattern; it extends
   the existing one by the size of the ported feature set.

## Scope

- Files: `source/source_lcao/module_ri/*` (Exx_LRI, RPA_LRI, ewald,
  exx_rotate_abfs, RI_Util), `source/source_io/module_parameter/*`,
  `source/source_hamilt/module_xc/*` (exx_info, xc_functional),
  `source/source_lcao/hamilt_lcao.cpp`, `source/source_hsolver/
  hsolver_lcao.cpp` and the operator glue.
- Count: net +94 GlobalV/GlobalC/PARAM references (124 added / 30
  removed), of which the majority are direct reads of already-existing
  parameters by ported code.

## Risk

- Global state complicates unit testing and future refactoring of the
  affected modules; the RPA/Exx modules are among the hardest to test in
  isolation today.
- No *new* mutable global introduced by this merge beyond the parameter
  objects themselves; the risk is the continued reliance on the pattern,
  not a new singleton.

## Follow-up cleanup plan (not part of this merge)

1. Convert the Exx/RPA input block to an explicit `Exx_Info_RI`-style
   configuration object passed through the call chain (the chassis
   already has `Exx_Info_RI info` as a by-value member - extend this
   pattern to the remaining read sites).
2. Replace `GlobalV`-scoped runtime flags touched by this port with
   `Input_Parameter` accessors or explicit parameters, in a dedicated
   refactor commit series tracked in MERGE_EXECUTION_PLAN.md.
3. Re-run `agent_governance_check.py` after each refactor step and
   record the net delta until the block threshold is reached.

## Relation to merge acceptance (2026-08-03, updated)

- Complex antiunitary coverage: CLOSED (a53bd8c6e); independent of this
  exception.
- Sanitizer: CLOSED (824768444, c97054108); ASan+UBSan focused passes under
  detect_leaks=0 and =1; LSan availability unverified - reported honestly,
  not as a leak-check PASS.
- Three-tree union gate: CLOSED by fresh build job 1094, clean raw run 1096,
  and immutable evidence reclassification job 1097.  The final 313-row table
  has zero merge regressions and zero blocking classifications; source and
  target manifests both verify.  This governance exception was not used to
  waive any union-test result.
- Cross-feature combined gate: formally UNVALIDATED (coverage gap, not a
  governance item). This exception does NOT cover it; the maintainer accepted
  that limitation separately on 2026-08-03.

## Cleanup ownership and milestones

- Owner: merge maintainer (bhjia-phys workspace), tracked in
  MERGE_EXECUTION_PLAN.md follow-up section.
- Milestone 1: Exx_Info_RI-style configuration object for the Exx/RPA input
  block (targets input_conv.cpp + Exx_LRI.hpp read sites; expected net
  reduction >= 30).
- Milestone 2: move RPA_LRI.hpp runtime flags to Input_Parameter accessors
  (targets the 42 findings in RPA_LRI.hpp).
- Milestone 3: re-run agent_governance_check.py and record the net delta
  until the block threshold is reached.
- Not in scope: QSGW module, G0W0 public logic, parent-architecture
  unification.

## Approval record

- [x] Maintainer approval (reason/scope/risk/plan accepted on 2026-08-03)
- [x] Cleanup plan registered as follow-up work items in
  `MERGE_EXECUTION_PLAN.md`

Approval closes the governance acceptance gate for this merge checkpoint.  It
does not erase the +94 delta, claim architectural cleanup is complete, waive a
test result, or authorize a push/formal-branch update.
