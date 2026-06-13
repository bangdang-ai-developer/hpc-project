#!/usr/bin/env python3
import argparse
import csv
import struct
import zlib
from collections import defaultdict
from pathlib import Path


def read_rows(path):
    with open(path, newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def to_float(value):
    try:
        return float(value)
    except (TypeError, ValueError):
        return None


def write_png(path, width, height, pixels):
    raw = bytearray()
    for y in range(height):
        raw.append(0)
        row = pixels[y]
        for r, g, b in row:
            raw.extend((r, g, b))

    def chunk(kind, data):
        body = kind + data
        return struct.pack(">I", len(data)) + body + struct.pack(">I", zlib.crc32(body) & 0xFFFFFFFF)

    png = bytearray(b"\x89PNG\r\n\x1a\n")
    png.extend(chunk(b"IHDR", struct.pack(">IIBBBBB", width, height, 8, 2, 0, 0, 0)))
    png.extend(chunk(b"IDAT", zlib.compress(bytes(raw), 9)))
    png.extend(chunk(b"IEND", b""))
    Path(path).write_bytes(png)


FONT = {
    " ": ["00000", "00000", "00000", "00000", "00000", "00000", "00000"],
    "-": ["00000", "00000", "00000", "11110", "00000", "00000", "00000"],
    "_": ["00000", "00000", "00000", "00000", "00000", "00000", "11111"],
    ".": ["00000", "00000", "00000", "00000", "00000", "01100", "01100"],
    "/": ["00001", "00010", "00100", "00100", "01000", "10000", "00000"],
    "=": ["00000", "11111", "00000", "11111", "00000", "00000", "00000"],
    ":": ["00000", "01100", "01100", "00000", "01100", "01100", "00000"],
    "0": ["01110", "10001", "10011", "10101", "11001", "10001", "01110"],
    "1": ["00100", "01100", "00100", "00100", "00100", "00100", "01110"],
    "2": ["01110", "10001", "00001", "00010", "00100", "01000", "11111"],
    "3": ["11110", "00001", "00001", "01110", "00001", "00001", "11110"],
    "4": ["00010", "00110", "01010", "10010", "11111", "00010", "00010"],
    "5": ["11111", "10000", "10000", "11110", "00001", "00001", "11110"],
    "6": ["01110", "10000", "10000", "11110", "10001", "10001", "01110"],
    "7": ["11111", "00001", "00010", "00100", "01000", "01000", "01000"],
    "8": ["01110", "10001", "10001", "01110", "10001", "10001", "01110"],
    "9": ["01110", "10001", "10001", "01111", "00001", "00001", "01110"],
    "A": ["01110", "10001", "10001", "11111", "10001", "10001", "10001"],
    "B": ["11110", "10001", "10001", "11110", "10001", "10001", "11110"],
    "C": ["01110", "10001", "10000", "10000", "10000", "10001", "01110"],
    "D": ["11110", "10001", "10001", "10001", "10001", "10001", "11110"],
    "E": ["11111", "10000", "10000", "11110", "10000", "10000", "11111"],
    "F": ["11111", "10000", "10000", "11110", "10000", "10000", "10000"],
    "G": ["01110", "10001", "10000", "10111", "10001", "10001", "01110"],
    "H": ["10001", "10001", "10001", "11111", "10001", "10001", "10001"],
    "I": ["01110", "00100", "00100", "00100", "00100", "00100", "01110"],
    "J": ["00111", "00010", "00010", "00010", "10010", "10010", "01100"],
    "K": ["10001", "10010", "10100", "11000", "10100", "10010", "10001"],
    "L": ["10000", "10000", "10000", "10000", "10000", "10000", "11111"],
    "M": ["10001", "11011", "10101", "10101", "10001", "10001", "10001"],
    "N": ["10001", "11001", "10101", "10011", "10001", "10001", "10001"],
    "O": ["01110", "10001", "10001", "10001", "10001", "10001", "01110"],
    "P": ["11110", "10001", "10001", "11110", "10000", "10000", "10000"],
    "Q": ["01110", "10001", "10001", "10001", "10101", "10010", "01101"],
    "R": ["11110", "10001", "10001", "11110", "10100", "10010", "10001"],
    "S": ["01111", "10000", "10000", "01110", "00001", "00001", "11110"],
    "T": ["11111", "00100", "00100", "00100", "00100", "00100", "00100"],
    "U": ["10001", "10001", "10001", "10001", "10001", "10001", "01110"],
    "V": ["10001", "10001", "10001", "10001", "10001", "01010", "00100"],
    "W": ["10001", "10001", "10001", "10101", "10101", "10101", "01010"],
    "X": ["10001", "10001", "01010", "00100", "01010", "10001", "10001"],
    "Y": ["10001", "10001", "01010", "00100", "00100", "00100", "00100"],
    "Z": ["11111", "00001", "00010", "00100", "01000", "10000", "11111"],
}


def text_width(text, scale=2):
    return sum((6 if ch.upper() in FONT else 6) * scale for ch in text)


def draw_text(pixels, x, y, text, color=(32, 32, 32), scale=2):
    height = len(pixels)
    width = len(pixels[0]) if height else 0
    cx = x
    for ch in text.upper():
        glyph = FONT.get(ch, FONT[" "])
        for gy, row in enumerate(glyph):
            for gx, bit in enumerate(row):
                if bit == "1":
                    for yy in range(scale):
                        for xx in range(scale):
                            px = cx + gx * scale + xx
                            py = y + gy * scale + yy
                            if 0 <= px < width and 0 <= py < height:
                                pixels[py][px] = color
        cx += 6 * scale


def fit_text(text, max_pixels, scale=2):
    if text_width(text, scale) <= max_pixels:
        return text
    suffix = "..."
    keep = max(1, (max_pixels // (6 * scale)) - len(suffix))
    return text[:keep] + suffix


def make_png(points_by_name, title, x_label, y_label, out_path):
    width, height = 900, 520
    left, right, top, bottom = 90, 30, 60, 80
    plot_w = width - left - right
    plot_h = height - top - bottom
    all_points = [p for pts in points_by_name.values() for p in pts]
    if not all_points:
        return

    pixels = [[(255, 255, 255) for _ in range(width)] for _ in range(height)]
    colors = [(31, 119, 180), (214, 39, 40), (44, 160, 44), (148, 103, 189), (255, 127, 14), (23, 190, 207)]
    xs = sorted({p[0] for p in all_points})
    max_y = max(p[1] for p in all_points) or 1.0
    min_x, max_x = min(xs), max(xs)
    if min_x == max_x:
        max_x = min_x + 1

    def sx(x):
        return int(left + (x - min_x) / (max_x - min_x) * plot_w)

    def sy(y):
        return int(top + plot_h - (y / max_y) * plot_h)

    def put(x, y, color):
        if 0 <= x < width and 0 <= y < height:
            pixels[y][x] = color

    def line(x0, y0, x1, y1, color):
        dx = abs(x1 - x0)
        sx_dir = 1 if x0 < x1 else -1
        dy = -abs(y1 - y0)
        sy_dir = 1 if y0 < y1 else -1
        err = dx + dy
        while True:
            put(x0, y0, color)
            if x0 == x1 and y0 == y1:
                break
            e2 = 2 * err
            if e2 >= dy:
                err += dy
                x0 += sx_dir
            if e2 <= dx:
                err += dx
                y0 += sy_dir

    def thick_line(x0, y0, x1, y1, color):
        for off in (-1, 0, 1):
            line(x0, y0 + off, x1, y1 + off, color)
            line(x0 + off, y0, x1 + off, y1, color)

    def rect(x0, y0, x1, y1, color):
        for y in range(y0, y1 + 1):
            for x in range(x0, x1 + 1):
                put(x, y, color)

    def circle(cx, cy, r, color):
        for y in range(cy - r, cy + r + 1):
            for x in range(cx - r, cx + r + 1):
                if (x - cx) * (x - cx) + (y - cy) * (y - cy) <= r * r:
                    put(x, y, color)

    for i in range(6):
        y = max_y * i / 5.0
        yy = sy(y)
        line(left, yy, left + plot_w, yy, (229, 231, 235))
        draw_text(pixels, 12, yy - 7, f"{y:.3g}", (80, 80, 80), 1)
    for x in xs:
        xx = sx(x)
        line(xx, top + plot_h, xx, top + plot_h + 8, (80, 80, 80))
        draw_text(pixels, xx - text_width(f"{x:g}", 1) // 2, top + plot_h + 16, f"{x:g}", (80, 80, 80), 1)
    line(left, top, left, top + plot_h, (51, 51, 51))
    line(left, top + plot_h, left + plot_w, top + plot_h, (51, 51, 51))
    title = fit_text(title, width - 24, 2)
    draw_text(pixels, max(8, width // 2 - text_width(title, 2) // 2), 18, title, (20, 40, 70), 2)
    draw_text(pixels, width // 2 - text_width(x_label, 2) // 2, height - 42, x_label, (50, 50, 50), 2)
    draw_text(pixels, 8, 38, y_label, (50, 50, 50), 1)

    for idx, (name, pts) in enumerate(sorted(points_by_name.items())):
        color = colors[idx % len(colors)]
        pts = sorted(pts)
        for (x0, y0), (x1, y1) in zip(pts, pts[1:]):
            thick_line(sx(x0), sy(y0), sx(x1), sy(y1), color)
        for x, y in pts:
            circle(sx(x), sy(y), 5, color)
        rect(left + plot_w - 240, top + idx * 22 - 12, left + plot_w - 220, top + idx * 22 - 4, color)
        draw_text(pixels, left + plot_w - 214, top + idx * 22 - 18, name, (50, 50, 50), 1)

    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    write_png(out_path, width, height, pixels)


def make_svg(points_by_name, title, x_label, y_label, out_path):
    width, height = 900, 520
    left, right, top, bottom = 90, 30, 60, 80
    plot_w = width - left - right
    plot_h = height - top - bottom
    all_points = [p for pts in points_by_name.values() for p in pts]
    if not all_points:
        return
    xs = sorted({p[0] for p in all_points})
    max_y = max(p[1] for p in all_points) or 1.0
    min_x, max_x = min(xs), max(xs)
    if min_x == max_x:
        max_x = min_x + 1
    colors = ["#1f77b4", "#d62728", "#2ca02c", "#9467bd", "#ff7f0e", "#17becf"]

    def sx(x):
        return left + (x - min_x) / (max_x - min_x) * plot_w

    def sy(y):
        return top + plot_h - (y / max_y) * plot_h

    lines = [
        f'<svg xmlns="http://www.w3.org/2000/svg" width="{width}" height="{height}" viewBox="0 0 {width} {height}">',
        '<rect width="100%" height="100%" fill="white"/>',
        f'<text x="{width/2}" y="32" text-anchor="middle" font-family="Arial" font-size="22" font-weight="700">{title}</text>',
        f'<line x1="{left}" y1="{top}" x2="{left}" y2="{top+plot_h}" stroke="#333"/>',
        f'<line x1="{left}" y1="{top+plot_h}" x2="{left+plot_w}" y2="{top+plot_h}" stroke="#333"/>',
        f'<text x="{width/2}" y="{height-24}" text-anchor="middle" font-family="Arial" font-size="14">{x_label}</text>',
        f'<text x="24" y="{height/2}" text-anchor="middle" font-family="Arial" font-size="14" transform="rotate(-90 24 {height/2})">{y_label}</text>',
    ]
    for i in range(6):
        y = max_y * i / 5.0
        yy = sy(y)
        lines.append(f'<line x1="{left}" y1="{yy:.1f}" x2="{left+plot_w}" y2="{yy:.1f}" stroke="#e5e7eb"/>')
        lines.append(f'<text x="{left-10}" y="{yy+4:.1f}" text-anchor="end" font-family="Arial" font-size="12">{y:.3g}</text>')
    for x in xs:
        xx = sx(x)
        lines.append(f'<text x="{xx:.1f}" y="{top+plot_h+22}" text-anchor="middle" font-family="Arial" font-size="12">{x:g}</text>')

    legend_y = top
    for idx, (name, pts) in enumerate(sorted(points_by_name.items())):
        color = colors[idx % len(colors)]
        pts = sorted(pts)
        path = " ".join(f'{sx(x):.1f},{sy(y):.1f}' for x, y in pts)
        lines.append(f'<polyline points="{path}" fill="none" stroke="{color}" stroke-width="3"/>')
        for x, y in pts:
            lines.append(f'<circle cx="{sx(x):.1f}" cy="{sy(y):.1f}" r="4" fill="{color}"/>')
        lx = left + plot_w - 240
        ly = legend_y + idx * 22
        lines.append(f'<rect x="{lx}" y="{ly-10}" width="14" height="4" fill="{color}"/>')
        lines.append(f'<text x="{lx+22}" y="{ly-4}" font-family="Arial" font-size="12">{name}</text>')
    lines.append("</svg>")
    Path(out_path).parent.mkdir(parents=True, exist_ok=True)
    Path(out_path).write_text("\n".join(lines), encoding="utf-8")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--summary", default="results/summary.csv")
    parser.add_argument("--out-dir", default="docs/charts")
    args = parser.parse_args()

    rows = read_rows(args.summary)
    metrics = [
        ("time_min_sec", "Execution Time", "Seconds"),
        ("speedup", "Speedup", "Speedup"),
        ("efficiency", "Parallel Efficiency", "Efficiency"),
    ]
    by_label_n = defaultdict(list)
    for row in rows:
        by_label_n[(row["run_label"], row["n"])].append(row)

    for (label, n), group in by_label_n.items():
        for metric, title, y_label in metrics:
            series = defaultdict(list)
            for row in group:
                y = to_float(row.get(metric))
                if y is None:
                    continue
                x = float(row["processes"] if label != "node_sweep" else row["nodes"])
                name = row["variant"]
                series[name].append((x, y))
            x_label = "Nodes" if label == "node_sweep" else "Processes"
            out = Path(args.out_dir) / f"{label}_N{n}_{metric}.svg"
            make_svg(series, f"{title} - {label} - N={n}", x_label, y_label, out)
            make_png(series, f"{title} - {label} - N={n}", x_label, y_label, out.with_suffix(".png"))


if __name__ == "__main__":
    main()
