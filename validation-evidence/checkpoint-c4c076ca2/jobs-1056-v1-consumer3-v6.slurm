#!/bin/bash
#SBATCH --job-name=v1-consumer3
#SBATCH --partition=debug
#SBATCH --nodes=1
#SBATCH --ntasks=8
#SBATCH --cpus-per-task=1
#SBATCH --mem=64G
#SBATCH --output=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260801-c4c076ca2-v3/logs/v1consumer3-%j.out

source /opt/intel/oneapi/setvars.sh >/dev/null 2>&1
source /home/bhj/software/abacus-librpa-env.sh >/dev/null 2>&1
export LD_LIBRARY_PATH=/home/bhj/LibRPA-qsgw/build:$LD_LIBRARY_PATH

set -euo pipefail
RUN=/home/bhj/ai-runs/abacus-master-ghj-soc-merge-20260801-c4c076ca2-v3
SRC=$RUN/validation/v1-consumer   # abacus-merge / abacus-master + assembled datasets
RESULT=$RUN/validation/v1-consumer-c
CHI0=/home/bhj/LibRPA-qsgw/build/chi0_main.exe
TESTCASE=/home/bhj/LibRPA-qsgw/regression_tests/testcases/rpa_abacus_BN_headwing_sym_shrink_v1_libri

test ! -e "$RESULT"
install -d -m 0755 "$RESULT"
exec > >(tee -a "$RESULT/console.log") 2>&1
echo "job_id=$SLURM_JOB_ID start=$(date -Is) host=$(hostname)"
export OMP_NUM_THREADS=1
JOBSTART=$(date +%s)

# reuse the assembled datasets from job 1050 (merge/master v1 outputs already
# swapped in); rebuild the run dirs fresh to guarantee no stale outputs
run_consumer() {
    local tag=$1
    local wd=$RESULT/consumer-$tag
    install -d -m 0755 "$wd/librpa"
    # assemble dataset: baseline skeleton + ABACUS v1 outputs swapped in.
    # ABACUS-produced: v1_coulomb/v1_Cs/basis/vxc/stru/bz_sampling.
    # Baseline-provided aux (kept): band_out, KS_eigenvector_*.dat,
    # velocity_matrix, k_path_info (pyatb head/wing meanfield).
    install -d -m 0755 "$wd/dataset"
    tar xzf "$TESTCASE/dataset.tar.gz" -C "$wd/"
    rm -f "$wd/dataset"/v1_* "$wd/dataset"/basis_* "$wd/dataset"/vxc_out.dat           "$wd/dataset"/bz_sampling_out "$wd/dataset"/stru_out
    local src=$SRC/abacus-$tag
    local n=0
    for pat in 'v1_coulomb_full_iq_*_rank0.dat' 'v1_coulomb_cut_iq_*_rank0.dat'                'v1_Cs_data_*.txt' 'v1_Cs_shrinked_data_*.txt' 'v1_shrink_sinvS_*.txt'                'basis_wfc_out' 'basis_aux_out' 'basis_aux_shrink_out'                'bz_sampling_out' 'stru_out' 'vxc_out.dat'; do
        for f in $(find "$src" -name "$pat" 2>/dev/null); do
            cp "$f" "$wd/dataset/"; n=$((n+1))
        done
    done
    echo "$tag: swapped $n ABACUS v1 files"
    for req in band_out KS_eigenvector_0.dat v1_Cs_data_0.txt                v1_coulomb_full_iq_1_rank0.dat basis_wfc_out stru_out                vxc_out.dat bz_sampling_out; do
        test -f "$wd/dataset/$req" || { echo "MISSING $req in $tag dataset"; exit 1; }
    done
    cp "$TESTCASE/librpa/librpa.in" "$wd/librpa/librpa.in"
    test ! -e "$wd/librpa/librpa.out"
    ( cd "$wd/librpa" && mpirun -np 4 "$CHI0" > librpa.out 2> librpa.err )
    local rc=$?
    echo "$tag chi0 exit=$rc"
    test $rc -eq 0
    test -f "$wd/librpa/librpa.out"
    stat -c '%y %s %n' "$wd/librpa/librpa.out" | tee -a "$RESULT/output-stats.txt"
    sha256sum "$wd/librpa/librpa.out" | tee -a "$RESULT/output-hashes.txt"
    grep -m2 'Global ID of master process' "$wd/librpa/librpa.out"
    echo "$tag CONSUMER_OK"
}
run_consumer merge
run_consumer master

echo "=== compare librpa.out merge vs master ==="
if cmp -s "$RESULT/consumer-merge/librpa/librpa.out" "$RESULT/consumer-master/librpa/librpa.out"; then
    echo "librpa.out IDENTICAL"
else
    echo "librpa.out DIFFERS"
    diff <(head -6 "$RESULT/consumer-merge/librpa/librpa.out") \
         <(head -6 "$RESULT/consumer-master/librpa/librpa.out") | head -8
fi
for i in 0 1 2 3; do
    a="$RESULT/consumer-merge/librpa/librpa_para_nprocs_4_myid_$i.out"
    b="$RESULT/consumer-master/librpa/librpa_para_nprocs_4_myid_$i.out"
    if [ -f "$a" ] && [ -f "$b" ]; then
        cmp -s "$a" "$b" && echo "para_myid_$i IDENTICAL" || echo "para_myid_$i DIFFERS"
    else
        echo "para_myid_$i missing"
    fi
done
echo "jobstart_epoch=$JOBSTART"
echo "ALL_DONE end=$(date -Is)"
