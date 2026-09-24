// The panel's end-to-end checks, in a real (headless) Edge against the real
// HTTP handler on a throwaway config.
//
//   node tests/ui.e2e.mjs [--legacy | --dist <dir>]
//
// Ported from the four agent-browser harnesses that used to sit in
// ForgePact/tests (panel_ui_browser.js, satanic_panel_browser.js,
// panel_install_browser.js, panel_launcher_browser.js): every check they
// made is here under the same label. They reached into page globals (ST,
// pendingWrites, SAT_UI, boot(), status(), pollOnce()) and wrapped
// window.fetch; the Svelte build has no globals, so this drives the page only
// through its DOM and the network:
//
// - a delayed, failed or lost request is a `page.route` on the request, not a
//   wrapped fetch ("lost" performs the request and then drops the answer);
// - "reload the settings" is a page reload (boot() runs again);
// - "poll now" is the page's own visibilitychange handler, which polls at
//   once when the window is visible;
// - "the game is running / the plugin is missing" is a patched /api/state
//   answer, the way a real poll would bring it.
//
// Each group gets its own sandbox server, so nothing needs putting back.
// --legacy runs the same checks against the page embedded in forgepact.py:
// the positive control that the checks pass on the page they were written for.

import { launchBrowser, openPanel, parseArgs, startSandbox, waitBooted, waitSaved } from './lib/browser.mjs';

const args = parseArgs(process.argv.slice(2));
const LEGACY = !!args.legacy;
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const PAGE_TITLES = {
  setup: 'Game setup', modifiers: 'Character modifiers', world: 'World settings', loot: 'Loot settings', mods: 'Mods',
};
const EXPECTED = [
  'All five pages, headings, selected states and overflow',
  'Density off state across load, refresh, slider input, editing, toggles and failed saves',
  'Increment/decrement, exact decimal persistence, Enter/Escape and bounds',
  'Character and Loot search/Modified filters, empty results, no value mutation',
  'Dependent switches, readable disabled text and arrow-key tabs',
  'Mods sub-tabs: default, click, arrow-key wrap and focus',
  'Sequential cross-setting saves, failure rollback, lost-response recovery and retry',
  'All six Setup/header actions route correctly (intercepted; no game or installation)',
  '3/2 minimum blocks mouse changes before HTTP',
  'Replacement, live counts and reloaded saved selections',
  'Name/effect search, both filters and empty states',
  'Enable all, restore defaults, untouched unrelated preferences',
  'Pending save prevents duplicate/concurrent edits',
  'Network rollback, lost-response reconciliation, successful retry',
  'unmodded game with stale IPC folder; warning on all five tabs; Setup shortcut',
  'each required installation component',
  'installation refresh, command wording and offline status',
  'double-click guard, visible error, retry, poll consistency',
  'launch response, verification result, already-running state',
];

function assert(ok, message) { if (!ok) throw new Error(message); }

// Delay / fail-before / fail-after injection on one route, and the count of
// requests in flight at once (the write queue must keep it at one).
async function injectFaults(page, pattern, net) {
  await page.route(pattern, async (route) => {
    if (route.request().method() !== 'POST') return route.continue();
    net.calls++;
    net.inflight++;
    net.peak = Math.max(net.peak, net.inflight);
    try {
      if (net.delay) await wait(net.delay);
      if (net.failure === 'before') return await route.abort('failed');
      const response = await route.fetch();
      if (net.failure === 'after') return await route.abort('failed');
      return await route.fulfill({ response });
    } finally {
      net.inflight--;
    }
  });
}

// Patch fields of every /api/state answer, the way a poll would bring them.
async function patchState(page, patch) {
  await page.route('**/api/state', async (route) => {
    const response = await route.fetch();
    const state = await response.json();
    return route.fulfill({ response, json: { ...state, ...patch(state) } });
  });
}

// The page's visibilitychange handler polls at once while visible; wait for
// that poll's answer and for status() to have painted it.
async function pollNow(page) {
  const answered = page.waitForResponse((r) => r.url().endsWith('/api/state'));
  await page.evaluate(() => document.dispatchEvent(new Event('visibilitychange')));
  await answered;
  await wait(100);
}

