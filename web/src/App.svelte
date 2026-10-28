<script lang="ts">
  import Home from './routes/Home.svelte';
  import Contiguous from './routes/Contiguous.svelte';
  import Paging from './routes/Paging.svelte';
  import Replay from './routes/Replay.svelte';
  import Icon from './components/Icon.svelte';
  import { routeName } from './lib/route';

  const routes = { '': Home, contiguous: Contiguous, paging: Paging, replay: Replay } as const;
  type Route = keyof typeof routes;
  const current = (): Route => (routeName() in routes ? (routeName() as Route) : '');
  let route = $state<Route>(current());
  let hash = $state(location.hash);
  $effect(() => {
    const on = () => {
      route = current();
      hash = location.hash;
      window.scrollTo(0, 0);
    };
    addEventListener('hashchange', on);
    return () => removeEventListener('hashchange', on);
  });

  let theme = $state(document.documentElement.dataset.theme === 'light' ? 'light' : 'dark');
  function toggleTheme() {
    theme = theme === 'light' ? 'dark' : 'light';
    document.documentElement.dataset.theme = theme;
    try {
      localStorage.setItem('memforge-theme', theme);
    } catch {}
  }
  const Page = $derived(routes[route]);
</script>

<header class="bar">
  <a class="brand" href="#/" aria-label="MemForge home">
    <span class="mark" aria-hidden="true"><i></i><i></i><i></i></span>
    MemForge
  </a>
  <nav aria-label="Main">
    <a href="#/" aria-current={route === '' ? 'page' : undefined}>Overview</a>
    <a href="#/contiguous" aria-current={route === 'contiguous' ? 'page' : undefined}>Contiguous memory</a>
    <a href="#/paging" aria-current={route === 'paging' ? 'page' : undefined}>Paging</a>
    <a href="#/replay" aria-current={route === 'replay' ? 'page' : undefined}>Replay a trace</a>
  </nav>
  <button class="btn ghost icon" onclick={toggleTheme} aria-label={`Switch to ${theme === 'light' ? 'dark' : 'light'} theme`}>
    <Icon name={theme === 'light' ? 'moon' : 'sun'} />
  </button>
</header>

<main>
  {#key hash}<Page />{/key}
</main>

<style>
  .bar {
    position: sticky;
    top: 0;
    z-index: 10;
    display: grid;
    grid-template-columns: auto 1fr auto;
    align-items: center;
    gap: 32px;
    height: 60px;
    padding: 0 24px;
    background: color-mix(in srgb, var(--ground) 88%, transparent);
    backdrop-filter: saturate(1.4) blur(10px);
    border-bottom: 1px solid var(--rule);
  }
  .brand {
    display: flex;
    align-items: center;
    gap: 10px;
    font-weight: 650;
    letter-spacing: -0.01em;
    color: var(--text);
    text-decoration: none;
  }
  .mark {
    display: flex;
    gap: 2px;
    height: 14px;
  }
  .mark i {
    display: block;
    border-radius: 2px;
  }
  .mark i:nth-child(1) {
    width: 5px;
    background: var(--accent);
  }
  .mark i:nth-child(2) {
    width: 10px;
    background: var(--p0);
  }
  .mark i:nth-child(3) {
    width: 4px;
    background: var(--p1);
  }
  nav {
    display: flex;
    gap: 4px;
  }
  nav a {
    padding: 6px 12px;
    border-radius: 6px;
    color: var(--muted);
    text-decoration: none;
    font-weight: 500;
    transition: color 160ms var(--ease), background 160ms var(--ease);
  }
  nav a:hover {
    color: var(--text);
  }
  nav a[aria-current='page'] {
    color: var(--text);
    background: var(--raised);
  }
  .icon {
    width: 36px;
    padding: 0;
    justify-content: center;
  }
  main {
    max-width: 1280px;
    margin: 0 auto;
    padding: 32px 24px 64px;
  }
  @media (max-width: 760px) {
    .bar {
      grid-template-columns: auto auto;
      gap: 12px;
    }
    nav {
      display: none;
    }
  }
</style>
