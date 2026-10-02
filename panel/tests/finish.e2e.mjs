// The Impeccable finish review's eight fixes (owner, 2026-09-26, all eight
// approved; .claude/workorders/forgepact-ui-ship-finish-review.md), in a real
// (headless) Edge against the sandbox server: F1 the redrawn Setup and Mods
// rail icons, F2 the ThemePicker over the kept native select, F3 the status
// bar's drawn check, F4 the rarity note hidden at rest, F5 the chain warning's
// mono run, F6 no glyph icons, F7 symmetric gutters and F8 the type drift;
// then the same rule for World's Restore defaults button (impeccable P2 on
// ForgePact PR #100, owner-approved 2026-09-27).
//
//   node tests/finish.e2e.mjs [--dist <dir>]    (npm run e2e:finish)
//
// The ThemePicker checks hold the picker to the select it presents: the
// derived oracle's own theme steps (behaviour-oracle-derived.json, the three
// `#theme` selects) say what choosing a palette must post, and
// theme-picker-posts requires exactly that through the picker, by pointer and
// by keyboard, and nothing at all on opening, closing, Escape or choosing the
// palette already chosen. Looks are compared with a token's own computed value
// (a probe painted with `var(--token)`), never with a literal colour, and
// every positive keeps its control beside it: the old gear and puzzle paths,
// a turned-on rarity row, a poll that does change the chain note, a saving
// and a failed save indicator. Every check runs on its own sandbox and page.
// The last line is `e2e-finish: <n>/<n> checks passed`.

import { readFileSync } from 'node:fs';
import { VIEWPORTS, launchBrowser, openPanel, parseArgs, postSet, startSandbox, waitBooted, waitSaved } from './lib/browser.mjs';
import { THEMES } from '../src/theme.js';

const args = parseArgs(process.argv.slice(2));
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const EXPECTED = [
  'finish-F1-rail-icons',
  'theme-picker-aria',
  'theme-picker-keyboard',
  'theme-picker-posts',
  'theme-picker-sync',
  'theme-picker-native-hidden',
  'theme-picker-swatches',
  'finish-F3-saved-check',
  'finish-F4-rarity-note',
  'finish-F5-chain-mono',
  'finish-F6-no-glyphs',
  'finish-F7-gutters',
  'finish-F8-type',
  'finish-restore-icon',
];
const TABS = ['setup', 'modifiers', 'world', 'loot', 'mods'];
// The derived oracle's theme steps: what choosing each palette posts.
const DERIVED = JSON.parse(readFileSync(new URL('./behaviour-oracle-derived.json', import.meta.url), 'utf8'));
const THEME_STEPS = DERIVED.steps.filter((s) => s.control === '#theme' && s.action === 'select');
// The icons F1 replaced: the rail's Setup gear and Mods puzzle piece.
const OLD_SETUP = 'M10 3h4l1 3 3 1 3 2v4l-3 2-1 3-3 3h-4l-1-3-3-1-3-2v-4l3-2 1-3zM15 12a3 3 0 1 1-6 0 3 3 0 0 1 6 0';
const OLD_MODS = 'M4 4h6V2a3 3 0 0 1 6 0v2h5v6h-2a3 3 0 0 0 0 6h2v5h-6v-2a3 3 0 0 0-6 0v2H4v-6H2a3 3 0 0 1 0-6h2Z';
const trigger = '.theme-picker-trigger';
const list = '.theme-picker-list';
const option = (value) => `.theme-picker-option[data-value="${value}"]`;

function assert(ok, message) { if (!ok) throw new Error(message); }
const $ = (page, fn, arg) => page.evaluate(fn, arg);
const frames = (page) => $(page, () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))));
const tab = async (page, name) => { await $(page, (n) => document.querySelector(`.tabbtn[data-tab="${n}"]`).click(), name); await frames(page); };
async function settled(page) { await wait(30); await waitSaved(page); await wait(60); await frames(page); }
async function reload(page) { await page.reload(); await waitBooted(page); await frames(page); }
// Setup writes go to the sandbox from this process, not the page (lib/browser.mjs postSet).
const post = postSet;
const isOpen = (page) => $(page, (s) => document.querySelector(s).checkVisibility(), list);
const focused = (page) => $(page, () => document.activeElement?.className || document.activeElement?.tagName);

// A token's computed value, from a probe painted with it.
const token = (page, name, property = 'color') => $(page, ([n, p]) => {
  const probe = document.createElement('div');
  probe.style.setProperty(p, `var(${n})`);
  document.body.append(probe);
  const value = getComputedStyle(probe).getPropertyValue(p);
  probe.remove();
  return value;
}, [name, property]);

