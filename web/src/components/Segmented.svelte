<script lang="ts">
  let { options, value = $bindable(), label }: { options: { value: string; label: string }[]; value: string; label: string } = $props();
</script>

<div class="seg" role="radiogroup" aria-label={label}>
  {#each options as o}
    <button role="radio" aria-checked={value === o.value} class:on={value === o.value} onclick={() => (value = o.value)}>{o.label}</button>
  {/each}
</div>

<style>
  .seg {
    display: inline-grid;
    grid-auto-flow: column;
    grid-auto-columns: 1fr;
    padding: 3px;
    gap: 2px;
    border: 1px solid var(--rule-strong);
    border-radius: 9px;
    background: var(--ground);
  }
  button {
    border: 0;
    background: transparent;
    padding: 6px 10px;
    border-radius: 6px;
    color: var(--muted);
    font-weight: 500;
    font-size: 14px;
    cursor: pointer;
    white-space: nowrap;
    transition: background 160ms var(--ease), color 160ms var(--ease);
  }
  button:hover {
    color: var(--text);
  }
  button.on {
    background: var(--raised);
    color: var(--text);
    box-shadow: 0 1px 2px rgb(0 0 0 / 0.25);
  }
</style>
