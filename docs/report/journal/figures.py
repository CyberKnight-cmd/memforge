"""All report figures as monochrome SVG box diagrams and plain line charts."""
from svg import D, strip, chart

# ----------------------------------------------------------------- helpers
AC4 = [1, 2, 3, 4, 1, 2, 5, 1, 2, 3, 4, 5]
AC3 = [7, 0, 1, 2, 0, 3, 0, 4, 2, 3, 0, 3, 2, 1, 2, 0, 1, 7, 0, 1]


def simulate(refs, frames, policy):
    """Return (columns of frame contents, fault flags). Ties -> lowest frame."""
    fr = [None] * frames
    loaded = [0] * frames
    used = [0] * frames
    cols, faults = [], []
    for t, p in enumerate(refs):
        if p in fr:
            i = fr.index(p)
            used[i] = t
            faults.append(False)
        else:
            faults.append(True)
            if None in fr:
                i = fr.index(None)
            elif policy == "fifo":
                i = min(range(frames), key=lambda k: (loaded[k], k))
            elif policy == "lru":
                i = min(range(frames), key=lambda k: (used[k], k))
            else:
                def nxt(k):
                    for u in range(t + 1, len(refs)):
                        if refs[u] == fr[k]:
                            return u
                    return 10 ** 9
                i = max(range(frames), key=lambda k: (nxt(k), -k))
            fr[i] = p
            loaded[i] = t
            used[i] = t
        cols.append(list(fr))
    return cols, faults


def faults(refs, f, pol):
    return sum(simulate(refs, f, pol)[1])


def grid(refs, frames, policy, cell=13.5, title=None):
    cols, fl = simulate(refs, frames, policy)
    n = len(refs)
    x0, top = 34, 14 if title else 4
    d = D(x0 + n * cell + 4, top + (frames + 2) * cell + 4)
    if title:
        d.text(d.w / 2, 10, title, 8, bold=True)
    d.text(x0 - 4, top + cell * 0.7, "ref", 7.5, anchor="end", italic=True)
    for r in range(frames):
        d.text(x0 - 4, top + cell * (r + 1.7), f"f{r}", 7.5, anchor="end", italic=True)
    d.text(x0 - 4, top + cell * (frames + 1.7), "fault", 7.5, anchor="end", italic=True)
    for c, p in enumerate(refs):
        x = x0 + c * cell
        d.rect(x, top, cell, cell, "#fff", sw=0.6)
        d.text(x + cell / 2, top + cell * 0.72, p, 8, bold=True)
        for r in range(frames):
            v = cols[c][r]
            changed = fl[c] and (c == 0 or cols[c - 1][r] != v)
            d.rect(x, top + cell * (r + 1), cell, cell, "#d9d9d9" if changed else "#fff", sw=0.6)
            if v is not None:
                d.text(x + cell / 2, top + cell * (r + 1.72), v, 8)
        d.rect(x, top + cell * (frames + 1), cell, cell, "#fff", sw=0.6)
        if fl[c]:
            d.text(x + cell / 2, top + cell * (frames + 1.72), "F", 8, bold=True)
    return d.svg()


# ----------------------------------------------------------------- figures
def fig_scope():
    d = D(340, 150)
    cols = [("IN SCOPE", ["First / Best / Worst-Fit", "Split + coalesce", "Fragmentation metrics",
                          "FIFO / LRU / OPT", "Translation, Belady sweep", "CLI + WebAssembly lab"]),
            ("SIMPLIFIED", ["Compaction: explicit op", "Global replacement only", "Plain-text workloads",
                            "One trace level", "Two seeded generators"]),
            ("EXCLUDED", ["Next-Fit, buddy", "Clock, LFU", "TLB, multi-level tables",
                          "Real RAM / disk latency", "Concurrency (1 CPU)"])]
    for i, (h, items) in enumerate(cols):
        x = 4 + i * 112
        d.box(x, 4, 106, 18, h, 8, bold_first=True, fill="#d9d9d9")
        d.rect(x, 22, 106, 124, dash=(i == 2))
        for j, s in enumerate(items):
            d.text(x + 53, 40 + j * 19, s, 7.6)
    return d.svg()


