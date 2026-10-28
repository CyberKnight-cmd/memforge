
#ifndef MEMSIM_CONTIGUOUS_PLACEMENT_H_
#define MEMSIM_CONTIGUOUS_PLACEMENT_H_

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef struct {
    int64_t start;
    int64_t size;
} ms_cont_placement_hole;

typedef struct {
    const ms_cont_placement_hole *holes;
    size_t hole_count;
} ms_cont_placement_view;

typedef struct {
    bool found;
    size_t hole_index;
    ms_cont_placement_hole hole;
    size_t scanned;
} ms_cont_placement_decision;

typedef ms_cont_placement_decision (*ms_cont_choose_fn)(const ms_cont_placement_view *view,
                                                        int64_t requested_size);

typedef struct {
    const char *name;
    ms_cont_choose_fn choose;
} ms_cont_placement_policy;

ms_cont_placement_decision ms_cont_first_fit_choose(const ms_cont_placement_view *view,
                                                    int64_t requested_size);

ms_cont_placement_decision ms_cont_best_fit_choose(const ms_cont_placement_view *view,
                                                   int64_t requested_size);

ms_cont_placement_decision ms_cont_worst_fit_choose(const ms_cont_placement_view *view,
                                                    int64_t requested_size);

extern const ms_cont_placement_policy ms_cont_first_fit_policy;
extern const ms_cont_placement_policy ms_cont_best_fit_policy;
extern const ms_cont_placement_policy ms_cont_worst_fit_policy;

#endif
