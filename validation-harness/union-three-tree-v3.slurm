#!/bin/bash
#SBATCH --job-name=union3tree-v3
#SBATCH --partition=debug
#SBATCH --nodes=1
#SBATCH --ntasks=16
#SBATCH --cpus-per-task=1
#SBATCH --mem=64G
#SBATCH --time=02:00:00

# Required environment variables are intentionally supplied by sbatch
# --export so the checked-in script has no mutable run path baked into it:
# RUN_ROOT, HARNESS, SEED_TABLE, MERGE_BUILD, SOC_BUILD, MASTER_BUILD,
# MERGE_COMMIT, SOC_COMMIT, MASTER_COMMIT.

source /opt/intel/oneapi/setvars.sh >/dev/null 2>&1
source /home/bhj/software/abacus-librpa-env.sh >/dev/null 2>&1

set -euo pipefail
: "${RUN_ROOT:?}"
: "${HARNESS:?}"
: "${SEED_TABLE:?}"
: "${MERGE_BUILD:?}"
: "${SOC_BUILD:?}"
: "${MASTER_BUILD:?}"
: "${MERGE_COMMIT:?}"
: "${SOC_COMMIT:?}"
: "${MASTER_COMMIT:?}"
TIMEOUT_SECONDS=${TIMEOUT_SECONDS:-900}
case "${TIMEOUT_SECONDS}" in
    ''|*[!0-9]*) echo "TIMEOUT_SECONDS must be a positive integer" >&2; exit 2;;
esac
test "${TIMEOUT_SECONDS}" -gt 0

test "${RUN_ROOT}" != /home/bhj
test "${RUN_ROOT}" != /data/users/bhj
case "${RUN_ROOT}" in
    /data/users/bhj/ai-runs/*) ;;
    *) echo "RUN_ROOT must be under /data/users/bhj/ai-runs" >&2; exit 2;;
esac

RESULT="${RUN_ROOT}/result"
test ! -e "${RESULT}"
install -d -m 0755 "${RUN_ROOT}/logs"
exec > >(tee "${RUN_ROOT}/logs/union3tree-v3-${SLURM_JOB_ID}.out") 2>&1

echo "job_id=${SLURM_JOB_ID} start=$(date -Is) host=$(hostname)"
echo "run_root=${RUN_ROOT}"
echo "merge_commit=${MERGE_COMMIT} soc_commit=${SOC_COMMIT} master_commit=${MASTER_COMMIT}"
export OMP_NUM_THREADS=1

JOB1091_SEED_TABLE="${SEED_TABLE}" \
  python3 "${HARNESS}/test_three_tree_union.py" -v

python3 "${HARNESS}/three_tree_union.py" run \
    --merge-build "${MERGE_BUILD}" \
    --soc-build "${SOC_BUILD}" \
    --master-build "${MASTER_BUILD}" \
    --merge-commit "${MERGE_COMMIT}" \
    --soc-commit "${SOC_COMMIT}" \
    --master-commit "${MASTER_COMMIT}" \
    --seed-table "${SEED_TABLE}" \
    --aliases "${HARNESS}/aliases.json" \
    --result-dir "${RESULT}" \
    --timeout "${TIMEOUT_SECONDS}"

test -f "${RESULT}/PASS"
(cd "${RESULT}" && sha256sum -c MANIFEST.sha256)
echo "ALL_DONE end=$(date -Is)"
