<script>
  // The Tracker tab's Alerts section (#185): what makes a sound and how. Ported
  // from HS Offline Tracker's Alerts and Settings views (MIT, Q7(b); the
  // notice is in CREDITS.md) into ForgePact's design (panel/DESIGN.md §
  // Tracker, design/tracker/tab.html "Alerts"):
  //
  // - Alert sounds: one row per rarity and one for the zone rotation
  //   (settings/SoundRow.svelte), the lowest grade, and the note a page shows
  //   while it may not play sound yet;
  // - Zone rotations worth a sound (settings/BuffPick.svelte);
  // - Sound lists, the filter editor (settings/SoundFilter.svelte);
  // - Notable finds (settings/NotableGroups.svelte);
  // - Discord status, only while `/api/tracker`'s `presence` says it is
  //   available (an application ID is set); otherwise the switch is not drawn.
  //
  // Every change goes through `write`: one queue, so two quick changes never
  // race, and each a read-modify-write of the whole document
  // (client.updateSettings): GET it, apply one edit from settings/edit.js,
  // POST all of it. Unknown fields, and what another window wrote meanwhile,
  // survive. Nothing is changed on the page before the server agrees; the
  // controls show what the document holds. Custom sounds go to
  // `POST /api/tracker/sound/<key>`; Test previews through audio.js.
  //
  // The overlay's and the drop card's settings are OverlaySettings.svelte's.
  import { onMount } from 'svelte';
  import { ST } from '../state.svelte.js';
  import { audioState } from './audio.js';
  import { GRADE_OPTIONS, RARITY_KEYS, RARITY_NAMES, setMinTier } from './settings/edit.js';
  import SoundRow from './settings/SoundRow.svelte';
  import BuffPick from './settings/BuffPick.svelte';
  import SoundFilter from './settings/SoundFilter.svelte';
  import NotableGroups from './settings/NotableGroups.svelte';

  let { client } = $props();

  const doc = $derived(client.settings);
  const loaded = $derived(!!doc);
  const presence = $derived(client.presence?.available === true);
  // `/api/state`'s buff list ([id, name, description] rows), read once: the
  // page fetched it at boot and it does not change while the panel runs.
  const buffs = (Array.isArray(ST?.satanicBuffs) ? ST.satanicBuffs : []).filter((row) => Array.isArray(row) && Number.isInteger(Number(row[0])));
  let soundError = $state(null);
  let gradeError = $state(null);
  let discordError = $state(null);
  let silent = $state(false);

  let chain = Promise.resolve();
  /** Queue one edit of the settings document; answers `{err}` or `{ok, settings}`. */
  function write(edit) {
    const run = async () => {
      try {
        const res = await client.updateSettings((d) => { edit(d); return d; });
        return res.err ? { err: res.err } : { ok: true, settings: res.settings };
      } catch (e) {
        return { err: e?.message || String(e) };
      }
    };
    const next = chain.then(run, run);
    chain = next.catch(() => {});
    return next;
  }

  onMount(() => {
    // Read fresh on opening: another window may have changed either.
    client.loadSettings();
    client.loadSounds();
    // The note says whether this page may play yet; a click or key is what unlocks it.
    const check = () => { silent = audioState() === 'suspended'; };
    const later = () => setTimeout(check, 300);
    check();
    for (const name of ['pointerdown', 'keydown']) document.addEventListener(name, later, true);
    return () => { for (const name of ['pointerdown', 'keydown']) document.removeEventListener(name, later, true); };
  });

  async function grade(select) {
    const res = await write((d) => setMinTier(d, select.value));
    if (res.err) { select.value = String(doc?.min_tier ?? 0); gradeError = res.err; } else gradeError = null;
  }

  async function discord(box) {
    const res = await write((d) => { d.discord = box.checked; return d; });
    if (res.err) { box.checked = !!doc?.discord; discordError = res.err; } else discordError = null;
  }
</script>

