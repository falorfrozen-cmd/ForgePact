// How fast the panel answers, in a real (headless) Edge against the sandbox
// server: every interaction the owner named (forgepact-ui-responsive,
// 2026-09-25: "Make sure everything is updating fast/feels responsive"),
// measured with rendering on, and held to the owner's budgets.
//
//   node tests/perf.e2e.mjs [--runs <n>] [--out <file.json>] [--trace-dir <dir>]
//                           [--only <name,name>] [--dist <dir>]   (npm run e2e:perf)
//
// Each run opens a fresh sandbox and a fresh browser context at 1280x800,
// fixes the CPU throttling rate at 1 over CDP, and repeats every interaction
// inside it (each input at least three times). The frames, input-to-next-paint,
// result paint and mutation counts come from the samplers in lib/perf-page.mjs;
// the statistics and budgets from lib/perf-stats.mjs. The median of each
// metric is taken across runs on its own (--runs, default 5; noise raises the
// runs, never the budgets).
//
// Timing runs are untraced, because tracing inflates frames. With --out or
// --trace-dir, one more pass traces each interaction on its own
// (browser.startTracing) and attributes its measured windows' renderer
// main-thread time to style, layout, paint, composite and script; --trace-dir
// keeps each trace as <name>.json. --out writes the results JSON and a table
// beside it (<name>.md). Nothing is written without them.
//
// Two controls run every time, and a number is trusted only when both pass:
// `perf-instrument-catches-jank` (an 80 ms click handler on a page of its own
// must read as a long frame and a slow input, and fail the hard budget) and
// `idle-poll-observer-control` (the observer that finds no idle mutation must
// count a real toggle's, and the idle window must have seen three polls).
// Every interaction but boot has a `budget-<name>` check against the hard
// budget; boot is measured and held to the target through the root causes.
// --only runs a subset (the controls always run). Last line
// `e2e-perf: <n>/<n> checks passed`.

import { mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { dirname, join, resolve } from 'node:path';
import { PANEL_DIR, launchBrowser, parseArgs, postSet, startSandbox, waitBooted, waitSaved } from './lib/browser.mjs';
import { CAP_MS, fromRecord, installSampler, measureSample } from './lib/perf-page.mjs';
import { HARD, TARGET, longestTasks, median, medianOfRuns, overHard, overTarget, p95, panelSrcDigest, round, traceBreakdown } from './lib/perf-stats.mjs';
import { BOOLEAN_MODS } from '../src/enabled-mods.js';
import { HOLD_IDLE_MS, HYSTERESIS_PX } from '../src/lib/enabled-mods-form.js';
import { FREEZE_MAX_MS } from '../src/lib/enabled-mods-undo.js';
import { INSTANT_MS } from '../src/lib/slider-note.js';

const args = parseArgs(process.argv.slice(2));
const RUNS = Math.max(1, parseInt(args.runs ?? '5', 10) || 5);
const OUT = typeof args.out === 'string' ? resolve(args.out) : null;
const TRACE_DIR = typeof args['trace-dir'] === 'string' ? resolve(args['trace-dir']) : null;
const TRACE = !!(OUT || TRACE_DIR);
const DIST = typeof args.dist === 'string' ? args.dist : null;
const VIEWPORT = { width: 1280, height: 800 };
const NARROW = { width: 900, height: 800 };
const CPU_RATE = 1;
const TRACE_CATEGORIES = ['devtools.timeline', 'disabled-by-default-devtools.timeline', 'blink.user_timing', 'toplevel', 'v8'];
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
// A press or a key is held against the list's form change for HOLD_IDLE_MS
// after its release (src/lib/enabled-mods-form.js), and that deferred decide
// is part of the input's cost: every pointer or key window lasts past it.
const MIN_MS = HOLD_IDLE_MS + 40;

export const INTERACTIONS = [
  'toggle-mod-inline', 'toggle-mod-tray', 'tray-open', 'tray-close',
  'form-threshold-toggle', 'form-threshold-resize', 'turn-off-inline', 'turn-off-tray', 'undo',
  'slider-drag-loot', 'slider-drag-modifiers',
  'tab-switch-setup', 'tab-switch-modifiers', 'tab-switch-world', 'tab-switch-loot', 'tab-switch-mods',
  'pool-search-satanic', 'pool-search-gems', 'search-controls',
  'note-hover-loot', 'note-hover-modifiers', 'idle-poll', 'boot',
];
const EXPECTED = [
  'perf-instrument-catches-jank',
  'idle-poll-observer-control',
  'idle-poll-no-mutation-enabled-mods',
  'idle-poll-no-mutation-page',
  'budget-toggle-mod-inline', 'budget-toggle-mod-tray', 'budget-tray-open', 'budget-tray-close',
  'budget-form-threshold-toggle', 'budget-form-threshold-resize', 'budget-turn-off-inline', 'budget-turn-off-tray', 'budget-undo',
  'budget-slider-drag-loot', 'budget-slider-drag-modifiers',
  'budget-tab-switch-setup', 'budget-tab-switch-modifiers', 'budget-tab-switch-world', 'budget-tab-switch-loot', 'budget-tab-switch-mods',
  'budget-pool-search-satanic', 'budget-pool-search-gems', 'budget-search-controls',
  'budget-note-hover-loot', 'budget-note-hover-modifiers', 'budget-idle-poll',
];

const ONLY = typeof args.only === 'string' ? args.only.split(',').map((s) => s.trim()).filter(Boolean) : null;
if (ONLY) {
  const unknown = ONLY.filter((n) => !INTERACTIONS.includes(n));
  if (unknown.length) { console.log(`unknown interaction(s): ${unknown.join(', ')}`); process.exit(2); }
}
const selected = (name) => !ONLY || ONLY.includes(name);
const expectedChecks = EXPECTED.filter((c) => {
  if (c === 'perf-instrument-catches-jank' || c === 'idle-poll-observer-control') return true;
  if (c.startsWith('idle-poll-no-mutation')) return selected('idle-poll');
  return selected(c.slice('budget-'.length));
});

function assert(ok, message) { if (!ok) throw new Error(message); }
const $ = (page, fn, arg) => page.evaluate(fn, arg);
const frames = (page) => $(page, () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))));
// Setup writes go to the sandbox from this process, not the page (lib/browser.mjs postSet).
const post = postSet;
// After an unmeasured press: past the hold's deferred decide, and still.
async function quiet(page) { await waitSaved(page); await wait(MIN_MS); await frames(page); }
// Somewhere that hovers nothing: the status bar's empty middle.
const away = (page) => page.mouse.move(VIEWPORT.width / 2, (page.viewportSize()?.height || VIEWPORT.height) - 8);

