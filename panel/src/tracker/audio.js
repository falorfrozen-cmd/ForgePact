// The Tracker's alert mixer (#185), ported from HS Offline Tracker's
// `src/audio.js` (MIT, Q7(b); the notice is in CREDITS.md): one shared
// AudioContext, at most two voices at once, a priority order deciding which
// voice gives way, and a gentle compressor so two loud alerts never clip.
// The Tracker's online-only mail alert is not carried over.
//
// The six bundled sounds sit beside this file in `sounds/` (SOURCES.md says
// where each comes from); a custom one is the panel's own
// `/api/tracker/sound/<key>`, stamped so a replaced file is a new URL.
//
// Who calls this. alerts.js plays the journal's and the zone's alerts from
// the main window; the Alerts section previews through `play` too. Nothing at
// module level touches `window`, so tests/tracker-alerts.test.js can import
// the pure parts (soundUrl and the tables) under plain node.

export const SOUND_KEYS = ['satanic', 'set', 'heroic', 'angelic', 'unholy', 'zone'];

// Vite emits each as an asset and rewrites the URL; under node they are file URLs.
export const BUNDLED = {
  satanic: new URL('./sounds/satanic.wav', import.meta.url).href,
  set: new URL('./sounds/set.wav', import.meta.url).href,
  heroic: new URL('./sounds/heroic.wav', import.meta.url).href,
  angelic: new URL('./sounds/angelic.wav', import.meta.url).href,
  unholy: new URL('./sounds/unholy.wav', import.meta.url).href,
  zone: new URL('./sounds/zone.wav', import.meta.url).href,
};

// Which voice gives way when both are busy: the lower number. A sound list's
// own sound outranks every rarity, as in the Tracker.
export const PRIORITY = { set: 1, satanic: 2, heroic: 3, angelic: 4, unholy: 5, zone: 6 };
const LIST_PRIORITY = 7;
export const MAX_VOICES = 2;

/** The custom file filed as `key` (`/api/tracker/sounds`' listing), or null. */
export function customUrl(key, sounds) {
  const entry = sounds?.[key];
  if (!entry) return null;
  const stamp = entry.stamp != null ? `?v=${encodeURIComponent(entry.stamp)}` : '';
  return `/api/tracker/sound/${encodeURIComponent(key)}${stamp}`;
}

/**
 * The URL that plays for `key`: the player's own file when the listing has
 * one, else the bundled sound. A `list-<id>` with no file of its own falls
 * back to `rarity` (the item's), resolved the same way.
 */
export function soundUrl(key, sounds, rarity = null) {
  const own = customUrl(key, sounds) ?? BUNDLED[key] ?? null;
  if (own || !rarity || rarity === key) return own;
  return customUrl(rarity, sounds) ?? BUNDLED[rarity] ?? null;
}

/** The mixer's priority for a key: a list's own sound outranks the rarities. */
export function priorityOf(key) {
  return PRIORITY[key] ?? LIST_PRIORITY;
}

let shared = null;
let master = null;
const buffers = new Map();
const voices = [];

/**
 * The one AudioContext, made on first use (null where the page has no Web
 * Audio). The compressor and master gain are the Tracker's values.
 *
 * Autoplay, measured 2026-10-14 with a throwaway pywebview window on a local
 * page (WebView2, Edg/155): with
 * WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS=--autoplay-policy=no-user-gesture-required
 * set before webview.start() (what forgepact.py's allow_alert_autoplay does),
 * a new AudioContext is `running` at creation; without it, `suspended`, and it
 * was still `suspended` 1.5 s after a resume() with no gesture. A browser
 * tab (the dev server, a panel opened in Edge or Chrome) stays suspended until
 * the player clicks, so alerts.js claims nothing until the context runs and
 * shows its "click to let it play alert sounds" note either way.
 */
export function context() {
  const Ctx = typeof window !== 'undefined' ? (window.AudioContext || window.webkitAudioContext) : null;
  if (!Ctx) return null;
  if (!shared || shared.state === 'closed') {
    shared = new Ctx({ latencyHint: 'interactive' });
    const compressor = shared.createDynamicsCompressor();
    compressor.threshold.value = -18;
    compressor.knee.value = 18;
    compressor.ratio.value = 8;
    compressor.attack.value = 0.003;
    compressor.release.value = 0.18;
    master = shared.createGain();
    master.gain.value = 0.86;
    master.connect(compressor);
    compressor.connect(shared.destination);
    voices.length = 0;
    buffers.clear();
  }
  return shared;
}

