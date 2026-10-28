/* MetricsCollector unit tests. Owner: P7. */
#include "memsim/metrics.h"
#include "test.h"

#include <string.h>

#define FREE_BLOCK(st, sz) {(st), (sz), MS_BLOCK_FREE, MS_NONE, MS_NONE, 0}
#define USED_BLOCK(st, sz, pid, id, req) {(st), (sz), MS_BLOCK_ALLOCATED, (pid), (id), (req)}

static const ms_block AC1_FIRST_FIT_BLOCKS[] = {
    FREE_BLOCK(0, 100),
    USED_BLOCK(100, 10, 0, 10, 10),
    USED_BLOCK(110, 212, 1, 21, 212),
    USED_BLOCK(322, 112, 3, 23, 112),
    FREE_BLOCK(434, 176),
    USED_BLOCK(610, 10, 0, 11, 10),
    FREE_BLOCK(620, 200),
    USED_BLOCK(820, 10, 0, 12, 10),
    FREE_BLOCK(830, 300),
    USED_BLOCK(1130, 10, 0, 13, 10),
    USED_BLOCK(1140, 417, 2, 22, 417),
    FREE_BLOCK(1557, 183),
};
#define AC1_COUNT (sizeof(AC1_FIRST_FIT_BLOCKS) / sizeof(AC1_FIRST_FIT_BLOCKS[0]))

static void test_lifecycle(void) {
    ms_metrics *m = ms_metrics_create(false);
    CHECK(m != NULL);
    ms_metrics_destroy(m);

    ms_metrics *p = ms_metrics_create(true);
    CHECK(p != NULL);
    ms_metrics_destroy(p);

    ms_metrics_destroy(NULL);
}

static void test_contiguous_empty(void) {
    ms_metrics *m = ms_metrics_create(false);
    CHECK(m != NULL);

    ms_metric out[MS_METRICS_MAX];
    size_t count = ms_metrics_summary(m, out, MS_METRICS_MAX);
    CHECK_EQ_INT(count, 19);

    const char *expected_keys[19] = {
        "alloc_requests",
        "alloc_successes",
        "alloc_failures",
        "failures_fragmentation",
        "failures_capacity",
        "rejections",
        "frees",
        "compactions",
        "blocks_moved",
        "units_moved",
        "scanned_total",
        "scanned_mean",
        "ext_frag_final",
        "ext_frag_mean",
        "hole_count_final",
        "total_free_final",
        "largest_hole_final",
        "used_final",
        "live_blocks_final",
    };

    for (size_t i = 0; i < 19; i++) {
        CHECK(strcmp(out[i].key, expected_keys[i]) == 0);
        bool expected_ratio = (strcmp(expected_keys[i], "scanned_mean") == 0 ||
                               strcmp(expected_keys[i], "ext_frag_final") == 0 ||
                               strcmp(expected_keys[i], "ext_frag_mean") == 0);
        CHECK(out[i].is_ratio == expected_ratio);
        CHECK_EQ_INT(out[i].value, 0);
    }

    ms_metrics_destroy(m);
}

