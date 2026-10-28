/* Replacement policy: fifo. Evicts the earliest-loaded resident page. Owner: P6. */
#include "replacement.h"

#include <stdlib.h>

static void *fifo_create(uint32_t frames) {
    return calloc(frames ? frames : 1, sizeof(uint64_t));
}

static void fifo_on_load(void *state, uint32_t frame, ms_page_key key, uint64_t t) {
    (void)key;
    ((uint64_t *)state)[frame] = t;
}

static uint32_t fifo_choose_victim(void *state, const ms_page_frame_view *view,
                                   const uint32_t *candidates, size_t n, uint64_t t) {
    (void)view;
    (void)t;
    return ms_page_pick_stamp(state, candidates, n, false);
}

const ms_page_replacement_policy ms_page_fifo = {
    .name = "fifo",
    .create = fifo_create,
    .destroy = free,
    .on_load = fifo_on_load,
    .choose_victim = fifo_choose_victim,
};
