<script>
  // The Tracker tab's Session section (#185): the session card (the character,
  // the clock, Pause clock and End run, then gold, experience, kills and drops
  // with their rates), Loot (by rarity, then Notable), Killed and opened, the
  // Satanic zone, the two series charts and the journal. Ported from HS
  // Offline Tracker's Stats view (MIT; the notice is in CREDITS.md) into the
  // panel's design: panel/DESIGN.md § Tracker, design/tracker/tab.html.
  //
  // Everything shown comes from the one client Tracker.svelte hands in
  // (client.svelte.js): `snapshot` from `/api/tracker`, `extra` (the series and
  // the journal) from `/api/tracker/extra`. What each card says is worked out
  // in session/view.js, which node tests on its own. The zone's buffs and
  // debuffs are named from `/api/state`'s Satanic lists, which the page
  // already holds in ST.
  //
  // Nothing a poll repaints moves: no transition or animation on a total, a
  // rate, the clock, the zone, a chart or a journal row. End run asks first, in
  // place, as Clear history does on Runs.
  import { ST } from '../state.svelte.js';
  import {
    characterLine, chartPoints, journalRows, lootRows, modRows, notableRows, rolledAgo, sessionClock, sessionState,
    tallyGroups, totals, zoneName, CHART_H, CHART_W,
  } from './session/view.js';
  import { fmt, rate, clock } from './format.js';

  let { client } = $props();

  const snap = $derived(client.snapshot);
  const view = $derived(sessionState({ snapshot: client.snapshot, session: client.session }));
  const character = $derived(snap?.character ?? client.extra?.character ?? null);
  const subLine = $derived(characterLine(character));
  const totalRows = $derived(totals(snap));
  const loot = $derived(lootRows(snap));
  const notable = $derived(notableRows(snap));
  const tallies = $derived(tallyGroups(snap?.tallies));
  const zone = $derived(snap?.satanic_zone ?? null);
  const buffs = $derived(modRows(zone?.buffs, ST?.satanicBuffs, 'Buff'));
  const debuffs = $derived(modRows(zone?.debuffs, ST?.satanicDebuffs, 'Debuff'));
  // Read again on every poll's answer: the snapshot is a new object each time.
  const rolled = $derived(snap ? rolledAgo(snap.satanic_at, Date.now()) : '');
  const charts = $derived([
    { key: 'gold', label: 'Gold earned', ...chartPoints(client.extra?.series, 'gold') },
    { key: 'xp', label: 'Experience earned', ...chartPoints(client.extra?.series, 'xp') },
  ]);
  const journal = $derived(journalRows(client.extra?.drops));

  let confirming = $state(false);
  let busy = $state(false);
  let failure = $state(null);
  let endButton = $state(null);
  let keepButton = $state(null);

  async function act(call) {
    if (busy) return;
    busy = true;
    failure = null;
    try {
      const res = await call();
      if (res?.err) failure = res.err;
      return res;
    } finally {
      busy = false;
    }
  }

  function togglePause() {
    act(() => client.pause(!snap?.paused));
  }

  function askEnd() {
    failure = null;
    confirming = true;
    queueMicrotask(() => keepButton?.focus());
  }

  function keepGoing() {
    confirming = false;
    queueMicrotask(() => endButton?.focus());
  }

  async function endRun() {
    const res = await act(() => client.reset());
    confirming = false;
    if (!res?.err) queueMicrotask(() => endButton?.focus());
  }

  function chartLabel(c) {
    if (!c.samples) return `${c.label}: no samples yet`;
    return `${c.label} rose from 0 to ${fmt(c.latest)} over ${clock(c.span)}`;
  }
</script>