def fig_concepts():
    d = D(340, 210)
    # address translation
    d.box(4, 8, 60, 26, ["CPU", "logical addr."], 7.6)
    d.box(100, 8, 70, 26, ["MMU", "translate + check"], 7.6, bold_first=True)
    d.box(206, 8, 60, 26, ["Physical", "memory"], 7.6)
    d.arrow(64, 21, 100, 21, "va", ly=17)
    d.arrow(170, 21, 206, 21, "pa", ly=17)
    d.box(282, 8, 54, 26, ["trap:", "violation"], 7.2, dash=True)
    d.path("M135,34 L135,46 L309,46 L309,34", arrow=True, dash=True)
    d.text(220, 56, "invalid access", 7, italic=True)
    d.text(170, 74, "(a) Logical vs physical addresses", 7.8, bold=True)
    # fragmentation
    y = 86
    strip(d, 10, y, 320, 20, 100, [(0, 18, "P1", "used"), (18, 10, "", "free"), (28, 22, "P2", "used"),
                                    (50, 12, "", "free"), (62, 24, "P3", "used"), (86, 14, "", "free")])
    d.text(170, y + 32, "free = 10 + 12 + 14 = 36, but a request of 20 fails", 7.4, italic=True)
    d.text(170, y + 44, "(b) External fragmentation", 7.8, bold=True)
    y = 146
    strip(d, 10, y, 140, 20, 100, [(0, 18, "P1", "used"), (18, 10, "", "free"), (28, 22, "P2", "used"),
                                    (50, 12, "", "free"), (62, 24, "P3", "used"), (86, 14, "", "free")])
    d.arrow(156, y + 10, 184, y + 10, "slide", ly=y + 5)
    strip(d, 190, y, 140, 20, 100, [(0, 18, "P1", "used"), (18, 22, "P2", "used"), (40, 24, "P3", "used"),
                                     (64, 36, "36 free", "free")])
    d.text(170, y + 40, "(c) Sliding compaction keeps order, merges holes", 7.8, bold=True)
    return d.svg()


def fig_paging_concept():
    d = D(340, 120)
    d.text(40, 12, "virtual address", 7.5, italic=True)
    d.box(8, 18, 40, 20, "p", 9, bold_first=True)
    d.box(48, 18, 40, 20, "d", 9, bold_first=True)
    d.text(170, 12, "page table", 7.5, italic=True)
    for i in range(5):
        d.box(150, 18 + i * 16, 40, 16, str([5, "-", 2, 7, "-"][i]), 8, fill="#d9d9d9" if i == 2 else "#fff")
        d.text(144, 30 + i * 16, i, 7, anchor="end")
    d.path("M28,38 L28,62 L150,62", arrow=True)
    d.text(90, 58, "index by p", 7, italic=True)
    d.text(282, 12, "physical address", 7.5, italic=True)
    d.box(250, 18, 40, 20, "f", 9, bold_first=True)
    d.box(290, 18, 40, 20, "d", 9, bold_first=True)
    d.path("M190,58 L230,58 L230,28 L250,28", arrow=True)
    d.path("M68,18 L68,6 L310,6 L310,18", arrow=True, dash=True)
    d.text(170, 112, "pa = f x page_size + d   (only the page number changes)", 7.6, italic=True)
    return d.svg()


def fig_belady_small():
    d = D(340, 140)
    xs = [1, 2, 3, 4, 5, 6]
    s = [("FIFO", [faults(AC4, f, "fifo") for f in xs], "o", None),
         ("LRU", [faults(AC4, f, "lru") for f in xs], "s", "4,2"),
         ("OPT", [faults(AC4, f, "opt") for f in xs], "^", "1,2")]
    px, py = chart(d, 40, 8, 290, 100, xs, s, "frames", "page faults", 0, 13, [0, 4, 8, 12], legend=True,
                   lx=52, ly=72)
    d.raw(f'<circle cx="{px(4)}" cy="{py(10)}" r="7" fill="none" stroke="#000" stroke-width="0.8"/>')
    d.text(px(4), py(12.2), "FIFO anomaly: 9 -> 10", 7.2, italic=True)
    return d.svg()


def fig_layers():
    d = D(340, 168)
    rows = [("5  Execution frontends", "memsim CLI   |   Svelte + WebAssembly lab"),
            ("4  Orchestration + telemetry", "Session   |   trace writer   |   metrics"),
            ("3  Policies", "First / Best / Worst-Fit   |   FIFO / LRU / OPT"),
            ("2  Engines + state models", "blocks, page tables, frames, invariants"),
            ("1  Shared kernel", "types, config, errors, events, bus")]
    for i, (a, b) in enumerate(rows):
        y = 4 + i * 32
        d.box(4, y, 120, 26, a, 7.8, bold_first=True, fill="#d9d9d9")
        d.box(124, y, 212, 26, b, 7.6)
        if i < 4:
            d.arrow(64, y + 32, 64, y + 26)
    d.text(330, 166, "arrows: 'uses'", 6.8, anchor="end", italic=True)
    return d.svg()


def fig_dataflow():
    d = D(340, 150)
    d.box(4, 10, 56, 30, ["workload", ".wl file"], 7.6, rx=6)
    d.box(76, 10, 56, 30, ["Loader +", "validator"], 7.6)
    d.box(148, 10, 64, 30, ["Session", "facade"], 7.8, bold_first=True, fill="#d9d9d9")
    d.box(230, 4, 106, 20, "Contiguous engine", 7.6)
    d.box(230, 28, 106, 20, "Paging engine", 7.6)
    for a, b in [(60, 76), (132, 148)]:
        d.arrow(a, 25, b, 25)
    d.text(140, 52, "ops", 7, italic=True)
    d.arrow(212, 20, 230, 14)
    d.arrow(212, 30, 230, 38)
    d.box(148, 70, 64, 22, "Event bus", 7.8, bold_first=True)
    d.path("M283,48 L283,81 L212,81", arrow=True)
    d.text(250, 76, "events (seq)", 7, italic=True)
    for i, (n, out) in enumerate([("Trace writer", ".jsonl"), ("Metrics", ".json/.csv"), ("Invariants", "abort on bug")]):
        x = 30 + i * 102
        d.box(x, 112, 84, 22, n, 7.6)
        d.text(x + 42, 146, out, 7, italic=True)
        d.path(f"M180,92 L180,102 L{x+42},102 L{x+42},112", arrow=True)
    return d.svg()


