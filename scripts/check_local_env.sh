#!/usr/bin/env bash
set -euo pipefail

INSTALL="${INSTALL:-0}"

need() {
  local cmd="$1"
  if command -v "$cmd" >/dev/null 2>&1; then
    printf "OK   %-10s %s\n" "$cmd" "$(command -v "$cmd")"
  else
    printf "MISS %-10s\n" "$cmd"
    return 1
  fi
}

need_python_module() {
  local module="$1"
  if python3 -c "import ${module}" >/dev/null 2>&1; then
    printf "OK   python module %s\n" "$module"
  else
    printf "MISS python module %s\n" "$module"
    return 1
  fi
}

echo "== Local machine =="
hostname
uname -a
echo

echo "== CPU/RAM =="
nproc || true
free -h || true
echo

missing=0
for cmd in g++ make mpic++ mpirun python3 rsync ssh; do
  need "$cmd" || missing=1
done
need_python_module docx || missing=1

if [[ "$missing" -ne 0 ]]; then
  echo
  echo "Some packages are missing."
  if [[ "$INSTALL" == "1" ]]; then
    sudo apt update
    sudo apt install -y build-essential openmpi-bin libopenmpi-dev python3 python3-docx rsync openssh-server
    sudo systemctl enable --now ssh || true
  else
    echo "Install them with:"
    echo "  sudo apt update"
    echo "  sudo apt install -y build-essential openmpi-bin libopenmpi-dev python3 python3-docx rsync openssh-server"
    echo "Or rerun: INSTALL=1 bash scripts/check_local_env.sh"
    exit 2
  fi
fi

echo
echo "== Versions =="
g++ --version | head -n 1
mpic++ --version | head -n 1
mpirun --version 2>/dev/null | head -n 1 || true
python3 --version

echo
echo "== Build =="
make

echo
echo "== Smoke tests =="
MPI_ROOT_FLAGS=()
if [[ "$(id -u)" == "0" ]]; then
  MPI_ROOT_FLAGS=(--allow-run-as-root)
fi
mpirun "${MPI_ROOT_FLAGS[@]}" -np 1 ./build/matrix_hpc --variant seq --m 64 --k 48 --n 32 --repeat 1 --max-ram-gb 1 --save-sample
mpirun "${MPI_ROOT_FLAGS[@]}" -np 1 ./build/matrix_hpc --variant seq_tiled --m 64 --k 48 --n 32 --repeat 1 --max-ram-gb 1 --save-sample
mpirun "${MPI_ROOT_FLAGS[@]}" -np 2 ./build/matrix_hpc --variant mpi --m 64 --k 48 --n 32 --repeat 1 --max-ram-gb 1 --save-sample
mpirun "${MPI_ROOT_FLAGS[@]}" -np 2 ./build/matrix_hpc --variant mpi_tiled --m 64 --k 48 --n 32 --repeat 1 --max-ram-gb 1 --save-sample
mpirun "${MPI_ROOT_FLAGS[@]}" -np 2 ./build/matrix_hpc --variant mpi_2d --m 64 --k 48 --n 32 --repeat 1 --max-ram-gb 1 --save-sample

echo
echo "Local environment is ready."
