/* Temporary copies of the README section 6 core types that P4 needs (private). Owner: P4.
 *
 * TODO(P4): delete this file and its includes once P1 merges the real definitions into
 * memsim/common/types.h. The definitions below are copied verbatim from docs/ARCHITECTURE.md,
 * so the swap is a one-line include change. */
#ifndef MEMSIM_CONTIGUOUS_P4_CORE_TYPES_H_
#define MEMSIM_CONTIGUOUS_P4_CORE_TYPES_H_

#include "memsim/common/types.h"

#ifndef MS_NONE
typedef uint32_t ms_pid;      /* interned "P1", "SYS" */
typedef uint32_t ms_alloc_id; /* interned, workload-assigned, never reused */
#define MS_NONE UINT32_MAX    /* no pid / no id */

typedef struct {
    int64_t start, size;
} ms_hole; /* half-open [start, start + size) */
static inline int64_t ms_hole_end(ms_hole h) {
    return h.start + h.size;
}

typedef enum { MS_BLOCK_FREE, MS_BLOCK_ALLOCATED } ms_block_state;

typedef struct {
    int64_t start; /* changes only through compaction */
    int64_t size;  /* granted size, >= requested */
    ms_block_state state;
    ms_pid pid;           /* MS_NONE iff FREE */
    ms_alloc_id alloc_id; /* MS_NONE iff FREE */
    int64_t requested;    /* internal fragmentation = size - requested */
} ms_block;
#endif

#endif /* MEMSIM_CONTIGUOUS_P4_CORE_TYPES_H_ */
