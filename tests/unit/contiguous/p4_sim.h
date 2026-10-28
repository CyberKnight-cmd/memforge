/* Test-only helpers for P4: a seeded generator, random layouts, a unit-level oracle and a tiny
 * First-Fit allocator for the compaction experiment. Owner: P4.
 *
 * The allocator here exists only to drive P4's experiment before P2's engine and P3's policies
 * land; it is not a second engine and is never linked into src/. */
#ifndef MEMSIM_TESTS_UNIT_CONTIGUOUS_P4_SIM_H_
#define MEMSIM_TESTS_UNIT_CONTIGUOUS_P4_SIM_H_

#include "memsim/common/types.h"

#include <string.h>

#define SIM_MAX_BLOCKS 512

/* xorshift64*: same sequence on every platform and compiler, unlike rand(). */
typedef struct {
    uint64_t s;
} sim_rng;

static inline uint64_t sim_next(sim_rng *r) {
    r->s ^= r->s >> 12;
    r->s ^= r->s << 25;
    r->s ^= r->s >> 27;
    return r->s * 2685821657736338717ULL;
}

/* Uniform in [lo, hi]. */
static inline int64_t sim_range(sim_rng *r, int64_t lo, int64_t hi) {
    return lo + (int64_t)(sim_next(r) % (uint64_t)(hi - lo + 1));
}

/* Merges adjacent FREE blocks in place (invariant I3); returns the new count. */
static inline size_t sim_coalesce(ms_block *b, size_t count) {
    size_t n = 0;
    for (size_t i = 0; i < count; i++) {
        if (n > 0 && b[i].state == MS_BLOCK_FREE && b[n - 1].state == MS_BLOCK_FREE) {
            b[n - 1].size += b[i].size;
        } else {
            b[n++] = b[i];
        }
    }
    return n;
}

/* A random valid layout tiling [0, total): random sizes 1..max_size, about half allocated. */
static inline size_t sim_random_layout(sim_rng *r, ms_block *out, int64_t total, int64_t max_size) {
    size_t n = 0;
    int64_t at = 0;
    uint32_t id = 1;
    while (at < total && n < SIM_MAX_BLOCKS) {
        int64_t size = sim_range(r, 1, max_size);
        if (at + size > total || n == SIM_MAX_BLOCKS - 1) {
            size = total - at;
        }
        if (sim_range(r, 0, 1) == 1) {
            int64_t req = size - sim_range(r, 0, size > 4 ? 3 : 0);
            out[n++] = (ms_block){at, size, MS_BLOCK_ALLOCATED, id % 7, id, req};
            id++;
        } else {
            out[n++] = (ms_block){at, size, MS_BLOCK_FREE, MS_NONE, MS_NONE, 0};
        }
        at += size;
    }
    return sim_coalesce(out, n);
}

/* Oracle: paints the layout unit by unit and measures free memory and the longest free run
 * directly, without looking at block boundaries. Independent of fragmentation.c. */
typedef struct {
    int64_t total_free, longest_run;
} sim_oracle;

static inline sim_oracle sim_oracle_of(const ms_block *b, size_t count) {
    static unsigned char used[1 << 16];
    int64_t m = 0;
    for (size_t i = 0; i < count; i++) {
        m = b[i].start + b[i].size;
    }
    memset(used, 0, (size_t)m);
    for (size_t i = 0; i < count; i++) {
        if (b[i].state == MS_BLOCK_ALLOCATED) {
            memset(used + b[i].start, 1, (size_t)b[i].size);
        }
    }
    sim_oracle o = {0, 0};
    int64_t run = 0;
    for (int64_t a = 0; a < m; a++) {
        run = used[a] ? 0 : run + 1;
        o.total_free += used[a] ? 0 : 1;
        o.longest_run = run > o.longest_run ? run : o.longest_run;
    }
    return o;
}

/* First-Fit allocate: returns the new count, or 0 if nothing fits. */
static inline size_t sim_first_fit(ms_block *b, size_t count, ms_alloc_id id, int64_t size) {
    for (size_t i = 0; i < count; i++) {
        if (b[i].state != MS_BLOCK_FREE || b[i].size < size || count >= SIM_MAX_BLOCKS) {
            continue;
        }
        ms_block rest = b[i];
        b[i] = (ms_block){b[i].start, size, MS_BLOCK_ALLOCATED, 1, id, size};
        if (rest.size > size) {
            memmove(&b[i + 2], &b[i + 1], (count - i - 1) * sizeof *b);
            b[i + 1] =
                (ms_block){rest.start + size, rest.size - size, MS_BLOCK_FREE, MS_NONE, MS_NONE, 0};
            return count + 1;
        }
        return count;
    }
    return 0;
}

static inline size_t sim_free(ms_block *b, size_t count, ms_alloc_id id) {
    for (size_t i = 0; i < count; i++) {
        if (b[i].state == MS_BLOCK_ALLOCATED && b[i].alloc_id == id) {
            b[i] = (ms_block){b[i].start, b[i].size, MS_BLOCK_FREE, MS_NONE, MS_NONE, 0};
        }
    }
    return sim_coalesce(b, count);
}

#endif /* MEMSIM_TESTS_UNIT_CONTIGUOUS_P4_SIM_H_ */
