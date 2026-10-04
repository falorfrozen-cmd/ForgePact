// The restyle's review fixes, in a real (headless) Edge against the sandbox
// server: the tray as a popover, Turn off's undo toast, announcement, focus
// move and reflow freeze, the hold on the inline/tray switch, the idle switch
// marker, and the smaller code-side findings.
//
//   node tests/review-fixes.e2e.mjs [--dist <dir>] [--shots <dir>]    (npm run e2e:review)
//
// Asserts through the DOM (attributes, checkVisibility(),
// getBoundingClientRect(), elementFromPoint(), scroll sizes,
// document.activeElement), sandbox.state(), sandbox.readCmds() and the POSTs
// the page sends - never through a computed style. The toast and
// announcement checks take their words from src/lib/enabled-mods-copy.js, so
// the copy itself is pinned only where the plan pins it.
//
// The same-posts checks are the derived oracle's `same` relation applied to
// Undo: the POSTs and commands of the control's own "on" step, then Turn
// off, then Undo, whose POSTs must be byte-identical and in the same order,
// and whose commands must be equal. Cases: one ordinary control and the
// outliers (a boolean, a switched slider, a select, and density with its own
// handler).
//
// --shots <dir> also writes screenshots of the states the design's
// composites cannot show (the tray open, the undo toast) for the owner.

import { readFileSync, mkdirSync } from 'node:fs';
import { join } from 'node:path';
import { launchBrowser, openPanel, parseArgs, postSet, startSandbox, waitBooted, waitSaved } from './lib/browser.mjs';
import { BOOLEAN_MODS } from '../src/enabled-mods.js';
import { UNDO_TEXTS, ENTRY_TITLE_JOINER, withName } from '../src/lib/enabled-mods-copy.js';
import { FREEZE_MAX_MS, UNDO_VISIBLE_MS } from '../src/lib/enabled-mods-undo.js';
import { HOLD_IDLE_MS } from '../src/lib/enabled-mods-form.js';
import { OPEN_DELAY_MS } from '../src/lib/slider-note.js';

const args = parseArgs(process.argv.slice(2));
const SHOTS = typeof args.shots === 'string' ? args.shots : null;
if (SHOTS) mkdirSync(SHOTS, { recursive: true });
const HEIGHT = 800;
const COMPLETE = { exeExists: true, patched: true, aurieCore: true, yytk: true, plugin: true };
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const EXPECTED = [
  'tray-popover-closed-by-default',
  'tray-popover-keyboard-open-focus',
  'tray-popover-escape-returns-focus',
  'tray-popover-outside-click-closes',
  'tray-popover-tab-change-closes',
  'tray-scroll-cue',
  'tray-long-name-not-clipped',
  'undo-toast-shows-name',
  'undo-boolean-same-posts',
  'undo-switched-slider-same-posts',
  'undo-select-same-posts',
  'undo-density-same-posts',
  'undo-toast-times-out',
  'undo-toast-own-timer',
  'turn-off-announced',
  'turn-off-focus-next',
  'turn-off-focus-previous',
  'turn-off-focus-heading',
  'turn-off-freeze-under-pointer',
  'form-held-while-key-down',
  'form-held-while-pointer-down',
  'idle-switch-marker',
  'setup-install-primary-when-chain-incomplete',
  'open-setup-hidden-on-setup',
  'idle-note-quiet',
  'empty-count-hidden',
  'pool-scroll-cue',
  'entry-title-full-path',
];
const THREE = ['map_reveal', 'headhunter', 'mod_orb_pickup_radius'];
const EXP = 'input[type=range][data-sec="stats"][data-key="exp"]';

function assert(ok, message) { if (!ok) throw new Error(message); }
const $ = (page, fn, arg) => page.evaluate(fn, arg);
// Setup writes, straight to the sandbox from this process, not by the page's
// fetch(): one of those failed on a CI runner with only "TypeError: Failed to
// fetch" (PR run 36390046850). lib/browser.mjs postSet says why, and a failed
// write names its socket error or the sandbox's answer. It is never retried.
const post = postSet;
const frames = (page) => $(page, () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))));
const formOf = (page) => $(page, () => document.getElementById('enabledMods').dataset.form);
const visible = (page, selector) => $(page, (s) => !!document.querySelector(s)?.checkVisibility(), selector);
const active = (page) => $(page, () => {
  const el = document.activeElement;
  return el ? { tag: el.tagName.toLowerCase(), cls: el.className, for: el.dataset?.for || null, heading: el.matches('#enabledMods > h2') } : null;
});

