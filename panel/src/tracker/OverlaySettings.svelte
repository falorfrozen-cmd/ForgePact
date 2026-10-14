<script>
  // The Tracker tab's Overlay section (#185): the overlay window (open and
  // close it, open it with the game, lock it in place, the newest drops,
  // counters only, its opacity and size) and the drop card (the Tracker's
  // flourish: which drops raise it and for how long). Ported from HS Offline
  // Tracker's Settings view (MIT; the notice is in CREDITS.md) into
  // ForgePact's design (design/tracker/tab.html, "Overlay").
  //
  // Every change is a read-modify-write of the whole settings document
  // through the client Tracker.svelte hands in, one at a time, so a field
  // this section does not know is kept. The overlay page reads them again
  // within a few seconds. The window itself is src/tracker_overlay.py's: the
  // Open button is offered only while `/api/tracker`'s `overlay.available`
  // says the panel runs in its own window, and says why when it does not.
  //
  // Left out on purpose: the Tracker's `flourish_scale`, `flourish_shade` and
  // `flourish_always` sized, shaded and held open the separate full-screen
  // window its flourish drew in. ForgePact's drop card draws inside the
  // overlay instead (that window is out of this release), so a switch for
  // them would do nothing; the document keeps whatever they hold. The
  // Discord switch is the Alerts section's.
  import { onMount } from 'svelte';
  import { RARITIES, RARITY_CLASS } from './format.js';

  let { client } = $props();

  // The drop card's lowest grade (`flourish_tier`): D means any grade.
  const TIERS = [[1, 'Any'], [2, 'C'], [3, 'B'], [4, 'A'], [5, 'S'], [6, 'SS']];

  const s = $derived(client.settings ?? {});
  const loaded = $derived(client.settings != null);
  const overlay = $derived(client.overlay);
  const available = $derived(overlay?.available === true);
  const isOpen = $derived(overlay?.open === true);
  const transparent = $derived(overlay?.transparent === true);
  const flourishOn = $derived(s.flourish !== false);
  const rarities = $derived(Array.isArray(s.flourish_rarities) ? s.flourish_rarities : RARITIES);

  let busy = $state(false);
  let failure = $state(null);
  // A range's value while it is being dragged, before the change is written.
  let draft = $state({});
  let pending = Promise.resolve();

  const pct = (value, fallback) => Math.round((typeof value === 'number' && Number.isFinite(value) ? value : fallback) * 100);
  const scalePct = $derived(draft.scale ?? pct(s.scale, 1));
  const opacityPct = $derived(draft.opacity ?? pct(s.opacity, 1));
  const secs = $derived(draft.flourish_secs ?? (typeof s.flourish_secs === 'number' ? Math.round(s.flourish_secs) : 4));

  onMount(() => { client.loadSettings(); });

  // One write at a time: two read-modify-writes in flight would each keep the
  // document the other had not changed yet.
  function write(change) {
    pending = pending.then(async () => {
      const res = await client.updateSettings(change);
      failure = res.err ?? null;
      return res;
    });
    return pending;
  }

  async function toggle(key, e) {
    const box = e.currentTarget;
    const res = await write({ [key]: box.checked });
    if (res.err) box.checked = !box.checked;
  }

  async function toggleRarity(name, e) {
    const box = e.currentTarget;
    const on = box.checked;
    const res = await write((doc) => {
      const list = Array.isArray(doc.flourish_rarities) ? doc.flourish_rarities : RARITIES;
      doc.flourish_rarities = RARITIES.filter((r) => (r === name ? on : list.includes(r)));
    });
    if (res.err) box.checked = !on;
  }

  // A range writes when it is let go of (or typed), not on every step it passes.
  async function setRange(key, value, toDoc) {
    draft = { ...draft, [key]: value };
    await write({ [key]: toDoc(value) });
    const { [key]: _, ...rest } = draft;
    draft = rest;
  }

  const scaleDoc = (p) => Math.round(p) / 100;

  async function openOrClose() {
    if (busy || !available) return;
    busy = true;
    failure = null;
    try {
      const res = await client.setOverlay(!isOpen);
      if (res.err) failure = res.err;
    } finally {
      busy = false;
    }
  }
