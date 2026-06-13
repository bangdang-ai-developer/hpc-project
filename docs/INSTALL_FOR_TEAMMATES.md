# Huong dan clone repo va cai moi truong tu dau

Tai lieu nay danh cho 2 thanh vien con lai trong nhom. Muc tieu la cai du moi truong, clone project, build duoc code C + MPI va chay demo nho thanh cong.

Repo GitHub:

```text
https://github.com/bangdang-ai-developer/hpc-project.git
```

Khuyen nghi: benchmark 3 node nen dung Ubuntu cai truc tiep tren 3 may that. WSL2 chi nen dung de test local khi chua cai duoc Ubuntu native.

## A. May Windows chua co Linux

1. Mo PowerShell bang quyen Administrator.

2. Cai WSL2 Ubuntu:

```powershell
wsl --install -d Ubuntu
```

3. Neu Windows yeu cau restart, hay restart may.

4. Mo app `Ubuntu` tu Start Menu.

5. Tao username/password Ubuntu khi duoc hoi.

6. Cai Git trong Ubuntu:

```bash
sudo apt update
sudo apt install -y git
```

7. Clone project:

```bash
cd ~
git clone https://github.com/bangdang-ai-developer/hpc-project.git
cd hpc-project
```

8. Cai moi truong can thiet:

```bash
bash scripts/setup_ubuntu.sh
```

9. Kiem tra moi truong, build va smoke test:

```bash
bash scripts/check_local_env.sh
```

10. Chay demo nho:

```bash
make demo
```

Khi chuong trinh hoi, nhap:

```text
processes: 2
algorithm: 4
N: 128
seed: Enter
save full result: n
```

## B. May cai Ubuntu truc tiep

1. Mo Terminal.

2. Cai Git:

```bash
sudo apt update
sudo apt install -y git
```

3. Clone project:

```bash
cd ~
git clone https://github.com/bangdang-ai-developer/hpc-project.git
cd hpc-project
```

4. Cai package can thiet:

```bash
bash scripts/setup_ubuntu.sh
```

5. Kiem tra moi truong:

```bash
bash scripts/check_local_env.sh
```

6. Build thu cong neu can:

```bash
make
```

7. Chay demo:

```bash
make demo
```

Gia tri demo nen dung:

```text
processes: 2 hoac 4
algorithm: 4
N: 128 hoac 256
seed: Enter
save full result: n
```

## C. Chuan bi lam worker node cho benchmark 3 may

1. Dam bao ca 3 may cung mot Wi-Fi/LAN.

2. Tren moi may, lay IP:

```bash
ip -4 addr
```

3. Gui cho may chinh cac thong tin sau:

```text
Username Ubuntu:
IP may:
So core/vCPU:
RAM:
```

4. Dam bao SSH server dang bat:

```bash
sudo systemctl enable --now ssh
```

5. Neu may bat firewall, cho phep SSH:

```bash
sudo ufw allow ssh
```

6. May chinh se copy SSH key sang 2 may worker:

```bash
ssh-copy-id USER@IP_WORKER_1
ssh-copy-id USER@IP_WORKER_2
```

7. Kiem tra SSH tu may chinh:

```bash
ssh USER@IP_WORKER_1 hostname
ssh USER@IP_WORKER_2 hostname
```

8. Sau khi SSH chay duoc, may chinh se sua `config/hosts`:

```text
IP_MAY_CHINH slots=4
IP_WORKER_1 slots=4
IP_WORKER_2 slots=4
```

9. May chinh kiem tra cluster:

```bash
SSH_USER=USER bash scripts/check_cluster.sh
```

10. May chinh chay benchmark nho truoc:

```bash
SSH_USER=USER HOSTFILE=config/hosts N_LIST="512" NODES_LIST="1 2 3" PPN=2 bash scripts/run_multinode.sh
```

11. Neu benchmark nho OK, may chinh chay benchmark chinh:

```bash
SSH_USER=USER HOSTFILE=config/hosts N_LIST="2048 4096" NODES_LIST="1 2 3" PPN=4 REPEAT=5 bash scripts/run_multinode_pipeline.sh
```

## D. Ghi chu rieng cho may chinh

Tao hostfile:

```bash
cp config/hosts.example config/hosts
```

Sua `config/hosts` theo IP that:

```text
IP_MAY_CHINH slots=4
IP_WORKER_1 slots=4
IP_WORKER_2 slots=4
```

Dong bo code sang worker:

```bash
SSH_USER=USER bash scripts/sync_to_nodes.sh
```

Neu muon worker dung duong dan cu the:

```bash
SSH_USER=USER REMOTE_DIR=/home/USER/hpc-project bash scripts/sync_to_nodes.sh
```

## E. Luu y

- Khong chay `N=4096` hoac `N=8192` ngay khi moi cai, vi co the mat nhieu thoi gian.
- Test ban dau chi dung `N=128`, `N=256`, hoac toi da `N=512`.
- Neu chi lam worker node thi khong can tu sua code.
- Neu bi loi thieu package, chay lai:

```bash
bash scripts/setup_ubuntu.sh
bash scripts/check_local_env.sh
```

- Neu can doc de viet bao cao hoac demo, xem:

```text
docs/CODE_WALKTHROUGH.md
```

## F. Ket qua mong doi

Sau khi cai dung, may cua moi ban phai:

- Clone duoc repo.
- Chay `make` thanh cong.
- Chay `bash scripts/check_local_env.sh` thanh cong.
- Chay `make demo` voi `N=128` thanh cong.
- Neu lam worker node, may chinh SSH vao duoc bang `ssh USER@IP hostname`.
