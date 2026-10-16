// The Tracker's overlay window (src/overlay/queue.js, #185), its pure parts
// under plain node: the newest-drops strip (the ticker) and the drop card's
// queue (the flourish). The browser half (that the page shows them from what
// the API answered, and never claims or plays an alert) is
// tests/tracker/overlay.check.mjs.
//
//   node --test tests/tracker-overlay.test.js

import { test } from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import {
  FlourishQueue, GROUP_MS, LEAVE_MS, MAX_WAITING, TICKER_ROWS, cardMs, dropCard, overlayState, tickerRows, zoneCard,
} from '../src/overlay/queue.js';

// A journal entry as `/api/tracker/extra` gives it (the ledger's shape).
let nextSeq = 1;
function entry(over = {}) {
  const sequence = nextSeq++;
  return {
    id: `session-777.ndjson:${sequence}`, sequence, ts_ms: 1700000000000 + sequence * 1000, rarity: 'Heroic', mf: false,
    tier: 4, name: `Drop ${sequence}`, announced: false, flourish: true, ...over,
  };
}
const LIFE = 4000;

// ---- The ticker ----------------------------------------------------------

test('baseline: an empty or missing journal draws no ticker row', () => {
  assert.deepEqual(tickerRows(undefined), []);
  assert.deepEqual(tickerRows([]), []);
});

test('the ticker shows the newest journal entries, newest first, at most three', () => {
  const journal = [entry({ name: 'Newest', rarity: 'Angelic', tier: 5 }), entry({ name: 'Second' }), entry({ name: 'Third' }), entry({ name: 'Oldest' })];
  const rows = tickerRows(journal);
  assert.equal(TICKER_ROWS, 3);
  assert.deepEqual(rows.map((r) => r.name), ['Newest', 'Second', 'Third']);
  assert.equal(rows[0].cls, 'r-angelic');
  assert.equal(rows[0].grade, 'S');
  assert.equal(rows[0].key, journal[0].id);
  assert.match(rows[0].time, /^\d\d:\d\d:\d\d$/);
});

test('a ticker row with no name says so rather than drawing nothing', () => {
  const [row] = tickerRows([entry({ name: '', tier: 0 })]);
  assert.equal(row.name, 'Heroic drop');
  assert.equal(row.grade, null);
});

// ---- The drop card -------------------------------------------------------

test('baseline: what the journal held when the page opened raises no card', () => {
  const q = new FlourishQueue();
  q.takeJournal([entry(), entry()], 0, LIFE);
  assert.equal(q.view(0).card, null);
  q.takeJournal(null, 10, LIFE);
  assert.equal(q.view(10).card, null);
});

test('a drop that arrives later with flourish raises its card, for the life it was given', () => {
  const q = new FlourishQueue();
  const old = [entry()];
  q.takeJournal(old, 0, LIFE);
  const fresh = entry({ name: 'Astral Covenant', rarity: 'Angelic', tier: 5, mf: true });
  q.takeJournal([fresh, ...old], 1000, LIFE);
  const { card, state } = q.view(1000);
  assert.equal(card.name, 'Astral Covenant');
  assert.equal(card.cls, 'r-angelic');
  assert.equal(card.meta, 'Angelic · Grade S · Magic Find');
  assert.equal(card.count, 1);
  assert.equal(state, 'enter');
  assert.equal(q.view(1000 + LIFE - LEAVE_MS).state, 'leave');
  assert.equal(q.view(1000 + LIFE).card, null);
  // the same journal read again raises nothing more
  q.takeJournal([fresh, ...old], 1000 + LIFE + 1, LIFE);
  assert.equal(q.view(1000 + LIFE + 1).card, null);
});

test('a new journal entry without flourish raises nothing', () => {
  const q = new FlourishQueue();
  q.takeJournal([], 0, LIFE);
  q.takeJournal([entry({ flourish: false })], 100, LIFE);
  assert.equal(q.view(100).card, null);
});

test('the same drop again within the grouping window counts on the card (x2) and keeps it up', () => {
  const q = new FlourishQueue();
  q.takeJournal([], 0, LIFE);
  const a = entry({ name: 'Soulforged Ring' });
  q.takeJournal([a], 1000, LIFE);
  const b = entry({ name: 'Soulforged Ring' });
  q.takeJournal([b, a], 1000 + GROUP_MS, LIFE);
  const { card } = q.view(1000 + GROUP_MS);
  assert.equal(card.count, 2);
  assert.equal(card.until, 1000 + GROUP_MS + LIFE);
  // outside the window it is a card of its own, after this one
  const c = entry({ name: 'Soulforged Ring' });
  q.takeJournal([c, b, a], 1000 + GROUP_MS * 3, LIFE);
  assert.equal(q.view(1000 + GROUP_MS * 3).card.count, 2);
  assert.equal(q.waiting.length, 1);
});

test('drops that arrive while a card is up wait their turn, oldest first', () => {
  const q = new FlourishQueue();
  q.takeJournal([], 0, LIFE);
  const one = entry({ name: 'One' });
  const two = entry({ name: 'Two' });
  // the journal is newest first; the queue takes the older one first
  q.takeJournal([two, one], 0, LIFE);
  assert.equal(q.view(0).card.name, 'One');
  assert.equal(q.nextAt(0), LIFE - LEAVE_MS);
  assert.equal(q.view(LIFE).card.name, 'Two');
  assert.equal(q.view(LIFE).card.until, LIFE + LIFE);
  assert.equal(q.view(LIFE * 2).card, null);
  assert.equal(q.nextAt(LIFE * 2), null);
});

