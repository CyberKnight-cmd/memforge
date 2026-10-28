/* Workloads: loader. Owner: P7. */
#include "memsim/workloads/loader.h"

#include <errno.h>
#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MS_WL_NAME_MAX 63
#define MS_WL_MAX_TOKENS 16

typedef struct {
    const char *start;
    size_t len;
} token_t;

typedef struct {
    char **names;
    size_t count;
    size_t capacity;
    uint32_t *table; /* 1-based index into names, 0 means empty */
    size_t table_cap;
} interner_t;

typedef struct {
    bool *allocated;
    size_t capacity;
} id_tracker_t;

typedef struct {
    interner_t pid_interner;
    interner_t id_interner;
    id_tracker_t id_tracker;
    size_t ops_cap;
    bool in_ops;
    bool has_schema;
    bool has_kind;
    ms_wl_kind kind_val;
    bool has_memory_size;
    size_t memory_size_line;
    int64_t memory_size_val;
    bool has_page_size;
    size_t page_size_line;
    uint32_t page_size_val;
    bool has_frames;
    size_t frames_line;
    uint32_t frames_val;
    bool has_negative_test;
    bool negative_test_val;
} parser_state_t;

static void set_error(ms_wl_error *err, size_t line, const char *fmt, ...) {
    if (err == NULL) {
        return;
    }
    err->line = line;
    va_list ap;
    va_start(ap, fmt);
    (void)vsnprintf(err->message, sizeof(err->message), fmt, ap);
    va_end(ap);
}

static char *duplicate_str(const char *s, size_t len) {
    char *copy = malloc(len + 1);
    if (copy != NULL) {
        memcpy(copy, s, len);
        copy[len] = '\0';
    }
    return copy;
}

static void interner_init(interner_t *it) {
    memset(it, 0, sizeof(*it));
}

static void interner_free(interner_t *it) {
    if (it == NULL) {
        return;
    }
    if (it->names != NULL) {
        for (size_t i = 0; i < it->count; i++) {
            free(it->names[i]);
        }
        free((void *)it->names);
        it->names = NULL;
    }
    free(it->table);
    it->table = NULL;
    it->count = 0;
    it->capacity = 0;
    it->table_cap = 0;
}

static uint32_t hash_str(const char *s) {
    uint32_t h = 2166136261U;
    while (*s != '\0') {
        h ^= (uint8_t)*s++;
        h *= 16777619U;
    }
    return h;
}

static ms_status interner_grow_table(interner_t *it) {
    size_t new_cap = it->table_cap == 0 ? 64 : it->table_cap * 2;
    uint32_t *new_table = calloc(new_cap, sizeof(uint32_t));
    if (new_table == NULL) {
        return MS_ERR_NOMEM;
    }
    for (size_t i = 0; i < it->count; i++) {
        uint32_t h = hash_str(it->names[i]);
        size_t mask = new_cap - 1;
        size_t idx = h & mask;
        while (new_table[idx] != 0) {
            idx = (idx + 1) & mask;
        }
        new_table[idx] = (uint32_t)(i + 1);
    }
    free(it->table);
    it->table = new_table;
    it->table_cap = new_cap;
    return MS_OK;
}

static ms_status interner_lookup_or_add(interner_t *it, const char *name, size_t name_len,
                                        uint32_t *out_id) {
    if (it->table_cap > 0) {
        uint32_t h = 2166136261U;
        for (size_t i = 0; i < name_len; i++) {
            h ^= (uint8_t)name[i];
            h *= 16777619U;
        }
        size_t mask = it->table_cap - 1;
        size_t idx = h & mask;
        while (it->table[idx] != 0) {
            uint32_t entry = it->table[idx] - 1;
            if (strncmp(it->names[entry], name, name_len) == 0 &&
                it->names[entry][name_len] == '\0') {
                *out_id = entry;
                return MS_OK;
            }
            idx = (idx + 1) & mask;
        }
    }

    if (it->table_cap == 0 || (it->count + 1) * 2 >= it->table_cap) {
        ms_status st = interner_grow_table(it);
        if (st != MS_OK) {
            return st;
        }
    }

    if (it->count >= it->capacity) {
        size_t new_cap = it->capacity == 0 ? 16 : it->capacity * 2;
        char **new_names = (char **)realloc((void *)it->names, new_cap * sizeof(char *));
        if (new_names == NULL) {
            return MS_ERR_NOMEM;
        }
        it->names = new_names;
        it->capacity = new_cap;
    }

    char *dup = duplicate_str(name, name_len);
    if (dup == NULL) {
        return MS_ERR_NOMEM;
    }
    size_t id = it->count;
    it->names[id] = dup;
    it->count++;

    uint32_t h = hash_str(dup);
    size_t mask = it->table_cap - 1;
    size_t idx = h & mask;
    while (it->table[idx] != 0) {
        idx = (idx + 1) & mask;
    }
    it->table[idx] = (uint32_t)(id + 1);

    *out_id = (uint32_t)id;
    return MS_OK;
}

