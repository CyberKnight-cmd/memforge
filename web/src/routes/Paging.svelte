<script lang="ts">
  import FrameTable from '../components/FrameTable.svelte';
  import Tokens from '../components/Tokens.svelte';
  import Segmented from '../components/Segmented.svelte';
  import Icon from '../components/Icon.svelte';
  import { run, pagingText, narrate, REPLACEMENT, POLICY_LABEL, AC3_REFS, AC4_REFS, type Run } from '../lib/sim';

  const presets = [
    { key: 'ac3', label: 'Textbook string, 3 frames', refs: AC3_REFS, frames: 3 },
    { key: 'ac4', label: "Belady's string, 3 frames", refs: AC4_REFS, frames: 3 },
  ];
  let refsText = $state(AC3_REFS.join(' '));
  let frames = $state(3);
  let mode = $state<string>('fifo');
  let runs = $state<Record<string, Run>>({});
  let sweep = $state<Record<string, number[]>>({});
  let cursor = $state<number | null>(null);
  let playing = $state(false);
  const MAX_SWEEP = 7;

  const refs = $derived(
    refsText
      .split(/[\s,]+/)
      .filter(Boolean)
      .map(Number)
      .filter((v) => Number.isInteger(v) && v >= 0 && v < 64),
  );
  const policies = $derived(mode === 'compare' ? [...REPLACEMENT] : [mode]);

  $effect(() => {
    const text = pagingText(frames, refs);
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
    Promise.all(
      REPLACEMENT.map(async (p) => {
        const out: number[] = [];
        for (let f = 1; f <= MAX_SWEEP; f++) {
          const res = await run(pagingText(f, r), p);
          out.push(res.ok ? res.summary.faults : 0);
        }
        return [p, out] as const;
      }),
    ).then((pairs) => (sweep = Object.fromEntries(pairs)));
  });

  const primary = $derived(runs[policies[0]]);
  const steps = $derived(primary?.ok ? primary.steps : []);
  const last = $derived(Math.max(1, steps.length - 1));
  const at = $derived(cursor === null ? last : Math.min(cursor, last));
  const go = (i: number) => (cursor = i >= last ? null : Math.max(1, i));

  $effect(() => {
    if (!playing) return;
    if (cursor === null) cursor = 1;
    const t = setInterval(() => {
      if (cursor === null || cursor >= last) {
        playing = false;
        cursor = null;
      } else cursor += 1;
    }, 650);
    return () => clearInterval(t);
  });

  function onKey(e: KeyboardEvent) {
    if ((e.target as HTMLElement).closest('input, select, textarea')) return;
    if (e.key === 'ArrowLeft') go(at - 1);
    else if (e.key === 'ArrowRight') go(at + 1);
    else if (e.key === ' ') playing = !playing;
    else return;
    e.preventDefault();
  }

  const faultsUpTo = (r: Run | undefined) => (r?.ok ? r.steps.slice(0, at + 1).filter((s) => s.events.some((e) => e.type === 'PageFault')).length : 0);
  const anomalies = $derived((sweep.fifo ?? []).flatMap((v, i, a) => (i > 0 && v > a[i - 1] ? [i + 1] : [])));

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
  <p class="lede">Type a reference string, choose how many frames the process gets, and watch the replacement policy decide which page to evict. Each column is computed by the real engine.</p>
</header>

