# Three-tree merge validation harness

This directory contains the auditable CTest comparison used for the
`master_ghj` x SOC/magnetic-symmetry semantic merge. It replaces the historical
jobs that swallowed CTest exit codes or classified wrapper summaries.

## Contract

- Discover the canonical union of merge, SOC-parent, and master-parent CTest
  registrations. Renamed tests are mapped explicitly by `aliases.json`.
- Import only real return-code observations from the pinned job-1091 seed.
  Every merge non-pass, alias, newly registered test, and test named by
  `force-rerun-current.txt` is rerun with verbose output.
- A merge failure is `MERGE_REGRESSION` whenever either comparable parent
  passes. Matching a failing parent cannot override that precedence.
- `PASS_ALL` requires all three trees to pass. A merge pass with a missing or
  failing parent is recorded separately as `MERGE_PASS_PARENT_NONPASS`.
- Missing/not-run merge tests, merge regressions, and unreviewed `UNKNOWN`
  results block the gate. A PASS marker is written only when none remain.
- Each CTest invocation runs in its own process group. A harness timeout
  terminates the entire group before the next test starts.
- Signature normalization removes only audited wrapper paths, test numbers,
  process identifiers, timestamps, and explicitly labelled elapsed-time
  metadata. Scientific numbers and failure text retain their order and value.

`build-three-tree-v3.slurm` creates fresh clean source/build trees. The union
wrapper records the seed commit, force-rerun list, source commits, build paths,
input hashes, timeout, raw logs, normalized signatures, observations, final
table, summary, marker, and `MANIFEST.sha256`.

`reclassify-three-tree-v3.slurm` exists for a normalizer correction after an
otherwise complete raw run. It first verifies the source manifest, copies the
raw result to a new immutable directory, verifies every raw-log hash, refreshes
signatures, and writes a new manifest. It never mutates or silently upgrades
the source result. Manual overrides are accepted only for an `UNKNOWN` row and
must include a reason and evidence reference.

Run `python3 test_three_tree_union.py -v` after any harness change. The Slurm
wrappers repeat this self-test on the compute node before touching CTest.