async function settled(page) {
  await wait(30);
  await waitSaved(page);
  await wait(60);
  await frames(page);
}

async function reload(page) {
  await page.reload();
  await waitBooted(page);
  await frames(page);
}

// Every boolean mod off but `on`, then `extra` bodies, then a reload.
async function only(page, on, extra = []) {
  for (const key of BOOLEAN_MODS) await post(page, { key, value: on.includes(key) });
  for (const body of extra) await post(page, body);
  await reload(page);
}

function capturePosts(page) {
  const posts = [];
  page.on('request', (r) => { if (r.method() === 'POST' && r.url().endsWith('/api/set')) posts.push(r.postData()); });
  return posts;
}

// The POSTs and command lines one action causes.
async function effects(ctx, action) {
  ctx.posts.length = 0;
  ctx.sandbox.truncateCmds();
  await action();
  await settled(ctx.page);
  return { posts: [...ctx.posts], cmds: ctx.sandbox.readCmds() };
}

const openTray = async (page) => {
  if (await $(page, () => document.querySelector('.enabled-mods-toggle')?.getAttribute('aria-expanded')) !== 'true') await page.click('.enabled-mods-toggle');
  await frames(page);
};
const trayOpen = (page) => $(page, () => document.querySelector('.enabled-mods-toggle')?.getAttribute('aria-expanded') === 'true' &&
  !!document.querySelector('#enabledMods > ul')?.checkVisibility());

