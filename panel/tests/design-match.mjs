// Does the built panel match the Figma export? Four checks, run against
// panel/dist through the sandbox server and the installed Edge:
//
//   node tests/design-match.mjs --export <export.json> --figma-dir <dir> --out <dir>
//                               [--checks texts,tokens,flatness,composites]
//
// texts       for each `screens[]` entry, the tab (and sub-tab: `mods-items`
//             is the mods tab plus #subtab-items) at that width; every
//             `texts` entry must be a substring of the page's innerText,
//             whitespace and case normalised.
// tokens      for each `selectorTokens[]` entry, the first element the
//             selector matches and its computed property, against the
//             variable's value - in the default palette as the page painted
//             it, then in each other palette with data-theme set to its name
//             (no POST; the attribute alone swaps the blocks), against that
//             palette's colours and the default palette's other values. See
//             lib/design-tokens.mjs for how values are compared.
// flatness    lib/design-flatness.mjs on every screen, with the export's
//             `borders` as the allowlist.
// composites  the Figma image (<figma-dir>/screens/<tab>-<w>.png) left and
//             the fresh render right, on one canvas as tall as the taller,
//             written to <out>/compare/<tab>-<w>.png.
//
// The design state. A screen may carry `state: {"game": "offline"|"running",
// "config": {<forgepact.json key>: <value>, ...}}`, the state its Figma frame
// draws. Screens are grouped by that state (its canonical JSON); each group
// gets one sandbox, started with --offline when `game` is "offline" and
// seeded with `config` (panel_sandbox_server.py --seed, which refuses a key
// forgepact.DEFAULTS lacks with exit 2 and names it on stderr). A screen
// without `state` is measured on today's sandbox: game running, nothing on.
// A `state` that is not an object, a `game` that is neither value or a
// `config` that is not an object is an error line naming the screen, and
// fails the run; so does a sandbox that will not start.
//
// Some selectors exist only in some states. A selector is checked in the
// first of these where it matches: the page as loaded (the unseeded sandbox
// starts with nothing on); after turning one mod on through the page's own
// switch (the sandbox's config is a temp file, the real forgepact.json is
// never touched); then, for a selector that is an existing element plus a
// trailing .class or [attr] qualifier (`#saveIndicator.error`), with that
// qualifier put on the element for the measurement and taken off after;
// then as loaded on each screen state's sandbox, in export order (the row's
// state names the first screen carrying it). A selector no state reaches is
// `not found`, a mismatch in every palette.
//
// Variants. When the export lists a variant of a selector S - S followed by
// .class / [attr] / [attr=value] qualifiers only (`.val.off` of `.val`) - S
// is measured on the first element that matches S and none of its listed
// variants, since S's token describes the element in none of those states;
// if no element does in a state, S falls through to the next state. With no
// variant listed, S is measured on its first match. lib/design-tokens.mjs
// holds the rule (variantsOf) and tests/tokens-selftest.mjs proves it.
//
// Summary lines, last: `texts: <k> missing`, `tokens: <k> mismatched`,
// `tokens[<palette>]: <k> mismatched` per other palette in export order,
// `flatness: <k> violations`, `composites: <n> written, <m> without a Figma
// image`; a check left out of --checks prints `<name>: skipped` instead.
// Exits 1 if any k or m is above 0, or a requested check has nothing to
// check. <out>/design-match.json holds every row. Nothing here knows a
// palette by name.

import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join } from 'node:path';
import { PNG } from 'pngjs';
import { BOOLEAN_MODS } from '../src/enabled-mods.js';
import { defaultPalette, here, loadExport } from '../scripts/tokens-from-export.mjs';
import { TABS, VIEWPORTS, launchBrowser, openPanel, openTab, parseArgs, startSandbox, waitSaved } from './lib/browser.mjs';
import { flatness } from './lib/design-flatness.mjs';
import { compareToken, expectedCss, measureTokens, variantsOf } from './lib/design-tokens.mjs';

const ALL_CHECKS = ['texts', 'tokens', 'flatness', 'composites'];
const USAGE = 'usage: design-match.mjs --export <export.json> --figma-dir <dir> --out <dir> [--checks texts,tokens,flatness,composites]';

