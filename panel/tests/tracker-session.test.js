// The Tracker tab's Session section (src/tracker/session/view.js, #185), its
// pure half under plain node: the four totals and their notes, the loot table,
// notable counts and tallies, the Satanic zone's name and mods, the series
// charts' points and the journal's rows. The browser half (that the page shows
// what `/api/tracker` and `/extra` answered, pause, End run) is
// tests/tracker/session.check.mjs.
//
//   node --test tests/tracker-session.test.js

import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  CHART_H, CHART_MAX_POINTS, CHART_W, characterLine, chartPoints, dropTotals, grade, journalRows, lootRows, modRows,
  notableRows, rolledAgo, sessionClock, sessionState, tallyGroups, timeOfDay, totals, zoneName,
} from '../src/tracker/session/view.js';
import { RARITIES } from '../src/tracker/format.js';

// The shape `/api/tracker`'s snapshot has (ledger.py `snapshot()`), as the
// sandbox's fixture session answers it, with every rarity the ledger keeps.
function snapshot(over = {}) {
  const items = {};
  for (const name of ['Common', 'Superior', 'Rare', 'Set', 'Mythic', 'Satanic', 'Angelic', 'Blessed', 'Heroic', 'Unholy']) {
    items[name] = { total: 0, mf: 0, per_hour: 0 };
  }
  items.Heroic = { total: 1, mf: 0, per_hour: 40 };
  return {
    session_secs: 90, paused: false,
    gold: { total: null, earned: 200, per_hour: 8000 },
    xp: { total: null, earned: 400, per_hour: 16000 },
    kills: { total: null, earned: 6, per_hour: 240 },
    notable: [{ label: 'Angelic Key', total: 0 }, { label: 'SS runes', total: 2 }],
    items, ss: 0, tallies: [],
    satanic_zone: { zone: 'Satanic_1_3', buffs: [6, 14], debuffs: [3] },
    character: { name: 'Sorak', level: 0, herolevel: 0, difficulty: 0, hell_sub: 0, hardcore: false, season: 0 },
    ...over,
  };
}

test('baseline: no snapshot yet is loading, and a store with no session and no character is the empty state', () => {
  assert.equal(sessionState({}), 'loading');
  assert.equal(sessionState({ snapshot: snapshot({ character: null }), session: { file: null, live: false } }), 'none');
});

test('target: a session file or a named character is a session to show', () => {
  assert.equal(sessionState({ snapshot: snapshot({ character: null }), session: { file: 'session-777-1.ndjson' } }), 'session');
  assert.equal(sessionState({ snapshot: snapshot(), session: null }), 'session');
});

test('the character line says only what the save told', () => {
  // The sensor names the character before its save is read: nothing else is known.
  assert.equal(characterLine({ name: 'Sorak', level: 0, herolevel: 0, difficulty: 0, hell_sub: 0, season: 0 }), '');
  assert.equal(characterLine({ name: 'Sorak', level: 100, herolevel: 300, difficulty: 2, hell_sub: 5, season: 10 }),
    'Level 100 · Hero 300 · Hell 5 · Season 10');
  assert.equal(characterLine({ level: 40, herolevel: 0, difficulty: 0, hell_sub: 0, season: 0, hardcore: true }),
    'Level 40 · Normal · Hardcore');
  assert.equal(characterLine(null), '');
});

test('the four totals: earned, its rate, and the lifetime count only where the save gave one', () => {
  const rows = totals(snapshot());
  assert.deepEqual(rows.map((r) => r.key), ['gold', 'xp', 'kills', 'drops']);
  const [gold, xp, kills, drops] = rows;
  assert.equal(gold.value, '200');
  assert.equal(gold.rate, '8,000/h');
  // Gold's bank is never read offline: no total, and the note says so.
  assert.equal(gold.total, null);
  assert.equal(gold.note, 'bank not read offline');
  assert.equal(xp.rate, '16.0k/h');
  assert.equal(kills.value, '6');
  assert.equal(kills.note, 'total not read yet');
  // Drops are the five rarities only: a Common in the snapshot is not one.
  const withCommon = snapshot();
  withCommon.items.Common = { total: 50, mf: 0, per_hour: 2000 };
  assert.equal(totals(withCommon)[3].earned, 1);
  assert.equal(drops.earned, 1);
  assert.equal(drops.rate, '40/h');
  const saved = totals(snapshot({ xp: { total: 728_840_000, earned: 42_180_000, per_hour: 41_360_000 },
    kills: { total: 91_400, earned: 4862, per_hour: 4766 } }));
  assert.equal(saved[1].value, '42.18kk');
  assert.equal(saved[1].note, '728.84kk total');
  assert.equal(saved[2].note, '91.4k total');
});

test('the loot table lists the five rarities in order, each with its class', () => {
  const rows = lootRows(snapshot());
  assert.deepEqual(rows.map((r) => r.name), RARITIES);
  const heroic = rows.find((r) => r.name === 'Heroic');
  assert.deepEqual(heroic, { name: 'Heroic', cls: 'r-heroic', total: 1, mf: 0, per_hour: 40 });
  assert.deepEqual(dropTotals(snapshot()), { total: 1, per_hour: 40 });
  // A snapshot with no items at all reads as zeros, not as missing rows.
  assert.equal(lootRows({}).length, 5);
});

test('notable counts: SS-grade gear first, then the named groups as the snapshot lists them', () => {
  assert.deepEqual(notableRows(snapshot({ ss: 7 })), [
    { label: 'SS-grade gear', total: 7 }, { label: 'Angelic Key', total: 0 }, { label: 'SS runes', total: 2 },
  ]);
  assert.deepEqual(notableRows(null), [{ label: 'SS-grade gear', total: 0 }]);
});

