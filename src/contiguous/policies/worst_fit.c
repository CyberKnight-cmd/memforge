
#include "placement.h"

static ms_cont_placement_decision no_result(size_t scanned)
{
    ms_cont_placement_decision result = {
        false, 0U, {0, 0}, scanned
    };
    return result;
}

ms_cont_placement_decision ms_cont_worst_fit_choose(
    const ms_cont_placement_view *view,
    int64_t requested_size)
{
    size_t i;
    size_t worst = 0U;
    bool found = false;

    if (view == NULL || view->holes == NULL ||
        requested_size <= 0) {
        return no_result(0U);
    }

    for (i = 0; i < view->hole_count; i++) {
        if (view->holes[i].size >= requested_size &&
            (!found ||
             view->holes[i].size > view->holes[worst].size)) {
            worst = i;
            found = true;
        }
    }

    if (!found) {
        return no_result(view->hole_count);
    }

    {
        ms_cont_placement_decision result = {
            true, worst, view->holes[worst], view->hole_count
        };
        return result;
    }
}

const ms_cont_placement_policy ms_cont_worst_fit_policy = {
    "worst-fit",
    ms_cont_worst_fit_choose
};
