/* ContiguousEngine: the module's exported interface. Owner: P2.
 *
 * The engine is the model's only mutator (README section 7). Every op returns its domain outcome;
 * rejected and failed ops leave the layout unchanged. Events are not emitted yet: the result
 * carries the fields section 11 needs until P1 lands events.h. */
#ifndef MEMSIM_CONTIGUOUS_ENGINE_H_
#define MEMSIM_CONTIGUOUS_ENGINE_H_

#include "memsim/common/types.h"

typedef struct ms_cont_engine ms_cont_engine; /* opaque */

typedef enum {
    MS_CONT_OK,
    MS_CONT_FAILED_CAPACITY,      /* total free < size */
    MS_CONT_FAILED_FRAGMENTATION, /* enough free memory, no single hole fits */
    MS_CONT_REJECTED_INVALID_SIZE,
    MS_CONT_REJECTED_DUPLICATE_ID,
    MS_CONT_REJECTED_UNKNOWN_ID,
    MS_CONT_REJECTED_DOUBLE_FREE,
    MS_CONT_REJECTED_WRONG_OWNER,
    MS_CONT_SKIPPED, /* free of an id whose allocation failed under this policy (ADR-003) */
} ms_cont_outcome;

typedef struct {
    ms_cont_outcome outcome;
    int64_t start, size;   /* alloc: the placed block; free: the freed block */
    uint32_t scanned;      /* alloc: holes the policy examined */
    int64_t total_free;    /* alloc failures: for AllocationFailed */
    int64_t largest_hole;  /* alloc failures: for AllocationFailed */
    uint32_t blocks_moved; /* compact; terminate: blocks freed */
    int64_t units_moved;   /* compact */
} ms_cont_result;

/* policy is "first-fit", "best-fit" or "worst-fit". Returns NULL for an unknown policy, a
 * non-positive memory_size, or out-of-memory. */
ms_cont_engine *ms_cont_engine_create(int64_t memory_size, const char *policy);
void ms_cont_engine_destroy(ms_cont_engine *e);

ms_cont_result ms_cont_engine_alloc(ms_cont_engine *e, ms_pid pid, ms_alloc_id id, int64_t size);
ms_cont_result ms_cont_engine_free(ms_cont_engine *e, ms_pid pid, ms_alloc_id id);
ms_cont_result ms_cont_engine_terminate(ms_cont_engine *e, ms_pid pid);
ms_cont_result ms_cont_engine_compact(ms_cont_engine *e);

/* The layout in address order, valid until the next op. */
const ms_block *ms_cont_engine_blocks(const ms_cont_engine *e, size_t *count);

/* Checked mode: 0 if invariants I1 to I7 hold, else the first broken one. */
int ms_cont_engine_check(const ms_cont_engine *e);

#endif /* MEMSIM_CONTIGUOUS_ENGINE_H_ */
