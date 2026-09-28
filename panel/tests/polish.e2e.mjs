// The owner's polish pass on the restyle, in a real (headless) Edge against
// the sandbox server: the Mining Ore Multiplier label and its note, one card
// per Mods mod with no repeated sub-tab heading, the theme on Setup, the
// footer credit, the plugin warning as an icon beside Apply all and beside
// "Settings loaded" with a tooltip, the Modifiers card's group separators,
// the Miner's Helmet stats in the accent, and an idle slider's note as a
// tooltip above its row instead of drawn over the row below it.
//
//   node tests/polish.e2e.mjs [--dist <dir>] [--shots <dir>]    (npm run e2e:polish)
//
// Every check runs on its own sandbox and page, so one failing check never
// hides another, and nothing needs putting back. Asserts through the DOM,
// getBoundingClientRect(), checkVisibility(), elementFromPoint() and, for the
// look this pass is about, computed styles compared with the token's own
// computed value (a probe element painted with `var(--token)`), never with a
// literal colour. "The game is running" or "the plugin is missing" is a routed
// /api/state answer, as review-fixes.e2e.mjs does it.
//
// The slider-note checks (owner, 2026-09-25: an idle row's hover note was drawn
// over the row below it) use three terms, at 1280 and at 900:
// - "clear": a note shown in the flow overlaps no visible `.row` outside its
//   own `.setting-entry` by more than 0.5px on both axes;
// - "a tooltip": the note is visible, not `position: static`, has
//   `pointer-events: none` and nothing focusable, is not what
//   elementFromPoint() finds at its centre, and sits above its own row
//   (bottom at or above the row's top) and within the row's width;
// - "still": every visible `.row` on the tab keeps its box, within 0.5px.
// Showing or hiding a note may never move a row: the behaviour oracle's replay
// clicks the next switch where it was, and so does a player.
// The tooltip opens after a delay, so a check waits for it with
// page.waitForFunction, bounded by the module's own OPEN_DELAY_MS, and never
// sleeps a fixed time instead.
//
// A slider at its default has no note (owner, 2026-09-26), so an idle row
// holds words only while its range carries a value not yet saved. The tooltip
// checks put that state on the rows they hover ("an unsaved value": the range
// moved and `input` dispatched, never `change`, so panel.js's own writer fills
// the note) and print how many tooltips opened, so none passes on zero.
// slider-note-none-at-default and slider-note-switched-off-by-value check the
// rule itself.
//
// --shots <dir> writes tooltip-open-1280.png, status-tooltip-900.png,
// note-hover-loot-900.png and note-default-loot-900.png for the owner.

import { mkdirSync } from 'node:fs';
import { join } from 'node:path';
import { VIEWPORTS, launchBrowser, openPanel, parseArgs, startSandbox, waitBooted, waitSaved } from './lib/browser.mjs';
import { THEMES } from '../src/theme.js';
import { OPEN_DELAY_MS, INSTANT_MS } from '../src/lib/slider-note.js';

const args = parseArgs(process.argv.slice(2));
const SHOTS = typeof args.shots === 'string' ? args.shots : null;
if (SHOTS) mkdirSync(SHOTS, { recursive: true });
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const COMPLETE = { exeExists: true, patched: true, aurieCore: true, yytk: true, plugin: true };
const NOT_INSTALLED = 'Plugin not installed. Your settings are saved, but modifiers cannot apply. Close the game, then install the plugin in Setup.';
const HELMET_STATUS = "Miner's Helmet: x4 replaces this slider while the helmet is worn; with the helmet off, this slider applies.";
const EXPECTED = [
  'mining-label-multiplier',
  'mining-note-empty-offline',
  'mining-status-still-shown',
  'mods-no-repeated-heading',
  'mods-one-card-per-mod-qol',
  'mods-one-card-per-mod-items',
  'mods-columns-balanced',
  'theme-on-setup',
  'footer-credit',
  'plugin-warning-indicator-placement',
  'plugin-warning-tooltip-hover',
  'plugin-warning-tooltip-focus',
  'plugin-warning-status-bar-tooltip',
  'plugin-warning-tooltip-in-viewport',
  'plugin-warning-follows-condition',
  'plugin-warning-opens-setup',
  'modifier-groups-separated',
  'helmet-stats-accent',
  'slider-note-tooltip-loot',
  'slider-note-tooltip-modifiers',
  'slider-note-tooltip-keyboard',
  'slider-note-tooltip-timing',
  'slider-note-clear-world',
  'slider-note-hover-moves-no-row',
  'slider-note-none-at-default',
  'slider-note-switched-off-by-value',
];
// Round 1's sentences above the default, byte for byte (D13 leaves them).
const DUNGEON_AT_4 = '4x its vanilla drop rate; where the game never rolls this family, the roll is opened at the normal-key chance first';
const DAMAGE_AT_100 = 'adds 100% to the final hit after the game finishes its own calculation (+100% doubles it)';
// Child rows sit in their parent's card, never in one of their own.
const CHILD_CONTROLS = ['map_reveal_packs', 'map_reveal_spawn', 'mod_auto_prospect_bag'];

function assert(ok, message) { if (!ok) throw new Error(message); }
const $ = (page, fn, arg) => page.evaluate(fn, arg);
const frames = (page) => $(page, () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))));
const tab = async (page, name) => { await $(page, (n) => document.querySelector(`.tabbtn[data-tab="${n}"]`).click(), name); await frames(page); };
const subtab = async (page, id) => { await $(page, (i) => document.getElementById(i).click(), id); await frames(page); };
const away = async (page) => {
  await page.mouse.move(1, (page.viewportSize()?.height || 800) - 60);
  await $(page, () => document.activeElement?.blur?.());
  await frames(page);
};
// A tooltip fades in: shoot it once its show has finished, not mid-fade.
async function settledShot(page, selector, path) {
  await $(page, (s) => Promise.all(document.querySelector(s).getAnimations().map((a) => a.finished)), selector);
  await page.screenshot({ path });
}
async function settled(page) { await wait(30); await waitSaved(page); await wait(60); await frames(page); }
async function reload(page) { await page.reload(); await waitBooted(page); await frames(page); }

// A token's computed value, from a probe painted with it.
const token = (page, name, property = 'color') => $(page, ([n, p]) => {
  const probe = document.createElement('div');
  probe.style.setProperty(p, `var(${n})`);
  document.body.append(probe);
  const value = getComputedStyle(probe).getPropertyValue(p);
  probe.remove();
  return value;
}, [name, property]);

// Every /api/state answer patched: the way a poll brings a running game.
async function patchState(page, patch) {
  await page.route('**/api/state', async (route) => {
    const response = await route.fetch();
    const state = await response.json();
    return route.fulfill({ response, json: { ...state, ...patch(state) } });
  });
}
const helmetOn = (state) => ({
  gameRunning: true,
  pluginMods: { ...(state.pluginMods || {}), minerHelmet: { available: true, enabled: true, reason: '' } },
});