def fig_alloc_seq():
    d = D(340, 196)
    parts = ["Session", "Engine", "Policy", "Model", "Bus"]
    xs = [36, 104, 172, 240, 306]
    for x, p in zip(xs, parts):
        d.box(x - 30, 4, 60, 18, p, 7.8, bold_first=True, fill="#d9d9d9")
        d.line(x, 22, x, 192, dash=True, sw=0.5)
    msgs = [(0, 1, "alloc(pid,id,s)"), (1, 1, "validate id/size"), (1, 2, "select(view,s)"),
            (2, 1, "decision, scanned"), (1, 3, "split(hole,s)"), (3, 1, "block index"),
            (1, 4, "Placement, Split,"), (1, 4, "Allocated events"), (1, 0, "status")]
    y = 38
    for a, b, m in msgs:
        if a == b:
            d.path(f"M{xs[a]},{y} L{xs[a]+18},{y} L{xs[a]+18},{y+8} L{xs[a]},{y+8}", arrow=True)
            d.text(xs[a] + 22, y + 7, m, 7, anchor="start", italic=True)
            y += 20
            continue
        d.arrow(xs[a], y, xs[b], y, dash=(b < a))
        d.text((xs[a] + xs[b]) / 2, y - 3, m, 7, italic=True)
        y += 18
    d.text(170, 194, "failure path: no hole -> AllocationFailed{cause}, state unchanged", 6.8, italic=True)
    return d.svg()


def fig_access_flow():
    d = D(340, 250)
    d.box(110, 4, 120, 18, "access(pid, vpn | va, w)", 7.6, rx=8)
    d.diamond(170, 44, 110, 30, "valid pid / vpn?", 7.4)
    d.arrow(170, 22, 170, 29)
    d.box(262, 34, 74, 20, "AccessViolation", 7.2, dash=True)
    d.arrow(225, 44, 262, 44, "no", ly=40)
    d.diamond(170, 92, 110, 30, "page present?", 7.4)
    d.arrow(170, 59, 170, 77, "yes")
    d.box(262, 82, 74, 20, "PageHit", 7.4)
    d.arrow(225, 92, 262, 92, "yes", ly=88)
    d.diamond(170, 140, 110, 30, "free frame?", 7.4)
    d.arrow(170, 107, 170, 125, "no: PageFault")
    d.box(10, 130, 76, 20, "lowest free", 7.2)
    d.arrow(115, 140, 86, 140, "yes", ly=136)
    d.box(120, 168, 100, 20, "policy.victim()", 7.4)
    d.arrow(170, 155, 170, 168, "no")
    d.diamond(170, 208, 96, 26, "victim dirty?", 7.2)
    d.arrow(170, 188, 170, 195)
    d.box(250, 198, 86, 20, "PageWrittenBack", 7.2)
    d.arrow(218, 208, 250, 208, "yes", ly=204)
    d.box(10, 200, 76, 30, ["PageEvicted", "PageLoaded"], 7.2)
    d.arrow(122, 208, 86, 212, "no", ly=204)
    d.path("M293,218 L293,240 L48,240 L48,230", arrow=True)
    d.path("M48,150 L48,200", arrow=True)
    d.box(262, 120, 74, 30, ["set R (and D", "if write); pa"], 7)
    d.path("M299,102 L299,120", arrow=True)
    d.path("M10,215 L2,215 L2,118 L262,118", arrow=True, dash=True)
    return d.svg()


def fig_modules():
    d = D(340, 150)
    pos = {"common": (170, 136), "contiguous": (50, 96), "paging": (130, 96), "trace": (210, 96),
           "workloads": (290, 96), "metrics": (290, 60), "session": (130, 56), "bench": (130, 24),
           "cli": (50, 24), "web (wasm)": (230, 24)}
    for n, (x, y) in pos.items():
        d.box(x - 34, y - 9, 68, 18, n, 7.6, bold_first=(n in ("common", "session")),
              fill="#d9d9d9" if n in ("common", "session") else "#fff")
    edges = [("contiguous", "common"), ("paging", "common"), ("trace", "common"), ("workloads", "common"),
             ("metrics", "common"), ("session", "contiguous"), ("session", "paging"), ("session", "trace"),
             ("bench", "session"), ("bench", "workloads"), ("bench", "metrics"), ("cli", "bench"),
             ("web (wasm)", "session"), ("web (wasm)", "workloads")]
    for a, b in edges:
        (x1, y1), (x2, y2) = pos[a], pos[b]
        if y1 == y2:
            continue
        d.line(x1, y1 + 9, x2, y2 - 9, arrow=True, sw=0.6)
    d.line(84, 24, 96, 24, arrow=True, sw=0.6)
    d.text(4, 146, "A -> B : A links (depends on) B", 6.8, anchor="start", italic=True)
    return d.svg()


