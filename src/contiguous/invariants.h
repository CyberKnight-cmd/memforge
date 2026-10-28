/* Contiguous invariants (private). Owner: P2. */
#ifndef MEMSIM_CONTIGUOUS_INVARIANTS_H_
#define MEMSIM_CONTIGUOUS_INVARIANTS_H_

#include "memsim/common/types.h"
#include "model.h"

/* Checks I1 to I7 (README section 7; I8 is moot while alignment is cut). Returns 0 if all hold,
 * else the number of the first broken invariant. */
int ms_cont_check_invariants(const ms_cont_model *m);

#endif /* MEMSIM_CONTIGUOUS_INVARIANTS_H_ */
