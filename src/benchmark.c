#include "matrix_hpc.h"

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define FNV_OFFSET 1469598103934665603ULL
#define FNV_PRIME 1099511628211ULL

/* File nay phu trach phan "chay chuong trinh":
   - doc tham so CLI hoac hoi nguoi dung khi demo
   - validate M/K/N, process, RAM
   - cap phat A/B/C tren rank 0
   - goi cac ham thuat toan trong matrix_hpc.c
   - do thoi gian, tinh checksum, ghi CSV/sample evidence */

/* Checksum dung de chung minh ket qua giua cac bien the la giong nhau.
   sum/abs_sum/max_abs de doc nhanh, hash de so sanh gon trong CSV. */
typedef struct {
    double sum, abs_sum, max_abs;
    uint64_t hash;
} Checksum;

static int eq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

/* Dat cau hinh mac dinh cho demo va benchmark.
   Neu nguoi dung khong truyen option, demo se uu tien mpi_tiled. */
static void defaults(Options *opt) {
    memset(opt, 0, sizeof(*opt));
    strcpy(opt->variant, "mpi_tiled");
    strcpy(opt->out_dir, "outputs");
    strcpy(opt->run_label, "manual");
    strcpy(opt->mapping, "default");
    opt->repeat = 5;
    opt->tile = 32;
    opt->seed = 2026;
    opt->max_ram_gb = 5.0;
    opt->pe = 1;
}

/* In huong dan CLI ngan gon khi nguoi dung chay --help. */
static void usage(int rank) {
    if (rank) return;
    printf(
        "Usage: mpirun -np P ./build/matrix_hpc [options]\n"
        "Variants: seq, seq_tiled, mpi, mpi_tiled, mpi_2d\n"
        "Matrix shape: A(MxK) * B(KxN) = C(MxN)\n"
        "Options: --variant NAME --m M --k K --n N --seed S --repeat R --tile T --max-ram-gb G\n"
        "         --output-dir DIR --checksum-file FILE --run-label LABEL\n"
        "         --mapping LABEL --pe N --save-sample --save-full-demo\n"
        "         --measure-speedup\n");
}

/* Parse tham so dong lenh.
   Tra ve:
   - 1: parse OK, tiep tuc chay
   - 0: da in help, thoat thanh cong
   - -1: option sai, thoat loi */
static int parse_args(int argc, char **argv, Options *opt, int rank) {
    defaults(opt);
    if (argc == 1) return 1;

    for (int i = 1; i < argc; i++) {
        if (eq(argv[i], "--variant") && i + 1 < argc) snprintf(opt->variant, sizeof(opt->variant), "%s", argv[++i]);
        else if (eq(argv[i], "--m") && i + 1 < argc) opt->m = atoi(argv[++i]);
        else if (eq(argv[i], "--k") && i + 1 < argc) opt->k = atoi(argv[++i]);
        else if (eq(argv[i], "--n") && i + 1 < argc) opt->n = atoi(argv[++i]);
        else if (eq(argv[i], "--seed") && i + 1 < argc) opt->seed = strtoull(argv[++i], NULL, 10);
        else if (eq(argv[i], "--repeat") && i + 1 < argc) opt->repeat = atoi(argv[++i]);
        else if (eq(argv[i], "--tile") && i + 1 < argc) opt->tile = atoi(argv[++i]);
        else if (eq(argv[i], "--max-ram-gb") && i + 1 < argc) opt->max_ram_gb = atof(argv[++i]);
        else if (eq(argv[i], "--output-dir") && i + 1 < argc) snprintf(opt->out_dir, sizeof(opt->out_dir), "%s", argv[++i]);
        else if (eq(argv[i], "--checksum-file") && i + 1 < argc) snprintf(opt->checksum_file, sizeof(opt->checksum_file), "%s", argv[++i]);
        else if (eq(argv[i], "--run-label") && i + 1 < argc) snprintf(opt->run_label, sizeof(opt->run_label), "%s", argv[++i]);
        else if (eq(argv[i], "--mapping") && i + 1 < argc) snprintf(opt->mapping, sizeof(opt->mapping), "%s", argv[++i]);
        else if (eq(argv[i], "--pe") && i + 1 < argc) opt->pe = atoi(argv[++i]);
        else if (eq(argv[i], "--save-sample")) opt->save_sample = 1;
        else if (eq(argv[i], "--save-full-demo")) opt->save_full = 1;
        else if (eq(argv[i], "--measure-speedup")) opt->measure_speedup = 1;
        else if (eq(argv[i], "--help") || eq(argv[i], "-h")) { usage(rank); return 0; }
        else { if (!rank) fprintf(stderr, "Unknown option: %s\n", argv[i]); return -1; }
    }
    return 1;
}

