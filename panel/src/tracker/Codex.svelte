<script>
  // The Tracker tab's Item codex section (#185): every named item, how rare it
  // is and where to farm it. Ported from HS Offline Tracker's Codex view (MIT,
  // Q7(b); the notice is in CREDITS.md) in ForgePact's design
  // (design/tracker/tab.html, "Item codex"): one table, with the rarity
  // written beside its colour, rather than the Tracker's cards.
  //
  // The rows come from `/api/tracker/codex` (src/forgepact_tracker/codex.py),
  // fetched once when this section mounts. Tracker.svelte mounts it only while
  // the tab is open on this section, so the page asks for the codex only when
  // the player opens it, and never polls it: the tables do not change while
  // the panel runs. The search, the filters and the order are
  // tracker/codex/rows.js, pure and tested under node
  // (tests/tracker-codex.test.js); the page draws the first SHOWN rows of what
  // they find and says how many more a narrower search would show.
  import { onMount } from 'svelte';
  import {
    SHOWN, findRows, gradeLabel, groupOptions, odds, rarityClass, rarityOptions, whereLabel,
  } from './codex/rows.js';

  let { client } = $props();

  let rows = $state([]);
  let loaded = $state(false);
  let error = $state(null);
  let query = $state('');
  let rarity = $state('');
  let group = $state('');
  let sort = $state('rarest');

  const rarities = $derived(rarityOptions(rows));
  const groups = $derived(groupOptions(rows));
  const found = $derived(findRows(rows, { query, rarity, group, sort }));
  const shown = $derived(found.slice(0, SHOWN));
  const grouped = (n) => n.toLocaleString('en-US');

  onMount(() => {
    let live = true;
    client.loadCodex().then((res) => {
      if (!live) return;
      if (res.err) error = res.err;
      else rows = Array.isArray(res.items) ? res.items : [];
      loaded = true;
    });
    return () => { live = false; };
  });
</script>

<section class="trk-section" data-section="codex" aria-label="Item codex" data-loaded={loaded ? '' : undefined}>
  <section class="card" aria-labelledby="trkCodexTitle">
    <div class="trk-head"><h2 id="trkCodexTitle">Item codex</h2>{#if rows.length}<span class="trk-count">{grouped(found.length)} of {grouped(rows.length)}</span>{/if}</div>
    <p class="hint">Every named item, how rare it is and where to farm it. Anywhere is the chance wherever you play; where it is tied is the better chance in its places, the one the game prints in green.</p>
    <div class="trk-tools">
      <input class="trk-field" id="trkCodexSearch" type="search" placeholder="Search by name" aria-label="Search the codex"
        autocomplete="off" spellcheck="false" bind:value={query}>
      <select class="style-select" id="trkCodexRarity" aria-label="Rarity" bind:value={rarity}>
        <option value="">Any rarity</option>
        {#each rarities as r (r)}<option value={r}>{r}</option>{/each}
      </select>
      <select class="style-select" id="trkCodexKind" aria-label="Kind" bind:value={group}>
        <option value="">Any kind</option>
        {#each groups as g (g)}<option value={g}>{g}</option>{/each}
      </select>
      <select class="style-select" id="trkCodexOrder" aria-label="Order" bind:value={sort}>
        <option value="rarest">Rarest first</option>
        <option value="name">By name</option>
      </select>
    </div>
    {#if error}
      <p class="note trk-error" role="alert"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2.5 14.5 13.5h-13ZM8 6.5v3.2M8 11.8v.1"/></svg>Could not read the codex: {error}</p>
    {/if}
    {#if shown.length}
      <div class="trk-scroll">
        <table class="trk-table trk-codex" aria-labelledby="trkCodexTitle">
          <thead><tr>
            <th scope="col">Item</th><th scope="col" class="trk-left">Rarity</th><th scope="col" class="trk-left">Kind</th><th scope="col">Grade</th>
            <th scope="col">Anywhere</th><th scope="col">Where it is tied</th><th scope="col" class="trk-left">Places</th>
          </tr></thead>
          <tbody>
            {#each shown as row (row.name)}
              <tr>
                <td class="trk-c-name {rarityClass(row.rarity)}">{row.name}</td>
                <td class="trk-c-rarity trk-word {rarityClass(row.rarity)}">{row.rarity}</td>
                <td class="trk-c-kind trk-word trk-dim">{row.type}</td>
                <td class="trk-c-grade">{gradeLabel(row.tier)}</td>
                <td class="trk-c-anywhere trk-dim">{odds(row.rate)}</td>
                <td class="trk-c-tied" class:trk-dim={!(row.chase > 0)}>{odds(row.chase)}</td>
                <td class="trk-c-where trk-where-cell" title={whereLabel(row)}>{whereLabel(row)}</td>
              </tr>
            {/each}
          </tbody>
        </table>
      </div>
    {:else if loaded && !error}
      <p class="trk-empty">{rows.length ? 'Nothing matches that. Clear the search or a filter to see more.' : 'The codex is empty.'}</p>
    {/if}
    {#if found.length > SHOWN}
      <p class="note trk-codex-more">Showing {grouped(SHOWN)} of {grouped(found.length)}. Narrow the search to see the rest.</p>
    {/if}
    {#if loaded && rows.length}
      <p class="note trk-codex-source">Drop chances come from the item tables HS Offline Tracker shipped.</p>
    {/if}
  </section>
</section>

<style>
  /* design/tracker/tracker.css's codex and table rules, tokens only. */
  .trk-tools { display: flex; flex-wrap: wrap; align-items: center; gap: var(--space-4); }
  .trk-tools .trk-field { flex: 1 1 220px; }
  .trk-scroll { min-width: 0; overflow-x: auto; }
  .trk-table { width: 100%; border-collapse: collapse; color: var(--color-text-primary); font-size: var(--font-size-sm); }
  .trk-table th {
    padding: 0 var(--space-4) var(--space-3);
    color: var(--color-text-muted);
    font-size: var(--font-size-xs);
    font-weight: var(--font-weight-regular);
    text-align: right;
    white-space: nowrap;
  }
  .trk-table td {
    height: 32px;
    padding: 0 var(--space-4);
    font-family: var(--font-mono-family), Consolas, monospace;
    text-align: right;
    white-space: nowrap;
  }
  .trk-table th:first-child,
  .trk-table td:first-child {
    padding-left: 0;
    font-family: var(--font-body-family), "Segoe UI", system-ui, sans-serif;
    font-weight: var(--font-weight-medium);
    text-align: left;
  }
  .trk-table td.trk-dim { color: var(--color-text-muted); }
  .trk-codex th.trk-left,
  .trk-table td.trk-word { text-align: left; }
  .trk-table td.trk-word { font-family: var(--font-body-family), "Segoe UI", system-ui, sans-serif; }
  .trk-codex td.trk-where-cell {
    max-width: 260px;
    overflow: hidden;
    color: var(--color-text-muted);
    font-family: var(--font-body-family), "Segoe UI", system-ui, sans-serif;
    text-align: left;
    text-overflow: ellipsis;
  }
  /* Hover, only for a real pointer, colour only. */
  @media (hover: hover) and (pointer: fine) {
    .trk-codex tbody tr:hover { background-color: var(--color-bg-hover); }
  }
</style>