async function reload(page) {
  await page.reload();
  await waitBooted(page);
}

const $ = (page, fn, arg) => page.evaluate(fn, arg);

async function panelUi(ctx) {
  const { page, sandbox, passed } = ctx;
  const read = async () => (await sandbox.state()).cfg;
  const baseline = await read();
  const net = { calls: 0, inflight: 0, peak: 0, delay: 0, failure: '' };
  await injectFaults(page, '**/api/set', net);
  const actions = [];
  await page.route(/\/api\/(applyall|browseexe|setexe|installmod|removeplugin|launch)$/, async (route) => {
    if (route.request().method() !== 'POST') return route.continue();
    actions.push(new URL(route.request().url()).pathname);
    return route.fulfill({ json: { ok: 'Test action only', cfg: await read(), path: baseline.game_exe } });
  });
  const settled = async () => { await wait(30); await waitSaved(page); await wait(30); };
  const tab = (name) => $(page, (n) => document.querySelector(`[data-tab="${n}"].tabbtn`).click(), name);
  const tap = async (selector) => { await $(page, (s) => document.querySelector(s).click(), selector); await settled(); };
  const typeValue = async (selector, value, key = 'Enter') => {
    const opened = await $(page, ([s, v, k]) => {
      document.querySelector(s).click();
      const input = document.querySelector(s + ' .numedit');
      if (!input) return false;
      input.value = v;
      input.dispatchEvent(new KeyboardEvent('keydown', { key: k, bubbles: true }));
      return true;
    }, [selector, value, key]);
    assert(opened, 'Numeric editor did not open');
    await settled();
  };
  const search = (value) => $(page, (v) => {
    const s = document.getElementById('controlSearch');
    s.value = v;
    s.dispatchEvent(new Event('input', { bubbles: true }));
  }, value);
  const hidden = (selector) => $(page, (s) => document.querySelector(s).hidden, selector);

  assert(await $(page, () => location.hostname) === '127.0.0.1' &&
    await $(page, () => document.getElementById('chipGame').textContent) === 'Game offline' &&
    !baseline.auto_apply, 'Use isolated PanelSandbox');
  for (const name of ['setup', 'modifiers', 'world', 'loot', 'mods']) {
    await tab(name);
    const s = await $(page, (n) => ({
      selected: document.querySelector(`[data-tab="${n}"].tabbtn`).getAttribute('aria-selected'),
      onlyThisTab: [...document.querySelectorAll('.tab-card.active')].every((c) => c.dataset.tab === n),
      title: document.getElementById('pageTitle').textContent,
      overflow: document.documentElement.scrollWidth > innerWidth,
    }), name);
    assert(s.selected === 'true', 'Tab state ' + name);
    assert(s.onlyThisTab, 'Unrelated visible card ' + name);
    assert(s.title === PAGE_TITLES[name], 'Heading ' + name);
    assert(!s.overflow, 'Horizontal overflow ' + name);
  }
  passed.push('All five pages, headings, selected states and overflow');

  await tab('world');
  const densityOff = async (context) => assert(await $(page, () => {
    const el = (s) => document.querySelector(s);
    return !el('#den_on').checked && el('#denval').textContent === 'off' &&
      el('#densityHero').textContent === 'off' && el('#denval').classList.contains('off');
  }), 'Disabled density looks enabled: ' + context);
  const denValue = () => $(page, () => +document.getElementById('den').value);
  const expInput = () => $(page, () => document.querySelector('[data-sec="stats"][data-key="exp"]').dispatchEvent(new Event('input', { bubbles: true })));
  await densityOff('initial load');
  await expInput();
  await densityOff('unrelated slider input');
  await tap('#densityCard .step-button:last-child');
  await densityOff('editing saved density while off');
  assert((await read()).density === 3.5 && await denValue() === 3.5, 'Density increment');
  await tap('#densityCard .step-button:first-child');
  assert((await read()).density === 3, 'Density decrement');
  net.delay = 80;
  await $(page, () => {
    document.querySelector('#densityCard .step-button:last-child').click();
    document.querySelector('#densityCard .step-button:last-child').click();
  });
  await settled();
  net.delay = 0;
  assert((await read()).density === 4, 'Rapid increments were dropped');
  await typeValue('#denval', '2.25');
  assert((await read()).density === 2.25 && await denValue() === 2.25, 'Decimal snapped after save');
  await densityOff('typed decimal while off');
  await reload(page);
  await tab('world');
  await densityOff('reload while off');
  assert(await denValue() === 2.25, 'Disabled density lost its saved multiplier');
  assert(await $(page, () => {
    document.getElementById('denval').click();
    const editor = document.querySelector('#denval .numedit');
    editor.value = '4';
    document.querySelector('[data-sec="stats"][data-key="exp"]').dispatchEvent(new Event('input', { bubbles: true }));
    const same = document.querySelector('#denval .numedit') === editor && editor.value === '4';
    editor.dispatchEvent(new KeyboardEvent('keydown', { key: 'Escape', bubbles: true }));
    return same;
  }), 'Unrelated slider replaced the density editor');
  await settled();
  await densityOff('cancelled numeric edit');
  assert((await read()).density === 2.25, 'Cancelled density edit was saved');
  net.failure = 'before';
  await tap('#den_on');
  net.failure = '';
  await densityOff('failed enable rolls back');
  await tap('#den_on');
  await reload(page);
  await tab('world');
  assert(await $(page, () => document.getElementById('den_on').checked && +document.getElementById('den').value === 2.25 &&
    document.getElementById('denval').textContent === 'x2.25'), 'Enabling density lost its saved multiplier');
  assert(await $(page, () => document.getElementById('densityHero').textContent.startsWith('×')), 'Enabled density has no multiplier');
  await tap('#den_on');
  await densityOff('disabled after enabling');
  await tap('#den_on');
  passed.push('Density off state across load, refresh, slider input, editing, toggles and failed saves');
  await typeValue('#denval', '4', 'Escape');
  assert((await read()).density === 2.25, 'Escape saved an edit');
  await typeValue('#denval', '900');
  assert((await read()).density === 5 && await $(page, () => document.querySelector('#densityCard .step-button:last-child').disabled), 'Upper bound');
  await typeValue('#denval', '-10');
  assert((await read()).density === 1 && await $(page, () => document.querySelector('#densityCard .step-button').disabled), 'Lower bound');
  passed.push('Increment/decrement, exact decimal persistence, Enter/Escape and bounds');

  await tab('modifiers');
  await $(page, () => { document.querySelector('[data-sec="stats"][data-key="exp"]').closest('.row').querySelector('.val').id = 'test-xp-value'; });
  await typeValue('#test-xp-value', '2.25');
  assert((await read()).stats.exp === 2.25, 'Stat edit');
  const entryHidden = (selector) => $(page, (s) => document.querySelector(s).closest('.setting-entry').hidden, selector);
  await $(page, () => document.querySelector('[data-control-filter="modified"]').click());
  assert(!await entryHidden('#test-xp-value') && await entryHidden('[data-key="magicfind"]'), 'Modified filter');
  await search('nothing-matches-this');
  assert(!await hidden('#emptySettings'), 'Missing empty state');
  await search('experience');
  assert(!await entryHidden('#test-xp-value') && await hidden('#emptySettings'), 'Name search');
  assert(await $(page, () => +document.querySelector('[data-key="exp"]').value) === 2.25, 'Search changed a decimal');
  await tab('loot');
  assert(await $(page, () => document.getElementById('controlSearch').value === '' &&
    document.querySelector('[data-control-filter="all"]').getAttribute('aria-pressed') === 'true'), 'Tab did not reset filters');
  await search('Angelic / Unholy');
  assert(!await hidden('#angelicCard'), 'Angelic search');
  await search('nothing-matches-this');
  assert(await hidden('#angelicCard') && !await hidden('#emptySettings'), 'Angelic excluded from empty state');
  await search('');
  await typeValue('#angelicval', '3');
  await $(page, () => document.querySelector('[data-control-filter="modified"]').click());
  assert(!await hidden('#angelicCard'), 'Modified Angelic hidden');
  passed.push('Character and Loot search/Modified filters, empty results, no value mutation');

  await tab('mods');
  assert(await $(page, () => !document.getElementById('modsSubtabs').hidden && document.getElementById('qolCard').classList.contains('active') &&
    !document.getElementById('itemsCard').classList.contains('active')), 'Mods opens on Quality of Life');
  if (!await $(page, () => document.getElementById('map_reveal').checked)) await tap('#map_reveal');
  assert(!await $(page, () => document.getElementById('map_reveal_packs').disabled), 'Parent did not enable child');
  await tap('#map_reveal');
  assert(await $(page, () => document.getElementById('map_reveal_packs').disabled && document.getElementById('mrpval').textContent === 'n/a'), 'Disabled child state');
  assert(await $(page, () => getComputedStyle(document.getElementById('map_reveal_packs_row')).opacity) === '1', 'Disabled explanatory text faded');
  assert(await $(page, () => {
    const world = document.getElementById('nav-world');
    world.focus();
    world.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowRight', bubbles: true }));
    return document.activeElement === document.getElementById('nav-loot') &&
      document.getElementById('nav-loot').getAttribute('aria-selected') === 'true';
  }), 'Keyboard navigation');
  passed.push('Dependent switches, readable disabled text and arrow-key tabs');

  await tab('mods');
  assert(await $(page, () => {
    document.getElementById('subtab-items').click();
    return document.getElementById('itemsCard').classList.contains('active') && !document.getElementById('qolCard').classList.contains('active');
  }), 'Items sub-tab click');
  assert(await $(page, () => {
    const items = document.getElementById('subtab-items');
    items.focus();
    items.dispatchEvent(new KeyboardEvent('keydown', { key: 'ArrowLeft', bubbles: true }));
    return document.activeElement === document.getElementById('subtab-qol') && document.getElementById('qolCard').classList.contains('active');
  }), 'Sub-tab arrow key returns to Quality of Life');
  await $(page, () => document.getElementById('subtab-items').click());
  passed.push('Mods sub-tabs: default, click, arrow-key wrap and focus');

  await tab('mods');
  net.delay = 100;
  const before = await $(page, () => ({ hh: document.getElementById('headhunter').checked, ty: document.getElementById('tyrant').checked }));
  const indicator = await $(page, () => {
    document.getElementById('headhunter').click();
    document.getElementById('tyrant').click();
    return document.getElementById('saveIndicator').textContent;
  });
  assert(indicator === 'Saving...', 'No saving state');
  await settled();
  net.delay = 0;
  const saved = await read();
  assert(saved.headhunter !== before.hh && saved.tyrant !== before.ty && net.peak === 1, 'Overlapping writes or dropped setting');
  net.failure = 'before';
  await tap('#headhunter');
  net.failure = '';
  assert(await $(page, () => document.getElementById('headhunter').checked) === saved.headhunter &&
    await $(page, () => document.getElementById('saveIndicator').classList.contains('error')), 'Failure did not roll back');
  net.failure = 'after';
  await tap('#headhunter');
  net.failure = '';
  assert(await $(page, () => document.getElementById('headhunter').checked) === (await read()).headhunter, 'Lost response not reconciled');
  await tap('#headhunter');
  assert(!await $(page, () => document.getElementById('saveIndicator').classList.contains('error')), 'Retry did not recover');
  passed.push('Sequential cross-setting saves, failure rollback, lost-response recovery and retry');

  await tab('setup');
  for (const selector of ['#applyall', '#exebrowse', '#exesave', '#installmod', '#removeplugin', '#launchgame']) await tap(selector);
  assert(actions.join(',') === '/api/applyall,/api/browseexe,/api/setexe,/api/installmod,/api/removeplugin,/api/launch', 'Setup action routing: ' + actions.join(','));
  assert(await $(page, () => !document.getElementById('installmod').disabled && !document.getElementById('removeplugin').disabled &&
    !document.getElementById('exebrowse').disabled), 'Setup button left locked');
  passed.push('All six Setup/header actions route correctly (intercepted; no game or installation)');
  ctx.note = `maxConcurrentWrites=${net.peak}`;
}