/* Uoc luong RAM de tu choi cau hinh qua lon truoc khi malloc.
   Ma tran day du tren rank 0: A(MxK), B(KxN), C(MxN).
   MPI co them local buffers; mpi_2d co them packed scatter/gather buffers. */
static double estimate_gb(const Options *opt, int processes) {
    int mpi_2d_gathers_result = hpc_is_2d(opt->variant) && (opt->save_sample || opt->save_full);
    double bytes = ((double)opt->m * opt->k + (double)opt->k * opt->n) * sizeof(double);

    if (!hpc_is_2d(opt->variant) || mpi_2d_gathers_result)
        bytes += (double)opt->m * opt->n * sizeof(double);

    if (hpc_is_2d(opt->variant)) {
        Hpc2DLayout layout;
        hpc_make_2d_layout(opt->m, opt->n, processes, &layout);
        int panel_k = hpc_2d_panel_k(opt);
        double panel_a = (double)layout.r_per_c * panel_k;
        double panel_b = (double)panel_k * layout.c_per_c;
        bytes += (panel_a + panel_b) * sizeof(double); /* root temp pack buffers */
        bytes += (panel_a + panel_b +
                  (double)layout.r_per_c * layout.c_per_c) * sizeof(double); /* local buffers */
        if (mpi_2d_gathers_result)
            bytes += (double)processes * layout.r_per_c * layout.c_per_c * sizeof(double);
    } else if (!hpc_is_seq(opt->variant)) {
        int max_rows = (opt->m + processes - 1) / processes;
        bytes += ((double)opt->k * opt->n +
                  (double)max_rows * opt->k +
                  (double)max_rows * opt->n) * sizeof(double);
    }

    if (!hpc_is_seq(opt->variant) && opt->measure_speedup) bytes += (double)opt->m * opt->n * sizeof(double);
    return bytes / (1024.0 * 1024.0 * 1024.0);
}

/* Validate cau hinh truoc khi chay.
   Demo interactive se lap lai neu validate fail, batch mode se in loi va thoat. */
static int validate(const Options *opt, int processes, char *msg, size_t len) {
    if (!hpc_valid_variant(opt->variant)) return snprintf(msg, len, "unknown variant '%s'", opt->variant), 0;
    if (opt->m <= 0) return snprintf(msg, len, "M must be positive"), 0;
    if (opt->k <= 0) return snprintf(msg, len, "K must be positive"), 0;
    if (opt->n <= 0) return snprintf(msg, len, "N must be positive"), 0;
    if (opt->repeat <= 0) return snprintf(msg, len, "repeat must be positive"), 0;
    if (opt->tile <= 0) return snprintf(msg, len, "tile must be positive"), 0;
    if (hpc_is_seq(opt->variant) && processes != 1) return snprintf(msg, len, "sequential variant needs -np 1"), 0;
    if (estimate_gb(opt, processes) > opt->max_ram_gb) {
        return snprintf(msg, len, "estimated %.2f GB exceeds %.2f GB", estimate_gb(opt, processes), opt->max_ram_gb), 0;
    }
    msg[0] = 0;
    return 1;
}

/* Ham hoi input don gian cho demo interactive. */
static void ask(char *buf, size_t size, const char *prompt) {
    printf("%s\n> ", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)size, stdin)) buf[0] = 0;
    buf[strcspn(buf, "\r\n")] = 0;
}

/* Map lua chon 1/2/3/4/5 trong menu demo sang ten variant that. */
static void choose_variant(Options *opt, const char *choice) {
    if (!choice[0] || eq(choice, "4")) strcpy(opt->variant, "mpi_tiled");
    else if (eq(choice, "1")) strcpy(opt->variant, "seq");
    else if (eq(choice, "2")) strcpy(opt->variant, "seq_tiled");
    else if (eq(choice, "3")) strcpy(opt->variant, "mpi");
    else if (eq(choice, "5")) strcpy(opt->variant, "mpi_2d");
    else snprintf(opt->variant, sizeof(opt->variant), "%.63s", choice);
}