/** The context's state without making one: `none` before first use, `unavailable` with no Web Audio. */
export function audioState() {
  if (shared) return shared.state;
  const Ctx = typeof window !== 'undefined' ? (window.AudioContext || window.webkitAudioContext) : null;
  return Ctx ? 'none' : 'unavailable';
}

/**
 * Make the context if need be and ask it to run. Call it from a click or key
 * handler to unlock sound; called without one it still runs where the page
 * may autoplay. Answers whether the context is running.
 */
export async function unlock() {
  const ctx = context();
  if (!ctx) return false;
  if (ctx.state === 'suspended') {
    try { await ctx.resume(); } catch {}
  }
  return ctx.state === 'running';
}

function bufferFor(url) {
  if (!url) return Promise.resolve(null);
  if (!buffers.has(url)) {
    buffers.set(url, (async () => {
      try {
        const ctx = context();
        if (!ctx) return null;
        // Bundled sounds never change; a custom file's URL says which version is wanted.
        const response = await fetch(url, { cache: url.includes('?v=') ? 'no-store' : 'force-cache' });
        if (!response.ok) throw new Error(`HTTP ${response.status}`);
        return await ctx.decodeAudioData(await response.arrayBuffer());
      } catch (error) {
        console.warn('Alert sound could not be decoded:', error);
        buffers.delete(url);
        return null;
      }
    })());
  }
  return buffers.get(url);
}

/** Fetch and decode sounds ahead of their first play (needs the context). */
export async function preload(urls = Object.values(BUNDLED)) {
  if (!context()) return;
  await Promise.allSettled([...new Set(urls.filter(Boolean))].map(bufferFor));
}

/** Forget a decoded sound, so its next play reads the file again. */
export function invalidate(url) {
  if (url) buffers.delete(url);
}

function removeVoice(voice) {
  const index = voices.indexOf(voice);
  if (index >= 0) voices.splice(index, 1);
  try { voice.source.disconnect(); } catch {}
  try { voice.gain.disconnect(); } catch {}
}

function fadeAndStop(voice, ctx) {
  try {
    const now = ctx.currentTime;
    voice.gain.gain.cancelScheduledValues(now);
    voice.gain.gain.setValueAtTime(Math.max(0.0001, voice.gain.gain.value), now);
    voice.gain.gain.exponentialRampToValueAtTime(0.0001, now + 0.025);
    voice.source.stop(now + 0.03);
  } catch {}
}

function start(buffer, volume, key) {
  const ctx = shared;
  if (!ctx || !master || !buffer) return false;
  const priority = priorityOf(key);
  while (voices.length >= MAX_VOICES) {
    const victim = voices.reduce((lowest, voice) => (voice.priority < lowest.priority ? voice : lowest), voices[0]);
    if (victim.priority >= priority) return false;
    fadeAndStop(victim, ctx);
    removeVoice(victim);
  }
  const source = ctx.createBufferSource();
  const gain = ctx.createGain();
  gain.gain.value = Math.min(1, Math.max(0, Number(volume) || 0));
  source.buffer = buffer;
  source.connect(gain);
  gain.connect(master);
  const voice = { source, gain, priority };
  voices.push(voice);
  source.onended = () => removeVoice(voice);
  source.start();
  return true;
}

/**
 * Play `url` at `volume` (0 to 1) on the mixer, as `priorityKey` (a rarity,
 * `zone` or `list-<id>`). A file that cannot be decoded falls back to
 * `fallbackUrl`. Answers whether a voice started: false while the context is
 * not running, when nothing decodes, or when both voices hold something more
 * important.
 */
export async function play(url, volume = 0.5, priorityKey = 'satanic', fallbackUrl = BUNDLED[priorityKey] ?? null) {
  try {
    const ctx = context();
    if (!ctx || ctx.state !== 'running') return false;
    let decoded = await bufferFor(url);
    if (!decoded && fallbackUrl && fallbackUrl !== url) decoded = await bufferFor(fallbackUrl);
    return start(decoded, volume, priorityKey);
  } catch (error) {
    console.warn('Alert sound could not play:', error);
    return false;
  }
}
