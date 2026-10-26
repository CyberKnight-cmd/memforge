```c
/* Replacement policy vtable (private). Owner: P6. */
#ifndef MEMSIM_PAGING_REPLACEMENT_PRIVATE_H_
#define MEMSIM_PAGING_REPLACEMENT_PRIVATE_H_

#include <stddef.h>
#include <stdint.h>
#include "memsim/paging/replacement.h"

typedef struct ms_page_policy_vtable {
    ms_page_policy id;
    const char *name;
    size_t (*choose_victim)(const uint32_t *frames, const size_t *loaded_at,
                            const size_t *last_used, size_t frame_count,
                            const uint32_t *references, size_t reference_count,
                            size_t next_reference);
} ms_page_policy_vtable;

const ms_page_policy_vtable *ms_page_fifo_vtable(void);
const ms_page_policy_vtable *ms_page_lru_vtable(void);
const ms_page_policy_vtable *ms_page_opt_vtable(void);

#endif /* MEMSIM_PAGING_REPLACEMENT_PRIVATE_H_ */
```
