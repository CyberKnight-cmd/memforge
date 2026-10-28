/* Paging invariants. Owner: P5. */
#include "invariants.h"

bool ms_page_invariants_hold(const ms_page_frames *f, const ms_page_tables *t,
                             const ms_page_stats *s, uint64_t pool_takes) {
    /* V1, frame side: every owned frame is the present PTE of its owner. V4 follows, since each
       resident page owns exactly one of the n frames. */
    for (uint32_t i = 0; i < f->n; i++) {
        ms_page_key k = f->owner[i];
        if (k.pid == MS_NONE) {
            continue;
        }
        const ms_page_table *pt = ms_page_tables_get(t, k.pid);
        if (pt == NULL || k.vpn >= pt->pages || !pt->ptes[k.vpn].present ||
            pt->ptes[k.vpn].frame != i) {
            return false;
        }
    }
    for (uint32_t p = 0; p < t->n; p++) {
        const ms_page_table *pt = ms_page_tables_get(t, p);
        for (uint32_t v = 0; pt != NULL && v < pt->pages; v++) {
            const ms_page_pte *e = &pt->ptes[v];
            if (e->present) {
                /* V1, table side; V2 holds by construction (free = no owner). */
                if (e->frame >= f->n || f->owner[e->frame].pid != p ||
                    f->owner[e->frame].vpn != v) {
                    return false;
                }
            } else if (e->frame != MS_NONE || e->referenced || e->dirty) {
                return false; /* V3 */
            }
        }
    }
    /* V5 */
    return s->hits + s->faults == s->refs && s->faults == pool_takes + s->evictions &&
           s->writebacks <= s->evictions;
}
