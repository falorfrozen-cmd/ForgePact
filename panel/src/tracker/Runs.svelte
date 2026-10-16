<script>
  // The Tracker tab's Runs section (#185): the run history, the picked run and
  // its run card, ported from HS Offline Tracker's Runs view and run card (MIT,
  // Q7(b)) in ForgePact's design (design/tracker/tab.html, "Runs").
  //
  // The history is `/api/tracker/runs`, newest first, read through the one
  // client: on opening this section and whenever a poll's `revision` moves
  // while it is open (client.svelte.js). Runs ForgePact filed and runs imported
  // from HS Offline Tracker's runs.json read the same; a run from before the
  // Tracker's 0.9.8 has no tallies, so it shows none (tracker/runs/summary.js).
  //
  // Which run is shown. Until the player picks one, the newest: a run filed
  // while the section is open is shown as it arrives. A picked run is followed
  // by its identity, not its place, so a run filed above it never slides the
  // detail onto its neighbour (the Tracker's own fix).
  //
  // The card is drawn on a canvas in the page's palette (tracker/runs/runcard.js)
  // when the shown run or the theme changes, never on a poll that changed
  // nothing, and Copy card puts it on the clipboard as a PNG. Clear history
  // asks first; only the confirmation posts `/api/tracker/runs/clear`.
  import { onMount, tick } from 'svelte';
  import { fmt, span, RARITIES, RARITY_CLASS } from './format.js';
  import { drops, finds, grade, headline, perHour, runKey, tallies, when, whoLine, zones } from './runs/summary.js';
  import { cardLabel, copyCard, drawRunCard } from './runs/runcard.js';

  let { client } = $props();

  const runs = $derived(Array.isArray(client.runs) ? client.runs : []);
  const loaded = $derived(Array.isArray(client.runs));
  let pickedKey = $state(null);
  const run = $derived(runs.find((r) => runKey(r) === pickedKey) ?? runs[0] ?? null);
  const shownKey = $derived(run ? runKey(run) : null);

  const totals = $derived(run ? [
    { key: 'gold', label: 'Gold', value: run.gold },
    { key: 'xp', label: 'Experience', value: run.xp },
    { key: 'kills', label: 'Kills', value: run.kills },
    { key: 'drops', label: 'Drops', value: drops(run) },
  ] : []);
  const bosses = $derived(run ? tallies(run, 'boss') : []);
  const chests = $derived(run ? tallies(run, 'chest') : []);
  const where = $derived(run ? zones(run) : []);
  const found = $derived(run ? finds(run) : []);

  let confirming = $state(false);
  let clearing = $state(false);
  let error = $state(null);

  let canvas = $state(null);
  let theme = $state(0);
  let drawn = Promise.resolve();
  let drawSeq = 0;
  let copyState = $state('');
  let copyTimer;

  const num = (v) => (typeof v === 'number' && Number.isFinite(v) ? v : 0);

  onMount(() => {
    // A theme change repaints the card in the new palette.
    const repaint = () => { theme += 1; };
    document.addEventListener('forgepact:theme', repaint);
    return () => {
      document.removeEventListener('forgepact:theme', repaint);
      clearTimeout(copyTimer);
    };
  });

  // Draw the shown run's card; the newest request wins.
  $effect(() => {
    void theme;
    if (!canvas || !run) return;
    const seq = ++drawSeq;
    const target = canvas;
    drawn = drawRunCard(target, $state.snapshot(run), { isCurrent: () => seq === drawSeq }).catch(() => null);
  });

  // A different run: the last copy's word was about the other one.
  $effect(() => {
    void shownKey;
    copyState = '';
    clearTimeout(copyTimer);
  });

  function pick(r) {
    pickedKey = runKey(r);
  }

  async function copy() {
    if (!canvas || !run) return;
    clearTimeout(copyTimer);
    copyState = '';
    try {
      await drawn;
      await copyCard(canvas);
      copyState = 'Copied. Paste it into any chat.';
      copyTimer = setTimeout(() => { copyState = ''; }, 4000);
    } catch (e) {
      copyState = 'Could not copy the card. Your browser blocked the clipboard.';
    }
  }

  async function askClear() {
    error = null;
    confirming = true;
    await tick();
    document.getElementById('trkRunsKeep')?.focus();
  }

  async function keep() {
    confirming = false;
    await tick();
    document.getElementById('trkRunsClear')?.focus();
  }

  async function clearAll() {
    if (clearing) return;
    clearing = true;
    try {
      const res = await client.clearRuns();
      if (res.err) { error = res.err; return; }
      error = null;
      pickedKey = null;
      confirming = false;
    } finally {
      clearing = false;
    }
  }

  function confirmKeys(e) {
    if (e.key === 'Escape') { e.preventDefault(); keep(); }
  }