test('a full queue drops the weakest waiting card for a rarer drop, never the other way round', () => {
  const q = new FlourishQueue();
  q.takeJournal([], 0, LIFE);
  q.takeJournal([entry({ name: 'Shown', rarity: 'Unholy' })], 0, LIFE);
  const waitingNames = ['Set A', 'Heroic B', 'Angelic C'];
  q.offer(dropCard(entry({ name: 'Set A', rarity: 'Set' })), 10, LIFE);
  q.offer(dropCard(entry({ name: 'Heroic B', rarity: 'Heroic' })), 20, LIFE);
  q.offer(dropCard(entry({ name: 'Angelic C', rarity: 'Angelic' })), 30, LIFE);
  assert.equal(q.waiting.length, MAX_WAITING);
  assert.deepEqual(q.waiting.map((c) => c.name), waitingNames);
  q.offer(dropCard(entry({ name: 'Set D', rarity: 'Set' })), 40, LIFE);
  assert.deepEqual(q.waiting.map((c) => c.name), waitingNames, 'an equal or weaker drop stays in the journal only');
  q.offer(dropCard(entry({ name: 'Unholy E', rarity: 'Unholy' })), 50, LIFE);
  assert.deepEqual(q.waiting.map((c) => c.name), ['Heroic B', 'Angelic C', 'Unholy E']);
});

test('a zone rotation present when the page opened raises no card; a later one does, once', () => {
  const buffs = [[6, 'Loot Goblin II', ''], [14, 'Rune Master', ''], [20, 'Third', '']];
  const q = new FlourishQueue();
  q.takeZone({ seq: 1, zone: 'Satanic_1_3', buffs: [6], debuffs: [] }, 0, LIFE, true, buffs);
  assert.equal(q.view(0).card, null);
  const rotation = { seq: 2, zone: 'Satanic_7_3', buffs: [6, 14, 20], debuffs: [3] };
  q.takeZone(rotation, 100, LIFE, true, buffs);
  const { card } = q.view(100);
  assert.equal(card.kind, 'zone');
  assert.equal(card.name, 'Satanic zone: Act 7 · The Black Hole');
  assert.equal(card.meta, 'Loot Goblin II · Rune Master · +1');
  q.takeZone(rotation, 200, LIFE, true, buffs);
  assert.equal(q.waiting.length, 0, 'the same rotation read again is not a new one');
});

test('a page that opens before any rotation still shows the first one', () => {
  const q = new FlourishQueue();
  q.takeZone(null, 0, LIFE, true, []);
  q.takeZone({ seq: 1, zone: 'Satanic_1_3', buffs: [], debuffs: [] }, 50, LIFE, true, []);
  assert.equal(q.view(50).card.meta, 'No buffs this rotation');
});

test('a rotation the player did not ask to see raises nothing, and is not shown later', () => {
  const q = new FlourishQueue();
  q.takeZone(null, 0, LIFE, false, []);
  const rotation = { seq: 1, zone: 'Satanic_1_3', buffs: [], debuffs: [] };
  q.takeZone(rotation, 10, LIFE, false, []);
  assert.equal(q.view(10).card, null);
  q.takeZone(rotation, 20, LIFE, true, []);
  assert.equal(q.view(20).card, null);
});

test('the card is a zone card or a drop card, by what it came from', () => {
  assert.equal(zoneCard({ seq: 4, zone: 'nonsense', buffs: [99] }, []).name, 'Satanic zone: nonsense');
  assert.equal(zoneCard({ seq: 4, zone: 'nonsense', buffs: [99] }, []).meta, 'Buff 99');
  const card = dropCard(entry({ name: 'Plain', rarity: 'Set', tier: 0, mf: false }));
  assert.equal(card.meta, 'Set');
  assert.equal(card.kind, 'drop');
});

test('the card stays up for the drop card setting, 2 to 10 s, 4 s by default', () => {
  assert.equal(cardMs(null), 4000);
  assert.equal(cardMs({ flourish_secs: 6 }), 6000);
  assert.equal(cardMs({ flourish_secs: 0.5 }), 2000);
  assert.equal(cardMs({ flourish_secs: 60 }), 10000);
  assert.equal(cardMs({ flourish_secs: 'x' }), 4000);
});

// ---- The top line --------------------------------------------------------

test('the top line says what the tracker is doing, shortest form', () => {
  assert.deepEqual(overlayState({ on: false }), { text: 'Tracker off', tone: 'off' });
  assert.deepEqual(overlayState({ on: true, session: { live: false } }), { text: 'Waiting for the game', tone: 'warn' });
  assert.deepEqual(overlayState({ on: true, session: { live: true }, catching_up: true }), { text: 'Catching up', tone: 'warn' });
  assert.deepEqual(overlayState({ on: true, session: { live: true }, snapshot: { paused: true } }), { text: 'Clock paused', tone: 'off' });
  assert.deepEqual(overlayState({ on: true, session: { live: true }, snapshot: { paused: false } }), { text: 'Recording', tone: 'on' });
});

// ---- The leaving fade ----------------------------------------------------

test('LEAVE_MS is the leaving fade, --motion-duration-fast in tokens.css', () => {
  // The queue keeps a leaving card this long, so the CSS fade (overlay.css,
  // fast) finishes before the card is removed: the two must not drift apart.
  const css = readFileSync(new URL('../src/tokens.css', import.meta.url), 'utf8');
  const declared = [...css.matchAll(/--motion-duration-fast:\s*([\d.]+)(ms|s)\s*;/g)];
  assert.ok(declared.length > 0, 'tokens.css declares no --motion-duration-fast');
  for (const [, value, unit] of declared) assert.equal(Number(value) * (unit === 's' ? 1000 : 1), LEAVE_MS);
});
