// The panel's motion, in a real (headless) Edge against the sandbox server:
// the design's `motion.notes` (design/figma-export.json) as app.css builds
// them, the owner's F2 (each slider note named by its range's
// aria-describedby), F4 (both tooltips fade without scaling under
// prefers-reduced-motion) and E4 (the plugin warning's tooltip enters through
// @starting-style, with no data-starting flag).
//
//   node tests/motion.e2e.mjs [--dist <dir>]    (npm run e2e:motion)
//
// How it reads motion: a recorder in the page logs every `transitionrun`
// event (the element, the property, and the CSSTransition's own duration and
// easing from getAnimations()), so a check never races a 120 ms transition by
// polling for it. A property the recorder never saw did not transition: a
// 0 ms duration (data-instant) creates no transition at all. Durations and
// easings are compared with the motion tokens' computed values, never with a
// literal. Reduced motion is page.emulateMedia({ reducedMotion: 'reduce' }),
// and every check that uses it first asserts that matchMedia reports it off
// before the emulation and on after it, so the emulation itself is proven on
// this Edge build rather than assumed.
//
// Every positive has its control beside it: a hover's colour transition next
// to a palette swap that runs none, a pointer's press next to the keyboard's,
// a pointer's tray next to the keyboard's and a rebuilt list's, a pointer's
// Turn off next to the keyboard's. Every check runs on its own sandbox and
// page. The last line is `e2e-motion: <n>/<n> checks passed`.

import { launchBrowser, openPanel, parseArgs, startSandbox, waitBooted, waitSaved } from './lib/browser.mjs';
import { BOOLEAN_MODS } from '../src/enabled-mods.js';
import { OPEN_DELAY_MS } from '../src/lib/slider-note.js';
import { THEMES } from '../src/theme.js';

const args = parseArgs(process.argv.slice(2));
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const EXPECTED = [
  'note-describedby',
  'tooltip-motion-default',
  'tooltip-reduced-motion',
  'plugin-warning-starting-style',
  'keyboard-opens-at-once',
  'reduced-motion-opacity-only',
  'motion-M1',
  'motion-M2',
  'motion-M3',
  'motion-M4',
  'motion-M6',
  'motion-M7',
  'motion-M8',
  'motion-M9',
  'motion-M10',
  'motion-M11',
];
const TABS = ['setup', 'modifiers', 'world', 'loot', 'mods'];
const COLOUR = ['color', 'background-color', 'border-color', 'border-top-color', 'border-right-color', 'border-bottom-color', 'border-left-color'];
const pageIcon = '#pluginWarning button';
const statusIcon = '#statusbar button[aria-label="Open Setup"]';
const tipOf = (icon) => `${icon} + [role="tooltip"]`;
const THREE = ['map_reveal', 'headhunter', 'mod_orb_pickup_radius'];

function assert(ok, message) { if (!ok) throw new Error(message); }
const $ = (page, fn, arg) => page.evaluate(fn, arg);
const frames = (page) => $(page, () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))));
const tab = async (page, name) => { await $(page, (n) => document.querySelector(`.tabbtn[data-tab="${n}"]`).click(), name); await frames(page); };
const post = (page, body) => $(page, (b) => fetch('/api/set', { method: 'POST', body: JSON.stringify(b) }).then((r) => r.json()), body);
const away = async (page) => {
  await page.mouse.move(1, (page.viewportSize()?.height || 800) - 60);
  await $(page, () => document.activeElement?.blur?.());
  await frames(page);
};
async function settled(page) { await wait(30); await waitSaved(page); await wait(60); await frames(page); }

// The recorder (see the header). Installing it again clears the log.
async function record(page) {
  await $(page, () => {
    if (window.__motion) { window.__motion.length = 0; return; }
    window.__motion = [];
    document.addEventListener('transitionrun', (e) => {
      const a = e.target.getAnimations().find((x) => x.transitionProperty === e.propertyName);
      const t = a?.effect?.getTiming();
      window.__motion.push({ el: e.target, prop: e.propertyName, duration: t ? Number(t.duration) : null, easing: t ? String(t.easing) : null });
    }, true);
  });
}
// What the recorder saw on elements matching `selector` (attached or not).
const seen = (page, selector) => $(page, (s) => window.__motion.filter((m) => m.el.matches(s))
  .map(({ prop, duration, easing }) => ({ prop, duration, easing })), selector);