// Runs in the page: is the note matching `selector` shown, and which visible
// rows outside its own entry does it overlap by more than 0.5px on both axes?
function noteOverlap(selector) {
  const note = document.querySelector(selector);
  if (!note) return { missing: true };
  if (!note.checkVisibility()) return { shown: false };
  const r = note.getBoundingClientRect();
  const own = note.closest('.setting-entry');
  const hits = [];
  for (const row of document.querySelectorAll('.row')) {
    if ((own && own.contains(row)) || !row.checkVisibility()) continue;
    const b = row.getBoundingClientRect();
    if (!b.width || !b.height) continue;
    const ox = Math.min(r.right, b.right) - Math.max(r.left, b.left);
    const oy = Math.min(r.bottom, b.bottom) - Math.max(r.top, b.top);
    if (ox > 0.5 && oy > 0.5) hits.push({ row: row.querySelector('.lbl')?.textContent.trim().slice(0, 40), ox: Math.round(ox), oy: Math.round(oy) });
  }
  return { shown: true, text: note.textContent.trim(), hits };
}
const overlap = (page, selector) => $(page, noteOverlap, selector);
const noteOf = (sec, key) => `.setting-entry:has(input[type=range][data-sec="${sec}"][data-key="${key}"]) > .note[data-note="${key}"]`;
const entryOf = (sec, key) => `.setting-entry:has(input[type=range][data-sec="${sec}"][data-key="${key}"])`;

async function mustBeClear(page, selector, what) {
  const got = await overlap(page, selector);
  assert(!got.missing, `${what}: no note ${selector}`);
  assert(got.shown, `${what}: the note is not shown`);
  assert(got.hits.length === 0, `${what}: the note "${got.text.slice(0, 50)}" covers ${JSON.stringify(got.hits)}`);
  return got;
}

// A note shown in the flow: not a tooltip, `position: static`, and clear.
async function mustBeInFlow(page, selector, what) {
  const got = await mustBeClear(page, selector, what);
  const how = await $(page, (s) => { const n = document.querySelector(s); return { position: getComputedStyle(n).position, tooltip: n.hasAttribute('data-tooltip') }; }, selector);
  assert(how.position === 'static' && !how.tooltip, `${what}: the note is not in the flow: ${JSON.stringify(how)}`);
  return got;
}

// Runs in the page: every visible `.row` on the tab, in the page's own
// coordinates (#wrap scrolls, and a hover may scroll it), so only a real move
// in the layout counts.
function rowBoxes() {
  const wrap = document.getElementById('wrap');
  return [...document.querySelectorAll('#workspace .row')].filter((r) => r.checkVisibility()).map((r) => {
    const b = r.getBoundingClientRect();
    return { label: r.querySelector('.lbl')?.textContent.trim().slice(0, 40) || '', top: b.top + wrap.scrollTop, left: b.left + wrap.scrollLeft, width: b.width, height: b.height };
  });
}
const boxes = (page) => $(page, rowBoxes);
function mustBeStill(before, after, what) {
  const sides = ['top', 'left', 'width', 'height'];
  const moved = before.length !== after.length ? [`${before.length} rows became ${after.length}`]
    : before.filter((b, i) => sides.some((s) => Math.abs(after[i][s] - b[s]) > 0.5))
      .map((b) => `${b.label} by ${sides.map((s) => Math.round(after[before.indexOf(b)][s] - b[s])).join('/')}`);
  assert(moved.length === 0, `${what}: rows moved (top/left/width/height): ${moved.slice(0, 5).join('; ')}`);
}

// Runs in the page: is the note matching `selector` a tooltip (see the header),
// and does it cover its own row or the row directly below it in its column?
function noteTooltip(selector) {
  const note = document.querySelector(selector);
  if (!note) return { missing: true };
  if (!note.checkVisibility()) return { visible: false };
  const cs = getComputedStyle(note);
  const r = note.getBoundingClientRect();
  const entry = note.closest('.setting-entry');
  const own = entry.querySelector('.row').getBoundingClientRect();
  const covers = (b) => Math.min(r.right, b.right) - Math.max(r.left, b.left) > 0.5 && Math.min(r.bottom, b.bottom) - Math.max(r.top, b.top) > 0.5;
  const below = [...document.querySelectorAll('#workspace .row')].filter((x) => x.checkVisibility() && !entry.contains(x))
    .map((x) => x.getBoundingClientRect()).filter((b) => Math.abs(b.left - own.left) < 2 && b.top >= own.bottom - 0.5)
    .sort((a, b) => a.top - b.top)[0] || null;
  const hit = document.elementFromPoint((r.left + r.right) / 2, (r.top + r.bottom) / 2);
  const focusable = note.querySelectorAll('a, button, input, select, textarea, [tabindex]').length;
  const why = [];
  if (cs.position === 'static') why.push('position static');
  if (cs.pointerEvents !== 'none') why.push('pointer-events ' + cs.pointerEvents);
  if (focusable) why.push(focusable + ' focusable inside');
  if (hit && (hit === note || note.contains(hit))) why.push('it is hit at its centre');
  if (r.bottom > own.top + 0.5) why.push(`its bottom ${Math.round(r.bottom)} is below its row's top ${Math.round(own.top)}`);
  if (r.left < own.left - 0.5 || r.right > own.right + 0.5) why.push(`it spans ${Math.round(r.left)}-${Math.round(r.right)}, its row ${Math.round(own.left)}-${Math.round(own.right)}`);
  if (note.hasAttribute('role') && note.getAttribute('role') !== 'tooltip') why.push('role ' + note.getAttribute('role'));
  return { visible: true, tooltip: why.length === 0, why, text: note.textContent.trim(), coversOwnRow: covers(own), hasRowBelow: !!below, coversRowBelow: below ? covers(below) : false };
}
const tooltipOf = (page, selector) => $(page, noteTooltip, selector);
async function mustBeTooltip(page, selector, what) {
  const got = await tooltipOf(page, selector);
  assert(!got.missing && got.visible, `${what}: the note is not shown`);
  assert(got.tooltip, `${what}: the note is not a tooltip: ${got.why.join('; ')}`);
  assert(!got.coversOwnRow && !got.coversRowBelow, `${what}: the tooltip covers ${got.coversOwnRow ? 'its own row' : 'the row below'}`);
  return got;
}

// A note opens after the module's delay: wait for it, bounded, never a fixed sleep.
async function waitNote(page, selector, shown, timeout, what) {
  try {
    await page.waitForFunction(([s, o]) => !!document.querySelector(s)?.checkVisibility() === o, [selector, shown], { timeout });
  } catch {
    throw new Error(`${what}: the note was ${shown ? 'not shown' : 'still shown'} after ${timeout}ms`);
  }
}
const waitShown = (page, selector, what) => waitNote(page, selector, true, OPEN_DELAY_MS + 2000, what);
const waitHidden = (page, selector, what) => waitNote(page, selector, false, 2000, what);
// A note that hovering will open as a tooltip: hidden now, and has words.
const opensOnHover = (page, selector) => $(page, (s) => { const n = document.querySelector(s); return !n.checkVisibility() && n.textContent.trim() !== ''; }, selector);

// Hover each note-carrying entry under `root` in turn: every shown note under
// `root` is a tooltip or in the flow and clear, and no row moves while it is
// shown. Answers how many entries it hovered and how many of them opened
// their note as a tooltip.
async function sweep(page, root, what) {
  const keys = await $(page, (r) => [...document.querySelectorAll(`${r} .setting-entry`)]
    .filter((e) => e.checkVisibility() && e.querySelector(':scope > .note[data-note]'))
    .map((e) => { const i = e.querySelector('input[type=range]'); return [i.dataset.sec, i.dataset.key]; }), root);
  let tooltips = 0;
  for (const [sec, key] of keys) {
    const at = `${what}, hovering ${sec}.${key}`;
    await away(page);
    const opens = await opensOnHover(page, noteOf(sec, key));
    const before = await boxes(page);
    await page.hover(`${entryOf(sec, key)} .lbl`);
    if (opens) {
      await waitShown(page, noteOf(sec, key), at);
      await mustBeTooltip(page, noteOf(sec, key), at);
      tooltips++;
    } else await frames(page);
    const shown = await $(page, (r) => [...document.querySelectorAll(`${r} .setting-entry > .note[data-note]`)]
      .filter((n) => n.checkVisibility()).map((n) => {
        const i = n.closest('.setting-entry').querySelector('input[type=range]');
        return [i.dataset.sec, i.dataset.key];
      }), root);
    assert(shown.some(([s, k]) => s === sec && k === key) || await $(page, (s) => document.querySelector(s).textContent.trim() === '', noteOf(sec, key)),
      `${at}: its note does not show`);
    for (const [s, k] of shown) {
      if ((await tooltipOf(page, noteOf(s, k))).tooltip) await mustBeTooltip(page, noteOf(s, k), `${at}, ${s}.${k}`);
      else await mustBeInFlow(page, noteOf(s, k), `${at}, ${s}.${k}`);
    }
    mustBeStill(before, await boxes(page), at);
  }
  return { hovered: keys.length, tooltips };
}

