/* Trace: snapshot. Owner: P1. */
#include "memsim/trace/snapshot.h"

#include <inttypes.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

struct ms_trace_writer {
    FILE *out;
    ms_trace_symbols symbols;
    bool ok;
};

typedef struct {
    int64_t total_free;
    int64_t largest_hole;
    uint64_t hole_count;
    int64_t used;
    uint64_t live_blocks;
    int64_t int_frag;
    uint64_t ext_frag_ppm;
} cont_frag_stats;

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

static void compute_frag_stats(const ms_block *blocks, size_t n, cont_frag_stats *st) {
    st->total_free = 0;
    st->largest_hole = 0;
    st->hole_count = 0;
    st->used = 0;
    st->live_blocks = 0;
    st->int_frag = 0;
    st->ext_frag_ppm = 0;

    for (size_t i = 0; i < n; i++) {
        if (blocks[i].state == MS_BLOCK_FREE) {
            st->total_free += blocks[i].size;
            if (blocks[i].size > st->largest_hole) {
                st->largest_hole = blocks[i].size;
            }
            st->hole_count++;
        } else {
            st->used += blocks[i].size;
            st->live_blocks++;
            st->int_frag += (blocks[i].size - blocks[i].requested);
        }
    }

    if (st->total_free > 0) {
        uint64_t p = (uint64_t)(st->total_free - st->largest_hole);
        uint64_t w_free = (uint64_t)st->total_free;
        st->ext_frag_ppm = (p / w_free) * 1000000ULL + ((p % w_free) * 1000000ULL) / w_free;
    }
}

static void write_cont_block(ms_trace_writer *w, const ms_block *b) {
    if (b->state == MS_BLOCK_FREE) {
        if (fprintf(w->out, "{\"start\":%" PRId64 ",\"size\":%" PRId64 ",\"state\":\"free\"}",
                    b->start, b->size) < 0) {
            w->ok = false;
        }
    } else {
        if (fprintf(w->out,
                    "{\"start\":%" PRId64 ",\"size\":%" PRId64 ",\"state\":\"allocated\",\"pid\":",
                    b->start, b->size) < 0) {
            w->ok = false;
        }
        write_pid(w, b->pid);
        write_raw(w, ",\"id\":");
        write_id(w, b->alloc_id);
        if (fprintf(w->out, ",\"requested\":%" PRId64 "}", b->requested) < 0) {
            w->ok = false;
        }
    }
}

void ms_trace_write_cont_snapshot(ms_trace_writer *w, uint64_t step, int64_t memory_size,
                                  const ms_block *blocks, size_t n) {
    if (w == NULL || !w->ok) {
        return;
    }

    cont_frag_stats st;
    compute_frag_stats(blocks, n, &st);

    if (fprintf(w->out,
                "{\"type\":\"Snapshot\",\"step\":%" PRIu64
                ",\"contiguous\":{\"memory_size\":%" PRId64 ",\"blocks\":[",
                step, memory_size) < 0) {
        w->ok = false;
        return;
    }

    for (size_t i = 0; i < n; i++) {
        if (i > 0) {
            write_raw(w, ",");
        }
        write_cont_block(w, &blocks[i]);
    }

    if (fprintf(w->out,
                "],\"frag\":{\"total_free\":%" PRId64 ",\"largest_hole\":%" PRId64
                ",\"hole_count\":%" PRIu64 ",\"ext_frag\":",
                st.total_free, st.largest_hole, st.hole_count) < 0) {
        w->ok = false;
    }
    write_ratio(w, (int64_t)st.ext_frag_ppm);
    if (fprintf(w->out,
                ",\"used\":%" PRId64 ",\"live_blocks\":%" PRIu64 ",\"int_frag\":%" PRId64 "}}}\n",
                st.used, st.live_blocks, st.int_frag) < 0) {
        w->ok = false;
    }
}

void ms_trace_write_page_snapshot(ms_trace_writer *w, uint64_t step, const ms_trace_frame *frames,
                                  size_t n) {
    if (w == NULL || !w->ok) {
        return;
    }

    if (fprintf(w->out, "{\"type\":\"Snapshot\",\"step\":%" PRIu64 ",\"paging\":{\"frames\":[",
                step) < 0) {
        w->ok = false;
        return;
    }

    for (size_t i = 0; i < n; i++) {
        if (i > 0) {
            write_raw(w, ",");
        }
        if (fprintf(w->out, "{\"frame\":%" PRIu32 ",\"pid\":", frames[i].frame) < 0) {
            w->ok = false;
        }
        if (frames[i].pid == MS_NONE) {
            write_raw(w, "null,\"vpn\":null");
        } else {
            write_pid(w, frames[i].pid);
            if (fprintf(w->out, ",\"vpn\":%" PRIu32, frames[i].vpn) < 0) {
                w->ok = false;
            }
        }
        if (fprintf(w->out, ",\"referenced\":%s,\"dirty\":%s}",
                    frames[i].referenced ? "true" : "false",
                    frames[i].dirty ? "true" : "false") < 0) {
            w->ok = false;
        }
    }

    write_raw(w, "]}}\n");
}
