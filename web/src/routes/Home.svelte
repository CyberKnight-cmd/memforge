<script lang="ts">
  import MemoryStrip from '../components/MemoryStrip.svelte';
  import Tokens from '../components/Tokens.svelte';
  import Icon from '../components/Icon.svelte';
  import { run, contiguousText, narrate, SILBERSCHATZ_SETUP, SILBERSCHATZ_REQUESTS, type Run } from '../lib/sim';

  // A live run of the real engine: the textbook five holes, then four requests under First-Fit.
  const ops = [...SILBERSCHATZ_SETUP, ...SILBERSCHATZ_REQUESTS, { kind: 'compact' as const }, { kind: 'alloc' as const, pid: 'P4', id: 'a4b', size: 426 }];
  let demo = $state<Run | null>(null);
  let i = $state(SILBERSCHATZ_SETUP.length);
  run(contiguousText(1740, ops), 'first-fit').then((r) => (demo = r));
  $effect(() => {
    if (!demo?.ok) return;
    const n = demo.steps.length;
    const t = setInterval(() => (i = i + 1 >= n ? SILBERSCHATZ_SETUP.length : i + 1), 2600);
    return () => clearInterval(t);
  });
  const st = $derived(demo?.ok ? demo.steps[i] : null);
  const palette = ['var(--p0)', 'var(--p1)', 'var(--p2)', 'var(--p3)', 'var(--p4)', 'var(--p5)'];
  const colorOf = (p: string) => (p === 'SYS' ? 'var(--sys)' : palette[(Number(p.replace(/\D/g, '')) || 0) % palette.length]);
</script>

<section class="hero">
  <div class="copy">
    <h1>Watch an operating system manage memory.</h1>
    <p class="lede">
      MemForge simulates how an OS places processes in memory and swaps pages in and out. The simulator is written in C,
      compiled to WebAssembly and running in this page, so every block you see is exactly what the engine decided.
    </p>
    <div class="cta">
      <a class="btn primary" href="#/contiguous">Open the memory lab <Icon name="arrow" /></a>
      <a class="btn" href="#/paging">Open the paging lab</a>
    </div>
  </div>

  <figure class="panel live">
    <figcaption>
      <span class="dot" aria-hidden="true"></span>
      <span>Live · First-Fit on the textbook five holes</span>
      {#if st}<span class="mono step">step {st.step}</span>{/if}
    </figcaption>
    {#if st}
      <MemoryStrip blocks={st.blocks} memorySize={1740} events={st.events} policy="first-fit" {colorOf} />
      <p class="narration"><Tokens toks={narrate(st.events, 'first-fit')} {colorOf} /></p>
    {:else}
      <div class="placeholder">Starting the engine…</div>
    {/if}
  </figure>
</section>

<section class="paths" aria-label="Explore">
  <a class="path" href="#/contiguous">
    <h2>Contiguous allocation</h2>
    <p>Place requests with First-, Best- or Worst-Fit, free blocks, and see holes split and merge. Watch a request fail with plenty of free memory, then compact and retry.</p>
    <span class="go">Memory lab <Icon name="arrow" /></span>
  </a>
  <a class="path" href="#/paging">
    <h2>Paging and replacement</h2>
    <p>Step through a reference string under FIFO, LRU and OPT in the textbook frames table, and find Belady's anomaly, where more frames cause more faults.</p>
    <span class="go">Paging lab <Icon name="arrow" /></span>
  </a>
</section>

<section class="facts" aria-label="How it works">
  <div>
    <h3>One engine everywhere</h3>
    <p>The same C code powers the <span class="mono">memsim</span> command line, the tests and this page. The browser build is checked to produce identical traces.</p>
  </div>
  <div>
    <h3>Deterministic</h3>
    <p>The same requests always give the same result, byte for byte. Every step in a lab can be replayed and inspected.</p>
  </div>
  <div>
    <h3>Checked as it runs</h3>
    <p>After every step the engine verifies its own invariants: no overlapping blocks, no adjacent holes, every unit accounted for.</p>
  </div>
</section>

<style>
  .hero {
    display: grid;
    grid-template-columns: minmax(0, 0.9fr) minmax(0, 1.1fr);
    gap: 48px;
    align-items: center;
    padding: 40px 0 56px;
  }
  h1 {
    font-size: clamp(34px, 4.4vw, 54px);
    letter-spacing: -0.035em;
  }
  .lede {
    margin-top: 20px;
    font-size: 18px;
    color: var(--muted);
    max-width: 52ch;
  }
  .cta {
    display: flex;
    gap: 10px;
    margin-top: 28px;
    flex-wrap: wrap;
  }
  .cta .btn {
    height: 42px;
    padding: 0 18px;
    text-decoration: none;
  }
  .live {
    margin: 0;
    padding: 18px 20px 20px;
  }
  figcaption {
    display: flex;
    align-items: center;
    gap: 8px;
    font-size: 13px;
    color: var(--muted);
    margin-bottom: 4px;
  }
  .dot {
    width: 7px;
    height: 7px;
    border-radius: 50%;
    background: var(--ok);
    animation: pulse 1.8s ease-in-out infinite;
  }
  @keyframes pulse {
    50% {
      opacity: 0.35;
    }
  }
  .step {
    margin-left: auto;
    color: var(--faint);
  }
  .narration {
    min-height: 3em;
    font-size: 16px;
  }
  .placeholder {
    height: 160px;
    display: grid;
    place-items: center;
    color: var(--faint);
  }
  .paths {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 16px;
  }
  .path {
    display: grid;
    gap: 10px;
    align-content: start;
    padding: 24px;
    border: 1px solid var(--rule);
    border-radius: 12px;
    background: var(--panel);
    color: inherit;
    text-decoration: none;
    transition: border-color 200ms var(--ease), transform 200ms var(--ease);
  }
  .path:hover {
    border-color: var(--rule-strong);
    transform: translateY(-2px);
  }
  .path h2 {
    font-size: 20px;
  }
  .path p {
    color: var(--muted);
  }
  .go {
    display: inline-flex;
    align-items: center;
    gap: 6px;
    margin-top: 6px;
    color: var(--accent);
    font-weight: 500;
  }
  .facts {
    display: grid;
    grid-template-columns: repeat(3, 1fr);
    gap: 32px;
    margin-top: 56px;
    padding-top: 32px;
    border-top: 1px solid var(--rule);
  }
  .facts h3 {
    font-size: 15px;
    margin-bottom: 6px;
  }
  .facts p {
    color: var(--muted);
    font-size: 14px;
  }
  @media (max-width: 960px) {
    .hero,
    .paths,
    .facts {
      grid-template-columns: minmax(0, 1fr);
    }
  }
</style>
