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


def fig(svg, cap, wide=False):
    _fig[0] += 1
    return (f'<figure class="fig"><div class="art">{svg}</div>'
            f'<figcaption>Fig. {_fig[0]}. {cap}</figcaption></figure>')


def tab(cap, head, rows, cls=""):
    _tab[0] += 1
    th = "".join(f"<th>{h}</th>" for h in head)
    tr = "".join("<tr>" + "".join(f"<td>{c}</td>" for c in r) + "</tr>" for r in rows)
    return (f'<div class="tbl {cls}"><div class="tcap">TABLE {ROMAN[_tab[0]-1]}<br>'
            f'<span>{cap}</span></div><table><thead><tr>{th}</tr></thead><tbody>{tr}</tbody></table></div>')


def sec(title):
    _sec[0] += 1
    r = ROMAN[_sec[0] - 1]
    TOC.append((title, f"{r}. {title.upper()}"))
    return f'<h2 id="s{_sec[0]}">{r}. {title}</h2>'


def sub(letter, t):
    return f"<h3>{letter}. {t}</h3>"


def alg(title, lines):
    _alg[0] += 1
    body = "".join(f"<div><span class='ln'>{i+1}:</span>{l}</div>" for i, l in enumerate(lines))
    return f'<div class="alg"><div class="ah"><b>Algorithm {_alg[0]}:</b> {title}</div>{body}</div>'


def term(lines):
    from html import escape
    return '<pre class="term">' + "\n".join(escape(l) for l in lines) + "</pre>"


# =================================================================== FRONT
def front(toc_pages):
    names = "".join(f"<div>{n} ({r})</div>" for n, r, _ in TEAM)
    cover = f"""
<section class="page cover">
  <div class="ctitle">{TITLE}</div>
  <p class="cnote">An OS Mini Project submitted for the partial fulfillment of<br>the requirements for the degree</p>
  <p>of</p><p class="deg">BACHELOR OF TECHNOLOGY</p><p>in</p>
  <p class="deg2">COMPUTER SCIENCE &amp; ENGINEERING</p><p>By</p>
  <div class="names">{names}</div>
  <p class="teamno">TEAM NO. 5</p>
  <div class="inst">Department of Computer Science and Engineering<br>Institute of Engineering and Management<br>
  (School of University of Engineering and Management, Kolkata)<br>Newtown Sector<br>2026</div>
</section>"""
    rows = "".join(f"<tr><td>{i+1}</td><td>{n}</td><td>{c}</td><td></td></tr>" for i, (n, _, c) in enumerate(TEAM))
    decl = f"""
<section class="page plain">
  <h1 class="ph">DECLARATION</h1>
  <p>We hereby declare that this project is the product of our own independent work and has not been
  submitted, in whole or in part, to any other university or institution for the award of any degree,
  diploma, or other qualification. All sources of information have been duly acknowledged, and any work
  carried out in collaboration with others has been clearly indicated in the text.</p>
  <p>Signed by the B.Tech Students: -</p>
  <table class="sig"><thead><tr><th>Sl. No.</th><th>Name</th><th>Class Roll No.</th><th>Signature</th></tr></thead>
  <tbody>{rows}</tbody></table>
  <p style="margin-top:28px">Date: -</p>
</section>"""
    ack = """
<section class="page plain">
  <h1 class="ph">ACKNOWLEDGEMENT</h1>
  <p class="ind">We would like to take this opportunity to express our heartfelt gratitude to everyone who
  inspired, supported, and contributed &mdash; directly or indirectly &mdash; to the successful completion of
  this OS mini project.</p>
  <p class="ind">First and foremost, we extend our deepest thanks to our families for their unwavering support
  and encouragement throughout our academic journey. We are equally grateful to our project guide,
  [Guide Name], for his/her unwavering support, guidance, and encouragement throughout this project.
  His/her knowledge and guidance have been invaluable in shaping our work and in preparing this report.
  We would also like to express our sincere thanks to the faculty members and fellow students of the
  Department of Computer Science and Engineering, University of Engineering and Management, Kolkata, for
  their helpful suggestions and support, and for allowing us to call this institution home.</p>
  <p class="ind">Finally, we bow in gratitude to the Almighty for the strength, clarity, and perseverance to
  endure and overcome the challenges along this journey. This project has been a rewarding learning
  experience, and we feel deeply blessed to have had the support and grace needed to complete it.</p>
  <p style="margin-top:40px">Team No. 5<br>University of Engineering and Management, Kolkata</p>
</section>"""
    trs = "".join(f"<tr><td>{i+1}</td><td>{t}</td><td>{toc_pages.get(i, '')}</td></tr>"
                  for i, (t, _) in enumerate(TOC_ENTRIES))
    toc = f"""
<section class="page plain">
  <h1 class="ph">TABLE OF CONTENTS</h1>
  <table class="toc"><thead><tr><th>S. No.</th><th>Section Topic</th><th>Page No.</th></tr></thead>
  <tbody>{trs}</tbody></table>
</section>"""
    return cover + decl + ack + toc


