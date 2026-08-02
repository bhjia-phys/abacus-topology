#!/bin/bash
#SBATCH --job-name=chk-build3
#SBATCH --partition=debug
#SBATCH --nodes=1
#SBATCH --ntasks=48
#SBATCH --cpus-per-task=1
#SBATCH --mem=96G
#SBATCH --output=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260801-c4c076ca2-v3/logs/build-%j.out

source /opt/intel/oneapi/setvars.sh >/dev/null 2>&1
source /home/bhj/software/abacus-librpa-env.sh >/dev/null 2>&1

set -euo pipefail
RUN=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260801-c4c076ca2-v3
OLD=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260731-269ac8a8-v3
COMMIT=c4c076ca29b2ab1fd9397523385ca6cc88f338ba
RESULT=$RUN/validation/checkpoint-build

test ! -e "$RESULT"
install -d -m 0755 "$RESULT" "$RUN/logs" "$RUN/source"
exec > >(tee -a "$RESULT/console.log") 2>&1
echo "job_id=$SLURM_JOB_ID"
echo "start=$(date -Is)"
echo "host=$(hostname)"
echo "commit=$COMMIT"

# ---- materialize the source tree from the bundle ----
cd "$RUN/source"
git init -q
git bundle unbundle /tmp/checkpoint-c4c076ca2.bundle > "$RESULT/bundle.log" 2>&1 || true
git checkout -q "$COMMIT" 2>&1 | tail -1
test "$(git rev-parse HEAD)" = "$COMMIT"
echo "source materialized at $(git rev-parse HEAD)"

# ---- configure: exact job-1005 parameter set ----
install -d -m 0755 "$RUN/build-release"
cmake \
  -S "$RUN/source" \
  -B "$RUN/build-release" \
  -DCMAKE_BUILD_TYPE=Release \
  -DBUILD_TESTING=ON \
  -DENABLE_MPI=ON \
  -DENABLE_OPENMP=ON \
  -DENABLE_LCAO=ON \
  -DENABLE_LIBRI=ON \
  -DLIBRI_DIR="$OLD/deps/libri-src" \
  -DLIBCOMM_DIR=/home/bhj/LibComm \
  -DENABLE_ELPA=ON \
  -DELPA_INCLUDE_DIR=/home/bhj/software/elpa-release_2025.01.001-build/include \
  -DELPA_LINK_LIBRARIES=/home/bhj/software/elpa-release_2025.01.001-build/lib/libelpa_openmp.so \
  -DENABLE_LIBXC=ON \
  -DLibxc_DIR=/home/bhj/software/libxc-6.2.2-build/lib/cmake/Libxc \
  -DENABLE_RAPIDJSON=OFF \
  -DENABLE_MLALGO=OFF \
  -DCMAKE_C_COMPILER=/opt/intel/oneapi/mpi/2021.16/bin/mpiicx \
  -DCMAKE_CXX_COMPILER=/opt/intel/oneapi/mpi/2021.16/bin/mpiicpx \
  -DCMAKE_Fortran_COMPILER=/opt/intel/oneapi/mpi/2021.16/bin/mpiifx \
  -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="$OLD/deps/googletest-src" \
  -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
  > "$RESULT/cmake.log" 2>&1 || { echo CMAKE_FAIL; tail -12 "$RESULT/cmake.log"; exit 1; }
echo "cmake OK"

# ---- build ----
make -C "$RUN/build-release" -j 48 > "$RESULT/build.log" 2>&1 || { echo BUILD_FAIL; tail -12 "$RESULT/build.log"; exit 1; }
echo "build OK"
BIN=$(find "$RUN/build-release" -maxdepth 1 -type f -perm -u+x -name 'abacus_*' | head -1)
sha256sum "$BIN" | tee "$RESULT/executable.sha256"

# ---- focused ctest: RI/symmetry/IO + new ABF/nspin4 tests ----
export OMP_NUM_THREADS=1
ctest --test-dir "$RUN/build-release" -R 'MODULE_RI_EXX_SYMMETRY|MODULE_IO_single_R|MODULE_RI_abfs|MODULE_RI_ewald' \
    --output-on-failure > "$RESULT/ctest-focused.log" 2>&1 || true
tail -6 "$RESULT/ctest-focused.log"

echo "ALL_DONE"
echo "end=$(date -Is)"