test('tallies split into bosses and chests, keeping the order they came in', () => {
  const groups = tallyGroups([
    { label: 'Satan', group: 'boss', total: 2 }, { label: 'Crystal', group: 'chest', total: 23 },
    { label: 'Wormholes', group: 'boss', total: 1 },
  ]);
  assert.deepEqual(groups.bosses.map((t) => t.label), ['Satan', 'Wormholes']);
  assert.deepEqual(groups.chests.map((t) => t.label), ['Crystal']);
  assert.deepEqual(tallyGroups(undefined), { bosses: [], chests: [] });
});

test('the zone chip names the zone, and a code it cannot read is shown as it came', () => {
  assert.equal(zoneName('Satanic_1_3'), 'Act 1 · The Pumpkin Patch');
  assert.equal(zoneName('Satanic_9_4'), 'Act 9 · Boreal Island');
  assert.equal(zoneName('SZ_5_5'), 'Act 5 · Temple of Zamjo');
  // A zone past the table's end is still placed, by number.
  assert.equal(zoneName('Satanic_2_9'), 'Act 2 · Zone 9');
  assert.equal(zoneName('Town_01_rm'), 'Town_01_rm');
});

test("the zone's mods are named from /api/state's lists, and an unknown id keeps its number", () => {
  const list = [[6, 'Angelic Fortune', 'Angelic Item drop chances increased'], [14, 'Artifact Digger', 'Magic Find increased']];
  assert.deepEqual(modRows([6, 14], list), [
    { id: 6, name: 'Angelic Fortune', desc: 'Angelic Item drop chances increased' },
    { id: 14, name: 'Artifact Digger', desc: 'Magic Find increased' },
  ]);
  assert.deepEqual(modRows([99], list, 'Debuff'), [{ id: 99, name: 'Debuff 99', desc: '' }]);
  assert.deepEqual(modRows(null, list), []);
});

test('when the zone was rolled', () => {
  const now = 1_700_000_000_000;
  assert.equal(rolledAgo(now - 20_000, now), 'rolled just now');
  assert.equal(rolledAgo(now - 11 * 60_000, now), 'rolled 11 min ago');
  assert.equal(rolledAgo(now - 3 * 3_600_000, now), 'rolled 3 h ago');
  assert.equal(rolledAgo(now - 3 * 86_400_000, now), 'rolled in an earlier session');
  assert.equal(rolledAgo(null, now), '');
});

test('a chart starts at nothing earned at 0:00 and ends at the latest sample, top of its own axis', () => {
  const series = [{ t: 60, gold: 100, xp: 10 }, { t: 120, gold: 300, xp: 40 }];
  const gold = chartPoints(series, 'gold');
  assert.equal(gold.samples, 2);
  assert.equal(gold.points.length, 3);
  assert.deepEqual(gold.points[0], [0, CHART_H]);
  assert.deepEqual(gold.points.at(-1), [CHART_W, 0]);
  assert.equal(gold.latest, 300);
  assert.equal(gold.span, 120);
  // Each counter has its own scale: xp's top is its own maximum.
  assert.deepEqual(chartPoints(series, 'xp').points.at(-1), [CHART_W, 0]);
  assert.equal(gold.attr, `0,${CHART_H} 50,26.67 100,0`);
  // No series yet: only the origin.
  const empty = chartPoints([], 'gold');
  assert.equal(empty.samples, 0);
  assert.equal(empty.points.length, 1);
});

test('a long series is thinned to the cap, keeping its last point', () => {
  const series = Array.from({ length: 4000 }, (_, i) => ({ t: (i + 1) * 60, gold: i, xp: i }));
  const chart = chartPoints(series, 'gold');
  assert.equal(chart.samples, 4000);
  assert.equal(chart.points.length, CHART_MAX_POINTS + 1);
  assert.deepEqual(chart.points.at(-1), [CHART_W, 0]);
});

test('grades: tier 1 is D, tier 6 is SS, anything else has none', () => {
  assert.equal(grade(1), 'D');
  assert.equal(grade(6), 'SS');
  assert.equal(grade(0), null);
  assert.equal(grade(undefined), null);
});

test("the journal keeps /extra's order and says what it does not know", () => {
  const drops = [
    { id: 'f:12', ts_ms: 1_700_000_090_000, rarity: 'Angelic', tier: 5, name: 'Astral Covenant', mf: true, announced: true },
    { id: 'f:9', ts_ms: 1_700_000_020_000, rarity: 'Satanic', tier: 0, name: '', mf: false, announced: false },
  ];
  const rows = journalRows(drops);
  assert.deepEqual(rows.map((r) => r.id), ['f:12', 'f:9']);
  assert.equal(rows[0].grade, 'S');
  assert.equal(rows[0].cls, 'r-angelic');
  assert.equal(rows[0].mf, true);
  assert.equal(rows[0].alerted, true);
  assert.equal(rows[0].time, timeOfDay(1_700_000_090_000));
  assert.match(rows[0].time, /^\d\d:\d\d:\d\d$/);
  assert.equal(rows[1].named, false);
  assert.equal(rows[1].name, 'A drop whose name the game did not give');
  assert.equal(rows[1].grade, null);
  assert.deepEqual(journalRows(null), []);
});

test('the session clock', () => {
  assert.equal(sessionClock({ session_secs: 3733 }), '1:02:13');
  assert.equal(sessionClock(null), '0:00:00');
});
