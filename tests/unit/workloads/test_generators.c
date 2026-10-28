/* Workload generators tests. Owner: P7. */
#include "memsim/workloads/generators.h"
#include "memsim/workloads/loader.h"
#include "test.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char *read_file_to_string(FILE *f, size_t *out_len) {
    if (fseek(f, 0, SEEK_END) != 0) {
        return NULL;
    }
    long sz = ftell(f);
    if (sz < 0) {
        return NULL;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        return NULL;
    }
    char *buf = malloc((size_t)sz + 1);
    if (buf == NULL) {
        return NULL;
    }
    size_t read_bytes = fread(buf, 1, (size_t)sz, f);
    buf[read_bytes] = '\0';
    if (out_len != NULL) {
        *out_len = read_bytes;
    }
    return buf;
}

static void test_alloc_uniform_valid_and_parse(void) {
    FILE *f = tmpfile();
    CHECK(f != NULL);

    ms_wl_alloc_uniform_params p = {
        .seed = 42,
        .ops = 200,
        .memory_size = 1048576,
        .size_min = 64,
        .size_max = 4096,
        .p_alloc_ppm = 600000,
    };

    ms_status st = ms_wl_gen_alloc_uniform(&p, f);
    CHECK_EQ_INT(st, MS_OK);

    size_t len = 0;
    char *text = read_file_to_string(f, &len);
    (void)fclose(f);
    CHECK(text != NULL);

    ms_workload w;
    ms_wl_error err = {0};
    ms_status pst = ms_wl_parse(text, len, &w, &err);
    CHECK_EQ_INT(pst, MS_OK);
    CHECK_EQ_INT(w.kind, MS_WL_CONTIGUOUS);
    CHECK_EQ_INT(w.memory_size, p.memory_size);
    CHECK_EQ_INT(w.n_ops, p.ops);

    for (size_t i = 0; i < w.n_ops; i++) {
        CHECK(w.ops[i].kind == MS_OP_ALLOC || w.ops[i].kind == MS_OP_FREE);
        if (w.ops[i].kind == MS_OP_ALLOC) {
            CHECK(w.ops[i].u.alloc.size >= p.size_min);
            CHECK(w.ops[i].u.alloc.size <= p.size_max);
        }
    }

    ms_wl_free(&w);
    free(text);
}

static void test_refs_locality_valid_and_parse(void) {
    FILE *f = tmpfile();
    CHECK(f != NULL);

    ms_wl_refs_locality_params p = {
        .seed = 12345,
        .n = 500,
        .pages = 64,
        .working_set = 16,
        .shift_every = 50,
        .p_out_ppm = 100000,
        .p_write_ppm = 200000,
        .frames = 8,
        .page_size = 4096,
    };

    ms_status st = ms_wl_gen_refs_locality(&p, f);
    CHECK_EQ_INT(st, MS_OK);

    size_t len = 0;
    char *text = read_file_to_string(f, &len);
    (void)fclose(f);
    CHECK(text != NULL);

    ms_workload w;
    ms_wl_error err = {0};
    ms_status pst = ms_wl_parse(text, len, &w, &err);
    CHECK_EQ_INT(pst, MS_OK);
    CHECK_EQ_INT(w.kind, MS_WL_PAGING);
    CHECK_EQ_INT(w.page_size, p.page_size);
    CHECK_EQ_INT(w.frames, p.frames);
    CHECK_EQ_INT(w.n_ops, p.n + 1);

    CHECK_EQ_INT(w.ops[0].kind, MS_OP_SPAWN);
    CHECK_EQ_INT(w.ops[0].u.spawn.pages, p.pages);

    for (size_t i = 1; i < w.n_ops; i++) {
        CHECK_EQ_INT(w.ops[i].kind, MS_OP_ACCESS);
        CHECK(!w.ops[i].u.access.by_va);
        CHECK(w.ops[i].u.access.vpn_or_va < p.pages);
    }

    ms_wl_free(&w);
    free(text);
}

