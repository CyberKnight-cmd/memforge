/* Replacement policy: OPT (Belady's optimal algorithm). Owner: P6. */
#include "replacement.h"

static size_t ms_page_opt_choose(const uint32_t *frames, const size_t *loaded_at,
                                 const size_t *last_used, size_t frame_count,
                                 const uint32_t *references, size_t reference_count,
                                 size_t next_reference)
{
    size_t victim = 0U;
    size_t farthest = 0U;
    (void)loaded_at; (void)last_used;
    for (size_t i = 0U; i < frame_count; ++i) {
        size_t next = next_reference;
        while (next < reference_count && references[next] != frames[i]) ++next;
        if (next == reference_count) return i;
        if (i == 0U || next > farthest) {
            farthest = next;
            victim = i;
        }
    }
    return victim;
}

const ms_page_policy_vtable *ms_page_opt_vtable(void)
{
    static const ms_page_policy_vtable policy = {
        MS_PAGE_OPT, "OPT", ms_page_opt_choose
    };
    return &policy;
}
