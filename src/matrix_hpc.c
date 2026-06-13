#include <math.h>
#include <mpi.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define PATH_LEN 512
#define NAME_LEN 64
#define FNV_OFFSET 1469598103934665603ULL
#define FNV_PRIME 1099511628211ULL

typedef struct {
    char variant[NAME_LEN];
    int n, repeat, tile, save_sample, save_full, gen_only, pe, measure_speedup;
    unsigned long long seed;
    double max_ram_gb;
    char out_dir[PATH_LEN], input_prefix[PATH_LEN], write_prefix[PATH_LEN];
    char checksum_file[PATH_LEN], run_label[NAME_LEN], mapping[NAME_LEN];
} Options;

typedef struct {
    double sum, abs_sum, max_abs;
    uint64_t hash;
} Checksum;

static int eq(const char *a, const char *b) { return strcmp(a, b) == 0; }
static int is_seq(const char *v) { return eq(v, "seq") || eq(v, "seq_naive") || eq(v, "seq_tiled"); }
static int is_tiled(const char *v) { return eq(v, "seq_tiled") || eq(v, "mpi_tiled") || eq(v, "mpi_row_tiled"); }
static int is_mpi_row(const char *v) { return eq(v, "mpi") || eq(v, "mpi_row") || eq(v, "mpi_row_bcast") || eq(v, "mpi_tiled") || eq(v, "mpi_row_tiled"); }
static int valid_variant(const char *v) { return is_seq(v) || is_mpi_row(v); }

static void defaults(Options *o) {
    memset(o, 0, sizeof(*o));
    strcpy(o->variant, "mpi_tiled");
    strcpy(o->out_dir, "outputs");
    strcpy(o->run_label, "manual");
    strcpy(o->mapping, "default");
    o->seed = 2026;
    o->repeat = 5;
    o->tile = 32;
    o->max_ram_gb = 5.0;
    o->pe = 1;
}

static void usage(int rank) {
    if (rank) return;
    fprintf(stderr,
        "Usage: mpirun -np P ./build/matrix_hpc [options]\n"
        "Variants: seq, seq_tiled, mpi, mpi_tiled\n"
        "Aliases: seq_naive, mpi_row_bcast, mpi_row_tiled\n\n"
        "Options:\n"
        "  --variant NAME              Algorithm variant\n"
        "  --n N                       Square matrix size\n"
        "  --seed S                    Deterministic seed (default 2026)\n"
        "  --repeat R                  Repeat count (default 5)\n"
        "  --tile T                    Tile size for tiled variants (default 32)\n"
        "  --max-ram-gb G              Refuse oversized runs (default 5.0)\n"
        "  --output-dir DIR            Evidence output directory\n"
        "  --save-sample               Save top-left 8x8 sample CSV\n"
        "  --save-full-demo            Save full C binary for run 0\n"
        "  --measure-speedup           Run seq_tiled baseline on rank 0 and print speedup/Amdahl metrics\n"
        "  --generate-only             Write A/B dataset and exit\n"
        "  --input-prefix PREFIX       Read PREFIX_A.bin and PREFIX_B.bin\n"
        "  --write-input-prefix PREFIX Write generated A/B dataset\n"
        "  --checksum-file FILE        Append checksum CSV\n"
        "  --run-label LABEL           Result label\n"
        "  --mapping LABEL             OpenMPI mapping label\n"
        "  --pe N                      Cores/process metadata\n");
}

