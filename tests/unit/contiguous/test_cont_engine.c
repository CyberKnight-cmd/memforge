/* ContiguousEngine unit tests: AC-1, AC-2, AC-5 and every domain outcome, with invariants
 * checked after every op. Owner: P2. */
#include "memsim/contiguous/engine.h"
#include "test.h"

#include <string.h>

enum { SYS = 0, P1 = 1, P2 = 2, P3 = 3, P4 = 4 };

static ms_cont_result alloc(ms_cont_engine *e, ms_pid pid, ms_alloc_id id, int64_t size) {
    ms_cont_result r = ms_cont_engine_alloc(e, pid, id, size);
    CHECK_EQ_INT(ms_cont_engine_check(e), 0);
    return r;
}

static ms_cont_result release(ms_cont_engine *e, ms_pid pid, ms_alloc_id id) {
    ms_cont_result r = ms_cont_engine_free(e, pid, id);
    CHECK_EQ_INT(ms_cont_engine_check(e), 0);
    return r;
}

/* AC-1 setup: holes 100, 500, 200, 300, 600 (ids 1..5) between four 10-unit SYS blocks (ids
 * 11..14), built by allocating all nine in order and then freeing the holes. */
static ms_cont_engine *silberschatz(const char *policy) {
    static const int64_t holes[] = {100, 500, 200, 300, 600};
    ms_cont_engine *e = ms_cont_engine_create(1740, policy);
    for (uint32_t k = 0; k < 5; k++) {
        CHECK_EQ_INT(alloc(e, SYS, k + 1, holes[k]).outcome, MS_CONT_OK);
        if (k < 4) {
            CHECK_EQ_INT(alloc(e, SYS, k + 11, 10).outcome, MS_CONT_OK);
        }
    }
    for (uint32_t k = 0; k < 5; k++) {
        CHECK_EQ_INT(release(e, SYS, k + 1).outcome, MS_CONT_OK);
    }
    return e;
}

/* Requests 212, 417, 112, 426 as ids 21..24; expected starts, -1 = fails with FRAGMENTATION. */
static void check_ac1(const char *policy, const int64_t expect[4]) {
    static const int64_t req[] = {212, 417, 112, 426};
    ms_cont_engine *e = silberschatz(policy);
    for (uint32_t k = 0; k < 4; k++) {
        ms_cont_result r = alloc(e, P1 + k, 21 + k, req[k]);
        if (expect[k] < 0) {
            CHECK_EQ_INT(r.outcome, MS_CONT_FAILED_FRAGMENTATION);
            CHECK_EQ_INT(r.total_free, 959);
            CHECK_EQ_INT(r.largest_hole, 300);
        } else {
            CHECK_EQ_INT(r.outcome, MS_CONT_OK);
            CHECK_EQ_INT(r.start, expect[k]);
        }
    }
    ms_cont_engine_destroy(e);
}

static void test_ac1_first_fit(void) {
    check_ac1("first-fit", (const int64_t[]){110, 1140, 322, -1});
}

static void test_ac1_best_fit(void) {
    check_ac1("best-fit", (const int64_t[]){830, 110, 620, 1140});
}

static void test_ac1_worst_fit(void) {
    check_ac1("worst-fit", (const int64_t[]){1140, 110, 1352, -1});
}

static void test_ac2_compact_then_retry(void) {
    ms_cont_engine *e = silberschatz("first-fit");
    alloc(e, P1, 21, 212);
    alloc(e, P2, 22, 417);
    alloc(e, P3, 23, 112);
    CHECK_EQ_INT(alloc(e, P4, 24, 426).outcome, MS_CONT_FAILED_FRAGMENTATION);

    ms_cont_result c = ms_cont_engine_compact(e);
    CHECK_EQ_INT(ms_cont_engine_check(e), 0);
    CHECK_EQ_INT(c.blocks_moved, 7);
    CHECK_EQ_INT(c.units_moved, 781);

    /* id 24 failed, so the retry is a new request id (ADR-003: ids are never reused). */
    ms_cont_result r = alloc(e, P4, 25, 426);
    CHECK_EQ_INT(r.outcome, MS_CONT_OK);
    CHECK_EQ_INT(r.start, 781);
    size_t n;
    const ms_block *b = ms_cont_engine_blocks(e, &n);
    CHECK_EQ_INT(b[n - 1].state, MS_BLOCK_FREE);
    CHECK_EQ_INT(b[n - 1].start, 1207);
    CHECK_EQ_INT(b[n - 1].size, 533);
    ms_cont_engine_destroy(e);
}

