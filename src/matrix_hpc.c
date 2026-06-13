#include "matrix_hpc.h"

#include <stdint.h>
#include <string.h>

/* - cong thuc nhan ma tran tuan tu
   - toi uu tile
   - cach chia hang va goi MPI Scatterv/Bcast/Gatherv */

static int same(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

/* Dung luong cua 1 ma tran N x N kieu double, tinh theo GB. */
double hpc_matrix_gb(int n) {
    return (double)n * n * sizeof(double) / (1024.0 * 1024.0 * 1024.0);
}

/* So phep toan dau cham dong ly thuyet cua C = A x B la 2 * N^3. */
double hpc_work_flops(int n) {
    return 2.0 * (double)n * n * n;
}

/* seq va seq_tiled chi duoc chay voi 1 process. */
int hpc_is_seq(const char *variant) {
    return same(variant, "seq") || same(variant, "seq_tiled");
}

/* tiled nghia la dung block/tile trong buoc tinh local de cache tot hon. */
int hpc_is_tiled(const char *variant) {
    return same(variant, "seq_tiled") || same(variant, "mpi_tiled");
}

/* Chi giu 4 bien the de code gon va de giai thich. */
int hpc_valid_variant(const char *variant) {
    return hpc_is_seq(variant) || same(variant, "mpi") || same(variant, "mpi_tiled");
}

/* Bo sinh so gia ngau nhien nho, khong can thu vien ngoai.
   Cung seed + cung stream + cung index thi luon ra cung gia tri.
   Dieu nay giup checksum giua seq va MPI co the so sanh duoc. */
static uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

/* Sinh ma tran A/B tren rank 0.
   stream khac nhau de A va B khong trung nhau du cung seed. */
void hpc_fill_matrix(double *a, int n, unsigned long long seed, unsigned long long stream) {
    size_t total = (size_t)n * n;
    for (size_t i = 0; i < total; i++) {
        uint64_t r = splitmix64(seed ^ (stream * 0xD1B54A32D192ED03ULL) ^ i);
        a[i] = (double)(r >> 11) / 9007199254740992.0;
    }
}

/* Nhan ma tran co ban:
   C[i][j] = sum(A[i][k] * B[k][j]), voi k = 0..N-1.
   rows co the bang N trong ban seq, hoac so hang local trong ban MPI. */
void hpc_multiply_basic(const double *A, const double *B, double *C, int rows, int n) {
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int k = 0; k < n; k++) sum += A[(size_t)i * n + k] * B[(size_t)k * n + j];
            C[(size_t)i * n + j] = sum;
        }
}

/* Nhan ma tran co chia tile.
   Y tuong: xu ly tung block nho de tang kha nang tai su dung du lieu trong cache.
   Cong thuc toan hoc khong doi so voi multiply_basic. */
void hpc_multiply_tiled(const double *A, const double *B, double *C, int rows, int n, int tile) {
    memset(C, 0, (size_t)rows * n * sizeof(double));

    for (int ii = 0; ii < rows; ii += tile)
        for (int kk = 0; kk < n; kk += tile)
            for (int jj = 0; jj < n; jj += tile)
                for (int i = ii; i < rows && i < ii + tile; i++)
                    for (int k = kk; k < n && k < kk + tile; k++) {
                        double aik = A[(size_t)i * n + k];
                        for (int j = jj; j < n && j < jj + tile; j++)
                            C[(size_t)i * n + j] += aik * B[(size_t)k * n + j];
                    }
}

/* Chon ham nhan local theo variant.
   MPI hay seq khac nhau o cach chia du lieu, con cong viec local van la nhan ma tran. */
void hpc_multiply_variant(const Options *opt, const double *A, const double *B, double *C, int rows) {
    if (hpc_is_tiled(opt->variant)) hpc_multiply_tiled(A, B, C, rows, opt->n, opt->tile);
    else hpc_multiply_basic(A, B, C, rows, opt->n);
}

/* Tinh counts/displs cho MPI_Scatterv va MPI_Gatherv.
   Neu N khong chia het cho so process, cac rank dau nhan them 1 hang.
   counts/displs tinh theo so phan tu double, khong tinh theo so hang. */
void hpc_make_counts(int n, int processes, int *counts, int *displs) {
    int offset = 0;
    for (int r = 0; r < processes; r++) {
        int rows = n / processes + (r < n % processes);
        counts[r] = rows * n;
        displs[r] = offset;
        offset += counts[r];
    }
}

/* Chay 1 lan tuan tu tren rank 0 va tra ve thoi gian. */
void hpc_run_seq_once(const Options *opt, const double *A, const double *B, double *C, double *seconds) {
    double start = MPI_Wtime();
    hpc_multiply_variant(opt, A, B, C, opt->n);
    *seconds = MPI_Wtime() - start;
}

/* Chay 1 lan MPI theo row decomposition.
   Luong chinh:
   1. Scatterv: rank 0 chia cac hang cua A cho moi process.
   2. Bcast   : rank 0 gui toan bo B cho moi process.
   3. Local   : moi process tinh cac hang C cua minh.
   4. Gatherv : rank 0 gom cac hang C thanh ma tran ket qua day du. */
void hpc_run_mpi_once(const Options *opt, int rank, int processes,
                      const int *counts, const int *displs,
                      double *A, double *B, double *C,
                      double *localA, double *localC,
                      double *seconds) {
    int n = opt->n;
    int local_count = counts[rank];
    int local_rows = local_count / n;

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    /* MPI row decomposition:
       rank 0 scatters rows of A, broadcasts B, then gathers rows of C. */
    MPI_Scatterv(A, counts, displs, MPI_DOUBLE, localA, local_count, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(B, n * n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    hpc_multiply_variant(opt, localA, B, localC, local_rows);
    MPI_Gatherv(localC, local_count, MPI_DOUBLE, C, counts, displs, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    /* Chi rank 0 can thoi gian tong de in va ghi CSV. */
    if (rank == 0) *seconds = MPI_Wtime() - start;
    (void)processes;
}
