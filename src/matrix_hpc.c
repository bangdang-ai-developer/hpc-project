#include "matrix_hpc.h"

#include <math.h>
#include <stdint.h>
#include <string.h>

/* - cong thuc nhan ma tran tuan tu A(MxK) * B(KxN) = C(MxN)
   - toi uu tile
   - cach chia hang va goi MPI Scatterv/Bcast/Gatherv
   - bien the mpi_2d theo style matrix-v2 */

static int same(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

/* Dung luong cua 1 ma tran rows x cols kieu double, tinh theo GB. */
double hpc_matrix_gb(int rows, int cols) {
    return (double)rows * cols * sizeof(double) / (1024.0 * 1024.0 * 1024.0);
}

/* So phep toan dau cham dong ly thuyet cua C = A x B la 2 * M * K * N. */
double hpc_work_flops(int m, int k, int n) {
    return 2.0 * (double)m * k * n;
}

/* seq va seq_tiled chi duoc chay voi 1 process. */
int hpc_is_seq(const char *variant) {
    return same(variant, "seq") || same(variant, "seq_tiled");
}

/* tiled nghia la dung block/tile trong buoc tinh local de cache tot hon. */
int hpc_is_tiled(const char *variant) {
    return same(variant, "seq_tiled") || same(variant, "mpi_tiled");
}

/* mpi_2d la bien the lay y tuong tu matrix-v2: chia luoi 2 chieu. */
int hpc_is_2d(const char *variant) {
    return same(variant, "mpi_2d");
}

/* Chi giu cac bien the de code gon va de giai thich. */
int hpc_valid_variant(const char *variant) {
    return hpc_is_seq(variant) || same(variant, "mpi") || same(variant, "mpi_tiled") || hpc_is_2d(variant);
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
   Gia tri la so nguyen double trong [0, 1000], lap lai duoc theo seed. */
void hpc_fill_matrix(double *a, int rows, int cols, unsigned long long seed, unsigned long long stream) {
    size_t total = (size_t)rows * cols;
    for (size_t i = 0; i < total; i++) {
        uint64_t r = splitmix64(seed ^ (stream * 0xD1B54A32D192ED03ULL) ^ i);
        a[i] = (double)(r % 1001ULL);
    }
}

/* Nhan ma tran co ban:
   C[i][j] = sum(A[i][kk] * B[kk][j]), voi kk = 0..K-1.
   rows co the bang M trong ban seq, hoac so hang local trong ban MPI. */
void hpc_multiply_basic(const double *A, const double *B, double *C, int rows, int k, int n) {
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < n; j++) {
            double sum = 0.0;
            for (int kk = 0; kk < k; kk++) sum += A[(size_t)i * k + kk] * B[(size_t)kk * n + j];
            C[(size_t)i * n + j] = sum;
        }
}

/* Nhan ma tran co chia tile.
   Y tuong: xu ly tung block nho de tang kha nang tai su dung du lieu trong cache.
   Cong thuc toan hoc khong doi so voi multiply_basic. */
void hpc_multiply_tiled(const double *A, const double *B, double *C, int rows, int k, int n, int tile) {
    memset(C, 0, (size_t)rows * n * sizeof(double));

    for (int ii = 0; ii < rows; ii += tile)
        for (int kk0 = 0; kk0 < k; kk0 += tile)
            for (int jj = 0; jj < n; jj += tile)
                for (int i = ii; i < rows && i < ii + tile; i++)
                    for (int kk = kk0; kk < k && kk < kk0 + tile; kk++) {
                        double aik = A[(size_t)i * k + kk];
                        for (int j = jj; j < n && j < jj + tile; j++)
                            C[(size_t)i * n + j] += aik * B[(size_t)kk * n + j];
                    }
}

/* Chon ham nhan local theo variant.
   MPI hay seq khac nhau o cach chia du lieu, con cong viec local van la nhan ma tran. */
void hpc_multiply_variant(const Options *opt, const double *A, const double *B, double *C, int rows) {
    if (hpc_is_tiled(opt->variant)) hpc_multiply_tiled(A, B, C, rows, opt->k, opt->n, opt->tile);
    else hpc_multiply_basic(A, B, C, rows, opt->k, opt->n);
}

/* Tinh counts/displs cho MPI_Scatterv va MPI_Gatherv.
   Neu rows khong chia het cho so process, cac rank dau nhan them 1 hang.
   counts/displs tinh theo so phan tu double voi width la so cot cua buffer. */
void hpc_make_counts(int rows, int width, int processes, int *counts, int *displs) {
    int offset = 0;
    for (int r = 0; r < processes; r++) {
        int local_rows = rows / processes + (r < rows % processes);
        counts[r] = local_rows * width;
        displs[r] = offset;
        offset += counts[r];
    }
}

static int min_int(int a, int b) {
    return a < b ? a : b;
}

/* Thiet ke luoi 2D cho mpi_2d.
   Luoi duoc chon theo ti le M/N: M lon thi chia nhieu hang hon, N lon thi chia
   nhieu cot hon. Cach nay giam viec lap lai block A/B tren root khi ma tran chu nhat. */
void hpc_make_2d_layout(int m, int n, int processes, Hpc2DLayout *layout) {
    double target_rows = sqrt((double)processes * (double)m / (double)n);
    int best_rows = 1;
    double best_score = fabs(target_rows - 1.0);

    for (int r = 1; r <= processes; r++) {
        if (processes % r) continue;
        double score = fabs(target_rows - (double)r);
        if (score < best_score) {
            best_score = score;
            best_rows = r;
        }
    }

    layout->row_parts = best_rows;
    layout->col_parts = processes / best_rows;
    layout->r_per_c = (m + layout->row_parts - 1) / layout->row_parts;
    layout->c_per_c = (n + layout->col_parts - 1) / layout->col_parts;
}

/* Do day panel K cua mpi_2d.
   --tile van giu y nghia tham so benchmark, nhung panel qua mong se tao qua nhieu
   vong pack/send. Panel 1024 van nho hon RAM WSL nhung cat overhead MPI manh cho bai lon. */
int hpc_2d_panel_k(const Options *opt) {
    int panel = opt->tile;
    if (panel < 1024) panel = 1024;
    if (panel > opt->k) panel = opt->k;
    return panel;
}

/* MASTER DONG GOI MOT PANEL K CHO 1 RANK.
   A duoc chia theo block hang cua C.
   B duoc chia theo block cot cua C, luu theo hang kk de cap nhat C theo cot lien tuc.
   Dong goi theo panel giup chay duoc ma tran rat lon ma khong can buffer scatter khong lo. */
static void hpc_pack_2d_panel(const double *A, const double *B,
                              double *A_dst, double *B_dst,
                              const Options *opt, int target_rank,
                              const Hpc2DLayout *layout, int kk0, int k_len) {
    int r_per_c = layout->r_per_c;
    int c_per_c = layout->c_per_c;
    int m = opt->m, k = opt->k, n = opt->n;

    int grid_row = target_rank / layout->col_parts;
    int grid_col = target_rank % layout->col_parts;

    for (int i = 0; i < r_per_c; i++) {
        int global_i = grid_row * r_per_c + i;
        if (global_i < m)
            memcpy(A_dst + (size_t)i * k_len, A + (size_t)global_i * k + kk0, (size_t)k_len * sizeof(double));
        else
            memset(A_dst + (size_t)i * k_len, 0, (size_t)k_len * sizeof(double));
    }

    for (int kk = 0; kk < k_len; kk++) {
        const double *B_src = B + (size_t)(kk0 + kk) * n + grid_col * c_per_c;
        double *B_row = B_dst + (size_t)kk * c_per_c;
        int valid_cols = n - grid_col * c_per_c;
        if (valid_cols >= c_per_c) {
            memcpy(B_row, B_src, (size_t)c_per_c * sizeof(double));
        } else {
            int copy_cols = valid_cols > 0 ? valid_cols : 0;
            if (copy_cols > 0) memcpy(B_row, B_src, (size_t)copy_cols * sizeof(double));
            if (copy_cols < c_per_c) memset(B_row + copy_cols, 0, (size_t)(c_per_c - copy_cols) * sizeof(double));
        }
    }
}

/* NHAN MANH CUC BO
   Kernel dang outer-product theo tung hang C:
   - moi A[i,kk] chi doc mot lan cho ca day cot
   - B_local[kk,*] va C_local[i,*] deu lien tuc, de compiler vectorize theo j. */
static void hpc_multiply_2d_panel(const double *A_local, const double *B_local,
                                  double *C_local, int k_len, const Hpc2DLayout *layout) {
    int r_per_c = layout->r_per_c;
    int c_per_c = layout->c_per_c;

    for (int i = 0; i < r_per_c; i++) {
        const double *a = A_local + (size_t)i * k_len;
        double *c = C_local + (size_t)i * c_per_c;

        for (int kk = 0; kk < k_len; kk++) {
            const double av = a[kk];
            const double *b = B_local + (size_t)kk * c_per_c;
            for (int j = 0; j < c_per_c; j++)
                c[j] += av * b[j];
        }
    }
}

static void hpc_distribute_2d_panel(const double *A, const double *B,
                                    double *A_tmp, double *B_tmp,
                                    double *A_local, double *B_local,
                                    const Options *opt, int rank, int processes,
                                    const Hpc2DLayout *layout, int kk0, int k_len) {
    const int tag_a = 2001;
    const int tag_b = 2002;
    int a_count = layout->r_per_c * k_len;
    int b_count = layout->c_per_c * k_len;

    if (rank == 0) {
        for (int r = 0; r < processes; r++) {
            double *A_dst = (r == 0) ? A_local : A_tmp;
            double *B_dst = (r == 0) ? B_local : B_tmp;

            hpc_pack_2d_panel(A, B, A_dst, B_dst, opt, r, layout, kk0, k_len);
            if (r != 0) {
                MPI_Send(A_dst, a_count, MPI_DOUBLE, r, tag_a, MPI_COMM_WORLD);
                MPI_Send(B_dst, b_count, MPI_DOUBLE, r, tag_b, MPI_COMM_WORLD);
            }
        }
    } else {
        MPI_Recv(A_local, a_count, MPI_DOUBLE, 0, tag_a, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(B_local, b_count, MPI_DOUBLE, 0, tag_b, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }
}

/* GOM CAC BLOCK C VE DUNG VI TRI GLOBAL.
   C_gather van o dang packed theo rank, con C la ma tran M x N day du. */
static void hpc_unpack_2d_result(const double *C_gather, double *C,
                                 const Options *opt, int processes, const Hpc2DLayout *layout) {
    int r_per_c = layout->r_per_c;
    int c_per_c = layout->c_per_c;
    int m = opt->m, n = opt->n;
    size_t c_count = (size_t)r_per_c * c_per_c;

    for (int r = 0; r < processes; r++) {
        int grid_row = r / layout->col_parts;
        int grid_col = r % layout->col_parts;
        const double *src = C_gather + (size_t)r * c_count;

        for (int i = 0; i < r_per_c; i++) {
            int global_i = grid_row * r_per_c + i;
            if (global_i >= m) continue;
            for (int j = 0; j < c_per_c; j++) {
                int global_j = grid_col * c_per_c + j;
                if (global_j < n) C[(size_t)global_i * n + global_j] = src[(size_t)i * c_per_c + j];
            }
        }
    }
}

/* Chay 1 lan tuan tu tren rank 0 va tra ve thoi gian. */
void hpc_run_seq_once(const Options *opt, const double *A, const double *B, double *C, double *seconds) {
    double start = MPI_Wtime();
    hpc_multiply_variant(opt, A, B, C, opt->m);
    *seconds = MPI_Wtime() - start;
}

/* Chay 1 lan MPI theo row decomposition.
   Luong chinh:
   1. Scatterv: rank 0 chia cac hang cua A cho moi process.
   2. Bcast   : rank 0 gui toan bo B cho moi process.
   3. Local   : moi process tinh cac hang C cua minh.
   4. Gatherv : rank 0 gom cac hang C thanh ma tran ket qua day du. */
void hpc_run_mpi_once(const Options *opt, int rank,
                      const int *countsA, const int *displsA,
                      const int *countsC, const int *displsC,
                      double *A, double *B, double *C,
                      double *localA, double *localC,
                      double *seconds) {
    int local_rows = countsA[rank] / opt->k;

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    MPI_Scatterv(A, countsA, displsA, MPI_DOUBLE, localA, countsA[rank], MPI_DOUBLE, 0, MPI_COMM_WORLD);
    MPI_Bcast(B, opt->k * opt->n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
    hpc_multiply_variant(opt, localA, B, localC, local_rows);
    MPI_Gatherv(localC, countsC[rank], MPI_DOUBLE, C, countsC, displsC, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    /* Chi rank 0 can thoi gian tong de in va ghi CSV. */
    if (rank == 0) *seconds = MPI_Wtime() - start;
}

/* Chay 1 lan MPI 2D theo style matrix-v2:
   1. Rank 0 pack A theo block hang, B theo block cot da transpose.
   2. Gui theo tung panel K de giam RAM cho case 10000^3.
   3. Moi rank tinh va cong don mot block C_local.
   4. Gather C_local ve rank 0 va unpack thanh C day du khi can evidence. */
void hpc_run_mpi_2d_once(const Options *opt, int rank, int processes,
                         const Hpc2DLayout *layout,
                         double *A, double *B, double *C,
                         double *A_scatter, double *B_scatter, double *C_gather,
                         double *A_local, double *B_local, double *C_local,
                         int gather_result,
                         double *seconds) {
    int c_count = layout->r_per_c * layout->c_per_c;

    MPI_Barrier(MPI_COMM_WORLD);
    double start = MPI_Wtime();

    memset(C_local, 0, (size_t)c_count * sizeof(double));
    int panel_k = hpc_2d_panel_k(opt);
    for (int kk0 = 0; kk0 < opt->k; kk0 += panel_k) {
        int k_len = min_int(panel_k, opt->k - kk0);
        hpc_distribute_2d_panel(A, B,
                                A_scatter, B_scatter,
                                A_local, B_local,
                                opt, rank, processes, layout, kk0, k_len);
        hpc_multiply_2d_panel(A_local, B_local, C_local, k_len, layout);
    }

    if (gather_result) MPI_Gather(C_local, c_count, MPI_DOUBLE, C_gather, c_count, MPI_DOUBLE, 0, MPI_COMM_WORLD);

    if (rank == 0 && gather_result) {
        hpc_unpack_2d_result(C_gather, C, opt, processes, layout);
        *seconds = MPI_Wtime() - start;
    } else if (rank == 0) {
        *seconds = MPI_Wtime() - start;
    }
}