static ms_status id_tracker_ensure_cap(id_tracker_t *tr, size_t needed) {
    if (needed <= tr->capacity) {
        return MS_OK;
    }
    size_t new_cap = tr->capacity == 0 ? 64 : tr->capacity * 2;
    while (new_cap < needed) {
        new_cap *= 2;
    }
    bool *new_arr = realloc(tr->allocated, new_cap * sizeof(bool));
    if (new_arr == NULL) {
        return MS_ERR_NOMEM;
    }
    memset(new_arr + tr->capacity, 0, (new_cap - tr->capacity) * sizeof(bool));
    tr->allocated = new_arr;
    tr->capacity = new_cap;
    return MS_OK;
}

static bool parse_int64(const char *str, int64_t *out) {
    if (str == NULL || *str == '\0') {
        return false;
    }
    const char *p = str;
    if (*p == '-' || *p == '+') {
        p++;
    }
    if (*p == '\0') {
        return false;
    }
    while (*p != '\0') {
        if (*p < '0' || *p > '9') {
            return false;
        }
        p++;
    }
    errno = 0;
    char *endptr = NULL;
    long long val = strtoll(str, &endptr, 10);
    if (errno == ERANGE || endptr == str || *endptr != '\0') {
        return false;
    }
    *out = (int64_t)val;
    return true;
}

static bool parse_uint32(const char *str, uint32_t *out) {
    if (str == NULL || *str == '\0') {
        return false;
    }
    const char *p = str;
    if (*p == '+') {
        p++;
    }
    if (*p == '\0') {
        return false;
    }
    while (*p != '\0') {
        if (*p < '0' || *p > '9') {
            return false;
        }
        p++;
    }
    errno = 0;
    char *endptr = NULL;
    unsigned long long val = strtoull(str, &endptr, 10);
    if (errno == ERANGE || endptr == str || *endptr != '\0' || val > UINT32_MAX) {
        return false;
    }
    *out = (uint32_t)val;
    return true;
}

static bool parse_uint64(const char *str, uint64_t *out) {
    if (str == NULL || *str == '\0') {
        return false;
    }
    const char *p = str;
    if (*p == '+') {
        p++;
    }
    if (*p == '\0') {
        return false;
    }
    while (*p != '\0') {
        if (*p < '0' || *p > '9') {
            return false;
        }
        p++;
    }
    errno = 0;
    char *endptr = NULL;
    unsigned long long val = strtoull(str, &endptr, 10);
    if (errno == ERANGE || endptr == str || *endptr != '\0') {
        return false;
    }
    *out = (uint64_t)val;
    return true;
}

static inline bool is_power_of_two_u32(uint32_t v) {
    if (v == 0) {
        return false;
    }
    return (v & (v - 1)) == 0 ? true : false;
}

static bool token_to_str(token_t tok, char *buf, size_t buf_size) {
    if (tok.len >= buf_size) {
        return false;
    }
    memcpy(buf, tok.start, tok.len);
    buf[tok.len] = '\0';
    return true;
}

static bool is_known_header_key(const char *key) {
    if (strcmp(key, "schema") == 0) {
        return true;
    }
    if (strcmp(key, "kind") == 0) {
        return true;
    }
    if (strcmp(key, "memory_size") == 0) {
        return true;
    }
    if (strcmp(key, "page_size") == 0) {
        return true;
    }
    if (strcmp(key, "frames") == 0) {
        return true;
    }
    return strcmp(key, "negative_test") == 0 ? true : false;
}

