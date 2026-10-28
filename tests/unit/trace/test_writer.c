#include "memsim/trace/snapshot.h"
#include "memsim/trace/writer.h"
#include "test.h"

#include <stdio.h>
#include <string.h>

static const char *const pid_names[] = {"SYS", "P1", "P2", "P3", "P4"};
static const char *const id_names[] = {"a0", "a1", "a2", "a3", "a4"};
static const ms_trace_symbols test_symbols = {
    .pid_names = pid_names,
    .n_pids = 5,
    .id_names = id_names,
    .n_ids = 5,
};

static void check_line(FILE *f, const char *expected) {
    char buf[4096];
    if (fgets(buf, sizeof(buf), f) == NULL) {
        fprintf(stderr, "Expected line not found (EOF): %s\n", expected);
        CHECK(false);
        return;
    }
    size_t len = strlen(buf);
    if (len > 0 && buf[len - 1] == '\n') {
        buf[len - 1] = '\0';
    }
    if (strcmp(buf, expected) != 0) {
        fprintf(stderr, "Line mismatch:\nGOT:      %s\nEXPECTED: %s\n", buf, expected);
        CHECK(false);
    }
}

static void test_headers(void) {
    FILE *f = tmpfile();
    CHECK(f != NULL);

    ms_trace_writer *w = ms_trace_writer_open(f, &test_symbols);
    CHECK(w != NULL);
    CHECK(ms_trace_writer_ok(w));

    ms_trace_header h_cont = {
        .engine = "contiguous",
        .git = "deadbeef",
        .workload = "test.wl",
        .workload_sha256 = "hash123",
        .policy = "first-fit",
        .paging = false,
        .memory_size = 1740,
    };
    ms_trace_write_header(w, &h_cont);

    ms_trace_header h_page = {
        .engine = "paging",
        .git = "deadbeef",
        .workload = "test.wl",
        .workload_sha256 = "hash123",
        .policy = "fifo",
        .paging = true,
        .page_size = 4096,
        .frames = 3,
    };
    ms_trace_write_header(w, &h_page);

    rewind(f);
    check_line(
        f, "{\"type\":\"TraceHeader\",\"schema\":1,\"engine\":\"contiguous\",\"git\":\"deadbeef\","
           "\"workload\":\"test.wl\",\"workload_sha256\":\"hash123\",\"policy\":\"first-fit\","
           "\"config\":{\"memory_size\":1740},\"trace_level\":1}");
    check_line(
        f, "{\"type\":\"TraceHeader\",\"schema\":1,\"engine\":\"paging\",\"git\":\"deadbeef\","
           "\"workload\":\"test.wl\",\"workload_sha256\":\"hash123\",\"policy\":\"fifo\","
           "\"config\":{\"page_size\":4096,\"frames\":3,\"scope\":\"global\"},\"trace_level\":1}");

    CHECK_EQ_INT(fgetc(f), EOF);
    ms_trace_writer_close(w);
    fclose(f);
}

static void test_ac1_lines(void) {
    FILE *f = tmpfile();
    CHECK(f != NULL);

    ms_trace_writer *w = ms_trace_writer_open(f, &test_symbols);
    CHECK(w != NULL);

    ms_event ev1 = {
        .seq = 52,
        .step = 17,
        .type = MS_EV_PLACEMENT_DECIDED,
        .data.placement_decided =
            {
                .policy = "first-fit",
                .requested = 426,
                .granted = 426,
                .found = false,
                .hole = {.start = 0, .size = 0},
                .scanned = 5,
            },
    };
    ms_trace_write_event(w, &ev1);

    ms_event ev2 = {
        .seq = 53,
        .step = 17,
        .type = MS_EV_ALLOCATION_FAILED,
        .data.allocation_failed =
            {
                .pid = 4,
                .id = 4,
                .requested = 426,
                .granted = 426,
                .cause = MS_ALLOC_FAIL_FRAGMENTATION,
                .total_free = 959,
                .largest_hole = 300,
            },
    };
    ms_trace_write_event(w, &ev2);

    rewind(f);
    check_line(
        f,
        "{\"seq\":52,\"step\":17,\"type\":\"PlacementDecided\",\"data\":{\"policy\":\"first-fit\","
        "\"requested\":426,\"granted\":426,\"hole\":null,\"scanned\":5}}");
    check_line(f, "{\"seq\":53,\"step\":17,\"type\":\"AllocationFailed\",\"data\":{\"pid\":\"P4\","
                  "\"id\":\"a4\","
                  "\"requested\":426,\"granted\":426,\"cause\":\"FRAGMENTATION\",\"total_free\":"
                  "959,\"largest_hole\":300}}");

    CHECK_EQ_INT(fgetc(f), EOF);
    ms_trace_writer_close(w);
    fclose(f);
}

