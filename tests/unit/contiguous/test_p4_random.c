/* Validation beyond the hand-picked cases: 5,000 seeded random layouts, checked against an
 * independent unit-by-unit oracle and the compaction guarantees (PT3). Owner: P4. */
#include "compaction.h"
#include "fragmentation.h"
#include "p4_sim.h"
#include "test.h"

#include <stdio.h>

#define LAYOUTS 5000
#define SEED 20261026ULL

static int64_t live_units(const ms_block *b, size_t n) {
    int64_t u = 0;
    for (size_t i = 0; i < n; i++) {
        u += b[i].state == MS_BLOCK_ALLOCATED ? b[i].size : 0;
    }
    return u;
}

/* PT3: same allocated blocks in the same order, same total free, still tiles [0, m), coalesced. */
static int check_guarantees(const ms_block *before, size_t nb, const ms_block *after, size_t na,
                            int64_t m) {
    int bad = 0;
    size_t j = 0;
    for (size_t i = 0; i < nb; i++) {
        if (before[i].state != MS_BLOCK_ALLOCATED) {
            continue;
        }
        while (j < na && after[j].state != MS_BLOCK_ALLOCATED) {
            j++;
        }
        bad |= j >= na || after[j].alloc_id != before[i].alloc_id ||
               after[j].size != before[i].size || after[j].pid != before[i].pid ||
               after[j].requested != before[i].requested;
        j++;
    }
    int64_t at = 0;
    for (size_t i = 0; i < na; i++) {
        bad |= after[i].start != at || after[i].size <= 0;
        bad |= i > 0 && after[i].state == MS_BLOCK_FREE && after[i - 1].state == MS_BLOCK_FREE;
        at += after[i].size;
    }
    bad |= at != m || live_units(after, na) != live_units(before, nb);
    return bad;
}

static void test_random_layouts(void) {
    sim_rng rng = {SEED};
    static ms_block layout[SIM_MAX_BLOCKS], after[2 * SIM_MAX_BLOCKS + 1];
    static ms_cont_move moves[SIM_MAX_BLOCKS];
    int metric_errors = 0, cause_errors = 0, sliding_errors = 0, window_errors = 0;
    int window_cheaper = 0, window_equal = 0, frag_cases = 0;
    int64_t sliding_units = 0, window_units = 0;

    for (int t = 0; t < LAYOUTS; t++) {
        int64_t m = sim_range(&rng, 1, 4000);
        size_t n = sim_random_layout(&rng, layout, m, sim_range(&rng, 1, 200));
        sim_oracle o = sim_oracle_of(layout, n);
        ms_cont_frag_report r = ms_cont_frag_report_of(layout, n);
        metric_errors += r.total_free != o.total_free || r.largest_hole != o.longest_run ||
                         r.used + r.total_free != m;

        int64_t need = sim_range(&rng, 1, m);
        ms_cont_failure_cause want =
            o.longest_run >= need
                ? MS_FAIL_NONE
                : (o.total_free >= need ? MS_FAIL_FRAGMENTATION : MS_FAIL_CAPACITY);
        cause_errors += ms_cont_frag_classify_failure(layout, n, need) != want;

        size_t k = ms_cont_compaction_plan_sliding(layout, n, moves);
        int64_t s_units = ms_cont_compaction_cost_of(layout, n, moves, k).units_moved;
        size_t na = ms_cont_compaction_preview(layout, n, moves, k, after);
        sliding_errors += check_guarantees(layout, n, after, na, m);
        sliding_errors += ms_cont_frag_report_of(after, na).hole_count > 1;

        if (want != MS_FAIL_FRAGMENTATION) {
            continue;
        }
        frag_cases++;
        k = ms_cont_compaction_plan_window(layout, n, need, moves);
        int64_t w_units = ms_cont_compaction_cost_of(layout, n, moves, k).units_moved;
        na = ms_cont_compaction_preview(layout, n, moves, k, after);
        window_errors += check_guarantees(layout, n, after, na, m);
        window_errors += ms_cont_frag_classify_failure(after, na, need) != MS_FAIL_NONE;
        window_errors += w_units > s_units; /* full sliding is one of the runs it considers */
        window_cheaper += w_units < s_units;
        window_equal += w_units == s_units;
        sliding_units += s_units;
        window_units += w_units;
    }
    CHECK_EQ_INT(metric_errors, 0);
    CHECK_EQ_INT(cause_errors, 0);
    CHECK_EQ_INT(sliding_errors, 0);
    CHECK_EQ_INT(window_errors, 0);
    printf("%d random layouts (seed %llu): metrics and failure cause match the oracle; "
           "compaction guarantees hold.\n",
           LAYOUTS, (unsigned long long)SEED);
    printf("On the %d FRAGMENTATION failures: window moved less in %d, the same in %d; "
           "units moved in total: sliding %lld, window %lld.\n",
           frag_cases, window_cheaper, window_equal, (long long)sliding_units,
           (long long)window_units);
}

int main(void) {
    RUN_TEST(test_random_layouts);
    TEST_EXIT();
}
