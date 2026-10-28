/* Workloads: loader. Owner: P7. */
#ifndef MEMSIM_WORKLOADS_LOADER_H_
#define MEMSIM_WORKLOADS_LOADER_H_

#include "memsim/common/errors.h"

typedef enum { MS_WL_CONTIGUOUS, MS_WL_PAGING } ms_wl_kind;

typedef struct {
    ms_wl_kind kind;
    int64_t memory_size; /* contiguous */
    uint32_t page_size;  /* paging */
    uint32_t frames;     /* paging */
    bool negative_test;
    ms_op *ops;
    size_t n_ops;
    char **pid_names; /* pid_names[p] is the string for ms_pid p */
    size_t n_pids;
    char **id_names; /* id_names[i] is the string for ms_alloc_id i */
    size_t n_ids;
} ms_workload;

typedef struct {
    size_t line;
    char message[160];
} ms_wl_error; /* line 0: not line-specific (e.g. missing header key, I/O) */

/* MS_OK, MS_ERR_INVALID (err filled), MS_ERR_NOMEM */
ms_status ms_wl_parse(const char *text, size_t len, ms_workload *out, ms_wl_error *err);

/* reads the file, then ms_wl_parse; unreadable file -> MS_ERR_INVALID, line 0 */
ms_status ms_wl_load(const char *path, ms_workload *out, ms_wl_error *err);

/* frees everything; safe on a zeroed or failed workload */
void ms_wl_free(ms_workload *w);

#endif /* MEMSIM_WORKLOADS_LOADER_H_ */
