/* ReplacementPolicy vtable (private, README section 10). Owner: P6.
 *
 * Hook order: a fault calls on_evict (when there is a victim), then on_load, then on_access; a hit
 * calls only on_access; terminate calls on_evict for each of the process's frames. `t` is 0-based
 * and counts every access op, so it indexes the list given to prepare. prepare, on_load, on_access
 * and on_evict may be NULL. describe_state and reset are cut with trace levels and Clock. */
#ifndef MEMSIM_PAGING_REPLACEMENT_H_
#define MEMSIM_PAGING_REPLACEMENT_H_

#include "memsim/common/types.h"

typedef struct ms_page_frame_view ms_page_frame_view; /* R and D bits; the engine implements it */
uint32_t ms_page_view_frames(const ms_page_frame_view *v);
ms_page_key ms_page_view_owner(const ms_page_frame_view *v, uint32_t frame);
bool ms_page_view_referenced(const ms_page_frame_view *v, uint32_t frame);
bool ms_page_view_dirty(const ms_page_frame_view *v, uint32_t frame);

typedef struct {
    const char *name;
    bool requires_lookahead; /* true only for OPT */
    void *(*create)(uint32_t frames);
    void (*destroy)(void *state);
    bool (*prepare)(void *state, const ms_page_key *refs, size_t n); /* false: out of memory */
    void (*on_load)(void *state, uint32_t frame, ms_page_key key, uint64_t t);
    void (*on_access)(void *state, uint32_t frame, ms_page_key key, uint64_t t, bool write);
    uint32_t (*choose_victim)(void *state, const ms_page_frame_view *view,
                              const uint32_t *candidates, size_t n, uint64_t t); /* in candidates */
    void (*on_evict)(void *state, uint32_t frame, ms_page_key key);
} ms_page_replacement_policy;

extern const ms_page_replacement_policy ms_page_fifo, ms_page_lru, ms_page_opt;

/* Registry: CLI name ("fifo", "lru", "opt") -> vtable, or NULL if unknown. */
const ms_page_replacement_policy *ms_page_replacement_find(const char *name);

static inline bool ms_page_key_eq(ms_page_key a, ms_page_key b) {
    return a.pid == b.pid && a.vpn == b.vpn;
}

/* Every policy keeps one stamp per frame (load time, last use or next use) and evicts the candidate
 * with the smallest or largest stamp; ties go to the lowest frame index.
 * shortcut: O(F) scan per fault instead of section 10's ring buffer or list; fine for E4's 1 to 16
 * frames, revisit if F reaches the thousands. */
static inline uint32_t ms_page_pick_stamp(const uint64_t *stamp, const uint32_t *candidates,
                                          size_t n, bool largest) {
    uint32_t best = candidates[0];
    for (size_t i = 1; i < n; i++) {
        uint32_t f = candidates[i];
        bool better = largest ? stamp[f] > stamp[best] : stamp[f] < stamp[best];
        if (better || (stamp[f] == stamp[best] && f < best)) {
            best = f;
        }
    }
    return best;
}

#endif /* MEMSIM_PAGING_REPLACEMENT_H_ */
