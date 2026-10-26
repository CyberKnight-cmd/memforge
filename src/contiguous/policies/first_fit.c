
#include "placement.h"

static ms_cont_placement_decision no_result(size_t scanned)
{
    ms_cont_placement_decision result = {
        false, 0U, {0, 0}, scanned
    };
    return result;
}

ms_cont_placement_decision ms_cont_first_fit_choose(
    const ms_cont_placement_view *view,
    int64_t requested_size)
{
    size_t i;

    if (view == NULL || view->holes == NULL ||
        requested_size <= 0) {
        return no_result(0U);
    }

    for (i = 0; i < view->hole_count; i++) {
        if (view->holes[i].size >= requested_size) {
            ms_cont_placement_decision result = {
                true, i, view->holes[i], i + 1U
            };
            return result;
        }
    }

    return no_result(view->hole_count);
}

const ms_cont_placement_policy ms_cont_first_fit_policy = {
    "first-fit",
    ms_cont_first_fit_choose
};