<div class="lab">
  <aside class="panel controls" aria-label="Controls">
    <section>
      <label class="label" for="preset">Example</label>
      <select id="preset" class="field" onchange={(e) => { const p = presets.find((x) => x.key === (e.target as HTMLSelectElement).value)!; refsText = p.refs.join(' '); frames = p.frames; cursor = null; }}>
        {#each presets as p}<option value={p.key}>{p.label}</option>{/each}
      </select>
    </section>
    <section>
      <label class="label" for="refs">Reference string (page numbers)</label>
      <input id="refs" class="field mono" bind:value={refsText} oninput={() => (cursor = null)} />
    </section>
    <section>
      <span class="label">Frames</span>
      <div class="stepper">
        <button class="btn" onclick={() => { frames = Math.max(1, frames - 1); cursor = null; }} aria-label="Fewer frames" disabled={frames <= 1}>−</button>
        <span class="mono count" aria-live="polite">{frames}</span>
        <button class="btn" onclick={() => { frames = Math.min(8, frames + 1); cursor = null; }} aria-label="More frames" disabled={frames >= 8}>+</button>
      </div>
    </section>
    <section>
      <span class="label">Replacement policy</span>
      <Segmented label="Replacement policy" bind:value={mode}
        options={[...REPLACEMENT.map((p) => ({ value: p, label: POLICY_LABEL[p] })), { value: 'compare', label: 'All 3' }]} />
      <p class="hint">
        {#if mode === 'fifo'}Evicts the page that was loaded earliest.
        {:else if mode === 'lru'}Evicts the page that was used least recently.
        {:else if mode === 'opt'}Evicts the page needed furthest in the future. Unrealisable, but the lower bound.
        {:else}Runs the same string under all three policies.{/if}
      </p>
    </section>
  </aside>

  <section class="stage">
    {#each policies as p (p)}
      {@const r = runs[p]}
      {#if r?.ok}
        {@const st = r.steps[Math.min(at, r.steps.length - 1)]}
        <article class="panel table-card">
          <div class="card-head">
            <h2>{POLICY_LABEL[p]}</h2>
            <span class="mono tally"><strong class="bad">{faultsUpTo(r)}</strong> faults so far · {r.summary.faults} in total</span>
          </div>
          <FrameTable steps={r.steps} {refs} {frames} {at} />
          <p class="narration"><Tokens toks={at <= 1 ? [{ t: 'All frames are empty. Press play or → to make the first reference.' }] : narrate(st.events, p)} colorOf={() => 'var(--text)'} /></p>
        </article>
      {:else if r && !r.ok}
        <p class="panel error">{r.error}</p>
      {/if}
    {/each}

    <div class="transport panel">
      <button class="btn ghost" onclick={() => go(1)} aria-label="Back to start"><Icon name="start" /></button>
      <button class="btn ghost" onclick={() => go(at - 1)} aria-label="Previous reference"><Icon name="back" /></button>
      <button class="btn primary" onclick={() => (playing = !playing)} aria-label={playing ? 'Pause' : 'Play'}><Icon name={playing ? 'pause' : 'play'} /> {playing ? 'Pause' : 'Play'}</button>
      <button class="btn ghost" onclick={() => go(at + 1)} aria-label="Next reference"><Icon name="fwd" /></button>
      <button class="btn ghost" onclick={() => go(last)} aria-label="Jump to end"><Icon name="end" /></button>
      <span class="mono pos">reference {Math.max(0, at - 1)} of {refs.length}</span>
    </div>

    <article class="panel belady">
      <div class="card-head">
        <h2>Faults against frames</h2>
        {#if anomalies.length}<span class="bad tally">Belady's anomaly: FIFO gets worse with more frames at {anomalies.join(', ')}</span>
        {:else}<span class="tally">No anomaly for this string</span>{/if}
      </div>
      <svg viewBox="0 0 {W} {H}" role="img" aria-label="Page faults for 1 to {MAX_SWEEP} frames under FIFO, LRU and OPT">
        {#each [0, 0.5, 1] as g}<line class="grid" x1={PAD.l} x2={W - PAD.r} y1={y(g * maxY)} y2={y(g * maxY)} /><text class="tick" x={PAD.l - 8} y={y(g * maxY) + 4} text-anchor="end">{Math.round(g * maxY)}</text>{/each}
        {#each Array(MAX_SWEEP) as _, i}<text class="tick" x={x(i + 1)} y={H - PAD.b + 18} text-anchor="middle">{i + 1}</text>{/each}
        <text class="axis" x={(W + PAD.l) / 2} y={H - 2} text-anchor="middle">frames</text>
        {#each REPLACEMENT as p}
          {#if sweep[p]}
            <path class="series" class:fifo={p === 'fifo'} d={line(sweep[p])} stroke-dasharray={lineStyle[p]} />
            {#each sweep[p] as v, i}<circle class="pt" class:fifo={p === 'fifo'} cx={x(i + 1)} cy={y(v)} r="3.5"><title>{POLICY_LABEL[p]}, {i + 1} frames: {v} faults</title></circle>{/each}
          {/if}
        {/each}
        {#each anomalies as f}<circle class="anomaly" cx={x(f)} cy={y(sweep.fifo[f - 1])} r="9" />{/each}
        <line class="now" x1={x(frames)} x2={x(frames)} y1={PAD.t} y2={H - PAD.b} />
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
  .controls section {
    display: grid;
    gap: 8px;
  }
  .stepper {
    display: grid;
    grid-template-columns: 36px 1fr 36px;
    align-items: center;
    gap: 8px;
  }
  .stepper .btn {
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
  .belady {
    padding: 18px 20px;
    display: grid;
    gap: 12px;
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
    font-size: 13px;
    color: var(--muted);
  }
  .bad {
    color: var(--bad);
  }
  .narration {
    font-size: 17px;
    max-width: 80ch;
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
  .pt {
    fill: var(--panel);
    stroke: var(--muted);
    stroke-width: 1.5;
  }
  .pt.fifo {
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
    .lab {
      grid-template-columns: minmax(0, 1fr);
    }
    .controls {
      position: static;
    }
  }
</style>