async function popover(ctx) {
  const { page, passed } = ctx;
  await only(page, BOOLEAN_MODS);
  assert(await formOf(page) === 'tray', 'Twelve entries at 1280 are not in the tray');
  assert(await $(page, () => {
    const t = document.querySelector('.enabled-mods-toggle');
    return !!t && !t.id && t.getAttribute('aria-labelledby') === 'enabledModsCount' && t.contains(document.getElementById('enabledModsCount'));
  }), 'The tray toggle is not the id-less button named by the count');
  assert(!await trayOpen(page) && !await visible(page, '#enabledMods .quick-disable'), 'The tray is open on load');
  passed.push('tray-popover-closed-by-default');

  await $(page, () => document.querySelector('.enabled-mods-toggle').focus());
  await page.keyboard.press('Enter');
  await frames(page);
  const first = await $(page, () => document.querySelector('#enabledMods > ul .quick-disable')?.dataset.for);
  const focused = await active(page);
  assert(await trayOpen(page) && focused?.for === first && /quick-disable/.test(focused.cls), 'Opening from the keyboard did not focus the first Turn off');
  passed.push('tray-popover-keyboard-open-focus');

  await page.keyboard.press('Escape');
  await frames(page);
  assert(!await trayOpen(page) && await $(page, () => document.activeElement === document.querySelector('.enabled-mods-toggle')),
    'Escape did not close the tray and return focus to the count');
  passed.push('tray-popover-escape-returns-focus');

  await page.click('.enabled-mods-toggle');
  await frames(page);
  assert(await trayOpen(page), 'A click on the count did not open the tray');
  if (SHOTS) await page.screenshot({ path: join(SHOTS, 'tray-open-1280.png') });
  // A click inside keeps it open.
  await page.click('#enabledMods > ul .enabled-mod-name');
  await frames(page);
  assert(await trayOpen(page), 'A click inside the tray closed it');
  const outside = await $(page, () => { const r = document.getElementById('pageTitle').getBoundingClientRect(); return { x: r.left + 10, y: r.top + r.height / 2 }; });
  await page.mouse.click(outside.x, outside.y);
  await frames(page);
  assert(!await trayOpen(page), 'An outside click left the tray open');
  passed.push('tray-popover-outside-click-closes');

  await page.click('.enabled-mods-toggle');
  await frames(page);
  assert(await trayOpen(page), 'The tray did not reopen');
  // The tab's own handler, with no pointer involved: only the tab change can close it.
  await $(page, () => document.querySelector('.tabbtn[data-tab="loot"]').click());
  await frames(page);
  assert(!await trayOpen(page), 'A tab change left the tray open');
  passed.push('tray-popover-tab-change-closes');

  await page.click('.enabled-mods-toggle');
  await frames(page);
  const cue = await $(page, () => {
    const ul = document.querySelector('#enabledMods > ul');
    return { more: ul.hasAttribute('data-more-below'), scrolls: ul.scrollHeight > ul.clientHeight, entries: ul.querySelectorAll('li.enabled-mod').length };
  });
  assert(cue.entries > 8 && cue.scrolls && cue.more, `No scroll cue on a tray of ${cue.entries}: ${JSON.stringify(cue)}`);
  await $(page, () => { const ul = document.querySelector('#enabledMods > ul'); ul.scrollTop = ul.scrollHeight; });
  await wait(50);
  await frames(page);
  assert(!await $(page, () => document.querySelector('#enabledMods > ul').hasAttribute('data-more-below')), 'The scroll cue stayed at the bottom');
  passed.push('tray-scroll-cue');

  const clip = await $(page, () => {
    const names = [...document.querySelectorAll('#enabledMods > ul .enabled-mod-name')];
    const name = names.reduce((a, b) => (b.textContent.length > a.textContent.length ? b : a));
    name.scrollIntoView({ block: 'nearest' });
    const range = document.createRange();
    range.selectNodeContents(name);
    const lines = new Set([...range.getClientRects()].map((r) => Math.round(r.top))).size;
    const ul = document.querySelector('#enabledMods > ul').getBoundingClientRect();
    const r = name.getBoundingClientRect();
    return { text: name.textContent, lines, clippedX: name.scrollWidth > name.clientWidth + 1, clippedY: name.scrollHeight > name.clientHeight + 1,
      inside: r.left >= ul.left && r.right <= ul.right };
  });
  assert(clip.lines >= 1 && clip.lines <= 2 && !clip.clippedX && !clip.clippedY && clip.inside,
    `The longest tray name is clipped or runs past two lines: ${JSON.stringify(clip)}`);
  passed.push('tray-long-name-not-clipped');
  ctx.note = `tray of ${cue.entries}; "${clip.text}" on ${clip.lines} line(s)`;
}

