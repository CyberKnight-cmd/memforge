/* CompactionStrategy unit tests (FR-C10, AC-2, the PT3 guarantees). Owner: P4. */
#include "compaction.h"
#include "fragmentation.h"
#include "p4_fixtures.h"
#include "test.h"

#include <string.h>

#define MAX_BLOCKS 32

/* AC-2: 7 blocks relocated, 781 units moved, one hole [781, 1740) before the retry. */
static void test_ac2_plan_and_cost(void) {
    ms_cont_move moves[MAX_BLOCKS];
    size_t n = ms_cont_compaction_plan_sliding(AC1_FIRST_FIT, AC1_COUNT, moves);
    CHECK_EQ_INT(n, 7);
    ms_cont_compaction_cost cost = ms_cont_compaction_cost_of(AC1_FIRST_FIT, AC1_COUNT, moves, n);
    CHECK_EQ_INT(cost.blocks_moved, 7);
    CHECK_EQ_INT(cost.units_moved, 781);

    /* moves come in ascending address order, each slid to the end of the previous block */
    const int64_t src[] = {100, 110, 322, 610, 820, 1130, 1140};
    const int64_t dst[] = {0, 10, 222, 334, 344, 354, 364};
    for (size_t i = 0; i < n && i < 7; i++) {
        CHECK_EQ_INT(moves[i].src, src[i]);
        CHECK_EQ_INT(moves[i].dst, dst[i]);
    }
}

/* AC-2, the point of compaction: the 426 request that failed now fits, at 781, leaving one hole
 * [1207, 1740) of 533. ext_frag goes from 0.687 to 0. */
static void test_ac2_retry_succeeds_after_compaction(void) {
    CHECK_EQ_INT(ms_cont_frag_classify_failure(AC1_FIRST_FIT, AC1_COUNT, 426),
                 MS_FAIL_FRAGMENTATION);

    ms_cont_move moves[MAX_BLOCKS];
    ms_block after[MAX_BLOCKS];
    size_t n = ms_cont_compaction_plan_sliding(AC1_FIRST_FIT, AC1_COUNT, moves);
    size_t count = ms_cont_compaction_preview(AC1_FIRST_FIT, AC1_COUNT, moves, n, after);

    ms_cont_frag_report r = ms_cont_frag_report_of(after, count);
    CHECK_EQ_INT(r.hole_count, 1);
    CHECK_EQ_INT(r.largest_hole, 959);
    CHECK_EQ_INT(r.ext_frag_ppm, 0);
    CHECK_EQ_INT(ms_cont_frag_classify_failure(after, count, 426), MS_FAIL_NONE);

    const ms_block *hole = &after[count - 1];
    CHECK_EQ_INT(hole->state, MS_BLOCK_FREE);
    CHECK_EQ_INT(hole->start, 781); /* First-Fit places the 426 retry here */
    CHECK_EQ_INT(hole->start + 426, 1207);
    CHECK_EQ_INT(hole->size - 426, 533);
}

/* The PT3 guarantees, checked on the AC-2 layout. */
static void test_compaction_guarantees(void) {
    ms_cont_move moves[MAX_BLOCKS];
    ms_block after[MAX_BLOCKS];
    size_t n = ms_cont_compaction_plan_sliding(AC1_FIRST_FIT, AC1_COUNT, moves);
    size_t count = ms_cont_compaction_preview(AC1_FIRST_FIT, AC1_COUNT, moves, n, after);

    /* every allocated block survives with the same id, pid, size and requested, in order */
    size_t j = 0;
    for (size_t i = 0; i < AC1_COUNT; i++) {
        const ms_block *b = &AC1_FIRST_FIT[i];
        if (b->state != MS_BLOCK_ALLOCATED) {
            continue;
        }
        CHECK(j < count);
        CHECK_EQ_INT(after[j].alloc_id, b->alloc_id);
        CHECK_EQ_INT(after[j].pid, b->pid);
        CHECK_EQ_INT(after[j].size, b->size);
        CHECK_EQ_INT(after[j].requested, b->requested);
        j++;
    }

    /* total free is unchanged and the result still tiles [0, 1740) without gaps (I1) */
    ms_cont_frag_report before_r = ms_cont_frag_report_of(AC1_FIRST_FIT, AC1_COUNT);
    ms_cont_frag_report after_r = ms_cont_frag_report_of(after, count);
    CHECK_EQ_INT(after_r.total_free, before_r.total_free);
    CHECK(after_r.hole_count <= 1);
    int64_t cursor = 0;
    for (size_t i = 0; i < count; i++) {
        CHECK_EQ_INT(after[i].start, cursor);
        cursor += after[i].size;
    }
    CHECK_EQ_INT(cursor, 1740);
}