const clearLog = (page) => $(page, () => { window.__motion.length = 0; });
const props = (list) => [...new Set(list.map((m) => m.prop))].sort();
const one = (list, prop) => list.find((m) => m.prop === prop);

// The motion tokens as the page computes them.
const tokens = (page) => $(page, () => {
  const cs = getComputedStyle(document.documentElement);
  const v = (n) => cs.getPropertyValue(n).trim();
  const ms = (n) => { const s = v(n); return /ms$/.test(s) ? parseFloat(s) : parseFloat(s) * 1000; };
  return {
    fast: ms('--motion-duration-fast'), base: ms('--motion-duration-base'),
    standard: v('--motion-easing-standard'), emphasized: v('--motion-easing-emphasized'), hover: v('--motion-easing-hover'),
  };
});
// The page writes a token's curve back as `.23`, a timing as `0.23`: compare the numbers.
const curve = (s) => (String(s).match(/-?\d*\.?\d+/g) || []).map(Number);
const sameCurve = (a, b) => /^cubic-bezier\(/.test(String(a)) && curve(a).length === 4 && curve(a).every((n, i) => Math.abs(n - curve(b)[i]) < 1e-6);
function timed(m, duration, easing, what) {
  assert(m, `${what}: no transition`);
  assert(m.duration === duration, `${what}: ${m.prop} runs ${m.duration} ms, not the token's ${duration}`);
  assert(sameCurve(m.easing, easing), `${what}: ${m.prop} eases ${m.easing}, not the token's ${easing}`);
}

// Reduced motion, with its own control: off before, on after.
async function reduce(page) {
  const before = await $(page, () => matchMedia('(prefers-reduced-motion: reduce)').matches);
  await page.emulateMedia({ reducedMotion: 'reduce' });
  const after = await $(page, () => matchMedia('(prefers-reduced-motion: reduce)').matches);
  assert(before === false && after === true, `reduced-motion emulation did not take: before ${before}, after ${after}`);
}

// ---- Opening things ----

// An idle slider row holding an unsaved value opens its note on hover (as
// polish.e2e.mjs does it): the first Loot key, switched on, at its default.
async function idleKeyNote(page) {
  await tab(page, 'loot');
  const key = await $(page, () => document.querySelector('#keys input[type=range][data-sec="keys"]').dataset.key);
  const sw = `#sw_keys_${key}`;
  if (!await $(page, (s) => document.querySelector(s).checked, sw)) {
    await $(page, (s) => document.querySelector(s).click(), sw);
    await settled(page);
  }
  await $(page, (k) => {
    const r = document.querySelector(`input[type=range][data-sec="keys"][data-key="${k}"]`);
    r.value = 4;
    r.dispatchEvent(new Event('input', { bubbles: true }));
  }, key);
  const note = `.setting-entry:has(input[type=range][data-sec="keys"][data-key="${key}"]) > .note[data-note="${key}"]`;
  assert(await $(page, (s) => { const n = document.querySelector(s); return !n.checkVisibility() && n.textContent.trim() !== ''; }, note),
    `keys.${key}: no hidden idle note with words to open`);
  const range = `input[type=range][data-sec="keys"][data-key="${key}"]`;
  return { key, note, range, label: `.setting-entry:has(${range}) .lbl` };
}
async function hoverNote(page) {
  const { key, note, label } = await idleKeyNote(page);
  await away(page);
  await record(page);
  await page.hover(label);
  await page.waitForFunction((s) => !!document.querySelector(s)?.checkVisibility(), note, { timeout: OPEN_DELAY_MS + 2000 });
  await wait(250);
  return { key, note, got: await seen(page, note) };
}
async function hoverWarning(page, icon = pageIcon) {
  await away(page);
  await record(page);
  await page.hover(icon);
  await page.waitForFunction((s) => !!document.querySelector(s)?.checkVisibility(), tipOf(icon), { timeout: 3000 });
  await wait(250);
  return seen(page, tipOf(icon));
}

// A tray of every boolean mod (the tray form at 1280), or three entries inline.
async function only(page, on) {
  for (const key of BOOLEAN_MODS) await post(page, { key, value: on.includes(key) });
  await page.reload();
  await waitBooted(page);
  await frames(page);
}
async function trayOf(page) {
  await only(page, BOOLEAN_MODS);
  await page.waitForFunction(() => document.getElementById('enabledMods').dataset.form === 'tray', null, { timeout: 5000 });
}
const trayUl = '#enabledMods > ul';

// ---- The checks ----

// F2: every note-carrying range on every tab names its note, and the ids are
// unique and resolve. A range with no note names none (the control).
async function noteDescribedby({ page }) {
  let described = 0;
  let bare = 0;
  for (const name of TABS) {
    await tab(page, name);
    const got = await $(page, () => {
      const bad = [];
      let ok = 0;
      let none = 0;
      for (const r of document.querySelectorAll('#workspace input[type=range]')) {
        if (!r.checkVisibility()) continue;
        const note = r.closest('.setting-entry')?.querySelector(':scope > .note[data-note]');
        const ids = (r.getAttribute('aria-describedby') || '').split(/\s+/).filter(Boolean);
        if (!note) {
          if (r.dataset.sec && ids.length) bad.push(`${r.dataset.sec}.${r.dataset.key}: no note but names ${ids.join(' ')}`);
          else if (r.dataset.sec) none++;
          continue;
        }
        if (!note.id || !ids.includes(note.id) || document.getElementById(note.id) !== note) bad.push(`${r.dataset.sec}.${r.dataset.key}: names ${ids.join(' ') || 'nothing'}, note id ${note.id || 'none'}`);
        else ok++;
      }
      return { ok, none, bad };
    });
    assert(got.bad.length === 0, `${name}: ${got.bad.slice(0, 4).join('; ')}`);
    described += got.ok;
    bare += got.none;
  }
  const ids = await $(page, () => {
    const all = [...document.querySelectorAll('.note[data-note][id]')].map((n) => n.id);
    const every = [...document.querySelectorAll('[id]')].map((e) => e.id);
    return { notes: all.length, dup: all.filter((id) => every.indexOf(id) !== every.lastIndexOf(id)) };
  });
  assert(described >= 10 && ids.dup.length === 0, `${described} ranges named their note; duplicate ids: ${ids.dup.join(',')}`);
  const mining = await $(page, () => {
    const r = document.querySelector('input[type=range][data-sec="drops"][data-key="mining_ore"]');
    const n = document.querySelector('.note[data-note="mining_ore"]');
    return r && n && r.getAttribute('aria-describedby') === n.id && !!n.id;
  });
  assert(mining, 'the Loot mining_ore range does not name its note');
  return `${described} ranges name their note (${ids.notes} note ids, unique), ${bare} ranges without a note name none, mining_ore included`;
}

// Both tooltips, motion allowed: opacity and transform, over fast/emphasized.
// The positive control for tooltip-reduced-motion.
async function tooltipDefault({ page }) {
  const t = await tokens(page);
  const note = await hoverNote(page);
  assert(props(note.got).join() === 'opacity,transform', `the note tooltip ran ${props(note.got).join(',') || 'nothing'}`);
  timed(one(note.got, 'opacity'), t.fast, t.emphasized, 'the note tooltip');
  timed(one(note.got, 'transform'), t.fast, t.emphasized, 'the note tooltip');
  await tab(page, 'world');
  const warn = await hoverWarning(page);
  assert(props(warn).join() === 'opacity,transform', `the plugin warning tooltip ran ${props(warn).join(',') || 'nothing'}`);
  timed(one(warn, 'opacity'), t.fast, t.emphasized, 'the plugin warning tooltip');
  timed(one(warn, 'transform'), t.fast, t.emphasized, 'the plugin warning tooltip');
  return `note keys.${note.key} and the warning: opacity + transform, ${t.fast} ms`;
}

// F4: under reduce, both tooltips fade (more than 0 ms) and never scale.
async function tooltipReduced({ page }) {
  await reduce(page);
  const note = await hoverNote(page);
  assert(props(note.got).join() === 'opacity', `reduced: the note tooltip ran ${props(note.got).join(',') || 'nothing'}`);
  assert(one(note.got, 'opacity').duration > 0, 'reduced: the note fade is 0 ms');
  const scale = await $(page, (s) => getComputedStyle(document.querySelector(s)).transform, note.note);
  assert(scale === 'none', `reduced: the open note is transformed: ${scale}`);
  await tab(page, 'world');
  const warn = await hoverWarning(page);
  assert(props(warn).join() === 'opacity', `reduced: the warning tooltip ran ${props(warn).join(',') || 'nothing'}`);
  assert(one(warn, 'opacity').duration > 0, 'reduced: the warning fade is 0 ms');
  return `both tooltips: opacity only, ${one(warn, 'opacity').duration} ms`;
}

// E4: no data-starting is ever written while the warning's tooltip opens, and
// its entrance still runs (on both icons).
async function warningStartingStyle({ page }) {
  await tab(page, 'world');
  const notes = [];
  for (const icon of [pageIcon, statusIcon]) {
    await $(page, (s) => {
      window.__starting = 0;
      new MutationObserver((rs) => { for (const r of rs) if (r.attributeName === 'data-starting') window.__starting++; })
        .observe(document.querySelector(s), { attributes: true });
    }, tipOf(icon));
    const got = await hoverWarning(page, icon);
    const flags = await $(page, (s) => window.__starting + (document.querySelector(s).hasAttribute('data-starting') ? 1 : 0), tipOf(icon));
    assert(flags === 0, `${icon}: data-starting was written ${flags} times`);
    assert(props(got).includes('opacity'), `${icon}: no opacity entrance (${props(got).join(',') || 'nothing'})`);
    notes.push(props(got).join('+'));
  }
  return `no data-starting; entrances ${notes.join(', ')}`;
}

// Keyboard: a tooltip focused from the keyboard and the tray opened from the
// keyboard show with no transition; the same tray opened by the pointer runs
// one (the control).
async function keyboardAtOnce({ page }) {
  await tab(page, 'world');
  await away(page);
  await record(page);
  await $(page, () => document.getElementById('applyall').focus());
  await page.keyboard.press('Tab');
  await page.waitForFunction((s) => !!document.querySelector(s)?.checkVisibility(), tipOf(pageIcon), { timeout: 2000 });
  await wait(200);
  assert(await $(page, (s) => document.activeElement === document.querySelector(s) && document.activeElement.matches(':focus-visible'), pageIcon), 'Tab did not reach the warning icon');
  const warn = await seen(page, tipOf(pageIcon));
  assert(warn.length === 0, `the keyboard-focused warning tooltip ran ${props(warn).join(',')}`);
  await page.keyboard.press('Escape');

  const { note, range } = await idleKeyNote(page);
  await away(page);
  await clearLog(page);
  await $(page, (s) => document.querySelector(s).focus(), range);
  await page.waitForFunction((s) => !!document.querySelector(s)?.checkVisibility(), note, { timeout: 2000 });
  await wait(200);
  const noteGot = await seen(page, note);
  assert(noteGot.length === 0, `the keyboard-focused note tooltip ran ${props(noteGot).join(',')}`);

  await trayOf(page);
  await record(page);
  await $(page, () => document.querySelector('.enabled-mods-toggle').focus());
  await page.keyboard.press('Enter');
  await wait(300);
  const kb = await seen(page, trayUl);
  assert(await $(page, (s) => document.querySelector(s).checkVisibility() && document.querySelector(s).getAnimations().length === 0, trayUl), 'the keyboard tray is not shown at rest');
  assert(kb.length === 0, `the keyboard-opened tray ran ${props(kb).join(',')}`);
  await page.keyboard.press('Escape');
  await wait(300);
  assert(await seen(page, trayUl).then((l) => l.length === 0), 'Escape closing the tray ran a transition');
  await clearLog(page);
  await page.click('.enabled-mods-toggle');
  await wait(300);
  const ptr = await seen(page, trayUl);
  assert(props(ptr).includes('opacity') && props(ptr).includes('transform'), `the pointer-opened tray ran ${props(ptr).join(',') || 'nothing'} (the control)`);
  return 'warning tooltip, note tooltip and tray: no transition from the keyboard; the pointer tray ran opacity + transform';
}

// Under reduce, every implemented motion animates opacity alone (display is
// the tray's discrete step, not motion): the tooltips, the tray, both toasts,
// a turned-off entry, a hover and a press. At least one opacity fade ran.
async function reducedOpacityOnly({ page }) {
  await reduce(page);
  await record(page);
  await hoverNote(page).catch((e) => { throw new Error('note: ' + e.message); });
  await tab(page, 'world');
  await hoverWarning(page);
  await away(page);
  await page.hover('#applyall');
  await wait(200);
  const box = await $(page, () => { const r = document.getElementById('applyall').getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; });
  await page.mouse.move(box.x, box.y);
  await page.mouse.down();
  await wait(200);
  const pressed = await $(page, () => getComputedStyle(document.getElementById('applyall')).transform);
  await page.mouse.move(1, 400);
  await page.mouse.up();
  assert(pressed === 'none', `reduced: a held button is transformed: ${pressed}`);
  await $(page, () => document.getElementById('autoapply').click());
  await wait(400);
  await trayOf(page);
  await record(page);
  await page.click('.enabled-mods-toggle');
  await wait(300);
  await page.click('#enabledMods > ul .quick-disable');
  await wait(400);
  await settled(page);
  await wait(300);
  const all = await $(page, () => window.__motion.map((m) => ({ prop: m.prop, el: m.el.id || m.el.className || m.el.tagName })));
  const moving = all.filter((m) => m.prop !== 'opacity' && m.prop !== 'display');
  assert(moving.length === 0, `reduced: ${moving.slice(0, 5).map((m) => `${m.el} ${m.prop}`).join('; ')}`);
  const opacity = all.filter((m) => m.prop === 'opacity');
  assert(opacity.length >= 2, `reduced: only ${opacity.length} opacity fades ran (the instrument must fire)`);
  return `${opacity.length} opacity fades (${[...new Set(opacity.map((m) => String(m.el).split(' ')[0]))].join(', ')}), nothing else moved`;
}

// M1: a hover eases its colour in (fast, hover easing) and snaps back; a
// palette swap with nothing hovered runs no colour transition anywhere.
async function m1Hover({ page }) {
  const t = await tokens(page);
  await tab(page, 'world');
  await away(page);
  await record(page);
  await page.hover('#applyall');
  await wait(250);
  const inn = await seen(page, '#applyall');
  const bg = one(inn, 'background-color');
  timed(bg, t.fast, t.hover, 'Apply all hovered');
  await clearLog(page);
  await away(page);
  await wait(250);
  const out = await seen(page, '#applyall');
  assert(!out.some((m) => COLOUR.includes(m.prop)), `leaving the hover ran ${props(out).join(',')}`);
  await clearLog(page);
  for (const theme of THEMES) {
    await $(page, (v) => { document.documentElement.dataset.theme = v; }, theme.value);
    await frames(page);
  }
  await wait(200);
  const swap = await $(page, (c) => window.__motion.filter((m) => c.includes(m.prop)).map((m) => `${m.el.id || m.el.className} ${m.prop}`), COLOUR);
  assert(swap.length === 0, `a palette swap ran colour transitions: ${swap.slice(0, 5).join('; ')}`);
  return `hover background-color ${bg.duration} ms; out and ${THEMES.length} palette swaps: none`;
}

// M2: a button held by the pointer scales to .97 over fast/standard; held
// from the keyboard it does not move.
async function m2Press({ page }) {
  const t = await tokens(page);
  await tab(page, 'world');
  await away(page);
  await record(page);
  const box = await $(page, () => { const r = document.getElementById('applyall').getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; });
  await page.mouse.move(box.x, box.y);
  await wait(200);
  await clearLog(page);
  await page.mouse.down();
  await wait(250);
  const held = await $(page, () => getComputedStyle(document.getElementById('applyall')).transform);
  const got = await seen(page, '#applyall');
  // Released away from the button: no click, nothing applied.
  await page.mouse.move(1, 400);
  await page.mouse.up();
  timed(one(got, 'transform'), t.fast, t.standard, 'Apply all pressed');
  const m = held.match(/^matrix\(([-\d.]+)/);
  assert(m && Math.abs(parseFloat(m[1]) - 0.97) < 0.005, `the held button's transform is ${held}`);
  await away(page);
  await record(page);
  await $(page, () => document.getElementById('autoapply').focus());
  await page.keyboard.press('Tab');
  assert(await $(page, () => document.activeElement?.id === 'applyall' && document.activeElement.matches(':focus-visible')), 'Tab did not reach Apply all');
  await page.keyboard.down('Space');
  await wait(200);
  const kbHeld = await $(page, () => getComputedStyle(document.getElementById('applyall')).transform);
  const kb = await seen(page, '#applyall');
  // Blurred before the key comes up: no click, nothing applied.
  await $(page, () => document.activeElement.blur());
  await page.keyboard.up('Space');
  assert(kbHeld === 'none' && !kb.some((x) => x.prop === 'transform'), `a keyboard press moved the button: ${kbHeld}, ${props(kb).join(',')}`);
  return `pointer: ${held}; keyboard: none`;
}

// M3: keyboard actions never animate: a keyboard Turn off removes its entry
// with no fade and shows its undo toast with no entrance (the tooltips and the
// tray from the keyboard are keyboard-opens-at-once).
async function m3Keyboard({ page }) {
  await only(page, THREE);
  await record(page);
  await $(page, () => document.querySelector('#enabledMods .quick-disable[data-for="headhunter"]').focus());
  await page.keyboard.press('Enter');
  await page.waitForFunction(() => !!document.querySelector('.undo-toast'), null, { timeout: 3000 });
  await settled(page);
  await wait(250);
  const entry = await seen(page, '.enabled-mod[data-for="headhunter"]');
  const toast = await seen(page, '.undo-toast');
  const copy = await seen(page, '.enabled-mod-ghost');
  assert(entry.length === 0 && toast.length === 0 && copy.length === 0,
    `a keyboard Turn off animated: entry ${props(entry).join(',')}, toast ${props(toast).join(',')}, copy ${props(copy).join(',')}`);
  await $(page, () => document.querySelector('.undo-toast-button').focus());
  await page.keyboard.press('Enter');
  await wait(100);
  assert(await $(page, () => !document.querySelector('.undo-toast')), 'a keyboard Undo left the toast fading');
  // A switch toggled from the keyboard: its #toast shows and leaves with no transition.
  await settled(page);
  await clearLog(page);
  await $(page, () => document.getElementById('autoapply').focus());
  await page.keyboard.press('Space');
  await page.waitForFunction(() => document.getElementById('toast').classList.contains('show'), null, { timeout: 3000 });
  await wait(250);
  const kbToast = await seen(page, '#toast');
  assert(kbToast.length === 0, `a keyboard toggle's toast ran ${props(kbToast).join(',')}`);
  return 'keyboard Turn off, Undo and a switch: no transition, the toasts at once';
}

// M4: the pointer's tray scales in from the count (opacity and transform over
// base/emphasized, origin top right) and out over fast/standard; a list
// rebuilt while it is open appears at once.
async function m4Tray({ page }) {
  const t = await tokens(page);
  await trayOf(page);
  await record(page);
  await page.click('.enabled-mods-toggle');
  await wait(350);
  const open = await seen(page, trayUl);
  timed(one(open, 'opacity'), t.base, t.emphasized, 'the tray opening');
  timed(one(open, 'transform'), t.base, t.emphasized, 'the tray opening');
  const origin = await $(page, (s) => getComputedStyle(document.querySelector(s)).transformOrigin, trayUl);
  const w = await $(page, (s) => document.querySelector(s).getBoundingClientRect().width, trayUl);
  assert(origin.startsWith(`${Math.round(w)}`) || origin.startsWith(`${w}`), `the tray scales from ${origin}, not its top right (width ${w})`);
  // Turn one off inside the open tray: the rebuilt list must not replay the entrance.
  await $(page, (s) => document.querySelector(s).setAttribute('data-probe-old', ''), trayUl);
  await clearLog(page);
  await page.click('#enabledMods > ul .quick-disable');
  await settled(page);
  await wait(300);
  const rebuilt = await seen(page, `${trayUl}:not([data-probe-old])`);
  assert(await $(page, (s) => !!document.querySelector(s), `${trayUl}:not([data-probe-old])`), 'the list was not rebuilt');
  assert(rebuilt.length === 0, `the list rebuilt in the open tray ran ${props(rebuilt).join(',')}`);
  await page.mouse.move(1, 400);
  await clearLog(page);
  const outside = await $(page, () => { const r = document.getElementById('pageTitle').getBoundingClientRect(); return { x: r.left + 10, y: r.top + r.height / 2 }; });
  await page.mouse.click(outside.x, outside.y);
  await wait(300);
  const close = await seen(page, trayUl);
  timed(one(close, 'opacity'), t.fast, t.standard, 'the tray closing');
  timed(one(close, 'transform'), t.fast, t.standard, 'the tray closing');
  assert(await $(page, (s) => !document.querySelector(s).checkVisibility(), trayUl), 'the tray is still shown after closing');
  // A measure after the close never replays it.
  await clearLog(page);
  await page.setViewportSize({ width: 1260, height: 800 });
  await wait(600);
  const replay = await seen(page, trayUl);
  assert(replay.length === 0, `a resize replayed the tray's close: ${props(replay).join(',')}`);
  return `open ${t.base} ms and close ${t.fast} ms, from ${origin}; rebuilt and resized: none`;
}

// M6: #toast rises in over base and sinks out over fast; the undo toast the
// same, and leaves (then is removed) after the pointer's Undo.
async function m6Toasts({ page }) {
  const t = await tokens(page);
  await only(page, THREE);
  await record(page);
  await $(page, () => document.getElementById('autoapply').click());
  await page.waitForFunction(() => document.getElementById('toast').classList.contains('show'), null, { timeout: 3000 });
  await wait(300);
  const inn = await seen(page, '#toast');
  timed(one(inn, 'opacity'), t.base, t.standard, '#toast in');
  timed(one(inn, 'transform'), t.base, t.standard, '#toast in');
  await clearLog(page);
  await page.waitForFunction(() => !document.getElementById('toast').classList.contains('show'), null, { timeout: 5000 });
  await wait(300);
  const out = await seen(page, '#toast');
  timed(one(out, 'opacity'), t.fast, t.standard, '#toast out');
  await settled(page);
  await clearLog(page);
  await page.click('#enabledMods .quick-disable[data-for="headhunter"]');
  await page.waitForFunction(() => !!document.querySelector('.undo-toast'), null, { timeout: 3000 });
  await wait(300);
  const undoIn = await seen(page, '.undo-toast');
  timed(one(undoIn, 'opacity'), t.base, t.standard, 'the undo toast in');
  timed(one(undoIn, 'transform'), t.base, t.standard, 'the undo toast in');
  await settled(page);
  await clearLog(page);
  await page.click('.undo-toast-button');
  await wait(20);
  const leaving = await $(page, () => { const el = document.querySelector('.undo-toast'); return el ? { leaving: el.hasAttribute('data-leaving'), inert: el.inert } : null; });
  await wait(400);
  const undoOut = await seen(page, '.undo-toast');
  timed(one(undoOut, 'opacity'), t.fast, t.standard, 'the undo toast out');
  assert(leaving && leaving.leaving && leaving.inert, `the undo toast did not leave inert: ${JSON.stringify(leaving)}`);
  assert(await $(page, () => !document.querySelector('.undo-toast')), 'the undo toast was not removed after its fade');
  return `#toast and the undo toast: in ${t.base} ms, out ${t.fast} ms`;
}

// M7: the pointer's Turn off: the list is rebuilt without the entry, and the
// placeholder holding its place starts as a copy of it (its name, not an
// .enabled-mod, nothing hit-testable) that fades and shrinks to .95 over
// fast/standard, once. The keyboard's Turn off has none (motion-M3).
async function m7Removed({ page }) {
  const t = await tokens(page);
  await only(page, THREE);
  const name = await $(page, () => document.querySelector('#enabledMods li[data-for="headhunter"] .enabled-mod-name').textContent.trim());
  const at = await $(page, () => { const r = document.querySelector('#enabledMods .quick-disable[data-for="headhunter"]').getBoundingClientRect(); return { x: r.left + r.width / 2, y: r.top + r.height / 2 }; });
  await record(page);
  await page.mouse.move(at.x, at.y);
  await page.mouse.click(at.x, at.y);
  await page.waitForFunction(() => !document.querySelector('#enabledMods li.enabled-mod[data-for="headhunter"]'), null, { timeout: 3000 });
  const ghost = await $(page, (p) => {
    const g = document.querySelector('#enabledMods .enabled-mod-ghost');
    const hit = document.elementFromPoint(p.x, p.y);
    return g ? { fading: g.hasAttribute('data-fading'), text: g.querySelector('.enabled-mod-name')?.textContent.trim(), ids: g.querySelectorAll('[id], [data-for]').length,
      hidden: g.getAttribute('aria-hidden'), hitGhost: !!hit && g.contains(hit), entries: document.querySelectorAll('#enabledMods li.enabled-mod').length } : null;
  }, at);
  await wait(250);
  const got = await seen(page, '.enabled-mod-ghost');
  assert(ghost && ghost.fading && ghost.text === name && ghost.ids === 0 && ghost.hidden === 'true' && !ghost.hitGhost && ghost.entries === 2,
    `the placeholder is not the entry's fading copy: ${JSON.stringify(ghost)}`);
  timed(one(got, 'opacity'), t.fast, t.standard, 'the removed entry');
  timed(one(got, 'transform'), t.fast, t.standard, 'the removed entry');
  const end = await $(page, () => { const g = document.querySelector('#enabledMods .enabled-mod-ghost'); return g ? { opacity: getComputedStyle(g).opacity, transform: getComputedStyle(g).transform } : null; });
  assert(!end || (end.opacity === '0' && /^matrix\(0\.95,/.test(end.transform)), `the copy did not end faded at .95: ${JSON.stringify(end)}`);
  return `"${name}" copy: opacity + transform ${t.fast} ms, ends ${end ? end.transform : 'removed'}`;
}

const CHECKS = [
  ['note-describedby', noteDescribedby],
  ['tooltip-motion-default', tooltipDefault],
  ['tooltip-reduced-motion', tooltipReduced],
  ['plugin-warning-starting-style', warningStartingStyle],
  ['keyboard-opens-at-once', keyboardAtOnce],
  ['reduced-motion-opacity-only', reducedOpacityOnly],
  ['motion-M1', m1Hover],
  ['motion-M2', m2Press],
  ['motion-M3', m3Keyboard],
  ['motion-M4', m4Tray],
  ['motion-M6', m6Toasts],
  ['motion-M7', m7Removed],
  // The reduced-motion row, F2, F4 and E4 are the required checks above, run again under the row's name.
  ['motion-M8', reducedOpacityOnly],
  ['motion-M9', noteDescribedby],
  ['motion-M10', tooltipReduced],
  ['motion-M11', warningStartingStyle],
];

async function withPage(browser, fn) {
  const sandbox = await startSandbox({ dist: typeof args.dist === 'string' ? args.dist : null });
  let page = null;
  try {
    page = await openPanel(browser, sandbox);
    await frames(page);
    return await fn({ page, sandbox });
  } finally {
    await page?.context().close();
    await sandbox.stop();
  }
}

const browser = await launchBrowser();
const passed = [];
let failures = 0;
try {
  for (const [name, fn] of CHECKS) {
    try {
      const note = await withPage(browser, fn);
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
console.log(`e2e-motion: ${passed.length}/${EXPECTED.length} checks passed`);
process.exitCode = failures || missing.length ? 1 : 0;
