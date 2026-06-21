#!/usr/bin/env bash
set -euo pipefail

HOSTFILE="${HOSTFILE:-config/hosts}"
SHAPES="${SHAPES:-2048x2048x2048 4096x4096x4096}"
NODES_LIST="${NODES_LIST:-1 2 3}"
PPN="${PPN:-4}"
PE="${PE:-1}"
VARIANTS="${VARIANTS:-mpi mpi_tiled mpi_2d}"
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
export HWLOC_COMPONENTS="${HWLOC_COMPONENTS:--gl,-opencl,-cuda,-nvml,-rsmi}"
unset DISPLAY WAYLAND_DISPLAY
detect_lan_cidr() {
  ip -o -4 route show scope link 2>/dev/null \
    | awk '$1 ~ /\// && $1 !~ /^127\./ && $1 !~ /^169\.254/ && $1 !~ /^172\.17\./ && !found {print $1; found=1}'
}
MPI_LAN_CIDR="${MPI_LAN_CIDR:-$(detect_lan_cidr)}"
MPI_LAN_CIDR="${MPI_LAN_CIDR:-10.11.64.0/19}"
MPI_SSH_FLAGS=()
if [[ -n "$SSH_USER" ]]; then
  MPI_SSH_FLAGS=(--mca plm_rsh_args "-l ${SSH_USER}")
fi
MPI_RSH_AGENT="${MPI_RSH_AGENT:-}"
if [[ -z "$MPI_RSH_AGENT" && -x "$REMOTE_DIR/scripts/ssh_no_display.sh" ]]; then
  MPI_RSH_AGENT="$REMOTE_DIR/scripts/ssh_no_display.sh"
elif [[ -z "$MPI_RSH_AGENT" && -x "$(pwd)/scripts/ssh_no_display.sh" ]]; then
  MPI_RSH_AGENT="$(pwd)/scripts/ssh_no_display.sh"
fi
MPI_AGENT_FLAGS=()
if [[ -n "$MPI_RSH_AGENT" ]]; then
  MPI_AGENT_FLAGS=(--mca plm_rsh_agent "$MPI_RSH_AGENT" --mca plm_rsh_no_tree_spawn "1")
fi
MPI_TCP_FLAGS=(
  --mca routed "direct"
  --mca oob_tcp_disable_ipv6_family "1"
  --mca oob_tcp_if_include "$MPI_LAN_CIDR"
  --mca btl_tcp_if_include "$MPI_LAN_CIDR"
  --mca oob_tcp_dynamic_ipv4_ports "40000-40099"
  --mca btl_tcp_port_min_v4 "40100"
  --mca btl_tcp_port_range_v4 "100"
)

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

HEADER="variant,m,k,n,processes,nodes,tile,repeat_index,seconds,checksum_sum,checksum_abs,checksum_max_abs,checksum_hash,run_label,mapping,pe,threads_per_process,sample_file,full_file"
if [[ -f "$RAW" ]] && [[ "$(head -n 1 "$RAW")" != "$HEADER" ]]; then
  echo "Existing $RAW uses an old schema; recreating it for m,k,n results."
  rm -f "$RAW"
fi
if [[ -f "$CHECKSUMS" ]] && [[ "$(head -n 1 "$CHECKSUMS")" != "$HEADER" ]]; then
  echo "Existing $CHECKSUMS uses an old schema; recreating it for m,k,n results."
  rm -f "$CHECKSUMS"
fi
if [[ ! -f "$RAW" ]]; then
  echo "$HEADER" > "$RAW"
fi

run_case() {
  local variant="$1"
  local shape="$2"
  local nodes="$3"
  local m k n
  local np=$((nodes * PPN))
  local tmp_hosts
  local tmp
  IFS=x read -r m k n <<< "$shape"
  if [[ -z "${m:-}" || -z "${k:-}" || -z "${n:-}" ]]; then
    echo "Invalid shape '$shape'. Use MxKxN, for example 2048x2048x2048."
    exit 2
  fi
  tmp_hosts="$(mktemp)"
  tmp="$(mktemp)"
  read_hosts | head -n "$nodes" | cut -d' ' -f2- > "$tmp_hosts"
  echo "Hosts used for this run:"
  cat "$tmp_hosts"
  echo "Running variant=$variant shape=${m}x${k}x${n} nodes=$nodes ppn=$PPN np=$np PE=$PE"
  mpirun "${MPI_ROOT_FLAGS[@]}" "${MPI_AGENT_FLAGS[@]}" "${MPI_SSH_FLAGS[@]}" "${MPI_TCP_FLAGS[@]}" -x HWLOC_COMPONENTS --hostfile "$tmp_hosts" -np "$np" --wdir "$REMOTE_DIR" --map-by "ppr:${PPN}:node:PE=${PE}" --bind-to core ./build/matrix_hpc \
    --variant "$variant" \
    --m "$m" \
    --k "$k" \
    --n "$n" \
    --seed "$SEED" \
    --repeat "$REPEAT" \
    --tile "$TILE" \
    --max-ram-gb "$MAX_RAM_GB" \
    --output-dir "$OUTPUT_DIR" \
    --save-sample \
    --checksum-file "$CHECKSUMS" \
    --run-label node_sweep \
    --mapping "ppr:${PPN}:node:PE=${PE}" \
    --pe "$PE" | tee "$tmp"
  grep '^RESULT,' "$tmp" | sed 's/^RESULT,//' >> "$RAW"
  rm -f "$tmp" "$tmp_hosts"
}

for shape in $SHAPES; do
  for variant in $VARIANTS; do
    for nodes in $NODES_LIST; do
      if [[ "$nodes" -gt "$host_count" ]]; then
        echo "Skipping nodes=$nodes because $HOSTFILE only has $host_count usable hosts"
        continue
      fi
      run_case "$variant" "$shape" "$nodes"
    done
  done
done

python3 scripts/summarize_results.py --raw "$RAW" --summary "$SUMMARY" --checksum-summary "$CHECKSUM_SUMMARY"
