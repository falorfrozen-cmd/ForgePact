// The page-side half of the responsiveness suite (tests/perf.e2e.mjs): the
// samplers that run inside the panel, and the node-side call that measures one
// sample with them. The statistics are in perf-stats.mjs.
//
// One sample is one input and what follows it, inside a window that opens
// before the input and closes once the page has been still (no DOM mutation)
// for `stillMs`, at least `minMs` after the input, and after the result and the
// `until` condition when there are any; or at the cap. Inside the window:
//   - frames: a requestAnimationFrame loop records every delta that ends at
//     or after the input (a drag's input is its press, so its moves count);
//   - input-to-next-paint: the Event Timing entries (durationThreshold 16) of
//     the interaction ids that start inside the window, max per id;
//   - result paint: from an input event's own timeStamp (the press, or the
//     release with `resultFrom`) to the first rAF after a predicate on the DOM
//     first holds (the entry listed, data-open, data-form, the tab shown, the
//     rows filtered, the entry back);
//   - settle: from the last release (pointerup, keyup, resize) to the moment
//     the `until` predicate first holds (the form scenarios' data-form change);
//   - mutations: every MutationObserver record under <html>, and under
//     #enabledMods on its own.
// The window is marked with performance.mark('perf-begin'/'perf-end') so a
// trace taken around it (blink.user_timing) can be cut to the same span.
// Nothing here posts, stores or changes anything in the page.

import { inputDelay, frameStats, round } from './perf-stats.mjs';

export const STILL_MS = 300;
export const CAP_MS = 4000;

// Runs in the page (addInitScript, and page.evaluate on a page opened
// without it): installs window.__perf once.
export function installSampler() {
  if (window.__perf) return;
  try { performance.setResourceTimingBufferSize(10000); } catch (e) { /* keep the default */ }
  const events = [];
  try {
    new PerformanceObserver((list) => {
      for (const e of list.getEntries()) if (e.interactionId) events.push({ id: e.interactionId, start: e.startTime, dur: e.duration, name: e.name });
    }).observe({ type: 'event', durationThreshold: 16, buffered: true });
  } catch (e) { /* no Event Timing: every input reads as the threshold */ }

  // One MutationObserver from the moment <html> exists, counting records under
  // the whole page and under #enabledMods (looked up per batch: it is mounted
  // later), and, while asked to, how long the Turn off placeholder is present.
  const counts = { page: 0, enabledMods: 0, last: performance.now() };
  const ghost = { track: false, since: null, spans: [] };
  let listener = null;
  const observer = new MutationObserver((records) => {
    counts.page += records.length;
    const box = document.getElementById('enabledMods');
    if (box) for (const r of records) if (box.contains(r.target) || r.target === box) counts.enabledMods++;
    counts.last = performance.now();
    if (ghost.track) {
      const present = !!document.querySelector('.enabled-mod-ghost');
      if (present && ghost.since === null) ghost.since = counts.last;
      else if (!present && ghost.since !== null) { ghost.spans.push(counts.last - ghost.since); ghost.since = null; }
    }
    if (listener) listener();
  });
  const attach = () => observer.observe(document.documentElement, { subtree: true, childList: true, attributes: true, characterData: true });
  if (document.documentElement) attach(); else document.addEventListener('readystatechange', attach, { once: true });

  let win = null;
  const compile = (src) => (src ? (0, eval)('(' + src + ')') : null);

  window.__perf = {
    counts,
    ghost,
    // Open a window. opts: result, until (predicate sources, called with the
    // window), inputEvents (the event types that are the input: frames and,
    // by default, result paint start at the first of them), resultFrom (the
    // type whose last timeStamp starts result paint instead), noInput (idle and
    // boot: the window's own start is the input), stillMs, minMs, capMs.
    begin(opts = {}) {
      const now = performance.now();
      const inputEvents = opts.inputEvents || ['pointerdown', 'keydown'];
      const releaseEvents = ['pointerup', 'keyup', 'resize'];
      const w = {
        t0: now,
        frames: [],
        lastRaf: null,
        inputTs: opts.noInput ? now : null,
        lastOf: {},
        inputEvents,
        types: [...new Set([...inputEvents, ...releaseEvents, ...(opts.resultFrom ? [opts.resultFrom] : [])])],
        resultFrom: opts.resultFrom || null,
        result: compile(opts.result),
        until: compile(opts.until),
        resultAt: null,
        resultOrigin: null,
        untilAt: null,
        stillMs: opts.stillMs ?? 300,
        minMs: opts.minMs ?? 0,
        capMs: opts.capMs ?? 4000,
        pageAt: counts.page,
        modsAt: counts.enabledMods,
        done: null,
        finished: null,
      };
      win = w;
      counts.last = now;
      const check = () => {
        if (w !== win || w.inputTs === null || w.finished) return;
        if (w.result && w.resultOrigin === null) {
          const origin = w.resultFrom ? w.lastOf[w.resultFrom] : w.inputTs;
          let ok = false;
          if (origin !== undefined) { try { ok = !!w.result(w); } catch (e) { ok = false; } }
          if (ok) {
            w.resultOrigin = origin;
            requestAnimationFrame((ts) => { if (w.resultAt === null) w.resultAt = ts; });
          }
        }
        if (w.until && w.untilAt === null) {
          let ok = false;
          try { ok = !!w.until(w); } catch (e) { ok = false; }
          if (ok) w.untilAt = performance.now();
        }
      };
      w.onEvent = (e) => {
        if (w !== win) return;
        w.lastOf[e.type] = e.timeStamp;
        if (w.inputTs === null && w.inputEvents.includes(e.type)) w.inputTs = e.timeStamp;
        check();
      };
      for (const t of w.types) window.addEventListener(t, w.onEvent, true);
      performance.mark('perf-begin');
      listener = check;
      const finish = (why) => {
        w.finished = why;
        performance.mark('perf-end');
        for (const t of w.types) window.removeEventListener(t, w.onEvent, true);
        if (listener === check) listener = null;
        if (w.done) w.done();
      };
      const tick = (ts) => {
        if (w !== win || w.finished) return;
        if (w.lastRaf !== null) w.frames.push([ts, ts - w.lastRaf]);
        w.lastRaf = ts;
        check();
        const t = performance.now();
        const waiting = (w.result && w.resultAt === null) || (w.until && w.untilAt === null);
        const still = w.inputTs !== null && !waiting && t - counts.last >= w.stillMs &&
          t - w.inputTs >= Math.max(w.stillMs, w.minMs);
        if (still || t - w.t0 > w.capMs) finish(still ? 'still' : 'cap');
        else requestAnimationFrame(tick);
      };
      requestAnimationFrame(tick);
      return true;
    },
    // Wait for the open window to close, then report it.
    async settle() {
      const w = win;
      if (!w) return null;
      if (!w.finished) await new Promise((r) => { w.done = r; });
      // Event Timing entries are delivered after the frame is presented:
      // give the observer one more frame.
      await new Promise((r) => requestAnimationFrame(() => setTimeout(r, 0)));
      const ids = {};
      for (const e of events) if (e.start >= w.t0 - 1) ids[e.id] = Math.max(ids[e.id] || 0, e.dur);
      const from = w.inputTs ?? Infinity;
      const release = Math.max(-Infinity, ...['pointerup', 'keyup', 'resize'].map((k) => w.lastOf[k] ?? -Infinity));
      if (win === w) win = null;
      return {
        deltas: w.frames.filter(([ts]) => ts >= from).map(([, d]) => d),
        inputSeen: w.inputTs !== null,
        resultPaintMs: w.resultAt !== null ? w.resultAt - w.resultOrigin : null,
        untilMs: w.untilAt !== null && w.inputTs !== null ? w.untilAt - w.inputTs : null,
        settleMs: w.untilAt !== null && Number.isFinite(release) ? w.untilAt - release : null,
        ids,
        mutationsPage: counts.page - w.pageAt,
        mutationsEnabledMods: counts.enabledMods - w.modsAt,
        ended: w.finished,
        spanMs: performance.now() - w.t0,
      };
    },
  };
}