const norm = (s) => String(s).toLowerCase().replace(/\s+/g, ' ').trim();
const viewportFor = (width) => VIEWPORTS[width] || { width: Number(width), height: 800 };
const screenName = (screen) => `${screen.tab}-${screen.width}`;
const isObject = (v) => v !== null && typeof v === 'object' && !Array.isArray(v);

// JSON with every object's keys sorted, so two equal states group together.
function canonical(v) {
  if (Array.isArray(v)) return `[${v.map(canonical).join(',')}]`;
  if (isObject(v)) return `{${Object.keys(v).sort().map((k) => `${JSON.stringify(k)}:${canonical(v[k])}`).join(',')}}`;
  return JSON.stringify(v);
}

// Why a screen's `state` is unusable, or null when it is absent or valid.
function stateError(screen) {
  if (screen.state === undefined) return null;
  const s = screen.state;
  if (!isObject(s)) return 'state is not an object';
  if (s.game !== 'offline' && s.game !== 'running') return `state.game ${JSON.stringify(s.game)} is neither "offline" nor "running"`;
  if (!isObject(s.config)) return 'state.config is not an object';
  return null;
}

// One sandbox per distinct state, started on first use and kept until
// stopAll(); `undefined` is the unseeded sandbox. A start that fails is
// remembered, so every screen of that state reports it without a retry.
function sandboxPool() {
  const pool = new Map();
  return {
    get(state) {
      const key = state === undefined ? '' : canonical(state);
      if (!pool.has(key)) {
        const started = state === undefined ? startSandbox() : startSandbox({ offline: state.game === 'offline', seed: state.config });
        started.catch(() => {});
        pool.set(key, started);
      }
      return pool.get(key);
    },
    async stopAll() {
      for (const started of pool.values()) {
        const sandbox = await started.catch(() => null);
        if (sandbox) await sandbox.stop();
      }
    },
  };
}

async function showScreen(page, tab) {
  if (TABS.includes(tab)) return openTab(page, tab);
  const m = /^([a-z]+)-(.+)$/.exec(tab);
  if (!m || !TABS.includes(m[1])) throw new Error(`screen tab ${JSON.stringify(tab)} is not a tab or <tab>-<sub-tab>`);
  await openTab(page, m[1]);
  await page.click(`#subtab-${m[2]}`);
}

// Pointer parked, writes drained, fonts in, the column balancer settled.
async function settle(page) {
  await page.mouse.move(0, 0);
  await waitSaved(page);
  await page.evaluate(() => document.fonts.ready);
  await page.waitForTimeout(400);
}

function sideBySide(left, right) {
  const gap = 8;
  const out = new PNG({ width: left.width + gap + right.width, height: Math.max(left.height, right.height) });
  for (let i = 0; i < out.data.length; i += 4) out.data.set([128, 128, 128, 255], i);
  PNG.bitblt(left, out, 0, 0, left.width, left.height, 0, 0);
  PNG.bitblt(right, out, 0, 0, right.width, right.height, left.width + gap, 0);
  return out;
}

async function screenChecks(browser, sandboxes, exp, checks, figmaDir, outDir, report) {
  const screens = Array.isArray(exp.screens) ? exp.screens : [];
  if (screens.length === 0) throw new Error('the export lists no screens');
  for (const screen of screens) {
    const name = screenName(screen);
    if (stateError(screen)) continue; // reported once, by main
    let sandbox;
    try {
      sandbox = await sandboxes.get(screen.state);
    } catch (e) {
      report.errors.push(`screen ${name}: no sandbox for its state: ${e.message}`);
      continue;
    }
    const page = await openPanel(browser, sandbox, viewportFor(screen.width));
    try {
      await showScreen(page, screen.tab);
      await settle(page);
      if (checks.has('texts')) {
        const text = norm(await page.evaluate(() => document.body.innerText));
        for (const want of screen.texts || []) {
          const ok = text.includes(norm(want));
          report.texts.push({ screen: name, text: want, ok });
          if (!ok) console.log(`texts ${name}: missing ${JSON.stringify(want)}`);
        }
      }
      if (checks.has('flatness')) {
        const r = await page.evaluate(flatness, exp.borders || []);
        report.flatness.push({ screen: name, ...r });
        for (const v of r.violations) console.log(`flatness ${name}: ${v.rule} ${v.element}`);
        for (const s of r.invalid) console.log(`flatness ${name}: borders selector ${JSON.stringify(s)} is not valid CSS`);
      }
      if (checks.has('composites')) {
        const figma = join(figmaDir, 'screens', `${name}.png`);
        if (!existsSync(figma)) {
          console.log(`composites ${name}: no Figma image at ${figma}`);
          report.composites.push({ screen: name, file: null });
        } else {
          const render = PNG.sync.read(await page.screenshot({ fullPage: true, animations: 'disabled', caret: 'hide' }));
          const file = join(outDir, 'compare', `${name}.png`);
          writeFileSync(file, PNG.sync.write(sideBySide(PNG.sync.read(readFileSync(figma)), render)));
          report.composites.push({ screen: name, file });
        }
      }
    } finally {
      await page.context().close();
    }
  }
}