async function undo(ctx) {
  const { page, sandbox, passed } = ctx;
  const read = async () => (await sandbox.state()).cfg;
  await only(page, ['headhunter']);
  const name = await $(page, () => document.querySelector('#enabledMods li[data-for="headhunter"] .enabled-mod-name').textContent.trim());
  await page.click('#enabledMods .quick-disable[data-for="headhunter"]');
  await settled(page);
  const toast = await $(page, () => {
    const t = document.querySelector('.undo-toast');
    if (!t) return null;
    const b = t.querySelector('button');
    const wrap = document.getElementById('wrap').getBoundingClientRect();
    return { text: t.querySelector('.undo-toast-text')?.textContent, button: b?.textContent, label: b?.getAttribute('aria-label'), id: t.id,
      visible: t.checkVisibility(), outside: !t.closest('.page-actions, #statusbar, .tab-card'), belowContent: t.getBoundingClientRect().top >= wrap.bottom - 1 };
  });
  assert(toast && toast.visible, 'No undo toast after a Turn off');
  assert(toast.text === withName(UNDO_TEXTS.turnedOff, name) && toast.button === UNDO_TEXTS.undo && toast.label === withName(UNDO_TEXTS.undoLabel, name),
    'The undo toast does not name the entry: ' + JSON.stringify(toast));
  assert(!toast.id && toast.outside && toast.belowContent, 'The undo toast has an id, sits in a control area or covers the page: ' + JSON.stringify(toast));
  if (SHOTS) await page.screenshot({ path: join(SHOTS, 'undo-toast-1280.png') });
  passed.push('undo-toast-shows-name');

  const announced = await $(page, (want) => [...document.querySelectorAll('[role="status"][aria-live="polite"]')]
    .some((el) => el.id !== 'toast' && el.id !== 'saveIndicator' && el.textContent === want), withName(UNDO_TEXTS.announceOff, name));
  assert(announced, 'The Turn off was not announced in its own polite region');
  passed.push('turn-off-announced');

  const same = async (label, turnOn, id, beforeOn) => {
    await beforeOn();
    await settled(page);
    const on = await effects(ctx, turnOn);
    assert(on.posts.length > 0, `${label}: the control's own on sent nothing (the instrument must fire)`);
    await page.click(`#enabledMods .quick-disable[data-for="${id}"]`);
    await settled(page);
    await page.mouse.move(1, HEIGHT / 2);
    const back = await effects(ctx, () => page.click('.undo-toast-button'));
    assert(JSON.stringify(back.posts) === JSON.stringify(on.posts), `${label}: Undo posted ${JSON.stringify(back.posts)}, on posted ${JSON.stringify(on.posts)}`);
    assert(JSON.stringify(back.cmds) === JSON.stringify(on.cmds), `${label}: Undo sent ${back.cmds.join('|')}, on sent ${on.cmds.join('|')}`);
    assert(await $(page, (i) => !!document.querySelector(`#enabledMods li.enabled-mod[data-for="${i}"]`), id), `${label}: the entry did not come back`);
  };
  const click = (s) => () => $(page, (sel) => document.querySelector(sel).click(), s);
  const slide = (s, which) => () => $(page, ([sel, w]) => {
    const r = document.querySelector(sel);
    r.value = w === 'max' ? r.max : r.min;
    r.dispatchEvent(new Event('input', { bubbles: true }));
    r.dispatchEvent(new Event('change', { bubbles: true }));
  }, [s, which]);
  const offFirst = (s) => async () => { if (await $(page, (sel) => document.querySelector(sel).checked, s)) await click(s)(); };

  await same('headhunter', click('#headhunter'), 'headhunter', offFirst('#headhunter'));
  assert((await read()).headhunter, 'Undo left headhunter off');
  passed.push('undo-boolean-same-posts');

  await same('sw_stats_exp', click('#sw_stats_exp'), 'sw_stats_exp', async () => { await slide(EXP, 'max')(); await settled(page); await offFirst('#sw_stats_exp')(); });
  passed.push('undo-switched-slider-same-posts');

  const select = () => $(page, () => {
    const s = document.getElementById('mod_skill_timer_style');
    s.value = 'arc';
    s.dispatchEvent(new Event('change', { bubbles: true }));
  });
  await same('mod_skill_timer_style', select, 'mod_skill_timer_style', () => $(page, () => {
    const s = document.getElementById('mod_skill_timer_style');
    if (s.value !== 'off') { s.value = 'off'; s.dispatchEvent(new Event('change', { bubbles: true })); }
  }));
  assert((await read()).mod_skill_timer_style === 'arc', 'Undo did not put the select back on arc');
  passed.push('undo-select-same-posts');

  await same('den_on', click('#den_on'), 'den_on', async () => { await slide('#den', 'max')(); await settled(page); await offFirst('#den_on')(); });
  passed.push('undo-density-same-posts');

  await page.click('#enabledMods .quick-disable[data-for="headhunter"]');
  await settled(page);
  await page.mouse.move(1, HEIGHT / 2);
  assert(await visible(page, '.undo-toast'), 'No undo toast before the timeout check');
  await wait(UNDO_VISIBLE_MS + 700);
  assert(!await $(page, () => !!document.querySelector('.undo-toast')) &&
    !await $(page, () => document.documentElement.hasAttribute('data-undo-open')), `The undo toast outlived ${UNDO_VISIBLE_MS} ms`);
  passed.push('undo-toast-times-out');
}