// An unsaved value: the range moved above its default and `input` dispatched,
// never `change`, so panel.js's own `oninput` writes the note and nothing is
// saved. The row stays idle. Any later write repaints every range from the
// saved config (refreshSavedControls), which clears it, so apply it after the
// page's last write. `value` null means one drag step above the default (the
// step panel.js snaps a dragged value to: a repaint leaves step="any" and keeps
// the drag step in data-step0).
async function unsaved(page, sec, key, value = null) {
  const range = `input[type=range][data-sec="${sec}"][data-key="${key}"]`;
  return $(page, ([s, v]) => {
    const r = document.querySelector(s);
    r.value = v === null ? Math.min(+r.max, +r.min + (parseFloat(r.dataset.step0 || r.step) || 1)) : v;
    r.dispatchEvent(new Event('input', { bubbles: true }));
    return +r.value;
  }, [range, value]);
}
// Back to the default the same way; the note is empty again.
async function restoreDefault(page, sec, key) {
  const range = `input[type=range][data-sec="${sec}"][data-key="${key}"]`;
  await $(page, (s) => { const r = document.querySelector(s); r.value = r.min; r.dispatchEvent(new Event('input', { bubbles: true })); }, range);
}
// The unsaved value on every idle note-carrying entry under `root` (switch on,
// not listed, not a drop row). Answers how many entries it was applied to.
async function unsavedIdle(page, root) {
  const keys = await $(page, (r) => [...document.querySelectorAll(`${r} .setting-entry`)]
    .filter((e) => e.checkVisibility() && e.querySelector(':scope > .note[data-note]') && !e.querySelector('[data-sec="drops"]')
      && e.querySelector(':scope .slider-switch:not([data-live]) > input:checked'))
    .map((e) => { const i = e.querySelector('input[type=range]'); return [i.dataset.sec, i.dataset.key]; }), root);
  for (const [sec, key] of keys) await unsaved(page, sec, key);
  return keys.length;
}

// Make `sec.key` idle: its switch on (clicked only if off), at its default,
// and not listed.
async function makeIdle(page, sec, key, what) {
  const range = `input[type=range][data-sec="${sec}"][data-key="${key}"]`;
  const sw = `#sw_${sec}_${key}`;
  if (!await $(page, (s) => document.querySelector(s).checked, sw)) {
    await $(page, (s) => document.querySelector(s).click(), sw);
    await settled(page);
  }
  assert(await $(page, (s) => { const r = document.querySelector(s); return +r.value === +r.min; }, range), `${what}: ${sec}.${key} is not at its default`);
  assert(!await $(page, (s) => document.querySelector(s).closest('label.slider-switch').hasAttribute('data-live'), sw), `${what}: ${sec}.${key} is not idle`);
}

// One idle entry holding an unsaved value hovered (a tooltip above its row,
// nothing moves, gone when the pointer leaves), then committed and live (in
// the flow and clear), then the unsaved value on every idle entry and the
// sweep, which must open a tooltip on each of them.
async function noteStates(page, sec, key, unsavedValue, liveValue, root, what, shot = null) {
  const range = `input[type=range][data-sec="${sec}"][data-key="${key}"]`;
  const note = noteOf(sec, key);
  await makeIdle(page, sec, key, what);
  await unsaved(page, sec, key, unsavedValue);
  await away(page);
  assert(await opensOnHover(page, note), `${what}: idle ${sec}.${key} with an unsaved value has no hidden note with words`);
  assert(!await $(page, (s) => document.querySelector(s).closest('label.slider-switch').hasAttribute('data-live'), `#sw_${sec}_${key}`), `${what}: ${sec}.${key} with an unsaved value is not idle`);
  const before = await boxes(page);
  await page.hover(`${entryOf(sec, key)} .lbl`);
  await waitShown(page, note, `${what}, idle ${sec}.${key} hovered`);
  const tip = await mustBeTooltip(page, note, `${what}, idle ${sec}.${key} hovered`);
  assert(tip.hasRowBelow, `${what}: ${sec}.${key} has no row below it to test against`);
  mustBeStill(before, await boxes(page), `${what}, idle ${sec}.${key} hovered`);
  if (shot) await settledShot(page, note, shot);
  await away(page);
  await waitHidden(page, note, `${what}, idle ${sec}.${key} after the pointer left`);
  mustBeStill(before, await boxes(page), `${what}, idle ${sec}.${key} after the pointer left`);
  await $(page, ([s, v]) => {
    const r = document.querySelector(s);
    r.value = v;
    r.dispatchEvent(new Event('input', { bubbles: true }));
    r.dispatchEvent(new Event('change', { bubbles: true }));
  }, [range, liveValue]);
  await settled(page);
  await away(page);
  assert(await $(page, (s) => document.querySelector(s).closest('label.slider-switch').hasAttribute('data-live'), `#sw_${sec}_${key}`), `${what}: ${sec}.${key} at ${liveValue} is not live`);
  const live = await mustBeInFlow(page, note, `${what}, live ${sec}.${key} at ${liveValue}`);
  const applied = await unsavedIdle(page, root);
  const swept = await sweep(page, root, what);
  assert(swept.tooltips >= 1 && swept.tooltips === applied, `${what}: ${swept.tooltips} tooltips opened for ${applied} idle entries holding an unsaved value`);
  return { live, ...swept };
}

// The first `#keys` entry with another row directly below it in its column.
const firstKeyWithRowBelow = (page) => $(page, () => {
  const entries = [...document.querySelectorAll('#keys .setting-entry')].filter((e) => e.checkVisibility());
  const rows = [...document.querySelectorAll('#dropSettings .row')].filter((r) => r.checkVisibility());
  for (const e of entries) {
    const b = e.querySelector('.row').getBoundingClientRect();
    if (rows.some((r) => { const c = r.getBoundingClientRect(); return Math.abs(c.left - b.left) < 2 && c.top >= b.bottom - 1 && !e.contains(r); })) {
      return e.querySelector('input[type=range]').dataset.key;
    }
  }
  return null;
});

// ---- The checks. Each gets a fresh sandbox and page (see run()). ----

async function miningLabel({ page }) {
  await tab(page, 'loot');
  const label = await $(page, () => document.querySelector('input[type=range][data-sec="drops"][data-key="mining_ore"]')?.closest('.row')?.querySelector('.lbl')?.textContent.trim());
  assert(label?.startsWith('Mining Ore Multiplier'), `The mining row reads "${label}"`);
  assert(!await $(page, () => document.body.innerText.includes('Mining Ore Amount')), 'The Loot tab still says Mining Ore Amount');
  await tab(page, 'mods');
  await subtab(page, 'subtab-items');
  assert(!await $(page, () => document.body.innerText.includes('Mining Ore Amount')), 'The Items tab still says Mining Ore Amount');
  assert(await $(page, () => document.body.innerText.includes('Mining Ore Multiplier')), 'The helmet hint does not name the Mining Ore Multiplier slider');
  assert(!await $(page, () => document.documentElement.textContent.includes('Multiplies ore from mining')), 'The static mining sentence is still in the page');
}

