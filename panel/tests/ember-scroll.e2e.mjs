// Actual user scrolling, not locator auto-scrolling: the old Ember shell let
// Playwright reach clipped controls while mouse-wheel users could not.
import assert from 'node:assert/strict';
import { mkdirSync } from 'node:fs';
import { resolve } from 'node:path';
import { chromium } from 'playwright-core';
import { startSandbox, openPanel, openTab, parseArgs, waitBooted } from './lib/browser.mjs';
import { BOOLEAN_MODS } from '../src/enabled-mods.js';

const args = parseArgs(process.argv.slice(2));
const sandbox = await startSandbox({ offline: true, dist: args.dist ? resolve(args.dist) : null });
// Headless normally hides native scrollbars, which would hide this regression
// in screenshots and make a real scrollbar drag impossible to exercise.
const browser = await chromium.launch({ channel: 'msedge', headless: true, ignoreDefaultArgs: ['--hide-scrollbars'] });
const checks = [];
const frames = p => p.evaluate(() => new Promise(r => requestAnimationFrame(() => requestAnimationFrame(r))));
mkdirSync('artifacts/ember-scroll', { recursive: true });
try {
  const page = await openPanel(browser, sandbox, { width: 1366, height: 768 });
  const errors = [];
  page.on('pageerror', e => errors.push(e.message));
  const wrap = page.locator('#wrap');
  const state = () => wrap.evaluate(el => ({ top: el.scrollTop, height: el.clientHeight, total: el.scrollHeight }));
  async function wheelDown() {
    const r = await wrap.boundingBox();
    // The right gutter stays outside nested lists and range inputs.
    await page.mouse.move(r.x + r.width - 20, r.y + r.height * .6);
    await page.mouse.wheel(0, 15000);
    await frames(page);
    await page.waitForTimeout(80);
  }
  async function reachBottom(label) {
    const before = await state();
    if (before.total > before.height + 2) {
      await wheelDown();
      await page.waitForFunction(() => document.getElementById('wrap').scrollTop > 0, null, { timeout: 1500 }).catch(async error => {
        const hit = await page.evaluate(() => { const r = document.getElementById('wrap').getBoundingClientRect(); return document.elementFromPoint(r.right - 20, r.top + r.height * .6)?.outerHTML.slice(0, 220); });
        throw new Error(`${label}: wheel did not move ${JSON.stringify(await state())}; hit=${hit}; ${error.message}`);
      });
      for (let i = 0; i < 5; i++) {
        const s = await state();
        if (s.top + s.height >= s.total - 3) break;
        await wheelDown();
      }
      const after = await state();
      assert.ok(after.top + after.height >= after.total - 3, `${label}: wheel cannot reach bottom ${JSON.stringify(after)}`);
    }
    const geometry = await page.evaluate(() => {
      const pane = document.getElementById('wrap').getBoundingClientRect();
      const footer = document.querySelector('.ember-footer').getBoundingClientRect();
      const cards = [...document.querySelectorAll('#workspace > .tab-card.active')].filter(el => el.checkVisibility());
      const bottom = Math.max(...cards.map(el => el.getBoundingClientRect().bottom));
      const scroller = document.getElementById('wrap');
      return { paneBottom: pane.bottom, footerTop: footer.top, footerBottom: footer.bottom, bottom,
        innerWidth, clientWidth: document.documentElement.clientWidth, pageWidth: document.documentElement.scrollWidth,
        windowY: scrollY, scrollbar: getComputedStyle(scroller).scrollbarWidth,
        overflow: getComputedStyle(scroller).overflowY, windowHeight: innerHeight };
    });
    assert.ok(geometry.paneBottom <= geometry.footerTop + 1, `${label}: pane lies behind footer`);
    assert.ok(geometry.bottom <= geometry.paneBottom + 1, `${label}: last card clipped ${JSON.stringify(geometry)}`);
    assert.ok(geometry.footerBottom <= geometry.windowHeight + 1, `${label}: footer off screen`);
    assert.ok(geometry.pageWidth <= geometry.clientWidth + 1, `${label}: horizontal overflow`);
    assert.equal(geometry.windowY, 0, `${label}: double scrolling`);
    assert.equal(geometry.overflow, 'auto', `${label}: no user scrolling`);
    assert.equal(geometry.scrollbar, 'auto', `${label}: scrollbar hidden or too thin`);
    await page.locator('#applyall').click({ trial: true });
  }

  // 1093x614 is the CSS viewport of a 1366x768 display at 125% scaling.
  for (const [width, height] of [[1366,768], [1600,1000], [1093,614], [900,600], [640,400], [390,640]]) {
    await page.setViewportSize({ width, height });
    for (const tab of ['overview','modifiers','world','loot','mods','setup','help']) {
      await openTab(page, tab);
      await frames(page);
      assert.equal((await state()).top, 0, `${width} ${tab}: retained previous page scroll`);
      await reachBottom(`${width}x${height} ${tab}`);
      if (tab === 'loot') await page.screenshot({ path: `artifacts/ember-scroll/loot-bottom-${width}.png` });
      if (tab === 'mods') {
        await openTab(page, 'mods');
        await page.click('#subtab-items');
        await reachBottom(`${width}x${height} item mods`);
      }
    }
    checks.push(`All seven pages and both Mods sections: wheel reaches the end at ${width}x${height}, footer stays separate`);
  }

  await page.setViewportSize({ width: 1366, height: 768 });
  await openTab(page, 'loot');
  const bar = await wrap.evaluate(el => {
    const r = el.getBoundingClientRect(), gutter = el.offsetWidth - el.clientWidth;
    const track = el.clientHeight - gutter * 2;
    const thumb = Math.max(20, track * el.clientHeight / el.scrollHeight);
    return { x: r.right - gutter / 2, y: r.top + gutter + thumb / 2, end: r.bottom - gutter - 4, gutter };
  });
  assert.ok(bar.gutter >= 12, `No visible scrollbar track: ${bar.gutter}px`);
  await page.mouse.move(bar.x, bar.y);
  await page.mouse.down();
  await page.mouse.move(bar.x, bar.end, { steps: 12 });
  await page.mouse.up();
  await page.waitForFunction(() => { const el = document.getElementById('wrap'); return el.scrollTop + el.clientHeight >= el.scrollHeight - 3; });
  checks.push('Visible native scrollbar can be dragged to the last Loot settings');

  await openTab(page, 'loot');
  // A focused scroll region supports Page Down/End as well as the wheel.
  await wrap.focus();
  await page.keyboard.press('PageDown');
  await page.waitForFunction(() => document.getElementById('wrap').scrollTop > 0);
  await page.keyboard.press('Control+End');
  await page.waitForFunction(() => { const el = document.getElementById('wrap'); return el.scrollTop + el.clientHeight >= el.scrollHeight - 3; });
  await reachBottom('keyboard');
  checks.push('Keyboard Page Down and End scroll the content');

  // Keyboard focus on a low control brings it into the pane, above the footer.
  await openTab(page, 'loot');
  await page.locator('#gemfilter_toggle').focus();
  await frames(page);
  const focus = await page.locator('#gemfilter_toggle').boundingBox();
  const pane = await wrap.boundingBox();
  assert.ok(focus.y >= pane.y && focus.y + focus.height <= pane.y + pane.height, 'bottom control focus is clipped');
  checks.push('Focusing the last Loot controls reveals them above the footer');

  // These long lists scroll independently inside the main page.
  for (const selector of ['#satbuffs', '#satdebuffs', '#gemfilter_panel .sat-list']) {
    await openTab(page, selector.startsWith('#sat') ? 'world' : 'loot');
    if (selector.startsWith('#gem')) await page.click('#gemfilter_toggle');
    const list = page.locator(selector);
    await list.scrollIntoViewIfNeeded();
    const r = await list.boundingBox();
    await page.mouse.move(r.x + r.width / 2, r.y + r.height / 2);
    await page.mouse.wheel(0, 10000);
    await page.waitForFunction(s => { const el = document.querySelector(s); return el.scrollTop + el.clientHeight >= el.scrollHeight - 3; }, selector);
  }
  checks.push('Both Satanic pools and the Gems filter retain independent list scrolling');

  for (const theme of ['ledger','graphite','sigil','ember']) {
    await openTab(page, 'setup');
    await page.selectOption('#theme', theme);
    await page.waitForFunction(v => document.documentElement.dataset.theme === v, theme);
    await openTab(page, 'loot');
    await wheelDown();
    await page.waitForFunction(() => document.getElementById('wrap').scrollTop > 0);
    await page.locator('#applyall').click({ trial: true });
  }
  checks.push('Scrolling survives all four theme switches');

  await page.setViewportSize({ width: 900, height: 600 });
  await openTab(page, 'overview');
  await page.locator('.sidebar').evaluate(el => { el.scrollTop = 0; });
  await page.mouse.move(80, 350);
  await page.mouse.wheel(0, 10000);
  await page.waitForFunction(() => document.querySelector('.sidebar').scrollTop > 0);
  const help = await page.locator('#nav-help').boundingBox();
  assert.ok(help.y >= 0 && help.y + help.height <= 600, 'Help is inaccessible in a short sidebar');
  assert.equal((await state()).top, 0, 'sidebar wheel scrolled the content');
  checks.push('Short-window sidebar scrolls independently so Setup and Help stay reachable');

  // Do not let the newly bounded pane clip menus that used to overflow it.
  async function assertNotCovered(selector) {
    assert.ok(await page.locator(selector).evaluate(el => {
      const r = el.getBoundingClientRect();
      return el.contains(document.elementFromPoint(r.x + r.width / 2, r.y + r.height / 2));
    }), `${selector}: covered by the status rail or another control`);
  }
  for (const [width, height] of [[1280,800], [900,600], [640,400], [390,640]]) {
    await page.setViewportSize({ width, height });
    await openTab(page, 'setup');
    await page.locator('.theme-picker-trigger').click();
    await page.keyboard.press('Home');
    await frames(page);
    await assertNotCovered('.theme-picker-option:first-child');
    await page.keyboard.press('End');
    await frames(page);
    const menu = await page.locator('.theme-picker-list').boundingBox();
    const pane = await wrap.boundingBox();
    const last = await page.locator('.theme-picker-option').last().boundingBox();
    assert.ok(menu.y >= pane.y && menu.y + menu.height <= pane.y + pane.height + 1, `${width}: theme menu clipped ${JSON.stringify({menu,pane})}`);
    assert.ok(last.y >= menu.y && last.y + last.height <= menu.y + menu.height + 1, `${width}: last palette clipped`);
    await assertNotCovered('.theme-picker-option:last-child');
    // A raw pointer click cannot quietly auto-scroll like locator.click().
    const theme = await page.locator('.theme-picker-option').last().getAttribute('data-value');
    await page.mouse.click(last.x + last.width / 2, last.y + last.height / 2);
    assert.equal(await page.locator('.theme-picker-trigger').getAttribute('aria-expanded'), 'false', `${width}: raw pointer click did not choose a palette`);
    await page.waitForFunction(v => document.documentElement.dataset.theme === v, theme);
    await page.selectOption('#theme', 'ember');
    await page.waitForFunction(() => document.documentElement.dataset.theme === 'ember');
  }
  checks.push('Theme menu and its last palette stay inside the pane, including short/mobile windows');

  const three = ['map_reveal','headhunter','mod_orb_pickup_radius'];
  for (const key of BOOLEAN_MODS) await page.request.post(`${sandbox.url}api/set`, { data: { key, value: three.includes(key) } });
  await page.setViewportSize({ width: 1600, height: 800 });
  await page.reload(); await waitBooted(page);
  await page.waitForFunction(() => document.getElementById('enabledMods').dataset.form === 'inline');
  await page.setViewportSize({ width: 1280, height: 800 });
  await page.waitForFunction(() => document.getElementById('enabledMods').dataset.form === 'tray');
  for (const key of BOOLEAN_MODS) await page.request.post(`${sandbox.url}api/set`, { data: { key, value: true } });
  await page.reload(); await waitBooted(page);
  for (const [width, height] of [[1280,800], [900,600], [640,400], [390,640]]) {
    await page.setViewportSize({ width, height });
    await openTab(page, 'overview');
    await page.locator('.enabled-mods-toggle').click();
    await frames(page);
    const list = page.locator('#enabledModsList');
    const rect = await list.boundingBox(), pane = await wrap.boundingBox();
    assert.ok(rect.y >= pane.y && rect.y + rect.height <= pane.y + pane.height + 1, `${width}: enabled list clipped`);
    await assertNotCovered('#enabledModsList li:first-child .quick-disable');
    await page.mouse.move(rect.x + rect.width / 2, rect.y + rect.height / 2);
    await page.mouse.wheel(0, 10000);
    await page.waitForFunction(() => { const el = document.getElementById('enabledModsList'); return el.scrollTop + el.clientHeight >= el.scrollHeight - 3; });
    const last = await list.locator('.quick-disable').last().boundingBox();
    assert.ok(last.y >= rect.y && last.y + last.height <= rect.y + rect.height + 1, `${width}: last Turn off clipped`);
    await assertNotCovered('#enabledModsList li:last-child .quick-disable');
    await page.keyboard.press('Escape');
  }
  checks.push('Enabled mods switches between inline/tray and its last entry remains reachable in short/mobile windows');
  // Undo must not cover either the scrolling settings or the Apply controls.
  for (const [width, height] of [[1280,800], [900,600], [640,400], [390,640]]) {
    for (const key of BOOLEAN_MODS) await page.request.post(`${sandbox.url}api/set`, { data: { key, value: key === 'mod_orb_pickup_radius' } });
    await page.setViewportSize({ width, height });
    await page.reload(); await waitBooted(page);
    if (await page.locator('#enabledMods').getAttribute('data-form') === 'tray')
      await page.locator('.enabled-mods-toggle').click();
    await page.locator('#enabledMods .quick-disable').click();
    await frames(page);
    const toast = await page.locator('.undo-toast').boundingBox(), pane = await wrap.boundingBox();
    assert.ok(toast.y >= pane.y + pane.height - 1 && toast.y + toast.height <= height, `${width}: Undo covers content or leaves viewport`);
    const status = await page.locator('#toast.show').boundingBox();
    if (status) assert.ok(status.y >= pane.y + pane.height - 1 &&
      (status.y + status.height <= toast.y + 1 || toast.y + toast.height <= status.y + 1),
      `${width}: status toast overlaps settings or Undo`);
    await assertNotCovered('.undo-toast-button');
    await assertNotCovered('#applyall');
    await page.screenshot({ path: `artifacts/ember-scroll/undo-${width}.png` });
    await page.locator('.undo-toast-button').click();
    await page.waitForFunction(() => !document.querySelector('.undo-toast'));
    assert.equal((await sandbox.state()).cfg.mod_orb_pickup_radius, true);
  }
  // Move an existing toast between layout hosts on a theme change, then Undo.
  await page.setViewportSize({ width: 1600, height: 1000 });
  await page.reload(); await waitBooted(page);
  await page.locator('#enabledMods .quick-disable').click();
  await openTab(page, 'setup');
  await page.selectOption('#theme', 'ledger');
  await page.waitForFunction(() => document.querySelector('.undo-toast')?.parentElement === document.body);
  await page.selectOption('#theme', 'ember');
  await page.waitForFunction(() => !!document.querySelector('.ember-notices > .undo-toast'));
  await page.locator('.undo-toast-button').click();
  assert.equal((await sandbox.state()).cfg.mod_orb_pickup_radius, true);
  checks.push('Undo reserves footer space at four sizes, remains clickable, and survives theme changes');
  assert.deepEqual(errors, []);
  console.log(JSON.stringify({ passed: checks.length, checks }, null, 2));
  console.log(`e2e-ember-scroll: ${checks.length}/${checks.length} checks passed`);
} finally { await browser.close(); await sandbox.stop(); }