static void test_same_seed_deterministic(void) {
    ms_wl_alloc_uniform_params ap = {
        .seed = 98765,
        .ops = 100,
        .memory_size = 65536,
        .size_min = 16,
        .size_max = 256,
        .p_alloc_ppm = 500000,
    };
    FILE *af1 = tmpfile();
    FILE *af2 = tmpfile();
    CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&ap, af1), MS_OK);
    CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&ap, af2), MS_OK);
    size_t alen1 = 0;
    size_t alen2 = 0;
    char *atxt1 = read_file_to_string(af1, &alen1);
    char *atxt2 = read_file_to_string(af2, &alen2);
    (void)fclose(af1);
    (void)fclose(af2);
    CHECK(atxt1 != NULL && atxt2 != NULL);
    CHECK_EQ_INT(alen1, alen2);
    CHECK(memcmp(atxt1, atxt2, alen1) == 0);
    free(atxt1);
    free(atxt2);

    ms_wl_refs_locality_params rp = {
        .seed = 54321,
        .n = 200,
        .pages = 32,
        .working_set = 8,
        .shift_every = 20,
        .p_out_ppm = 150000,
        .p_write_ppm = 300000,
        .frames = 4,
        .page_size = 2048,
    };
    FILE *rf1 = tmpfile();
    FILE *rf2 = tmpfile();
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&rp, rf1), MS_OK);
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&rp, rf2), MS_OK);
    size_t rlen1 = 0;
    size_t rlen2 = 0;
    char *rtxt1 = read_file_to_string(rf1, &rlen1);
    char *rtxt2 = read_file_to_string(rf2, &rlen2);
    (void)fclose(rf1);
    (void)fclose(rf2);
    CHECK(rtxt1 != NULL && rtxt2 != NULL);
    CHECK_EQ_INT(rlen1, rlen2);
    CHECK(memcmp(rtxt1, rtxt2, rlen1) == 0);
    free(rtxt1);
    free(rtxt2);
}

static void test_different_seeds_differ(void) {
    ms_wl_alloc_uniform_params ap1 = {
        .seed = 111,
        .ops = 100,
        .memory_size = 65536,
        .size_min = 16,
        .size_max = 256,
        .p_alloc_ppm = 500000,
    };
    ms_wl_alloc_uniform_params ap2 = ap1;
    ap2.seed = 222;

    FILE *af1 = tmpfile();
    FILE *af2 = tmpfile();
    CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&ap1, af1), MS_OK);
    CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&ap2, af2), MS_OK);
    size_t alen1 = 0;
    size_t alen2 = 0;
    char *atxt1 = read_file_to_string(af1, &alen1);
    char *atxt2 = read_file_to_string(af2, &alen2);
    (void)fclose(af1);
    (void)fclose(af2);
    CHECK(atxt1 != NULL && atxt2 != NULL);
    CHECK(alen1 != alen2 || memcmp(atxt1, atxt2, alen1) != 0);
    free(atxt1);
    free(atxt2);

    ms_wl_refs_locality_params rp1 = {
        .seed = 333,
        .n = 200,
        .pages = 32,
        .working_set = 8,
        .shift_every = 20,
        .p_out_ppm = 150000,
        .p_write_ppm = 300000,
        .frames = 4,
        .page_size = 2048,
    };
    ms_wl_refs_locality_params rp2 = rp1;
    rp2.seed = 444;

    FILE *rf1 = tmpfile();
    FILE *rf2 = tmpfile();
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&rp1, rf1), MS_OK);
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&rp2, rf2), MS_OK);
    size_t rlen1 = 0;
    size_t rlen2 = 0;
    char *rtxt1 = read_file_to_string(rf1, &rlen1);
    char *rtxt2 = read_file_to_string(rf2, &rlen2);
    (void)fclose(rf1);
    (void)fclose(rf2);
    CHECK(rtxt1 != NULL && rtxt2 != NULL);
    CHECK(rlen1 != rlen2 || memcmp(rtxt1, rtxt2, rlen1) != 0);
    free(rtxt1);
    free(rtxt2);
}