async function miningNoteOffline({ page }) {
  await tab(page, 'loot');
  const note = await $(page, () => {
    const n = document.querySelector('.note[data-note="mining_ore"]');
    return n ? { text: n.textContent, visible: n.checkVisibility(), entry: !!n.closest('.setting-entry') } : null;
  });
  assert(note, 'The mining row has no note element');
  assert(note.entry, 'The mining note is not in its row\'s entry');
  assert(note.text.trim() === '' && !note.visible, `The offline mining note is not empty: "${note.text}"`);
}

async function miningStatus({ page }) {
  await patchState(page, helmetOn);
  await reload(page);
  await tab(page, 'loot');
  await page.hover(`${entryOf('drops', 'mining_ore')} .lbl`);
  await frames(page);
  const note = await $(page, () => { const n = document.querySelector('.note[data-note="mining_ore"]'); return { text: n.textContent.trim(), visible: n.checkVisibility() }; });
  assert(note.visible && note.text === HELMET_STATUS, `The mining note with the helmet worn reads "${note.text}"`);
}

async function modsHeading({ page }) {
  await tab(page, 'mods');
  for (const [sub, label] of [['subtab-qol', 'Quality of Life'], ['subtab-items', 'Items']]) {
    await subtab(page, sub);
    const got = await $(page, (l) => ({
      h2: document.querySelectorAll('#qolCard h2, #itemsCard h2').length,
      count: document.getElementById('workspace').innerText.split('\n').filter((line) => line.trim() === l).length,
    }), label);
    assert(got.h2 === 0, `${label}: a Mods panel still has ${got.h2} heading(s)`);
    assert(got.count === 1, `${label}: the sub-tab's name is shown ${got.count} times`);
  }
}

function modCards([id, children]) {
  const card = document.getElementById(id);
  const probe = document.createElement('div');
  probe.style.backgroundColor = 'var(--color-bg-raised)';
  document.body.append(probe);
  const RAISED = getComputedStyle(probe).backgroundColor;
  probe.remove();
  const bg = (el) => getComputedStyle(el).backgroundColor;
  const raised = [...card.querySelectorAll('*')].filter((el) => el.checkVisibility() && bg(el) === RAISED);
  const top = raised.filter((el) => !raised.some((o) => o !== el && o.contains(el)));
  const units = [...card.querySelectorAll('.mods-col > *')].filter((el) => el.checkVisibility());
  const controls = [...card.querySelectorAll('input[type=checkbox], select')];
  const tops = controls.filter((c) => !children.includes(c.id));
  const perCard = top.map((t) => ({ id: t.id || t.querySelector('input,select')?.id || t.className, controls: tops.filter((c) => t.contains(c)).map((c) => c.id) }));
  const childHome = children.filter((c) => document.getElementById(c) && card.contains(document.getElementById(c))).map((c) => {
    const holder = top.find((t) => t.contains(document.getElementById(c)));
    return { child: c, parentInSame: !!holder && holder.querySelectorAll('input[type=checkbox]').length > 1 };
  });
  return {
    wrapper: bg(card), transparent: bg(card) === 'rgba(0, 0, 0, 0)', raised: raised.length, top: top.length,
    unitsAreTops: units.length === top.length && units.every((u) => top.includes(u)), units: units.length, perCard, childHome,
  };
}

async function modsCardsQol({ page }) {
  await tab(page, 'mods');
  await subtab(page, 'subtab-qol');
  const got = await $(page, modCards, ['qolCard', CHILD_CONTROLS]);
  assert(got.transparent, `#qolCard is still drawn as a card (${got.wrapper})`);
  assert(got.top === 11 && got.raised === 11, `Quality of Life: ${got.top} top-level cards (${got.raised} raised), not 11`);
  assert(got.unitsAreTops, `Quality of Life: the cards are not the column's ${got.units} mods`);
  assert(got.perCard.every((p) => p.controls.length === 1), 'A Quality of Life card does not hold exactly one mod: ' + JSON.stringify(got.perCard));
  assert(got.childHome.length === CHILD_CONTROLS.length && got.childHome.every((c) => c.parentInSame), 'A child row is not in its parent\'s card: ' + JSON.stringify(got.childHome));
}

async function modsCardsItems({ page }) {
  await tab(page, 'mods');
  await subtab(page, 'subtab-items');
  const got = await $(page, modCards, ['itemsCard', []]);
  assert(got.transparent, `#itemsCard is still drawn as a card (${got.wrapper})`);
  assert(got.top === 4 && got.raised === 4, `Items: ${got.top} top-level cards (${got.raised} raised), not 4`);
  assert(got.unitsAreTops, `Items: the cards are not the column's ${got.units} mods`);
  const helmet = got.perCard.filter((p) => p.id === 'minerHelmetCard');
  assert(helmet.length === 1 && helmet[0].controls.length === 0, 'The Miner\'s Helmet is not a card of its own: ' + JSON.stringify(got.perCard));
  assert(got.perCard.filter((p) => p.id !== 'minerHelmetCard').every((p) => p.controls.length === 1), 'An Items card does not hold exactly one mod: ' + JSON.stringify(got.perCard));
}

async function modsColumns({ page }) {
  await tab(page, 'mods');
  for (const [sub, id] of [['subtab-qol', 'qolCard'], ['subtab-items', 'itemsCard']]) {
    await subtab(page, sub);
    await wait(50);
    await frames(page);
    const got = await $(page, (i) => {
      const cols = [...document.querySelectorAll(`#${i} .mods-grid > .mods-col`)];
      return { n: cols.length, h: cols.map((c) => c.getBoundingClientRect().height) };
    }, id);
    assert(got.n === 2, `${id}: ${got.n} columns`);
    assert(got.h[0] >= got.h[1] - 0.5, `${id}: the left column (${got.h[0]}) is shorter than the right (${got.h[1]})`);
  }
}

async function themeOnSetup({ page }) {
  const where = await $(page, () => {
    const t = document.getElementById('theme');
    return { inStatus: !!t.closest('#statusbar'), inSetup: !!t.closest('.tab-card[data-tab="setup"]'), options: [...t.options].map((o) => o.value) };
  });
  assert(!where.inStatus, 'The theme is still in the status bar');
  assert(where.inSetup, 'The theme is not in a Setup card');
  assert(JSON.stringify(where.options) === JSON.stringify(THEMES.map((t) => t.value)), 'Theme options out of THEMES order: ' + where.options.join(','));
  await tab(page, 'modifiers');
  assert(!await $(page, () => document.getElementById('theme').checkVisibility()), 'The theme shows on Modifiers');
  await tab(page, 'setup');
  assert(await $(page, () => document.getElementById('theme').checkVisibility()), 'The theme does not show on Setup');
}

async function footerCredit({ page }) {
  const got = await $(page, () => {
    const ver = document.getElementById('panelver');
    const span = ver?.parentElement;
    return { own: span ? [...span.childNodes].filter((n) => n !== ver).map((n) => n.textContent).join('').trim() : null, inFoot: !!span?.closest('.status-foot'), visible: !!span?.checkVisibility() };
  });
  assert(got.own === 'Created by Falor and ST4H' && got.inFoot && got.visible, 'The credit reads ' + JSON.stringify(got));
}

