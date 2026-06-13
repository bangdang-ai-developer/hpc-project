# START HERE

Doc file nay dau tien khi mo project.

## 1. Cai dat moi truong

Neu la thanh vien moi clone project ve lan dau, doc:

```text
docs/INSTALL_FOR_TEAMMATES.md
```

Tren Ubuntu/WSL:

```bash
bash scripts/setup_ubuntu.sh
make check
```

Neu dung Windows, mo PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/windows_setup_wsl.ps1
```

## 2. Demo nhanh

PowerShell Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/windows_run_demo.ps1
```

WSL/Linux:

```bash
make demo
```

Khi demo, chon `mpi_tiled`, nhap `N=128` hoac `N=256` de thay ket qua nhanh. Muon benchmark nghiem tuc dung `N=2048` hoac `N=4096`.

## 3. Chay benchmark local

PowerShell Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/windows_run_local_pipeline.ps1 -NList "2048"
```

WSL/Linux:

```bash
make pipeline
```

Ket qua se nam trong:

```text
results/*.csv
outputs/C_sample_*.csv
docs/charts/*.png
docs/BaoCao_DeTai1_NhanMaTran_MPI.docx
```

## 4. Chuan bi report

Tao file thong tin nhom:

```bash
make group-info
```

Tren Windows co the dung:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/windows_prepare_group_info.ps1
```

Sau khi co benchmark va da dien ten nhom:

```bash
make report
```

## 5. Benchmark 3 may that

Doc:

```text
docs/THREE_PHYSICAL_MACHINES_SETUP.md
```

Sau khi co IP/SSH cua 3 may Linux:

```bash
bash scripts/check_cluster.sh
SSH_USER=USER HOSTFILE=config/hosts N_LIST="2048 4096" NODES_LIST="1 2 3" PPN=4 REPEAT=5 bash scripts/run_multinode_pipeline.sh
```

## 6. Truoc khi nop

Doc:

```text
docs/SUBMISSION_CHECKLIST.md
```

Chay:

```bash
make preflight
make package
```

`make preflight` chi pass khi da co du benchmark, checksum, chart, report va thong tin nhom.

## 7. Khi can giai thich

- `docs/CODE_WALKTHROUGH.md`: giai thich code va luong MPI.
- `docs/DEMO_CHEAT_SHEET.md`: kich ban demo ngan.