// A toast that was hovered and then used keeps no timer of its own: hovered
// late (LATE ms left) and left after its Undo, it must not hide the next Turn
// off's toast when those LATE ms run out. That toast keeps its full time, so
// it is still up at LATE + 2 s. (Hovering paused the used toast's timer and
// leaving it started that timer again, which then hid whichever toast was
// showing: the perf suite's third Undo found no toast.)
async function undoTimer(ctx) {
  const { page, passed } = ctx;
  const LATE = 1500;
  await only(page, ['map_reveal', 'headhunter']);
  assert(await formOf(page) === 'inline', 'Two entries at 1280 are not inline');
  const name = await $(page, () => document.querySelector('#enabledMods li[data-for="headhunter"] .enabled-mod-name').textContent.trim());
  await page.click('#enabledMods .quick-disable[data-for="map_reveal"]');
  await page.mouse.move(1, HEIGHT / 2);
  await wait(UNDO_VISIBLE_MS - LATE);
  await page.hover('.undo-toast-button');
  await page.click('.undo-toast-button');
  await page.mouse.move(1, HEIGHT / 2);
  await settled(page);
  await page.click('#enabledMods .quick-disable[data-for="headhunter"]');
  await page.mouse.move(1, HEIGHT / 2);
  await wait(LATE + 2000);
  const next = await $(page, () => document.querySelector('.undo-toast:not([data-leaving]) .undo-toast-text')?.textContent ?? null);
  assert(next === withName(UNDO_TEXTS.turnedOff, name),
    `The next Turn off's toast was hidden by the used toast's timer: ${JSON.stringify(next)} at ${LATE + 2000} ms of ${UNDO_VISIBLE_MS}`);
  passed.push('undo-toast-own-timer');
}

async function focus(ctx) {
  const { page, passed } = ctx;
  const byKeyboard = async (id) => {
    await $(page, (i) => document.querySelector(`#enabledMods .quick-disable[data-for="${i}"]`).focus(), id);
    await page.keyboard.press('Enter');
    await settled(page);
    await page.waitForFunction((i) => !document.querySelector(`#enabledMods li.enabled-mod[data-for="${i}"]`), id);
    await frames(page);
    return active(page);
  };
  await only(page, THREE);
  let order = await $(page, () => [...document.querySelectorAll('#enabledMods li.enabled-mod')].map((li) => li.dataset.for));
  let got = await byKeyboard(order[0]);
  assert(got?.for === order[1] && /quick-disable/.test(got.cls), `Focus went to ${JSON.stringify(got)}, not the next entry ${order[1]}`);
  passed.push('turn-off-focus-next');

  order = order.slice(1);
  got = await byKeyboard(order[1]);
  assert(got?.for === order[0] && /quick-disable/.test(got.cls), `Focus went to ${JSON.stringify(got)}, not the previous entry ${order[0]}`);
  passed.push('turn-off-focus-previous');

  got = await byKeyboard(order[0]);
  assert(got?.heading, `Focus went to ${JSON.stringify(got)}, not the heading`);
  passed.push('turn-off-focus-heading');

  await only(page, THREE);
  const before = await $(page, () => {
    const buttons = [...document.querySelectorAll('#enabledMods .quick-disable')].map((b) => b.getBoundingClientRect());
    return { first: { x: buttons[0].left + buttons[0].width / 2, y: buttons[0].top + buttons[0].height / 2 }, second: buttons[1].left };
  });
  await page.mouse.click(before.first.x, before.first.y);
  const clickedAt = Date.now();
  await settled(page);
  await page.waitForFunction(() => document.querySelectorAll('#enabledMods li.enabled-mod').length === 2);
  const under = await $(page, (p) => {
    const el = document.elementFromPoint(p.x, p.y);
    return el?.closest('.quick-disable') ? el.closest('.quick-disable').dataset.for : null;
  }, before.first);
  assert(Date.now() - clickedAt < FREEZE_MAX_MS, 'The list took longer than the freeze to rebuild');
  assert(under === null, `After the Turn off, ${under}'s Turn off slid under the pointer`);
  await page.mouse.move(1, HEIGHT / 2);
  await frames(page);
  const second = await $(page, () => document.querySelectorAll('#enabledMods .quick-disable')[0].getBoundingClientRect().left);
  assert(Math.abs(second - before.second) > 1, 'The second Turn off never moved once the pointer left');
  passed.push('turn-off-freeze-under-pointer');
}