static bool is_known_op(const char *name) {
    if (strcmp(name, "alloc") == 0) {
        return true;
    }
    if (strcmp(name, "free") == 0) {
        return true;
    }
    if (strcmp(name, "compact") == 0) {
        return true;
    }
    if (strcmp(name, "spawn") == 0) {
        return true;
    }
    if (strcmp(name, "access") == 0) {
        return true;
    }
    return strcmp(name, "terminate") == 0 ? true : false;
}

static ms_status check_headers_complete(const parser_state_t *st, ms_wl_error *err) {
    if (!st->has_schema) {
        set_error(err, 0, "missing required header: schema");
        return MS_ERR_INVALID;
    }
    if (!st->has_kind) {
        set_error(err, 0, "missing required header: kind");
        return MS_ERR_INVALID;
    }
    if (st->kind_val == MS_WL_CONTIGUOUS) {
        if (st->page_size_line != 0) {
            set_error(err, st->page_size_line, "page_size not allowed for contiguous workload");
            return MS_ERR_INVALID;
        }
        if (st->frames_line != 0) {
            set_error(err, st->frames_line, "frames not allowed for contiguous workload");
            return MS_ERR_INVALID;
        }
        if (!st->has_memory_size) {
            set_error(err, 0, "missing required header: memory_size");
            return MS_ERR_INVALID;
        }
    } else {
        if (st->memory_size_line != 0) {
            set_error(err, st->memory_size_line, "memory_size not allowed for paging workload");
            return MS_ERR_INVALID;
        }
        if (!st->has_page_size) {
            set_error(err, 0, "missing required header: page_size");
            return MS_ERR_INVALID;
        }
        if (!st->has_frames) {
            set_error(err, 0, "missing required header: frames");
            return MS_ERR_INVALID;
        }
    }
    return MS_OK;
}

static ms_status append_op(ms_workload *out, size_t *ops_cap, const ms_op *op) {
    if (out->n_ops >= *ops_cap) {
        size_t new_cap = *ops_cap == 0 ? 32 : *ops_cap * 2;
        ms_op *new_ops = realloc(out->ops, new_cap * sizeof(ms_op));
        if (new_ops == NULL) {
            return MS_ERR_NOMEM;
        }
        out->ops = new_ops;
        *ops_cap = new_cap;
    }
    out->ops[out->n_ops] = *op;
    out->n_ops++;
    return MS_OK;
}

void ms_wl_free(ms_workload *w) {
    if (w == NULL) {
        return;
    }
    free(w->ops);
    w->ops = NULL;
    w->n_ops = 0;
    if (w->pid_names != NULL) {
        for (size_t i = 0; i < w->n_pids; i++) {
            free(w->pid_names[i]);
        }
        free((void *)w->pid_names);
        w->pid_names = NULL;
        w->n_pids = 0;
    }
    if (w->id_names != NULL) {
        for (size_t i = 0; i < w->n_ids; i++) {
            free(w->id_names[i]);
        }
        free((void *)w->id_names);
        w->id_names = NULL;
        w->n_ids = 0;
    }
    memset(w, 0, sizeof(*w));
}

static ms_status parse_header_schema(parser_state_t *st, const char *val_str, size_t line_num,
                                     ms_wl_error *err) {
    if (st->has_schema) {
        set_error(err, line_num, "duplicate header key: schema");
        return MS_ERR_INVALID;
    }
    uint32_t val = 0;
    if (!parse_uint32(val_str, &val)) {
        set_error(err, line_num, "bad number for schema");
        return MS_ERR_INVALID;
    }
    if (val != 1) {
        set_error(err, line_num, "unknown schema: %u", val);
        return MS_ERR_INVALID;
    }
    st->has_schema = true;
    return MS_OK;
}

