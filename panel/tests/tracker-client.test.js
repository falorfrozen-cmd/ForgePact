// The Tracker tab's client (src/tracker/client.svelte.js, #185), its pure
// parts under plain node: when the page polls `/api/tracker`, when a poll
// fetches `/extra` and `/runs`, the read-modify-write of the settings
// document, and the switch card's state line. The browser half (that the page
// really sends nothing with the switch off and another tab open) is
// tests/tracker/shell.check.mjs.
//
//   node --test tests/tracker-client.test.js

import { test } from 'node:test';
import assert from 'node:assert/strict';
import {
  POLL_MS, fetchPlan, mergeSettings, pollWait, readModifyWrite, shouldPoll, stateLine,
} from '../src/tracker/client.svelte.js';
import { clock, fmt, rate, span, difficulty, RARITIES, RARITY_CLASS } from '../src/tracker/format.js';

test('baseline: with the switch off, the tab closed and no overlay, nothing polls', () => {
  assert.equal(shouldPoll({ on: false, tabOpen: false, overlay: false }), false);
  assert.equal(shouldPoll({}), false);
  // An unread setting is not on.
  assert.equal(shouldPoll({ on: undefined, tabOpen: false }), false);
});

test('target: the switch on, the tab open or the overlay page each poll on their own', () => {
  assert.equal(shouldPoll({ on: true, tabOpen: false, overlay: false }), true);
  assert.equal(shouldPoll({ on: false, tabOpen: true, overlay: false }), true);
  assert.equal(shouldPoll({ on: false, tabOpen: false, overlay: true }), true);
});

test('at most once a second: the wait before the next poll', () => {
  assert.equal(POLL_MS, 1000);
  assert.equal(pollWait(null, 5000), 0, 'the first poll goes at once');
  assert.equal(pollWait(5000, 5000), 1000);
  assert.equal(pollWait(5000, 5400), 600);
  assert.equal(pollWait(5000, 6000), 0);
  assert.equal(pollWait(5000, 9000), 0);
});

test('/extra is fetched when extra_revision changes, and only then', () => {
  const seen = { revision: 4, extraRevision: 2, runsRevision: 4 };
  assert.deepEqual(fetchPlan(seen, { revision: 4, extra_revision: 2 }, false), { extra: false, runs: false });
  assert.deepEqual(fetchPlan(seen, { revision: 5, extra_revision: 2 }, false), { extra: false, runs: false });
  assert.deepEqual(fetchPlan(seen, { revision: 5, extra_revision: 3 }, false), { extra: true, runs: false });
  // Nothing fetched yet: the first answer fetches it.
  assert.deepEqual(fetchPlan({ revision: null, extraRevision: null, runsRevision: null }, { revision: 0, extra_revision: 0 }, false),
    { extra: true, runs: false });
});

test('/runs is fetched when revision changes while the Runs section is open', () => {
  const seen = { revision: 4, extraRevision: 2, runsRevision: 4 };
  assert.equal(fetchPlan(seen, { revision: 4, extra_revision: 2 }, true).runs, false);
  assert.equal(fetchPlan(seen, { revision: 5, extra_revision: 2 }, true).runs, true);
  // Closed, a new revision only leaves the runs stale; opening the section fetches them.
  assert.equal(fetchPlan(seen, { revision: 5, extra_revision: 2 }, false).runs, false);
  assert.equal(fetchPlan({ ...seen, runsRevision: null }, { revision: 4, extra_revision: 2 }, true).runs, true);
});

test('mergeSettings changes only what it is given and never the document it read', () => {
  const doc = { alerts: { satanic: { on: true, volume: 0.8 }, set: { on: false, volume: 0.5 } }, lists: [{ id: 'a' }], unknown_field: 7 };
  const frozen = JSON.parse(JSON.stringify(doc));
  const next = mergeSettings(doc, { alerts: { satanic: { volume: 0.3 } }, lists: [] });
  assert.deepEqual(doc, frozen, 'the read document was changed');
  assert.deepEqual(next, { alerts: { satanic: { on: true, volume: 0.3 }, set: { on: false, volume: 0.5 } }, lists: [], unknown_field: 7 });
  // A function gets a copy and may return it or a new document.
  const byFn = mergeSettings(doc, (d) => { d.alerts.set.on = true; return d; });
  assert.equal(byFn.alerts.set.on, true);
  assert.equal(doc.alerts.set.on, false);
  assert.deepEqual(mergeSettings(doc, (d) => { d.unknown_field = 8; }).unknown_field, 8);
});