def fig_vtable():
    d = D(340, 120)
    d.box(4, 8, 96, 64, ["Engine", "", "policy->select(", "  ctx, view, s)"], 7.4, bold_first=True, mono=False)
    d.box(130, 8, 96, 18, "ms_cont_placement_policy", 7, bold_first=True, fill="#d9d9d9")
    for i, f in enumerate(["name", "select()", "ctx (void *)"]):
        d.box(130, 26 + i * 16, 96, 16, f, 7.4, mono=True)
    d.arrow(100, 40, 130, 40)
    d.text(283, 12, "registry (by name)", 7.4, italic=True)
    for i, n in enumerate(["first-fit", "best-fit", "worst-fit"]):
        d.box(250, 18 + i * 18, 70, 18, n, 7.4, mono=True)
    d.arrow(250, 27, 226, 27)
    d.text(170, 96, "policy reads a const MemoryView, returns a decision;", 7.2, italic=True)
    d.text(170, 108, "only the engine mutates state. FIFO/LRU/OPT use the same pattern.", 7.2, italic=True)
    return d.svg()


def fig_block_model():
    d = D(340, 150)
    strip(d, 8, 10, 324, 22, 1740, [(0, 100, "free", "free"), (100, 10, "", "sys"), (110, 212, "P1:a1", "used"),
                                     (322, 288, "free", "free"), (610, 10, "", "sys"), (620, 200, "free", "free"),
                                     (820, 10, "", "sys"), (830, 300, "free", "free"), (1130, 10, "", "sys"),
                                     (1140, 600, "free", "free")])
    d.text(8, 42, "0", 7, anchor="start")
    d.text(332, 42, "M = 1740", 7, anchor="end")
    d.text(170, 52, "address-ordered array of blocks tiling [0, M) exactly", 7.4, italic=True)
    flds = ["start", "size", "state", "pid", "alloc_id", "requested"]
    for i, f in enumerate(flds):
        d.box(8 + i * 54, 62, 54, 16, f, 7.2, mono=True)
    d.text(170, 90, "ms_block", 7.4, bold=True)
    # lifecycle
    y = 122
    st = [("UNSEEN", 40), ("LIVE", 140), ("FREED", 240)]
    for n, x in st:
        d.ellipse(x, y, 32, 11, n, 7.4)
    d.ellipse(140, y + 1, 1, 1, "")
    d.ellipse(306, y, 28, 11, "FAILED", 7.4)
    d.arrow(72, y, 108, y, "alloc ok", ly=y - 5)
    d.arrow(172, y, 208, y, "free", ly=y - 5)
    d.path(f"M40,{y+11} Q40,{y+26} 170,{y+26} Q306,{y+26} 306,{y+11}", arrow=True)
    d.text(170, y + 23, "alloc fails (no hole)", 7, italic=True)
    return d.svg()


def fig_split_coalesce():
    d = D(340, 128)
    d.text(4, 10, "split  alloc(s = 212) into hole [110, 610)", 7.6, anchor="start", bold=True)
    strip(d, 8, 16, 150, 18, 500, [(0, 500, "hole 500", "free")])
    d.arrow(164, 25, 182, 25)
    strip(d, 188, 16, 150, 18, 500, [(0, 212, "a1 212", "new"), (212, 288, "288", "free")])
    d.text(263, 46, "allocate at low end, remainder stays free", 7, italic=True)
    d.text(4, 70, "coalesce  free(B): merge right, then left", 7.6, anchor="start", bold=True)
    strip(d, 8, 76, 150, 18, 300, [(0, 80, "free", "free"), (80, 120, "B", "used"), (200, 100, "free", "free")])
    d.arrow(164, 85, 182, 85)
    strip(d, 188, 76, 150, 18, 300, [(0, 300, "one hole 300", "free")])
    d.text(170, 116, "O(1) without shifting, O(N) worst case (array shift)", 7, italic=True)
    return d.svg()


AC1_HOLES = [(0, 100), (110, 500), (620, 200), (830, 300), (1140, 600)]
AC1_SYS = [(100, 10), (610, 10), (820, 10), (1130, 10)]


def layout(allocs):
    segs = [(s, z, "", "sys") for s, z in AC1_SYS]
    used = []
    for s, z, lab in allocs:
        segs.append((s, z, lab, "used"))
        used.append((s, z))
    occ = sorted([(s, z) for s, z in AC1_SYS] + used)
    pos = 0
    for s, z in occ:
        if s > pos:
            segs.append((pos, s - pos, str(s - pos), "free"))
        pos = s + z
    if pos < 1740:
        segs.append((pos, 1740 - pos, str(1740 - pos), "free"))
    return segs


