#!/usr/bin/env python3
import argparse
import csv
import math
from collections import defaultdict
from pathlib import Path
from statistics import mean, pstdev


GROUP_FIELDS = ["variant", "n", "processes", "nodes", "tile", "run_label", "mapping", "pe"]


def amdahl_metrics(speedup, p):
    if speedup <= 0 or p <= 1:
        return "", "", "", ""
    serial = (1.0 / speedup - 1.0 / p) / (1.0 - 1.0 / p)
    if serial < 0:
        serial = 0.0
    if serial > 1:
        serial = 1.0
    parallel = 1.0 - serial
    amdahl_speedup = 1.0 / (serial + parallel / p)
    amdahl_max = "" if serial == 0 else f"{1.0 / serial:.6f}"
    return f"{serial:.6f}", f"{parallel:.6f}", f"{amdahl_speedup:.6f}", amdahl_max


def parse_float(value):
    try:
        return float(value)
    except ValueError:
        return math.nan


def read_rows(path):
    if not Path(path).exists():
        return []
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def write_rows(path, fieldnames, rows):
    Path(path).parent.mkdir(parents=True, exist_ok=True)
    with open(path, "w", newline="", encoding="utf-8") as f:
        writer = csv.DictWriter(f, fieldnames=fieldnames)
        writer.writeheader()
        writer.writerows(rows)


def summarize(raw_path, summary_path, checksum_summary_path):
    rows = read_rows(raw_path)
    summary_fields = GROUP_FIELDS + [
        "runs",
        "time_avg_sec",
        "time_min_sec",
        "time_std_sec",
        "baseline_time_sec",
        "speedup",
        "efficiency",
        "amdahl_serial_fraction",
        "amdahl_parallel_fraction",
        "amdahl_speedup",
        "amdahl_max_speedup",
        "checksum_hashes",
    ]
    if not rows:
        write_rows(summary_path, summary_fields, [])
        write_rows(checksum_summary_path,
                   ["variant", "n", "processes", "nodes", "run_label", "unique_checksum_hashes", "status"],
                   [])
        return
    groups = defaultdict(list)
    for row in rows:
        if not row.get("variant"):
            continue
        key = tuple(row.get(field, "") for field in GROUP_FIELDS)
        groups[key].append(row)

    summary_rows = []
    for key, items in groups.items():
        seconds = [parse_float(item["seconds"]) for item in items]
        seconds = [s for s in seconds if not math.isnan(s)]
        if not seconds:
            continue
        out = {field: value for field, value in zip(GROUP_FIELDS, key)}
        out["runs"] = str(len(seconds))
        out["time_avg_sec"] = f"{mean(seconds):.9f}"
        out["time_min_sec"] = f"{min(seconds):.9f}"
        out["time_std_sec"] = f"{pstdev(seconds):.9f}" if len(seconds) > 1 else "0.000000000"
        out["checksum_hashes"] = str(len({item.get("checksum_hash", "") for item in items}))
        summary_rows.append(out)

    baseline = {}
    for row in summary_rows:
        if row["run_label"] == "node_sweep" and row["nodes"] == "1":
            base_key = (row["variant"], row["n"], row["run_label"])
            baseline[base_key] = float(row["time_min_sec"])
        elif row["run_label"] != "node_sweep" and row["processes"] == "1":
            base_key = (row["variant"], row["n"], row["run_label"])
            baseline[base_key] = float(row["time_min_sec"])

    for row in summary_rows:
        base_key = (row["variant"], row["n"], row["run_label"])
        b = baseline.get(base_key)
        t = float(row["time_min_sec"])
        p = int(row["nodes"]) if row["run_label"] == "node_sweep" else int(row["processes"])
        if b and t > 0:
            speedup = b / t
            efficiency = speedup / p
            serial, parallel, amdahl_speedup, amdahl_max = amdahl_metrics(speedup, p)
            row["baseline_time_sec"] = f"{b:.9f}"
            row["speedup"] = f"{speedup:.6f}"
            row["efficiency"] = f"{efficiency:.6f}"
            row["amdahl_serial_fraction"] = serial
            row["amdahl_parallel_fraction"] = parallel
            row["amdahl_speedup"] = amdahl_speedup
            row["amdahl_max_speedup"] = amdahl_max
        else:
            row["baseline_time_sec"] = ""
            row["speedup"] = ""
            row["efficiency"] = ""
            row["amdahl_serial_fraction"] = ""
            row["amdahl_parallel_fraction"] = ""
            row["amdahl_speedup"] = ""
            row["amdahl_max_speedup"] = ""

    summary_rows.sort(key=lambda r: (r["run_label"], int(r["n"]), r["variant"], int(r["processes"]), int(r["nodes"])))
    write_rows(summary_path, summary_fields, summary_rows)

    checksum_groups = defaultdict(set)
    for row in rows:
        key = (row["variant"], row["n"], row["processes"], row["nodes"], row["run_label"])
        checksum_groups[key].add(row.get("checksum_hash", ""))
    checksum_rows = []
    for key, hashes in checksum_groups.items():
        checksum_rows.append({
            "variant": key[0],
            "n": key[1],
            "processes": key[2],
            "nodes": key[3],
            "run_label": key[4],
            "unique_checksum_hashes": str(len(hashes)),
            "status": "OK" if len(hashes) == 1 else "CHECK",
        })
    checksum_rows.sort(key=lambda r: (r["run_label"], int(r["n"]), r["variant"], int(r["processes"])))
    write_rows(checksum_summary_path,
               ["variant", "n", "processes", "nodes", "run_label", "unique_checksum_hashes", "status"],
               checksum_rows)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--raw", default="results/raw_runs.csv")
    parser.add_argument("--summary", default="results/summary.csv")
    parser.add_argument("--checksum-summary", default="results/checksums_summary.csv")
    args = parser.parse_args()
    summarize(args.raw, args.summary, args.checksum_summary)


if __name__ == "__main__":
    main()
