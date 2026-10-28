# Live demo run sheet (10 minutes)

Before the room fills: `bash scripts/build_wasm.sh`, then `cd web && npm ci && npm run dev`, open
http://localhost:5173 in a full-screen browser at 1280x720 or larger, light theme for a bright room.
Have a terminal with a release build ready: `cmake --preset release && cmake --build --preset release`.
Fallback if the laptop fails: a screen recording made at rehearsal, and the report figures.

| Time | Show | Say |
| --- | --- | --- |
| 0:00 | Overview page, live panel running | MemForge simulates an OS memory manager. The C engine runs in this page as WebAssembly, so every block on screen is what the engine decided. |
| 1:00 | Terminal: `build/release/bin/memsim compare tests/fixtures/ac1_silberschatz.wl --policies first-fit,best-fit,worst-fit` | The textbook five holes. The same four requests under three policies: only Best-Fit places the 426 request (AC-1, AC-8). |
| 2:00 | Scenario 1, "One request, three policies"; press ← back to a3, then → | Watch the holes each policy examines light up. On the last request First- and Worst-Fit fail. |
| 3:30 | Scenario 2, "Fragmentation, then compaction"; at the failure point to the readout, then press Compact, then Allocate P4 426 | 959 units are free but the largest hole is 300: external fragmentation. Compaction moves 7 blocks, 781 units; the retry fits (AC-2). |
| 5:00 | Scenario 3, "Page replacement"; press Play | The textbook string with 3 frames: FIFO 15, LRU 12, OPT 9 faults (AC-3). |
| 6:30 | Scenario 4, "Belady's anomaly"; frames 3 then 4; point at the circled point | With FIFO a fourth frame causes more faults, 9 to 10 (AC-4); LRU and OPT never do. |
| 7:30 | Scenario 6, "Address translation"; step once | A virtual address splits into page number and offset; only the page number is replaced by the frame number. |
| 8:15 | Contiguous lab, Random workload, seed 3, 90%, 3000 requests, All 3 | This is exactly one of experiment E1's workloads, generated in the browser byte-for-byte like the CLI. |
| 9:00 | Report figures e1_placement.png and e4_faults_vs_frames.png | At 90% utilisation Best-Fit succeeds 91.4%, First-Fit 90.0%, Worst-Fit 87.2%; OPT < LRU < FIFO at every frame count. All 8 acceptance criteria pass in CI. |

Likely questions: why Best-Fit fragments less here (it preserves large holes); why OPT is not
implementable (it needs the future, `prepare` gives it the whole reference string); how the web app is
known to match the C code (CI compares their traces on every fixture).