def fig_ac1_policies():
    d = D(340, 168)
    d.text(4, 10, "initial: holes 100, 500, 200, 300, 600 (hatched = SYS)", 7.4, anchor="start", italic=True)
    strip(d, 52, 16, 284, 18, 1740, layout([]))
    rows = [("First-Fit", [(110, 212, "a1"), (1140, 417, "a2"), (322, 112, "a3")], "a4: fail (frag.)"),
            ("Best-Fit", [(830, 212, "a1"), (110, 417, "a2"), (620, 112, "a3"), (1140, 426, "a4")], "all placed"),
            ("Worst-Fit", [(1140, 212, "a1"), (110, 417, "a2"), (1352, 112, "a3")], "a4: fail (frag.)")]
    for i, (n, al, note) in enumerate(rows):
        y = 50 + i * 36
        d.text(48, y + 12, n, 7.8, anchor="end", bold=True)
        strip(d, 52, y, 284, 18, 1740, layout(al), size=6.5)
        d.text(336, y + 28, note, 7, anchor="end", italic=True)
    d.text(4, 162, "requests a1 = 212, a2 = 417, a3 = 112, a4 = 426 (grey = placed)", 7.2, anchor="start", italic=True)
    return d.svg()


def fig_frag_metrics():
    d = D(340, 92)
    strip(d, 8, 8, 324, 20, 1740, layout([(110, 212, "a1"), (1140, 417, "a2"), (322, 112, "a3")]), size=6.5)
    items = [("total_free", "959"), ("largest_hole", "300"), ("hole_count", "5"), ("ext_frag", "1-300/959=0.687")]
    for i, (k, v) in enumerate(items):
        d.box(8 + i * 81, 40, 81, 26, [k, v], 7.2, bold_first=True)
    d.text(170, 84, "request 426 <= 959 free but > 300 largest  =>  cause FRAGMENTATION", 7.2, italic=True)
    return d.svg()


def fig_compaction():
    d = D(340, 150)
    for i, (n, sub) in enumerate([("Planner (pure)", "list of moves"), ("Model", "validate plan"),
                                  ("Model", "apply moves"), ("Engine", "BlockRelocated x n")]):
        x = 4 + i * 84
        d.box(x, 4, 76, 30, [n, sub], 7.4, bold_first=True)
        if i:
            d.arrow(x - 8, 19, x, 19)
    d.text(170, 46, "move = (alloc_id, old_start, new_start), ascending order", 7, italic=True)
    d.text(4, 64, "before (First-Fit after a4 failed)", 7.4, anchor="start", bold=True)
    strip(d, 8, 70, 324, 18, 1740, layout([(110, 212, "a1"), (1140, 417, "a2"), (322, 112, "a3")]), size=6.5)
    d.text(4, 104, "after compact + retry a4: 7 blocks, 781 units moved", 7.4, anchor="start", bold=True)
    strip(d, 8, 110, 324, 18, 1740, [(0, 781, "7 live blocks (781)", "used"), (781, 426, "a4 @781", "new"),
                                      (1207, 533, "533 free", "free")], size=6.8)
    d.text(170, 144, "live set and total free unchanged; order kept; <= 1 hole", 7, italic=True)
    return d.svg()


E1 = {  # util: policy: (success%, frag failures, holes scanned, extfrag)
    60: {"FF": (99.9, 1.2, 2.29, .359), "BF": (100.0, 0.3, 3.77, .322), "WF": (99.0, 14.3, 3.88, .546)},
    75: {"FF": (97.0, 44.6, 2.78, .460), "BF": (97.9, 32.0, 4.66, .419), "WF": (93.9, 92.4, 4.61, .597)},
    90: {"FF": (90.0, 148.6, 3.21, .516), "BF": (91.4, 127.7, 5.24, .473), "WF": (87.2, 192.0, 5.19, .629)}}
STY = {"FF": ("First-Fit", "o", None), "BF": ("Best-Fit", "s", "4,2"), "WF": ("Worst-Fit", "^", "1,2")}


def fig_e1(idx, ylabel, lo, hi, ticks, fmt=lambda v: str(v), leg=(70, 18)):
    d = D(340, 140)
    xs = [60, 75, 90]
    s = [(STY[p][0], [E1[u][p][idx] for u in xs], STY[p][1], STY[p][2]) for p in ("FF", "BF", "WF")]
    chart(d, 44, 8, 286, 100, xs, s, "target utilisation (%)", ylabel, lo, hi, ticks, legend=True,
          fmt=fmt, lx=leg[0], ly=leg[1])
    return d.svg()


def fig_pte():
    d = D(340, 112)
    d.text(84, 10, "page-table entry (per process)", 7.4, bold=True)
    for i, f in enumerate(["frame", "P", "R", "D", "seen"]):
        w = 50 if i == 0 else 26
        x = 8 + (0 if i == 0 else 50 + (i - 1) * 26)
        d.box(x, 16, w, 18, f, 7.6, mono=True)
    d.text(84, 48, "P present, R referenced, D dirty", 7, italic=True)
    d.text(260, 10, "inverted frame table", 7.4, bold=True)
    for i, (a, b) in enumerate([("0", "P0:2"), ("1", "P1:0"), ("2", "free"), ("3", "P0:5")]):
        d.box(214, 16 + i * 16, 22, 16, a, 7.2)
        d.box(236, 16 + i * 16, 90, 16, b, 7.2, mono=True, fill="#fff" if b != "free" else "#d9d9d9")
    d.text(270, 92, "(pid, vpn) owner per frame;", 7, italic=True)
    d.text(270, 103, "free frames taken lowest first", 7, italic=True)
    d.path("M58,34 L58,70 L190,70 L190,48 L214,48", arrow=True, dash=True)
    d.text(120, 66, "frame = index", 7, italic=True)
    return d.svg()


