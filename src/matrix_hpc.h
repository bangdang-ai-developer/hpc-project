#ifndef MATRIX_HPC_H
#define MATRIX_HPC_H

#include <mpi.h>

/* File header dung chung cho 2 file C++:
   - matrix_hpc.cpp: chi chua thuat toan va cac ham MPI cot loi.
   - benchmark.cpp : chua CLI, demo, do thoi gian va ghi evidence.
   Tach header giup code de doc hon va tranh lap lai khai bao. */

#define HPC_STR 64
#define HPC_PATH 256

/* Options gom toan bo cau hinh cua mot lan chay.
   Struct nay duoc dung ca o phan thuat toan va phan benchmark:
   - variant: seq, seq_tiled, mpi, mpi_tiled, mpi_2d.
   - m/k/n: kich thuoc A(MxK), B(KxN), C(MxN).
   - repeat/tile/seed: thong so benchmark.
   - save_sample/save_full/checksum_file: evidence.
   - mapping/pe: thong tin cach map process/core cua Open MPI. */
typedef struct {
    char variant[HPC_STR], out_dir[HPC_PATH], checksum_file[HPC_PATH];
    char run_label[HPC_STR], mapping[HPC_STR];
    int m, k, n, repeat, tile, save_sample, save_full, measure_speedup, pe;
    unsigned long long seed;
    double max_ram_gb;
} Options;

/* Layout cho bien the mpi_2d.
   row_parts chia hang cua C theo M, col_parts chia cot cua C theo N.
   r_per_c/c_per_c la kich thuoc block moi process nhan, co padding neu khong chia het. */
typedef struct {
    int row_parts, col_parts;
    int r_per_c, c_per_c;
} Hpc2DLayout;

/* Helper tinh dung luong va so phep tinh ly thuyet. */
double hpc_matrix_gb(int rows, int cols);
double hpc_work_flops(int m, int k, int n);

/* Helper phan loai bien the thuat toan. */
int hpc_is_seq(const char *variant);
int hpc_is_tiled(const char *variant);
int hpc_is_2d(const char *variant);
int hpc_valid_variant(const char *variant);

/* Sinh du lieu va nhan ma tran local.
   A la rows x K, B la K x N, C la rows x N, tat ca luu bang flat array 1D. */
void hpc_fill_matrix(double *a, int rows, int cols, unsigned long long seed, unsigned long long stream);
void hpc_multiply_basic(const double *A, const double *B, double *C, int rows, int k, int n);
void hpc_multiply_tiled(const double *A, const double *B, double *C, int rows, int k, int n, int tile);
void hpc_multiply_variant(const Options *opt, const double *A, const double *B, double *C, int rows);

/* Chia hang va chay mot lan tinh toan.
   benchmark.cpp lap lai cac ham nay nhieu lan de do repeat/min/avg/stddev. */
void hpc_make_counts(int rows, int width, int processes, int *counts, int *displs);
void hpc_make_2d_layout(int m, int n, int processes, Hpc2DLayout *layout);
int hpc_2d_panel_k(const Options *opt);
void hpc_run_seq_once(const Options *opt, const double *A, const double *B, double *C, double *seconds);
void hpc_run_mpi_once(const Options *opt, int rank,
                      const int *countsA, const int *displsA,
                      const int *countsC, const int *displsC,
                      double *A, double *B, double *C,
                      double *localA, double *localC,
                      double *seconds);
void hpc_run_mpi_2d_once(const Options *opt, int rank, int processes,
                         const Hpc2DLayout *layout,
                         double *A, double *B, double *C,
                         double *A_scatter, double *B_scatter, double *C_gather,
                         double *A_local, double *B_local, double *C_local,
                         int gather_result,
                         double *seconds);

#endif