/* Demo interactive chi hoi tren rank 0.
   Sau khi rank 0 co Options hop le, broadcast ca struct cho moi process. */
static void interactive(Options *opt, int rank, int processes) {
    if (!rank) {
        char s[128], msg[256];
        defaults(opt);
        opt->repeat = 1;
        opt->save_sample = 1;
        opt->measure_speedup = 1;
        strcpy(opt->run_label, "interactive_demo");

        for (;;) {
            printf("\n=== Matrix Multiplication Demo ===\n");
            printf("Running with %d MPI process(es).\n", processes);
            printf("1. seq       (needs 1 process)\n");
            printf("2. seq_tiled (needs 1 process)\n");
            printf("3. mpi       (row split + broadcast B)\n");
            printf("4. mpi_tiled (recommended)\n");
            printf("5. mpi_2d    (2D grid like matrix-v2)\n");
            ask(s, sizeof(s), "Choose algorithm [4]:");
            choose_variant(opt, s);

            ask(s, sizeof(s), "Rows M of A/C [128]:");
            opt->m = s[0] ? atoi(s) : 128;
            ask(s, sizeof(s), "Shared dimension K [128]:");
            opt->k = s[0] ? atoi(s) : 128;
            ask(s, sizeof(s), "Columns N of B/C [128]:");
            opt->n = s[0] ? atoi(s) : 128;
            ask(s, sizeof(s), "Random seed [2026]:");
            opt->seed = s[0] ? strtoull(s, NULL, 10) : 2026;
            ask(s, sizeof(s), "Save full result matrix? y/N [N]:");
            opt->save_full = (s[0] == 'y' || s[0] == 'Y');

            if (validate(opt, processes, msg, sizeof(msg))) break;
            printf("\nInput is not valid: %s. Please choose again.\n", msg);
        }
        printf("\nStarting variant=%s M=%d K=%d N=%d seed=%llu processes=%d\n",
               opt->variant, opt->m, opt->k, opt->n, opt->seed, processes);
    }
    MPI_Bcast(opt, sizeof(*opt), MPI_BYTE, 0, MPI_COMM_WORLD);
}

/* malloc co kiem tra loi.
   Neu thieu RAM thi dung MPI_Abort de tat tat ca process, tranh treo chuong trinh. */
static void *xmalloc(size_t bytes) {
    void *p = malloc(bytes ? bytes : 1);
    if (!p) {
        fprintf(stderr, "Out of memory\n");
        MPI_Abort(MPI_COMM_WORLD, 10);
    }
    return p;
}

/* Chi rank 0 giu A, B, C day du.
   Worker rank chi nhan B va cac hang A local trong run_mpi. */
static void allocate_root_matrices(const Options *opt, int rank, double **A, double **B, double **C) {
    *A = *B = *C = NULL;
    if (rank) return;
    *A = xmalloc((size_t)opt->m * opt->k * sizeof(double));
    *B = xmalloc((size_t)opt->k * opt->n * sizeof(double));
    *C = xmalloc((size_t)opt->m * opt->n * sizeof(double));
    hpc_fill_matrix(*A, opt->m, opt->k, opt->seed, 1);
    hpc_fill_matrix(*B, opt->k, opt->n, opt->seed, 2);
}

/* Dem so node bang hostname ma MPI bao cao.
   Neu nhieu process nam tren cung hostname thi chi tinh la 1 node. */
static int count_nodes(int rank, int processes) {
    char host[MPI_MAX_PROCESSOR_NAME], *hosts = NULL;
    int len, nodes = 0;
    MPI_Get_processor_name(host, &len);
    if (!rank) hosts = calloc((size_t)processes, MPI_MAX_PROCESSOR_NAME);
    MPI_Gather(host, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, hosts, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);
    if (!rank) {
        for (int i = 0; i < processes; i++) {
            int seen = 0;
            for (int j = 0; j < i; j++)
                if (!strcmp(hosts + (size_t)i * MPI_MAX_PROCESSOR_NAME, hosts + (size_t)j * MPI_MAX_PROCESSOR_NAME)) seen = 1;
            if (!seen) nodes++;
        }
        free(hosts);
    }
    MPI_Bcast(&nodes, 1, MPI_INT, 0, MPI_COMM_WORLD);
    return nodes;
}

