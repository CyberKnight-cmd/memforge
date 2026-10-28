<script lang="ts">
  // One stroke icon set, 16px, 1.5px stroke, drawn for this app.
  let { name, size = 16 }: { name: string; size?: number } = $props();
  const paths: Record<string, string> = {
    sun: 'M8 2.5v1.2M8 12.3v1.2M2.5 8h1.2M12.3 8h1.2M4.1 4.1l.85.85M11.05 11.05l.85.85M4.1 11.9l.85-.85M11.05 4.95l.85-.85M10.5 8a2.5 2.5 0 1 1-5 0 2.5 2.5 0 0 1 5 0Z',
    moon: 'M13 9.6A5.5 5.5 0 0 1 6.4 3a5.5 5.5 0 1 0 6.6 6.6Z',
    play: 'M5 3.5v9l7-4.5-7-4.5Z',
    pause: 'M5.5 3.5v9M10.5 3.5v9',
    back: 'M10 3.5 5.5 8l4.5 4.5',
    fwd: 'M6 3.5 10.5 8 6 12.5',
    start: 'M4 3.5v9M11.5 3.5 7 8l4.5 4.5',
    end: 'M12 3.5v9M4.5 3.5 9 8l-4.5 4.5',
    undo: 'M5 6.5H10a3 3 0 0 1 0 6H7M5 6.5 7.5 4M5 6.5 7.5 9',
    reset: 'M3 8a5 5 0 1 0 1.5-3.6M3 3.5v3h3',
    compact: 'M2.5 4.5h11M2.5 8h7M2.5 11.5h4',
    arrow: 'M3 8h10M9 4l4 4-4 4',
  };
</script>

<svg width={size} height={size} viewBox="0 0 16 16" fill="none" stroke="currentColor" stroke-width="1.5" stroke-linecap="round" stroke-linejoin="round" aria-hidden="true">
  <path d={paths[name]} />
</svg>