</script>

<section class="trk-section" data-section="runs" aria-label="Runs">
  <section class="card trk-half" aria-labelledby="trkRunsTitle">
    <div class="trk-head"><h2 id="trkRunsTitle">Runs</h2>{#if runs.length}<span class="trk-count">{runs.length}</span>{/if}</div>
    {#if runs.length}
      <ul class="trk-runs" id="trkRunList">
        {#each runs as r (runKey(r))}
          <li>
            <button type="button" class="trk-run" aria-current={runKey(r) === shownKey ? 'true' : undefined}
              data-started={r.started_ms} data-ended={r.ended_ms} onclick={() => pick(r)}>
              <span class="trk-run-when">{when(r.started_ms)}</span>
              <span class="trk-run-len">{span(r.secs)}</span>
              <span class="trk-run-sum">{headline(r)}</span>
            </button>
          </li>
        {/each}
      </ul>
      {#if confirming}
        <!-- svelte-ignore a11y_no_noninteractive_element_interactions -->
        <div class="trk-confirm" id="trkRunsConfirm" role="group" aria-labelledby="trkClearTitle" onkeydown={confirmKeys}>
          <h3 class="trk-confirm-title" id="trkClearTitle">Clear every run?</h3>
          <p class="hint">All {runs.length} {runs.length === 1 ? 'run goes' : 'runs go'}, and this cannot be undone. Runs you imported from HS Offline Tracker go too; its own files are not touched.</p>
          <div class="trk-actions">
            <button type="button" class="btn primary" id="trkRunsClearConfirm" disabled={clearing} onclick={clearAll}>Clear history</button>
            <button type="button" class="btn" id="trkRunsKeep" disabled={clearing} onclick={keep}>Keep them</button>
          </div>
        </div>
      {:else}
        <div class="trk-actions"><button type="button" class="trk-quiet" id="trkRunsClear" onclick={askClear}>Clear history</button></div>
      {/if}
    {:else if loaded}
      <p class="trk-empty" id="trkRunsEmpty">No runs yet. A run is filed when a session ends: End run, a change of character, or the game closing. One under a minute, or one that earned nothing, is not kept.</p>
    {/if}
    {#if error}
      <p class="note trk-error" role="alert"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2.5 14.5 13.5h-13ZM8 6.5v3.2M8 11.8v.1"/></svg>{error}</p>
    {/if}
  </section>

  {#if run}
    <section class="card trk-half" id="trkRunDetail" aria-labelledby="trkRunTitle">
      <div class="trk-head">
        <h2 id="trkRunTitle">{when(run.started_ms)}</h2>
        <div class="trk-head-side"><span class="trk-count">{span(run.secs)}</span><button type="button" class="btn" id="trkRunCopy" onclick={copy}>Copy card</button></div>
      </div>
      <p class="trk-sub">{whoLine(run)}</p>
      <p class="trk-copy-state trk-live" id="trkRunCopyState" role="status">{copyState}</p>
      <div class="trk-totals">
        {#each totals as t (t.key)}
          <div class="trk-total" data-total={t.key}>
            <span class="trk-total-label">{t.label}</span>
            <span class="trk-total-value">{fmt(num(t.value))}</span>
            <span class="trk-total-rate">{fmt(perHour(num(t.value), num(run.secs)))}/h</span>
          </div>
        {/each}
      </div>
      <div class="trk-group" data-group="loot"><h3 class="trk-subhead">Loot</h3>
        <dl class="trk-tally">
          {#each RARITIES as name (name)}<dt class={RARITY_CLASS[name]}>{name}</dt><dd>{fmt(num(run.items?.[name]))}</dd>{/each}
        </dl>
      </div>
      {#if bosses.length || chests.length}
        <div class="trk-group" data-group="tallies"><h3 class="trk-subhead">Killed and opened</h3>
          <dl class="trk-tally">
            {#each [...bosses, ...chests] as t, i (t.group + '\u0000' + t.label + '\u0000' + i)}<dt>{t.label}</dt><dd>{fmt(t.total)}</dd>{/each}
          </dl>
        </div>
      {/if}
      {#if where.length}
        <div class="trk-group" data-group="zones"><h3 class="trk-subhead">Where it happened</h3>
          {#each where as z, i (z.room + '\u0000' + i)}
            <div class="trk-where"><span class="trk-where-name">{z.label}</span><span class="trk-bar" aria-hidden="true"><i style:width="{Math.round(z.share * 100)}%"></i></span><span class="trk-len">{span(z.secs)}</span></div>
          {/each}
        </div>
      {/if}
      {#if found.length}
        <div class="trk-group" data-group="finds"><div class="trk-head"><h3 class="trk-subhead">Finds</h3><span class="trk-count">{found.length}</span></div>
          <dl class="trk-tally trk-finds">
            {#each found as d, i (d.name + '\u0000' + i)}<dt class={RARITY_CLASS[d.rarity] ?? ''}>{d.name}</dt><dd>{grade(d.tier)}</dd>{/each}
          </dl>
        </div>
      {/if}
    </section>

    <section class="card trk-half" aria-labelledby="trkRunCardTitle">
      <div class="trk-head"><h2 id="trkRunCardTitle">Run card</h2></div>
      <p class="hint">What Copy card puts on the clipboard, drawn in this theme's colours, to paste into a chat.</p>
      <canvas class="trk-runcard" id="trkRunCard" role="img" aria-label={cardLabel(run)} bind:this={canvas}></canvas>
    </section>
  {/if}
</section>

<style>
  /* design/tracker/tracker.css's Runs rules, tokens only. Nothing here moves
     when a poll repaints it. */
  .trk-runs { display: flex; flex-direction: column; max-height: 480px; margin: 0; padding: 0; overflow-y: auto; list-style: none; }
  .trk-run {
    display: grid;
    grid-template-columns: minmax(0, 1fr) auto;
    gap: var(--space-1) var(--space-6);
    width: 100%;
    padding: var(--space-4) var(--space-5);
    border: 0;
    border-radius: var(--radius-md);
    background: none;
    color: var(--color-text-primary);
    font: inherit;
    text-align: left;
    cursor: pointer;
  }
  .trk-run[aria-current="true"] { background-color: var(--color-bg-hover); }
  .trk-run-when { font-size: var(--font-size-sm); font-weight: var(--font-weight-medium); }
  .trk-run-len { color: var(--color-text-muted); font-family: var(--font-mono-family), Consolas, monospace; font-size: var(--font-size-xs); text-align: right; }
  .trk-run-sum { grid-column: 1 / -1; min-width: 0; overflow: hidden; color: var(--color-text-muted); font-family: var(--font-mono-family), Consolas, monospace; font-size: var(--font-size-xs); text-overflow: ellipsis; white-space: nowrap; }
  /* Empty, it takes no room but stays a live region (app.css's .trk-live). */
  .trk-copy-state { margin: 0; color: var(--color-text-muted); font-size: var(--font-size-xs); }

  .trk-totals { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: var(--space-7) var(--space-9); }
  .trk-total { display: flex; flex-direction: column; gap: var(--space-2); min-width: 0; }
  .trk-total-label { color: var(--color-text-muted); font-size: var(--font-size-xs); }
  .trk-total-value {
    color: var(--color-text-primary);
    font-family: var(--font-mono-family), Consolas, monospace;
    font-size: var(--font-size-2xl);
    line-height: var(--font-line-height-tight);
    white-space: nowrap;
  }
  .trk-total-rate { color: var(--color-text-muted); font-family: var(--font-mono-family), Consolas, monospace; font-size: var(--font-size-xs); white-space: nowrap; }

  .trk-group { display: flex; flex-direction: column; gap: var(--space-4); }
  .trk-tally { display: grid; grid-template-columns: minmax(0, 1fr) auto; gap: var(--space-3) var(--space-7); margin: 0; color: var(--color-text-primary); }
  .trk-tally dt { min-width: 0; overflow: hidden; font-size: var(--font-size-sm); text-overflow: ellipsis; white-space: nowrap; }
  .trk-tally dd { margin: 0; font-family: var(--font-mono-family), Consolas, monospace; font-size: var(--font-size-sm); text-align: right; }
  .trk-finds { max-height: 240px; overflow-y: auto; }
  .trk-where { display: grid; grid-template-columns: minmax(0, 1fr) 120px 56px; align-items: center; gap: var(--space-6); }
  .trk-where-name { min-width: 0; overflow: hidden; font-size: var(--font-size-sm); text-overflow: ellipsis; white-space: nowrap; }
  .trk-len { color: var(--color-text-muted); font-family: var(--font-mono-family), Consolas, monospace; font-size: var(--font-size-xs); text-align: right; }
  .trk-bar { height: 4px; border-radius: var(--radius-pill); background-color: var(--color-bg-sunken); }
  .trk-bar > i { display: block; height: 100%; border-radius: var(--radius-pill); background-color: var(--color-text-muted); }

  /* The card at its drawn size (480 wide), scaled down in a narrow column. */
  .trk-runcard { display: block; width: 480px; max-width: 100%; height: auto; border-radius: var(--radius-sm); }

  @media (hover: hover) and (pointer: fine) {
    .trk-run:hover { background-color: var(--color-bg-hover); }
  }
</style>
