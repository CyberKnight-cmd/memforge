/* Trace: writer. Owner: P1. */
#include "memsim/trace/writer.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct ms_trace_writer {
    FILE *out;
    ms_trace_symbols symbols;
    bool ok;
};

static void write_raw(ms_trace_writer *w, const char *s) {
    if (!w->ok) {
        return;
    }
    if (fputs(s, w->out) == EOF) {
        w->ok = false;
    }
}

static void write_escaped_char(ms_trace_writer *w, unsigned char c) {
    if (c == '"') {
        if (fputs("\\\"", w->out) == EOF) {
            w->ok = false;
        }
    } else if (c == '\\') {
        if (fputs("\\\\", w->out) == EOF) {
            w->ok = false;
        }
    } else if (c < 0x20 || c == 0x7F) {
        if (fprintf(w->out, "\\u00%02X", (unsigned int)c) < 0) {
            w->ok = false;
        }
    } else {
        if (fputc(c, w->out) == EOF) {
            w->ok = false;
        }
    }
}

static void write_escaped(ms_trace_writer *w, const char *s) {
    if (!w->ok) {
        return;
    }
    if (fputc('"', w->out) == EOF) {
        w->ok = false;
        return;
    }
    if (s != NULL) {
        for (const unsigned char *p = (const unsigned char *)s; *p != '\0' && w->ok; p++) {
            write_escaped_char(w, *p);
        }
    }
    if (w->ok && fputc('"', w->out) == EOF) {
        w->ok = false;
    }
}

static void write_pid(ms_trace_writer *w, ms_pid pid) {
    if (!w->ok) {
        return;
    }
    if (pid == MS_NONE || w->symbols.pid_names == NULL || (size_t)pid >= w->symbols.n_pids ||
        w->symbols.pid_names[pid] == NULL) {
        write_raw(w, "null");
    } else {
        write_escaped(w, w->symbols.pid_names[pid]);
    }
}

static void write_id(ms_trace_writer *w, ms_alloc_id id) {
    if (!w->ok) {
        return;
    }
    if (id == MS_NONE || w->symbols.id_names == NULL || (size_t)id >= w->symbols.n_ids ||
        w->symbols.id_names[id] == NULL) {
        write_raw(w, "null");
    } else {
        write_escaped(w, w->symbols.id_names[id]);
    }
}

static void write_ratio(ms_trace_writer *w, int64_t ppm) {
    if (!w->ok) {
        return;
    }
    if (ppm < 0) {
        int64_t pos = -ppm;
        if (fprintf(w->out, "-%" PRId64 ".%06" PRId64, pos / 1000000, pos % 1000000) < 0) {
            w->ok = false;
        }
    } else {
        if (fprintf(w->out, "%" PRId64 ".%06" PRId64, ppm / 1000000, ppm % 1000000) < 0) {
            w->ok = false;
        }
    }
}

static void write_hole(ms_trace_writer *w, ms_hole h) {
    if (!w->ok) {
        return;
    }
    if (fprintf(w->out, "{\"start\":%" PRId64 ",\"size\":%" PRId64 "}", h.start, h.size) < 0) {
        w->ok = false;
    }
}

static const char *fault_kind_str(ms_fault_kind k) {
    switch (k) {
    case MS_FAULT_COMPULSORY:
        return "COMPULSORY";
    case MS_FAULT_CAPACITY:
        return "CAPACITY";
    default:
        abort();
    }
}

static const char *violation_reason_str(ms_violation_reason r) {
    switch (r) {
    case MS_VIOLATION_NO_SUCH_PROCESS:
        return "NO_SUCH_PROCESS";
    case MS_VIOLATION_OUT_OF_RANGE:
        return "OUT_OF_RANGE";
    default:
        abort();
    }
}

static const char *alloc_fail_cause_str(ms_alloc_fail_cause c) {
    switch (c) {
    case MS_ALLOC_FAIL_CAPACITY:
        return "CAPACITY";
    case MS_ALLOC_FAIL_FRAGMENTATION:
        return "FRAGMENTATION";
    default:
        abort();
    }
}

