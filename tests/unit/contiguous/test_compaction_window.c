/* Windowed ("just enough") compaction, the alternative to sliding. Owner: P4. */
#include "compaction.h"
#include "fragmentation.h"
#include "p4_fixtures.h"
#include "test.h"

#define MAX_BLOCKS 32

/* AC-1 First-Fit, request 426. The cheapest run is [620, 1130): sliding the 10-unit SYS block at
 * 820 down to 620 joins the 200 and 300 holes into one hole [630, 1130) of 500. That is 1 block
 * and 10 units moved, against 7 blocks and 781 units for full sliding. */
static void test_window_on_ac2_moves_less(void) {
    ms_cont_move moves[MAX_BLOCKS];
    ms_block after[2 * MAX_BLOCKS + 1];
    size_t n = ms_cont_compaction_plan_window(AC1_FIRST_FIT, AC1_COUNT, 426, moves);
    ms_cont_compaction_cost cost = ms_cont_compaction_cost_of(AC1_FIRST_FIT, AC1_COUNT, moves, n);
    CHECK_EQ_INT(cost.blocks_moved, 1);
    CHECK_EQ_INT(cost.units_moved, 10);
    CHECK_EQ_INT(moves[0].alloc_id, ID_S3);
    CHECK_EQ_INT(moves[0].src, 820);
    CHECK_EQ_INT(moves[0].dst, 620);

    size_t count = ms_cont_compaction_preview(AC1_FIRST_FIT, AC1_COUNT, moves, n, after);
    CHECK_EQ_INT(ms_cont_frag_classify_failure(after, count, 426), MS_FAIL_NONE);
    ms_cont_frag_report r = ms_cont_frag_report_of(after, count);
    CHECK_EQ_INT(r.total_free, 959);
    CHECK_EQ_INT(r.largest_hole, 500);
    CHECK_EQ_INT(r.hole_count, 4); /* holes outside the run are left alone */
}

static void test_window_noop_when_a_hole_fits(void) {
    ms_cont_move moves[MAX_BLOCKS];
    CHECK_EQ_INT(ms_cont_compaction_plan_window(AC1_FIRST_FIT, AC1_COUNT, 300, moves), 0);
}

static void test_window_noop_on_capacity(void) {
    ms_cont_move moves[MAX_BLOCKS];
    CHECK_EQ_INT(ms_cont_compaction_plan_window(AC1_FIRST_FIT, AC1_COUNT, 960, moves), 0);
}

static void test_window_with_need_of_all_free_matches_sliding_result(void) {
    ms_cont_move moves[MAX_BLOCKS];
    ms_block after[2 * MAX_BLOCKS + 1];
    size_t n = ms_cont_compaction_plan_window(AC1_FIRST_FIT, AC1_COUNT, 959, moves);
    size_t count = ms_cont_compaction_preview(AC1_FIRST_FIT, AC1_COUNT, moves, n, after);
    ms_cont_frag_report r = ms_cont_frag_report_of(after, count);
    CHECK_EQ_INT(r.hole_count, 1);
    CHECK_EQ_INT(r.ext_frag_ppm, 0);
}

int main(void) {
    RUN_TEST(test_window_on_ac2_moves_less);
    RUN_TEST(test_window_noop_when_a_hole_fits);
    RUN_TEST(test_window_noop_on_capacity);
    RUN_TEST(test_window_with_need_of_all_free_matches_sliding_result);
    TEST_EXIT();
}
