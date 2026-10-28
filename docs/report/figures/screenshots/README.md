# Screenshots to capture

Run the web app (`cd web && npm run dev`), set the browser window to 1440x900, light theme for print
(sun/moon button), and save each PNG here under the exact file name.

| File | Open | State to capture |
| --- | --- | --- |
| web_overview_hero.png | `#/` | Top of the page with the live panel mid-animation |
| web_contiguous_lab.png | `#/contiguous?preset=requests&mode=first-fit` | Whole lab after loading: controls, strip, steps, measurements |
| web_compare_view.png | `#/contiguous?preset=requests&mode=compare` | The three strips at the last step (Best-Fit placed 426, the others failed) |
| web_fragmentation_failure.png | `#/contiguous?preset=requests&mode=first-fit` | Last step: dashed red request bar and "free 959 · largest hole 300" |
| web_compaction.png | same, then press Compact | Right after compaction: one hole at the end, narration "slides 7 blocks" |
| web_paging_table.png | `#/paging?preset=ac3&mode=compare` | All three FIFO/LRU/OPT tables at the end (15, 12, 9 faults) |
| web_belady_chart.png | `#/paging?preset=ac4&mode=fifo` | The "Faults against frames" panel with the anomaly circled |
| web_address_translation.png | `#/paging?preset=va&mode=fifo` | Step to a reference, then capture page tables + address translation |
| web_replay_page.png | `#/replay` | After opening a trace from `memsim run tests/fixtures/ac2_compact.wl --policy first-fit --trace t.jsonl` |
