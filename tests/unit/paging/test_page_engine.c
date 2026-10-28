/* Paging engine tests. Owner: P5. */
#include "memsim/paging/engine.h"
#include "test.h"

#include <stdlib.h>

static void test_translation(void) {
    /* va 6700 at page size 4096: vpn 1, offset 2604; frame 0 gives pa 2604 */
    ms_paging_config cfg = ms_paging_config_default();
    cfg.page_size = 4096;
    cfg.frames = 8;
    ms_page_engine *e = NULL;
    CHECK_EQ_INT(ms_page_engine_create(&cfg, &e), MS_OK);

    ms_op spawn = {.kind = MS_OP_SPAWN, .u.spawn = {1, 2}};
    ms_page_step out = {0};
    CHECK_EQ_INT(ms_page_engine_apply(e, &spawn, &out), MS_OK);

    /* va 6700 -> vpn 1, offset 2604; fault loads into frame 0 */
    ms_op access = {.kind = MS_OP_ACCESS, .u.access = {1, true, 6700, false}};
    CHECK_EQ_INT(ms_page_engine_apply(e, &access, &out), MS_OK);
    CHECK(out.fault);
    CHECK_EQ_INT(out.pa, 2604); /* frame 0 << 12 | 2604 */
    /* fault, frame allocated, loaded, translated (4 events for va fault) */
    CHECK_EQ_INT(out.n_events, 4);
    CHECK_EQ_INT(out.events[out.n_events - 1].type, MS_EV_ADDRESS_TRANSLATED);
    CHECK_EQ_INT(out.events[out.n_events - 1].data.address_translated.pa, 2604);
    CHECK(ms_page_engine_check(e));
    ms_page_engine_destroy(e);
}