/* Rejections leave the layout unchanged. */
static void expect_unchanged(ms_cont_engine *e,
                             ms_cont_result (*op)(ms_cont_engine *, ms_pid, ms_alloc_id),
                             ms_pid pid, ms_alloc_id id, ms_cont_outcome want) {
    size_t n0, n1;
    ms_block before[16];
    const ms_block *b = ms_cont_engine_blocks(e, &n0);
    memcpy(before, b, n0 * sizeof *b);
    CHECK_EQ_INT(op(e, pid, id).outcome, want);
    b = ms_cont_engine_blocks(e, &n1);
    CHECK_EQ_INT(n1, n0);
    for (size_t i = 0; i < n0; i++) { /* field by field: memcmp would read ms_block's padding */
        CHECK(before[i].start == b[i].start && before[i].size == b[i].size &&
              before[i].state == b[i].state && before[i].pid == b[i].pid &&
              before[i].alloc_id == b[i].alloc_id && before[i].requested == b[i].requested);
    }
    CHECK_EQ_INT(ms_cont_engine_check(e), 0);
}

static ms_cont_result alloc_10(ms_cont_engine *e, ms_pid pid, ms_alloc_id id) {
    return ms_cont_engine_alloc(e, pid, id, 10);
}

static void test_ac5_rejections(void) {
    ms_cont_engine *e = ms_cont_engine_create(100, "first-fit");
    alloc(e, P1, 1, 30);
    alloc(e, P1, 2, 30);
    release(e, P1, 1);
    expect_unchanged(e, ms_cont_engine_free, P1, 1, MS_CONT_REJECTED_DOUBLE_FREE);
    expect_unchanged(e, ms_cont_engine_free, P2, 2, MS_CONT_REJECTED_WRONG_OWNER);
    expect_unchanged(e, ms_cont_engine_free, P1, 99, MS_CONT_REJECTED_UNKNOWN_ID);
    expect_unchanged(e, alloc_10, P1, 2, MS_CONT_REJECTED_DUPLICATE_ID);
    expect_unchanged(e, alloc_10, P1, 1, MS_CONT_REJECTED_DUPLICATE_ID);
    CHECK_EQ_INT(alloc(e, P1, 3, 0).outcome, MS_CONT_REJECTED_INVALID_SIZE);
    CHECK_EQ_INT(alloc(e, P1, 3, -5).outcome, MS_CONT_REJECTED_INVALID_SIZE);
    ms_cont_engine_destroy(e);
}

static void test_capacity_and_skipped_free(void) {
    ms_cont_engine *e = ms_cont_engine_create(100, "best-fit");
    alloc(e, P1, 1, 60);
    ms_cont_result r = alloc(e, P1, 2, 41);
    CHECK_EQ_INT(r.outcome, MS_CONT_FAILED_CAPACITY);
    CHECK_EQ_INT(r.total_free, 40);
    CHECK_EQ_INT(release(e, P1, 2).outcome, MS_CONT_SKIPPED);
    CHECK_EQ_INT(alloc(e, P1, 2, 5).outcome, MS_CONT_REJECTED_DUPLICATE_ID);
    /* Exact fit: no split, no zero-size remainder. */
    CHECK_EQ_INT(alloc(e, P1, 3, 40).outcome, MS_CONT_OK);
    size_t n;
    ms_cont_engine_blocks(e, &n);
    CHECK_EQ_INT(n, 2);
    ms_cont_engine_destroy(e);
}