static ms_status parse_header_kind(parser_state_t *st, const char *val_str, size_t line_num,
                                   ms_wl_error *err) {
    if (st->has_kind) {
        set_error(err, line_num, "duplicate header key: kind");
        return MS_ERR_INVALID;
    }
    if (strcmp(val_str, "contiguous") == 0) {
        st->kind_val = MS_WL_CONTIGUOUS;
    } else if (strcmp(val_str, "paging") == 0) {
        st->kind_val = MS_WL_PAGING;
    } else {
        set_error(err, line_num, "unknown kind: %s", val_str);
        return MS_ERR_INVALID;
    }
    st->has_kind = true;
    return MS_OK;
}

static ms_status parse_header_mem_size(parser_state_t *st, const char *val_str, size_t line_num,
                                       ms_wl_error *err) {
    if (st->has_memory_size) {
        set_error(err, line_num, "duplicate header key: memory_size");
        return MS_ERR_INVALID;
    }
    int64_t val = 0;
    if (!parse_int64(val_str, &val) || val <= 0) {
        set_error(err, line_num, "bad number for memory_size");
        return MS_ERR_INVALID;
    }
    st->has_memory_size = true;
    st->memory_size_line = line_num;
    st->memory_size_val = val;
    return MS_OK;
}

static ms_status parse_header_page_size(parser_state_t *st, const char *val_str, size_t line_num,
                                        ms_wl_error *err) {
    if (st->has_page_size) {
        set_error(err, line_num, "duplicate header key: page_size");
        return MS_ERR_INVALID;
    }
    uint32_t val = 0;
    if (!parse_uint32(val_str, &val) || val == 0) {
        set_error(err, line_num, "bad number for page_size");
        return MS_ERR_INVALID;
    }
    if (!is_power_of_two_u32(val)) {
        set_error(err, line_num, "page_size not a power of 2: %u", val);
        return MS_ERR_INVALID;
    }
    st->has_page_size = true;
    st->page_size_line = line_num;
    st->page_size_val = val;
    return MS_OK;
}

static ms_status parse_header_frames(parser_state_t *st, const char *val_str, size_t line_num,
                                     ms_wl_error *err) {
    if (st->has_frames) {
        set_error(err, line_num, "duplicate header key: frames");
        return MS_ERR_INVALID;
    }
    uint32_t val = 0;
    if (!parse_uint32(val_str, &val) || val == 0) {
        set_error(err, line_num, "bad number for frames");
        return MS_ERR_INVALID;
    }
    st->has_frames = true;
    st->frames_line = line_num;
    st->frames_val = val;
    return MS_OK;
}

static ms_status parse_header_negative_test(parser_state_t *st, const char *val_str,
                                            size_t line_num, ms_wl_error *err) {
    if (st->has_negative_test) {
        set_error(err, line_num, "duplicate header key: negative_test");
        return MS_ERR_INVALID;
    }
    uint32_t val = 0;
    if (!parse_uint32(val_str, &val) || (val != 0 && val != 1)) {
        set_error(err, line_num, "bad number for negative_test");
        return MS_ERR_INVALID;
    }
    st->has_negative_test = true;
    st->negative_test_val = (val == 1);
    return MS_OK;
}

static ms_status parse_header_line(parser_state_t *st, const token_t *tokens, size_t n_tokens,
                                   size_t line_num, ms_wl_error *err) {
    char tok0[64];
    if (!token_to_str(tokens[0], tok0, sizeof(tok0))) {
        set_error(err, line_num, "token too long");
        return MS_ERR_INVALID;
    }

    if (strncmp(tok0, "gen_", 4) == 0) {
        return MS_OK;
    }

    if (!is_known_header_key(tok0)) {
        if (n_tokens == 2) {
            set_error(err, line_num, "unknown key: %s", tok0);
        } else {
            set_error(err, line_num, "unknown op: %s", tok0);
        }
        return MS_ERR_INVALID;
    }

    if (n_tokens != 2) {
        set_error(err, line_num, "wrong token count for header key '%s'", tok0);
        return MS_ERR_INVALID;
    }

    char tok1[64];
    if (!token_to_str(tokens[1], tok1, sizeof(tok1))) {
        set_error(err, line_num, "header value too long");
        return MS_ERR_INVALID;
    }

    if (strcmp(tok0, "schema") == 0) {
        return parse_header_schema(st, tok1, line_num, err);
    }
    if (strcmp(tok0, "kind") == 0) {
        return parse_header_kind(st, tok1, line_num, err);
    }
    if (strcmp(tok0, "memory_size") == 0) {
        return parse_header_mem_size(st, tok1, line_num, err);
    }
    if (strcmp(tok0, "page_size") == 0) {
        return parse_header_page_size(st, tok1, line_num, err);
    }
    if (strcmp(tok0, "frames") == 0) {
        return parse_header_frames(st, tok1, line_num, err);
    }
    return parse_header_negative_test(st, tok1, line_num, err);
}

