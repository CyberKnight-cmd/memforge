

#include "placement.h"
#include "test.h"

static void test_basic_selection(void) {
    const ms_cont_placement_hole holes[] = {{0, 100}, {120, 30}, {200, 60}, {300, 40}};

    const ms_cont_placement_view view = {holes, 4U};
    ms_cont_placement_decision d;

    d = ms_cont_first_fit_choose(&view, 35);
    CHECK(d.found && d.hole.start == 0);
    CHECK(d.scanned == 1U);

    d = ms_cont_best_fit_choose(&view, 35);
    CHECK(d.found && d.hole.start == 300);
    CHECK(d.hole.size == 40);
    CHECK(d.scanned == 4U);

    d = ms_cont_worst_fit_choose(&view, 35);
    CHECK(d.found && d.hole.start == 0);
    CHECK(d.hole.size == 100);
    CHECK(d.scanned == 4U);
}

static void test_tie_breaking(void) {
    const ms_cont_placement_hole holes[] = {{10, 50}, {100, 50}, {200, 70}, {300, 70}};

    const ms_cont_placement_view view = {holes, 4U};
    ms_cont_placement_decision d;

    d = ms_cont_best_fit_choose(&view, 40);
    CHECK(d.found && d.hole.start == 10);

    d = ms_cont_worst_fit_choose(&view, 40);
    CHECK(d.found && d.hole.start == 200);
}

static void test_no_fit(void) {
    const ms_cont_placement_hole holes[] = {{0, 10}, {20, 15}};

    const ms_cont_placement_view view = {holes, 2U};
    ms_cont_placement_decision d;

    d = ms_cont_first_fit_choose(&view, 16);
    CHECK(!d.found);
    CHECK(d.scanned == 2U);

    d = ms_cont_best_fit_choose(&view, 16);
    CHECK(!d.found);

    d = ms_cont_worst_fit_choose(&view, 16);
    CHECK(!d.found);
}

static void test_invalid_requests(void) {
    const ms_cont_placement_hole holes[] = {{0, 20}};
    const ms_cont_placement_view view = {holes, 1U};
    ms_cont_placement_decision d;

    d = ms_cont_first_fit_choose(&view, 0);
    CHECK(!d.found);

    d = ms_cont_best_fit_choose(NULL, 10);
    CHECK(!d.found);

    d = ms_cont_worst_fit_choose(&view, -5);
    CHECK(!d.found);
}

static void test_exact_fit(void) {
    const ms_cont_placement_hole holes[] = {{5, 12}, {30, 20}};

    const ms_cont_placement_view view = {holes, 2U};
    ms_cont_placement_decision d;

    d = ms_cont_best_fit_choose(&view, 12);
    CHECK(d.found);
    CHECK(d.hole.start == 5);
    CHECK(d.hole.size == 12);
}

int main(void) {
    RUN_TEST(test_basic_selection);
    RUN_TEST(test_tie_breaking);
    RUN_TEST(test_no_fit);
    RUN_TEST(test_invalid_requests);
    RUN_TEST(test_exact_fit);

    TEST_EXIT();
}
