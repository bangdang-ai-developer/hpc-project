# Do an HPC: Nhan ma tran lon bang C + MPI

Project thuc hien de tai 1: song song hoa phep nhan ma tran kich thuoc lon bang C + MPI/OpenMPI.

Muc tieu cua project la giu code va tai lieu gon: de build, de demo, de benchmark, va de nop bai.

## Cau truc chinh

```text
src/matrix_hpc.c        Code C chinh
Makefile                Build va cac lenh tien ich
scripts/                Script demo, benchmark, report, package
config/                 Mau hostfile va thong tin nhom
docs/                   Huong dan toi thieu va bao cao sinh ra sau benchmark
```

## Cai dat tren Ubuntu/WSL

```bash
bash scripts/setup_ubuntu.sh
make check
```

Neu cai thu cong:

```bash
sudo apt update
sudo apt install -y build-essential openmpi-bin libopenmpi-dev python3 python3-docx rsync openssh-server
```

## Build va demo

```bash
make
make demo
```

Tren Windows PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/windows_run_demo.ps1
```

## Benchmark local va tao report

Chay nhanh tren may hien tai:

```bash
make pipeline
```

Tren Windows PowerShell:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/windows_run_local_pipeline.ps1 -NList "2048"
```

Neu may chiu duoc, chay them `N=4096`:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/windows_run_local_pipeline.ps1 -NList "2048 4096"
```

Ket qua sinh ra:

```text
results/*.csv
outputs/C_sample_*.csv
docs/charts/*.png
docs/BaoCao_DeTai1_NhanMaTran_MPI.docx
```

## Benchmark 3 may that

Sua `config/hosts` theo `config/hosts.example`, sau do chay:

```bash
bash scripts/check_cluster.sh
SSH_USER=USER HOSTFILE=config/hosts N_LIST="2048 4096" NODES_LIST="1 2 3" PPN=4 REPEAT=5 bash scripts/run_multinode_pipeline.sh
```

Phan 3 node chi chay khi co 3 may Linux that trong cung LAN/Wi-Fi.

## Thuat toan

- `seq`: nhan ma tran tuan tu, dung de phan tich `O(N^3)`.
- `seq_tiled`: ban tuan tu co toi uu cache.
- `mpi`: chia hang cua A, broadcast B, gather C.
- `mpi_tiled`: chia hang + tiled local multiply, dung cho benchmark chinh.

## Chuan bi nop bai

Tao file thong tin nhom:

```bash
make group-info
```

Tao report sau benchmark:

```bash
make report
```

Kiem tra va dong goi:

```bash
make preflight
make package
```

Neu `make preflight` fail, nghia la chua du evidence benchmark/report de nop.

## Tai lieu can doc

- `START_HERE.md`: luong thao tac ngan nhat.
- `docs/INSTALL_FOR_TEAMMATES.md`: huong dan clone va cai moi truong cho thanh vien con lai.
- `docs/CODE_WALKTHROUGH.md`: giai thich code khi demo.
- `docs/DEMO_CHEAT_SHEET.md`: cach demo truoc thay.
- `docs/THREE_PHYSICAL_MACHINES_SETUP.md`: setup 3 may Linux.
- `docs/SUBMISSION_CHECKLIST.md`: checklist truoc khi nop.
