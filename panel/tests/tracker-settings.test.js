// The Tracker tab's Alerts section (#185): the edits it makes to the settings
// document (tracker/settings/edit.js) and the filter export it writes and
// reads (tracker/settings/transfer.js). Pure modules under plain node; the
// page itself is tests/tracker/settings.check.mjs's, in a real browser
// against the sandbox.
//
// Every edit is applied to a copy of the document the API answered
// (client.svelte.js's readModifyWrite), so each test checks two things: the
// field it means to change, and that every other field, unknown ones
// included, is left as it was.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  ALERT_KEYS, GRADE_OPTIONS, RARITY_KEYS, LIST_VOLUME,
  activeFilter, addFilter, addItem, addList, addNotable, alertOn, clashes, clearBuffs, duplicateFilter, makeId,
  moveList, parseNames, removeFilter, removeItem, removeList, removeNotable, renameFilter, selectFilter, setAlertOn,
  setListField, setMinTier, setVolume, toggleBuff, volumePercent,
} from '../src/tracker/settings/edit.js';
import {
  EXPORT_APP, base64ToBytes, bytesToBase64, exportFileName, exportFilter, importFilter, parseFilterExport, soundExt,
  soundSource,
} from '../src/tracker/settings/transfer.js';

// The store's default_settings() fields this section edits, plus fields it
// never touches, one of them unknown to the store.
function doc() {
  const sound = () => ({ enabled: true, volume: 0.5 });
  return {
    satanic: sound(), set: sound(), heroic: sound(), angelic: sound(), unholy: sound(), mail: sound(), zone: sound(),
    zone_buffs: [],
    alerts: ['Satanic', 'Set', 'Heroic', 'Angelic', 'Unholy'],
    min_tier: 0,
    notable: [{ label: 'Angelic Key', names: ['angelic key'] }, { label: 'S runes', names: ['qi', 'xo'] }],
    filters: [{
      id: 'f1', name: 'Hunt', note: 'unknown, kept',
      lists: [
        { id: 'a', name: 'Chase', enabled: true, volume: 0.7, items: ['Zed Rune', 'Astral Covenant'], extra: 1 },
        { id: 'b', name: 'Keys', enabled: false, volume: 0.3, items: ['Angelic Key', 'zed rune'] },
      ],
    }],
    filter: 'f1',
    use_filter: true,
    opacity: 1.0,
    zz_unknown: { kept: [1, 2] },
  };
}

// Everything but `keys` is as it was.
function onlyChanged(before, after, keys) {
  for (const key of new Set([...Object.keys(before), ...Object.keys(after)])) {
    if (keys.includes(key)) continue;
    assert.deepEqual(after[key], before[key], `${key} changed`);
  }
}

test('the alert rows are the five rarities and the zone, and the grades run Any to SS', () => {
  assert.deepEqual(RARITY_KEYS, ['satanic', 'set', 'heroic', 'angelic', 'unholy']);
  assert.deepEqual(ALERT_KEYS, [...RARITY_KEYS, 'zone']);
  assert.deepEqual(GRADE_OPTIONS.map(([v]) => v), [0, 1, 2, 3, 4, 5, 6]);
  assert.deepEqual(GRADE_OPTIONS.map(([, l]) => l), ['Any', 'D', 'C', 'B', 'A', 'S', 'SS']);
});

test('a rarity is on when it alerts and its sound is enabled; the zone by its own switch', () => {
  const d = doc();
  assert.equal(alertOn(d, 'heroic'), true);
  d.alerts = d.alerts.filter((r) => r !== 'Heroic');
  assert.equal(alertOn(d, 'heroic'), false, 'out of alerts');
  const e = doc();
  e.heroic.enabled = false;
  assert.equal(alertOn(e, 'heroic'), false, 'sound switched off');
  e.zone.enabled = false;
  e.alerts = [];
  assert.equal(alertOn(e, 'zone'), false);
  assert.equal(alertOn(doc(), 'zone'), true);
  // A document missing the sound object reads as the store's default, on.
  const f = doc();
  delete f.angelic;
  assert.equal(alertOn(f, 'angelic'), true);
});