// ---- predicates (sources run in the page; `w` is the sample's window) --------
const listed = (id) => `() => !!document.querySelector('#enabledMods li.enabled-mod[data-for="${id}"]')`;
const unlisted = (id) => `() => !document.querySelector('#enabledMods li.enabled-mod[data-for="${id}"]')`;
const formIs = (f) => `() => document.getElementById('enabledMods').dataset.form === '${f}'`;
const tabShown = (t) => `() => document.getElementById('workspace').getAttribute('aria-labelledby') === 'nav-${t}' && [...document.querySelectorAll('.tab-card.active')].some((c) => c.checkVisibility())`;
const countIs = (countSrc, n) => `() => (${countSrc})() === ${n}`;
const COUNT_SAT = `() => [...document.querySelectorAll('#satbuffs .sat-option, #satdebuffs .sat-option')].filter((r) => !r.hidden).length`;
const COUNT_GEMS = `() => [...document.querySelectorAll('#gemfilter_panel .sat-option')].filter((r) => !r.hidden).length`;
const COUNT_CONTROLS = `() => [...document.querySelectorAll('.tab-card.active .setting-entry')].filter((e) => !e.hidden).length`;
const POLLS_3 = `(w) => performance.getEntriesByType('resource').filter((e) => e.name.endsWith('/api/state') && e.responseEnd >= w.t0).length >= 3`;
const BOOTED = `() => document.getElementById('saveIndicator')?.textContent === 'Settings loaded'`;

// ---- the page ------------------------------------------------------------------
async function openRun(browser, sandbox, trace) {
  const context = await browser.newContext({ viewport: VIEWPORT });
  await context.addInitScript(installSampler);
  // Every document opens a boot window at once; a reload's is simply replaced
  // by the next sample's.
  await context.addInitScript(`window.__perf && window.__perf.begin(${JSON.stringify({ noInput: true, until: BOOTED, stillMs: 300, capMs: 10000 })})`);
  const page = await context.newPage();
  const cdp = await context.newCDPSession(page);
  await cdp.send('Emulation.setCPUThrottlingRate', { rate: CPU_RATE });
  await cdp.send('Performance.enable');
  return { context, page, cdp, sandbox, trace, browser };
}

async function reload(page) { await page.reload(); await waitBooted(page); await frames(page); }

// Every boolean mod off but `on`, then a reload: the list renders from the
// saved config in boot().
async function setMods(page, on) {
  for (const key of BOOLEAN_MODS) await post(page, { key, value: on.includes(key) });
  await reload(page);
}

async function openTab(page, name) {
  if (await $(page, (n) => document.querySelector('.tabbtn.active')?.dataset.tab === n, name)) return;
  await $(page, (n) => document.querySelector(`.tabbtn[data-tab="${n}"]`).click(), name);
  await quiet(page);
}

// Show the tab (and the Mods sub-tab) a control sits on.
async function reveal(page, id) {
  const where = await $(page, (i) => {
    const el = document.getElementById(i);
    const card = el?.closest('.tab-card');
    return { tab: card?.dataset.tab, sub: el?.closest('#qolCard') ? 'subtab-qol' : el?.closest('#itemsCard') ? 'subtab-items' : el?.closest('#gameplayCard') ? 'subtab-gameplay' : null };
  }, id);
  await openTab(page, where.tab);
  if (where.sub && await $(page, (s) => document.getElementById(s).getAttribute('aria-selected') !== 'true', where.sub)) {
    await $(page, (s) => document.getElementById(s).click(), where.sub);
    await quiet(page);
  }
}

// ---- samples ---------------------------------------------------------------------
// ctx.samples[name] collects one run's samples; ctx.trace is the traced pass.
async function interaction(ctx, name, fn) {
  if (!selected(name)) return;
  const samples = [];
  const extra = {};
  let traceJson = null;
  if (ctx.trace) await ctx.browser.startTracing(ctx.page, { categories: TRACE_CATEGORIES });
  try {
    await fn(samples, extra);
  } catch (e) {
    ctx.errors[name] = e.message.split('\n')[0];
  } finally {
    if (ctx.trace) {
      const buffer = await ctx.browser.stopTracing();
      traceJson = JSON.parse(buffer.toString('utf8'));
      if (TRACE_DIR) { mkdirSync(TRACE_DIR, { recursive: true }); writeFileSync(join(TRACE_DIR, `${name}.json`), buffer); }
    }
  }
  ctx.samples[name] = { samples, extra, trace: traceJson ? { ...traceBreakdown(traceJson, samples.length), longestTasksMs: longestTasks(traceJson) } : null };
}

const measure = (ctx, spec) => measureSample(ctx.page, { minMs: MIN_MS, ...spec }, ctx.cdp);

// Do what a spec does without measuring it (the other half of a pair, a
// reset); nothing when its result already holds.
async function act(ctx, spec) {
  if (spec.result && await ctx.page.evaluate(`!!(${spec.result})({})`)) return;
  await spec.input();
  if (spec.result) await ctx.page.waitForFunction(`(${spec.result})({})`, null, { timeout: 8000, polling: 20 });
  if (spec.until) await ctx.page.waitForFunction(`(${spec.until})({})`, null, { timeout: 8000, polling: 20 });
  await quiet(ctx.page);
}

