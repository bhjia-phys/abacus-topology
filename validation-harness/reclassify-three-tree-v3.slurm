#!/bin/bash
#SBATCH --job-name=union3tree-reclass
#SBATCH --partition=debug
#SBATCH --nodes=1
#SBATCH --ntasks=1
#SBATCH --cpus-per-task=1
#SBATCH --mem=8G
#SBATCH --time=00:30:00

# Required sbatch --export variables:
# SOURCE_RESULT, RESULT, HARNESS, SEED_TABLE, MERGE_COMMIT.
# Optional: OVERRIDES (an empty value means no manual overrides).

set -euo pipefail
: "${SOURCE_RESULT:?}"
: "${RESULT:?}"
: "${HARNESS:?}"
: "${SEED_TABLE:?}"
: "${MERGE_COMMIT:?}"
OVERRIDES=${OVERRIDES:-}

for path in "${SOURCE_RESULT}" "${RESULT}"; do
    case "${path}" in
        /data/users/bhj/ai-runs/*) ;;
        *) echo "result paths must be under /data/users/bhj/ai-runs" >&2; exit 2;;
    esac
done
test -d "${SOURCE_RESULT}"
test ! -e "${RESULT}"
test -f "${SOURCE_RESULT}/MANIFEST.sha256"
test -z "${OVERRIDES}" || test -f "${OVERRIDES}"

JOB1091_SEED_TABLE="${SEED_TABLE}" \
  python3 "${HARNESS}/test_three_tree_union.py" -v

command=(
  python3 "${HARNESS}/three_tree_union.py" reclassify
  --source-result "${SOURCE_RESULT}"
  --result-dir "${RESULT}"
  --merge-commit "${MERGE_COMMIT}"
)
if [ -n "${OVERRIDES}" ]; then
    command+=(--overrides "${OVERRIDES}")
fi

set +e
"${command[@]}"
rc=$?
set -e
(cd "${RESULT}" && sha256sum -c MANIFEST.sha256)
if [ "${rc}" -eq 0 ]; then
    test -f "${RESULT}/PASS"
    test ! -e "${RESULT}/FAIL"
else
    test -f "${RESULT}/FAIL"
    test ! -e "${RESULT}/PASS"
fi
echo "RECLASSIFY_RC=${rc}"
exit "${rc}"
