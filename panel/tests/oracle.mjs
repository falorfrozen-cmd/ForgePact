// The behaviour oracle: proof that a panel build sends exactly what the old
// page sent.
//
//   node tests/oracle.mjs record [--legacy] --out tests/behaviour-oracle.json
//   node tests/oracle.mjs replay --oracle tests/behaviour-oracle.json [--legacy]
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
  const header = await page.evaluate(enumerateIn, '.page-actions');
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

async function runSteps(page, sandbox, plan, onStep) {
  let posts = [];
  let inflight = 0;
  page.on('request', (req) => {
    if (req.method() !== 'POST') return;
    inflight++;
    const url = new URL(req.url()).pathname;
    const raw = normalise(req.postData() ?? '', sandbox);
    let body = raw;
    try { body = JSON.parse(raw); } catch { /* keep the text */ }
    posts.push({ url, body });
  });
  const done = (req) => { if (req.method() === 'POST') inflight--; };
  page.on('requestfinished', done);
  page.on('requestfailed', done);
  const settle = async () => {
    await page.waitForTimeout(30);
    for (let round = 0; round < 3; round++) {
      await waitSaved(page);
      const end = Date.now() + 10000;
      while (inflight > 0 && Date.now() < end) await page.waitForTimeout(10);
      await page.waitForTimeout(60);
    }
  };
  for (let i = 0; i < plan.length; i++) {
    const step = plan[i];
    sandbox.truncateCmds();
    posts = [];
    const outcome = await act(page, step, sandbox);
    await settle();
    const cmds = sandbox.readCmds().map((line) => normalise(line, sandbox));
    await onStep(i, step, outcome, posts, cmds);
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

async function record(args) {
  const out = resolve(PANEL_DIR, args.out || 'tests/behaviour-oracle.json');
  const legacy = !!args.legacy;
  const viewport = VIEWPORTS[1280];
  const sandbox = await startSandbox({ legacy, dist: args.dist });
  const browser = await launchBrowser();
  try {
    const page = await openPanel(browser, sandbox, viewport);
    const { plan, controls } = await planSteps(page);
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
    const oracle = {
      recordedFrom: legacy ? 'legacy' : 'build',
      recordedAt: new Date().toISOString(),
      viewport,
      controls,
      steps,
    };
    writeFileSync(out, JSON.stringify(oracle, null, 1) + '\n');
    const posted = steps.filter((s) => s.posts.length).length;
    console.log(`oracle: recorded ${steps.length} steps (${posted} with POSTs, ${steps.filter((s) => s.action === 'skipped-disabled').length} skipped-disabled) to ${out}`);
  } finally {
    await browser.close();
    await sandbox.stop();
  }
}

async function replay(args) {
  const oracle = JSON.parse(readFileSync(resolve(PANEL_DIR, args.oracle || 'tests/behaviour-oracle.json'), 'utf8'));
  const sandbox = await startSandbox({ legacy: !!args.legacy, dist: args.dist });
  const browser = await launchBrowser();
  const mismatches = [];
  try {
    const page = await openPanel(browser, sandbox, oracle.viewport);
    // Coverage: a control the build offers that the oracle never exercised is
    // a behaviour nobody compared.
    const { controls } = await planSteps(page);
    const recorded = new Set(oracle.controls);
    for (const c of controls) if (!recorded.has(c)) mismatches.push({ step: '-', control: c, problem: 'control not in the oracle' });
    for (const c of oracle.controls) if (!controls.includes(c)) mismatches.push({ step: '-', control: c, problem: 'control missing from this build' });
    const plan = oracle.steps.map((s) => ({ control: s.control, action: s.action === 'skipped-disabled' ? s.intended : s.action, value: s.value }));
    await runSteps(page, sandbox, plan, (i, step, outcome, posts, cmds) => {
      const want = oracle.steps[i];
      const wantOutcome = want.action === 'skipped-disabled' ? 'skipped-disabled' : 'done';
      if (outcome !== wantOutcome) {
        mismatches.push({ step: i, control: step.control, problem: `expected ${wantOutcome}, got ${outcome}` });
        return;
      }
      if (!same(want.posts, posts)) mismatches.push({ step: i, control: step.control, action: step.action, problem: 'posts', expected: want.posts, actual: posts });
      if (!same(want.cmds, cmds)) mismatches.push({ step: i, control: step.control, action: step.action, problem: 'cmds', expected: want.cmds, actual: cmds });
    });
  } finally {
    await browser.close();
    await sandbox.stop();
  }
  for (const m of mismatches) console.log('mismatch', JSON.stringify(m));
  console.log(`oracle: ${oracle.steps.length} steps, ${mismatches.length} mismatches`);
  return mismatches.length ? 1 : 0;
}

const args = parseArgs(process.argv.slice(2));
const mode = args._[0];
if (mode === 'record') await record(args);
else if (mode === 'replay') process.exitCode = await replay(args);
else {
  console.error('usage: oracle.mjs record [--legacy] [--out <file>] | replay [--oracle <file>] [--legacy]');
  process.exitCode = 2;
}
