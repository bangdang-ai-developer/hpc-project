#!/usr/bin/env python3
import argparse
import csv
import math
from collections import defaultdict


def read_rows(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def f(value):
    try:
        return float(value)
    except (TypeError, ValueError):
        return math.nan


def main():
    parser = argparse.ArgumentParser(description="Check checksum stability and compare MPI runs against a baseline variant.")
    parser.add_argument("--input", default="results/checksums.csv")
    parser.add_argument("--baseline", default="seq_tiled")
    parser.add_argument("--tolerance", type=float, default=1e-8)
    args = parser.parse_args()

    rows = [r for r in read_rows(args.input) if r.get("variant")]
    if not rows:
        print("No checksum rows found.")
        return 0

    stable_groups = defaultdict(set)
    for r in rows:
        key = (r["variant"], r["n"], r["processes"], r["nodes"], r["run_label"])
        stable_groups[key].add(r["checksum_hash"])

    ok = True
    print("== Stability inside each configuration ==")
    for key, hashes in sorted(stable_groups.items()):
        status = "OK" if len(hashes) == 1 else "CHECK"
        if status != "OK":
            ok = False
        print(f"{status} {key}: {len(hashes)} unique hash(es)")

    baseline_by_n = {}
    for r in rows:
        if r["variant"] == args.baseline:
            baseline_by_n.setdefault(r["n"], r)

    if baseline_by_n:
        print("\n== Approximate comparison with baseline sums ==")
        for r in rows:
            base = baseline_by_n.get(r["n"])
            if not base or r["variant"] == args.baseline:
                continue
            bsum = f(base["checksum_sum"])
            rsum = f(r["checksum_sum"])
            denom = max(1.0, abs(bsum))
            rel = abs(rsum - bsum) / denom
            status = "OK" if rel <= args.tolerance else "CHECK"
            if status != "OK":
                ok = False
            print(f"{status} N={r['n']} {r['variant']} p={r['processes']} rel_sum_error={rel:.3e}")
    else:
        print(f"\nNo baseline variant '{args.baseline}' found. Run seq_tiled once for comparison evidence.")

    return 0 if ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
