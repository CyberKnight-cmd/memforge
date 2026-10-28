/* Shared kernel: config. Owner: P1. */
#include "memsim/common/config.h"

ms_paging_config ms_paging_config_default(void) {
    return (ms_paging_config){.page_size = 4096,
                              .frames = 3,
                              .policy = "fifo",
                              .scope = MS_SCOPE_GLOBAL,
                              .t_mem_ns = 200,
                              .t_fault_ns = 8000000,
                              .t_writeback_ns = 8000000};
}
