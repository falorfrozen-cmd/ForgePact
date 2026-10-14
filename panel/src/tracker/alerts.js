// Rarity and zone alerts (#185): the main window plays each journal entry and
// each Satanic zone rotation once. Tracker.svelte, which is always mounted
// whichever tab is open, calls startAlerts(client) with its one TrackerClient;
// the overlay page never does, so it neither claims nor plays.
//
// - Baseline. On the page's first `/extra`, every journal id there (and the
//   zone alert's `zone:<seq>`, from the first `/api/tracker` answer) counts as
//   seen. Nothing seen plays, so opening the panel mid-session is quiet.
// - Claim, then play. Each unseen entry with `announce` and a `sound` is
//   claimed through `POST /api/tracker/announce`, and played only on
//   `{"play": true}`, so two windows of the panel never both play an id.
// - Only a page that can play claims. Its AudioContext must be running;
//   otherwise it claims nothing, shows a quiet "click to let it play alert
//   sounds" note in the switch card, and tries again on the next click or
//   key. An id is never marked announced while this page is silent.
// - Sound and volume come from the settings document (`<rarity>.enabled` and
//   `.volume`, `zone`, or the active filter's list for a `list-<id>`), the
//   file from `/api/tracker/sounds` when the player chose one, else the
//   bundled one (audio.js). A list with no file borrows the item's rarity.
// - Nothing happens while the switch is off: the client does not poll then
//   either, so no request goes out.
//
// The client is reactive state (Svelte runes) and this is a plain module, so
// a quarter-second timer compares what the client holds; it costs a few
// property reads while the switch is on and nothing else. The pure decision
// (baselineIds, channelFor, decideAlerts) is exported for
// tests/tracker-alerts.test.js; the browser half is
// tests/tracker/alerts.check.mjs.

import { audioState, play, preload, soundUrl, BUNDLED, SOUND_KEYS, unlock } from './audio.js';

const CHECK_MS = 250;
const DEFAULT_VOLUME = 0.5; // the store's default_settings()
const NOTE_ID = 'trackerAudioNote';
const NOTE_TEXT = 'Click anywhere in the panel to let it play alert sounds.';

/** The ids a page counts as seen on its first answer: the journal's and `zone:<seq>`. */
export function baselineIds(drops, zoneAlert) {
  const ids = (drops ?? []).map((d) => d?.id).filter((id) => typeof id === 'string' && id);
  if (zoneAlert && Number.isFinite(zoneAlert.seq)) ids.push(`zone:${zoneAlert.seq}`);
  return ids;
}

/** The settings that rule a sound key: `<rarity>`/`zone`, or the active filter's list for `list-<id>`. */
export function channelFor(settings, key) {
  if (!settings || !key) return null;
  if (!key.startsWith('list-')) return settings[key] ?? null;
  const active = (settings.filters ?? []).find((f) => f?.id === settings.filter);
  return (active?.lists ?? []).find((l) => `list-${l?.id}` === key) ?? null;
}

function alertFor(id, key, rarity, settings, sounds) {
  const c = channelFor(settings, key);
  if (c && c.enabled === false) return null;
  const url = soundUrl(key, sounds, rarity);
  if (!url) return null;
  const volume = Number.isFinite(c?.volume) ? c.volume : DEFAULT_VOLUME;
  return { id, key, url, volume, priority: key, fallback: BUNDLED[key] ?? BUNDLED[rarity] ?? null };
}

/**
 * What a page does with what the client holds now.
 *
 * `claim`: the unseen alerts to claim, oldest first, each with its `url`,
 * `volume`, mixer `priority` and `fallback` URL; empty while `running` is
 * false. `skip`: unseen ids that make no sound here (no sound, not announced,
 * already claimed by another page, switched off), which the page counts as
 * seen without a claim. `waiting`: how many would be claimed if the page could
 * play.
 */