static void test_ac3_fifo(void) {
    /* AC-3: string 7 0 1 2 0 3 0 4 2 3 0 3 2 1 2 0 1 7 0 1 with 3 frames.
       FIFO 15, LRU 12, OPT 9 faults. Build ops, prepare, apply, check. */
    const uint32_t refs[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
    size_t n_refs = sizeof refs / sizeof refs[0];

    /* Build ops: spawn + 20 accesses */
    ms_op ops[21];
    ops[0] = (ms_op){.kind = MS_OP_SPAWN, .u.spawn = {0, 8}};
    for (size_t i = 0; i < n_refs; i++) {
        ops[i + 1] =
            (ms_op){.kind = MS_OP_ACCESS,
                    .u.access = {.pid = 0, .by_va = false, .vpn_or_va = refs[i], .write = false}};
    }

    /* Test each policy */
    struct {
        const char *name;
        int expected_faults;
    } policies[] = {{"fifo", 15}, {"lru", 12}, {"opt", 9}};

    for (size_t p = 0; p < 3; p++) {
        ms_paging_config cfg = ms_paging_config_default();
        cfg.page_size = 4096;
        cfg.frames = 3;
        cfg.policy = policies[p].name;

        ms_page_engine *e = NULL;
        CHECK_EQ_INT(ms_page_engine_create(&cfg, &e), MS_OK);
        CHECK_EQ_INT(ms_page_engine_prepare(e, ops, 21), MS_OK);

        ms_page_step out = {0};
        CHECK_EQ_INT(ms_page_engine_apply(e, &ops[0], &out), MS_OK);
        for (size_t i = 1; i <= n_refs; i++) {
            CHECK_EQ_INT(ms_page_engine_apply(e, &ops[i], &out), MS_OK);
            CHECK(ms_page_engine_check(e));
        }

        ms_page_stats s = ms_page_engine_stats(e);
        CHECK_EQ_INT(s.refs, n_refs);
        CHECK_EQ_INT(s.hits + s.faults, n_refs);
        CHECK_EQ_INT(s.faults, policies[p].expected_faults);
        ms_page_engine_destroy(e);
    }
}

static void test_dirty_and_writeback(void) {
    ms_paging_config cfg = ms_paging_config_default();
    cfg.frames = 1;
    ms_page_engine *e = NULL;
    CHECK_EQ_INT(ms_page_engine_create(&cfg, &e), MS_OK);

    ms_op spawn = {.kind = MS_OP_SPAWN, .u.spawn = {0, 2}};
    ms_page_step out = {0};
    CHECK_EQ_INT(ms_page_engine_apply(e, &spawn, &out), MS_OK);

    /* write to vpn 0: loads clean, marks dirty in the same access */
    ms_op access0 = {.kind = MS_OP_ACCESS, .u.access = {0, false, 0, true}};
    CHECK_EQ_INT(ms_page_engine_apply(e, &access0, &out), MS_OK);
    CHECK(out.fault);
    /* fault, frame allocated, loaded (no address translation for vpn access) */
    CHECK_EQ_INT(out.n_events, 3);
    CHECK_EQ_INT(ms_page_engine_stats(e).writebacks, 0);

    /* touch vpn 1: evicts vpn 0 (dirty) and counts its write-back */
    ms_op access1 = {.kind = MS_OP_ACCESS, .u.access = {0, false, 1, false}};
    CHECK_EQ_INT(ms_page_engine_apply(e, &access1, &out), MS_OK);
    CHECK(out.fault);
    /* Verify eviction and write-back occurred */
    bool saw_evicted = false, saw_writeback = false;
    for (size_t i = 0; i < out.n_events; i++) {
        if (out.events[i].type == MS_EV_PAGE_EVICTED && out.events[i].data.page_evicted.dirty) {
            saw_evicted = true;
        }
        if (out.events[i].type == MS_EV_PAGE_WRITTEN_BACK) {
            saw_writeback = true;
        }
    }
    CHECK(saw_evicted);
    CHECK(saw_writeback);
    CHECK_EQ_INT(ms_page_engine_stats(e).writebacks, 1);

    ms_page_engine_destroy(e);
}

static void test_violations(void) {
    ms_paging_config cfg = ms_paging_config_default();
    ms_page_engine *e = NULL;
    CHECK_EQ_INT(ms_page_engine_create(&cfg, &e), MS_OK);

    ms_op spawn = {.kind = MS_OP_SPAWN, .u.spawn = {0, 2}};
    ms_page_step out = {0};
    CHECK_EQ_INT(ms_page_engine_apply(e, &spawn, &out), MS_OK);

    /* unknown pid */
    ms_op access_bad_pid = {.kind = MS_OP_ACCESS, .u.access = {99, false, 0, false}};
    CHECK_EQ_INT(ms_page_engine_apply(e, &access_bad_pid, &out), MS_OK);
    CHECK_EQ_INT(out.n_events, 1);
    CHECK_EQ_INT(out.events[0].type, MS_EV_ACCESS_VIOLATION);
    CHECK_EQ_INT(out.events[0].data.access_violation.reason, MS_VIOLATION_NO_SUCH_PROCESS);
    CHECK(!out.fault); /* not a fault */

    /* vpn out of range */
    ms_op access_bad_vpn = {.kind = MS_OP_ACCESS, .u.access = {0, false, 2, false}};
    CHECK_EQ_INT(ms_page_engine_apply(e, &access_bad_vpn, &out), MS_OK);
    CHECK_EQ_INT(out.events[0].type, MS_EV_ACCESS_VIOLATION);
    CHECK_EQ_INT(out.events[0].data.access_violation.reason, MS_VIOLATION_OUT_OF_RANGE);

    ms_page_engine_destroy(e);
}

static void test_terminate(void) {
    ms_paging_config cfg = ms_paging_config_default();
    ms_page_engine *e = NULL;
    CHECK_EQ_INT(ms_page_engine_create(&cfg, &e), MS_OK);

    ms_op spawn = {.kind = MS_OP_SPAWN, .u.spawn = {0, 2}};
    ms_page_step out = {0};
    CHECK_EQ_INT(ms_page_engine_apply(e, &spawn, &out), MS_OK);

    /* access vpn 0, 1 */
    for (int v = 0; v < 2; v++) {
        ms_op access = {.kind = MS_OP_ACCESS, .u.access = {0, false, (uint64_t)v, false}};
        CHECK_EQ_INT(ms_page_engine_apply(e, &access, &out), MS_OK);
    }
    ms_page_stats s1 = ms_page_engine_stats(e);
    CHECK_EQ_INT(s1.faults, 2);

    /* terminate: releases the 2 frames */
    ms_op term = {.kind = MS_OP_TERMINATE, .u.terminate = {0}};
    CHECK_EQ_INT(ms_page_engine_apply(e, &term, &out), MS_OK);
    CHECK_EQ_INT(out.events[0].type, MS_EV_PROCESS_TERMINATED);
    CHECK_EQ_INT(out.events[0].data.process_terminated.released_frames, 2);

    /* invariants pass */
    CHECK(ms_page_engine_check(e));
    ms_page_engine_destroy(e);
}

static void test_frame_accessor(void) {
    /* AC-3 FIFO: string 7 0 1 ... with 3 frames.
       After the first 3 accesses, frames hold pages 7, 0, 1 with referenced true. */
    const uint32_t refs[] = {7, 0, 1};
    ms_op ops[4];
    ops[0] = (ms_op){.kind = MS_OP_SPAWN, .u.spawn = {.pid = 0, .pages = 8}};
    for (size_t i = 0; i < 3; i++) {
        ops[i + 1] =
            (ms_op){.kind = MS_OP_ACCESS,
                    .u.access = {.pid = 0, .by_va = false, .vpn_or_va = refs[i], .write = false}};
    }

    ms_paging_config cfg = ms_paging_config_default();
    cfg.page_size = 4096;
    cfg.frames = 3;
    cfg.policy = "fifo";

    ms_page_engine *e = NULL;
    CHECK_EQ_INT(ms_page_engine_create(&cfg, &e), MS_OK);
    CHECK_EQ_INT(ms_page_engine_prepare(e, ops, 4), MS_OK);

    ms_page_frame_info info[3];
    CHECK_EQ_INT(ms_page_engine_frames(e, info, 3), 3);
    for (size_t i = 0; i < 3; i++) {
        CHECK_EQ_INT(info[i].pid, MS_NONE);
    }

    ms_page_step out = {0};
    CHECK_EQ_INT(ms_page_engine_apply(e, &ops[0], &out), MS_OK);
    for (size_t i = 1; i <= 3; i++) {
        CHECK_EQ_INT(ms_page_engine_apply(e, &ops[i], &out), MS_OK);
    }

    CHECK_EQ_INT(ms_page_engine_frames(e, info, 3), 3);
    CHECK_EQ_INT(info[0].pid, 0);
    CHECK_EQ_INT(info[0].vpn, 7);
    CHECK(info[0].referenced);
    CHECK(!info[0].dirty);

    CHECK_EQ_INT(info[1].pid, 0);
    CHECK_EQ_INT(info[1].vpn, 0);
    CHECK(info[1].referenced);
    CHECK(!info[1].dirty);

    CHECK_EQ_INT(info[2].pid, 0);
    CHECK_EQ_INT(info[2].vpn, 1);
    CHECK(info[2].referenced);
    CHECK(!info[2].dirty);

    ms_page_frame_info info2[2];
    CHECK_EQ_INT(ms_page_engine_frames(e, info2, 2), 3);
    CHECK_EQ_INT(info2[0].vpn, 7);
    CHECK_EQ_INT(info2[1].vpn, 0);

    ms_page_engine_destroy(e);
}

int main(void) {
    RUN_TEST(test_translation);
    RUN_TEST(test_ac3_fifo);
    RUN_TEST(test_dirty_and_writeback);
    RUN_TEST(test_violations);
    RUN_TEST(test_terminate);
    RUN_TEST(test_frame_accessor);
    TEST_EXIT();
}
