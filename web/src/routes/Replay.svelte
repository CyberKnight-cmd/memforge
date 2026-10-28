<script lang="ts">
  import MemoryStrip from '../components/MemoryStrip.svelte';
  import FrameTable from '../components/FrameTable.svelte';
  import Tokens from '../components/Tokens.svelte';
  import Icon from '../components/Icon.svelte';
  import { toSteps, narrate, POLICY_LABEL, type Run, type Ref } from '../lib/sim';

  // Replays a trace written by `memsim run --trace FILE`, the same canonical format the labs use.
  let header = $state<any>(null);
  let trace = $state<Run | null>(null);
  let error = $state('');
  let name = $state('');
  let cursor = $state(0);
  let dragging = $state(false);

  async function open(file: File) {
    error = '';
    name = file.name;
    try {
      const lines = (await file.text()).trim().split('\n').map((l, i) => {
        try {
          return JSON.parse(l);
        } catch {
          throw new Error(`line ${i + 1} is not valid JSON`);
        }
      });
      if (lines[0]?.type !== 'TraceHeader') throw new Error('the first line is not a TraceHeader');
      if (lines[0].schema !== 1) throw new Error(`schema ${lines[0].schema} is not supported`);
      header = lines[0];
      trace = toSteps(lines);
      cursor = 0;
    } catch (e) {
      error = `This is not a MemForge trace: ${(e as Error).message}.`;
      trace = null;
    }
  }

  const steps = $derived(trace?.ok ? trace.steps : []);
  const paging = $derived(!!header?.config?.frames);
  const last = $derived(Math.max(0, steps.length - 1));
  const at = $derived(Math.min(cursor, last));
  // Paging references are recovered from the hit and fault events of each step.
  const refs = $derived(
    steps.flatMap((st): Ref[] => {
      const e = st.events.find((x) => x.type === 'PageHit' || x.type === 'PageFault')?.data;
      return e ? [{ pid: e.pid, vpn: e.vpn, va: null, write: e.write }] : [];
    }),
  );
  const firstAccess = $derived(Math.max(0, steps.findIndex((st) => st.events.some((x) => x.type === 'PageHit' || x.type === 'PageFault'))));
  const palette = ['var(--p0)', 'var(--p1)', 'var(--p2)', 'var(--p3)', 'var(--p4)', 'var(--p5)'];
  const colorOf = (p: string) => (p === 'SYS' ? 'var(--sys)' : palette[(Number(p.replace(/\D/g, '')) || 0) % palette.length]);

  function onKey(e: KeyboardEvent) {
    if (!steps.length) return;
    if (e.key === 'ArrowLeft') cursor = Math.max(0, at - 1);
    else if (e.key === 'ArrowRight') cursor = Math.min(last, at + 1);
    else if (e.key === 'Home') cursor = 0;
    else if (e.key === 'End') cursor = last;
    else return;
    e.preventDefault();
  }
</script>

<svelte:window onkeydown={onKey} />

<header class="head">
  <h1>Replay a trace</h1>
  <p class="lede">Open a trace written by <span class="mono">memsim run W --policy P --trace out.jsonl</span> and step through it. Nothing is re-simulated: what you see is the file.</p>
</header>

<label class="panel drop" class:dragging
  ondragover={(e) => { e.preventDefault(); dragging = true; }}
  ondragleave={() => (dragging = false)}
  ondrop={(e) => { e.preventDefault(); dragging = false; const f = e.dataTransfer?.files[0]; if (f) open(f); }}>
  <input type="file" accept=".jsonl,.json,application/json" onchange={(e) => { const f = (e.target as HTMLInputElement).files?.[0]; if (f) open(f); }} />
  <span class="btn">Choose a trace</span>
  <span class="label">{name ? name : 'or drop a .jsonl file here'}</span>
