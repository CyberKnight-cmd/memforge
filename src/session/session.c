/* Session facade. Owner: P1. */
#include "memsim/session.h"

#include "memsim/common/errors.h"
#include "memsim/contiguous/engine.h"
#include "memsim/paging/engine.h"
#include "memsim/trace/snapshot.h"
#include "memsim/trace/writer.h"

#include <stdlib.h>
#include <string.h>

struct ms_session {
    bool paging;
    bool check;
    int64_t memory_size;
    uint32_t frames;
    ms_cont_engine *cont;
    ms_page_engine *page;
    ms_trace_writer *trace_writer;
    uint64_t next_seq;
    uint64_t step_count;
    ms_event *event_buf;
    size_t event_cap;
    ms_page_frame_info *frame_infos;
    ms_trace_frame *trace_frames;
};

static void write_snapshot(ms_session *s, uint64_t step) {
    if (s->trace_writer == NULL) {
        return;
    }
    if (s->paging) {
        size_t n_frames = ms_page_engine_frames(s->page, s->frame_infos, s->frames);
        for (size_t i = 0; i < n_frames; i++) {
            s->trace_frames[i] = (ms_trace_frame){
                .frame = (uint32_t)i,
                .pid = s->frame_infos[i].pid,
                .vpn = s->frame_infos[i].vpn,
                .referenced = s->frame_infos[i].referenced,
                .dirty = s->frame_infos[i].dirty,
            };
        }
        ms_trace_write_page_snapshot(s->trace_writer, step, s->trace_frames, n_frames);
    } else {
        size_t n_blocks = 0;
        const ms_block *blocks = ms_cont_engine_blocks(s->cont, &n_blocks);
        ms_trace_write_cont_snapshot(s->trace_writer, step, s->memory_size, blocks, n_blocks);
    }
}

ms_status ms_session_create(const ms_session_config *cfg, const ms_op *ops, size_t n_ops,
                            ms_session **out) {
    if (out == NULL) {
        return MS_ERR_INVALID;
    }
    *out = NULL;
    if (cfg == NULL || cfg->policy == NULL) {
        return MS_ERR_INVALID;
    }

    if (!cfg->paging) {
        if (cfg->memory_size <= 0) {
            return MS_ERR_INVALID;
        }
        if (strcmp(cfg->policy, "first-fit") != 0 && strcmp(cfg->policy, "best-fit") != 0 &&
            strcmp(cfg->policy, "worst-fit") != 0) {
            return MS_ERR_INVALID;
        }
        ms_cont_engine *cont = ms_cont_engine_create(cfg->memory_size, cfg->policy);
        if (cont == NULL) {
            return MS_ERR_NOMEM;
        }
        ms_session *s = calloc(1, sizeof(*s));
        if (s == NULL) {
            ms_cont_engine_destroy(cont);
            return MS_ERR_NOMEM;
        }
        s->paging = false;
        s->check = cfg->check;
        s->memory_size = cfg->memory_size;
        s->cont = cont;
        *out = s;
        return MS_OK;
    }

    if (strcmp(cfg->policy, "fifo") != 0 && strcmp(cfg->policy, "lru") != 0 &&
        strcmp(cfg->policy, "opt") != 0) {
        return MS_ERR_INVALID;
    }
    if (ops == NULL && n_ops > 0) {
        return MS_ERR_INVALID;
    }

    ms_paging_config pcfg = cfg->paging_cfg;
    pcfg.policy = cfg->policy;
    ms_page_engine *page = NULL;
    ms_status st = ms_page_engine_create(&pcfg, &page);
    if (st != MS_OK) {
        return st;
    }

    st = ms_page_engine_prepare(page, ops, n_ops);
    if (st != MS_OK) {
        ms_page_engine_destroy(page);
        return st;
    }

    ms_session *s = calloc(1, sizeof(*s));
    if (s == NULL) {
        ms_page_engine_destroy(page);
        return MS_ERR_NOMEM;
    }
    s->paging = true;
    s->check = cfg->check;
    s->frames = pcfg.frames;
    s->page = page;

    if (s->frames > 0) {
        s->frame_infos = malloc(s->frames * sizeof(*s->frame_infos));
        s->trace_frames = malloc(s->frames * sizeof(*s->trace_frames));
        if (s->frame_infos == NULL || s->trace_frames == NULL) {
            ms_session_destroy(s);
            return MS_ERR_NOMEM;
        }
    }

    *out = s;
    return MS_OK;
}