test('switching a rarity writes its enabled and its place in alerts, in the rarities order', () => {
  const before = doc();
  const off = setAlertOn(structuredClone(before), 'set', false);
  assert.deepEqual(off.set, { enabled: false, volume: 0.5 });
  assert.deepEqual(off.alerts, ['Satanic', 'Heroic', 'Angelic', 'Unholy']);
  onlyChanged(before, off, ['set', 'alerts']);
  const on = setAlertOn(off, 'set', true);
  assert.deepEqual(on.alerts, ['Satanic', 'Set', 'Heroic', 'Angelic', 'Unholy'], 'back in its place, not at the end');
  assert.equal(on.set.enabled, true);
  // An entry this panel does not know stays in alerts.
  const odd = doc();
  odd.alerts = ['Mythic', 'Set'];
  assert.deepEqual(setAlertOn(odd, 'satanic', true).alerts, ['Satanic', 'Set', 'Mythic']);
});

test('switching the zone writes zone.enabled and leaves alerts alone', () => {
  const before = doc();
  const off = setAlertOn(structuredClone(before), 'zone', false);
  assert.deepEqual(off.zone, { enabled: false, volume: 0.5 });
  onlyChanged(before, off, ['zone']);
});

test('a volume is stored as 0 to 1 in hundredths, and read back as a percentage', () => {
  const before = doc();
  const d = setVolume(structuredClone(before), 'angelic', 0.372);
  assert.equal(d.angelic.volume, 0.37);
  assert.equal(d.angelic.enabled, true);
  onlyChanged(before, d, ['angelic']);
  assert.equal(setVolume(doc(), 'zone', 1.4).zone.volume, 1);
  assert.equal(setVolume(doc(), 'zone', -2).zone.volume, 0);
  assert.equal(volumePercent({ volume: 0.42 }), 42);
  assert.equal(volumePercent(undefined), 50, "the store's default");
  assert.equal(volumePercent({ volume: 'loud' }), 50);
});

test('the lowest grade is a whole number from 0 to 6', () => {
  const before = doc();
  const d = setMinTier(structuredClone(before), '4');
  assert.equal(d.min_tier, 4);
  onlyChanged(before, d, ['min_tier']);
  assert.equal(setMinTier(doc(), 9).min_tier, 6);
  assert.equal(setMinTier(doc(), 'x').min_tier, 0);
});

test('the buff pick keeps zone_buffs sorted and unique, and Clear empties it', () => {
  const before = doc();
  let d = toggleBuff(structuredClone(before), 9, true);
  d = toggleBuff(d, 2, true);
  d = toggleBuff(d, 9, true);
  assert.deepEqual(d.zone_buffs, [2, 9]);
  onlyChanged(before, d, ['zone_buffs']);
  d = toggleBuff(d, 2, false);
  assert.deepEqual(d.zone_buffs, [9]);
  assert.deepEqual(clearBuffs(d).zone_buffs, []);
  assert.throws(() => toggleBuff(doc(), 300, true), /buff/);
});

test('a notable group is a label and its names; a blank one is refused', () => {
  assert.deepEqual(parseNames(' Zed, Jah ,,  Ber \n Ohm'), ['Zed', 'Jah', 'Ber', 'Ohm']);
  assert.deepEqual(parseNames('a, A, b'), ['a', 'b'], 'the same name twice, any case, once');
  const before = doc();
  const d = addNotable(structuredClone(before), '  Chase runes ', 'Zed, Jah');
  assert.deepEqual(d.notable.at(-1), { label: 'Chase runes', names: ['Zed', 'Jah'] });
  assert.deepEqual(d.notable.slice(0, 2), before.notable);
  onlyChanged(before, d, ['notable']);
  assert.throws(() => addNotable(doc(), '   ', 'zed'), /name the group/i);
  assert.throws(() => addNotable(doc(), 'Runes', ' , '), /at least one item/i);
  assert.throws(() => addNotable(doc(), 'angelic KEY', 'x'), /already/i);
  const removed = removeNotable(d, 'Chase runes');
  assert.deepEqual(removed.notable, before.notable);
  assert.deepEqual(removeNotable(doc(), 'Angelic Key').notable, [before.notable[1]]);
});

test('ids are ones the sound store files, and never repeat one in the document', () => {
  const taken = new Set(['a', 'b']);
  for (let i = 0; i < 200; i++) {
    const id = makeId(taken);
    assert.match(id, /^[A-Za-z0-9_-]{1,64}$/);
    assert.ok(!taken.has(id));
    taken.add(id);
  }
});

test('the active filter is the one `filter` names, or none', () => {
  assert.equal(activeFilter(doc()).id, 'f1');
  const d = doc();
  d.filter = 'gone';
  assert.equal(activeFilter(d), null);
  assert.equal(activeFilter({}), null);
  const before = doc();
  before.filters.push({ id: 'f2', name: 'Two', lists: [] });
  const picked = selectFilter(structuredClone(before), 'f2');
  assert.equal(picked.filter, 'f2');
  onlyChanged(before, picked, ['filter']);
});

