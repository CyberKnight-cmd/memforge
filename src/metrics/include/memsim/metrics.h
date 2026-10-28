/* MetricsCollector. Owner: P7. */
#ifndef MEMSIM_METRICS_H_
#define MEMSIM_METRICS_H_

#include "memsim/common/events.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MS_METRICS_MAX 32

typedef struct {
    const char *key;
    bool is_ratio;
    int64_t value;
} ms_metric; /* is_ratio: value is parts per million */

typedef struct ms_metrics ms_metrics;

ms_metrics *ms_metrics_create(bool paging); /* NULL on OOM */
void ms_metrics_destroy(ms_metrics *m);
void ms_metrics_on_events(ms_metrics *m, const ms_event *events, size_t n);
void ms_metrics_on_blocks(ms_metrics *m, const ms_block *blocks,
                          size_t n); /* contiguous: the layout after each step */
size_t ms_metrics_summary(const ms_metrics *m, ms_metric *out,
                          size_t cap); /* writes min(count, cap), returns count */

#endif /* MEMSIM_METRICS_H_ */
