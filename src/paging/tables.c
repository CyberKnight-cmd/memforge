/* Paging tables. Owner: P5. */
#include "tables.h"

#include <stdlib.h>

ms_status ms_page_tables_spawn(ms_page_tables *t, ms_pid pid, uint32_t pages) {
    if (pid == MS_NONE || pages == 0 || ms_page_tables_get(t, pid) != NULL) {
        return MS_ERR_INVALID;
    }
    if (pid >= t->n) {
        ms_page_table *v = realloc(t->v, ((size_t)pid + 1) * sizeof *v);
        if (v == NULL) {
            return MS_ERR_NOMEM;
        }
        for (uint32_t i = t->n; i <= pid; i++) {
            v[i] = (ms_page_table){0};
        }
        t->v = v;
        t->n = pid + 1;
    }
    ms_page_pte *ptes = malloc((size_t)pages * sizeof *ptes);
    if (ptes == NULL) {
        return MS_ERR_NOMEM;
    }
    for (uint32_t i = 0; i < pages; i++) {
        ptes[i] = (ms_page_pte){.frame = MS_NONE};
    }
    t->v[pid] = (ms_page_table){.pages = pages, .ptes = ptes};
    return MS_OK;
}

ms_page_table *ms_page_tables_get(const ms_page_tables *t, ms_pid pid) {
    return pid < t->n && t->v[pid].ptes != NULL ? &t->v[pid] : NULL;
}

void ms_page_tables_release(ms_page_tables *t, ms_pid pid) {
    ms_page_table *pt = ms_page_tables_get(t, pid);
    if (pt != NULL) {
        free(pt->ptes);
        *pt = (ms_page_table){0};
    }
}

void ms_page_tables_destroy(ms_page_tables *t) {
    for (uint32_t i = 0; i < t->n; i++) {
        free(t->v[i].ptes);
    }
    free(t->v);
    *t = (ms_page_tables){0};
}
