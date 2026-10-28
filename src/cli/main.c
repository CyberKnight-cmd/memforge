/* memsim command-line interface. Owner: P9. */
#include "memsim/bench.h"
#include "memsim/common/errors.h"
#include "memsim/common/types.h"
#include "memsim/paging/belady.h"
#include "memsim/paging/engine.h"
#include "memsim/workloads/generators.h"
#include "memsim/workloads/loader.h"
#include "sha256.h"

#include <ctype.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MEMSIM_GIT
#define MEMSIM_GIT "unknown"
#endif

/* Traces name the workload by file stem, not path, so they do not depend on where it ran. */
static const char *workload_name(const char *path, char *buf, size_t cap) {
    const char *base = strrchr(path, '/');
    base = base != NULL ? base + 1 : path;
    size_t n = strlen(base);
    if (n > 3 && strcmp(base + n - 3, ".wl") == 0) {
        n -= 3;
    }
    if (n >= cap) {
        n = cap - 1;
    }
    memcpy(buf, base, n);
    buf[n] = '\0';
    return buf;
}

static void print_usage(FILE *out) {
    (void)fprintf(out, "Usage: memsim <command> [options]\n\n"
                       "Commands:\n"
                       "  validate <workload>\n"
                       "  run <workload> --policy <P> [--trace <file>] [--no-check] [--json]\n"
                       "  compare <workload> --policies <a,b,c> [--json]\n"
                       "  refs <\"7 0 1 ...\"> --frames <F> --policy <P> [--table] [--json]\n"
                       "  sweep <file> --policy <P> --frames <A..B> [--json]\n"
                       "  gen alloc-uniform|refs-locality --seed <S> [options] -o <file>\n"
                       "  help, --help\n");
}

static void format_ratio_str(int64_t ppm, char *buf, size_t size) {
    if (ppm < 0) {
        int64_t pos = -ppm;
        (void)snprintf(buf, size, "-%" PRId64 ".%06" PRId64, pos / 1000000, pos % 1000000);
    } else {
        (void)snprintf(buf, size, "%" PRId64 ".%06" PRId64, ppm / 1000000, ppm % 1000000);
    }
}

static int cmd_validate(int argc, char **argv) {
    if (argc != 3) {
        print_usage(stderr);
        return 2;
    }
    const char *path = argv[2];
    ms_workload w = {0};
    ms_wl_error err = {0};
    ms_status st = ms_wl_load(path, &w, &err);
    if (st != MS_OK) {
        (void)fprintf(stderr, "%s:%zu: %s\n", path, err.line, err.message);
        return 2;
    }
    ms_wl_free(&w);
    return 0;
}

static bool parse_run_flags(int argc, char **argv, const char **out_path, const char **out_policy,
                            const char **out_trace, bool *out_check, bool *out_json) {
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--policy") == 0 && i + 1 < argc) {
            *out_policy = argv[++i];
        } else if (strcmp(argv[i], "--trace") == 0 && i + 1 < argc) {
            *out_trace = argv[++i];
        } else if (strcmp(argv[i], "--no-check") == 0) {
            *out_check = false;
        } else if (strcmp(argv[i], "--json") == 0) {
            *out_json = true;
        } else if (argv[i][0] != '-' && *out_path == NULL) {
            *out_path = argv[i];
        } else {
            return false;
        }
    }
    return (*out_path != NULL && *out_policy != NULL) != 0;
}

static void print_run_json(const char *policy, const ms_bench_result *res) {
    printf("{\"policy\":\"%s\",\"metrics\":{", policy);
    for (size_t i = 0; i < res->n_metrics; i++) {
        if (i > 0) {
            printf(",");
        }
        if (res->metrics[i].is_ratio) {
            char buf[64];
            format_ratio_str(res->metrics[i].value, buf, sizeof(buf));
            printf("\"%s\":%s", res->metrics[i].key, buf);
        } else {
            printf("\"%s\":%" PRId64, res->metrics[i].key, res->metrics[i].value);
        }
    }
    printf("}}\n");
}

static void print_run_text(const ms_bench_result *res) {
    for (size_t i = 0; i < res->n_metrics; i++) {
        if (res->metrics[i].is_ratio) {
            char buf[64];
            format_ratio_str(res->metrics[i].value, buf, sizeof(buf));
            printf("%s %s\n", res->metrics[i].key, buf);
        } else {
            printf("%s %" PRId64 "\n", res->metrics[i].key, res->metrics[i].value);
        }
    }
}