static int parse(int argc, char **argv, Options *o, int rank) {
    defaults(o);
    if (argc == 1) return 1;
    for (int i = 1; i < argc; i++) {
        if (eq(argv[i], "--variant") && i + 1 < argc) snprintf(o->variant, sizeof(o->variant), "%s", argv[++i]);
        else if (eq(argv[i], "--n") && i + 1 < argc) o->n = atoi(argv[++i]);
        else if (eq(argv[i], "--seed") && i + 1 < argc) o->seed = strtoull(argv[++i], NULL, 10);
        else if (eq(argv[i], "--repeat") && i + 1 < argc) o->repeat = atoi(argv[++i]);
        else if (eq(argv[i], "--tile") && i + 1 < argc) o->tile = atoi(argv[++i]);
        else if (eq(argv[i], "--max-ram-gb") && i + 1 < argc) o->max_ram_gb = atof(argv[++i]);
        else if (eq(argv[i], "--output-dir") && i + 1 < argc) snprintf(o->out_dir, sizeof(o->out_dir), "%s", argv[++i]);
        else if (eq(argv[i], "--save-sample")) o->save_sample = 1;
        else if (eq(argv[i], "--save-full-demo")) o->save_full = 1;
        else if (eq(argv[i], "--measure-speedup")) o->measure_speedup = 1;
        else if (eq(argv[i], "--generate-only")) o->gen_only = 1;
        else if (eq(argv[i], "--input-prefix") && i + 1 < argc) snprintf(o->input_prefix, sizeof(o->input_prefix), "%s", argv[++i]);
        else if (eq(argv[i], "--write-input-prefix") && i + 1 < argc) snprintf(o->write_prefix, sizeof(o->write_prefix), "%s", argv[++i]);
        else if (eq(argv[i], "--checksum-file") && i + 1 < argc) snprintf(o->checksum_file, sizeof(o->checksum_file), "%s", argv[++i]);
        else if (eq(argv[i], "--run-label") && i + 1 < argc) snprintf(o->run_label, sizeof(o->run_label), "%s", argv[++i]);
        else if (eq(argv[i], "--mapping") && i + 1 < argc) snprintf(o->mapping, sizeof(o->mapping), "%s", argv[++i]);
        else if (eq(argv[i], "--pe") && i + 1 < argc) o->pe = atoi(argv[++i]);
        else if (eq(argv[i], "--help") || eq(argv[i], "-h")) { usage(rank); return 0; }
        else { if (!rank) fprintf(stderr, "Unknown/incomplete option: %s\n", argv[i]); return -1; }
    }
    return 1;
}

static double estimated_gb(const Options *o, int size) {
    double matrix_gb = (double)o->n * o->n * sizeof(double) / (1024.0 * 1024.0 * 1024.0);
    double gb = is_seq(o->variant) ? 3.0 * matrix_gb : 3.0 * matrix_gb + 2.0 * matrix_gb / (size ? size : 1);
    if (o->measure_speedup && !is_seq(o->variant)) gb += matrix_gb;
    return gb;
}

static double one_matrix_gb(int n) {
    return (double)n * n * sizeof(double) / (1024.0 * 1024.0 * 1024.0);
}

static double theoretical_flops(int n) {
    return 2.0 * (double)n * (double)n * (double)n;
}

static const char *variant_note(const char *v) {
    if (eq(v, "seq") || eq(v, "seq_naive")) return "Sequential baseline O(N^3)";
    if (eq(v, "seq_tiled")) return "Sequential tiled baseline, cache optimized";
    if (eq(v, "mpi") || eq(v, "mpi_row") || eq(v, "mpi_row_bcast")) return "Pure MPI row split: Scatterv + Bcast + Gatherv";
    return "Pure MPI row split + tiled local multiply (recommended)";
}