static ms_status parse_op_alloc(parser_state_t *st, const token_t *tokens, size_t n_tokens,
                                size_t line_num, ms_op *op, ms_wl_error *err) {
    if (st->kind_val != MS_WL_CONTIGUOUS) {
        set_error(err, line_num, "op 'alloc' not allowed for paging workload");
        return MS_ERR_INVALID;
    }
    if (n_tokens != 4) {
        set_error(err, line_num, "wrong token count for alloc");
        return MS_ERR_INVALID;
    }
    if (tokens[1].len > MS_WL_NAME_MAX) {
        set_error(err, line_num, "pid name exceeds 63 characters");
        return MS_ERR_INVALID;
    }
    if (tokens[2].len > MS_WL_NAME_MAX) {
        set_error(err, line_num, "alloc id exceeds 63 characters");
        return MS_ERR_INVALID;
    }

    char tok_size[64];
    if (!token_to_str(tokens[3], tok_size, sizeof(tok_size))) {
        set_error(err, line_num, "bad number for alloc size");
        return MS_ERR_INVALID;
    }
    int64_t size = 0;
    if (!parse_int64(tok_size, &size)) {
        set_error(err, line_num, "bad number for alloc size");
        return MS_ERR_INVALID;
    }
    if (!st->negative_test_val && size <= 0) {
        set_error(err, line_num, "alloc size <= 0");
        return MS_ERR_INVALID;
    }

    ms_status res =
        interner_lookup_or_add(&st->pid_interner, tokens[1].start, tokens[1].len, &op->u.alloc.pid);
    if (res != MS_OK) {
        return res;
    }
    res = interner_lookup_or_add(&st->id_interner, tokens[2].start, tokens[2].len, &op->u.alloc.id);
    if (res != MS_OK) {
        return res;
    }

    res = id_tracker_ensure_cap(&st->id_tracker, op->u.alloc.id + 1);
    if (res != MS_OK) {
        return res;
    }
    if (st->id_tracker.allocated[op->u.alloc.id] && !st->negative_test_val) {
        char id_name[64];
        (void)token_to_str(tokens[2], id_name, sizeof(id_name));
        set_error(err, line_num, "alloc id '%s' reused", id_name);
        return MS_ERR_INVALID;
    }
    st->id_tracker.allocated[op->u.alloc.id] = true;

    op->kind = MS_OP_ALLOC;
    op->u.alloc.size = size;
    return MS_OK;
}

static ms_status parse_op_free(parser_state_t *st, const token_t *tokens, size_t n_tokens,
                               size_t line_num, ms_op *op, ms_wl_error *err) {
    if (st->kind_val != MS_WL_CONTIGUOUS) {
        set_error(err, line_num, "op 'free' not allowed for paging workload");
        return MS_ERR_INVALID;
    }
    if (n_tokens != 3) {
        set_error(err, line_num, "wrong token count for free");
        return MS_ERR_INVALID;
    }
    if (tokens[1].len > MS_WL_NAME_MAX) {
        set_error(err, line_num, "pid name exceeds 63 characters");
        return MS_ERR_INVALID;
    }
    if (tokens[2].len > MS_WL_NAME_MAX) {
        set_error(err, line_num, "alloc id exceeds 63 characters");
        return MS_ERR_INVALID;
    }

    ms_status res =
        interner_lookup_or_add(&st->pid_interner, tokens[1].start, tokens[1].len, &op->u.free.pid);
    if (res != MS_OK) {
        return res;
    }
    res = interner_lookup_or_add(&st->id_interner, tokens[2].start, tokens[2].len, &op->u.free.id);
    if (res != MS_OK) {
        return res;
    }

    if ((op->u.free.id >= st->id_tracker.capacity || !st->id_tracker.allocated[op->u.free.id]) &&
        !st->negative_test_val) {
        char id_name[64];
        (void)token_to_str(tokens[2], id_name, sizeof(id_name));
        set_error(err, line_num, "free of unallocated id '%s'", id_name);
        return MS_ERR_INVALID;
    }

    op->kind = MS_OP_FREE;
    return MS_OK;
}

