#!/bin/bash
#SBATCH --job-name=union-soc46
#SBATCH --partition=debug
#SBATCH --nodes=1
#SBATCH --ntasks=8
#SBATCH --cpus-per-task=1
#SBATCH --mem=32G
#SBATCH --output=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260801-c4c076ca2-v3/logs/unionsoc46-%j.out

source /opt/intel/oneapi/setvars.sh >/dev/null 2>&1
source /home/bhj/software/abacus-librpa-env.sh >/dev/null 2>&1
set -euo pipefail
RUN=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260801-c4c076ca2-v3
OLD=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260731-269ac8a8-v3
RESULT=$RUN/validation/union-soc46
SOC_BUILD=$OLD/soc-parent-build

test ! -e "$RESULT"
install -d -m 0755 "$RESULT"
exec > >(tee -a "$RESULT/console.log") 2>&1
echo "job_id=$SLURM_JOB_ID start=$(date -Is) host=$(hostname)"
export OMP_NUM_THREADS=1

# the 46 merge-failed tests from the union table
grep ',SOC_CONFIG_UNAVAILABLE$' "$RUN/validation/union-3tree/union-table.csv" \
  | cut -d, -f1 > "$RESULT/merge-failed-list.txt"
echo "tests to verify on soc parent: $(wc -l < "$RESULT/merge-failed-list.txt")"

echo "test,soc_rc,soc_sig" > "$RESULT/soc-results.csv"
mapfile -t TESTS < "$RESULT/merge-failed-list.txt"
for t in "${TESTS[@]}"; do
    [ -z "$t" ] && continue
    ctest --test-dir "$SOC_BUILD" -R "^${t}$" < /dev/null > "$RESULT/soc-$t.log" 2>&1 || true
    rc=$?
    if grep -qE 'tests passed.*0 tests failed' "$RESULT/soc-$t.log"; then
        sig="PASS"
    else
        sig=$(grep -oE 'Death test|failed to die|Segmentation|SEGFAULT|PMI server not found|No such file or directory|Permission denied|error:|Error|Fatal|Mismatch|convergence|timed out|Not Run|Timeout' "$RESULT/soc-$t.log" | sort -u | tr '\n' ';' | head -c 120)
    fi
    echo "$t,$rc,$sig" >> "$RESULT/soc-results.csv"
done

echo "=== soc-side results for the 46 merge-failed tests ==="
cut -d, -f3 "$RESULT/soc-results.csv" | sort | uniq -c | sort -rn | head -12
echo "=== merge-fail but soc-PASS (potential regressions) ==="
grep ',PASS$' "$RESULT/soc-results.csv" | head -10
echo "ALL_DONE end=$(date -Is)"
