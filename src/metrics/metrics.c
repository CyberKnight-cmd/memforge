/* MetricsCollector. Owner: P7. */
#include "memsim/metrics.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define CONTIGUOUS_METRIC_COUNT 19
#define PAGING_METRIC_COUNT 9
#define PPM_SCALE 1000000ULL

struct ms_metrics {
    bool paging;

    /* Contiguous metrics from events */
    int64_t alloc_requests;
    int64_t alloc_successes;
    int64_t alloc_failures;
    int64_t failures_fragmentation;
    int64_t failures_capacity;
    int64_t rejections;
    int64_t frees;
    int64_t compactions;
    int64_t blocks_moved;
    int64_t units_moved;
    int64_t scanned_total;

    /* Contiguous metrics from layouts (on_blocks) */
    int64_t ext_frag_final;
    int64_t ext_frag_sum;
    int64_t on_blocks_calls;
    int64_t hole_count_final;
    int64_t total_free_final;
    int64_t largest_hole_final;
    int64_t used_final;
    int64_t live_blocks_final;

    /* Paging metrics from events */
    int64_t hits;
    int64_t faults;
    int64_t compulsory_faults;
    int64_t capacity_faults;
    int64_t evictions;
    int64_t writebacks;
    int64_t violations;
};

static int64_t calc_ppm(int64_t part, int64_t whole) {
    if (whole <= 0 || part <= 0) {
        return 0;
    }
    uint64_t p = (uint64_t)part;
    uint64_t w = (uint64_t)whole;
    uint64_t q = p / w;
    uint64_t r = p % w;
    return (int64_t)((q * PPM_SCALE) + ((r * PPM_SCALE) / w));
}

ms_metrics *ms_metrics_create(bool paging) {
    ms_metrics *m = calloc(1, sizeof(*m));
    if (m == NULL) {
        return NULL;
    }
    m->paging = paging;
    return m;
}

void ms_metrics_destroy(ms_metrics *m) {
    free(m);
}

void ms_metrics_on_events(ms_metrics *m, const ms_event *events, size_t n) {
    if (m == NULL || events == NULL) {
        return;
    }
    for (size_t i = 0; i < n; i++) {
        const ms_event *ev = &events[i];
        if (m->paging) {
            switch (ev->type) {
            case MS_EV_PAGE_HIT:
                m->hits++;
                break;
            case MS_EV_PAGE_FAULT:
                m->faults++;
                if (ev->data.page_fault.kind == MS_FAULT_COMPULSORY) {
                    m->compulsory_faults++;
                } else if (ev->data.page_fault.kind == MS_FAULT_CAPACITY) {
                    m->capacity_faults++;
                }
                break;
            case MS_EV_PAGE_EVICTED:
                m->evictions++;
                break;
            case MS_EV_PAGE_WRITTEN_BACK:
                m->writebacks++;
                break;
            case MS_EV_ACCESS_VIOLATION:
                m->violations++;
                break;
            default:
                break;
            }
        } else {
            switch (ev->type) {
            case MS_EV_PLACEMENT_DECIDED:
                m->alloc_requests++;
                m->scanned_total += (int64_t)ev->data.placement_decided.scanned;
                break;
            case MS_EV_BLOCK_ALLOCATED:
                m->alloc_successes++;
                break;
            case MS_EV_ALLOCATION_FAILED:
                m->alloc_failures++;
                if (ev->data.allocation_failed.cause == MS_ALLOC_FAIL_FRAGMENTATION) {
                    m->failures_fragmentation++;
                } else if (ev->data.allocation_failed.cause == MS_ALLOC_FAIL_CAPACITY) {
                    m->failures_capacity++;
                }
                break;
            case MS_EV_ALLOC_REJECTED:
            case MS_EV_FREE_REJECTED:
                m->rejections++;
                break;
            case MS_EV_BLOCK_FREED:
                m->frees++;
                break;
            case MS_EV_COMPACTION_COMPLETED:
                m->compactions++;
                m->blocks_moved += (int64_t)ev->data.compaction_completed.blocks_moved;
                m->units_moved += ev->data.compaction_completed.units_moved;
                break;
            default:
                break;
            }
        }
    }
}