// A width where `base` is inline and one more entry overfills the row:
// the widest width at which base plus that entry is in the tray.
async function edgeWidth(page, base, extra) {
  const startWidth = await $(page, () => document.documentElement.dataset.theme === 'ember' ? 1600 : 1280);
  await page.setViewportSize({ width: startWidth, height: HEIGHT });
  await only(page, base, extra.on);
  let W = 0;
  for (let width = startWidth; width >= 480; width -= 10) {
    await page.setViewportSize({ width, height: HEIGHT });
    await frames(page);
    if (await formOf(page) === 'tray') { W = width; break; }
  }
  assert(W > 0, 'No width puts the longer list in the tray');
  await only(page, base, extra.off);
  assert(await formOf(page) === 'inline', `At ${W}px the shorter list is not inline`);
  return W;
}

async function hold(ctx) {
  const { page, passed } = ctx;
  let W = await edgeWidth(page, THREE, {
    on: [{ section: 'stats', key: 'exp', value: 2 }], off: [{ section: 'stats', key: 'exp', value: 1 }],
  });
  await $(page, () => document.querySelector('.tabbtn[data-tab="modifiers"]').click());
  await $(page, (s) => document.querySelector(s).focus(), EXP);
  await page.keyboard.down('ArrowRight');
  await settled(page);
  await page.waitForFunction(() => !!document.querySelector('#enabledMods li.enabled-mod[data-for="sw_stats_exp"]'));
  await wait(HOLD_IDLE_MS + 200);
  assert(await formOf(page) === 'inline', `At ${W}px the form changed while a key was down`);
  await page.keyboard.up('ArrowRight');
  await wait(HOLD_IDLE_MS + 300);
  await frames(page);
  assert(await formOf(page) === 'tray', `At ${W}px the form never followed the list once the key was let go`);
  passed.push('form-held-while-key-down');

  const base = ['map_reveal', 'mod_orb_pickup_radius', 'tyrant'];
  W = await edgeWidth(page, base, { on: [{ key: 'headhunter', value: true }], off: [{ key: 'headhunter', value: false }] });
  const spot = await $(page, () => { const r = document.getElementById('pageDescription').getBoundingClientRect(); return { x: r.left + 5, y: r.top + r.height / 2 }; });
  await page.mouse.move(spot.x, spot.y);
  await page.mouse.down();
  await $(page, () => document.getElementById('headhunter').click());
  await settled(page);
  await page.waitForFunction(() => !!document.querySelector('#enabledMods li.enabled-mod[data-for="headhunter"]'));
  await wait(HOLD_IDLE_MS + 200);
  assert(await formOf(page) === 'inline', `At ${W}px the form changed while a pointer button was down`);
  await page.mouse.up();
  await wait(HOLD_IDLE_MS + 300);
  await frames(page);
  assert(await formOf(page) === 'tray', `At ${W}px the form never followed the list once the button was let go`);
  passed.push('form-held-while-pointer-down');
  await page.setViewportSize({ width: 1280, height: HEIGHT });
}

