/* Golden workload fixtures end-to-end tests for acceptance criteria AC-1 to AC-5. */
#include "memsim/common/config.h"
#include "memsim/common/errors.h"
#include "memsim/common/events.h"
#include "memsim/common/types.h"
#include "memsim/contiguous/engine.h"
#include "memsim/paging/engine.h"
#include "memsim/workloads/loader.h"
#include "test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static void get_fixture_path(const char *name, char *buf, size_t size) {
    const char *last_slash = strrchr(__FILE__, '/');
    if (last_slash != NULL) {
        size_t dir_len = (size_t)(last_slash - __FILE__);
        (void)snprintf(buf, size, "%.*s/../fixtures/%s", (int)dir_len, __FILE__, name);
    } else {
        (void)snprintf(buf, size, "../fixtures/%s", name);
    }
}

static ms_cont_result apply_cont_op(ms_cont_engine *e, const ms_op *op) {
    ms_cont_result res = {0};
    switch (op->kind) {
    case MS_OP_ALLOC:
        res = ms_cont_engine_alloc(e, op->u.alloc.pid, op->u.alloc.id, op->u.alloc.size);
        break;
    case MS_OP_FREE:
        res = ms_cont_engine_free(e, op->u.free.pid, op->u.free.id);
        break;
    case MS_OP_TERMINATE:
        res = ms_cont_engine_terminate(e, op->u.terminate.pid);
        break;
    case MS_OP_COMPACT:
        res = ms_cont_engine_compact(e);
        break;
    case MS_OP_SPAWN:
    case MS_OP_ACCESS:
        break;
    }
    CHECK_EQ_INT(ms_cont_engine_check(e), 0);
    return res;
}

static void run_ac1(const char *policy, const int64_t expected_starts[4],
                    bool fails_fragmentation) {
    char path[512];
    get_fixture_path("ac1_silberschatz.wl", path, sizeof(path));
    ms_workload w = {0};
    ms_wl_error err = {0};
    CHECK_EQ_INT(ms_wl_load(path, &w, &err), MS_OK);
    CHECK_EQ_INT(w.kind, MS_WL_CONTIGUOUS);
    CHECK_EQ_INT(w.n_ops, 18);

    ms_cont_engine *e = ms_cont_engine_create(w.memory_size, policy);
    CHECK(e != NULL);

    for (size_t i = 0; i < 14; i++) {
        ms_cont_result r = apply_cont_op(e, &w.ops[i]);
        CHECK_EQ_INT(r.outcome, MS_CONT_OK);
    }

    for (size_t i = 0; i < 3; i++) {
        ms_cont_result r = apply_cont_op(e, &w.ops[14 + i]);
        CHECK_EQ_INT(r.outcome, MS_CONT_OK);
        CHECK_EQ_INT(r.start, expected_starts[i]);
    }

    ms_cont_result r_last = apply_cont_op(e, &w.ops[17]);
    if (fails_fragmentation) {
        CHECK_EQ_INT(r_last.outcome, MS_CONT_FAILED_FRAGMENTATION);
        CHECK_EQ_INT(r_last.total_free, 959);
        CHECK_EQ_INT(r_last.largest_hole, 300);
    } else {
        CHECK_EQ_INT(r_last.outcome, MS_CONT_OK);
        CHECK_EQ_INT(r_last.start, expected_starts[3]);
    }

    ms_cont_engine_destroy(e);
    ms_wl_free(&w);
}

static void test_ac1_first_fit(void) {
    const int64_t starts[4] = {110, 1140, 322, -1};
    run_ac1("first-fit", starts, true);
}

static void test_ac1_best_fit(void) {
    const int64_t starts[4] = {830, 110, 620, 1140};
    run_ac1("best-fit", starts, false);
}

static void test_ac1_worst_fit(void) {
    const int64_t starts[4] = {1140, 110, 1352, -1};
    run_ac1("worst-fit", starts, true);
}

