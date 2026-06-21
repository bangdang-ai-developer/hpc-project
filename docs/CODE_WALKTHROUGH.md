# Code walkthrough

Tai lieu nay dung de giai thich nhanh code C khi demo/bao ve.

File can xem:

```text
src/matrix_hpc.c   thuat toan chinh
src/benchmark.c    CLI, do thoi gian, checksum, ghi evidence
src/matrix_hpc.h   khai bao dung chung
```

## 1. Muc tieu cua chuong trinh

Chuong trinh thuc hien phep nhan ma tran chu nhat:

```text
A(M x K) x B(K x N) = C(M x N)
```

Voi kich thuoc:

```text
A co M x K phan tu, B co K x N phan tu, C co M x N phan tu
```

Do phuc tap thuat toan co ban:

```text
O(M x K x N)
```

So phep tinh dau cham dong ly thuyet:

```text
2 x M x K x N FLOP
```

## 2. Cac bien the thuat toan

| Variant | Y nghia |
|---|---|
| `seq` | Tuan tu 3 vong lap, de giai thich baseline |
| `seq_tiled` | Tuan tu co chia tile de cai thien cache |
| `mpi` | MPI chia hang A, broadcast B, gather C |
| `mpi_tiled` | MPI chia hang + tiled local multiply, khuyen nghi benchmark |
| `mpi_2d` | MPI chia process theo luoi 2D, scatter block A/B theo y tuong matrix-v2 |

## 3. Luong chay tong quat

```text
main()
  MPI_Init
  parse CLI hoac interactive input
  validate M/K/N/variant/repeat/tile/RAM
  detect node count
  in RUN CONFIG
  neu variant seq/seq_tiled -> run_seq()
  neu variant mpi/mpi_tiled -> run_mpi()
  neu variant mpi_2d -> run_mpi_2d()
  MPI_Finalize
```

Khi bao ve, chi can tap trung vao 4 ham chinh trong code:

```text
multiply_basic()   cong thuc nhan ma tran tuan tu
multiply_tiled()   toi uu cache bang tile
run_seq()          chay baseline tuan tu
run_mpi()          song song hoa bang Scatterv, Bcast, Gatherv
```

Nhung ham con lai chu yeu phuc vu input, checksum va ghi evidence.

## 4. Input dong va validate

Demo interactive cho nguoi dung nhap:

```text
variant
M
K
N
seed
co luu full result khong
```

Neu sai, chuong trinh khong crash ma bao loi va yeu cau nhap lai.

Cac loi duoc chan:

```text
variant khong hop le
M/K/N <= 0
repeat <= 0
tile <= 0
seq nhung chay nhieu process
uoc luong RAM vuot --max-ram-gb
```

## 5. Sinh du lieu

Rank 0 sinh ma tran A va B bang seed co dinh.

Ly do dung seed:

```text
Cung M/K/N + cung seed -> cung input -> co the so sanh checksum giua cac variant
```

## 6. Thuat toan tuan tu

`seq` tinh moi phan tu C:

```text
C[i][j] = sum(A[i][t] * B[t][j]), voi t = 0..K-1
```

`seq_tiled` van la cung cong thuc, nhung chia block/tile de cache tot hon.

## 7. Thuat toan MPI

Y tuong cua `mpi` va `mpi_tiled`:

```text
Rank 0 giu A, B, C day du
MPI_Scatterv chia cac hang cua A cho process
MPI_Bcast gui B(KxN) cho tat ca process
Moi process tinh cac hang C cua minh
MPI_Gatherv gom C ve rank 0
```

Y tuong cua `mpi_2d`:

```text
MPI_Dims_create tao luoi row_parts x col_parts
Rank 0 pack A theo block hang va B theo block cot
B local duoc luu transpose de moi rank doc lien tuc theo k
MPI_Scatter gui A_local va B_local cho tung rank
Moi rank tinh mot block C_local
MPI_Gather gom block C, rank 0 unpack ve ma tran C day du
```

Vi chia theo hang, moi process nhan gan bang nhau:

```text
rows = M / P, mot so process dau nhan them 1 hang neu M khong chia het cho P
```