// Every POST the page makes, as { url, body } in the oracle's shape.
function recordPosts(page) {
  const posts = [];
  page.on('request', (req) => {
    if (req.method() !== 'POST') return;
    let body = req.postData();
    try { body = JSON.parse(body); } catch { /* keep the text */ }
    posts.push({ url: new URL(req.url()).pathname, body });
  });
  return posts;
}

// tokens.css's palettes, parsed the plain way, for the swatch check.
function palettes() {
  const css = readFileSync(new URL('../src/tokens.css', import.meta.url), 'utf8') +
    readFileSync(new URL('../src/ember/palette.css', import.meta.url), 'utf8');
  const out = {};
  for (const [, selector, body] of css.matchAll(/(:root(?:\[[^\]]+\])?)\s*\{([^}]*)\}/g)) {
    const name = /data-theme="?([\w-]+)"?/.exec(selector)?.[1] || THEMES[0].value;
    out[name] = { ...(out[name] || {}), ...Object.fromEntries([...body.matchAll(/(--[\w-]+)\s*:\s*([^;]+);/g)].map(([, k, v]) => [k, v.trim()])) };
  }
  return out;
}
const hexRgb = (hex) => {
  const h = hex.replace('#', '');
  const full = h.length === 3 ? [...h].map((c) => c + c).join('') : h.slice(0, 6);
  return `rgb(${parseInt(full.slice(0, 2), 16)}, ${parseInt(full.slice(2, 4), 16)}, ${parseInt(full.slice(4, 6), 16)})`;
};

// ---- F1 ----

async function railIcons({ page }) {
  const got = await $(page, () => {
    const icon = (id) => document.querySelector(`#${id} svg`);
    const d = (id) => [...icon(id).querySelectorAll('path')].map((p) => p.getAttribute('d')).join(' ');
    const stroke = (id) => { const cs = getComputedStyle(icon(id)); return [cs.strokeWidth, cs.strokeLinecap, cs.strokeLinejoin, cs.fill, cs.width].join(' '); };
    return { setup: d('nav-setup'), mods: d('nav-mods'), strokes: ['setup', 'modifiers', 'world', 'loot', 'mods'].map((t) => stroke('nav-' + t)),
      hidden: ['setup', 'mods'].every((t) => icon('nav-' + t).getAttribute('aria-hidden') === 'true') };
  });
  assert(got.setup !== OLD_SETUP && got.mods !== OLD_MODS, 'a rail icon is still the old gear or puzzle piece');
  // The ring: an outer circle and an inner one, each two arcs of one radius.
  const radii = [...got.setup.matchAll(/a([\d.]+) \1 0 1 1[^M]*/g)].map((m) => m[1]);
  assert(JSON.stringify(radii) === JSON.stringify(['8', '2.5']) && (got.setup.match(/M/g) || []).length === 2, `the Setup icon is not a ring and a centre: ${got.setup}`);
  // The grid: four closed squares.
  assert((got.mods.match(/z/gi) || []).length === 4 && (got.mods.match(/h6v6h-6z/g) || []).length === 4, `the Mods icon is not a 2x2 grid: ${got.mods}`);
  assert(new Set(got.strokes).size === 1, `the rail icons differ in stroke: ${got.strokes.join(' | ')}`);
  assert(got.hidden, 'a redrawn icon is not aria-hidden');
  return `ring and 2x2 grid, one stroke across the five tabs (${got.strokes[0]})`;
}

// ---- F2: the ThemePicker ----