def fig_translate_example():
    d = D(340, 96)
    d.text(170, 10, "page size S = 4096  =>  k = log2 S = 12 offset bits", 7.4, italic=True)
    d.box(8, 18, 70, 20, "va = 6700", 7.8, bold_first=True)
    d.box(110, 18, 50, 20, "vpn = 1", 7.6)
    d.box(160, 18, 70, 20, "off = 2604", 7.6)
    d.arrow(78, 28, 110, 28)
    d.text(135, 52, "va >> k", 7, italic=True)
    d.text(195, 52, "va & (S-1)", 7, italic=True)
    d.box(110, 62, 50, 20, "f = 5", 7.6, fill="#d9d9d9")
    d.box(160, 62, 70, 20, "off = 2604", 7.6)
    d.arrow(135, 38, 135, 62, "PT[1]")
    d.box(262, 62, 74, 20, "pa = 23084", 7.8, bold_first=True)
    d.arrow(230, 72, 262, 72)
    d.text(299, 94, "(f << k) | off", 7, italic=True)
    return d.svg()


def fig_page_life():
    d = D(340, 92)
    d.ellipse(50, 46, 44, 16, ["non-", "resident"], 7.4)
    d.ellipse(170, 22, 46, 13, "resident clean", 7.4)
    d.ellipse(170, 72, 46, 13, "resident dirty", 7.4)
    d.arrow(84, 36, 125, 24, "fault: load", lx=96, ly=22)
    d.arrow(170, 35, 170, 59, "write")
    d.arrow(124, 70, 92, 54, "evict + write-back", lx=86, ly=78)
    d.path("M216,22 Q290,22 290,46 Q290,60 94,48", arrow=True, dash=True)
    d.text(296, 40, "evict", 7, anchor="start", italic=True)
    d.text(296, 50, "(clean)", 7, anchor="start", italic=True)
    return d.svg()


def fig_hooks():
    d = D(340, 74)
    d.box(4, 6, 50, 20, "hit", 7.6, bold_first=True, fill="#d9d9d9")
    d.box(80, 6, 70, 20, "on_access", 7.4, mono=True)
    d.arrow(54, 16, 80, 16)
    d.box(4, 44, 50, 20, "fault", 7.6, bold_first=True, fill="#d9d9d9")
    for i, n in enumerate(["victim", "on_evict", "on_load", "on_access"]):
        x = 80 + i * 66
        d.box(x, 44, 58, 20, n, 7.2, mono=True, dash=(i < 2))
        d.arrow(x - (26 if i == 0 else 8), 54, x, 54)
    d.text(240, 36, "dashed: only when all frames are full", 6.8, italic=True)
    return d.svg()


def fig_e4():
    d = D(340, 150)
    xs = [2, 4, 8, 12, 16]
    s = [("FIFO", [1398.2, 840.0, 253.1, 151.7, 106.7], "o", None),
         ("LRU", [1393.0, 819.6, 138.9, 100.7, 83.5], "s", "4,2"),
         ("OPT", [1070.8, 466.0, 103.2, 70.7, 53.2], "^", "1,2")]
    chart(d, 46, 8, 284, 110, xs, s, "frames", "page faults (of 2,000 refs)", 0, 1500,
          [0, 300, 600, 900, 1200, 1500], legend=True, lx=250, ly=18)
    return d.svg()


def fig_workload():
    d = D(340, 140)
    txt = ["schema 1", "kind contiguous", "memory_size 1740", "alloc P1 a1 212", "free P1 a1", "compact"]
    d.rect(4, 4, 104, 96)
    for i, t in enumerate(txt):
        d.text(10, 18 + i * 13, t, 7.2, anchor="start", mono=True)
    d.text(56, 112, ".wl (header + ops)", 7, italic=True)
    steps = [("parse", "line by line"), ("validate", "header, args"), ("intern", "\"P1\" -> 0")]
    for i, (a, b) in enumerate(steps):
        y = 6 + i * 34
        d.box(130, y, 80, 26, [a, b], 7.2, bold_first=True)
        if i:
            d.arrow(170, y - 8, 170, y)
    d.arrow(108, 40, 130, 40)
    d.box(236, 30, 100, 26, ["ms_op[ ]", "dense integer ids"], 7.2, bold_first=True, fill="#d9d9d9")
    d.arrow(210, 87, 286, 87)
    d.path("M286,87 L286,56", arrow=True)
    d.box(236, 98, 100, 22, "error: file:line, exit 2", 7, dash=True)
    d.path("M170,32 L170,32", arrow=False)
    d.text(170, 134, "paging ops: spawn pid pages | access pid vpn | access pid @va [w]", 6.8, italic=True)
    return d.svg()