// Steps that undo each other (open/close, the five tabs): the timing pass
// measures them in turn; the traced pass traces each on its own, with the step
// before it done unmeasured, so every trace holds only its own windows.
async function cycle(ctx, steps, rounds) {
  const live = steps.filter((s) => selected(s.name));
  if (!live.length) return;
  if (!ctx.trace) {
    const acc = Object.fromEntries(live.map((s) => [s.name, []]));
    for (let r = 0; r < rounds; r++) for (const s of steps) {
      if (acc[s.name]) acc[s.name].push(await measure(ctx, s.spec));
      else await act(ctx, s.spec);
    }
    for (const s of live) await interaction(ctx, s.name, async (samples) => { samples.push(...acc[s.name]); });
    return;
  }
  for (let i = 0; i < steps.length; i++) {
    const s = steps[i];
    const before = steps[(i + steps.length - 1) % steps.length];
    if (!selected(s.name)) continue;
    await interaction(ctx, s.name, async (samples) => {
      for (let r = 0; r < rounds; r++) {
        if (r > 0 || i === 0) await act(ctx, before.spec);
        samples.push(await measure(ctx, s.spec));
      }
    });
  }
}

const click = (ctx, selector) => () => ctx.page.click(selector);

// The first sample of a page: its boot, measured by the window every document
// opens.
async function boot(ctx) {
  const { page } = ctx;
  if (!selected('boot')) { await page.goto(ctx.sandbox.url); await waitBooted(page); return; }
  await interaction(ctx, 'boot', async (samples, extra) => {
    await page.goto(ctx.sandbox.url);
    const r = await $(page, () => window.__perf.settle());
    samples.push(fromRecord(r));
    extra.bootSettledMs = round(r.untilMs);
  });
  await waitBooted(page);
  await frames(page);
}

// Three polls with nothing to change, right after boot (the poll's fast tier
// covers the first seconds of a session). Then, once, the control: the same
// observer must count a real toggle's mutations.
async function idlePoll(ctx) {
  const { page } = ctx;
  let polls = 0;
  const onDone = (r) => { if (r.method() === 'GET' && r.url().endsWith('/api/state')) polls++; };
  page.on('requestfinished', onDone);
  await interaction(ctx, 'idle-poll', async (samples, extra) => {
    polls = 0;
    const s = await measureSample(page, { noInput: true, inp: false, until: POLLS_3, stillMs: 300, capMs: 12000 }, ctx.cdp);
    samples.push(s);
    extra.polls = polls;
    extra.mutationsPage = s.mutationsPage;
    extra.mutationsEnabledMods = s.mutationsEnabledMods;
  });
  page.off('requestfinished', onDone);
  if (ctx.control) {
    const idle = ctx.samples['idle-poll'];
    let idleMutations = idle?.extra;
    if (!idleMutations) {
      polls = 0;
      page.on('requestfinished', onDone);
      const s = await measureSample(page, { noInput: true, inp: false, until: POLLS_3, stillMs: 300, capMs: 12000 });
      page.off('requestfinished', onDone);
      idleMutations = { polls, mutationsPage: s.mutationsPage, mutationsEnabledMods: s.mutationsEnabledMods };
    }
    await reveal(page, 'mod_orb_pickup_radius');
    const toggled = await measure(ctx, { input: click(ctx, '#mod_orb_pickup_radius'), result: listed('mod_orb_pickup_radius'), inputEvents: ['pointerdown'] });
    await act(ctx, { input: click(ctx, '#mod_orb_pickup_radius'), result: unlisted('mod_orb_pickup_radius') });
    ctx.control.observer = { idle: idleMutations, toggled: { page: toggled.mutationsPage, enabledMods: toggled.mutationsEnabledMods } };
  }
}

async function tabSwitches(ctx) {
  const { page } = ctx;
  const tabs = ['setup', 'modifiers', 'world', 'loot', 'mods'];
  const current = await $(page, () => document.querySelector('.tabbtn.active')?.dataset.tab);
  const at = tabs.indexOf(current);
  const order = tabs.slice(at + 1).concat(tabs.slice(0, at + 1));
  await cycle(ctx, order.map((t) => ({
    name: `tab-switch-${t}`,
    spec: { input: click(ctx, `.tabbtn[data-tab="${t}"]`), result: tabShown(t), inputEvents: ['pointerdown'] },
  })), 3);
}

// Three keystrokes into a search box: a letter that filters the rows, the
// Backspace that brings them back, the letter again. The counts each should
// reach are read once beforehand (a warm-up keystroke, unmeasured).
async function search(ctx, name, inputSel, countSrc) {
  const { page } = ctx;
  if (!selected(name)) return;
  await page.click(inputSel);
  await quiet(page);
  const count = () => $(page, (src) => (0, eval)('(' + src + ')')(), countSrc);
  const all = await count();
  let key = null;
  let some = null;
  for (const k of ['x', 'z', 'q', 'v', 'k', 'j']) {
    await page.keyboard.press(k);
    const n = await count();
    await page.keyboard.press('Backspace');
    if (n !== all) { key = k; some = n; break; }
  }
  await quiet(page);
  assert(key, `${name}: no letter filtered the rows`);
  const type = { input: () => page.keyboard.press(key), result: countIs(countSrc, some), inputEvents: ['keydown'] };
  const back = { input: () => page.keyboard.press('Backspace'), result: countIs(countSrc, all), inputEvents: ['keydown'] };
  await interaction(ctx, name, async (samples) => {
    samples.push(await measure(ctx, type));
    samples.push(await measure(ctx, back));
    samples.push(await measure(ctx, type));
  });
  await act(ctx, back);
}