async function satanicPanel(ctx) {
  const { page, sandbox, passed } = ctx;
  const read = async () => (await sandbox.state()).cfg;
  const net = { calls: 0, inflight: 0, peak: 0, delay: 0, failure: '' };
  await injectFaults(page, '**/api/set', net);
  const box = (polarity, id) => `input[data-sat-polarity="${polarity}"][data-sat-id="${id}"]`;
  const checked = (polarity, id) => $(page, (s) => document.querySelector(s).checked, box(polarity, id));
  const count = (polarity) => $(page, (p) => document.querySelectorAll(`input[data-sat-polarity="${p}"]:checked`).length, polarity);
  const visible = (polarity) => $(page, (p) => document.querySelectorAll(`#sat${p}s .sat-option:not([hidden])`).length, polarity);
  // The Satanic card is aria-busy while a save runs (it was SAT_UI.busy).
  const settled = async () => {
    await wait(30);
    await page.waitForFunction(() => document.getElementById('satanicMods').getAttribute('aria-busy') !== 'true', null, { timeout: 5000, polling: 20 });
    await waitSaved(page);
  };
  const toggle = async (polarity, id) => { await $(page, (s) => document.querySelector(s).closest('label').click(), box(polarity, id)); await settled(); };
  const search = (value) => $(page, (v) => { const s = document.getElementById('satSearch'); s.value = v; s.dispatchEvent(new Event('input', { bubbles: true })); }, value);
  const filter = (value) => $(page, (v) => document.querySelector(`[data-sat-filter="${v}"]`).click(), value);

  assert(await count('buff') === 3 && await count('debuff') === 2, 'Use a fresh minimum fixture');
  assert(await visible('buff') === 25 && await visible('debuff') === 26, 'Missing SDK rows');
  const baseline = await read();
  await toggle('buff', '1');
  await toggle('debuff', '1');
  assert(net.calls === 0 && await checked('buff', '1') && await checked('debuff', '1'), 'Minimum was not protected');
  passed.push('3/2 minimum blocks mouse changes before HTTP');

  await toggle('buff', '4');
  await toggle('buff', '1');
  const saved = await read();
  assert(saved.satanic_mods.buff['4'] && !saved.satanic_mods.buff['1'], 'Replacement did not persist');
  assert(await count('buff') === 3 && await count('debuff') === 2, 'Counts did not update');
  await reload(page);
  assert(await checked('buff', '4') && !await checked('buff', '1'), 'Reload reset saved choices');
  passed.push('Replacement, live counts and reloaded saved selections');

  await search('Rune Master');
  assert(await visible('buff') === 1 && await visible('debuff') === 0, 'Name search');
  await search('life decreased');
  assert(await visible('buff') === 0 && await visible('debuff') === 1, 'Effect search');
  await search('no such modifier');
  assert(await visible('buff') === 0 && !await $(page, () => document.querySelector('#satbuffs .sat-empty').hidden), 'Empty state');
  await search('');
  await filter('enabled');
  assert(await visible('buff') === 3 && await visible('debuff') === 2, 'Enabled filter');
  await filter('disabled');
  assert(await visible('buff') === 22 && await visible('debuff') === 24, 'Disabled filter');
  assert(await $(page, () => document.getElementById('satbuffCount').textContent) === '3 enabled', 'Filtered count must include all enabled mods');
  passed.push('Name/effect search, both filters and empty states');

  const beforeBulk = net.calls;
  await $(page, () => document.getElementById('satbuffAll').click());
  await settled();
  assert(net.calls === beforeBulk + 1 && await count('buff') === 25 && await count('debuff') === 2, 'Enable all must be a single pool request');
  assert(await visible('buff') === 0 && !await $(page, () => document.querySelector('#satbuffs .sat-empty').hidden), 'Disabled filter not refreshed');
  await $(page, () => document.getElementById('satRestore').click());
  await settled();
  assert(await count('buff') === 25 && await count('debuff') === 26, 'Restore defaults');
  const restored = await read();
  for (const [key, value] of Object.entries(baseline)) {
    if (key !== 'satanic_mods') assert(JSON.stringify(restored[key]) === JSON.stringify(value), 'Unrelated setting changed: ' + key);
  }
  assert(net.peak === 1, 'Bulk requests overlapped');
  passed.push('Enable all, restore defaults, untouched unrelated preferences');

  await filter('all');
  net.delay = 150;
  const beforeRapid = net.calls;
  await $(page, ([a, b]) => {
    document.querySelector(a).closest('label').click();
    document.querySelector(b).closest('label').click();
    document.getElementById('satRestore').click();
  }, [box('buff', '1'), box('buff', '2')]);
  await settled();
  net.delay = 0;
  assert(net.calls === beforeRapid + 1 && !await checked('buff', '1') && await checked('buff', '2'), 'Rapid edits were not locked');
  passed.push('Pending save prevents duplicate/concurrent edits');

  net.failure = 'before';
  await toggle('buff', '1');
  net.failure = '';
  assert(!await checked('buff', '1') && await $(page, () => document.getElementById('satSaveState').classList.contains('is-error')), 'Network failure left an unsaved checkbox');
  net.failure = 'after';
  await toggle('buff', '1');
  net.failure = '';
  assert(await checked('buff', '1'), 'Lost response did not reconcile the saved config');
  await toggle('buff', '1');
  assert(!await checked('buff', '1') && !await $(page, () => document.getElementById('satSaveState').classList.contains('is-error')), 'Retry did not recover');
  passed.push('Network rollback, lost-response reconciliation, successful retry');
  ctx.note = `requests=${net.calls} maxConcurrentRequests=${net.peak}`;
}