async function pickerAria({ page }) {
  await tab(page, 'setup');
  const closed = await $(page, ([t, l]) => {
    const btn = document.querySelector(t);
    const name = (btn.getAttribute('aria-labelledby') || '').split(/\s+/).map((id) => document.getElementById(id)?.textContent.trim()).join(' ');
    const ul = document.querySelector(l);
    const opts = [...ul.querySelectorAll('[role="option"]')];
    const card = btn.closest('.tab-card');
    return {
      tag: btn.tagName, type: btn.type, id: btn.id, popup: btn.getAttribute('aria-haspopup'), expanded: btn.getAttribute('aria-expanded'),
      controls: btn.getAttribute('aria-controls') === ul.id, name, role: ul.getAttribute('role'), labelled: document.getElementById(ul.getAttribute('aria-labelledby'))?.textContent.trim(),
      options: opts.map((o) => [o.textContent.trim(), o.getAttribute('aria-selected'), o.tagName]),
      buttonIds: [...document.querySelectorAll('.tab-card[data-tab="setup"] button[id]')].map((b) => b.id), inCard: !!card,
    };
  }, [trigger, list]);
  assert(closed.tag === 'BUTTON' && closed.type === 'button' && closed.id === '', `the trigger is not an id-less button: ${JSON.stringify(closed)}`);
  assert(closed.popup === 'listbox' && closed.expanded === 'false' && closed.controls, `trigger ARIA: ${JSON.stringify(closed)}`);
  assert(closed.name === 'Theme Ledger', `the trigger is named "${closed.name}", not "Theme Ledger"`);
  assert(closed.role === 'listbox' && closed.labelled === 'Theme', `the list: ${closed.role}, labelled ${closed.labelled}`);
  assert(JSON.stringify(closed.options.map((o) => o[0])) === JSON.stringify(THEMES.map((t) => t.label)) && closed.options.every((o) => o[2] === 'LI'),
    `options: ${JSON.stringify(closed.options)}`);
  assert(closed.options.filter((o) => o[1] === 'true').map((o) => o[0]).join() === 'Ledger', `aria-selected: ${JSON.stringify(closed.options)}`);
  // The oracle's coverage walk counts button[id]: the picker adds none.
  assert(JSON.stringify(closed.buttonIds.sort()) === JSON.stringify(['exebrowse', 'exesave', 'installmod', 'launchgame', 'openreports', 'removeplugin']), `Setup's button ids: ${closed.buttonIds.join(',')}`);
  await page.click(trigger);
  await wait(300);
  const open = await $(page, ([t, l]) => ({ expanded: document.querySelector(t).getAttribute('aria-expanded'), active: document.querySelector(l).getAttribute('aria-activedescendant'),
    focus: document.activeElement === document.querySelector(l) }), [trigger, list]);
  assert(open.expanded === 'true' && open.active === 'themeOption-ledger' && open.focus, `open: ${JSON.stringify(open)}`);
  const snap = await page.locator('.theme-picker').ariaSnapshot();
  assert(/listbox/.test(snap) && /option "Graphite"/.test(snap) && !/combobox/.test(snap), `the open picker's accessibility tree: ${snap}`);
  return `button "Theme Ledger" (haspopup listbox, no id), listbox of ${closed.options.length} options, one selected`;
}

async function pickerKeyboard({ page }) {
  await tab(page, 'setup');
  const posts = recordPosts(page);
  await $(page, (t) => document.querySelector(t).focus(), trigger);
  await page.keyboard.press('Enter');
  await frames(page);
  assert(await isOpen(page) && (await focused(page)).includes('theme-picker-list'), 'Enter on the trigger did not open the list and focus it');
  const act = () => $(page, (l) => document.querySelector(l).getAttribute('aria-activedescendant'), list);
  assert(await act() === 'themeOption-ledger', 'the list did not open on the chosen palette');
  await page.keyboard.press('ArrowDown');
  assert(await act() === 'themeOption-graphite', 'ArrowDown did not move');
  await page.keyboard.press('End');
  assert(await act() === `themeOption-${THEMES.at(-1).value}`, 'End did not move to the last');
  await page.keyboard.press('ArrowDown');
  assert(await act() === `themeOption-${THEMES.at(-1).value}`, 'ArrowDown moved past the end');
  await page.keyboard.press('Home');
  assert(await act() === 'themeOption-ledger', 'Home did not move to the first');
  await page.keyboard.press('g');
  assert(await act() === 'themeOption-graphite', 'typing g did not move to Graphite');
  await page.keyboard.press('Escape');
  await frames(page);
  assert(!await isOpen(page) && (await focused(page)).includes('theme-picker-trigger'), 'Escape did not close the list and return focus to the trigger');
  assert(await $(page, () => document.getElementById('theme').value) === 'ledger', 'Escape changed the theme');
  await page.keyboard.press('ArrowUp');
  assert(await isOpen(page), 'ArrowUp on the trigger did not open the list');
  await page.keyboard.press('Escape');
  await page.keyboard.press('ArrowDown');
  assert(await isOpen(page), 'ArrowDown on the trigger did not open the list');
  await page.keyboard.press('Tab');
  await frames(page);
  assert(!await isOpen(page), 'Tab did not close the list');
  assert(!(await focused(page)).includes('theme-picker') && await $(page, () => document.activeElement?.id !== 'theme'), 'Tab left focus in the picker or on the hidden select');
  await $(page, (t) => document.querySelector(t).focus(), trigger);
  await page.keyboard.press('Space');
  await frames(page);
  assert(await isOpen(page), 'Space on the trigger did not open the list');
  await page.keyboard.press('ArrowDown');
  await page.keyboard.press('Space');
  await settled(page);
  assert(!await isOpen(page) && (await focused(page)).includes('theme-picker-trigger'), 'Space did not choose, close and return focus');
  const chosen = await $(page, () => ({ value: document.getElementById('theme').value, painted: document.documentElement.dataset.theme }));
  assert(chosen.value === 'graphite' && chosen.painted === 'graphite', `Space chose ${JSON.stringify(chosen)}`);
  assert(posts.length === 1 && posts[0].body.key === 'theme' && posts[0].body.value === 'graphite', `posts: ${JSON.stringify(posts)}`);
  return 'Enter, Space and the arrows open; arrows, Home, End and a letter move; Escape and Tab close with no change; Space chooses (1 post)';
}

