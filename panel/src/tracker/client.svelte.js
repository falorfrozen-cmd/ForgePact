// The Tracker tab's one road to `/api/tracker*` (#185): reactive state the
// sections read, and the calls they make. Tracker.svelte makes one client and
// hands it to every section (and to startAlerts); the overlay page makes its
// own with `overlay: true`.
//
// When it polls. `/api/tracker` at most once a second, and only while the
// `mod_tracker` switch is on, the Tracker tab is open, or this is the overlay
// page. With the switch off and another tab open the page sends no
// `/api/tracker` request at all, which keeps `e2e:perf` and the behaviour
// oracle as they were: the one-second timer then only asks the two functions
// it was given. A poll fetches `/extra` when `extra_revision` changes and
// `/runs` when `revision` changes while the Runs section is open, or when the
// section opens. The settings and the custom sounds are read once, on the
// first answer, and after every change made here.
//
// Settings are read-modify-write: GET the whole document, change it, POST the
// whole of it, so a field this page does not know (or another window changed)
// is kept.
//
// The pure parts (shouldPoll, pollWait, fetchPlan, mergeSettings,
// readModifyWrite, stateLine) are exported for tests/tracker-client.test.js,
// which imports this file under plain node: nothing at module level may call
// a rune or touch the DOM.

export const POLL_MS = 1000;

/** Whether the page may poll `/api/tracker` now. */
export function shouldPoll({ on = false, tabOpen = false, overlay = false } = {}) {
  return !!(on || tabOpen || overlay);
}

/** How long to wait before the next poll, given when the last one started. */
export function pollWait(lastAt, now) {
  if (lastAt == null) return 0;
  return Math.max(0, lastAt + POLL_MS - now);
}

/** Which of `/extra` and `/runs` an answer of `/api/tracker` calls for. */
export function fetchPlan(seen, body, runsOpen) {
  return {
    extra: body.extra_revision !== seen.extraRevision,
    runs: !!runsOpen && body.revision !== seen.runsRevision,
  };
}

const isPlain = (v) => v !== null && typeof v === 'object' && !Array.isArray(v);

function mergeInto(base, patch) {
  const out = { ...base };
  for (const [key, value] of Object.entries(patch)) {
    out[key] = isPlain(value) && isPlain(base?.[key]) ? mergeInto(base[key], value) : value;
  }
  return out;
}

/**
 * The settings document with `change` applied, never changing `doc` itself.
 * `change` is a partial document (objects merge, anything else replaces) or a
 * function given a copy, which may change it in place or return a new one.
 */
export function mergeSettings(doc, change) {
  const copy = structuredClone(doc ?? {});
  if (typeof change === 'function') return change(copy) ?? copy;
  return mergeInto(copy, change ?? {});
}

/** GET the settings document, change it, POST the whole of it. */
export async function readModifyWrite(fetchJson, change) {
  const read = await fetchJson('/api/tracker/settings');
  if (read.err) return read;
  const next = mergeSettings(read.settings, change);
  return fetchJson('/api/tracker/settings', {
    method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(next),
  });
}

/** The switch card's one state line: its text and its dot's tone (off, on, warn). */
export function stateLine({ on, snapshot = null, session = null, catching_up = false } = {}) {
  const name = typeof snapshot?.character?.name === 'string' && snapshot.character.name ? snapshot.character.name : null;
  if (!on) return { text: name ? `Off · last session: ${name}` : 'Off', tone: 'off' };
  if (!session?.live) return { text: name ? `On · waiting for the game · last session: ${name}` : 'On · waiting for the game', tone: 'off' };
  const whose = name ? `${name}’s session` : 'this session';
  if (catching_up) return { text: `Catching up on ${whose}`, tone: 'warn' };
  if (snapshot?.paused) return { text: `Clock paused · ${whose}`, tone: 'warn' };
  return { text: `Recording ${whose}`, tone: 'on' };
}

async function fetchJsonDefault(path, init) {
  try {
    const res = await fetch(path, init);
    let body;
    try { body = await res.json(); } catch { body = {}; }
    if (!res.ok && !body.err) body.err = `Request failed (${res.status})`;
    return body;
  } catch {
    return { err: 'Could not reach ForgePact. Reconnect and try again.' };
  }
}

export class TrackerClient {
  // What /api/tracker answered last (`on` follows the switch between polls).
  on = $state(false);
  snapshot = $state(null);
  catching_up = $state(false);
  session = $state(null);
  zone_alert = $state(null);
  overlay = $state(null);
  presence = $state(null);
  // Fetched on their own revisions or on demand.
  extra = $state(null);
  runs = $state(null);
  settings = $state(null);
  sounds = $state(null);
  // Whether the first answer has come, and the last refusal or lost request.
  polled = $state(false);
  error = $state(null);
  // The Tracker tab's open section (Tracker.svelte sets it through openSection).
  section = $state('session');

  #switchOn;
  #tabOpen;
  #overlayPage;
  #fetchJson;
  #timer = null;
  #lastAt = null;
  #inFlight = false;
  #started = false;
  #seen = { revision: null, extraRevision: null, runsRevision: null };
  #unlisten = [];

  constructor({ switchOn = () => false, tabOpen = () => false, overlay = false, fetchJson = fetchJsonDefault } = {}) {
    this.#switchOn = switchOn;
    this.#tabOpen = tabOpen;
    this.#overlayPage = overlay;
    this.#fetchJson = fetchJson;
  }

