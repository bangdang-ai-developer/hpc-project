# Do an HPC: Nhan ma tran lon bang C++ + MPI

Project thuc hien de tai 1: song song hoa phep nhan ma tran kich thuoc lon bang C++ + MPI/Open MPI.

Muc tieu cua project la giu code va tai lieu gon: de build, de demo, de benchmark, va de nop bai.
Code C++ co chu dich viet theo style bai thuc hanh MPI: tach file thuat toan va file benchmark, it abstraction, de chi ra `Scatterv`, `Bcast`, `Gatherv` khi bao ve.

## Cau truc chinh

```text
src/matrix_hpc.cpp      Thuat toan chinh
src/benchmark.cpp       CLI, demo, benchmark, evidence
src/matrix_hpc.h        Khai bao dung chung
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
powershell -ExecutionPolicy Bypass -File scripts/windows_run_local_pipeline.ps1 -Shapes "2048x2048x2048"
```

Neu may chiu duoc, chay them shape lon hon:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/windows_run_local_pipeline.ps1 -Shapes "2048x2048x2048 4096x4096x4096"
```

Ket qua sinh ra:

```text
results/*.csv
outputs/C_sample_*.csv
docs/charts/*.png
docs/BaoCao_DeTai1_NhanMaTran_MPI.docx
```

Ket qua benchmark cuoi cho bai `10000x10000x10000` da duoc commit trong:

```text
results/
docs/report_10000/BaoCao_MPI_NhanMaTran_10000.pdf
docs/report_10000/report_10000.tex
docs/report_10000/figures/
```

## Benchmark 3 may that

Sua `config/hosts` theo `config/hosts.example`, hoac copy cac template
`config/hosts_9`, `config/hosts_12`, `config/hosts_44` roi thay `node1/node2/node3`
bang IP/hostname that cua cac may trong LAN. Sau do chay:

```bash
bash scripts/check_cluster.sh
SSH_USER=USER HOSTFILE=config/hosts SHAPES="2048x2048x2048 4096x4096x4096" NODES_LIST="1 2 3" PPN=4 REPEAT=5 bash scripts/run_multinode_pipeline.sh
```

Phan 3 node chi chay khi co 3 may Linux that trong cung LAN/Wi-Fi.
Nhung hostfile `hosts_*` trong repo chi giu slot mapping de tai hien benchmark,
khong chua IP may ca nhan.

## Thuat toan

- `seq`: nhan ma tran tuan tu A(MxK) * B(KxN), dung de phan tich `O(M*K*N)`.
- `seq_tiled`: ban tuan tu co toi uu cache.
- `mpi`: chia hang cua A, broadcast B, gather C.
- `mpi_tiled`: chia hang + tiled local multiply, dung cho benchmark chinh.
- `mpi_2d`: chia process theo luoi 2D, scatter block hang A va block cot B da transpose theo y tuong matrix-v2.

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
- `docs/CODE_WALKTHROUGH.md`: giai thich code va cach demo truoc thay.
- `docs/INSTALL_FOR_TEAMMATES.md`: setup may moi va chuan bi 3 may Linux.
- `docs/SUBMISSION_CHECKLIST.md`: checklist truoc khi nop.
