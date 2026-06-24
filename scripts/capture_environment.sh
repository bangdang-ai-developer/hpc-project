#!/usr/bin/env bash
set -euo pipefail

# Save machine/software information used by the experiment.
# Local:
#   bash scripts/capture_environment.sh
#
# Cluster:
#   CHECK_REMOTE=1 SSH_USER=USER HOSTFILE=config/hosts bash scripts/capture_environment.sh

OUT="${OUT:-results/environment.csv}"
HOSTFILE="${HOSTFILE:-config/hosts}"
SSH_USER="${SSH_USER:-}"
CHECK_REMOTE="${CHECK_REMOTE:-0}"
REMOTE_DIR="${REMOTE_DIR:-$(pwd)}"

mkdir -p "$(dirname "$OUT")"
echo "role,host,hostname,os,cpu_count,mem_total,compiler,mpirun,project_dir" > "$OUT"

target_for_ssh() {
  local host="$1"
  if [[ -n "$SSH_USER" ]]; then
    printf "%s@%s" "$SSH_USER" "$host"
  else
    printf "%s" "$host"
  fi
}

quote_csv() {
  local s="${1:-}"
  s="${s//$'\n'/ }"
  s="${s//\"/\"\"}"
  printf '"%s"' "$s"
}

capture_one() {
  local role="$1"
  local host="$2"
  local prefix=()
  if [[ "$host" != "local" ]]; then
    prefix=(ssh -o BatchMode=yes -o ConnectTimeout=5 "$(target_for_ssh "$host")")
  fi

  local hostname os cpu mem compiler mpirun
  hostname="$("${prefix[@]}" hostname 2>/dev/null || echo unknown)"
  os="$("${prefix[@]}" sh -lc '. /etc/os-release 2>/dev/null && echo "$PRETTY_NAME" || uname -a' 2>/dev/null || echo unknown)"
  cpu="$("${prefix[@]}" nproc 2>/dev/null || echo unknown)"
  mem="$("${prefix[@]}" sh -lc "free -h 2>/dev/null | awk '/^Mem:/ {print \$2}'" 2>/dev/null || echo unknown)"
  compiler="$("${prefix[@]}" sh -lc 'mpic++ --version 2>/dev/null | head -n 1 || g++ --version 2>/dev/null | head -n 1' 2>/dev/null || echo unknown)"
  mpirun="$("${prefix[@]}" sh -lc 'mpirun --version 2>/dev/null | head -n 1' 2>/dev/null || echo unknown)"

  {
    quote_csv "$role"; printf ","
    quote_csv "$host"; printf ","
    quote_csv "$hostname"; printf ","
    quote_csv "$os"; printf ","
    quote_csv "$cpu"; printf ","
    quote_csv "$mem"; printf ","
    quote_csv "$compiler"; printf ","
    quote_csv "$mpirun"; printf ","
    quote_csv "$REMOTE_DIR"; printf "\n"
  } >> "$OUT"
}

capture_one "local" "local"

if [[ "$CHECK_REMOTE" == "1" ]]; then
  if [[ ! -f "$HOSTFILE" ]]; then
    echo "Missing hostfile: $HOSTFILE" >&2
    exit 2
  fi
  idx=0
  while read -r host _rest; do
    [[ -z "$host" || "$host" =~ ^# ]] && continue
    idx=$((idx + 1))
    capture_one "node_${idx}" "$host"
  done < <(grep -v '^[[:space:]]*$' "$HOSTFILE" | grep -v '^[[:space:]]*#' | sed 's/#.*//')
fi

echo "Environment evidence written to $OUT"
