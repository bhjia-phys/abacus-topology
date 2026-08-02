#!/bin/bash
#SBATCH --job-name=union3tree
#SBATCH --partition=debug
#SBATCH --nodes=1
#SBATCH --ntasks=16
#SBATCH --cpus-per-task=1
#SBATCH --mem=64G
#SBATCH --output=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260801-c4c076ca2-v3/logs/union3tree-%j.out

source /opt/intel/oneapi/setvars.sh >/dev/null 2>&1
source /home/bhj/software/abacus-librpa-env.sh >/dev/null 2>&1

set -euo pipefail
RUN=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260801-c4c076ca2-v3
OLD=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260731-269ac8a8-v3
RESULT=$RUN/validation/union-3tree
MERGE_BUILD=$RUN/build-release
SOC_BUILD=$OLD/soc-parent-build
MASTER_BUILD=$OLD/master-parent-build

test ! -e "$RESULT"
install -d -m 0755 "$RESULT"
exec > >(tee -a "$RESULT/console.log") 2>&1
echo "job_id=$SLURM_JOB_ID start=$(date -Is) host=$(hostname)"
export OMP_NUM_THREADS=1

# ---- the common test list = this checkpoint's registered tests ----
ctest --test-dir "$MERGE_BUILD" -N 2>/dev/null \
  | grep -oE 'Test[[:space:]]+#[0-9]+: [^ ]+' | sed 's/.*: //' | sort -u \
  | grep -vE '^(01_PW|03_NAO_multik|07_OFDFT|11_PW_GPU|17_DS_DFTU)$' \
  > "$RESULT/testlist.txt"
echo "integration tests excluded (Autotest-driven; recorded separately in job 1023 raw result)" > "$RESULT/integration-excluded.txt"
echo "common test list: $(wc -l < "$RESULT/testlist.txt") tests"

# ---- per-test execution on merge and soc-parent builds ----
echo "test,merge_rc,soc_rc,master_rc,merge_sig,soc_sig,master_sig,classification" > "$RESULT/union-table.csv"

sig_of() { # $1 = log file -> error signature, no re-run
    local log=$1 out
    out=$(cat "$log" 2>/dev/null || true)
    if echo "$out" | grep -qE 'tests passed.*0 tests failed'; then
        echo "PASS"
    else
        echo "$out" | grep -oE 'Death test|failed to die|Segmentation|SEGFAULT|PMI server not found|No such file or directory|Permission denied|error:|Error|Fatal|Mismatch|convergence|timed out|Not Run|Timeout' | sort -u | tr '\n' ';' | head -c 120
    fi
}

mapfile -t TESTS < "$RESULT/testlist.txt"
for t in "${TESTS[@]}"; do
    [ -z "$t" ] && continue
    ctest --test-dir "$MERGE_BUILD" -R "^${t}$" < /dev/null > "$RESULT/merge-$t.log" 2>&1 || true
    mrc=$?
    msig=$(sig_of "$RESULT/merge-$t.log")
    if ctest --test-dir "$SOC_BUILD" -N 2>/dev/null | grep -q " $t$"; then
        ctest --test-dir "$SOC_BUILD" -R "^${t}$" < /dev/null > "$RESULT/soc-$t.log" 2>&1 || true
        src=$?
        ssig=$(sig_of "$RESULT/soc-$t.log")
    else
        src="NA"; ssig="CONFIG_UNAVAILABLE"
    fi
    # classification
    cls="UNKNOWN"
    if [ "$msig" = "PASS" ] && [ "$ssig" = "PASS" ]; then cls="PASS_ALL"
    elif [ "$msig" != "PASS" ] && [ "$ssig" != "PASS" ] && [ "$msig" = "$ssig" ]; then cls="INHERITED_SOC_PARENT"
    elif [ "$msig" != "PASS" ] && [ "$ssig" = "CONFIG_UNAVAILABLE" ]; then cls="SOC_CONFIG_UNAVAILABLE"
    elif [ "$msig" != "PASS" ] && [ "$ssig" = "PASS" ]; then cls="MERGE_REGRESSION"
    fi
    echo "$t,$mrc,$src,NA,$msig,$ssig,NA,$cls" >> "$RESULT/union-table.csv"
done

# ---- summary ----
echo "=== classification summary ==="
cut -d, -f8 "$RESULT/union-table.csv" | sort | uniq -c | sort -rn
echo "=== MERGE_REGRESSION rows (if any) ==="
grep ',MERGE_REGRESSION$' "$RESULT/union-table.csv" || echo "(none)"
echo "ALL_DONE end=$(date -Is)"
