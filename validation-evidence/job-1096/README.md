# Fish job 1096: clean three-tree raw result

Job 1096 ran the corrected three-tree union against the fresh job-1094 builds
in the immutable directory
`/data/users/bhj/ai-runs/abacus-union-56569c641-20260802-codex-v4`.
It used merge checkpoint `7ef8506a8`, SOC parent `4aa46ed6`, master parent
`dd421665`, seed checkpoint `c4c076ca2`, a 330-second per-test limit, and
explicit force-reruns for the ABF and nspin4 tests changed after the seed.

The run completed 55 verbose reruns (159 role logs) over a 313-test union.
Its source manifest verified with return code zero.  The critical
`03_NAO_multik` master timeout left no numeric Slurm step before the next test
started, and no job-local step existed after completion.  Therefore the raw
run has a valid process and evidence boundary.

The conservative first classification was `FAIL` solely because 22 non-pass
rows were `UNKNOWN`; it found zero merge regressions, zero missing merge
registrations, and zero not-run merge tests.  `UNKNOWN_REVIEW.md` reviews each
row without changing raw observations.  `manual-overrides.csv` is intended
only for immutable reclassification; the harness rejects an override of any
row whose original class is not `UNKNOWN`.

`REMOTE-RESULT-MANIFEST.sha256` is the exact manifest from fish.  It cannot be
fully checked against this curated local bundle because the 408 MB verbose-log
tree is intentionally not duplicated into Git.  Its successful remote check
is retained in `union3tree-v3-1096.out`; raw hashes and relative paths are in
`observations.json` and `union-table-final.csv`.
