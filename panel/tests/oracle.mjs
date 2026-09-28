// The behaviour oracle: proof that a panel build sends exactly what the old
// page sent.
//
//   node tests/oracle.mjs record [--legacy] --out tests/behaviour-oracle.json
//   node tests/oracle.mjs record --legacy --only gems --src <dir> --source-rev <sha> --out tests/behaviour-oracle-gems.json
//   node tests/oracle.mjs record --legacy --only primeevil --src <dir> --source-rev <sha> --out tests/behaviour-oracle-primeevil.json
//   node tests/oracle.mjs replay --oracle tests/behaviour-oracle.json [--derived tests/behaviour-oracle-derived.json]
//                               [--supplement tests/behaviour-oracle-gems.json]
//                               [--key-supplement tests/behaviour-oracle-primeevil.json] [--legacy]
//
// `--derived` adds the steps tests/oracle-derive.mjs generates for the controls
// the legacy page never had (slider switches, the Enabled mods list's Turn off
// buttons, the theme): they run on a second fresh sandbox after the legacy
// steps, and coverage counts both files' controls.
//
// `--supplement` adds a recording of controls the legacy page gained after
// behaviour-oracle.json was recorded (Gems of Incarnation, from origin/main's
// last pre-port page): its steps run on a third fresh sandbox, compared exactly
// as the legacy steps are, and coverage counts its `controls` too. Its two
// navigation steps (`tab:mods`, `subtab:qol`) are replayed as `tab:loot`,
// where the Gems controls now sit, through tests/lib/oracle-relocate.mjs's
// SUPPLEMENT_RELOCATION; the file itself is never edited, and the legacy
// recording and the recorder are never relocated. It is
// recorded by `record --only gems`, a fixed scenario rather than the walk,
// from the tree `--src` names (a `git archive <sha> src` extracted outside the
// checkout, served with `--legacy`), and it names that tree as `sourceRev`.
// The recorder refuses when the page's enumerated gem controls are not
// exactly GEM_CONTROLS, and it never writes behaviour-oracle.json.
//
// `--key-supplement` adds a recording of a key slider a later origin/main
// added to KEYS (Prime Evil Parts, from 1.4.7's page at 841c2db): its steps run
// on a fourth fresh sandbox, compared exactly as recorded (no relocation, no
// transform), and coverage counts its `controls`. It is recorded by `record
// --only primeevil`, a fixed scenario (the Loot tab, the slider's max, min,
// `+`, `-` and typed midpoint, then Apply all) from the tree `--src` names,
// and the recorder refuses unless that page's controls beyond the legacy and
// Gems recordings are exactly the one slider. Because the backend now sends
// that key's reset line in every full key reset, the legacy recording and the
// Gems supplement are replayed through tests/lib/oracle-relocate.mjs's
// insertAddedKeys, which inserts exactly that line and nothing else; neither
// file is edited, and the key supplement and the recorder never pass through
// it.
//
// `record` walks every control the page offers, tab by tab in document order,
// and writes one step per action: the POST requests the page made (`/api/state`
// polls are GETs and never counted) and the plugin command lines the sandbox
// server's `send_cmds` wrote. `replay` runs the same steps against the current
// build and compares both, step by step. Every live command is decided by
// src/forgepact.py from the POST bodies (the page never composes one), so
// equal POSTs and equal commands mean the port changed no behaviour.
//
// Actions: a checkbox is clicked twice (it ends where it started); a range is
// driven to its max, its min, one `+` and one `-` click, then its value is
// typed through the numeric editor; a select takes each option; a button is
// clicked. A disabled control is recorded as `skipped-disabled`, and replay
// must find it disabled too. Two scenarios are spelled out because a generic
// walk never reaches them: a parent switch's children are exercised while the
// parent is on (they are disabled while it is off), and the Satanic bulk
// buttons are exercised after one row on each side is switched off (they are
// disabled while every row is on).
//
// After each action the step waits for the page's own save indicator to settle
// (no class `saving`, no text "Saving...") - DOM state the page already shows;
// no hook is added to the product for this.

