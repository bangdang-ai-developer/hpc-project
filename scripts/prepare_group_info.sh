#!/usr/bin/env bash
set -euo pipefail

# Create config/group_info.txt from the example template if it does not exist.
# Edit this file before generating the final Word report.

SRC="config/group_info.example.txt"
DST="config/group_info.txt"

if [[ ! -f "$SRC" ]]; then
  echo "Missing template: $SRC"
  exit 2
fi

if [[ -f "$DST" ]]; then
  echo "$DST already exists. Edit it if member information changed."
else
  cp "$SRC" "$DST"
  echo "Created $DST from $SRC"
fi

echo
echo "Next step: edit $DST with real names, student IDs, and roles."