void ms_metrics_on_blocks(ms_metrics *m, const ms_block *blocks, size_t n) {
    if (m == NULL || m->paging) {
        return;
    }
    if (blocks == NULL && n > 0) {
        return;
    }
    int64_t total_free = 0;
    int64_t largest_hole = 0;
    int64_t hole_count = 0;
    int64_t used = 0;
    int64_t live_blocks = 0;

    for (size_t i = 0; i < n; i++) {
        if (blocks[i].state == MS_BLOCK_FREE) {
            total_free += blocks[i].size;
            hole_count++;
            if (blocks[i].size > largest_hole) {
                largest_hole = blocks[i].size;
            }
        } else if (blocks[i].state == MS_BLOCK_ALLOCATED) {
            used += blocks[i].size;
            live_blocks++;
        }
    }

    int64_t ext_frag = calc_ppm(total_free - largest_hole, total_free);

    m->hole_count_final = hole_count;
    m->total_free_final = total_free;
    m->largest_hole_final = largest_hole;
    m->used_final = used;
    m->live_blocks_final = live_blocks;
    m->ext_frag_final = ext_frag;

    m->ext_frag_sum += ext_frag;
    m->on_blocks_calls++;
}

size_t ms_metrics_summary(const ms_metrics *m, ms_metric *out, size_t cap) {
    if (m == NULL) {
        return 0;
    }

    if (m->paging) {
        int64_t refs = m->hits + m->faults;
        ms_metric items[PAGING_METRIC_COUNT] = {
            {"refs", false, refs},
            {"hits", false, m->hits},
            {"faults", false, m->faults},
            {"compulsory_faults", false, m->compulsory_faults},
            {"capacity_faults", false, m->capacity_faults},
            {"hit_ratio", true, calc_ppm(m->hits, refs)},
            {"evictions", false, m->evictions},
            {"writebacks", false, m->writebacks},
            {"violations", false, m->violations},
        };

        size_t count = PAGING_METRIC_COUNT;
        size_t to_write = 0;
        if (out != NULL) {
            to_write = cap < count ? cap : count;
        }
        if (to_write > 0) {
            memcpy(out, items, to_write * sizeof(*out));
        }
        return count;
    }

    int64_t scanned_mean = calc_ppm(m->scanned_total, m->alloc_requests);
    int64_t ext_frag_mean =
        m->on_blocks_calls == 0 ? 0 : m->ext_frag_sum / (int64_t)m->on_blocks_calls;
    ms_metric items[CONTIGUOUS_METRIC_COUNT] = {
        {"alloc_requests", false, m->alloc_requests},
        {"alloc_successes", false, m->alloc_successes},
        {"alloc_failures", false, m->alloc_failures},
        {"failures_fragmentation", false, m->failures_fragmentation},
        {"failures_capacity", false, m->failures_capacity},
        {"rejections", false, m->rejections},
        {"frees", false, m->frees},
        {"compactions", false, m->compactions},
        {"blocks_moved", false, m->blocks_moved},
        {"units_moved", false, m->units_moved},
        {"scanned_total", false, m->scanned_total},
        {"scanned_mean", true, scanned_mean},
        {"ext_frag_final", true, m->ext_frag_final},
        {"ext_frag_mean", true, ext_frag_mean},
        {"hole_count_final", false, m->hole_count_final},
        {"total_free_final", false, m->total_free_final},
        {"largest_hole_final", false, m->largest_hole_final},
        {"used_final", false, m->used_final},
        {"live_blocks_final", false, m->live_blocks_final},
    };

    size_t count = CONTIGUOUS_METRIC_COUNT;
    size_t to_write = 0;
    if (out != NULL) {
        to_write = cap < count ? cap : count;
    }
    if (to_write > 0) {
        memcpy(out, items, to_write * sizeof(*out));
    }
    return count;
}
