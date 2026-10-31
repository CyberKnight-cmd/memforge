"""Build the IEEE-style MemForge report: python3 build.py -> memforge-ieee.pdf"""
import re
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
import figures as F  # noqa: E402

HERE = Path(__file__).parent
ROMAN = ["I", "II", "III", "IV", "V", "VI", "VII", "VIII", "IX", "X", "XI", "XII", "XIII", "XIV", "XV",
         "XVI", "XVII", "XVIII", "XIX", "XX", "XXI", "XXII", "XXIII", "XXIV", "XXV"]

TEAM = [("Adrija Das", "12024002001001", 51), ("Meghna Dhar", "12024002001045", 52),
        ("Arpita Goswami", "12024002001021", 53), ("Ankita Dhara", "12024002001263", 54),
        ("Pritam Majumder", "12024002001303", 55), ("Anusuya Sarkar", "12024002001179", 56),
        ("Jayasree Saha", "12024002001289", 57), ("Aloke Dipto Banerjee", "12024002001006", 58),
        ("Sandipan Gain", "12024002001232", 59), ("Arya Gupta", "12024002001189", 60)]
TITLE = "MemForge: A Deterministic Simulator for Operating-System Memory Management"

_fig = [0]
_tab = [0]
_sec = [0]
_alg = [0]
TOC = []


def fig(svg, cap, w=None):
    _fig[0] += 1
    st = f' style="width:{w}mm"' if w else ""
    return (f'<figure class="fig"{st}><div class="art">{svg}</div>'
            f'<figcaption>Fig. {_fig[0]}. {cap}</figcaption></figure>')


def tab(cap, head, rows, cls=""):
    _tab[0] += 1
    th = "".join(f"<th>{h}</th>" for h in head)
    tr = "".join("<tr>" + "".join(f"<td>{c}</td>" for c in r) + "</tr>" for r in rows)
    return (f'<div class="tbl {cls}"><div class="tcap">TABLE {ROMAN[_tab[0]-1]}<br>'
            f'<span>{cap}</span></div><table><thead><tr>{th}</tr></thead><tbody>{tr}</tbody></table></div>')


_subn = [0]


def sec(title):
    _sec[0] += 1
    _subn[0] = 0
    n = _sec[0]
    TOC.append((title, f"{n}. {title.upper()}"))
    return f'<h2 id="s{n}">{n}. {title.upper()}</h2>'


def sub(letter, t):
    _subn[0] += 1
    return f"<h3>{_sec[0]}.{_subn[0]} {t}</h3>"


def alg(title, lines):
    _alg[0] += 1
    body = "".join(f"<div><span class='ln'>{i+1}:</span>{l}</div>" for i, l in enumerate(lines))
    return f'<div class="alg"><div class="ah"><b>Algorithm {_alg[0]}:</b> {title}</div>{body}</div>'


def term(lines):
    from html import escape
    return '<pre class="term">' + "\n".join(escape(l) for l in lines) + "</pre>"


# =================================================================== FRONT
def front(toc_pages):
    trs = "".join(f"<tr><td>{i+1}</td><td><b>{t}</b></td><td>{toc_pages.get(i, '')}</td></tr>"
                  for i, (t, _) in enumerate(TOC_ENTRIES))
    return f"""
<section class="page">
  <h1 class="ph">TABLE OF CONTENTS</h1>
  <table class="toc"><thead><tr><th>S.I NO.</th><th>SECTION TOPIC</th><th>PAGE NO.</th></tr></thead>
  <tbody>{trs}</tbody></table>
</section>"""


TOC_ENTRIES = []  # filled after body generation: (title, search key)