static void test_ac2_compact(void) {
    char path[512];
    get_fixture_path("ac2_compact.wl", path, sizeof(path));
    ms_workload w = {0};
    ms_wl_error err = {0};
    CHECK_EQ_INT(ms_wl_load(path, &w, &err), MS_OK);
    CHECK_EQ_INT(w.kind, MS_WL_CONTIGUOUS);
    CHECK_EQ_INT(w.n_ops, 20);

    ms_cont_engine *e = ms_cont_engine_create(w.memory_size, "first-fit");
    CHECK(e != NULL);

    for (size_t i = 0; i < 14; i++) {
        ms_cont_result r = apply_cont_op(e, &w.ops[i]);
        CHECK_EQ_INT(r.outcome, MS_CONT_OK);
    }

    /* P1 a1 212 */
    ms_cont_result r0 = apply_cont_op(e, &w.ops[14]);
    CHECK_EQ_INT(r0.outcome, MS_CONT_OK);
    CHECK_EQ_INT(r0.start, 110);

    /* P2 a2 417 */
    ms_cont_result r1 = apply_cont_op(e, &w.ops[15]);
    CHECK_EQ_INT(r1.outcome, MS_CONT_OK);
    CHECK_EQ_INT(r1.start, 1140);

    /* P3 a3 112 */
    ms_cont_result r2 = apply_cont_op(e, &w.ops[16]);
    CHECK_EQ_INT(r2.outcome, MS_CONT_OK);
    CHECK_EQ_INT(r2.start, 322);

    /* P4 a4 426 fails with FRAGMENTATION */
    ms_cont_result r3 = apply_cont_op(e, &w.ops[17]);
    CHECK_EQ_INT(r3.outcome, MS_CONT_FAILED_FRAGMENTATION);

    /* Compact: moves 7 blocks and 781 units */
    ms_cont_result c = apply_cont_op(e, &w.ops[18]);
    CHECK_EQ_INT(c.blocks_moved, 7);
    CHECK_EQ_INT(c.units_moved, 781);

    /* a4-retry starts at 781 */
    ms_cont_result r_retry = apply_cont_op(e, &w.ops[19]);
    CHECK_EQ_INT(r_retry.outcome, MS_CONT_OK);
    CHECK_EQ_INT(r_retry.start, 781);

    /* Final layout ends with one free block [1207, 1740) */
    size_t n_blocks = 0;
    const ms_block *blocks = ms_cont_engine_blocks(e, &n_blocks);
    CHECK(n_blocks > 0);
    for (size_t i = 0; i < n_blocks - 1; i++) {
        CHECK_EQ_INT(blocks[i].state, MS_BLOCK_ALLOCATED);
    }
    CHECK_EQ_INT(blocks[n_blocks - 1].state, MS_BLOCK_FREE);
    CHECK_EQ_INT(blocks[n_blocks - 1].start, 1207);
    CHECK_EQ_INT(blocks[n_blocks - 1].size, 533);
    CHECK_EQ_INT(blocks[n_blocks - 1].start + blocks[n_blocks - 1].size, 1740);

    ms_cont_engine_destroy(e);
    ms_wl_free(&w);
}

static uint64_t run_page_workload(const ms_workload *w, const char *policy, uint32_t frames) {
    ms_paging_config cfg = ms_paging_config_default();
    cfg.page_size = w->page_size;
    cfg.frames = frames;
    cfg.policy = policy;
    ms_page_engine *e = NULL;
    CHECK_EQ_INT(ms_page_engine_create(&cfg, &e), MS_OK);
    CHECK_EQ_INT(ms_page_engine_prepare(e, w->ops, w->n_ops), MS_OK);
    for (size_t i = 0; i < w->n_ops; i++) {
        ms_page_step step = {0};
        CHECK_EQ_INT(ms_page_engine_apply(e, &w->ops[i], &step), MS_OK);
        CHECK(ms_page_engine_check(e));
    }
    ms_page_stats stats = ms_page_engine_stats(e);
    ms_page_engine_destroy(e);
    return stats.faults;
}

static void test_ac3_refs(void) {
    char path[512];
    get_fixture_path("ac3_refs.wl", path, sizeof(path));
    ms_workload w = {0};
    ms_wl_error err = {0};
    CHECK_EQ_INT(ms_wl_load(path, &w, &err), MS_OK);
    CHECK_EQ_INT(w.kind, MS_WL_PAGING);

    CHECK_EQ_INT(run_page_workload(&w, "fifo", w.frames), 15);
    CHECK_EQ_INT(run_page_workload(&w, "lru", w.frames), 12);
    CHECK_EQ_INT(run_page_workload(&w, "opt", w.frames), 9);

    ms_wl_free(&w);
}