static void test_alloc_uniform_invalid_params(void) {
    FILE *f = tmpfile();
    CHECK(f != NULL);

    ms_wl_alloc_uniform_params valid = {
        .seed = 1,
        .ops = 10,
        .memory_size = 1000,
        .size_min = 10,
        .size_max = 50,
        .p_alloc_ppm = 500000,
    };

    CHECK_EQ_INT(ms_wl_gen_alloc_uniform(NULL, f), MS_ERR_INVALID);
    CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&valid, NULL), MS_ERR_INVALID);

    ms_wl_alloc_uniform_params p = valid;
    p.ops = 0;
    CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&p, f), MS_ERR_INVALID);

    p = valid;
    p.size_min = 0;
    CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&p, f), MS_ERR_INVALID);

    p = valid;
    p.size_min = -1;
    CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&p, f), MS_ERR_INVALID);

    p = valid;
    p.size_min = 60;
    p.size_max = 50;
    CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&p, f), MS_ERR_INVALID);

    p = valid;
    p.memory_size = 0;
    CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&p, f), MS_ERR_INVALID);

    p = valid;
    p.memory_size = -100;
    CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&p, f), MS_ERR_INVALID);

    p = valid;
    p.p_alloc_ppm = 1000001;
    CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&p, f), MS_ERR_INVALID);

    (void)fclose(f);
}

static void test_refs_locality_invalid_params(void) {
    FILE *f = tmpfile();
    CHECK(f != NULL);

    ms_wl_refs_locality_params valid = {
        .seed = 1,
        .n = 10,
        .pages = 16,
        .working_set = 4,
        .shift_every = 5,
        .p_out_ppm = 100000,
        .p_write_ppm = 100000,
        .frames = 4,
        .page_size = 4096,
    };

    CHECK_EQ_INT(ms_wl_gen_refs_locality(NULL, f), MS_ERR_INVALID);
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&valid, NULL), MS_ERR_INVALID);

    ms_wl_refs_locality_params p = valid;
    p.n = 0;
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&p, f), MS_ERR_INVALID);

    p = valid;
    p.pages = 0;
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&p, f), MS_ERR_INVALID);

    p = valid;
    p.working_set = 0;
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&p, f), MS_ERR_INVALID);

    p = valid;
    p.working_set = 20;
    p.pages = 16;
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&p, f), MS_ERR_INVALID);

    p = valid;
    p.shift_every = 0;
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&p, f), MS_ERR_INVALID);

    p = valid;
    p.frames = 0;
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&p, f), MS_ERR_INVALID);

    p = valid;
    p.page_size = 0;
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&p, f), MS_ERR_INVALID);

    p = valid;
    p.page_size = 3000;
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&p, f), MS_ERR_INVALID);

    p = valid;
    p.p_out_ppm = 1000001;
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&p, f), MS_ERR_INVALID);

    p = valid;
    p.p_write_ppm = 1000001;
    CHECK_EQ_INT(ms_wl_gen_refs_locality(&p, f), MS_ERR_INVALID);

    (void)fclose(f);
}

static void test_alloc_uniform_edge_probabilities(void) {
    /* 100% alloc: every op is alloc */
    {
        FILE *f = tmpfile();
        CHECK(f != NULL);
        ms_wl_alloc_uniform_params p = {
            .seed = 777,
            .ops = 50,
            .memory_size = 65536,
            .size_min = 10,
            .size_max = 20,
            .p_alloc_ppm = 1000000,
        };
        CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&p, f), MS_OK);
        size_t len = 0;
        char *text = read_file_to_string(f, &len);
        (void)fclose(f);
        CHECK(text != NULL);

        ms_workload w;
        ms_wl_error err = {0};
        CHECK_EQ_INT(ms_wl_parse(text, len, &w, &err), MS_OK);
        CHECK_EQ_INT(w.n_ops, 50);
        for (size_t i = 0; i < w.n_ops; i++) {
            CHECK_EQ_INT(w.ops[i].kind, MS_OP_ALLOC);
        }
        ms_wl_free(&w);
        free(text);
    }

    /* 0% alloc: alternates alloc then free */
    {
        FILE *f = tmpfile();
        CHECK(f != NULL);
        ms_wl_alloc_uniform_params p = {
            .seed = 888,
            .ops = 20,
            .memory_size = 65536,
            .size_min = 10,
            .size_max = 20,
            .p_alloc_ppm = 0,
        };
        CHECK_EQ_INT(ms_wl_gen_alloc_uniform(&p, f), MS_OK);
        size_t len = 0;
        char *text = read_file_to_string(f, &len);
        (void)fclose(f);
        CHECK(text != NULL);

        ms_workload w;
        ms_wl_error err = {0};
        CHECK_EQ_INT(ms_wl_parse(text, len, &w, &err), MS_OK);
        CHECK_EQ_INT(w.n_ops, 20);
        for (size_t i = 0; i < w.n_ops; i++) {
            if (i % 2 == 0) {
                CHECK_EQ_INT(w.ops[i].kind, MS_OP_ALLOC);
            } else {
                CHECK_EQ_INT(w.ops[i].kind, MS_OP_FREE);
            }
        }
        ms_wl_free(&w);
        free(text);
    }
}

