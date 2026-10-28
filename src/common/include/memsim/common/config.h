/* Shared kernel: config. Owner: P1. */
#ifndef MEMSIM_COMMON_CONFIG_H_
#define MEMSIM_COMMON_CONFIG_H_

#include "memsim/common/types.h"

typedef enum { MS_SCOPE_GLOBAL, MS_SCOPE_LOCAL } ms_replacement_scope;

typedef struct {
    uint32_t page_size; /* power of 2 */
    uint32_t frames;
    const char *policy;         /* "fifo" */
    ms_replacement_scope scope; /* MS_SCOPE_GLOBAL */
    uint64_t t_mem_ns;          /* 200; effective-access-time inputs */
    uint64_t t_fault_ns;        /* 8000000 */
    uint64_t t_writeback_ns;    /* 8000000 */
} ms_paging_config;

ms_paging_config ms_paging_config_default(void);

#endif /* MEMSIM_COMMON_CONFIG_H_ */