static void test_contiguous_events_and_layout(void) {
    ms_metrics *m = ms_metrics_create(false);
    CHECK(m != NULL);

    /* Feed hand-built event array */
    ms_event events[] = {
        {
            .type = MS_EV_PLACEMENT_DECIDED,
            .data.placement_decided.scanned = 3,
        },
        {
            .type = MS_EV_PLACEMENT_DECIDED,
            .data.placement_decided.scanned = 7,
        },
        {
            .type = MS_EV_BLOCK_ALLOCATED,
        },
        {
            .type = MS_EV_ALLOCATION_FAILED,
            .data.allocation_failed.cause = MS_ALLOC_FAIL_FRAGMENTATION,
        },
        {
            .type = MS_EV_ALLOCATION_FAILED,
            .data.allocation_failed.cause = MS_ALLOC_FAIL_CAPACITY,
        },
        {
            .type = MS_EV_ALLOC_REJECTED,
        },
        {
            .type = MS_EV_FREE_REJECTED,
        },
        {
            .type = MS_EV_BLOCK_FREED,
        },
        {
            .type = MS_EV_COMPACTION_COMPLETED,
            .data.compaction_completed.blocks_moved = 4,
            .data.compaction_completed.units_moved = 120,
        },
    };
    size_t n_events = sizeof(events) / sizeof(events[0]);
    ms_metrics_on_events(m, events, n_events);

    /* First layout: single hole of 1000 units */
    const ms_block layout1[] = {FREE_BLOCK(0, 1000)};
    ms_metrics_on_blocks(m, layout1, 1);

    /* Second layout: AC-1 First-Fit (total_free 959, largest_hole 300, hole_count 5) */
    ms_metrics_on_blocks(m, AC1_FIRST_FIT_BLOCKS, AC1_COUNT);

    ms_metric out[MS_METRICS_MAX];
    size_t count = ms_metrics_summary(m, out, MS_METRICS_MAX);
    CHECK_EQ_INT(count, 19);

    /* Check each metric value */
    CHECK(strcmp(out[0].key, "alloc_requests") == 0);
    CHECK_EQ_INT(out[0].value, 2);
    CHECK(!out[0].is_ratio);

    CHECK(strcmp(out[1].key, "alloc_successes") == 0);
    CHECK_EQ_INT(out[1].value, 1);
    CHECK(!out[1].is_ratio);

    CHECK(strcmp(out[2].key, "alloc_failures") == 0);
    CHECK_EQ_INT(out[2].value, 2);
    CHECK(!out[2].is_ratio);

    CHECK(strcmp(out[3].key, "failures_fragmentation") == 0);
    CHECK_EQ_INT(out[3].value, 1);
    CHECK(!out[3].is_ratio);

    CHECK(strcmp(out[4].key, "failures_capacity") == 0);
    CHECK_EQ_INT(out[4].value, 1);
    CHECK(!out[4].is_ratio);

    CHECK(strcmp(out[5].key, "rejections") == 0);
    CHECK_EQ_INT(out[5].value, 2);
    CHECK(!out[5].is_ratio);

    CHECK(strcmp(out[6].key, "frees") == 0);
    CHECK_EQ_INT(out[6].value, 1);
    CHECK(!out[6].is_ratio);

    CHECK(strcmp(out[7].key, "compactions") == 0);
    CHECK_EQ_INT(out[7].value, 1);
    CHECK(!out[7].is_ratio);

    CHECK(strcmp(out[8].key, "blocks_moved") == 0);
    CHECK_EQ_INT(out[8].value, 4);
    CHECK(!out[8].is_ratio);

    CHECK(strcmp(out[9].key, "units_moved") == 0);
    CHECK_EQ_INT(out[9].value, 120);
    CHECK(!out[9].is_ratio);

    CHECK(strcmp(out[10].key, "scanned_total") == 0);
    CHECK_EQ_INT(out[10].value, 10);
    CHECK(!out[10].is_ratio);

    CHECK(strcmp(out[11].key, "scanned_mean") == 0);
    /* scanned_total = 10, alloc_requests = 2 -> 5000000 ppm */
    CHECK_EQ_INT(out[11].value, 5000000);
    CHECK(out[11].is_ratio);

    /* AC-1 numbers: ext_frag_final 687174 */
    CHECK(strcmp(out[12].key, "ext_frag_final") == 0);
    CHECK_EQ_INT(out[12].value, 687174);
    CHECK(out[12].is_ratio);

    /* ext_frag_mean: (0 + 687174) / 2 = 343587 */
    CHECK(strcmp(out[13].key, "ext_frag_mean") == 0);
    CHECK_EQ_INT(out[13].value, 343587);
    CHECK(out[13].is_ratio);

    CHECK(strcmp(out[14].key, "hole_count_final") == 0);
    CHECK_EQ_INT(out[14].value, 5);
    CHECK(!out[14].is_ratio);

    CHECK(strcmp(out[15].key, "total_free_final") == 0);
    CHECK_EQ_INT(out[15].value, 959);
    CHECK(!out[15].is_ratio);

    CHECK(strcmp(out[16].key, "largest_hole_final") == 0);
    CHECK_EQ_INT(out[16].value, 300);
    CHECK(!out[16].is_ratio);

    CHECK(strcmp(out[17].key, "used_final") == 0);
    CHECK_EQ_INT(out[17].value, 781);
    CHECK(!out[17].is_ratio);

    CHECK(strcmp(out[18].key, "live_blocks_final") == 0);
    CHECK_EQ_INT(out[18].value, 7);
    CHECK(!out[18].is_ratio);

    ms_metrics_destroy(m);
}