// A slider dragged by its thumb: pressed, moved in steps, released. The
// release saves, and the entry it lists (or unlists) is the result, timed
// from the release; frames count from the press.
async function drag(page, selector, toFraction) {
  const { box, frac } = await $(page, (s) => {
    const r = document.querySelector(s);
    r.scrollIntoView({ block: 'center' });
    const b = r.getBoundingClientRect();
    return { box: { x: b.x, y: b.y, width: b.width, height: b.height }, frac: (r.value - r.min) / (r.max - r.min) };
  }, selector);
  const thumb = 6;
  const xAt = (f) => box.x + thumb / 2 + f * (box.width - thumb);
  const y = box.y + box.height / 2;
  await page.mouse.move(xAt(frac), y);
  await page.mouse.down();
  const to = xAt(toFraction) + (toFraction <= 0 ? -4 : 0);
  for (let i = 1; i <= 8; i++) await page.mouse.move(xAt(frac) + ((to - xAt(frac)) * i) / 8, y);
  await page.mouse.up();
}

async function sliderDrag(ctx, name, tab, sec, key) {
  const { page } = ctx;
  if (!selected(name)) return;
  await openTab(page, tab);
  const sel = `input[type=range][data-sec="${sec}"][data-key="${key}"]`;
  const id = `sw_${sec}_${key}`;
  const up = { input: () => drag(page, sel, 0.3), result: listed(id), inputEvents: ['pointerdown'], resultFrom: 'pointerup', inp: false };
  const down = { input: () => drag(page, sel, 0), result: unlisted(id), inputEvents: ['pointerdown'], resultFrom: 'pointerup', inp: false };
  await interaction(ctx, name, async (samples) => {
    samples.push(await measure(ctx, up));
    samples.push(await measure(ctx, down));
    samples.push(await measure(ctx, up));
  });
  await act(ctx, down);
  await away(page);
}

// An idle row's note, as polish's item 9 draws it: a tooltip above the row.
// A row at its default has no note, so each hovered row first holds an
// unsaved value (the range moved and `input` dispatched, never `change`, so
// panel.js writes the note and nothing is saved), as polish.e2e.mjs does.
// Each hover starts from nowhere, past INSTANT_MS, so it takes the delayed
// path; the window closes once the tooltip is up and the page is still.
async function noteHover(ctx, name, tab, sec, keys) {
  const { page } = ctx;
  if (!selected(name)) return;
  await openTab(page, tab);
  const entries = [];
  for (const key of keys) {
    const sel = `input[type=range][data-sec="${sec}"][data-key="${key}"]`;
    const ok = await $(page, (s) => {
      const r = document.querySelector(s);
      const e = r?.closest('.setting-entry');
      if (!e?.checkVisibility() || !e.querySelector(':scope .slider-switch:not([data-live]) > input:checked')) return false;
      r.value = Math.min(+r.max, +r.min + (parseFloat(r.dataset.step0 || r.step) || 1));
      r.dispatchEvent(new Event('input', { bubbles: true }));
      return true;
    }, sel);
    if (ok) entries.push({ sel, key });
  }
  assert(entries.length >= 3, `${name}: ${entries.length} idle rows to hover, not 3`);
  await interaction(ctx, name, async (samples) => {
    for (const { key } of entries.slice(0, 3)) {
      await away(page);
      await wait(INSTANT_MS + 60);
      await frames(page);
      const entry = `.setting-entry:has(> .row input[type=range][data-sec="${sec}"][data-key="${key}"])`;
      samples.push(await measure(ctx, {
        input: () => page.hover(`${entry} .lbl`),
        until: `() => !!document.querySelector('${entry} > .note[data-tooltip]')`,
        inputEvents: ['pointermove'], inp: false, minMs: 0,
      }));
    }
  });
  await away(page);
  for (const { sel } of entries) await $(page, (s) => { const r = document.querySelector(s); r.value = r.min; r.dispatchEvent(new Event('input', { bubbles: true })); }, sel);
  await frames(page);
}

// A switch clicked on, off and on again; the entry in #enabledMods is the
// result. Put back off afterwards.
async function toggleMod(ctx, name, id) {
  const { page } = ctx;
  if (!selected(name)) return;
  await reveal(page, id);
  const on = await $(page, (i) => document.getElementById(i).checked, id);
  const flip = (to) => ({ input: click(ctx, `#${id}`), result: to ? listed(id) : unlisted(id), inputEvents: ['pointerdown'] });
  await interaction(ctx, name, async (samples) => {
    samples.push(await measure(ctx, flip(!on)));
    samples.push(await measure(ctx, flip(on)));
    samples.push(await measure(ctx, flip(!on)));
  });
  await act(ctx, flip(on));
}

