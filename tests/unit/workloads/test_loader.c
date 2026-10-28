/* Workload loader tests. Owner: P7. */
#include "memsim/workloads/loader.h"
#include "test.h"

#include <string.h>

static void test_contiguous_workload(void) {
    const char *text = "# Contiguous workload demo\n"
                       "\n"
                       "schema 1\n"
                       "gen_provenance test_gen 1.0\n"
                       "kind contiguous # contiguous mode\n"
                       "memory_size 1048576\n"
                       "\n"
                       "alloc SYS A1 65536 # initial system alloc\n"
                       "alloc P1 A2 131072\n"
                       "free SYS A1\n"
                       "compact\n"
                       "terminate P1\n";

    ms_workload w;
    ms_wl_error err = {0};
    CHECK_EQ_INT(ms_wl_parse(text, strlen(text), &w, &err), MS_OK);
    CHECK_EQ_INT(w.kind, MS_WL_CONTIGUOUS);
    CHECK_EQ_INT(w.memory_size, 1048576);
    CHECK_EQ_INT(w.negative_test, 0);
    CHECK_EQ_INT(w.n_ops, 5);

    /* Check ops */
    CHECK_EQ_INT(w.ops[0].kind, MS_OP_ALLOC);
    CHECK_EQ_INT(w.ops[0].u.alloc.pid, 0);
    CHECK_EQ_INT(w.ops[0].u.alloc.id, 0);
    CHECK_EQ_INT(w.ops[0].u.alloc.size, 65536);

    CHECK_EQ_INT(w.ops[1].kind, MS_OP_ALLOC);
    CHECK_EQ_INT(w.ops[1].u.alloc.pid, 1);
    CHECK_EQ_INT(w.ops[1].u.alloc.id, 1);
    CHECK_EQ_INT(w.ops[1].u.alloc.size, 131072);

    CHECK_EQ_INT(w.ops[2].kind, MS_OP_FREE);
    CHECK_EQ_INT(w.ops[2].u.free.pid, 0);
    CHECK_EQ_INT(w.ops[2].u.free.id, 0);

    CHECK_EQ_INT(w.ops[3].kind, MS_OP_COMPACT);

    CHECK_EQ_INT(w.ops[4].kind, MS_OP_TERMINATE);
    CHECK_EQ_INT(w.ops[4].u.terminate.pid, 1);

    /* Interning: SYS -> 0, P1 -> 1, ids in first-seen order */
    CHECK_EQ_INT(w.n_pids, 2);
    CHECK(strcmp(w.pid_names[0], "SYS") == 0);
    CHECK(strcmp(w.pid_names[1], "P1") == 0);

    CHECK_EQ_INT(w.n_ids, 2);
    CHECK(strcmp(w.id_names[0], "A1") == 0);
    CHECK(strcmp(w.id_names[1], "A2") == 0);

    /* Round-trip names */
    CHECK(strcmp(w.pid_names[w.ops[0].u.alloc.pid], "SYS") == 0);
    CHECK(strcmp(w.pid_names[w.ops[1].u.alloc.pid], "P1") == 0);
    CHECK(strcmp(w.id_names[w.ops[0].u.alloc.id], "A1") == 0);
    CHECK(strcmp(w.id_names[w.ops[1].u.alloc.id], "A2") == 0);

    ms_wl_free(&w);
}

static void test_paging_workload(void) {
    const char *text = "schema 1\n"
                       "kind paging\n"
                       "page_size 4096\n"
                       "frames 8\n"
                       "spawn P0 16\n"
                       "access P0 7\n"
                       "access P0 @6700 w\n"
                       "terminate P0\n";

    ms_workload w;
    ms_wl_error err = {0};
    CHECK_EQ_INT(ms_wl_parse(text, strlen(text), &w, &err), MS_OK);
    CHECK_EQ_INT(w.kind, MS_WL_PAGING);
    CHECK_EQ_INT(w.page_size, 4096);
    CHECK_EQ_INT(w.frames, 8);
    CHECK_EQ_INT(w.negative_test, 0);
    CHECK_EQ_INT(w.n_ops, 4);

    CHECK_EQ_INT(w.ops[0].kind, MS_OP_SPAWN);
    CHECK_EQ_INT(w.ops[0].u.spawn.pid, 0);
    CHECK_EQ_INT(w.ops[0].u.spawn.pages, 16);

    /* access P0 7: by_va false, write false */
    CHECK_EQ_INT(w.ops[1].kind, MS_OP_ACCESS);
    CHECK_EQ_INT(w.ops[1].u.access.pid, 0);
    CHECK_EQ_INT(w.ops[1].u.access.by_va, false);
    CHECK_EQ_INT(w.ops[1].u.access.vpn_or_va, 7);
    CHECK_EQ_INT(w.ops[1].u.access.write, false);

    /* access P0 @6700 w: by_va true, write true */
    CHECK_EQ_INT(w.ops[2].kind, MS_OP_ACCESS);
    CHECK_EQ_INT(w.ops[2].u.access.pid, 0);
    CHECK_EQ_INT(w.ops[2].u.access.by_va, true);
    CHECK_EQ_INT(w.ops[2].u.access.vpn_or_va, 6700);
    CHECK_EQ_INT(w.ops[2].u.access.write, true);

    CHECK_EQ_INT(w.ops[3].kind, MS_OP_TERMINATE);
    CHECK_EQ_INT(w.ops[3].u.terminate.pid, 0);

    CHECK_EQ_INT(w.n_pids, 1);
    CHECK(strcmp(w.pid_names[0], "P0") == 0);

    ms_wl_free(&w);
}