static void test_paging_empty(void) {
    ms_metrics *m = ms_metrics_create(true);
    CHECK(m != NULL);

    ms_metric out[MS_METRICS_MAX];
    size_t count = ms_metrics_summary(m, out, MS_METRICS_MAX);
    CHECK_EQ_INT(count, 9);

    const char *expected_keys[9] = {
        "refs",      "hits",      "faults",     "compulsory_faults", "capacity_faults",
        "hit_ratio", "evictions", "writebacks", "violations",
    };

    for (size_t i = 0; i < 9; i++) {
        CHECK(strcmp(out[i].key, expected_keys[i]) == 0);
        bool expected_ratio = (strcmp(expected_keys[i], "hit_ratio") == 0);
        CHECK(out[i].is_ratio == expected_ratio);
        CHECK_EQ_INT(out[i].value, 0);
    }

    ms_metrics_destroy(m);
}

static void test_paging_events(void) {
    ms_metrics *m = ms_metrics_create(true);
    CHECK(m != NULL);

    /* 5 hits, 15 faults (10 compulsory, 5 capacity) -> 20 refs, hit_ratio = 250000 */
    ms_event events[25];
    size_t idx = 0;
    for (size_t i = 0; i < 5; i++) {
        events[idx++] = (ms_event){.type = MS_EV_PAGE_HIT};
    }
    for (size_t i = 0; i < 10; i++) {
        events[idx++] = (ms_event){
            .type = MS_EV_PAGE_FAULT,
            .data.page_fault.kind = MS_FAULT_COMPULSORY,
        };
    }
    for (size_t i = 0; i < 5; i++) {
        events[idx++] = (ms_event){
            .type = MS_EV_PAGE_FAULT,
            .data.page_fault.kind = MS_FAULT_CAPACITY,
        };
    }
    for (size_t i = 0; i < 3; i++) {
        events[idx++] = (ms_event){.type = MS_EV_PAGE_EVICTED};
    }
    events[idx++] = (ms_event){.type = MS_EV_PAGE_WRITTEN_BACK};
    events[idx++] = (ms_event){.type = MS_EV_ACCESS_VIOLATION};

    ms_metrics_on_events(m, events, idx);

    ms_metric out[MS_METRICS_MAX];
    size_t count = ms_metrics_summary(m, out, MS_METRICS_MAX);
    CHECK_EQ_INT(count, 9);

    CHECK(strcmp(out[0].key, "refs") == 0);
    CHECK_EQ_INT(out[0].value, 20);
    CHECK(!out[0].is_ratio);

    CHECK(strcmp(out[1].key, "hits") == 0);
    CHECK_EQ_INT(out[1].value, 5);
    CHECK(!out[1].is_ratio);

    CHECK(strcmp(out[2].key, "faults") == 0);
    CHECK_EQ_INT(out[2].value, 15);
    CHECK(!out[2].is_ratio);

    CHECK(strcmp(out[3].key, "compulsory_faults") == 0);
    CHECK_EQ_INT(out[3].value, 10);
    CHECK(!out[3].is_ratio);

    CHECK(strcmp(out[4].key, "capacity_faults") == 0);
    CHECK_EQ_INT(out[4].value, 5);
    CHECK(!out[4].is_ratio);

    CHECK(strcmp(out[5].key, "hit_ratio") == 0);
    CHECK_EQ_INT(out[5].value, 250000);
    CHECK(out[5].is_ratio);

    CHECK(strcmp(out[6].key, "evictions") == 0);
    CHECK_EQ_INT(out[6].value, 3);
    CHECK(!out[6].is_ratio);

    CHECK(strcmp(out[7].key, "writebacks") == 0);
    CHECK_EQ_INT(out[7].value, 1);
    CHECK(!out[7].is_ratio);

    CHECK(strcmp(out[8].key, "violations") == 0);
    CHECK_EQ_INT(out[8].value, 1);
    CHECK(!out[8].is_ratio);

    ms_metrics_destroy(m);
}

