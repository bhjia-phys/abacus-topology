# Fish job 1092: harness failure

Job 1092 used the fresh immutable run directory
`/data/users/bhj/ai-runs/abacus-three-tree-40e30173c-20260802-codex`.
It stopped before source materialization, CMake, compilation, or CTest because
the submitted script referenced `role` within the same `local` declaration
that initialized it while `set -u` was active.

This directory therefore records an infrastructure-script failure only. It is
not evidence for or against the ABACUS merge. The failing script and captured
Slurm console are preserved byte-for-byte; commit `90d497c91` contains the
strict-shell fix. The failed remote run directory was not reused.
