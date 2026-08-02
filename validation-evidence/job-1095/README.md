# Fish job 1095: diagnostic union run, not acceptance evidence

Job 1095 uses the fresh three-tree builds from job 1094, but it cannot be
used as the final union gate.  Its harness predates the timeout-cleanup and
normalization corrections now documented in `validation-harness/README.md`.

During the run, the 03_NAO_multik master invocation exceeded its harness
timeout but its Slurm MPI step and four `abacus_3p` ranks remained alive while
the harness advanced to 07_OFDFT.  The captured process snapshot shows the
old step 531 and its processes after roughly 1762 seconds, together with the
new step 564.  Later readback also found concurrent numeric steps 566 and 591.
This proves cross-test process contamination, independently of any eventual
job exit code or marker.

The run also used the earlier metadata normalizer and did not force fresh
execution of focused tests changed after the imported seed checkpoint.
Therefore no result from job 1095 may resolve the three-tree acceptance gate.
The historical remote run remains untouched as failure evidence.

This README records a diagnostic conclusion while the Slurm allocation is
still active.  Final scheduler state and console evidence will be added only
after the job ends.