static ms_status parse_op_compact(const parser_state_t *st, size_t n_tokens, size_t line_num,
                                  ms_op *op, ms_wl_error *err) {
    if (st->kind_val != MS_WL_CONTIGUOUS) {
        set_error(err, line_num, "op 'compact' not allowed for paging workload");
        return MS_ERR_INVALID;
    }
    if (n_tokens != 1) {
        set_error(err, line_num, "wrong token count for compact");
        return MS_ERR_INVALID;
    }
    op->kind = MS_OP_COMPACT;
    return MS_OK;
}

static ms_status parse_op_spawn(parser_state_t *st, const token_t *tokens, size_t n_tokens,
                                size_t line_num, ms_op *op, ms_wl_error *err) {
    if (st->kind_val != MS_WL_PAGING) {
        set_error(err, line_num, "op 'spawn' not allowed for contiguous workload");
        return MS_ERR_INVALID;
    }
    if (n_tokens != 3) {
        set_error(err, line_num, "wrong token count for spawn");
        return MS_ERR_INVALID;
    }
    if (tokens[1].len > MS_WL_NAME_MAX) {
        set_error(err, line_num, "pid name exceeds 63 characters");
        return MS_ERR_INVALID;
    }

    char tok_pages[64];
    if (!token_to_str(tokens[2], tok_pages, sizeof(tok_pages))) {
        set_error(err, line_num, "bad number for spawn pages");
        return MS_ERR_INVALID;
    }
    uint32_t pages = 0;
    if (!parse_uint32(tok_pages, &pages)) {
        set_error(err, line_num, "bad number for spawn pages");
        return MS_ERR_INVALID;
    }

    ms_status res =
        interner_lookup_or_add(&st->pid_interner, tokens[1].start, tokens[1].len, &op->u.spawn.pid);
    if (res != MS_OK) {
        return res;
    }
    op->kind = MS_OP_SPAWN;
    op->u.spawn.pages = pages;
    return MS_OK;
}

static ms_status parse_op_access(parser_state_t *st, const token_t *tokens, size_t n_tokens,
                                 size_t line_num, ms_op *op, ms_wl_error *err) {
    if (st->kind_val != MS_WL_PAGING) {
        set_error(err, line_num, "op 'access' not allowed for contiguous workload");
        return MS_ERR_INVALID;
    }
    if (n_tokens < 3 || n_tokens > 4) {
        set_error(err, line_num, "wrong token count for access");
        return MS_ERR_INVALID;
    }
    if (tokens[1].len > MS_WL_NAME_MAX) {
        set_error(err, line_num, "pid name exceeds 63 characters");
        return MS_ERR_INVALID;
    }

    ms_status res = interner_lookup_or_add(&st->pid_interner, tokens[1].start, tokens[1].len,
                                           &op->u.access.pid);
    if (res != MS_OK) {
        return res;
    }

    char tok_vpn[64];
    if (!token_to_str(tokens[2], tok_vpn, sizeof(tok_vpn))) {
        set_error(err, line_num, "bad number for access target");
        return MS_ERR_INVALID;
    }

    if (tok_vpn[0] == '@') {
        op->u.access.by_va = true;
        if (!parse_uint64(tok_vpn + 1, &op->u.access.vpn_or_va)) {
            set_error(err, line_num, "bad number for access virtual address");
            return MS_ERR_INVALID;
        }
    } else {
        op->u.access.by_va = false;
        if (!parse_uint64(tok_vpn, &op->u.access.vpn_or_va)) {
            set_error(err, line_num, "bad number for access vpn");
            return MS_ERR_INVALID;
        }
    }

    if (n_tokens == 4) {
        char tok_w[64];
        (void)token_to_str(tokens[3], tok_w, sizeof(tok_w));
        if (strcmp(tok_w, "w") != 0) {
            set_error(err, line_num, "invalid access modifier '%s', expected 'w'", tok_w);
            return MS_ERR_INVALID;
        }
        op->u.access.write = true;
    } else {
        op->u.access.write = false;
    }
    op->kind = MS_OP_ACCESS;
    return MS_OK;
}

