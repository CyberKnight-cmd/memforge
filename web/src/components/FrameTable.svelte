<script lang="ts">
  import type { Step } from '../lib/sim';
  // The textbook frames x references table. Column j is the state after reference j.
  let { steps, refs, frames, at }: { steps: Step[]; refs: number[]; frames: number; at: number } = $props();

  // steps[0] is the initial state, steps[1] the spawn; reference j is step j + 2.
  const cols = $derived(
    refs.map((ref, j) => {
      const st = steps[j + 2];
      if (!st) return { ref, cells: [], fault: false, evicted: null as number | null, loadedFrame: -1, hitFrame: -1 };
      const fault = st.events.find((e) => e.type === 'PageFault');
      const ev = st.events.find((e) => e.type === 'PageEvicted')?.data;
      const loaded = st.events.find((e) => e.type === 'PageLoaded')?.data;
      const hit = st.events.find((e) => e.type === 'PageHit')?.data;
      return {
        ref,
        cells: st.frames.map((f) => f.vpn),
        fault: !!fault,
        evicted: ev ? ev.vpn : null,
        loadedFrame: loaded ? loaded.frame : -1,
        hitFrame: hit ? hit.frame : -1,
      };
    }),
  );
  const current = $derived(at - 2);
  let scroller: HTMLDivElement;
  $effect(() => {
    const el = scroller?.querySelector<HTMLElement>(`[data-col="${current}"]`);
    el?.scrollIntoView({ block: 'nearest', inline: 'center', behavior: 'smooth' });
  });
</script>

<div class="scroller" bind:this={scroller}>
  <table>
    <thead>
      <tr>
        <th scope="row" class="side">reference</th>
        {#each cols as c, j}<th scope="col" class="mono ref" class:now={j === current} class:future={j > current} data-col={j}>{c.ref}</th>{/each}
      </tr>
    </thead>
    <tbody>
      {#each Array(frames) as _, f}
        <tr>
          <th scope="row" class="side">frame {f}</th>
          {#each cols as c, j}
            {@const v = c.cells[f]}
            <td class="mono" class:now={j === current} class:future={j > current}
              class:loaded={j <= current && c.loadedFrame === f} class:hit={j <= current && c.hitFrame === f}>
              {#if j <= current && v !== null && v !== undefined}<span class="v">{v}</span>{/if}
            </td>
          {/each}
        </tr>
      {/each}
      <tr class="faults">
        <th scope="row" class="side">fault</th>
        {#each cols as c, j}
          <td class="mono" class:now={j === current} class:future={j > current}>
            {#if j <= current && c.fault}<span class="f" title={c.evicted !== null ? `evicted page ${c.evicted}` : 'loaded into a free frame'}>F</span>{#if c.evicted !== null}<span class="out">−{c.evicted}</span>{/if}{/if}
          </td>
        {/each}
      </tr>
    </tbody>
  </table>
</div>

<style>
  .scroller {
    overflow-x: auto;
    padding-bottom: 6px;
  }
  table {
    border-collapse: separate;
    border-spacing: 3px;
    font-size: 15px;
  }
  .side {
    position: sticky;
    left: 0;
    z-index: 1;
    background: var(--panel);
    padding-right: 10px;
    text-align: left;
    font-weight: 500;
    font-size: 13px;
    color: var(--faint);
    white-space: nowrap;
  }
  .ref {
    width: 38px;
    min-width: 38px;
    height: 30px;
    color: var(--muted);
    font-weight: 600;
    border-bottom: 2px solid transparent;
  }
  .ref.now {
    color: var(--text);
    border-bottom-color: var(--accent);
  }
  td {
    width: 38px;
    height: 38px;
    text-align: center;
    border-radius: 6px;
    background: var(--ground);
    border: 1px solid var(--rule);
    transition: background 200ms var(--ease), opacity 200ms var(--ease);
  }
  td.future,
  th.future {
    opacity: 0.35;
  }
  td.now {
    border-color: var(--rule-strong);
  }
  .v {
    display: inline-block;
    animation: drop 260ms var(--ease);
  }
  td.loaded {
    background: color-mix(in srgb, var(--bad) 16%, var(--ground));
    border-color: var(--bad);
    font-weight: 600;
  }
  td.hit {
    background: color-mix(in srgb, var(--ok) 14%, var(--ground));
    border-color: var(--ok);
  }
  .faults td {
    background: transparent;
    border-color: transparent;
    height: 30px;
  }
  .f {
    color: var(--bad);
    font-weight: 700;
  }
  .out {
    display: block;
    font-size: 11px;
    color: var(--faint);
    line-height: 1;
  }
  @keyframes drop {
    from {
      transform: translateY(-8px);
      opacity: 0;
    }
  }
</style>