async function switches(ctx) {
  const { page, passed } = ctx;
  await only(page, []);
  assert(await $(page, () => document.getElementById('enabledModsCount').textContent === '0 on' &&
    !document.getElementById('enabledModsCount').checkVisibility()), 'The empty list shows its count');
  passed.push('empty-count-hidden');

  const live = () => $(page, () => document.getElementById('sw_stats_exp').closest('label.slider-switch').hasAttribute('data-live'));
  const slide = (which) => $(page, ([s, w]) => {
    const r = document.querySelector(s);
    r.value = w === 'max' ? r.max : r.min;
    r.dispatchEvent(new Event('input', { bubbles: true }));
    r.dispatchEvent(new Event('change', { bubbles: true }));
  }, [EXP, which]);
  const tap = (s) => $(page, (sel) => document.querySelector(sel).click(), s);
  assert(await $(page, () => !document.querySelector('label.slider-switch[data-live]')), 'A switch is live on a fresh config');
  await slide('max');
  await settled(page);
  assert(await live(), 'Experience at max is listed but its switch is not live');
  await slide('min');
  await settled(page);
  assert(!await live(), 'Experience back at min kept data-live');
  await slide('max');
  await settled(page);
  await tap('#sw_stats_exp');
  await settled(page);
  assert(!await live() && !await $(page, () => document.getElementById('sw_stats_exp').checked), 'A switched-off switch is marked live');
  await tap('#sw_stats_exp');
  await settled(page);
  await slide('min');
  await settled(page);
  passed.push('idle-switch-marker');

  await $(page, () => document.querySelector('.tabbtn[data-tab="modifiers"]').click());
  await page.mouse.move(1, HEIGHT - 60);
  await frames(page);
  const note = '#stats .setting-entry:has([data-key="exp"]) > .note[data-note="exp"]';
  // A slider at its default has no note (owner, 2026-09-26), so exp gets an
  // unsaved value: moved above its minimum with `input` only, never `change`,
  // so panel.js writes the note and nothing is saved; the row stays idle.
  const unsaved = (above) => $(page, ([s, a]) => {
    const r = document.querySelector(s);
    r.value = a ? Math.min(+r.max, +r.min + (parseFloat(r.dataset.step0 || r.step) || 1)) : r.min;
    r.dispatchEvent(new Event('input', { bubbles: true }));
  }, [EXP, above]);
  await unsaved(true);
  await frames(page);
  assert(!await visible(page, note), 'An idle row shows its note');
  // The note opens as a tooltip after OPEN_DELAY_MS (src/lib/slider-note.js): wait for it, bounded.
  const noteShows = () => page.waitForFunction((s) => !!document.querySelector(s)?.checkVisibility(), note, { timeout: OPEN_DELAY_MS + 2000 }).catch(() => {});
  await page.hover('#stats .setting-entry:has([data-key="exp"]) .lbl');
  await noteShows();
  assert(await visible(page, note), 'Hovering an idle row does not show its note');
  await page.mouse.move(1, HEIGHT - 60);
  await $(page, (s) => document.querySelector(s).focus(), EXP);
  await noteShows();
  assert(await visible(page, note), 'Focus inside an idle row does not show its note');
  await $(page, () => document.activeElement.blur());
  await unsaved(false);
  await $(page, () => document.querySelector('.tabbtn[data-tab="world"]').click());
  await frames(page);
  assert(!await visible(page, '.sat-footer-note'), 'The pool footer still repeats its counts');
  passed.push('idle-note-quiet');

  await only(page, ['headhunter'], [{ key: 'rarity_rare', value: 25 }]);
  const titles = await $(page, () => Object.fromEntries([...document.querySelectorAll('#enabledMods li.enabled-mod')].map((li) => [li.dataset.for, li.title])));
  assert(titles.sw_rarity_rare === `Monster Rarity${ENTRY_TITLE_JOINER}Ancient`, 'Ancient\'s title: ' + titles.sw_rarity_rare);
  assert(titles.headhunter === `Items${ENTRY_TITLE_JOINER}Headhunter buffs on rare kills`, 'Headhunter\'s title: ' + titles.headhunter);
  passed.push('entry-title-full-path');
}