TOC_ENTRIES = []  # filled after body generation: (title, search key)


# =================================================================== BODY
def body():
    authors = ", ".join(n for n, _, _ in TEAM)
    h = []
    a = h.append
    a(f"""<div class="ieee-head"><div class="ttl">{TITLE}</div>
<div class="auth">{authors}</div>
<div class="aff">Department of Computer Science and Engineering, Institute of Engineering and Management,
University of Engineering and Management, Kolkata, India &middot; Team No. 5</div></div>""")
    a('<div class="cols">')
    # ---------------------------------------------------------- abstract
    a("""<p class="abs"><b><i>Abstract</i>&mdash;MemForge is a deterministic simulator of an operating-system
memory manager, written in C17 and also compiled to WebAssembly. It covers contiguous allocation
(First-, Best- and Worst-Fit, splitting, coalescing, sliding compaction) and demand paging (single-level
page tables, address translation, FIFO, LRU and OPT replacement). Policies only decide and engines only
mutate, and every change is published as an event, so identical inputs and seeds always produce
byte-identical traces. On 30 seeded workloads at 90% utilisation, Best-Fit reaches a 91.4&nbsp;&plusmn;&nbsp;0.4%
success rate against 90.0&nbsp;&plusmn;&nbsp;0.6% for First-Fit and 87.2&nbsp;&plusmn;&nbsp;0.7% for Worst-Fit,
while First-Fit scans the fewest holes (3.21 vs. 5.24 per request). On 2,000-reference workloads,
faults follow OPT &lt; LRU &lt; FIFO at every frame count (103.2, 138.9 and 253.1 at 8 frames), and a
frame sweep reproduces Belady's anomaly (9 &rarr; 10 faults). All eight acceptance criteria and 17
test suites pass under AddressSanitizer, UBSan and Valgrind.</b></p>
<p class="abs"><b><i>Index Terms</i>&mdash;memory management, deterministic simulation, contiguous
allocation, fragmentation, demand paging, page replacement, Belady's anomaly.</b></p>""")

    # ---------------------------------------------------------- 1
    a(sec("Introduction"))
    a("""<p class="dropcap">Memory management decides where each process lives in physical memory and how
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
    a("""<p>The MMU maps each <i>logical</i> address produced by the CPU to a <i>physical</i> address and traps
invalid accesses. Under contiguous allocation each request receives one run of memory taken from a free
<i>hole</i>: First-Fit takes the first hole that fits, Best-Fit the smallest and Worst-Fit the largest.
<i>External</i> fragmentation is free memory split into holes too small to use; <i>internal</i>
fragmentation is unused space inside a block (zero in MemForge, since granted = requested).
Compaction slides blocks together to merge the holes (Fig.&nbsp;3).</p>""")
    a(fig(F.fig_concepts(), "Address translation, external fragmentation and compaction."))
    a("""<p>Paging splits logical memory into fixed-size <i>pages</i> and physical memory into equal
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
    a(fig(F.fig_layers(), "Five layers; each layer uses only the ones below it."))
    a(sub("B", "Deterministic Core and Session Facade"))
    a("""<p>The core performs no I/O and reads no wall clock; it uses logical clocks (<i>step</i>,
<i>t</i>, <i>seq</i>), integer arithmetic and fixed tie-breaks (lowest address, lowest frame). Every
client goes through the <code>Session</code> facade, which applies one operation, checks invariants and
publishes the step's events in <code>seq</code> order (Fig.&nbsp;7).</p>""")
    a(fig(F.fig_dataflow(), "End-to-end data flow of one run."))
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
    a(fig(F.fig_alloc_seq(), "Allocation sequence in the contiguous subsystem."))
    a(fig(F.fig_access_flow(), "Paged memory access with fault handling and replacement."))

    # ---------------------------------------------------------- 5
    a(sec("Module Description and Dependencies"))
    a(tab("Module catalogue", ["Module", "Responsibility"], [
        ["common", "types, config, errors, events, bus, registry"],
        ["contiguous", "blocks, engine, FF/BF/WF, fragmentation, compaction"],
        ["paging", "page/frame tables, MMU, FIFO/LRU/OPT, Belady sweep"],
        ["trace", "JSONL writer and snapshots"], ["workloads", "loader, validator, seeded generators"],
        ["metrics", "every reported figure, from events"], ["session", "facade: apply, check, publish"],
        ["bench", "runner over workload &times; policy &times; seed"], ["cli", "memsim commands, SHA-256"]]))
    a(fig(F.fig_modules(), "Module dependency graph (acyclic, checked by check_layers.sh)."))
    a("""<p>Each module exports only what others need; policies cannot modify engine data, and the core
may not include <code>stdio.h</code>, <code>time.h</code> or system headers. Policies are
function-pointer tables registered by name (Fig.&nbsp;11), so a new policy is one file and one
registry line.</p>""")
    a(fig(F.fig_vtable(), "Policy vtable and registry."))

    # ---------------------------------------------------------- 6
    a(sec("Memory Model and Allocation State"))
    a("""<p>Memory [0,&nbsp;M) is an address-ordered array of <code>ms_block</code> records that tile it
exactly. Workloads refer to blocks by stable allocation ids, never by address, because compaction moves
blocks; each id passes through the lifecycle in Fig.&nbsp;12, which rules out duplicate ids and
double frees.</p>""")
    a(fig(F.fig_block_model(), "Block array, block record and allocation-id lifecycle."))
    a(fig(F.fig_split_coalesce(), "Splitting a hole on allocation and coalescing on free."))
    a(tab("Model operations", ["Operation", "Time", "Space"], [
        ["split", "O(1), O(N) worst (shift)", "O(1)"], ["free + coalesce", "O(1), O(N) worst", "O(1)"]]))

    # ---------------------------------------------------------- 7
    a(sec("First-Fit, Best-Fit and Worst-Fit Algorithms"))
    a("""<p>A placement policy is a pure selector: given the holes in address order and a size
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
    a(fig(F.fig_ac1_policies(), "Placements on the Silberschatz five-hole memory (AC-1)."))
    a("""<p><i>Excluded:</i> Next-Fit (a roving cursor complicates coalescing and compaction for no clear
gain), size-indexed trees (O(log h) but harder to keep deterministic) and the buddy system
(power-of-two sizes cause internal fragmentation).</p>""")

    # ---------------------------------------------------------- 8
    a(sec("Fragmentation and Compaction"))
    a(tab("Fragmentation metrics", ["Metric", "Definition"], [
        ["total_free, largest_hole, hole_count", "sum of holes; largest; number of holes"],
        ["ext_frag", "1 &minus; largest_hole / total_free (0 if none free), in ppm"],
        ["failure cause", "FRAGMENTATION if total_free &ge; s &gt; largest; else CAPACITY"],
        ["blocks_moved, units_moved", "cost of compaction"]]))
    a("""<p>Ratios are integer parts-per-million, (p/w)&middot;10<sup>6</sup> + ((p mod w)&middot;10<sup>6</sup>)/w,
so every machine prints the same value (Fig.&nbsp;15).</p>""")
    a(fig(F.fig_frag_metrics(), "Fragmentation metrics when First-Fit fails the 426-unit request."))
    a(fig(F.fig_compaction(), "Compaction pipeline and its effect on AC-2."))
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
the retried a4 lands at 781, leaving [1207,&nbsp;1740) free (Fig.&nbsp;16).</p>""")
    a("""<p><b>Experiment E1</b> runs 30 seeded workloads (60/75/90% target utilisation &times; 10 seeds;
3,000 operations on 1,024 units; sizes 16&ndash;128; allocation probability 0.6). Figs.&nbsp;17&ndash;18
plot mean values; Section&nbsp;XV gives 95% confidence intervals.</p>""")
    a(fig(F.fig_e1(0, "allocation success (%)", 86, 100, [86, 90, 94, 98], leg=(70, 72)),
          "E1: allocation success rate vs. target utilisation."))
    a(fig(F.fig_e1(2, "holes scanned / request", 2, 6, [2, 3, 4, 5, 6], leg=(70, 18)),
          "E1: search cost (mean holes scanned per request)."))
    a("""<p>Best-Fit wins on success and fragmentation, Worst-Fit loses on both, and First-Fit is the
cheapest to run. All policies degrade as utilisation rises.</p>""")

    # ---------------------------------------------------------- 10
    a(sec("Paging Model and Address Translation"))
    a("""<p>The paging subsystem has four parts: MMU, per-process page tables, an inverted frame table and
the paging engine (Fig.&nbsp;19).</p>""")
    a(fig(F.fig_pte(), "Page-table entry and inverted frame table."))
    a("""<p>For a power-of-two page size <i>S</i>, k = log<sub>2</sub>S; vpn = va &gt;&gt; k,
offset = va &amp; (S&minus;1) and pa = (f &lt;&lt; k) | offset (Fig.&nbsp;20). The full access path,
including hit, fault, eviction and write-back, is Fig.&nbsp;9. Faults are <i>compulsory</i> on a page's
first use and <i>capacity</i> afterwards. A hit costs O(1), a fault O(F); space is O(P + F).</p>""")
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
    a("""<p>A replacement policy keeps only its own bookkeeping and answers one question: which frame to
evict. The engine calls its hooks in a fixed order (Fig.&nbsp;22); ties go to the lowest frame.</p>""")
    a(fig(F.fig_hooks(), "Order of replacement-policy hooks on a hit and on a fault."))
    a(tab("Replacement policies compared", ["Policy", "State", "Per fault", "Future?", "Stack?"], [
        ["FIFO", "load time / frame", "O(F)", "no", "no"], ["LRU", "last use / frame", "O(F)", "no", "yes"],
        ["OPT", "next-use table", "O(F)", "yes", "yes"]]))
    a("""<p>OPT evicts the page whose next use is furthest away, which can never cause an earlier fault
than any other choice [3]; it is therefore the lower bound used to judge FIFO and LRU. Because it needs
the whole reference string, the session refuses OPT in live interactive runs.</p>""")

    # ---------------------------------------------------------- 12
    a(sec("Page Replacement Worked Examples"))
    a("""<p>The grids below are what <code>memsim refs --table</code> prints for the 20-reference string of
Silberschatz with 3 frames (AC-3). Shaded cells mark the frame loaded on a fault.</p>""")
    for pol, n in [("fifo", "FIFO: 15 faults (6 compulsory, 9 capacity)"),
                   ("lru", "LRU: 12 faults (6 compulsory, 6 capacity)"),
                   ("opt", "OPT: 9 faults (6 compulsory, 3 capacity)")]:
        a(fig(F.grid(F.AC3, 3, pol, cell=14.2), n + "."))
    a("""<p>All three evict page 7 at reference 4. They first diverge at reference 6 (page 3): FIFO
evicts page 0, which is needed immediately, while LRU and OPT evict page 1.</p>""")
    a(sub("A", "Belady's Anomaly (AC-4)"))
    a(fig(F.grid(F.AC4, 3, "fifo", cell=16), "FIFO, 3 frames: 9 faults."))
    a(fig(F.grid(F.AC4, 4, "fifo", cell=16), "FIFO, 4 frames: 10 faults."))
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
    a(fig(F.fig_workload(), "Workload file format and loader pipeline."))
    a(fig(F.fig_metrics(), "One collector derives every metric from the event stream."))

    # ---------------------------------------------------------- 15
    a(sec("Benchmarking and Performance Analysis"))
    a(fig(F.fig_bench_pipe(), "Experiment pipeline (paired, seeded, reproducible)."))
    a(tab("E1 results without compaction, mean &plusmn; 95% CI", ["Util.", "Policy", "Success %", "Frag. fail.",
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
          "E1: requests failed by fragmentation despite enough free memory."))
    a("""<p><b>Quality:</b> Best-Fit has the highest success and lowest fragmentation at every level.
<b>Cost:</b> First-Fit scans 2.3&ndash;3.2 holes vs. 3.8&ndash;5.2. <b>Pressure:</b> at 90%,
fragmentation failures reach 149 (FF), 128 (BF) and 192 (WF). <b>Reproducibility:</b> every cell
regenerates from the generator, the seed and the git revision.</p>""")

    # ---------------------------------------------------------- 16
    a(sec("Testing Strategy and Correctness"))
    a(fig(F.fig_test_layers(), "Independent test layers; a bug missed by one is caught by another."))
    a(fig(F.fig_checked(), "Checked mode: invariants after every operation."))
    a("""<p>Randomised oracles: 5,000 compactions match a unit-by-unit oracle; no policy beats OPT; LRU and
OPT never show Belady's anomaly; every fixture run twice gives equal SHA-256 trace digests (AC-6).
Expected values are written into each test before it is compared with the engine, taken from the
textbook [1] or from Belady's paper [4].</p>""")

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
    a('<h2 class="unnum" id="refs">References</h2>')
    refs = [
        "A. Silberschatz, P. B. Galvin and G. Gagne, <i>Operating System Concepts</i>, 10th ed. Hoboken, NJ, USA: Wiley, 2018.",
        "A. S. Tanenbaum and H. Bos, <i>Modern Operating Systems</i>, 4th ed. Boston, MA, USA: Pearson, 2015.",
        "L. A. Belady, &ldquo;A study of replacement algorithms for a virtual-storage computer,&rdquo; <i>IBM Syst. J.</i>, vol. 5, no. 2, pp. 78&ndash;101, 1966.",
        "L. A. Belady, R. A. Nelson and G. S. Shedler, &ldquo;An anomaly in space-time characteristics of certain programs running in a paging machine,&rdquo; <i>Commun. ACM</i>, vol. 12, no. 6, pp. 349&ndash;353, Jun. 1969.",
        "D. E. Knuth, <i>The Art of Computer Programming, Vol. 1: Fundamental Algorithms</i>, 3rd ed. Reading, MA, USA: Addison-Wesley, 1997, sec. 2.5.",
        "P. J. Denning, &ldquo;The working set model for program behavior,&rdquo; <i>Commun. ACM</i>, vol. 11, no. 5, pp. 323&ndash;333, May 1968.",
        "R. L. Mattson, J. Gecsei, D. R. Slutz and I. L. Traiger, &ldquo;Evaluation techniques for storage hierarchies,&rdquo; <i>IBM Syst. J.</i>, vol. 9, no. 2, pp. 78&ndash;117, 1970.",
        "D. Blackman and S. Vigna, &ldquo;Scrambled linear pseudorandom number generators,&rdquo; <i>ACM Trans. Math. Softw.</i>, vol. 47, no. 4, art. 36, 2021.",
        "<i>Information Technology&mdash;Programming Languages&mdash;C</i>, ISO/IEC 9899:2018.",
        "WebAssembly Community Group, <i>WebAssembly Core Specification</i>, W3C.",
        "Kitware, <i>CMake Reference Documentation</i>. [Online]. Available: https://cmake.org/documentation/",
        "<i>Svelte Documentation</i>, https://svelte.dev/docs; <i>Valgrind User Manual</i>, https://valgrind.org/docs/manual/."]
    a('<ol class="refs">' + "".join(f"<li>{r}</li>" for r in refs) + "</ol>")
    TOC_ENTRIES.append(("References", "References"))

    # ---------------------------------------------------------- appendix
    a('<h2 class="unnum" id="app">Appendix</h2>')
    TOC_ENTRIES.append(("Appendix", "Appendix"))
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
@page { size: A4; margin: 17mm 15mm 18mm 15mm; }
* { box-sizing: border-box; }
body { margin: 0; font-family: 'Liberation Serif', 'Times New Roman', Times, serif; font-size: 10pt;
  line-height: 1.24; color: #000; background: #fff; }
.page { page-break-after: always; break-after: page; min-height: 255mm; }
.cover { text-align: center; padding-top: 6mm; font-size: 13pt; }
.cover p { margin: 4px 0; }
.ctitle { font-size: 21pt; font-weight: bold; margin: 0 8mm 14mm; line-height: 1.25; }
.deg, .deg2 { font-weight: bold; font-size: 15pt; }
.names { margin: 10px 0 6px; line-height: 1.55; font-size: 12.5pt; }
.teamno { font-weight: bold; }
.inst { margin-top: 18mm; font-weight: bold; line-height: 1.5; }
.plain { font-size: 12pt; line-height: 1.6; text-align: justify; padding: 4mm 6mm; }
.ph { text-align: center; font-size: 16pt; margin: 4mm 0 10mm; }
.ind { text-indent: 12mm; }
table.sig, table.toc { width: 100%; border-collapse: collapse; margin-top: 10px; font-size: 11.5pt; }
.sig td, .sig th, .toc td, .toc th { border: 0.6pt solid #000; padding: 4px 8px; }
.sig td:nth-child(1), .sig td:nth-child(3), .toc td:nth-child(1), .toc td:nth-child(3) { text-align: center; }
.toc td, .toc th { padding: 3.4px 8px; }
.ieee-head { text-align: center; margin-bottom: 10px; }
.ttl { font-size: 21pt; line-height: 1.15; margin: 2px 6mm 8px; }
.auth { font-size: 10.5pt; margin: 0 4mm 4px; }
.aff { font-size: 8.6pt; font-style: italic; margin-bottom: 4px; }
.cols { column-count: 2; column-gap: 6mm; column-fill: auto; text-align: justify; hyphens: auto; }
p { margin: 0 0 5px; text-indent: 1em; }
p.abs { text-indent: 0; font-size: 8.8pt; line-height: 1.2; }
p.dropcap::first-letter { float: left; font-size: 25pt; line-height: 0.9; padding: 1px 2px 0 0; }
p.dropcap { text-indent: 0; }
h2 { font-size: 9.6pt; font-weight: normal; font-variant: small-caps; text-align: center; margin: 10px 0 5px;
  letter-spacing: 0.3px; break-after: avoid; }
h3 { font-size: 9.6pt; font-weight: normal; font-style: italic; margin: 6px 0 3px; break-after: avoid; }
code { font-family: 'Liberation Mono', monospace; font-size: 8pt; }
ol.tight { margin: 0 0 5px; padding-left: 16px; } ol.tight li { margin: 0 0 1px; }
.fig { margin: 8px auto 10px; break-inside: avoid; text-align: center; column-span: all; width: 132mm; }
.fig .art { width: 100%; }
.fig svg { display: block; margin: 0 auto; }
figcaption { font-size: 8pt; text-align: center; margin-top: 3px; text-indent: 0; }
.tbl { break-inside: avoid; margin: 6px 0 8px; }
.tcap { text-align: center; font-size: 8pt; margin-bottom: 3px; letter-spacing: 0.4px; }
.tcap span { font-variant: small-caps; letter-spacing: 0; }
.tbl table { width: 100%; border-collapse: collapse; font-size: 7.8pt; line-height: 1.18; text-align: left; }
.tbl.small table { font-size: 7.2pt; }
.tbl th { border-top: 0.9pt solid #000; border-bottom: 0.5pt solid #000; padding: 2px 3px; font-weight: bold; }
.tbl td { padding: 1.5px 3px; vertical-align: top; }
.tbl tbody tr:last-child td { border-bottom: 0.9pt solid #000; }
.alg { border-top: 0.9pt solid #000; border-bottom: 0.9pt solid #000; margin: 6px 0 8px; font-size: 8.2pt;
  break-inside: avoid; text-align: left; }
.alg .ah { border-bottom: 0.5pt solid #000; padding: 2px 0; margin-bottom: 2px; }
.alg .ln { display: inline-block; width: 16px; font-size: 7.2pt; }
pre.term { font-family: 'Liberation Mono', monospace; font-size: 6.5pt; line-height: 1.25; border: 0.6pt solid #000;
  padding: 4px 5px; margin: 4px 0 7px; white-space: pre-wrap; break-inside: avoid; text-align: left; }
ol.refs { padding-left: 18px; font-size: 7.8pt; margin: 0; text-align: left; }
ol.refs li { margin-bottom: 2px; }
ol.refs li::marker { content: "[" counter(list-item) "]  "; }
h2.unnum { font-variant: small-caps; }
"""


def render(html_path, pdf_path):
    js = f"""
const {{ chromium }} = require('playwright');
(async () => {{
  const b = await chromium.launch();
  const p = await b.newPage();
  await p.goto('file://{html_path}');
  await p.pdf({{ path: '{pdf_path}', format: 'A4', printBackground: true, displayHeaderFooter: true,
    headerTemplate: '<span></span>',
    footerTemplate: '<div style="width:100%;text-align:center;font-family:Liberation Serif,serif;font-size:9px"><span class="pageNumber"></span></div>',
    margin: {{ top: '17mm', bottom: '18mm', left: '15mm', right: '15mm' }} }});
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
        if key in ("References", "Appendix"):
            pat = re.compile(r"^\s*" + key + r"\s*$", re.M | re.I)
        else:
            pat = re.compile(r"(^|\s)" + re.escape(key) + r"\b")
        for i, t in enumerate(texts):
            if i < 4:
                continue
            if pat.search(t):
                out[idx] = i + 1
                break
    return out, n


def main():
    b = body()
    # headings render in small caps; pdftotext gives the source case, so search the source title
    global TOC_ENTRIES
    TOC_ENTRIES = [(t, k if t in ("Abstract", "References", "Appendix") else k.split(". ", 1)[0] + ". " + t)
                   for t, k in TOC_ENTRIES]
    html_path = HERE / "memforge-ieee.html"
    pdf_path = HERE / "memforge-ieee.pdf"
    pages = {}
    for _ in range(2):
        html = (f"<!doctype html><html><head><meta charset='utf-8'><title>MemForge IEEE Report</title>"
                f"<style>{CSS}</style></head><body>{front(pages)}{b}</body></html>")
        html_path.write_text(html)
        render(html_path, pdf_path)
        pages, n = page_of(str(pdf_path), None)
    print("pages:", n, "toc:", pages)


if __name__ == "__main__":
    main()