// Turn off from the list, three times; with `undo`, each is followed by the
// undo toast's Undo, measured as `undo`. The first Turn off of a run leaves
// the pointer in the list and waits for the placeholder to let go by itself
// (freezeMs: how long it held); the others end it by leaving the list.
async function turnOffs(ctx, name, ids, { tray = false, undo = false } = {}) {
  const { page } = ctx;
  const doUndo = undo && selected('undo');
  if (!selected(name) && !doUndo) return;
  const undoSamples = [];
  const freezes = [];
  const once = async (id, i, samples) => {
    if (tray && await $(page, () => !document.getElementById('enabledMods').hasAttribute('data-open'))) {
      await act(ctx, { input: click(ctx, '.enabled-mods-toggle'), result: `() => document.getElementById('enabledMods').hasAttribute('data-open')` });
    }
    await $(page, () => { window.__perf.ghost.track = true; window.__perf.ghost.spans.length = 0; window.__perf.ghost.since = null; });
    const spec = { input: click(ctx, `#enabledMods .quick-disable[data-for="${id}"]`), result: unlisted(id), inputEvents: ['pointerdown'] };
    if (samples) samples.push(await measure(ctx, spec));
    else await act(ctx, spec);
    if (i === 0) {
      await page.waitForFunction(() => !document.querySelector('.enabled-mod-ghost') && window.__perf.ghost.since === null, null, { timeout: FREEZE_MAX_MS + 2000, polling: 20 });
      const spans = await $(page, () => window.__perf.ghost.spans.slice());
      if (spans.length) freezes.push(round(spans.reduce((a, b) => a + b, 0)));
    } else {
      await away(page);
      await page.waitForFunction(() => !document.querySelector('.enabled-mod-ghost'), null, { timeout: 4000, polling: 20 });
    }
    await $(page, () => { window.__perf.ghost.track = false; });
    await quiet(page);
    if (undo) {
      const back = { input: click(ctx, '.undo-toast-button'), result: listed(id), inputEvents: ['pointerdown'] };
      if (doUndo && !ctx.trace) undoSamples.push(await measure(ctx, back));
      else await act(ctx, back);
      await away(page);
      await quiet(page);
    }
  };
  if (selected(name)) {
    await interaction(ctx, name, async (samples, extra) => {
      for (let i = 0; i < ids.length; i++) await once(ids[i], i, samples);
      extra.freezeMs = freezes[0] ?? null;
    });
  } else {
    for (let i = 0; i < ids.length; i++) await once(ids[i], i, null);
  }
  if (doUndo) {
    if (!ctx.trace) await interaction(ctx, 'undo', async (samples) => { samples.push(...undoSamples); });
    else {
      // Traced on its own: each Undo after an unmeasured Turn off.
      await interaction(ctx, 'undo', async (samples) => {
        for (const id of ids) {
          await act(ctx, { input: click(ctx, `#enabledMods .quick-disable[data-for="${id}"]`), result: unlisted(id) });
          await away(page);
          samples.push(await measure(ctx, { input: click(ctx, '.undo-toast-button'), result: listed(id), inputEvents: ['pointerdown'] }));
          await away(page);
          await quiet(page);
        }
      });
    }
  }
}

// The Enabled mods row just short of the tray: every entry's width read once
// with the row laid out on one line (the form module's own data-measure),
// then the widest Mods-tab entry is the tip and a set of others fills the row
// so that the tip tips it into the tray and taking the tip away brings it
// back past the hysteresis band.
async function findThreshold(page) {
  const candidates = BOOLEAN_MODS.filter((k) => !k.startsWith('mod_gem_'));
  await setMods(page, candidates);
  const m = await $(page, () => {
    const box = document.getElementById('enabledMods');
    box.setAttribute('data-measure', '');
    const style = getComputedStyle(box);
    const available = box.clientWidth - parseFloat(style.paddingLeft) - parseFloat(style.paddingRight);
    const head = box.querySelector(':scope > h2').getBoundingClientRect();
    const items = [...box.querySelectorAll(':scope > ul > li.enabled-mod')];
    const rects = items.map((li) => li.getBoundingClientRect());
    const out = {
      available,
      base: rects[0].left - head.left,
      gap: rects.length > 1 ? rects[1].left - rects[0].right : 0,
      widths: Object.fromEntries(items.map((li, i) => [li.dataset.for, rects[i].width])),
      onMods: Object.fromEntries(items.map((li) => [li.dataset.for, !!document.getElementById(li.dataset.for)?.closest('#qolCard, #itemsCard, #gameplayCard')])),
    };
    box.removeAttribute('data-measure');
    return out;
  });
  const natural = (ids) => m.base + ids.reduce((a, id) => a + m.widths[id], 0) + m.gap * Math.max(0, ids.length - 1);
  const safe = 12;
  const tips = Object.keys(m.widths).filter((id) => m.onMods[id]).sort((a, b) => m.widths[b] - m.widths[a]);
  for (const tip of tips) {
    const rest = Object.keys(m.widths).filter((id) => id !== tip);
    let best = null;
    for (let mask = 1; mask < (1 << rest.length); mask++) {
      const ids = rest.filter((_, i) => mask & (1 << i));
      const without = natural(ids);
      const withTip = natural([...ids, tip]);
      if (without <= m.available - HYSTERESIS_PX - safe && withTip > m.available + safe && (!best || ids.length < best.length)) best = ids;
    }
    if (best) {
      await setMods(page, best);
      assert(await $(page, () => document.getElementById('enabledMods').dataset.form) === 'inline', 'threshold: the filled row is not inline at 1280');
      return { tip, fillers: best };
    }
  }
  throw new Error('threshold: no set of entries sits just short of the tray at 1280');
}

async function formThreshold(ctx) {
  const { page } = ctx;
  if (!selected('form-threshold-toggle') && !selected('form-threshold-resize')) return;
  const { tip } = await findThreshold(page);
  await reveal(page, tip);
  if (selected('form-threshold-toggle')) {
    const flip = (on) => ({ input: click(ctx, `#${tip}`), result: on ? listed(tip) : unlisted(tip), until: formIs(on ? 'tray' : 'inline'), inputEvents: ['pointerdown'], capMs: CAP_MS + 2000 });
    await interaction(ctx, 'form-threshold-toggle', async (samples, extra) => {
      samples.push(await measure(ctx, flip(true)));
      samples.push(await measure(ctx, flip(false)));
      samples.push(await measure(ctx, flip(true)));
      extra.formSettleMs = median(samples.map((s) => s.settleMs));
    });
    await act(ctx, flip(false));
  }
  if (selected('form-threshold-resize')) {
    await away(page);
    await wait(MIN_MS);
    const to = (size, form) => ({ input: () => page.setViewportSize(size), result: formIs(form), until: formIs(form), inputEvents: ['resize'], inp: false, minMs: 0 });
    await interaction(ctx, 'form-threshold-resize', async (samples, extra) => {
      samples.push(await measure(ctx, to(NARROW, 'tray')));
      samples.push(await measure(ctx, to(VIEWPORT, 'inline')));
      samples.push(await measure(ctx, to(NARROW, 'tray')));
      samples.push(await measure(ctx, to(VIEWPORT, 'inline')));
      extra.formSettleMs = median(samples.map((s) => s.settleMs));
    });
    await page.setViewportSize(VIEWPORT);
    await frames(page);
  }
}

