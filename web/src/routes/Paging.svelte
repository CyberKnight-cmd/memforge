<script lang="ts">
  import FrameTable from '../components/FrameTable.svelte';
  import Tokens from '../components/Tokens.svelte';
  import Segmented from '../components/Segmented.svelte';
  import Icon from '../components/Icon.svelte';
  import { generateReferences, run, pagingText, parseRefs, processesOf, narrate, REPLACEMENT, POLICY_LABEL, AC3_REFS, AC4_REFS, MULTI_REFS, VA_REFS, type Run } from '../lib/sim';
  import { query } from '../lib/route';

  const presets = [
    { key: 'ac3', label: 'Textbook string, 3 frames', refs: AC3_REFS, frames: 3, pageSize: 4096 },
    { key: 'ac4', label: "Belady's string, 3 frames", refs: AC4_REFS, frames: 3, pageSize: 4096 },
    { key: 'multi', label: 'Two processes with writes, 4 frames', refs: MULTI_REFS, frames: 4, pageSize: 4096 },
    { key: 'va', label: 'Virtual addresses, 4 KiB pages', refs: VA_REFS, frames: 2, pageSize: 4096 },
  ];
  const q = query();
  const start = presets.find((p) => p.key === q.get('preset')) ?? presets[0];
  let presetKey = $state(start.key);
  let refsText = $state(start.refs);
  let frames = $state(start.frames);
  let pageSize = $state(start.pageSize);
  let mode = $state<string>(q.get('mode') ?? 'fifo');
  let runs = $state<Record<string, Run>>({});
  let sweep = $state<Record<string, number[]>>({});
  let cursor = $state<number | null>(null);
  let playing = $state(false);
  const MAX_SWEEP = 7;

  const parsed = $derived(parseRefs(refsText, pageSize));
  const refs = $derived(parsed.refs);
  const procs = $derived(processesOf(refs));
  const firstAccess = $derived(1 + procs.length);
  const policies = $derived(mode === 'compare' ? [...REPLACEMENT] : [mode]);
  const offsetBits = $derived(Math.log2(pageSize));

  // Random references from the refs-locality generator: a working set that drifts over time.
  let gen = $state({ seed: 1, n: 60, pages: 12, workingSet: 4, shiftEvery: 20, pOut: 5, pWrite: 20 });
  async function generate() {
    const text = await generateReferences({ seed: gen.seed, n: gen.n, pages: gen.pages, workingSet: gen.workingSet, shiftEvery: gen.shiftEvery,
      pOutPpm: gen.pOut * 10000, pWritePpm: gen.pWrite * 10000, frames, pageSize });
    if (text === null) return;
    refsText = text;
    cursor = null;
    playing = false;
  }
  function useE4() {
    gen = { seed: gen.seed, n: 2000, pages: 32, workingSet: 6, shiftEvery: 200, pOut: 5, pWrite: 0 };
    generate();
  }

  function pick(key: string) {
    if (key === 'random') return void generate();
    const p = presets.find((x) => x.key === key)!;
    refsText = p.refs;
    frames = p.frames;
    pageSize = p.pageSize;
    cursor = null;
    playing = false;
  }

  $effect(() => {
    const text = pagingText(frames, pageSize, refs);
    const want = [...policies];
    Promise.all(want.map((p) => run(text, p))).then((rs) => {
      const next: Record<string, Run> = {};
      want.forEach((p, i) => (next[p] = rs[i]));
      runs = next;
    });
  });
  // Faults against frame count, from real runs at every frame count.
  $effect(() => {
    const r = [...refs];
    const ps = pageSize;
    Promise.all(
      REPLACEMENT.map(async (p) => {
        const out: number[] = [];
        for (let f = 1; f <= MAX_SWEEP; f++) {
          const res = await run(pagingText(f, ps, r), p);
          out.push(res.ok ? res.summary.faults : 0);
        }
        return [p, out] as const;
      }),
    ).then((pairs) => (sweep = Object.fromEntries(pairs)));
  });

  const primary = $derived(runs[policies[0]]);
  const steps = $derived(primary?.ok ? primary.steps : []);
  const last = $derived(Math.max(firstAccess - 1, steps.length - 1));
  const at = $derived(cursor === null ? last : Math.min(cursor, last));
  const go = (i: number) => (cursor = i >= last ? null : Math.max(firstAccess - 1, i));

  $effect(() => {
    if (!playing) return;
    if (cursor === null) cursor = firstAccess - 1;
    const t = setInterval(() => {
      if (cursor === null || cursor >= last) {
        playing = false;
        cursor = null;
      } else cursor += 1;
    }, 900);
    return () => clearInterval(t);
  });

  function onKey(e: KeyboardEvent) {
    if ((e.target as HTMLElement).closest('input, select, textarea')) return;
    if (e.key === 'ArrowLeft') go(at - 1);
    else if (e.key === 'ArrowRight') go(at + 1);
    else if (e.key === ' ') playing = !playing;
    else if (e.key === 'Home') go(firstAccess - 1);
    else if (e.key === 'End') go(last);
    else return;
    e.preventDefault();
  }

  function stats(r: Run | undefined) {
    const out = { refs: 0, hits: 0, faults: 0, compulsory: 0, capacity: 0, evictions: 0, writebacks: 0 };
    if (!r?.ok) return out;
    for (const st of r.steps.slice(0, at + 1))
      for (const e of st.events) {
        if (e.type === 'PageHit') out.hits++;
        else if (e.type === 'PageFault') {
          out.faults++;
          if (e.data.kind === 'COMPULSORY') out.compulsory++;
          else out.capacity++;
        } else if (e.type === 'PageEvicted') out.evictions++;
        else if (e.type === 'PageWrittenBack') out.writebacks++;
      }
    out.refs = out.hits + out.faults;
    return out;
  }
  const anomalies = $derived((sweep.fifo ?? []).flatMap((v, i, a) => (i > 0 && v > a[i - 1] ? [i + 1] : [])));

  // Page tables are what the frames say: a page is present exactly when some frame holds it.
  const pageTables = $derived.by(() => {
    const st = steps[at];
    return procs.map((p) => ({
      pid: p.pid,
      rows: Array.from({ length: p.pages }, (_, vpn) => {
        const f = st?.frames.find((x) => x.pid === p.pid && x.vpn === vpn);
        return { vpn, frame: f ? f.frame : null, r: !!f?.referenced, d: !!f?.dirty };
      }),
    }));
  });
  const translation = $derived(steps[at]?.events.find((e) => e.type === 'AddressTranslated')?.data);
  const bits = (v: number, width: number) => v.toString(2).padStart(width, '0');

  const palette = ['var(--p0)', 'var(--p1)', 'var(--p2)', 'var(--p3)', 'var(--p4)', 'var(--p5)'];
  const colorOf = (p: string) => palette[(Number(p.replace(/\D/g, '')) || 0) % palette.length];

  // Chart geometry
  const W = 520, H = 220, PAD = { l: 40, r: 16, t: 16, b: 34 };
  const maxY = $derived(Math.max(1, ...Object.values(sweep).flat()));
  const x = (f: number) => PAD.l + ((f - 1) / (MAX_SWEEP - 1)) * (W - PAD.l - PAD.r);
  const y = (v: number) => H - PAD.b - (v / maxY) * (H - PAD.t - PAD.b);
  const line = (vals: number[]) => vals.map((v, i) => `${i ? 'L' : 'M'}${x(i + 1)},${y(v)}`).join(' ');
  const lineStyle: Record<string, string> = { fifo: '', lru: '6 4', opt: '2 4' };
