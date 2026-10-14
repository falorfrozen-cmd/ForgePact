// The Tracker tab's Alerts section (#185): AlertSettings.svelte and its parts
// in src/tracker/settings/, against `/api/tracker/settings` and
// `/api/tracker/sounds`. Run by tests/tracker.e2e.mjs
// (`npm run e2e:tracker -- --only settings`).
//
// Every expectation is read back from the API, never from a literal: a check
// changes a control in the page and then asks the sandbox what the document
// (or the sound listing) now holds. Each check starts from the document the
// sandbox first answered, posted back whole, so one check's writes never
// carry into the next. Each has its control beside it: the keys a write must
// not touch, the bundled sound before the custom one, a refused file next to
// an accepted one, the hidden Discord switch next to a shown one.
//
// Plays are counted by wrapping the Web Audio API in a page init script
// (COUNT_PLAYS, as alerts.check.mjs does): every AudioBufferSourceNode.start
// is one play, carrying the byte length of the file it was decoded from. No
// test hook ships in the product.

import { statSync } from 'node:fs';
import { join } from 'node:path';
import { PANEL_DIR } from '../lib/browser.mjs';

export const EXPECTED = [
  'rarity-and-zone-switches-and-volumes-write',
  'alerts-and-min-tier-write',
  'custom-sound-imports-plays-and-clears',
  'notable-group-adds-and-removes',
  'zone-buff-pick-lists-the-sdk-names-and-writes',
  'filter-list-items-and-sound-write-filters',
  'filter-export-and-import-round-trip',
  'unknown-fields-survive-every-write',
  'discord-switch-hidden-unless-presence-available',
];

const SECTION = '[data-section="alerts"]';
const RARITIES = [['satanic', 'Satanic'], ['set', 'Set'], ['heroic', 'Heroic'], ['angelic', 'Angelic'], ['unholy', 'Unholy']];
const SOUNDS = join(PANEL_DIR, 'src', 'tracker', 'sounds');
const bundledBytes = (key) => statSync(join(SOUNDS, `${key}.wav`)).size;
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
const isSettingsPost = (req) => req.method() === 'POST' && new URL(req.url()).pathname === '/api/tracker/settings';

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

// A short mono 16-bit PCM WAV, built here: `samples` frames of a quiet tone.
function wav(samples = 1600, rate = 8000) {
  const data = Buffer.alloc(samples * 2);
  for (let i = 0; i < samples; i++) data.writeInt16LE(Math.round(Math.sin(i / 6) * 3000), i * 2);
  const head = Buffer.alloc(44);
  head.write('RIFF', 0, 'ascii');
  head.writeUInt32LE(36 + data.length, 4);
  head.write('WAVE', 8, 'ascii');
  head.write('fmt ', 12, 'ascii');
  head.writeUInt32LE(16, 16);
  head.writeUInt16LE(1, 20);
  head.writeUInt16LE(1, 22);
  head.writeUInt32LE(rate, 24);
  head.writeUInt32LE(rate * 2, 28);
  head.writeUInt16LE(2, 32);
  head.writeUInt16LE(16, 34);
  head.write('data', 36, 'ascii');
  head.writeUInt32LE(data.length, 40);
  return Buffer.concat([head, data]);
}

async function settings(t) {
  const res = await t.api('/api/tracker/settings');
  assert(res.settings && typeof res.settings === 'object', `GET /api/tracker/settings answered ${JSON.stringify(res)}`);
  return res.settings;
}

async function sounds(t) {
  const res = await t.api('/api/tracker/sounds');
  assert(res.sounds && typeof res.sounds === 'object', `GET /api/tracker/sounds answered ${JSON.stringify(res)}`);
  return res.sounds;
}

async function soundBytes(t, key) {
  const res = await fetch(new URL(`/api/tracker/sound/${encodeURIComponent(key)}`, t.sandbox.url));
  assert(res.ok, `GET /api/tracker/sound/${key} answered ${res.status}`);
  return Buffer.from(await res.arrayBuffer());
}

async function writeSettings(t, doc) {
  const res = await t.api('/api/tracker/settings', { body: doc });
  assert(!res.err && t.lastStatus === 200, `seeding the settings was refused: ${t.lastStatus} ${JSON.stringify(res)}`);
  return res.settings;
}