static void print_run_config(const Options *o, int size, int nodes) {
    int base_rows = size ? o->n / size : o->n;
    int extra_rows = size ? o->n % size : 0;
    double matrix_gb = one_matrix_gb(o->n);
    double flops = theoretical_flops(o->n);

    printf("\n================ RUN CONFIG ================\n");
    printf("Variant                 : %s\n", o->variant);
    printf("Variant meaning         : %s\n", variant_note(o->variant));
    printf("Matrix size             : N=%d, A/B/C are %d x %d\n", o->n, o->n, o->n);
    printf("Data type               : double (8 bytes/value)\n");
    printf("Repeat count            : %d\n", o->repeat);
    printf("Speedup measurement     : %s\n", o->measure_speedup ? "enabled, baseline=seq_tiled on rank 0" : "disabled for inline run");
    printf("Random seed             : %llu\n", o->seed);
    printf("MPI processes           : %d\n", size);
    printf("Physical/logical nodes  : %d detected by MPI hostname\n", nodes);
    printf("Processes per node      : %.2f average\n", nodes ? (double)size / nodes : (double)size);
    printf("Threads per process     : 1 (pure MPI, no OpenMP)\n");
    printf("Cores/process metadata  : PE=%d, mapping=%s\n", o->pe, o->mapping);
    printf("Rows per process        : %d or %d rows (balanced split)\n", base_rows, base_rows + (extra_rows > 0));
    printf("Tile size               : %s%d\n", is_tiled(o->variant) ? "" : "not used, value=", o->tile);
    printf("One matrix memory       : %.4f GB\n", matrix_gb);
    printf("Estimated total memory  : %.4f GB (limit %.4f GB)\n", estimated_gb(o, size), o->max_ram_gb);
    printf("Work per repeat         : %.0f floating-point operations (about %.3f GFLOP)\n", flops, flops / 1e9);
    printf("Output directory        : %s\n", o->out_dir);
    printf("Sample evidence         : %s\n", o->save_sample ? "enabled (top-left 8x8 CSV)" : "disabled");
    printf("Full result evidence    : %s\n", o->save_full ? "enabled for run 0" : "disabled");
    printf("Checksum CSV            : %s\n", o->checksum_file[0] ? o->checksum_file : "not set for this run");
    printf("============================================\n\n");
    fflush(stdout);
}

static int validate(const Options *o, int size, char *msg, size_t len) {
    if (!valid_variant(o->variant)) return snprintf(msg, len, "Unknown variant '%s'.", o->variant), 0;
    if (o->n <= 0) return snprintf(msg, len, "Matrix size N must be positive."), 0;
    if (o->repeat <= 0) return snprintf(msg, len, "Repeat must be positive."), 0;
    if (o->tile <= 0) return snprintf(msg, len, "Tile size must be positive."), 0;
    if (o->gen_only && !o->write_prefix[0]) return snprintf(msg, len, "--generate-only requires --write-input-prefix."), 0;
    if (!o->gen_only && is_seq(o->variant) && size != 1) return snprintf(msg, len, "Sequential variant needs -np 1."), 0;
    if (estimated_gb(o, size) > o->max_ram_gb) return snprintf(msg, len, "Estimated %.2f GB exceeds --max-ram-gb %.2f.", estimated_gb(o, size), o->max_ram_gb), 0;
    msg[0] = 0;
    return 1;
}

static void ask(char *buf, size_t n, const char *prompt) {
    printf("%s\n> ", prompt);
    fflush(stdout);
    if (!fgets(buf, (int)n, stdin)) buf[0] = 0;
    buf[strcspn(buf, "\r\n")] = 0;
}

static void set_variant_from_choice(Options *o, const char *s) {
    if (!s[0] || eq(s, "4")) strcpy(o->variant, "mpi_tiled");
    else if (eq(s, "1")) strcpy(o->variant, "seq");
    else if (eq(s, "2")) strcpy(o->variant, "seq_tiled");
    else if (eq(s, "3")) strcpy(o->variant, "mpi");
    else snprintf(o->variant, sizeof(o->variant), "%.63s", s);
}

static void interactive(Options *o, int rank, int size) {
    if (!rank) {
        char s[128], msg[256];
        defaults(o);
        o->repeat = 1;
        o->save_sample = 1;
        o->measure_speedup = 1;
        strcpy(o->run_label, "interactive_demo");
        for (;;) {
            printf("\n=== Matrix Multiplication Demo ===\n");
            printf("Running with %d MPI process(es).\n", size);
            printf("Step 1/4 - Choose algorithm:\n");
            printf("  1. seq       - sequential, easiest baseline (requires 1 process)\n");
            printf("  2. seq_tiled - sequential + cache tiling (requires 1 process)\n");
            printf("  3. mpi       - MPI split rows + broadcast B\n");
            printf("  4. mpi_tiled - MPI split rows + cache tiling (recommended)\n");
            ask(s, sizeof(s), "Enter 1, 2, 3, or 4. Press Enter for 4:");
            set_variant_from_choice(o, s);

            printf("\nStep 2/4 - Matrix size.\n");
            printf("N means A, B, C are N x N matrices.\n");
            printf("Good demo values: 128 or 256. Benchmark values: 2048 or 4096.\n");
            ask(s, sizeof(s), "Enter one number for N. Press Enter for 128:");
            o->n = s[0] ? atoi(s) : 128;

            printf("\nStep 3/4 - Random seed.\n");
            ask(s, sizeof(s), "Press Enter to use seed 2026:");
            o->seed = s[0] ? strtoull(s, NULL, 10) : 2026;

            printf("\nStep 4/4 - Evidence file.\n");
            ask(s, sizeof(s), "Save full result matrix? Type y or n. Press Enter for n:");
            o->save_full = (s[0] == 'y' || s[0] == 'Y');

            if (validate(o, size, msg, sizeof(msg))) break;
            printf("\nInput is not valid: %s\nPlease choose again.\n", msg);
        }
        printf("\nStarting: variant=%s, N=%d, seed=%llu, processes=%d\n",
               o->variant, o->n, o->seed, size);
        printf("Computing... please wait. The RESULT line will appear when it finishes.\n\n");
        fflush(stdout);
    }
    MPI_Bcast(o, sizeof(*o), MPI_BYTE, 0, MPI_COMM_WORLD);
}

