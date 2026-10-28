/* Trace: snapshot. Owner: P1. */
#ifndef MEMSIM_TRACE_SNAPSHOT_H_
#define MEMSIM_TRACE_SNAPSHOT_H_

#include "memsim/trace/writer.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint32_t frame;
    ms_pid pid;
    uint32_t vpn;
    bool referenced, dirty;
} ms_trace_frame; /* pid MS_NONE: free frame */

void ms_trace_write_cont_snapshot(ms_trace_writer *w, uint64_t step, int64_t memory_size,
                                  const ms_block *blocks, size_t n);
void ms_trace_write_page_snapshot(ms_trace_writer *w, uint64_t step, const ms_trace_frame *frames,
                                  size_t n);

#endif /* MEMSIM_TRACE_SNAPSHOT_H_ */