<section class="trk-section" data-section="alerts" aria-label="Alerts" data-loaded={loaded ? '' : undefined}>
  {#if doc}
    <section class="card" aria-labelledby="trkSoundsTitle">
      <h2 id="trkSoundsTitle">Alert sounds</h2>
      <p class="hint">A sound plays once for each drop of a rarity you turn on, from this window, whichever tab is open. Turning a rarity off here also keeps it off the overlay's drop card.</p>
      <!-- Always there, so the note is announced when it appears; empty, it takes no room. -->
      <div class="trk-live" role="status">{#if silent}<p class="trk-gesture"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2.5 14.5 13.5h-13ZM8 6.5v3.2M8 11.8v.1"/></svg>Click anywhere in the panel to let it play alert sounds.</p>{/if}</div>
      {#each RARITY_KEYS as key (key)}
        <SoundRow {client} {key} label={RARITY_NAMES[key]} rarityClass={'r-' + key} {write} onerror={(m) => (soundError = m)} />
      {/each}
      <SoundRow {client} key="zone" label="Zone rotation" {write} onerror={(m) => (soundError = m)} />
      {#if soundError}
        <p class="note trk-sound-src trk-error" role="alert"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2.5 14.5 13.5h-13ZM8 6.5v3.2M8 11.8v.1"/></svg>{soundError}</p>
      {/if}
      <div class="row"><span class="lbl">Lowest grade</span>
        <select class="style-select" id="trkMinTier" aria-label="Lowest grade that alerts" value={String(doc.min_tier ?? 0)} onchange={(e) => grade(e.currentTarget)}>
          {#each GRADE_OPTIONS as [value, label] (value)}<option value={String(value)}>{label}</option>{/each}
        </select>
        <span class="hint">Gear below this grade stays quiet. A list's items always play.</span>
      </div>
      {#if gradeError}<p class="note trk-error" role="alert"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2.5 14.5 13.5h-13ZM8 6.5v3.2M8 11.8v.1"/></svg>{gradeError}</p>{/if}
    </section>
    <BuffPick {client} {buffs} {write} />
    <SoundFilter {client} {write} />
    <NotableGroups {client} {write} />
    {#if presence}
      <section class="card trk-half" aria-labelledby="trkDiscordTitle" id="trkDiscordCard">
        <h2 id="trkDiscordTitle">Discord status</h2>
        <p class="hint">Shows the zone, the difficulty, this run's finds and how long it has been going under your name in Discord, while the game is open. Discord must run on this computer.</p>
        <div class="row"><span class="lbl">Show in Discord</span><label class="switch"><input type="checkbox" id="trkDiscord" checked={!!doc.discord} aria-label="Show in Discord" onchange={(e) => discord(e.currentTarget)}><span class="sl"></span></label></div>
        {#if discordError}<p class="note trk-error" role="alert"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2.5 14.5 13.5h-13ZM8 6.5v3.2M8 11.8v.1"/></svg>{discordError}</p>{/if}
      </section>
    {/if}
  {:else if client.error}
    <section class="card"><h2>Alerts</h2><p class="note trk-error" role="alert"><svg viewBox="0 0 16 16" aria-hidden="true"><path d="M8 2.5 14.5 13.5h-13ZM8 6.5v3.2M8 11.8v.1"/></svg>{client.error}</p></section>
  {/if}
</section>

<style>
  .trk-gesture {
    display: flex;
    align-items: center;
    gap: var(--space-4);
    margin: 0;
    padding: var(--space-4) var(--space-6);
    border-radius: var(--radius-md);
    background-color: var(--color-warn-tint);
    color: var(--color-text-primary);
    font-size: var(--font-size-sm);
  }
  .trk-gesture svg { flex: none; width: 16px; height: 16px; color: var(--color-warn); stroke: currentColor; stroke-width: 1.4; fill: none; stroke-linecap: round; stroke-linejoin: round; }
  .trk-sound-src { margin: 0; padding-left: calc(168px + var(--space-7)); }
  @media (max-width: 1100px) {
    .trk-sound-src { padding-left: calc(152px + var(--space-7)); }
  }
</style>