async function trayOpenClose(ctx) {
  const open = `() => document.getElementById('enabledMods').hasAttribute('data-open')`;
  const closed = `() => !document.getElementById('enabledMods').hasAttribute('data-open')`;
  await cycle(ctx, [
    { name: 'tray-open', spec: { input: click(ctx, '.enabled-mods-toggle'), result: open, inputEvents: ['pointerdown'] } },
    { name: 'tray-close', spec: { input: click(ctx, '.enabled-mods-toggle'), result: closed, inputEvents: ['pointerdown'] } },
  ], 3);
}

// One run: a fresh sandbox, a fresh context, every interaction in turn.
async function run(browser, { trace = false, control = null } = {}) {
  const sandbox = await startSandbox({ dist: DIST });
  const ctx = await openRun(browser, sandbox, trace);
  ctx.samples = {};
  ctx.errors = {};
  ctx.control = control;
  // A step that throws is recorded against the interactions it had not
  // measured yet, and the run goes on with the next step.
  const step = async (names, fn) => {
    if (!names.some(selected)) return;
    const t = Date.now();
    try { await fn(); if (args.verbose) console.log(`     step ${names[0]}: ${((Date.now() - t) / 1000).toFixed(1)} s`); } catch (e) {
      for (const n of names) if (!ctx.samples[n] && !ctx.errors[n]) ctx.errors[n] = e.message.split('\n')[0];
      await away(ctx.page).catch(() => {});
      await quiet(ctx.page).catch(() => {});
    }
  };
  const keysOf = (sel) => $(ctx.page, (s) => [...document.querySelectorAll(`${s} input[type=range]`)].map((r) => r.dataset.key), sel);
  try {
    // Product defaults: nothing on, the list inline.
    await boot(ctx);
    await step(['idle-poll'], () => idlePoll(ctx));
    await step(INTERACTIONS.filter((n) => n.startsWith('tab-switch-')), () => tabSwitches(ctx));
    await step(['search-controls'], async () => { await openTab(ctx.page, 'loot'); await search(ctx, 'search-controls', '#controlSearch', COUNT_CONTROLS); });
    await step(['pool-search-gems'], async () => {
      await openTab(ctx.page, 'loot');
      await act(ctx, { input: click(ctx, '#gemfilter_toggle'), result: `() => document.getElementById('gemfilter_panel').style.display === 'block'` });
      await search(ctx, 'pool-search-gems', '#gemSearch', COUNT_GEMS);
      await act(ctx, { input: click(ctx, '#gemfilter_toggle'), result: `() => document.getElementById('gemfilter_panel').style.display === 'none'` });
    });
    await step(['pool-search-satanic'], async () => { await openTab(ctx.page, 'world'); await search(ctx, 'pool-search-satanic', '#satSearch', COUNT_SAT); });
    await step(['note-hover-loot'], async () => noteHover(ctx, 'note-hover-loot', 'loot', 'keys', await keysOf('#keys')));
    await step(['note-hover-modifiers'], async () => noteHover(ctx, 'note-hover-modifiers', 'modifiers', 'stats', await keysOf('#stats')));
    await step(['slider-drag-loot'], async () => sliderDrag(ctx, 'slider-drag-loot', 'loot', 'keys', (await keysOf('#keys'))[0]));
    await step(['slider-drag-modifiers'], () => sliderDrag(ctx, 'slider-drag-modifiers', 'modifiers', 'stats', 'exp'));
    await step(['toggle-mod-inline'], () => toggleMod(ctx, 'toggle-mod-inline', 'mod_orb_pickup_radius'));

    // A few mods on, inline: Turn off and Undo.
    await step(['turn-off-inline', 'undo'], async () => {
      // Keep three real entries inline at the standard 1280px viewport, also
      // with Ember's sidebar/visible scrollbar. The long orb-pickup label
      // correctly overflows there; no performance budget or sample changes.
      // Each Turn off is put back by its Undo.
      const three = ['map_reveal', 'headhunter', 'beacon'];
      await setMods(ctx.page, three);
      assert(await $(ctx.page, () => document.getElementById('enabledMods').dataset.form) === 'inline', 'three entries at 1280 are not inline');
      await turnOffs(ctx, 'turn-off-inline', three, { undo: true });
    });

    // Every boolean mod on: the tray.
    await step(['tray-open', 'tray-close', 'toggle-mod-tray', 'turn-off-tray'], async () => {
      await setMods(ctx.page, BOOLEAN_MODS);
      assert(await $(ctx.page, () => document.getElementById('enabledMods').dataset.form) === 'tray', 'every mod on at 1280 is not the tray');
      await trayOpenClose(ctx);
      await toggleMod(ctx, 'toggle-mod-tray', 'mod_orb_pickup_radius');
      await turnOffs(ctx, 'turn-off-tray', ['headhunter', 'tyrant', 'beacon'], { tray: true });
      await away(ctx.page);
    });

    // The row just short of the line.
    await step(['form-threshold-toggle', 'form-threshold-resize'], () => formThreshold(ctx));
  } finally {
    await ctx.context.close();
    await sandbox.stop();
  }
  return ctx;
}

