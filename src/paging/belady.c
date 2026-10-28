/* Belady sweep. Owner: P6. */
#include "memsim/paging/belady.h"

#include "replacement.h"

#include <stdlib.h>

/* Same hook order as the engine. Frames fill from 0 upward and are never freed mid-run, so the
 * lowest free frame is always `used`.
 * shortcut: drives the policy through this built-in frame table with a NULL frame view (FIFO, LRU
 * and OPT never read it) because PagingEngine is still a stub; switch to P5's engine when it
 * merges. */
/* All frames are full: ask the policy for a victim and tell it the old page leaves. */
static uint32_t evict(const ms_page_replacement_policy *p, void *state, const ms_page_key *owner,
                      uint32_t *candidates, uint32_t frames, uint64_t t) {
    for (uint32_t i = 0; i < frames; i++) {
        candidates[i] = i;
    }
    uint32_t f = p->choose_victim(state, NULL, candidates, frames, t);
    if (p->on_evict) {
        p->on_evict(state, f, owner[f]);
    }
    return f;
}

static bool run(const ms_page_replacement_policy *p, const ms_page_key *refs, size_t n,
                uint32_t frames, uint64_t *faults) {
    void *state = p->create(frames);
    ms_page_key *owner = calloc(frames, sizeof *owner);
    uint32_t *candidates = malloc(frames * sizeof *candidates);
    bool ok = state && owner && candidates && (!p->prepare || p->prepare(state, refs, n));
    uint32_t used = 0;
    *faults = 0;
    for (size_t t = 0; ok && t < n; t++) {
        uint32_t f = 0;
        while (f < used && !ms_page_key_eq(owner[f], refs[t])) {
            f++;
        }
        if (f == used) {
            ++*faults;
            if (used < frames) {
                used++;
            } else {
                f = evict(p, state, owner, candidates, frames, t);
            }
            owner[f] = refs[t];
            if (p->on_load) {
                p->on_load(state, f, refs[t], t);
            }
        }
        if (p->on_access) {
            p->on_access(state, f, refs[t], t, false);
        }
    }
    if (state) {
        p->destroy(state);
    }
    free(owner);
    free(candidates);
    return ok;
}

bool ms_page_count_faults(const char *policy, const ms_page_key *refs, size_t n, uint32_t frames,
                          uint64_t *faults) {
    const ms_page_replacement_policy *p = ms_page_replacement_find(policy);
    return p && frames > 0 && run(p, refs, n, frames, faults);
}

bool ms_page_sweep(const char *policy, const ms_page_key *refs, size_t n, uint32_t max_frames,
                   uint64_t *faults) {
    bool ok = max_frames > 0;
    for (uint32_t f = 1; ok && f <= max_frames; f++) {
        ok = ms_page_count_faults(policy, refs, n, f, &faults[f - 1]);
    }
    return ok;
}

size_t ms_page_belady_points(const uint64_t *faults, uint32_t max_frames, uint32_t *points) {
    size_t count = 0;
    for (uint32_t f = 1; f < max_frames; f++) {
        if (faults[f] > faults[f - 1]) {
            points[count++] = f;
        }
    }
    return count;
}