static void test_coalesce_both_sides(void) {
    ms_cont_engine *e = ms_cont_engine_create(30, "first-fit");
    alloc(e, P1, 1, 10);
    alloc(e, P1, 2, 10);
    alloc(e, P1, 3, 10);
    release(e, P1, 1);
    release(e, P1, 3);
    release(e, P1, 2);
    size_t n;
    const ms_block *b = ms_cont_engine_blocks(e, &n);
    CHECK_EQ_INT(n, 1);
    CHECK_EQ_INT(b[0].size, 30);
    ms_cont_engine_destroy(e);
}

static void test_terminate(void) {
    ms_cont_engine *e = ms_cont_engine_create(60, "first-fit");
    alloc(e, P1, 1, 10);
    alloc(e, P2, 2, 10);
    alloc(e, P1, 3, 10);
    alloc(e, P1, 4, 10);
    alloc(e, P2, 5, 10);
    ms_cont_result r = ms_cont_engine_terminate(e, P1);
    CHECK_EQ_INT(ms_cont_engine_check(e), 0);
    CHECK_EQ_INT(r.blocks_moved, 3);
    CHECK_EQ_INT(release(e, P1, 3).outcome, MS_CONT_REJECTED_DOUBLE_FREE);
    size_t n;
    ms_cont_engine_blocks(e, &n);
    CHECK_EQ_INT(n, 5); /* free 10, P2 10, free 20, P2 10, free 10 */
    CHECK_EQ_INT(ms_cont_engine_terminate(e, P1).blocks_moved, 0);
    ms_cont_engine_destroy(e);
}

static void test_create_rejects_bad_config(void) {
    CHECK(ms_cont_engine_create(100, "next-fit") == NULL);
    CHECK(ms_cont_engine_create(0, "first-fit") == NULL);
    CHECK(ms_cont_engine_create(100, NULL) == NULL);
}

/* Seeded random ops under every policy, invariants after each one. */
static void test_random_ops_keep_invariants(void) {
    static const char *const names[] = {"first-fit", "best-fit", "worst-fit"};
    for (size_t p = 0; p < 3; p++) {
        uint64_t s = 0x9E3779B97F4A7C15ULL + p;
        ms_cont_engine *e = ms_cont_engine_create(1000, names[p]);
        ms_alloc_id next = 0;
        for (int k = 0; k < 2000; k++) {
            s ^= s >> 12, s ^= s << 25, s ^= s >> 27;
            uint64_t x = s * 2685821657736338717ULL;
            switch (x % 8) {
            case 0:
                ms_cont_engine_compact(e);
                break;
            case 1:
                ms_cont_engine_terminate(e, (ms_pid)(x >> 8) % 4);
                break;
            case 2:
            case 3:
            case 4:
                ms_cont_engine_free(e, (ms_pid)(x >> 8) % 4, (ms_alloc_id)((x >> 16) % (next + 1)));
                break;
            default:
                ms_cont_engine_alloc(e, (ms_pid)(x >> 8) % 4, next++,
                                     1 + (int64_t)((x >> 16) % 120));
            }
            int bad = ms_cont_engine_check(e);
            if (bad != 0) {
                CHECK_EQ_INT(bad, 0);
                break;
            }
        }
        ms_cont_engine_destroy(e);
    }
}

int main(void) {
    RUN_TEST(test_ac1_first_fit);
    RUN_TEST(test_ac1_best_fit);
    RUN_TEST(test_ac1_worst_fit);
    RUN_TEST(test_ac2_compact_then_retry);
    RUN_TEST(test_ac5_rejections);
    RUN_TEST(test_capacity_and_skipped_free);
    RUN_TEST(test_coalesce_both_sides);
    RUN_TEST(test_terminate);
    RUN_TEST(test_create_rejects_bad_config);
    RUN_TEST(test_random_ops_keep_invariants);
    TEST_EXIT();
}