static uint64_t mix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

static uint64_t checksum_entry_hash(size_t index, double value) {
    union { double d; unsigned char b[sizeof(double)]; } u;
    uint64_t h = FNV_OFFSET ^ mix64((uint64_t)index);
    u.d = value;
    for (size_t j = 0; j < sizeof(double); j++) {
        h ^= u.b[j];
        h *= FNV_PRIME;
    }
    return h;
}

static void checksum_add(Checksum *c, size_t index, double value) {
    double av = fabs(value);
    c->sum += value;
    c->abs_sum += av;
    if (av > c->max_abs) c->max_abs = av;
    c->hash ^= checksum_entry_hash(index, value);
}

/* Tinh checksum cho ma tran C tren rank 0.
   Hash duoc tron theo tung global index roi XOR, nen co the so sanh voi checksum
   phan tan cua mpi_2d ma khong can gather toan bo C. */
static Checksum checksum(const double *a, int rows, int cols) {
    Checksum c = {0.0, 0.0, 0.0, 0};
    for (size_t i = 0, total = (size_t)rows * cols; i < total; i++) {
        checksum_add(&c, i, a[i]);
    }
    return c;
}

static Checksum checksum_2d_local(const double *C_local, const Options *opt,
                                  const Hpc2DLayout *layout, int rank) {
    Checksum c = {0.0, 0.0, 0.0, 0};
    int grid_row = rank / layout->col_parts;
    int grid_col = rank % layout->col_parts;

    for (int i = 0; i < layout->r_per_c; i++) {
        int global_i = grid_row * layout->r_per_c + i;
        if (global_i >= opt->m) continue;
        for (int j = 0; j < layout->c_per_c; j++) {
            int global_j = grid_col * layout->c_per_c + j;
            if (global_j >= opt->n) continue;
            size_t global_index = (size_t)global_i * opt->n + global_j;
            checksum_add(&c, global_index, C_local[(size_t)i * layout->c_per_c + j]);
        }
    }
    return c;
}

static Checksum reduce_checksum(const Checksum *local, int rank) {
    Checksum global = {0.0, 0.0, 0.0, 0};
    unsigned long long local_hash = (unsigned long long)local->hash;
    unsigned long long global_hash = 0;

    MPI_Reduce(&local->sum, &global.sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local->abs_sum, &global.abs_sum, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local->max_abs, &global.max_abs, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&local_hash, &global_hash, 1, MPI_UNSIGNED_LONG_LONG, MPI_BXOR, 0, MPI_COMM_WORLD);
    if (!rank) global.hash = (uint64_t)global_hash;
    return global;
}

/* Luu goc tren trai 8x8 cua C de lam evidence nho, khong can ghi ca ma tran lon. */
static void save_sample(const char *path, const double *C, int rows, int cols) {
    FILE *f = fopen(path, "w");
    if (!f) return;
    fprintf(f, "row,col,value\n");
    int row_lim = rows < 8 ? rows : 8;
    int col_lim = cols < 8 ? cols : 8;
    for (int i = 0; i < row_lim; i++)
        for (int j = 0; j < col_lim; j++) fprintf(f, "%d,%d,%.17g\n", i, j, C[(size_t)i * cols + j]);
    fclose(f);
}

/* Tuy chon luu day du C cho demo nho.
   Benchmark lon khong nen bat vi file co the rat lon. */
static void save_full(const char *path, const double *C, int rows, int cols) {
    FILE *f = fopen(path, "wb");
    if (!f) return;
    fwrite(C, sizeof(double), (size_t)rows * cols, f);
    fclose(f);
}

