// The Tracker's alerts (#185): the main window plays each journal entry and
// each zone rotation once, after claiming it through
// `POST /api/tracker/announce`, and only while its AudioContext runs. Run by
// tests/tracker.e2e.mjs (`npm run e2e:tracker -- --only alerts`).
//
// Plays are counted by wrapping the Web Audio API in a page init script
// (COUNT_PLAYS): every AudioBufferSourceNode.start is one play, carrying the
// byte length of the file its buffer was decoded from, which says which sound
// played. SILENT_UNTIL_CLICKED models a browser that keeps a context
// suspended until the player clicks. No test hook ships in the product.
//
// The baselines come first: the fixture's own drop plays nothing on first
// load, and with the switch off the page neither polls nor claims.

import { statSync } from 'node:fs';
import { join } from 'node:path';
import { PANEL_DIR } from '../lib/browser.mjs';

export const EXPECTED = [
  'first-load-plays-nothing-it-already-had',
  'switch-off-polls-and-claims-nothing',
  'heroic-drop-claimed-once-and-played-once',
  'mythic-drop-neither-claimed-nor-played',
  'two-pages-play-it-once',
  'zone-rotation-plays-the-zone-sound-once',
  'custom-heroic-sound-is-the-one-played',
  'silent-page-claims-nothing-and-shows-the-note',
];

const SOUNDS = join(PANEL_DIR, 'src', 'tracker', 'sounds');
const bundledBytes = (key) => statSync(join(SOUNDS, `${key}.wav`)).size;
const wait = (ms) => new Promise((r) => setTimeout(r, ms));

function assert(ok, message) {
  if (!ok) throw new Error(message);
}

function COUNT_PLAYS() {
  window.__plays = [];
  const decode = BaseAudioContext.prototype.decodeAudioData;
  BaseAudioContext.prototype.decodeAudioData = function (data, ...rest) {
    const bytes = data && typeof data.byteLength === 'number' ? data.byteLength : null;
    return decode.call(this, data, ...rest).then((buffer) => {
      try { Object.defineProperty(buffer, '__bytes', { value: bytes }); } catch (e) {}
      return buffer;
    });
  };
  const start = AudioBufferSourceNode.prototype.start;
  AudioBufferSourceNode.prototype.start = function (...args) {
    window.__plays.push({ bytes: this.buffer ? this.buffer.__bytes ?? null : null, at: Date.now() });
    return start.apply(this, args);
  };
}

// A context reads `suspended` and resume() does nothing until the page has
// had a real click or key, as in a browser tab. "Real" is the event's
// isTrusted, not navigator.userActivation: Playwright runs page.evaluate as a
// user gesture, which would unlock the page without anyone clicking.
function SILENT_UNTIL_CLICKED() {
  const state = Object.getOwnPropertyDescriptor(BaseAudioContext.prototype, 'state');
  const resume = AudioContext.prototype.resume;
  let clicked = false;
  for (const name of ['pointerdown', 'keydown']) {
    window.addEventListener(name, (e) => { if (e.isTrusted) clicked = true; }, true);
  }
  const allowed = () => clicked;
  Object.defineProperty(BaseAudioContext.prototype, 'state', {
    configurable: true,
    get() { return allowed() ? state.get.call(this) : 'suspended'; },
  });
  AudioContext.prototype.resume = function () { return allowed() ? resume.call(this) : Promise.resolve(); };
}

let serial = 0;
function groundDrop(rarity, name) {
  serial += 1;
  return {
    kind: 'ground_drop', event_id: `alerts-check-${Date.now()}-${serial}`, source: 'game_ground_item_create',
    room: 'Act_01_02', item: { name, rarity, item_type: 0, item_id: 104, weapon_type: 5, amount: 1 },
  };
}

async function setSwitch(t, value) {
  const res = await t.api('/api/set', { body: { key: 'mod_tracker', value } });
  assert(!res.err, `mod_tracker=${value} was refused: ${res.err}`);
}

// A page with the play counter (and optionally the silent context), and every
// claim it makes with the server's answer.
async function alertPage(t, { silent = false, tab = null, sandbox } = {}) {
  const page = await t.openPanel(sandbox ? { sandbox } : {});
  const claims = [];
  const trackerGets = [];
  page.on('request', (req) => {
    const path = new URL(req.url()).pathname;
    if (req.method() === 'GET' && path.startsWith('/api/tracker')) trackerGets.push(path);
  });
  page.on('response', async (res) => {
    const req = res.request();
    if (req.method() !== 'POST' || new URL(req.url()).pathname !== '/api/tracker/announce') return;
    let body = {};
    try { body = await res.json(); } catch (e) {}
    claims.push({ id: JSON.parse(req.postData() || '{}').id, play: body.play === true });
  });
  return { page, claims, trackerGets, silent, tab };
}