static void test_other_events(void) {
    FILE *f = tmpfile();
    CHECK(f != NULL);

    ms_trace_writer *w = ms_trace_writer_open(f, &test_symbols);
    CHECK(w != NULL);

    /* Found PlacementDecided */
    ms_event ev_found = {
        .seq = 10,
        .step = 3,
        .type = MS_EV_PLACEMENT_DECIDED,
        .data.placement_decided =
            {
                .policy = "best-fit",
                .requested = 100,
                .granted = 128,
                .found = true,
                .hole = {.start = 200, .size = 128},
                .scanned = 2,
            },
    };
    ms_trace_write_event(w, &ev_found);

    /* BlocksCoalesced with 3 merged */
    ms_event ev_coal = {
        .seq = 20,
        .step = 5,
        .type = MS_EV_BLOCKS_COALESCED,
        .data.blocks_coalesced =
            {
                .merged =
                    {
                        {.start = 0, .size = 100},
                        {.start = 100, .size = 50},
                        {.start = 150, .size = 150},
                    },
                .n_merged = 3,
                .result = {.start = 0, .size = 300},
            },
    };
    ms_trace_write_event(w, &ev_coal);

    /* BlockRelocated */
    ms_event ev_reloc = {
        .seq = 30,
        .step = 7,
        .type = MS_EV_BLOCK_RELOCATED,
        .data.block_relocated =
            {
                .id = 1,
                .pid = 2,
                .src = 500,
                .dst = 200,
                .size = 64,
            },
    };
    ms_trace_write_event(w, &ev_reloc);

    /* PageFault */
    ms_event ev_fault = {
        .seq = 40,
        .step = 8,
        .type = MS_EV_PAGE_FAULT,
        .data.page_fault =
            {
                .pid = 1,
                .vpn = 3,
                .kind = MS_FAULT_COMPULSORY,
                .write = true,
            },
    };
    ms_trace_write_event(w, &ev_fault);

    /* AccessViolation with vpn (by_va = false) */
    ms_event ev_viol = {
        .seq = 50,
        .step = 9,
        .type = MS_EV_ACCESS_VIOLATION,
        .data.access_violation =
            {
                .pid = 3,
                .by_va = false,
                .vpn_or_va = 99,
                .reason = MS_VIOLATION_OUT_OF_RANGE,
            },
    };
    ms_trace_write_event(w, &ev_viol);

    /* AccessViolation with va (by_va = true) */
    ms_event ev_viol_va = {
        .seq = 51,
        .step = 10,
        .type = MS_EV_ACCESS_VIOLATION,
        .data.access_violation =
            {
                .pid = 3,
                .by_va = true,
                .vpn_or_va = 40960,
                .reason = MS_VIOLATION_NO_SUCH_PROCESS,
            },
    };
    ms_trace_write_event(w, &ev_viol_va);

    rewind(f);
    check_line(
        f,
        "{\"seq\":10,\"step\":3,\"type\":\"PlacementDecided\",\"data\":{\"policy\":\"best-fit\","
        "\"requested\":100,\"granted\":128,\"hole\":{\"start\":200,\"size\":128},\"scanned\":2}}");
    check_line(
        f, "{\"seq\":20,\"step\":5,\"type\":\"BlocksCoalesced\",\"data\":{\"merged\":["
           "{\"start\":0,\"size\":100},{\"start\":100,\"size\":50},{\"start\":150,\"size\":150}],"
           "\"result\":{\"start\":0,\"size\":300}}}");
    check_line(
        f,
        "{\"seq\":30,\"step\":7,\"type\":\"BlockRelocated\",\"data\":{\"id\":\"a1\",\"pid\":\"P2\","
        "\"src\":500,\"dst\":200,\"size\":64}}");
    check_line(f,
               "{\"seq\":40,\"step\":8,\"type\":\"PageFault\",\"data\":{\"pid\":\"P1\",\"vpn\":3,"
               "\"kind\":\"COMPULSORY\",\"write\":true}}");
    check_line(
        f,
        "{\"seq\":50,\"step\":9,\"type\":\"AccessViolation\",\"data\":{\"pid\":\"P3\",\"vpn\":99,"
        "\"reason\":\"OUT_OF_RANGE\"}}");
    check_line(f, "{\"seq\":51,\"step\":10,\"type\":\"AccessViolation\",\"data\":{\"pid\":\"P3\","
                  "\"va\":40960,"
                  "\"reason\":\"NO_SUCH_PROCESS\"}}");

    CHECK_EQ_INT(fgetc(f), EOF);
    ms_trace_writer_close(w);
    fclose(f);
}