/* In cau hinh truoc khi chay de nguoi xem biet shape, process, node, RAM va FLOP. */
static void print_config(const Options *opt, int processes, int nodes) {
    printf("\n================ RUN CONFIG ================\n");
    printf("Variant                 : %s\n", opt->variant);
    printf("Matrix shape            : A(%dx%d) x B(%dx%d) = C(%dx%d)\n",
           opt->m, opt->k, opt->k, opt->n, opt->m, opt->n);
    printf("Repeat count            : %d\n", opt->repeat);
    printf("Seed                    : %llu\n", opt->seed);
    printf("Input value range       : integer double [0, 1000]\n");
    printf("MPI processes           : %d\n", processes);
    printf("Nodes detected          : %d\n", nodes);
    printf("Threads per process     : 1 (pure MPI)\n");
    printf("Process/core setting    : PE=%d, mapping=%s\n", opt->pe, opt->mapping);
    printf("Tile size               : %d\n", opt->tile);
    if (hpc_is_2d(opt->variant)) {
        Hpc2DLayout layout;
        hpc_make_2d_layout(opt->m, opt->n, processes, &layout);
        printf("MPI 2D grid             : %d x %d\n", layout.row_parts, layout.col_parts);
        printf("MPI 2D block            : %d rows x %d cols\n", layout.r_per_c, layout.c_per_c);
        printf("MPI 2D panel K          : %d\n", hpc_2d_panel_k(opt));
    }
    printf("Estimated memory        : %.4f GB / %.4f GB limit\n", estimate_gb(opt, processes), opt->max_ram_gb);
    printf("Work per repeat         : %.0f FLOP (%.3f GFLOP)\n",
           hpc_work_flops(opt->m, opt->k, opt->n), hpc_work_flops(opt->m, opt->k, opt->n) / 1e9);
    printf("============================================\n\n");
}

/* Ghi ket qua 1 run:
   - in terminal cho demo
   - in dong RESULT de script benchmark grep vao raw_runs.csv
   - ghi checksum_file neu duoc truyen tu script
   - luu sample/full result neu duoc bat */
static void emit_result(const Options *opt, int processes, int nodes, int run, double sec,
                        const double *C, const Checksum *precomputed, double base_sec) {
    mkdir(opt->out_dir, 0775);
    Checksum cs = precomputed ? *precomputed : checksum(C, opt->m, opt->n);
    double gflops = sec > 0.0 ? hpc_work_flops(opt->m, opt->k, opt->n) / sec / 1e9 : 0.0;
    double speedup = base_sec > 0.0 ? base_sec / sec : 0.0;
    double efficiency = speedup > 0.0 ? speedup / processes : 0.0;
    double serial = 0.0, parallel = 0.0, amdahl = 0.0;

    if (speedup > 0.0 && processes > 1) {
        serial = (1.0 / speedup - 1.0 / processes) / (1.0 - 1.0 / processes);
        if (serial < 0.0) serial = 0.0;
        if (serial > 1.0) serial = 1.0;
        parallel = 1.0 - serial;
        amdahl = 1.0 / (serial + parallel / processes);
    }

    char sample[HPC_PATH * 2] = "", full[HPC_PATH * 2] = "";
    if (opt->save_sample && C) {
        snprintf(sample, sizeof(sample), "%s/C_sample_%s_M%d_K%d_N%d_P%d_R%d.csv",
                 opt->out_dir, opt->variant, opt->m, opt->k, opt->n, processes, run);
        save_sample(sample, C, opt->m, opt->n);
    }
    if (opt->save_full && C && run == 0) {
        snprintf(full, sizeof(full), "%s/C_demo_%s_M%d_K%d_N%d_P%d_R%d.bin",
                 opt->out_dir, opt->variant, opt->m, opt->k, opt->n, processes, run);
        save_full(full, C, opt->m, opt->n);
    }

    if (opt->checksum_file[0]) {
        int exists = access(opt->checksum_file, F_OK) == 0;
        FILE *f = fopen(opt->checksum_file, "a");
        if (f) {
            if (!exists) fprintf(f, "variant,m,k,n,processes,nodes,tile,repeat_index,seconds,checksum_sum,checksum_abs,checksum_max_abs,checksum_hash,run_label,mapping,pe,threads_per_process,sample_file,full_file\n");
            fprintf(f, "%s,%d,%d,%d,%d,%d,%d,%d,%.9f,%.17g,%.17g,%.17g,%016llx,%s,%s,%d,1,%s,%s\n",
                    opt->variant, opt->m, opt->k, opt->n, processes, nodes, opt->tile, run, sec,
                    cs.sum, cs.abs_sum, cs.max_abs, (unsigned long long)cs.hash,
                    opt->run_label, opt->mapping, opt->pe, sample, full);
            fclose(f);
        }
    }

    printf("\n------------- PERFORMANCE METRICS -----------\n");
    printf("Run index               : %d/%d\n", run + 1, opt->repeat);
    printf("Execution time          : %.9f seconds\n", sec);
    printf("Throughput              : %.6f GFLOP/s\n", gflops);
    printf("Processes               : %d MPI process(es), %d node(s)\n", processes, nodes);
    if (base_sec > 0.0) {
        printf("Baseline time           : %.9f seconds\n", base_sec);
        printf("Traditional speedup     : %.6f\n", speedup);
        printf("Traditional efficiency  : %.6f\n", efficiency);
        if (processes > 1) {
            printf("Amdahl serial fraction  : %.6f\n", serial);
            printf("Amdahl parallel fraction: %.6f\n", parallel);
            printf("Amdahl speedup          : %.6f\n", amdahl);
        }
    } else {
        printf("Speedup/Efficiency      : computed later in summary.csv\n");
    }
    printf("Checksum hash           : %016llx\n", (unsigned long long)cs.hash);
    printf("Sample file             : %s\n", sample[0] ? sample : "not saved");
    printf("---------------------------------------------\n");
    printf("RESULT,%s,%d,%d,%d,%d,%d,%d,%d,%.9f,%.17g,%.17g,%.17g,%016llx,%s,%s,%d,1,%s,%s\n",
           opt->variant, opt->m, opt->k, opt->n, processes, nodes, opt->tile, run, sec,
           cs.sum, cs.abs_sum, cs.max_abs, (unsigned long long)cs.hash,
           opt->run_label, opt->mapping, opt->pe, sample, full);
    fflush(stdout);
}

