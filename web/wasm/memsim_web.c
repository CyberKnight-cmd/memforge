/* WebAssembly bridge: the browser hands over a workload and gets the canonical trace back.
 * Each call is a full deterministic run through the same pipeline as `memsim run`, so the web
 * app can never disagree with the CLI. Owner: P1. */
#include "memsim/bench.h"

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