static void *xmalloc(size_t bytes) {
    void *p = malloc(bytes);
    if (!p) { fprintf(stderr, "Out of memory (%zu bytes)\n", bytes); MPI_Abort(MPI_COMM_WORLD, 10); }
    return p;
}

static uint64_t splitmix64(uint64_t x) {
    x += 0x9E3779B97F4A7C15ULL;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ULL;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBULL;
    return x ^ (x >> 31);
}

static double rnd(unsigned long long seed, uint64_t stream, uint64_t i) {
    return (double)(splitmix64(seed ^ (stream * 0xD1B54A32D192ED03ULL) ^ i) >> 11) / 9007199254740992.0;
}

static void fill(double *a, int n, unsigned long long seed, uint64_t stream) {
    for (size_t i = 0, total = (size_t)n * n; i < total; i++) a[i] = rnd(seed, stream, i);
}

static int read_bin(const char *path, double *a, size_t total) {
    FILE *f = fopen(path, "rb"); if (!f) return perror(path), 0;
    size_t got = fread(a, sizeof(double), total, f); fclose(f);
    return got == total;
}

static int write_bin(const char *path, const double *a, size_t total) {
    FILE *f = fopen(path, "wb"); if (!f) return perror(path), 0;
    size_t wrote = fwrite(a, sizeof(double), total, f); fclose(f);
    return wrote == total;
}

static void load_or_make(const Options *o, int rank, double **A, double **B, double **C) {
    *A = *B = *C = NULL;
    if (rank) return;
    size_t total = (size_t)o->n * o->n;
    *A = xmalloc(total * sizeof(double));
    *B = xmalloc(total * sizeof(double));
    *C = xmalloc(total * sizeof(double));
    if (o->input_prefix[0]) {
        char pa[PATH_LEN + 16], pb[PATH_LEN + 16];
        snprintf(pa, sizeof(pa), "%s_A.bin", o->input_prefix);
        snprintf(pb, sizeof(pb), "%s_B.bin", o->input_prefix);
        if (!read_bin(pa, *A, total) || !read_bin(pb, *B, total)) MPI_Abort(MPI_COMM_WORLD, 11);
    } else {
        fill(*A, o->n, o->seed, 1);
        fill(*B, o->n, o->seed, 2);
        if (o->write_prefix[0]) {
            char pa[PATH_LEN + 16], pb[PATH_LEN + 16];
            snprintf(pa, sizeof(pa), "%s_A.bin", o->write_prefix);
            snprintf(pb, sizeof(pb), "%s_B.bin", o->write_prefix);
            if (!write_bin(pa, *A, total) || !write_bin(pb, *B, total)) MPI_Abort(MPI_COMM_WORLD, 12);
        }
    }
}

static void tiled_mul(const double *A, const double *B, double *C, int rows, int n, int tile) {
    memset(C, 0, (size_t)rows * n * sizeof(double));
    int t = tile > 0 ? tile : n;
    for (int ii = 0; ii < rows; ii += t)
        for (int kk = 0; kk < n; kk += t)
            for (int jj = 0; jj < n; jj += t)
                for (int i = ii; i < rows && i < ii + t; i++)
                    for (int k = kk; k < n && k < kk + t; k++) {
                        double aik = A[(size_t)i * n + k];
                        for (int j = jj; j < n && j < jj + t; j++) C[(size_t)i * n + j] += aik * B[(size_t)k * n + j];
                    }
}

