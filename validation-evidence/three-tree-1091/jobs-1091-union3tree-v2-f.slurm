#!/bin/bash
#SBATCH --job-name=union3tree-v2
#SBATCH --partition=debug
#SBATCH --nodes=1
#SBATCH --ntasks=16
#SBATCH --cpus-per-task=1
#SBATCH --mem=64G
#SBATCH --output=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260802-three-tree-v1/logs/union3tree-v2-%j.out

source /opt/intel/oneapi/setvars.sh >/dev/null 2>&1
source /home/bhj/software/abacus-librpa-env.sh >/dev/null 2>&1

set -euo pipefail
RUN=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260802-three-tree-v1
OLD=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260731-269ac8a8-v3
RESULT=$RUN/validation/union-3tree-v2
MERGE_BUILD=$RUN/../abacus-master-ghj-soc-merge-20260801-c4c076ca2-v3/build-release
SOC_BUILD=$OLD/soc-parent-build
MASTER_BUILD=$RUN/master-test-build-v7
MERGE_COMMIT=c4c076ca29b2ab1fd9397523385ca6cc88f338ba
SOC_COMMIT=4aa46ed65caf72fca593bf1a99a4e1705526f1a8
MASTER_COMMIT=dd4216653386d32f79e3219f3ea5dd2d229c1c5a

test ! -e "$RESULT"
install -d -m 0755 "$RESULT"
exec > >(tee -a "$RESULT/console.log") 2>&1
echo "job_id=$SLURM_JOB_ID start=$(date -Is) host=$(hostname)"
export OMP_NUM_THREADS=1

# ---- registrations via json-v1 ----
reg_list() { # $1=build dir -> names on stdout
    local build=$1
    # Preferred: ctest -N --show-only=json-v1. Tested in Slurm jobs on fish:
    # it exits 1 with empty output there (works on the login node), so this
    # harness records that limitation and falls back to the plain -N text
    # listing parsed with a fixed regex (same registration source: ctest).
    set +e
    ctest --test-dir "$build" -N --show-only=json-v1 > "$RESULT/reg-json-$(basename $build).json" 2>/dev/null
    local rc=$?
    set -e
    if [ $rc -eq 0 ] && [ -s "$RESULT/reg-json-$(basename $build).json" ]; then
        python3 - "$RESULT/reg-json-$(basename $build).json" <<'PYEOF'
import json,sys
d=json.load(open(sys.argv[1]))
print('\n'.join(t['name'] for t in d['tests']))
PYEOF
    else
        echo "NOTE: json-v1 unavailable in Slurm job (rc=$rc); using ctest -N text registration" >&2
        ctest --test-dir "$build" -N 2>/dev/null | grep -oE 'Test[[:space:]]+#[0-9]+: [^ ]+' | sed 's/.*: //'
    fi
}
reg_list "$MERGE_BUILD" | sort -u > "$RESULT/merge-registered.txt"
reg_list "$SOC_BUILD" | sort -u > "$RESULT/soc-registered.txt"
reg_list "$MASTER_BUILD" | sort -u > "$RESULT/master-registered.txt"
echo "registered: merge=$(wc -l < $RESULT/merge-registered.txt) soc=$(wc -l < $RESULT/soc-registered.txt) master=$(wc -l < $RESULT/master-registered.txt)"

# the union list = merge registered (all of them, incl. integration)
cp "$RESULT/merge-registered.txt" "$RESULT/testlist.txt"
echo "union test list: $(wc -l < "$RESULT/testlist.txt")"

# ---- run one test on one tree; record rc + full log + normalized signature ----
run_tree() { # $1=build $2=test $3=outprefix -> true exit code
    local build=$1 t=$2 pre=$3
    set +e
    timeout 300 ctest --test-dir "$build" -R "^${t}$" < /dev/null > "$RESULT/$pre-$t.log" 2>&1
    local rc=$?
    set -e
    echo "$rc"
}

normalize_sig() { # $1 = log -> normalized signature (paths/timestamps/pids/addresses stripped)
    sed -E \
      -e 's#/home/bhj/ai-runs/[^ ]+##g' \
      -e 's#[0-9]{4}-[0-9]{2}-[0-9]{2}[ T][0-9:]+##g' \
      -e 's/0x[0-9a-fA-F]+/ADDR/g' \
      -e 's/pid [0-9]+/pid PID/g' \
      -e 's/\[[0-9]+\]/[N]/g' \
      -e 's/[0-9]+\.[0-9]+ sec/N.N sec/g' \
      "$1" | sort -u
}

echo "test,merge_reg,soc_reg,master_reg,merge_rc,soc_rc,master_rc,merge_status,soc_status,master_status,merge_sig_sha,soc_sig_sha,master_sig_sha,classification,reason" > "$RESULT/union-table.csv"

