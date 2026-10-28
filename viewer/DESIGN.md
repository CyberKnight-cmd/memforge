# MemForge Trace Viewer — Visual System Design

This document details the visual and architectural specification for the MemForge trace viewer (`viewer/`). The viewer is an offline, instrument-style diagnostic dashboard built to replay deterministic execution traces from the `memsim` C memory management simulator.

---

## 1. Positioning and Principles

- **Mechanism-first**: Memory state is rendered not as an arbitrary graphic, but as an exact instrument reflecting every byte, address, and event from the simulator trace.
- **Calm at a distance**: High informational density without visual noise. No glows, gradients (except the free memory hatch), pill badges, card-in-card containers, emoji, or Unicode symbol icons.
- **Zero setup**: Pure static HTML/CSS/JavaScript with no external dependencies, CDNs, or frameworks. Runs locally directly from `file://` and bundles into a single-file `standalone.html`.

---

## 2. Design Tokens and Theming

The visual layer uses CSS custom properties defined on `:root`, with dark mode as the default and `[data-theme="light"]` for light mode overrides. Themes toggle via the top bar button or the `T` keyboard shortcut, persisted in `localStorage` inside safe `try/catch` blocks. `@media print` automatically enforces light mode values and hides interactive controls.

### Token Palettes

| Token | Dark (Default) | Light (`[data-theme="light"]`) | Usage |
| :--- | :--- | :--- | :--- |
| `--ground` | `#121316` | `#f6f5f2` | Application viewport background, free block base |
| `--panel` | `#181a1e` | `#ffffff` | Panel surfaces, top bar, transport bar |
| `--raised` | `#1f2227` | `#efeeea` | Buttons, step chart background, popover surfaces |
| `--rule` | `#2b2f36` | `#d8d7d2` | Borders, dividers, scrubber track, chart area fill |
| `--text` | `#e7e9ec` | `#17181b` | Primary text and headings |
| `--muted` | `#9ba2ac` | `#555b63` | Subtitles, table labels, metric labels, chart line |
| `--faint` | `#6b727c` | `#8a9099` | Event sequence numbers, secondary annotations |
| `--accent` | `#f0a640` | `#b26a00` | Playhead, play button, active buttons, focus outlines |
| `--accent-ink` | `#121316` | `#ffffff` | Text and icons placed on top of `--accent` |
| `--ok` | `#3fb68b` | `#0b8a5f` | Hits, memory frees, positive deltas |
| `--bad` | `#ff7849` | `#c94a12` | Allocation failures, page faults, negative deltas |
| `--free-hatch` | `#3a3f47` | `#c9c8c2` | Diagonal hatch pattern lines for free memory |
| `--sys` | `#5c626b` | `#9aa0a8` | Reserved kernel/system memory blocks |

### Process Colour Assignment
Process IDs are assigned distinct colors dynamically in **first-seen order** across the trace (with `SYS` always fixed to `--sys`):
- **Dark palette**: `["#56b4e9", "#3fb68b", "#e3c84a", "#4f8fe0", "#ff7849", "#d38ab6"]`
- **Light palette**: `["#2f86c4", "#0b8a5f", "#a68a00", "#1f5fb4", "#c94a12", "#a6527f"]`

### Surface Styling Rules
- **Free memory**: Repeating linear diagonal hatch `repeating-linear-gradient(135deg, var(--free-hatch) 0 1px, transparent 1px 7px)` over `--ground`.
- **Allocated block**: Process colour at 85% opacity (`color-mix(in srgb, var(--block-color) 85%, transparent)`) with a 1px darker inner edge (`box-shadow: inset 0 0 0 1px rgba(0, 0, 0, 0.35)`). Label text uses `--ground` in dark mode and `#ffffff` in light mode, rendered only when the block's physical rendered width exceeds 56px.
- **Shadows**: Exactly one shadow in the entire interface: popovers use `0 8px 24px rgb(0 0 0 / .35)`.
- **Browser controls**: Custom thin scrollbars using `--rule` thumb, `::selection` highlighted in `--accent`, focus rings using `2px solid var(--accent)` with a 2px offset.

---

## 3. Typography & Spacing Scale

- **Fonts**:
  - `--sans`: `system-ui, -apple-system, "Segoe UI", Roboto, sans-serif`
  - `--mono`: `ui-monospace, "SF Mono", "Cascadia Mono", "JetBrains Mono", Menlo, Consolas, monospace`
- **Scale**: `12px`, `13px`, `15px` (body), `18px` (narration), `24px` (display titles).
- **Tabular figures**: All addresses, IDs, sizes, step indices, and metrics render in `--mono` with `font-variant-numeric: tabular-nums` and right alignment in tables.
- **Headings**: 13px, weight 600, color `--muted`, normal sentence case (no uppercase tracking or eyebrow badges).
- **Spacing scale**: 4px, 8px, 12px, 16px, 24px, 32px.
- **Radius**: 6px on panels, buttons, and popovers; 3px on memory blocks.

