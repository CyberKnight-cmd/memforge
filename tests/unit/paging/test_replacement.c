/* P6 unit tests: FIFO, LRU and OPT fault counts (AC-3), the Belady sweep (AC-4) and a seeded
 * property run (stack algorithms never show the anomaly; OPT is never beaten). Owner: P6. */
#include "memsim/paging/belady.h"
#include "replacement.h"
#include "test.h"

#include <string.h>

#define MAX_REFS 64

static size_t refs_of(const uint32_t *vpns, size_t n, ms_page_key *out) {
    for (size_t i = 0; i < n; i++) {
        out[i] = (ms_page_key){0, vpns[i]};
    }
    return n;
}

static uint64_t faults_of(const char *policy, const ms_page_key *refs, size_t n, uint32_t frames) {
    uint64_t faults = 0;
    CHECK(ms_page_count_faults(policy, refs, n, frames, &faults));
    return faults;
}

static void test_ac3_fault_counts(void) {
    static const uint32_t s[] = {7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1};
    ms_page_key refs[MAX_REFS];
    size_t n = refs_of(s, sizeof s / sizeof *s, refs);
    CHECK_EQ_INT(faults_of("fifo", refs, n, 3), 15);
    CHECK_EQ_INT(faults_of("lru", refs, n, 3), 12);
    CHECK_EQ_INT(faults_of("opt", refs, n, 3), 9);
}

static void test_ac4_belady(void) {
    static const uint32_t s[] = {1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5};
    static const uint64_t fifo_expected[] = {12, 12, 9, 10, 5, 5};
    ms_page_key refs[MAX_REFS];
    size_t n = refs_of(s, sizeof s / sizeof *s, refs);
    uint64_t sweep[6];
    uint32_t points[5];

    CHECK(ms_page_sweep("fifo", refs, n, 6, sweep));
    for (int f = 0; f < 6; f++) {
        CHECK_EQ_INT(sweep[f], fifo_expected[f]);
    }
    CHECK_EQ_INT(ms_page_belady_points(sweep, 6, points), 1);
    CHECK_EQ_INT(points[0], 3);

    CHECK_EQ_INT(faults_of("lru", refs, n, 3), 10);
    CHECK_EQ_INT(faults_of("lru", refs, n, 4), 8);
    CHECK_EQ_INT(faults_of("opt", refs, n, 3), 7);
    CHECK_EQ_INT(faults_of("opt", refs, n, 4), 6);
    CHECK(ms_page_sweep("lru", refs, n, 6, sweep));
    CHECK_EQ_INT(ms_page_belady_points(sweep, 6, points), 0);
    CHECK(ms_page_sweep("opt", refs, n, 6, sweep));
    CHECK_EQ_INT(ms_page_belady_points(sweep, 6, points), 0);
}

static void test_registry(void) {
    CHECK(ms_page_replacement_find("fifo") == &ms_page_fifo);
    CHECK(ms_page_replacement_find("lru") == &ms_page_lru);
    CHECK(ms_page_replacement_find("opt") == &ms_page_opt);
    CHECK(ms_page_replacement_find("clock") == NULL);
    CHECK(ms_page_replacement_find(NULL) == NULL);
    CHECK(ms_page_opt.requires_lookahead);
    CHECK(!ms_page_fifo.requires_lookahead && !ms_page_lru.requires_lookahead);
}

static void test_edge_cases(void) {
    ms_page_key refs[4] = {{0, 1}, {1, 1}, {0, 1}, {1, 1}};
    uint64_t faults = 99;
    CHECK(!ms_page_count_faults("clock", refs, 4, 2, &faults));
    CHECK(!ms_page_count_faults("fifo", refs, 4, 0, &faults));
    CHECK(!ms_page_sweep("lru", refs, 4, 0, &faults));
    CHECK(ms_page_count_faults("opt", refs, 0, 2, &faults));
    CHECK_EQ_INT(faults, 0);
    /* Same vpn under two pids is two pages. */
    CHECK_EQ_INT(faults_of("lru", refs, 4, 1), 4);
    CHECK_EQ_INT(faults_of("lru", refs, 4, 2), 2);
}

/* OPT ties: frames 0..2 hold pages never used again, so the lowest frame index goes. */
static void test_opt_tie_break(void) {
    static const ms_page_key refs[] = {{0, 1}, {0, 2}, {0, 3}, {0, 4}};
    void *s = ms_page_opt.create(3);
    CHECK(ms_page_opt.prepare(s, refs, 4));
    for (uint32_t f = 0; f < 3; f++) {
        ms_page_opt.on_access(s, f, refs[f], f, false);
    }
    static const uint32_t candidates[] = {2, 0, 1};
    CHECK_EQ_INT(ms_page_opt.choose_victim(s, NULL, candidates, 3, 3), 0);
    ms_page_opt.destroy(s);
}

/* xorshift64*: the same sequence on every platform. */
static uint64_t rng_next(uint64_t *s) {
    *s ^= *s >> 12;
    *s ^= *s << 25;
    *s ^= *s >> 27;
    return *s * 2685821657736338717ULL;
}

static void test_seeded_properties(void) {
    enum { STRINGS = 2000, PAGES = 7, FRAMES = 8 };
    uint64_t seed = 0x6d656d666f726765ULL;
    for (int k = 0; k < STRINGS; k++) {
        ms_page_key refs[30];
        size_t n = 1 + rng_next(&seed) % 30;
        bool seen[PAGES] = {false};
        uint64_t distinct = 0;
        for (size_t i = 0; i < n; i++) {
            uint32_t v = (uint32_t)(rng_next(&seed) % PAGES);
            distinct += !seen[v];
            seen[v] = true;
            refs[i] = (ms_page_key){0, v};
        }
        uint64_t fifo[FRAMES], lru[FRAMES], opt[FRAMES];
        uint32_t points[FRAMES];
        CHECK(ms_page_sweep("fifo", refs, n, FRAMES, fifo));
        CHECK(ms_page_sweep("lru", refs, n, FRAMES, lru));
        CHECK(ms_page_sweep("opt", refs, n, FRAMES, opt));
        CHECK_EQ_INT(ms_page_belady_points(lru, FRAMES, points), 0);
        CHECK_EQ_INT(ms_page_belady_points(opt, FRAMES, points), 0);
        for (uint32_t f = 0; f < FRAMES; f++) {
            CHECK(opt[f] <= lru[f] && opt[f] <= fifo[f]);
            CHECK(opt[f] >= distinct);
            if (f + 1 >= distinct) {
                CHECK(fifo[f] == distinct && lru[f] == distinct && opt[f] == distinct);
            }
        }
        if (test_failures_) {
            return; /* one bad string is enough output */
        }
    }
}

int main(void) {
    RUN_TEST(test_ac3_fault_counts);
    RUN_TEST(test_ac4_belady);
    RUN_TEST(test_registry);
    RUN_TEST(test_edge_cases);
    RUN_TEST(test_opt_tie_break);
    RUN_TEST(test_seeded_properties);
    TEST_EXIT();
}
