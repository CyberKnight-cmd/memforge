<script lang="ts">
  import type { Block } from '../lib/sim';
  import type { TraceLine } from '../lib/engine';
  import { fade } from 'svelte/transition';

  let {
    blocks,
    memorySize,
    events = [],
    policy,
    colorOf,
    onBlockClick,
    compact = false,
  }: {
    blocks: Block[];
    memorySize: number;
    events?: TraceLine[];
    policy: string;
    colorOf: (pid: string) => string;
    onBlockClick?: (b: Block) => void;
    compact?: boolean;
  } = $props();

  const pct = (v: number) => `${(v / memorySize) * 100}%`;
  const ev = (t: string) => events.find((e) => e.type === t)?.data;

  const placed = $derived(ev('PlacementDecided'));
  const failed = $derived(ev('AllocationFailed'));
  const allocated = $derived(ev('BlockAllocated'));
  const compacting = $derived(!!ev('CompactionStarted'));
  const holes = $derived(blocks.filter((b) => b.state === 'free'));

  // Which holes the policy examined, in the order it examined them. The layout shown is after the
  // step, so a placed block's hole is reconstructed as block + its remainder.
  const scan = $derived.by(() => {
    if (!placed) return [] as { start: number; size: number; chosen: boolean }[];
    let before = holes.map((h) => ({ start: h.start, size: h.size }));
    if (allocated && placed.hole) {
      before = before.filter((h) => h.start !== placed.hole.start + allocated.size);
      before.push({ start: placed.hole.start, size: placed.hole.size });
      before.sort((a, b) => a.start - b.start);
    }
    const examined = policy === 'first-fit' ? before.slice(0, placed.scanned) : before;
    return examined.map((h) => ({ ...h, chosen: !!placed.hole && h.start === placed.hole.start }));
  });

  const ticks = $derived([0, 0.25, 0.5, 0.75, 1].map((f) => Math.round(f * memorySize)));
  let hover = $state<Block | null>(null);
</script>