static void basic_mul(const double *A, const double *B, double *C, int rows, int n) {
    for (int i = 0; i < rows; i++)
        for (int j = 0; j < n; j++) {
            double s = 0;
            for (int k = 0; k < n; k++) s += A[(size_t)i * n + k] * B[(size_t)k * n + j];
            C[(size_t)i * n + j] = s;
        }
}

static void counts(int n, int p, int *cnt, int *disp) {
    int d = 0;
    for (int r = 0; r < p; r++) {
        int rows = n / p + (r < n % p);
        cnt[r] = rows * n; disp[r] = d; d += cnt[r];
    }
}

static Checksum checksum(const double *a, int n) {
    Checksum c = {0, 0, 0, FNV_OFFSET};
    for (size_t i = 0, total = (size_t)n * n; i < total; i++) {
        double v = a[i], av = fabs(v);
        c.sum += v; c.abs_sum += av; if (av > c.max_abs) c.max_abs = av;
        union { double d; unsigned char b[sizeof(double)]; } u; u.d = v;
        for (size_t j = 0; j < sizeof(double); j++) { c.hash ^= u.b[j]; c.hash *= FNV_PRIME; }
    }
    return c;
}

static int node_count(int rank, int size) {
    char name[MPI_MAX_PROCESSOR_NAME], *all = NULL; int len, nodes = 0;
    MPI_Get_processor_name(name, &len);
    if (!rank) all = calloc((size_t)size, MPI_MAX_PROCESSOR_NAME);
    MPI_Gather(name, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, all, MPI_MAX_PROCESSOR_NAME, MPI_CHAR, 0, MPI_COMM_WORLD);
    if (!rank) {
        for (int i = 0; i < size; i++) {
            int seen = 0;
            for (int j = 0; j < i; j++) if (!strcmp(all + (size_t)i * MPI_MAX_PROCESSOR_NAME, all + (size_t)j * MPI_MAX_PROCESSOR_NAME)) seen = 1;
            if (!seen) nodes++;
        }
        free(all);
    }
    MPI_Bcast(&nodes, 1, MPI_INT, 0, MPI_COMM_WORLD);
    return nodes;
}

static void save_sample(const char *path, const double *C, int n) {
    FILE *f = fopen(path, "w"); if (!f) return;
    fprintf(f, "row,col,value\n");
    for (int i = 0, lim = n < 8 ? n : 8; i < lim; i++)
        for (int j = 0; j < lim; j++) fprintf(f, "%d,%d,%.17g\n", i, j, C[(size_t)i * n + j]);
    fclose(f);
}

