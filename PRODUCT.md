# Product

<!-- impeccable:product-schema 1 -->

## Platform

web

## Users
A university OS course team (P1-P10) presenting MemForge, and the professor and classmates watching. The presenter drives the viewer live with the keyboard while talking; the audience must follow what memory is doing without having read the code. A second use: the same screens are captured for a 30-page academic report.

## Product Purpose
MemForge is a deterministic C simulator of an OS memory manager: contiguous allocation (First-, Best- and Worst-Fit, splitting, coalescing, fragmentation, compaction) and paging (FIFO, LRU, OPT, Belady's anomaly). The trace viewer replays a JSONL trace step by step and makes each step's change visible and explainable. Success: a teacher watching the demo understands why a request failed through fragmentation and how compaction fixed it, and any question can be answered from what is on screen.

## Positioning
Every number on screen comes from a deterministic, byte-identical trace of the real simulator, not an animation made up for slides. The viewer explains mechanisms (scan, split, coalesce, relocate, fault, evict) as they happen.

## Operating Context
Live 10-minute demo on a lecture-room projector at 1280x720, presenter at a laptop. Screenshots and prints for the report. Runs offline from a static file; traces are files produced by `memsim` or bundled samples in viewer/traces/.

## Capabilities and Constraints
- Static HTML, CSS and plain JavaScript; no framework, no build step besides `node viewer/build.js` that inlines into standalone.html (ADR-004). Must work offline from file://.
- Input is the section 11 JSONL trace: TraceHeader, events with seq/step, Snapshot lines, RunSummary.
- Both a dark and a light theme, user-switchable; print uses light.
- The presenter should not need to learn the UI or configure anything to demo it.

## Product Principles
- Show the mechanism, not just the state: every step animates what changed and says why in one sentence.
- Numbers are measurements: exact, aligned, traceable to the trace.
- Zero setup for the presenter: open a trace and press play.
- Calm at a distance: readable from the back of the room, never crowded.

## Accessibility & Inclusion
Colour-blind-safe semantics that also survive greyscale print; full keyboard operation with visible focus; respects prefers-reduced-motion.
