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
    return e;
}

void ms_cont_engine_destroy(ms_cont_engine *e) {
    if (e != NULL) {
        ms_cont_model_fini(&e->model);
        free(e);
    }
}

ms_cont_result ms_cont_engine_alloc(ms_cont_engine *e, ms_pid pid, ms_alloc_id id, int64_t size) {
    ms_cont_model *m = &e->model;
    ms_cont_result r = {.size = size};
    if (size <= 0 || id == MS_NONE) { /* section 7 has no INVALID_ID reason */
        r.outcome = MS_CONT_REJECTED_INVALID_SIZE;
        return r;
    }
    if (ms_cont_model_id_state(m, id) != MS_CONT_ID_UNSEEN) {
        r.outcome = MS_CONT_REJECTED_DUPLICATE_ID;
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

    if (!d.found) {
        ms_cont_frag_report fr = ms_cont_frag_report_of(m->blocks, m->count);
        ms_cont_failure_cause c = ms_cont_frag_classify_failure(m->blocks, m->count, size);
        contract(c != MS_FAIL_NONE); /* the policy missed a hole that fits */
        r.outcome = c == MS_FAIL_CAPACITY ? MS_CONT_FAILED_CAPACITY : MS_CONT_FAILED_FRAGMENTATION;
        r.total_free = fr.total_free;
        r.largest_hole = fr.largest_hole;
        ms_cont_model_set_id_state(m, id, MS_CONT_ID_FAILED);
        return r;
    }
    /* Section 8: a decision naming a hole that is not free or too small breaks the contract. */
    contract(d.hole_index < h && m->blocks[block].start == d.hole.start &&
             m->blocks[block].size == d.hole.size && d.hole.size >= size);
    ms_cont_model_allocate_in(m, block, pid, id, size);
    ms_cont_model_set_id_state(m, id, MS_CONT_ID_LIVE);
    r.outcome = MS_CONT_OK;
    r.start = d.hole.start;
    return r;
}

ms_cont_result ms_cont_engine_free(ms_cont_engine *e, ms_pid pid, ms_alloc_id id) {
    ms_cont_model *m = &e->model;
    ms_cont_result r = {0};
    switch (ms_cont_model_id_state(m, id)) {
    case MS_CONT_ID_UNSEEN:
        r.outcome = MS_CONT_REJECTED_UNKNOWN_ID;
        return r;
    case MS_CONT_ID_FREED:
        r.outcome = MS_CONT_REJECTED_DOUBLE_FREE;
        return r;
    case MS_CONT_ID_FAILED:
        r.outcome = MS_CONT_SKIPPED;
        return r;
    case MS_CONT_ID_LIVE:
        break;
    }
    size_t i = ms_cont_model_find(m, id);
    contract(i < m->count);
    if (m->blocks[i].pid != pid) {
        r.outcome = MS_CONT_REJECTED_WRONG_OWNER;
        return r;
    }
    r.start = m->blocks[i].start;
    r.size = m->blocks[i].size;
    ms_cont_model_free_at(m, i);
    ms_cont_model_set_id_state(m, id, MS_CONT_ID_FREED);
    r.outcome = MS_CONT_OK;
    return r;
}

ms_cont_result ms_cont_engine_terminate(ms_cont_engine *e, ms_pid pid) {
    ms_cont_model *m = &e->model;
    ms_cont_result r = {.outcome = MS_CONT_OK};
    /* Address order. A free merges into i - 1 at most, so resume the scan there. */
    size_t i = 0;
    while (i < m->count) {
        if (m->blocks[i].state == MS_BLOCK_ALLOCATED && m->blocks[i].pid == pid) {
            ms_cont_model_set_id_state(m, m->blocks[i].alloc_id, MS_CONT_ID_FREED);
            ms_cont_model_free_at(m, i);
            r.blocks_moved++;
            i = i > 0 ? i - 1 : 0;
        } else {
            i++;
        }
    }
    return r;
}

ms_cont_result ms_cont_engine_compact(ms_cont_engine *e) {
    ms_cont_model *m = &e->model;
    ms_cont_result r = {.outcome = MS_CONT_OK};
    ms_cont_move *moves = malloc(m->count * sizeof *moves);
    contract(moves != NULL);
    size_t n = ms_cont_compaction_plan_sliding(m->blocks, m->count, moves);
    ms_cont_compaction_cost cost = ms_cont_compaction_cost_of(m->blocks, m->count, moves, n);
    ms_cont_model_apply_relocations(m, moves, n);
    free(moves);
    r.blocks_moved = cost.blocks_moved;
    r.units_moved = cost.units_moved;
    return r;
}

const ms_block *ms_cont_engine_blocks(const ms_cont_engine *e, size_t *count) {
    *count = e->model.count;
    return e->model.blocks;
}

int ms_cont_engine_check(const ms_cont_engine *e) {
    return ms_cont_check_invariants(&e->model);
}