static void emit(const Options *o, int size, int nodes, int run, double sec, const double *C, double baseline_sec) {
    mkdir(o->out_dir, 0775);
    Checksum cs = checksum(C, o->n);
    double flops = theoretical_flops(o->n);
    double gflops = sec > 0.0 ? flops / sec / 1e9 : 0.0;
    double speedup = (baseline_sec > 0.0 && sec > 0.0) ? baseline_sec / sec : 0.0;
    double efficiency = (speedup > 0.0 && size > 0) ? speedup / size : 0.0;
    double amdahl_serial = 0.0, amdahl_parallel = 0.0, amdahl_speedup = 0.0, amdahl_max = 0.0;
    int amdahl_ok = 0, amdahl_superlinear = 0;
    if (speedup > 0.0 && size > 1) {
        amdahl_serial = (1.0 / speedup - 1.0 / size) / (1.0 - 1.0 / size);
        if (amdahl_serial < 0.0) {
            amdahl_superlinear = 1;
            amdahl_serial = 0.0;
        }
        if (amdahl_serial > 1.0) amdahl_serial = 1.0;
        amdahl_parallel = 1.0 - amdahl_serial;
        amdahl_speedup = 1.0 / (amdahl_serial + amdahl_parallel / size);
        amdahl_max = amdahl_serial > 0.0 ? 1.0 / amdahl_serial : 0.0;
        amdahl_ok = 1;
    }
    char sample[PATH_LEN + NAME_LEN + 64] = "", full[PATH_LEN + NAME_LEN + 64] = "";
    if (o->save_sample) { snprintf(sample, sizeof(sample), "%s/C_sample_%s_N%d_P%d_R%d.csv", o->out_dir, o->variant, o->n, size, run); save_sample(sample, C, o->n); }
    if (o->save_full && run == 0) { snprintf(full, sizeof(full), "%s/C_demo_%s_N%d_P%d_R%d.bin", o->out_dir, o->variant, o->n, size, run); write_bin(full, C, (size_t)o->n * o->n); }
    if (o->checksum_file[0]) {
        int exists = access(o->checksum_file, F_OK) == 0;
        FILE *f = fopen(o->checksum_file, "a");
        if (f) {
            if (!exists) fprintf(f, "variant,n,processes,nodes,tile,repeat_index,seconds,checksum_sum,checksum_abs,checksum_max_abs,checksum_hash,run_label,mapping,pe,threads_per_process,sample_file,full_file\n");
            fprintf(f, "%s,%d,%d,%d,%d,%d,%.9f,%.17g,%.17g,%.17g,%016llx,%s,%s,%d,1,%s,%s\n", o->variant, o->n, size, nodes, o->tile, run, sec, cs.sum, cs.abs_sum, cs.max_abs, (unsigned long long)cs.hash, o->run_label, o->mapping, o->pe, sample, full);
            fclose(f);
        }
    }
    printf("\n------------- PERFORMANCE METRICS -----------\n");
    printf("Run index               : %d/%d\n", run + 1, o->repeat);
    printf("Execution time          : %.9f seconds\n", sec);
    printf("Theoretical work        : %.0f FLOP (%.3f GFLOP)\n", flops, flops / 1e9);
    printf("Throughput              : %.6f GFLOP/s\n", gflops);
    printf("Processes               : %d MPI process(es), %d node(s)\n", size, nodes);
    printf("Process/core setting    : threads_per_process=1, PE=%d, mapping=%s\n", o->pe, o->mapping);
    if (baseline_sec > 0.0) {
        printf("Baseline time           : %.9f seconds (seq_tiled on rank 0)\n", baseline_sec);
        printf("Traditional speedup     : %.6f = baseline_time / parallel_time\n", speedup);
        printf("Traditional efficiency  : %.6f = speedup / processes\n", efficiency);
        if (amdahl_ok) {
            printf("Amdahl serial fraction  : %.6f%s\n", amdahl_serial, amdahl_superlinear ? " (clamped; measured speedup was superlinear/noisy)" : "");
            printf("Amdahl parallel fraction: %.6f\n", amdahl_parallel);
            printf("Amdahl speedup          : %.6f = 1 / (serial + parallel/processes)\n", amdahl_speedup);
            if (amdahl_max > 0.0) printf("Amdahl max speedup      : %.6f as processes approach infinity\n", amdahl_max);
            else printf("Amdahl max speedup      : unlimited by model because serial fraction is approximately 0\n");
        } else {
            printf("Amdahl speedup          : N/A for 1 process\n");
        }
    } else {
        printf("Baseline time           : not measured in this run\n");
        printf("Traditional speedup     : add --measure-speedup or use summary.csv benchmark baseline\n");
        printf("Amdahl speedup          : add --measure-speedup to estimate serial/parallel fraction\n");
    }
    printf("Checksum sum            : %.17g\n", cs.sum);
    printf("Checksum abs_sum        : %.17g\n", cs.abs_sum);
    printf("Checksum max_abs        : %.17g\n", cs.max_abs);
    printf("Checksum hash           : %016llx\n", (unsigned long long)cs.hash);
    printf("Sample file             : %s\n", sample[0] ? sample : "not saved");
    printf("Full result file        : %s\n", full[0] ? full : "not saved");
    printf("---------------------------------------------\n");
    printf("RESULT,%s,%d,%d,%d,%d,%d,%.9f,%.17g,%.17g,%.17g,%016llx,%s,%s,%d,1,%s,%s\n", o->variant, o->n, size, nodes, o->tile, run, sec, cs.sum, cs.abs_sum, cs.max_abs, (unsigned long long)cs.hash, o->run_label, o->mapping, o->pe, sample, full);
    fflush(stdout);
}