<div class="wrap" class:compact>
  {#if failed}
    <div class="request" style:width={pct(failed.requested)} transition:fade={{ duration: 200 }}>
      <span class="mono">{failed.pid} needs {failed.requested}</span>
    </div>
  {/if}

  <div class="strip" role="group" aria-label="Physical memory, addresses 0 to {memorySize}">
    {#each blocks.filter((b) => b.state === 'free') as b (`free-${b.start}-${b.size}`)}
      <div class="hole" class:fail={!!failed} style:left={pct(b.start)} style:width={pct(b.size)} transition:fade={{ duration: 260 }}
        role="img" aria-label="Free hole of {b.size} at {b.start}"
        onpointerenter={() => (hover = b)} onpointerleave={() => (hover = null)}>
        {#if b.size / memorySize > 0.05}<span class="mono size">{b.size}</span>{/if}
      </div>
    {/each}
    {#each blocks.filter((b) => b.state === 'allocated') as b (b.id)}
      <button
        class="block"
        class:fresh={allocated && allocated.id === b.id}
        class:moving={compacting}
        style:left={pct(b.start)}
        style:width={pct(b.size)}
        style:--c={colorOf(b.pid!)}
        aria-label="{b.id}, {b.size} units at {b.start}, owned by {b.pid}{onBlockClick ? '. Click to free.' : ''}"
        disabled={!onBlockClick}
        onclick={() => onBlockClick?.(b)}
        onpointerenter={() => (hover = b)}
        onpointerleave={() => (hover = null)}
        in:fade={{ duration: 220 }}
        out:fade={{ duration: 220 }}
      >
        {#if b.size / memorySize > 0.045}<span class="mono">{b.id}</span>{/if}
      </button>
    {/each}
    {#each scan as h, i (`scan-${h.start}`)}
      <div class="scan" class:chosen={h.chosen} style:left={pct(h.start)} style:width={pct(h.size)} style:--i={i} aria-hidden="true"></div>
    {/each}
  </div>

  {#if !compact}
    <div class="ruler mono" aria-hidden="true">
      {#each ticks as t}<span style:left={pct(t)}>{t}</span>{/each}
    </div>
  {/if}

  <div class="readout mono" aria-live="polite">
    {#if hover}
      {#if hover.state === 'free'}
        hole · start {hover.start} · size {hover.size} · end {hover.start + hover.size}
      {:else}
        {hover.id} · {hover.pid} · start {hover.start} · size {hover.size} · end {hover.start + hover.size}
      {/if}
    {:else if failed}
      <span class="bad">free {failed.total_free} · largest hole {failed.largest_hole} · request {failed.requested}</span>
    {:else}
      &nbsp;
    {/if}
  </div>
</div>

<style>
  .wrap {
    position: relative;
    padding-top: 30px;
  }
  .wrap.compact {
    padding-top: 26px;
  }
  .strip {
    position: relative;
    height: 84px;
    border-radius: 8px;
    background: var(--ground);
    border: 1px solid var(--rule-strong);
    overflow: hidden;
  }
  .compact .strip {
    height: 56px;
  }
  .hole,
  .block,
  .scan {
    position: absolute;
    top: 0;
    bottom: 0;
  }
  .hole {
    background: repeating-linear-gradient(135deg, var(--hatch) 0 1px, transparent 1px 7px);
    display: grid;
    place-items: center;
    transition: left 520ms var(--ease), width 520ms var(--ease);
  }
  .hole.fail {
    box-shadow: inset 0 0 0 2px var(--bad);
  }
  .size {
    font-size: 12px;
    color: var(--faint);
    background: var(--ground);
    padding: 0 4px;
    border-radius: 3px;
  }
  .block {
    border: 0;
    padding: 0;
    margin: 0;
    font: inherit;
    background: color-mix(in srgb, var(--c) 82%, var(--ground));
    box-shadow:
      inset 1px 0 0 color-mix(in srgb, var(--c) 40%, #000),
      inset -1px 0 0 color-mix(in srgb, var(--c) 40%, #000);
    color: var(--ground);
    display: grid;
    place-items: center;
    font-size: 12px;
    font-weight: 600;
    overflow: hidden;
    cursor: default;
    transition:
      left 520ms var(--ease),
      width 520ms var(--ease),
      filter 160ms var(--ease);
  }
  :global([data-theme='light']) .block {
    color: #fff;
  }
  .block:not(:disabled) {
    cursor: pointer;
  }
  .block:not(:disabled):hover {
    filter: brightness(1.12);
  }
  .block.moving {
    transition-duration: 700ms;
  }
  .block.fresh {
    animation: drop 420ms var(--ease);
  }
  @keyframes drop {
    from {
      transform: translateY(-36px);
      opacity: 0;
    }
  }
  .scan {
    pointer-events: none;
    box-shadow: inset 0 0 0 2px var(--accent);
    opacity: 0;
    animation: scan 380ms var(--ease) forwards;
    animation-delay: calc(var(--i) * 260ms);
  }
  .scan:not(.chosen) {
    animation-name: scan-pass;
  }
  @keyframes scan {
    to {
      opacity: 1;
    }
  }
  @keyframes scan-pass {
    50% {
      opacity: 1;
    }
    to {
      opacity: 0.25;
    }
  }
  .request {
    position: absolute;
    top: 0;
    left: 0;
    height: 22px;
    border: 1.5px dashed var(--bad);
    border-radius: 5px;
    color: var(--bad);
    font-size: 12px;
    display: flex;
    align-items: center;
    padding: 0 8px;
    white-space: nowrap;
    animation: shake 420ms var(--ease) 1;
  }
  @keyframes shake {
    25% {
      transform: translateX(-4px);
    }
    75% {
      transform: translateX(4px);
    }
  }
  .ruler {
    position: relative;
    height: 22px;
    font-size: 12px;
    color: var(--faint);
  }
  .ruler span {
    position: absolute;
    top: 4px;
    transform: translateX(-50%);
  }
  .ruler span:first-child {
    transform: none;
  }
  .ruler span:last-child {
    transform: translateX(-100%);
  }
  .readout {
    margin-top: 6px;
    min-height: 20px;
    font-size: 13px;
    color: var(--muted);
  }
  .bad {
    color: var(--bad);
  }
</style>
