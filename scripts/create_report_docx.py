#!/usr/bin/env python3
from pathlib import Path
import csv

from docx import Document
from docx.enum.table import WD_CELL_VERTICAL_ALIGNMENT
from docx.enum.text import WD_ALIGN_PARAGRAPH
from docx.oxml import OxmlElement
from docx.oxml.ns import qn
from docx.shared import Inches, Pt, RGBColor

ROOT = Path(__file__).resolve().parents[1]
SUMMARY = ROOT / "results" / "summary.csv"
CHECKSUM_SUMMARY = ROOT / "results" / "checksums_summary.csv"
ENVIRONMENT = ROOT / "results" / "environment.csv"
CHART_DIR = ROOT / "docs" / "charts"
GROUP_INFO = ROOT / "config" / "group_info.txt"
OUT = ROOT / "docs" / "BaoCao_DeTai1_NhanMaTran_MPI.docx"
MAX_REPORT_CHARTS = 6


def group_info():
    if not GROUP_INFO.exists():
        return "Nhom thuc hien: chua cap nhat ten 3 thanh vien"
    lines = []
    with GROUP_INFO.open(encoding="utf-8") as f:
        for raw in f:
            line = raw.strip()
            if line and not line.startswith("#"):
                lines.append(line)
    if not lines:
        return "Nhom thuc hien: chua cap nhat ten 3 thanh vien"
    return "Nhom thuc hien: " + "; ".join(lines)


def rows(path):
    if not path.exists():
        return []
    with path.open(newline="", encoding="utf-8") as f:
        return list(csv.DictReader(f))


def shade(cell, fill="F2F4F7"):
    shd = OxmlElement("w:shd")
    shd.set(qn("w:fill"), fill)
    cell._tc.get_or_add_tcPr().append(shd)


def table(doc, headers, data):
    t = doc.add_table(rows=1, cols=len(headers))
    t.style = "Table Grid"
    for i, h in enumerate(headers):
        t.rows[0].cells[i].text = h
        shade(t.rows[0].cells[i])
    for row in data:
        cells = t.add_row().cells
        for i, v in enumerate(row):
            cells[i].text = str(v)
            cells[i].vertical_alignment = WD_CELL_VERTICAL_ALIGNMENT.CENTER
    return t


def h(doc, text, level=1):
    p = doc.add_heading(text, level=level)
    if p.runs:
        p.runs[0].font.color.rgb = RGBColor(0x2E, 0x74, 0xB5)
    return p


def p(doc, text):
    para = doc.add_paragraph(text)
    para.paragraph_format.space_after = Pt(6)
    para.paragraph_format.line_spacing = 1.10
    return para


def bullet(doc, text):
    para = doc.add_paragraph(text, style="List Bullet")
    para.paragraph_format.space_after = Pt(4)
    return para


def add_chart_images(doc):
    charts = sorted(CHART_DIR.glob("*.png"))
    if not charts:
        p(doc, "Bieu do se duoc tu dong chen tu docs/charts sau khi chay scripts/make_chart_svgs.py.")
        return
    for chart in charts[:MAX_REPORT_CHARTS]:
        doc.add_picture(str(chart), width=Inches(6.4))
        cap = doc.add_paragraph(chart.stem)
        cap.alignment = WD_ALIGN_PARAGRAPH.CENTER
        cap.paragraph_format.space_after = Pt(8)
    if len(charts) > MAX_REPORT_CHARTS:
        p(doc, f"Bao cao chi chen {MAX_REPORT_CHARTS} bieu do tieu bieu de giu do dai gon. Toan bo bieu do day du nam trong thu muc docs/charts.")


