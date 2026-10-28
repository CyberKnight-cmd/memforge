/* Paging frames (private). Owner: P5. */
#ifndef MEMSIM_PAGING_FRAMES_H_
#define MEMSIM_PAGING_FRAMES_H_

#include "memsim/common/types.h"

/* Inverted frame table: frame -> (pid, vpn), pid MS_NONE when free. Free frames are those with
   no owner, so the pool and the occupied frames are disjoint by construction (V2). */
typedef struct {
    ms_page_key *owner;
    uint32_t n;
} ms_page_frames;

/* shortcut: O(F) scan instead of section 9's min-heap; same order, switch if F grows past ~1e4. */
static inline uint32_t ms_page_frames_lowest_free(const ms_page_frames *f) {
    for (uint32_t i = 0; i < f->n; i++) {
        if (f->owner[i].pid == MS_NONE) {
            return i;
        }
    }
    return MS_NONE;
}

#endif /* MEMSIM_PAGING_FRAMES_H_ */