## 8. Tai sao dung flat array 1D

Ma tran duoc luu bang mang 1 chieu:

```text
A[i * K + j], B[i * N + j], C[i * N + j]
```

Ly do:

```text
Cap phat dong don gian
Truyen MPI lien tuc trong bo nho
Cache-friendly hon con tro 2 chieu roi rac
```

## 9. Chi so hieu nang in ra terminal

Moi run in:

```text
Execution time
GFLOP/s
Traditional speedup
Efficiency
Amdahl serial fraction
Amdahl parallel fraction
Amdahl speedup
Amdahl max speedup
Checksum
Sample file
```

Phan RUN CONFIG in them M/K/N, so process, so node, RAM uoc luong va FLOP ly thuyet.

Cong thuc:

```text
Speedup S(p) = T(1) / T(p)
Efficiency E(p) = S(p) / p
Amdahl S(p) = 1 / (serial + parallel / p)
```

## 10. Checksum va evidence

Rank 0 tinh checksum cua C:

```text
sum
abs_sum
max_abs
hash
```

Muc dich:

```text
Chung minh cac bien the seq/mpi tao ket qua tuong duong
Luu evidence vao results/checksums.csv
Dung compare_checksums.py de kiem tra OK/CHECK
```

Ngoai checksum, chuong trinh luu sample:

```text
outputs/C_sample_*.csv
```

Sample la goc tren trai 8x8 cua ma tran C, de dua vao phu luc neu can.

## 11. Cau tra loi ngan khi thay hoi

Neu thay hoi “song song hoa o dau?”, tra loi:

```text
Em chia ma tran A theo hang cho cac MPI process bang MPI_Scatterv.
Ma tran B duoc broadcast cho tat ca process bang MPI_Bcast.
Moi process tinh mot nhom hang cua C, sau do gom ket qua ve rank 0 bang MPI_Gatherv.
```

Neu thay hoi “vi sao speedup khong bang so process?”, tra loi:

```text
Vi co overhead truyen du lieu, broadcast B, scatter/gather A/C, dong bo MPI, gioi han bang thong bo nho va phan khong song song hoa duoc theo Amdahl.
```

Neu thay hoi “vi sao dung mpi_tiled?”, tra loi:

```text
mpi_tiled van giu y tuong MPI don gian, nhung phan tinh local chia tile de tang cache locality, nen thuong nhanh hon mpi co ban.
```

## 12. Kich ban demo ngan

Chay demo:

```bash
make demo
```

Hoac tren PowerShell Windows:

```powershell
powershell -ExecutionPolicy Bypass -File scripts/windows_run_demo.ps1
```

Gia tri demo nhanh:

```text
How many parallel CPU/processes? 2 hoac 4
Choose algorithm: 4
M: 128
K: 128
N: 128
Seed: Enter
Save full result: n
```

Neu muon demo workload lon hon va co thoi gian:

```text
M/K/N: 1024
```

Khong nen demo truc tiep voi shape `8192x8192x8192` neu khong chac RAM/thoi gian.

Neu thay hoi `mpi_2d` khac gi `mpi_tiled`, tra loi:

```text
mpi_tiled chia A theo hang va broadcast toan bo B cho moi process. mpi_2d chia theo luoi 2 chieu: moi process chi nhan mot block hang cua A va mot block cot cua B da transpose, sau do tinh mot block C. Cach nay giam kich thuoc B local va gan voi y tuong trong matrix-v2.
```

Neu thay hoi `4096x4096x4096` lon co nao, tra loi:

```text
Shape 4096x4096x4096 nghia la A co 4096 x 4096 phan tu, B co 4096 x 4096 phan tu va C co 4096 x 4096 phan tu. So phep tinh ly thuyet la 2 x 4096 x 4096 x 4096, xap xi 137.4 ty FLOP.
```

Neu thay hoi vi sao dung checksum thay vi in ca ma tran, tra loi:

```text
Ma tran lon co the hang tram MB den nhieu GB, nen in full khong thuc te. Checksum va sample 8x8 giup chung minh correctness gon hon.
```
