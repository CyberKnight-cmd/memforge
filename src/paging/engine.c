/* PagingEngine. Owner: P5. */
#include "memsim/paging/engine.h"

#include "frames.h"
#include "invariants.h"
#include "mmu.h"
#include "replacement.h"
#include "tables.h"

#include <stdlib.h>

#define MAX_STEP_EVENTS                                                                            \
    8 /* worst case: fault, victim, evicted, written back, loaded, translated                      \
       */

struct ms_page_frame_view {
    const ms_page_engine *engine;
};

struct ms_page_engine {
    uint32_t k; /* offset bits */
    const ms_page_replacement_policy *policy;
    void *state;
    bool prepared;
    ms_page_frames frames;
    ms_page_tables tables;
    uint32_t *candidates; /* 0 .. F-1: global scope, every frame is resident when one is needed */
    ms_page_frame_view view;
    uint64_t t;          /* ACCESS index, violations included */
    uint64_t pool_takes; /* for V5 */
    ms_page_stats stats;
    ms_event events[MAX_STEP_EVENTS];
    size_t n_events;
};

static ms_event *emit(ms_page_engine *e, ms_event_type type) {
    MS_ASSERT(e->n_events < MAX_STEP_EVENTS);
    ms_event *ev = &e->events[e->n_events++];
    *ev = (ms_event){.type = type};
    return ev;
}

static const ms_page_pte *view_pte(const ms_page_frame_view *v, uint32_t frame) {
    MS_ASSERT(frame < v->engine->frames.n);
    ms_page_key k = v->engine->frames.owner[frame];
    const ms_page_table *pt = ms_page_tables_get(&v->engine->tables, k.pid);
    return pt == NULL ? NULL : &pt->ptes[k.vpn];
}

uint32_t ms_page_view_frames(const ms_page_frame_view *v) {
    return v->engine->frames.n;
}

ms_page_key ms_page_view_owner(const ms_page_frame_view *v, uint32_t frame) {
    MS_ASSERT(frame < v->engine->frames.n);
    return v->engine->frames.owner[frame];
}

bool ms_page_view_referenced(const ms_page_frame_view *v, uint32_t frame) {
    const ms_page_pte *p = view_pte(v, frame);
    return p != NULL && p->referenced;
}

bool ms_page_view_dirty(const ms_page_frame_view *v, uint32_t frame) {
    const ms_page_pte *p = view_pte(v, frame);
    return p != NULL && p->dirty;
}

ms_status ms_page_engine_create(const ms_paging_config *cfg, ms_page_engine **out) {
    *out = NULL;
    uint32_t k = ms_page_offset_bits(cfg->page_size);
    const ms_page_replacement_policy *policy =
        cfg->policy == NULL ? NULL : ms_page_replacement_find(cfg->policy);
    if (k == MS_NONE || cfg->frames == 0 || cfg->frames == MS_NONE || policy == NULL ||
        cfg->scope != MS_SCOPE_GLOBAL) {
        return MS_ERR_INVALID;
    }
    ms_page_engine *e = calloc(1, sizeof *e);
    if (e == NULL) {
        return MS_ERR_NOMEM;
    }
    e->k = k;
    e->policy = policy;
    e->view.engine = e;
    e->frames.n = cfg->frames;
    e->frames.owner = malloc((size_t)cfg->frames * sizeof *e->frames.owner);
    e->candidates = malloc((size_t)cfg->frames * sizeof *e->candidates);
    e->state = policy->create(cfg->frames);
    if (e->frames.owner == NULL || e->candidates == NULL || e->state == NULL) {
        ms_page_engine_destroy(e);
        return MS_ERR_NOMEM;
    }
    for (uint32_t i = 0; i < cfg->frames; i++) {
        e->frames.owner[i] = (ms_page_key){.pid = MS_NONE, .vpn = MS_NONE};
        e->candidates[i] = i;
    }
    *out = e;
    return MS_OK;
}

void ms_page_engine_destroy(ms_page_engine *e) {
    if (e == NULL) {
        return;
    }
    if (e->state != NULL) {
        e->policy->destroy(e->state);
    }
    ms_page_tables_destroy(&e->tables);
    free(e->frames.owner);
    free(e->candidates);
    free(e);
}