async function pickerPosts({ page, sandbox }) {
  assert(THEME_STEPS.length === THEMES.length, `the derived oracle has ${THEME_STEPS.length} theme steps`);
  await tab(page, 'setup');
  const posts = recordPosts(page);
  const run = [];
  // Start away from the first step's palette, so every step is a real change.
  await post(page, { key: 'theme', value: THEME_STEPS.at(-1).value });
  await reload(page);
  await tab(page, 'setup');
  posts.length = 0;
  let viaKeyboard = false;
  for (const step of THEME_STEPS) {
    // Nothing on open and close, on Escape, or on choosing the palette already chosen.
    await page.click(trigger);
    await wait(250);
    await page.click(trigger);
    await wait(250);
    await $(page, (t) => document.querySelector(t).focus(), trigger);
    await page.keyboard.press('Enter');
    await page.keyboard.press('Escape');
    const current = await $(page, () => document.getElementById('theme').value);
    await page.click(trigger);
    await wait(250);
    await page.click(option(current));
    await settled(page);
    assert(posts.length === 0, `step ${step.step}: posted before choosing: ${JSON.stringify(posts)}`);
    sandbox.truncateCmds();
    if (viaKeyboard) {
      await $(page, (t) => document.querySelector(t).focus(), trigger);
      await page.keyboard.press('Enter');
      const target = THEMES.findIndex((t) => t.value === step.value);
      await page.keyboard.press('Home');
      for (let i = 0; i < target; i++) await page.keyboard.press('ArrowDown');
      await page.keyboard.press('Enter');
    } else {
      await page.click(trigger);
      await wait(250);
      await page.click(option(step.value));
    }
    await settled(page);
    const want = step.expect.posts.is;
    assert(JSON.stringify(posts) === JSON.stringify(want), `step ${step.step} (${viaKeyboard ? 'keyboard' : 'pointer'}): posted ${JSON.stringify(posts)}, the oracle says ${JSON.stringify(want)}`);
    const cmds = sandbox.readCmds();
    assert(JSON.stringify(cmds) === JSON.stringify(step.expect.cmds.is), `step ${step.step}: sent ${JSON.stringify(cmds)}, the oracle says ${JSON.stringify(step.expect.cmds.is)}`);
    run.push(`${step.step} ${step.value} ${viaKeyboard ? 'keyboard' : 'pointer'}`);
    posts.length = 0;
    viaKeyboard = !viaKeyboard;
  }
  // And the reverse of each: every palette chosen by the other input too.
  for (const step of THEME_STEPS) {
    const current = await $(page, () => document.getElementById('theme').value);
    if (current === step.value) continue;
    if (!viaKeyboard) { await page.click(trigger); await wait(250); await page.click(option(step.value)); } else {
      await $(page, (t) => document.querySelector(t).focus(), trigger);
      await page.keyboard.press('Enter');
      await page.keyboard.press('Home');
      for (let i = 0; i < THEMES.findIndex((t) => t.value === step.value); i++) await page.keyboard.press('ArrowDown');
      await page.keyboard.press('Enter');
    }
    await settled(page);
    assert(JSON.stringify(posts) === JSON.stringify(step.expect.posts.is), `step ${step.step} again: posted ${JSON.stringify(posts)}`);
    posts.length = 0;
    viaKeyboard = !viaKeyboard;
  }
  return `the derived oracle's ${THEME_STEPS.length} theme steps, exactly (${run.join('; ')}); open, close, Escape and the same palette: nothing`;
}

