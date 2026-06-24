#!/usr/bin/env bash
set -euo pipefail

HOSTFILE="${HOSTFILE:-config/hosts}"
SSH_USER="${SSH_USER:-}"
PPN="${PPN:-1}"
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

if [[ ! -f "$HOSTFILE" ]]; then
  echo "Missing hostfile: $HOSTFILE"
  exit 2
fi

hosts() {
  grep -v '^[[:space:]]*$' "$HOSTFILE" \
    | grep -v '^[[:space:]]*#' \
    | sed 's/#.*//' \
    | awk '{print $1}'
}

target_for_ssh() {
  local host="$1"
  if [[ "$host" == "localhost" || "$host" == "127.0.0.1" || "$host" == "::1" ]]; then
    printf "%s" "$host"
    return
  fi
  if [[ -n "$SSH_USER" ]]; then
    printf "%s@%s" "$SSH_USER" "$host"
  else
    printf "%s" "$host"
  fi
}

check_command="cd $REMOTE_DIR && hostname; command -v mpic++; command -v mpirun; command -v python3; python3 -c 'import docx'; command -v rsync; test -x ./build/matrix_hpc && echo ./build/matrix_hpc; nproc; free -h | head -n 2"

mapfile -t HOSTS < <(hosts)
if [[ "${#HOSTS[@]}" -lt 1 ]]; then
  echo "No usable hosts found in $HOSTFILE"
  exit 2
fi

echo "== Hosts =="
printf "%s\n" "${HOSTS[@]}"
echo

echo "== SSH and package checks =="
for host in "${HOSTS[@]}"; do
  target="$(target_for_ssh "$host")"
  echo "-- $target"
  if [[ "$target" == "localhost" || "$target" == "127.0.0.1" || "$target" == "::1" ]]; then
    bash -lc "$check_command"
  else
    ssh -o BatchMode=yes -o ConnectTimeout=5 "$target" "$check_command"
  fi
done

tmp_hosts="$(mktemp)"
grep -v '^[[:space:]]*$' "$HOSTFILE" | grep -v '^[[:space:]]*#' | sed 's/#.*//' > "$tmp_hosts"

echo
echo "== Open MPI cluster smoke test =="
np=$((${#HOSTS[@]} * PPN))
mpirun "${MPI_ROOT_FLAGS[@]}" "${MPI_AGENT_FLAGS[@]}" "${MPI_SSH_FLAGS[@]}" "${MPI_TCP_FLAGS[@]}" -x HWLOC_COMPONENTS --hostfile "$tmp_hosts" -np "$np" --wdir "$REMOTE_DIR" --map-by "ppr:${PPN}:node" hostname | sort

rm -f "$tmp_hosts"
echo
echo "Cluster connectivity is ready."
