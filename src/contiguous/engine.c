/* ContiguousEngine. Owner: P2. */
#include "memsim/contiguous/engine.h"

#include "compaction.h"
#include "fragmentation.h"
#include "invariants.h"
#include "model.h"
#include "placement.h"

#include <stdlib.h>
#include <string.h>

struct ms_cont_engine {
    ms_cont_model model;
    const ms_cont_placement_policy *policy;
    ms_event *events;
    size_t event_count;
    size_t event_cap;
};

/* shortcut: local lookup table; move to P3's policies/registry.c once it exists. */
static const ms_cont_placement_policy *const policies[] = {
    &ms_cont_first_fit_policy,
    &ms_cont_best_fit_policy,
    &ms_cont_worst_fit_policy,
};

/* shortcut: aborts on contract violations; switch to MS_ASSERT once P1 lands errors.h. */
static void contract(bool ok) {
    if (!ok) {
        abort();
    }
}

static void emit(ms_cont_engine *e, ms_event ev) {
    if (e->event_count == e->event_cap) {
        size_t next_cap = e->event_cap == 0 ? 8 : e->event_cap * 2;
        ms_event *next = realloc(e->events, next_cap * sizeof *next);
        contract(next != NULL);
        e->events = next;
        e->event_cap = next_cap;
    }
    ev.seq = 0;
    ev.step = 0;
    e->events[e->event_count++] = ev;
}

static void emit_free_and_coalesce(ms_cont_engine *e, size_t i) {
    ms_cont_model *m = &e->model;
    ms_pid pid = m->blocks[i].pid;
    ms_alloc_id id = m->blocks[i].alloc_id;
    int64_t start = m->blocks[i].start;
    int64_t size = m->blocks[i].size;

    emit(e, (ms_event){
                .type = MS_EV_BLOCK_FREED,
                .data.block_freed =
                    {
                        .pid = pid,
                        .id = id,
                        .start = start,
                        .size = size,
                    },
            });

    bool left_free = (i > 0 && m->blocks[i - 1].state == MS_BLOCK_FREE);
    bool right_free = (i + 1 < m->count && m->blocks[i + 1].state == MS_BLOCK_FREE);

    if (left_free || right_free) {
        ms_event ev = {
            .type = MS_EV_BLOCKS_COALESCED,
        };
        uint32_t n = 0;
        int64_t u_start = left_free ? m->blocks[i - 1].start : start;
        int64_t u_size = 0;

        if (left_free) {
            ev.data.blocks_coalesced.merged[n++] =
                (ms_hole){m->blocks[i - 1].start, m->blocks[i - 1].size};
            u_size += m->blocks[i - 1].size;
        }
        ev.data.blocks_coalesced.merged[n++] = (ms_hole){start, size};
        u_size += size;
        if (right_free) {
            ev.data.blocks_coalesced.merged[n++] =
                (ms_hole){m->blocks[i + 1].start, m->blocks[i + 1].size};
            u_size += m->blocks[i + 1].size;
        }
        ev.data.blocks_coalesced.n_merged = n;
        ev.data.blocks_coalesced.result = (ms_hole){u_start, u_size};
        emit(e, ev);
    }
}

ms_cont_engine *ms_cont_engine_create(int64_t memory_size, const char *policy) {
    if (memory_size <= 0 || policy == NULL) {
        return NULL;
    }
    const ms_cont_placement_policy *chosen = NULL;
    for (size_t i = 0; i < sizeof policies / sizeof policies[0]; i++) {
        if (strcmp(policies[i]->name, policy) == 0) {
            chosen = policies[i];
        }
    }
    ms_cont_engine *e = malloc(sizeof *e);
    if (chosen == NULL || e == NULL || !ms_cont_model_init(&e->model, memory_size)) {
        free(e);
        return NULL;
    }
    e->policy = chosen;
    e->events = NULL;
    e->event_count = 0;
    e->event_cap = 0;
    return e;
}

void ms_cont_engine_destroy(ms_cont_engine *e) {
    if (e != NULL) {
        ms_cont_model_fini(&e->model);
        free(e->events);
        free(e);
    }
}

