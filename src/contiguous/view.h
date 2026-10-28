/* MemoryView: read-only view handed to policies (private). Owner: P2.
 *
 * The block list in address order (ADR-002: a sorted array), which is exactly the
 * (const ms_block *, size_t) layout P4's fragmentation and compaction functions read. */
#ifndef MEMSIM_CONTIGUOUS_VIEW_H_
#define MEMSIM_CONTIGUOUS_VIEW_H_

#include "memsim/common/types.h"

typedef struct {
    const ms_block *blocks;
    size_t count;
    int64_t size; /* M */
} ms_cont_view;

#endif /* MEMSIM_CONTIGUOUS_VIEW_H_ */