const COMPLETE = { exeExists: true, patched: true, aurieCore: true, yytk: true, plugin: true };

async function install(ctx) {
  const { page, passed } = ctx;
  let patch = { gameRunning: true, ipcOk: true, chain: { ...COMPLETE, patched: false, aurieCore: false, yytk: false, plugin: false } };
  await patchState(page, () => patch);
  await reload(page);
  const text = (id) => $(page, (i) => document.getElementById(i).textContent, id);
  assert(await text('chipGame') === 'Game open · plugin missing', 'Running unmodded game must show missing plugin');
  assert(await $(page, () => document.getElementById('chipGame').classList.contains('warn')), 'Missing plugin must not look connected');
  for (const name of ['setup', 'modifiers', 'world', 'loot', 'mods']) {
    await $(page, (n) => document.querySelector(`.tabbtn[data-tab="${n}"]`).click(), name);
    assert(await $(page, () => document.getElementById('pluginWarning').getBoundingClientRect().height) > 0, 'Missing warning in ' + name);
  }
  await $(page, () => document.getElementById('pluginWarning').querySelector('button').click());
  assert(await $(page, () => document.getElementById('nav-setup').getAttribute('aria-selected')) === 'true', 'Warning must open Setup');
  passed.push('unmodded game with stale IPC folder; warning on all five tabs; Setup shortcut');
  for (const part of ['patched', 'aurieCore', 'yytk', 'plugin']) {
    patch = { gameRunning: true, ipcOk: true, chain: { ...COMPLETE, [part]: false } };
    await pollNow(page);
    assert(!await $(page, () => document.getElementById('pluginWarning').hidden), 'Missing component ignored: ' + part);
  }
  passed.push('each required installation component');
  patch = { gameRunning: true, ipcOk: true, chain: COMPLETE, lastApplied: '12:34:56' };
  await pollNow(page);
  assert(await $(page, () => document.getElementById('pluginWarning').hidden), 'Successful installation must refresh on the existing poll');
  assert(await text('chipGame') === 'Game open', 'File checks must not claim runtime acknowledgement');
  assert((await text('chipApply')).includes('commands sent:'), 'Sent commands must not be labelled applied');
  patch = { ...patch, gameRunning: false };
  await pollNow(page);
  assert(await text('chipGame') === 'Game offline', 'Offline state');
  passed.push('installation refresh, command wording and offline status');
}