# =================================================================== BODY
def body():
    authors = ", ".join(n for n, _, _ in TEAM)
    h = []
    a = h.append
    a('<div class="body">')
    # ---------------------------------------------------------- abstract
    a("""<h2 id="abs">ABSTRACT</h2>
<p class="abs">MemForge is a deterministic simulator of an operating-system
memory manager, written in C17 and also compiled to WebAssembly. It covers contiguous allocation
(First-, Best- and Worst-Fit, splitting, coalescing, sliding compaction) and demand paging (single-level
page tables, address translation, FIFO, LRU and OPT replacement). Policies only decide and engines only
mutate, and every change is published as an event, so identical inputs and seeds always produce
byte-identical traces. On 30 seeded workloads at 90% utilisation, Best-Fit reaches a 91.4&nbsp;&plusmn;&nbsp;0.4%
success rate against 90.0&nbsp;&plusmn;&nbsp;0.6% for First-Fit and 87.2&nbsp;&plusmn;&nbsp;0.7% for Worst-Fit,
while First-Fit scans the fewest holes (3.21 vs. 5.24 per request). On 2,000-reference workloads,
faults follow OPT &lt; LRU &lt; FIFO at every frame count (103.2, 138.9 and 253.1 at 8 frames), and a
frame sweep reproduces Belady's anomaly (9 &rarr; 10 faults). All eight acceptance criteria and 17
test suites pass under AddressSanitizer, UBSan and Valgrind.</p>
<p class="abs"><b><i>Keywords</i>&mdash;</b>memory management, deterministic simulation, contiguous
allocation, fragmentation, demand paging, page replacement, Belady's anomaly.</p>""")

    # ---------------------------------------------------------- 1
    a(sec("Introduction"))
    a("""<p class="dropcap">Every running program (a <i>process</i>) needs space in the computer's
main memory (RAM) to hold its code and data. RAM is limited and shared by many processes, so the
operating system must decide <i>who gets which part of memory</i>, give it back when a process finishes,
and stop processes from touching each other's memory. This job is called <i>memory management</i>.</p>
<p>Memory management decides where each process lives in physical memory and how
its addresses are translated. Textbooks present contiguous allocation and demand paging with small
hand-worked examples, but the trade-offs between policies only become visible when they run on the same
workload, step by step, at scale.</p>
<p>MemForge closes that gap. It is a modular C17 simulator with a command-line tool for scripted
experiments and a browser laboratory that runs the <i>same</i> engine compiled to WebAssembly. Its three
design pillars are summarised in Fig.&nbsp;1; the rest of this paper follows the order in which the
system was built: concepts, architecture, the contiguous subsystem, the paging subsystem, experiments,
testing, interfaces and engineering.</p>""")
    d = F.D(340, 70)
    for i, (t, s) in enumerate([("Determinism", ["integer state,", "logical clocks,", "fixed tie-breaks"]),
                                ("Modularity", ["policies decide,", "engines mutate,", "events report"]),
                                ("Correctness", ["invariants every", "step, golden tests,", "sanitizers"])]):
        d.box(4 + i * 112, 4, 106, 62, [t] + s, 7.6, bold_first=True)
    a(fig(d.svg(), "The three design pillars of MemForge."))

    # ---------------------------------------------------------- 2
    a(sec("Objectives, Scope and Limitations"))
    a(sub("A", "Objectives"))
    a("""<ol class="tight"><li>Implement First-, Best- and Worst-Fit with splitting, coalescing, fragmentation
metrics and compaction.</li><li>Simulate translation, page faults and FIFO, LRU and OPT replacement.</li>
<li>Compare policies fairly on identical, reproducible workloads.</li><li>Verify correctness with invariants,
automated tests and memory-safety tools.</li><li>Offer a CLI and an interactive WebAssembly lab.</li></ol>""")
    a(sub("B", "Scope"))
    a(fig(F.fig_scope(), "Scope boundaries: what is implemented, simplified and excluded."))
    a(sub("C", "Limitations"))
    a("""<p>Memory is simulated, not real RAM; one CPU applies one operation at a time; disk I/O is counted
but its latency is not modelled; page tables are single-level with no TLB; Next-Fit, Clock and other
policies are out; compaction is never automatic.</p>""")

    # ---------------------------------------------------------- 3
    a(sec("Operating System Memory Management Concepts"))
    a("""<p><b>From the ground up.</b> Think of memory as one long row of numbered boxes (addresses 0, 1,
2,&nbsp;&hellip;). A program asks for, say, 212 boxes; the OS must find 212 <i>free boxes in a row</i>, mark
them as used, and later mark them free again. Two broad strategies exist: give each request one
unbroken run of boxes (<i>contiguous allocation</i>), or cut both programs and memory into equal-sized
pieces that can be placed anywhere (<i>paging</i>). MemForge simulates both.</p>
<p>The MMU maps each <i>logical</i> address produced by the CPU to a <i>physical</i> address and traps
invalid accesses. Under contiguous allocation each request receives one run of memory taken from a free
<i>hole</i>: First-Fit takes the first hole that fits, Best-Fit the smallest and Worst-Fit the largest.
<i>External</i> fragmentation is free memory split into holes too small to use; <i>internal</i>
fragmentation is unused space inside a block (zero in MemForge, since granted = requested).
Compaction slides blocks together to merge the holes (Fig.&nbsp;3).</p>""")
    a(fig(F.fig_concepts(), "Address translation, external fragmentation and compaction."))
    a("""<p><b>Why paging?</b> With contiguous allocation, free space ends up scattered in small gaps.
Paging avoids this: every piece is the same size, so any free slot fits any piece. The cost is a
lookup table per program that records where each piece currently lives.</p>
<p>Paging splits logical memory into fixed-size <i>pages</i> and physical memory into equal
<i>frames</i>; a page table maps one to the other (Fig.&nbsp;4). Under demand paging a page is loaded only
on its first use; a reference to a non-resident page is a <i>page fault</i>. When no frame is free, a
replacement policy picks a victim: FIFO the oldest page, LRU the least recently used, OPT the page used
furthest in the future. Belady's anomaly&mdash;more frames, more faults&mdash;can occur with FIFO but not
with stack algorithms such as LRU and OPT (Fig.&nbsp;5).</p>""")
    a(fig(F.fig_paging_concept(), "Paging: the page number is replaced by a frame number."))
    a(fig(F.fig_belady_small(), "Faults vs. frames on the string 1 2 3 4 1 2 5 1 2 3 4 5."))

    # ---------------------------------------------------------- 4
    a(sec("System Architecture and Design"))
    a(sub("A", "Layered Architecture"))
    a(fig(F.fig_layers(), "Five layers; each layer uses only the ones below it.", w=110))
    a(sub("B", "Deterministic Core and Session Facade"))
    a("""<p>The core performs no I/O and reads no wall clock; it uses logical clocks (<i>step</i>,
<i>t</i>, <i>seq</i>), integer arithmetic and fixed tie-breaks (lowest address, lowest frame). Every
client goes through the <code>Session</code> facade, which applies one operation, checks invariants and
publishes the step's events in <code>seq</code> order (Fig.&nbsp;7).</p>""")
    a(fig(F.fig_dataflow(), "End-to-end data flow of one run.", w=110))
    a(sub("C", "Events, Traces and WebAssembly Bridge"))
    a("""<p>Each step yields events (23 types, Appendix) that the trace writer stores as JSON Lines with a
snapshot after every step. The same C modules are compiled with Clang for the WASI target and loaded by
the web app, so browser and CLI traces are byte-identical.</p>""")
    a(tab("Architecture decision log", ["Decision", "Reason"], [
        ["C17", "systems language, no runtime"], ["Address-ordered block array", "simple, deterministic"],
        ["Policies separate from engines", "testable, cannot corrupt state"],
        ["Event log as contract", "one source for trace, metrics, tests"],
        ["Integer-only state", "identical output on every machine"],
        ["Global frame pool", "matches textbook examples"], ["Plain-text workloads", "no JSON parser in C"]]))
    a(sub("D", "Component Workflows"))
    a(fig(F.fig_alloc_seq(), "Allocation sequence in the contiguous subsystem.", w=105))
    a(fig(F.fig_access_flow(), "Paged memory access with fault handling and replacement.", w=100))

    # ---------------------------------------------------------- 5
    a(sec("Module Description and Dependencies"))
    a(tab("Module catalogue", ["Module", "Responsibility"], [
        ["common", "types, config, errors, events, bus, registry"],
        ["contiguous", "blocks, engine, FF/BF/WF, fragmentation, compaction"],
        ["paging", "page/frame tables, MMU, FIFO/LRU/OPT, Belady sweep"],
        ["trace", "JSONL writer and snapshots"], ["workloads", "loader, validator, seeded generators"],
        ["metrics", "every reported figure, from events"], ["session", "facade: apply, check, publish"],
        ["bench", "runner over workload &times; policy &times; seed"], ["cli", "memsim commands, SHA-256"]]))
    a(fig(F.fig_modules(), "Module dependency graph (acyclic, checked by check_layers.sh).", w=125))
    a("""<p>Each module exports only what others need; policies cannot modify engine data, and the core
may not include <code>stdio.h</code>, <code>time.h</code> or system headers. Policies are
function-pointer tables registered by name (Fig.&nbsp;11), so a new policy is one file and one
registry line.</p>""")
    a(fig(F.fig_vtable(), "Policy vtable and registry.", w=115))

    # ---------------------------------------------------------- 6
    a(sec("Memory Model and Allocation State"))
    a("""<p>In plain terms, MemForge keeps a list of memory pieces in address order; each piece is either
<i>used</i> (owned by a process) or <i>free</i> (a hole). Allocation cuts a hole in two; freeing joins a
piece with free neighbours so holes never sit side by side.</p>
<p>Memory [0,&nbsp;M) is an address-ordered array of <code>ms_block</code> records that tile it
exactly. Workloads refer to blocks by stable allocation ids, never by address, because compaction moves
blocks; each id passes through the lifecycle in Fig.&nbsp;12, which rules out duplicate ids and
double frees.</p>""")
    a(fig(F.fig_block_model(), "Block array, block record and allocation-id lifecycle.", w=95))
    a(alg("Block splitting", ["check the hole is free and size &ge; s",
        "<b>if</b> size &gt; s <b>then</b> insert a free block [start + s, end) after it",
        "set size &larr; s, pid, alloc_id, requested; mark ALLOCATED", "<b>return</b> block index"]))
    a(alg("Block freeing and coalescing", ["verify the block is ALLOCATED",
        "mark FREE; clear pid and alloc_id", "<b>if</b> right neighbour is free <b>then</b> merge it",
        "<b>if</b> left neighbour is free <b>then</b> merge into it"]))
    a(fig(F.fig_split_coalesce(), "Splitting a hole on allocation and coalescing on free.", w=90))
    a("<p>Splitting and freeing cost O(1) when no block has to shift and O(N) in the worst case (array shift); both use O(1) extra space.</p>")

    # ---------------------------------------------------------- 7
    a(sec("First-Fit, Best-Fit and Worst-Fit Algorithms"))
    a("""<p>When several holes are big enough, which one should be used? Picture parking a car: take the
<i>first</i> space that fits (First-Fit), the <i>tightest</i> space that fits (Best-Fit), or the
<i>biggest</i> space (Worst-Fit). Each choice affects how usable the leftover gaps will be later.</p>
<p>A placement policy is a pure selector: given the holes in address order and a size
<i>s</i>, it returns whether a hole was found, which one, and how many holes it <i>scanned</i> (the
search-cost metric). Algorithm&nbsp;1 gives all three; they differ only in the selection test.</p>""")
    a(alg("Placement selection (FF / BF / WF)", [
        "<b>if</b> view = NULL <b>or</b> s &le; 0 <b>then return</b> (found = false, scanned = 0)",
        "best &larr; none",
        "<b>for</b> i &larr; 0 <b>to</b> h &minus; 1 <b>do</b>",
        "&nbsp;&nbsp;<b>if</b> holes[i].size &lt; s <b>then continue</b>",
        "&nbsp;&nbsp;<b>FF:</b> <b>return</b> (true, i, scanned = i + 1)",
        "&nbsp;&nbsp;<b>BF:</b> <b>if</b> best = none <b>or</b> size &lt; holes[best].size <b>then</b> best &larr; i",
        "&nbsp;&nbsp;<b>WF:</b> <b>if</b> best = none <b>or</b> size &gt; holes[best].size <b>then</b> best &larr; i",
        "<b>end for</b>",
        "<b>return</b> (best &ne; none, best, scanned = h)"]))
    a("""<p>Strict comparisons keep the lowest address on ties. All three run in O(h) time and O(1) space.
Fig.&nbsp;14 shows them on the textbook five-hole memory.</p>""")
    a(fig(F.fig_ac1_policies(), "Placements on the Silberschatz five-hole memory (AC-1).", w=125))
    a("""<p><i>Excluded:</i> Next-Fit (a roving cursor complicates coalescing and compaction for no clear
gain), size-indexed trees (O(log h) but harder to keep deterministic) and the buddy system
(power-of-two sizes cause internal fragmentation).</p>""")

    # ---------------------------------------------------------- 8
    a(sec("Fragmentation and Compaction"))
    a("""<p><b>The problem.</b> After many allocations and frees, free memory is broken into small gaps.
A new request may fail even though the <i>total</i> free memory is enough, simply because no single gap
is big enough. MemForge measures how bad this is, and can fix it by <i>compaction</i>: sliding all used
pieces to one end so the gaps merge into one big hole, at the cost of copying data.</p>""")
    a(tab("Fragmentation metrics", ["Metric", "Definition"], [
        ["total_free, largest_hole, hole_count", "sum of holes; largest; number of holes"],
        ["ext_frag", "1 &minus; largest_hole / total_free (0 if none free), in ppm"],
        ["failure cause", "FRAGMENTATION if total_free &ge; s &gt; largest; else CAPACITY"],
        ["blocks_moved, units_moved", "cost of compaction"]]))

    a(alg("External fragmentation and failure cause (integer, ppm)", [
        "total &larr; &Sigma; hole sizes; largest &larr; max hole size",
        "<b>if</b> total = 0 <b>then</b> ext_frag &larr; 0 <b>else</b> ext_frag &larr; 10<sup>6</sup> &minus; ppm(largest, total)",
        "<b>on failed request</b> s: cause &larr; FRAGMENTATION <b>if</b> total &ge; s <b>else</b> CAPACITY"]))
    a("<p>Example: First-Fit fails 426 with 959 free and largest hole 300, so ext_frag = 1 &minus; 300/959 = 0.687 and the cause is FRAGMENTATION.</p>")
    a(alg("Sliding compaction", [
        "next &larr; 0; moves &larr; [ ]",
        "<b>for each</b> allocated block b <b>in</b> address order <b>do</b>",
        "&nbsp;&nbsp;<b>if</b> b.start &ne; next <b>then</b> append (b.id, b.start, next) to moves",
        "&nbsp;&nbsp;next &larr; next + b.size",
        "validate moves (sources match, ascending); apply; emit BlockRelocated per move",
        "replace all free blocks by one hole [next, M)"]))
    a(fig(F.fig_compaction(), "Compaction pipeline and its effect on AC-2.", w=88))
    a("""<p>Tested guarantees: the live set and total free memory are unchanged, at most one hole remains,
block order is preserved, and 5,000 random layouts agree with an independent oracle.</p>""")

    # ---------------------------------------------------------- 9
    a(sec("Memory Allocation Examples and Results"))
    a(tab("AC-1: start address per request (memory 1740)", ["Req.", "Size", "FF", "BF", "WF"], [
        ["a1", "212", "110", "830", "1140"], ["a2", "417", "1140", "110", "110"], ["a3", "112", "322", "620", "1352"],
        ["a4", "426", "fail", "1140", "fail"]]))
    a(tab("AC-1 summary metrics", ["Metric", "FF", "BF", "WF"], [
        ["holes scanned", "23", "29", "29"], ["mean scanned", "1.769", "2.231", "2.231"],
        ["used / free", "781 / 959", "1207 / 533", "781 / 959"]]))
    a("""<p>First- and Worst-Fit fail a4 with 959 units free because the largest hole is 300 (external
fragmentation). In AC-2, compaction moves 7 blocks (781 units) into one 959-unit hole [781,&nbsp;1740);
the retried a4 lands at 781, leaving [1207,&nbsp;1740) free (Fig.&nbsp;15).</p>""")
    a("""<p><b>Experiment E1</b> runs 30 seeded workloads (60/75/90% target utilisation &times; 10 seeds;
3,000 operations on 1,024 units; sizes 16&ndash;128; allocation probability 0.6). Figs.&nbsp;16&ndash;17
plot mean values; Section&nbsp;15 gives 95% confidence intervals.</p>""")
    a(fig(F.fig_e1(0, "allocation success (%)", 86, 100, [86, 90, 94, 98], leg=(70, 72)),
          "E1: allocation success rate vs. target utilisation.", w=75))
    a(fig(F.fig_e1(2, "holes scanned / request", 2, 6, [2, 3, 4, 5, 6], leg=(70, 18)),
          "E1: search cost (mean holes scanned per request).", w=75))

    # ---------------------------------------------------------- 10
    a(sec("Paging Model and Address Translation"))
    a("""<p>Each program sees its own private <i>virtual</i> addresses starting at 0. Its memory is cut into
pages (here 4096 bytes); RAM is cut into frames of the same size. A per-program <i>page table</i> says
which frame holds each page. To read an address, the hardware splits it into "which page" and "how far
into the page", looks up the frame, and keeps the offset unchanged.</p>
<p>The paging subsystem has four parts: MMU, per-process page tables, an inverted frame table and
the paging engine (Fig.&nbsp;18).</p>""")
    a(fig(F.fig_pte(), "Page-table entry and inverted frame table."))
    a("""<p>For a power-of-two page size <i>S</i>, k = log<sub>2</sub>S; vpn = va &gt;&gt; k,
offset = va &amp; (S&minus;1) and pa = (f &lt;&lt; k) | offset (Fig.&nbsp;19). The full access path,
including hit, fault, eviction and write-back, is Fig.&nbsp;9. Faults are <i>compulsory</i> on a page's
first use and <i>capacity</i> afterwards. A hit costs O(1), a fault O(F); space is O(P + F).</p>""")
    a(alg("Paging address translation", ["validate pid and vpn; <b>if</b> invalid <b>then</b> AccessViolation, <b>return</b>",
        "refs &larr; refs + 1", "<b>if</b> page present <b>then</b> PageHit",
        "<b>else</b> PageFault; take lowest free frame <b>or</b> policy victim",
        "&nbsp;&nbsp;<b>if</b> victim dirty <b>then</b> PageWrittenBack; evict; load page; update frame owner",
        "set R; <b>if</b> write <b>then</b> set D", "<b>return</b> pa = (frame &lt;&lt; k) | offset"]))
    a(fig(F.fig_translate_example(), "Translating va = 6700 with page size 4096."))
    a(fig(F.fig_page_life(), "Page life cycle; dirty victims are written back."))
    a(tab("Paging invariants (checked after every step)", ["ID", "Invariant"], [
        ["V1", "page tables and frame owners agree"], ["V2", "every frame is free or owned"],
        ["V3", "non-resident pages have no frame, R or D"], ["V4", "resident pages &le; frames"],
        ["V5", "refs = hits + faults; write-backs &le; evictions"]]))
    a(tab("Negative tests (state unchanged)", ["Access", "Condition", "Result"], [
        ["P0, page 9", "outside address space", "OUT_OF_RANGE"],
        ["P9, page 0", "no such process", "NO_SUCH_PROCESS"]]))

    # ---------------------------------------------------------- 11
    a(sec("FIFO, LRU and Optimal Replacement"))
    a("""<p>When a needed page is not in RAM and every frame is already full, some page must be thrown out
to make room. A good choice throws out a page that will not be needed soon; a bad choice throws out a
page that is needed again immediately, causing another fault. FIFO, LRU and OPT are three rules for
making this choice.</p>
<p>A replacement policy keeps only its own bookkeeping and answers one question: which frame to
evict. The engine calls its hooks in a fixed order (Fig.&nbsp;21); ties go to the lowest frame.</p>""")
    a(fig(F.fig_hooks(), "Order of replacement-policy hooks on a hit and on a fault."))
    a(alg("Victim selection for FIFO, LRU and OPT", [
        "<b>input:</b> full frames 0..F&minus;1, current time t",
        "<b>FIFO:</b> <b>return</b> frame with smallest load_time",
        "<b>LRU:</b> <b>return</b> frame with smallest last_use",
        "<b>OPT:</b> <b>for each</b> frame f: nu(f) &larr; next index &gt; t where its page is referenced (&infin; if never)",
        "&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;&nbsp;<b>return</b> frame with largest nu(f)",
        "ties: lowest frame index; hooks update load_time on load and last_use on every access"]))
    a(tab("Replacement policies compared", ["Policy", "State", "Per fault", "Future?", "Stack?"], [
        ["FIFO", "load time / frame", "O(F)", "no", "no"], ["LRU", "last use / frame", "O(F)", "no", "yes"],
        ["OPT", "next-use table", "O(F)", "yes", "yes"]]))
    a("""<p>OPT evicts the page whose next use is furthest away, which can never cause an earlier fault
than any other choice (Belady, 1966); it is therefore the lower bound used to judge FIFO and LRU. Because it needs
the whole reference string, the session refuses OPT in live interactive runs.</p>""")

    # ---------------------------------------------------------- 12
    a(sec("Page Replacement Worked Examples"))
    a("""<p>The grids below are what <code>memsim refs --table</code> prints for the 20-reference string of
Silberschatz with 3 frames (AC-3). Shaded cells mark the frame loaded on a fault.</p>""")
    for pol, n in [("fifo", "FIFO: 15 faults (6 compulsory, 9 capacity)"),
                   ("lru", "LRU: 12 faults (6 compulsory, 6 capacity)"),
                   ("opt", "OPT: 9 faults (6 compulsory, 3 capacity)")]:
        a(fig(F.grid(F.AC3, 3, pol, cell=14.2), n + ".", w=100))
    a("""<p>All three evict page 7 at reference 4. They first diverge at reference 6 (page 3): FIFO
evicts page 0, which is needed immediately, while LRU and OPT evict page 1.</p>""")
    a(sub("A", "Belady's Anomaly (AC-4)"))
    a(fig(F.grid(F.AC4, 3, "fifo", cell=16), "FIFO, 3 frames: 9 faults.", w=82))
    a(fig(F.grid(F.AC4, 4, "fifo", cell=16), "FIFO, 4 frames: 10 faults.", w=82))
    a("""<p>With 4 frames, from the seventh reference on FIFO always evicts exactly the page needed next,
so every reference faults.</p>""")

    # ---------------------------------------------------------- 13
    a(sec("Paging Results and Comparison"))
    a(tab("E4 configuration", ["Parameter", "Value"], [
        ["virtual pages / page size", "32 / 4096 B"], ["references per run", "2,000"],
        ["working set / phase length", "6 pages / 200 refs"], ["out-of-set probability", "5%"],
        ["seeds / frames", "10 / 1&ndash;16"]]))
    a(fig(F.fig_e4(), "E4: mean page faults vs. frames (10 seeds)."))
    a(tab("E4 page faults, mean &plusmn; 95% CI", ["Frames", "FIFO", "LRU", "OPT"], [
        ["2", "1398.2 &plusmn; 21.2", "1393.0 &plusmn; 18.8", "1070.8 &plusmn; 13.9"],
        ["4", "840.0 &plusmn; 15.9", "819.6 &plusmn; 19.9", "466.0 &plusmn; 9.3"],
        ["8", "253.1 &plusmn; 16.5", "138.9 &plusmn; 10.1", "103.2 &plusmn; 5.9"],
        ["12", "151.7 &plusmn; 9.7", "100.7 &plusmn; 6.0", "70.7 &plusmn; 4.0"],
        ["16", "106.7 &plusmn; 9.0", "83.5 &plusmn; 5.5", "53.2 &plusmn; 3.2"]], cls="small"))
    a("""<p>Smooth locality hides non-monotonic behaviour, but the AC-4 sweep exposes it: FIFO gives
12,&nbsp;12,&nbsp;9,&nbsp;10,&nbsp;5,&nbsp;5 faults for 1&ndash;6 frames while LRU and OPT never rise
(Fig.&nbsp;5). In practice OPT is unimplementable, FIFO is simple but anomalous, and LRU is strong under
locality but costly, which is why real systems approximate it with Clock.</p>""")

    # ---------------------------------------------------------- 14
    a(sec("Workloads and Performance Metrics"))
    a("""<p>A <i>workload</i> is simply a script of memory requests (allocate, free, access a page) that is
fed to the simulator. A <i>metric</i> is a number measured while it runs, such as how many requests
succeeded or how many page faults occurred. Using the same workload for every policy makes the
comparison fair.</p>""")
    a(fig(F.fig_workload(), "Workload file format and loader pipeline."))
    a(fig(F.fig_metrics(), "One collector derives every metric from the event stream."))

    # ---------------------------------------------------------- 15
    a(sec("Benchmarking and Performance Analysis"))
    a("""<p>Each benchmark below is reported in the same four parts: <b>setup</b>, <b>result table</b>,
<b>chart</b> and <b>finding</b>. Values are mean &plusmn; 95% CI over 10 seeds.</p>""")
    a(fig(F.fig_bench_pipe(), "Experiment pipeline (paired, seeded, reproducible).", w=82))
    a(sub("A", "Benchmark E1: Placement Policies under Memory Pressure"))
    a("""<p><b>Setup.</b> 30 workloads (60%, 75%, 90% target utilisation &times; 10 seeds), 3,000 operations
each on 1,024 units, request sizes 16&ndash;128, no compaction.</p>""")
    a(tab("E1 results, mean &plusmn; 95% CI", ["Util.", "Policy", "Success %", "Frag. fail.",
                                                "Scanned", "Ext. frag."], [
        ["60%", "FF", "99.9 &plusmn; 0.0", "1.2 &plusmn; 0.7", "2.29", "0.359"],
        ["60%", "BF", "100.0 &plusmn; 0.0", "0.3 &plusmn; 0.3", "3.77", "0.322"],
        ["60%", "WF", "99.0 &plusmn; 0.2", "14.3 &plusmn; 3.1", "3.88", "0.546"],
        ["75%", "FF", "97.0 &plusmn; 0.3", "44.6 &plusmn; 4.7", "2.78", "0.460"],
        ["75%", "BF", "97.9 &plusmn; 0.2", "32.0 &plusmn; 2.8", "4.66", "0.419"],
        ["75%", "WF", "93.9 &plusmn; 0.5", "92.4 &plusmn; 6.8", "4.61", "0.597"],
        ["90%", "FF", "90.0 &plusmn; 0.6", "148.6 &plusmn; 8.6", "3.21", "0.516"],
        ["90%", "BF", "91.4 &plusmn; 0.4", "127.7 &plusmn; 7.0", "5.24", "0.473"],
        ["90%", "WF", "87.2 &plusmn; 0.7", "192.0 &plusmn; 11.1", "5.19", "0.629"]], cls="small"))
    a(fig(F.fig_e1(1, "fragmentation failures", 0, 200, [0, 50, 100, 150, 200], leg=(70, 18)),
          "E1: requests failed by fragmentation despite enough free memory.", w=66))
    a(tab("E1 findings", ["Aspect", "Finding"], [
        ["Quality", "Best-Fit: highest success, lowest fragmentation at every level"],
        ["Cost", "First-Fit scans 2.3&ndash;3.2 holes vs. 3.8&ndash;5.2 for the others"],
        ["Pressure", "at 90%: 149 (FF), 128 (BF), 192 (WF) fragmentation failures"],
        ["Reproducibility", "every cell regenerates from generator + seed + git revision"]]))
    a(sub("B", "Benchmark E4: Page Faults against Frames"))
    a("""<p><b>Setup.</b> 10 seeds &times; 2,000 references, 32 pages, working set 6, phase change every 200
references; FIFO, LRU, OPT at 2&ndash;16 frames. <b>Result:</b> Table&nbsp;X and Fig.&nbsp;27 (Section 13).</p>""")
    a(tab("E4 findings", ["Aspect", "Finding"], [
        ["Ordering", "OPT &lt; LRU &lt; FIFO at every frame count"],
        ["At 8 frames", "103.2 (OPT), 138.9 (LRU), 253.1 (FIFO) faults"],
        ["Anomaly", "only FIFO, on the AC-4 string: 9 &rarr; 10 faults"]]))

    # ---------------------------------------------------------- 16
    a(sec("Testing Strategy and Correctness"))
    a("""<p>Testing answers one question: does the simulator give the right answers? We check it against
textbook examples whose answers are known, check internal consistency rules after every single step,
run it twice to confirm identical output, and run memory-error detectors over the C code.</p>""")
    a(fig(F.fig_test_layers(), "Independent test layers; a bug missed by one is caught by another."))
    a(fig(F.fig_checked(), "Checked mode: invariants after every operation."))
    a("""<p>Randomised oracles: 5,000 compactions match a unit-by-unit oracle; no policy beats OPT; LRU and
OPT never show Belady's anomaly; every fixture run twice gives equal SHA-256 trace digests (AC-6).
Expected values are written into each test before it is compared with the engine, taken from the
Silberschatz textbook or from Belady's original paper.</p>""")

    # ---------------------------------------------------------- 17
    a(sec("Test Cases and Test Results"))
    a(tab("Acceptance criteria (verify_report.sh)", ["ID", "Scenario", "Expected", "Result"], [
        ["AC-1", "five-hole placement", "FF/WF fail a4, BF places all", "PASS"],
        ["AC-2", "compaction + retry", "7 blocks, 781 units; [1207,1740)", "PASS"],
        ["AC-3", "20 refs, 3 frames", "FIFO 15, LRU 12, OPT 9", "PASS"],
        ["AC-4", "Belady sweep", "FIFO 12,12,9,10,5,5", "PASS"],
        ["AC-5", "negative tests", "3 rejections, 2 violations", "PASS"],
        ["AC-6", "determinism", "6/6 traces identical", "PASS"],
        ["AC-7", "invariants", "17/17 suites", "PASS"],
        ["AC-8", "compare table", "shows fail FRAGMENTATION", "PASS"]], cls="small"))
    suites = ["smoke", "cont_engine", "p3_policies", "fragmentation", "compaction", "compaction_window",
              "p4_demo", "p4_random", "page_engine", "replacement", "writer", "loader", "generators", "metrics",
              "ac_fixtures", "pipeline", "cli_smoke"]
    d = F.D(340, 86)
    for i, s in enumerate(suites):
        x = 4 + (i % 4) * 83
        y = 4 + (i // 4) * 16
        d.box(x, y, 79, 14, s, 6.8, mono=True)
    d.text(170, 98 - 14, "17 / 17 suites pass under ASan + UBSan and Valgrind", 7.2, italic=True)
    a(fig(d.svg(), "Automated test suites (all passing)."))

    # ---------------------------------------------------------- 18
    a(sec("User Interface and Workflow"))
    a(fig(F.fig_cli(), "The memsim command-line interface."))
    a(fig(F.fig_web(), "Layout of the Contiguous page of the web laboratory."))
    a("""<p>The lab has four pages: Overview (live run and six classic scenarios), Contiguous, Paging
(frames grid, page tables, translation, Belady curve) and Replay (step through any
<code>memsim run --trace</code> file). Neither interface contains allocation or replacement logic.</p>""")
    a(fig(F.fig_workflow(), "Typical workflow in the lab."))

    # ---------------------------------------------------------- 19
    a(sec("Execution Examples and Output"))
    a("<p>Unedited terminal output (commit ab5d17d).</p>")
    a(term(["$ memsim compare ac1_silberschatz.wl \\", "    --policies first-fit,best-fit,worst-fit",
            "id  size  first-fit           best-fit  worst-fit", "a1  212   110                 830       1140",
            "a2  417   1140                110       110", "a3  112   322                 620       1352",
            "a4  426   fail FRAGMENTATION  1140      fail FRAGMENTATION"]))
    a(term(["$ memsim run ac2_compact.wl --policy first-fit", "compactions 1   blocks moved 7   units moved 781",
            "ext frag final 0.000000   hole count final 1", "total free final 533   largest hole final 533"]))
    a(term(["$ memsim sweep ac4_belady.wl --policy fifo --frames 1..6", "1 12", "2 12", "3 9", "4 10 *", "5 5",
            "6 5"]))
    a(term(["$ memsim run ac5_contiguous.wl  -> rejections 3", "$ memsim run ac5_paging.wl      -> violations 2",
            "access P0 @6700 -> AddressTranslated vpn 1,", "    offset 2604, frame 1, pa 6700",
            "run summary: refs 4 hits 1 faults 3 evictions 1"]))
    a(term(["$ memsim gen alloc-uniform --seed 7 -o w.wl",
            "schema 1 / kind contiguous / memory_size 1024 /", "gen_name alloc-uniform / gen_seed 7 / gen_ops 2000"]))

    # ---------------------------------------------------------- 20
    a(sec("Repository Structure and Development Conventions"))
    a(fig(F.fig_repo(), "Top-level repository layout."))
    a(fig(F.fig_layer_chain(), "Allowed include direction between module layers."))
    a(tab("Conventions and ownership", ["Rule", "Practice"], [
        ["naming", "prefix ms_ / MS_, e.g. ms_cont_engine_alloc"], ["format", "clang-format, .editorconfig"],
        ["ownership", "CODEOWNERS, roles P1&ndash;P10"], ["interfaces", "change via RFC, lead approves"],
        ["commits", "pre-commit: format + layering"]]))

    # ---------------------------------------------------------- 21
    a(sec("Build System and Continuous Integration"))
    a(fig(F.fig_build(), "Build orchestration: native presets and the WebAssembly build."))
    a(tab("CMake presets", ["Preset", "Configuration", "Use"], [
        ["dev", "Debug + ASan + UBSan", "daily work, main tests"], ["release", "-O2", "experiments, binary"],
        ["valgrind", "Debug", "memcheck on every test"]]))
    a(fig(F.fig_ci(), "GitHub Actions jobs and the single merge gate."))
    a("""<p>All code builds as C17 with <code>-Wall -Wextra -Wpedantic -Wshadow -Wconversion
-Wsign-conversion</code>, and with <code>-Werror</code> in CI.</p>""")

    # ---------------------------------------------------------- 22
    a(sec("Setup Instructions and Documentation"))
    a("""<p><b>Requirements:</b> Linux or macOS (Windows via WSL), GCC or Clang, CMake &ge; 3.25, Ninja,
pre-commit; Python 3 + matplotlib for experiments; Node.js and Clang/lld/wasi-libc for the web app.</p>""")
    a(fig(F.fig_setup(), "Build, test and run sequence."))
    a(term(["$ cmake --preset release && cmake --build --preset release", "$ bash scripts/experiments/run_all.sh",
            "$ bash scripts/report/verify_report.sh"]))
    a(tab("Documentation", ["Document", "Content"], [
        ["README.md", "overview, quick start, commands"], ["docs/ARCHITECTURE.md", "requirements, design, ADRs"],
        ["docs/report/", "report, tables, figures"], ["analysis/README.md", "statistics and plots"],
        [".github/", "PR, bug and RFC templates"]]))
    a("<p>A hosted copy of the lab is at memforge.aryagupta-dev.xyz.</p>")

    # ---------------------------------------------------------- 23
    a(sec("Conclusion and Future Scope"))
    a("""<p>MemForge meets its goals: a deterministic, modular C17 simulator that reproduces the textbook
examples exactly and compares policies fairly. Best-Fit gives the best success and lowest fragmentation,
First-Fit is cheapest, Worst-Fit is worst on both; OPT &lt; LRU &lt; FIFO at every frame count, and only
FIFO shows Belady's anomaly.</p>""")
    a(tab("Goals and evidence", ["Area", "Delivered", "Evidence"], [
        ["Contiguous", "FF/BF/WF, split, coalesce, compaction", "AC-1, AC-2, E1"],
        ["Paging", "translation, FIFO/LRU/OPT, sweep", "AC-3&ndash;5, E4"],
        ["Determinism", "byte-identical traces", "AC-6"], ["Correctness", "invariants, oracles", "AC-7, 17/17"],
        ["Delivery", "CLI + wasm from one code base", "web equivalence test"]], cls="small"))
    a("""<p><b>Lessons:</b> determinism must be designed in (integer state, logical clocks, tie-breaks);
separating decisions from mutation made policies trivially testable; compiling the real engine to
WebAssembly removed demo drift; automated layering checks caught boundary violations early.</p>
<p><b>Future work:</b> Next-Fit, buddy allocation, alignment; Clock, LFU, a TLB and two-level tables;
local replacement; Knuth's fifty-percent rule; real program traces; fuzzing and a coverage gate.</p>""")

    # ---------------------------------------------------------- refs
    TOC_ENTRIES.extend([(t, k) for t, k in TOC])
    TOC_ENTRIES.append(("Appendix", "24. APPENDIX"))

    # ---------------------------------------------------------- appendix
    a('<h2 id="app">24. APPENDIX</h2>')
    a(tab("Complete AC-1 comparison metrics", ["Metric", "FF", "BF", "WF"], [
        ["requests / successes", "13 / 12", "13 / 13", "13 / 12"], ["failures (frag.)", "1", "0", "1"],
        ["frees", "5", "5", "5"], ["scanned total / mean", "23 / 1.769", "29 / 2.231", "29 / 2.231"],
        ["ext_frag final / mean", "0.687 / 0.246", "0.674 / 0.212", "0.687 / 0.245"],
        ["holes / free / largest", "5 / 959 / 300", "5 / 533 / 174", "5 / 959 / 300"],
        ["used / live blocks", "781 / 7", "1207 / 8", "781 / 7"]], cls="small"))
    a(tab("Paging metrics for the textbook strings", ["Run", "Hits", "Faults", "Comp.", "Cap.", "Hit ratio", "Evict."], [
        ["AC-3 FIFO", "5", "15", "6", "9", "0.250", "12"], ["AC-3 LRU", "8", "12", "6", "6", "0.400", "9"],
        ["AC-3 OPT", "11", "9", "6", "3", "0.550", "6"], ["AC-4 FIFO", "3", "9", "5", "4", "0.250", "6"],
        ["AC-4 LRU", "2", "10", "5", "5", "0.167", "7"], ["AC-4 OPT", "5", "7", "5", "2", "0.417", "4"]], cls="small"))
    a(term(['{"seq":40,"step":18,"type":"PlacementDecided",', ' "data":{"policy":"first-fit","requested":426,',
            '  "hole":null,"scanned":5}}', '{"seq":41,"step":18,"type":"AllocationFailed",',
            ' "data":{"pid":"P4","id":"a4","requested":426,', '  "cause":"FRAGMENTATION","total_free":959,',
            '  "largest_hole":300}}']))
    a(fig(F.fig_events(), "Event catalogue (23 types)."))
    a("</div>")
    return "".join(h)


CSS = """
@page { size: Letter; margin: 20mm 21mm 22mm 21mm; }
* { box-sizing: border-box; }
html, body { background: #fff; }
body { margin: 0; font-family: 'Liberation Serif', 'Times New Roman', Times, serif; font-size: 12pt;
  line-height: 1.5; color: #000; text-align: justify; }
.frame { position: fixed; top: -12mm; bottom: -12mm; left: -10mm; right: -10mm; border: 3px double #000;
  pointer-events: none; }
.page { break-after: page; }
.ph, h2 { text-align: right; font-size: 14pt; font-weight: bold; margin: 18px 0 14px; padding-bottom: 6px;
  border-bottom: 3px double #000; break-after: avoid; }
.ph { margin-top: 30px; text-align: center; }
table.toc { width: 100%; border-collapse: collapse; font-size: 11.5pt; }
.toc td, .toc th { border: 0.8pt solid #000; padding: 2px 8px; line-height: 1.3; }
.toc td:nth-child(1), .toc td:nth-child(3), .toc th { text-align: center; }
h2 { margin-top: 0; break-before: page; }
h2#abs { break-before: auto; }
h3 { font-size: 12pt; font-weight: bold; margin: 12px 0 4px; break-after: avoid; }
p { margin: 0 0 8px; text-indent: 12mm; }
p.abs { text-indent: 12mm; }
p.dropcap::first-letter { font-size: inherit; }
code { font-family: 'Liberation Mono', monospace; font-size: 10pt; }
ol.tight { margin: 0 0 8px; padding-left: 22px; }
.fig { margin: 10px auto 14px; break-inside: avoid; text-align: center; width: 150mm; }
.fig svg { display: block; margin: 0 auto; }
figcaption { font-size: 10pt; font-style: italic; text-align: center; margin-top: 4px; }
.tbl { break-inside: avoid; margin: 10px auto 14px; width: 92%; }
.tcap { text-align: center; font-size: 10pt; font-weight: bold; margin-bottom: 4px; }
.tcap span { font-weight: normal; font-style: italic; }
.tbl table { width: 100%; border-collapse: collapse; font-size: 10pt; line-height: 1.3; text-align: left; }
.tbl.small table { font-size: 9.5pt; }
.tbl th { border-top: 1.2pt solid #000; border-bottom: 0.6pt solid #000; padding: 3px 6px; }
.tbl td { padding: 2px 6px; vertical-align: top; }
.tbl tbody tr:last-child td { border-bottom: 1.2pt solid #000; }
.alg { border-top: 1.2pt solid #000; border-bottom: 1.2pt solid #000; margin: 10px 0 14px; font-size: 10.5pt;
  line-height: 1.4; break-inside: avoid; text-align: left; }
.alg .ah { border-bottom: 0.6pt solid #000; padding: 3px 0; margin-bottom: 3px; }
.alg .ln { display: inline-block; width: 24px; font-size: 9.5pt; }
pre.term { font-family: 'Liberation Mono', monospace; font-size: 9.6pt; line-height: 1.3; border: 0.8pt solid #000;
  padding: 6px 8px; margin: 6px 0 12px; white-space: pre-wrap; break-inside: avoid; text-align: left; }
ol.refs { padding-left: 30px; font-size: 11pt; text-align: left; }
ol.refs li::marker { content: "[" counter(list-item) "]  "; }
"""


def render(html_path, pdf_path):
    js = f"""
const {{ chromium }} = require('playwright');
(async () => {{
  const b = await chromium.launch();
  const p = await b.newPage();
  await p.goto('file://{html_path}');
  await p.pdf({{ path: '{pdf_path}', format: 'Letter', printBackground: true, displayHeaderFooter: false,
    headerTemplate: '<span></span>',
    footerTemplate: '<div style="width:100%;text-align:center;font-family:Liberation Serif,serif;font-size:9px"><span class="pageNumber"></span></div>',
    preferCSSPageSize: true }});
  await b.close();
}})();"""
    (HERE / "_render.js").write_text(js)
    subprocess.run(["node", str(HERE / "_render.js")], check=True, cwd=HERE)


def page_of(pdf, keys):
    n = int(re.search(r"Pages:\s+(\d+)", subprocess.run(["pdfinfo", pdf], capture_output=True, text=True).stdout).group(1))
    texts = [subprocess.run(["pdftotext", "-f", str(i), "-l", str(i), pdf, "-"], capture_output=True,
                            text=True).stdout for i in range(1, n + 1)]
    out = {}
    for idx, (title, key) in enumerate(TOC_ENTRIES):
        pat = re.compile(r"^\s*" + re.escape(key) + r"\s*$", re.M)
        for i, t in enumerate(texts):
            if i < 1:
                continue
            if pat.search(t):
                out[idx] = i + 1 + 3
                break
    return out, n


def main():
    b = body()
    # headings render in small caps; pdftotext gives the source case, so search the source title
    global TOC_ENTRIES

    html_path = HERE / "memforge-ieee.html"
    pdf_path = HERE / "memforge-ieee.pdf"
    pages = {}
    for _ in range(2):
        html = (f"<!doctype html><html><head><meta charset='utf-8'><title>MemForge IEEE Report</title>"
                f"<style>{CSS}</style></head><body>{front(pages)}{b}</body></html>")
        html_path.write_text(html)
        render(html_path, pdf_path)
        pages, n = page_of(str(pdf_path), None)
    finish(pdf_path)
    print("pages:", n + 3, "toc:", pages)


def finish(pdf_path):
    """Prepend the college's first three pages verbatim and stamp page numbers from 4."""
    from pypdf import PdfReader, PdfWriter
    from reportlab.pdfgen import canvas
    import io
    body = PdfReader(str(pdf_path))
    w = PdfWriter()
    for p in PdfReader(str(HERE / "first3.pdf")).pages:
        w.add_page(p)
    for k, p in enumerate(body.pages):
        buf = io.BytesIO()
        W, H = float(p.mediabox.width), float(p.mediabox.height)
        c = canvas.Canvas(buf, pagesize=(W, H))
        c.setLineWidth(1)
        c.rect(36, 36, W - 72, H - 72)
        c.rect(39.5, 39.5, W - 79, H - 79)
        c.setFont("Times-Roman", 10)
        c.drawCentredString(W / 2, 46, str(k + 4))
        c.save()
        buf.seek(0)
        p.merge_page(PdfReader(buf).pages[0])
        w.add_page(p)
    with open(HERE / "MemForge-Report.pdf", "wb") as f:
        w.write(f)


if __name__ == "__main__":
    main()