static const char *reject_reason_str(ms_reject_reason r) {
    switch (r) {
    case MS_REJECT_INVALID_SIZE:
        return "INVALID_SIZE";
    case MS_REJECT_DUPLICATE_ID:
        return "DUPLICATE_ID";
    case MS_REJECT_UNKNOWN_ID:
        return "UNKNOWN_ID";
    case MS_REJECT_DOUBLE_FREE:
        return "DOUBLE_FREE";
    case MS_REJECT_WRONG_OWNER:
        return "WRONG_OWNER";
    default:
        abort();
    }
}

static const char *event_name(ms_event_type t) {
    switch (t) {
    case MS_EV_PROCESS_SPAWNED:
        return "ProcessSpawned";
    case MS_EV_PROCESS_TERMINATED:
        return "ProcessTerminated";
    case MS_EV_PAGE_HIT:
        return "PageHit";
    case MS_EV_PAGE_FAULT:
        return "PageFault";
    case MS_EV_FRAME_ALLOCATED:
        return "FrameAllocated";
    case MS_EV_VICTIM_SELECTED:
        return "VictimSelected";
    case MS_EV_PAGE_EVICTED:
        return "PageEvicted";
    case MS_EV_PAGE_WRITTEN_BACK:
        return "PageWrittenBack";
    case MS_EV_PAGE_LOADED:
        return "PageLoaded";
    case MS_EV_ACCESS_VIOLATION:
        return "AccessViolation";
    case MS_EV_ADDRESS_TRANSLATED:
        return "AddressTranslated";
    case MS_EV_PLACEMENT_DECIDED:
        return "PlacementDecided";
    case MS_EV_BLOCK_SPLIT:
        return "BlockSplit";
    case MS_EV_BLOCK_ALLOCATED:
        return "BlockAllocated";
    case MS_EV_ALLOCATION_FAILED:
        return "AllocationFailed";
    case MS_EV_ALLOC_REJECTED:
        return "AllocRejected";
    case MS_EV_BLOCK_FREED:
        return "BlockFreed";
    case MS_EV_BLOCKS_COALESCED:
        return "BlocksCoalesced";
    case MS_EV_FREE_REJECTED:
        return "FreeRejected";
    case MS_EV_FREE_SKIPPED:
        return "FreeSkipped";
    case MS_EV_COMPACTION_STARTED:
        return "CompactionStarted";
    case MS_EV_BLOCK_RELOCATED:
        return "BlockRelocated";
    case MS_EV_COMPACTION_COMPLETED:
        return "CompactionCompleted";
    default:
        abort();
    }
}

ms_trace_writer *ms_trace_writer_open(FILE *out, const ms_trace_symbols *symbols) {
    if (out == NULL) {
        return NULL;
    }
    ms_trace_writer *w = malloc(sizeof(*w));
    if (w == NULL) {
        return NULL;
    }
    w->out = out;
    if (symbols != NULL) {
        w->symbols = *symbols;
    } else {
        memset(&w->symbols, 0, sizeof(w->symbols));
    }
    w->ok = true;
    return w;
}

void ms_trace_writer_close(ms_trace_writer *w) {
    free(w);
}

bool ms_trace_writer_ok(const ms_trace_writer *w) {
    return w != NULL && w->ok;
}

