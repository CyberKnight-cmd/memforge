/* Contiguous compaction. Owner: P4. */
#include "compaction.h"

size_t ms_cont_compaction_plan_sliding(const ms_block *blocks, size_t count, ms_cont_move *out) {
    size_t n = 0;
    int64_t cursor = 0; /* end of the last block already packed at the bottom */
    for (size_t i = 0; i < count; i++) {
        const ms_block *b = &blocks[i];
        if (b->state != MS_BLOCK_ALLOCATED) {
            continue;
        }
        if (b->start != cursor) {
            out[n++] = (ms_cont_move){.alloc_id = b->alloc_id, .src = b->start, .dst = cursor};
        }
        cursor += b->size;
    }
    return n;
}

/* Packs the ALLOCATED blocks of blocks[first..last] down to blocks[first].start. */
static size_t pack_run(const ms_block *blocks, size_t first, size_t last, ms_cont_move *out) {
    size_t n = 0;
    int64_t cursor = blocks[first].start;
    for (size_t i = first; i <= last; i++) {
        const ms_block *b = &blocks[i];
        if (b->state != MS_BLOCK_ALLOCATED) {
            continue;
        }
        if (b->start != cursor) {
            out[n++] = (ms_cont_move){.alloc_id = b->alloc_id, .src = b->start, .dst = cursor};
        }
        cursor += b->size;
    }
    return n;
}

size_t ms_cont_compaction_plan_window(const ms_block *blocks, size_t count, int64_t need,
                                      ms_cont_move *out) {
    for (size_t i = 0; i < count; i++) {
        if (blocks[i].state == MS_BLOCK_FREE && blocks[i].size >= need) {
            return 0; /* a hole is already large enough */
        }
    }
    bool found = false;
    size_t best_first = 0, best_last = 0;
    int64_t best_units = 0;
    uint32_t best_blocks = 0;
    for (size_t first = 0; first < count; first++) {
        int64_t free_units = 0, units = 0, cursor = blocks[first].start;
        uint32_t moved = 0;
        for (size_t last = first; last < count; last++) {
            const ms_block *b = &blocks[last];
            if (b->state == MS_BLOCK_FREE) {
                free_units += b->size;
            } else {
                if (b->start != cursor) {
                    units += b->size;
                    moved++;
                }
                cursor += b->size;
            }
            if (found && (units > best_units || (units == best_units && moved >= best_blocks))) {
                break; /* extending this run can only cost more */
            }
            if (free_units >= need) {
                found = true;
                best_first = first;
                best_last = last;
                best_units = units;
                best_blocks = moved;
                break;
            }
        }
    }
    return found ? pack_run(blocks, best_first, best_last, out) : 0;
}

static const ms_block *find_by_id(const ms_block *blocks, size_t count, ms_alloc_id id) {
    for (size_t i = 0; i < count; i++) {
        if (blocks[i].state == MS_BLOCK_ALLOCATED && blocks[i].alloc_id == id) {
            return &blocks[i];
        }
    }
    return NULL;
}

ms_cont_compaction_cost ms_cont_compaction_cost_of(const ms_block *blocks, size_t count,
                                                   const ms_cont_move *moves, size_t move_count) {
    ms_cont_compaction_cost cost = {0};
    for (size_t i = 0; i < move_count; i++) {
        const ms_block *b = find_by_id(blocks, count, moves[i].alloc_id);
        if (b != NULL) {
            cost.blocks_moved++;
            cost.units_moved += b->size;
        }
    }
    return cost;
}

static size_t emit_free(ms_block *out, size_t n, int64_t start, int64_t end) {
    if (end > start) {
        out[n++] = (ms_block){.start = start,
                              .size = end - start,
                              .state = MS_BLOCK_FREE,
                              .pid = MS_NONE,
                              .alloc_id = MS_NONE,
                              .requested = 0};
    }
    return n;
}

size_t ms_cont_compaction_preview(const ms_block *blocks, size_t count, const ms_cont_move *moves,
                                  size_t move_count, ms_block *out) {
    size_t n = 0;
    int64_t memory_size = 0, end = 0;
    for (size_t i = 0; i < count; i++) {
        const ms_block *b = &blocks[i];
        if (b->start + b->size > memory_size) {
            memory_size = b->start + b->size;
        }
        if (b->state != MS_BLOCK_ALLOCATED) {
            continue;
        }
        ms_block moved = *b;
        for (size_t m = 0; m < move_count; m++) {
            if (moves[m].alloc_id == b->alloc_id) {
                moved.start = moves[m].dst;
                break;
            }
        }
        n = emit_free(out, n, end, moved.start);
        out[n++] = moved;
        end = moved.start + moved.size;
    }
    return emit_free(out, n, end, memory_size);
}