async function pickerSync({ page }) {
  await tab(page, 'setup');
  const shown = () => $(page, (t) => ({ text: document.querySelector(t).textContent.trim(),
    selected: [...document.querySelectorAll('.theme-picker-option[aria-selected="true"]')].map((o) => o.dataset.value).join() }), trigger);
  assert((await shown()).text === 'Ledger', 'the trigger does not start at Ledger');
  // The select driven directly, as the oracle and ui.e2e.mjs do.
  await page.selectOption('#theme', 'sigil');
  await settled(page);
  let s = await shown();
  assert(s.text === 'Sigil' && s.selected === 'sigil', `after selectOption: ${JSON.stringify(s)}`);
  // Reload: boot's applyTheme paints the saved palette, and the trigger follows.
  await reload(page);
  await tab(page, 'setup');
  s = await shown();
  assert(s.text === 'Sigil' && s.selected === 'sigil', `after a reload: ${JSON.stringify(s)}`);
  // The root attribute alone (applyTheme after a save answers another palette).
  await $(page, () => { document.documentElement.dataset.theme = 'graphite'; });
  await frames(page);
  s = await shown();
  assert(s.text === 'Graphite' && s.selected === 'graphite', `after data-theme: ${JSON.stringify(s)}`);
  // Repainting the same palette writes nothing.
  const writes = await $(page, async () => {
    let n = 0;
    const mo = new MutationObserver((rs) => { n += rs.length; });
    mo.observe(document.querySelector('.theme-picker'), { subtree: true, childList: true, characterData: true, attributes: true });
    document.documentElement.dataset.theme = 'graphite';
    await new Promise((r) => setTimeout(r, 50));
    mo.disconnect();
    return n;
  });
  assert(writes === 0, `repainting the same palette wrote ${writes} mutations into the picker`);
  return 'follows selectOption, a reload and data-theme; a same-palette repaint writes nothing';
}

async function nativeHidden({ page }) {
  await tab(page, 'setup');
  const got = await $(page, () => {
    const s = document.getElementById('theme');
    const cs = getComputedStyle(s);
    const r = s.getBoundingClientRect();
    return { visible: s.checkVisibility(), w: r.width, h: r.height, display: cs.display, visibility: cs.visibility, ariaHidden: s.getAttribute('aria-hidden'), tab: s.tabIndex,
      borders: [cs.borderTopWidth, cs.borderRightWidth, cs.borderBottomWidth, cs.borderLeftWidth], options: [...s.options].map((o) => o.value) };
  });
  assert(got.visible && got.w >= 1 && got.h >= 1 && got.display !== 'none' && got.visibility !== 'hidden', `the select lost its box: ${JSON.stringify(got)}`);
  assert(got.w <= 1 && got.h <= 1, `the select is drawn: ${got.w}x${got.h}`);
  assert(got.ariaHidden === 'true' && got.tab === -1, `the select is reachable: ${JSON.stringify(got)}`);
  assert(got.borders.every((b) => b === '0px'), `the select carries a border: ${got.borders.join(' ')}`);
  assert(JSON.stringify(got.options) === JSON.stringify(THEMES.map((t) => t.value)), `options: ${got.options.join(',')}`);
  // The Appearance card by what it holds: Setup has a third card since issue #76 (Incident reports).
  const snap = await page.locator('.tab-card[data-tab="setup"]:has(#theme)').ariaSnapshot();
  assert(!/combobox/.test(snap) && /button "Theme Ledger"/.test(snap), `the Appearance card's tree: ${snap}`);
  // Tab from the trigger never lands on the select.
  await $(page, () => document.querySelector('.theme-picker-trigger').focus());
  await page.keyboard.press('Tab');
  assert(await $(page, () => document.activeElement?.id !== 'theme'), 'Tab reached the hidden select');
  await tab(page, 'modifiers');
  assert(!await $(page, () => document.getElementById('theme').checkVisibility()), 'the select shows on Modifiers');
  return `${got.w}x${got.h}, aria-hidden, tabindex -1, no border; the tree shows the button, not a combobox`;
}

async function swatches({ page }) {
  await tab(page, 'setup');
  const want = palettes();
  await page.click(trigger);
  await wait(300);
  const got = await $(page, () => [...document.querySelectorAll('.theme-picker-option')].map((o) => ({
    value: o.dataset.value, chips: [...o.querySelectorAll('.theme-picker-swatches > span')].map((c) => getComputedStyle(c).backgroundColor),
    check: getComputedStyle(o, '::after').maskImage || getComputedStyle(o, '::after').webkitMaskImage, selected: o.getAttribute('aria-selected'),
  })));
  for (const o of got) {
    const exp = ['--color-border-strong', '--color-text-muted', '--color-accent'].map((k) => hexRgb(want[o.value][k]));
    assert(JSON.stringify(o.chips) === JSON.stringify(exp), `${o.value}: swatches ${o.chips.join(' ')}, the palette's ${exp.join(' ')}`);
  }
  assert(new Set(got.map((o) => o.chips.join())).size === THEMES.length, 'two palettes show the same swatches');
  const checked = got.filter((o) => /svg/.test(o.check || ''));
  assert(checked.length === 1 && checked[0].value === 'ledger' && checked[0].selected === 'true', `the drawn check marks ${checked.map((o) => o.value).join(',') || 'nothing'}`);
  const edge = await $(page, (l) => { const cs = getComputedStyle(document.querySelector(l)); return [cs.borderTopWidth, cs.borderTopStyle, cs.borderTopColor]; }, list);
  assert(edge[0] === '1px' && edge[1] === 'solid' && edge[2] === await token(page, '--color-border-subtle'), `the list's edge: ${edge.join(' ')}`);
  return `3 swatches per palette from tokens.css, all different; a 1px border/subtle edge; the check on Ledger`;
}

