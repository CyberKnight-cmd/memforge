/* Session facade: the single entry point for every client. Owner: P1. */
#ifndef MEMSIM_SESSION_H_
#define MEMSIM_SESSION_H_

#include "memsim/common/config.h"
#include "memsim/common/errors.h"
#include "memsim/common/events.h"
#include "memsim/common/types.h"
#include "memsim/trace/writer.h"

typedef struct {
    bool paging;
    int64_t memory_size;         /* contiguous */
    ms_paging_config paging_cfg; /* paging; its policy field is ignored */
    const char *policy; /* "first-fit" | "best-fit" | "worst-fit" | "fifo" | "lru" | "opt" */
    bool check;         /* run the engine invariant check after every step; MS_ASSERT on failure */
} ms_session_config;

typedef struct ms_session ms_session;

/* paging: calls ms_page_engine_prepare with ops; MS_ERR_INVALID for an unknown policy or bad config
 */
ms_status ms_session_create(const ms_session_config *cfg, const ms_op *ops, size_t n_ops,
                            ms_session **out);

void ms_session_destroy(ms_session *s);

/* writes the step-0 snapshot now; afterwards every step writes its events then its snapshot */
void ms_session_attach_trace(ms_session *s, ms_trace_writer *w);

/* events valid until next step, with seq (global, gapless from 0) and step (1-based op index)
 * filled in; MS_ERR_INVALID if op kind does not match the subsystem */
ms_status ms_session_step(ms_session *s, const ms_op *op, const ms_event **events, size_t *n);

/* contiguous layout; NULL/0 for paging */
const ms_block *ms_session_blocks(const ms_session *s, size_t *n);

#endif /* MEMSIM_SESSION_H_ */
