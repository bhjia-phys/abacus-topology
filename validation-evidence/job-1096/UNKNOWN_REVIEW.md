# Job 1096 UNKNOWN review

Job 1096 compared the 313-test registration union of merge checkpoint
`7ef8506a8`, SOC parent `4aa46ed6`, and master parent `dd421665` using the
fresh builds from job 1094.  It reran 55 selected tests and produced 159 raw
verbose logs.  The source result is immutable at
`/data/users/bhj/ai-runs/abacus-union-56569c641-20260802-codex-v4/result`.

The remote `MANIFEST.sha256` verified with return code zero after the run.  Its
local copy, `REMOTE-RESULT-MANIFEST.sha256`, has SHA-256
`7930525208e8be41cb7b6f8d5cb0c871844bce17f8a53152f833b80546277fb4`.
The run reported no `MERGE_REGRESSION`, `MERGE_TEST_MISSING`, or
`MERGE_NOT_RUN`; its only blockers were the 22 rows initially classified
`UNKNOWN`.  The `03_NAO_multik` master timeout was process-clean: Slurm step
531 disappeared before the next test's step 553 started, and no job-local
numeric step remained after job completion.

Manual review is restricted to those 22 `UNKNOWN` rows.  It cannot override a
regression, missing merge registration, or not-run merge test.  Scientific
numbers and failed test-case names were retained when comparing the raw logs.

| Test | Reviewed classification | Three-tree evidence |
|---|---|---|
| `03_NAO_multik` | `INHERITED_SOC` | Merge and SOC both fail only `relax_cell_vdw4_d4` and `relax_cell_vdw4_d4s` out of 336 cases. Master hangs earlier in its inherited `scf_in_dmr_GaAs` path and is cleanly stopped at 330 s. |
| `07_OFDFT` | `ENVIRONMENT_REPRODUCED_ALL` | All three fail exactly cases 04/06 with `ML KEDF requires ENABLE_MLALGO`; all three job-1094 caches record `ENABLE_MLALGO=OFF`. |
| `11_PW_GPU` | `ENVIRONMENT_REPRODUCED_ALL` | All seven GPU cases fail on every tree with `device=gpu` but no available GPU. |
| `MODULE_AO_ORB_nonlocal_lm_test` | `INHERITED_BOTH` | The same six named GoogleTest cases fail on all three trees. |
| `MODULE_BASE_complexarray` | `INHERITED_BOTH` | The same six named cases fail on all three Release builds. |
| `MODULE_BASE_complexmatrix` | `INHERITED_BOTH` | The same three cases fail to satisfy death-test expectations on all three Release builds. |
| `MODULE_BASE_container` | `INHERITED_BOTH` | The same two length-check death tests receive escaping `std::exception` on all three trees. |
| `MODULE_BASE_integral` | `INHERITED_BOTH` | The same two Simpson-integral death tests fail to die on all three trees. |
| `MODULE_BASE_math_sphbes` | `INHERITED_BOTH` | All three fail the same two precision cases; the reviewed numeric tail is identical, including differences 2.05 through 2.20 against tolerance `1e-12`. |
| `MODULE_BASE_matrix` | `INHERITED_BOTH` | The same eight named cases fail on all three Release builds. |
| `MODULE_IO_orb_io_test_parallel` | `ENVIRONMENT_REPRODUCED_ALL` | All three report `Couldn't open orbital file` and the remaining MPI ranks are killed; this is the same missing-fixture behavior in every tree. |
| `MODULE_IO_to_qo_test` | `ENVIRONMENT_REPRODUCED_ALL` | All three fail before the test body with Intel MPI `PMI server not found`. |
| `MODULE_LCAO_tddft_snap_psibeta_half_test` | `INHERITED_SOC` | Merge and SOC both fail before the test body with identical Intel MPI PMI setup error; the master tree does not register this SOC-side test. |
| `MODULE_NAO_atomic_radials` | `ENVIRONMENT_REPRODUCED_ALL` | All three fail before the test body with Intel MPI `PMI server not found`. |
| `MODULE_NAO_real_gaunt_table` | `INHERITED_BOTH` | All three fail only `RealGauntTableTest.Check3`; reviewed numeric differences are identical, including `2.3560188023355995e-4`, `2.4800197919322102e-5`, and `1.2400098959661052e-6`. |
| `MODULE_NAO_sphbes_radials` | `ENVIRONMENT_REPRODUCED_ALL` | All three fail before the test body with Intel MPI `PMI server not found`. |
| `MODULE_PSI_initializer_unit_test` | `ENVIRONMENT_REPRODUCED_ALL` | All three fail before the test body with Intel MPI `PMI server not found`. |
| `MODULE_RELAX_bfgs_basic_test` | `INHERITED_BOTH` | All three fail the same zero-dimension allocation and inverse-Hessian death-test cases; parent API spelling differs but the Release failure contract is the same. |
| `MODULE_RELAX_bfgs_test` | `INHERITED_BOTH` | All three fail only the zero-dimension allocation death test. |
| `MODULE_RELAX_ions_move_cg_test` | `INHERITED_BOTH` | All three fail only the zero-dimension allocation death test. |
| `MODULE_RELAX_lattice_change_cg_test` | `INHERITED_BOTH` | All three fail only the zero-dimension allocation death test. |
| `MODULE_RELAX_relax_new_relax` | `INHERITED_BOTH` | All three fail only `Test_RELAX.relax_new` at the same assertion site. |

The two very large raw-log families (`MODULE_BASE_math_sphbes`, about
153 MB total; `MODULE_NAO_real_gaunt_table`, about 86 MB total) remain in the
immutable fish result and are covered by its verified manifest.  They are not
duplicated into Git; their raw hashes are recorded in `observations.json` and
their reviewed failure tails are summarized above.  The small raw logs used
during review were copied to local scratch for inspection but are not added to
the branch; the authoritative copies remain under the verified fish result.

Applying `manual-overrides.csv` to an immutable copy should yield the
following non-blocking counts without rerunning or mutating any raw test:

- `PASS_ALL`: 203
- `MERGE_PASS_PARENT_NONPASS`: 59
- `INHERITED_BOTH`: 28
- `INHERITED_SOC`: 16
- `ENVIRONMENT_REPRODUCED_ALL`: 7
- blocking classifications: 0
