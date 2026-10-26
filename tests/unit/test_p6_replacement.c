#include "test.h"
#include "memsim/paging/belady.h"
#include "memsim/paging/replacement.h"

static const uint32_t ac3[] = {
    7U, 0U, 1U, 2U, 0U, 3U, 0U, 4U, 2U, 3U,
    0U, 3U, 2U, 1U, 2U, 0U, 1U, 7U, 0U, 1U
};
static const uint32_t ac4[] = {1U, 2U, 3U, 4U, 1U, 2U, 5U, 1U, 2U, 3U, 4U, 5U};

static void test_ac3_fault_counts(void)
{
    ms_page_result result = {0};
    CHECK(ms_page_simulate(MS_PAGE_FIFO, ac3, sizeof(ac3) / sizeof(ac3[0]), 3U, &result));
    CHECK_EQ_INT(result.faults, 15);
    CHECK_EQ_INT(result.hits, 5);
    CHECK_EQ_INT(result.evictions, 12);
    ms_page_result_destroy(&result);

    CHECK(ms_page_simulate(MS_PAGE_LRU, ac3, sizeof(ac3) / sizeof(ac3[0]), 3U, &result));
    CHECK_EQ_INT(result.faults, 12);
    CHECK_EQ_INT(result.hits, 8);
    ms_page_result_destroy(&result);

    CHECK(ms_page_simulate(MS_PAGE_OPT, ac3, sizeof(ac3) / sizeof(ac3[0]), 3U, &result));
    CHECK_EQ_INT(result.faults, 9);
    CHECK_EQ_INT(result.hits, 11);
    ms_page_result_destroy(&result);
}

static void test_ac4_frame_sweep(void)
{
    ms_page_sweep_result sweep = {0};
    CHECK(ms_page_belady_sweep(MS_PAGE_FIFO, ac4, sizeof(ac4) / sizeof(ac4[0]),
                               3U, 4U, &sweep));
    CHECK_EQ_INT(sweep.point_count, 2);
    CHECK_EQ_INT(sweep.points[0].faults, 9);
    CHECK_EQ_INT(sweep.points[1].faults, 10);
    CHECK(sweep.belady_anomaly);
    ms_page_sweep_result_destroy(&sweep);

    CHECK(ms_page_belady_sweep(MS_PAGE_LRU, ac4, sizeof(ac4) / sizeof(ac4[0]),
                               3U, 4U, &sweep));
    CHECK_EQ_INT(sweep.points[0].faults, 10);
    CHECK_EQ_INT(sweep.points[1].faults, 8);
    CHECK(!sweep.belady_anomaly);
    ms_page_sweep_result_destroy(&sweep);

    CHECK(ms_page_belady_sweep(MS_PAGE_OPT, ac4, sizeof(ac4) / sizeof(ac4[0]),
                               3U, 4U, &sweep));
    CHECK_EQ_INT(sweep.points[0].faults, 7);
    CHECK_EQ_INT(sweep.points[1].faults, 6);
    CHECK(!sweep.belady_anomaly);
    ms_page_sweep_result_destroy(&sweep);
}

static void test_edge_cases_and_invalid_inputs(void)
{
    ms_page_result result = {0};
    CHECK(!ms_page_simulate((ms_page_policy)99, ac3, 20U, 3U, &result));
    CHECK(!ms_page_simulate(MS_PAGE_FIFO, ac3, 20U, 0U, &result));
    CHECK(!ms_page_simulate(MS_PAGE_FIFO, NULL, 1U, 3U, &result));
    CHECK(ms_page_simulate(MS_PAGE_FIFO, NULL, 0U, 3U, &result));
    CHECK_EQ_INT(result.faults, 0);
    CHECK_EQ_INT(result.hits, 0);
    ms_page_result_destroy(&result);
}

int main(void)
{
    RUN_TEST(test_ac3_fault_counts);
    RUN_TEST(test_ac4_frame_sweep);
    RUN_TEST(test_edge_cases_and_invalid_inputs);
    TEST_EXIT();
}