static void test_cont_snapshot(void) {
    FILE *f = tmpfile();
    CHECK(f != NULL);

    ms_trace_writer *w = ms_trace_writer_open(f, &test_symbols);
    CHECK(w != NULL);

    ms_block blocks[3] = {
        {.start = 0,
         .size = 300,
         .state = MS_BLOCK_FREE,
         .pid = MS_NONE,
         .alloc_id = MS_NONE,
         .requested = 0},
        {.start = 300,
         .size = 100,
         .state = MS_BLOCK_ALLOCATED,
         .pid = 1,
         .alloc_id = 2,
         .requested = 80},
        {.start = 400,
         .size = 659,
         .state = MS_BLOCK_FREE,
         .pid = MS_NONE,
         .alloc_id = MS_NONE,
         .requested = 0},
    };
    ms_trace_write_cont_snapshot(w, 17, 1059, blocks, 3);

    rewind(f);
    check_line(
        f,
        "{\"type\":\"Snapshot\",\"step\":17,\"contiguous\":{\"memory_size\":1059,\"blocks\":["
        "{\"start\":0,\"size\":300,\"state\":\"free\"},"
        "{\"start\":300,\"size\":100,\"state\":\"allocated\",\"pid\":\"P1\",\"id\":\"a2\","
        "\"requested\":80},"
        "{\"start\":400,\"size\":659,\"state\":\"free\"}],"
        "\"frag\":{\"total_free\":959,\"largest_hole\":659,\"hole_count\":2,\"ext_frag\":0.312825,"
        "\"used\":100,\"live_blocks\":1,\"int_frag\":20}}}");

    CHECK_EQ_INT(fgetc(f), EOF);
    ms_trace_writer_close(w);
    fclose(f);
}

static void test_page_snapshot(void) {
    FILE *f = tmpfile();
    CHECK(f != NULL);

    ms_trace_writer *w = ms_trace_writer_open(f, &test_symbols);
    CHECK(w != NULL);

    ms_trace_frame frames[2] = {
        {.frame = 0, .pid = 0, .vpn = 7, .referenced = true, .dirty = false},
        {.frame = 1, .pid = MS_NONE, .vpn = 0, .referenced = false, .dirty = false},
    };
    ms_trace_write_page_snapshot(w, 5, frames, 2);

    rewind(f);
    check_line(f,
               "{\"type\":\"Snapshot\",\"step\":5,\"paging\":{\"frames\":["
               "{\"frame\":0,\"pid\":\"SYS\",\"vpn\":7,\"referenced\":true,\"dirty\":false},"
               "{\"frame\":1,\"pid\":null,\"vpn\":null,\"referenced\":false,\"dirty\":false}]}}");

    CHECK_EQ_INT(fgetc(f), EOF);
    ms_trace_writer_close(w);
    fclose(f);
}

static void test_summary(void) {
    FILE *f = tmpfile();
    CHECK(f != NULL);

    ms_trace_writer *w = ms_trace_writer_open(f, &test_symbols);
    CHECK(w != NULL);

    ms_trace_metric metrics[2] = {
        {.key = "total_ops", .is_ratio = false, .value = 100},
        {.key = "hit_ratio", .is_ratio = true, .value = 687174},
    };
    ms_trace_write_summary(w, metrics, 2);

    rewind(f);
    check_line(f,
               "{\"type\":\"RunSummary\",\"metrics\":{\"total_ops\":100,\"hit_ratio\":0.687174}}");

    CHECK_EQ_INT(fgetc(f), EOF);
    ms_trace_writer_close(w);
    fclose(f);
}

static void test_escaping(void) {
    FILE *f = tmpfile();
    CHECK(f != NULL);

    ms_trace_writer *w = ms_trace_writer_open(f, &test_symbols);
    CHECK(w != NULL);

    ms_trace_header h = {
        .engine = "eng\"ine\\with\tctrl\x1f",
        .git = "",
        .workload = "",
        .workload_sha256 = "",
        .policy = "",
        .paging = false,
        .memory_size = 0,
    };
    ms_trace_write_header(w, &h);

    rewind(f);
    check_line(f, "{\"type\":\"TraceHeader\",\"schema\":1,\"engine\":"
                  "\"eng\\\"ine\\\\with\\u0009ctrl\\u001F\","
                  "\"git\":\"\",\"workload\":\"\",\"workload_sha256\":\"\",\"policy\":\"\","
                  "\"config\":{\"memory_size\":0},\"trace_level\":1}");

    CHECK_EQ_INT(fgetc(f), EOF);
    ms_trace_writer_close(w);
    fclose(f);
}

static void test_pid_none(void) {
    FILE *f = tmpfile();
    CHECK(f != NULL);

    ms_trace_writer *w = ms_trace_writer_open(f, &test_symbols);
    CHECK(w != NULL);

    ms_event ev = {
        .seq = 1,
        .step = 1,
        .type = MS_EV_PROCESS_SPAWNED,
        .data.process_spawned =
            {
                .pid = MS_NONE,
                .pages = 10,
            },
    };
    ms_trace_write_event(w, &ev);

    rewind(f);
    check_line(
        f,
        "{\"seq\":1,\"step\":1,\"type\":\"ProcessSpawned\",\"data\":{\"pid\":null,\"pages\":10}}");

    CHECK_EQ_INT(fgetc(f), EOF);
    ms_trace_writer_close(w);
    fclose(f);
}

int main(void) {
    RUN_TEST(test_headers);
    RUN_TEST(test_ac1_lines);
    RUN_TEST(test_other_events);
    RUN_TEST(test_cont_snapshot);
    RUN_TEST(test_page_snapshot);
    RUN_TEST(test_summary);
    RUN_TEST(test_escaping);
    RUN_TEST(test_pid_none);
    TEST_EXIT();
}