static void test_refs_locality_edge_cases(void) {
    /* 100% write */
    {
        FILE *f = tmpfile();
        CHECK(f != NULL);
        ms_wl_refs_locality_params p = {
            .seed = 111,
            .n = 50,
            .pages = 16,
            .working_set = 8,
            .shift_every = 10,
            .p_out_ppm = 0,
            .p_write_ppm = 1000000,
            .frames = 4,
            .page_size = 4096,
        };
        CHECK_EQ_INT(ms_wl_gen_refs_locality(&p, f), MS_OK);
        size_t len = 0;
        char *text = read_file_to_string(f, &len);
        (void)fclose(f);
        CHECK(text != NULL);

        ms_workload w;
        ms_wl_error err = {0};
        CHECK_EQ_INT(ms_wl_parse(text, len, &w, &err), MS_OK);
        for (size_t i = 1; i < w.n_ops; i++) {
            CHECK_EQ_INT(w.ops[i].kind, MS_OP_ACCESS);
            CHECK(w.ops[i].u.access.write);
        }
        ms_wl_free(&w);
        free(text);
    }

    /* 0% write */
    {
        FILE *f = tmpfile();
        CHECK(f != NULL);
        ms_wl_refs_locality_params p = {
            .seed = 222,
            .n = 50,
            .pages = 16,
            .working_set = 8,
            .shift_every = 10,
            .p_out_ppm = 0,
            .p_write_ppm = 0,
            .frames = 4,
            .page_size = 4096,
        };
        CHECK_EQ_INT(ms_wl_gen_refs_locality(&p, f), MS_OK);
        size_t len = 0;
        char *text = read_file_to_string(f, &len);
        (void)fclose(f);
        CHECK(text != NULL);

        ms_workload w;
        ms_wl_error err = {0};
        CHECK_EQ_INT(ms_wl_parse(text, len, &w, &err), MS_OK);
        for (size_t i = 1; i < w.n_ops; i++) {
            CHECK_EQ_INT(w.ops[i].kind, MS_OP_ACCESS);
            CHECK(!w.ops[i].u.access.write);
        }
        ms_wl_free(&w);
        free(text);
    }

    /* working_set == pages */
    {
        FILE *f = tmpfile();
        CHECK(f != NULL);
        ms_wl_refs_locality_params p = {
            .seed = 333,
            .n = 50,
            .pages = 16,
            .working_set = 16,
            .shift_every = 10,
            .p_out_ppm = 500000,
            .p_write_ppm = 500000,
            .frames = 4,
            .page_size = 4096,
        };
        CHECK_EQ_INT(ms_wl_gen_refs_locality(&p, f), MS_OK);
        size_t len = 0;
        char *text = read_file_to_string(f, &len);
        (void)fclose(f);
        CHECK(text != NULL);

        ms_workload w;
        ms_wl_error err = {0};
        CHECK_EQ_INT(ms_wl_parse(text, len, &w, &err), MS_OK);
        for (size_t i = 1; i < w.n_ops; i++) {
            CHECK(w.ops[i].u.access.vpn_or_va < 16);
        }
        ms_wl_free(&w);
        free(text);
    }
}

int main(void) {
    RUN_TEST(test_alloc_uniform_valid_and_parse);
    RUN_TEST(test_refs_locality_valid_and_parse);
    RUN_TEST(test_same_seed_deterministic);
    RUN_TEST(test_different_seeds_differ);
    RUN_TEST(test_alloc_uniform_invalid_params);
    RUN_TEST(test_refs_locality_invalid_params);
    RUN_TEST(test_alloc_uniform_edge_probabilities);
    RUN_TEST(test_refs_locality_edge_cases);
    TEST_EXIT();
}