void ms_session_destroy(ms_session *s) {
    if (s == NULL) {
        return;
    }
    if (s->cont != NULL) {
        ms_cont_engine_destroy(s->cont);
    }
    if (s->page != NULL) {
        ms_page_engine_destroy(s->page);
    }
    free(s->event_buf);
    free(s->frame_infos);
    free(s->trace_frames);
    free(s);
}

void ms_session_attach_trace(ms_session *s, ms_trace_writer *w) {
    if (s == NULL) {
        return;
    }
    s->trace_writer = w;
    if (w != NULL) {
        write_snapshot(s, 0);
    }
}

static ms_status step_cont(ms_session *s, const ms_op *op, const ms_event **events, size_t *count) {
    switch (op->kind) {
    case MS_OP_ALLOC:
        (void)ms_cont_engine_alloc(s->cont, op->u.alloc.pid, op->u.alloc.id, op->u.alloc.size);
        break;
    case MS_OP_FREE:
        (void)ms_cont_engine_free(s->cont, op->u.free.pid, op->u.free.id);
        break;
    case MS_OP_TERMINATE:
        (void)ms_cont_engine_terminate(s->cont, op->u.terminate.pid);
        break;
    case MS_OP_COMPACT:
        (void)ms_cont_engine_compact(s->cont);
        break;
    default:
        return MS_ERR_INVALID;
    }
    *events = ms_cont_engine_events(s->cont, count);
    return MS_OK;
}

static ms_status step_page(ms_session *s, const ms_op *op, const ms_event **events, size_t *count) {
    switch (op->kind) {
    case MS_OP_SPAWN:
    case MS_OP_ACCESS:
    case MS_OP_TERMINATE:
        break;
    default:
        return MS_ERR_INVALID;
    }
    ms_page_step step = {0};
    ms_status st = ms_page_engine_apply(s->page, op, &step);
    if (st != MS_OK) {
        return st;
    }
    *events = step.events;
    *count = step.n_events;
    return MS_OK;
}

static void check_invariants(const ms_session *s) {
    if (s->paging) {
        MS_ASSERT(ms_page_engine_check(s->page));
    } else {
        MS_ASSERT(ms_cont_engine_check(s->cont) == 0);
    }
}

ms_status ms_session_step(ms_session *s, const ms_op *op, const ms_event **events, size_t *n) {
    if (s == NULL || op == NULL || events == NULL || n == NULL) {
        return MS_ERR_INVALID;
    }

    const ms_event *src_events = NULL;
    size_t src_count = 0;
    ms_status st = s->paging ? step_page(s, op, &src_events, &src_count)
                             : step_cont(s, op, &src_events, &src_count);
    if (st != MS_OK) {
        return st;
    }

    s->step_count++;

    if (src_count > s->event_cap) {
        ms_event *new_buf = realloc(s->event_buf, src_count * sizeof(*new_buf));
        if (new_buf == NULL) {
            return MS_ERR_NOMEM;
        }
        s->event_buf = new_buf;
        s->event_cap = src_count;
    }

    for (size_t i = 0; i < src_count; i++) {
        s->event_buf[i] = src_events[i];
        s->event_buf[i].seq = s->next_seq++;
        s->event_buf[i].step = s->step_count;
    }

    if (s->trace_writer != NULL) {
        for (size_t i = 0; i < src_count; i++) {
            ms_trace_write_event(s->trace_writer, &s->event_buf[i]);
        }
        write_snapshot(s, s->step_count);
    }

    if (s->check) {
        check_invariants(s);
    }

    *events = s->event_buf;
    *n = src_count;
    return MS_OK;
}

const ms_block *ms_session_blocks(const ms_session *s, size_t *n) {
    if (s == NULL || n == NULL || s->paging) {
        if (n != NULL) {
            *n = 0;
        }
        return NULL;
    }
    return ms_cont_engine_blocks(s->cont, n);
}