static void test_ac4_belady(void) {
    char path[512];
    get_fixture_path("ac4_belady.wl", path, sizeof(path));
    ms_workload w = {0};
    ms_wl_error err = {0};
    CHECK_EQ_INT(ms_wl_load(path, &w, &err), MS_OK);
    CHECK_EQ_INT(w.kind, MS_WL_PAGING);

    static const uint64_t expected_fifo[] = {12, 12, 9, 10, 5, 5};
    for (uint32_t f = 1; f <= 6; f++) {
        CHECK_EQ_INT(run_page_workload(&w, "fifo", f), expected_fifo[f - 1]);
    }

    CHECK_EQ_INT(run_page_workload(&w, "fifo", 3), 9);
    CHECK_EQ_INT(run_page_workload(&w, "fifo", 4), 10);

    CHECK_EQ_INT(run_page_workload(&w, "lru", 3), 10);
    CHECK_EQ_INT(run_page_workload(&w, "lru", 4), 8);

    CHECK_EQ_INT(run_page_workload(&w, "opt", 3), 7);
    CHECK_EQ_INT(run_page_workload(&w, "opt", 4), 6);

    ms_wl_free(&w);
}

static void check_blocks_equal(const ms_block *a, const ms_block *b, size_t n) {
    for (size_t i = 0; i < n; i++) {
        CHECK(a[i].start == b[i].start);
        CHECK(a[i].size == b[i].size);
        CHECK(a[i].state == b[i].state);
        CHECK(a[i].pid == b[i].pid);
        CHECK(a[i].alloc_id == b[i].alloc_id);
        CHECK(a[i].requested == b[i].requested);
    }
}

static void test_ac5_contiguous(void) {
    char path[512];
    get_fixture_path("ac5_contiguous.wl", path, sizeof(path));
    ms_workload w = {0};
    ms_wl_error err = {0};
    CHECK_EQ_INT(ms_wl_load(path, &w, &err), MS_OK);
    CHECK_EQ_INT(w.kind, MS_WL_CONTIGUOUS);
    CHECK(w.negative_test);
    CHECK_EQ_INT(w.n_ops, 6);

    ms_cont_engine *e = ms_cont_engine_create(w.memory_size, "first-fit");
    CHECK(e != NULL);

    /* Op 0: alloc P1 a1 30 */
    ms_cont_result r0 = apply_cont_op(e, &w.ops[0]);
    CHECK_EQ_INT(r0.outcome, MS_CONT_OK);

    /* Op 1: alloc P1 a2 30 */
    ms_cont_result r1 = apply_cont_op(e, &w.ops[1]);
    CHECK_EQ_INT(r1.outcome, MS_CONT_OK);

    /* Op 2: free P1 a1 */
    ms_cont_result r2 = apply_cont_op(e, &w.ops[2]);
    CHECK_EQ_INT(r2.outcome, MS_CONT_OK);

    /* Op 3: free P1 a1 -> DOUBLE_FREE */
    size_t snap_cnt = 0;
    const ms_block *snap_b = ms_cont_engine_blocks(e, &snap_cnt);
    ms_block snap3[16];
    CHECK(snap_cnt <= 16);
    memcpy(snap3, snap_b, snap_cnt * sizeof(ms_block));

    ms_cont_result r3 = apply_cont_op(e, &w.ops[3]);
    CHECK_EQ_INT(r3.outcome, MS_CONT_REJECTED_DOUBLE_FREE);
    size_t cur_cnt = 0;
    const ms_block *cur_b = ms_cont_engine_blocks(e, &cur_cnt);
    CHECK_EQ_INT(cur_cnt, snap_cnt);
    check_blocks_equal(snap3, cur_b, snap_cnt);

    /* Op 4: free P2 a2 -> WRONG_OWNER */
    ms_block snap4[16];
    snap_b = ms_cont_engine_blocks(e, &snap_cnt);
    memcpy(snap4, snap_b, snap_cnt * sizeof(ms_block));

    ms_cont_result r4 = apply_cont_op(e, &w.ops[4]);
    CHECK_EQ_INT(r4.outcome, MS_CONT_REJECTED_WRONG_OWNER);
    cur_b = ms_cont_engine_blocks(e, &cur_cnt);
    CHECK_EQ_INT(cur_cnt, snap_cnt);
    check_blocks_equal(snap4, cur_b, snap_cnt);

    /* Op 5: free P1 a99 -> UNKNOWN_ID */
    ms_block snap5[16];
    snap_b = ms_cont_engine_blocks(e, &snap_cnt);
    memcpy(snap5, snap_b, snap_cnt * sizeof(ms_block));

    ms_cont_result r5 = apply_cont_op(e, &w.ops[5]);
    CHECK_EQ_INT(r5.outcome, MS_CONT_REJECTED_UNKNOWN_ID);
    cur_b = ms_cont_engine_blocks(e, &cur_cnt);
    CHECK_EQ_INT(cur_cnt, snap_cnt);
    check_blocks_equal(snap5, cur_b, snap_cnt);

    ms_cont_engine_destroy(e);
    ms_wl_free(&w);
}