</script>

<svelte:window onkeydown={onKey} />

<header class="head">
  <h1>Paging</h1>
  <p class="lede">Each process sees its own pages; the OS keeps only some of them in physical frames. Make references, and watch the replacement policy choose which page to evict when memory is full. Every column comes from the real engine.</p>
</header>

<div class="lab">
  <aside class="panel controls" aria-label="Controls">
    <section>
      <label class="label" for="preset">Example</label>
      <select id="preset" class="field" bind:value={presetKey} onchange={() => pick(presetKey)}>
        {#each presets as p}<option value={p.key}>{p.label}</option>{/each}
        <option value="random">Random references (generator)</option>
      </select>
      {#if presetKey === 'random'}
        <div class="gen">
          <label><span class="label">Seed</span><input class="field mono" type="number" min="0" bind:value={gen.seed} /></label>
          <label><span class="label">References</span><input class="field mono" type="number" min="1" max="2000" bind:value={gen.n} /></label>
          <label><span class="label">Pages</span><input class="field mono" type="number" min="1" max="64" bind:value={gen.pages} /></label>
          <label><span class="label">Working set</span><input class="field mono" type="number" min="1" bind:value={gen.workingSet} /></label>
          <label><span class="label">Shift every</span><input class="field mono" type="number" min="1" bind:value={gen.shiftEvery} /></label>
          <label><span class="label">Writes %</span><input class="field mono" type="number" min="0" max="100" bind:value={gen.pWrite} /></label>
          <button class="btn primary" onclick={generate}>Generate</button>
          <button class="btn" onclick={useE4}>E4 settings</button>
          <p class="hint span">Most references fall in a working set of pages that moves every few references, with {gen.pOut}% outside it. “E4 settings” reproduces experiment E4's workload for this seed.</p>
        </div>
      {/if}
    </section>
    <section>
      <label class="label" for="refs">References</label>
      <textarea id="refs" class="field mono" rows="3" bind:value={refsText} oninput={() => (cursor = null)} spellcheck="false"></textarea>
      <p class="hint"><span class="mono">7</span> page 7 · <span class="mono">7w</span> write · <span class="mono">P2:3</span> process P2 · <span class="mono">@6700</span> virtual address</p>
      {#if parsed.bad.length}<p class="hint bad">Ignored: <span class="mono">{parsed.bad.join(' ')}</span></p>{/if}
    </section>
    <div class="two">
      <section>
        <span class="label">Frames</span>
        <div class="stepper">
          <button class="btn" onclick={() => { frames = Math.max(1, frames - 1); cursor = null; }} aria-label="Fewer frames" disabled={frames <= 1}>−</button>
          <span class="mono count" aria-live="polite">{frames}</span>
          <button class="btn" onclick={() => { frames = Math.min(8, frames + 1); cursor = null; }} aria-label="More frames" disabled={frames >= 8}>+</button>
        </div>
      </section>
      <section>
        <label class="label" for="ps">Page size</label>
        <select id="ps" class="field mono" bind:value={pageSize} onchange={() => (cursor = null)}>
          {#each [256, 1024, 4096] as v}<option value={v}>{v} B</option>{/each}
        </select>
      </section>
    </div>
    <section>
      <span class="label">Replacement policy</span>
      <Segmented label="Replacement policy" bind:value={mode}
        options={[...REPLACEMENT.map((p) => ({ value: p, label: POLICY_LABEL[p] })), { value: 'compare', label: 'All 3' }]} />
      <p class="hint">
        {#if mode === 'fifo'}Evicts the page loaded earliest, however often it is used.
        {:else if mode === 'lru'}Evicts the page used least recently.
        {:else if mode === 'opt'}Evicts the page needed furthest in the future: unrealisable, but the lower bound every policy is measured against.
        {:else}Runs the same references under all three policies.{/if}
      </p>
    </section>
  </aside>

  <section class="stage">
    {#each policies as p (p)}
      {@const r = runs[p]}
      {#if r?.ok}
        {@const st = r.steps[Math.min(at, r.steps.length - 1)]}
        {@const s = stats(r)}
        <article class="panel table-card">
          <div class="card-head">
            <h2>{POLICY_LABEL[p]}</h2>
            <dl class="mono tally">
              <div><dt>faults</dt><dd class="bad">{s.faults}</dd></div>
              <div><dt>hits</dt><dd class="ok">{s.hits}</dd></div>
              <div><dt>hit ratio</dt><dd>{s.refs ? ((s.hits / s.refs) * 100).toFixed(0) : 0}%</dd></div>
              <div><dt>evictions</dt><dd>{s.evictions}</dd></div>
              <div><dt>write-backs</dt><dd>{s.writebacks}</dd></div>
            </dl>
          </div>
          <FrameTable steps={r.steps} {refs} {frames} {at} {firstAccess} {colorOf} />
          <p class="narration"><Tokens toks={at < firstAccess ? [{ t: `All ${frames} frames are empty. Press play or → to make the first reference.` }] : narrate(st.events, p)} {colorOf} /></p>
        </article>
      {:else if r && !r.ok}
        <p class="panel error">{r.error}</p>
      {/if}
    {/each}

    <div class="transport panel">
      <button class="btn ghost" onclick={() => go(firstAccess - 1)} aria-label="Back to start"><Icon name="start" /></button>
      <button class="btn ghost" onclick={() => go(at - 1)} aria-label="Previous reference"><Icon name="back" /></button>
      <button class="btn primary" onclick={() => (playing = !playing)} aria-label={playing ? 'Pause' : 'Play'}><Icon name={playing ? 'pause' : 'play'} /> {playing ? 'Pause' : 'Play'}</button>
      <button class="btn ghost" onclick={() => go(at + 1)} aria-label="Next reference"><Icon name="fwd" /></button>
      <button class="btn ghost" onclick={() => go(last)} aria-label="Jump to end"><Icon name="end" /></button>
      <span class="mono pos">reference {Math.max(0, at - firstAccess + 1)} of {refs.length}</span>
    </div>

    <div class="lower">
      <article class="panel">
        <h2>Page tables</h2>
        <p class="hint">One table per process. A page is present when a frame holds it; R is set on every use and D on every write.</p>
        <div class="tables">
          {#each pageTables as t (t.pid)}
            <table class="pt">
              <caption class="mono" style:color={colorOf(t.pid)}>{t.pid}</caption>
              <thead><tr><th>page</th><th>frame</th><th>R</th><th>D</th></tr></thead>
              <tbody>
                {#each t.rows as row}
                  <tr class:present={row.frame !== null}>
                    <td class="mono">{row.vpn}</td>
                    <td class="mono">{row.frame ?? '·'}</td>
                    <td class="mono">{row.frame !== null ? (row.r ? 1 : 0) : ''}</td>
                    <td class="mono" class:dirty={row.d}>{row.frame !== null ? (row.d ? 1 : 0) : ''}</td>
                  </tr>
                {/each}
              </tbody>
            </table>
          {/each}
        </div>
      </article>

      <article class="panel">
        <h2>Address translation</h2>
        {#if translation}
          {@const pageBits = Math.max(1, Math.ceil(Math.log2(Math.max(2, procs.find((p) => p.pid === translation.pid)?.pages ?? 2))))}
          <div class="tr">
            <div class="mono va">
              <span class="lbl">virtual</span>
              <span class="hi">{bits(translation.vpn, pageBits)}</span><span class="lo">{bits(translation.offset, offsetBits)}</span>
              <span class="dec">= {translation.va}</span>
            </div>
            <div class="mono split"><span class="hi">page {translation.vpn}</span><span class="lo">offset {translation.offset}</span></div>
            <div class="mono map">page {translation.vpn} <Icon name="arrow" size={14} /> frame {translation.frame} (page table)</div>
            <div class="mono va">
              <span class="lbl">physical</span>
              <span class="hi frame">{bits(translation.frame, Math.max(1, Math.ceil(Math.log2(Math.max(2, frames)))))}</span><span class="lo">{bits(translation.offset, offsetBits)}</span>
              <span class="dec">= {translation.pa}</span>
            </div>
            <p class="hint">With {pageSize}-byte pages the low {offsetBits} bits are the offset and pass through unchanged; only the page number is replaced by the frame number.</p>
          </div>
        {:else}
          <p class="hint">Use a virtual address such as <span class="mono">@6700</span> (try the “Virtual addresses” example) to see how the page number is replaced by a frame number while the offset is kept.</p>
        {/if}
      </article>
    </div>

    <article class="panel belady">
      <div class="card-head">
        <h2>Faults against frames</h2>
        {#if anomalies.length}<span class="bad tally-text">Belady's anomaly: with FIFO, {anomalies.map((f) => `${f} frames cause more faults than ${f - 1}`).join('; ')}</span>
        {:else}<span class="tally-text">No anomaly for these references</span>{/if}
      </div>
      <svg viewBox="0 0 {W} {H}" role="img" aria-label="Page faults for 1 to {MAX_SWEEP} frames under FIFO, LRU and OPT">
        {#each [0, 0.5, 1] as g}<line class="grid" x1={PAD.l} x2={W - PAD.r} y1={y(g * maxY)} y2={y(g * maxY)} /><text class="tick" x={PAD.l - 8} y={y(g * maxY) + 4} text-anchor="end">{Math.round(g * maxY)}</text>{/each}
        {#each Array(MAX_SWEEP) as _, i}<text class="tick" x={x(i + 1)} y={H - PAD.b + 18} text-anchor="middle">{i + 1}</text>{/each}
        <text class="axis" x={(W + PAD.l) / 2} y={H - 2} text-anchor="middle">frames</text>
        <line class="now" x1={x(frames)} x2={x(frames)} y1={PAD.t} y2={H - PAD.b} />
        {#each REPLACEMENT as p}
          {#if sweep[p]}
            <path class="series" class:fifo={p === 'fifo'} d={line(sweep[p])} stroke-dasharray={lineStyle[p]} />
            {#each sweep[p] as v, i}<circle class="pt-dot" class:fifo={p === 'fifo'} cx={x(i + 1)} cy={y(v)} r="3.5"><title>{POLICY_LABEL[p]}, {i + 1} frames: {v} faults</title></circle>{/each}
          {/if}
        {/each}
        {#each anomalies as f}<circle class="anomaly" cx={x(f)} cy={y(sweep.fifo[f - 1])} r="9" />{/each}
      </svg>
      <p class="legend mono"><span class="k fifo"></span>FIFO <span class="k lru"></span>LRU <span class="k opt"></span>OPT <span class="k now"></span>current frames</p>
    </article>
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
    max-width: 72ch;
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
    gap: 20px;
    padding: 20px;
  }
  .controls section {
    display: grid;
    gap: 8px;
    min-width: 0;
  }
  textarea.field {
    height: auto;
    padding: 8px 10px;
    resize: vertical;
    line-height: 1.5;
  }
  .gen {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 10px;
    margin-top: 6px;
  }
  .gen label {
    display: grid;
    gap: 6px;
  }
  .gen .span {
    grid-column: span 2;
  }
  .gen .btn {
    justify-content: center;
  }
  .two {
    display: grid;
    grid-template-columns: 1fr 1fr;
    gap: 12px;
  }
  .stepper {
    display: grid;
    grid-template-columns: 32px 1fr 32px;
    align-items: center;
    gap: 6px;
  }
  .stepper .btn {
    width: 32px;
    padding: 0;
    justify-content: center;
  }
  .count {
    text-align: center;
    font-size: 18px;
    font-weight: 600;
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
  .table-card,
  .belady,
  .lower > .panel {
    padding: 18px 20px;
    display: grid;
    gap: 12px;
    align-content: start;
    min-width: 0;
  }
  .card-head {
    display: flex;
    justify-content: space-between;
    align-items: baseline;
    gap: 16px;
    flex-wrap: wrap;
  }
  h2 {
    font-size: 15px;
  }
  .tally {
    display: flex;
    gap: 18px;
    margin: 0;
    font-size: 13px;
  }
  .tally div {
    display: flex;
    gap: 6px;
  }
  .tally dt {
    color: var(--faint);
  }
  .tally dd {
    margin: 0;
    font-weight: 600;
  }
  .tally-text {
    font-size: 13px;
    color: var(--muted);
  }
  .bad {
    color: var(--bad);
  }
  .ok {
    color: var(--ok);
  }
  .tally-text.bad {
    color: var(--bad);
  }
  .narration {
    font-size: 17px;
    max-width: 80ch;
    min-height: 3em;
  }
  .transport {
    display: flex;
    align-items: center;
    gap: 4px;
    padding: 8px;
    position: sticky;
    bottom: 12px;
    z-index: 2;
  }
  .transport .ghost {
    width: 36px;
    padding: 0;
    justify-content: center;
  }
  .pos {
    margin-left: auto;
    padding-right: 8px;
    font-size: 13px;
    color: var(--muted);
  }
  .lower {
    display: grid;
    grid-template-columns: minmax(0, 1fr) minmax(0, 1fr);
    gap: 16px;
    align-items: start;
  }
  .tables {
    display: flex;
    flex-wrap: wrap;
    gap: 20px;
  }
  .pt {
    border-collapse: collapse;
    font-size: 13px;
  }
  .pt caption {
    text-align: left;
    font-weight: 600;
    padding-bottom: 4px;
  }
  .pt th {
    color: var(--faint);
    font-weight: 500;
    padding: 2px 10px 4px 0;
    text-align: right;
  }
  .pt td {
    padding: 3px 10px 3px 0;
    text-align: right;
    color: var(--faint);
    border-top: 1px solid var(--rule);
  }
  .pt tr.present td {
    color: var(--text);
  }
  .pt td.dirty {
    color: var(--accent);
    font-weight: 600;
  }
  .tr {
    display: grid;
    gap: 10px;
  }
  .va {
    display: flex;
    align-items: baseline;
    gap: 2px;
    font-size: 16px;
  }
  .lbl {
    width: 72px;
    font-size: 12px;
    color: var(--faint);
  }
  .hi {
    padding: 2px 4px;
    border-radius: 4px 0 0 4px;
    background: color-mix(in srgb, var(--p0) 22%, transparent);
  }
  .hi.frame {
    background: color-mix(in srgb, var(--accent) 22%, transparent);
  }
  .lo {
    padding: 2px 4px;
    border-radius: 0 4px 4px 0;
    background: var(--raised);
    color: var(--muted);
  }
  .dec {
    margin-left: 10px;
    color: var(--muted);
    font-size: 14px;
  }
  .split {
    display: flex;
    gap: 12px;
    padding-left: 74px;
    font-size: 12px;
    color: var(--faint);
  }
  .map {
    display: flex;
    align-items: center;
    gap: 8px;
    padding-left: 74px;
    font-size: 13px;
    color: var(--muted);
  }
  svg {
    width: 100%;
    max-width: 640px;
    height: auto;
  }
  .grid {
    stroke: var(--rule);
  }
  .tick {
    fill: var(--faint);
    font: 12px var(--mono);
  }
  .axis {
    fill: var(--muted);
    font: 12px var(--sans);
  }
  .series {
    fill: none;
    stroke: var(--muted);
    stroke-width: 1.5;
  }
  .series.fifo {
    stroke: var(--text);
    stroke-width: 2;
  }
  .pt-dot {
    fill: var(--panel);
    stroke: var(--muted);
    stroke-width: 1.5;
  }
  .pt-dot.fifo {
    stroke: var(--text);
  }
  .anomaly {
    fill: none;
    stroke: var(--bad);
    stroke-width: 2;
  }
  .now {
    stroke: var(--accent);
    stroke-width: 1.5;
  }
  .legend {
    display: flex;
    gap: 16px;
    align-items: center;
    font-size: 12px;
    color: var(--muted);
  }
  .k {
    display: inline-block;
    width: 18px;
    height: 0;
    border-top: 2px solid var(--muted);
    margin-right: -10px;
  }
  .k.fifo {
    border-color: var(--text);
  }
  .k.lru {
    border-top-style: dashed;
  }
  .k.opt {
    border-top-style: dotted;
  }
  .k.now {
    border-color: var(--accent);
  }
  .error {
    padding: 20px;
    color: var(--bad);
  }
  @media (max-width: 1080px) {
    .lab,
    .lower {
      grid-template-columns: minmax(0, 1fr);
    }
    .controls {
      position: static;
    }
  }
</style>