void ms_trace_write_header(ms_trace_writer *w, const ms_trace_header *h) {
    if (w == NULL || !w->ok || h == NULL) {
        return;
    }
    write_raw(w, "{\"type\":\"TraceHeader\",\"schema\":1,\"engine\":");
    write_escaped(w, h->engine);
    write_raw(w, ",\"git\":");
    write_escaped(w, h->git);
    write_raw(w, ",\"workload\":");
    write_escaped(w, h->workload);
    write_raw(w, ",\"workload_sha256\":");
    write_escaped(w, h->workload_sha256);
    write_raw(w, ",\"policy\":");
    write_escaped(w, h->policy);
    write_raw(w, ",\"config\":");
    if (!h->paging) {
        if (fprintf(w->out, "{\"memory_size\":%" PRId64 "}", h->memory_size) < 0) {
            w->ok = false;
        }
    } else {
        if (fprintf(w->out,
                    "{\"page_size\":%" PRIu32 ",\"frames\":%" PRIu32 ",\"scope\":\"global\"}",
                    h->page_size, h->frames) < 0) {
            w->ok = false;
        }
    }
    write_raw(w, ",\"trace_level\":1}\n");
}

void ms_trace_write_summary(ms_trace_writer *w, const ms_trace_metric *metrics, size_t n) {
    if (w == NULL || !w->ok) {
        return;
    }
    write_raw(w, "{\"type\":\"RunSummary\",\"metrics\":{");
    for (size_t i = 0; i < n; i++) {
        if (i > 0) {
            write_raw(w, ",");
        }
        write_escaped(w, metrics[i].key);
        write_raw(w, ":");
        if (metrics[i].is_ratio) {
            write_ratio(w, metrics[i].value);
        } else {
            if (fprintf(w->out, "%" PRId64, metrics[i].value) < 0) {
                w->ok = false;
            }
        }
    }
    write_raw(w, "}}\n");
}

static void write_page_access_event_data(ms_trace_writer *w, const ms_event *ev) {
    switch (ev->type) {
    case MS_EV_PAGE_HIT:
        write_raw(w, "{\"pid\":");
        write_pid(w, ev->data.page_hit.pid);
        if (fprintf(w->out, ",\"vpn\":%" PRIu32 ",\"frame\":%" PRIu32 ",\"write\":%s}",
                    ev->data.page_hit.vpn, ev->data.page_hit.frame,
                    ev->data.page_hit.write ? "true" : "false") < 0) {
            w->ok = false;
        }
        break;
    case MS_EV_PAGE_FAULT:
        write_raw(w, "{\"pid\":");
        write_pid(w, ev->data.page_fault.pid);
        if (fprintf(w->out, ",\"vpn\":%" PRIu32 ",\"kind\":\"%s\",\"write\":%s}",
                    ev->data.page_fault.vpn, fault_kind_str(ev->data.page_fault.kind),
                    ev->data.page_fault.write ? "true" : "false") < 0) {
            w->ok = false;
        }
        break;
    case MS_EV_ACCESS_VIOLATION:
        write_raw(w, "{\"pid\":");
        write_pid(w, ev->data.access_violation.pid);
        if (ev->data.access_violation.by_va) {
            if (fprintf(w->out, ",\"va\":%" PRIu64 ",\"reason\":\"%s\"}",
                        ev->data.access_violation.vpn_or_va,
                        violation_reason_str(ev->data.access_violation.reason)) < 0) {
                w->ok = false;
            }
        } else {
            if (fprintf(w->out, ",\"vpn\":%" PRIu64 ",\"reason\":\"%s\"}",
                        ev->data.access_violation.vpn_or_va,
                        violation_reason_str(ev->data.access_violation.reason)) < 0) {
                w->ok = false;
            }
        }
        break;
    case MS_EV_ADDRESS_TRANSLATED:
        write_raw(w, "{\"pid\":");
        write_pid(w, ev->data.address_translated.pid);
        if (fprintf(w->out,
                    ",\"va\":%" PRIu64 ",\"vpn\":%" PRIu32 ",\"offset\":%" PRIu64
                    ",\"frame\":%" PRIu32 ",\"pa\":%" PRIu64 "}",
                    ev->data.address_translated.va, ev->data.address_translated.vpn,
                    ev->data.address_translated.offset, ev->data.address_translated.frame,
                    ev->data.address_translated.pa) < 0) {
            w->ok = false;
        }
        break;
    default:
        abort();
    }
}