static void test_cap_truncation(void) {
    ms_metrics *m = ms_metrics_create(false);
    CHECK(m != NULL);

    ms_metric out[MS_METRICS_MAX];
    memset(out, 0x5a, sizeof(out));

    /* Cap of 4 is smaller than contiguous count of 19 */
    size_t count = ms_metrics_summary(m, out, 4);
    CHECK_EQ_INT(count, 19);

    /* First 4 entries written */
    CHECK(strcmp(out[0].key, "alloc_requests") == 0);
    CHECK(strcmp(out[1].key, "alloc_successes") == 0);
    CHECK(strcmp(out[2].key, "alloc_failures") == 0);
    CHECK(strcmp(out[3].key, "failures_fragmentation") == 0);

    /* 5th entry must be untouched */
    unsigned char untouched[sizeof(ms_metric)];
    memset(untouched, 0x5a, sizeof(untouched));
    CHECK(memcmp(&out[4], untouched, sizeof(ms_metric)) == 0);

    /* Cap of 0 returns full count and writes nothing */
    size_t count0 = ms_metrics_summary(m, out, 0);
    CHECK_EQ_INT(count0, 19);

    /* NULL out returns full count */
    size_t count_null = ms_metrics_summary(m, NULL, 0);
    CHECK_EQ_INT(count_null, 19);

    ms_metrics_destroy(m);

    /* Test paging cap */
    ms_metrics *p = ms_metrics_create(true);
    CHECK(p != NULL);
    memset(out, 0x5a, sizeof(out));
    size_t pcount = ms_metrics_summary(p, out, 2);
    CHECK_EQ_INT(pcount, 9);
    CHECK(strcmp(out[0].key, "refs") == 0);
    CHECK(strcmp(out[1].key, "hits") == 0);
    CHECK(memcmp(&out[2], untouched, sizeof(ms_metric)) == 0);

    ms_metrics_destroy(p);
}

static void test_streamed_chunks(void) {
    ms_metrics *m = ms_metrics_create(false);
    CHECK(m != NULL);

    ms_event ev1 = {.type = MS_EV_BLOCK_ALLOCATED};
    ms_event ev2 = {.type = MS_EV_BLOCK_FREED};

    ms_metrics_on_events(m, &ev1, 1);
    ms_metrics_on_events(m, &ev1, 1);
    ms_metrics_on_events(m, &ev2, 1);

    ms_metric out[MS_METRICS_MAX];
    ms_metrics_summary(m, out, MS_METRICS_MAX);

    CHECK_EQ_INT(out[1].value, 2); /* alloc_successes */
    CHECK_EQ_INT(out[6].value, 1); /* frees */

    ms_metrics_destroy(m);
}

int main(void) {
    RUN_TEST(test_lifecycle);
    RUN_TEST(test_contiguous_empty);
    RUN_TEST(test_contiguous_events_and_layout);
    RUN_TEST(test_paging_empty);
    RUN_TEST(test_paging_events);
    RUN_TEST(test_cap_truncation);
    RUN_TEST(test_streamed_chunks);
    TEST_EXIT();
}
