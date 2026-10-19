// The Tracker tab's Runs section (#185): what a run says in the list and the
// picked run (tracker/runs/summary.js), the room names it uses
// (tracker/runs/rooms.js), and the run card's layout arithmetic
// (tracker/runs/runcard.js). Pure modules under plain node; the drawing and
// the clipboard are tests/tracker/runs.check.mjs's, in a real browser.

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fmt, span, RARITIES } from '../src/tracker/format.js';
import { ROOMS } from '../src/tracker/runs/rooms.js';
import {
  drops, finds, grade, headline, perHour, runKey, tallies, titleLine, when, whoLine, zoneLabel, zones,
} from '../src/tracker/runs/summary.js';
import { CARD, FONTS, cardLabel, cardLayout, fit, fontString, readPalette } from '../src/tracker/runs/runcard.js';

// A run as the API answers it, with the fields a ForgePact-filed run carries.
const RUN = {
  id: 'session-777-1700000000000.ndjson#1700000000000',
  started_ms: new Date(2026, 9, 14, 20, 39).getTime(),
  ended_ms: new Date(2026, 9, 14, 21, 41).getTime(),
  secs: 3720,
  character: 'Sorak',
  level: 100,
  difficulty: 2,
  hell_sub: 5,
  herolevel: 300,
  hardcore: false,
  gold: 14860000,
  xp: 42180000,
  kills: 4862,
  items: { Common: 400, Satanic: 91, Set: 17, Heroic: 8, Angelic: 5, Unholy: 2 },
  notable: [
    { name: 'Astral Covenant', rarity: 'Angelic', tier: 5, ts_ms: 3 },
    { name: 'Oath of the Hollow Star', rarity: 'Unholy', tier: 4, ts_ms: 2 },
    { name: 'Qi Rune', rarity: 'Satanic', tier: 6, ts_ms: 1 },
  ],
  zones: [['Act_01_02', 2280], ['Chaos_Tower_rm', 1140], ['Town_09', 300]],
  tallies: [
    { label: 'Act bosses', group: 'boss', total: 6 },
    { label: 'Golden Chest', group: 'chest', total: 3 },
    { label: 'Crystal Chest', group: 'chest', total: 0 },
  ],
};

// A run as HS Offline Tracker filed it before 0.9.8: no id, no tallies, no
// hero level.
const OLD = {
  started_ms: new Date(2026, 9, 12, 19, 40).getTime(),
  ended_ms: new Date(2026, 9, 12, 19, 58).getTime(),
  secs: 1080, character: null, level: 0, difficulty: 0, gold: 2070000, xp: 0, kills: 0,
  items: { Satanic: 0, Set: 0, Heroic: 0, Angelic: 0, Unholy: 0 }, notable: [], zones: [],
};

// Text as wide as a canvas would measure it, near enough: 0.6 em a character.
const measure = (text, role) => String(text).length * FONTS[role].size * 0.6;

// ---- rooms.js is item_table.py's ROOMS -------------------------------------

