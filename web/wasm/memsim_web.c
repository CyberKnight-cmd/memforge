/* WebAssembly bridge: the browser hands over a workload and gets the canonical trace back.
 * Each call is a full deterministic run through the same pipeline as `memsim run`, so the web
 * app can never disagree with the CLI. Owner: P1. */
#include "memsim/bench.h"
#include "memsim/workloads/generators.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define WEB_EXPORT(name) __attribute__((export_name(name)))

WEB_EXPORT("memsim_alloc") void *memsim_alloc(size_t n) {
    return malloc(n);
}

WEB_EXPORT("memsim_free") void memsim_free(void *p) {
    free(p);
}

/* Runs `text` (a workload) under `policy`. Returns a NUL-terminated string the caller frees:
 * the JSONL trace, or "!<line>:<message>" when the workload is invalid. */
WEB_EXPORT("memsim_run") char *memsim_run(const char *text, size_t len, const char *policy) {
    char *out = NULL;
    size_t out_len = 0;
    ms_workload w = {0};
    ms_wl_error err = {0};
    if (ms_wl_parse(text, len, &w, &err) != MS_OK) {
        size_t cap = sizeof err.message + 32;
        out = malloc(cap);
        if (out != NULL) {
            (void)snprintf(out, cap, "!%zu:%s", err.line, err.message);
        }
        return out;
    }
    FILE *f = open_memstream(&out, &out_len);
    if (f == NULL) {
        ms_wl_free(&w);
        return NULL;
    }
    ms_bench_opts opts = {
        .trace = f, .workload_name = "web", .workload_sha256 = "", .git = "web", .check = true};
    ms_bench_result r = {0};
    ms_status st = ms_bench_run(&w, policy, &opts, &r);
    (void)fclose(f);
    ms_bench_result_free(&r);
    ms_wl_free(&w);
    if (st != MS_OK) {
        free(out);
        out = malloc(64);
        if (out != NULL) {
            (void)snprintf(out, 64, "!0:unknown policy or engine error %d", (int)st);
        }
    }
    return out;
}

/* The section 12 generators, so the web app can load exactly the workloads the experiments use.
 * Return the workload text (caller frees), or NULL on invalid parameters. */
static char *generated(ms_status (*gen)(const void *, FILE *), const void *params) {
    char *out = NULL;
    size_t len = 0;
    FILE *f = open_memstream(&out, &len);
    if (f == NULL) {
        return NULL;
    }
    ms_status st = gen(params, f);
    (void)fclose(f);
    if (st != MS_OK) {
        free(out);
        return NULL;
    }
    return out;
}

static ms_status gen_alloc(const void *p, FILE *f) {
    return ms_wl_gen_alloc_uniform(p, f);
}

static ms_status gen_refs(const void *p, FILE *f) {
    return ms_wl_gen_refs_locality(p, f);
}

WEB_EXPORT("memsim_gen_alloc_uniform")
char *memsim_gen_alloc_uniform(uint64_t seed, uint32_t ops, int64_t memory_size, int64_t size_min,
                               int64_t size_max, uint32_t p_alloc_ppm, uint32_t target_util_ppm) {
    ms_wl_alloc_uniform_params p = {seed,     ops,         memory_size,    size_min,
                                    size_max, p_alloc_ppm, target_util_ppm};
    return generated(gen_alloc, &p);
}

WEB_EXPORT("memsim_gen_refs_locality")
char *memsim_gen_refs_locality(uint64_t seed, uint32_t n, uint32_t pages, uint32_t working_set,
                               uint32_t shift_every, uint32_t p_out_ppm, uint32_t p_write_ppm,
                               uint32_t frames, uint32_t page_size) {
    ms_wl_refs_locality_params p = {seed,      n,           pages,  working_set, shift_every,
                                    p_out_ppm, p_write_ppm, frames, page_size};
    return generated(gen_refs, &p);
}
