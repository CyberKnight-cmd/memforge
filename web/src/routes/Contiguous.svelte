<script lang="ts">
  import MemoryStrip from '../components/MemoryStrip.svelte';
  import Tokens from '../components/Tokens.svelte';
  import Segmented from '../components/Segmented.svelte';
  import Icon from '../components/Icon.svelte';
  import {
    run, contiguousText, narrate, PLACEMENT, POLICY_LABEL, SILBERSCHATZ_SETUP, SILBERSCHATZ_REQUESTS,
    type ContOp, type Run, type Block,
  } from '../lib/sim';
  import { query } from '../lib/route';

  type Preset = { key: string; label: string; memory: number; ops: ContOp[] };
  const presets: Preset[] = [
    { key: 'holes', label: 'Textbook: five holes', memory: 1740, ops: SILBERSCHATZ_SETUP },
    { key: 'requests', label: 'Textbook: five holes + four requests', memory: 1740, ops: [...SILBERSCHATZ_SETUP, ...SILBERSCHATZ_REQUESTS] },
    { key: 'empty', label: 'Empty memory (1024 units)', memory: 1024, ops: [] },
  ];

  const q = query();
  const startPreset = presets.find((p) => p.key === q.get('preset')) ?? presets[0];
  let presetKey = $state(startPreset.key);
  let memory = $state(startPreset.memory);
  let ops = $state<ContOp[]>([...startPreset.ops]);
  let setupCount = $state(startPreset.ops.filter((o) => o.kind === 'alloc' || o.kind === 'free' ? o.pid === 'SYS' : false).length);
  let mode = $state<string>(q.get('mode') ?? 'first-fit');
  let pid = $state('P1');
  let size = $state(212);
  let runs = $state<Record<string, Run>>({});
  let cursor = $state<number | null>(null); // null: follow the latest step

  function load(key: string) {
    const p = presets.find((x) => x.key === key)!;
    memory = p.memory;
    ops = [...p.ops];
    setupCount = p.ops.filter((o) => (o.kind === 'alloc' || o.kind === 'free' ? o.pid === 'SYS' : false)).length;
    cursor = null;
  }

  const policies = $derived(mode === 'compare' ? [...PLACEMENT] : [mode]);
  $effect(() => {
    const text = contiguousText(memory, ops);
    const want = [...policies];
    Promise.all(want.map((p) => run(text, p))).then((rs) => {
      const next: Record<string, Run> = {};
      want.forEach((p, i) => (next[p] = rs[i]));
      runs = next;
    });
  });

  const primary = $derived(runs[policies[0]]);
  const steps = $derived(primary?.ok ? primary.steps : []);
  const last = $derived(Math.max(0, steps.length - 1));
  const at = $derived(cursor === null ? last : Math.min(cursor, last));
  const firstVisible = $derived(Math.min(setupCount, last));

  const palette = ['var(--p0)', 'var(--p1)', 'var(--p2)', 'var(--p3)', 'var(--p4)', 'var(--p5)'];
  const colorOf = (p: string) => (p === 'SYS' ? 'var(--sys)' : palette[(Number(p.replace(/\D/g, '')) || 0) % palette.length]);

  let nextId = 0;
  function freshId() {
    const used = new Set(ops.map((o) => (o.kind === 'alloc' ? o.id : '')));
    do nextId++;
    while (used.has(`r${nextId}`));
    return `r${nextId}`;
  }
  function push(op: ContOp) {
    if (cursor !== null) ops = ops.slice(0, cursor); // acting from the past discards the future
    ops = [...ops, op];
    cursor = null;
  }
  const allocate = () => size > 0 && push({ kind: 'alloc', pid, id: freshId(), size: Math.round(size) });
  const freeBlock = (b: Block) => push({ kind: 'free', pid: b.pid!, id: b.id! });
  const compactNow = () => push({ kind: 'compact' });
  const terminate = (p: string) => push({ kind: 'terminate', pid: p });
  const live = $derived([...new Set((steps[at]?.blocks ?? []).filter((b) => b.state === 'allocated' && b.pid !== 'SYS').map((b) => b.pid!))].sort());
  const undo = () => {
    if (ops.length > setupCount) {
      ops = ops.slice(0, -1);
      cursor = null;
    }
  };

  function onKey(e: KeyboardEvent) {
    if ((e.target as HTMLElement).closest('input, select, textarea')) return;
    if (e.key === 'ArrowLeft') cursor = Math.max(firstVisible, at - 1);
    else if (e.key === 'ArrowRight') cursor = at + 1 >= last ? null : at + 1;
    else return;
    e.preventDefault();
  }

  const fmtPct = (v: number) => `${(v * 100).toFixed(1)}%`;
  const CW = 420, CH = 90;
  const cx = (i: number) => (last > firstVisible ? ((i - firstVisible) / (last - firstVisible)) * (CW - 8) + 4 : 4);
  const cy = (v: number) => CH - 6 - v * (CH - 12);
  const fragPath = (r: Run) => (r.ok ? r.steps.slice(firstVisible).map((st, i) => `${i ? 'L' : 'M'}${cx(firstVisible + i)},${cy(st.frag?.ext_frag ?? 0)}`).join(' ') : '');
  const rows = [
    { key: 'total_free', label: 'Free units', better: 'high' },
    { key: 'largest_hole', label: 'Largest hole', better: 'high' },
    { key: 'hole_count', label: 'Holes', better: 'low' },
    { key: 'ext_frag', label: 'External fragmentation', better: 'low' },
    { key: 'used', label: 'Used units', better: 'high' },
  ] as const;
  const failuresUpTo = (r: Run | undefined, i: number) =>
    r?.ok ? r.steps.slice(0, i + 1).filter((s) => s.events.some((e) => e.type === 'AllocationFailed')).length : 0;
