# Huong dan chay tren 3 may Linux rieng biet

May cua ban la may chinh. Hai may con lai la worker.

## 1. Ba may cung mot mang

Tren moi may Ubuntu, lay IP:

```bash
ip -4 addr
```

Vi du:

```text
May chinh: 192.168.1.10
May ban 2: 192.168.1.11
May ban 3: 192.168.1.12
```

Neu Wi-Fi truong chan SSH, dung hotspot/router rieng.

## 2. Cai phan mem tren ca 3 may

```bash
sudo apt update
sudo apt install -y build-essential openmpi-bin libopenmpi-dev python3 python3-docx rsync openssh-server
sudo systemctl enable --now ssh
```

## 3. SSH tu may chinh sang worker

Tren may chinh:

```bash
ssh-keygen -t ed25519
ssh-copy-id USER@192.168.1.11
ssh-copy-id USER@192.168.1.12
```

Kiem tra:

```bash
ssh USER@192.168.1.11 hostname
ssh USER@192.168.1.12 hostname
```

Neu username tren cac may khac username hien tai, luon them `SSH_USER=USER` khi chay cac script cluster. Script se truyen user nay cho ca `ssh` va `mpirun`.

## 4. Tao hostfile

```bash
cp config/hosts.example config/hosts
```

Sua `config/hosts`:

```text
192.168.1.10 slots=4
192.168.1.11 slots=4
192.168.1.12 slots=4
```

## 5. Build va test tren tung may

```bash
make
mpirun -np 2 ./build/matrix_hpc --variant mpi_tiled --n 128 --repeat 1 --max-ram-gb 1 --save-sample
```

## 6. Dong bo code sang worker

Tren may chinh:

```bash
SSH_USER=USER bash scripts/sync_to_nodes.sh
```

Mac dinh script se dong bo worker vao cung duong dan voi thu muc hien tai tren may chinh. Khuyen nghi dat project tren may chinh o:

```bash
~/TinhToanHieuNangCao
```

Neu muon dung duong dan khac tren ca 3 may, truyen ro:

```bash
SSH_USER=USER REMOTE_DIR=/home/USER/TinhToanHieuNangCao bash scripts/sync_to_nodes.sh
```

## 7. Kiem tra cluster

```bash
SSH_USER=USER bash scripts/check_cluster.sh
```

## 8. Benchmark nho truoc

```bash
SSH_USER=USER HOSTFILE=config/hosts N_LIST="512" NODES_LIST="1 2 3" PPN=2 bash scripts/run_multinode.sh
python3 scripts/summarize_results.py
python3 scripts/compare_checksums.py
```

## 9. Benchmark chinh

```bash
SSH_USER=USER HOSTFILE=config/hosts N_LIST="2048 4096" NODES_LIST="1 2 3" PPN=4 REPEAT=5 bash scripts/run_multinode_pipeline.sh
```

## 10. Giai thich voi thay

Code chi nam trong `src/matrix_hpc.c`.

Y tuong MPI:

- Rank 0 sinh/doc A va B.
- Chia cac hang cua A cho cac process bang `MPI_Scatterv`.
- Gui B cho moi process bang `MPI_Bcast`.
- Moi process tinh cac hang cua C ma minh phu trach.
- Rank 0 gom C bang `MPI_Gatherv`.

Bien the `mpi_tiled` chi khac o buoc tinh local: chia tile de cache tot hon.
