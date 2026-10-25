/* Contiguous compaction (private). Owner: P4.
 *
 * CompactionStrategy from README section 8. Planning is pure: a strategy reads the layout and
 * returns moves; only P2's apply_relocations changes the model, and the engine emits one
 * BlockRelocated per move, so compaction is never silent (ADR-011). */
#ifndef MEMSIM_CONTIGUOUS_COMPACTION_H_
#define MEMSIM_CONTIGUOUS_COMPACTION_H_

#include "memsim/common/types.h"
#include "p4_core_types.h"

typedef struct {
    ms_alloc_id alloc_id;
    int64_t src; /* old start */
    int64_t dst; /* new start */
} ms_cont_move;

typedef struct {
    uint32_t blocks_moved;
    int64_t units_moved; /* sum of the sizes of the moved blocks */
} ms_cont_compaction_cost;

/* Sliding compaction (v1): visit ALLOCATED blocks in address order and slide each to the end of
 * the previous one. Writes at most one move per ALLOCATED block into out and returns the count.
 * Blocks already in place produce no move. Applying the moves in the returned (ascending) order
 * never overwrites a block that has not moved yet. */
size_t ms_cont_compaction_plan_sliding(const ms_block *blocks, size_t count, ms_cont_move *out);

ms_cont_compaction_cost ms_cont_compaction_cost_of(const ms_block *blocks, size_t count,
                                                   const ms_cont_move *moves, size_t move_count);

/* Simulates a plan without touching the model: writes the post-compaction layout into out
 * (ALLOCATED blocks at their new starts, then one FREE block for the rest of memory, if any) and
 * returns its block count. out needs room for count + 1 blocks. Used by tests, the viewer's
 * "what if" and the report; the real move is still P2's apply_relocations. */
size_t ms_cont_compaction_preview(const ms_block *blocks, size_t count, const ms_cont_move *moves,
                                  size_t move_count, ms_block *out);

#endif /* MEMSIM_CONTIGUOUS_COMPACTION_H_ */