// openPanel boots the page before an init script could be added to it, so
// the counters go in through a reload. With the switch on, the page's first
// `/api/tracker/extra` after the reload is its baseline; a moment later the
// alerts have taken it as seen.
async function armed(t, options = {}) {
  const p = await alertPage(t, options);
  if (options.silent) await p.page.addInitScript(SILENT_UNTIL_CLICKED);
  await p.page.addInitScript(COUNT_PLAYS);
  let extras = 0;
  p.page.on('response', (r) => { if (new URL(r.url()).pathname === '/api/tracker/extra') extras += 1; });
  await p.page.reload();
  await p.page.waitForFunction(() => Array.isArray(window.__plays));
  p.trackerGets.length = 0;
  if (options.tab) {
    // Playwright's click is a real (trusted) one, which would let a silent
    // page play; an element.click() from the page's own script is not.
    if (options.silent) await p.page.evaluate((n) => document.querySelector(`.tabbtn[data-tab="${n}"]`).click(), options.tab);
    else await p.page.click(`.tabbtn[data-tab="${options.tab}"]`);
    await p.page.waitForFunction((n) => document.querySelector(`.tabbtn[data-tab="${n}"]`)?.getAttribute('aria-selected') === 'true', options.tab);
  }
  if (options.expectExtra !== false) {
    await until(() => extras > 0, 10000, 'the page to fetch /api/tracker/extra with the switch on');
    await wait(750);
  }
  return p;
}

const plays = (p) => p.page.evaluate(() => window.__plays.slice());

async function until(fn, ms, what) {
  const end = Date.now() + ms;
  for (;;) {
    const value = await fn();
    if (value) return value;
    if (Date.now() > end) throw new Error(`timed out after ${ms} ms waiting for ${what}`);
    await wait(150);
  }
}

// The journal entry the API files for `name` (it ingests on GET /api/tracker).
async function journalEntry(t, name, ms = 6000) {
  return until(async () => {
    await t.api('/api/tracker');
    return (await t.api('/api/tracker/extra')).drops.find((d) => d.name === name) || null;
  }, ms, `the journal to hold ${name}`);
}