async function launcher(ctx) {
  const { page, passed } = ctx;
  let count = 0;
  let reply = { err: 'Steam was not found', launch: { phase: 'error', message: 'Steam was not found', pid: 0 } };
  await page.route('**/api/launch', async (route) => {
    count++;
    await wait(180);
    return route.fulfill({ json: reply });
  });
  let running = false;
  await patchState(page, () => ({ gameRunning: running, launch: count ? reply.launch : { phase: 'idle', message: 'Ready' } }));
  await reload(page);
  await $(page, () => document.querySelector('.tabbtn[data-tab="setup"]').click());
  const feedback = () => $(page, () => document.getElementById('launchFeedback').textContent);
  const disabled = () => $(page, () => document.getElementById('launchgame').disabled);
  assert(await $(page, () => { const b = document.getElementById('launchgame'); b.click(); b.click(); return b.disabled; }), 'Launch must be disabled while waiting for Steam');
  await wait(300);
  assert(count === 1, 'Double click must send exactly one request');
  assert(!await disabled(), 'Failed launch must permit a retry');
  assert(await feedback() === 'Steam was not found', 'Persistent failure must be visible');
  await pollNow(page);
  assert(await feedback() === 'Steam was not found', 'Poll must retain server failure');
  passed.push('double-click guard, visible error, retry, poll consistency');
  reply = { ok: 'Offline launch requested', launch: { phase: 'started', message: 'Offline launch requested', pid: 1234 } };
  await $(page, () => document.getElementById('launchgame').click());
  await wait(300);
  assert(count === 2 && await feedback() === 'Offline launch requested', 'Successful response');
  reply.launch = { phase: 'verified', message: 'Game process verified; EAC is inactive.', pid: 1234 };
  await pollNow(page);
  assert(await feedback() === reply.launch.message, 'One-shot verification result must reach the UI');
  running = true;
  await pollNow(page);
  assert(await disabled(), 'Already-running game must disable Launch');
  passed.push('launch response, verification result, already-running state');
}

