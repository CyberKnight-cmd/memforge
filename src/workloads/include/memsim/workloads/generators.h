/* Workloads: generators. Owner: P7. */
#ifndef MEMSIM_WORKLOADS_GENERATORS_H_
#define MEMSIM_WORKLOADS_GENERATORS_H_

#include "memsim/common/errors.h"

#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint64_t seed;
    uint32_t ops;
    int64_t memory_size;
    int64_t size_min, size_max;
    uint32_t p_alloc_ppm;
    uint32_t target_util_ppm; /* 0: off; else free whenever the emitted live units reach this share of memory */
} ms_wl_alloc_uniform_params;

typedef struct {
    uint64_t seed;
    uint32_t n;
    uint32_t pages;
    uint32_t working_set;
    uint32_t shift_every;
    uint32_t p_out_ppm;
    uint32_t p_write_ppm;
    uint32_t frames;
    uint32_t page_size;
} ms_wl_refs_locality_params;

ms_status ms_wl_gen_alloc_uniform(const ms_wl_alloc_uniform_params *p, FILE *out);
ms_status ms_wl_gen_refs_locality(const ms_wl_refs_locality_params *p, FILE *out);

#endif /* MEMSIM_WORKLOADS_GENERATORS_H_ */