// ---- F3: the status bar's drawn check, only when settled ----

async function savedCheck({ page }) {
  const mark = () => $(page, () => { const cs = getComputedStyle(document.getElementById('saveIndicator'), '::before');
    return { content: cs.content, mask: cs.maskImage || cs.webkitMaskImage, bg: cs.backgroundColor, text: document.getElementById('saveIndicator').textContent, cls: document.getElementById('saveIndicator').className }; });
  const ok = await token(page, '--color-ok', 'background-color');
  let m = await mark();
  assert(m.text === 'Settings loaded' && /svg/.test(m.mask) && m.bg === ok, `settled "Settings loaded": ${JSON.stringify(m)}`);
  await tab(page, 'mods');
  // A slow write: the indicator says Saving... for as long as it takes.
  await page.route('**/api/set', async (route) => { await wait(800); return route.continue(); });
  await $(page, () => document.getElementById('headhunter').click());
  await page.waitForFunction(() => document.getElementById('saveIndicator').className === 'saving');
  m = await mark();
  assert(!/svg/.test(m.mask || '') && m.text === 'Saving...', `saving: ${JSON.stringify(m)}`);
  await settled(page);
  await page.unroute('**/api/set');
  m = await mark();
  assert(m.text === 'Saved' && /svg/.test(m.mask) && m.bg === ok, `after a save: ${JSON.stringify(m)}`);
  await page.route('**/api/set', (route) => route.abort());
  await $(page, () => document.getElementById('headhunter').click());
  await page.waitForFunction(() => document.getElementById('saveIndicator').className === 'error');
  m = await mark();
  assert(!/svg/.test(m.mask || ''), `a failed save still shows the check: ${JSON.stringify(m)}`);
  return `the check (color/ok) on "Settings loaded" and "Saved"; none while saving or after a failure`;
}

// ---- F4: the rarity note ----

async function rarityNote({ page }) {
  await tab(page, 'world');
  const note = () => $(page, () => { const n = document.getElementById('raritynote'); return { shown: n.checkVisibility(), text: n.textContent.trim() }; });
  let n = await note();
  assert(!n.shown && n.text.startsWith('off'), `at rest: ${JSON.stringify(n)}`);
  // The control: a turned-on row shows it, with the shares.
  await $(page, () => {
    const sw = document.getElementById('sw_rarity_rare');
    if (!sw.checked) sw.click();
    const r = document.getElementById('rarity_rare');
    r.value = 25;
    r.dispatchEvent(new Event('input', { bubbles: true }));
  });
  await frames(page);
  n = await note();
  assert(n.shown && /25% Rare/.test(n.text), `a Rare share of 25%: ${JSON.stringify(n)}`);
  return `hidden at rest (its words kept: "${n.text.slice(0, 24)}..." once on); shown with a share`;
}

// ---- F5: the chain warning's mono run, and an idle poll that writes nothing ----

async function chainMono({ page }) {
  await tab(page, 'setup');
  const got = await $(page, () => {
    const cn = document.getElementById('chainnote');
    const run = cn.querySelector('.chain-command');
    return { text: cn.textContent, run: run?.textContent, font: run ? getComputedStyle(run).fontFamily : '', body: getComputedStyle(cn).fontFamily, lines: cn.children.length };
  });
  assert(got.run === 'Install Mod Plugin' && /IBM Plex Mono/.test(got.font) && !/IBM Plex Mono/.test(got.body), `the run: ${JSON.stringify(got)}`);
  assert(!got.text.includes('"') && got.text.endsWith('- click Install Mod Plugin (game must be closed)'), `the sentence: ${got.text}`);
  // Idle polls: count /api/state requests and the note's mutations over two polls.
  let polls = 0;
  page.on('request', (req) => { if (req.url().endsWith('/api/state')) polls++; });
  await $(page, () => { window.__chain = 0; new MutationObserver((rs) => { window.__chain += rs.length; }).observe(document.getElementById('chainnote'), { subtree: true, childList: true, characterData: true }); });
  const start = Date.now();
  while (polls < 2 && Date.now() - start < 12000) await wait(200);
  const idle = await $(page, () => window.__chain);
  assert(polls >= 2 && idle === 0, `${polls} idle polls wrote ${idle} mutations into the chain note`);
  // The control: a poll that changes what is missing does rewrite it.
  await page.route('**/api/state', async (route) => {
    const response = await route.fetch();
    const state = await response.json();
    return route.fulfill({ response, json: { ...state, chain: { ...(state.chain || {}), patched: true, aurieCore: true, yytk: true, plugin: false } } });
  });
  const before = polls;
  while (polls < before + 1 && Date.now() - start < 40000) await wait(200);
  await wait(300);
  const changed = await $(page, () => ({ n: window.__chain, text: document.getElementById('chainnote').textContent }));
  assert(changed.n > 0 && changed.text === 'mod chain incomplete: mod plugin - click Install Mod Plugin (game must be closed)', `a changed chain: ${JSON.stringify(changed)}`);
  return `"Install Mod Plugin" in Plex Mono, no quotes; ${polls - 1} idle polls wrote nothing, a changed chain rewrote it`;
}

