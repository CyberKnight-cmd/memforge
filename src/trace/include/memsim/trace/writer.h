/* Trace: writer. Owner: P1. */
#ifndef MEMSIM_TRACE_WRITER_H_
#define MEMSIM_TRACE_WRITER_H_

#include "memsim/common/events.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef struct { /* the run's symbol table: names[p] is the string for id p */
    const char *const *pid_names;
    size_t n_pids;
    const char *const *id_names;
    size_t n_ids;
} ms_trace_symbols;

typedef struct {
    const char *engine, *git, *workload, *workload_sha256, *policy; /* NULL prints "" */
    bool paging;                                                    /* false: contiguous */
    int64_t memory_size;                                            /* contiguous */
    uint32_t page_size, frames;                                     /* paging */
} ms_trace_header;

typedef struct {
    const char *key;
    bool is_ratio;
    int64_t value;
} ms_trace_metric; /* ratio: value is ppm */

typedef struct ms_trace_writer ms_trace_writer;

ms_trace_writer *ms_trace_writer_open(
    FILE *out, const ms_trace_symbols *symbols); /* does not take ownership of out; copies the
                                                    symbols struct (not the strings); NULL on OOM */
void ms_trace_writer_close(ms_trace_writer *w);  /* frees w; does not fclose */
void ms_trace_write_header(ms_trace_writer *w, const ms_trace_header *h);
void ms_trace_write_event(ms_trace_writer *w, const ms_event *ev);
void ms_trace_write_summary(ms_trace_writer *w, const ms_trace_metric *metrics, size_t n);
bool ms_trace_writer_ok(const ms_trace_writer *w); /* false once any fprintf failed */

#endif /* MEMSIM_TRACE_WRITER_H_ */