const WARN_TINT_WIDE = () => {
  const probe = document.createElement('div');
  probe.style.backgroundColor = 'var(--color-warn-tint)';
  document.body.append(probe);
  const TINT = getComputedStyle(probe).backgroundColor;
  probe.remove();
  const wrap = document.getElementById('wrap');
  const half = wrap.getBoundingClientRect().width / 2;
  return [...wrap.querySelectorAll('*')].filter((el) => el.checkVisibility() && getComputedStyle(el).backgroundColor === TINT && el.getBoundingClientRect().width > half)
    .map((el) => el.id || el.className);
};
const pageIcon = '#pluginWarning button';
const statusIcon = '#statusbar button[aria-label="Open Setup"]';
const tipOf = (icon) => `${icon} + [role="tooltip"]`;

async function indicatorPlacement({ page }) {
  await tab(page, 'world');
  const got = await $(page, () => {
    const w = document.getElementById('pluginWarning');
    const save = document.getElementById('saveIndicator');
    const next = save.nextElementSibling;
    const icon = next?.querySelector('button:not([id])');
    const a = save.getBoundingClientRect();
    const b = icon?.getBoundingClientRect();
    return {
      inActions: !!w.closest('.page-actions'), width: w.getBoundingClientRect().width, visible: w.checkVisibility(),
      statusIcon: !!icon && icon.checkVisibility() && icon.getAttribute('aria-label') === 'Open Setup',
      gap: b ? Math.round(b.left - a.right) : null, sameLine: b ? Math.abs((a.top + a.bottom) / 2 - (b.top + b.bottom) / 2) < 4 : false,
    };
  });
  assert(got.visible && got.inActions, 'The warning is not in the page actions: ' + JSON.stringify(got));
  assert(got.width <= 48, `The closed warning is ${got.width}px wide`);
  assert(got.statusIcon && got.gap !== null && got.gap >= 0 && got.gap <= 24 && got.sameLine, 'No warning icon beside Settings loaded: ' + JSON.stringify(got));
  const wide = await $(page, WARN_TINT_WIDE);
  assert(wide.length === 0, 'A warn-tinted band still spans the page: ' + wide.join(','));
}

const tipState = (page, icon) => $(page, ([i, t]) => {
  const tip = document.querySelector(t);
  const text = document.getElementById('pluginWarningText').textContent;
  if (!tip) return { missing: true };
  const r = tip.getBoundingClientRect();
  const lines = [...tip.children].filter((c) => c.checkVisibility()).map((c) => c.textContent.trim());
  return { visible: tip.checkVisibility(), lines, text, rect: { top: r.top, bottom: r.bottom, left: r.left, right: r.right }, icon: document.querySelector(i).getBoundingClientRect().top };
}, [icon, tipOf(icon)]);
const waitTip = (page, icon, open) => page.waitForFunction(([t, o]) => !!document.querySelector(t)?.checkVisibility() === o, [tipOf(icon), open], { timeout: 2000 });

async function tooltipHover({ page }) {
  await tab(page, 'world');
  await page.hover(pageIcon);
  await waitTip(page, pageIcon, true);
  const got = await tipState(page, pageIcon);
  assert(got.visible && got.lines[0] === got.text && got.text !== '' && got.lines[1] === 'Open Setup' && got.lines.length === 2,
    'The hover tooltip reads ' + JSON.stringify(got.lines));
  if (SHOTS) await settledShot(page, tipOf(pageIcon), join(SHOTS, 'tooltip-open-1280.png'));
  // Onto the tooltip itself: it stays open (WCAG 1.4.13).
  await page.mouse.move((got.rect.left + got.rect.right) / 2, (got.rect.top + got.rect.bottom) / 2, { steps: 4 });
  await wait(300);
  assert((await tipState(page, pageIcon)).visible, 'The tooltip closed while hovered');
  await away(page);
  await waitTip(page, pageIcon, false);
}

async function tooltipFocus({ page }) {
  await tab(page, 'world');
  await $(page, () => document.getElementById('applyall').focus());
  await page.keyboard.press('Tab');
  await frames(page);
  assert(await $(page, (s) => document.activeElement === document.querySelector(s) && document.activeElement.matches(':focus-visible'), pageIcon),
    'Tab from Apply all does not reach the warning icon');
  await waitTip(page, pageIcon, true);
  const got = await tipState(page, pageIcon);
  assert(got.lines[0] === got.text && got.lines[1] === 'Open Setup', 'The focus tooltip reads ' + JSON.stringify(got.lines));
  await page.keyboard.press('Escape');
  await frames(page);
  assert(!(await tipState(page, pageIcon)).visible, 'Escape did not close the tooltip');
  assert(await $(page, (s) => document.activeElement === document.querySelector(s), pageIcon), 'Escape moved focus off the icon');
}

async function statusTooltip({ page }) {
  await tab(page, 'loot');
  await page.hover(statusIcon);
  await waitTip(page, statusIcon, true);
  const got = await tipState(page, statusIcon);
  assert(got.lines[0] === got.text && got.text !== '' && got.lines[1] === 'Open Setup', 'The status bar tooltip reads ' + JSON.stringify(got.lines));
  assert(got.rect.bottom <= got.icon + 0.5, `The status bar tooltip does not open upward (bottom ${got.rect.bottom}, icon top ${got.icon})`);
  await away(page);
  await waitTip(page, statusIcon, false);
}

async function tooltipViewport({ browser }) {
  for (const width of [1280, 900]) {
    await withPage(browser, { viewport: VIEWPORTS[width] }, async ({ page }) => {
      await tab(page, 'mods');
      for (const icon of [pageIcon, statusIcon]) {
        await $(page, (s) => document.querySelector(s).focus(), icon);
        await waitTip(page, icon, true);
        const got = await $(page, (t) => {
          const tip = document.querySelector(t);
          const r = tip.getBoundingClientRect();
          const hit = document.elementFromPoint((r.left + r.right) / 2, (r.top + r.bottom) / 2);
          return { r: [r.left, r.top, r.right, r.bottom].map(Math.round), vw: innerWidth, vh: innerHeight, inside: !!hit && tip.contains(hit) };
        }, tipOf(icon));
        assert(got.r[0] >= 0 && got.r[1] >= 0 && got.r[2] <= got.vw && got.r[3] <= got.vh, `${width}: ${icon}'s tooltip leaves the window: ${JSON.stringify(got)}`);
        assert(got.inside, `${width}: ${icon}'s tooltip is covered at its centre`);
        if (SHOTS && width === 900 && icon === statusIcon) await settledShot(page, tipOf(icon), join(SHOTS, 'status-tooltip-900.png'));
        await page.keyboard.press('Escape');
        await away(page);
      }
    });
  }
}

async function followsCondition({ page }) {
  let chain = COMPLETE;
  await patchState(page, () => ({ chain }));
  await reload(page);
  const shown = () => $(page, ([a, b]) => [!!document.querySelector(a)?.checkVisibility(), !!document.querySelector(b)?.checkVisibility()], [pageIcon, statusIcon]);
  let got = await shown();
  assert(!got[0] && !got[1], 'A complete chain still shows a warning icon: ' + JSON.stringify(got));
  chain = { ...COMPLETE, patched: false, aurieCore: false, yytk: false, plugin: false };
  await reload(page);
  got = await shown();
  assert(got[0] && got[1], 'A missing plugin does not show both icons: ' + JSON.stringify(got));
  const texts = await $(page, ([a, b]) => [a, b].map((s) => document.querySelector(s).querySelector('span')?.textContent), [tipOf(pageIcon), tipOf(statusIcon)]);
  assert(texts.every((t) => t === NOT_INSTALLED), 'The tooltips do not carry the not-installed message: ' + JSON.stringify(texts));
  await page.unroute('**/api/state');
}