def fig_metrics():
    d = D(340, 96)
    d.box(4, 30, 64, 30, ["event", "stream"], 7.6, rx=6)
    d.box(96, 26, 70, 38, ["Metrics", "collector", "(only reader)"], 7.4, bold_first=True, fill="#d9d9d9")
    d.arrow(68, 45, 96, 45)
    d.box(196, 4, 140, 42, ["contiguous: requests, successes,", "failures (frag / cap), scanned,",
                            "ext_frag, holes, moved units"], 6.9)
    d.box(196, 50, 140, 42, ["paging: refs, hits, faults", "(compulsory / capacity), hit ratio,",
                             "evictions, write-backs, violations"], 6.9)
    d.arrow(166, 38, 196, 25)
    d.arrow(166, 52, 196, 71)
    return d.svg()


def fig_bench_pipe():
    d = D(340, 112)
    st = [("generator", "seed 1..10"), ("memsim run", "x 3 policies"), ("CSV", "+ git SHA"),
          ("stats.py", "mean, 95% CI"), ("tables +", "figures")]
    for i, (a, b) in enumerate(st):
        x = 4 + i * 67
        d.box(x, 8, 60, 30, [a, b], 7.2, bold_first=True)
        if i:
            d.arrow(x - 7, 23, x, 23)
    d.text(170, 54, "E1: 3 utilisations x 10 seeds = 30 workloads, 3,000 ops, 1,024 units, sizes 16-128", 6.9, italic=True)
    d.text(170, 66, "E4: 10 seeds x 2,000 refs, 32 pages, working set 6, phase every 200, 5% outside", 6.9, italic=True)
    d.text(170, 82, "paired design: every policy replays the same file per seed", 7.2, bold=True)
    d.text(170, 98, "scripts/experiments/run_all.sh regenerates everything", 6.9, italic=True)
    return d.svg()


def fig_test_layers():
    d = D(340, 150)
    rows = [("Web equivalence", "wasm trace == CLI trace"), ("CLI smoke", "scripts/smoke_cli.sh"),
            ("Integration AC-6..8", "determinism, invariants, compare"),
            ("Golden fixtures AC-1..5", "hand-derived from textbooks"),
            ("Oracles + properties", "5,000 compactions, OPT bound"), ("Unit tests (14 suites)", "every module")]
    for i, (a, b) in enumerate(rows):
        w = 150 + i * 30
        x = 170 - w / 2
        y = 4 + i * 20
        d.box(x, y, w, 20, f"{a}  |  {b}", 7, fill="#d9d9d9" if i == 5 else "#fff")
    d.rect(4, 128, 332, 18, dash=True)
    d.text(170, 140, "all of it under ASan + UBSan, Valgrind memcheck, clang-tidy, -Werror", 7.2, italic=True)
    return d.svg()


def fig_checked():
    d = D(340, 70)
    for i, (a, b) in enumerate([("apply op", "engine"), ("check", "I1-I7 / V1-V5"), ("flush events", "to sinks")]):
        x = 4 + i * 96
        d.box(x, 8, 80, 28, [a, b], 7.4, bold_first=True)
        if i:
            d.arrow(x - 16, 22, x, 22)
    d.box(296, 8, 40, 28, ["next", "op"], 7.2, rx=6)
    d.arrow(276, 22, 296, 22)
    d.box(100, 46, 80, 20, "abort, exit 3", 7.2, dash=True)
    d.arrow(140, 36, 140, 46, "violated")
    return d.svg()


def fig_cli():
    d = D(340, 110)
    d.box(130, 44, 80, 22, "memsim", 9, bold_first=True, fill="#d9d9d9")
    cmds = [("validate", "check .wl"), ("run", "trace + metrics"), ("compare", "policies side by side"),
            ("refs", "string, --table"), ("sweep", "faults vs frames"), ("gen", "seeded workload")]
    for i, (c, w) in enumerate(cmds):
        x = 4 if i < 3 else 236
        y = 4 + (i % 3) * 36
        d.box(x, y, 100, 30, [c, w], 7.2, bold_first=True)
        d.line(130 if i < 3 else 210, 55, 104 if i < 3 else 236, y + 15, sw=0.6)
    d.text(170, 98, "exit: 0 ok | 2 bad input | 3 contract violation", 7, italic=True)
    return d.svg()


def fig_web():
    d = D(340, 150)
    d.rect(4, 4, 332, 142)
    d.box(4, 4, 332, 16, "Overview | Contiguous | Paging | Replay", 7.2, fill="#d9d9d9")
    d.box(10, 26, 220, 20, "controls: policy (FF | BF | WF | all 3), size, pid, Allocate / Free / Compact", 6.6)
    d.box(236, 26, 94, 64, ["measurements", "free, largest hole,", "ext_frag, scanned"], 6.8, bold_first=True)
    for i in range(3):
        strip(d, 10, 52 + i * 14, 220, 10, 100, [(0, 20, "", "used"), (20, 15, "", "free"),
                                                  (35 + i * 5, 25, "", "used"), (60 + i * 5, 40 - i * 5, "", "free")])
    d.box(10, 96, 160, 44, ["step history", "\"failed: free 959,", "largest hole 300\""], 6.8, bold_first=True)
    d.box(176, 96, 154, 44, ["fragmentation chart", "ext_frag per step"], 6.8, bold_first=True)
    return d.svg()


