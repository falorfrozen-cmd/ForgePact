// The adaptive poll policy (src/poll-policy.js): its delay tiers and its
// change clock. tests/test_panel_performance.py runs the same truth tables
// from Python and holds POLL_WATCHED_FIELDS equal to forgepact.py's copy.
import test from 'node:test';
import assert from 'node:assert/strict';
import {
  POLL_FAST_MS, POLL_IDLE_MS, POLL_FAST_WINDOW_MS, POLL_WATCHED_FIELDS,
  pollDelayMs, pollFieldValue, pollPayloadChanged, pollNextChangeAt,
} from '../src/poll-policy.js';

test('a hidden window schedules no poll at all', () => {
  assert.equal(pollDelayMs(true, 0), null);
  assert.equal(pollDelayMs(true, 999999), null);
});

test('fast inside the window after a change, idle from its boundary on', () => {
  assert.equal(pollDelayMs(false, 0), POLL_FAST_MS);
  assert.equal(pollDelayMs(false, POLL_FAST_WINDOW_MS - 1), POLL_FAST_MS);
  assert.equal(pollDelayMs(false, POLL_FAST_WINDOW_MS), POLL_IDLE_MS);
  assert.equal(pollDelayMs(false, 999999), POLL_IDLE_MS);
});

test('the watched fields are the four the status chips read', () => {
  assert.deepEqual(POLL_WATCHED_FIELDS, ['gameRunning', 'ipcOk', 'lastApplied', 'queued']);
});

test('dotted field paths read nested values and survive missing parents', () => {
  assert.equal(pollFieldValue({ game: { build: 7 } }, 'game.build'), 7);
  assert.equal(pollFieldValue({ game: null }, 'game.build'), undefined);
  assert.equal(pollFieldValue(undefined, 'queued'), undefined);
});

test('only a watched difference, a local action or the first payload resets the clock', () => {
  const a = { gameRunning: false, ipcOk: true, lastApplied: '12:00:00', queued: false, chain: { plugin: true } };
  const same = structuredClone(a);
  const unwatched = structuredClone(a);
  unwatched.chain.plugin = false;
  const watched = structuredClone(a);
  watched.queued = true;
  assert.equal(pollPayloadChanged(a, same), false);
  assert.equal(pollNextChangeAt(a, same, false, 9000, 100), 100);
  assert.equal(pollNextChangeAt(a, unwatched, false, 9000, 100), 100);
  assert.equal(pollNextChangeAt(a, watched, false, 9000, 100), 9000);
  assert.equal(pollNextChangeAt(a, same, true, 9000, 100), 9000);
  assert.equal(pollNextChangeAt(null, a, false, 9000, 100), 9000);
});