import { writeFileSync, readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import {
  PANEL_DIR, TABS, VIEWPORTS, launchBrowser, openPanel, parseArgs, startSandbox, waitSaved,
} from './lib/browser.mjs';
import { SUPPLEMENT_RELOCATION, insertAddedKeys, relocate } from './lib/oracle-relocate.mjs';

const DEPENDENTS = {
  map_reveal: ['map_reveal_packs', 'map_reveal_spawn'],
  mod_auto_prospect: ['mod_auto_prospect_bag'],
};
const CHILDREN = new Set(Object.values(DEPENDENTS).flat());
const SAT = (polarity, id) => `input[data-sat-polarity="${polarity}"][data-sat-id="${id}"]`;
const SAT_BULK = [
  SAT('buff', '1'), '#satbuffAll',
  SAT('debuff', '1'), '#satdebuffAll',
  SAT('buff', '1'), SAT('debuff', '1'), '#satRestore',
];

// Gems of Incarnation: the controls the page's walk enumerates (the filter's
// inner buttons and boxes carry no id), and the fixed scenario the supplement
// records. Both switches are off by default, so each is turned on before
// anything that needs it on, and both end off again. In between: the filter's
// client-side refusal (nothing ticked: a toast, no POST), a narrowed filter
// saved while Mythic is off, both switches on with it and Apply all, the
// filter widened back to every mod, and the list closed, reopened from the
// saved filter and closed.
const GEM_CONTROLS = ['#mod_gem_mythic', '#mod_gem_maxroll', '#gemfilter_toggle'];
const GF = (selector) => `#gemfilter_panel ${selector}`;
const GEMS_SCENARIO = [
  'tab:mods', 'subtab:qol',
  '#mod_gem_mythic', '#mod_gem_mythic',
  '#mod_gem_maxroll', '#mod_gem_maxroll',
  '#gemfilter_toggle',
  GF('[data-gf="none"]'), GF('[data-gf="save"]'),
  GF('[data-gfcat="Loot"][data-gfset="1"]'), GF('input[data-gfstat="68"]'), GF('[data-gf="save"]'),
  '#mod_gem_mythic', '#mod_gem_maxroll', '#applyall',
  GF('[data-gfcat="Attack"][data-gfset="0"]'), GF('[data-gf="all"]'), GF('[data-gf="save"]'),
  '#gemfilter_toggle', '#gemfilter_toggle', '#gemfilter_toggle',
  '#mod_gem_mythic', '#mod_gem_maxroll',
].map((control) => ({ control, action: 'click' }));

// Prime Evil Parts (1.4.7): the one control main's page added beyond the
// legacy and Gems recordings, and its fixed scenario - the walk's own range
// actions on the Loot tab (the typed value is the walk's midpoint), then Apply
// all, so one step compares the full command list with the slider left raised.
const PRIMEEVIL_RANGE = 'input[type=range][data-sec="keys"][data-key="primeevil"]';
const PRIMEEVIL_SCENARIO = (typed) => [
  { control: 'tab:loot', action: 'click' },
  { control: PRIMEEVIL_RANGE, action: 'max' },
  { control: PRIMEEVIL_RANGE, action: 'min' },
  { control: PRIMEEVIL_RANGE, action: 'increment' },
  { control: PRIMEEVIL_RANGE, action: 'decrement' },
  { control: PRIMEEVIL_RANGE, action: 'type', value: typed },
  { control: '#applyall', action: 'click' },
];

// Runs in the page: every control under `root`, in document order.
function enumerateIn(rootSelector) {
  const out = [];
  const roots = [...document.querySelectorAll(rootSelector)];
  for (const root of roots) {
    for (const el of root.querySelectorAll('input[type=checkbox], input[type=range], select, button[id]')) {
      if (el.matches('input[type=checkbox]')) {
        if (el.dataset.satPolarity) out.push({ kind: 'checkbox', selector: `input[data-sat-polarity="${el.dataset.satPolarity}"][data-sat-id="${el.dataset.satId}"]` });
        else if (el.id) out.push({ kind: 'checkbox', selector: '#' + el.id });
      } else if (el.matches('input[type=range]')) {
        const selector = el.dataset.sec
          ? `input[type=range][data-sec="${el.dataset.sec}"][data-key="${el.dataset.key}"]`
          : '#' + el.id;
        out.push({ kind: 'range', selector, min: +el.min, max: +el.max });
      } else if (el.matches('select')) {
        out.push({ kind: 'select', selector: '#' + el.id, options: [...el.options].map((o) => o.value) });
      } else {
        out.push({ kind: 'button', selector: '#' + el.id });
      }
    }
  }
  return out;
}

// The step list, before anything is clicked: the plan `record` then executes.
async function planSteps(page) {
  const plan = [];
  const push = (control, action, extra = {}) => plan.push({ control, action, ...extra });
  // The page header's controls, then the status bar's (the legacy page had
  // none there; the theme choice, once in it, is on the Setup tab now).
  const header = [...await page.evaluate(enumerateIn, '.page-actions'), ...await page.evaluate(enumerateIn, '#statusbar')];
  const groups = [];
  for (const tab of TABS) {
    if (tab === 'mods') {
      for (const sub of ['qol', 'items']) {
        groups.push({ tab, sub, controls: await page.evaluate(enumerateIn, `#${sub}Card`) });
      }
    } else {
      groups.push({ tab, controls: await page.evaluate(enumerateIn, `.tab-card[data-tab="${tab}"]`) });
    }
  }
  const walk = (controls) => {
    for (const c of controls) {
      if (CHILDREN.has(c.selector.slice(1)) || c.selector === '#satbuffAll' || c.selector === '#satdebuffAll' || c.selector === '#satRestore') continue;
      if (c.kind === 'checkbox') {
        push(c.selector, 'click');
        const children = DEPENDENTS[c.selector.slice(1)];
        if (children) {
          // First click turned the parent on (it is off by default in the
          // sandbox): walk the children now, then again once it is off.
          for (const child of children) { push('#' + child, 'click'); push('#' + child, 'click'); }
          push(c.selector, 'click');
          for (const child of children) push('#' + child, 'click');
        } else {
          push(c.selector, 'click');
        }
      } else if (c.kind === 'range') {
        push(c.selector, 'max');
        push(c.selector, 'min');
        push(c.selector, 'increment');
        push(c.selector, 'decrement');
        push(c.selector, 'type', { value: String(+(c.min + (c.max - c.min) / 2).toFixed(2)) });
      } else if (c.kind === 'select') {
        for (const option of c.options) push(c.selector, 'select', { value: option });
      } else if (c.selector === '#exesave') {
        push('#exepath', 'type-exe-path');
        push(c.selector, 'click');
      } else {
        push(c.selector, 'click');
      }
    }
  };
  walk(header);
  for (const group of groups) {
    const first = !plan.some((p) => p.control === `tab:${group.tab}`);
    if (first) push(`tab:${group.tab}`, 'click');
    if (group.sub) push(`subtab:${group.sub}`, 'click');
    walk(group.controls);
    if (group.controls.some((c) => c.selector === '#satRestore')) for (const s of SAT_BULK) push(s, 'click');
    if (group.tab === 'loot' || group.tab === 'modifiers') {
      push('#controlSearch', 'search', { value: 'x' });
      push('#controlSearch', 'search', { value: '' });
      push('[data-control-filter="modified"]', 'click');
      push('[data-control-filter="all"]', 'click');
    }
  }
  // Last: Apply all now sends build_cmds() of whatever the walk left saved
  // (every range ends on its typed value), so one step compares the whole
  // resulting config's command list.
  push('#applyall', 'click');
  const controls = [...header, ...groups.flatMap((g) => g.controls)].map((c) => c.selector);
  return { plan, controls };
}

function locate(page, step) {
  const { control } = step;
  if (control.startsWith('tab:')) return page.locator(`.tabbtn[data-tab="${control.slice(4)}"]`);
  if (control.startsWith('subtab:')) return page.locator(`#subtab-${control.slice(7)}`);
  return page.locator(control);
}

// The element an action lands on: a range's `+`/`-` or its value box, else
// the control itself.
async function target(page, step) {
  const base = locate(page, step);
  if (await base.count() !== 1) return null;
  if (step.action === 'increment' || step.action === 'decrement') {
    const buttons = base.locator('xpath=ancestor::*[contains(concat(" ",normalize-space(@class)," ")," range-control ")][1]').locator('.step-button');
    return buttons.nth(step.action === 'increment' ? 1 : 0);
  }
  if (step.action === 'type') {
    return base.locator('xpath=ancestor::*[contains(concat(" ",normalize-space(@class)," ")," row ")][1]').locator('.val').first();
  }
  return base;
}

async function act(page, step, sandbox) {
  const el = await target(page, step);
  if (!el) return 'missing';
  if (['click', 'increment', 'decrement', 'max', 'min', 'select'].includes(step.action) && await el.isDisabled()) return 'skipped-disabled';
  switch (step.action) {
    case 'click':
    case 'increment':
    case 'decrement':
      await el.click();
      break;
    case 'max':
    case 'min':
      await el.evaluate((r, which) => {
        r.value = which === 'max' ? r.max : r.min;
        r.dispatchEvent(new Event('input', { bubbles: true }));
        r.dispatchEvent(new Event('change', { bubbles: true }));
      }, step.action);
      break;
    case 'type':
      await el.click();
      await el.locator('.numedit').fill(step.value);
      await el.locator('.numedit').press('Enter');
      break;
    case 'select':
      await el.selectOption(step.value);
      break;
    case 'type-exe-path':
      await el.fill(`${sandbox.root}\\Hero_Siege.exe`);
      break;
    case 'search':
      await el.fill(step.value);
      break;
    default:
      throw new Error('unknown action ' + step.action);
  }
  return 'done';
}

function normalise(text, sandbox) {
  if (typeof text !== 'string') return text;
  const raw = sandbox.root;
  const escaped = JSON.stringify(raw).slice(1, -1);
  return text.split(escaped).join('<sandbox>').split(raw).join('<sandbox>');
}

// `onStep` also gets `requests`: how each of the step's POSTs ended, as text.
// It is never compared with a recording, only reported with a mismatch or a
// timeout, so a step that captured no commands says why: the POST failed on
// the way (Chromium's net:: error), the server answered an error status, or
// no answer came at all.
async function runSteps(page, sandbox, plan, onStep) {
  let posts = [];
  let requests = [];
  let inflight = 0;
  page.on('request', (req) => {
    if (req.method() !== 'POST') return;
    inflight++;
    const url = new URL(req.url()).pathname;
    const raw = normalise(req.postData() ?? '', sandbox);
    let body = raw;
    try { body = JSON.parse(raw); } catch { /* keep the text */ }
    posts.push({ url, body });
    requests.push({ req, url, at: Date.now() });
  });
  const entry = (req) => requests.find((r) => r.req === req);
  page.on('response', (res) => { const r = entry(res.request()); if (r) r.status = res.status(); });
  const done = (req, failed) => {
    if (req.method() !== 'POST') return;
    inflight--;
    const r = entry(req);
    if (!r) return;
    r.ms = Date.now() - r.at;
    if (failed) r.failed = req.failure()?.errorText || 'failed';
    else if (r.status >= 400) req.response().then((res) => res?.text()).then((text) => { r.body = text?.slice(0, 300); }, () => {});
  };
  page.on('requestfinished', (req) => done(req, false));
  page.on('requestfailed', (req) => done(req, true));
  const outcomes = () => requests.map((r) => {
    if (r.failed) return `${r.url} failed after ${r.ms} ms: ${r.failed}`;
    if (r.ms === undefined) return `${r.url} unanswered after ${Date.now() - r.at} ms`;
    return `${r.url} answered ${r.status} in ${r.ms} ms${r.body ? `: ${r.body}` : ''}`;
  });
  const settle = async (i, step) => {
    await page.waitForTimeout(30);
    for (let round = 0; round < 3; round++) {
      try {
        await waitSaved(page);
      } catch (e) {
        throw new Error(`step ${i} (${step.control} ${step.action}): the save did not end (${e.message.split('\n')[0]}); `
          + `its POSTs: ${outcomes().join('; ') || 'none'}; ${sandbox.describe()}`, { cause: e });
      }
      const end = Date.now() + 10000;
      while (inflight > 0 && Date.now() < end) await page.waitForTimeout(10);
      await page.waitForTimeout(60);
    }
  };
  for (let i = 0; i < plan.length; i++) {
    const step = plan[i];
    sandbox.truncateCmds();
    posts = [];
    requests = [];
    const outcome = await act(page, step, sandbox);
    await settle(i, step);
    const cmds = sandbox.readCmds().map((line) => normalise(line, sandbox));
    await onStep(i, step, outcome, posts, cmds, outcomes());
  }
}

// Order-insensitive for object keys (a body is a JSON object; its key order
// is not behaviour), order-sensitive for arrays.
function canonical(value) {
  if (Array.isArray(value)) return value.map(canonical);
  if (value && typeof value === 'object') {
    return Object.fromEntries(Object.keys(value).sort().map((k) => [k, canonical(value[k])]));
  }
  return value;
}
const same = (a, b) => JSON.stringify(canonical(a)) === JSON.stringify(canonical(b));

// Runs `plan` and returns one recorded step per action.
async function recordSteps(page, sandbox, plan) {
  const steps = [];
  await runSteps(page, sandbox, plan, (i, step, outcome, posts, cmds) => {
    if (outcome === 'missing') throw new Error(`step ${i}: ${step.control} not found while recording`);
    const entry = { step: i, control: step.control, action: outcome === 'skipped-disabled' ? 'skipped-disabled' : step.action };
    if (outcome === 'skipped-disabled') entry.intended = step.action;
    if (step.value !== undefined) entry.value = step.value;
    entry.posts = posts;
    entry.cmds = cmds;
    steps.push(entry);
  });
  return steps;
}

function reportRecorded(steps, out) {
  const posted = steps.filter((s) => s.posts.length).length;
  console.log(`oracle: recorded ${steps.length} steps (${posted} with POSTs, ${steps.filter((s) => s.action === 'skipped-disabled').length} skipped-disabled) to ${out}`);
}

async function record(args) {
  if (args.only === 'primeevil') return recordPrimeEvil(args);
  if (args.only !== undefined) return recordGems(args);
  const out = resolve(PANEL_DIR, args.out || 'tests/behaviour-oracle.json');
  const legacy = !!args.legacy;
  const viewport = VIEWPORTS[1280];
  const sandbox = await startSandbox({ legacy, dist: args.dist });
  const browser = await launchBrowser();
  try {
    const page = await openPanel(browser, sandbox, viewport);
    const { plan, controls } = await planSteps(page);
    const steps = await recordSteps(page, sandbox, plan);
    const oracle = {
      recordedFrom: legacy ? 'legacy' : 'build',
      recordedAt: new Date().toISOString(),
      viewport,
      controls,
      steps,
    };
    writeFileSync(out, JSON.stringify(oracle, null, 1) + '\n');
    reportRecorded(steps, out);
  } finally {
    await browser.close();
    await sandbox.stop();
  }
}

// The Gems of Incarnation supplement: GEMS_SCENARIO on the legacy page of the
// tree `--src` names, at that tree's product defaults.
async function recordGems(args) {
  const usage = 'record --only gems needs --legacy, --src <dir>, --source-rev <40-hex sha> and --out <file>';
  if (args.only !== 'gems' || !args.legacy || typeof args.src !== 'string' || typeof args.out !== 'string' ||
      !/^[0-9a-f]{40}$/.test(String(args['source-rev']))) throw new Error(usage);
  const out = resolve(PANEL_DIR, args.out);
  if (out === resolve(PANEL_DIR, 'tests/behaviour-oracle.json')) throw new Error('record --only gems never writes tests/behaviour-oracle.json');
  const viewport = VIEWPORTS[1280];
  const sandbox = await startSandbox({ legacy: true, src: resolve(args.src) });
  const browser = await launchBrowser();
  try {
    const page = await openPanel(browser, sandbox, viewport);
    // Every control that page enumerates beyond the legacy recording's must be
    // exactly GEM_CONTROLS: one added on that tree would otherwise go
    // unrecorded. (A plain "contains gem" filter would also catch the Boss
    // Gems key slider, which the legacy recording already covers.)
    const legacy = new Set(JSON.parse(readFileSync(resolve(PANEL_DIR, 'tests/behaviour-oracle.json'), 'utf8')).controls);
    const added = (await planSteps(page)).controls.filter((c) => !legacy.has(c));
    if (JSON.stringify(added) !== JSON.stringify(GEM_CONTROLS)) {
      throw new Error(`the page's controls beyond the legacy recording are ${JSON.stringify(added)}, not ${JSON.stringify(GEM_CONTROLS)}`);
    }
    const steps = await recordSteps(page, sandbox, GEMS_SCENARIO);
    const oracle = {
      recordedFrom: 'legacy',
      sourceRev: args['source-rev'],
      recordedAt: new Date().toISOString(),
      viewport,
      controls: GEM_CONTROLS,
      steps,
    };
    writeFileSync(out, JSON.stringify(oracle, null, 1) + '\n');
    reportRecorded(steps, out);
  } finally {
    await browser.close();
    await sandbox.stop();
  }
}

// The Prime Evil Parts supplement: PRIMEEVIL_SCENARIO on the legacy page of
// the tree `--src` names (origin/main at 1.4.7), at that tree's product
// defaults. Never through insertAddedKeys: this page already sends the key.
async function recordPrimeEvil(args) {
  const usage = 'record --only primeevil needs --legacy, --src <dir>, --source-rev <40-hex sha> and --out <file>';
  if (!args.legacy || typeof args.src !== 'string' || typeof args.out !== 'string' ||
      !/^[0-9a-f]{40}$/.test(String(args['source-rev']))) throw new Error(usage);
  const out = resolve(PANEL_DIR, args.out);
  for (const kept of ['tests/behaviour-oracle.json', 'tests/behaviour-oracle-gems.json']) {
    if (out === resolve(PANEL_DIR, kept)) throw new Error(`record --only primeevil never writes ${kept}`);
  }
  const viewport = VIEWPORTS[1280];
  const sandbox = await startSandbox({ legacy: true, src: resolve(args.src) });
  const browser = await launchBrowser();
  try {
    const page = await openPanel(browser, sandbox, viewport);
    // Every control that page enumerates beyond the legacy and Gems recordings
    // must be exactly the Prime Evil Parts slider: one more added on that tree
    // would otherwise go unrecorded.
    const known = new Set(['tests/behaviour-oracle.json', 'tests/behaviour-oracle-gems.json']
      .flatMap((f) => JSON.parse(readFileSync(resolve(PANEL_DIR, f), 'utf8')).controls));
    const { plan, controls } = await planSteps(page);
    const added = controls.filter((c) => !known.has(c));
    if (JSON.stringify(added) !== JSON.stringify([PRIMEEVIL_RANGE])) {
      throw new Error(`the page's controls beyond the legacy and Gems recordings are ${JSON.stringify(added)}, not ${JSON.stringify([PRIMEEVIL_RANGE])}`);
    }
    const typed = plan.find((p) => p.control === PRIMEEVIL_RANGE && p.action === 'type').value;
    const steps = await recordSteps(page, sandbox, PRIMEEVIL_SCENARIO(typed));
    const oracle = {
      recordedFrom: 'legacy',
      sourceRev: args['source-rev'],
      recordedAt: new Date().toISOString(),
      viewport,
      controls: [PRIMEEVIL_RANGE],
      steps,
    };
    writeFileSync(out, JSON.stringify(oracle, null, 1) + '\n');
    reportRecorded(steps, out);
  } finally {
    await browser.close();
    await sandbox.stop();
  }
}

// The derived oracle's steps (tests/oracle-derive.mjs), on a sandbox of their
// own: each step's capture is kept by index, and its `expect` compares it with
// an earlier step's capture (`same`) or with a literal (`is`). A reference
// that captured nothing is a mismatch in itself - a comparison of two empty
// captures would pass without the instrument ever having fired.
async function replayDerived(browser, derived, viewport, args, mismatches) {
  const sandbox = await startSandbox({ legacy: !!args.legacy, dist: args.dist });
  try {
    const page = await openPanel(browser, sandbox, viewport);
    const captured = [];
    const plan = derived.steps.map((s) => ({ control: s.control, action: s.action, value: s.value }));
    await runSteps(page, sandbox, plan, (i, step, outcome, posts, cmds, requests) => {
      captured[i] = { posts, cmds, requests };
      const label = `derived ${i}`;
      if (outcome !== 'done') {
        mismatches.push({ step: label, control: step.control, problem: `expected done, got ${outcome}` });
        return;
      }
      const expect = derived.steps[i].expect || {};
      for (const field of ['posts', 'cmds']) {
        const want = expect[field];
        if (!want) continue;
        const actual = field === 'posts' ? posts : cmds;
        if ('same' in want) {
          const reference = captured[want.same];
          if (!reference?.[field]?.length) {
            mismatches.push({ step: label, control: step.control, action: step.action, problem: `${field}: reference sent nothing`, reference: want.same, referenceRequests: reference?.requests });
          } else if (!same(reference[field], actual)) {
            mismatches.push({ step: label, control: step.control, action: step.action, problem: field, expected: reference[field], expectedFrom: want.same, actual, requests });
          }
        } else if (!same(want.is, actual)) {
          mismatches.push({ step: label, control: step.control, action: step.action, problem: field, expected: want.is, actual, requests });
        }
      }
    });
  } finally {
    await sandbox.stop();
  }
}

// A recorded file's steps (the legacy recording, or the supplement) on `page`:
// each step's outcome, POST bodies and commands must equal the recording's.
async function replayRecorded(page, sandbox, oracle, mismatches, prefix = '') {
  const plan = oracle.steps.map((s) => ({ control: s.control, action: s.action === 'skipped-disabled' ? s.intended : s.action, value: s.value }));
  await runSteps(page, sandbox, plan, (i, step, outcome, posts, cmds, requests) => {
    const want = oracle.steps[i];
    const label = prefix ? `${prefix} ${i}` : i;
    const wantOutcome = want.action === 'skipped-disabled' ? 'skipped-disabled' : 'done';
    if (outcome !== wantOutcome) {
      mismatches.push({ step: label, control: step.control, problem: `expected ${wantOutcome}, got ${outcome}` });
      return;
    }
    if (!same(want.posts, posts)) mismatches.push({ step: label, control: step.control, action: step.action, problem: 'posts', expected: want.posts, actual: posts, requests });
    if (!same(want.cmds, cmds)) mismatches.push({ step: label, control: step.control, action: step.action, problem: 'cmds', expected: want.cmds, actual: cmds, requests });
  });
}

// The supplement's steps, on a sandbox of their own at the product defaults.
// The Gems controls moved from Mods › Quality of Life to the Loot tab after
// the supplement was recorded, so its two navigation steps are relocated
// (tests/lib/oracle-relocate.mjs: navigation only, and only steps that sent
// nothing); every other step, and every post and command, is compared as
// recorded.
async function replaySupplement(browser, supplement, args, mismatches) {
  const sandbox = await startSandbox({ legacy: !!args.legacy, dist: args.dist });
  try {
    const page = await openPanel(browser, sandbox, supplement.viewport);
    await replayRecorded(page, sandbox, relocate(insertAddedKeys(supplement), SUPPLEMENT_RELOCATION), mismatches, 'supplement');
  } finally {
    await sandbox.stop();
  }
}

// The key supplement's steps (a key slider a later main added), on a sandbox
// of their own at the product defaults, compared exactly as recorded: it was
// recorded after the key existed, so neither transform applies to it.
async function replayKeySupplement(browser, keySupplement, args, mismatches) {
  const sandbox = await startSandbox({ legacy: !!args.legacy, dist: args.dist });
  try {
    const page = await openPanel(browser, sandbox, keySupplement.viewport);
    await replayRecorded(page, sandbox, keySupplement, mismatches, 'key-supplement');
  } finally {
    await sandbox.stop();
  }
}

async function replay(args) {
  // The legacy recording predates keys a later main added to KEYS: its full
  // key resets gain exactly those reset lines (insertAddedKeys) and nothing
  // else; the file itself is never edited.
  const oracle = insertAddedKeys(JSON.parse(readFileSync(resolve(PANEL_DIR, args.oracle || 'tests/behaviour-oracle.json'), 'utf8')));
  const derived = args.derived ? JSON.parse(readFileSync(resolve(PANEL_DIR, args.derived), 'utf8')) : null;
  const supplement = args.supplement ? JSON.parse(readFileSync(resolve(PANEL_DIR, args.supplement), 'utf8')) : null;
  const keySupplement = args['key-supplement'] ? JSON.parse(readFileSync(resolve(PANEL_DIR, args['key-supplement']), 'utf8')) : null;
  const sandbox = await startSandbox({ legacy: !!args.legacy, dist: args.dist });
  const browser = await launchBrowser();
  const mismatches = [];
  let stopped = false;
  try {
    const page = await openPanel(browser, sandbox, oracle.viewport);
    // Coverage: a control the build offers that no oracle file ever exercised
    // is a behaviour nobody compared.
    const { controls } = await planSteps(page);
    const covered = [...oracle.controls, ...(derived ? derived.controls : []), ...(supplement ? supplement.controls : []),
      ...(keySupplement ? keySupplement.controls : [])];
    const recorded = new Set(covered);
    for (const c of controls) if (!recorded.has(c)) mismatches.push({ step: '-', control: c, problem: 'control not in the oracle' });
    for (const c of covered) if (!controls.includes(c)) mismatches.push({ step: '-', control: c, problem: 'control missing from this build' });
    await replayRecorded(page, sandbox, oracle, mismatches);
    await page.context().close();
    stopped = true;
    await sandbox.stop();
    if (derived) await replayDerived(browser, derived, oracle.viewport, args, mismatches);
    if (supplement) await replaySupplement(browser, supplement, args, mismatches);
    if (keySupplement) await replayKeySupplement(browser, keySupplement, args, mismatches);
  } finally {
    await browser.close();
    if (!stopped) await sandbox.stop();
  }
  for (const m of mismatches) console.log('mismatch', JSON.stringify(m));
  const total = oracle.steps.length + (derived ? derived.steps.length : 0) + (supplement ? supplement.steps.length : 0) +
    (keySupplement ? keySupplement.steps.length : 0);
  console.log(`oracle: ${total} steps, ${mismatches.length} mismatches`);
  return mismatches.length ? 1 : 0;
}

const args = parseArgs(process.argv.slice(2));
const mode = args._[0];
if (mode === 'record') await record(args);
else if (mode === 'replay') process.exitCode = await replay(args);
else {
  console.error('usage: oracle.mjs record [--legacy] [--out <file>] | record --legacy --only gems|primeevil --src <dir> --source-rev <sha> --out <file> | ' +
    'replay [--oracle <file>] [--derived <file>] [--supplement <file>] [--key-supplement <file>] [--legacy]');
  process.exitCode = 2;
}
