/* Shared kernel: errors. Owner: P1. */
#include "memsim/common/errors.h"

#include <stdlib.h>

static ms_contract_handler contract_handler = NULL;

void ms_set_contract_handler(ms_contract_handler handler) {
    contract_handler = handler;
}

_Noreturn void ms_contract_failed(const char *expr, const char *file, int line) {
    if (contract_handler != NULL) {
        contract_handler(expr, file, line);
    }
    abort();
}
