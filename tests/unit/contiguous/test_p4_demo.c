/* P4 sample run and experiment: memory layout before and after each compaction strategy on AC-1,
 * how the candidate fragmentation metrics disagree, and a seeded First-Fit workload comparing no
 * compaction, sliding and windowed compaction. Output is deterministic. Owner: P4. */
#include "compaction.h"
#include "fragmentation.h"
#include "p4_fixtures.h"
#include "p4_sim.h"
#include "test.h"

#include <stdio.h>

#define MAP_WIDTH 58

static void print_map(const char *title, const ms_block *b, size_t n) {
    int64_t m = b[n - 1].start + b[n - 1].size;
    char bar[MAP_WIDTH + 1];
    for (int c = 0; c < MAP_WIDTH; c++) {
        int64_t lo = m * c / MAP_WIDTH, hi = m * (c + 1) / MAP_WIDTH;
        char ch = '.';
        for (size_t i = 0; i < n; i++) {
            if (b[i].state == MS_BLOCK_ALLOCATED && b[i].start < hi &&
                b[i].start + b[i].size > lo) {
                ch = b[i].pid == PID_SYS ? 'S' : (char)('0' + b[i].pid);
            }
        }
        bar[c] = ch;
    }
    bar[MAP_WIDTH] = '\0';
    ms_cont_frag_report r = ms_cont_frag_report_of(b, n);
    printf("%s\n  |%s|  0..%lld\n  ", title, bar, (long long)m);
    for (size_t i = 0; i < n; i++) {
        if (b[i].state == MS_BLOCK_FREE) {
            printf("[%lld,%lld) free %lld  ", (long long)b[i].start,
                   (long long)(b[i].start + b[i].size), (long long)b[i].size);
        }
    }
    printf("\n  total_free %lld, largest_hole %lld, holes %u, ext_frag %.3f\n\n",
           (long long)r.total_free, (long long)r.largest_hole, r.hole_count,
           ms_cont_ppm_to_double(r.ext_frag_ppm));
}

static void show_strategy(const char *name, const ms_cont_move *moves, size_t k) {
    ms_block after[2 * 32 + 1];
    ms_cont_compaction_cost cost = ms_cont_compaction_cost_of(AC1_FIRST_FIT, AC1_COUNT, moves, k);
    size_t n = ms_cont_compaction_preview(AC1_FIRST_FIT, AC1_COUNT, moves, k, after);
    char title[160];
    snprintf(title, sizeof title, "After %s: %u blocks / %lld units moved; request 426 -> %s", name,
             cost.blocks_moved, (long long)cost.units_moved,
             ms_cont_frag_classify_failure(after, n, 426) == MS_FAIL_NONE ? "fits" : "FAILS");
    print_map(title, after, n);
}

static void sample_run(void) {
    ms_cont_move moves[32];
    printf("== Sample run: AC-1 First-Fit (S = SYS, digits = process, . = free) ==\n\n");
    print_map("Before compaction: request 426 -> FRAGMENTATION", AC1_FIRST_FIT, AC1_COUNT);
    show_strategy("sliding", moves,
                  ms_cont_compaction_plan_sliding(AC1_FIRST_FIT, AC1_COUNT, moves));
    show_strategy("window", moves,
                  ms_cont_compaction_plan_window(AC1_FIRST_FIT, AC1_COUNT, 426, moves));
}

/* Where each candidate metric gives a misleading answer. */
static void metric_comparison(void) {
    static const ms_block two_halves[] = {FREE_BLOCK(0, 500), USED_BLOCK(500, 10, 1, 1, 10),
                                          FREE_BLOCK(510, 500)};
    static const ms_block one_big_many_tiny[] = {
        FREE_BLOCK(0, 900),          USED_BLOCK(900, 5, 1, 1, 5), FREE_BLOCK(905, 1),
        USED_BLOCK(906, 5, 1, 2, 5), FREE_BLOCK(911, 1),          USED_BLOCK(912, 5, 1, 3, 5),
        FREE_BLOCK(917, 1),          USED_BLOCK(918, 5, 1, 4, 5), FREE_BLOCK(923, 1),
    };
    static const ms_block dust[] = {FREE_BLOCK(0, 20), USED_BLOCK(20, 300, 1, 1, 300),
                                    FREE_BLOCK(320, 20), USED_BLOCK(340, 300, 1, 2, 300),
                                    FREE_BLOCK(640, 20)};
    struct {
        const char *name;
        const ms_block *b;
        size_t n;
    } cases[] = {
        {"AC-1 First-Fit", AC1_FIRST_FIT, AC1_COUNT},
        {"two 500 holes", two_halves, 3},
        {"900 hole + 4 x 1", one_big_many_tiny, 9},
        {"three 20 holes", dust, 5},
    };
    printf("== Candidate metrics (unusable_free for a 100-unit request) ==\n\n");
    printf("  %-18s %6s %6s %8s %13s %13s\n", "layout", "free", "holes", "ext_frag", "avg hole",
           "unusable(100)");
    for (size_t i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        ms_cont_frag_report r = ms_cont_frag_report_of(cases[i].b, cases[i].n);
        printf("  %-18s %6lld %6u %8.3f %13.1f %13.3f\n", cases[i].name, (long long)r.total_free,
               r.hole_count, ms_cont_ppm_to_double(r.ext_frag_ppm),
               r.hole_count ? (double)r.total_free / r.hole_count : 0.0,
               ms_cont_ppm_to_double(ms_cont_frag_unusable_free_ppm(cases[i].b, cases[i].n, 100)));
    }
    printf("\n");
}

