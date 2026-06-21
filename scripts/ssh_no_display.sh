#!/usr/bin/env bash
set -euo pipefail

host="$1"
shift

exec ssh -x "$host" env \
  -u DISPLAY \
  -u WAYLAND_DISPLAY \
  HWLOC_COMPONENTS=-gl,-opencl,-cuda,-nvml,-rsmi \
  "$@"