/* Chay bien the tuan tu.
   Chi rank 0 co du lieu va thuc su tinh toan. */
static void run_seq(const Options *opt, int rank, int processes, int nodes) {
    double *A, *B, *C;
    allocate_root_matrices(opt, rank, &A, &B, &C);
    if (!rank) {
        for (int r = 0; r < opt->repeat; r++) {
            double sec;
            hpc_run_seq_once(opt, A, B, C, &sec);
            emit_result(opt, processes, nodes, r, sec, C, NULL, sec);
        }
    }
    free(A);
    free(B);
    free(C);
}

/* Chay bien the MPI.
   benchmark.c lo cap phat buffer va lap repeat.
   matrix_hpc.c lo mot lan Scatterv/Bcast/local multiply/Gatherv. */
static void run_mpi(const Options *opt, int rank, int processes, int nodes) {
    int *countsA = xmalloc((size_t)processes * sizeof(int));
    int *displsA = xmalloc((size_t)processes * sizeof(int));
    int *countsC = xmalloc((size_t)processes * sizeof(int));
    int *displsC = xmalloc((size_t)processes * sizeof(int));
    hpc_make_counts(opt->m, opt->k, processes, countsA, displsA);
    hpc_make_counts(opt->m, opt->n, processes, countsC, displsC);

    double *A, *B, *C;
    allocate_root_matrices(opt, rank, &A, &B, &C);
    if (rank) B = xmalloc((size_t)opt->k * opt->n * sizeof(double));

    double *localA = xmalloc((size_t)countsA[rank] * sizeof(double));
    double *localC = xmalloc((size_t)countsC[rank] * sizeof(double));
    double *baselineC = (!rank && opt->measure_speedup) ? xmalloc((size_t)opt->m * opt->n * sizeof(double)) : NULL;

    for (int r = 0; r < opt->repeat; r++) {
        double base_sec = 0.0, sec = 0.0;
        if (!rank && opt->measure_speedup) {
            double t = MPI_Wtime();
            hpc_multiply_tiled(A, B, baselineC, opt->m, opt->k, opt->n, opt->tile);
            base_sec = MPI_Wtime() - t;
            printf("Baseline seq_tiled finished in %.9f seconds.\n", base_sec);
        }

        hpc_run_mpi_once(opt, rank, countsA, displsA, countsC, displsC, A, B, C, localA, localC, &sec);
        if (!rank) emit_result(opt, processes, nodes, r, sec, C, NULL, base_sec);
    }

    free(countsA);
    free(displsA);
    free(countsC);
    free(displsC);
    free(A);
    free(B);
    free(C);
    free(localA);
    free(localC);
    free(baselineC);
}