static int cmd_run(int argc, char **argv) {
    const char *workload_path = NULL;
    const char *policy = NULL;
    const char *trace_path = NULL;
    bool check = true;
    bool json = false;

    if (!parse_run_flags(argc, argv, &workload_path, &policy, &trace_path, &check, &json)) {
        print_usage(stderr);
        return 2;
    }

    ms_workload w = {0};
    ms_wl_error err = {0};
    ms_status st = ms_wl_load(workload_path, &w, &err);
    if (st != MS_OK) {
        (void)fprintf(stderr, "%s:%zu: %s\n", workload_path, err.line, err.message);
        return 2;
    }

    char sha256_hex[65] = {0};
    if (!ms_sha256_file(workload_path, sha256_hex)) {
        (void)fprintf(stderr, "%s: failed to compute sha256\n", workload_path);
        ms_wl_free(&w);
        return 2;
    }

    FILE *trace_file = NULL;
    if (trace_path != NULL) {
        trace_file = fopen(trace_path, "w");
        if (trace_file == NULL) {
            perror(trace_path);
            ms_wl_free(&w);
            return 2;
        }
    }

    char name_buf[128];
    ms_bench_opts opts = {
        .trace = trace_file,
        .workload_name = workload_name(workload_path, name_buf, sizeof name_buf),
        .workload_sha256 = sha256_hex,
        .git = MEMSIM_GIT,
        .check = check,
    };

    ms_bench_result res = {0};
    st = ms_bench_run(&w, policy, &opts, &res);
    if (trace_file != NULL) {
        (void)fclose(trace_file);
    }
    if (st != MS_OK) {
        ms_wl_free(&w);
        return (st == MS_ERR_NOMEM) ? 3 : 2;
    }

    if (json) {
        print_run_json(policy, &res);
    } else {
        print_run_text(&res);
    }

    ms_bench_result_free(&res);
    ms_wl_free(&w);
    return 0;
}

static void print_compare_placements_json(const ms_workload *w, size_t n_policies,
                                          const ms_bench_result *results) {
    size_t n_placements = results[0].n_placements;
    printf("],\"placements\":[");
    for (size_t i = 0; i < n_placements; i++) {
        if (i > 0) {
            printf(",");
        }
        const char *id_str = "";
        ms_alloc_id id = results[0].placements[i].id;
        if (w->id_names != NULL && id < w->n_ids && w->id_names[id] != NULL) {
            id_str = w->id_names[id];
        }
        printf("{\"id\":\"%s\",\"size\":%" PRId64 ",\"results\":[", id_str,
               results[0].placements[i].size);
        for (size_t p = 0; p < n_policies; p++) {
            if (p > 0) {
                printf(",");
            }
            const ms_bench_placement *pl = &results[p].placements[i];
            if (pl->ok) {
                printf("%" PRId64, pl->start);
            } else if (pl->cause == MS_ALLOC_FAIL_FRAGMENTATION) {
                printf("\"fail FRAGMENTATION\"");
            } else if (pl->cause == MS_ALLOC_FAIL_CAPACITY) {
                printf("\"fail CAPACITY\"");
            } else {
                printf("\"rejected\"");
            }
        }
        printf("]}");
    }
}

static void print_compare_metrics_json(size_t n_policies, const ms_bench_result *results) {
    size_t n_metrics = results[0].n_metrics;
    printf("],\"metrics\":{");
    for (size_t m = 0; m < n_metrics; m++) {
        if (m > 0) {
            printf(",");
        }
        printf("\"%s\":[", results[0].metrics[m].key);
        for (size_t p = 0; p < n_policies; p++) {
            if (p > 0) {
                printf(",");
            }
            if (results[p].metrics[m].is_ratio) {
                char buf[64];
                format_ratio_str(results[p].metrics[m].value, buf, sizeof(buf));
                printf("%s", buf);
            } else {
                printf("%" PRId64, results[p].metrics[m].value);
            }
        }
        printf("]");
    }
    printf("}}\n");
}

static void print_compare_json(const ms_workload *w, char *const *policies, size_t n_policies,
                               const ms_bench_result *results) {
    printf("{\"policies\":[");
    for (size_t p = 0; p < n_policies; p++) {
        printf("%s\"%s\"", p > 0 ? "," : "", policies[p]);
    }
    print_compare_placements_json(w, n_policies, results);
    print_compare_metrics_json(n_policies, results);
}

static void format_placement_res(const ms_bench_placement *pl, char *buf, size_t size) {
    if (pl->ok) {
        (void)snprintf(buf, size, "%" PRId64, pl->start);
    } else if (pl->cause == MS_ALLOC_FAIL_FRAGMENTATION) {
        (void)snprintf(buf, size, "fail FRAGMENTATION");
    } else if (pl->cause == MS_ALLOC_FAIL_CAPACITY) {
        (void)snprintf(buf, size, "fail CAPACITY");
    } else {
        (void)snprintf(buf, size, "rejected");
    }
}