static void test_ac5_paging(void) {
    char path[512];
    get_fixture_path("ac5_paging.wl", path, sizeof(path));
    ms_workload w = {0};
    ms_wl_error err = {0};
    CHECK_EQ_INT(ms_wl_load(path, &w, &err), MS_OK);
    CHECK_EQ_INT(w.kind, MS_WL_PAGING);
    CHECK(w.negative_test);
    CHECK_EQ_INT(w.n_ops, 3);

    ms_paging_config cfg = ms_paging_config_default();
    cfg.page_size = w.page_size;
    cfg.frames = w.frames;
    cfg.policy = "fifo";

    ms_page_engine *e = NULL;
    CHECK_EQ_INT(ms_page_engine_create(&cfg, &e), MS_OK);
    CHECK_EQ_INT(ms_page_engine_prepare(e, w.ops, w.n_ops), MS_OK);

    /* Op 0: spawn P0 4 */
    ms_page_step step = {0};
    CHECK_EQ_INT(ms_page_engine_apply(e, &w.ops[0], &step), MS_OK);
    CHECK(ms_page_engine_check(e));

    ms_page_stats stats_before = ms_page_engine_stats(e);

    /* Op 1: access P0 9 (beyond address space) */
    CHECK_EQ_INT(ms_page_engine_apply(e, &w.ops[1], &step), MS_OK);
    CHECK(ms_page_engine_check(e));
    CHECK_EQ_INT(step.n_events, 1);
    CHECK_EQ_INT(step.events[0].type, MS_EV_ACCESS_VIOLATION);
    CHECK_EQ_INT(step.events[0].data.access_violation.reason, MS_VIOLATION_OUT_OF_RANGE);

    ms_page_stats stats_after1 = ms_page_engine_stats(e);
    CHECK_EQ_INT(stats_after1.refs, stats_before.refs);
    CHECK_EQ_INT(stats_after1.hits, stats_before.hits);
    CHECK_EQ_INT(stats_after1.faults, stats_before.faults);
    CHECK_EQ_INT(stats_after1.evictions, stats_before.evictions);
    CHECK_EQ_INT(stats_after1.writebacks, stats_before.writebacks);

    /* Op 2: access P9 0 (no such process) */
    CHECK_EQ_INT(ms_page_engine_apply(e, &w.ops[2], &step), MS_OK);
    CHECK(ms_page_engine_check(e));
    CHECK_EQ_INT(step.n_events, 1);
    CHECK_EQ_INT(step.events[0].type, MS_EV_ACCESS_VIOLATION);
    CHECK_EQ_INT(step.events[0].data.access_violation.reason, MS_VIOLATION_NO_SUCH_PROCESS);

    ms_page_stats stats_after2 = ms_page_engine_stats(e);
    CHECK_EQ_INT(stats_after2.refs, stats_before.refs);
    CHECK_EQ_INT(stats_after2.hits, stats_before.hits);
    CHECK_EQ_INT(stats_after2.faults, stats_before.faults);
    CHECK_EQ_INT(stats_after2.evictions, stats_before.evictions);
    CHECK_EQ_INT(stats_after2.writebacks, stats_before.writebacks);

    ms_page_engine_destroy(e);
    ms_wl_free(&w);
}

int main(void) {
    RUN_TEST(test_ac1_first_fit);
    RUN_TEST(test_ac1_best_fit);
    RUN_TEST(test_ac1_worst_fit);
    RUN_TEST(test_ac2_compact);
    RUN_TEST(test_ac3_refs);
    RUN_TEST(test_ac4_belady);
    RUN_TEST(test_ac5_contiguous);
    RUN_TEST(test_ac5_paging);
    TEST_EXIT();
}
