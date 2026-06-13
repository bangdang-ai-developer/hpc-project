#!/usr/bin/env bash
set -euo pipefail

# Quick pre-submission audit.
# It does not run benchmark. It only checks whether expected artifacts exist.

fail=0

ok() {
  printf "OK   %s\n" "$1"
}

miss() {
  printf "MISS %s\n" "$1"
  fail=1
}

warn() {
  printf "WARN %s\n" "$1"
}

need_file() {
  local path="$1"
  [[ -f "$path" ]] && ok "$path" || miss "$path"
}

need_glob() {
  local pattern="$1"
  compgen -G "$pattern" >/dev/null && ok "$pattern" || miss "$pattern"
}

need_command() {
  local cmd="$1"
  command -v "$cmd" >/dev/null 2>&1 && ok "command: $cmd" || miss "missing command: $cmd"
}

need_csv_data() {
  local path="$1"
  local min_rows="${2:-2}"
  if [[ ! -f "$path" ]]; then
    return
  fi
  local rows
  rows="$(wc -l < "$path" | tr -d ' ')"
  if [[ "$rows" -ge "$min_rows" ]]; then
    ok "$path has data rows"
  else
    miss "$path has header only or no benchmark data"
  fi
}

echo "== Required source/report files =="
need_file START_HERE.md
need_file README.md
need_file Makefile
need_file src/matrix_hpc.c
need_file docs/BaoCao_DeTai1_NhanMaTran_MPI.docx
need_file config/group_info.txt
need_command python3

echo
echo "== Evidence CSV files =="
need_file results/environment.csv
need_file results/raw_runs.csv
need_file results/summary.csv
need_file results/checksums.csv
need_file results/checksums_summary.csv
need_csv_data results/environment.csv 2
need_csv_data results/raw_runs.csv 2
need_csv_data results/summary.csv 2
need_csv_data results/checksums.csv 2
need_csv_data results/checksums_summary.csv 2

echo
echo "== Chart/sample evidence =="
need_glob "docs/charts/*.png"
need_glob "outputs/C_sample_*.csv"

echo
echo "== Content checks =="
if [[ -f results/checksums_summary.csv ]]; then
  if grep -q ',CHECK' results/checksums_summary.csv; then
    miss "results/checksums_summary.csv contains CHECK"
  else
    ok "checksums_summary has no CHECK rows"
  fi
fi

if [[ -f results/summary.csv ]]; then
  if grep -q 'speedup' results/summary.csv && grep -q 'efficiency' results/summary.csv && grep -q 'amdahl_serial_fraction' results/summary.csv; then
    ok "summary.csv has speedup/efficiency/Amdahl columns"
  else
    miss "summary.csv missing required performance columns"
  fi
fi

if [[ -f docs/BaoCao_DeTai1_NhanMaTran_MPI.docx ]] && command -v python3 >/dev/null 2>&1; then
  if ! python3 - <<'PY'
from pathlib import Path
from xml.etree import ElementTree as ET
import sys
import zipfile

path = Path("docs/BaoCao_DeTai1_NhanMaTran_MPI.docx")
bad_terms = [
    "Can dien",
    "Chua co du lieu benchmark chinh thuc",
    "cap nhat ten 3 thanh vien",
    "chua cap nhat ten 3 thanh vien",
]
try:
    with zipfile.ZipFile(path) as z:
        xml = z.read("word/document.xml")
    root = ET.fromstring(xml)
    ns = {"w": "http://schemas.openxmlformats.org/wordprocessingml/2006/main"}
    text = "\n".join(
        "".join(t.text or "" for t in para.findall(".//w:t", ns))
        for para in root.findall(".//w:p", ns)
    )
except Exception as exc:
    print(f"MISS cannot inspect Word report: {exc}")
    sys.exit(2)

found = [term for term in bad_terms if term in text]
if found:
    print("MISS Word report still looks like a draft: " + ", ".join(found))
    sys.exit(2)

print("OK   Word report has no known draft placeholders")
PY
  then
    fail=1
  fi
fi

if [[ -f docs/GROUP_AND_ENVIRONMENT_TEMPLATE.md ]] && grep -q 'Cap nhat' docs/GROUP_AND_ENVIRONMENT_TEMPLATE.md; then
  warn "docs/GROUP_AND_ENVIRONMENT_TEMPLATE.md still contains 'Cap nhat' placeholders"
fi

echo
if [[ "$fail" -ne 0 ]]; then
  echo "Preflight failed. See MISS lines above."
  exit 2
fi

echo "Preflight passed. Submission artifacts look ready."
