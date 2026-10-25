/* Contiguous fragmentation. Owner: P4. */
#include "fragmentation.h"

/* floor(part * 1e6 / whole) for 0 <= part <= whole, whole > 0, without overflow. */
static uint32_t ppm_of(int64_t part, int64_t whole) {
    uint64_t p = (uint64_t)part;
    uint64_t w = (uint64_t)whole;
    return (uint32_t)(((p / w) * MS_PPM) + (((p % w) * MS_PPM) / w));
}

ms_cont_frag_report ms_cont_frag_report_of(const ms_block *blocks, size_t count) {
    ms_cont_frag_report r = {0};
    for (size_t i = 0; i < count; i++) {
        const ms_block *b = &blocks[i];
        if (b->state == MS_BLOCK_FREE) {
            r.total_free += b->size;
            r.hole_count++;
            if (b->size > r.largest_hole) {
                r.largest_hole = b->size;
            }
        } else {
            r.used += b->size;
            r.int_frag += b->size - b->requested;
            r.live_blocks++;
        }
    }
    if (r.total_free > 0) {
        r.ext_frag_ppm = ppm_of(r.total_free - r.largest_hole, r.total_free);
    }
    return r;
}

ms_cont_failure_cause ms_cont_frag_classify_failure(const ms_block *blocks, size_t count,
                                                    int64_t granted) {
    ms_cont_frag_report r = ms_cont_frag_report_of(blocks, count);
    if (r.largest_hole >= granted) {
        return MS_FAIL_NONE;
    }
    return r.total_free >= granted ? MS_FAIL_FRAGMENTATION : MS_FAIL_CAPACITY;
}

uint32_t ms_cont_frag_unusable_free_ppm(const ms_block *blocks, size_t count, int64_t s) {
    int64_t total = 0;
    int64_t unusable = 0;
    for (size_t i = 0; i < count; i++) {
        if (blocks[i].state == MS_BLOCK_FREE) {
            total += blocks[i].size;
            if (blocks[i].size < s) {
                unusable += blocks[i].size;
            }
        }
    }
    return total > 0 ? ppm_of(unusable, total) : 0;
}

const char *ms_cont_failure_cause_name(ms_cont_failure_cause cause) {
    switch (cause) {
    case MS_FAIL_NONE:
        return "NONE";
    case MS_FAIL_CAPACITY:
        return "CAPACITY";
    case MS_FAIL_FRAGMENTATION:
        return "FRAGMENTATION";
    }
    return "UNKNOWN";
}