ms_cont_result ms_cont_engine_alloc(ms_cont_engine *e, ms_pid pid, ms_alloc_id id, int64_t size) {
    e->event_count = 0;
    ms_cont_model *m = &e->model;
    ms_cont_result r = {.size = size};
    if (size <= 0 || id == MS_NONE) { /* section 7 has no INVALID_ID reason */
        r.outcome = MS_CONT_REJECTED_INVALID_SIZE;
        emit(e, (ms_event){
                    .type = MS_EV_ALLOC_REJECTED,
                    .data.alloc_rejected =
                        {
                            .pid = pid,
                            .id = id,
                            .reason = MS_REJECT_INVALID_SIZE,
                        },
                });
        return r;
    }
    if (ms_cont_model_id_state(m, id) != MS_CONT_ID_UNSEEN) {
        r.outcome = MS_CONT_REJECTED_DUPLICATE_ID;
        emit(e, (ms_event){
                    .type = MS_EV_ALLOC_REJECTED,
                    .data.alloc_rejected =
                        {
                            .pid = pid,
                            .id = id,
                            .reason = MS_REJECT_DUPLICATE_ID,
                        },
                });
        return r;
    }

    /* P3's policies read the holes in address order; remember which block each one is. */
    ms_cont_placement_hole *holes = malloc(m->count * sizeof *holes);
    size_t *block_of = malloc(m->count * sizeof *block_of);
    contract(holes != NULL && block_of != NULL);
    size_t h = 0;
    for (size_t i = 0; i < m->count; i++) {
        if (m->blocks[i].state == MS_BLOCK_FREE) {
            holes[h] = (ms_cont_placement_hole){m->blocks[i].start, m->blocks[i].size};
            block_of[h++] = i;
        }
    }
    ms_cont_placement_view view = {holes, h};
    ms_cont_placement_decision d = e->policy->choose(&view, size);
    size_t block = d.found ? block_of[d.hole_index] : 0;
    free(holes);
    free(block_of);
    r.scanned = (uint32_t)d.scanned;

    emit(e, (ms_event){
                .type = MS_EV_PLACEMENT_DECIDED,
                .data.placement_decided =
                    {
                        .policy = e->policy->name,
                        .requested = size,
                        .granted = size,
                        .found = d.found,
                        .hole = d.found ? (ms_hole){d.hole.start, d.hole.size} : (ms_hole){0, 0},
                        .scanned = (uint32_t)d.scanned,
                    },
            });

    if (!d.found) {
        ms_cont_frag_report fr = ms_cont_frag_report_of(m->blocks, m->count);
        ms_cont_failure_cause c = ms_cont_frag_classify_failure(m->blocks, m->count, size);
        contract(c != MS_FAIL_NONE); /* the policy missed a hole that fits */
        r.outcome = c == MS_FAIL_CAPACITY ? MS_CONT_FAILED_CAPACITY : MS_CONT_FAILED_FRAGMENTATION;
        r.total_free = fr.total_free;
        r.largest_hole = fr.largest_hole;
        ms_cont_model_set_id_state(m, id, MS_CONT_ID_FAILED);
        emit(e, (ms_event){
                    .type = MS_EV_ALLOCATION_FAILED,
                    .data.allocation_failed =
                        {
                            .pid = pid,
                            .id = id,
                            .requested = size,
                            .granted = size,
                            .cause = c == MS_FAIL_CAPACITY ? MS_ALLOC_FAIL_CAPACITY
                                                           : MS_ALLOC_FAIL_FRAGMENTATION,
                            .total_free = fr.total_free,
                            .largest_hole = fr.largest_hole,
                        },
                });
        return r;
    }
    /* Section 8: a decision naming a hole that is not free or too small breaks the contract. */
    contract(d.hole_index < h && m->blocks[block].start == d.hole.start &&
             m->blocks[block].size == d.hole.size && d.hole.size >= size);
    if (d.hole.size > size) {
        emit(e, (ms_event){
                    .type = MS_EV_BLOCK_SPLIT,
                    .data.block_split =
                        {
                            .hole = (ms_hole){d.hole.start, d.hole.size},
                            .allocated = (ms_hole){d.hole.start, size},
                            .remainder = (ms_hole){d.hole.start + size, d.hole.size - size},
                        },
                });
    }
    emit(e, (ms_event){
                .type = MS_EV_BLOCK_ALLOCATED,
                .data.block_allocated =
                    {
                        .pid = pid,
                        .id = id,
                        .start = d.hole.start,
                        .size = size,
                        .requested = size,
                    },
            });
    ms_cont_model_allocate_in(m, block, pid, id, size);
    ms_cont_model_set_id_state(m, id, MS_CONT_ID_LIVE);
    r.outcome = MS_CONT_OK;
    r.start = d.hole.start;
    return r;
}

ms_cont_result ms_cont_engine_free(ms_cont_engine *e, ms_pid pid, ms_alloc_id id) {
    e->event_count = 0;
    ms_cont_model *m = &e->model;
    ms_cont_result r = {0};
    switch (ms_cont_model_id_state(m, id)) {
    case MS_CONT_ID_UNSEEN:
        r.outcome = MS_CONT_REJECTED_UNKNOWN_ID;
        emit(e, (ms_event){
                    .type = MS_EV_FREE_REJECTED,
                    .data.free_rejected =
                        {
                            .pid = pid,
                            .id = id,
                            .reason = MS_REJECT_UNKNOWN_ID,
                        },
                });
        return r;
    case MS_CONT_ID_FREED:
        r.outcome = MS_CONT_REJECTED_DOUBLE_FREE;
        emit(e, (ms_event){
                    .type = MS_EV_FREE_REJECTED,
                    .data.free_rejected =
                        {
                            .pid = pid,
                            .id = id,
                            .reason = MS_REJECT_DOUBLE_FREE,
                        },
                });
        return r;
    case MS_CONT_ID_FAILED:
        r.outcome = MS_CONT_SKIPPED;
        emit(e, (ms_event){
                    .type = MS_EV_FREE_SKIPPED,
                    .data.free_skipped =
                        {
                            .pid = pid,
                            .id = id,
                        },
                });
        return r;
    case MS_CONT_ID_LIVE:
        break;
    }
    size_t i = ms_cont_model_find(m, id);
    contract(i < m->count);
    if (m->blocks[i].pid != pid) {
        r.outcome = MS_CONT_REJECTED_WRONG_OWNER;
        emit(e, (ms_event){
                    .type = MS_EV_FREE_REJECTED,
                    .data.free_rejected =
                        {
                            .pid = pid,
                            .id = id,
                            .reason = MS_REJECT_WRONG_OWNER,
                        },
                });
        return r;
    }
    r.start = m->blocks[i].start;
    r.size = m->blocks[i].size;
    emit_free_and_coalesce(e, i);
    ms_cont_model_free_at(m, i);
    ms_cont_model_set_id_state(m, id, MS_CONT_ID_FREED);
    r.outcome = MS_CONT_OK;
    return r;
}

