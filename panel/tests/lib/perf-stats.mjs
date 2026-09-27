// The pure half of the panel's responsiveness suite (tests/perf.e2e.mjs):
// the statistics, the frame counts, the trace attribution, the two budgets
// and the source fingerprint. Nothing here touches a browser, so
// tests/perf-budget.test.js runs all of it under plain node.
//
// The numbers it works on are measured in headless Edge, which has no vsync:
// frames arrive about every 5 ms, so a rAF delta over 16.7 ms is a slow frame,
// not jitter, and one over 33.4 ms is at least one frame a 60 Hz display would
// drop. The budgets are the owner's (forgepact-ui-responsive, 2026-09-25).

import { createHash } from 'node:crypto';
import { readdirSync, readFileSync } from 'node:fs';
import { join } from 'node:path';

// What "over budget" means when choosing what needs a root cause.
export const TARGET = {
  longestFrameMs: 33.4,
  framesOver50: 0,
  inpP95Ms: 50,
  resultPaintP95Ms: 100,
  mutations: 0,
};

// What the suite asserts, so CI noise cannot fail it: the owner's ceilings.
export const HARD = {
  longestFrameMs: 50,
  framesOver50: 0,
  inpP95Ms: 100,
  resultPaintP95Ms: 100,
  mutations: 0,
};

// An input that produced no Event Timing entry was faster than the API's
// threshold, so the threshold is an upper bound for it.
export const EVENT_TIMING_THRESHOLD_MS = 16;
export const SLOW_FRAME_MS = 16.7;
export const DROPPED_FRAME_MS = 50;

// The median of the numbers in `values`, nulls and non-numbers skipped; null
// when nothing is left.
export function median(values) {
  const xs = values.filter((v) => typeof v === 'number' && Number.isFinite(v)).sort((a, b) => a - b);
  if (!xs.length) return null;
  const mid = xs.length >> 1;
  return xs.length % 2 ? xs[mid] : (xs[mid - 1] + xs[mid]) / 2;
}

// The p-th percentile by nearest rank: the smallest sample with at least p%
// of the samples at or below it. p95 of 3 samples is the largest of them.
export function nearestRank(values, p) {
  const xs = values.filter((v) => typeof v === 'number' && Number.isFinite(v)).sort((a, b) => a - b);
  if (!xs.length) return null;
  const rank = Math.max(1, Math.ceil((p / 100) * xs.length));
  return xs[rank - 1];
}

export const p95 = (values) => nearestRank(values, 95);

// rAF deltas in, the frame metrics out.
export function frameStats(deltas) {
  const xs = deltas.filter((d) => typeof d === 'number' && Number.isFinite(d));
  return {
    longestFrameMs: xs.length ? round(Math.max(...xs)) : null,
    framesOver16_7: xs.filter((d) => d > SLOW_FRAME_MS).length,
    framesOver50: xs.filter((d) => d > DROPPED_FRAME_MS).length,
  };
}

// One value per input sent: the longest Event Timing duration among the
// interaction ids seen in its window, or the threshold when it had none.
export function inputDelay(durationsById) {
  const values = Object.values(durationsById || {});
  return values.length ? Math.max(...values) : EVENT_TIMING_THRESHOLD_MS;
}

// ---- trace attribution ----------------------------------------------------
// The renderer main thread's time, by the trace event names this Edge writes
// (browser.startTracing with devtools.timeline and
// disabled-by-default-devtools.timeline).
export const TRACE_GROUPS = {
  styleMs: ['UpdateLayoutTree'],
  layoutMs: ['Layout'],
  paintMs: ['PrePaint', 'Paint'],
  compositeMs: ['Layerize', 'Commit', 'UpdateLayer'],
  scriptMs: ['FunctionCall', 'EventDispatch', 'FireAnimationFrame'],
};
// Also counted, for the root causes: the task envelope and the collector.
export const TRACE_EXTRA = { taskMs: ['RunTask'], gcMs: ['MinorGC', 'MajorGC', 'V8.GC_SCAVENGER', 'BlinkGC.AtomicPhase'] };

// The main thread of the page's renderer: the CrRendererMain thread with the
// most timeline events (a spare renderer has a main thread too, and does
// nothing).
export function rendererMain(events) {
  const mains = events.filter((e) => e.ph === 'M' && e.name === 'thread_name' && e.args?.name === 'CrRendererMain');
  let best = null;
  let bestCount = -1;
  for (const m of mains) {
    const count = events.filter((e) => e.pid === m.pid && e.tid === m.tid && e.ph === 'X').length;
    if (count > bestCount) { best = { pid: m.pid, tid: m.tid }; bestCount = count; }
  }
  return best;
}

// Complete events ('X', with `dur`) and begin/end pairs ('B'/'E') as
// [start, end] intervals in microseconds, per name.
function intervalsByName(events, thread) {
  const out = new Map();
  const open = new Map();
  const add = (name, a, b) => { if (!out.has(name)) out.set(name, []); out.get(name).push([a, b]); };
  for (const e of events) {
    if (thread && (e.pid !== thread.pid || e.tid !== thread.tid)) continue;
    if (e.ph === 'X' && typeof e.dur === 'number') add(e.name, e.ts, e.ts + e.dur);
    else if (e.ph === 'B') { if (!open.has(e.name)) open.set(e.name, []); open.get(e.name).push(e.ts); }
    else if (e.ph === 'E') { const s = open.get(e.name)?.pop(); if (s !== undefined) add(e.name, s, e.ts); }
  }
  return out;
}