def build_doc():
    doc = Document()
    sec = doc.sections[0]
    sec.top_margin = sec.bottom_margin = sec.left_margin = sec.right_margin = Inches(1)
    doc.styles["Normal"].font.name = "Calibri"
    doc.styles["Normal"].font.size = Pt(11)

    title = doc.add_paragraph()
    title.alignment = WD_ALIGN_PARAGRAPH.CENTER
    r = title.add_run("BAO CAO CUOI KY\nDE TAI 1: SONG SONG HOA NHAN MA TRAN KICH THUOC LON")
    r.bold = True
    r.font.size = Pt(18)
    r.font.color.rgb = RGBColor(0x0B, 0x25, 0x45)
    sub = doc.add_paragraph("Hoc phan: Tinh toan hieu nang cao (High Performance Computing)")
    sub.alignment = WD_ALIGN_PARAGRAPH.CENTER
    info = doc.add_paragraph(group_info())
    info.alignment = WD_ALIGN_PARAGRAPH.CENTER

    h(doc, "1. Co so ly thuyet va phan tich thuat toan")
    p(doc, "Gioi thieu bai toan: de tai thuc hien phep nhan ma tran kich thuoc lon, xay dung chuong trinh tuan tu va chuong trinh song song bang MPI/OpenMPI, sau do danh gia kha nang tang toc khi thay doi so process va so node.")
    p(doc, "Bai toan de tai 1 la nhan hai ma tran chu nhat kich thuoc lon C = A x B. Voi A co kich thuoc M x K va B co kich thuoc K x N, ket qua C co kich thuoc M x N. Moi phan tu C[i,j] duoc tinh bang tong A[i,t] * B[t,j] voi t tu 0 den K-1.")
    p(doc, "Thuat toan tuan tu co ba vong lap long nhau nen do phuc tap tinh toan la O(M x K x N). So phep toan dau cham dong ly thuyet xap xi 2 x M x K x N FLOP.")
    table(doc, ["Variant", "Vai tro", "Ghi chu"], [
        ["seq", "Baseline tuan tu", "De giai thich cong thuc O(M x K x N)."],
        ["seq_tiled", "Baseline tuan tu toi uu cache", "Dung tile de tang locality bo nho."],
        ["mpi", "Ban song song MPI", "Chia hang A, broadcast B, gather C."],
        ["mpi_tiled", "Ban song song MPI toi uu cache", "Khuyen nghi dung cho benchmark chinh."],
        ["mpi_2d", "Ban MPI chia luoi 2D", "Scatter block hang A va block cot B da transpose theo matrix-v2."],
    ])

    h(doc, "2. Thiet ke giai phap song song bang MPI/OpenMPI")
    p(doc, "Chuong trinh dung pure MPI, khong dung OpenMP. Cac bien the mpi/mpi_tiled chia cac hang cua A bang MPI_Scatterv, broadcast toan bo B bang MPI_Bcast, moi process tinh mot khoi hang cua C, sau do rank 0 gom ket qua bang MPI_Gatherv. Bien the mpi_2d dung luoi process 2 chieu, scatter block hang A va block cot B da transpose, moi process tinh mot block C.")
    table(doc, ["Thanh phan", "Thiet ke don gian"], [
        ["Phan chia du lieu", "Chia theo hang; cac process nhan so hang gan bang nhau."],
        ["Tinh local", "Nhan basic/tiled tren phan hang, hoac nhan block voi B transpose trong mpi_2d."],
        ["Input dong", "Demo interactive cho nhap N/variant/seed; sai thi bao loi va nhap lai."],
        ["Kiem soat RAM", "Uoc luong RAM truoc khi chay va tu choi cau hinh vuot nguong."],
        ["Core/process", "threads_per_process = 1; danh gia mapping/binding bang OpenMPI."],
    ])

    h(doc, "3. Cai dat chuong trinh")
    p(doc, "De giu code de doc va de demo, phan thuat toan nam trong src/matrix_hpc.c, phan CLI/benchmark/evidence nam trong src/benchmark.c. Makefile build bang mpicc voi -O3.")
    table(doc, ["Hang muc", "Trang thai"], [
        ["Ma nguon", "src/matrix_hpc.c, src/benchmark.c, src/matrix_hpc.h"],
        ["Build", "make tao build/matrix_hpc"],
        ["Demo", "scripts/demo.sh hoi so process truoc roi chay mpirun"],
        ["Benchmark 1 may", "scripts/run_benchmark.sh"],
        ["Benchmark 3 may that", "scripts/run_multinode.sh, can config/hosts"],
        ["Evidence", "raw_runs.csv, summary.csv, checksums.csv, sample CSV"],
    ])
    table(doc, ["File/script", "Muc dich"], [
        ["scripts/run_local_pipeline.sh", "Chay benchmark local, tao chart, kiem checksum, tao Word report."],
        ["scripts/run_multinode.sh", "Chay benchmark 1/2/3 node khi co 3 may that."],
        ["scripts/check_local_env.sh", "Kiem tra GCC, OpenMPI, Python va python-docx."],
        ["docs/INSTALL_FOR_TEAMMATES.md", "Huong dan clone, cai moi truong va chuan bi 3 may."],
        ["docs/SUBMISSION_CHECKLIST.md", "Checklist artifact truoc khi nop."],
    ])

    h(doc, "4. Ket qua thuc nghiem va danh gia hieu nang")
    p(doc, "Chi so bat buoc gom Execution Time, Speedup S(p)=T(1)/T(p), Parallel Efficiency E(p)=S(p)/p. Bao cao cung ghi them Amdahl serial fraction va Amdahl max speedup de giai thich gioi han tang toc.")
    table(doc, ["Cau hinh yeu cau", "Trang thai"], [
        ["Process sweep", "Can chay 1,2,4,8 va 16 neu tai nguyen cho phep."],
        ["Node sweep", "Can chay 1,2,3 node tren 3 may Linux rieng biet."],
        ["Repeat", "5 lan moi cau hinh de giam nhieu."],
        ["Kich thuoc MxKxN", "Khuyen nghi 2048x2048x2048, 4096x4096x4096; thu shape chu nhat neu can demo M,K,N khac nhau."],
    ])
    env_rows = rows(ENVIRONMENT)
    if env_rows:
        table(doc, ["role", "host", "hostname", "OS", "CPU", "RAM", "GCC", "MPI"],
              [[
                  r.get("role", ""),
                  r.get("host", ""),
                  r.get("hostname", ""),
                  r.get("os", ""),
                  r.get("cpu_count", ""),
                  r.get("mem_total", ""),
                  r.get("gcc", ""),
                  r.get("mpirun", ""),
              ] for r in env_rows[:4]])
    else:
        table(doc, ["Thong tin moi truong", "Gia tri can cap nhat"], [
            ["May chinh", "Cap nhat CPU/RAM/OS sau khi benchmark local."],
            ["May worker 1", "Cap nhat CPU/RAM/OS khi co may that."],
            ["May worker 2", "Cap nhat CPU/RAM/OS khi co may that."],
            ["Mang", "Cung LAN/Wi-Fi, SSH tu may chinh sang worker."],
            ["Compiler/MPI", "GCC + OpenMPI, lay version tu scripts/check_local_env.sh."],
        ])
    summary = rows(SUMMARY)
    if summary:
        table(doc, ["variant", "M", "K", "N", "P", "nodes", "avg(s)", "min(s)", "speedup", "eff", "Amdahl serial", "Amdahl max"],
              [[
                  r["variant"],
                  r["m"],
                  r["k"],
                  r["n"],
                  r["processes"],
                  r["nodes"],
                  r["time_avg_sec"],
                  r["time_min_sec"],
                  r.get("speedup", ""),
                  r.get("efficiency", ""),
                  r.get("amdahl_serial_fraction", ""),
                  r.get("amdahl_max_speedup", ""),
              ] for r in summary[:14]])
    else:
        p(doc, "Chua co du lieu benchmark chinh thuc trong results/summary.csv. Bang duoi day la ke hoach do, khong phai so lieu thuc nghiem. Sau khi chay pipeline benchmark, script se tu dong chen bang ket qua that.")
        table(doc, ["MxKxN", "Variant", "Pham vi do", "Trang thai evidence"], [
            ["2048x2048x2048", "mpi_tiled, mpi_2d", "1,2,4,8 processes", "Cho chay benchmark local/cluster"],
            ["4096x4096x4096", "mpi_tiled, mpi_2d", "1,2,4,8 processes; 1,2,3 nodes", "Cho chay benchmark local/cluster"],
            ["3072x2048x4096", "mpi_tiled, mpi_2d", "Mo rong neu RAM/thoi gian cho phep", "Tuy chon"],
        ])

    h(doc, "Bieu do Execution Time, Speedup, Efficiency", 2)
    add_chart_images(doc)

    checks = rows(CHECKSUM_SUMMARY)
    if checks:
        h(doc, "Evidence checksum", 2)
        table(doc, ["variant", "M", "K", "N", "processes", "nodes", "status"],
              [[r["variant"], r["m"], r["k"], r["n"], r["processes"], r["nodes"], r["status"]] for r in checks[:8]])

    h(doc, "5. Ket luan va phu luc nop bai")
    p(doc, "Do an da xay dung chuong trinh C + MPI gon, de giai thich, co ban tuan tu, ban song song, benchmark script va evidence checksum/sample. Cac ket qua 3 node se duoc cap nhat sau khi co du 3 may Linux that trong cung LAN/Wi-Fi.")

    h(doc, "Phu luc: lenh chay mau")
    p(doc, 'SHAPES="2048x2048x2048 4096x4096x4096" NP_LIST="1 2 4 8" bash scripts/run_benchmark.sh')
    p(doc, 'Neu cluster du tai nguyen, co the them 16 vao NP_LIST.')
    p(doc, 'HOSTFILE=config/hosts SHAPES="2048x2048x2048 4096x4096x4096" NODES_LIST="1 2 3" PPN=4 bash scripts/run_multinode.sh')

    OUT.parent.mkdir(parents=True, exist_ok=True)
    doc.save(OUT)
    print(OUT)


if __name__ == "__main__":
    build_doc()