</script>

<svelte:window onkeydown={onKey} />

<header class="head">
  <div>
    <h1>Contiguous memory</h1>
    <p class="lede">Request memory, free blocks and compact. Every action runs the real C engine; the strip shows its exact layout after each step.</p>
  </div>
</header>

<div class="lab">
  <aside class="panel controls" aria-label="Controls">
    <section>
      <label class="label" for="preset">Starting memory</label>
      <select id="preset" class="field" bind:value={presetKey} onchange={() => load(presetKey)}>
        {#each presets as p}<option value={p.key}>{p.label}</option>{/each}
      </select>
    </section>

    <section>
      <span class="label">Placement policy</span>
      <Segmented label="Placement policy" bind:value={mode}
        options={[...PLACEMENT.map((p) => ({ value: p, label: POLICY_LABEL[p].replace('-Fit', '') })), { value: 'compare', label: 'All 3' }]} />
      <p class="hint">
        {#if mode === 'first-fit'}Takes the first hole large enough, scanning from address 0.
        {:else if mode === 'best-fit'}Takes the smallest hole that fits, so it scans every hole.
        {:else if mode === 'worst-fit'}Takes the largest hole, leaving the biggest remainder.
        {:else}Runs the same requests under all three policies side by side.{/if}
      </p>
    </section>

    <form class="request" onsubmit={(e) => { e.preventDefault(); allocate(); }}>
      <span class="label">New request</span>
      <div class="row">
        <select class="field" bind:value={pid} aria-label="Process">
          {#each ['P1', 'P2', 'P3', 'P4', 'P5'] as p}<option>{p}</option>{/each}
        </select>
        <input class="field mono" type="number" min="1" max={memory} bind:value={size} aria-label="Size in units" />
        <button class="btn primary" type="submit">Allocate</button>
      </div>
      <p class="hint">Click any block in memory to free it.</p>
    </form>

    <section class="actions">
      <button class="btn" onclick={compactNow}><Icon name="compact" /> Compact</button>
      <button class="btn" onclick={undo} disabled={ops.length <= setupCount}><Icon name="undo" /> Undo</button>
      <button class="btn ghost" onclick={() => load(presetKey)}><Icon name="reset" /> Reset</button>
    </section>

    {#if live.length}
      <section>
        <span class="label">Running processes</span>
        <ul class="procs">
          {#each live as p (p)}
            {@const mine = (steps[at]?.blocks ?? []).filter((b) => b.pid === p)}
            <li>
              <span class="swatch" style:background={colorOf(p)}></span>
              <span class="mono">{p}</span>
              <span class="mono faint">{mine.length} {mine.length === 1 ? 'block' : 'blocks'} · {mine.reduce((a, b) => a + b.size, 0)} units</span>
              <button class="btn ghost small" onclick={() => terminate(p)} disabled={cursor !== null}>Terminate</button>
            </li>
          {/each}
        </ul>
      </section>
    {/if}
  </aside>

  <section class="stage">
    {#if primary && !primary.ok}
      <p class="panel error">The engine rejected this workload: {primary.error}</p>
    {:else if steps.length}
      {#each policies as p (p)}
        {@const r = runs[p]}
        {#if r?.ok}
          {@const st = r.steps[Math.min(at, r.steps.length - 1)]}
          <article class="panel memory">
            <div class="memory-head">
              <h2>{POLICY_LABEL[p]}</h2>
              <span class="mono step">step {st.step} of {r.steps.length - 1}</span>
            </div>
            <MemoryStrip blocks={st.blocks} memorySize={memory} events={st.events} policy={p} {colorOf}
              onBlockClick={cursor === null ? freeBlock : undefined} compact={policies.length > 1} />
            <p class="narration"><Tokens toks={narrate(st.events, p)} {colorOf} /></p>
          </article>
        {/if}
      {/each}

      <div class="lower">
        <article class="panel history">
          <h2>Steps</h2>
          <ol>
            {#if setupCount > 0}
              <li><button class:on={at === firstVisible} onclick={() => (cursor = firstVisible)}>
                <span class="mono idx">0</span><span>Setup: {steps[firstVisible]?.blocks.filter((b) => b.state === 'free').length} holes prepared by SYS</span></button></li>
            {/if}
            {#each steps.slice(firstVisible + 1) as st, i (st.step)}
              <li><button class:on={at === firstVisible + 1 + i} onclick={() => (cursor = firstVisible + 1 + i === last ? null : firstVisible + 1 + i)}>
                <span class="mono idx">{i + 1}</span><Tokens toks={narrate(st.events, policies[0])} {colorOf} /></button></li>
            {/each}
          </ol>
          <p class="hint">Use ← and → to step through history. Acting from an earlier step discards the later ones.</p>
        </article>

        <article class="panel metrics">
          <h2>Measurements</h2>
          <table>
            <thead>
              <tr><th scope="col"></th>{#each policies as p}<th scope="col">{POLICY_LABEL[p]}</th>{/each}</tr>
            </thead>
            <tbody>
              {#each rows as row}
                {@const vals = policies.map((p) => { const r = runs[p]; return r?.ok ? (r.steps[Math.min(at, r.steps.length - 1)].frag?.[row.key] ?? 0) : 0; })}
                {@const best = row.better === 'high' ? Math.max(...vals) : Math.min(...vals)}
                <tr>
                  <th scope="row">{row.label}</th>
                  {#each vals as v}<td class="mono" class:best={policies.length > 1 && v === best}>{row.key === 'ext_frag' ? fmtPct(v) : v}</td>{/each}
                </tr>
              {/each}
              <tr>
                <th scope="row">Failed requests</th>
                {#each policies as p}{@const f = failuresUpTo(runs[p], at)}<td class="mono" class:bad={f > 0}>{f}</td>{/each}
              </tr>
            </tbody>
          </table>
          <p class="hint">External fragmentation = 1 − largest hole ÷ free units: the share of free memory a single request cannot use.</p>
          <svg class="chart" viewBox="0 0 {CW} {CH}" role="img" aria-label="External fragmentation at every step">
            <line class="grid" x1="0" x2={CW} y1={cy(0)} y2={cy(0)} /><line class="grid" x1="0" x2={CW} y1={cy(1)} y2={cy(1)} />
            {#each policies as p, k}
              {@const r = runs[p]}
              {#if r?.ok}
                <path class="frag" class:dim={policies.length > 1 && k > 0} stroke-dasharray={['', '6 4', '2 4'][k]} d={fragPath(r)} />
                {#each r.steps.slice(firstVisible) as st, i}{#if st.events.some((e) => e.type === 'AllocationFailed')}<circle class="failpt" cx={cx(firstVisible + i)} cy={cy(st.frag?.ext_frag ?? 0)} r="4" />{/if}{/each}
              {/if}
            {/each}
            <line class="cursor" x1={cx(at)} x2={cx(at)} y1="4" y2={CH - 4} />
          </svg>
          <p class="hint">Fragmentation after every step{policies.length > 1 ? ' (solid First, dashed Best, dotted Worst)' : ''}; red dots mark failed requests.</p>
        </article>
      </div>
    {:else}
      <p class="panel loading">Starting the engine…</p>
    {/if}
  </section>
</div>

<style>
  .head {
    margin-bottom: 24px;
  }
  h1 {
    font-size: 28px;
  }
  .lede {
    margin-top: 8px;
    color: var(--muted);
    max-width: 68ch;
  }
  .lab {
    display: grid;
    grid-template-columns: 300px minmax(0, 1fr);
    gap: 20px;
    align-items: start;
  }
  .controls {
    position: sticky;
    top: 80px;
    display: grid;
    gap: 22px;
    padding: 20px;
  }
  .controls section,
  .request {
    display: grid;
    gap: 8px;
  }
  .row {
    display: grid;
    grid-template-columns: 72px minmax(0, 1fr) auto;
    gap: 8px;
  }
  .actions {
    grid-template-columns: 1fr 1fr;
  }
  .actions .ghost {
    grid-column: span 2;
    justify-content: center;
  }
  .actions .btn {
    justify-content: center;
  }
  .hint {
    font-size: 13px;
    color: var(--faint);
  }
  .stage {
    display: grid;
    gap: 16px;
    min-width: 0;
  }
  .memory {
    padding: 18px 20px 16px;
  }
  .memory-head {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
  }
  h2 {
    font-size: 15px;
  }
  .step {
    font-size: 13px;
    color: var(--faint);
  }
  .narration {
    margin-top: 4px;
    font-size: 17px;
    line-height: 1.5;
    max-width: 80ch;
  }
  .lower {
    display: grid;
    grid-template-columns: minmax(0, 1.2fr) minmax(0, 1fr);
    gap: 16px;
    align-items: start;
  }
  .history,
  .metrics {
    padding: 18px 20px;
    display: grid;
    gap: 12px;
  }
  ol {
    list-style: none;
    margin: 0;
    padding: 0;
    display: grid;
    gap: 2px;
    max-height: 340px;
    overflow: auto;
  }
  ol button {
    display: grid;
    grid-template-columns: 28px 1fr;
    gap: 8px;
    width: 100%;
    text-align: left;
    padding: 8px 10px;
    border: 0;
    border-radius: 6px;
    background: transparent;
    color: var(--muted);
    font-size: 14px;
    cursor: pointer;
  }
  ol button:hover {
    background: var(--raised);
    color: var(--text);
  }
  ol button.on {
    background: var(--raised);
    color: var(--text);
    box-shadow: inset 2px 0 0 var(--accent);
  }
  .idx {
    color: var(--faint);
  }
  table {
    width: 100%;
    border-collapse: collapse;
    font-size: 14px;
  }
  th,
  td {
    padding: 8px 0;
    border-bottom: 1px solid var(--rule);
    text-align: right;
    font-weight: 500;
  }
  th[scope='row'] {
    text-align: left;
    color: var(--muted);
  }
  thead th {
    color: var(--faint);
    font-size: 13px;
  }
  td.best {
    color: var(--ok);
  }
  td.bad {
    color: var(--bad);
  }
  .procs {
    list-style: none;
    margin: 0;
    padding: 0;
    display: grid;
    gap: 4px;
  }
  .procs li {
    display: grid;
    grid-template-columns: 10px 28px 1fr auto;
    align-items: center;
    gap: 8px;
    font-size: 13px;
  }
  .swatch {
    width: 10px;
    height: 10px;
    border-radius: 3px;
  }
  .faint {
    color: var(--faint);
  }
  .small {
    height: 28px;
    padding: 0 8px;
    font-size: 13px;
  }
  .chart {
    width: 100%;
    height: auto;
  }
  .chart .grid {
    stroke: var(--rule);
  }
  .frag {
    fill: none;
    stroke: var(--text);
    stroke-width: 1.75;
  }
  .frag.dim {
    stroke: var(--muted);
    stroke-width: 1.25;
  }
  .failpt {
    fill: var(--bad);
  }
  .cursor {
    stroke: var(--accent);
    stroke-width: 1.5;
  }
  .error,
  .loading {
    padding: 20px;
    color: var(--muted);
  }
  .error {
    color: var(--bad);
  }
  @media (max-width: 1080px) {
    .lab {
      grid-template-columns: minmax(0, 1fr);
    }
    .controls {
      position: static;
    }
    .lower {
      grid-template-columns: minmax(0, 1fr);
    }
  }
</style>