static void test_always_errors(void) {
    ms_workload w;
    ms_wl_error err = {0};

    /* Missing schema -> line 0 */
    const char *no_schema = "kind contiguous\nmemory_size 1024\nalloc P1 A1 10\n";
    CHECK_EQ_INT(ms_wl_parse(no_schema, strlen(no_schema), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 0);
    ms_wl_free(&w);

    /* Missing kind -> line 0 */
    const char *no_kind = "schema 1\nmemory_size 1024\nalloc P1 A1 10\n";
    CHECK_EQ_INT(ms_wl_parse(no_kind, strlen(no_kind), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 0);
    ms_wl_free(&w);

    /* Missing memory_size for contiguous -> line 0 */
    const char *no_mem = "schema 1\nkind contiguous\nalloc P1 A1 10\n";
    CHECK_EQ_INT(ms_wl_parse(no_mem, strlen(no_mem), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 0);
    ms_wl_free(&w);

    /* Missing page_size for paging -> line 0 */
    const char *no_page_sz = "schema 1\nkind paging\nframes 8\nspawn P1 10\n";
    CHECK_EQ_INT(ms_wl_parse(no_page_sz, strlen(no_page_sz), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 0);
    ms_wl_free(&w);

    /* Missing frames for paging -> line 0 */
    const char *no_frames = "schema 1\nkind paging\npage_size 4096\nspawn P1 10\n";
    CHECK_EQ_INT(ms_wl_parse(no_frames, strlen(no_frames), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 0);
    ms_wl_free(&w);

    /* Unknown schema -> line 2 */
    const char *bad_schema = "\nschema 2\nkind contiguous\nmemory_size 1024\n";
    CHECK_EQ_INT(ms_wl_parse(bad_schema, strlen(bad_schema), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 2);
    ms_wl_free(&w);

    /* Unknown key -> line 3 */
    const char *bad_key = "schema 1\nkind contiguous\nbad_key 42\nmemory_size 1024\n";
    CHECK_EQ_INT(ms_wl_parse(bad_key, strlen(bad_key), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 3);
    ms_wl_free(&w);

    /* Unknown op -> line 4 */
    const char *bad_op = "schema 1\nkind contiguous\nmemory_size 1024\nfly P1 A1 10\n";
    CHECK_EQ_INT(ms_wl_parse(bad_op, strlen(bad_op), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 4);
    ms_wl_free(&w);

    /* Op not allowed for kind: spawn in contiguous -> line 4 */
    const char *spawn_cont = "schema 1\nkind contiguous\nmemory_size 1024\nspawn P1 10\n";
    CHECK_EQ_INT(ms_wl_parse(spawn_cont, strlen(spawn_cont), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 4);
    ms_wl_free(&w);

    /* Op not allowed for kind: alloc in paging -> line 5 */
    const char *alloc_page = "schema 1\nkind paging\npage_size 4096\nframes 8\nalloc P1 A1 10\n";
    CHECK_EQ_INT(ms_wl_parse(alloc_page, strlen(alloc_page), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 5);
    ms_wl_free(&w);

    /* Op not allowed for kind: compact in paging -> line 5 */
    const char *compact_page = "schema 1\nkind paging\npage_size 4096\nframes 8\ncompact\n";
    CHECK_EQ_INT(ms_wl_parse(compact_page, strlen(compact_page), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 5);
    ms_wl_free(&w);

    /* Wrong token count: header key with 3 tokens -> line 1 */
    const char *header_toks = "schema 1 2\nkind contiguous\nmemory_size 1024\n";
    CHECK_EQ_INT(ms_wl_parse(header_toks, strlen(header_toks), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 1);
    ms_wl_free(&w);

    /* Wrong token count: header key with 1 token -> line 1 */
    const char *header_tok1 = "schema\nkind contiguous\nmemory_size 1024\n";
    CHECK_EQ_INT(ms_wl_parse(header_tok1, strlen(header_tok1), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 1);
    ms_wl_free(&w);

    /* Wrong token count: alloc with 3 tokens -> line 4 */
    const char *alloc_toks = "schema 1\nkind contiguous\nmemory_size 1024\nalloc P1 A1\n";
    CHECK_EQ_INT(ms_wl_parse(alloc_toks, strlen(alloc_toks), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 4);
    ms_wl_free(&w);

    /* Wrong token count: free with 2 tokens -> line 4 */
    const char *free_toks = "schema 1\nkind contiguous\nmemory_size 1024\nfree P1\n";
    CHECK_EQ_INT(ms_wl_parse(free_toks, strlen(free_toks), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 4);
    ms_wl_free(&w);

    /* Wrong token count: compact with extra token -> line 4 */
    const char *compact_toks = "schema 1\nkind contiguous\nmemory_size 1024\ncompact now\n";
    CHECK_EQ_INT(ms_wl_parse(compact_toks, strlen(compact_toks), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 4);
    ms_wl_free(&w);

    /* Wrong token count: spawn with 2 tokens -> line 5 */
    const char *spawn_toks = "schema 1\nkind paging\npage_size 4096\nframes 8\nspawn P1\n";
    CHECK_EQ_INT(ms_wl_parse(spawn_toks, strlen(spawn_toks), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 5);
    ms_wl_free(&w);

    /* Wrong token count: terminate with 3 tokens -> line 4 */
    const char *term_toks = "schema 1\nkind contiguous\nmemory_size 1024\nterminate P1 extra\n";
    CHECK_EQ_INT(ms_wl_parse(term_toks, strlen(term_toks), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 4);
    ms_wl_free(&w);

    /* Wrong token count: access with 2 tokens -> line 5 */
    const char *acc_toks = "schema 1\nkind paging\npage_size 4096\nframes 8\naccess P1\n";
    CHECK_EQ_INT(ms_wl_parse(acc_toks, strlen(acc_toks), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 5);
    ms_wl_free(&w);

    /* Wrong token count: access with 5 tokens -> line 5 */
    const char *acc_toks5 =
        "schema 1\nkind paging\npage_size 4096\nframes 8\naccess P1 0 w extra\n";
    CHECK_EQ_INT(ms_wl_parse(acc_toks5, strlen(acc_toks5), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 5);
    ms_wl_free(&w);

    /* Wrong modifier for access -> line 5 */
    const char *acc_bad_mod = "schema 1\nkind paging\npage_size 4096\nframes 8\naccess P1 0 r\n";
    CHECK_EQ_INT(ms_wl_parse(acc_bad_mod, strlen(acc_bad_mod), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 5);
    ms_wl_free(&w);

    /* Bad number: memory_size 0 -> line 3 */
    const char *bad_mem0 = "schema 1\nkind contiguous\nmemory_size 0\n";
    CHECK_EQ_INT(ms_wl_parse(bad_mem0, strlen(bad_mem0), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 3);
    ms_wl_free(&w);

    /* Bad number: memory_size abc -> line 3 */
    const char *bad_mem_abc = "schema 1\nkind contiguous\nmemory_size 100xyz\n";
    CHECK_EQ_INT(ms_wl_parse(bad_mem_abc, strlen(bad_mem_abc), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 3);
    ms_wl_free(&w);

    /* Bad number: frames 0 -> line 4 */
    const char *bad_frames = "schema 1\nkind paging\npage_size 4096\nframes 0\n";
    CHECK_EQ_INT(ms_wl_parse(bad_frames, strlen(bad_frames), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 4);
    ms_wl_free(&w);

    /* Bad number: alloc size non-number -> line 4 */
    const char *bad_alloc_sz =
        "schema 1\nkind contiguous\nmemory_size 1024\nalloc P1 A1 not_a_num\n";
    CHECK_EQ_INT(ms_wl_parse(bad_alloc_sz, strlen(bad_alloc_sz), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 4);
    ms_wl_free(&w);

    /* Bad number: spawn pages non-number -> line 5 */
    const char *bad_spawn_pg = "schema 1\nkind paging\npage_size 4096\nframes 8\nspawn P1 xyz\n";
    CHECK_EQ_INT(ms_wl_parse(bad_spawn_pg, strlen(bad_spawn_pg), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 5);
    ms_wl_free(&w);

    /* Bad number: access @ without digits -> line 5 */
    const char *bad_acc_at = "schema 1\nkind paging\npage_size 4096\nframes 8\naccess P1 @\n";
    CHECK_EQ_INT(ms_wl_parse(bad_acc_at, strlen(bad_acc_at), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 5);
    ms_wl_free(&w);

    /* Page size not a power of 2 -> line 3 */
    const char *non_pow2 = "schema 1\nkind paging\npage_size 3000\nframes 8\n";
    CHECK_EQ_INT(ms_wl_parse(non_pow2, strlen(non_pow2), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 3);
    ms_wl_free(&w);
}

static void test_semantic_checks(void) {
    ms_workload w;
    ms_wl_error err = {0};

    /* 1. Reused id: fails when negative_test 0 */
    const char *reused_pos = "schema 1\n"
                             "kind contiguous\n"
                             "memory_size 1024\n"
                             "alloc P1 A1 100\n"
                             "alloc P2 A1 200\n";
    CHECK_EQ_INT(ms_wl_parse(reused_pos, strlen(reused_pos), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 5);
    ms_wl_free(&w);

    /* Reused id: succeeds when negative_test 1 */
    const char *reused_neg = "schema 1\n"
                             "kind contiguous\n"
                             "memory_size 1024\n"
                             "negative_test 1\n"
                             "alloc P1 A1 100\n"
                             "alloc P2 A1 200\n";
    CHECK_EQ_INT(ms_wl_parse(reused_neg, strlen(reused_neg), &w, &err), MS_OK);
    CHECK_EQ_INT(w.negative_test, 1);
    CHECK_EQ_INT(w.n_ops, 2);
    ms_wl_free(&w);

    /* 2. Free of unknown id: fails when negative_test 0 */
    const char *free_unk_pos = "schema 1\n"
                               "kind contiguous\n"
                               "memory_size 1024\n"
                               "free P1 A1\n";
    CHECK_EQ_INT(ms_wl_parse(free_unk_pos, strlen(free_unk_pos), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 4);
    ms_wl_free(&w);

    /* Free of unknown id: succeeds when negative_test 1 */
    const char *free_unk_neg = "schema 1\n"
                               "kind contiguous\n"
                               "memory_size 1024\n"
                               "negative_test 1\n"
                               "free P1 A1\n";
    CHECK_EQ_INT(ms_wl_parse(free_unk_neg, strlen(free_unk_neg), &w, &err), MS_OK);
    CHECK_EQ_INT(w.negative_test, 1);
    CHECK_EQ_INT(w.n_ops, 1);
    ms_wl_free(&w);

    /* 3. Alloc size <= 0: fails when negative_test 0 */
    const char *sz_zero_pos = "schema 1\n"
                              "kind contiguous\n"
                              "memory_size 1024\n"
                              "alloc P1 A1 0\n";
    CHECK_EQ_INT(ms_wl_parse(sz_zero_pos, strlen(sz_zero_pos), &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 4);
    ms_wl_free(&w);

    /* Alloc size <= 0: succeeds when negative_test 1 */
    const char *sz_zero_neg = "schema 1\n"
                              "kind contiguous\n"
                              "memory_size 1024\n"
                              "negative_test 1\n"
                              "alloc P1 A1 -50\n";
    CHECK_EQ_INT(ms_wl_parse(sz_zero_neg, strlen(sz_zero_neg), &w, &err), MS_OK);
    CHECK_EQ_INT(w.negative_test, 1);
    CHECK_EQ_INT(w.n_ops, 1);
    CHECK_EQ_INT(w.ops[0].u.alloc.size, -50);
    ms_wl_free(&w);
}

static void test_free_zeroed(void) {
    ms_workload w = {0};
    ms_wl_free(&w);
    CHECK_EQ_INT(w.n_ops, 0);
    CHECK_EQ_INT(w.n_pids, 0);
    CHECK_EQ_INT(w.n_ids, 0);
}

static void test_load_missing_file(void) {
    ms_workload w;
    ms_wl_error err = {0};
    CHECK_EQ_INT(ms_wl_load("this_file_does_not_exist_at_all.wl", &w, &err), MS_ERR_INVALID);
    CHECK_EQ_INT(err.line, 0);
    ms_wl_free(&w);
}

int main(void) {
    RUN_TEST(test_contiguous_workload);
    RUN_TEST(test_paging_workload);
    RUN_TEST(test_always_errors);
    RUN_TEST(test_semantic_checks);
    RUN_TEST(test_free_zeroed);
    RUN_TEST(test_load_missing_file);
    TEST_EXIT();
}
