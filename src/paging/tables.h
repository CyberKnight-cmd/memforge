/* Paging tables (private). Owner: P5. */
#ifndef MEMSIM_PAGING_TABLES_H_
#define MEMSIM_PAGING_TABLES_H_

#include "memsim/common/errors.h"

typedef struct {
    uint32_t frame; /* MS_NONE unless present */
    bool present, referenced, dirty;
    bool seen; /* referenced before: a fault is CAPACITY, not COMPULSORY */
} ms_page_pte;

typedef struct {
    uint32_t pages;
    ms_page_pte *ptes; /* NULL: no such process */
} ms_page_table;

/* One table per process, indexed by the dense interned pid. */
typedef struct {
    ms_page_table *v;
    uint32_t n;
} ms_page_tables;

ms_status ms_page_tables_spawn(ms_page_tables *t, ms_pid pid, uint32_t pages);
ms_page_table *ms_page_tables_get(const ms_page_tables *t, ms_pid pid); /* NULL if not live */
void ms_page_tables_release(ms_page_tables *t, ms_pid pid);
void ms_page_tables_destroy(ms_page_tables *t);

#endif /* MEMSIM_PAGING_TABLES_H_ */