ms_status ms_page_engine_prepare(ms_page_engine *e, const ms_op *ops, size_t n) {
    size_t n_refs = 0;
    for (size_t i = 0; i < n; i++) {
        n_refs += ops[i].kind == MS_OP_ACCESS;
    }
    ms_page_key *refs = malloc((n_refs == 0 ? 1 : n_refs) * sizeof *refs);
    if (refs == NULL) {
        return MS_ERR_NOMEM;
    }
    size_t j = 0;
    for (size_t i = 0; i < n; i++) {
        if (ops[i].kind != MS_OP_ACCESS) {
            continue;
        }
        uint64_t vpn = ops[i].u.access.by_va ? ms_page_vpn_of(ops[i].u.access.vpn_or_va, e->k)
                                             : ops[i].u.access.vpn_or_va;
        /* an out-of-range vpn is a violation at run time; any non-resident key works here */
        refs[j++] = (ms_page_key){.pid = ops[i].u.access.pid,
                                  .vpn = vpn > UINT32_MAX ? UINT32_MAX : (uint32_t)vpn};
    }
    bool ok = e->policy->prepare == NULL || e->policy->prepare(e->state, refs, n_refs);
    free(refs); /* policies copy what they keep */
    if (!ok) {
        return MS_ERR_NOMEM;
    }
    e->prepared = true;
    return MS_OK;
}

static void evict(ms_page_engine *e, uint32_t frame) {
    ms_page_key key = e->frames.owner[frame];
    ms_page_pte *pte = &ms_page_tables_get(&e->tables, key.pid)->ptes[key.vpn];
    ms_event *ev = emit(e, MS_EV_PAGE_EVICTED);
    ev->data.page_evicted.frame = frame;
    ev->data.page_evicted.pid = key.pid;
    ev->data.page_evicted.vpn = key.vpn;
    ev->data.page_evicted.dirty = pte->dirty;
    if (pte->dirty) {
        ev = emit(e, MS_EV_PAGE_WRITTEN_BACK);
        ev->data.page_written_back.frame = frame;
        ev->data.page_written_back.pid = key.pid;
        ev->data.page_written_back.vpn = key.vpn;
        e->stats.writebacks++;
    }
    pte->present = pte->referenced = pte->dirty = false;
    pte->frame = MS_NONE;
    e->frames.owner[frame] = (ms_page_key){.pid = MS_NONE, .vpn = MS_NONE};
    e->stats.evictions++;
    if (e->policy->on_evict != NULL) {
        e->policy->on_evict(e->state, frame, key);
    }
}

/* Section 9, one access: validate, hit or fault, free frame or victim, load, complete. */
static void access(ms_page_engine *e, ms_pid pid, bool by_va, uint64_t vpn_or_va, bool write,
                   ms_page_step *out) {
    uint64_t t = e->t++;
    ms_page_table *pt = ms_page_tables_get(&e->tables, pid);
    uint64_t vpn = by_va ? ms_page_vpn_of(vpn_or_va, e->k) : vpn_or_va;
    if (pt == NULL || vpn >= pt->pages) {
        ms_event *ev = emit(e, MS_EV_ACCESS_VIOLATION);
        ev->data.access_violation.pid = pid;
        ev->data.access_violation.by_va = by_va;
        ev->data.access_violation.vpn_or_va = vpn_or_va;
        ev->data.access_violation.reason =
            pt == NULL ? MS_VIOLATION_NO_SUCH_PROCESS : MS_VIOLATION_OUT_OF_RANGE;
        return;
    }
    ms_page_key key = {.pid = pid, .vpn = (uint32_t)vpn};
    ms_page_pte *pte = &pt->ptes[vpn];
    e->stats.refs++;

    if (pte->present) {
        ms_event *ev = emit(e, MS_EV_PAGE_HIT);
        ev->data.page_hit.pid = pid;
        ev->data.page_hit.vpn = key.vpn;
        ev->data.page_hit.frame = pte->frame;
        ev->data.page_hit.write = write;
        e->stats.hits++;
    } else {
        ms_event *ev = emit(e, MS_EV_PAGE_FAULT);
        ev->data.page_fault.pid = pid;
        ev->data.page_fault.vpn = key.vpn;
        ev->data.page_fault.kind = pte->seen ? MS_FAULT_CAPACITY : MS_FAULT_COMPULSORY;
        ev->data.page_fault.write = write;
        pte->seen = true;
        e->stats.faults++;
        out->fault = true;

        uint32_t frame = ms_page_frames_lowest_free(&e->frames);
        if (frame != MS_NONE) {
            ev = emit(e, MS_EV_FRAME_ALLOCATED);
            ev->data.frame_allocated.frame = frame;
            ev->data.frame_allocated.pid = pid;
            ev->data.frame_allocated.vpn = key.vpn;
            e->pool_takes++;
        } else {
            frame = e->policy->choose_victim(e->state, &e->view, e->candidates, e->frames.n, t);
            MS_ASSERT(frame < e->frames.n);
            ms_page_key victim = e->frames.owner[frame];
            ev = emit(e, MS_EV_VICTIM_SELECTED);
            ev->data.victim_selected.policy = e->policy->name;
            ev->data.victim_selected.frame = frame;
            ev->data.victim_selected.pid = victim.pid;
            ev->data.victim_selected.vpn = victim.vpn;
            evict(e, frame);
        }
        /* load clean: a write fault dirties the page below, in the same access */
        pte->present = true;
        pte->frame = frame;
        pte->referenced = pte->dirty = false;
        e->frames.owner[frame] = key;
        if (e->policy->on_load != NULL) {
            e->policy->on_load(e->state, frame, key, t);
        }
        ev = emit(e, MS_EV_PAGE_LOADED);
        ev->data.page_loaded.frame = frame;
        ev->data.page_loaded.pid = pid;
        ev->data.page_loaded.vpn = key.vpn;
    }

    pte->referenced = true;
    pte->dirty = pte->dirty || write;
    if (e->policy->on_access != NULL) {
        e->policy->on_access(e->state, pte->frame, key, t, write);
    }
    uint64_t offset = by_va ? ms_page_offset_of(vpn_or_va, e->k) : 0;
    out->pa = ms_page_pa(pte->frame, offset, e->k);
    if (by_va) {
        ms_event *ev = emit(e, MS_EV_ADDRESS_TRANSLATED);
        ev->data.address_translated.pid = pid;
        ev->data.address_translated.va = vpn_or_va;
        ev->data.address_translated.vpn = key.vpn;
        ev->data.address_translated.offset = offset;
        ev->data.address_translated.frame = pte->frame;
        ev->data.address_translated.pa = out->pa;
    }
}

