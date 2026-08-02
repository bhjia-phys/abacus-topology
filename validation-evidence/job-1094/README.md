# Fish job 1094: fresh three-tree build gate

Job 1094 materialized and built merge checkpoint
`7ef8506a8bd4890e34d3e027660670bff3477ffb`, SOC parent
`4aa46ed65caf72fca593bf1a99a4e1705526f1a8`, and master parent
`dd4216653386d32f79e3219f3ea5dd2d229c1c5a` in the fresh immutable run
`/data/users/bhj/ai-runs/abacus-three-tree-7ef8506a8-20260802-codex-v5`.

The merge and SOC builds returned zero. The master `make -k` returned two, but
the parsed failed-target set was exactly the predeclared upstream defect
`MODULE_RI_ri_cv_io_test`; no other failed target was accepted. The build
policy returned zero and `BUILD_PASS` contains the exact merge checkpoint.
Registered CTest counts are merge 313, SOC 310, and master 287.

The CMake caches, bundle and executable hashes, compiler/MPI versions,
registrations, master diagnostics, console, and marker are preserved here.
`dependency-trees.txt` adds a post-run deterministic identity check for the
non-Git LibRI source directories: merge and SOC are byte-identical trees;
master uses its distinct compatible LibRI tree. This build gate alone does not
claim a union-test PASS; that is a separate Slurm job.