async function clearSounds(t) {
  for (const key of Object.keys(await sounds(t))) {
    await t.api(`/api/tracker/sound/${encodeURIComponent(key)}`, { body: { clear: true } });
  }
  assert(Object.keys(await sounds(t)).length === 0, 'the custom sounds did not clear');
}

// Wait until the API's document passes `test`; answers the last one read.
async function until(t, test, what, timeout = 5000) {
  const deadline = Date.now() + timeout;
  let doc = await settings(t);
  while (!test(doc) && Date.now() < deadline) {
    await wait(100);
    doc = await settings(t);
  }
  assert(test(doc), `${what}: the API holds ${JSON.stringify(doc).slice(0, 600)}`);
  return doc;
}

async function untilSounds(t, test, what, timeout = 5000) {
  const deadline = Date.now() + timeout;
  let listing = await sounds(t);
  while (!test(listing) && Date.now() < deadline) {
    await wait(100);
    listing = await sounds(t);
  }
  assert(test(listing), `${what}: the listing is ${JSON.stringify(listing)}`);
  return listing;
}

// A fresh page with the Tracker tab's Alerts section open, its settings read.
async function openAlerts(t, options = {}) {
  const page = await t.openPanel(options);
  await page.click('.tabbtn[data-tab="tracker"]');
  await page.waitForFunction(() => document.getElementById('nav-tracker')?.getAttribute('aria-selected') === 'true');
  await page.click('#trk-tab-alerts');
  await page.waitForSelector(`${SECTION}[data-loaded]`, { state: 'attached', timeout: 8000 });
  return page;
}

// A range set the way a drag ends: input events, then one change.
async function setRange(locator, value) {
  await locator.evaluate((el, v) => {
    el.value = String(v);
    el.dispatchEvent(new Event('input', { bubbles: true }));
    el.dispatchEvent(new Event('change', { bubbles: true }));
  }, value);
}

const row = (page, key) => page.locator(`${SECTION} .trk-alert[data-key="${key}"]`);