async function opensSetup({ page }) {
  await tab(page, 'world');
  await page.click(statusIcon);
  await frames(page);
  assert(await $(page, () => document.getElementById('nav-setup').getAttribute('aria-selected') === 'true'), 'The status bar icon does not open Setup');
}

async function groupsSeparated({ browser }) {
  for (const [width, bordered] of [[1280, [2, 3]], [900, [1, 2, 3]]]) {
    await withPage(browser, { viewport: VIEWPORTS[width] }, async ({ page }) => {
      await tab(page, 'modifiers');
      const subtle = await token(page, '--color-border-subtle', 'border-top-color');
      const got = await $(page, () => [...document.querySelectorAll('.modifier-grid > .modifier-group')].map((g) => {
        const cs = getComputedStyle(g);
        return { top: cs.borderTopStyle !== 'none' ? parseFloat(cs.borderTopWidth) : 0, color: cs.borderTopColor,
          others: ['right', 'bottom', 'left'].some((s) => cs.getPropertyValue(`border-${s}-style`) !== 'none' && parseFloat(cs.getPropertyValue(`border-${s}-width`)) > 0) };
      }));
      assert(got.length === 4, `${width}: ${got.length} modifier groups`);
      got.forEach((g, i) => {
        const want = bordered.includes(i);
        assert(!g.others, `${width}: group ${i} has a side or bottom border`);
        assert(want ? g.top === 1 && g.color === subtle : g.top === 0, `${width}: group ${i} ${want ? 'lacks its 1px subtle rule' : 'has a rule'}: ${JSON.stringify(g)}`);
      });
    });
  }
}

async function helmetAccent({ page }) {
  await tab(page, 'mods');
  await subtab(page, 'subtab-items');
  const accent = await token(page, '--color-accent');
  const got = await $(page, () => {
    const all = [...document.querySelectorAll('#minerHelmetCard *')].filter((el) => el.textContent.trim().startsWith('+1000 Defense'));
    const el = all.at(-1);
    if (!el) return null;
    const cs = getComputedStyle(el);
    return { color: cs.color, family: cs.fontFamily, hint: el.classList.contains('hint'), text: el.textContent.trim() };
  });
  assert(got, 'No Miner\'s Helmet stat lines');
  assert(got.color === accent && /IBM Plex Mono/.test(got.family) && !got.hint, 'The helmet stats are drawn ' + JSON.stringify(got));
}

async function noteLoot({ browser }) {
  const notes = [];
  for (const width of [1280, 900]) {
    await withPage(browser, { viewport: VIEWPORTS[width] }, async ({ page }) => {
      await tab(page, 'loot');
      const key = await firstKeyWithRowBelow(page);
      assert(key, `${width}: no #keys entry has a row below it`);
      const shot = SHOTS && width === 900 ? join(SHOTS, 'note-hover-loot-900.png') : null;
      const got = await noteStates(page, 'keys', key, 4, 4, '.tab-card[data-tab="loot"]', `Loot ${width}`, shot);
      assert(/^4x /.test(got.live.text), `${width}: the live ${key} note reads "${got.live.text}"`);
      notes.push(`${width}: keys.${key}, ${got.hovered} hovered, ${got.tooltips} tooltips`);
    });
    await withPage(browser, { viewport: VIEWPORTS[width], routes: (page) => patchState(page, helmetOn) }, async ({ page }) => {
      await tab(page, 'loot');
      const got = await mustBeInFlow(page, noteOf('drops', 'mining_ore'), `Loot ${width}, the running mining note`);
      assert(got.text === HELMET_STATUS, `${width}: the mining note reads "${got.text}"`);
    });
  }
  return notes.join('; ');
}

async function noteModifiers({ browser }) {
  const notes = [];
  for (const width of [1280, 900]) {
    await withPage(browser, { viewport: VIEWPORTS[width] }, async ({ page }) => {
      await tab(page, 'modifiers');
      const max = await $(page, () => document.querySelector('input[type=range][data-sec="stats"][data-key="exp"]').max);
      const got = await noteStates(page, 'stats', 'exp', null, +max, '.modifier-card', `Modifiers ${width}`);
      notes.push(`${width}: ${got.hovered} hovered, ${got.tooltips} tooltips`);
    });
  }
  return notes.join('; ');
}

async function noteWorld({ browser }) {
  const notes = [];
  for (const width of [1280, 900]) {
    await withPage(browser, { viewport: VIEWPORTS[width] }, async ({ page }) => {
      await tab(page, 'world');
      const entries = await $(page, () => [...document.querySelectorAll('#workspace .setting-entry')].filter((e) => e.checkVisibility()).length);
      assert(entries > 0, `World ${width}: no visible .setting-entry to measure`);
      const { hovered } = await sweep(page, '#workspace', `World ${width}`);
      notes.push(`${width}: ${entries} entries, ${hovered} with a note${hovered ? '' : ' (a guard for a note added later, not a proof)'}`);
    });
  }
  return notes.join('; ');
}

// Keyboard: focus opens an idle row's note as a tooltip, Escape closes it and
// leaves focus where it was, and moving focus to the next row moves the note.
async function noteKeyboard({ page }) {
  const what = 'Modifiers 1280, keyboard';
  await tab(page, 'modifiers');
  const keys = await $(page, () => [...document.querySelectorAll('#stats .setting-entry')].map((e) => e.querySelector('input[type=range]').dataset.key));
  const next = keys[keys.indexOf('exp') + 1];
  assert(keys.includes('exp') && next, `${what}: no entry after exp in #stats (${keys.join(',')})`);
  for (const key of ['exp', next]) await makeIdle(page, 'stats', key, what);
  for (const key of ['exp', next]) await unsaved(page, 'stats', key);
  const range = (k) => `input[type=range][data-sec="stats"][data-key="${k}"]`;
  const note = noteOf('stats', 'exp');
  const focus = (k) => $(page, (s) => document.querySelector(s).focus(), range(k));
  await away(page);
  const before = await boxes(page);
  await focus('exp');
  await waitShown(page, note, `${what}, exp focused`);
  const tip = await mustBeTooltip(page, note, `${what}, exp focused`);
  mustBeStill(before, await boxes(page), `${what}, exp focused`);
  const role = await $(page, (s) => document.querySelector(s).getAttribute('role'), note);
  assert(role === null || role === 'tooltip', `${what}: the tooltip's role is ${role}`);
  await page.keyboard.press('Escape');
  await waitHidden(page, note, `${what}, Escape`);
  assert(await $(page, (s) => document.activeElement === document.querySelector(s), range('exp')), `${what}: Escape moved focus off the range`);
  await $(page, () => document.activeElement.blur());
  await frames(page);
  await focus('exp');
  await waitShown(page, note, `${what}, exp focused again`);
  await focus(next);
  await waitHidden(page, note, `${what}, focus moved to ${next}`);
  await waitShown(page, noteOf('stats', next), `${what}, ${next} focused`);
  await mustBeTooltip(page, noteOf('stats', next), `${what}, ${next} focused`);
  mustBeStill(before, await boxes(page), `${what}, ${next} focused`);
  return `exp then ${next}: "${tip.text.slice(0, 30)}…"`;
}

