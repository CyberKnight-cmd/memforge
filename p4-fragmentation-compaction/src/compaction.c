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
        out[n++] = moved;
        if (moved.start + moved.size > end) {
            end = moved.start + moved.size;
        }
    }
    if (end < memory_size) {
        out[n++] = (ms_block){.start = end,
                              .size = memory_size - end,
                              .state = MS_BLOCK_FREE,
                              .pid = MS_NONE,
                              .alloc_id = MS_NONE,
                              .requested = 0};
    }
    return n;
}