// ---- F6: no glyph icons anywhere ----

async function noGlyphs({ page }) {
  const found = [];
  for (const name of TABS) {
    await tab(page, name);
    if (name === 'mods') {
      for (const sub of ['qol', 'items']) {
        await $(page, (s) => document.getElementById('subtab-' + s).click(), sub);
        await frames(page);
        const t = await $(page, () => document.body.innerText);
        if (/[✓↳↺]/.test(t)) found.push(`mods-${sub}`);
      }
    } else {
      const t = await $(page, () => document.body.innerText);
      if (/[✓↳↺]/.test(t)) found.push(name);
    }
  }
  assert(found.length === 0, `glyph icons on ${found.join(', ')}`);
  await tab(page, 'world');
  const sat = await $(page, () => { const s = document.getElementById('satSummary'); const cs = getComputedStyle(s, '::before'); return { text: s.textContent.trim(), mask: cs.maskImage || cs.webkitMaskImage, invalid: s.classList.contains('is-invalid') }; });
  assert(/^Selection valid/.test(sat.text) && !sat.invalid && /svg/.test(sat.mask), `the valid selection: ${JSON.stringify(sat)}`);
  await tab(page, 'mods');
  await $(page, () => document.getElementById('subtab-qol').click());
  await frames(page);
  const indent = await $(page, () => ['map_reveal_packs_row', 'map_reveal_spawn_row', 'mod_auto_prospect_bag_row', 'mod_hidden_loot_key_row'].map((id) => {
    const row = document.getElementById(id);
    const parent = row.parentElement.firstElementChild;
    return { id, indent: Math.round(row.getBoundingClientRect().left - parent.getBoundingClientRect().left), label: row.querySelector('.lbl').firstChild.textContent.trim() };
  }));
  assert(indent.every((r) => r.indent >= 12 && !/^[↳✓]/.test(r.label)), `sub-item rows: ${JSON.stringify(indent)}`);
  return `no ✓, ↳ or ↺ on any tab; Selection valid carries the drawn check; sub-items indented ${indent[0].indent}px`;
}

// ---- Restore defaults: the drawn icon, not the ↺ glyph (PR #100 review) ----
// The markup still starts the label with the glyph, as Browse and Launch start
// theirs, so decoratePanelIcons() is what takes it off: the source line is the
// control that there was a glyph to strip. Browse's folder icon is the
// pipeline's existing positive. The title stays, and the click is the
// oracle's (oracle:replay).

