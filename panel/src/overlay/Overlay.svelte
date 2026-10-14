<script>
  // The Tracker's overlay window (#185): one plate with the state, the clock
  // and the zone, gold, experience and kills with their rates, the five
  // rarity counts and SS; the newest drops under them when the player asks
  // for them; and the drop card over the counters for the drops the player
  // chose. Ported from HS Offline Tracker's compact window, Ticker and
  // Flourish (MIT; the notice is in CREDITS.md) into ForgePact's design
  // (panel/DESIGN.md § Tracker, design/tracker/overlay.html).
  //
  // It reads `/api/tracker` through the same client the Tracker tab uses
  // (overlay mode: it polls whatever the switch says), and never claims or
  // plays an alert: sounds are the main window's (alerts.js). The settings are
  // read again every few seconds, so a change made in the main window reaches
  // this one. What it shows is worked out in queue.js.
  //
  // Inside ForgePact's own overlay window, pywebview offers this page two
  // calls (src/tracker_overlay.py's OverlayBridge): `place_done` when the
  // player lets go of a drag, so the place is saved, and `fit`, so the window
  // takes the plate's size (the newest drops, counters only and the size
  // setting each change it). In a browser tab neither exists and nothing is
  // called. The plate is the window's drag region unless the overlay is
  // locked.
  //
  // Nothing a poll repaints moves; the drop card is the one thing that does.
  import { onMount, untrack } from 'svelte';
  import { TrackerClient } from '../tracker/client.svelte.js';
  import { fmt, rate, clock, RARITIES, RARITY_CLASS } from '../tracker/format.js';
  import { zoneName } from '../tracker/session/view.js';
  import { applyTheme } from '../theme.js';
  import { FlourishQueue, cardMs, overlayState, tickerRows } from './queue.js';

  const SETTINGS_MS = 3000;

  // Overlay mode polls on its own; the switch's state is whatever the last answer said.
  const client = new TrackerClient({ overlay: true, switchOn: () => client.on });
  const queue = new FlourishQueue();

  let buffList = [];
  let card = $state(null);
  let cardState = $state(null);
  let plate = $state(null);
  let cardTimer;
  let lastFit = '';

  const snap = $derived(client.snapshot);
  const settings = $derived(client.settings ?? {});
  const hasSession = $derived(!!(snap && (client.session?.file || snap.character)));
  const line = $derived(client.error && !client.polled
    ? { text: 'Waiting for ForgePact', tone: 'warn' }
    : overlayState({ on: client.on, session: client.session, snapshot: snap, catching_up: client.catching_up }));
  const compact = $derived(settings.compact === true);
  const showTicker = $derived(settings.ticker === true && !compact);
  const locked = $derived(settings.locked === true);
  const transparent = $derived(client.overlay?.transparent === true);
  const scale = $derived(clamp(settings.scale, 0.5, 2, 1));
  const plateOpacity = $derived(clamp(settings.opacity, 0.9, 1, 1));
  const metrics = $derived(['gold', 'xp', 'kills'].map((key) => ({
    key,
    label: { gold: 'Gold', xp: 'Experience', kills: 'Kills' }[key],
    value: fmt(snap?.[key]?.earned ?? 0),
    rate: rate(snap?.[key]?.per_hour ?? 0),
  })));
  const drops = $derived(RARITIES.map((name) => ({ name, cls: RARITY_CLASS[name], total: fmt(snap?.items?.[name]?.total ?? 0) })));
  const zone = $derived(snap?.satanic_zone?.zone ? zoneName(snap.satanic_zone.zone) : null);
  const ticker = $derived(showTicker ? tickerRows(client.extra?.drops) : []);

  function clamp(value, low, high, fallback) {
    return typeof value === 'number' && Number.isFinite(value) ? Math.min(high, Math.max(low, value)) : fallback;
  }

  // The card on show now, and a timer for when that next changes.
  function showCard() {
    const now = Date.now();
    const view = queue.view(now);
    card = view.card ? { ...view.card } : null;
    cardState = view.state;
    clearTimeout(cardTimer);
    const at = queue.nextAt(now);
    if (at != null) cardTimer = setTimeout(showCard, Math.max(0, at - Date.now()));
  }

  // A new journal: the entries that arrived since the overlay opened, with `flourish`, raise a card.
  $effect(() => {
    const journal = client.extra?.drops;
    if (!journal) return;
    untrack(() => {
      queue.takeJournal(journal, Date.now(), cardMs(client.settings));
      showCard();
    });
  });

  // A new zone rotation raises a card while the drop card and its zone switch are on.
  $effect(() => {
    const alert = client.zone_alert;
    if (!client.polled) return;
    untrack(() => {
      const s = client.settings ?? {};
      queue.takeZone(alert, Date.now(), cardMs(s), s.flourish !== false && s.flourish_zone !== false, buffList);
      showCard();
    });
  });

  // A transparent window shows the game around the plate; a solid one is the plate's colour throughout.
  $effect(() => {
    document.documentElement.classList.toggle('ovl-see-through', transparent);
  });

  // pywebview's bridge, when this page is in ForgePact's own overlay window.
  const bridge = () => window.pywebview?.api ?? null;

  // The window takes the plate's size, scale included.
  function fit() {
    if (!plate) return;
    const box = plate.getBoundingClientRect();
    const size = `${Math.ceil(box.width)}x${Math.ceil(box.height)}`;
    const api = bridge();
    if (!api?.fit || size === lastFit) return;
    lastFit = size;
    Promise.resolve(api.fit(Math.ceil(box.width), Math.ceil(box.height))).catch(() => { lastFit = ''; });
  }

  $effect(() => {
    if (!plate) return;
    const observer = new ResizeObserver(fit);
    observer.observe(plate);
    return () => observer.disconnect();
  });

  // A drag ends wherever the button comes up: save the place then.
  let pressed = false;
  function down(e) {
    pressed = e.button === 0 && !locked;
  }
  function up() {
    if (!pressed) return;
    pressed = false;
    Promise.resolve(bridge()?.place_done?.()).catch(() => {});
  }

  async function loadState() {
    try {
      const res = await fetch('/api/state');
      const state = await res.json();
      applyTheme(state?.cfg?.theme);
      if (Array.isArray(state?.satanicBuffs)) buffList = state.satanicBuffs;
    } catch (e) {
      // The default palette stays; zone cards name buffs by number.
    }
  }

  onMount(() => {
    loadState();
    client.start();
    const settingsTimer = setInterval(() => {
      if (client.polled && document.visibilityState === 'visible') client.loadSettings();
    }, SETTINGS_MS);
    const ready = () => { lastFit = ''; fit(); };
    window.addEventListener('pywebviewready', ready);
    window.addEventListener('mouseup', up);
    return () => {
      client.stop();
      clearInterval(settingsTimer);
      clearTimeout(cardTimer);
      window.removeEventListener('pywebviewready', ready);
      window.removeEventListener('mouseup', up);
    };
  });
