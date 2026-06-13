#!/usr/bin/env bash
set -euo pipefail

N_LIST="${N_LIST:-2048 4096}"
NP_LIST="${NP_LIST:-1 2 4 8}"
VARIANTS="${VARIANTS:-mpi mpi_tiled}"
SEQ_VARIANTS="${SEQ_VARIANTS:-seq seq_tiled}"
REPEAT="${REPEAT:-5}"
SEED="${SEED:-2026}"
TILE="${TILE:-32}"
MAX_RAM_GB="${MAX_RAM_GB:-5.0}"
OUTPUT_DIR="${OUTPUT_DIR:-outputs}"
RAW="${RAW:-results/raw_runs.csv}"
CHECKSUMS="${CHECKSUMS:-results/checksums.csv}"
SUMMARY="${SUMMARY:-results/summary.csv}"
CHECKSUM_SUMMARY="${CHECKSUM_SUMMARY:-results/checksums_summary.csv}"
MPI_ROOT_FLAGS=()
if [[ "$(id -u)" == "0" ]]; then
  MPI_ROOT_FLAGS=(--allow-run-as-root)
fi

mkdir -p build results outputs data docs/charts
make

HEADER="variant,n,processes,nodes,tile,repeat_index,seconds,checksum_sum,checksum_abs,checksum_max_abs,checksum_hash,run_label,mapping,pe,threads_per_process,sample_file,full_file"
if [[ ! -f "$RAW" ]]; then
  echo "$HEADER" > "$RAW"
fi

run_case() {
  local variant="$1"
  local n="$2"
  local np="$3"
  local tmp
  tmp="$(mktemp)"
  echo "Running variant=$variant N=$n np=$np repeat=$REPEAT"
  mpirun "${MPI_ROOT_FLAGS[@]}" -np "$np" --bind-to core ./build/matrix_hpc \
    --variant "$variant" \
    --n "$n" \
    --seed "$SEED" \
    --repeat "$REPEAT" \
    --tile "$TILE" \
    --max-ram-gb "$MAX_RAM_GB" \
    --output-dir "$OUTPUT_DIR" \
    --save-sample \
    --checksum-file "$CHECKSUMS" \
    --run-label process_sweep \
    --mapping bind_to_core \
    --pe 1 | tee "$tmp"
  grep '^RESULT,' "$tmp" | sed 's/^RESULT,//' >> "$RAW"
  rm -f "$tmp"
}

for n in $N_LIST; do
  for variant in $SEQ_VARIANTS; do
    run_case "$variant" "$n" 1
  done

  for variant in $VARIANTS; do
    for np in $NP_LIST; do
      run_case "$variant" "$n" "$np"
    done
  done
done

python3 scripts/summarize_results.py --raw "$RAW" --summary "$SUMMARY" --checksum-summary "$CHECKSUM_SUMMARY"
