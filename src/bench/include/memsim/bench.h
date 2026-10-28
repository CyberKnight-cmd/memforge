/* Benchmark runner. Owner: P7. */
#ifndef MEMSIM_BENCH_H_
#define MEMSIM_BENCH_H_

#include "memsim/common/errors.h"
#include "memsim/common/types.h"
#include "memsim/metrics.h"
#include "memsim/workloads/loader.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    FILE *trace;
    const char *workload_name;
    const char *workload_sha256;
    const char *git;
    bool check;
} ms_bench_opts; /* trace NULL: no trace */

typedef struct {
    ms_alloc_id id;
    int64_t size;
    bool ok;
    int64_t start;
    int cause;
} ms_bench_placement; /* cause: ms_alloc_fail_cause when !ok, or -1 when the alloc was rejected */

typedef struct {
    ms_metric metrics[MS_METRICS_MAX];
    size_t n_metrics;
    ms_bench_placement *placements;
    size_t n_placements;
} ms_bench_result;

/* builds session, metrics, optional trace (header from w + opts + engine version "0.1.0",
 * then steps, then RunSummary from the metrics), fills out */
ms_status ms_bench_run(const ms_workload *w, const char *policy, const ms_bench_opts *o,
                       ms_bench_result *out);

void ms_bench_result_free(ms_bench_result *r);

#endif /* MEMSIM_BENCH_H_ */