test('settings are read-modify-write: GET the document, change it, POST the whole of it', async () => {
  const calls = [];
  const server = { settings: { a: 1, nested: { b: 2, c: 3 }, kept: 'x' } };
  const fetchJson = async (path, init) => {
    calls.push([init?.method || 'GET', path, init?.body === undefined ? undefined : JSON.parse(init.body)]);
    if (!init?.method) return { settings: server.settings };
    server.settings = JSON.parse(init.body);
    return { ok: true, settings: server.settings };
  };
  const res = await readModifyWrite(fetchJson, { nested: { c: 9 } });
  assert.deepEqual(calls, [
    ['GET', '/api/tracker/settings', undefined],
    ['POST', '/api/tracker/settings', { a: 1, nested: { b: 2, c: 9 }, kept: 'x' }],
  ]);
  assert.deepEqual(res.settings, { a: 1, nested: { b: 2, c: 9 }, kept: 'x' });
  // A refused read posts nothing.
  calls.length = 0;
  const refused = await readModifyWrite(async (path, init) => { calls.push(path); return { err: 'no' }; }, { a: 2 });
  assert.equal(refused.err, 'no');
  assert.deepEqual(calls, ['/api/tracker/settings']);
});

test('the state line: off, on, recording, catching up, and the character the API names', () => {
  const snap = (name) => ({ character: name ? { name } : null, paused: false });
  assert.deepEqual(stateLine({ on: false }), { text: 'Off', tone: 'off' });
  assert.match(stateLine({ on: false, snapshot: snap('Sorak') }).text, /Sorak/);
  assert.equal(stateLine({ on: false, snapshot: snap('Sorak') }).tone, 'off');
  assert.deepEqual(stateLine({ on: true, snapshot: snap(null), session: { live: false } }), { text: 'On · waiting for the game', tone: 'off' });
  assert.deepEqual(stateLine({ on: true, snapshot: snap('Sorak'), session: { live: true } }), { text: 'Recording Sorak’s session', tone: 'on' });
  assert.deepEqual(stateLine({ on: true, snapshot: snap(null), session: { live: true } }), { text: 'Recording this session', tone: 'on' });
  assert.equal(stateLine({ on: true, snapshot: snap('Sorak'), session: { live: true }, catching_up: true }).tone, 'warn');
  assert.match(stateLine({ on: true, snapshot: snap('Sorak'), session: { live: true }, catching_up: true }).text, /^Catching up/);
  assert.match(stateLine({ on: true, snapshot: { ...snap('Sorak'), paused: true }, session: { live: true } }).text, /^Clock paused/);
  // A name never appears that the API did not give.
  assert.ok(!stateLine({ on: true, snapshot: snap(null), session: { live: false } }).text.includes('undefined'));
});

test('format: the game\'s dialect, ported from HS Offline Tracker', () => {
  assert.equal(fmt(1234), '1,234');
  assert.equal(fmt(12345), '12.3k');
  assert.equal(fmt(1234567), '1.23kk');
  assert.equal(fmt(1234567890), '1.23kkk');
  assert.equal(fmt(null), '0');
  assert.equal(rate(14570000), '14.57kk/h');
  assert.equal(clock(3661), '1:01:01');
  assert.equal(span(3599), '1h');
  assert.equal(span(5040), '1h 24m');
  assert.equal(difficulty(2, 5), 'Hell 5');
  assert.equal(difficulty(7), 'D7');
  assert.deepEqual(RARITIES.map((r) => RARITY_CLASS[r]), ['r-satanic', 'r-set', 'r-heroic', 'r-angelic', 'r-unholy']);
});
