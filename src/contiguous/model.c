/* Contiguous model. Owner: P2. */
#include "model.h"

#include <stdlib.h>
#include <string.h>

/* shortcut: aborts on out-of-memory and contract violations; switch to MS_ASSERT once P1 lands
 * errors.h. */
static void contract(bool ok) {
    if (!ok) {
        abort();
    }
}

static const ms_block FREE_TEMPLATE = {0, 0, MS_BLOCK_FREE, MS_NONE, MS_NONE, 0};

static void reserve_blocks(ms_cont_model *m, size_t need) {
    if (need <= m->cap) {
        return;
    }
    size_t cap = m->cap * 2 > need ? m->cap * 2 : need;
    ms_block *b = realloc(m->blocks, cap * sizeof *b);
    contract(b != NULL);
    m->blocks = b;
    m->cap = cap;
}

bool ms_cont_model_init(ms_cont_model *m, int64_t memory_size) {
    *m = (ms_cont_model){.size = memory_size};
    m->blocks = malloc(8 * sizeof *m->blocks);
    if (m->blocks == NULL) {
        return false;
    }
    m->cap = 8;
    m->blocks[0] = FREE_TEMPLATE;
    m->blocks[0].size = memory_size;
    m->count = 1;
    return true;
}

void ms_cont_model_fini(ms_cont_model *m) {
    free(m->blocks);
    free(m->ids);
    *m = (ms_cont_model){0};
}

ms_cont_view ms_cont_model_view(const ms_cont_model *m) {
    return (ms_cont_view){m->blocks, m->count, m->size};
}

ms_cont_id_state ms_cont_model_id_state(const ms_cont_model *m, ms_alloc_id id) {
    return id < m->id_cap ? (ms_cont_id_state)m->ids[id] : MS_CONT_ID_UNSEEN;
}

void ms_cont_model_set_id_state(ms_cont_model *m, ms_alloc_id id, ms_cont_id_state s) {
    contract(id != MS_NONE);
    if (id >= m->id_cap) {
        size_t cap = m->id_cap * 2 > (size_t)id + 1 ? m->id_cap * 2 : (size_t)id + 1;
        uint8_t *ids = realloc(m->ids, cap);
        contract(ids != NULL);
        memset(ids + m->id_cap, MS_CONT_ID_UNSEEN, cap - m->id_cap);
        m->ids = ids;
        m->id_cap = cap;
    }
    m->ids[id] = (uint8_t)s;
}

/* shortcut: O(n) scan; add the by_id index from section 7 if E1 runs get slow. */
size_t ms_cont_model_find(const ms_cont_model *m, ms_alloc_id id) {
    for (size_t i = 0; i < m->count; i++) {
        if (m->blocks[i].state == MS_BLOCK_ALLOCATED && m->blocks[i].alloc_id == id) {
            return i;
        }
    }
    return m->count;
}

size_t ms_cont_model_allocate_in(ms_cont_model *m, size_t i, ms_pid pid, ms_alloc_id id,
                                 int64_t size) {
    contract(i < m->count && m->blocks[i].state == MS_BLOCK_FREE && size > 0 &&
             m->blocks[i].size >= size);
    ms_block hole = m->blocks[i];
    if (hole.size > size) {
        reserve_blocks(m, m->count + 1);
        memmove(&m->blocks[i + 2], &m->blocks[i + 1], (m->count - i - 1) * sizeof *m->blocks);
        m->blocks[i + 1] = FREE_TEMPLATE;
        m->blocks[i + 1].start = hole.start + size;
        m->blocks[i + 1].size = hole.size - size;
        m->count++;
    }
    m->blocks[i] = (ms_block){hole.start, size, MS_BLOCK_ALLOCATED, pid, id, size};
    return i;
}

static void remove_at(ms_cont_model *m, size_t i) {
    memmove(&m->blocks[i], &m->blocks[i + 1], (m->count - i - 1) * sizeof *m->blocks);
    m->count--;
}

void ms_cont_model_free_at(ms_cont_model *m, size_t i) {
    contract(i < m->count && m->blocks[i].state == MS_BLOCK_ALLOCATED);
    int64_t start = m->blocks[i].start;
    int64_t size = m->blocks[i].size;
    m->blocks[i] = FREE_TEMPLATE;
    m->blocks[i].start = start;
    m->blocks[i].size = size;
    if (i + 1 < m->count && m->blocks[i + 1].state == MS_BLOCK_FREE) {
        m->blocks[i].size += m->blocks[i + 1].size;
        remove_at(m, i + 1);
    }
    if (i > 0 && m->blocks[i - 1].state == MS_BLOCK_FREE) {
        m->blocks[i - 1].size += m->blocks[i].size;
        remove_at(m, i);
    }
}

void ms_cont_model_apply_relocations(ms_cont_model *m, const ms_cont_move *moves, size_t n) {
    /* Don't trust the plan: every move must name a live block at its current start, in
     * ascending order (README section 8). */
    for (size_t k = 0; k < n; k++) {
        size_t i = ms_cont_model_find(m, moves[k].alloc_id);
        contract(i < m->count && m->blocks[i].start == moves[k].src);
        contract(k == 0 || moves[k].src > moves[k - 1].src);
    }
    size_t cap = 2 * m->count + 1; /* what preview needs */
    ms_block *out = malloc(cap * sizeof *out);
    contract(out != NULL);
    m->count = ms_cont_compaction_preview(m->blocks, m->count, moves, n, out);
    free(m->blocks);
    m->blocks = out;
    m->cap = cap;
}
