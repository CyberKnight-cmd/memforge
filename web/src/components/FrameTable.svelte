<script lang="ts">
  import type { Step, Ref } from '../lib/sim';
  // The textbook frames x references table. Column j is memory after reference j.
  let {
    steps,
    refs,
    frames,
    at,
    firstAccess,
    colorOf,
  }: { steps: Step[]; refs: Ref[]; frames: number; at: number; firstAccess: number; colorOf: (pid: string) => string } = $props();

  const multi = $derived(new Set(refs.map((r) => r.pid)).size > 1);
  const cols = $derived(
    refs.map((ref, j) => {
      const st = steps[firstAccess + j];
      const ev = (t: string) => st?.events.find((e) => e.type === t)?.data;
      return {
        ref,
        cells: st ? st.frames : [],
        fault: !!ev('PageFault'),
        evicted: ev('PageEvicted') ?? null,
        writeback: !!ev('PageWrittenBack'),
        loadedFrame: ev('PageLoaded')?.frame ?? -1,
        hitFrame: ev('PageHit')?.frame ?? -1,
        violation: !!ev('AccessViolation'),
      };
    }),
  );
  const current = $derived(at - firstAccess);
  let scroller: HTMLDivElement;
  $effect(() => {
    const el = scroller?.querySelector<HTMLElement>(`[data-col="${current}"]`);
    el?.scrollIntoView({ block: 'nearest', inline: 'center', behavior: 'smooth' });
  });
</script>

<div class="scroller" bind:this={scroller}>
  <table class:multi>
    <thead>
      <tr>
        <th scope="row" class="side">reference</th>
        {#each cols as c, j}
          <th scope="col" class="mono ref" class:now={j === current} class:future={j > current} data-col={j}>
            {#if multi}<span class="pid" style:color={colorOf(c.ref.pid)}>{c.ref.pid}</span>{/if}
            <span>{c.ref.vpn}</span>{#if c.ref.write}<span class="w" title="write">w</span>{/if}
          </th>
        {/each}
      </tr>
    </thead>
    <tbody>
      {#each Array(frames) as _, f}
        <tr>
          <th scope="row" class="side">frame {f}</th>
          {#each cols as c, j}
            {@const cell = c.cells[f]}
            <td class="mono" class:now={j === current} class:future={j > current}
              class:loaded={j <= current && c.loadedFrame === f} class:hit={j <= current && c.hitFrame === f}>
              {#if j <= current && cell && cell.pid !== null}
                <span class="v" style:color={multi ? colorOf(cell.pid) : undefined}>{multi ? `${cell.pid.replace('P', '')}:` : ''}{cell.vpn}</span>
                {#if cell.dirty}<span class="dirty" title="modified (dirty)"></span>{/if}
              {/if}
            </td>
          {/each}
        </tr>
      {/each}
      <tr class="faults">
        <th scope="row" class="side">fault</th>
        {#each cols as c, j}
          <td class="mono" class:now={j === current} class:future={j > current}>
            {#if j <= current && c.violation}<span class="f">rej</span>
            {:else if j <= current && c.fault}
              <span class="f">F</span>
              {#if c.evicted}<span class="out">−{multi ? `${c.evicted.pid.replace('P', '')}:` : ''}{c.evicted.vpn}</span>{/if}
              {#if c.writeback}<span class="wb" title="dirty victim written back to disk">WB</span>{/if}
            {/if}
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
    min-width: 40px;
    height: 34px;
    color: var(--muted);
    font-weight: 600;
    border-bottom: 2px solid transparent;
    line-height: 1.1;
    white-space: nowrap;
  }
  .multi .ref {
    min-width: 46px;
  }
  .ref .pid {
    display: block;
    font-size: 11px;
  }
  .ref .w {
    color: var(--accent);
    font-size: 12px;
  }
  .ref.now {
    color: var(--text);
    border-bottom-color: var(--accent);
  }
  td {
    position: relative;
    min-width: 40px;
    height: 40px;
    text-align: center;
    border-radius: 6px;
    background: var(--ground);
    border: 1px solid var(--rule);
    transition: background 200ms var(--ease), opacity 200ms var(--ease);
  }
  .multi td {
    min-width: 46px;
  }
  td.future,
  th.future {
    opacity: 0.3;
  }
  td.now {
    border-color: var(--rule-strong);
  }
  .v {
    display: inline-block;
    animation: drop 260ms var(--ease);
  }
  .dirty {
    position: absolute;
    top: 4px;
    right: 4px;
    width: 6px;
    height: 6px;
    border-radius: 50%;
    background: var(--accent);
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
    height: 44px;
    vertical-align: top;
    padding-top: 4px;
  }
  .f {
    color: var(--bad);
    font-weight: 700;
  }
  .out,
  .wb {
    display: block;
    font-size: 11px;
    line-height: 1.2;
  }
  .out {
    color: var(--faint);
  }
  .wb {
    color: var(--accent);
    font-weight: 600;
  }
  @keyframes drop {
    from {
      transform: translateY(-8px);
      opacity: 0;
    }
  }
</style>
