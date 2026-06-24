#!/usr/bin/env bash
set -euo pipefail

# One-command pipeline for the final 3-physical-machine benchmark.
# Run only after config/hosts contains the real machines and SSH/Open MPI works.
#
# Example:
#   SSH_USER=your_user HOSTFILE=config/hosts SHAPES="2048x2048x2048 4096x4096x4096" NODES_LIST="1 2 3" PPN=4 REPEAT=5 bash scripts/run_multinode_pipeline.sh

HOSTFILE="${HOSTFILE:-config/hosts}"
SHAPES="${SHAPES:-2048x2048x2048 4096x4096x4096}"
NODES_LIST="${NODES_LIST:-1 2 3}"
PPN="${PPN:-4}"
REPEAT="${REPEAT:-5}"
SYNC_FIRST="${SYNC_FIRST:-1}"
REMOTE_DIR="${REMOTE_DIR:-$(pwd)}"
FRESH="${FRESH:-1}"
RUN_SEQ_BASELINE="${RUN_SEQ_BASELINE:-1}"

echo "========================================"
echo " Multinode benchmark + report pipeline"
echo "========================================"
echo "HOSTFILE=$HOSTFILE"
echo "SHAPES=$SHAPES"
echo "NODES_LIST=$NODES_LIST"
echo "PPN=$PPN"
echo "REPEAT=$REPEAT"
echo "SYNC_FIRST=$SYNC_FIRST"
echo "REMOTE_DIR=$REMOTE_DIR"
echo "FRESH=$FRESH"
echo "RUN_SEQ_BASELINE=$RUN_SEQ_BASELINE"
echo

if [[ ! -f "$HOSTFILE" ]]; then
  echo "Missing hostfile: $HOSTFILE"
  echo "Copy config/hosts.example to config/hosts and edit real IPs first."
  exit 2
fi

if [[ "$FRESH" == "1" ]]; then
  echo "Cleaning previous CSV/chart evidence for a fresh multinode run..."
  rm -f results/raw_runs.csv results/summary.csv results/checksums.csv results/checksums_summary.csv
  rm -f docs/charts/*.svg docs/charts/*.png 2>/dev/null || true
fi

if [[ "$SYNC_FIRST" == "1" ]]; then
  HOSTFILE="$HOSTFILE" REMOTE_DIR="$REMOTE_DIR" bash scripts/sync_to_nodes.sh
fi

HOSTFILE="$HOSTFILE" PPN="$PPN" REMOTE_DIR="$REMOTE_DIR" bash scripts/check_cluster.sh
HOSTFILE="$HOSTFILE" CHECK_REMOTE=1 REMOTE_DIR="$REMOTE_DIR" bash scripts/capture_environment.sh
if [[ "$RUN_SEQ_BASELINE" == "1" ]]; then
  echo
  echo "Running local seq_tiled baseline for checksum correctness evidence..."
  SHAPES="$SHAPES" NP_LIST="1" VARIANTS="" SEQ_VARIANTS="seq_tiled" REPEAT="$REPEAT" bash scripts/run_benchmark.sh
fi
HOSTFILE="$HOSTFILE" SHAPES="$SHAPES" NODES_LIST="$NODES_LIST" PPN="$PPN" REPEAT="$REPEAT" REMOTE_DIR="$REMOTE_DIR" bash scripts/run_multinode.sh
python3 scripts/make_chart_svgs.py
python3 scripts/compare_checksums.py
python3 scripts/create_report_docx.py

echo
echo "Done. Main artifacts:"
echo "  results/raw_runs.csv"
echo "  results/summary.csv"
echo "  results/checksums.csv"
echo "  results/checksums_summary.csv"
echo "  docs/charts/*.png"
echo "  docs/BaoCao_DeTai1_NhanMaTran_MPI.docx"