</script>

<!-- svelte-ignore a11y_no_static_element_interactions -->
<div class="ovl" id="ovl" class:transparent class:pywebview-drag-region={!locked} class:locked
  style:zoom={scale} style:--ovl-plate={transparent ? `${Math.round(plateOpacity * 100)}%` : '100%'}
  aria-label="Tracker overlay" bind:this={plate} onmousedown={down}
  data-polled={client.polled ? '1' : ''} data-revision={snap?.revision ?? ''}
  data-journal={client.extra ? String(client.extra.drops?.length ?? 0) : ''}>
  <div class="ovl-top">
    <span class="ovl-state" id="ovlState"><span class="trk-dot {line.tone}" aria-hidden="true"></span>{line.text}{#if hasSession}<span class="ovl-clock" id="ovlClock">{clock(snap.session_secs ?? 0)}</span>{/if}</span>
    {#if hasSession && compact}
      <span class="ovl-zone">SS <b>{fmt(snap.ss ?? 0)}</b></span>
    {:else if hasSession && zone}
      <span class="ovl-zone" title={zone}>Satanic zone <b>{zone}</b>{#if snap.satanic_here}· here{/if}</span>
    {/if}
  </div>

  {#if hasSession}
    <div class="ovl-metrics">
      {#each metrics as m (m.key)}
        <div class="ovl-metric" data-key={m.key}><span>{m.label}</span><strong>{m.value}</strong><em>{m.rate}</em></div>
      {/each}
    </div>
    {#if !compact}
      <div class="ovl-drops">
        {#each drops as d (d.name)}
          <span class="ovl-drop" data-rarity={d.name}><span class={d.cls}>{d.name}</span><strong>{d.total}</strong></span>
        {/each}
        <span class="ovl-drop" data-rarity="SS">SS<strong>{fmt(snap.ss ?? 0)}</strong></span>
      </div>
    {/if}
    {#if showTicker}
      <ol class="ovl-ticker" id="ovlTicker" aria-label="Newest drops">
        {#each ticker as row (row.key)}
          <li><span class="trk-time">{row.time}</span><span class="trk-item {row.cls}">{row.name}</span>{#if row.grade}<span class="trk-tag" title={`Grade ${row.grade}`}>{row.grade}</span>{:else}<span></span>{/if}</li>
        {:else}
          <li class="ovl-ticker-empty"><span></span><span class="trk-empty">No drops yet this session.</span><span></span></li>
        {/each}
      </ol>
    {/if}
    {#if card}
      {#key card.key}
        <div class="ovl-flourish {card.cls}" id="ovlFlourish" data-state={cardState} data-kind={card.kind} role="status">
          <span class="ovl-flourish-edge" aria-hidden="true"></span>
          <span class="ovl-flourish-line"><span class="ovl-flourish-name">{card.name}</span><span class="ovl-flourish-meta">{card.meta}</span></span>
          {#if card.count > 1}<span class="ovl-flourish-count" aria-label={`${card.count} drops`}>×{card.count}</span>{:else}<span></span>{/if}
        </div>
      {/key}
    {/if}
  {:else}
    <p class="trk-empty ovl-waiting">Load a character with the tracker on. The counters start at the first record.</p>
  {/if}
</div>