/* Fixture setup blocks owned by SYS are noise in the placement table; --json keeps them. */
static bool is_setup_alloc(const ms_workload *w, ms_alloc_id id) {
    for (size_t i = 0; i < w->n_ops; i++) {
        const ms_op *op = &w->ops[i];
        if (op->kind == MS_OP_ALLOC && op->u.alloc.id == id) {
            return op->u.alloc.pid < w->n_pids && strcmp(w->pid_names[op->u.alloc.pid], "SYS") == 0;
        }
    }
    return false;
}

static void compute_placement_col_widths(const ms_workload *w, char *const *policies,
                                         size_t n_policies, const ms_bench_result *results,
                                         size_t *col_widths) {
    size_t n_placements = results[0].n_placements;
    col_widths[0] = strlen("id");
    col_widths[1] = strlen("size");
    for (size_t p = 0; p < n_policies; p++) {
        col_widths[2 + p] = strlen(policies[p]);
    }

    for (size_t i = 0; i < n_placements; i++) {
        const char *id_str = "";
        ms_alloc_id id = results[0].placements[i].id;
        if (is_setup_alloc(w, id)) {
            continue;
        }
        if (w->id_names != NULL && id < w->n_ids && w->id_names[id] != NULL) {
            id_str = w->id_names[id];
        }
        if (strlen(id_str) > col_widths[0]) {
            col_widths[0] = strlen(id_str);
        }

        char sz_buf[32];
        (void)snprintf(sz_buf, sizeof(sz_buf), "%" PRId64, results[0].placements[i].size);
        if (strlen(sz_buf) > col_widths[1]) {
            col_widths[1] = strlen(sz_buf);
        }

        for (size_t p = 0; p < n_policies; p++) {
            char res_buf[64];
            format_placement_res(&results[p].placements[i], res_buf, sizeof(res_buf));
            if (strlen(res_buf) > col_widths[2 + p]) {
                col_widths[2 + p] = strlen(res_buf);
            }
        }
    }
}

static void print_compare_placements_table(const ms_workload *w, char *const *policies,
                                           size_t n_policies, const ms_bench_result *results) {
    size_t n_placements = results[0].n_placements;
    if (n_placements == 0) {
        return;
    }
    size_t col_widths[34];
    compute_placement_col_widths(w, policies, n_policies, results, col_widths);

    printf("%-*s  %-*s", (int)col_widths[0], "id", (int)col_widths[1], "size");
    for (size_t p = 0; p < n_policies; p++) {
        printf("  %-*s", (int)col_widths[2 + p], policies[p]);
    }
    printf("\n");

    for (size_t i = 0; i < n_placements; i++) {
        const char *id_str = "";
        ms_alloc_id id = results[0].placements[i].id;
        if (is_setup_alloc(w, id)) {
            continue;
        }
        if (w->id_names != NULL && id < w->n_ids && w->id_names[id] != NULL) {
            id_str = w->id_names[id];
        }
        char sz_buf[32];
        (void)snprintf(sz_buf, sizeof(sz_buf), "%" PRId64, results[0].placements[i].size);
        printf("%-*s  %-*s", (int)col_widths[0], id_str, (int)col_widths[1], sz_buf);

        for (size_t p = 0; p < n_policies; p++) {
            char res_buf[64];
            format_placement_res(&results[p].placements[i], res_buf, sizeof(res_buf));
            printf("  %-*s", (int)col_widths[2 + p], res_buf);
        }
        printf("\n");
    }
    printf("\n");
}

static void print_compare_metrics_table(char **policies, size_t n_policies,
                                        const ms_bench_result *results) {
    size_t n_metrics = results[0].n_metrics;
    if (n_metrics == 0) {
        return;
    }
    size_t mcol_widths[33];
    mcol_widths[0] = strlen("metric");
    for (size_t p = 0; p < n_policies; p++) {
        mcol_widths[1 + p] = strlen(policies[p]);
    }

    for (size_t m = 0; m < n_metrics; m++) {
        const char *k = results[0].metrics[m].key;
        if (strlen(k) > mcol_widths[0]) {
            mcol_widths[0] = strlen(k);
        }
        for (size_t p = 0; p < n_policies; p++) {
            char val_buf[64];
            if (results[p].metrics[m].is_ratio) {
                format_ratio_str(results[p].metrics[m].value, val_buf, sizeof(val_buf));
            } else {
                (void)snprintf(val_buf, sizeof(val_buf), "%" PRId64, results[p].metrics[m].value);
            }
            if (strlen(val_buf) > mcol_widths[1 + p]) {
                mcol_widths[1 + p] = strlen(val_buf);
            }
        }
    }

    printf("%-*s", (int)mcol_widths[0], "metric");
    for (size_t p = 0; p < n_policies; p++) {
        printf("  %-*s", (int)mcol_widths[1 + p], policies[p]);
    }
    printf("\n");

    for (size_t m = 0; m < n_metrics; m++) {
        printf("%-*s", (int)mcol_widths[0], results[0].metrics[m].key);
        for (size_t p = 0; p < n_policies; p++) {
            char val_buf[64];
            if (results[p].metrics[m].is_ratio) {
                format_ratio_str(results[p].metrics[m].value, val_buf, sizeof(val_buf));
            } else {
                (void)snprintf(val_buf, sizeof(val_buf), "%" PRId64, results[p].metrics[m].value);
            }
            printf("  %-*s", (int)mcol_widths[1 + p], val_buf);
        }
        printf("\n");
    }
}

