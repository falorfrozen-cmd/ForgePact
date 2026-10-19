// The Tracker's alerts (src/tracker/alerts.js and src/tracker/audio.js, #185),
// their pure parts under plain node: what a page counts as already seen on its
// first answer, which journal entries and zone rotations it claims, with what
// sound and volume, and that it claims nothing while it cannot play. The
// browser half (one play per entry, two pages, the gesture note) is
// tests/tracker/alerts.check.mjs.
//
//   node --test tests/tracker-alerts.test.js

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { baselineIds, channelFor, decideAlerts } from '../src/tracker/alerts.js';
import { BUNDLED, MAX_VOICES, PRIORITY, SOUND_KEYS, customUrl, priorityOf, soundUrl } from '../src/tracker/audio.js';

// A journal entry as /api/tracker/extra serves it (the ledger's fields that matter here).
function entry(id, rarity, { sound = rarity.toLowerCase(), announce = true, announced = false } = {}) {
  return { id, rarity, sound, announce, announced, name: `${rarity} thing` };
}

// The store's default_settings() halves alerts.js reads.
function settings(extra = {}) {
  const sound = () => ({ enabled: true, volume: 0.5 });
  return {
    satanic: sound(), set: sound(), heroic: sound(), angelic: sound(), unholy: sound(), zone: sound(),
    alerts: ['Satanic', 'Set', 'Heroic', 'Angelic', 'Unholy'], filters: [], filter: '', use_filter: true,
    ...extra,
  };
}

const run = (over) => decideAlerts({ drops: [], zoneAlert: null, seen: new Set(), settings: settings(), sounds: {}, running: true, ...over });

test('baseline: everything on the first answer counts as seen, and nothing seen is claimed', () => {
  const drops = [entry('s:9', 'Heroic'), entry('s:5', 'Satanic')];
  const zoneAlert = { seq: 3, zone: 'Satanic_1_3', buffs: [6, 14], announced: false };
  const seen = new Set(baselineIds(drops, zoneAlert));
  assert.deepEqual([...seen].sort(), ['s:5', 's:9', 'zone:3']);
  const out = run({ drops, zoneAlert, seen });
  assert.deepEqual(out.claim, []);
  assert.deepEqual(out.skip, []);
  // No zone alert yet: only the journal.
  assert.deepEqual(baselineIds([entry('a', 'Set')], null), ['a']);
  assert.deepEqual(baselineIds(null, null), []);
});

test('target: an unseen Heroic is claimed once, with its bundled sound and its volume', () => {
  const seen = new Set(['s:9']);
  const drops = [entry('s:12', 'Heroic'), entry('s:9', 'Heroic')];
  const out = run({ drops, seen, settings: settings({ heroic: { enabled: true, volume: 0.8 } }) });
  assert.deepEqual(out.claim.map((c) => c.id), ['s:12']);
  const [c] = out.claim;
  assert.equal(c.key, 'heroic');
  assert.equal(c.url, BUNDLED.heroic);
  assert.equal(c.volume, 0.8);
  assert.equal(c.priority, 'heroic');
});

test('unseen entries are claimed oldest first (the journal is newest first)', () => {
  const drops = [entry('s:3', 'Set'), entry('s:2', 'Angelic'), entry('s:1', 'Unholy')];
  assert.deepEqual(run({ drops }).claim.map((c) => c.id), ['s:1', 's:2', 's:3']);
});

test('what makes no sound is skipped, not claimed: no sound, not announced, another page claimed it, switched off', () => {
  const drops = [
    entry('quiet', 'Heroic', { sound: null }),
    entry('flourish-only', 'Heroic', { announce: false }),
    entry('elsewhere', 'Heroic', { announced: true }),
    entry('off', 'Satanic'),
    entry('plays', 'Set'),
  ];
  const out = run({ drops, settings: settings({ satanic: { enabled: false, volume: 0.5 } }) });
  assert.deepEqual(out.claim.map((c) => c.id), ['plays']);
  assert.deepEqual(out.skip.sort(), ['elsewhere', 'flourish-only', 'off', 'quiet']);
});

test('a custom file in the listing is the one played, stamped so a new file is a new URL', () => {
  const sounds = { heroic: { ext: 'wav', stamp: 'a1' } };
  const [c] = run({ drops: [entry('s:1', 'Heroic')], sounds }).claim;
  assert.equal(c.url, '/api/tracker/sound/heroic?v=a1');
  assert.equal(c.fallback, BUNDLED.heroic, 'a custom file that will not decode falls back to the bundled one');
  // The control: another rarity's file changes nothing for Heroic.
  const [d] = run({ drops: [entry('s:1', 'Heroic')], sounds: { satanic: { ext: 'wav', stamp: 'b' } } }).claim;
  assert.equal(d.url, BUNDLED.heroic);
});