  /** Start the one-second loop, and wake it when the tab or the settings change. */
  start() {
    if (this.#started) return;
    this.#started = true;
    const wake = () => this.wake();
    for (const name of ['forgepact:navigate', 'forgepact:settings', 'visibilitychange']) {
      document.addEventListener(name, wake);
      this.#unlisten.push(() => document.removeEventListener(name, wake));
    }
    this.wake();
  }

  stop() {
    this.#started = false;
    clearTimeout(this.#timer);
    this.#timer = null;
    for (const off of this.#unlisten.splice(0)) off();
  }

  /** Check now whether to poll, and poll if a second has passed since the last one. */
  wake() {
    if (!this.#started || this.#inFlight) return;
    clearTimeout(this.#timer);
    this.#timer = null;
    this.#tick();
  }

  #schedule(ms) {
    clearTimeout(this.#timer);
    if (this.#started) this.#timer = setTimeout(() => { this.#timer = null; this.#tick(); }, ms);
  }

  async #tick() {
    // Between polls the switch's saved setting says whether it is on; a poll's
    // answer says the same from the server.
    const on = !!this.#switchOn();
    this.on = on;
    if (!shouldPoll({ on, tabOpen: this.#tabOpen(), overlay: this.#overlayPage })) {
      this.#schedule(POLL_MS);
      return;
    }
    const wait = pollWait(this.#lastAt, Date.now());
    if (wait > 0) { this.#schedule(wait); return; }
    this.#inFlight = true;
    try {
      await this.poll();
    } finally {
      this.#inFlight = false;
      this.#schedule(POLL_MS);
    }
  }

  /** One `/api/tracker` read, and the `/extra` and `/runs` reads it calls for. */
  async poll() {
    this.#lastAt = Date.now();
    const body = await this.#fetchJson('/api/tracker');
    if (body.err) { this.error = body.err; return; }
    this.error = null;
    this.on = !!body.on;
    this.snapshot = body.snapshot ?? null;
    this.catching_up = !!body.catching_up;
    this.session = body.session ?? null;
    this.zone_alert = body.zone_alert ?? null;
    this.overlay = body.overlay ?? null;
    if ('presence' in body) this.presence = body.presence;
    this.#seen.revision = body.revision;
    const plan = fetchPlan(this.#seen, body, this.section === 'runs');
    const firstAnswer = !this.polled;
    this.polled = true;
    const reads = [];
    if (plan.extra) reads.push(this.#loadExtra(body.extra_revision));
    if (plan.runs) reads.push(this.loadRuns(body.revision));
    if (firstAnswer) reads.push(this.loadSettings(), this.loadSounds());
    await Promise.all(reads);
  }

  async #loadExtra(revision) {
    const extra = await this.#fetchJson('/api/tracker/extra');
    if (extra.err) { this.error = extra.err; return; }
    this.extra = extra;
    this.#seen.extraRevision = revision;
  }

  /** Tracker.svelte's sub-tabs: opening Runs reads the run history. */
  openSection(name) {
    this.section = name;
    if (name === 'runs') this.loadRuns();
  }

  #post(path, body = {}) {
    return this.#fetchJson(path, { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: JSON.stringify(body) });
  }

  // ---- Reads -------------------------------------------------------------

  async loadRuns(revision = this.#seen.revision) {
    const res = await this.#fetchJson('/api/tracker/runs');
    if (!res.err) { this.runs = res.runs ?? []; this.#seen.runsRevision = revision; }
    return res;
  }

  async loadSettings() {
    const res = await this.#fetchJson('/api/tracker/settings');
    if (!res.err) this.settings = res.settings ?? null;
    return res;
  }

  async loadSounds() {
    const res = await this.#fetchJson('/api/tracker/sounds');
    if (!res.err) this.sounds = res.sounds ?? {};
    return res;
  }

  loadShopping() { return this.#fetchJson('/api/tracker/shopping'); }

  loadCodex() { return this.#fetchJson('/api/tracker/codex'); }

  // ---- Changes: each answers the server's body, `err` on a refusal --------

  async #thenPoll(res) {
    if (!res.err && this.#started) { this.#lastAt = null; this.wake(); }
    return res;
  }

  /** End run: files the current run when it qualifies. */
  reset() { return this.#post('/api/tracker/reset').then((r) => this.#thenPoll(r)); }

  /** Pause or resume the session clock. */
  pause(on) { return this.#post('/api/tracker/pause', { on: !!on }).then((r) => this.#thenPoll(r)); }

  /** Change the settings: a partial document or a function (see mergeSettings). */
  async updateSettings(change) {
    const res = await readModifyWrite(this.#fetchJson, change);
    if (!res.err && res.settings) this.settings = res.settings;
    return res;
  }

  runImport() { return this.#post('/api/tracker/import'); }

  /** Claim an alert (a journal entry's id or `zone:<seq>`); `play` is true once. */
  announce(id) { return this.#post('/api/tracker/announce', { id }); }

  /** Upload (`{name, data}`), clear (`{clear: true}`) or copy (`{copy_from}`) a custom sound. */
  async changeSound(key, body) {
    const res = await this.#post(`/api/tracker/sound/${encodeURIComponent(key)}`, body);
    if (!res.err && res.sounds) this.sounds = res.sounds;
    return res;
  }

  saveShopping(items) { return this.#post('/api/tracker/shopping', { items }); }

  async clearRuns() {
    const res = await this.#post('/api/tracker/runs/clear');
    if (!res.err) this.runs = res.runs ?? [];
    return res;
  }

  /** Open or close the overlay window. */
  async setOverlay(on) {
    const res = await this.#post('/api/tracker/overlay', { on: !!on });
    if (!res.err && this.overlay) this.overlay = { ...this.overlay, open: !!res.open };
    return res;
  }
}