async function restoreIcon({ page }) {
  const source = readFileSync(new URL('../src/tabs/World.svelte', import.meta.url), 'utf8');
  assert(/id="satRestore"[^>]*>&#8634; Restore defaults</.test(source), 'World.svelte no longer starts Restore defaults with the glyph; this check has nothing to strip');
  await tab(page, 'world');
  const r = await $(page, () => {
    const b = document.getElementById('satRestore');
    const icon = b.querySelector(':scope > svg.setting-icon');
    const href = icon?.querySelector('use')?.getAttribute('href') || '';
    const symbol = href ? document.querySelector(href) : null;
    return {
      text: b.textContent, label: b.querySelector(':scope > .label-copy')?.textContent, title: b.title,
      icon: icon?.dataset.icon, href, symbol: symbol?.tagName.toLowerCase(), paths: symbol?.querySelectorAll('path').length || 0,
      browse: document.getElementById('exebrowse').querySelector(':scope > svg.setting-icon')?.dataset.icon,
    };
  });
  assert(r.browse === 'folder', `Browse lost its icon, the pipeline's positive: ${JSON.stringify(r)}`);
  assert(r.icon === 'restore' && r.href === '#fp-icon-restore' && r.symbol === 'symbol' && r.paths > 0, `Restore defaults carries no drawn icon: ${JSON.stringify(r)}`);
  assert(r.text === 'Restore defaults' && r.label === 'Restore defaults' && !/↺/.test(r.text), `Restore defaults' text: ${JSON.stringify(r)}`);
  assert(r.title === 'Enable every positive and negative zone modifier', `Restore defaults' title: ${r.title}`);
  return `the restore icon (${r.href}), text "${r.text}", no ↺; Browse keeps its folder`;
}

// ---- F7: symmetric gutters ----

async function gutters({ browser }) {
  const notes = [];
  for (const [width, want] of [[1280, 32], [900, 20]]) {
    await withPage(browser, { viewport: VIEWPORTS[width] }, async ({ page }) => {
      for (const name of TABS) {
        await tab(page, name);
        const g = await $(page, () => {
          const cards = [...document.querySelectorAll('#workspace > .tab-card.active')].filter((c) => c.checkVisibility());
          const left = Math.min(...cards.map((c) => c.getBoundingClientRect().left));
          const right = Math.max(...cards.map((c) => c.getBoundingClientRect().right));
          return { left, right: document.documentElement.clientWidth - right };
        });
        assert(Math.abs(g.left - want) <= 0.5 && Math.abs(g.right - want) <= 0.5, `${name} at ${width}: ${g.left}/${g.right}, not ${want}/${want}`);
      }
      notes.push(`${want}/${want} at ${width}`);
    });
  }
  return notes.join(', ') + ' on every tab';
}

// ---- F8: the launcher note and the helmet's line break ----

async function typeDrift({ page }) {
  await tab(page, 'setup');
  const note = await $(page, () => { const cs = getComputedStyle(document.querySelector('.setup-launch > .launch-description')); return { size: cs.fontSize, color: cs.color }; });
  const sm = await token(page, '--font-size-sm', 'font-size');
  const muted = await token(page, '--color-text-muted');
  assert(note.size === sm && note.color === muted, `the launcher description: ${JSON.stringify(note)}, want ${sm} ${muted}`);
  await tab(page, 'mods');
  await $(page, () => document.getElementById('subtab-items').click());
  await frames(page);
  const vein = await $(page, () => {
    const strong = [...document.querySelectorAll('#minerHelmetCard .hint strong')].find((s) => s.textContent.trim() === 'Vein Resonance:');
    const p = strong.closest('p');
    const first = p.getClientRects()[0];
    const r = strong.getBoundingClientRect();
    const before = strong.previousSibling;
    return { left: Math.round(r.left - first.left), below: r.top > first.top + 1, br: before?.nodeName, after: strong.nextSibling?.nodeName };
  });
  assert(vein.left === 0 && vein.below && vein.br === 'BR' && vein.after === 'BR', `Vein Resonance: ${JSON.stringify(vein)}`);
  return `launcher description ${note.size} text/muted; "Vein Resonance:" starts its own line`;
}

async function withPage(browser, { viewport = VIEWPORTS[1280] } = {}, fn) {
  // These layout assertions retain the approved Ledger contract. The theme
  // picker still visits all four palettes; Ember's layout has its own suite.
  const sandbox = await startSandbox({ dist: typeof args.dist === 'string' ? args.dist : null, seed: { theme: 'ledger' } });
  let page = null;
  try {
    page = await openPanel(browser, sandbox, viewport);
    await frames(page);
    return await fn({ page, sandbox, browser });
  } finally {
    await page?.context().close();
    await sandbox.stop();
  }
}

const CHECKS = [
  ['finish-F1-rail-icons', railIcons],
  ['theme-picker-aria', pickerAria],
  ['theme-picker-keyboard', pickerKeyboard],
  ['theme-picker-posts', pickerPosts],
  ['theme-picker-sync', pickerSync],
  ['theme-picker-native-hidden', nativeHidden],
  ['theme-picker-swatches', swatches],
  ['finish-F3-saved-check', savedCheck],
  ['finish-F4-rarity-note', rarityNote],
  ['finish-F5-chain-mono', chainMono],
  ['finish-F6-no-glyphs', noGlyphs],
  ['finish-F7-gutters', gutters, { own: true }],
  ['finish-F8-type', typeDrift],
  ['finish-restore-icon', restoreIcon],
];

const browser = await launchBrowser();
const passed = [];
let failures = 0;
try {
  for (const [name, fn, options = {}] of CHECKS) {
    try {
      const note = options.own ? await fn({ browser }) : await withPage(browser, options, fn);
      passed.push(name);
      console.log(`ok   ${name}${typeof note === 'string' && note ? ' (' + note + ')' : ''}`);
    } catch (e) {
      failures++;
      console.log(`FAIL ${name}: ${e.message.split('\n')[0]}`);
    }
  }
} finally {
  await browser.close();
}
const missing = EXPECTED.filter((label) => !passed.includes(label));
for (const label of missing) console.log(`FAIL not passed: ${label}`);
console.log(`e2e-finish: ${passed.length}/${EXPECTED.length} checks passed`);
process.exitCode = failures || missing.length ? 1 : 0;