static ms_status run_compare_benchmarks(const ms_workload *w, const char *workload_path,
                                        const char *sha256_hex, char *const *policies,
                                        size_t n_policies, ms_bench_result *results) {
    char name_buf[128];
    for (size_t p = 0; p < n_policies; p++) {
        ms_bench_opts opts = {
            .trace = NULL,
            .workload_name = workload_name(workload_path, name_buf, sizeof name_buf),
            .workload_sha256 = sha256_hex,
            .git = MEMSIM_GIT,
            .check = true,
        };
        ms_status st = ms_bench_run(w, policies[p], &opts, &results[p]);
        if (st != MS_OK) {
            for (size_t k = 0; k < p; k++) {
                ms_bench_result_free(&results[k]);
            }
            return st;
        }
    }
    return MS_OK;
}

static int cmd_compare(int argc, char **argv) {
    const char *workload_path = NULL;
    char *policies_arg = NULL;
    bool json = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--policies") == 0 && i + 1 < argc) {
            policies_arg = argv[++i];
        } else if (strcmp(argv[i], "--json") == 0) {
            json = true;
        } else if (argv[i][0] != '-' && workload_path == NULL) {
            workload_path = argv[i];
        } else {
            print_usage(stderr);
            return 2;
        }
    }

    if (workload_path == NULL || policies_arg == NULL) {
        print_usage(stderr);
        return 2;
    }

    char *policies[32];
    size_t n_policies = 0;
    char *tok = strtok(policies_arg, ",");
    while (tok != NULL && n_policies < 32) {
        policies[n_policies++] = tok;
        tok = strtok(NULL, ",");
    }
    if (n_policies == 0) {
        print_usage(stderr);
        return 2;
    }

    ms_workload w = {0};
    ms_wl_error err = {0};
    ms_status st = ms_wl_load(workload_path, &w, &err);
    if (st != MS_OK) {
        (void)fprintf(stderr, "%s:%zu: %s\n", workload_path, err.line, err.message);
        return 2;
    }

    char sha256_hex[65] = {0};
    if (!ms_sha256_file(workload_path, sha256_hex)) {
        (void)fprintf(stderr, "%s: failed to compute sha256\n", workload_path);
        ms_wl_free(&w);
        return 2;
    }

    ms_bench_result results[32];
    memset(results, 0, sizeof(results));

    st = run_compare_benchmarks(&w, workload_path, sha256_hex, policies, n_policies, results);
    if (st != MS_OK) {
        ms_wl_free(&w);
        return (st == MS_ERR_NOMEM) ? 3 : 2;
    }

    if (json) {
        print_compare_json(&w, policies, n_policies, results);
    } else {
        print_compare_placements_table(&w, policies, n_policies, results);
        print_compare_metrics_table(policies, n_policies, results);
    }

    for (size_t p = 0; p < n_policies; p++) {
        ms_bench_result_free(&results[p]);
    }
    ms_wl_free(&w);
    return 0;
}

static void compute_refs_col_widths(const uint32_t *refs, size_t n_refs, uint32_t frames,
                                    const uint32_t *grid, const bool *present, size_t *col_w) {
    for (size_t i = 0; i < n_refs; i++) {
        char b[32];
        (void)snprintf(b, sizeof(b), "%" PRIu32, refs[i]);
        col_w[i] = strlen(b);
        for (uint32_t f = 0; f < frames; f++) {
            if (present[(f * n_refs) + i]) {
                (void)snprintf(b, sizeof(b), "%" PRIu32, grid[(f * n_refs) + i]);
                if (strlen(b) > col_w[i]) {
                    col_w[i] = strlen(b);
                }
            }
        }
    }
}

