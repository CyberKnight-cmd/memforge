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
    MS_EV_ADDRESS_TRANSLATED
} ms_event_type;

typedef enum { MS_FAULT_COMPULSORY, MS_FAULT_CAPACITY } ms_fault_kind;
typedef enum { MS_VIOLATION_NO_SUCH_PROCESS, MS_VIOLATION_OUT_OF_RANGE } ms_violation_reason;

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
    } data;
} ms_event;

#endif /* MEMSIM_COMMON_EVENTS_H_ */
