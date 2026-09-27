// tests/lib/perf-stats.mjs: the statistics, frame counts, trace attribution,
// budgets and source fingerprint behind `npm run e2e:perf`. Pure, so it runs
// here without a browser; tests/perf.e2e.mjs does the measuring, and its two
// controls prove the samplers can see a slow frame and a mutation at all.
import test from 'node:test';
import assert from 'node:assert/strict';
import { createHash } from 'node:crypto';
import { mkdtempSync, mkdirSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import {
  HARD, TARGET, coveredUs, digestFiles, frameStats, inputDelay, markWindows, median, medianOfRuns,
  nearestRank, overHard, overTarget, p95, panelSrcDigest, rendererMain, traceBreakdown,
} from './lib/perf-stats.mjs';

const TRACE = JSON.parse(readFileSync(new URL('./fixtures/perf/trace-mini.json', import.meta.url), 'utf8'));

// An interaction's medians, all within both budgets, to vary one key at a time.
const quiet = { longestFrameMs: 6, framesOver50: 0, inpP95Ms: 16, resultPaintP95Ms: 8 };

test('median: the middle value, the mean of the middle two, nulls skipped, null for nothing', () => {
  assert.equal(median([5, 1, 3]), 3);
  assert.equal(median([4, 1, 3, 2]), 2.5);
  assert.equal(median([null, 7, undefined, 1, NaN, 4]), 4);
  assert.equal(median([]), null);
  assert.equal(median([null, null]), null);
});

test('p95 is the nearest rank: the largest of three samples, the 19th of twenty', () => {
  assert.equal(p95([16, 88, 24]), 88);
  assert.equal(p95(Array.from({ length: 20 }, (_, i) => i + 1)), 19);
  assert.equal(nearestRank([10, 20, 30, 40], 50), 20);
  assert.equal(p95([null, 40]), 40);
  assert.equal(p95([]), null);
});

test('frame counts: over 16.7 and over 50 are strict, and the longest delta is kept', () => {
  assert.deepEqual(frameStats([5, 16.7, 17, 50, 50.1, 4.9]), { longestFrameMs: 50.1, framesOver16_7: 3, framesOver50: 1 });
  assert.deepEqual(frameStats([]), { longestFrameMs: null, framesOver16_7: 0, framesOver50: 0 });
});

test('an input with no Event Timing entry reads as the 16 ms threshold; else the longest id', () => {
  assert.equal(inputDelay({}), 16);
  assert.equal(inputDelay(undefined), 16);
  assert.equal(inputDelay({ 7: 24, 8: 88 }), 88);
});

test('the renderer main thread is the busy CrRendererMain, not a spare renderer\'s', () => {
  assert.deepEqual(rendererMain(TRACE.traceEvents), { pid: 1, tid: 10 });
});

test('trace attribution sums UpdateLayoutTree, Layout, paint, composite and script over the marked windows', () => {
  assert.deepEqual(markWindows(TRACE.traceEvents), [[10000, 20000], [50000, 60000]]);
  const one = traceBreakdown(TRACE, 1);
  // Style: 1000 us in the first window plus the 2000 us of the last event
  // that fall before the second window closes; the event at 30000 is outside.
  assert.equal(one.styleMs, 3);
  // Layout: 1500 + the 1000 us inside the second window; the compositor's and
  // the spare renderer's Layout never count.
  assert.equal(one.layoutMs, 2.5);
  // PrePaint + Paint, and a B/E Paint pair.
  assert.equal(one.paintMs, 2);
  assert.equal(one.compositeMs, 1);
  // EventDispatch holds its FunctionCall: counted once. Plus FireAnimationFrame.
  assert.equal(one.scriptMs, 7);
  assert.equal(one.taskMs, 9);
  assert.equal(one.windows, 2);
  // Per sample: divided by the samples measured.
  assert.equal(traceBreakdown(TRACE, 2).scriptMs, 3.5);
});

test('with no marks the whole trace counts, and overlapping intervals count once', () => {
  const unmarked = { traceEvents: TRACE.traceEvents.filter((e) => e.ph !== 'R') };
  assert.equal(traceBreakdown(unmarked).styleMs, 10);
  assert.equal(coveredUs([[0, 10], [5, 15], [20, 25]]), 20);
  assert.equal(coveredUs([[0, 10]], [[5, 8]]), 3);
});

test('the target is the owner\'s 33.4 ms frame and 50 ms input; the hard budget their 50 ms and 100 ms ceilings', () => {
  assert.deepEqual(TARGET, { longestFrameMs: 33.4, framesOver50: 0, inpP95Ms: 50, resultPaintP95Ms: 100, mutations: 0 });
  assert.deepEqual(HARD, { longestFrameMs: 50, framesOver50: 0, inpP95Ms: 100, resultPaintP95Ms: 100, mutations: 0 });
  assert.deepEqual(overTarget('tray-open', quiet), []);
  assert.deepEqual(overHard('tray-open', quiet), []);
});

test('a 40 ms frame or a 60 ms input is over the target but within the hard budget', () => {
  assert.deepEqual(overTarget('tray-open', { ...quiet, longestFrameMs: 40 }), ['frame']);
  assert.deepEqual(overHard('tray-open', { ...quiet, longestFrameMs: 40 }), []);
  assert.deepEqual(overTarget('tab-switch-world', { ...quiet, inpP95Ms: 60 }), ['inp']);
  assert.deepEqual(overHard('tab-switch-world', { ...quiet, inpP95Ms: 60 }), []);
});

test('the jank control\'s 80 ms frame fails the hard budget; so do a frame over 50 ms and a late result', () => {
  assert.deepEqual(overHard('perf-instrument-catches-jank', { longestFrameMs: 80.6, framesOver50: 1, inpP95Ms: 88, resultPaintP95Ms: 90 }), ['frame', 'over50']);
  assert.deepEqual(overHard('undo', { ...quiet, framesOver50: 1 }), ['over50']);
  assert.deepEqual(overHard('toggle-mod-tray', { ...quiet, resultPaintP95Ms: 140 }), ['result']);
});

test('a metric that does not apply is null and fails nothing; idle mutations fail both budgets', () => {
  const hover = { longestFrameMs: 6, framesOver50: 0, inpP95Ms: null, resultPaintP95Ms: null };
  assert.deepEqual(overTarget('note-hover-loot', hover), []);
  assert.deepEqual(overHard('note-hover-loot', hover), []);
  const idle = { ...hover, mutationsPage: 42, mutationsEnabledMods: 0 };
  assert.deepEqual(overTarget('idle-poll', idle), ['mutations']);
  assert.deepEqual(overHard('idle-poll', idle), ['mutations']);
  assert.deepEqual(overHard('idle-poll', { ...idle, mutationsPage: 0 }), []);
  // Only idle-poll is judged on mutations: a toggle mutates by design.
  assert.deepEqual(overHard('toggle-mod-inline', { ...quiet, mutationsPage: 345 }), []);
});

test('medians across runs are taken for each key on its own', () => {
  const runs = [{ a: 1, b: 30 }, { a: 9, b: 10 }, { a: 5, b: 20 }];
  assert.deepEqual(medianOfRuns(runs, ['a', 'b']), { a: 5, b: 20 });
});

test('panelSrcDigest: sorted paths with / separators, path NUL bytes NUL, every CR removed', () => {
  const lf = [['main.js', Buffer.from('a\nb\n')], ['lib/x.js', Buffer.from('x')]];
  const crlf = [['lib/x.js', Buffer.from('x')], ['main.js', Buffer.from('a\r\nb\r\n')]];
  assert.equal(digestFiles(lf), digestFiles(crlf));
  // The format spelled out: what the criteria recompute from git.
  const h = createHash('sha256');
  h.update(Buffer.from('lib/x.js\0x\0main.js\0a\nb\n\0', 'utf8'));
  assert.equal(digestFiles(lf), h.digest('hex'));
  assert.notEqual(digestFiles(lf), digestFiles([['main.js', Buffer.from('a\nb\n')], ['lib/y.js', Buffer.from('x')]]));

  const dir = mkdtempSync(join(tmpdir(), 'perf-digest-'));
  try {
    mkdirSync(join(dir, 'lib'));
    writeFileSync(join(dir, 'main.js'), 'a\r\nb\r\n');
    writeFileSync(join(dir, 'lib', 'x.js'), 'x');
    assert.equal(panelSrcDigest(dir), digestFiles(lf));
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});
