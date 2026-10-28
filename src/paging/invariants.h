/* Paging invariants (private). Owner: P5. */
#ifndef MEMSIM_PAGING_INVARIANTS_H_
#define MEMSIM_PAGING_INVARIANTS_H_

#include "frames.h"
#include "memsim/paging/engine.h"
#include "tables.h"

/* V1 to V5 from section 9. pool_takes counts frames ever handed out by the free pool. */
bool ms_page_invariants_hold(const ms_page_frames *f, const ms_page_tables *t,
                             const ms_page_stats *s, uint64_t pool_takes);

#endif /* MEMSIM_PAGING_INVARIANTS_H_ */