// The time covered by a set of intervals, overlaps counted once: an
// EventDispatch holds the FunctionCall of its listener, and summing both
// would count that script twice.
export function coveredUs(intervals, windows = null) {
  let xs = intervals;
  if (windows) {
    xs = [];
    for (const [a, b] of intervals) for (const [wa, wb] of windows) {
      const s = Math.max(a, wa), e = Math.min(b, wb);
      if (e > s) xs.push([s, e]);
    }
  }
  xs = [...xs].sort((p, q) => p[0] - q[0]);
  let total = 0;
  let cur = null;
  for (const [a, b] of xs) {
    if (!cur || a > cur[1]) { if (cur) total += cur[1] - cur[0]; cur = [a, b]; } else cur[1] = Math.max(cur[1], b);
  }
  if (cur) total += cur[1] - cur[0];
  return total;
}

// The measured windows inside a trace: the page marks `perf-begin` and
// `perf-end` (performance.mark) around each sample, and blink.user_timing
// writes them with trace timestamps.
export function markWindows(events) {
  const marks = events.filter((e) => e.name === 'perf-begin' || e.name === 'perf-end').sort((a, b) => a.ts - b.ts);
  const windows = [];
  let start = null;
  for (const m of marks) {
    if (m.name === 'perf-begin') start = m.ts;
    else if (start !== null) { windows.push([start, m.ts]); start = null; }
  }
  return windows;
}

// A trace's renderer-main-thread time per group, in ms, over the marked
// windows when there are any (else the whole trace), divided by `samples`.
export function traceBreakdown(traceJson, samples = 1) {
  const events = Array.isArray(traceJson) ? traceJson : traceJson?.traceEvents || [];
  const thread = rendererMain(events);
  const windows = markWindows(events);
  const byName = intervalsByName(events, thread);
  const out = {};
  for (const [key, names] of Object.entries({ ...TRACE_GROUPS, ...TRACE_EXTRA })) {
    const intervals = names.flatMap((n) => byName.get(n) || []);
    out[key] = round(coveredUs(intervals, windows.length ? windows : null) / 1000 / Math.max(1, samples));
  }
  out.windows = windows.length;
  return out;
}

// The longest main-thread tasks in a trace's windows, for root causes.
export function longestTasks(traceJson, n = 5) {
  const events = Array.isArray(traceJson) ? traceJson : traceJson?.traceEvents || [];
  const thread = rendererMain(events);
  const windows = markWindows(events);
  const inWindow = (e) => !windows.length || windows.some(([a, b]) => e.ts >= a && e.ts <= b);
  return events.filter((e) => thread && e.pid === thread.pid && e.tid === thread.tid && e.ph === 'X' && e.name === 'RunTask' && inWindow(e))
    .sort((a, b) => b.dur - a.dur).slice(0, n).map((e) => round(e.dur / 1000));
}

// ---- budgets ----------------------------------------------------------------
// What `m` (an interaction's medians) breaks of `limits`, as a list of names;
// empty is within. A metric that is null does not apply and cannot fail.
export function overBudget(name, m, limits) {
  const out = [];
  if (m.longestFrameMs !== null && m.longestFrameMs > limits.longestFrameMs) out.push('frame');
  if (m.framesOver50 !== null && m.framesOver50 > limits.framesOver50) out.push('over50');
  if (m.inpP95Ms !== null && m.inpP95Ms !== undefined && m.inpP95Ms > limits.inpP95Ms) out.push('inp');
  if (m.resultPaintP95Ms !== null && m.resultPaintP95Ms !== undefined && m.resultPaintP95Ms > limits.resultPaintP95Ms) out.push('result');
  if (name === 'idle-poll' && ((m.mutationsPage ?? 0) > limits.mutations || (m.mutationsEnabledMods ?? 0) > limits.mutations)) out.push('mutations');
  return out;
}

export const overTarget = (name, m) => overBudget(name, m, TARGET);
export const overHard = (name, m) => overBudget(name, m, HARD);

// The medians of every numeric key across runs, each key on its own.
export function medianOfRuns(runs, keys) {
  const out = {};
  for (const k of keys) out[k] = median(runs.map((r) => r[k]));
  return out;
}

// ---- the source fingerprint -------------------------------------------------
// sha256 over every file under panel/src in sorted relative-path order, `/`
// separators: each file adds its path (UTF-8), a NUL, its bytes with every CR
// removed, and a NUL. The criteria recompute it from git, so a results file
// says which source it measured.
export function digestFiles(files) {
  const h = createHash('sha256');
  for (const [path, bytes] of [...files].sort((a, b) => (a[0] < b[0] ? -1 : a[0] > b[0] ? 1 : 0))) {
    h.update(Buffer.from(path, 'utf8'));
    h.update(Buffer.from([0]));
    h.update(Buffer.from(bytes).filter((b) => b !== 13));
    h.update(Buffer.from([0]));
  }
  return h.digest('hex');
}

export function panelSrcDigest(srcDir) {
  const files = [];
  const walk = (dir, rel) => {
    for (const entry of readdirSync(dir, { withFileTypes: true })) {
      const path = rel ? `${rel}/${entry.name}` : entry.name;
      if (entry.isDirectory()) walk(join(dir, entry.name), path);
      else if (entry.isFile()) files.push([path, readFileSync(join(dir, entry.name))]);
    }
  };
  walk(srcDir, '');
  return digestFiles(files);
}

export function round(x) {
  return typeof x === 'number' && Number.isFinite(x) ? Math.round(x * 10) / 10 : x;
}
