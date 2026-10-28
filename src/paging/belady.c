/* Belady sweep. Owner: P6. */
#include "memsim/paging/belady.h"

#include "memsim/paging/engine.h"

#include <stdlib.h>

typedef struct {
    ms_pid pid;
    uint32_t max_vpn;
} proc_spec;

static ms_op *build_ops(const ms_page_key *refs, size_t n, size_t *out_count) {
    *out_count = 0;
    if (n == 0) {
        return NULL;
    }
    proc_spec *procs = malloc(n * sizeof *procs);
    if (procs == NULL) {
        return NULL;
    }
    size_t n_procs = 0;
    for (size_t i = 0; i < n; i++) {
        size_t p = 0;
        while (p < n_procs && procs[p].pid != refs[i].pid) {
            p++;
        }
        if (p < n_procs) {
            if (refs[i].vpn > procs[p].max_vpn) {
                procs[p].max_vpn = refs[i].vpn;
            }
        } else {
            procs[n_procs++] = (proc_spec){.pid = refs[i].pid, .max_vpn = refs[i].vpn};
        }
    }

    size_t total = n_procs + n;
    ms_op *ops = malloc(total * sizeof *ops);
    if (ops == NULL) {
        free(procs);
        return NULL;
    }
    for (size_t i = 0; i < n_procs; i++) {
        ops[i] = (ms_op){
            .kind = MS_OP_SPAWN,
            .u.spawn =
                {
                    .pid = procs[i].pid,
                    .pages = procs[i].max_vpn + 1,
                },
        };
    }
    for (size_t i = 0; i < n; i++) {
        ops[n_procs + i] = (ms_op){
            .kind = MS_OP_ACCESS,
            .u.access =
                {
                    .pid = refs[i].pid,
                    .by_va = false,
                    .vpn_or_va = refs[i].vpn,
                    .write = false,
                },
        };
    }
    free(procs);
    *out_count = total;
    return ops;
}

bool ms_page_count_faults(const char *policy, const ms_page_key *refs, size_t n, uint32_t frames,
                          uint64_t *faults) {
    if (policy == NULL || frames == 0) {
        return false;
    }

    ms_paging_config cfg = ms_paging_config_default();
    cfg.frames = frames;
    cfg.policy = policy;

    ms_page_engine *e = NULL;
    if (ms_page_engine_create(&cfg, &e) != MS_OK) {
        return false;
    }

    if (n == 0) {
        if (ms_page_engine_prepare(e, NULL, 0) != MS_OK) {
            ms_page_engine_destroy(e);
            return false;
        }
        *faults = ms_page_engine_stats(e).faults;
        ms_page_engine_destroy(e);
        return true;
    }

    size_t total_ops = 0;
    ms_op *ops = build_ops(refs, n, &total_ops);
    if (ops == NULL) {
        ms_page_engine_destroy(e);
        return false;
    }

    if (ms_page_engine_prepare(e, ops, total_ops) != MS_OK) {
        free(ops);
        ms_page_engine_destroy(e);
        return false;
    }

    bool ok = true;
    for (size_t i = 0; ok && i < total_ops; i++) {
        ms_page_step step;
        if (ms_page_engine_apply(e, &ops[i], &step) != MS_OK) {
            ok = false;
        }
    }

    if (ok) {
        *faults = ms_page_engine_stats(e).faults;
    }
    free(ops);
    ms_page_engine_destroy(e);
    return ok;
}

bool ms_page_sweep(const char *policy, const ms_page_key *refs, size_t n, uint32_t max_frames,
                   uint64_t *faults) {
    bool ok = max_frames > 0;
    for (uint32_t f = 1; ok && f <= max_frames; f++) {
        ok = ms_page_count_faults(policy, refs, n, f, &faults[f - 1]);
    }
    return ok;
}

size_t ms_page_belady_points(const uint64_t *faults, uint32_t max_frames, uint32_t *points) {
    size_t count = 0;
    for (uint32_t f = 1; f < max_frames; f++) {
        if (faults[f] > faults[f - 1]) {
            points[count++] = f;
        }
    }
    return count;
}
