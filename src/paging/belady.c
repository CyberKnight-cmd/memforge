/* Belady sweep. Owner: P6. */
#include <stdlib.h>
#include "memsim/paging/belady.h"

bool ms_page_belady_sweep(ms_page_policy policy, const uint32_t *references,
                          size_t reference_count, size_t min_frames,
                          size_t max_frames, ms_page_sweep_result *result)
{
    if (result == NULL) return false;
    result->point_count = 0U;
    result->points = NULL;
    result->belady_anomaly = false;
    if ((references == NULL && reference_count != 0U) || min_frames == 0U ||
        max_frames < min_frames || max_frames - min_frames == SIZE_MAX) return false;

    size_t count = max_frames - min_frames + 1U;
    if (count > SIZE_MAX / sizeof(*result->points)) return false;
    result->points = calloc(count, sizeof(*result->points));
    if (result->points == NULL) return false;
    result->point_count = count;

    for (size_t i = 0U; i < count; ++i) {
        ms_page_result simulation = {0};
        size_t frames = min_frames + i;
        if (!ms_page_simulate(policy, references, reference_count, frames, &simulation)) {
            ms_page_sweep_result_destroy(result);
            return false;
        }
        result->points[i].frame_count = frames;
        result->points[i].faults = simulation.faults;
        if (i > 0U && result->points[i].faults > result->points[i - 1U].faults)
            result->belady_anomaly = true;
        ms_page_result_destroy(&simulation);
    }
    return true;
}

void ms_page_sweep_result_destroy(ms_page_sweep_result *result)
{
    if (result == NULL) return;
    free(result->points);
    result->points = NULL;
    result->point_count = 0U;
    result->belady_anomaly = false;
}