ms_cont_result ms_cont_engine_terminate(ms_cont_engine *e, ms_pid pid) {
    e->event_count = 0;
    ms_cont_model *m = &e->model;
    ms_cont_result r = {.outcome = MS_CONT_OK};
    /* Address order. A free merges into i - 1 at most, so resume the scan there. */
    size_t i = 0;
    while (i < m->count) {
        if (m->blocks[i].state == MS_BLOCK_ALLOCATED && m->blocks[i].pid == pid) {
            ms_cont_model_set_id_state(m, m->blocks[i].alloc_id, MS_CONT_ID_FREED);
            emit_free_and_coalesce(e, i);
            ms_cont_model_free_at(m, i);
            r.blocks_moved++;
            i = i > 0 ? i - 1 : 0;
        } else {
            i++;
        }
    }
    emit(e, (ms_event){
                .type = MS_EV_PROCESS_TERMINATED,
                .data.process_terminated =
                    {
                        .pid = pid,
                        .released_frames = 0,
                        .freed_blocks = r.blocks_moved,
                    },
            });
    return r;
}

ms_cont_result ms_cont_engine_compact(ms_cont_engine *e) {
    e->event_count = 0;
    ms_cont_model *m = &e->model;
    ms_cont_result r = {.outcome = MS_CONT_OK};

    uint32_t holes_before = 0;
    int64_t largest_before = 0;
    for (size_t i = 0; i < m->count; i++) {
        if (m->blocks[i].state == MS_BLOCK_FREE) {
            holes_before++;
            if (m->blocks[i].size > largest_before) {
                largest_before = m->blocks[i].size;
            }
        }
    }

    emit(e, (ms_event){
                .type = MS_EV_COMPACTION_STARTED,
                .data.compaction_started =
                    {
                        .trigger = "explicit",
                        .holes_before = holes_before,
                        .largest_before = largest_before,
                    },
            });

    contract(m->count > 0); /* the model always holds at least one block */
    ms_cont_move *moves = malloc(m->count * sizeof *moves);
    contract(moves != NULL);
    size_t n = ms_cont_compaction_plan_sliding(m->blocks, m->count, moves);
    ms_cont_compaction_cost cost = ms_cont_compaction_cost_of(m->blocks, m->count, moves, n);

    for (size_t k = 0; k < n; k++) {
        size_t idx = ms_cont_model_find(m, moves[k].alloc_id);
        contract(idx < m->count && m->blocks[idx].start == moves[k].src);
        emit(e, (ms_event){
                    .type = MS_EV_BLOCK_RELOCATED,
                    .data.block_relocated =
                        {
                            .id = moves[k].alloc_id,
                            .pid = m->blocks[idx].pid,
                            .src = moves[k].src,
                            .dst = moves[k].dst,
                            .size = m->blocks[idx].size,
                        },
                });
    }

    ms_cont_model_apply_relocations(m, moves, n);
    free(moves);

    uint32_t holes_after = 0;
    int64_t largest_after = 0;
    for (size_t i = 0; i < m->count; i++) {
        if (m->blocks[i].state == MS_BLOCK_FREE) {
            holes_after++;
            if (m->blocks[i].size > largest_after) {
                largest_after = m->blocks[i].size;
            }
        }
    }

    emit(e, (ms_event){
                .type = MS_EV_COMPACTION_COMPLETED,
                .data.compaction_completed =
                    {
                        .blocks_moved = cost.blocks_moved,
                        .units_moved = cost.units_moved,
                        .holes_after = holes_after,
                        .largest_after = largest_after,
                    },
            });

    r.blocks_moved = cost.blocks_moved;
    r.units_moved = cost.units_moved;
    return r;
}

const ms_block *ms_cont_engine_blocks(const ms_cont_engine *e, size_t *count) {
    *count = e->model.count;
    return e->model.blocks;
}

const ms_event *ms_cont_engine_events(const ms_cont_engine *e, size_t *count) {
    *count = e->event_count;
    return e->events;
}

int ms_cont_engine_check(const ms_cont_engine *e) {
    return ms_cont_check_invariants(&e->model);
}