// Timing, measured inside the page so the check does not race the delay: the
// first hover waits OPEN_DELAY_MS; the next row straight after opens at once,
// with no animation.
async function noteTiming({ page }) {
  const what = 'Loot 1280, timing';
  await tab(page, 'loot');
  const pair = await $(page, () => {
    const entries = [...document.querySelectorAll('#keys .setting-entry')].filter((e) => e.checkVisibility());
    const box = (e) => e.querySelector('.row').getBoundingClientRect();
    for (const e of entries) {
      const below = entries.find((o) => o !== e && Math.abs(box(o).left - box(e).left) < 2 && box(o).top >= box(e).bottom - 1);
      if (below) return [e, below].map((x) => x.querySelector('input[type=range]').dataset.key);
    }
    return null;
  });
  assert(pair, `${what}: no two #keys entries one above the other`);
  for (const key of pair) await makeIdle(page, 'keys', key, what);
  for (const key of pair) await unsaved(page, 'keys', key);
  await away(page);
  await $(page, (keys) => {
    const log = window.__noteTiming = keys.map(() => ({ enter: [], shown: [] }));
    keys.forEach((k, i) => {
      const entry = document.querySelector(`.setting-entry:has(input[type=range][data-sec="keys"][data-key="${k}"])`);
      const note = entry.querySelector(':scope > .note[data-note]');
      entry.addEventListener('pointerenter', (e) => { if (e.target === entry) log[i].enter.push(performance.now()); }, true);
      new MutationObserver(() => { if (note.hasAttribute('data-tooltip')) log[i].shown.push(performance.now()); })
        .observe(note, { attributes: true, attributeFilter: ['data-tooltip'] });
    });
  }, pair);
  const [first, second] = pair.map((k) => noteOf('keys', k));
  await page.hover(`${entryOf('keys', pair[0])} .lbl`);
  await waitShown(page, first, `${what}, ${pair[0]} hovered`);
  await page.hover(`${entryOf('keys', pair[1])} .lbl`);
  await waitShown(page, second, `${what}, ${pair[1]} hovered straight after`);
  const animations = await $(page, (s) => new Promise((r) => requestAnimationFrame(() => r(document.querySelector(s).getAnimations().length))), second);
  assert(!await $(page, (s) => document.querySelector(s).checkVisibility(), first), `${what}: ${pair[0]}'s note stayed open`);
  const log = await $(page, () => window.__noteTiming);
  assert(log.every((l) => l.enter.length && l.shown.length), `${what}: missing timestamps ${JSON.stringify(log)}`);
  const delay = log.map((l) => l.shown[0] - l.enter.at(-1));
  assert(delay[0] >= OPEN_DELAY_MS - 20 && delay[0] <= OPEN_DELAY_MS + 1500, `${what}: the first note opened after ${Math.round(delay[0])}ms, not about ${OPEN_DELAY_MS}ms`);
  assert(delay[1] <= 50, `${what}: the next note, within ${INSTANT_MS}ms of the last close, opened after ${Math.round(delay[1])}ms`);
  assert(animations === 0, `${what}: the instant note still animates (${animations})`);
  await page.mouse.move(1, (page.viewportSize()?.height || 800) - 60);
  await frames(page);
  assert(!await $(page, (s) => document.querySelector(s).checkVisibility(), second), `${what}: the note is still shown two frames after the pointer left`);
  return `first ${Math.round(delay[0])}ms, next ${Math.round(delay[1])}ms`;
}

// Hovering any note-carrying row on Modifiers moves no row: not while its
// tooltip is shown, and not after the pointer leaves and it hides. Every idle
// row holds an unsaved value first, so every hover opens a tooltip.
async function noteMovesNoRow({ browser }) {
  const notes = [];
  for (const width of [1280, 900]) {
    await withPage(browser, { viewport: VIEWPORTS[width] }, async ({ page }) => {
      await tab(page, 'modifiers');
      await unsavedIdle(page, '.modifier-card');
      const keys = await $(page, () => [...document.querySelectorAll('.modifier-card .setting-entry')]
        .filter((e) => e.checkVisibility() && e.querySelector(':scope > .note[data-note]'))
        .map((e) => { const i = e.querySelector('input[type=range]'); return [i.dataset.sec, i.dataset.key]; }));
      let tooltips = 0;
      for (const [sec, key] of keys) {
        const at = `Modifiers ${width}, hovering ${sec}.${key}`;
        await away(page);
        const opens = await opensOnHover(page, noteOf(sec, key));
        const before = await boxes(page);
        await page.hover(`${entryOf(sec, key)} .lbl`);
        if (opens) {
          await waitShown(page, noteOf(sec, key), at);
          await mustBeTooltip(page, noteOf(sec, key), at);
          tooltips++;
        } else await frames(page);
        mustBeStill(before, await boxes(page), at);
        await away(page);
        if (opens) await waitHidden(page, noteOf(sec, key), `${at}, after the pointer left`);
        mustBeStill(before, await boxes(page), `${at}, after the pointer left`);
      }
      assert(tooltips > 0, `Modifiers ${width}: no idle row to hover`);
      assert(tooltips === keys.length, `Modifiers ${width}: ${tooltips} tooltips for ${keys.length} rows hovered`);
      notes.push(`${width}: ${keys.length} hovered, ${tooltips} tooltips`);
    });
  }
  return notes.join('; ');
}

// No tooltip anywhere in the document within `timeout`: a bounded wait that
// must time out.
async function noTooltip(page, timeout, what) {
  const opened = await page.waitForFunction(() => {
    const t = document.querySelector('[data-tooltip]');
    return t ? (t.closest('.setting-entry')?.querySelector('input[type=range]')?.dataset.key || t.className || 'an element') : false;
  }, null, { timeout }).then((h) => h.jsonValue(), () => null);
  assert(opened === null, `${what}: a tooltip opened on ${opened}`);
}

// Every keys, stats and percent_stats range on the open tab sits in a
// .setting-entry holding its own note element, empty and hidden.
const defaultNotes = (page, secs) => $(page, (ss) => [...document.querySelectorAll('input[type=range][data-sec]')]
  .filter((r) => ss.includes(r.dataset.sec) && r.checkVisibility())
  .map((r) => {
    const n = r.closest('.setting-entry')?.querySelector(`:scope > .note[data-note="${r.dataset.key}"]`);
    return { key: `${r.dataset.sec}.${r.dataset.key}`, atDefault: +r.value === +r.min, note: !!n, text: n?.textContent ?? null, visible: !!n?.checkVisibility() };
  }), secs);
async function mustHaveEmptyNotes(page, secs, what) {
  const got = await defaultNotes(page, secs);
  assert(got.length > 0, `${what}: no ${secs.join('/')} range shown`);
  const bad = got.filter((g) => !g.atDefault || !g.note || g.text !== '' || g.visible);
  assert(bad.length === 0, `${what}: a slider at its default has a note: ${JSON.stringify(bad.slice(0, 3))}`);
  return got.length;
}

