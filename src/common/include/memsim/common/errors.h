/* Shared kernel: errors. Owner: P1. */
#ifndef MEMSIM_COMMON_ERRORS_H_
#define MEMSIM_COMMON_ERRORS_H_

#include "memsim/common/types.h"

typedef enum { MS_OK, MS_ERR_NOMEM, MS_ERR_INVALID } ms_status;

/* Contract violations (ADR-006): on in release builds; the client installs the handler. */
typedef void (*ms_contract_handler)(const char *expr, const char *file, int line);
void ms_set_contract_handler(ms_contract_handler handler); /* NULL restores abort() */
_Noreturn void ms_contract_failed(const char *expr, const char *file, int line);

#define MS_ASSERT(cond) ((cond) ? (void)0 : ms_contract_failed(#cond, __FILE__, __LINE__))

#endif /* MEMSIM_COMMON_ERRORS_H_ */
