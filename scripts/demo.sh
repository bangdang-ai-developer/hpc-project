#!/usr/bin/env bash
set -euo pipefail

MPI_ROOT_FLAGS=()
if [[ "$(id -u)" == "0" ]]; then
  MPI_ROOT_FLAGS=(--allow-run-as-root)
fi
MPI_DEMO_FLAGS=(--oversubscribe --bind-to none)

cpu_count="$(nproc 2>/dev/null || echo 1)"

echo "========================================"
echo " Matrix Multiplication MPI Demo"
echo "========================================"
echo "This launcher asks for the number of CPU/processes first."
echo "Your machine reports $cpu_count logical CPU(s)."
echo

while true; do
  read -r -p "How many parallel CPU/processes do you want to use? [2]: " np
  np="${np:-2}"
  if [[ "$np" =~ ^[0-9]+$ ]] && [[ "$np" -ge 1 ]] && [[ "$np" -le "$cpu_count" ]]; then
    break
  fi
  echo "Please enter an integer from 1 to $cpu_count."
done

make

echo
echo "Launching program with $np MPI process(es)..."
echo
mpirun "${MPI_ROOT_FLAGS[@]}" "${MPI_DEMO_FLAGS[@]}" -np "$np" ./build/matrix_hpc