// F1 (owner, 2026-09-26): a slider at its default writes no note, so hovering
// or focusing an idle row at its default opens nothing. Dungeon Keys is the
// owner's case, exp an ordinary stat, Total Damage the outlier (default 0 and
// its own "off" wording). Positive control on the same page: an unsaved value
// on Dungeon Keys opens its tooltip, so the waits above could have seen one.
async function noteNoneAtDefault({ browser }) {
  const notes = [];
  await withPage(browser, { viewport: VIEWPORTS[1280] }, async ({ page }) => {
    const what = 'at the default, 1280';
    await tab(page, 'loot');
    const first = await $(page, () => document.querySelector('#keys .setting-entry input[type=range]').dataset.key);
    const counts = [await mustHaveEmptyNotes(page, ['keys'], `Loot ${what}`)];
    await tab(page, 'modifiers');
    counts.push(await mustHaveEmptyNotes(page, ['stats', 'percent_stats'], `Modifiers ${what}`));
    for (const [t, sec, key] of [['loot', 'keys', first], ['modifiers', 'stats', 'exp'], ['modifiers', 'percent_stats', 'damage']]) {
      const at = `${what}, ${sec}.${key}`;
      await tab(page, t);
      await makeIdle(page, sec, key, at);
      await away(page);
      const before = await boxes(page);
      await page.hover(`${entryOf(sec, key)} .lbl`);
      await noTooltip(page, OPEN_DELAY_MS + 1000, `${at} hovered`);
      mustBeStill(before, await boxes(page), `${at} hovered`);
      await away(page);
      await $(page, (s) => document.querySelector(s).focus(), `input[type=range][data-sec="${sec}"][data-key="${key}"]`);
      await noTooltip(page, 1000, `${at} focused`);
      mustBeStill(before, await boxes(page), `${at} focused`);
      await away(page);
    }
    await tab(page, 'loot');
    await unsaved(page, 'keys', first, 4);
    await away(page);
    await page.hover(`${entryOf('keys', first)} .lbl`);
    await waitShown(page, noteOf('keys', first), `${what}, positive control: keys.${first} with an unsaved value`);
    await mustBeTooltip(page, noteOf('keys', first), `${what}, positive control`);
    await away(page);
    await restoreDefault(page, 'keys', first);
    const text = await $(page, (s) => document.querySelector(s).textContent, noteOf('keys', first));
    assert(text === '', `${what}: keys.${first} back at its default still has a note "${text}"`);
    notes.push(`1280: ${counts[0]} keys and ${counts[1]} modifier notes empty; keys.${first}, stats.exp, percent_stats.damage opened nothing; the control opened`);
  });
  await withPage(browser, { viewport: VIEWPORTS[900] }, async ({ page }) => {
    const what = 'at the default, 900';
    await tab(page, 'loot');
    const first = await $(page, () => document.querySelector('#keys .setting-entry input[type=range]').dataset.key);
    await mustHaveEmptyNotes(page, ['keys'], `Loot ${what}`);
    await away(page);
    await page.hover(`${entryOf('keys', first)} .lbl`);
    await noTooltip(page, OPEN_DELAY_MS + 1000, `${what}, keys.${first} hovered`);
    if (SHOTS) await page.screenshot({ path: join(SHOTS, 'note-default-loot-900.png') });
    notes.push(`900: keys.${first} opened nothing`);
  });
  return notes.join('; ');
}

// Switched off at the default: the value box says "off" and there is no line
// under the row. Switched on above the default, then off again: round 1's
// sentence, in the flow, both times. Back at the default: empty.
async function noteSwitchedOffByValue({ page }) {
  const done = [];
  for (const [t, sec, key, value, sentence] of [['loot', 'keys', 'dungeon', 4, DUNGEON_AT_4], ['modifiers', 'percent_stats', 'damage', 100, DAMAGE_AT_100]]) {
    const what = `${sec}.${key}`;
    const range = `input[type=range][data-sec="${sec}"][data-key="${key}"]`;
    const sw = `#sw_${sec}_${key}`;
    const note = noteOf(sec, key);
    const state = () => $(page, ([n, r]) => {
      const el = document.querySelector(n);
      return { text: el.textContent, visible: el.checkVisibility(), val: document.querySelector(r).closest('.row').querySelector('.val').textContent.trim() };
    }, [note, range]);
    const click = async () => { await $(page, (s) => document.querySelector(s).click(), sw); await settled(page); await away(page); };
    const commit = async (v) => {
      await $(page, ([s, x]) => { const r = document.querySelector(s); r.value = x; r.dispatchEvent(new Event('input', { bubbles: true })); r.dispatchEvent(new Event('change', { bubbles: true })); }, [range, v]);
      await settled(page);
      await away(page);
    };
    await tab(page, t);
    await makeIdle(page, sec, key, what);
    await away(page);
    const before = await boxes(page);
    await click();
    let got = await state();
    assert(!await $(page, (s) => document.querySelector(s).checked, sw), `${what}: the switch did not turn off`);
    assert(got.val === 'off' && got.text === '' && !got.visible, `${what} switched off at its default: ${JSON.stringify(got)}`);
    mustBeStill(before, await boxes(page), `${what} switched off at its default`);
    await click();
    await commit(value);
    const on = await mustBeInFlow(page, note, `${what} on at ${value}`);
    assert(on.text === sentence, `${what} on at ${value} reads "${on.text}"`);
    await click();
    got = await state();
    assert(got.val === 'off', `${what} switched off at ${value}: the value box reads "${got.val}"`);
    const off = await mustBeInFlow(page, note, `${what} switched off at ${value}`);
    assert(off.text === sentence, `${what} switched off at ${value} reads "${off.text}"`);
    await click();
    await commit(await $(page, (s) => document.querySelector(s).min, range));
    got = await state();
    assert(got.text === '' && !got.visible, `${what} back at its default: ${JSON.stringify(got)}`);
    done.push(what);
  }
  return done.join(', ');
}

// ---- Harness ----

async function withPage(browser, { viewport = VIEWPORTS[1280], offline = false, routes = null } = {}, fn) {
  // Preserve the approved flat-panel geometry; e2e:ember owns Ember's layout.
  const sandbox = await startSandbox({ dist: typeof args.dist === 'string' ? args.dist : null, offline, seed: { theme: 'ledger' } });
  let page = null;
  try {
    page = await openPanel(browser, sandbox, viewport, routes ? { routes } : {});
    await frames(page);
    return await fn({ page, sandbox, browser });
  } finally {
    await page?.context().close();
    await sandbox.stop();
  }
}

const CHECKS = [
  ['mining-label-multiplier', miningLabel],
  ['mining-note-empty-offline', miningNoteOffline, { offline: true }],
  ['mining-status-still-shown', miningStatus],
  ['mods-no-repeated-heading', modsHeading],
  ['mods-one-card-per-mod-qol', modsCardsQol],
  ['mods-one-card-per-mod-items', modsCardsItems],
  ['mods-columns-balanced', modsColumns],
  ['theme-on-setup', themeOnSetup],
  ['footer-credit', footerCredit],
  ['plugin-warning-indicator-placement', indicatorPlacement],
  ['plugin-warning-tooltip-hover', tooltipHover],
  ['plugin-warning-tooltip-focus', tooltipFocus],
  ['plugin-warning-status-bar-tooltip', statusTooltip],
  ['plugin-warning-tooltip-in-viewport', tooltipViewport, { own: true }],
  ['plugin-warning-follows-condition', followsCondition],
  ['plugin-warning-opens-setup', opensSetup],
  ['modifier-groups-separated', groupsSeparated, { own: true }],
  ['helmet-stats-accent', helmetAccent],
  ['slider-note-tooltip-loot', noteLoot, { own: true }],
  ['slider-note-tooltip-modifiers', noteModifiers, { own: true }],
  ['slider-note-tooltip-keyboard', noteKeyboard],
  ['slider-note-tooltip-timing', noteTiming],
  ['slider-note-clear-world', noteWorld, { own: true }],
  ['slider-note-hover-moves-no-row', noteMovesNoRow, { own: true }],
  ['slider-note-none-at-default', noteNoneAtDefault, { own: true }],
  ['slider-note-switched-off-by-value', noteSwitchedOffByValue],
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
console.log(`e2e-polish: ${passed.length}/${EXPECTED.length} checks passed`);
process.exitCode = failures || missing.length ? 1 : 0;