/* Planning is pure: the input layout is byte-for-byte unchanged. */
static void test_plan_never_mutates(void) {
    ms_block copy[MAX_BLOCKS];
    memcpy(copy, AC1_FIRST_FIT, sizeof AC1_FIRST_FIT);
    ms_cont_move moves[MAX_BLOCKS];
    ms_cont_compaction_plan_sliding(copy, AC1_COUNT, moves);
    CHECK(memcmp(copy, AC1_FIRST_FIT, sizeof AC1_FIRST_FIT) == 0);
}

/* Applying moves in order never writes over a block that has not moved yet: each destination
 * range ends at or before the source start of every later move. */
static void test_moves_never_overlap_unmoved_blocks(void) {
    ms_cont_move moves[MAX_BLOCKS];
    size_t n = ms_cont_compaction_plan_sliding(AC1_FIRST_FIT, AC1_COUNT, moves);
    for (size_t i = 0; i < n; i++) {
        CHECK(moves[i].dst < moves[i].src);
        for (size_t k = i + 1; k < n; k++) {
            CHECK(moves[i].dst < moves[k].src);
        }
    }
}

static void test_already_compact_needs_no_moves(void) {
    const ms_block packed[] = {
        USED_BLOCK(0, 50, PID_P1, ID_A1, 50),
        USED_BLOCK(50, 30, PID_P2, ID_A2, 30),
        FREE_BLOCK(80, 20),
    };
    ms_cont_move moves[MAX_BLOCKS];
    CHECK_EQ_INT(ms_cont_compaction_plan_sliding(packed, 3, moves), 0);
}

static void test_only_misplaced_blocks_move(void) {
    const ms_block blocks[] = {
        USED_BLOCK(0, 50, PID_P1, ID_A1, 50), /* already at the bottom: stays */
        FREE_BLOCK(50, 20),
        USED_BLOCK(70, 30, PID_P2, ID_A2, 30),
    };
    ms_cont_move moves[MAX_BLOCKS];
    size_t n = ms_cont_compaction_plan_sliding(blocks, 3, moves);
    CHECK_EQ_INT(n, 1);
    CHECK_EQ_INT(moves[0].alloc_id, ID_A2);
    CHECK_EQ_INT(moves[0].dst, 50);
    CHECK_EQ_INT(ms_cont_compaction_cost_of(blocks, 3, moves, n).units_moved, 30);
}

static void test_empty_and_full_memory(void) {
    const ms_block empty[] = {FREE_BLOCK(0, 100)};
    const ms_block full[] = {USED_BLOCK(0, 100, PID_P1, ID_A1, 100)};
    ms_cont_move moves[MAX_BLOCKS];
    ms_block after[MAX_BLOCKS];
    CHECK_EQ_INT(ms_cont_compaction_plan_sliding(empty, 1, moves), 0);
    CHECK_EQ_INT(ms_cont_compaction_preview(empty, 1, moves, 0, after), 1);
    CHECK_EQ_INT(after[0].size, 100);
    CHECK_EQ_INT(ms_cont_compaction_plan_sliding(full, 1, moves), 0);
    CHECK_EQ_INT(ms_cont_compaction_preview(full, 1, moves, 0, after), 1);
    CHECK_EQ_INT(after[0].state, MS_BLOCK_ALLOCATED);
}

int main(void) {
    RUN_TEST(test_ac2_plan_and_cost);
    RUN_TEST(test_ac2_retry_succeeds_after_compaction);
    RUN_TEST(test_compaction_guarantees);
    RUN_TEST(test_plan_never_mutates);
    RUN_TEST(test_moves_never_overlap_unmoved_blocks);
    RUN_TEST(test_already_compact_needs_no_moves);
    RUN_TEST(test_only_misplaced_blocks_move);
    RUN_TEST(test_empty_and_full_memory);
    TEST_EXIT();
}
