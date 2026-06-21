# Submission checklist

Checklist nay dung truoc khi nop bai.

## File can co

| Hang muc | File/thu muc |
|---|---|
| Bao cao Word | `docs/BaoCao_DeTai1_NhanMaTran_MPI.docx` |
| Source code | `src/matrix_hpc.c`, `src/benchmark.c`, `src/matrix_hpc.h` |
| Build | `Makefile` |
| Huong dan | `README.md`, `START_HERE.md`, `docs/INSTALL_FOR_TEAMMATES.md` |
| Thong tin nhom | `config/group_info.txt` |
| Benchmark CSV | `results/raw_runs.csv`, `results/summary.csv`, `results/checksums.csv` |
| Bieu do | `docs/charts/*.png` |
| Sample ket qua | `outputs/C_sample_*.csv` |

## Lenh tao evidence local

```bash
SHAPES="2048x2048x2048" NP_LIST="1 2 4 8" REPEAT=5 bash scripts/run_local_pipeline.sh
```

Neu may chiu duoc:

```bash
SHAPES="2048x2048x2048 4096x4096x4096" NP_LIST="1 2 4 8" REPEAT=5 bash scripts/run_local_pipeline.sh
```

## Lenh benchmark 3 may that

Chi chay khi da co 3 may Linux that va `config/hosts` dung IP/hostname that.

```bash
SSH_USER=USER HOSTFILE=config/hosts SHAPES="2048x2048x2048 4096x4096x4096" NODES_LIST="1 2 3" PPN=4 REPEAT=5 bash scripts/run_multinode_pipeline.sh
```

## Kiem tra truoc khi nop

- Da tao `config/group_info.txt` va dien ten 3 thanh vien.
- Bao cao Word khong con placeholder.
- `results/summary.csv` co data that, khong chi co header.
- `results/checksums_summary.csv` khong co status `CHECK`.
- Co bieu do Execution Time, Speedup, Efficiency trong `docs/charts/`.
- Neu chua co 3 may that, khong gia lap so lieu node benchmark.

Chay:

```bash
make preflight
make package
```

Neu `make preflight` fail thi chua nen nop.