static void print_refs_table(const uint32_t *refs, size_t n_refs, uint32_t frames,
                             const uint32_t *grid, const bool *present, const bool *faulted) {
    size_t *col_w = malloc(n_refs * sizeof(size_t));
    if (col_w == NULL) {
        return;
    }
    compute_refs_col_widths(refs, n_refs, frames, grid, present, col_w);

    for (size_t i = 0; i < n_refs; i++) {
        printf("%*u%s", (int)col_w[i], refs[i], (i + 1 < n_refs) ? " " : "\n");
    }

    for (uint32_t f = 0; f < frames; f++) {
        for (size_t i = 0; i < n_refs; i++) {
            if (present[(f * n_refs) + i]) {
                printf("%*u%s", (int)col_w[i], grid[(f * n_refs) + i],
                       (i + 1 < n_refs) ? " " : "\n");
            } else {
                printf("%*s%s", (int)col_w[i], "", (i + 1 < n_refs) ? " " : "\n");
            }
        }
    }

    for (size_t i = 0; i < n_refs; i++) {
        printf("%*s%s", (int)col_w[i], faulted[i] ? "F" : "", (i + 1 < n_refs) ? " " : "\n");
    }
    free(col_w);
}

static int parse_refs_string(const char *ref_str, uint32_t **out_refs, size_t *out_n,
                             uint32_t *out_max) {
    size_t cap = 64;
    size_t n_refs = 0;
    uint32_t *refs = malloc(cap * sizeof(*refs));
    if (refs == NULL) {
        return 3;
    }

    uint32_t max_page = 0;
    const char *p = ref_str;
    while (*p != '\0') {
        while (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r') {
            p++;
        }
        if (*p == '\0') {
            break;
        }
        char *end = NULL;
        unsigned long v = strtoul(p, &end, 10);
        if (p == end) {
            free(refs);
            return 2;
        }
        if (n_refs == cap) {
            cap *= 2;
            uint32_t *new_refs = realloc(refs, cap * sizeof(*new_refs));
            if (new_refs == NULL) {
                free(refs);
                return 3;
            }
            refs = new_refs;
        }
        uint32_t page = (uint32_t)v;
        refs[n_refs++] = page;
        if (page > max_page) {
            max_page = page;
        }
        p = end;
    }

    if (n_refs == 0) {
        free(refs);
        return 2;
    }

    *out_refs = refs;
    *out_n = n_refs;
    *out_max = max_page;
    return 0;
}

static bool parse_refs_flags(int argc, char **argv, const char **out_ref_str, uint32_t *out_frames,
                             const char **out_policy, bool *out_table, bool *out_json) {
    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            *out_frames = (uint32_t)strtoul(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--policy") == 0 && i + 1 < argc) {
            *out_policy = argv[++i];
        } else if (strcmp(argv[i], "--table") == 0) {
            *out_table = true;
        } else if (strcmp(argv[i], "--json") == 0) {
            *out_json = true;
        } else if (argv[i][0] != '-' && *out_ref_str == NULL) {
            *out_ref_str = argv[i];
        } else {
            return false;
        }
    }
    return (*out_ref_str != NULL && *out_frames != 0 && *out_policy != NULL) != 0;
}

static bool run_refs_sim(ms_page_engine *e, const ms_op *ops, size_t n_refs, uint32_t frames,
                         bool *faulted, uint32_t *grid, bool *present, ms_page_frame_info *finfo) {
    for (size_t i = 0; i < n_refs; i++) {
        ms_page_step step;
        if (ms_page_engine_apply(e, &ops[1 + i], &step) != MS_OK) {
            return false;
        }
        faulted[i] = step.fault;
        ms_page_engine_frames(e, finfo, frames);
        for (uint32_t f = 0; f < frames; f++) {
            present[(f * n_refs) + i] = (finfo[f].pid != MS_NONE);
            grid[(f * n_refs) + i] = finfo[f].vpn;
        }
    }
    return true;
}