// Measure one sample: open the window, run `input` (a node-side async
// function driving the page), wait for the window to close. Answers the frame
// metrics, the input delay (when `inp` is set), the result paint and the raw
// record. A result that never showed reads as the window's whole span and is
// flagged, so it fails a budget instead of disappearing.
export async function measureSample(page, spec, cdp = null) {
  const { input, result = null, until = null, inputEvents, resultFrom = null, noInput = false, inp = true,
    stillMs = STILL_MS, minMs = 0, capMs = CAP_MS } = spec;
  const m0 = cdp ? await metrics(cdp) : null;
  await page.evaluate((o) => window.__perf.begin(o), {
    result: result ? String(result) : null,
    until: until ? String(until) : null,
    inputEvents, resultFrom, noInput, stillMs, minMs, capMs,
  });
  if (input) await input();
  const r = await page.evaluate(() => window.__perf.settle());
  const m1 = cdp ? await metrics(cdp) : null;
  return fromRecord(r, { result, inp }, m0 && m1 ? diffMetrics(m0, m1) : null);
}

// A settled window's record as a sample.
export function fromRecord(r, { result = null, inp = false } = {}, metricDeltas = null) {
  const missing = !!result && r.resultPaintMs === null;
  return {
    ...frameStats(r.deltas),
    inpMs: inp ? inputDelay(r.ids) : null,
    resultPaintMs: result ? round(missing ? r.spanMs : r.resultPaintMs) : null,
    resultMissing: missing,
    untilMs: round(r.untilMs),
    settleMs: round(r.settleMs),
    mutationsPage: r.mutationsPage,
    mutationsEnabledMods: r.mutationsEnabledMods,
    ended: r.ended,
    inputSeen: r.inputSeen,
    metrics: metricDeltas,
  };
}

// Performance.getMetrics, as a name -> value map.
export async function metrics(cdp) {
  const { metrics: list } = await cdp.send('Performance.getMetrics');
  return Object.fromEntries(list.map((m) => [m.name, m.value]));
}

// The deltas the suite keeps, in ms.
export function diffMetrics(a, b) {
  const d = (k) => round(((b[k] ?? 0) - (a[k] ?? 0)) * 1000);
  return { recalcStyleMs: d('RecalcStyleDuration'), layoutMs: d('LayoutDuration'), scriptMs: d('ScriptDuration'), taskMs: d('TaskDuration') };
}
