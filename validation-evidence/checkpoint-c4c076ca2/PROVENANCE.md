# Provenance note (checkpoint c4c076ca2 fish evidence)

Pulled 2026-08-02 from Fisherd-Server via ssh.
Authoritative originals remain on fish:
/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260801-c4c076ca2-v3/
(jobs 1048, 1056, 1060, 1061 run directory).

Job roles:
- 1048: checkpoint build (exe SHA-256 23fc8201...) + focused CTest 6/6
- 1056: strict ABACUS->LibRPA v1 consumer comparison
- 1060/1061: three-tree union attempt - RETRACTED as invalid-harness
  evidence (exit-code swallowing: `ctest ... || true` then `$?` is always 0;
  token-level signatures; master never run). See MERGE_AUDIT.md and AITP
  entry-ccbe20d61a9b4a57a2bfbe88559bc546.

Verification: `sha256sum -c MANIFEST.sha256` must report all OK.