</script>

<section class="trk-section" data-section="overlay" aria-label="Overlay" id="trkOverlaySettings" data-loaded={loaded ? '1' : ''}>
  <section class="card trk-half" id="trkOverlayCard" aria-labelledby="trkOverlayTitle">
    <div class="trk-head">
      <h2 id="trkOverlayTitle">Overlay window</h2>
      <button type="button" class="btn primary" id="trkOverlayOpen" disabled={!available || busy}
        aria-describedby={overlay && !available ? 'trkOverlayReason' : undefined} onclick={openOrClose}>{isOpen ? 'Close overlay' : 'Open overlay'}</button>
    </div>
    <p class="hint">A small window that stays above the game with the counters, the newest drops and a card for the drops you ask to see. Play windowed or borderless: a full-screen game draws over it.</p>
    {#if overlay && !available}
      <p class="trk-state" id="trkOverlayReason"><span class="trk-dot" aria-hidden="true"></span>The overlay needs ForgePact's own window: it opens beside the panel when ForgePact runs as its app, not from a browser tab.</p>
    {/if}
    {#if failure}<p class="trk-state" id="trkOverlayError" role="alert"><span class="trk-dot warn" aria-hidden="true"></span>{failure}</p>{/if}

    <div class="row"><span class="lbl">Open with the game</span><label class="switch"><input type="checkbox" id="trkOvlAutoShow" aria-label="Open the overlay with the game" checked={s.auto_show !== false} disabled={!loaded} onchange={(e) => toggle('auto_show', e)}><span class="sl"></span></label></div>
    <div class="row"><span class="lbl">Lock in place</span><label class="switch"><input type="checkbox" id="trkOvlLocked" aria-label="Lock the overlay in place" checked={s.locked === true} disabled={!loaded} onchange={(e) => toggle('locked', e)}><span class="sl"></span></label></div>
    <div class="row"><span class="lbl">Newest drops</span><label class="switch"><input type="checkbox" id="trkOvlTicker" aria-label="Show the newest drops" checked={s.ticker === true} disabled={!loaded} onchange={(e) => toggle('ticker', e)}><span class="sl"></span></label></div>
    <div class="row"><span class="lbl">Counters only</span><label class="switch"><input type="checkbox" id="trkOvlCompact" aria-label="Counters only" checked={s.compact === true} disabled={!loaded} onchange={(e) => toggle('compact', e)}><span class="sl"></span></label></div>
    <div class="row">
      <span class="lbl">Opacity</span>
      <div class="range-control">
        <input type="range" id="trkOvlOpacity" min="90" max="100" step="1" value={Math.min(100, Math.max(90, opacityPct))} aria-label="Overlay opacity"
          disabled={!loaded || !transparent} aria-describedby={transparent ? undefined : 'trkOvlSolidNote'}
          oninput={(e) => { draft = { ...draft, opacity: Number(e.currentTarget.value) }; }}
          onchange={(e) => setRange('opacity', Number(e.currentTarget.value), scaleDoc)}>
        <div class="value-stepper"><span class="val">{Math.min(100, Math.max(90, opacityPct))}%</span></div>
      </div>
    </div>
    <div class="row">
      <span class="lbl">Size</span>
      <div class="range-control">
        <input type="range" id="trkOvlScale" min="50" max="200" step="5" value={scalePct} aria-label="Overlay size" disabled={!loaded}
          oninput={(e) => { draft = { ...draft, scale: Number(e.currentTarget.value) }; }}
          onchange={(e) => setRange('scale', Number(e.currentTarget.value), scaleDoc)}>
        <div class="value-stepper">
          <button type="button" class="step-button" id="trkOvlScaleLess" aria-label="Smaller" disabled={!loaded || scalePct <= 50} onclick={() => setRange('scale', Math.max(50, scalePct - 5), scaleDoc)}>−</button>
          <span class="val">{scalePct}%</span>
          <button type="button" class="step-button" id="trkOvlScaleMore" aria-label="Larger" disabled={!loaded || scalePct >= 200} onclick={() => setRange('scale', Math.min(200, scalePct + 5), scaleDoc)}>+</button>
        </div>
      </div>
    </div>
    {#if !transparent}
      <p class="note" id="trkOvlSolidNote">The overlay is a solid plate: ForgePact's window cannot be made see-through on Windows yet, so there is no opacity to set.</p>
    {/if}
  </section>

  <section class="card trk-half" id="trkFlourishCard" aria-labelledby="trkFlourishTitle">
    <div class="trk-head">
      <h2 id="trkFlourishTitle">Drop card</h2>
      <label class="switch"><input type="checkbox" id="trkFlourishOn" aria-label="Show a card for chosen drops" checked={flourishOn} disabled={!loaded} onchange={(e) => toggle('flourish', e)}><span class="sl"></span></label>
    </div>
    <p class="hint">The overlay shows a card for a few seconds when one of these drops. The journal keeps every drop either way.</p>
    <div class="ovs-picks">
      {#each RARITIES as name (name)}
        <label class="ovs-pick"><span class={RARITY_CLASS[name]}>{name}</span><span class="switch"><input type="checkbox" id={'trkFlourish-' + name} aria-label={name} checked={rarities.includes(name)} disabled={!loaded || !flourishOn} onchange={(e) => toggleRarity(name, e)}><span class="sl"></span></span></label>
      {/each}
      <label class="ovs-pick">Items on a sound list<span class="switch"><input type="checkbox" id="trkFlourishListed" aria-label="Items on a sound list" checked={s.flourish_listed === true} disabled={!loaded || !flourishOn} onchange={(e) => toggle('flourish_listed', e)}><span class="sl"></span></span></label>
      <label class="ovs-pick">Zone rotations<span class="switch"><input type="checkbox" id="trkFlourishZone" aria-label="Zone rotations" checked={s.flourish_zone !== false} disabled={!loaded || !flourishOn} onchange={(e) => toggle('flourish_zone', e)}><span class="sl"></span></span></label>
    </div>
    <div class="row">
      <span class="lbl">Lowest grade</span>
      <select class="style-select" id="trkFlourishTier" aria-label="Lowest grade for a card" disabled={!loaded || !flourishOn}
        value={String(s.flourish_tier ?? 1)} onchange={(e) => write({ flourish_tier: Number(e.currentTarget.value) })}>
        {#each TIERS as [tier, label] (tier)}<option value={String(tier)}>{label}</option>{/each}
      </select>
    </div>
    <div class="row">
      <span class="lbl">Shown for</span>
      <div class="range-control">
        <input type="range" id="trkFlourishSecs" min="2" max="10" step="1" value={secs} aria-label="Card duration" disabled={!loaded || !flourishOn}
          oninput={(e) => { draft = { ...draft, flourish_secs: Number(e.currentTarget.value) }; }}
          onchange={(e) => setRange('flourish_secs', Number(e.currentTarget.value), (v) => v)}>
        <div class="value-stepper">
          <button type="button" class="step-button" aria-label="Shorter" disabled={!loaded || !flourishOn || secs <= 2} onclick={() => setRange('flourish_secs', Math.max(2, secs - 1), (v) => v)}>−</button>
          <span class="val">{secs} s</span>
          <button type="button" class="step-button" aria-label="Longer" disabled={!loaded || !flourishOn || secs >= 10} onclick={() => setRange('flourish_secs', Math.min(10, secs + 1), (v) => v)}>+</button>
        </div>
      </div>
    </div>
  </section>
</section>

<style>
  /* The drop card's switches: compact pairs in two columns, 40px apart so a
     switch is never read as the next column's (design/tracker README, P3). */
  .ovs-picks { display: grid; grid-template-columns: repeat(2, minmax(0, 1fr)); gap: var(--space-2) var(--space-11); }
  .ovs-pick { display: flex; align-items: center; justify-content: space-between; gap: var(--space-6); min-height: 36px; font-size: var(--font-size-sm); }
  @media (max-width: 720px) { .ovs-picks { grid-template-columns: minmax(0, 1fr); } }
</style>