static void write_page_frame_event_data(ms_trace_writer *w, const ms_event *ev) {
    switch (ev->type) {
    case MS_EV_FRAME_ALLOCATED:
        if (fprintf(w->out, "{\"frame\":%" PRIu32 ",\"pid\":", ev->data.frame_allocated.frame) <
            0) {
            w->ok = false;
        }
        write_pid(w, ev->data.frame_allocated.pid);
        if (fprintf(w->out, ",\"vpn\":%" PRIu32 "}", ev->data.frame_allocated.vpn) < 0) {
            w->ok = false;
        }
        break;
    case MS_EV_VICTIM_SELECTED:
        write_raw(w, "{\"policy\":");
        write_escaped(w, ev->data.victim_selected.policy);
        if (fprintf(w->out, ",\"frame\":%" PRIu32 ",\"pid\":", ev->data.victim_selected.frame) <
            0) {
            w->ok = false;
        }
        write_pid(w, ev->data.victim_selected.pid);
        if (fprintf(w->out, ",\"vpn\":%" PRIu32 "}", ev->data.victim_selected.vpn) < 0) {
            w->ok = false;
        }
        break;
    case MS_EV_PAGE_EVICTED:
        if (fprintf(w->out, "{\"frame\":%" PRIu32 ",\"pid\":", ev->data.page_evicted.frame) < 0) {
            w->ok = false;
        }
        write_pid(w, ev->data.page_evicted.pid);
        if (fprintf(w->out, ",\"vpn\":%" PRIu32 ",\"dirty\":%s}", ev->data.page_evicted.vpn,
                    ev->data.page_evicted.dirty ? "true" : "false") < 0) {
            w->ok = false;
        }
        break;
    case MS_EV_PAGE_WRITTEN_BACK:
        if (fprintf(w->out, "{\"frame\":%" PRIu32 ",\"pid\":", ev->data.page_written_back.frame) <
            0) {
            w->ok = false;
        }
        write_pid(w, ev->data.page_written_back.pid);
        if (fprintf(w->out, ",\"vpn\":%" PRIu32 "}", ev->data.page_written_back.vpn) < 0) {
            w->ok = false;
        }
        break;
    case MS_EV_PAGE_LOADED:
        if (fprintf(w->out, "{\"frame\":%" PRIu32 ",\"pid\":", ev->data.page_loaded.frame) < 0) {
            w->ok = false;
        }
        write_pid(w, ev->data.page_loaded.pid);
        if (fprintf(w->out, ",\"vpn\":%" PRIu32 "}", ev->data.page_loaded.vpn) < 0) {
            w->ok = false;
        }
        break;
    default:
        abort();
    }
}