static void run_seq(const Options *o, int rank, int size, int nodes) {
    double *A, *B, *C; load_or_make(o, rank, &A, &B, &C);
    if (!rank) for (int r = 0; r < o->repeat; r++) {
        MPI_Barrier(MPI_COMM_WORLD);
        double t = MPI_Wtime();
        if (is_tiled(o->variant)) tiled_mul(A, B, C, o->n, o->n, o->tile); else basic_mul(A, B, C, o->n, o->n);
        double sec = MPI_Wtime() - t;
        emit(o, size, nodes, r, sec, C, sec);
    }
    free(A); free(B); free(C);
}

static void run_mpi_row(const Options *o, int rank, int size, int nodes) {
    int n = o->n, *cnt = xmalloc((size_t)size * sizeof(int)), *disp = xmalloc((size_t)size * sizeof(int));
    counts(n, size, cnt, disp);
    double *A, *B, *C; load_or_make(o, rank, &A, &B, &C);
    if (rank) B = xmalloc((size_t)n * n * sizeof(double));
    double *baselineC = NULL;
    if (!rank && o->measure_speedup) baselineC = xmalloc((size_t)n * n * sizeof(double));
    int local_count = cnt[rank], rows = local_count / n;
    double *localA = xmalloc((size_t)local_count * sizeof(double)), *localC = xmalloc((size_t)local_count * sizeof(double));
    for (int r = 0; r < o->repeat; r++) {
        double baseline_sec = 0.0;
        if (!rank && o->measure_speedup) {
            double tb = MPI_Wtime();
            tiled_mul(A, B, baselineC, n, n, o->tile);
            baseline_sec = MPI_Wtime() - tb;
            printf("Baseline seq_tiled finished for run %d/%d in %.9f seconds.\n", r + 1, o->repeat, baseline_sec);
            fflush(stdout);
        }
        MPI_Barrier(MPI_COMM_WORLD);
        double t = MPI_Wtime();
        MPI_Scatterv(A, cnt, disp, MPI_DOUBLE, localA, local_count, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        MPI_Bcast(B, n * n, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        if (is_tiled(o->variant)) tiled_mul(localA, B, localC, rows, n, o->tile); else basic_mul(localA, B, localC, rows, n);
        MPI_Gatherv(localC, local_count, MPI_DOUBLE, C, cnt, disp, MPI_DOUBLE, 0, MPI_COMM_WORLD);
        if (!rank) emit(o, size, nodes, r, MPI_Wtime() - t, C, baseline_sec);
    }
    free(cnt); free(disp); free(A); free(B); free(C); free(baselineC); free(localA); free(localC);
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);
    int rank, size; MPI_Comm_rank(MPI_COMM_WORLD, &rank); MPI_Comm_size(MPI_COMM_WORLD, &size);
    Options o; int ps = parse(argc, argv, &o, rank);
    if (ps <= 0) { MPI_Finalize(); return ps == 0 ? 0 : 2; }
    if (argc == 1) interactive(&o, rank, size);
    char msg[256]; if (!validate(&o, size, msg, sizeof(msg))) { if (!rank) fprintf(stderr, "Configuration error: %s\n", msg); MPI_Finalize(); return 2; }
    int nodes = node_count(rank, size);
    if (!rank) print_run_config(&o, size, nodes);
    if (o.gen_only) {
        double *A, *B, *C; load_or_make(&o, rank, &A, &B, &C);
        if (!rank) printf("DATASET,%s_A.bin,%s_B.bin,%d,%llu\n", o.write_prefix, o.write_prefix, o.n, o.seed);
        free(A); free(B); free(C); MPI_Finalize(); return 0;
    }
    if (is_seq(o.variant)) run_seq(&o, rank, size, nodes); else run_mpi_row(&o, rank, size, nodes);
    MPI_Finalize();
    return 0;
}
