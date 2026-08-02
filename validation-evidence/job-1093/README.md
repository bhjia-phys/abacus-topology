# Fish job 1093: incomplete master-parent dependency reproduction

Job 1093 used merge checkpoint `b19d4b6fcab6287428ec88703d6175a09d57dfc1`
and the exact pinned SOC and `master_ghj` parents in the fresh immutable run
`/data/users/bhj/ai-runs/abacus-three-tree-b19d4b6fc-20260802-codex-v4`.

The merge and SOC builds completed. The master build failed because its cache
recorded `CEREAL_INCLUDE_DIR-NOTFOUND`; this caused missing
`cereal/cereal.hpp` and `cereal/archives/binary.hpp` diagnostics across many
test targets. The harness returned `master_policy=1` and did not write
`BUILD_PASS`. No CTest or three-tree classification was run.

This is a validation-environment reproduction failure, not evidence for or
against the merged implementation. The corrective harness explicitly uses the
Cereal installation recorded by job 1091's master cache and accepts a partial
master build only if its failed-target set is exactly
`MODULE_RI_ri_cv_io_test`.