const GROUPS = [
  ['panel_ui', panelUi, { offline: true }],
  ['satanic_panel', satanicPanel, { offline: true, satanicMinimum: true }],
  ['panel_install', install, { offline: true }],
  ['panel_launcher', launcher, { offline: true }],
];

const browser = await launchBrowser();
const passedAll = [];
let failures = 0;
try {
  for (const [name, fn, options] of GROUPS) {
    const sandbox = await startSandbox({ ...options, legacy: LEGACY, dist: typeof args.dist === 'string' ? args.dist : null });
    const ctx = { sandbox, passed: [], note: '' };
    try {
      ctx.page = await openPanel(browser, sandbox);
      await fn(ctx);
      console.log(`ok   ${name}: ${ctx.passed.length} checks${ctx.note ? ' (' + ctx.note + ')' : ''}`);
    } catch (e) {
      failures++;
      console.log(`FAIL ${name}: ${e.message} (after ${ctx.passed.length} passing checks)`);
    } finally {
      for (const label of ctx.passed) console.log(`     passed: ${label}`);
      passedAll.push(...ctx.passed);
      await ctx.page?.context().close();
      await sandbox.stop();
    }
  }
} finally {
  await browser.close();
}
const missing = EXPECTED.filter((label) => !passedAll.includes(label));
for (const label of missing) console.log(`FAIL not passed: ${label}`);
console.log(`e2e: ${passedAll.length}/${EXPECTED.length} checks passed${LEGACY ? ' (legacy page)' : ''}`);
process.exitCode = failures || missing.length ? 1 : 0;
