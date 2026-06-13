#ifndef MATRIX_HPC_H
#define MATRIX_HPC_H

#include <mpi.h>

/* File header dung chung cho 2 file C:
   - matrix_hpc.c: chi chua thuat toan va cac ham MPI cot loi.
   - benchmark.c : chua CLI, demo, do thoi gian va ghi evidence.
   Tach header giup code de doc hon va tranh lap lai khai bao. */

#define HPC_STR 64
#define HPC_PATH 256

/* Options gom toan bo cau hinh cua mot lan chay.
   Struct nay duoc dung ca o phan thuat toan va phan benchmark:
   - variant: seq, seq_tiled, mpi, mpi_tiled.
   - n/repeat/tile/seed: thong so bai toan va lap benchmark.
   - save_sample/save_full/checksum_file: evidence.
   - mapping/pe: thong tin cach map process/core cua OpenMPI. */
typedef struct {
    char variant[HPC_STR], out_dir[HPC_PATH], checksum_file[HPC_PATH];
    char run_label[HPC_STR], mapping[HPC_STR];
    int n, repeat, tile, save_sample, save_full, measure_speedup, pe;
    unsigned long long seed;
    double max_ram_gb;
} Options;

/* Helper tinh dung luong va so phep tinh ly thuyet. */
double hpc_matrix_gb(int n);
double hpc_work_flops(int n);

/* Helper phan loai bien the thuat toan. */
int hpc_is_seq(const char *variant);
int hpc_is_tiled(const char *variant);
int hpc_valid_variant(const char *variant);

/* Sinh du lieu va nhan ma tran local.
   Tat ca ma tran duoc luu bang flat array 1D: A[i * n + j]. */
void hpc_fill_matrix(double *a, int n, unsigned long long seed, unsigned long long stream);
void hpc_multiply_basic(const double *A, const double *B, double *C, int rows, int n);
void hpc_multiply_tiled(const double *A, const double *B, double *C, int rows, int n, int tile);
void hpc_multiply_variant(const Options *opt, const double *A, const double *B, double *C, int rows);

/* Chia hang va chay mot lan tinh toan.
   benchmark.c lap lai cac ham nay nhieu lan de do repeat/min/avg/stddev. */
void hpc_make_counts(int n, int processes, int *counts, int *displs);
void hpc_run_seq_once(const Options *opt, const double *A, const double *B, double *C, double *seconds);
void hpc_run_mpi_once(const Options *opt, int rank, int processes,
                      const int *counts, const int *displs,
                      double *A, double *B, double *C,
                      double *localA, double *localC,
                      double *seconds);

#endif
