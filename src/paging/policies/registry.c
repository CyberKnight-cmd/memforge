/* Replacement policy registry: CLI name -> vtable, one line per policy. Owner: P6. */
#include "replacement.h"

#include <string.h>

static const ms_page_replacement_policy *const replacement_registry[] = {
    &ms_page_fifo,
    &ms_page_lru,
    &ms_page_opt,
};

const ms_page_replacement_policy *ms_page_replacement_find(const char *name) {
    for (size_t i = 0; name && i < sizeof replacement_registry / sizeof *replacement_registry;
         i++) {
        if (strcmp(replacement_registry[i]->name, name) == 0) {
            return replacement_registry[i];
        }
    }
    return NULL;
}