export function decideAlerts({ drops = [], zoneAlert = null, seen, settings = null, sounds = null, running = true }) {
  const due = [];
  const skip = [];
  // The journal is newest first; play in the order they dropped.
  for (const d of [...(drops ?? [])].reverse()) {
    const id = d?.id;
    if (typeof id !== 'string' || !id || seen.has(id)) continue;
    const key = typeof d.sound === 'string' && d.sound ? d.sound : null;
    const rarity = typeof d.rarity === 'string' ? d.rarity.toLowerCase() : null;
    const alert = d.announce && !d.announced && key ? alertFor(id, key, rarity, settings, sounds) : null;
    if (alert) due.push(alert); else skip.push(id);
  }
  if (zoneAlert && Number.isFinite(zoneAlert.seq)) {
    const id = `zone:${zoneAlert.seq}`;
    if (!seen.has(id)) {
      const alert = zoneAlert.announced ? null : alertFor(id, 'zone', null, settings, sounds);
      if (alert) due.push(alert); else skip.push(id);
    }
  }
  return { claim: running ? due : [], skip, waiting: due.length };
}

/** Start playing alerts for `client`; answers the function that stops it. */
export function startAlerts(client) {
  const seen = new Set();
  let journalBaseline = false;
  let zoneBaseline = false;
  let busy = false;
  let stopped = false;
  let preloaded = '';
  let triedUngestured = false;

  function note(show) {
    const existing = document.getElementById(NOTE_ID);
    if (!show) { existing?.remove(); return; }
    if (existing) return;
    const card = document.getElementById('trackerCard');
    if (!card) return;
    const p = document.createElement('p');
    p.id = NOTE_ID;
    p.className = 'trk-state';
    p.setAttribute('role', 'status');
    const dot = document.createElement('span');
    dot.className = 'trk-dot warn';
    dot.setAttribute('aria-hidden', 'true');
    p.append(dot, NOTE_TEXT);
    card.append(p);
  }

  // Warm the files this page would play, once the context runs and whenever the listing changes.
  function warm() {
    const urls = SOUND_KEYS.map((key) => soundUrl(key, client.sounds));
    const sig = urls.join('|');
    if (sig === preloaded) return;
    preloaded = sig;
    void preload(urls);
  }

  async function step() {
    if (busy || stopped) return;
    if (!client.on || !client.polled) { note(false); return; }
    if (!zoneBaseline) {
      for (const id of baselineIds(null, client.zone_alert)) seen.add(id);
      zoneBaseline = true;
    }
    if (!client.extra) return;
    if (!journalBaseline) {
      for (const id of baselineIds(client.extra.drops, null)) seen.add(id);
      journalBaseline = true;
    }
    busy = true;
    try {
      // Asked once without a gesture: that runs where the page may autoplay
      // (ForgePact's own window); elsewhere the next click or key does it.
      let running = audioState() === 'running';
      if (!running && !triedUngestured) { triedUngestured = true; running = await unlock(); }
      note(!running);
      if (running) warm();
      const { claim, skip } = decideAlerts({
        drops: client.extra.drops, zoneAlert: client.zone_alert, seen,
        settings: client.settings, sounds: client.sounds, running,
      });
      for (const id of skip) seen.add(id);
      for (const alert of claim) {
        if (stopped) break;
        // Seen before the claim: a second claim of an id would only answer false.
        seen.add(alert.id);
        const res = await client.announce(alert.id);
        if (res?.play === true) void play(alert.url, alert.volume, alert.priority, alert.fallback);
      }
    } finally {
      busy = false;
    }
  }

  // A click or a key is the gesture a browser wants before it lets a page play.
  function gesture() {
    if (stopped || !client.on || audioState() === 'running') return;
    unlock().then((running) => { if (running) step(); });
  }

  const GESTURES = ['pointerdown', 'click', 'keydown'];
  const timer = setInterval(step, CHECK_MS);
  for (const name of GESTURES) document.addEventListener(name, gesture, true);
  return function stopAlerts() {
    stopped = true;
    clearInterval(timer);
    for (const name of GESTURES) document.removeEventListener(name, gesture, true);
    note(false);
  };
}
