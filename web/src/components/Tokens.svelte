<script lang="ts">
  import type { Tok } from '../lib/sim';
  let { toks, colorOf }: { toks: Tok[]; colorOf: (pid: string) => string } = $props();
</script>

<span class="toks">{#each toks as t}{#if t.k === 'num'}<span class="mono num">{t.t}</span>{:else if t.k === 'id'}<span class="mono id" style:--c={t.pid ? colorOf(t.pid) : 'var(--text)'}>{t.t}</span>{:else if t.k === 'bad'}<strong class="bad">{t.t}</strong>{:else if t.k === 'ok'}<strong class="ok">{t.t}</strong>{:else}{t.t}{/if}{/each}</span>

<style>
  .num {
    font-weight: 600;
  }
  .id {
    color: var(--c);
    font-weight: 600;
  }
  .bad {
    color: var(--bad);
    font-weight: 600;
  }
  .ok {
    color: var(--ok);
    font-weight: 600;
  }
</style>