---

## 4. Iconography

All icons are inline SVGs drawn with a uniform 1.5px stroke, 16px bounding box, `currentColor` stroke, and round line-caps/line-joins:
1. `play`: Equilateral right-facing triangle (`M5 3.5 L12 8 L5 12.5 Z`).
2. `pause`: Dual vertical bars (`M5.5 3.5 V12.5`, `M10.5 3.5 V12.5`).
3. `step back`: Left-pointing chevron (`M10 3.5 L5 8 L10 12.5`).
4. `step forward`: Right-pointing chevron (`M6 3.5 L11 8 L6 12.5`).
5. `skip to start`: Left stop bar and chevron (`M3.5 3.5 V12.5`, `M12.5 3.5 L7.5 8 L12.5 12.5`).
6. `skip to end`: Right stop bar and chevron (`M12.5 3.5 V12.5`, `M3.5 3.5 L8.5 8 L3.5 12.5`).
7. `sun`: Central circle with 8 radial rays.
8. `moon`: Smooth crescent path.
9. `open file`: Folder silhouette outline.
10. `keyboard`: Rounded key matrix frame with spacebar and key dots.

---

## 5. Layout Architecture

The viewer fills 100% of the viewport with **zero window scrolling** at standard 1280x720 demo resolution:

1. **Top Bar (48px)**:
   - Left: "MemForge" logo (15px 600 weight), 1px vertical rule divider, trace filename in `--mono`, followed by metadata tags (policy and memory/paging configuration).
   - Right: "Open trace" button (triggers native file dialog and window drag-and-drop), sample trace `<select>`, theme toggle, and keyboard shortcuts help popover trigger.
2. **Main Area (CSS Grid, 2fr / 1fr, 16px gap, 16px padding)**:
   - **Left Column**: "Memory" panel (or "Frames" panel in paging mode).
     - Contiguous: 88px horizontal memory strip with absolute percentage block positioning, address ruler with 10% interval ticks and dynamic block hover ticks, interactive block popovers, and animated failure brackets.
     - Paging: Textbook frame replacement matrix table with 36px column cells, reference page headers, slide-in animations, fault/hit status row, and eviction strikethroughs.
     - Narration Sentence: 18px text under the visualizer explaining mechanism, scanned holes, failure causes, or eviction reasons in plain English with mono process-colored IDs and figures.
   - **Right Column (Two stacked panels)**:
     - "Fragmentation" / "Faults": 2-column key-value metrics table with animated counting over 380ms and delta badges that fade after 1.2s. Below sits a 120px inline SVG step chart with a stepped line, baseline failure/compaction ticks, and interactive seek playhead.
     - "Events": Compact, internally scrollable event stream with sequence numbers, event types, and data key-values.
3. **Transport Bar (Fixed bottom, 56px)**:
   - Skip to start, step back, 40px accent play/pause button, step forward, skip to end.
   - Tabular mono step counter (`17 / 24`).
   - Timeline scrubber with a 4px `--rule` track, `--accent` progress fill, 14px thumb, and tick marks for failure and compaction steps.
   - Speed select: `0.5x` (2.4s), `1x` (1.2s), `2x` (0.6s), `4x` (0.3s).
4. **Empty State**:
   - Clean centered card with "Open a MemForge trace" display title, description, file button, and direct one-click sample buttons with descriptions. Parser syntax or header errors display inline in `--bad` with exact line numbers without browser alerts.

---

## 6. Signature Animations

All animations strictly use the Web Animations API (`element.animate`) and CSS transitions:

- **Allocation**:
  - A ghost bar matching the requested size appears above address 0 in the requester's process color (40% fill).
  - The ghost steps across the holes scanned by the allocator policy (pausing 120ms per hole).
  - On success: The ghost drops down into the memory strip, the hole splits, and the remainder block fades in with the hatched pattern (380ms).
  - On failure: The ghost shakes horizontally (3px, 2 cycles) in `--bad`, all free holes outline in `--bad`, and a bracket spans the free space below the strip labeled `free <total> · largest <size>` until the next step.
- **Free**:
  - The freed block smoothly dissolves its process color to hatch over 380ms.
  - On coalescing, the boundary rules dissolve, and the merged hole's size counts up.
- **Compaction (FLIP)**:
  - Every relocated block slides left to its new destination over 380ms with a 40ms stagger in address order.
  - A mono badge reading `"-src->dst"` rides atop each sliding block during translation.
  - Once blocks settle, the single consolidated free hole at the end flashes a brief `--accent` outline.
- **Paging Transition**:
  - Cells in the textbook frame replacement table slide down into view over 160ms upon step advancement.
  - Newly allocated frame cells outline in `--bad` with an `"F"` status label beneath. Page hits highlight in `--ok`. Evicted page numbers remain visible struck through (`<s>vpn</s>`).