<section class="trk-section" data-section="session" aria-label="Session" data-state={view} data-revision={snap?.revision ?? ''}>
  {#if view === 'none'}
    <section class="card trk-half" id="trkSessionCard" aria-labelledby="trkNoSessionTitle">
      <h2 id="trkNoSessionTitle">No session yet</h2>
      {#if client.on}
        <p class="trk-empty">Start the game with the plugin installed and load a character. The first numbers arrive within a second.</p>
      {:else}
        <p class="trk-empty">The tracker is off. Turn it on above, then start the game and load a character to count a session.</p>
      {/if}
    </section>
  {:else if view === 'session'}
    <section class="card" id="trkSessionCard" aria-labelledby="trkSessionTitle">
      <div class="trk-head">
        <h2 id="trkSessionTitle">{character?.name || 'This session'}</h2>
        <div class="trk-head-side">
          <span class="trk-clock" class:paused={snap.paused} id="trkClock" data-secs={snap.session_secs ?? 0}
            aria-label={snap.paused ? 'Session time, paused' : 'Session time'}>{sessionClock(snap)}</span>
          {#if client.on && !confirming}
            <button type="button" class="btn" id="trkPause" disabled={busy} onclick={togglePause}>{snap.paused ? 'Carry on' : 'Pause clock'}</button>
            <button type="button" class="trk-quiet" id="trkEndRun" disabled={busy} bind:this={endButton} onclick={askEnd}>End run</button>
          {/if}
        </div>
      </div>
      {#if subLine}<p class="trk-sub" id="trkCharacterLine">{subLine}</p>{/if}

      {#if confirming}
        <div class="trk-confirm" id="trkEndConfirm" role="group" aria-labelledby="trkEndTitle">
          <h3 class="trk-confirm-title" id="trkEndTitle">End this run?</h3>
          <p class="hint">It is filed under Runs if it lasted a minute and earned something, and the counters start again from zero.</p>
          <div class="trk-actions">
            <button type="button" class="btn primary" id="trkEndYes" disabled={busy} onclick={endRun}>End run</button>
            <button type="button" class="btn" id="trkEndNo" bind:this={keepButton} onclick={keepGoing}>Keep going</button>
          </div>
        </div>
      {/if}

      {#if !client.on}
        <p class="trk-state" id="trkSessionNote"><span class="trk-dot" aria-hidden="true"></span>The tracker is off. Turn it on above to count this session; these are the last numbers it read.</p>
      {:else if client.catching_up}
        <p class="trk-state" id="trkSessionNote"><span class="trk-dot warn" aria-hidden="true"></span>Catching up on this session's records…</p>
      {:else if snap.paused}
        <p class="trk-state" id="trkSessionNote"><span class="trk-dot" aria-hidden="true"></span>Clock paused: the counters keep counting, only the per-hour rates stop.</p>
      {/if}
      {#if failure}<p class="trk-state" id="trkSessionError" role="alert"><span class="trk-dot warn" aria-hidden="true"></span>{failure}</p>{/if}

      <div class="trk-totals">
        {#each totalRows as t (t.key)}
          <div class="trk-total" data-key={t.key}>
            <span class="trk-total-label">{t.label}</span>
            <span class="trk-total-value" data-value={t.earned}>{t.value}</span>
            <span class="trk-total-rate" data-value={t.per_hour}>{t.rate}</span>
            <span class="trk-total-note" data-total={t.total ?? ''}>{t.note}</span>
          </div>
        {/each}
      </div>
    </section>

    <div class="trk-stack">
      <section class="card" aria-labelledby="trkLootTitle">
        <div class="trk-head"><h2 id="trkLootTitle">Loot</h2><span class="trk-count">this session</span></div>
        <table class="trk-table" id="trkLoot">
          <thead><tr><th scope="col">Rarity</th><th scope="col">Drops</th><th scope="col" title="Of those, the ones the game credited to Magic Find">Magic Find</th><th scope="col">Per hour</th></tr></thead>
          <tbody>
            {#each loot as r (r.name)}
              <tr data-rarity={r.name}>
                <td class={r.cls}>{r.name}</td>
                <td data-value={r.total}>{fmt(r.total)}</td>
                <td class="trk-dim" data-value={r.mf}>{r.mf ? fmt(r.mf) : '–'}</td>
                <td class="trk-dim" data-value={r.per_hour}>{rate(r.per_hour)}</td>
              </tr>
            {/each}
          </tbody>
        </table>
        <div class="trk-group">
          <h3 class="trk-subhead">Notable</h3>
          <dl class="trk-tally" id="trkNotable">
            {#each notable as n (n.label)}<dt>{n.label}</dt><dd data-label={n.label} data-value={n.total}>{fmt(n.total)}</dd>{/each}
          </dl>
        </div>
      </section>
      <section class="card" aria-labelledby="trkTallyTitle" id="trkTallies">
        <div class="trk-head"><h2 id="trkTallyTitle">Killed and opened</h2><span class="trk-count">this session</span></div>
        {#if tallies.bosses.length || tallies.chests.length}
          {#if tallies.bosses.length}
            <div class="trk-group" data-group="boss"><h3 class="trk-subhead">Bosses</h3>
              <dl class="trk-tally">{#each tallies.bosses as b (b.label)}<dt>{b.label}</dt><dd data-label={b.label} data-value={b.total}>{fmt(b.total)}</dd>{/each}</dl>
            </div>
          {/if}
          {#if tallies.chests.length}
            <div class="trk-group" data-group="chest"><h3 class="trk-subhead">Chests</h3>
              <dl class="trk-tally">{#each tallies.chests as c (c.label)}<dt>{c.label}</dt><dd data-label={c.label} data-value={c.total}>{fmt(c.total)}</dd>{/each}</dl>
            </div>
          {/if}
        {:else}
          <p class="trk-empty">Bosses killed and chests opened show here once the character's save has counted one this session.</p>
        {/if}
      </section>
    </div>

    <div class="trk-stack">
      <section class="card" aria-labelledby="trkZoneTitle" id="trkZone">
        <div class="trk-head"><h2 id="trkZoneTitle">Satanic zone</h2>{#if zone && rolled}<span class="trk-count">{rolled}</span>{/if}</div>
        {#if zone}
          <p class="trk-zone-name" id="trkZoneName" data-zone={zone.zone}><span>{zoneName(zone.zone)}</span>{#if snap.satanic_here}<span class="trk-here"><span class="trk-dot on" aria-hidden="true"></span>You are here</span>{/if}</p>
          <div class="trk-mods">
            <div class="trk-group"><h3 class="trk-subhead">Buffs</h3>
              <ul class="trk-mod-list">
                {#each buffs as m, i (i)}
                  <li class="trk-mod buff" data-id={m.id}><svg viewBox="0 0 12 12" aria-hidden="true"><path d="M6 2v8M2 6h8"/></svg><span class="trk-mod-name">{m.name}</span>{#if m.desc}<span class="trk-mod-desc">{m.desc}</span>{/if}</li>
                {:else}
                  <li class="trk-mod-none">None</li>
                {/each}
              </ul>
            </div>
            <div class="trk-group"><h3 class="trk-subhead">Debuffs</h3>
              <ul class="trk-mod-list">
                {#each debuffs as m, i (i)}
                  <li class="trk-mod debuff" data-id={m.id}><svg viewBox="0 0 12 12" aria-hidden="true"><path d="M2 6h8"/></svg><span class="trk-mod-name">{m.name}</span>{#if m.desc}<span class="trk-mod-desc">{m.desc}</span>{/if}</li>
                {:else}
                  <li class="trk-mod-none">None</li>
                {/each}
              </ul>
            </div>
          </div>
        {:else}
          <p class="trk-empty">Known once the game reports it: enter the world with the tracker on.</p>
        {/if}
      </section>
      <section class="card" aria-labelledby="trkSeriesTitle" id="trkSeries">
        <div class="trk-head"><h2 id="trkSeriesTitle">Over the session</h2><span class="trk-count">one point a minute</span></div>
        <div class="trk-series">
          {#each charts as c (c.key)}
            <figure class="trk-chart" data-key={c.key} data-samples={c.samples}>
              <figcaption><span>{c.label}</span><b>{fmt(c.latest)}</b></figcaption>
              <svg viewBox="0 0 {CHART_W} {CHART_H}" preserveAspectRatio="none" role="img" aria-label={chartLabel(c)}>
                <line class="trk-grid-line" x1="0" y1={CHART_H} x2={CHART_W} y2={CHART_H} vector-effect="non-scaling-stroke"/>
                <line class="trk-grid-line" x1="0" y1={CHART_H / 2} x2={CHART_W} y2={CHART_H / 2} vector-effect="non-scaling-stroke"/>
                {#if c.samples}<polyline class="trk-line" vector-effect="non-scaling-stroke" points={c.attr}/>{/if}
              </svg>
              <figcaption><span class="trk-axis-text">0:00</span><span class="trk-axis-text">{clock(c.span)}</span></figcaption>
            </figure>
          {/each}
        </div>
      </section>
    </div>

    <section class="card" aria-labelledby="trkJournalTitle">
      <div class="trk-head"><h2 id="trkJournalTitle">Journal</h2><span class="trk-count">{journal.length === 1 ? '1 drop' : `${journal.length} drops`}</span></div>
      {#if journal.length}
        <ol class="trk-journal" id="trkJournal" aria-label="Drops, newest first">
          {#each journal as d (d.key)}
            <li class="trk-entry" data-id={d.id}>
              <span class="trk-time">{d.time}</span>
              {#if d.grade}<span class="trk-tag" title="Grade {d.grade}">{d.grade}</span>{:else}<span class="trk-tag trk-tag-none" title="No grade">–</span>{/if}
              <span class="trk-rarity {d.cls}">{d.rarity}</span>
              <span class="trk-item {d.cls}" title={d.name}>{d.name}</span>
              <span class="trk-tags">
                {#if d.mf}<span class="trk-tag" title="Credited to Magic Find">MF</span>{/if}
                {#if !d.named}<span class="trk-tag-word">unnamed</span>{/if}
                {#if d.alerted}<span class="trk-tag-word">alerted</span>{/if}
              </span>
            </li>
          {/each}
        </ol>
      {:else}
        <p class="trk-empty" id="trkJournalEmpty">Nothing yet. The drops your alerts follow land here, newest first.</p>
      {/if}
    </section>
  {/if}
</section>

<style>
  /* The Session section's own rules, ported from design/tracker/tracker.css.
     Tokens only; numbers in Plex Mono; no transition or animation anywhere:
     every value here is repainted by a poll. */
  .trk-clock {
    color: var(--color-text-primary);
    font-family: var(--font-mono-family), Consolas, monospace;
    font-size: var(--font-size-xl);
    line-height: var(--font-line-height-tight);
  }
  .trk-clock.paused { color: var(--color-text-muted); }
  .trk-quiet {
    display: inline-flex;
    align-items: center;
    gap: var(--space-3);
    min-height: 28px;
    padding: 0 var(--space-4);
    border: 0;
    border-radius: var(--radius-sm);
    background: none;
    color: var(--color-text-muted);
    font: inherit;
    font-size: var(--font-size-sm);
    white-space: nowrap;
    cursor: pointer;
  }
  .trk-quiet:disabled { cursor: default; }
  .trk-confirm { display: flex; flex-direction: column; gap: var(--space-4); }
  .trk-confirm .hint { margin: 0; }
  .trk-confirm-title { margin: 0; color: var(--color-text-primary); font-size: var(--font-size-sm); font-weight: var(--font-weight-semibold); }
  .trk-actions { display: flex; flex-wrap: wrap; align-items: center; gap: var(--space-4); }

  .trk-totals {
    display: grid;
    grid-template-columns: repeat(4, minmax(0, 1fr));
    gap: var(--space-7) var(--space-9);
  }
  .trk-total { display: flex; flex-direction: column; gap: var(--space-2); min-width: 0; }
  .trk-total-label { color: var(--color-text-muted); font-size: var(--font-size-xs); }
  .trk-total-value {
    color: var(--color-text-primary);
    font-family: var(--font-mono-family), Consolas, monospace;
    font-size: var(--font-size-2xl);
    line-height: var(--font-line-height-tight);
    white-space: nowrap;
  }
  .trk-total-rate,
  .trk-total-note {
    color: var(--color-text-muted);
    font-family: var(--font-mono-family), Consolas, monospace;
    font-size: var(--font-size-xs);
    white-space: nowrap;
  }
  .trk-total-note { color: var(--color-text-faint); }

  .trk-table { width: 100%; border-collapse: collapse; color: var(--color-text-primary); font-size: var(--font-size-sm); }
  .trk-table th {
    padding: 0 var(--space-4) var(--space-3);
    color: var(--color-text-muted);
    font-size: var(--font-size-xs);
    font-weight: var(--font-weight-regular);
    text-align: right;
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

  .trk-tally { display: grid; grid-template-columns: minmax(0, 1fr) auto; gap: var(--space-3) var(--space-7); margin: 0; color: var(--color-text-primary); }
  .trk-tally dt { min-width: 0; font-size: var(--font-size-sm); }
  .trk-tally dd {
    margin: 0;
    font-family: var(--font-mono-family), Consolas, monospace;
    font-size: var(--font-size-sm);
    text-align: right;
  }
  .trk-group { display: flex; flex-direction: column; gap: var(--space-4); }

  .trk-zone-name {
    display: flex;
    align-items: center;
    flex-wrap: wrap;
    gap: var(--space-4) var(--space-6);
    margin: 0;
    color: var(--color-text-primary);
    font-size: var(--font-size-lg);
    font-weight: var(--font-weight-semibold);
  }
  .trk-here {
    display: inline-flex;
    align-items: center;
    gap: var(--space-3);
    height: 24px;
    padding: 0 var(--space-5);
    border-radius: var(--radius-pill);
    background-color: var(--color-bg-hover);
    color: var(--color-text-muted);
    font-size: var(--font-size-xs);
    font-weight: var(--font-weight-regular);
  }
  .trk-mods { display: grid; grid-template-columns: minmax(0, 1fr) minmax(0, 1fr); gap: var(--space-7); }
  .trk-mod-list { display: flex; flex-direction: column; gap: var(--space-4); margin: 0; padding: 0; list-style: none; }
  .trk-mod { display: grid; grid-template-columns: 12px minmax(0, 1fr); gap: 0 var(--space-4); }
  .trk-mod svg { width: 12px; height: 12px; margin-top: 3px; stroke: currentColor; stroke-width: 1.6; stroke-linecap: round; fill: none; }
  .trk-mod.buff svg { color: var(--color-ok); }
  .trk-mod.debuff svg { color: var(--color-danger); }
  .trk-mod-name { color: var(--color-text-primary); font-size: var(--font-size-sm); font-weight: var(--font-weight-medium); }
  .trk-mod-desc { grid-column: 2; color: var(--color-text-muted); font-size: var(--font-size-xs); }
  .trk-mod-none { color: var(--color-text-muted); font-size: var(--font-size-sm); }

  .trk-series { display: grid; grid-template-columns: minmax(0, 1fr); gap: var(--space-7); }
  .trk-chart { display: flex; flex-direction: column; gap: var(--space-3); margin: 0; }
  .trk-chart figcaption {
    display: flex;
    justify-content: space-between;
    gap: var(--space-6);
    color: var(--color-text-muted);
    font-size: var(--font-size-xs);
  }
  .trk-chart figcaption b {
    color: var(--color-text-primary);
    font-family: var(--font-mono-family), Consolas, monospace;
    font-weight: var(--font-weight-regular);
  }
  .trk-axis-text { font-family: var(--font-mono-family), Consolas, monospace; }
  .trk-chart svg { display: block; width: 100%; height: 96px; overflow: visible; }
  .trk-grid-line { stroke: var(--color-border-subtle); stroke-width: 1; }
  .trk-line { fill: none; stroke: var(--color-text-primary); stroke-width: 2; stroke-linejoin: round; stroke-linecap: round; }

  .trk-journal {
    display: flex;
    flex-direction: column;
    max-height: 352px;
    margin: 0;
    padding: 0;
    overflow-y: auto;
    list-style: none;
  }
  .trk-entry {
    display: grid;
    grid-template-columns: 64px 32px 64px minmax(0, 1fr) auto;
    align-items: center;
    gap: var(--space-6);
    min-height: 32px;
    padding: 0 var(--space-4);
    border-radius: var(--radius-sm);
  }
  .trk-time { color: var(--color-text-muted); font-family: var(--font-mono-family), Consolas, monospace; font-size: var(--font-size-xs); }
  .trk-rarity { font-size: var(--font-size-xs); }
  .trk-item { min-width: 0; overflow: hidden; font-size: var(--font-size-sm); font-weight: var(--font-weight-medium); text-overflow: ellipsis; white-space: nowrap; }
  .trk-tags { display: flex; align-items: center; gap: var(--space-4); }
  .trk-tag {
    min-width: 24px;
    padding: 0 var(--space-3);
    border-radius: var(--radius-sm);
    background-color: var(--color-bg-hover);
    color: var(--color-text-muted);
    font-family: var(--font-mono-family), Consolas, monospace;
    font-size: var(--font-size-xs);
    line-height: 20px;
    text-align: center;
  }
  .trk-tag-word { color: var(--color-text-muted); font-size: var(--font-size-xs); }
  .trk-tag-none { background: none; color: var(--color-text-muted); }
  .trk-empty { margin: 0; padding: var(--space-7) 0; color: var(--color-text-faint); font-size: var(--font-size-sm); }

  /* Hover, only for a real pointer, colour only. */
  @media (hover: hover) and (pointer: fine) {
    .trk-entry:hover { background-color: var(--color-bg-hover); }
    .trk-quiet:hover:not(:disabled) { background-color: var(--color-bg-hover); color: var(--color-text-primary); }
  }

  @media (max-width: 720px) {
    .trk-totals { grid-template-columns: repeat(2, minmax(0, 1fr)); }
    .trk-mods { grid-template-columns: minmax(0, 1fr); }
    .trk-entry { grid-template-columns: 56px 32px minmax(0, 1fr) auto; }
    .trk-entry .trk-rarity { display: none; }
  }
</style>
