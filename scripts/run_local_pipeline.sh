#!/usr/bin/env bash
set -euo pipefail

# One-command local pipeline for the report evidence.
# Override from the command line when needed, for example:
#   SHAPES="2048x2048x2048 4096x4096x4096" NP_LIST="1 2 4 8" REPEAT=5 bash scripts/run_local_pipeline.sh

SHAPES="${SHAPES:-2048x2048x2048}"
NP_LIST="${NP_LIST:-1 2 4 8}"
REPEAT="${REPEAT:-5}"
FRESH="${FRESH:-1}"

echo "========================================"
echo " Local benchmark + report pipeline"
echo "========================================"
echo "SHAPES=$SHAPES"
echo "NP_LIST=$NP_LIST"
echo "REPEAT=$REPEAT"
echo "FRESH=$FRESH"
echo

if [[ "$FRESH" == "1" ]]; then
  echo "Cleaning previous CSV/chart evidence for a fresh local run..."
  rm -f results/raw_runs.csv results/summary.csv results/checksums.csv results/checksums_summary.csv
  rm -f docs/charts/*.svg docs/charts/*.png 2>/dev/null || true
fi

bash scripts/capture_environment.sh
SHAPES="$SHAPES" NP_LIST="$NP_LIST" REPEAT="$REPEAT" bash scripts/run_benchmark.sh
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
