/* Benchmark runner. Owner: P7. */
#include "memsim/bench.h"

#include "memsim/metrics.h"
#include "memsim/session.h"
#include "memsim/trace/writer.h"

#include <stdlib.h>
#include <string.h>

static ms_bench_placement record_placement(const ms_op *op, const ms_event *events,
                                           size_t n_events) {
    ms_bench_placement p = {
        .id = op->u.alloc.id,
        .size = op->u.alloc.size,
        .ok = false,
        .start = -1,
        .cause = -1,
    };
    for (size_t k = 0; k < n_events; k++) {
        if (events[k].type == MS_EV_BLOCK_ALLOCATED) {
            p.ok = true;
            p.start = events[k].data.block_allocated.start;
            p.cause = 0;
            break;
        }
        if (events[k].type == MS_EV_ALLOCATION_FAILED) {
            p.ok = false;
            p.start = -1;
            p.cause = (int)events[k].data.allocation_failed.cause;
            break;
        }
        if (events[k].type == MS_EV_ALLOC_REJECTED) {
            p.ok = false;
            p.start = -1;
            p.cause = -1;
            break;
        }
    }
    return p;
}

static ms_trace_writer *init_trace_writer(const ms_workload *w, const char *policy,
                                          const ms_bench_opts *o, bool paging) {
    if (o == NULL || o->trace == NULL) {
        return NULL;
    }
    ms_trace_symbols syms = {
        .pid_names = (const char *const *)w->pid_names,
        .n_pids = w->n_pids,
        .id_names = (const char *const *)w->id_names,
        .n_ids = w->n_ids,
    };
    ms_trace_writer *tw = ms_trace_writer_open(o->trace, &syms);
    if (tw == NULL) {
        return NULL;
    }
    ms_trace_header th = {
        .engine = "0.1.0",
        .git = (o->git != NULL) ? o->git : "",
        .workload = (o->workload_name != NULL) ? o->workload_name : "",
        .workload_sha256 = (o->workload_sha256 != NULL) ? o->workload_sha256 : "",
        .policy = policy,
        .paging = paging,
        .memory_size = paging ? 0 : w->memory_size,
        .page_size = paging ? w->page_size : 0,
        .frames = paging ? w->frames : 0,
    };
    ms_trace_write_header(tw, &th);
    return tw;
}

static void write_trace_summary(ms_trace_writer *tw, const ms_bench_result *out) {
    ms_trace_metric tm[MS_METRICS_MAX];
    for (size_t i = 0; i < out->n_metrics; i++) {
        tm[i] = (ms_trace_metric){
            .key = out->metrics[i].key,
            .is_ratio = out->metrics[i].is_ratio,
            .value = out->metrics[i].value,
        };
    }
    ms_trace_write_summary(tw, tm, out->n_metrics);
    ms_trace_writer_close(tw);
}

static ms_status run_bench_ops(ms_session *s, const ms_workload *w, bool paging, ms_metrics *m,
                               ms_bench_placement *placements) {
    size_t alloc_idx = 0;
    for (size_t i = 0; i < w->n_ops; i++) {
        const ms_event *events = NULL;
        size_t n_events = 0;
        ms_status st = ms_session_step(s, &w->ops[i], &events, &n_events);
        if (st != MS_OK) {
            return st;
        }

        ms_metrics_on_events(m, events, n_events);
        if (!paging) {
            size_t n_blocks = 0;
            const ms_block *blocks = ms_session_blocks(s, &n_blocks);
            ms_metrics_on_blocks(m, blocks, n_blocks);
        }

        if (w->ops[i].kind == MS_OP_ALLOC) {
            placements[alloc_idx++] = record_placement(&w->ops[i], events, n_events);
        }
    }
    return MS_OK;
}

ms_status ms_bench_run(const ms_workload *w, const char *policy, const ms_bench_opts *o,
                       ms_bench_result *out) {
    if (w == NULL || policy == NULL || out == NULL) {
        return MS_ERR_INVALID;
    }
    memset(out, 0, sizeof(*out));

    size_t n_allocs = 0;
    for (size_t i = 0; i < w->n_ops; i++) {
        if (w->ops[i].kind == MS_OP_ALLOC) {
            n_allocs++;
        }
    }

    ms_bench_placement *placements = NULL;
    if (n_allocs > 0) {
        placements = malloc(n_allocs * sizeof(*placements));
        if (placements == NULL) {
            return MS_ERR_NOMEM;
        }
    }

    ms_session_config scfg = {false};
    scfg.paging = (w->kind == MS_WL_PAGING);
    scfg.policy = policy;
    scfg.check = ((o != NULL) && o->check) != 0;
    if (scfg.paging) {
        scfg.paging_cfg = ms_paging_config_default();
        scfg.paging_cfg.page_size = w->page_size;
        scfg.paging_cfg.frames = w->frames;
        scfg.paging_cfg.policy = policy;
    } else {
        scfg.memory_size = w->memory_size;
    }

    ms_session *s = NULL;
    ms_status st = ms_session_create(&scfg, w->ops, w->n_ops, &s);
    if (st != MS_OK) {
        free(placements);
        return st;
    }

    ms_metrics *m = ms_metrics_create(scfg.paging);
    if (m == NULL) {
        ms_session_destroy(s);
        free(placements);
        return MS_ERR_NOMEM;
    }

    ms_trace_writer *tw = NULL;
    if (o != NULL && o->trace != NULL) {
        tw = init_trace_writer(w, policy, o, scfg.paging);
        if (tw == NULL) {
            ms_metrics_destroy(m);
            ms_session_destroy(s);
            free(placements);
            return MS_ERR_NOMEM;
        }
        ms_session_attach_trace(s, tw);
    }

    st = run_bench_ops(s, w, scfg.paging, m, placements);
    if (st != MS_OK) {
        if (tw != NULL) {
            ms_trace_writer_close(tw);
        }
        ms_metrics_destroy(m);
        ms_session_destroy(s);
        free(placements);
        return st;
    }

    out->n_metrics = ms_metrics_summary(m, out->metrics, MS_METRICS_MAX);

    if (tw != NULL) {
        write_trace_summary(tw, out);
    }

    ms_session_destroy(s);
    ms_metrics_destroy(m);

    out->placements = placements;
    out->n_placements = n_allocs;
    return MS_OK;
}

void ms_bench_result_free(ms_bench_result *r) {
    if (r != NULL) {
        free(r->placements);
        r->placements = NULL;
        r->n_placements = 0;
    }
}