test('rooms.js holds exactly the backend item table\'s ROOMS', () => {
  const src = readFileSync(new URL('../../src/forgepact_tracker/item_table.py', import.meta.url), 'utf8').replace(/\r\n/g, '\n');
  const start = src.indexOf('\nROOMS = (\n');
  assert.ok(start >= 0, 'item_table.py has no ROOMS table');
  const body = src.slice(start + '\nROOMS = (\n'.length);
  const lines = body.slice(0, body.indexOf('\n)\n')).split('\n');
  const pairs = lines.map((line) => {
    const m = /^\s*\((['"])(.*?)\1, (['"])(.*?)\3\),$/.exec(line);
    assert.ok(m, `an unparsed ROOMS line: ${line}`);
    return [m[2], m[4]];
  });
  assert.ok(pairs.length > 100, `only ${pairs.length} rooms parsed`);
  assert.deepEqual([...ROOMS.entries()], pairs);
});

// ---- summary.js -------------------------------------------------------------

test('zoneLabel: the game\'s name, else Town, else the act and zone, else the asset name', () => {
  assert.equal(zoneLabel('Act_01_02'), ROOMS.get('Act_01_02'));
  assert.equal(zoneLabel('Town_09'), ROOMS.get('Town_09') ?? 'Town');
  assert.equal(zoneLabel('Town_Nowhere'), 'Town');
  assert.equal(zoneLabel('Act_42_07'), 'Act 42 · Zone 7');
  assert.equal(zoneLabel('Some_Odd_Place_rm'), 'Some Odd Place');
  assert.equal(zoneLabel(null), 'Somewhere');
});

test('when: today, yesterday, this year, another year, all in local time', () => {
  const now = new Date(2026, 9, 14, 23, 0).getTime();
  assert.equal(when(new Date(2026, 9, 14, 20, 39).getTime(), now), 'Today, 20:39');
  assert.equal(when(new Date(2026, 9, 13, 22, 15).getTime(), now), 'Yesterday, 22:15');
  assert.equal(when(new Date(2026, 9, 12, 9, 5).getTime(), now), '12 Oct, 09:05');
  assert.equal(when(new Date(2025, 0, 2, 7, 0).getTime(), now), '2 Jan 2025, 07:00');
  // Yesterday across a month's end.
  assert.equal(when(new Date(2026, 8, 30, 23, 59).getTime(), new Date(2026, 9, 1, 0, 1).getTime()), 'Yesterday, 23:59');
  assert.equal(when(undefined, now), 'Unknown time');
});

test('runKey: the id when there is one, else the run\'s own start and end', () => {
  assert.equal(runKey(RUN), RUN.id);
  assert.equal(runKey(OLD), `${OLD.started_ms}-${OLD.ended_ms}`);
  assert.notEqual(runKey(OLD), runKey({ ...OLD, ended_ms: OLD.ended_ms + 1 }));
});

test('drops, perHour: the five rarities only, a rate over the run\'s own seconds', () => {
  assert.equal(drops(RUN), 91 + 17 + 8 + 5 + 2, 'Common is not one of the five');
  assert.equal(drops(OLD), 0);
  assert.equal(drops({}), 0);
  assert.equal(perHour(100, 1800), 200);
  assert.equal(perHour(100, 0), 0);
});

test('headline: gold and the first two rarities that dropped, else gold and kills', () => {
  assert.equal(headline(RUN), `${fmt(RUN.gold)} gold · 91 Satanic · 17 Set`);
  assert.equal(headline({ ...RUN, items: { Satanic: 60, Unholy: 1 } }), `${fmt(RUN.gold)} gold · 60 Satanic · 1 Unholy`);
  assert.equal(headline(OLD), `${fmt(OLD.gold)} gold · 0 kills`);
});

test('whoLine and titleLine name the character, its levels and the difficulty', () => {
  assert.equal(whoLine(RUN), 'Sorak · Level 100 · Hero 300 · Hell 5');
  assert.equal(whoLine({ ...RUN, hardcore: true }), 'Sorak · Level 100 · Hero 300 · Hell 5 · Hardcore');
  assert.equal(whoLine(OLD), 'Unknown character · Normal');
  assert.equal(titleLine(RUN), 'Sorak · Hell 5');
  assert.equal(titleLine({ character: 'Sorak' }), 'Sorak');
});

test('tallies, zones, finds and grades: only what the run has', () => {
  assert.deepEqual(tallies(RUN, 'boss').map((t) => t.label), ['Act bosses']);
  assert.deepEqual(tallies(RUN, 'chest').map((t) => t.label), ['Golden Chest'], 'a zero tally is left out');
  assert.deepEqual(tallies(OLD, 'boss'), []);
  const where = zones(RUN);
  assert.deepEqual(where.map((z) => z.label), [zoneLabel('Act_01_02'), zoneLabel('Chaos_Tower_rm'), zoneLabel('Town_09')]);
  assert.equal(where[0].share, 2280 / 3720);
  assert.ok(zones({ secs: 10, zones: [['A', 20]] })[0].share <= 1, 'a share never passes the whole run');
  assert.deepEqual(zones(OLD), []);
  assert.deepEqual(finds(RUN).map((f) => f.name), ['Astral Covenant', 'Oath of the Hollow Star', 'Qi Rune']);
  assert.deepEqual(finds({ notable: [{ name: '', rarity: 'Set', tier: 1 }] }), []);
  assert.equal(grade(1), 'D');
  assert.equal(grade(6), 'SS');
  assert.equal(grade(0), '');
});

// ---- runcard.js: the layout arithmetic --------------------------------------

function textOps(layout) {
  return layout.ops.filter((op) => op.kind === 'text');
}

// Every text and every bar stays inside the card's padding.
function assertInside(layout) {
  const left = CARD.pad;
  const right = CARD.width - CARD.pad;
  for (const op of layout.ops) {
    if (op.kind === 'rect') {
      if (op.color === 'bg') continue;
      assert.ok(op.x >= left && op.x + op.w <= right + 0.001, `a bar runs outside the card: ${JSON.stringify(op)}`);
      assert.ok(op.y + op.h <= layout.height - CARD.pad, `a bar runs into the bottom padding: ${JSON.stringify(op)}`);
      continue;
    }
    const w = measure(op.text, op.font);
    const x0 = op.align === 'right' ? op.x - w : op.x;
    assert.ok(x0 >= left - 0.001 && x0 + w <= right + 0.001, `"${op.text}" runs outside the card (${x0}..${x0 + w})`);
    assert.ok(op.y <= layout.height - CARD.pad, `"${op.text}" runs into the bottom padding`);
  }
}

test('the card: a background first, then everything inside the padding', () => {
  const layout = cardLayout(RUN, { measure, now: RUN.ended_ms });
  assert.equal(layout.width, CARD.width);
  assert.deepEqual(layout.ops[0], { kind: 'rect', x: 0, y: 0, w: CARD.width, h: layout.height, color: 'bg' });
  assertInside(layout);
  const texts = textOps(layout).map((op) => op.text);
  assert.ok(texts.includes('ForgePact Tracker'));
  assert.ok(texts.includes(`Today, 20:39 · ${span(RUN.secs)}`));
  assert.ok(texts.includes('Sorak · Hell 5'));
  for (const value of [fmt(RUN.gold), fmt(RUN.xp), fmt(RUN.kills), fmt(drops(RUN))]) assert.ok(texts.includes(value), `no ${value}`);
});

test('the card\'s totals: two by two, the same column width, the rate under each value', () => {
  const layout = cardLayout(RUN, { measure, now: RUN.ended_ms });
  const labels = textOps(layout).filter((op) => op.font === 'label');
  assert.deepEqual(labels.map((op) => op.text), ['Gold', 'Experience', 'Kills', 'Drops']);
  const col = (CARD.width - 2 * CARD.pad - CARD.gap) / 2;
  assert.equal(labels[0].x, CARD.pad);
  assert.equal(labels[1].x, CARD.pad + col + CARD.gap);
  assert.equal(labels[2].x, labels[0].x);
  assert.equal(labels[0].y, labels[1].y);
  assert.ok(labels[2].y > labels[0].y);
  const rates = textOps(layout).filter((op) => op.font === 'rate').map((op) => op.text);
  assert.deepEqual(rates, [RUN.gold, RUN.xp, RUN.kills, drops(RUN)].map((v) => `${fmt(perHour(v, RUN.secs))}/h`));
});

test('the card\'s loot: the rarities that dropped, in their colours, two to a row; all five when none did', () => {
  const some = cardLayout({ ...RUN, items: { Satanic: 3, Angelic: 1, Unholy: 2 } }, { measure, now: 0 });
  const names = textOps(some).filter((op) => op.color.startsWith('rarity-'));
  const loot = names.filter((op) => RARITIES.includes(op.text));
  assert.deepEqual(loot.map((op) => op.text), ['Satanic', 'Angelic', 'Unholy']);
  assert.deepEqual(loot.map((op) => op.color), ['rarity-satanic', 'rarity-angelic', 'rarity-unholy']);
  assert.equal(loot[0].y, loot[1].y, 'two to a row');
  assert.ok(loot[2].y > loot[0].y);
  const none = cardLayout(OLD, { measure, now: 0 });
  assert.deepEqual(textOps(none).filter((op) => RARITIES.includes(op.text)).map((op) => op.text), RARITIES);
  assertInside(none);
});

test('the card\'s height: each optional block adds exactly its own rows', () => {
  const bare = { ...RUN, tallies: [], zones: [], notable: [] };
  const base = cardLayout(bare, { measure, now: 0 }).height;
  const z1 = cardLayout({ ...bare, zones: RUN.zones.slice(0, 1) }, { measure, now: 0 }).height;
  const z3 = cardLayout({ ...bare, zones: RUN.zones }, { measure, now: 0 }).height;
  const z5 = cardLayout({ ...bare, zones: [...RUN.zones, ['Act_02_01', 10], ['Act_02_02', 5]] }, { measure, now: 0 }).height;
  assert.equal(z1 - base, CARD.gap + CARD.subhead + CARD.zoneRow);
  assert.equal(z3 - z1, 2 * CARD.zoneRow);
  assert.equal(z5, z3, 'at most three zones');
  const t2 = cardLayout({ ...bare, tallies: RUN.tallies }, { measure, now: 0 }).height;
  assert.equal(t2 - base, CARD.gap + CARD.subhead + CARD.row, 'two tallies fill one row; a zero is left out');
  const f = cardLayout({ ...bare, notable: RUN.notable }, { measure, now: 0 }).height;
  assert.equal(f - base, CARD.gap + CARD.line);
  const all = cardLayout(RUN, { measure, now: 0 });
  assert.equal(all.height, base + (t2 - base) + (z3 - base) + (f - base));
  assertInside(all);
});

test('the card\'s bars: the share of the run, on a track the content\'s width', () => {
  const layout = cardLayout(RUN, { measure, now: 0 });
  const rects = layout.ops.filter((op) => op.kind === 'rect' && op.color !== 'bg');
  const tracks = rects.filter((op) => op.color === 'sunken');
  const fills = rects.filter((op) => op.color === 'muted');
  assert.equal(tracks.length, 3);
  assert.equal(fills.length, 3);
  const content = CARD.width - 2 * CARD.pad;
  for (const track of tracks) assert.equal(track.w, content);
  assert.equal(fills[0].w, Math.round((2280 / 3720) * content));
  assert.ok(fills.every((op) => op.w >= 2), 'a sliver still shows');
});

test('the card\'s finds: as many names as fit on one line, then how many more', () => {
  const many = Array.from({ length: 12 }, (_, i) => ({ name: `Long Named Treasure ${i + 1}`, rarity: 'Set', tier: 3 }));
  const layout = cardLayout({ ...RUN, notable: many }, { measure, now: 0 });
  assertInside(layout);
  const line = textOps(layout).filter((op) => op.font === 'row' && op.y === Math.max(...textOps(layout).map((o) => o.y)));
  const shown = line.filter((op) => op.text.startsWith('Long Named')).length;
  assert.ok(shown >= 1 && shown < many.length, `${shown} names on the line`);
  assert.equal(line.at(-1).text, `+${many.length - shown} more`);
  assert.equal(line[0].text, 'Finds:');
  // One name too long for the line is cut, never dropped.
  const one = cardLayout({ ...RUN, notable: [{ name: 'X'.repeat(200), rarity: 'Satanic', tier: 6 }] }, { measure, now: 0 });
  assertInside(one);
  assert.ok(textOps(one).some((op) => op.text.endsWith('…') && op.color === 'rarity-satanic'));
});

test('the card cuts a long character name rather than run off its edge', () => {
  const layout = cardLayout({ ...RUN, character: 'Z'.repeat(120) }, { measure, now: 0 });
  assertInside(layout);
  assert.ok(textOps(layout).some((op) => op.font === 'title' && op.text.endsWith('…')));
});

test('fit: the whole text when it fits, else the longest cut with an ellipsis', () => {
  assert.equal(fit('Sorak', 1000, 'title', measure), 'Sorak');
  const cut = fit('abcdefghij', measure('abcde', 'title'), 'title', measure);
  assert.equal(cut, 'abcd…');
  assert.equal(fit('abc', 0, 'title', measure), '');
});

test('readPalette: the page\'s tokens, and the ones it could not read', () => {
  const values = {
    '--color-bg-raised': ' #251e17', '--color-bg-sunken': '#100d0a', '--color-text-primary': '#efe4d3',
    '--color-text-muted': '#b5a794', '--color-rarity-satanic': '#ff6e78', '--color-rarity-set': '#86d877',
    '--color-rarity-heroic': '#5cc8ee', '--color-rarity-angelic': '#f2da8a', '--color-rarity-unholy': '#cc8dff',
    '--font-body-family': ' "IBM Plex Sans"', '--font-mono-family': '"IBM Plex Mono"',
  };
  const palette = readPalette({ getPropertyValue: (name) => values[name] ?? '' });
  assert.deepEqual(palette.missing, []);
  assert.equal(palette.colors.bg, '#251e17');
  assert.equal(palette.colors['rarity-satanic'], '#ff6e78');
  assert.equal(palette.fonts.mono, '"IBM Plex Mono"');
  assert.equal(fontString(FONTS.value, palette.fonts), '400 22px "IBM Plex Mono", system-ui, sans-serif');
  // The negative control: a page without the tokens says which.
  const bare = readPalette({ getPropertyValue: () => '' });
  assert.ok(bare.missing.includes('--color-bg-raised'));
  assert.ok(bare.missing.includes('--font-mono-family'));
});

test('cardLabel says in words what the picture shows', () => {
  const label = cardLabel(RUN, RUN.ended_ms);
  for (const part of ['Sorak', span(RUN.secs), `${fmt(RUN.gold)} gold`, `${fmt(RUN.kills)} kills`]) assert.ok(label.includes(part), `no ${part}: ${label}`);
});