static int cmd_refs(int argc, char **argv) {
    const char *ref_str = NULL;
    uint32_t frames = 0;
    const char *policy = NULL;
    bool table = false;
    bool json = false;

    if (!parse_refs_flags(argc, argv, &ref_str, &frames, &policy, &table, &json)) {
        print_usage(stderr);
        return 2;
    }

    uint32_t *refs = NULL;
    size_t n_refs = 0;
    uint32_t max_page = 0;
    int rc = parse_refs_string(ref_str, &refs, &n_refs, &max_page);
    if (rc != 0) {
        if (rc == 2) {
            print_usage(stderr);
        }
        return rc;
    }

    size_t n_ops = 1 + n_refs;
    ms_op *ops = malloc(n_ops * sizeof(*ops));
    if (ops == NULL) {
        free(refs);
        return 3;
    }

    ops[0] = (ms_op){
        .kind = MS_OP_SPAWN,
        .u.spawn = {.pid = 0, .pages = max_page + 1},
    };
    for (size_t i = 0; i < n_refs; i++) {
        ops[1 + i] = (ms_op){
            .kind = MS_OP_ACCESS,
            .u.access = {.pid = 0, .by_va = false, .vpn_or_va = refs[i], .write = false},
        };
    }

    ms_paging_config pcfg = ms_paging_config_default();
    pcfg.frames = frames;
    pcfg.page_size = 4096;
    pcfg.policy = policy;

    ms_page_engine *e = NULL;
    if (ms_page_engine_create(&pcfg, &e) != MS_OK) {
        free(ops);
        free(refs);
        return 2;
    }

    if (ms_page_engine_prepare(e, ops, n_ops) != MS_OK) {
        ms_page_engine_destroy(e);
        free(ops);
        free(refs);
        return 2;
    }

    ms_page_step step;
    if (ms_page_engine_apply(e, &ops[0], &step) != MS_OK) {
        ms_page_engine_destroy(e);
        free(ops);
        free(refs);
        return 2;
    }

    bool *faulted = malloc(n_refs * sizeof(bool));
    uint32_t *grid = malloc((size_t)frames * n_refs * sizeof(uint32_t));
    bool *present = malloc((size_t)frames * n_refs * sizeof(bool));
    ms_page_frame_info *finfo = malloc(frames * sizeof(*finfo));

    if (faulted == NULL || grid == NULL || present == NULL || finfo == NULL) {
        free(faulted);
        free(grid);
        free(present);
        free(finfo);
        ms_page_engine_destroy(e);
        free(ops);
        free(refs);
        return 3;
    }

    if (!run_refs_sim(e, ops, n_refs, frames, faulted, grid, present, finfo)) {
        free(faulted);
        free(grid);
        free(present);
        free(finfo);
        ms_page_engine_destroy(e);
        free(ops);
        free(refs);
        return 2;
    }

    uint64_t total_faults = ms_page_engine_stats(e).faults;

    if (json) {
        printf("{\"policy\":\"%s\",\"frames\":%" PRIu32 ",\"faults\":%" PRIu64 "}\n", policy,
               frames, total_faults);
    } else if (table) {
        print_refs_table(refs, n_refs, frames, grid, present, faulted);
    } else {
        printf("faults %" PRIu64 "\n", total_faults);
    }

    free(faulted);
    free(grid);
    free(present);
    free(finfo);
    ms_page_engine_destroy(e);
    free(ops);
    free(refs);
    return 0;
}

static bool load_refs_from_workload(const char *path, ms_page_key **out_refs, size_t *out_n) {
    ms_workload w = {0};
    ms_wl_error err = {0};
    if (ms_wl_load(path, &w, &err) != MS_OK) {
        return false;
    }
    if (w.kind != MS_WL_PAGING) {
        ms_wl_free(&w);
        return false;
    }
    size_t count = 0;
    for (size_t i = 0; i < w.n_ops; i++) {
        if (w.ops[i].kind == MS_OP_ACCESS) {
            count++;
        }
    }
    ms_page_key *refs = malloc(count > 0 ? (count * sizeof(*refs)) : sizeof(*refs));
    if (refs == NULL) {
        ms_wl_free(&w);
        return false;
    }
    size_t idx = 0;
    for (size_t i = 0; i < w.n_ops; i++) {
        if (w.ops[i].kind == MS_OP_ACCESS) {
            uint64_t vpn = w.ops[i].u.access.by_va ? (w.ops[i].u.access.vpn_or_va / w.page_size)
                                                   : w.ops[i].u.access.vpn_or_va;
            refs[idx++] = (ms_page_key){
                .pid = w.ops[i].u.access.pid,
                .vpn = vpn > UINT32_MAX ? UINT32_MAX : (uint32_t)vpn,
            };
        }
    }
    ms_wl_free(&w);
    *out_refs = refs;
    *out_n = count;
    return true;
}

static bool load_refs_from_file(const char *path, ms_page_key **out_refs, size_t *out_n) {
    FILE *f = fopen(path, "r");
    if (f == NULL) {
        return false;
    }
    size_t cap = 64;
    size_t count = 0;
    ms_page_key *refs = malloc(cap * sizeof(*refs));
    if (refs == NULL) {
        (void)fclose(f);
        return false;
    }
    char token[64];
    while (fscanf(f, "%63s", token) == 1) {
        char *end = NULL;
        unsigned long val = strtoul(token, &end, 10);
        if (end == token) {
            continue;
        }
        if (count == cap) {
            cap *= 2;
            ms_page_key *new_refs = realloc(refs, cap * sizeof(*new_refs));
            if (new_refs == NULL) {
                free(refs);
                (void)fclose(f);
                return false;
            }
            refs = new_refs;
        }
        refs[count++] =
            (ms_page_key){.pid = 0, .vpn = (val > UINT32_MAX) ? UINT32_MAX : (uint32_t)val};
    }
    (void)fclose(f);
    *out_refs = refs;
    *out_n = count;
    return true;
}

static bool load_page_refs(const char *path, ms_page_key **out_refs, size_t *out_n) {
    if (load_refs_from_workload(path, out_refs, out_n)) {
        return true;
    }
    return load_refs_from_file(path, out_refs, out_n);
}

