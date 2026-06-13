#!/usr/bin/env bash
set -euo pipefail

N="${N:-8192}"
SEED="${SEED:-2026}"
MAX_RAM_GB="${MAX_RAM_GB:-5.0}"
PREFIX="${PREFIX:-data/matrix_N${N}_seed${SEED}}"
MPI_ROOT_FLAGS=()
if [[ "$(id -u)" == "0" ]]; then
  MPI_ROOT_FLAGS=(--allow-run-as-root)
fi

mkdir -p build data
make

mpirun "${MPI_ROOT_FLAGS[@]}" -np 1 ./build/matrix_hpc \
  --generate-only \
  --n "$N" \
  --seed "$SEED" \
  --max-ram-gb "$MAX_RAM_GB" \
  --write-input-prefix "$PREFIX"

echo "Generated ${PREFIX}_A.bin and ${PREFIX}_B.bin"