static ms_status parse_op_terminate(parser_state_t *st, const token_t *tokens, size_t n_tokens,
                                    size_t line_num, ms_op *op, ms_wl_error *err) {
    if (n_tokens != 2) {
        set_error(err, line_num, "wrong token count for terminate");
        return MS_ERR_INVALID;
    }
    if (tokens[1].len > MS_WL_NAME_MAX) {
        set_error(err, line_num, "pid name exceeds 63 characters");
        return MS_ERR_INVALID;
    }
    ms_status res = interner_lookup_or_add(&st->pid_interner, tokens[1].start, tokens[1].len,
                                           &op->u.terminate.pid);
    if (res != MS_OK) {
        return res;
    }
    op->kind = MS_OP_TERMINATE;
    return MS_OK;
}

static ms_status parse_op_line(parser_state_t *st, const char *tok0, const token_t *tokens,
                               size_t n_tokens, size_t line_num, ms_op *op, ms_wl_error *err) {
    if (strcmp(tok0, "alloc") == 0) {
        return parse_op_alloc(st, tokens, n_tokens, line_num, op, err);
    }
    if (strcmp(tok0, "free") == 0) {
        return parse_op_free(st, tokens, n_tokens, line_num, op, err);
    }
    if (strcmp(tok0, "compact") == 0) {
        return parse_op_compact(st, n_tokens, line_num, op, err);
    }
    if (strcmp(tok0, "spawn") == 0) {
        return parse_op_spawn(st, tokens, n_tokens, line_num, op, err);
    }
    if (strcmp(tok0, "access") == 0) {
        return parse_op_access(st, tokens, n_tokens, line_num, op, err);
    }
    if (strcmp(tok0, "terminate") == 0) {
        return parse_op_terminate(st, tokens, n_tokens, line_num, op, err);
    }
    set_error(err, line_num, "unknown op: %s", tok0);
    return MS_ERR_INVALID;
}

static size_t tokenize_line(const char *text, size_t line_start, size_t code_end, token_t *tokens,
                            size_t max_tokens) {
    size_t n_tokens = 0;
    size_t p = line_start;
    while (p < code_end) {
        while (p < code_end && (text[p] == ' ' || text[p] == '\t' || text[p] == '\r')) {
            p++;
        }
        if (p >= code_end) {
            break;
        }
        size_t tok_start = p;
        while (p < code_end && text[p] != ' ' && text[p] != '\t' && text[p] != '\r') {
            p++;
        }
        if (n_tokens < max_tokens) {
            tokens[n_tokens].start = text + tok_start;
            tokens[n_tokens].len = p - tok_start;
        }
        n_tokens++;
    }
    return n_tokens;
}

static ms_status process_line(parser_state_t *st, const char *text, size_t line_start,
                              size_t line_end, size_t line_num, ms_workload *out,
                              ms_wl_error *err) {
    while (line_end > line_start && (text[line_end - 1] == '\r' || text[line_end - 1] == '\n')) {
        line_end--;
    }

    size_t code_end = line_start;
    while (code_end < line_end && text[code_end] != '#') {
        code_end++;
    }

    token_t tokens[MS_WL_MAX_TOKENS];
    size_t n_tokens = tokenize_line(text, line_start, code_end, tokens, MS_WL_MAX_TOKENS);
    if (n_tokens == 0) {
        return MS_OK;
    }

    char tok0[64];
    if (!token_to_str(tokens[0], tok0, sizeof(tok0))) {
        set_error(err, line_num, "token too long on line %zu", line_num);
        return MS_ERR_INVALID;
    }

    if (!st->in_ops) {
        if (is_known_op(tok0)) {
            ms_status status = check_headers_complete(st, err);
            if (status != MS_OK) {
                return status;
            }
            st->in_ops = true;
        } else {
            return parse_header_line(st, tokens, n_tokens, line_num, err);
        }
    }

    ms_op op;
    memset(&op, 0, sizeof(op));
    ms_status status = parse_op_line(st, tok0, tokens, n_tokens, line_num, &op, err);
    if (status != MS_OK) {
        return status;
    }

    return append_op(out, &st->ops_cap, &op);
}

