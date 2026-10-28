/* Contiguous model (private). Owner: P2.
 *
 * MemoryModel from README section 7: blocks tile [0, M) in address order, plus one state byte per
 * alloc id so the live, freed and failed sets are disjoint by construction (I7). Only the engine
 * calls the mutators; preconditions are the engine's job and break the contract if violated. */
#ifndef MEMSIM_CONTIGUOUS_MODEL_H_
#define MEMSIM_CONTIGUOUS_MODEL_H_

#include "compaction.h"
#include "memsim/common/types.h"
#include "view.h"

typedef enum {
    MS_CONT_ID_UNSEEN,
    MS_CONT_ID_LIVE,
    MS_CONT_ID_FREED,
    MS_CONT_ID_FAILED
} ms_cont_id_state;

typedef struct {
    int64_t size; /* M */
    ms_block *blocks;
    size_t count, cap;
    uint8_t *ids; /* ms_cont_id_state per ms_alloc_id */
    size_t id_cap;
} ms_cont_model;

/* Returns false on out-of-memory. memory_size must be > 0. */
bool ms_cont_model_init(ms_cont_model *m, int64_t memory_size);
void ms_cont_model_fini(ms_cont_model *m);

ms_cont_view ms_cont_model_view(const ms_cont_model *m);

ms_cont_id_state ms_cont_model_id_state(const ms_cont_model *m, ms_alloc_id id);
void ms_cont_model_set_id_state(ms_cont_model *m, ms_alloc_id id, ms_cont_id_state s);

/* Index of the ALLOCATED block holding id, or m->count if none. */
size_t ms_cont_model_find(const ms_cont_model *m, ms_alloc_id id);

/* Places `size` units at the low end of the FREE block at index i (which must fit it) and splits
 * off the remainder. Returns the index of the new ALLOCATED block (always i). */
size_t ms_cont_model_allocate_in(ms_cont_model *m, size_t i, ms_pid pid, ms_alloc_id id,
                                 int64_t size);

/* Frees the ALLOCATED block at index i and merges it with FREE neighbours (I3). */
void ms_cont_model_free_at(ms_cont_model *m, size_t i);

/* Applies a compaction plan. Each move's src must be its block's current start. */
void ms_cont_model_apply_relocations(ms_cont_model *m, const ms_cont_move *moves, size_t n);

#endif /* MEMSIM_CONTIGUOUS_MODEL_H_ */
