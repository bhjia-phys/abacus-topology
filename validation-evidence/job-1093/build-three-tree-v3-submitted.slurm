#!/bin/bash
#SBATCH --job-name=build3tree-v3
#SBATCH --partition=debug
#SBATCH --nodes=1
#SBATCH --ntasks=96
#SBATCH --cpus-per-task=1
#SBATCH --mem=300G
#SBATCH --time=04:00:00

# Required sbatch --export variables:
# RUN_ROOT, BASE_BUNDLE, UPDATE_BUNDLE, MERGE_COMMIT, SOC_COMMIT, MASTER_COMMIT,
# MERGE_LIBRI_DIR, SOC_LIBRI_DIR, MASTER_LIBRI_DIR, GOOGLETEST_DIR.

source /opt/intel/oneapi/setvars.sh >/dev/null 2>&1
source /home/bhj/software/abacus-librpa-env.sh >/dev/null 2>&1

set -euo pipefail
: "${RUN_ROOT:?}"
: "${BASE_BUNDLE:?}"
: "${UPDATE_BUNDLE:?}"
: "${MERGE_COMMIT:?}"
: "${SOC_COMMIT:?}"
: "${MASTER_COMMIT:?}"
: "${MERGE_LIBRI_DIR:?}"
: "${SOC_LIBRI_DIR:?}"
: "${MASTER_LIBRI_DIR:?}"
: "${GOOGLETEST_DIR:?}"

