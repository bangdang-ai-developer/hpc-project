# Demo cheat sheet

Tai lieu nay dung khi demo truc tiep cho thay.

## 1. Chuan bi truoc demo

Tren PowerShell Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/windows_run_demo.ps1
```

Hoac trong WSL/Linux:

```bash
bash scripts/demo.sh
```

## 2. Gia tri nen nhap khi demo nhanh

Neu may bao co 8 logical CPU:

```text
How many parallel CPU/processes? 8
Choose algorithm: 4
N: 1024
Seed: Enter
Save full result: n
```

Neu muon demo workload lon hon va co thoi gian cho may chay:

```text
How many parallel CPU/processes? 8
Choose algorithm: 4
N: 4096
Seed: Enter
Save full result: n
```

Khong nen demo truc tiep voi `N=8192` neu khong chac thoi gian/RAM.

## 3. Noi gi khi chuong trinh hien RUN CONFIG

Noi ngan gon:

```text
Day la cau hinh chay hien tai: kich thuoc ma tran N, so MPI process, so node, tile size, RAM uoc luong va so FLOP ly thuyet. Chuong trinh dung pure MPI nen threads_per_process = 1.
```

Neu thay hoi `N=4096` lon co nao:

```text
N=4096 nghia la moi ma tran co 4096 x 4096 phan tu. So phep tinh ly thuyet la 2 x 4096^3, xap xi 137.4 ty FLOP.
```

## 4. Noi gi khi chuong trinh hien PERFORMANCE METRICS

Giai thich theo thu tu:

```text
Execution time la thoi gian chay thuc te.
Throughput la GFLOP/s, tinh tu so FLOP ly thuyet chia cho thoi gian.
Traditional speedup = baseline_time / parallel_time.
Efficiency = speedup / so process.
Amdahl serial fraction uoc luong phan khong song song hoa hieu qua.
Checksum hash dung de kiem tra ket qua dung/sai giua cac bien the.
Sample file la mau 8x8 cua ma tran ket qua C de lam evidence.
```

## 5. Cau tra loi nhanh cho cac cau hoi hay gap

Hoi: Vi sao speedup khong bang 8 khi chay 8 process?

Tra loi:

```text
Vi MPI co overhead scatter/broadcast/gather, dong bo process, gioi han bang thong bo nho va phan serial theo Amdahl.
```

Hoi: Vi sao dung checksum thay vi in ca ma tran?

Tra loi:

```text
Ma tran lon co the hang tram MB den nhieu GB, nen in full khong thuc te. Checksum va sample 8x8 giup chung minh correctness gon hon.
```

Hoi: Code song song hoa o dau?

Tra loi:

```text
Song song hoa o buoc chia hang A cho cac MPI process bang MPI_Scatterv, broadcast B bang MPI_Bcast, moi process tinh mot phan C, sau do gom ket qua bang MPI_Gatherv.
```

Hoi: Vi sao co `seq_tiled`?

Tra loi:

```text
seq_tiled la baseline tuan tu toi uu cache. Dung baseline manh hon giup so sanh voi MPI cong bang hon.
```

## 6. Neu demo bi loi

Mo:

```text
docs/TROUBLESHOOTING.md
```

Loi hay gap nhat:

```text
Thieu python3-docx
OpenMPI khong cho chay root
Khong du slot khi chay nhieu process
N qua lon vuot RAM
```