declare -A REG
for b in merge soc master; do
    while read -r n; do REG["$b:$n"]=1; done < "$RESULT/${b}-registered.txt"
done

mapfile -t TESTS < "$RESULT/testlist.txt"
NFAIL=0; NUNK=0; NPASS=0; NINH=0
: > "$RESULT/merge-failures.txt"
for t in "${TESTS[@]}"; do
    [ -z "$t" ] && continue
    mr=$(run_tree "$MERGE_BUILD" "$t" merge)
    sr="NA"; mtr="NA"
    if [ -n "${REG[soc:$t]:-}" ]; then sr=$(run_tree "$SOC_BUILD" "$t" soc); else sr="NOT_REG"; fi
    if [ -n "${REG[master:$t]:-}" ]; then mtr=$(run_tree "$MASTER_BUILD" "$t" master); else mtr="NOT_REG"; fi

    ms="PASS"; ss="PASS"; mts="PASS"
    [ "$mr" = "124" ] && ms="TIMEOUT"
    [ "$sr" = "124" ] && ss="TIMEOUT"
    [ "$mtr" = "124" ] && mts="TIMEOUT"
    [ "$mr" != "0" ] && [ "$mr" != "124" ] && ms="FAIL"
    [ "$sr" != "0" ] && [ "$sr" != "124" ] && ss="FAIL"
    [ "$mtr" != "0" ] && [ "$mtr" != "124" ] && mts="FAIL"
    [ "$sr" = "NOT_REG" ] && ss="NOT_REG"
    [ "$mtr" = "NOT_REG" ] && mts="NOT_REG"

    msha=$(normalize_sig "$RESULT/merge-$t.log" | sha256sum | cut -d' ' -f1)
    ssha="NA"; tsha="NA"
    [ -f "$RESULT/soc-$t.log" ]    && ssha=$(normalize_sig "$RESULT/soc-$t.log" | sha256sum | cut -d' ' -f1)
    [ -f "$RESULT/master-$t.log" ] && tsha=$(normalize_sig "$RESULT/master-$t.log" | sha256sum | cut -d' ' -f1)

    cls="PASS_ALL"; reason="all rc=0"
    if [ "$ms" = "FAIL" ]; then
        if [ "$ss" = "FAIL" ] && [ "$msha" = "$ssha" ]; then
            cls="INHERITED_SOC"; reason="same normalized signature as soc parent"
        elif [ "$mts" = "FAIL" ] && [ "$msha" = "$tsha" ]; then
            cls="INHERITED_MASTER"; reason="same normalized signature as master parent"
        elif [ "$ss" = "PASS" ] || [ "$mts" = "PASS" ]; then
            cls="MERGE_REGRESSION"; reason="merge fails, at least one parent passes"
        elif [ "$ss" = "NOT_REG" ] && [ "$mts" = "NOT_REG" ]; then
            cls="UNKNOWN"; reason="not registered on either parent tree"
        elif [ "$ms" = "TIMEOUT" ]; then
            cls="UNKNOWN"; reason="merge test timed out (300s) - manual review needed"
        else
            cls="UNKNOWN"; reason="no matching parent failure signature (manual review needed)"
        fi
    fi
    case "$cls" in
        MERGE_REGRESSION) NFAIL=$((NFAIL+1)); echo "$t" >> "$RESULT/merge-failures.txt";;
        UNKNOWN) NUNK=$((NUNK+1)); echo "$t" >> "$RESULT/merge-failures.txt";;
        PASS_ALL) NPASS=$((NPASS+1));;
        INHERITED*) NINH=$((NINH+1));;
    esac
    echo "$t,${REG[merge:$t]:-0},${REG[soc:$t]:-0},${REG[master:$t]:-0},$mr,$sr,$mtr,$ms,$ss,$mts,$msha,$ssha,$tsha,$cls,$reason" >> "$RESULT/union-table.csv"
done

echo "=== summary ==="
echo "PASS_ALL=$NPASS INHERITED=$NINH MERGE_REGRESSION=$NFAIL UNKNOWN=$NUNK"
echo "summary: PASS_ALL=$NPASS INHERITED=$NINH MERGE_REGRESSION=$NFAIL UNKNOWN=$NUNK" > "$RESULT/summary.txt"
if [ "$NFAIL" -gt 0 ] || [ "$NUNK" -gt 0 ]; then
    echo "GATE_FAIL: merge-regression or unknown present"
    cat "$RESULT/merge-failures.txt"
    exit 1
fi
printf '%s\n' "$MERGE_COMMIT" > "$RESULT/PASS"
echo "ALL_DONE end=$(date -Is)"
