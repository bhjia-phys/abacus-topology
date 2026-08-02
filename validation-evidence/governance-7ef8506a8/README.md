# Governance check at merge checkpoint 7ef8506a8

The committed governance checker was run locally against the pinned SOC
chassis base and the exact source checkpoint used by fish build job 1094:

```text
python3 tools/03_code_analysis/agent_governance_check.py \
  --base 4aa46ed65caf72fca593bf1a99a4e1705526f1a8 \
  --head 7ef8506a8bd4890e34d3e027660670bff3477ffb \
  --format json
```

The expected nonzero checker exit status was preserved. The JSON contains 168
findings: 105 errors and 63 warnings. The 105 global-dependency findings report
124 additions, 30 removals, and net delta +94. These counts are unchanged from
the earlier 269ac8a8/c4c076ca2 evidence; the later changes are tests, harness,
and evidence only.

This evidence does not approve the exception. Maintainer approval or a code
refactor is still required to close the governance gate.