export default async function (t) {
  await t.check('first-load-plays-nothing-it-already-had', async () => {
    await setSwitch(t, true);
    // The API ingests the session on GET /api/tracker, so ask that first.
    const journal = await until(async () => {
      await t.api('/api/tracker');
      const drops = (await t.api('/api/tracker/extra')).drops ?? [];
      return drops.length ? drops : null;
    }, 6000, 'the fixture\'s drop to reach the journal').catch(() => []);
    assert(journal.length > 0, 'the fixture put nothing in the journal, so this baseline would prove nothing');
    const p = await armed(t);
    await wait(3000);
    assert(p.claims.length === 0, `the page claimed what it already had: ${JSON.stringify(p.claims)}`);
    assert((await plays(p)).length === 0, `the page played ${(await plays(p)).length} sounds on first load`);
    // The control: the same page plays the next drop.
    const name = 'Alerts Baseline Control Heroic';
    t.appendSession([groundDrop('Heroic', name)]);
    const filed = await journalEntry(t, name);
    await until(async () => (await plays(p)).length === 1, 6000, 'the control drop to play');
    assert(p.claims.length === 1 && p.claims[0].id === filed.id, `claims: ${JSON.stringify(p.claims)}`);
  });

  await t.check('switch-off-polls-and-claims-nothing', async () => {
    await setSwitch(t, false);
    const p = await armed(t, { tab: 'loot', expectExtra: false });
    const name = 'Alerts Switch Off Heroic';
    t.appendSession([groundDrop('Heroic', name)]);
    await wait(3500);
    assert(p.trackerGets.length === 0, `the page sent ${p.trackerGets.length} tracker requests with the switch off: ${p.trackerGets.join(', ')}`);
    assert(p.claims.length === 0, `the page claimed with the switch off: ${JSON.stringify(p.claims)}`);
    assert((await plays(p)).length === 0, 'the page played with the switch off');
    // The control: turned on, it takes what is there as seen and plays the next one.
    await p.page.click('.tabbtn[data-tab="tracker"]');
    const extra = p.page.waitForResponse((r) => new URL(r.url()).pathname === '/api/tracker/extra', { timeout: 8000 });
    await Promise.all([
      p.page.waitForRequest((r) => r.method() === 'POST' && new URL(r.url()).pathname === '/api/set'),
      p.page.locator('#mod_tracker').click(),
    ]);
    await extra;
    await wait(1500);
    assert(p.claims.length === 0, `turning the switch on claimed the drop it missed: ${JSON.stringify(p.claims)}`);
    const next = 'Alerts Switch On Heroic';
    t.appendSession([groundDrop('Heroic', next)]);
    const filed = await journalEntry(t, next);
    await until(async () => (await plays(p)).length === 1, 6000, 'the drop after the switch went on to play');
    assert(p.claims.map((c) => c.id).join() === filed.id, `claims: ${JSON.stringify(p.claims)}`);
  });

  await t.check('heroic-drop-claimed-once-and-played-once', async () => {
    await setSwitch(t, true);
    const p = await armed(t);
    const name = 'Alerts Once Heroic';
    t.appendSession([groundDrop('Heroic', name)]);
    const filed = await journalEntry(t, name);
    assert(filed.sound === 'heroic' && filed.announce === true, `the API files it as ${JSON.stringify(filed)}`);
    await until(async () => (await plays(p)).length >= 1, 6000, 'the Heroic to play');
    await wait(3000);
    const played = await plays(p);
    assert(played.length === 1, `${played.length} plays`);
    assert(played[0].bytes === bundledBytes('heroic'), `it played a ${played[0].bytes}-byte file, not the bundled Heroic`);
    assert(p.claims.length === 1 && p.claims[0].id === filed.id && p.claims[0].play, `claims: ${JSON.stringify(p.claims)}`);
    const after = (await t.api('/api/tracker/extra')).drops.find((d) => d.id === filed.id);
    assert(after?.announced === true, 'the API does not read the entry as announced');
  });

  await t.check('mythic-drop-neither-claimed-nor-played', async () => {
    await setSwitch(t, true);
    const p = await armed(t);
    const mythic = 'Alerts Mythic Thing';
    const heroic = 'Alerts After Mythic Heroic';
    t.appendSession([groundDrop('Mythic', mythic), groundDrop('Heroic', heroic)]);
    // The Heroic after it is the positive control in the same poll.
    const filed = await journalEntry(t, heroic);
    const journal = (await t.api('/api/tracker/extra')).drops;
    assert(!journal.some((d) => d.name === mythic), 'the API put the Mythic in the journal');
    await until(async () => (await plays(p)).length >= 1, 6000, 'the Heroic after the Mythic to play');
    await wait(2000);
    assert((await plays(p)).length === 1, `${(await plays(p)).length} plays`);
    assert(p.claims.map((c) => c.id).join() === filed.id, `claims: ${JSON.stringify(p.claims)}`);
  });

  await t.check('two-pages-play-it-once', async () => {
    await setSwitch(t, true);
    const a = await armed(t);
    const b = await armed(t);
    // A page that polls after the other's claim sees the entry announced and
    // never claims it, so the server's refusal would go untested. Page b
    // therefore reads every entry as unannounced: both pages claim, and only
    // the server's answer keeps the second one quiet.
    await b.page.route('**/api/tracker/extra', async (route) => {
      const response = await route.fetch();
      const json = await response.json();
      json.drops = (json.drops ?? []).map((d) => ({ ...d, announced: false }));
      await route.fulfill({ response, json });
    });
    const name = 'Alerts Two Pages Heroic';
    t.appendSession([groundDrop('Heroic', name)]);
    const filed = await journalEntry(t, name);
    await until(() => a.claims.length >= 1 && b.claims.length >= 1, 8000, 'both pages to claim');
    await wait(2000);
    const total = (await plays(a)).length + (await plays(b)).length;
    assert(total === 1, `${total} plays across the two pages`);
    const all = [...a.claims, ...b.claims];
    assert(all.every((c) => c.id === filed.id), `claims: ${JSON.stringify(all)}`);
    assert(all.filter((c) => c.play).length === 1, `the server let ${all.filter((c) => c.play).length} claims play`);
  });

  await t.check('zone-rotation-plays-the-zone-sound-once', async () => {
    await setSwitch(t, true);
    const before = (await t.api('/api/tracker')).zone_alert;
    const p = await armed(t);
    await wait(1500);
    t.appendSession([{ kind: 'satanic_zone', zone: 'Satanic_2_1', buffs: [2, 9], debuffs: [1] }]);
    const zone = await until(async () => {
      const now = (await t.api('/api/tracker')).zone_alert;
      return now && now.seq !== before?.seq ? now : null;
    }, 6000, 'the API to report the rotation');
    await until(async () => (await plays(p)).length >= 1, 6000, 'the zone sound to play');
    await wait(2500);
    const played = await plays(p);
    assert(played.length === 1, `${played.length} plays`);
    assert(played[0].bytes === bundledBytes('zone'), `it played a ${played[0].bytes}-byte file, not the bundled zone sound`);
    assert(p.claims.map((c) => c.id).join() === `zone:${zone.seq}`, `claims: ${JSON.stringify(p.claims)}`);
    assert((await t.api('/api/tracker')).zone_alert.announced === true, 'the API does not read the rotation as announced');
  });

  await t.check('custom-heroic-sound-is-the-one-played', async () => {
    await setSwitch(t, true);
    const posted = await t.api('/api/tracker/sound/heroic', { body: { name: 'my-heroic.wav', data: tinyWav().toString('base64') } });
    assert(!posted.err && posted.sounds?.heroic, `the custom sound was refused: ${JSON.stringify(posted)}`);
    try {
      const served = await fetch(new URL('/api/tracker/sound/heroic', t.sandbox.url));
      const customBytes = (await served.arrayBuffer()).byteLength;
      assert(served.ok && customBytes !== bundledBytes('heroic'), 'the custom file cannot be told from the bundled one');
      const p = await armed(t);
      const name = 'Alerts Custom Heroic';
      t.appendSession([groundDrop('Heroic', name)]);
      await journalEntry(t, name);
      await until(async () => (await plays(p)).length >= 1, 6000, 'the Heroic to play');
      await wait(1500);
      const played = await plays(p);
      assert(played.length === 1, `${played.length} plays`);
      assert(played[0].bytes === customBytes, `it played a ${played[0].bytes}-byte file, not the ${customBytes}-byte custom one`);
    } finally {
      await t.api('/api/tracker/sound/heroic', { body: { clear: true } });
    }
  });

  await t.check('silent-page-claims-nothing-and-shows-the-note', async () => {
    await setSwitch(t, true);
    const p = await armed(t, { silent: true, tab: 'tracker' });
    const note = p.page.locator('#trackerAudioNote');
    await note.waitFor({ state: 'visible', timeout: 6000 });
    assert(/click/i.test(await note.textContent()), `the note reads ${await note.textContent()}`);
    const name = 'Alerts Silent Heroic';
    t.appendSession([groundDrop('Heroic', name)]);
    const filed = await journalEntry(t, name);
    await wait(3000);
    assert(p.claims.length === 0, `a silent page claimed: ${JSON.stringify(p.claims)}`);
    assert((await plays(p)).length === 0, 'a silent page played');
    const unclaimed = (await t.api('/api/tracker/extra')).drops.find((d) => d.id === filed.id);
    assert(unclaimed?.announced === false, 'the entry reads announced while the page was silent');
    // The control: the next click lets it play, and it plays what waited.
    await note.click();
    await until(async () => (await plays(p)).length === 1, 6000, 'the waiting drop to play after a click');
    assert(p.claims.map((c) => c.id).join() === filed.id, `claims: ${JSON.stringify(p.claims)}`);
    await note.waitFor({ state: 'hidden', timeout: 4000 });
  });
}

// A 0.2 s, 8 kHz, 16-bit mono sine: a valid WAV of a size no bundled sound has.
function tinyWav() {
  const rate = 8000;
  const samples = Math.round(rate * 0.2);
  const buf = Buffer.alloc(44 + samples * 2);
  buf.write('RIFF', 0); buf.writeUInt32LE(36 + samples * 2, 4); buf.write('WAVE', 8);
  buf.write('fmt ', 12); buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20); buf.writeUInt16LE(1, 22);
  buf.writeUInt32LE(rate, 24); buf.writeUInt32LE(rate * 2, 28); buf.writeUInt16LE(2, 32); buf.writeUInt16LE(16, 34);
  buf.write('data', 36); buf.writeUInt32LE(samples * 2, 40);
  for (let i = 0; i < samples; i++) buf.writeInt16LE(Math.round(8000 * Math.sin((2 * Math.PI * 660 * i) / rate)), 44 + i * 2);
  return buf;
}
