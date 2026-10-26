/* Shared replacement simulation driver. Owner: P6. */
#include <stdlib.h>
#include <string.h>
#include "replacement.h"

static const ms_page_policy_vtable *ms_page_get_vtable(ms_page_policy policy)
{
    switch (policy) {
    case MS_PAGE_FIFO: return ms_page_fifo_vtable();
    case MS_PAGE_LRU: return ms_page_lru_vtable();
    case MS_PAGE_OPT: return ms_page_opt_vtable();
    default: return NULL;
    }
}

const char *ms_page_policy_name(ms_page_policy policy)
{
    const ms_page_policy_vtable *vtable = ms_page_get_vtable(policy);
    return vtable == NULL ? "unknown" : vtable->name;
}

bool ms_page_simulate(ms_page_policy policy, const uint32_t *references,
                      size_t reference_count, size_t frame_count,
                      ms_page_result *result)
{
    if (result == NULL) return false;
    memset(result, 0, sizeof(*result));
    const ms_page_policy_vtable *vtable = ms_page_get_vtable(policy);
    if (vtable == NULL || frame_count == 0U ||
        (references == NULL && reference_count != 0U)) return false;
    if (reference_count > SIZE_MAX / sizeof(*result->steps) ||
        frame_count > SIZE_MAX / sizeof(uint32_t) ||
        frame_count > SIZE_MAX / sizeof(size_t)) return false;

    result->steps = reference_count == 0U ? NULL : calloc(reference_count, sizeof(*result->steps));
    uint32_t *frames = calloc(frame_count, sizeof(*frames));
    bool *occupied = calloc(frame_count, sizeof(*occupied));
    size_t *loaded_at = calloc(frame_count, sizeof(*loaded_at));
    size_t *last_used = calloc(frame_count, sizeof(*last_used));
    if ((reference_count != 0U && result->steps == NULL) || frames == NULL ||
        occupied == NULL || loaded_at == NULL || last_used == NULL) {
        free(result->steps); free(frames); free(occupied); free(loaded_at); free(last_used);
        memset(result, 0, sizeof(*result));
        return false;
    }

    result->reference_count = reference_count;
    result->frame_count = frame_count;
    for (size_t r = 0U; r < reference_count; ++r) {
        size_t slot = frame_count;
        for (size_t i = 0U; i < frame_count; ++i) {
            if (occupied[i] && frames[i] == references[r]) { slot = i; break; }
        }
        ms_page_step *step = &result->steps[r];
        step->page = references[r];
        if (slot != frame_count) {
            step->hit = true;
            ++result->hits;
        } else {
            ++result->faults;
            for (size_t i = 0U; i < frame_count; ++i) {
                if (!occupied[i]) { slot = i; break; }
            }
            if (slot == frame_count) {
                slot = vtable->choose_victim(frames, loaded_at, last_used, frame_count,
                                             references, reference_count, r + 1U);
                step->evicted = true;
                step->victim_page = frames[slot];
                ++result->evictions;
            }
            frames[slot] = references[r];
            occupied[slot] = true;
            loaded_at[slot] = r;
        }
        last_used[slot] = r;
        step->frame_index = slot;
    }
    free(frames); free(occupied); free(loaded_at); free(last_used);
    return true;
}

void ms_page_result_destroy(ms_page_result *result)
{
    if (result == NULL) return;
    free(result->steps);
    memset(result, 0, sizeof(*result));
}