static bool parse_frames_range(const char *frames_str, uint32_t *min_f, uint32_t *max_f) {
    char *dot = strstr(frames_str, "..");
    if (dot == NULL || dot == frames_str) {
        return false;
    }
    char *end1 = NULL;
    unsigned long v1 = strtoul(frames_str, &end1, 10);
    if (end1 != dot) {
        return false;
    }
    char *end2 = NULL;
    unsigned long v2 = strtoul(dot + 2, &end2, 10);
    if (end2 == dot + 2 || *end2 != '\0' || v1 == 0 || v1 > v2) {
        return false;
    }
    *min_f = (uint32_t)v1;
    *max_f = (uint32_t)v2;
    return true;
}

static void print_sweep_json(const char *policy, uint32_t min_f, uint32_t max_f,
                             const uint64_t *faults, const uint32_t *points, size_t n_points) {
    printf("{\"policy\":\"%s\",\"frames\":[", policy);
    for (uint32_t f = min_f; f <= max_f; f++) {
        printf("%s%" PRIu32, f > min_f ? "," : "", f);
    }
    printf("],\"faults\":[");
    for (uint32_t f = min_f; f <= max_f; f++) {
        printf("%s%" PRIu64, f > min_f ? "," : "", faults[f - 1]);
    }
    printf("],\"belady_points\":[");
    bool first = true;
    /* Report the frame count whose faults rose, as the text output marks it (library: the one
     * before). */
    for (size_t i = 0; i < n_points; i++) {
        uint32_t frames = points[i] + 1;
        if (frames >= min_f && frames <= max_f) {
            printf("%s%" PRIu32, first ? "" : ",", frames);
            first = false;
        }
    }
    printf("]}\n");
}

static void print_sweep_text(uint32_t min_f, uint32_t max_f, const uint64_t *faults) {
    for (uint32_t f = min_f; f <= max_f; f++) {
        bool belady = ((f > 1) && (faults[f - 1] > faults[f - 2])) != 0;
        if (belady) {
            printf("%" PRIu32 " %" PRIu64 " *\n", f, faults[f - 1]);
        } else {
            printf("%" PRIu32 " %" PRIu64 "\n", f, faults[f - 1]);
        }
    }
}

static int cmd_sweep(int argc, char **argv) {
    const char *file_path = NULL;
    const char *policy = NULL;
    const char *frames_str = NULL;
    bool json = false;

    for (int i = 2; i < argc; i++) {
        if (strcmp(argv[i], "--policy") == 0 && i + 1 < argc) {
            policy = argv[++i];
        } else if (strcmp(argv[i], "--frames") == 0 && i + 1 < argc) {
            frames_str = argv[++i];
        } else if (strcmp(argv[i], "--json") == 0) {
            json = true;
        } else if (argv[i][0] != '-' && file_path == NULL) {
            file_path = argv[i];
        } else {
            print_usage(stderr);
            return 2;
        }
    }

    if (file_path == NULL || policy == NULL || frames_str == NULL) {
        print_usage(stderr);
        return 2;
    }

    uint32_t min_f = 0;
    uint32_t max_f = 0;
    if (!parse_frames_range(frames_str, &min_f, &max_f)) {
        print_usage(stderr);
        return 2;
    }

    ms_page_key *refs = NULL;
    size_t n_refs = 0;
    if (!load_page_refs(file_path, &refs, &n_refs)) {
        (void)fprintf(stderr, "%s: failed to read page references\n", file_path);
        return 2;
    }

    uint64_t *faults = malloc((size_t)max_f * sizeof(*faults));
    uint32_t *points = malloc((size_t)max_f * sizeof(*points));
    if (faults == NULL || points == NULL) {
        free(faults);
        free(points);
        free(refs);
        return 3;
    }

    if (!ms_page_sweep(policy, refs, n_refs, max_f, faults)) {
        free(faults);
        free(points);
        free(refs);
        return 2;
    }

    size_t n_points = ms_page_belady_points(faults, max_f, points);

    if (json) {
        print_sweep_json(policy, min_f, max_f, faults, points, n_points);
    } else {
        print_sweep_text(min_f, max_f, faults);
    }

    free(faults);
    free(points);
    free(refs);
    return 0;
}

