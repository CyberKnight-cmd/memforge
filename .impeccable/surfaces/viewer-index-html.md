---
version: 1
slug: "viewer-index-html"
primary_target: "viewer/index.html"
related_targets: []
---

Scope: viewer/index.html, the whole trace viewer. Mode: Operate.
Audience and job: presenter replays a trace live; audience follows why allocations fail and how compaction and page replacement behave. Same screens feed report figures.
Constraints: plain HTML/CSS/JS, offline, dark and light themes with a toggle, 1280x720 without scroll, keyboard driven, reduced-motion honoured.
Pinned by the user: Terminal / instrument world; must not read as a hacker-movie costume, a generic card dashboard, too dense, or toy-like.

## Direction contract

THESIS: A memory instrument that narrates. Each step animates exactly what changed in memory and states, in one plain sentence, what happened and why. Refuses the category default of stat cards around a static bar.

OWN-WORLD: Instrument panel, not terminal costume. Warm graphite ground (dark) or bone paper (light), hairline 1px rules, one amber playhead accent used only for "now" (current step, focus, selection). Process colours from Okabe-Ito; free memory is diagonal hatching, so greyscale print keeps every distinction. System UI sans for words, monospace with tabular figures only for addresses, sizes, ids and counts.

STORY: The visitor sees memory as one proportional strip, watches a request search the holes, sees it fail with the free total and the largest hole bracketed, watches compaction slide blocks together, then sees the retry land. Paging reads as the textbook replacement table filling column by column.

FIRST VIEWPORT: 48px top bar (MemForge, trace and policy, Open trace, theme toggle, keyboard help). Left two thirds: memory strip at full width with address ruler, and the narration sentence beneath it. Right third: fragmentation readout (aligned table with this-step deltas) over an ext-frag step chart with a playhead. Bottom: transport bar with play, step, and a scrubber whose ticks mark failures and compactions.

FORM: Pinned by the user (Terminal / instrument), Operate mode rules applied; concept-seed not run. seed: none (pinned).

RISK: Animation that is pretty but slow in a live demo; mitigated by 300-450ms motion, instant jumps when scrubbing, and reduced-motion support.
