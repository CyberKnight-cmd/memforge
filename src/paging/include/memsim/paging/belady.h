/* Frame sweep and Belady anomaly detection (exported, README section 10). Owner: P6.
 *
 * Global replacement over one reference list, frames starting empty; `policy` is a registry name
 * ("fifo", "lru", "opt"). Each bool function returns false for an unknown policy, zero frames or
 * out of memory. */
#ifndef MEMSIM_PAGING_BELADY_H_
#define MEMSIM_PAGING_BELADY_H_

#include "memsim/common/types.h"

/* Page faults when `refs` runs against `frames` frames. */
bool ms_page_count_faults(const char *policy, const ms_page_key *refs, size_t n, uint32_t frames,
                          uint64_t *faults);

/* faults[f - 1] = page faults with f frames, for f = 1 .. max_frames. */
bool ms_page_sweep(const char *policy, const ms_page_key *refs, size_t n, uint32_t max_frames,
                   uint64_t *faults);

/* Anomaly points of a sweep: every f with faults(f + 1) > faults(f), in ascending order. `points`
 * must have room for max_frames - 1 entries. Returns how many there are. */
size_t ms_page_belady_points(const uint64_t *faults, uint32_t max_frames, uint32_t *points);

#endif /* MEMSIM_PAGING_BELADY_H_ */