</label>
{#if error}<p class="err">{error}</p>{/if}

{#if trace?.ok && header}
  {@const st = steps[at]}
  <article class="panel view">
    <div class="meta">
      <h2>{header.workload}</h2>
      <span class="mono">{POLICY_LABEL[header.policy] ?? header.policy}</span>
      <span class="mono faint">{paging ? `${header.config.frames} frames · ${header.config.page_size} B pages` : `${header.config.memory_size} units`}</span>
      <span class="mono faint">engine {header.engine} · {header.git}</span>
      <span class="mono step">step {st.step} of {last}</span>
    </div>
    {#if paging}
      <FrameTable {steps} {refs} frames={header.config.frames} {at} {firstAccess} {colorOf} />
    {:else}
      <MemoryStrip blocks={st.blocks} memorySize={header.config.memory_size} events={st.events} policy={header.policy} {colorOf} />
    {/if}
    <p class="narration"><Tokens toks={narrate(st.events, header.policy)} {colorOf} /></p>
    <div class="transport">
      <button class="btn ghost" onclick={() => (cursor = 0)} aria-label="First step"><Icon name="start" /></button>
      <button class="btn ghost" onclick={() => (cursor = Math.max(0, at - 1))} aria-label="Previous step"><Icon name="back" /></button>
      <input type="range" min="0" max={last} bind:value={cursor} aria-label="Step" />
      <button class="btn ghost" onclick={() => (cursor = Math.min(last, at + 1))} aria-label="Next step"><Icon name="fwd" /></button>
      <button class="btn ghost" onclick={() => (cursor = last)} aria-label="Last step"><Icon name="end" /></button>
    </div>
    <details>
      <summary class="label">Raw events of this step ({st.events.length})</summary>
      <pre class="mono">{st.events.map((e) => JSON.stringify(e)).join('\n') || 'none'}</pre>
    </details>
    <details>
      <summary class="label">Run summary</summary>
      <table class="mono sum">
        <tbody>{#each Object.entries(trace.summary) as [k, v]}<tr><th>{k}</th><td>{v}</td></tr>{/each}</tbody>
      </table>
    </details>
  </article>
{/if}

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
  .drop {
    display: flex;
    align-items: center;
    gap: 14px;
    padding: 22px;
    border-style: dashed;
    cursor: pointer;
  }
  .drop.dragging {
    border-color: var(--accent);
  }
  .drop input {
    position: absolute;
    width: 1px;
    height: 1px;
    opacity: 0;
  }
  .drop:focus-within {
    outline: 2px solid var(--accent);
    outline-offset: 2px;
  }
  .err {
    margin-top: 12px;
    color: var(--bad);
  }
  .view {
    margin-top: 16px;
    padding: 18px 20px;
    display: grid;
    gap: 14px;
    min-width: 0;
  }
  .meta {
    display: flex;
    flex-wrap: wrap;
    align-items: baseline;
    gap: 14px;
    font-size: 13px;
  }
  .meta h2 {
    font-size: 16px;
  }
  .faint {
    color: var(--faint);
  }
  .step {
    margin-left: auto;
    color: var(--muted);
  }
  .narration {
    font-size: 17px;
  }
  .transport {
    display: grid;
    grid-template-columns: auto auto 1fr auto auto;
    align-items: center;
    gap: 4px;
  }
  .transport .ghost {
    width: 36px;
    padding: 0;
    justify-content: center;
  }
  input[type='range'] {
    accent-color: var(--accent);
    width: 100%;
  }
  pre {
    margin: 8px 0 0;
    padding: 12px;
    border-radius: 8px;
    background: var(--ground);
    font-size: 12px;
    overflow-x: auto;
    white-space: pre;
  }
  .sum {
    margin-top: 8px;
    font-size: 13px;
    border-collapse: collapse;
  }
  .sum th {
    text-align: left;
    font-weight: 400;
    color: var(--muted);
    padding: 2px 16px 2px 0;
  }
</style>
