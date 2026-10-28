/* Integration tests for the session, bench pipeline, and trace determinism. */
#include "../../src/cli/sha256.c"
#include "memsim/bench.h"
#include "memsim/common/errors.h"
#include "memsim/common/events.h"
#include "memsim/session.h"
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

static void test_sha256_self(void) {
    CHECK(ms_sha256_self_test());
}

static void test_ac6_ac7_fixtures_determinism(void) {
    static const char *const fixtures[] = {
        "ac1_silberschatz.wl", "ac2_compact.wl",    "ac3_refs.wl",
        "ac4_belady.wl",       "ac5_contiguous.wl", "ac5_paging.wl",
    };
    size_t n_fixtures = sizeof(fixtures) / sizeof(fixtures[0]);

    for (size_t i = 0; i < n_fixtures; i++) {
        char path[512];
        get_fixture_path(fixtures[i], path, sizeof(path));

        ms_workload w = {0};
        ms_wl_error err = {0};
        CHECK_EQ_INT(ms_wl_load(path, &w, &err), MS_OK);

        const char *policy = (w.kind == MS_WL_CONTIGUOUS) ? "first-fit" : "fifo";

        FILE *f1 = tmpfile();
        FILE *f2 = tmpfile();
        CHECK(f1 != NULL && f2 != NULL);

        ms_bench_opts opts1 = {
            .trace = f1,
            .workload_name = fixtures[i],
            .workload_sha256 = "test_sha256",
            .git = "test_git",
            .check = true, /* AC-7: check = true on every run */
        };
        ms_bench_result res1 = {0};
        CHECK_EQ_INT(ms_bench_run(&w, policy, &opts1, &res1), MS_OK);

        ms_bench_opts opts2 = {
            .trace = f2,
            .workload_name = fixtures[i],
            .workload_sha256 = "test_sha256",
            .git = "test_git",
            .check = true, /* AC-7: check = true on every run */
        };
        ms_bench_result res2 = {0};
        CHECK_EQ_INT(ms_bench_run(&w, policy, &opts2, &res2), MS_OK);

        /* AC-6: compare two trace files byte for byte */
        rewind(f1);
        rewind(f2);
        int c1 = 0;
        int c2 = 0;
        do {
            c1 = fgetc(f1);
            c2 = fgetc(f2);
            CHECK_EQ_INT(c1, c2);
        } while (c1 != EOF && c2 != EOF);

        /* Check trace lines format:
         * every line starts with '{' and ends with '}';
         * first line is TraceHeader, last is RunSummary */
        rewind(f1);
        char line[8192];
        bool first_line = true;
        char last_line[8192] = {0};
        while (fgets(line, sizeof(line), f1) != NULL) {
            size_t len = strlen(line);
            while (len > 0 && (line[len - 1] == '\n' || line[len - 1] == '\r')) {
                line[--len] = '\0';
            }
            if (len == 0) {
                continue;
            }
            CHECK(line[0] == '{');
            CHECK(line[len - 1] == '}');
            if (first_line) {
                CHECK(strncmp(line, "{\"type\":\"TraceHeader\"", 21) == 0);
                first_line = false;
            }
            size_t copy_len = (len < sizeof(last_line) - 1) ? len : sizeof(last_line) - 1;
            memcpy(last_line, line, copy_len);
            last_line[copy_len] = '\0';
        }
        CHECK(strncmp(last_line, "{\"type\":\"RunSummary\"", 20) == 0);

        fclose(f1);
        fclose(f2);
        ms_bench_result_free(&res1);
        ms_bench_result_free(&res2);
        ms_wl_free(&w);
    }
}

static void test_ac8_table_values(void) {
    char path[512];
    get_fixture_path("ac1_silberschatz.wl", path, sizeof(path));

    ms_workload w = {0};
    ms_wl_error err = {0};
    CHECK_EQ_INT(ms_wl_load(path, &w, &err), MS_OK);

    /* First-fit */
    {
        ms_bench_opts opts = {.check = true};
        ms_bench_result res = {0};
        CHECK_EQ_INT(ms_bench_run(&w, "first-fit", &opts, &res), MS_OK);
        CHECK_EQ_INT(res.n_placements, 13);
        /* Setup placements */
        for (size_t i = 0; i < 9; i++) {
            CHECK(res.placements[i].ok);
        }
        /* Requests a1, a2, a3, a4 */
        CHECK(res.placements[9].ok);
        CHECK_EQ_INT(res.placements[9].start, 110);
        CHECK(res.placements[10].ok);
        CHECK_EQ_INT(res.placements[10].start, 1140);
        CHECK(res.placements[11].ok);
        CHECK_EQ_INT(res.placements[11].start, 322);
        CHECK(!res.placements[12].ok);
        CHECK_EQ_INT(res.placements[12].cause, MS_ALLOC_FAIL_FRAGMENTATION);
        ms_bench_result_free(&res);
    }

    /* Best-fit */
    {
        ms_bench_opts opts = {.check = true};
        ms_bench_result res = {0};
        CHECK_EQ_INT(ms_bench_run(&w, "best-fit", &opts, &res), MS_OK);
        CHECK_EQ_INT(res.n_placements, 13);
        for (size_t i = 0; i < 9; i++) {
            CHECK(res.placements[i].ok);
        }
        CHECK(res.placements[9].ok);
        CHECK_EQ_INT(res.placements[9].start, 830);
        CHECK(res.placements[10].ok);
        CHECK_EQ_INT(res.placements[10].start, 110);
        CHECK(res.placements[11].ok);
        CHECK_EQ_INT(res.placements[11].start, 620);
        CHECK(res.placements[12].ok);
        CHECK_EQ_INT(res.placements[12].start, 1140);
        ms_bench_result_free(&res);
    }

    /* Worst-fit */
    {
        ms_bench_opts opts = {.check = true};
        ms_bench_result res = {0};
        CHECK_EQ_INT(ms_bench_run(&w, "worst-fit", &opts, &res), MS_OK);
        CHECK_EQ_INT(res.n_placements, 13);
        for (size_t i = 0; i < 9; i++) {
            CHECK(res.placements[i].ok);
        }
        CHECK(res.placements[9].ok);
        CHECK_EQ_INT(res.placements[9].start, 1140);
        CHECK(res.placements[10].ok);
        CHECK_EQ_INT(res.placements[10].start, 110);
        CHECK(res.placements[11].ok);
        CHECK_EQ_INT(res.placements[11].start, 1352);
        CHECK(!res.placements[12].ok);
        CHECK_EQ_INT(res.placements[12].cause, MS_ALLOC_FAIL_FRAGMENTATION);
        ms_bench_result_free(&res);
    }

    ms_wl_free(&w);
}

int main(void) {
    RUN_TEST(test_sha256_self);
    RUN_TEST(test_ac6_ac7_fixtures_determinism);
    RUN_TEST(test_ac8_table_values);
    TEST_EXIT();
}
