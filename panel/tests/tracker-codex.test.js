// The Tracker tab's item codex (src/tracker/codex/rows.js, #185), its pure
// parts under plain node: the name search, the rarity and kind filters, the
// two orders, and how a chance, a grade and a place are written. The browser
// half (that the page fetches `/api/tracker/codex` only when the section
// opens, and shows what it answers) is tests/tracker/codex.check.mjs.
//
//   node --test tests/tracker-codex.test.js
//
// The rows are shaped as `/api/tracker/codex` answers them
// (src/forgepact_tracker/codex.py): `{name, rarity, tier, type, group, rate,
// chase, zones, places}`, with `""`, 0 and empty lists where the tables have
// nothing, which is what most rows carry.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  SHOWN, findRows, gradeLabel, groupOptions, matches, odds, rarityClass, rarityOptions, sortRows, whereLabel,
} from '../src/tracker/codex/rows.js';

const row = (name, fields = {}) => ({
  name, rarity: '', tier: 0, type: 'Ring', group: 'Ring', rate: 0, chase: 0, zones: [], places: [], ...fields,
});

const ROWS = [
  row('The Colossal Avenger', { rarity: 'Heroic', tier: 6, type: 'Helmet', group: 'Helmet', rate: 496750, chase: 157718, zones: ['9-4', '9-5'], places: ['Act IX Zone 4-5'] }),
  row('The Bucket', { rarity: 'Satanic', tier: 6, type: 'Helmet', group: 'Helmet', rate: 282250, chase: 282250, places: ['Common Chest', 'Crystal Chest'] }),
  row("Grimbone's Visage", { rarity: 'Heroic', tier: 6, type: 'Helmet', group: 'Helmet', places: ['Grimbone'] }),
  row('Bloodmoon Cleaver', { rarity: 'Satanic', tier: 3, type: 'Axe', group: 'Weapon', rate: 880 }),
  row('Astral Covenant', { rarity: 'Angelic', tier: 5, type: 'Amulet', group: 'Amulet', rate: 31900 }),
  row('Oath of the Hollow Star', { rarity: 'Unholy', tier: 4, rate: 48200, chase: 6400, zones: ['9-BD'] }),
  row('Iron Ring', { rarity: 'Common', tier: 1, rate: 120 }),
  row('Mystery Shard', { group: 'Material', type: 'Material' }),
];

const names = (list) => list.map((r) => r.name);

test('baseline: no search and no filter keeps every row', () => {
  assert.equal(findRows(ROWS, {}).length, ROWS.length);
  assert.equal(findRows(ROWS, { query: '   ', rarity: '', group: '' }).length, ROWS.length);
  assert.deepEqual(new Set(names(findRows(ROWS, {}))), new Set(names(ROWS)));
  // An empty or missing answer is an empty codex, never an error.
  assert.deepEqual(findRows([], { query: 'x' }), []);
  assert.deepEqual(findRows(undefined, {}), []);
});

test('the name search narrows the list, ignoring case and the spaces around it', () => {
  assert.deepEqual(names(findRows(ROWS, { query: 'the', sort: 'name' })), ['Oath of the Hollow Star', 'The Bucket', 'The Colossal Avenger']);
  assert.deepEqual(names(findRows(ROWS, { query: '  BUCKET ' })), ['The Bucket']);
  assert.deepEqual(findRows(ROWS, { query: 'no such item' }), []);
  // It reads the name only: a place or a kind is not a name.
  assert.deepEqual(findRows(ROWS, { query: 'grimbone' }).length, 1);
  assert.deepEqual(findRows(ROWS, { query: 'crystal chest' }), []);
  assert.deepEqual(findRows(ROWS, { query: 'helmet' }), []);
});

test('the rarity and kind filters narrow the list, and stack with the search', () => {
  assert.deepEqual(new Set(names(findRows(ROWS, { rarity: 'Heroic' }))), new Set(['The Colossal Avenger', "Grimbone's Visage"]));
  assert.deepEqual(names(findRows(ROWS, { rarity: 'Unholy' })), ['Oath of the Hollow Star']);
  assert.deepEqual(new Set(names(findRows(ROWS, { group: 'Helmet' }))), new Set(['The Colossal Avenger', 'The Bucket', "Grimbone's Visage"]));
  assert.deepEqual(names(findRows(ROWS, { group: 'Helmet', rarity: 'Satanic' })), ['The Bucket']);
  assert.deepEqual(names(findRows(ROWS, { query: 'the', rarity: 'Heroic' })), ['The Colossal Avenger']);
  // The kind filter reads the item type, not the weapon's own kind.
  assert.deepEqual(names(findRows(ROWS, { group: 'Weapon' })), ['Bloodmoon Cleaver']);
  assert.deepEqual(findRows(ROWS, { group: 'Axe' }), []);
  assert.equal(matches(ROWS[0], { rarity: 'Heroic', group: 'Helmet', query: 'avenger' }), true);
  assert.equal(matches(ROWS[0], { rarity: 'Satanic' }), false);
});