test("a sound list plays its own file at its own volume, else the item's rarity", () => {
  const filters = [{ id: 'f1', name: 'Mine', lists: [{ id: 'runes', name: 'Runes', enabled: true, volume: 0.9, items: ['ber'] }] }];
  const base = settings({ filters, filter: 'f1' });
  const drops = [entry('s:1', 'Heroic', { sound: 'list-runes' })];
  const own = run({ drops, settings: base, sounds: { 'list-runes': { ext: 'mp3', stamp: 7 } } }).claim[0];
  assert.equal(own.url, '/api/tracker/sound/list-runes?v=7');
  assert.equal(own.volume, 0.9);
  assert.equal(own.priority, 'list-runes');
  assert.equal(priorityOf('list-runes') > priorityOf('unholy'), true, 'a list outranks every rarity in the mixer');
  const borrowed = run({ drops, settings: base }).claim[0];
  assert.equal(borrowed.url, BUNDLED.heroic, "no file of its own: the item's rarity");
  const borrowedCustom = run({ drops, settings: base, sounds: { heroic: { ext: 'wav', stamp: 2 } } }).claim[0];
  assert.equal(borrowedCustom.url, '/api/tracker/sound/heroic?v=2', "the rarity's own custom file, when it has one");
  // A list switched off makes no sound.
  const off = settings({ filters: [{ id: 'f1', lists: [{ id: 'runes', enabled: false, volume: 0.9, items: ['ber'] }] }], filter: 'f1' });
  const out = run({ drops, settings: off });
  assert.deepEqual(out.claim, []);
  assert.deepEqual(out.skip, ['s:1']);
  assert.equal(channelFor(off, 'list-runes').enabled, false);
  assert.equal(channelFor(off, 'list-nope'), null);
});

test('a zone rotation is claimed as zone:<seq> once, and not while the zone alert is off', () => {
  const zoneAlert = { seq: 4, zone: 'Satanic_2_1', buffs: [3], announced: false };
  const out = run({ zoneAlert, seen: new Set(['zone:3']) });
  assert.deepEqual(out.claim.map((c) => [c.id, c.key, c.url]), [['zone:4', 'zone', BUNDLED.zone]]);
  assert.deepEqual(run({ zoneAlert, seen: new Set(['zone:4']) }).claim, []);
  const off = run({ zoneAlert, settings: settings({ zone: { enabled: false, volume: 0.5 } }) });
  assert.deepEqual(off.claim, []);
  assert.deepEqual(off.skip, ['zone:4']);
  assert.deepEqual(run({ zoneAlert: { ...zoneAlert, announced: true } }).skip, ['zone:4']);
});

test('a page that cannot play claims nothing and marks nothing: the same entries wait for it', () => {
  const drops = [entry('s:1', 'Heroic')];
  const zoneAlert = { seq: 2, announced: false };
  const silent = run({ drops, zoneAlert, running: false });
  assert.deepEqual(silent.claim, []);
  assert.equal(silent.waiting, 2);
  // The control: the same input on a running page claims both.
  assert.deepEqual(run({ drops, zoneAlert }).claim.map((c) => c.id), ['s:1', 'zone:2']);
});

test('settings not read yet: every alert on at the store default volume', () => {
  const [c] = run({ drops: [entry('s:1', 'Angelic')], settings: null }).claim;
  assert.equal(c.volume, 0.5);
  assert.equal(c.url, BUNDLED.angelic);
});

test('a Mythic, which the journal never holds, is never claimed even if served', () => {
  const out = run({ drops: [entry('m:1', 'Mythic', { sound: null })] });
  assert.deepEqual(out.claim, []);
  // And an unknown sound key resolves to nothing rather than to Satanic.
  assert.equal(soundUrl('mythic', {}), null);
});

test('the mixer keeps the Tracker\'s six keys, two voices and order; mail is gone', () => {
  assert.deepEqual(SOUND_KEYS, ['satanic', 'set', 'heroic', 'angelic', 'unholy', 'zone']);
  assert.equal(MAX_VOICES, 2);
  assert.equal('mail' in PRIORITY, false);
  assert.equal('mail' in BUNDLED, false);
  assert.ok(PRIORITY.set < PRIORITY.satanic && PRIORITY.satanic < PRIORITY.heroic
    && PRIORITY.heroic < PRIORITY.angelic && PRIORITY.angelic < PRIORITY.unholy && PRIORITY.unholy < PRIORITY.zone);
  assert.equal(customUrl('heroic', {}), null);
  assert.equal(customUrl('list-a b', { 'list-a b': { stamp: 1 } }), '/api/tracker/sound/list-a%20b?v=1');
});