async function setup(ctx) {
  const { page, passed } = ctx;
  const primary = () => $(page, () => ({ install: document.getElementById('installmod').classList.contains('primary'), launch: document.getElementById('launchgame').classList.contains('primary') }));
  // The sandbox reports nothing installed.
  let got = await primary();
  assert(await $(page, () => document.getElementById('chainnote').textContent.trim() !== ''), 'The sandbox chain is not incomplete');
  assert(got.install && !got.launch, 'Incomplete chain: ' + JSON.stringify(got));
  await $(page, () => document.querySelector('.tabbtn[data-tab="setup"]').click());
  await frames(page);
  assert(await $(page, () => document.getElementById('pluginWarning').getBoundingClientRect().height > 0), 'The sandbox shows no plugin warning');
  // The warning is an icon whose tooltip (opened here by focus) ends with an
  // "Open Setup" line: not on Setup, where it would point at itself.
  const openTip = async () => {
    await $(page, () => document.querySelector('#pluginWarning button').focus());
    await page.waitForFunction(() => !!document.querySelector('#pluginWarning [role="tooltip"]')?.checkVisibility(), null, { timeout: 2000 });
  };
  const line = '#pluginWarning [role="tooltip"] .plugin-warning-action';
  await openTip();
  assert(await $(page, () => document.querySelector('#pluginWarning [role="tooltip"]').textContent.includes('Open Setup')), 'The tooltip has no Open Setup line');
  assert(!await visible(page, line), 'Open Setup shows on Setup');
  await $(page, () => document.activeElement.blur());
  await $(page, () => document.querySelector('.tabbtn[data-tab="world"]').click());
  await frames(page);
  await openTip();
  assert(await visible(page, line), 'Open Setup is missing on World');
  await $(page, () => document.activeElement.blur());
  passed.push('open-setup-hidden-on-setup');

  await page.route('**/api/state', async (route) => {
    const response = await route.fetch();
    const state = await response.json();
    return route.fulfill({ response, json: { ...state, gameRunning: true, ipcOk: true, chain: COMPLETE } });
  });
  await reload(page);
  got = await primary();
  assert(!got.install && got.launch, 'Complete chain: ' + JSON.stringify(got));
  await page.unroute('**/api/state');
  passed.push('setup-install-primary-when-chain-incomplete');
}

async function pool(ctx) {
  const { page, passed } = ctx;
  await $(page, () => document.querySelector('.tabbtn[data-tab="world"]').click());
  await frames(page);
  const list = '#satbuffs';
  const state = () => $(page, (s) => { const l = document.querySelector(s); return { more: l.hasAttribute('data-more-below'), scrolls: l.scrollHeight > l.clientHeight }; }, list);
  let got = await state();
  assert(got.scrolls && got.more, 'A pool list with more below has no cue: ' + JSON.stringify(got));
  await $(page, (s) => { const l = document.querySelector(s); l.scrollTop = l.scrollHeight; }, list);
  await wait(50);
  await frames(page);
  got = await state();
  assert(!got.more, 'The pool cue stayed at the bottom');
  passed.push('pool-scroll-cue');
}

const GROUPS = [
  ['popover', popover],
  ['undo', undo],
  ['undo-timer', undoTimer],
  ['focus', focus],
  ['hold', hold],
  ['switches', switches],
  ['setup', setup],
  ['pool', pool],
];

const browser = await launchBrowser();
const passedAll = [];
let failures = 0;
try {
  // These long inline names fit Ledger at 1280 and Ember at 1600. Run the
  // entire contract in both palettes instead of relying on the default theme.
  for (const theme of ['ember', 'ledger']) {
  for (const [group, fn] of GROUPS) {
    const name = `${theme}/${group}`;
    const sandbox = await startSandbox({ dist: typeof args.dist === 'string' ? args.dist : null, seed: { theme } });
    const ctx = { sandbox, passed: [], note: '' };
    try {
      ctx.page = await openPanel(browser, sandbox, { width: theme === 'ember' ? 1600 : 1280, height: HEIGHT });
      ctx.posts = capturePosts(ctx.page);
      await fn(ctx);
      console.log(`ok   ${name}: ${ctx.passed.length} checks${ctx.note ? ' (' + ctx.note + ')' : ''}`);
    } catch (e) {
      failures++;
      console.log(`FAIL ${name}: ${e.message} (after ${ctx.passed.length} passing checks)`);
    } finally {
      for (const label of ctx.passed) console.log(`     passed: ${label}`);
      passedAll.push(...ctx.passed.map(label => `${theme}/${label}`));
      await ctx.page?.context().close();
      await sandbox.stop();
    }
  }
  }
} finally {
  await browser.close();
}
const allExpected = ['ember', 'ledger'].flatMap(theme => EXPECTED.map(label => `${theme}/${label}`));
const missing = allExpected.filter((label) => !passedAll.includes(label));
for (const label of missing) console.log(`FAIL not passed: ${label}`);
console.log(`e2e-review: ${passedAll.length}/${allExpected.length} checks passed`);
process.exitCode = failures || missing.length ? 1 : 0;
