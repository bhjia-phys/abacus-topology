# Governance exception draft (PENDING MAINTAINER APPROVAL)

Status: **DRAFT - not approved**. This document records the reason, scope,
risk and cleanup plan for the global-state growth reported by
`agent_governance_check.py`. Approval is required from the repository
maintainer before the governance gate can be lifted.

## Evidence

Re-run on the current checkpoint (base `4aa46ed6`, head `269ac8a8`):

```text
python3 tools/03_code_analysis/agent_governance_check.py \
  --base 4aa46ed65caf72fca593bf1a99a4e1705526f1a8 \
  --head 269ac8a8ec48237797d1929142ececbc7d2c6dbc --format json
```

- total findings: 168 (105 errors, 63 warnings)
- Global dependency budget (error): 105 findings
- GlobalV/GlobalC/PARAM references: added = 124, removed = 30,
  **net_delta = +94**
- other categories: "No new default parameters" (6),
  "Avoid new .hpp propagation" (1), "Header dependency review" (56)

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

## Relation to the remaining acceptance blockers (2026-08-02)

- Complex antiunitary coverage: CLOSED (commit a53bd8c6e); independent of
  this exception.
- Sanitizer: serial focused ASan/UBSan run in progress; if the toolchain
  cannot run it, the exact error will be recorded here and an exemption
  requested separately.
- Master-parent union column: NA; a test-enabled master-parent rebuild
  (~1-2 h, ~20 GB) or an explicit maintainer exemption is required -
  recorded separately in MERGE_AUDIT.md, not covered by this exception.
- Cross-feature combined gate: formally UNVALIDATED (coverage gap, not a
  governance item).

## Approval record

- [ ] Maintainer approval (reason/scope/risk/plan accepted)
- [ ] Cleanup plan registered as follow-up work items
