/* Shared kernel: core types (README section 6). Owner: P1. */
#ifndef MEMSIM_COMMON_TYPES_H_
#define MEMSIM_COMMON_TYPES_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MEMSIM_VERSION_MAJOR 0
#define MEMSIM_VERSION_MINOR 1
#define MEMSIM_VERSION_PATCH 0

typedef uint32_t ms_pid;      /* interned "P1", "SYS" */
typedef uint32_t ms_alloc_id; /* interned, workload-assigned, never reused */
#define MS_NONE UINT32_MAX    /* no pid / no id */

typedef struct {
    ms_pid pid;
    uint32_t vpn;
} ms_page_key;

typedef struct {
    int64_t start, size;
} ms_hole; /* half-open [start, start + size) */
static inline int64_t ms_hole_end(ms_hole h) {
    return h.start + h.size;
}

typedef enum { MS_BLOCK_FREE, MS_BLOCK_ALLOCATED } ms_block_state;

typedef struct {
    int64_t start; /* changes only through compaction */
    int64_t size;  /* granted size, >= requested */
    ms_block_state state;
    ms_pid pid;           /* MS_NONE iff FREE */
    ms_alloc_id alloc_id; /* MS_NONE iff FREE */
    int64_t requested;    /* internal fragmentation = size - requested */
} ms_block;

/* Commands: a tagged union; on ACCESS exactly one of vpn / va is set (the loader checks) */
typedef enum {
    MS_OP_ALLOC,
    MS_OP_FREE,
    MS_OP_TERMINATE,
    MS_OP_COMPACT,
    MS_OP_SPAWN,
    MS_OP_ACCESS
} ms_op_kind;

typedef struct {
    ms_op_kind kind;
    union {
        struct {
            ms_pid pid;
            ms_alloc_id id;
            int64_t size;
        } alloc;
        struct {
            ms_pid pid;
            ms_alloc_id id;
        } free;
        struct {
            ms_pid pid;
        } terminate;
        struct {
            ms_pid pid;
            uint32_t pages;
        } spawn;
        struct {
            ms_pid pid;
            bool by_va;
            uint64_t vpn_or_va;
            bool write;
        } access;
    } u;
} ms_op;

#endif /* MEMSIM_COMMON_TYPES_H_ */
