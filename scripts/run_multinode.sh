#!/usr/bin/env bash
set -euo pipefail

HOSTFILE="${HOSTFILE:-config/hosts}"
N_LIST="${N_LIST:-2048 4096}"
NODES_LIST="${NODES_LIST:-1 2 3}"
PPN="${PPN:-4}"
PE="${PE:-1}"
VARIANTS="${VARIANTS:-mpi mpi_tiled}"
REPEAT="${REPEAT:-5}"
SEED="${SEED:-2026}"
TILE="${TILE:-32}"
MAX_RAM_GB="${MAX_RAM_GB:-5.0}"
OUTPUT_DIR="${OUTPUT_DIR:-outputs}"
RAW="${RAW:-results/raw_runs.csv}"
CHECKSUMS="${CHECKSUMS:-results/checksums.csv}"
SUMMARY="${SUMMARY:-results/summary.csv}"
CHECKSUM_SUMMARY="${CHECKSUM_SUMMARY:-results/checksums_summary.csv}"
SSH_USER="${SSH_USER:-}"
REMOTE_DIR="${REMOTE_DIR:-$(pwd)}"
MPI_ROOT_FLAGS=()
if [[ "$(id -u)" == "0" ]]; then
  MPI_ROOT_FLAGS=(--allow-run-as-root)
fi
MPI_SSH_FLAGS=()
if [[ -n "$SSH_USER" ]]; then
  MPI_SSH_FLAGS=(--mca plm_rsh_args "-l ${SSH_USER}")
fi

mkdir -p build results outputs data docs/charts
make

if [[ ! -f "$HOSTFILE" ]]; then
  echo "Missing hostfile: $HOSTFILE"
  echo "Copy config/hosts.example to config/hosts and edit physical machine hostnames/IPs first."
  exit 2
fi

read_hosts() {
  grep -v '^[[:space:]]*$' "$HOSTFILE" \
    | grep -v '^[[:space:]]*#' \
    | sed 's/#.*//' \
    | awk '{print $1 " " $0}'
}

host_count="$(read_hosts | wc -l | tr -d ' ')"
if [[ "$host_count" -lt 1 ]]; then
  echo "No usable hosts found in $HOSTFILE"
  exit 2
fi

HEADER="variant,n,processes,nodes,tile,repeat_index,seconds,checksum_sum,checksum_abs,checksum_max_abs,checksum_hash,run_label,mapping,pe,threads_per_process,sample_file,full_file"
if [[ ! -f "$RAW" ]]; then
  echo "$HEADER" > "$RAW"
fi

run_case() {
  local variant="$1"
  local n="$2"
  local nodes="$3"
  local np=$((nodes * PPN))
  local tmp_hosts
  local tmp
  tmp_hosts="$(mktemp)"
  tmp="$(mktemp)"
  read_hosts | head -n "$nodes" | cut -d' ' -f2- > "$tmp_hosts"
  echo "Hosts used for this run:"
  cat "$tmp_hosts"
  echo "Running variant=$variant N=$n nodes=$nodes ppn=$PPN np=$np PE=$PE"
  mpirun "${MPI_ROOT_FLAGS[@]}" "${MPI_SSH_FLAGS[@]}" --hostfile "$tmp_hosts" -np "$np" --wdir "$REMOTE_DIR" --map-by "ppr:${PPN}:node:PE:${PE}" --bind-to core ./build/matrix_hpc \
    --variant "$variant" \
    --n "$n" \
    --seed "$SEED" \
    --repeat "$REPEAT" \
    --tile "$TILE" \
    --max-ram-gb "$MAX_RAM_GB" \
    --output-dir "$OUTPUT_DIR" \
    --save-sample \
    --checksum-file "$CHECKSUMS" \
    --run-label node_sweep \
    --mapping "ppr:${PPN}:node:PE:${PE}" \
    --pe "$PE" | tee "$tmp"
  grep '^RESULT,' "$tmp" | sed 's/^RESULT,//' >> "$RAW"
  rm -f "$tmp" "$tmp_hosts"
}

for n in $N_LIST; do
  for variant in $VARIANTS; do
    for nodes in $NODES_LIST; do
      if [[ "$nodes" -gt "$host_count" ]]; then
        echo "Skipping nodes=$nodes because $HOSTFILE only has $host_count usable hosts"
        continue
      fi
      run_case "$variant" "$n" "$nodes"
    done
  done
done

python3 scripts/summarize_results.py --raw "$RAW" --summary "$SUMMARY" --checksum-summary "$CHECKSUM_SUMMARY"