async function tokenChecks(browser, sandboxes, exp, report) {
  const entries = Array.isArray(exp.selectorTokens) ? exp.selectorTokens : [];
  if (entries.length === 0) throw new Error('the export lists no selectorTokens');
  const base = defaultPalette(exp);
  const palettes = [base, ...exp.palettes.filter((p) => p !== base)];
  const themes = [null, ...palettes.slice(1).map((p) => p.name)];
  const wants = expectedCss(exp, entries, palettes.map((p) => p.name));
  const selectors = entries.map((e) => e.selector);
  const variants = entries.map((e) => variantsOf(e.selector, selectors));
  const widest = Math.max(...(exp.screens || []).map((s) => Number(s.width)).filter(Number.isFinite), 1280);
  const found = new Array(entries.length).fill(null);
  const pending = () => entries.map((e, i) => i).filter((i) => !found[i]);
  const measureOn = async (page, state, qualify) => {
    const idx = pending();
    if (!idx.length) return;
    const list = idx.map((i) => ({ selector: entries[i].selector, property: entries[i].property, css: wants[i].map((w) => w.css ?? null), variants: variants[i] }));
    const got = await page.evaluate(measureTokens, { list, themes, qualify });
    got.forEach((r, k) => { if (r) found[idx[k]] = { ...r, state: r.qualifier ? `${state} ${r.qualifier}` : state }; });
  };
  const page = await openPanel(browser, await sandboxes.get(undefined), viewportFor(widest));
  try {
    await settle(page);
    const measure = (state, qualify) => measureOn(page, state, qualify);
    await measure('as loaded', false);
    if (pending().length) {
      // One mod on, through its own switch: the Enabled mods list then shows
      // an entry and its Turn off button.
      const turned = await page.evaluate((ids) => {
        for (const id of ids) {
          const el = document.getElementById(id);
          if (el && el.type === 'checkbox') { if (!el.checked) el.click(); return id; }
        }
        return null;
      }, BOOLEAN_MODS);
      if (turned) {
        const selectors = pending().map((i) => entries[i].selector);
        await page.waitForFunction((sels) => sels.some((s) => { try { return document.querySelector(s); } catch { return false; } }),
          selectors, { timeout: 3000 }).catch(() => {});
        await settle(page);
        await measure(`${turned} on`, false);
      }
    }
    await measure('qualifier', true);
  } finally {
    await page.context().close();
  }
  // The design states, after the three stages above and never instead of
  // them (`.enabled-mods-empty` exists only while nothing is on).
  const seen = new Set();
  for (const screen of exp.screens || []) {
    if (!pending().length) break;
    if (screen.state === undefined || stateError(screen)) continue;
    const key = canonical(screen.state);
    if (seen.has(key)) continue;
    seen.add(key);
    let sandbox;
    try {
      sandbox = await sandboxes.get(screen.state);
    } catch (e) {
      report.errors.push(`tokens: no sandbox for the state of ${screenName(screen)}: ${e.message}`);
      continue;
    }
    const seeded = await openPanel(browser, sandbox, viewportFor(widest));
    try {
      await settle(seeded);
      await measureOn(seeded, `as loaded, state of ${screenName(screen)}`, false);
    } finally {
      await seeded.context().close();
    }
  }
  palettes.forEach((palette, t) => {
    const rows = entries.map((entry, i) => {
      const f = found[i];
      if (!f) return { selector: entry.selector, property: entry.property, variable: entry.variable, expected: wants[i][t].css ?? '-', actual: 'selector not found', ok: false, state: '-' };
      const c = compareToken(entry, wants[i][t], f.actual[t], f.expected[t]);
      return { selector: entry.selector, property: entry.property, variable: entry.variable, expected: c.expected, actual: c.actual, ok: c.ok, why: c.why, state: f.state };
    });
    report.tokens.push({ palette: palette.name, default: t === 0, rows });
    console.log(`tokens[${palette.name}]${t === 0 ? ' (default, as painted)' : ''}: selector | property | expected | actual | ok | state`);
    for (const r of rows) console.log(`  ${r.selector} | ${r.property} | ${r.expected} | ${r.actual} | ${r.ok ? 'ok' : 'MISMATCH'} | ${r.state}`);
  });
}

