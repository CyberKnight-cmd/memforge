/* PagingEngine: the module's exported interface. Owner: P5. */
#ifndef MEMSIM_PAGING_ENGINE_H_
#define MEMSIM_PAGING_ENGINE_H_

#include "memsim/common/config.h"
#include "memsim/common/errors.h"
#include "memsim/common/events.h"
#include "memsim/common/types.h"

typedef struct ms_page_engine ms_page_engine;

/* One step's outcome. events stay valid until the next apply; seq and step are left 0 for the
   session to fill in. pa is the physical address of the access (offset 0 for a vpn access). */
typedef struct {
    const ms_event *events;
    size_t n_events;
    bool fault;
    uint64_t pa;
} ms_page_step;

/* refs counts valid accesses only, so hits + faults == refs (V5). */
typedef struct {
    uint64_t refs, hits, faults, evictions, writebacks;
} ms_page_stats;

/* Global replacement only. MS_ERR_INVALID: page size not a power of 2, no frames, unknown policy.
 */
ms_status ms_page_engine_create(const ms_paging_config *cfg, ms_page_engine **out);
void ms_page_engine_destroy(ms_page_engine *e);

/* Hands the policy every ACCESS key in order (OPT's lookahead). Required before apply when the
   policy needs lookahead; MS_ERR_NOMEM if the policy runs out of memory. */
ms_status ms_page_engine_prepare(ms_page_engine *e, const ms_op *ops, size_t n);

/* SPAWN, ACCESS or TERMINATE. Bad accesses are AccessViolation events, not errors. */
ms_status ms_page_engine_apply(ms_page_engine *e, const ms_op *op, ms_page_step *out);

ms_page_stats ms_page_engine_stats(const ms_page_engine *e);

typedef struct {
    ms_pid pid;
    uint32_t vpn;
    bool referenced, dirty;
} ms_page_frame_info; /* pid MS_NONE: free frame */

/* Writes min(frames, cap) entries in frame order; returns the frame count. */
size_t ms_page_engine_frames(const ms_page_engine *e, ms_page_frame_info *out, size_t cap);

/* Paging invariants V1 to V5 (section 9); V6 is not checked. */
bool ms_page_engine_check(const ms_page_engine *e);

#endif /* MEMSIM_PAGING_ENGINE_H_ */
