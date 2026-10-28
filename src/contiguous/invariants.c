/* Contiguous invariants. Owner: P2. */
#include "invariants.h"

int ms_cont_check_invariants(const ms_cont_model *m) {
    int64_t at = 0, used = 0, free_units = 0;
    size_t allocated = 0, live_ids = 0;
    for (size_t i = 0; i < m->count; i++) {
        const ms_block *b = &m->blocks[i];
        if (b->start != at) {
            return 1;
        }
        if (b->size <= 0) {
            return 2;
        }
        if (b->state == MS_BLOCK_FREE && i > 0 && m->blocks[i - 1].state == MS_BLOCK_FREE) {
            return 3;
        }
        bool owned = b->pid != MS_NONE && b->alloc_id != MS_NONE;
        bool unowned = b->pid == MS_NONE && b->alloc_id == MS_NONE;
        if (b->state == MS_BLOCK_ALLOCATED ? !owned : !unowned) {
            return 4;
        }
        if (b->state == MS_BLOCK_ALLOCATED) {
            /* alignment and min_split are 1, so granted == requested */
            if (b->requested != b->size) {
                return 6;
            }
            if (ms_cont_model_id_state(m, b->alloc_id) != MS_CONT_ID_LIVE) {
                return 5;
            }
            used += b->size;
            allocated++;
        } else {
            free_units += b->size;
        }
        at += b->size;
    }
    if (m->count == 0 || at != m->size) {
        return 1;
    }
    for (size_t id = 0; id < m->id_cap; id++) {
        live_ids += m->ids[id] == MS_CONT_ID_LIVE;
    }
    if (live_ids != allocated) { /* also catches one id held by two blocks */
        return 5;
    }
    if (used + free_units != m->size) {
        return 6;
    }
    return 0; /* I7 holds structurally: one state byte per id */
}