test('rarest first: the longest odds lead, and what the tables cannot price goes last', () => {
  assert.deepEqual(names(findRows(ROWS, {})), [
    'The Colossal Avenger', 'The Bucket', 'Oath of the Hollow Star', 'Astral Covenant', 'Bloodmoon Cleaver', 'Iron Ring',
    // Unpriced, by name.
    "Grimbone's Visage", 'Mystery Shard',
  ]);
  // The default is rarest first; an unknown order is too.
  assert.deepEqual(names(sortRows(ROWS, 'rarest')), names(findRows(ROWS, {})));
  assert.deepEqual(names(sortRows(ROWS, 'what')), names(findRows(ROWS, {})));
  // Equal odds fall back to the name, so the order never depends on the answer's.
  const tied = [row('B', { rate: 10 }), row('A', { rate: 10 }), row('C', { rate: 20 })];
  assert.deepEqual(names(sortRows(tied, 'rarest')), ['C', 'A', 'B']);
  assert.deepEqual(names(sortRows([...tied].reverse(), 'rarest')), ['C', 'A', 'B']);
});

test('by name: alphabetical, whatever the odds', () => {
  assert.deepEqual(names(findRows(ROWS, { sort: 'name' })), [
    'Astral Covenant', 'Bloodmoon Cleaver', "Grimbone's Visage", 'Iron Ring', 'Mystery Shard', 'Oath of the Hollow Star', 'The Bucket', 'The Colossal Avenger',
  ]);
});

test('sorting never changes the list it was given', () => {
  const before = names(ROWS);
  sortRows(ROWS, 'name');
  findRows(ROWS, { sort: 'name', rarity: 'Heroic' });
  assert.deepEqual(names(ROWS), before);
});

test('the filters offer what the rows carry: the five rarities in the tab\'s order, then the rest', () => {
  assert.deepEqual(rarityOptions(ROWS), ['Satanic', 'Heroic', 'Angelic', 'Unholy', 'Common']);
  assert.deepEqual(rarityOptions([row('a', { rarity: 'Unholy' }), row('b', { rarity: 'Set' }), row('c', { rarity: 'Set' })]), ['Set', 'Unholy']);
  assert.deepEqual(groupOptions(ROWS), ['Amulet', 'Helmet', 'Material', 'Ring', 'Weapon']);
  assert.deepEqual(rarityOptions([]), []);
  assert.deepEqual(groupOptions(undefined), []);
});

test('a chance is "1 in N" with the digits grouped, and none is a dash', () => {
  assert.equal(odds(48200), '1/48,200');
  assert.equal(odds(496750), '1/496,750');
  assert.equal(odds(880), '1/880');
  assert.equal(odds(0), '—');
  assert.equal(odds(undefined), '—');
  assert.equal(odds(-5), '—');
});

test('a grade is the game\'s letter; a tier the letters lack says its number, and none is a dash', () => {
  assert.deepEqual([1, 2, 3, 4, 5, 6].map(gradeLabel), ['D', 'C', 'B', 'A', 'S', 'SS']);
  assert.equal(gradeLabel(7), 'T7');
  assert.equal(gradeLabel(0), '—');
  assert.equal(gradeLabel(undefined), '—');
});

test('where it drops: the places in words, else the zone codes, else anywhere', () => {
  assert.equal(whereLabel(ROWS[0]), 'Act IX Zone 4-5');
  assert.equal(whereLabel(ROWS[1]), 'Common Chest · Crystal Chest');
  assert.equal(whereLabel(ROWS[5]), '9-BD');
  assert.equal(whereLabel(ROWS[3]), 'anywhere');
  assert.equal(whereLabel({ name: 'x' }), 'anywhere');
});

test('a rarity colour only for the five the panel colours', () => {
  assert.equal(rarityClass('Satanic'), 'r-satanic');
  assert.equal(rarityClass('Unholy'), 'r-unholy');
  assert.equal(rarityClass('Common'), '');
  assert.equal(rarityClass(''), '');
});

test('the page draws a bounded number of rows at once', () => {
  assert.ok(Number.isInteger(SHOWN) && SHOWN > 0 && SHOWN < 1000);
});