export default async function (t) {
  const initial = await settings(t);
  const reset = async () => {
    await writeSettings(t, structuredClone(initial));
    await clearSounds(t);
  };

  await t.check('rarity-and-zone-switches-and-volumes-write', async () => {
    await reset();
    const page = await openAlerts(t);
    for (const key of [...RARITIES.map(([k]) => k), 'zone']) {
      const before = await settings(t);
      const on = row(page, key).locator('input.trk-alert-on');
      const was = await on.isChecked();
      assert(was === (before[key].enabled !== false), `${key}'s switch reads ${was}, the API's enabled is ${before[key].enabled}`);
      await on.click({ force: true });
      const flipped = await until(t, (d) => d[key].enabled === !was, `${key}'s switch did not write`);
      // The control: no other sound's fields moved.
      for (const other of [...RARITIES.map(([k]) => k), 'zone'].filter((k) => k !== key)) {
        assert(same(flipped[other], before[other]), `switching ${key} changed ${other}: ${JSON.stringify(before[other])} -> ${JSON.stringify(flipped[other])}`);
      }
      await on.click({ force: true });
      await until(t, (d) => d[key].enabled === was, `${key}'s switch did not write back`);
      // The volume: a drag's end writes, and the stepper's + writes 5% more.
      const range = row(page, key).locator('input[type="range"].trk-volume');
      await setRange(range, 37);
      const dragged = await until(t, (d) => Math.abs(d[key].volume - 0.37) < 1e-9, `${key}'s volume did not write 0.37`);
      const shown = (await row(page, key).locator('.value-stepper .val').textContent()).trim();
      assert(shown === `${Math.round(dragged[key].volume * 100)}%`, `${key}'s volume reads ${shown}`);
      await row(page, key).locator('.step-button[data-step="1"]').click();
      await until(t, (d) => Math.abs(d[key].volume - 0.42) < 1e-9, `${key}'s + did not write 0.42`);
    }
  });

  await t.check('alerts-and-min-tier-write', async () => {
    await reset();
    const page = await openAlerts(t);
    const before = await settings(t);
    assert(before.alerts.includes('Heroic'), `the default alerts lack Heroic: ${JSON.stringify(before.alerts)}`);
    // A rarity's switch says whether it alerts at all: off takes it out of `alerts`.
    await row(page, 'heroic').locator('input.trk-alert-on').click({ force: true });
    const off = await until(t, (d) => !d.alerts.includes('Heroic'), 'Heroic stayed in alerts');
    assert(same(off.alerts, before.alerts.filter((r) => r !== 'Heroic')), `alerts became ${JSON.stringify(off.alerts)}`);
    await row(page, 'heroic').locator('input.trk-alert-on').click({ force: true });
    await until(t, (d) => d.alerts.includes('Heroic') && d.heroic.enabled === true, 'Heroic did not come back');
    // The zone's switch is not a rarity: `alerts` stays as it was.
    const zoneBefore = await settings(t);
    await row(page, 'zone').locator('input.trk-alert-on').click({ force: true });
    const zoneOff = await until(t, (d) => d.zone.enabled === false, 'the zone switch did not write');
    assert(same(zoneOff.alerts, zoneBefore.alerts), `the zone switch changed alerts to ${JSON.stringify(zoneOff.alerts)}`);
    // The lowest grade.
    const options = await page.$$eval('#trkMinTier option', (els) => els.map((e) => Number(e.value)));
    assert(same(options, [0, 1, 2, 3, 4, 5, 6]), `the grade options are ${JSON.stringify(options)}`);
    assert(Number(await page.inputValue('#trkMinTier')) === before.min_tier, 'the grade does not show the API value');
    await page.selectOption('#trkMinTier', '5');
    await until(t, (d) => d.min_tier === 5, 'min_tier did not write 5');
    await page.selectOption('#trkMinTier', '0');
    await until(t, (d) => d.min_tier === 0, 'min_tier did not write 0');
  });

  await t.check('custom-sound-imports-plays-and-clears', async () => {
    await reset();
    const page = await openAlerts(t, { routes: (p) => p.addInitScript(COUNT_PLAYS) });
    const set = row(page, 'set');
    assert((await set.locator('.trk-sound-src b').textContent()).trim() === 'bundled', 'Set does not say bundled');
    assert(await set.locator('.trk-sound-clear').count() === 0, 'a bundled sound offers "Use the bundled sound"');
    // The control: Test plays the bundled file first.
    await set.locator('.trk-test').click();
    await page.waitForFunction(() => window.__plays.length >= 1, null, { timeout: 5000 });
    const first = await page.evaluate(() => window.__plays.at(-1).bytes);
    assert(first === bundledBytes('set'), `Test played ${first} bytes, the bundled Set is ${bundledBytes('set')}`);
    // A refused file changes nothing and says so in place.
    await set.locator('input[type="file"].trk-sound-file').setInputFiles({ name: 'chime.wav', mimeType: 'audio/wav', buffer: Buffer.from('not a wave file at all') });
    const error = page.locator(`${SECTION} .trk-error`);
    await error.waitFor({ state: 'visible', timeout: 5000 });
    assert((await error.textContent()).includes('chime.wav'), `the refusal reads ${await error.textContent()}`);
    assert(!('set' in (await sounds(t))), 'a refused file was filed');
    // The real file: listed, named, and the one Test plays.
    const file = wav();
    await set.locator('input[type="file"].trk-sound-file').setInputFiles({ name: 'my-set.wav', mimeType: 'audio/wav', buffer: file });
    const listing = await untilSounds(t, (s) => s.set?.ext === 'wav', 'the Set sound was not filed');
    assert((await soundBytes(t, 'set')).equals(file), 'the filed Set sound is not the file chosen');
    await page.waitForFunction((sel) => document.querySelector(sel)?.textContent.trim() === 'set.wav', `${SECTION} .trk-alert[data-key="set"] .trk-sound-src b`, { timeout: 5000 });
    assert(await error.count() === 0, 'the refusal stayed after a file was filed');
    const plays = await page.evaluate(() => window.__plays.length);
    await set.locator('.trk-test').click();
    await page.waitForFunction((n) => window.__plays.length > n, plays, { timeout: 5000 });
    const custom = await page.evaluate(() => window.__plays.at(-1).bytes);
    assert(custom === file.length, `Test played ${custom} bytes, the custom file is ${file.length} (listing ${JSON.stringify(listing)})`);
    // Use the bundled sound: the file goes, and Test plays the bundled one again.
    await set.locator('.trk-sound-clear').click();
    await untilSounds(t, (s) => !('set' in s), 'the Set sound did not clear');
    await page.waitForFunction((sel) => document.querySelector(sel)?.textContent.trim() === 'bundled', `${SECTION} .trk-alert[data-key="set"] .trk-sound-src b`, { timeout: 5000 });
    assert(await set.locator('.trk-sound-clear').count() === 0, 'a cleared sound still offers "Use the bundled sound"');
    // Test plays the bundled one again. The mixer holds two voices and an
    // equal priority never cuts one off (audio.js), and this page's two Set
    // plays may both still sound (measured: they started 180 ms apart, the
    // bundled Set lasts 1.14 s), so the third waits for them to end.
    await wait(1500);
    const again = await page.evaluate(() => window.__plays.length);
    await set.locator('.trk-test').click();
    await page.waitForFunction((n) => window.__plays.length > n, again, { timeout: 5000 });
    const back = await page.evaluate(() => window.__plays.at(-1).bytes);
    assert(back === bundledBytes('set'), `after clearing, Test played ${back} bytes`);
  });

  await t.check('notable-group-adds-and-removes', async () => {
    await reset();
    const page = await openAlerts(t);
    const before = await settings(t);
    const shownLabels = () => page.$$eval('#trkNotable li[data-label]', (els) => els.map((e) => e.dataset.label));
    await page.waitForFunction((n) => document.querySelectorAll('#trkNotable li[data-label]').length === n, before.notable.length, { timeout: 5000 });
    assert(same(await shownLabels(), before.notable.map((g) => g.label)), `the page shows ${JSON.stringify(await shownLabels())}`);
    // A blank group sends nothing.
    const posts = [];
    page.on('request', (r) => { if (isSettingsPost(r)) posts.push(r.postData()); });
    await page.fill('#trkNotableLabel', '   ');
    await page.fill('#trkNotableNames', 'zed');
    await page.click('#trkNotableForm button[type="submit"]');
    await wait(600);
    assert(posts.length === 0, `a blank group posted ${posts.length} writes`);
    // A real one adds, its names split on commas.
    await page.fill('#trkNotableLabel', 'Chase runes');
    await page.fill('#trkNotableNames', 'Zed, Jah ,  Ber');
    await page.click('#trkNotableForm button[type="submit"]');
    const added = await until(t, (d) => d.notable.some((g) => g.label === 'Chase runes'), 'the group was not added');
    const group = added.notable.find((g) => g.label === 'Chase runes');
    assert(same(group.names, ['Zed', 'Jah', 'Ber']), `the group's names are ${JSON.stringify(group.names)}`);
    assert(same(added.notable.slice(0, before.notable.length), before.notable), 'adding a group changed the others');
    await page.waitForFunction(() => [...document.querySelectorAll('#trkNotable li[data-label]')].some((e) => e.dataset.label === 'Chase runes'), null, { timeout: 5000 });
    // Remove it again: the others stay as they were.
    await page.click('#trkNotable button[aria-label="Remove the Chase runes group"]');
    const removed = await until(t, (d) => !d.notable.some((g) => g.label === 'Chase runes'), 'the group was not removed');
    assert(same(removed.notable, before.notable), `after removing, notable is ${JSON.stringify(removed.notable)}`);
    // And a default one goes too.
    const gone = before.notable[0].label;
    await page.click(`#trkNotable button[aria-label="Remove the ${gone} group"]`);
    const less = await until(t, (d) => !d.notable.some((g) => g.label === gone), `${gone} was not removed`);
    assert(same(less.notable, before.notable.slice(1)), `after removing ${gone}, notable is ${JSON.stringify(less.notable)}`);
  });

  await t.check('zone-buff-pick-lists-the-sdk-names-and-writes', async () => {
    await reset();
    const state = await t.api('/api/state');
    const buffs = state.satanicBuffs;
    assert(Array.isArray(buffs) && buffs.length > 3, `/api/state has no buff list: ${JSON.stringify(buffs)}`);
    const page = await openAlerts(t);
    await page.waitForFunction((n) => document.querySelectorAll('#trkBuffs .trk-pick input[data-id]').length === n, buffs.length, { timeout: 5000 });
    const shown = await page.$$eval('#trkBuffs .trk-pick', (els) => els.map((e) => [Number(e.querySelector('input').dataset.id), e.textContent.trim()]));
    assert(same(shown, buffs.map(([id, name]) => [id, name])), `the pick shows ${JSON.stringify(shown).slice(0, 300)}`);
    assert((await settings(t)).zone_buffs.length === 0, 'the default pick is not empty');
    // Pick two out of order: the document keeps them sorted.
    const [a, b] = [buffs[3][0], buffs[1][0]];
    await page.locator(`#trkBuffs input[data-id="${a}"]`).click({ force: true });
    await until(t, (d) => same(d.zone_buffs, [a]), `picking ${a} did not write`);
    await page.locator(`#trkBuffs input[data-id="${b}"]`).click({ force: true });
    await until(t, (d) => same(d.zone_buffs, [b, a].sort((x, y) => x - y)), `picking ${b} did not write`);
    await page.locator(`#trkBuffs input[data-id="${a}"]`).click({ force: true });
    await until(t, (d) => same(d.zone_buffs, [b]), `unpicking ${a} did not write`);
    // Clear: every rotation alerts again.
    await page.click('#trkBuffsClear');
    await until(t, (d) => d.zone_buffs.length === 0, 'Clear did not empty zone_buffs');
    assert(!(await page.locator(`#trkBuffs input[data-id="${b}"]`).isChecked()), 'the switch stayed on after Clear');
  });

  await t.check('filter-list-items-and-sound-write-filters', async () => {
    await reset();
    const page = await openAlerts(t);
    // New filter: one filter with one list, picked as the active one.
    await page.click('#trkFilterNew');
    const made = await until(t, (d) => d.filters.length === 1 && d.filters[0].lists?.length === 1, 'New filter did not write');
    const filter = made.filters[0];
    assert(made.filter === filter.id, `the new filter is not the active one: ${made.filter} vs ${filter.id}`);
    const list = filter.lists[0];
    assert(/^[A-Za-z0-9_-]{1,64}$/.test(list.id), `the list id ${list.id} is not one the sound store files`);
    // Items: typed names, trimmed; a name already there (any case) is not added twice.
    for (const name of ['Zed Rune', '  Astral Covenant ', 'zed rune']) {
      await page.fill('#trkItemField', name);
      await page.press('#trkItemField', 'Enter');
      await wait(250);
    }
    const listed = await until(t, (d) => d.filters[0].lists[0].items.length === 2, 'the items did not write');
    assert(same(listed.filters[0].lists[0].items, ['Zed Rune', 'Astral Covenant']), `the items are ${JSON.stringify(listed.filters[0].lists[0].items)}`);
    await page.waitForFunction(() => document.querySelectorAll('.trk-items li[data-name]').length === 2, null, { timeout: 5000 });
    // The list's own name, switch and volume.
    await page.fill('#trkListName', 'Chase');
    await page.press('#trkListName', 'Enter');
    await until(t, (d) => d.filters[0].lists[0].name === 'Chase', 'the list name did not write');
    await setRange(page.locator('#trkListVolume'), 64);
    await until(t, (d) => Math.abs(d.filters[0].lists[0].volume - 0.64) < 1e-9, 'the list volume did not write');
    await page.locator('#trkListOn').click({ force: true });
    await until(t, (d) => d.filters[0].lists[0].enabled === false, 'the list switch did not write');
    await page.locator('#trkListOn').click({ force: true });
    await until(t, (d) => d.filters[0].lists[0].enabled === true, 'the list switch did not write back');
    // Its own sound, filed under list-<id>.
    const file = wav(2400);
    await page.locator('#trkListFile').setInputFiles({ name: 'chase.wav', mimeType: 'audio/wav', buffer: file });
    const key = `list-${list.id}`;
    await untilSounds(t, (s) => s[key]?.ext === 'wav', `the list's sound was not filed as ${key}`);
    assert((await soundBytes(t, key)).equals(file), 'the filed list sound is not the file chosen');
    // A second list, then remove an item from the first.
    await page.click('#trkListAdd');
    const two = await until(t, (d) => d.filters[0].lists.length === 2, 'Add a list did not write');
    assert(two.filters[0].lists[0].id === list.id && same(two.filters[0].lists[0].items, ['Zed Rune', 'Astral Covenant']), 'adding a list changed the first');
    await page.click(`.trk-lists button[data-list="${list.id}"]`);
    await page.click('.trk-items button[aria-label="Remove Zed Rune"]');
    await until(t, (d) => same(d.filters[0].lists[0].items, ['Astral Covenant']), 'removing an item did not write');
    // The filter's switch: use_filter.
    const use = (await settings(t)).use_filter;
    await page.locator('#trkFilterUse').click({ force: true });
    await until(t, (d) => d.use_filter === !use, 'the sound lists switch did not write use_filter');
  });

  await t.check('filter-export-and-import-round-trip', async () => {
    await reset();
    const seeded = structuredClone(initial);
    seeded.filters = [{
      id: 'hunt1', name: 'Hell farming', lists: [
        { id: 'chase1', name: 'Chase', enabled: true, volume: 0.8, items: ['Astral Covenant', 'Zed Rune'] },
        { id: 'keys1', name: 'Keys', enabled: false, volume: 0.3, items: ['Angelic Key'] },
      ],
    }];
    seeded.filter = 'hunt1';
    await writeSettings(t, seeded);
    const file = wav(1200);
    const up = await t.api('/api/tracker/sound/list-chase1', { body: { name: 'chase.wav', data: file.toString('base64') } });
    assert(!up.err, `seeding the list sound was refused: ${JSON.stringify(up)}`);
    const page = await openAlerts(t);
    await page.context().grantPermissions(['clipboard-read', 'clipboard-write'], { origin: new URL(t.sandbox.url).origin });
    await page.evaluate(() => navigator.clipboard.writeText('before'));
    // Export: the filter, its lists and the chase list's sound, inlined.
    await page.click('#trkFilterMore');
    await page.click('#trkFilterExport');
    // Headless Edge shares the machine's clipboard, and a failed read can come
    // back empty, so the poll that sees the export is the one that keeps it:
    // a second read could still return 'before'.
    const seen = await page.waitForFunction(async () => {
      let text = null;
      try { text = await navigator.clipboard.readText(); } catch (e) { return null; }
      try { return JSON.parse(text)?.name === 'Hell farming' ? text : null; } catch (e) { return null; }
    }, null, { timeout: 8000 });
    const text = await seen.jsonValue();
    const exported = JSON.parse(text);
    assert(exported.name === 'Hell farming' && exported.lists.length === 2, `the export is ${text.slice(0, 300)}`);
    for (const [i, list] of seeded.filters[0].lists.entries()) {
      const got = exported.lists[i];
      assert(got.name === list.name && got.enabled === list.enabled && got.volume === list.volume && same(got.items, list.items),
        `exported list ${i} is ${JSON.stringify({ ...got, sound: got.sound ? '…' : null })}`);
    }
    assert(exported.lists[0].sound?.ext === 'wav' && Buffer.from(exported.lists[0].sound.data, 'base64').equals(file), 'the chase sound is not inlined');
    assert(!exported.lists[1].sound, 'the keys list, which has no sound, exported one');
    // Import it back: a second filter, fresh ids, the same lists and the sound filed under the new id.
    await page.click('#trkFilterMore');
    await page.locator('#trkFilterImportInput').setInputFiles({ name: 'Hell farming.hsofflinetracker.json', mimeType: 'application/json', buffer: Buffer.from(text) });
    const after = await until(t, (d) => d.filters.length === 2, 'the import did not add a filter');
    const imported = after.filters[1];
    assert(after.filter === imported.id && imported.id !== 'hunt1', `the imported filter is not active and new: ${after.filter}, ${imported.id}`);
    assert(imported.name === 'Hell farming', `the imported name is ${imported.name}`);
    for (const [i, list] of seeded.filters[0].lists.entries()) {
      const got = imported.lists[i];
      assert(got.id !== list.id && got.name === list.name && got.enabled === list.enabled && got.volume === list.volume && same(got.items, list.items),
        `imported list ${i} is ${JSON.stringify(got)}`);
    }
    assert(same(after.filters[0], seeded.filters[0]), 'the import changed the original filter');
    const newKey = `list-${imported.lists[0].id}`;
    await untilSounds(t, (s) => s[newKey]?.ext === 'wav', `the imported sound was not filed as ${newKey}`);
    assert((await soundBytes(t, newKey)).equals(file), 'the imported sound is not the exported one');
    assert(!(`list-${imported.lists[1].id}` in (await sounds(t))), 'the keys list was given a sound on import');
    // The control: a file that is not a filter export changes nothing and says so.
    await page.click('#trkFilterMore');
    await page.locator('#trkFilterImportInput').setInputFiles({ name: 'notes.json', mimeType: 'application/json', buffer: Buffer.from('{"hello": 1}') });
    const error = page.locator(`${SECTION} .trk-error`);
    await error.waitFor({ state: 'visible', timeout: 5000 });
    await wait(300);
    assert((await settings(t)).filters.length === 2, 'a refused import changed the filters');
  });

  await t.check('unknown-fields-survive-every-write', async () => {
    await reset();
    const seeded = structuredClone(initial);
    seeded.zz_unknown = { kept: [1, 2, 3], note: 'from a newer panel' };
    seeded.filters = [{ id: 'f1', name: 'Mine', zz_filter_note: 'kept', lists: [{ id: 'l1', name: 'A', enabled: true, volume: 0.5, items: ['Zed Rune'], zz_list_note: 7 }] }];
    seeded.filter = 'f1';
    await writeSettings(t, seeded);
    const kept = (d) => same(d.zz_unknown, seeded.zz_unknown) && d.filters[0]?.zz_filter_note === 'kept' && d.filters[0]?.lists[0]?.zz_list_note === 7;
    assert(kept(await settings(t)), 'the store did not keep the seeded unknown fields');
    const page = await openAlerts(t);
    const writes = [
      ['a switch', () => row(page, 'angelic').locator('input.trk-alert-on').click({ force: true }), (d) => d.angelic.enabled === false],
      ['a volume', () => setRange(row(page, 'zone').locator('input.trk-volume'), 20), (d) => Math.abs(d.zone.volume - 0.2) < 1e-9],
      ['the grade', () => page.selectOption('#trkMinTier', '3'), (d) => d.min_tier === 3],
      ['a buff', () => page.locator('#trkBuffs input[data-id]').first().click({ force: true }), (d) => d.zone_buffs.length === 1],
      ['a notable group', async () => { await page.fill('#trkNotableLabel', 'Gems'); await page.fill('#trkNotableNames', 'Ruby'); await page.click('#trkNotableForm button[type="submit"]'); }, (d) => d.notable.some((g) => g.label === 'Gems')],
      ['an item', async () => { await page.fill('#trkItemField', 'Jah Rune'); await page.press('#trkItemField', 'Enter'); }, (d) => d.filters[0].lists[0].items.includes('Jah Rune')],
      ['a new list', () => page.click('#trkListAdd'), (d) => d.filters[0].lists.length === 2],
    ];
    for (const [what, act, wrote] of writes) {
      await act();
      const doc = await until(t, wrote, `${what} did not write`);
      assert(kept(doc), `${what} dropped an unknown field: ${JSON.stringify({ top: doc.zz_unknown, filter: doc.filters[0] }).slice(0, 400)}`);
    }
  });

  await t.check('discord-switch-hidden-unless-presence-available', async () => {
    await reset();
    const body = await t.api('/api/tracker');
    assert(body.presence?.available !== true, `the sandbox offers presence: ${JSON.stringify(body.presence)}`);
    const switchOf = (page) => page.locator(`${SECTION} #trkDiscord`);
    // As the sandbox answers (no application ID): hidden.
    const plain = await openAlerts(t);
    await wait(1500);
    assert(await switchOf(plain).count() === 0, 'the Discord switch shows while presence is not available');
    // With `presence` missing from the answer: hidden.
    const missing = await openAlerts(t, {
      routes: (p) => p.route('**/api/tracker', async (route) => {
        const res = await route.fetch();
        const json = await res.json();
        delete json.presence;
        await route.fulfill({ response: res, json });
      }),
    });
    await wait(1500);
    assert(await switchOf(missing).count() === 0, 'the Discord switch shows while presence is absent');
    // The control: available, and the switch is there and writes `discord`.
    const offered = await openAlerts(t, {
      routes: (p) => p.route('**/api/tracker', async (route) => {
        const res = await route.fetch();
        const json = await res.json();
        json.presence = { available: true };
        await route.fulfill({ response: res, json });
      }),
    });
    await switchOf(offered).waitFor({ state: 'attached', timeout: 5000 });
    const before = (await settings(t)).discord;
    assert(await switchOf(offered).isChecked() === !!before, 'the Discord switch does not show the API value');
    await switchOf(offered).click({ force: true });
    await until(t, (d) => d.discord === !before, 'the Discord switch did not write');
    await reset();
  });
}