// ---- the controls ----------------------------------------------------------------
// An 80 ms click handler on a page of its own, through the same samplers.
async function jankControl(browser) {
  const context = await browser.newContext({ viewport: VIEWPORT });
  const page = await context.newPage();
  const cdp = await context.newCDPSession(page);
  await cdp.send('Emulation.setCPUThrottlingRate', { rate: CPU_RATE });
  await page.setContent('<button id="busy" style="width:200px;height:60px">busy</button><div id="out"></div>' +
    '<script>document.getElementById("busy").onclick=()=>{const t=performance.now();while(performance.now()-t<80);document.getElementById("out").textContent=String(Math.random());};</script>');
  await page.evaluate(installSampler);
  const samples = [];
  for (let i = 0; i < 3; i++) {
    samples.push(await measureSample(page, { input: () => page.click('#busy'), result: `() => document.getElementById('out').textContent !== ''`, inputEvents: ['pointerdown'] }));
    await page.evaluate(() => { document.getElementById('out').textContent = ''; });
    await wait(50);
  }
  await context.close();
  const m = aggregate(samples);
  return { median: m, hard: overHard('jank-control', m) };
}

// ---- aggregation -----------------------------------------------------------------
// One run's samples of one interaction as the run's record.
function aggregate(samples, extra = {}) {
  const inp = samples.some((s) => s.inpMs !== null);
  const res = samples.some((s) => s.resultPaintMs !== null);
  const sum = (k) => samples.reduce((a, s) => a + (s[k] ?? 0), 0);
  const metricKeys = ['recalcStyleMs', 'layoutMs', 'scriptMs', 'taskMs'];
  const withMetrics = samples.filter((s) => s.metrics);
  return {
    longestFrameMs: samples.length ? round(Math.max(...samples.map((s) => s.longestFrameMs ?? 0))) : null,
    framesOver16_7: sum('framesOver16_7'),
    framesOver50: sum('framesOver50'),
    inpP95Ms: inp ? p95(samples.map((s) => s.inpMs)) : null,
    resultPaintP95Ms: res ? round(p95(samples.map((s) => s.resultPaintMs))) : null,
    samples: samples.length,
    resultMissing: samples.filter((s) => s.resultMissing).length,
    capped: samples.filter((s) => s.ended === 'cap').length,
    metrics: withMetrics.length ? Object.fromEntries(metricKeys.map((k) => [k, round(withMetrics.reduce((a, s) => a + (s.metrics[k] ?? 0), 0) / withMetrics.length)])) : null,
    ...extra,
  };
}

const TIMING_KEYS = ['longestFrameMs', 'framesOver16_7', 'framesOver50', 'inpP95Ms', 'resultPaintP95Ms'];
const ATTRIBUTION_KEYS = ['styleMs', 'layoutMs', 'paintMs', 'compositeMs', 'scriptMs'];
const EXTRA_KEYS = { 'idle-poll': ['mutationsEnabledMods', 'mutationsPage', 'polls'], 'form-threshold-toggle': ['formSettleMs'], 'form-threshold-resize': ['formSettleMs'], 'turn-off-inline': ['freezeMs'], 'turn-off-tray': ['freezeMs'], boot: ['bootSettledMs'] };

function fmt(v) { return v === null || v === undefined ? '-' : String(v); }

function table(interactions) {
  const lines = ['| interaction | longest frame ms | frames >16.7 | frames >50 | INP p95 ms | result p95 ms | style ms | layout ms | paint ms | composite ms | script ms | target | hard |',
    '|---|---|---|---|---|---|---|---|---|---|---|---|---|'];
  for (const [name, d] of Object.entries(interactions)) {
    const m = d.median;
    const t = overTarget(name, m);
    const h = name === 'boot' ? null : overHard(name, m);
    lines.push(`| ${name} | ${[...TIMING_KEYS, ...ATTRIBUTION_KEYS].map((k) => fmt(m[k])).join(' | ')} | ${t.length ? 'over (' + t.join(', ') + ')' : 'within'} | ${h === null ? 'n/a' : h.length ? 'over (' + h.join(', ') + ')' : 'within'} |`);
  }
  return lines.join('\n');
}

// ---- main ------------------------------------------------------------------------
const started = Date.now();
const browser = await launchBrowser();
const passed = [];
const failed = [];
const pass = (c, note = '') => { passed.push(c); console.log(`ok   ${c}${note ? ' (' + note + ')' : ''}`); };
const fail = (c, why) => { failed.push(c); console.log(`FAIL ${c}: ${why}`); };
const runs = [];
let tracedRun = null;
let jank = null;
const control = {};
let edgeVersion = null;
try {
  edgeVersion = browser.version();
  jank = await jankControl(browser);
  for (let i = 0; i < RUNS; i++) {
    const t = Date.now();
    runs.push(await run(browser, { control: i === 0 ? control : null }));
    console.log(`     run ${i + 1}/${RUNS}: ${Math.round((Date.now() - t) / 1000)} s`);
  }
  if (TRACE) {
    const t = Date.now();
    tracedRun = await run(browser, { trace: true });
    console.log(`     traced pass: ${Math.round((Date.now() - t) / 1000)} s`);
  }
} finally {
  await browser.close();
}

// Controls first: nothing else is trusted without them.
if (jank.median.longestFrameMs >= 70 && jank.median.inpP95Ms >= 70 && jank.hard.length) {
  pass('perf-instrument-catches-jank', `longest frame ${jank.median.longestFrameMs} ms, INP p95 ${jank.median.inpP95Ms} ms, hard: ${jank.hard.join(', ')}`);
} else fail('perf-instrument-catches-jank', `longest frame ${jank.median.longestFrameMs} ms, INP p95 ${jank.median.inpP95Ms} ms, hard verdict ${jank.hard.join(', ') || 'within'}`);
const ob = control.observer;
if (ob && ob.toggled.page > 0 && ob.toggled.enabledMods > 0 && ob.idle.polls >= 3) {
  pass('idle-poll-observer-control', `toggle: ${ob.toggled.page} page / ${ob.toggled.enabledMods} list mutations; idle window: ${ob.idle.polls} polls`);
} else fail('idle-poll-observer-control', ob ? `toggle counted ${ob.toggled.page} page / ${ob.toggled.enabledMods} list mutations, idle window saw ${ob.idle.polls} polls` : 'did not run');

