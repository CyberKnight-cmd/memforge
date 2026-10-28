/* Replacement policy: lru. Evicts the least recently used resident page. Owner: P6. */
#include "replacement.h"

#include <stdlib.h>

static void *lru_create(uint32_t frames) {
    return calloc(frames ? frames : 1, sizeof(uint64_t));
}

/* on_access follows every on_load, so stamping here covers loads too. */
static void lru_on_access(void *state, uint32_t frame, ms_page_key key, uint64_t t, bool write) {
    (void)key;
    (void)write;
    ((uint64_t *)state)[frame] = t;
}

static uint32_t lru_choose_victim(void *state, const ms_page_frame_view *view,
                                  const uint32_t *candidates, size_t n, uint64_t t) {
    (void)view;
    (void)t;
    return ms_page_pick_stamp(state, candidates, n, false);
}

const ms_page_replacement_policy ms_page_lru = {
    .name = "lru",
    .create = lru_create,
    .destroy = free,
    .on_access = lru_on_access,
    .choose_victim = lru_choose_victim,
};