static void finalize_workload(parser_state_t *st, ms_workload *out) {
    out->kind = st->kind_val;
    out->memory_size = (st->kind_val == MS_WL_CONTIGUOUS) ? st->memory_size_val : 0;
    out->page_size = (st->kind_val == MS_WL_PAGING) ? st->page_size_val : 0;
    out->frames = (st->kind_val == MS_WL_PAGING) ? st->frames_val : 0;
    out->negative_test = st->negative_test_val;

    out->pid_names = st->pid_interner.names;
    out->n_pids = st->pid_interner.count;
    st->pid_interner.names = NULL;
    st->pid_interner.count = 0;

    out->id_names = st->id_interner.names;
    out->n_ids = st->id_interner.count;
    st->id_interner.names = NULL;
    st->id_interner.count = 0;
}

ms_status ms_wl_parse(const char *text, size_t len, ms_workload *out, ms_wl_error *err) {
    if (out == NULL) {
        return MS_ERR_INVALID;
    }
    memset(out, 0, sizeof(*out));
    if (text == NULL && len > 0) {
        set_error(err, 0, "null text with non-zero length");
        return MS_ERR_INVALID;
    }

    parser_state_t st;
    memset(&st, 0, sizeof(st));
    interner_init(&st.pid_interner);
    interner_init(&st.id_interner);

    ms_status status = MS_OK;
    size_t pos = 0;
    size_t line_num = 1;

    while (pos < len) {
        size_t line_start = pos;
        while (pos < len && text[pos] != '\n') {
            pos++;
        }
        size_t line_end = pos;
        if (pos < len && text[pos] == '\n') {
            pos++;
        }

        status = process_line(&st, text, line_start, line_end, line_num, out, err);
        if (status != MS_OK) {
            goto cleanup;
        }

        line_num++;
    }

    if (!st.in_ops) {
        status = check_headers_complete(&st, err);
        if (status != MS_OK) {
            goto cleanup;
        }
    }

    finalize_workload(&st, out);

cleanup:
    interner_free(&st.pid_interner);
    interner_free(&st.id_interner);
    free(st.id_tracker.allocated);

    if (status != MS_OK) {
        ms_wl_free(out);
    }
    return status;
}

ms_status ms_wl_load(const char *path, ms_workload *out, ms_wl_error *err) {
    if (out == NULL) {
        return MS_ERR_INVALID;
    }
    memset(out, 0, sizeof(*out));
    if (path == NULL) {
        set_error(err, 0, "null path");
        return MS_ERR_INVALID;
    }
    FILE *f = fopen(path, "rb");
    if (f == NULL) {
        set_error(err, 0, "cannot open file: %s", path);
        return MS_ERR_INVALID;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        (void)fclose(f);
        set_error(err, 0, "cannot seek file: %s", path);
        return MS_ERR_INVALID;
    }
    long size = ftell(f);
    if (size < 0 || (unsigned long)size > 1000000000UL) {
        (void)fclose(f);
        set_error(err, 0, "invalid file size: %s", path);
        return MS_ERR_INVALID;
    }
    if (fseek(f, 0, SEEK_SET) != 0) {
        (void)fclose(f);
        set_error(err, 0, "cannot rewind file: %s", path);
        return MS_ERR_INVALID;
    }
    size_t usize = (size_t)size;
    char *buf = calloc(usize + 1, sizeof(char));
    if (buf == NULL) {
        (void)fclose(f);
        return MS_ERR_NOMEM;
    }
    size_t read_bytes = fread(buf, 1, usize, f);
    (void)fclose(f);
    if (read_bytes != usize) {
        free(buf);
        set_error(err, 0, "read error on file: %s", path);
        return MS_ERR_INVALID;
    }
    ms_status st = ms_wl_parse(buf, usize, out, err);
    free(buf);
    return st;
}