static void write_cont_alloc_event_data(ms_trace_writer *w, const ms_event *ev) {
    switch (ev->type) {
    case MS_EV_PLACEMENT_DECIDED:
        write_raw(w, "{\"policy\":");
        write_escaped(w, ev->data.placement_decided.policy);
        if (fprintf(w->out, ",\"requested\":%" PRId64 ",\"granted\":%" PRId64 ",\"hole\":",
                    ev->data.placement_decided.requested, ev->data.placement_decided.granted) < 0) {
            w->ok = false;
        }
        if (ev->data.placement_decided.found) {
            write_hole(w, ev->data.placement_decided.hole);
        } else {
            write_raw(w, "null");
        }
        if (fprintf(w->out, ",\"scanned\":%" PRIu32 "}", ev->data.placement_decided.scanned) < 0) {
            w->ok = false;
        }
        break;
    case MS_EV_BLOCK_SPLIT:
        write_raw(w, "{\"hole\":");
        write_hole(w, ev->data.block_split.hole);
        write_raw(w, ",\"allocated\":");
        write_hole(w, ev->data.block_split.allocated);
        write_raw(w, ",\"remainder\":");
        write_hole(w, ev->data.block_split.remainder);
        write_raw(w, "}");
        break;
    case MS_EV_BLOCK_ALLOCATED:
        write_raw(w, "{\"pid\":");
        write_pid(w, ev->data.block_allocated.pid);
        write_raw(w, ",\"id\":");
        write_id(w, ev->data.block_allocated.id);
        if (fprintf(w->out,
                    ",\"start\":%" PRId64 ",\"size\":%" PRId64 ",\"requested\":%" PRId64 "}",
                    ev->data.block_allocated.start, ev->data.block_allocated.size,
                    ev->data.block_allocated.requested) < 0) {
            w->ok = false;
        }
        break;
    case MS_EV_ALLOCATION_FAILED:
        write_raw(w, "{\"pid\":");
        write_pid(w, ev->data.allocation_failed.pid);
        write_raw(w, ",\"id\":");
        write_id(w, ev->data.allocation_failed.id);
        if (fprintf(w->out,
                    ",\"requested\":%" PRId64 ",\"granted\":%" PRId64
                    ",\"cause\":\"%s\",\"total_free\":%" PRId64 ",\"largest_hole\":%" PRId64 "}",
                    ev->data.allocation_failed.requested, ev->data.allocation_failed.granted,
                    alloc_fail_cause_str(ev->data.allocation_failed.cause),
                    ev->data.allocation_failed.total_free,
                    ev->data.allocation_failed.largest_hole) < 0) {
            w->ok = false;
        }
        break;
    case MS_EV_ALLOC_REJECTED:
        write_raw(w, "{\"pid\":");
        write_pid(w, ev->data.alloc_rejected.pid);
        write_raw(w, ",\"id\":");
        write_id(w, ev->data.alloc_rejected.id);
        if (fprintf(w->out, ",\"reason\":\"%s\"}",
                    reject_reason_str(ev->data.alloc_rejected.reason)) < 0) {
            w->ok = false;
        }
        break;
    default:
        abort();
    }
}

static void write_cont_free_compact_event_data(ms_trace_writer *w, const ms_event *ev) {
    switch (ev->type) {
    case MS_EV_BLOCK_FREED:
        write_raw(w, "{\"pid\":");
        write_pid(w, ev->data.block_freed.pid);
        write_raw(w, ",\"id\":");
        write_id(w, ev->data.block_freed.id);
        if (fprintf(w->out, ",\"start\":%" PRId64 ",\"size\":%" PRId64 "}",
                    ev->data.block_freed.start, ev->data.block_freed.size) < 0) {
            w->ok = false;
        }
        break;
    case MS_EV_BLOCKS_COALESCED:
        write_raw(w, "{\"merged\":[");
        for (uint32_t i = 0; i < ev->data.blocks_coalesced.n_merged && i < 3; i++) {
            if (i > 0) {
                write_raw(w, ",");
            }
            write_hole(w, ev->data.blocks_coalesced.merged[i]);
        }
        write_raw(w, "],\"result\":");
        write_hole(w, ev->data.blocks_coalesced.result);
        write_raw(w, "}");
        break;
    case MS_EV_FREE_REJECTED:
        write_raw(w, "{\"pid\":");
        write_pid(w, ev->data.free_rejected.pid);
        write_raw(w, ",\"id\":");
        write_id(w, ev->data.free_rejected.id);
        if (fprintf(w->out, ",\"reason\":\"%s\"}",
                    reject_reason_str(ev->data.free_rejected.reason)) < 0) {
            w->ok = false;
        }
        break;
    case MS_EV_FREE_SKIPPED:
        write_raw(w, "{\"pid\":");
        write_pid(w, ev->data.free_skipped.pid);
        write_raw(w, ",\"id\":");
        write_id(w, ev->data.free_skipped.id);
        write_raw(w, "}");
        break;
    case MS_EV_COMPACTION_STARTED:
        write_raw(w, "{\"trigger\":");
        write_escaped(w, ev->data.compaction_started.trigger);
        if (fprintf(w->out, ",\"holes_before\":%" PRIu32 ",\"largest_before\":%" PRId64 "}",
                    ev->data.compaction_started.holes_before,
                    ev->data.compaction_started.largest_before) < 0) {
            w->ok = false;
        }
        break;
    case MS_EV_BLOCK_RELOCATED:
        write_raw(w, "{\"id\":");
        write_id(w, ev->data.block_relocated.id);
        write_raw(w, ",\"pid\":");
        write_pid(w, ev->data.block_relocated.pid);
        if (fprintf(w->out, ",\"src\":%" PRId64 ",\"dst\":%" PRId64 ",\"size\":%" PRId64 "}",
                    ev->data.block_relocated.src, ev->data.block_relocated.dst,
                    ev->data.block_relocated.size) < 0) {
            w->ok = false;
        }
        break;
    case MS_EV_COMPACTION_COMPLETED:
        if (fprintf(w->out,
                    "{\"blocks_moved\":%" PRIu32 ",\"units_moved\":%" PRId64
                    ",\"holes_after\":%" PRIu32 ",\"largest_after\":%" PRId64 "}",
                    ev->data.compaction_completed.blocks_moved,
                    ev->data.compaction_completed.units_moved,
                    ev->data.compaction_completed.holes_after,
                    ev->data.compaction_completed.largest_after) < 0) {
            w->ok = false;
        }
        break;
    default:
        abort();
    }
}