/* Chay bien the MPI 2D.
   Style giong matrix-v2: root pack A/B thanh buffer rieng, moi rank giu mot manh nho. */
static void run_mpi_2d(const Options *opt, int rank, int processes, int nodes) {
    Hpc2DLayout layout;
    hpc_make_2d_layout(opt->m, opt->n, processes, &layout);
    int gather_result = opt->save_sample || opt->save_full;

    int panel_k = hpc_2d_panel_k(opt);
    size_t a_count = (size_t)layout.r_per_c * panel_k;
    size_t b_count = (size_t)panel_k * layout.c_per_c;
    size_t c_count = (size_t)layout.r_per_c * layout.c_per_c;

    double *A = NULL, *B = NULL, *C = NULL;
    if (!rank) {
        A = xmalloc((size_t)opt->m * opt->k * sizeof(double));
        B = xmalloc((size_t)opt->k * opt->n * sizeof(double));
        if (gather_result) C = xmalloc((size_t)opt->m * opt->n * sizeof(double));
        hpc_fill_matrix(A, opt->m, opt->k, opt->seed, 1);
        hpc_fill_matrix(B, opt->k, opt->n, opt->seed, 2);
    }

    double *A_scatter = !rank ? xmalloc(a_count * sizeof(double)) : NULL;
    double *B_scatter = !rank ? xmalloc(b_count * sizeof(double)) : NULL;
    double *C_gather = (!rank && gather_result) ? xmalloc((size_t)processes * c_count * sizeof(double)) : NULL;
    double *A_local = xmalloc(a_count * sizeof(double));
    double *B_local = xmalloc(b_count * sizeof(double));
    double *C_local = xmalloc(c_count * sizeof(double));
    double *baselineC = (!rank && opt->measure_speedup) ? xmalloc((size_t)opt->m * opt->n * sizeof(double)) : NULL;

    if (!rank) printf("[HPC 2D] %dx%d\n", layout.row_parts, layout.col_parts);

    for (int r = 0; r < opt->repeat; r++) {
        double base_sec = 0.0, sec = 0.0;
        if (!rank && opt->measure_speedup) {
            double t = MPI_Wtime();
            hpc_multiply_tiled(A, B, baselineC, opt->m, opt->k, opt->n, opt->tile);
            base_sec = MPI_Wtime() - t;
            printf("Baseline seq_tiled finished in %.9f seconds.\n", base_sec);
        }

        hpc_run_mpi_2d_once(opt, rank, processes, &layout,
                            A, B, C,
                            A_scatter, B_scatter, C_gather,
                            A_local, B_local, C_local,
                            gather_result,
                            &sec);

        Checksum local_cs = checksum_2d_local(C_local, opt, &layout, rank);
        Checksum global_cs = reduce_checksum(&local_cs, rank);
        if (!rank) emit_result(opt, processes, nodes, r, sec, C, &global_cs, base_sec);
    }

    free(A);
    free(B);
    free(C);
    free(A_scatter);
    free(B_scatter);
    free(C_gather);
    free(A_local);
    free(B_local);
    free(C_local);
    free(baselineC);
}

/* Luong chinh:
   1. MPI_Init
   2. doc input CLI hoac interactive
   3. validate
   4. dem node va in config
   5. chon run_seq, run_mpi hoac run_mpi_2d
   6. MPI_Finalize */
int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    int rank, processes;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &processes);

    Options opt;
    int parsed = parse_args(argc, argv, &opt, rank);
    if (parsed <= 0) {
        MPI_Finalize();
        return parsed == 0 ? 0 : 2;
    }

    if (argc == 1) interactive(&opt, rank, processes);

    char msg[256];
    if (!validate(&opt, processes, msg, sizeof(msg))) {
        if (!rank) fprintf(stderr, "Configuration error: %s\n", msg);
        MPI_Finalize();
        return 2;
    }

    int nodes = count_nodes(rank, processes);
    if (!rank) print_config(&opt, processes, nodes);

    if (hpc_is_seq(opt.variant)) run_seq(&opt, rank, processes, nodes);
    else if (hpc_is_2d(opt.variant)) run_mpi_2d(&opt, rank, processes, nodes);
    else run_mpi(&opt, rank, processes, nodes);

    MPI_Finalize();
    return 0;
}