- **Scrubbing & Motion Preferences**:
  - Dragging the timeline scrubber or jumping multiple steps executes instant rendering with zero animation.
  - `@media (prefers-reduced-motion: reduce)` disables all spatial motion, replacing movements with 120ms opacity fades.

---

## 7. Demo Scenarios, Chapters, and Multi-View Architectures

MemForge features interactive demo scenarios tailored for live classroom presentations and academic walkthroughs, providing specialized comparative and analytical layouts while maintaining existing typography, color tokens, and animation timing rules:

### 7.1 Scenario Selection & Empty State
- **Top Bar Selector**: A `<select id="scenario-select">` menu provides instant access to the four core scenarios, as well as an "Open your own trace" option that activates the native file picker.
- **Empty State Cards**: The initial empty state prominently displays four primary scenario buttons with one-line mechanism summaries:
  1. *"Placement: First, Best and Worst Fit"* (Stacked comparison over AC-1 workloads)
  2. *"Fragmentation and compaction"* (Single-trace sliding compaction over AC-2)
  3. *"Page replacement: FIFO, LRU and OPT"* (Textbook matrix with policy tabs over AC-3)
  4. *"Belady's anomaly"* (Fault sweep chart and dual FIFO replays over AC-4)

### 7.2 Chapters & Storyline Navigation
Chapters are computed deterministically from trace events to bookmark critical simulator milestones:
- **Contiguous Extraction**:
  - `"Holes ready"`: Initial snapshot state before the first non-SYS allocation request.
  - Per-request markers: `<id> requests <size>` for each user allocation.
  - `"Request fails"`: The first `AllocationFailed` event due to external fragmentation or capacity exhaustion.
  - `"Compaction"`: Triggering of `CompactionStarted`.
  - `"Retry succeeds"`: The first `BlockAllocated` successfully satisfied post-compaction.
- **Paging Extraction**:
  - `"First fault"`: Initial `PageFault` compulsory miss.
  - `"First eviction"`: First `PageEvicted` or `VictimSelected` under frame exhaustion.
  - `"End"`: Final reference and simulation summary.
- **Display & Limits**:
  - Max 9 chapters; per-request entries are prioritized for truncation if limits are exceeded.
  - Rendered as small 12px mono labels above the scrubber track (`--muted`, active in `--text`) and dashed vertical reference rules with titles in the step chart.
- **Keyboard Controls**:
  - Keys `1` through `9`: Instantly seeks to Chapter N.
  - Key `N` (lowercase): Advances to next chapter.
  - Key `Shift+N`: Steps back to previous chapter.

### 7.3 Compare View (Stacked Allocation Policies)
- **Visual Structure**: Three full-width contiguous memory strips (First-Fit, Best-Fit, Worst-Fit) stacked vertically, each fixed at 64px height.
- **Outcomes & Alignment**: Each strip header denotes the policy name on the left and current outcome status on the right (e.g. `"placed at 1140"` or `"fails: fragmentation"` in `--bad`). Only the final strip renders the address ruler to prevent visual clutter.
- **Synchronized Playback**: All three strips share a unified step index, scrubber, and transport controls. Ghost bars, splitting dissolves, and failure shake brackets run simultaneously across strips.
- **Per-Policy Narrations**: Compact 15px mechanism sentences explain placement or failure for each allocator.
- **Aligned Comparison Table**: A 3-column table tracking Free units, Largest hole, Hole count, External fragmentation percentage, and cumulative Failure counts. The optimal metric per row is highlighted in `--ok`.

### 7.4 Replacement View (Policy Tabs)
- **WAI-ARIA Tablist**: Accessible tabs (`role="tablist"`, `role="tab"`) display dynamic fault counts: `"FIFO 15 faults"`, `"LRU 12 faults"`, and `"OPT 9 faults"`.
- **Keyboard Semantics**: Left/Right arrow keys cycle through policies with roving `tabindex` and automatic focus management.
- **Index Preservation**: Switching policy tabs maintains the presenter's exact current step index, allowing direct comparison of resident frames at any reference moment.

### 7.5 Belady's Anomaly View
- **Left Column (3/5)**: An SVG line chart (540x340 viewBox) plotting page faults ($y \ge 0$) against physical frames ($1 \le x \le 6$):
  - **FIFO**: Solid 2px line in `--text` with 4px circular data points.
  - **LRU**: 1px dashed line in `--muted` with 4px circular data points.
  - **OPT**: 1px dotted line in `--muted` with 4px circular data points.
  - **Anomaly Indicator**: Frame counts from `sweeps.fifo.belady_points` are circled in `--bad` with a descriptive label: `"4 frames: 10 faults, more than 9 with 3 frames"`.
  - **Axes & Ticks**: Mono coordinate ticks with sans-serif `"frames"` and `"page faults"` labels.
- **Right Column (2/5)**: Dual vertically stacked textbook replacement tables (`ac4_fifo_f3` with 3 frames above `ac4_fifo_f4` with 4 frames). Both tables scrub in unison from the shared bottom transport bar, labeled with frame capacity and total faults.