static bool parse_alloc_flag(const char *k, const char *v, ms_wl_alloc_uniform_params *p) {
    if (strcmp(k, "--seed") == 0) {
        p->seed = (uint64_t)strtoull(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--ops") == 0) {
        p->ops = (uint32_t)strtoul(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--memory-size") == 0) {
        p->memory_size = (int64_t)strtoll(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--size-min") == 0) {
        p->size_min = (int64_t)strtoll(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--size-max") == 0) {
        p->size_max = (int64_t)strtoll(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--p-alloc-ppm") == 0) {
        p->p_alloc_ppm = (uint32_t)strtoul(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--target-util-ppm") == 0) {
        p->target_util_ppm = (uint32_t)strtoul(v, NULL, 10);
        return true;
    }
    return false;
}

static int gen_alloc_uniform(int argc, char **argv) {
    ms_wl_alloc_uniform_params p = {
        .seed = 0,
        .ops = 2000,
        .memory_size = 1024,
        .size_min = 10,
        .size_max = 200,
        .p_alloc_ppm = 600000,
    };
    bool seed_set = false;
    const char *out_path = NULL;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (i + 1 < argc && parse_alloc_flag(argv[i], argv[i + 1], &p)) {
            if (strcmp(argv[i], "--seed") == 0) {
                seed_set = true;
            }
            i++;
        } else {
            print_usage(stderr);
            return 2;
        }
    }

    if (!seed_set || out_path == NULL) {
        print_usage(stderr);
        return 2;
    }

    FILE *f = fopen(out_path, "w");
    if (f == NULL) {
        perror(out_path);
        return 2;
    }
    ms_status st = ms_wl_gen_alloc_uniform(&p, f);
    (void)fclose(f);
    return (st == MS_OK) ? 0 : 2;
}

static bool parse_ref_flag(const char *k, const char *v, ms_wl_refs_locality_params *p) {
    if (strcmp(k, "--seed") == 0) {
        p->seed = (uint64_t)strtoull(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--n") == 0) {
        p->n = (uint32_t)strtoul(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--pages") == 0) {
        p->pages = (uint32_t)strtoul(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--working-set") == 0) {
        p->working_set = (uint32_t)strtoul(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--shift-every") == 0) {
        p->shift_every = (uint32_t)strtoul(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--p-out-ppm") == 0) {
        p->p_out_ppm = (uint32_t)strtoul(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--p-write-ppm") == 0) {
        p->p_write_ppm = (uint32_t)strtoul(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--frames") == 0) {
        p->frames = (uint32_t)strtoul(v, NULL, 10);
        return true;
    }
    if (strcmp(k, "--page-size") == 0) {
        p->page_size = (uint32_t)strtoul(v, NULL, 10);
        return true;
    }
    return false;
}

static int gen_refs_locality(int argc, char **argv) {
    ms_wl_refs_locality_params p = {
        .seed = 0,
        .n = 1000,
        .pages = 64,
        .working_set = 16,
        .shift_every = 50,
        .p_out_ppm = 100000,
        .p_write_ppm = 200000,
        .frames = 8,
        .page_size = 4096,
    };
    bool seed_set = false;
    const char *out_path = NULL;

    for (int i = 3; i < argc; i++) {
        if (strcmp(argv[i], "-o") == 0 && i + 1 < argc) {
            out_path = argv[++i];
        } else if (i + 1 < argc && parse_ref_flag(argv[i], argv[i + 1], &p)) {
            if (strcmp(argv[i], "--seed") == 0) {
                seed_set = true;
            }
            i++;
        } else {
            print_usage(stderr);
            return 2;
        }
    }

    if (!seed_set || out_path == NULL) {
        print_usage(stderr);
        return 2;
    }

    FILE *f = fopen(out_path, "w");
    if (f == NULL) {
        perror(out_path);
        return 2;
    }
    ms_status st = ms_wl_gen_refs_locality(&p, f);
    (void)fclose(f);
    return (st == MS_OK) ? 0 : 2;
}

static int cmd_gen(int argc, char **argv) {
    if (argc < 3) {
        print_usage(stderr);
        return 2;
    }
    const char *subcmd = argv[2];
    if (strcmp(subcmd, "alloc-uniform") == 0) {
        return gen_alloc_uniform(argc, argv);
    }
    if (strcmp(subcmd, "refs-locality") == 0) {
        return gen_refs_locality(argc, argv);
    }
    print_usage(stderr);
    return 2;
}

int main(int argc, char **argv) {
    MS_ASSERT(ms_sha256_self_test());

    if (argc < 2) {
        print_usage(stderr);
        return 2;
    }

    if (strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "help") == 0 ||
        strcmp(argv[1], "-h") == 0) {
        print_usage(stdout);
        return 0;
    }

    if (strcmp(argv[1], "validate") == 0) {
        return cmd_validate(argc, argv);
    }
    if (strcmp(argv[1], "run") == 0) {
        return cmd_run(argc, argv);
    }
    if (strcmp(argv[1], "compare") == 0) {
        return cmd_compare(argc, argv);
    }
    if (strcmp(argv[1], "refs") == 0) {
        return cmd_refs(argc, argv);
    }
    if (strcmp(argv[1], "sweep") == 0) {
        return cmd_sweep(argc, argv);
    }
    if (strcmp(argv[1], "gen") == 0) {
        return cmd_gen(argc, argv);
    }

    print_usage(stderr);
    return 2;
}
