# Fish job 1097: immutable union reclassification PASS

Job 1097 is a lightweight evidence operation; it did not compile or run
ABACUS.  It copied the verified job-1096 result from
`/data/users/bhj/ai-runs/abacus-union-56569c641-20260802-codex-v4/result`
into the new immutable directory
`/data/users/bhj/ai-runs/abacus-union-reclass-35c900857-20260803-codex-v1`,
verified every source raw-log hash, refreshed signatures with harness SHA-256
`1b62080aa6d100ea6b93169afe1d1cc404ad09be11ea55ad94086daec4754df6`,
and applied the 22-row review in
`validation-evidence/job-1096/manual-overrides.csv`.

The source-manifest SHA-256 recorded in provenance is
`7930525208e8be41cb7b6f8d5cb0c871844bce17f8a53152f833b80546277fb4`,
exactly matching job 1096.  The target manifest verified with return code zero
and its local copy has SHA-256
`60d0686399ece0154364bd84c98b6e0db8feaeb5266f45ab2c45c43e4b93e4a6`.
The Slurm console ends `RECLASSIFY_RC=0`.

Final 313-test counts:

- `PASS_ALL`: 203
- `MERGE_PASS_PARENT_NONPASS`: 59
- `INHERITED_BOTH`: 28
- `INHERITED_SOC`: 16
- `ENVIRONMENT_REPRODUCED_ALL`: 7
- `MERGE_REGRESSION`: 0
- blocking classifications: 0

`PASS` therefore closes the three-tree union gate for source checkpoint
`7ef8506a8`.  It does not approve the separate combined cross-feature coverage
gap or the governance exception, and it does not authorize pushing the local
integration branch.

`REMOTE-RESULT-MANIFEST.sha256` is the exact target manifest from fish.  The
408 MB raw-log tree remains in the immutable fish result and is not duplicated
into Git.  The localized final table is line-ending-normalized to LF, so the
curated local bundle uses its own `MANIFEST.sha256`.
