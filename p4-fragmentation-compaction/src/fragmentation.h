/* Contiguous fragmentation (private). Owner: P4.
 *
 * FragmentationAnalyzer from README section 8. Every function is pure: it reads a layout and
 * never changes it. A layout is the block list in address order, as MemoryView exposes it
 * (I1: blocks tile [0, M); I3: no two adjacent FREE blocks, so each FREE block is one hole). */
#ifndef MEMSIM_CONTIGUOUS_FRAGMENTATION_H_
#define MEMSIM_CONTIGUOUS_FRAGMENTATION_H_

#include "memsim/common/types.h"
#include "p4_core_types.h"

/* Fractions are integer parts per million, so no float ever enters engine state. */
#define MS_PPM 1000000

typedef struct {
    int64_t total_free;    /* sum of FREE block sizes */
    int64_t largest_hole;  /* size of the largest FREE block, 0 if none */
    uint32_t hole_count;   /* number of FREE blocks */
    uint32_t ext_frag_ppm; /* 1 - largest_hole / total_free, rounded down; 0 if total_free == 0 */
    int64_t int_frag;      /* sum of (size - requested) over ALLOCATED blocks */
    int64_t used;          /* sum of ALLOCATED block sizes */
    uint32_t live_blocks;  /* number of ALLOCATED blocks */
} ms_cont_frag_report;

typedef enum {
    MS_FAIL_NONE,          /* the request fits in some hole: not a failure */
    MS_FAIL_CAPACITY,      /* total_free < granted: memory is genuinely full */
    MS_FAIL_FRAGMENTATION, /* total_free >= granted, but no single hole is large enough */
} ms_cont_failure_cause;

ms_cont_frag_report ms_cont_frag_report_of(const ms_block *blocks, size_t count);

/* Why would a request of `granted` units fail on this layout? */
ms_cont_failure_cause ms_cont_frag_classify_failure(const ms_block *blocks, size_t count,
                                                    int64_t granted);

/* Share of free memory sitting in holes smaller than the reference request size s, in ppm. */
uint32_t ms_cont_frag_unusable_free_ppm(const ms_block *blocks, size_t count, int64_t s);

const char *ms_cont_failure_cause_name(ms_cont_failure_cause cause);

/* For reports and the trace only; never store the result in engine state. */
static inline double ms_cont_ppm_to_double(uint32_t ppm) {
    return (double)ppm / MS_PPM;
}

#endif /* MEMSIM_CONTIGUOUS_FRAGMENTATION_H_ */