/* Releases every frame of pid, no write-back. Unknown pid: no-op (it may be contiguous-only). */
static void terminate(ms_page_engine *e, ms_pid pid) {
    ms_page_table *pt = ms_page_tables_get(&e->tables, pid);
    if (pt == NULL) {
        return;
    }
    uint32_t released = 0;
    for (uint32_t v = 0; v < pt->pages; v++) {
        if (!pt->ptes[v].present) {
            continue;
        }
        uint32_t frame = pt->ptes[v].frame;
        e->frames.owner[frame] = (ms_page_key){.pid = MS_NONE, .vpn = MS_NONE};
        released++;
        if (e->policy->on_evict != NULL) {
            e->policy->on_evict(e->state, frame, (ms_page_key){.pid = pid, .vpn = v});
        }
    }
    ms_page_tables_release(&e->tables, pid);
    ms_event *ev = emit(e, MS_EV_PROCESS_TERMINATED);
    ev->data.process_terminated.pid = pid;
    ev->data.process_terminated.released_frames = released;
}

ms_status ms_page_engine_apply(ms_page_engine *e, const ms_op *op, ms_page_step *out) {
    *out = (ms_page_step){0};
    e->n_events = 0;
    if (e->policy->requires_lookahead && !e->prepared) {
        return MS_ERR_INVALID;
    }
    switch (op->kind) {
    case MS_OP_SPAWN: {
        ms_status st = ms_page_tables_spawn(&e->tables, op->u.spawn.pid, op->u.spawn.pages);
        if (st != MS_OK) {
            return st;
        }
        ms_event *ev = emit(e, MS_EV_PROCESS_SPAWNED);
        ev->data.process_spawned.pid = op->u.spawn.pid;
        ev->data.process_spawned.pages = op->u.spawn.pages;
        break;
    }
    case MS_OP_ACCESS:
        access(e, op->u.access.pid, op->u.access.by_va, op->u.access.vpn_or_va, op->u.access.write,
               out);
        break;
    case MS_OP_TERMINATE:
        terminate(e, op->u.terminate.pid);
        break;
    default:
        return MS_ERR_INVALID;
    }
    out->events = e->events;
    out->n_events = e->n_events;
    return MS_OK;
}

ms_page_stats ms_page_engine_stats(const ms_page_engine *e) {
    return e->stats;
}

bool ms_page_engine_check(const ms_page_engine *e) {
    return ms_page_invariants_hold(&e->frames, &e->tables, &e->stats, e->pool_takes);
}
