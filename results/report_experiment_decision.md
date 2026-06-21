# MPI Matrix Multiplication Experiment Decision

## Cluster
- node1: 12 WSL logical cores
- node2: 16 WSL logical cores
- node3: 16 WSL logical cores
- Total available: 44 WSL logical cores

## Medium Benchmark Winner
### 1024x1024x1024
- mpi_2d p=16 tile=32 time=2.872s throughput=0.748 GFLOP/s hash=7303585a77c34a74
- mpi p=44 tile=32 time=4.781s throughput=0.449 GFLOP/s hash=7303585a77c34a74
- mpi_tiled p=44 tile=32 time=5.214s throughput=0.412 GFLOP/s hash=7303585a77c34a74
- mpi_2d p=32 tile=32 time=5.397s throughput=0.398 GFLOP/s hash=7303585a77c34a74
- mpi_tiled p=32 tile=32 time=5.787s throughput=0.371 GFLOP/s hash=7303585a77c34a74

### 2048x2048x2048
- mpi_2d p=16 tile=32 time=10.297s throughput=1.668 GFLOP/s hash=eb3bcbdd1e4476ee
- mpi_tiled p=44 tile=32 time=11.564s throughput=1.486 GFLOP/s hash=eb3bcbdd1e4476ee
- mpi p=44 tile=32 time=12.438s throughput=1.381 GFLOP/s hash=eb3bcbdd1e4476ee
- mpi_tiled p=32 tile=32 time=14.926s throughput=1.151 GFLOP/s hash=eb3bcbdd1e4476ee
- mpi p=32 tile=32 time=15.004s throughput=1.145 GFLOP/s hash=eb3bcbdd1e4476ee

## Confirmation and Tuning
### 4096^3 top candidates
- mpi_2d p=16 tile=32 mapping=slots_4_6_6 time=34.113s throughput=4.029 GFLOP/s hash=99a5a38cbc1e12b7
- mpi_tiled p=44 tile=32 mapping=slots_12_16_16 time=36.426s throughput=3.773 GFLOP/s hash=99a5a38cbc1e12b7
- mpi p=44 tile=32 mapping=slots_12_16_16 time=122.705s throughput=1.120 GFLOP/s hash=99a5a38cbc1e12b7

### 4096^3 mpi_2d panel sweep
- mpi_2d p=16 tile=4096 mapping=slots_4_6_6_panel_4096 time=33.862s throughput=4.059 GFLOP/s hash=99a5a38cbc1e12b7
- mpi_2d p=16 tile=2048 mapping=slots_4_6_6_panel_2048 time=34.406s throughput=3.995 GFLOP/s hash=99a5a38cbc1e12b7
- mpi_2d p=16 tile=1024 mapping=slots_4_6_6_panel_1024 time=36.469s throughput=3.769 GFLOP/s hash=99a5a38cbc1e12b7

### 4096^3 mpi_2d grid sweep
- mpi_2d p=25 tile=4096 mapping=slots_7_9_9_grid_5x5 time=42.395s throughput=3.242 GFLOP/s hash=99a5a38cbc1e12b7
- mpi_2d p=36 tile=4096 mapping=slots_10_13_13_grid_6x6 time=51.704s throughput=2.658 GFLOP/s hash=99a5a38cbc1e12b7

## Large 10000^3 Result
- report_large_10000: mpi_2d p=16 tile=32 mapping=slots_4_6_6 time=214.210s throughput=9.337 GFLOP/s hash=c14405814301863a
- report_large_10000_opt: mpi_2d p=16 tile=10000 mapping=slots_4_6_6_panel_10000 time=194.534s throughput=10.281 GFLOP/s hash=c14405814301863a

## Decision
- Use `mpi_2d` with 16 MPI processes on `config/hosts_16` for the final 10000x10000x10000 report run.
- Use `--tile 10000` for the large run so `mpi_2d` uses one K panel; this improved 10000^3 from 214.210s to 194.534s.
- Do not use row-decomposition `mpi`/`mpi_tiled` for 10000^3 on 44 processes: each process receives full B, so worker nodes with 16 ranks would replicate about 12.8GB of B alone, before local A/C buffers.
- More MPI ranks were not automatically faster under WSL/OpenMPI. For 4096^3, 16 ranks (4x4 grid) beat 25 ranks (5x5) and 36 ranks (6x6).