async function main() {
  const args = parseArgs(process.argv.slice(2));
  if (typeof args.export !== 'string' || typeof args['figma-dir'] !== 'string' || typeof args.out !== 'string') {
    console.error(USAGE);
    return 2;
  }
  const checks = new Set(typeof args.checks === 'string' ? args.checks.split(',').map((s) => s.trim()).filter(Boolean) : ALL_CHECKS);
  const unknown = [...checks].filter((c) => !ALL_CHECKS.includes(c));
  if (unknown.length || checks.size === 0) {
    console.error(`design-match: unknown check(s) ${unknown.join(', ')}\n${USAGE}`);
    return 2;
  }
  let exp, base;
  try {
    exp = loadExport(here(args.export));
    base = defaultPalette(exp);
  } catch (e) {
    console.error(`design-match: cannot read ${args.export}: ${e.message}`);
    return 1;
  }
  const figmaDir = here(args['figma-dir']);
  const outDir = here(args.out);
  mkdirSync(join(outDir, 'compare'), { recursive: true });

  const report = { export: here(args.export), checks: [...checks], texts: [], tokens: [], flatness: [], composites: [], errors: [] };
  for (const screen of Array.isArray(exp.screens) ? exp.screens : []) {
    const why = stateError(screen);
    if (why) report.errors.push(`screen ${screenName(screen)}: ${why}`);
  }
  const sandboxes = sandboxPool();
  const browser = await launchBrowser();
  try {
    if (['texts', 'flatness', 'composites'].some((c) => checks.has(c))) {
      try { await screenChecks(browser, sandboxes, exp, checks, figmaDir, outDir, report); } catch (e) { report.errors.push(e.message); }
    }
    if (checks.has('tokens')) {
      try { await tokenChecks(browser, sandboxes, exp, report); } catch (e) { report.errors.push(e.message); }
    }
  } finally {
    await browser.close();
    await sandboxes.stopAll();
  }
  writeFileSync(join(outDir, 'design-match.json'), JSON.stringify(report, null, 2) + '\n');

  let bad = report.errors.length;
  for (const e of report.errors) console.log(`design-match: ${e}`);
  const summary = [];
  if (checks.has('texts')) {
    const k = report.texts.filter((t) => !t.ok).length;
    bad += k;
    summary.push(`texts: ${k} missing`);
  } else summary.push('texts: skipped');
  if (checks.has('tokens')) {
    for (const p of [base, ...exp.palettes.filter((q) => q !== base)]) {
      const block = report.tokens.find((b) => b.palette === p.name);
      const k = block ? block.rows.filter((r) => !r.ok).length : 0;
      bad += k;
      summary.push(p === base ? `tokens: ${k} mismatched` : `tokens[${p.name}]: ${k} mismatched`);
    }
  } else summary.push('tokens: skipped');
  if (checks.has('flatness')) {
    const k = report.flatness.reduce((n, f) => n + f.count + f.invalid.length, 0);
    bad += k;
    summary.push(`flatness: ${k} violations`);
  } else summary.push('flatness: skipped');
  if (checks.has('composites')) {
    const written = report.composites.filter((c) => c.file).length;
    const missing = report.composites.length - written;
    bad += missing;
    summary.push(`composites: ${written} written, ${missing} without a Figma image`);
  } else summary.push('composites: skipped');
  for (const line of summary) console.log(line);
  return bad > 0 ? 1 : 0;
}

process.exitCode = await main();
