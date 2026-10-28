/* Workloads: generators. Owner: P7. */
#include "memsim/workloads/generators.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

typedef struct {
    uint64_t s[4];
} xoshiro256ss_rng;

static uint64_t splitmix64(uint64_t *state) {
    uint64_t z = (*state += 0x9e3779b97f4a7c15ULL);
    z = (z ^ (z >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    z = (z ^ (z >> 27U)) * 0x94d049bb133111ebULL;
    return z ^ (z >> 31U);
}

static void rng_seed(xoshiro256ss_rng *rng, uint64_t seed) {
    uint64_t sm = seed;
    rng->s[0] = splitmix64(&sm);
    rng->s[1] = splitmix64(&sm);
    rng->s[2] = splitmix64(&sm);
    rng->s[3] = splitmix64(&sm);
}

static inline uint64_t rotl(uint64_t x, unsigned int k) {
    return (x << k) | (x >> (64U - k));
}

static uint64_t rng_next(xoshiro256ss_rng *rng) {
    const uint64_t result = rotl(rng->s[1] * 5ULL, 7U) * 9ULL;
    const uint64_t t = rng->s[1] << 17U;

    rng->s[2] ^= rng->s[0];
    rng->s[3] ^= rng->s[1];
    rng->s[1] ^= rng->s[2];
    rng->s[0] ^= rng->s[3];

    rng->s[2] ^= t;
    rng->s[3] = rotl(rng->s[3], 45U);

    return result;
}

static uint64_t rng_draw(xoshiro256ss_rng *rng, uint64_t bound) {
    if (bound <= 1ULL) {
        return 0ULL;
    }
    uint64_t threshold = (UINT64_MAX - bound + 1ULL) % bound;
    for (;;) {
        uint64_t r = rng_next(rng);
        if (r >= threshold) {
            return r % bound;
        }
    }
}

ms_status ms_wl_gen_alloc_uniform(const ms_wl_alloc_uniform_params *p, FILE *out) {
    if (p == NULL || out == NULL) {
        return MS_ERR_INVALID;
    }
    if (p->ops == 0U || p->size_min <= 0 || p->size_min > p->size_max || p->memory_size <= 0 ||
        p->p_alloc_ppm > 1000000U || p->target_util_ppm > 1000000U) {
        return MS_ERR_INVALID;
    }

    if (fprintf(out, "schema 1\n") < 0 || fprintf(out, "kind contiguous\n") < 0 ||
        fprintf(out, "memory_size %" PRId64 "\n", p->memory_size) < 0 ||
        fprintf(out, "gen_name alloc-uniform\n") < 0 || fprintf(out, "gen_version 1\n") < 0 ||
        fprintf(out, "gen_seed %" PRIu64 "\n", p->seed) < 0 ||
        fprintf(out, "gen_ops %" PRIu32 "\n", p->ops) < 0 ||
        fprintf(out, "gen_size_min %" PRId64 "\n", p->size_min) < 0 ||
        fprintf(out, "gen_size_max %" PRId64 "\n", p->size_max) < 0 ||
        fprintf(out, "gen_p_alloc_ppm %" PRIu32 "\n", p->p_alloc_ppm) < 0 ||
        fprintf(out, "gen_target_util_ppm %" PRIu32 "\n", p->target_util_ppm) < 0) {
        return MS_ERR_INVALID;
    }

    uint32_t *live_blocks = malloc((size_t)p->ops * sizeof(uint32_t));
    int64_t *live_sizes = malloc((size_t)p->ops * sizeof(int64_t));
    if (live_blocks == NULL || live_sizes == NULL) {
        free(live_blocks);
        free(live_sizes);
        return MS_ERR_NOMEM;
    }
    /* Emitted live units, not the engine's: a failed alloc still counts until it is freed. */
    int64_t live_units = 0;
    int64_t target_units = (int64_t)(((uint64_t)p->memory_size * p->target_util_ppm) / 1000000U);

    xoshiro256ss_rng rng;
    rng_seed(&rng, p->seed);

    uint32_t k = 0;
    size_t live_count = 0;
    ms_status status = MS_OK;

    for (uint32_t i = 0; i < p->ops; i++) {
        bool do_alloc = false;
        if (live_count == 0) {
            do_alloc = true;
        } else if (p->target_util_ppm > 0 && live_units >= target_units) {
            do_alloc = false;
        } else {
            do_alloc = (rng_draw(&rng, 1000000ULL) < (uint64_t)p->p_alloc_ppm);
        }

        if (do_alloc) {
            uint64_t range = (uint64_t)(p->size_max - p->size_min + 1);
            int64_t sz = p->size_min + (int64_t)rng_draw(&rng, range);
            if (fprintf(out, "alloc P%" PRIu32 " b%" PRIu32 " %" PRId64 "\n", k % 4U, k, sz) < 0) {
                status = MS_ERR_INVALID;
                break;
            }
            live_sizes[live_count] = sz;
            live_blocks[live_count++] = k;
            live_units += sz;
            k++;
        } else {
            uint64_t idx = rng_draw(&rng, (uint64_t)live_count);
            uint32_t victim = live_blocks[idx];
            live_units -= live_sizes[idx];
            live_blocks[idx] = live_blocks[live_count - 1];
            live_sizes[idx] = live_sizes[live_count - 1];
            live_count--;
            if (fprintf(out, "free P%" PRIu32 " b%" PRIu32 "\n", victim % 4U, victim) < 0) {
                status = MS_ERR_INVALID;
                break;
            }
        }
    }

    free(live_blocks);
    free(live_sizes);
    return status;
}

ms_status ms_wl_gen_refs_locality(const ms_wl_refs_locality_params *p, FILE *out) {
    if (p == NULL || out == NULL) {
        return MS_ERR_INVALID;
    }
    if (p->n == 0U || p->working_set == 0U || p->pages == 0U || p->working_set > p->pages ||
        p->shift_every == 0U || p->frames == 0U || p->page_size == 0U ||
        (p->page_size & (p->page_size - 1U)) != 0U || p->p_out_ppm > 1000000U ||
        p->p_write_ppm > 1000000U) {
        return MS_ERR_INVALID;
    }

    if (fprintf(out, "schema 1\n") < 0 || fprintf(out, "kind paging\n") < 0 ||
        fprintf(out, "page_size %" PRIu32 "\n", p->page_size) < 0 ||
        fprintf(out, "frames %" PRIu32 "\n", p->frames) < 0 ||
        fprintf(out, "gen_name refs-locality\n") < 0 || fprintf(out, "gen_version 1\n") < 0 ||
        fprintf(out, "gen_seed %" PRIu64 "\n", p->seed) < 0 ||
        fprintf(out, "gen_n %" PRIu32 "\n", p->n) < 0 ||
        fprintf(out, "gen_pages %" PRIu32 "\n", p->pages) < 0 ||
        fprintf(out, "gen_working_set %" PRIu32 "\n", p->working_set) < 0 ||
        fprintf(out, "gen_shift_every %" PRIu32 "\n", p->shift_every) < 0 ||
        fprintf(out, "gen_p_out_ppm %" PRIu32 "\n", p->p_out_ppm) < 0 ||
        fprintf(out, "gen_p_write_ppm %" PRIu32 "\n", p->p_write_ppm) < 0 ||
        fprintf(out, "gen_frames %" PRIu32 "\n", p->frames) < 0 ||
        fprintf(out, "gen_page_size %" PRIu32 "\n", p->page_size) < 0 ||
        fprintf(out, "spawn P0 %" PRIu32 "\n", p->pages) < 0) {
        return MS_ERR_INVALID;
    }

    xoshiro256ss_rng rng;
    rng_seed(&rng, p->seed);

    uint32_t base = 0;
    uint32_t shift_count = 0;

    for (uint32_t i = 0; i < p->n; i++) {
        uint32_t vpn = 0;
        if (p->working_set < p->pages && rng_draw(&rng, 1000000ULL) < (uint64_t)p->p_out_ppm) {
            uint64_t out_count = (uint64_t)(p->pages - p->working_set);
            uint32_t offset = (uint32_t)rng_draw(&rng, out_count);
            vpn = (base + p->working_set + offset) % p->pages;
        } else {
            uint32_t offset = (uint32_t)rng_draw(&rng, (uint64_t)p->working_set);
            vpn = (base + offset) % p->pages;
        }

        bool is_write = rng_draw(&rng, 1000000ULL) < (uint64_t)p->p_write_ppm;
        int write_res = is_write ? fprintf(out, "access P0 %" PRIu32 " w\n", vpn)
                                 : fprintf(out, "access P0 %" PRIu32 "\n", vpn);
        if (write_res < 0) {
            return MS_ERR_INVALID;
        }

        shift_count++;
        if (shift_count == p->shift_every) {
            shift_count = 0;
            base = (base + (p->working_set / 2U)) % p->pages;
        }
    }

    return MS_OK;
}