test('New filter adds one list and makes it active; Delete answers the sound keys it leaves behind', () => {
  const before = doc();
  const d = structuredClone(before);
  const made = addFilter(d, 'Runes');
  assert.equal(d.filter, made.id);
  assert.equal(d.filters.length, 2);
  assert.deepEqual(d.filters[0], before.filters[0]);
  assert.equal(d.filters[1].name, 'Runes');
  assert.equal(d.filters[1].lists.length, 1);
  assert.deepEqual({ ...d.filters[1].lists[0], id: 'x' }, { id: 'x', name: 'List 1', enabled: true, volume: LIST_VOLUME, items: [] });
  onlyChanged(before, d, ['filters', 'filter']);
  const gone = removeFilter(d, 'f1');
  assert.deepEqual(gone, ['list-a', 'list-b']);
  assert.deepEqual(d.filters.map((f) => f.id), [made.id]);
  assert.equal(d.filter, made.id, 'the active filter stays when another goes');
  removeFilter(d, made.id);
  assert.deepEqual(d.filters, []);
  assert.equal(d.filter, '');
  const again = doc();
  again.filters.push({ id: 'f2', name: 'Two', lists: [] });
  removeFilter(again, 'f1');
  assert.equal(again.filter, 'f2', 'deleting the active filter picks the first left');
});

test('Duplicate copies the lists under fresh ids and answers which sounds to copy', () => {
  const before = doc();
  const d = structuredClone(before);
  const { id, pairs } = duplicateFilter(d, 'f1');
  const copy = d.filters[1];
  assert.equal(copy.id, id);
  assert.equal(d.filter, id);
  assert.equal(copy.name, 'Hunt copy');
  assert.deepEqual(copy.lists.map((l) => l.items), before.filters[0].lists.map((l) => l.items));
  assert.ok(copy.lists.every((l, i) => l.id !== before.filters[0].lists[i].id));
  assert.deepEqual(pairs, copy.lists.map((l, i) => [`list-${before.filters[0].lists[i].id}`, `list-${l.id}`]));
  assert.deepEqual(d.filters[0], before.filters[0], 'the original is untouched');
  copy.lists[0].items.push('more');
  assert.equal(d.filters[0].lists[0].items.length, 2, 'the copy shares no array with the original');
});

test('renaming, lists and items keep every field they do not change', () => {
  const before = doc();
  const d = structuredClone(before);
  renameFilter(d, 'f1', '  Hell farming ');
  assert.equal(d.filters[0].name, 'Hell farming');
  assert.equal(d.filters[0].note, 'unknown, kept');
  const listId = addList(d, 'f1');
  assert.equal(d.filters[0].lists.length, 3);
  assert.equal(d.filters[0].lists[2].id, listId);
  assert.equal(d.filters[0].lists[2].name, 'List 3');
  setListField(d, 'f1', 'a', 'volume', 0.555);
  setListField(d, 'f1', 'a', 'enabled', false);
  setListField(d, 'f1', 'a', 'name', ' Chase it ');
  assert.deepEqual(d.filters[0].lists[0], { id: 'a', name: 'Chase it', enabled: false, volume: 0.56, items: ['Zed Rune', 'Astral Covenant'], extra: 1 });
  assert.equal(addItem(d, 'f1', 'a', '  Jah Rune '), true);
  assert.equal(addItem(d, 'f1', 'a', 'JAH RUNE'), false, 'already there, any case');
  assert.equal(addItem(d, 'f1', 'a', '   '), false);
  assert.deepEqual(d.filters[0].lists[0].items, ['Zed Rune', 'Astral Covenant', 'Jah Rune']);
  removeItem(d, 'f1', 'a', 'Zed Rune');
  assert.deepEqual(d.filters[0].lists[0].items, ['Astral Covenant', 'Jah Rune']);
  assert.equal(moveList(d, 'f1', 'b', -1), true);
  assert.deepEqual(d.filters[0].lists.map((l) => l.id), ['b', 'a', listId]);
  assert.equal(moveList(d, 'f1', 'b', -1), false, 'already first');
  assert.deepEqual(removeList(d, 'f1', 'a'), 'list-a');
  assert.deepEqual(d.filters[0].lists.map((l) => l.id), ['b', listId]);
  onlyChanged(before, d, ['filters']);
  assert.throws(() => addItem(d, 'nope', 'a', 'x'), /filter/);
});