const interactions = {};
for (const name of INTERACTIONS.filter(selected)) {
  const perRun = runs.map((r) => r.samples[name] ? aggregate(r.samples[name].samples, r.samples[name].extra) : null).filter(Boolean);
  const errors = runs.map((r) => r.errors[name]).filter(Boolean);
  const med = medianOfRuns(perRun, TIMING_KEYS);
  for (const k of EXTRA_KEYS[name] || []) med[k] = median(perRun.map((r) => r[k]));
  const tr = tracedRun?.samples[name]?.trace || null;
  for (const k of ATTRIBUTION_KEYS) med[k] = tr ? tr[k] : null;
  interactions[name] = { median: med, runs: perRun, ...(tr ? { trace: tr } : {}), ...(errors.length ? { errors } : {}) };
  const t = overTarget(name, med);
  console.log(`     ${name}: frame ${fmt(med.longestFrameMs)} ms, >16.7 ${fmt(med.framesOver16_7)}, >50 ${fmt(med.framesOver50)}, INP p95 ${fmt(med.inpP95Ms)}, result p95 ${fmt(med.resultPaintP95Ms)}` +
    `${(EXTRA_KEYS[name] || []).map((k) => `, ${k} ${fmt(med[k])}`).join('')}${tr ? `; style ${tr.styleMs} layout ${tr.layoutMs} paint ${tr.paintMs} composite ${tr.compositeMs} script ${tr.scriptMs}` : ''}; target ${t.length ? 'over (' + t.join(', ') + ')' : 'within'}`);
  if (name === 'boot') continue;
  const why = [];
  if (errors.length) why.push(`${errors.length} run(s) failed: ${errors[0]}`);
  if (perRun.length < RUNS) why.push(`${perRun.length}/${RUNS} runs measured`);
  const missing = perRun.reduce((a, r) => a + r.resultMissing, 0);
  if (missing) why.push(`${missing} sample(s) never showed their result`);
  const h = overHard(name, med);
  if (h.length) why.push(`over the hard budget: ${h.join(', ')} (frame ${fmt(med.longestFrameMs)} ms, >50 ${fmt(med.framesOver50)}, INP ${fmt(med.inpP95Ms)}, result ${fmt(med.resultPaintP95Ms)})`);
  if (why.length) fail(`budget-${name}`, why.join('; '));
  else pass(`budget-${name}`);
}
if (selected('idle-poll')) {
  const m = interactions['idle-poll']?.median;
  if (m && m.mutationsEnabledMods === 0) pass('idle-poll-no-mutation-enabled-mods');
  else fail('idle-poll-no-mutation-enabled-mods', `${fmt(m?.mutationsEnabledMods)} mutations under #enabledMods across ${fmt(m?.polls)} idle polls`);
  if (m && m.mutationsPage === 0) pass('idle-poll-no-mutation-page');
  else fail('idle-poll-no-mutation-page', `${fmt(m?.mutationsPage)} mutations in the page across ${fmt(m?.polls)} idle polls`);
}

if (OUT) {
  const pw = JSON.parse(readFileSync(join(PANEL_DIR, 'node_modules', 'playwright-core', 'package.json'), 'utf8')).version;
  const result = {
    meta: {
      edgeVersion,
      playwrightVersion: pw,
      cpuThrottlingRate: CPU_RATE,
      runs: RUNS,
      viewport: VIEWPORT,
      measuredAt: new Date().toISOString(),
      panelSrcDigest: panelSrcDigest(join(PANEL_DIR, 'src')),
      only: ONLY,
      target: TARGET,
      hard: HARD,
      attribution: 'styleMs, layoutMs, paintMs, compositeMs and scriptMs are per sample, from one traced pass of each interaction, cut to its measured windows; every other median is across the untraced runs',
      durationS: Math.round((Date.now() - started) / 1000),
    },
    controls: { jank, observer: control.observer || null },
    interactions,
  };
  mkdirSync(dirname(OUT), { recursive: true });
  writeFileSync(OUT, JSON.stringify(result, null, 2) + '\n');
  const md = OUT.replace(/\.json$/i, '') + '.md';
  writeFileSync(md, `# Panel responsiveness: ${OUT.split(/[\\/]/).pop()}\n\n` +
    `Edge ${edgeVersion}, playwright-core ${pw}, CPU throttling rate ${CPU_RATE}, ${RUNS} runs, ${VIEWPORT.width}x${VIEWPORT.height}, ` +
    `measured ${result.meta.measuredAt}, panel/src ${result.meta.panelSrcDigest.slice(0, 12)}.\n\n` +
    `Medians across runs. Target: longest frame <= ${TARGET.longestFrameMs} ms, no frame over 50 ms, INP p95 <= ${TARGET.inpP95Ms} ms, result paint p95 <= ${TARGET.resultPaintP95Ms} ms, no idle mutation. ` +
    `Hard (what e2e:perf asserts): longest frame <= ${HARD.longestFrameMs} ms, no frame over 50 ms, both p95 <= ${HARD.inpP95Ms} ms, no idle mutation. ` +
    `Style, layout, paint, composite and script are ms per sample from the traced pass.\n\n${table(interactions)}\n\n` +
    `Extras: ${Object.entries(EXTRA_KEYS).filter(([n]) => interactions[n]).map(([n, ks]) => `${n}: ${ks.map((k) => `${k} ${fmt(interactions[n].median[k])}`).join(', ')}`).join('; ')}.\n\n` +
    `Checks: ${passed.length}/${expectedChecks.length} passed${failed.length ? '; failed: ' + failed.join(', ') : ''}.\n`);
}

console.log(`     total: ${Math.round((Date.now() - started) / 1000)} s`);
const missing = expectedChecks.filter((c) => !passed.includes(c) && !failed.includes(c));
for (const c of missing) console.log(`FAIL not run: ${c}`);
console.log(`e2e-perf: ${passed.length}/${expectedChecks.length} checks passed`);
process.exitCode = failed.length || missing.length ? 1 : 0;
