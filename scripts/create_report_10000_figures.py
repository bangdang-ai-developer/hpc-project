#!/usr/bin/env python3
"""Regenerate report_10000 chart PNGs from derived_process_metrics.csv."""
from pathlib import Path
import csv

import matplotlib

matplotlib.use("Agg")
import matplotlib.pyplot as plt


ROOT = Path(__file__).resolve().parents[1]
REPORT_DIR = ROOT / "docs" / "report_10000"
METRICS = REPORT_DIR / "derived_process_metrics.csv"
FIGURES = REPORT_DIR / "figures"


def read_metrics():
    with METRICS.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def numeric(rows, field, cast=float):
    return [cast(float(row[field])) for row in rows]


def setup_style():
    plt.rcParams.update(
        {
            "font.family": ["DejaVu Sans", "Arial", "sans-serif"],
            "axes.titlesize": 14,
            "axes.labelsize": 11,
            "xtick.labelsize": 10,
            "ytick.labelsize": 10,
            "legend.fontsize": 10,
            "figure.dpi": 160,
            "savefig.dpi": 160,
        }
    )


def save_line_chart(filename, title, ylabel, values, highlight_best=False):
    rows = read_metrics()
    processes = numeric(rows, "processes", int)
    labels = [f"p{p}" for p in processes]

    fig, ax = plt.subplots(figsize=(8.2, 4.6))
    ax.plot(processes, values, marker="o", linewidth=2.2, color="#1F5A8A")
    ax.set_title(title)
    ax.set_xlabel("Số tiến trình MPI")
    ax.set_ylabel(ylabel)
    ax.set_xticks(processes)
    ax.set_xticklabels(labels)
    ax.grid(True, axis="y", color="#D9E2EC", linewidth=0.8)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)

    if highlight_best:
        best_i = min(range(len(values)), key=lambda i: values[i])
        ax.scatter([processes[best_i]], [values[best_i]], s=90, color="#C23B22", zorder=3)
        ax.annotate(
            f"Tốt nhất: p{processes[best_i]}",
            xy=(processes[best_i], values[best_i]),
            xytext=(14, 24),
            textcoords="offset points",
            arrowprops={"arrowstyle": "->", "color": "#C23B22"},
            color="#8A1F11",
        )

    fig.tight_layout()
    FIGURES.mkdir(parents=True, exist_ok=True)
    fig.savefig(FIGURES / filename, bbox_inches="tight")
    plt.close(fig)


def save_bar_chart(filename, title, ylabel, values):
    rows = read_metrics()
    processes = numeric(rows, "processes", int)
    labels = [f"p{p}" for p in processes]

    fig, ax = plt.subplots(figsize=(8.2, 4.6))
    colors = ["#1F5A8A" if p != 9 else "#C23B22" for p in processes]
    ax.bar(labels, values, color=colors, width=0.62)
    ax.set_title(title)
    ax.set_xlabel("Số tiến trình MPI")
    ax.set_ylabel(ylabel)
    ax.grid(True, axis="y", color="#D9E2EC", linewidth=0.8)
    ax.spines["top"].set_visible(False)
    ax.spines["right"].set_visible(False)
    for idx, value in enumerate(values):
        ax.text(idx, value, f"{value:.2f}", ha="center", va="bottom", fontsize=9)
    fig.tight_layout()
    FIGURES.mkdir(parents=True, exist_ok=True)
    fig.savefig(FIGURES / filename, bbox_inches="tight")
    plt.close(fig)


def main():
    rows = read_metrics()
    setup_style()
    save_line_chart(
        "runtime_by_process.png",
        "Thời gian chạy tốt nhất theo số tiến trình MPI",
        "Thời gian tốt nhất (giây)",
        numeric(rows, "time_min_sec"),
        highlight_best=True,
    )
    save_bar_chart(
        "gflops_by_process.png",
        "Thông lượng theo số tiến trình MPI",
        "Thông lượng (GFLOP/s)",
        numeric(rows, "best_gflops"),
    )
    save_line_chart(
        "speedup_vs_p4.png",
        "Speedup tương đối so với p4",
        "Speedup tương đối",
        numeric(rows, "speedup_vs_p4"),
    )
    save_line_chart(
        "efficiency_vs_p4.png",
        "Hiệu suất tương đối so với p4",
        "Hiệu suất tương đối (%)",
        numeric(rows, "eff_vs_p4"),
    )
    print(f"Wrote figures to {FIGURES}")


if __name__ == "__main__":
    main()