test('an item on two lists is a clash, named with both lists', () => {
  const found = clashes(doc().filters[0]);
  assert.deepEqual([...found.entries()], [['zed rune', ['Chase', 'Keys']]]);
  assert.equal(clashes(null).size, 0);
});

test('a sound file is known by its extension, and its source line names it', () => {
  assert.equal(soundExt('My Chime.WAV'), 'wav');
  assert.equal(soundExt('x.flac'), 'flac');
  assert.equal(soundExt('x.m4a'), null);
  assert.equal(soundExt('wav'), null);
  const listing = { set: { ext: 'ogg', stamp: '1' }, 'list-a': { ext: 'mp3', stamp: '2' } };
  assert.equal(soundSource('set', listing), 'set.ogg');
  assert.equal(soundSource('satanic', listing), null, 'no file: the bundled sound');
  assert.equal(soundSource('list-a', listing), 'list-a.mp3');
});

test('base64 round-trips bytes', () => {
  const bytes = new Uint8Array(70000).map((_, i) => (i * 37) & 255);
  const text = bytesToBase64(bytes);
  assert.equal(text, Buffer.from(bytes).toString('base64'));
  assert.deepEqual(base64ToBytes(text), bytes);
});

test("a filter exports in HS Offline Tracker's format, its sounds inlined, and imports back under fresh ids", () => {
  const filter = doc().filters[0];
  const exported = exportFilter(filter, { a: { ext: 'wav', data: 'UklGRg==' } });
  assert.deepEqual(exported, {
    app: EXPORT_APP, version: 1, name: 'Hunt',
    lists: [
      { name: 'Chase', enabled: true, volume: 0.7, items: ['Zed Rune', 'Astral Covenant'], sound: { ext: 'wav', data: 'UklGRg==' } },
      { name: 'Keys', enabled: false, volume: 0.3, items: ['Angelic Key', 'zed rune'], sound: null },
    ],
  });
  assert.equal(EXPORT_APP, 'hs-offline-tracker');
  assert.equal(exportFileName('Hell / farming'), 'Hell - farming.hsofflinetracker.json');
  const parsed = parseFilterExport(JSON.stringify(exported));
  const before = doc();
  const d = structuredClone(before);
  const { id, sounds } = importFilter(d, parsed);
  const made = d.filters.at(-1);
  assert.equal(made.id, id);
  assert.equal(d.filter, id);
  assert.equal(made.name, 'Hunt');
  assert.deepEqual(made.lists.map(({ id: _id, ...rest }) => rest), filter.lists.map(({ id: _id, extra: _x, ...rest }) => rest));
  assert.ok(made.lists.every((l) => !['a', 'b'].includes(l.id)));
  assert.deepEqual(sounds, [{ key: `list-${made.lists[0].id}`, ext: 'wav', data: 'UklGRg==' }]);
  assert.deepEqual(d.filters[0], before.filters[0]);
});

test('an import reads the original project and older files, and refuses anything else', () => {
  const tracker = { app: 'hs-tracker', version: 1, name: 'Old', lists: [{ name: 'L', items: ['x'] }] };
  const parsed = parseFilterExport(JSON.stringify(tracker));
  assert.deepEqual(parsed.lists, [{ name: 'L', enabled: true, volume: LIST_VOLUME, items: ['x'], sound: null }], 'missing fields take the defaults');
  assert.equal(parseFilterExport(JSON.stringify({ ...tracker, app: 'forgepact' })).name, 'Old');
  assert.throws(() => parseFilterExport('{"hello": 1}'), /not a sound filter/i);
  assert.throws(() => parseFilterExport('not json'), /not a sound filter/i);
  assert.throws(() => parseFilterExport(JSON.stringify({ ...tracker, app: 'other' })), /not a sound filter/i);
  assert.throws(() => parseFilterExport(JSON.stringify({ ...tracker, lists: [{ name: 'L', items: [3] }] })), /not a sound filter/i);
  const loud = parseFilterExport(JSON.stringify({ ...tracker, lists: [{ name: 'L', volume: 4, items: [], sound: { ext: 'exe', data: 'AA==' } }] }));
  assert.equal(loud.lists[0].volume, 1, 'clamped');
  assert.equal(loud.lists[0].sound, null, 'a sound of a kind the store does not file is dropped');
});
