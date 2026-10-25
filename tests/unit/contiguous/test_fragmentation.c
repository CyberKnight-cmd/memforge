/* FragmentationAnalyzer unit tests (FR-C9). Owner: P4. */
#include "fragmentation.h"
#include "p4_fixtures.h"
#include "test.h"

#include <string.h>

/* README section 8 worked example: 959 free, largest hole 300, ext_frag 0.687. */
static void test_ac1_first_fit_report(void) {
    ms_cont_frag_report r = ms_cont_frag_report_of(AC1_FIRST_FIT, AC1_COUNT);
    CHECK_EQ_INT(r.total_free, 959);
    CHECK_EQ_INT(r.largest_hole, 300);
    CHECK_EQ_INT(r.hole_count, 5);
    CHECK_EQ_INT(r.ext_frag_ppm, 687174); /* floor(659 / 959 * 1e6) */
    CHECK_EQ_INT(r.used, 781);
    CHECK_EQ_INT(r.live_blocks, 7);
    CHECK_EQ_INT(r.int_frag, 0);
    CHECK_EQ_INT(r.used + r.total_free, 1740);
}

static void test_ac1_426_fails_by_fragmentation(void) {
    CHECK_EQ_INT(ms_cont_frag_classify_failure(AC1_FIRST_FIT, AC1_COUNT, 426),
                 MS_FAIL_FRAGMENTATION);
    CHECK(strcmp(ms_cont_failure_cause_name(MS_FAIL_FRAGMENTATION), "FRAGMENTATION") == 0);
}

static void test_capacity_when_total_free_too_small(void) {
    CHECK_EQ_INT(ms_cont_frag_classify_failure(AC1_FIRST_FIT, AC1_COUNT, 960), MS_FAIL_CAPACITY);
}

static void test_boundaries_of_the_cause(void) {
    /* exactly the largest hole fits; exactly total_free is still fragmentation */
    CHECK_EQ_INT(ms_cont_frag_classify_failure(AC1_FIRST_FIT, AC1_COUNT, 300), MS_FAIL_NONE);
    CHECK_EQ_INT(ms_cont_frag_classify_failure(AC1_FIRST_FIT, AC1_COUNT, 301),
                 MS_FAIL_FRAGMENTATION);
    CHECK_EQ_INT(ms_cont_frag_classify_failure(AC1_FIRST_FIT, AC1_COUNT, 959),
                 MS_FAIL_FRAGMENTATION);
}

static void test_empty_memory_is_unfragmented(void) {
    const ms_block one_hole[] = {FREE_BLOCK(0, 1000)};
    ms_cont_frag_report r = ms_cont_frag_report_of(one_hole, 1);
    CHECK_EQ_INT(r.total_free, 1000);
    CHECK_EQ_INT(r.hole_count, 1);
    CHECK_EQ_INT(r.ext_frag_ppm, 0);
}

static void test_full_memory_has_no_holes(void) {
    const ms_block full[] = {USED_BLOCK(0, 1000, PID_P1, ID_A1, 1000)};
    ms_cont_frag_report r = ms_cont_frag_report_of(full, 1);
    CHECK_EQ_INT(r.total_free, 0);
    CHECK_EQ_INT(r.largest_hole, 0);
    CHECK_EQ_INT(r.hole_count, 0);
    CHECK_EQ_INT(r.ext_frag_ppm, 0); /* defined as 0, not a division by zero */
    CHECK_EQ_INT(ms_cont_frag_classify_failure(full, 1, 1), MS_FAIL_CAPACITY);
}

static void test_internal_fragmentation(void) {
    const ms_block blocks[] = {
        USED_BLOCK(0, 16, PID_P1, ID_A1, 13),
        USED_BLOCK(16, 8, PID_P2, ID_A2, 8),
        FREE_BLOCK(24, 8),
    };
    CHECK_EQ_INT(ms_cont_frag_report_of(blocks, 3).int_frag, 3);
}

static void test_unusable_free(void) {
    /* holes 100, 176, 200, 300, 183: for s = 200, holes 100 + 176 + 183 = 459 of 959 are useless */
    CHECK_EQ_INT(ms_cont_frag_unusable_free_ppm(AC1_FIRST_FIT, AC1_COUNT, 200), 478623);
    CHECK_EQ_INT(ms_cont_frag_unusable_free_ppm(AC1_FIRST_FIT, AC1_COUNT, 1), 0);
    CHECK_EQ_INT(ms_cont_frag_unusable_free_ppm(AC1_FIRST_FIT, AC1_COUNT, 301), MS_PPM);
}

static void test_ppm_to_double(void) {
    double f = ms_cont_ppm_to_double(687174);
    CHECK(f > 0.687 && f < 0.688);
}

int main(void) {
    RUN_TEST(test_ac1_first_fit_report);
    RUN_TEST(test_ac1_426_fails_by_fragmentation);
    RUN_TEST(test_capacity_when_total_free_too_small);
    RUN_TEST(test_boundaries_of_the_cause);
    RUN_TEST(test_empty_memory_is_unfragmented);
    RUN_TEST(test_full_memory_has_no_holes);
    RUN_TEST(test_internal_fragmentation);
    RUN_TEST(test_unusable_free);
    RUN_TEST(test_ppm_to_double);
    TEST_EXIT();
}