void ms_trace_write_event(ms_trace_writer *w, const ms_event *ev) {
    if (w == NULL || !w->ok || ev == NULL) {
        return;
    }
    const char *name = event_name(ev->type);
    if (fprintf(w->out, "{\"seq\":%" PRIu64 ",\"step\":%" PRIu64 ",\"type\":\"%s\",\"data\":",
                ev->seq, ev->step, name) < 0) {
        w->ok = false;
        return;
    }
    switch (ev->type) {
    case MS_EV_PROCESS_SPAWNED:
        write_raw(w, "{\"pid\":");
        write_pid(w, ev->data.process_spawned.pid);
        if (fprintf(w->out, ",\"pages\":%" PRIu32 "}", ev->data.process_spawned.pages) < 0) {
            w->ok = false;
        }
        break;

    case MS_EV_PROCESS_TERMINATED:
        write_raw(w, "{\"pid\":");
        write_pid(w, ev->data.process_terminated.pid);
        if (fprintf(w->out, ",\"released_frames\":%" PRIu32 ",\"freed_blocks\":%" PRIu32 "}",
                    ev->data.process_terminated.released_frames,
                    ev->data.process_terminated.freed_blocks) < 0) {
            w->ok = false;
        }
        break;

    case MS_EV_PAGE_HIT:
    case MS_EV_PAGE_FAULT:
    case MS_EV_ACCESS_VIOLATION:
    case MS_EV_ADDRESS_TRANSLATED:
        write_page_access_event_data(w, ev);
        break;

    case MS_EV_FRAME_ALLOCATED:
    case MS_EV_VICTIM_SELECTED:
    case MS_EV_PAGE_EVICTED:
    case MS_EV_PAGE_WRITTEN_BACK:
    case MS_EV_PAGE_LOADED:
        write_page_frame_event_data(w, ev);
        break;

    case MS_EV_PLACEMENT_DECIDED:
    case MS_EV_BLOCK_SPLIT:
    case MS_EV_BLOCK_ALLOCATED:
    case MS_EV_ALLOCATION_FAILED:
    case MS_EV_ALLOC_REJECTED:
        write_cont_alloc_event_data(w, ev);
        break;

    case MS_EV_BLOCK_FREED:
    case MS_EV_BLOCKS_COALESCED:
    case MS_EV_FREE_REJECTED:
    case MS_EV_FREE_SKIPPED:
    case MS_EV_COMPACTION_STARTED:
    case MS_EV_BLOCK_RELOCATED:
    case MS_EV_COMPACTION_COMPLETED:
        write_cont_free_compact_event_data(w, ev);
        break;

    default:
        abort();
    }
    write_raw(w, "}\n");
}