case "${RUN_ROOT}" in
    /data/users/bhj/ai-runs/*) ;;
    *) echo "RUN_ROOT must be under /data/users/bhj/ai-runs" >&2; exit 2;;
esac
test ! -e "${RUN_ROOT}"
test -f "${BASE_BUNDLE}"
test -f "${UPDATE_BUNDLE}"
test -d "${MERGE_LIBRI_DIR}"
test -d "${SOC_LIBRI_DIR}"
test -d "${MASTER_LIBRI_DIR}"
test -d "${GOOGLETEST_DIR}"

install -d -m 0755 "${RUN_ROOT}/logs" "${RUN_ROOT}/provenance"
exec > >(tee "${RUN_ROOT}/logs/build3tree-v3-${SLURM_JOB_ID}.out") 2>&1
echo "job_id=${SLURM_JOB_ID} start=$(date -Is) host=$(hostname)"
echo "run_root=${RUN_ROOT}"
df -h "${RUN_ROOT}"

sha256sum "${BASE_BUNDLE}" "${UPDATE_BUNDLE}" \
  | tee "${RUN_ROOT}/provenance/bundles.sha256"
{
    echo "merge=${MERGE_COMMIT}"
    echo "soc=${SOC_COMMIT}"
    echo "master=${MASTER_COMMIT}"
} > "${RUN_ROOT}/provenance/commits.txt"

materialize() {
    local role=$1
    local commit=$2
    local source_dir="${RUN_ROOT}/source-${role}"
    git clone -q "${BASE_BUNDLE}" "${source_dir}"
    git -C "${source_dir}" fetch -q "${UPDATE_BUNDLE}" HEAD
    git -C "${source_dir}" checkout -q "${commit}"
    test "$(git -C "${source_dir}" rev-parse HEAD)" = "${commit}"
    test -z "$(git -C "${source_dir}" status --porcelain=v1)"
}

materialize merge "${MERGE_COMMIT}"
materialize soc "${SOC_COMMIT}"
materialize master "${MASTER_COMMIT}"

configure() {
    local role=$1 libri=$2
    local source_dir="${RUN_ROOT}/source-${role}"
    local build_dir="${RUN_ROOT}/build-${role}"
    install -d -m 0755 "${build_dir}"
    cmake \
      -S "${source_dir}" \
      -B "${build_dir}" \
      -DCMAKE_BUILD_TYPE=Release \
      -DBUILD_TESTING=ON \
      -DENABLE_MPI=ON \
      -DENABLE_OPENMP=ON \
      -DENABLE_LCAO=ON \
      -DENABLE_LIBRI=ON \
      -DLIBRI_DIR="${libri}" \
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
      -DFETCHCONTENT_SOURCE_DIR_GOOGLETEST="${GOOGLETEST_DIR}" \
      -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
      > "${RUN_ROOT}/logs/configure-${role}.log" 2>&1
    cp "${build_dir}/CMakeCache.txt" "${RUN_ROOT}/provenance/CMakeCache-${role}.txt"
}

configure merge "${MERGE_LIBRI_DIR}"
configure soc "${SOC_LIBRI_DIR}"
configure master "${MASTER_LIBRI_DIR}"

build_strict() {
    local role=$1
    make -C "${RUN_ROOT}/build-${role}" -j 32 \
      > "${RUN_ROOT}/logs/build-${role}.log" 2>&1
}

build_master_expected_exclusion() {
    local build_dir="${RUN_ROOT}/build-master"
    set +e
    make -C "${build_dir}" -k -j 32 \
      > "${RUN_ROOT}/logs/build-master.log" 2>&1
    local rc=$?
    set -e
    echo "master_make_rc=${rc}" | tee "${RUN_ROOT}/provenance/master-build-status.txt"
    if [ "${rc}" -ne 0 ]; then
        # The pinned parent is known to fail only this target against its
        # native LibRI headers.  Record the exact diagnostics and reject any
        # additional compiler failure.
        grep -E 'error:|Error [0-9]+|FAILED:' "${RUN_ROOT}/logs/build-master.log" \
          > "${RUN_ROOT}/provenance/master-build-errors.txt" || true
        grep -q 'MODULE_RI_ri_cv_io_test' "${RUN_ROOT}/logs/build-master.log"
        if grep -E 'error:|Error [0-9]+|FAILED:' "${RUN_ROOT}/logs/build-master.log" \
             | grep -v -E 'MODULE_RI_ri_cv_io_test|LRI_CV_Tools|write_ri_cv|Error 1|Error 2' \
             > "${RUN_ROOT}/provenance/master-unexpected-errors.txt"; then
            echo "MASTER_UNEXPECTED_BUILD_FAILURE"
            cat "${RUN_ROOT}/provenance/master-unexpected-errors.txt"
            return 1
        fi
        echo "expected_unbuilt_target=MODULE_RI_ri_cv_io_test" \
          >> "${RUN_ROOT}/provenance/master-build-status.txt"
    fi
}

set +e
build_strict merge & merge_pid=$!
build_strict soc & soc_pid=$!
build_master_expected_exclusion & master_pid=$!
wait "${merge_pid}"; merge_rc=$?
wait "${soc_pid}"; soc_rc=$?
wait "${master_pid}"; master_rc=$?
set -e
echo "parallel_build_rc merge=${merge_rc} soc=${soc_rc} master_policy=${master_rc}"
test "${merge_rc}" -eq 0
test "${soc_rc}" -eq 0
test "${master_rc}" -eq 0

for role in merge soc master; do
    build_dir="${RUN_ROOT}/build-${role}"
    bin=$(find "${build_dir}" -maxdepth 1 -type f -perm -u+x -name 'abacus_*' | head -1)
    test -n "${bin}"
    sha256sum "${bin}" | tee "${RUN_ROOT}/provenance/executable-${role}.sha256"
    ctest --test-dir "${build_dir}" -N > "${RUN_ROOT}/provenance/registration-${role}.txt"
    tail -1 "${RUN_ROOT}/provenance/registration-${role}.txt"
done

python3 --version | tee "${RUN_ROOT}/provenance/python-version.txt"
cmake --version | head -1 | tee "${RUN_ROOT}/provenance/cmake-version.txt"
icpx --version | head -1 | tee "${RUN_ROOT}/provenance/compiler-version.txt"
mpirun --version | head -2 | tee "${RUN_ROOT}/provenance/mpi-version.txt"

printf '%s\n' "${MERGE_COMMIT}" > "${RUN_ROOT}/BUILD_PASS"
echo "ALL_DONE end=$(date -Is)"
