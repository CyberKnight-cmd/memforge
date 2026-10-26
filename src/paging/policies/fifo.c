/* Replacement policy: FIFO. Owner: P6. */
#include "replacement.h"

static size_t ms_page_fifo_choose(const uint32_t *frames, const size_t *loaded_at,
                                  const size_t *last_used, size_t frame_count,
                                  const uint32_t *references, size_t reference_count,
                                  size_t next_reference)
{
    size_t victim = 0U;
    (void)frames; (void)last_used; (void)references; (void)reference_count;
    (void)next_reference;
    for (size_t i = 1U; i < frame_count; ++i) {
        if (loaded_at[i] < loaded_at[victim]) victim = i;
    }
    return victim;
}

const ms_page_policy_vtable *ms_page_fifo_vtable(void)
{
    static const ms_page_policy_vtable policy = {
        MS_PAGE_FIFO, "FIFO", ms_page_fifo_choose
    };
    return &policy;
}
