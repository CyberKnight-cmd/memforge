/* Replacement policy: opt. Evicts the page whose next use is farthest; a page never used again
 * counts as infinity and ties go to the lowest frame index (README section 10). OPT reads the
 * future from the whole reference list passed to prepare. Owner: P6. */
#include "replacement.h"

#include <stdlib.h>

#define OPT_NEVER UINT64_MAX

typedef struct {
    uint64_t *next;     /* per frame: next use of the resident page */
    uint64_t *next_use; /* per reference t: index of the next reference to the same page */
    size_t n;
} opt_state;

typedef struct {
    ms_page_key key;
    size_t t;
} opt_ref;

static int opt_ref_cmp(const void *pa, const void *pb) {
    const opt_ref *a = pa;
    const opt_ref *b = pb;
    if (a->key.pid != b->key.pid) {
        return a->key.pid < b->key.pid ? -1 : 1;
    }
    if (a->key.vpn != b->key.vpn) {
        return a->key.vpn < b->key.vpn ? -1 : 1;
    }
    return (a->t > b->t) - (a->t < b->t);
}

static void *opt_create(uint32_t frames) {
    opt_state *s = calloc(1, sizeof *s);
    if (s == NULL) {
        return NULL;
    }
    s->next = calloc(frames ? frames : 1, sizeof *s->next);
    if (s->next == NULL) {
        free(s);
        return NULL;
    }
    return s;
}

static void opt_destroy(void *state) {
    opt_state *s = state;
    free(s->next);
    free(s->next_use);
    free(s);
}

/* Sorting (key, t) pairs puts each page's references side by side, so every reference's next use
 * is its neighbour: O(n log n) with no hash map. */
static bool opt_prepare(void *state, const ms_page_key *refs, size_t n) {
    opt_state *s = state;
    free(s->next_use);
    s->n = 0;
    s->next_use = malloc((n ? n : 1) * sizeof *s->next_use);
    opt_ref *sorted = malloc((n ? n : 1) * sizeof *sorted);
    if (!s->next_use || !sorted) {
        free(sorted);
        return false;
    }
    for (size_t t = 0; t < n; t++) {
        sorted[t] = (opt_ref){refs[t], t};
    }
    qsort(sorted, n, sizeof *sorted, opt_ref_cmp);
    for (size_t i = 0; i < n; i++) {
        bool more = i + 1 < n && ms_page_key_eq(sorted[i].key, sorted[i + 1].key);
        s->next_use[sorted[i].t] = more ? sorted[i + 1].t : OPT_NEVER;
    }
    free(sorted);
    s->n = n;
    return true;
}

static void opt_on_access(void *state, uint32_t frame, ms_page_key key, uint64_t t, bool write) {
    (void)key;
    (void)write;
    opt_state *s = state;
    s->next[frame] = t < s->n ? s->next_use[t] : OPT_NEVER;
}

static uint32_t opt_choose_victim(void *state, const ms_page_frame_view *view,
                                  const uint32_t *candidates, size_t n, uint64_t t) {
    (void)view;
    (void)t;
    return ms_page_pick_stamp(((opt_state *)state)->next, candidates, n, true);
}

const ms_page_replacement_policy ms_page_opt = {
    .name = "opt",
    .requires_lookahead = true,
    .create = opt_create,
    .destroy = opt_destroy,
    .prepare = opt_prepare,
    .on_access = opt_on_access,
    .choose_victim = opt_choose_victim,
};
