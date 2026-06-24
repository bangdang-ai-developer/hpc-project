#!/usr/bin/env bash
set -euo pipefail

HOSTFILE="${HOSTFILE:-config/hosts}"
SSH_USER="${SSH_USER:-}"
REMOTE_DIR="${REMOTE_DIR:-$(pwd)}"
SYNC_DELETE="${SYNC_DELETE:-0}"
BUILD_REMOTE="${BUILD_REMOTE:-1}"
COPY_BUILD="${COPY_BUILD:-0}"

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
if [[ "${#HOSTS[@]}" -lt 2 ]]; then
  echo "Need at least 2 hosts to sync workers. Host 1 is treated as the master machine."
  exit 2
fi

RSYNC_DELETE_ARG=""
if [[ "$SYNC_DELETE" == "1" ]]; then
  RSYNC_DELETE_ARG="--delete"
fi

for i in "${!HOSTS[@]}"; do
  if [[ "$i" == "0" ]]; then
    continue
  fi
  target="$(target_for_ssh "${HOSTS[$i]}")"
  echo "Syncing to $target:$REMOTE_DIR"
  ssh "$target" "mkdir -p $REMOTE_DIR"
  rsync -az $RSYNC_DELETE_ARG \
    --exclude '.git/' \
    --exclude 'build/' \
    --exclude 'results/' \
    --exclude 'outputs/' \
    --exclude 'data/' \
    --exclude 'docs/rendered/' \
    ./ "$target:$REMOTE_DIR/"
  if [[ "$COPY_BUILD" == "1" ]]; then
    if [[ ! -x build/matrix_hpc ]]; then
      echo "COPY_BUILD=1 requested, but build/matrix_hpc is missing or not executable."
      echo "Run make first, preferably with portable CXXFLAGS when copying across different CPUs."
      exit 2
    fi
    echo "Copying local build/matrix_hpc to $target:$REMOTE_DIR/build/matrix_hpc"
    ssh "$target" "mkdir -p $REMOTE_DIR/build"
    rsync -az build/matrix_hpc "$target:$REMOTE_DIR/build/matrix_hpc"
  fi
  if [[ "$BUILD_REMOTE" == "1" ]]; then
    echo "Building on $target:$REMOTE_DIR"
    ssh "$target" "cd $REMOTE_DIR && make"
  fi
done

echo "Sync complete."
