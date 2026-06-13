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
MPI_SSH_FLAGS=()
if [[ -n "$SSH_USER" ]]; then
  MPI_SSH_FLAGS=(--mca plm_rsh_args "-l ${SSH_USER}")
fi

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
  if [[ -n "$SSH_USER" ]]; then
    printf "%s@%s" "$SSH_USER" "$host"
  else
    printf "%s" "$host"
  fi
}

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
  ssh -o BatchMode=yes -o ConnectTimeout=5 "$target" "cd $REMOTE_DIR && hostname; command -v mpicc; command -v mpirun; command -v python3; python3 -c 'import docx'; command -v rsync; test -x ./build/matrix_hpc && echo ./build/matrix_hpc; nproc; free -h | head -n 2"
done

tmp_hosts="$(mktemp)"
grep -v '^[[:space:]]*$' "$HOSTFILE" | grep -v '^[[:space:]]*#' | sed 's/#.*//' > "$tmp_hosts"

echo
echo "== OpenMPI cluster smoke test =="
np=$((${#HOSTS[@]} * PPN))
mpirun "${MPI_ROOT_FLAGS[@]}" "${MPI_SSH_FLAGS[@]}" --hostfile "$tmp_hosts" -np "$np" --wdir "$REMOTE_DIR" --map-by "ppr:${PPN}:node" hostname | sort

rm -f "$tmp_hosts"
echo
echo "Cluster connectivity is ready."