typedef enum { NEVER, SLIDING, WINDOW } strategy;

typedef struct {
    int64_t requests, capacity_fail, frag_fail, compactions, units_moved;
    uint64_t ext_frag_sum, samples;
} run_stats;

static void run_workload(strategy s, uint64_t seed, run_stats *st) {
    static ms_block b[SIM_MAX_BLOCKS], after[2 * SIM_MAX_BLOCKS + 1];
    static ms_cont_move moves[SIM_MAX_BLOCKS];
    ms_alloc_id live[SIM_MAX_BLOCKS];
    size_t n_live = 0, n = 1;
    ms_alloc_id next_id = 1;
    sim_rng rng = {seed};
    b[0] = (ms_block){0, 1024, MS_BLOCK_FREE, MS_NONE, MS_NONE, 0};

    for (int op = 0; op < 3000; op++) {
        if (n_live > 0 && sim_range(&rng, 0, 99) < 45) {
            size_t pick = (size_t)sim_range(&rng, 0, (int64_t)n_live - 1);
            n = sim_free(b, n, live[pick]);
            live[pick] = live[--n_live];
        } else {
            int64_t size = sim_range(&rng, 8, 128);
            st->requests++;
            size_t got = sim_first_fit(b, n, next_id, size);
            if (got == 0 && s != NEVER &&
                ms_cont_frag_classify_failure(b, n, size) == MS_FAIL_FRAGMENTATION) {
                size_t k = s == SLIDING ? ms_cont_compaction_plan_sliding(b, n, moves)
                                        : ms_cont_compaction_plan_window(b, n, size, moves);
                st->compactions++;
                st->units_moved += ms_cont_compaction_cost_of(b, n, moves, k).units_moved;
                n = ms_cont_compaction_preview(b, n, moves, k, after);
                memcpy(b, after, n * sizeof *b);
                got = sim_first_fit(b, n, next_id, size);
            }
            if (got == 0) {
                ms_cont_failure_cause c = ms_cont_frag_classify_failure(b, n, size);
                st->capacity_fail += c == MS_FAIL_CAPACITY;
                st->frag_fail += c == MS_FAIL_FRAGMENTATION;
            } else {
                n = got;
                live[n_live++] = next_id;
            }
            next_id++;
        }
        st->ext_frag_sum += ms_cont_frag_report_of(b, n).ext_frag_ppm;
        st->samples++;
    }
}

static void experiment(void) {
    const char *names[] = {"never", "sliding", "window"};
    run_stats st[3] = {{0}};
    for (int s = NEVER; s <= WINDOW; s++) {
        for (uint64_t seed = 1; seed <= 10; seed++) {
            run_workload((strategy)s, seed * 7919, &st[s]);
        }
    }
    printf("== Experiment: First-Fit, memory 1024, 3000 ops x 10 seeds, sizes 8-128, 45%% frees "
           "==\n\n");
    printf("  %-8s %8s %9s %9s %12s %12s %13s\n", "compact", "requests", "frag-fail", "cap-fail",
           "compactions", "units moved", "mean ext_frag");
    for (int s = NEVER; s <= WINDOW; s++) {
        printf("  %-8s %8lld %9lld %9lld %12lld %12lld %13.3f\n", names[s],
               (long long)st[s].requests, (long long)st[s].frag_fail,
               (long long)st[s].capacity_fail, (long long)st[s].compactions,
               (long long)st[s].units_moved,
               (double)st[s].ext_frag_sum / (double)st[s].samples / MS_PPM);
    }
    printf("\n");
    /* Compaction must remove every FRAGMENTATION failure: after it, a request fails only for
     * CAPACITY. */
    CHECK_EQ_INT(st[SLIDING].frag_fail, 0);
    CHECK_EQ_INT(st[WINDOW].frag_fail, 0);
    CHECK(st[NEVER].frag_fail > 0);
}

int main(void) {
    sample_run();
    metric_comparison();
    experiment();
    TEST_EXIT();
}
