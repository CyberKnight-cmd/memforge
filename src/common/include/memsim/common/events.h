/* Shared kernel: events (README section 11). Owner: P1. */
#ifndef MEMSIM_COMMON_EVENTS_H_
#define MEMSIM_COMMON_EVENTS_H_

#include "memsim/common/types.h"

typedef enum {
    MS_EV_PROCESS_SPAWNED,
    MS_EV_PROCESS_TERMINATED,
    MS_EV_PAGE_HIT,
    MS_EV_PAGE_FAULT,
    MS_EV_FRAME_ALLOCATED,
    MS_EV_VICTIM_SELECTED,
    MS_EV_PAGE_EVICTED,
    MS_EV_PAGE_WRITTEN_BACK,
    MS_EV_PAGE_LOADED,
    MS_EV_ACCESS_VIOLATION,
    MS_EV_ADDRESS_TRANSLATED,
    MS_EV_PLACEMENT_DECIDED,
    MS_EV_BLOCK_SPLIT,
    MS_EV_BLOCK_ALLOCATED,
    MS_EV_ALLOCATION_FAILED,
    MS_EV_ALLOC_REJECTED,
    MS_EV_BLOCK_FREED,
    MS_EV_BLOCKS_COALESCED,
    MS_EV_FREE_REJECTED,
    MS_EV_FREE_SKIPPED,
    MS_EV_COMPACTION_STARTED,
    MS_EV_BLOCK_RELOCATED,
    MS_EV_COMPACTION_COMPLETED
} ms_event_type;

typedef enum { MS_FAULT_COMPULSORY, MS_FAULT_CAPACITY } ms_fault_kind;
typedef enum { MS_VIOLATION_NO_SUCH_PROCESS, MS_VIOLATION_OUT_OF_RANGE } ms_violation_reason;
typedef enum { MS_ALLOC_FAIL_CAPACITY, MS_ALLOC_FAIL_FRAGMENTATION } ms_alloc_fail_cause;
typedef enum {
    MS_REJECT_INVALID_SIZE,
    MS_REJECT_DUPLICATE_ID,
    MS_REJECT_UNKNOWN_ID,
    MS_REJECT_DOUBLE_FREE,
    MS_REJECT_WRONG_OWNER
} ms_reject_reason;

typedef struct {
    uint64_t seq;       /* global, gapless, from 0 */
    uint64_t step;      /* the op that caused it */
    ms_event_type type; /* catalogue in section 11 */
    union {             /* one member per event type; field names match the trace keys */
        struct {
            ms_pid pid;
            uint32_t pages;
        } process_spawned;
        struct {
            ms_pid pid;
            uint32_t released_frames;
            uint32_t freed_blocks;
        } process_terminated;
        struct {
            ms_pid pid;
            uint32_t vpn, frame;
            bool write;
        } page_hit;
        struct {
            ms_pid pid;
            uint32_t vpn;
            ms_fault_kind kind;
            bool write;
        } page_fault;
        struct {
            uint32_t frame;
            ms_pid pid;
            uint32_t vpn;
        } frame_allocated;
        struct {
            const char *policy;
            uint32_t frame;
            ms_pid pid;
            uint32_t vpn;
        } victim_selected;
        struct {
            uint32_t frame;
            ms_pid pid;
            uint32_t vpn;
            bool dirty;
        } page_evicted;
        struct {
            uint32_t frame;
            ms_pid pid;
            uint32_t vpn;
        } page_written_back;
        struct {
            uint32_t frame;
            ms_pid pid;
            uint32_t vpn;
        } page_loaded;
        struct {
            ms_pid pid;
            bool by_va; /* vpn_or_va prints as "va" when set, else "vpn" */
            uint64_t vpn_or_va;
            ms_violation_reason reason;
        } access_violation;
        struct {
            ms_pid pid;
            uint64_t va;
            uint32_t vpn;
            uint64_t offset;
            uint32_t frame;
            uint64_t pa;
        } address_translated;
        struct {
            const char *policy;
            int64_t requested, granted;
            bool found;
            ms_hole hole;
            uint32_t scanned;
        } placement_decided;
        struct {
            ms_hole hole, allocated, remainder;
        } block_split;
        struct {
            ms_pid pid;
            ms_alloc_id id;
            int64_t start, size, requested;
        } block_allocated;
        struct {
            ms_pid pid;
            ms_alloc_id id;
            int64_t requested, granted;
            ms_alloc_fail_cause cause;
            int64_t total_free, largest_hole;
        } allocation_failed;
        struct {
            ms_pid pid;
            ms_alloc_id id;
            ms_reject_reason reason;
        } alloc_rejected;
        struct {
            ms_pid pid;
            ms_alloc_id id;
            int64_t start, size;
        } block_freed;
        struct {
            ms_hole merged[3];
            uint32_t n_merged;
            ms_hole result;
        } blocks_coalesced; /* merged in address order */
        struct {
            ms_pid pid;
            ms_alloc_id id;
            ms_reject_reason reason;
        } free_rejected;
        struct {
            ms_pid pid;
            ms_alloc_id id;
        } free_skipped;
        struct {
            const char *trigger;
            uint32_t holes_before;
            int64_t largest_before;
        } compaction_started; /* trigger is "explicit" */
        struct {
            ms_alloc_id id;
            ms_pid pid;
            int64_t src, dst, size;
        } block_relocated;
        struct {
            uint32_t blocks_moved;
            int64_t units_moved;
            uint32_t holes_after;
            int64_t largest_after;
        } compaction_completed;
    } data;
} ms_event;

#endif /* MEMSIM_COMMON_EVENTS_H_ */