def fig_workflow():
    d = D(340, 52)
    st = ["choose scenario / seed", "select policy (or all)", "step + read narration", "compact, compare, export"]
    for i, s in enumerate(st):
        x = 4 + i * 84
        d.box(x, 10, 76, 30, [f"{i+1}", s], 6.8, bold_first=True)
        if i:
            d.arrow(x - 8, 25, x, 25)
    return d.svg()


def fig_repo():
    d = D(340, 130)
    d.box(130, 4, 80, 18, "memforge/", 7.8, bold_first=True, fill="#d9d9d9")
    items = [("src/", "9 modules (C17)"), ("tests/", "unit, golden, integ."), ("web/", "Svelte + wasm"),
             ("scripts/", "build, layers, exp."), ("analysis/", "stats + plots"), ("docs/", "arch, report")]
    for i, (a, b) in enumerate(items):
        x = 4 + (i % 3) * 112
        y = 44 + (i // 3) * 42
        d.box(x, y, 106, 30, [a, b], 7.2, bold_first=True)
        d.line(170, 22, x + 53, y, sw=0.6)
    d.text(170, 126, "~6,900 lines C in src/, ~4,000 lines C tests, ~3,100 lines web", 7, italic=True)
    return d.svg()


def fig_layer_chain():
    d = D(340, 40)
    ch = ["common", "engines, trace, workloads, metrics", "session", "bench", "cli"]
    ws = [44, 140, 46, 38, 30]
    x = 4
    for i, (c, w) in enumerate(zip(ch, ws)):
        d.box(x, 10, w, 20, c, 7, bold_first=(i == 0))
        if i:
            d.arrow(x, 20, x - 10, 20)
        x += w + 10
    return d.svg()


def fig_ci():
    d = D(340, 130)
    jobs = ["Lint (pre-commit)", "Build+test gcc/clang x dev/release", "Valgrind", "clang-tidy", "Web app (wasm, tests)"]
    for i, j in enumerate(jobs):
        d.box(4, 4 + i * 24, 170, 20, j, 7.2)
        d.line(174, 14 + i * 24, 214, 60, arrow=True, sw=0.6)
    d.box(214, 46, 70, 28, ["CI passed", "(gate)"], 7.6, bold_first=True, fill="#d9d9d9")
    d.box(296, 46, 40, 28, "merge", 7.4, rx=6)
    d.arrow(284, 60, 296, 60)
    d.box(214, 100, 122, 22, "Pages: deploy web on main", 7, dash=True)
    return d.svg()


def fig_build():
    d = D(340, 120)
    d.box(4, 44, 64, 30, ["src/ (C17)", "modules"], 7.2, bold_first=True)
    d.box(96, 4, 100, 30, ["CMake presets", "dev | release | valgrind"], 7, bold_first=True)
    d.box(96, 84, 100, 30, ["build_wasm.sh", "clang --target=wasm32-wasi"], 7, bold_first=True)
    d.arrow(68, 52, 96, 22)
    d.arrow(68, 66, 96, 96)
    d.box(224, 4, 112, 30, ["memsim + 17 test", "suites (ctest)"], 7.2)
    d.box(224, 84, 112, 30, ["web/public/", "memsim.wasm -> Svelte"], 7.2)
    d.arrow(196, 19, 224, 19)
    d.arrow(196, 99, 224, 99)
    d.path("M280,34 L280,84", dash=True)
    d.text(284, 62, "same traces", 7, anchor="start", italic=True)
    return d.svg()


def fig_setup():
    d = D(340, 64)
    st = [("pre-commit", "install"), ("cmake", "--preset dev"), ("cmake --build", "--preset dev"),
          ("ctest", "--preset dev"), ("memsim", "run / compare")]
    for i, (a, b) in enumerate(st):
        x = 4 + i * 67
        d.box(x, 8, 60, 30, [a, b], 6.9, mono=True)
        if i:
            d.arrow(x - 7, 23, x, 23)
    d.text(170, 56, "web: build_wasm.sh -> npm ci -> npm run dev | npm test", 7, italic=True, mono=False)
    return d.svg()


def fig_events():
    d = D(340, 120)
    groups = [("Contiguous", ["PlacementDecided", "BlockSplit", "BlockAllocated", "AllocationFailed",
                              "AllocRejected", "BlockFreed", "BlocksCoalesced", "FreeRejected", "FreeSkipped"]),
              ("Process + compaction", ["ProcessSpawned", "ProcessTerminated", "CompactionStarted",
                                        "BlockRelocated", "CompactionCompleted"]),
              ("Paging", ["PageHit", "PageFault", "FrameAllocated", "VictimSelected", "PageEvicted",
                          "PageWrittenBack", "PageLoaded", "AddressTranslated", "AccessViolation"])]
    for i, (g, ev) in enumerate(groups):
        x = 4 + i * 112
        d.box(x, 4, 106, 16, g, 7.4, bold_first=True, fill="#d9d9d9")
        d.rect(x, 20, 106, 96)
        for j, e in enumerate(ev):
            d.text(x + 53, 30 + j * 10, e, 6.8, mono=True)
    return d.svg()
