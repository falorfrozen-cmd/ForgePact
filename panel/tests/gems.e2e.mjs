// The Gems of Incarnation controls on the Mods tab, in a real (headless) Edge
// against the sandbox server at the product defaults: the DOM facts the
// behaviour oracle cannot see. What the controls send is proven by the
// supplementary oracle (tests/behaviour-oracle-gems.json, recorded from
// origin/main's legacy page and replayed by `npm run oracle:replay`); this
// suite checks where the controls sit, how they start, how the Enabled mods
// list treats them, and what the mod filter's list draws and refuses.
//
//   node tests/gems.e2e.mjs [--dist <dir>] [--shots <dir>]    (npm run e2e:gems)
//
// Asserts through the DOM (ids, classes, checked, text, checkVisibility()),
// sandbox.state() and the POSTs the page sends - never through a computed
// style. --shots <dir> also writes gems-closed-1280.png and gems-open-1280.png
// (the Mods tab with the filter closed and open) for the owner.

import { mkdirSync } from 'node:fs';
import { join } from 'node:path';
import { launchBrowser, openPanel, parseArgs, startSandbox, waitBooted, waitSaved } from './lib/browser.mjs';
import { FREEZE_MAX_MS } from '../src/lib/enabled-mods-undo.js';

const args = parseArgs(process.argv.slice(2));
const SHOTS = typeof args.shots === 'string' ? args.shots : null;
if (SHOTS) mkdirSync(SHOTS, { recursive: true });
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const EXPECTED = [
  'gems-group-in-qol',
  'gems-default-off',
  'gems-in-enabled-mods',
  'gems-turn-off',
  'gemfilter-summary-all',
  'gemfilter-lists-36-in-6',
  'gemfilter-refuses-none',
  'gemfilter-saves-two',
  'gemfilter-closes',
];
const TITLES = { mod_gem_mythic: 'Mythic Gems of Incarnation', mod_gem_maxroll: 'Max-roll Gems of Incarnation' };
const FILTER_CONTROLS = ['gemfilter_toggle', 'gemfilter_row', 'gemfilter_panel', 'gem_filter'];

function assert(ok, message) { if (!ok) throw new Error(message); }
const $ = (page, fn, arg) => page.evaluate(fn, arg);

async function settled(page) {
  await wait(30);
  await waitSaved(page);
  await wait(30);
}

async function openQol(page) {
  await page.click('.tabbtn[data-tab="mods"]');
  await page.click('#subtab-qol');
  await settled(page);
}

const listed = (page) => $(page, () => [...document.querySelectorAll('#enabledMods li.enabled-mod')]
  .map((li) => ({ id: li.dataset.for, name: li.querySelector('.enabled-mod-name')?.textContent.trim() })));
const text = (page, id) => $(page, (i) => document.getElementById(i).textContent.trim(), id);
const checked = (page, id) => $(page, (i) => document.getElementById(i).checked, id);
const panelOpen = (page) => $(page, () => document.getElementById('gemfilter_panel').checkVisibility());

// POSTs sent while `fn` runs.
async function postsDuring(page, fn) {
  const posts = [];
  const on = (r) => { if (r.method() === 'POST') posts.push({ url: new URL(r.url()).pathname, body: r.postData() }); };
  page.on('request', on);
  try { await fn(); await settled(page); } finally { page.off('request', on); }
  return posts;
}

async function switches(ctx) {
  const { page, passed } = ctx;
  await openQol(page);

  const group = await $(page, () => {
    const card = document.getElementById('qolCard');
    const mythic = document.getElementById('mod_gem_mythic').closest('.row');
    const parent = mythic.parentElement;
    return {
      inCard: card.contains(parent),
      isGroup: parent.classList.contains('feature-with-child'),
      groupsHoldingGems: [...card.querySelectorAll('.feature-with-child')].filter((g) => g.querySelector('[id^="mod_gem_"],#gemfilter_row')).length,
      order: [...parent.children].map((el) => el.id || (el === mythic ? 'mythic' : el.className)),
      rows: [mythic, document.getElementById('mod_gem_maxroll_row'), document.getElementById('gemfilter_row')].map((r) => r.classList.contains('feature-row')),
    };
  });
  assert(group.inCard && group.isGroup && group.groupsHoldingGems === 1, `the gem rows are not one .feature-with-child in #qolCard: ${JSON.stringify(group)}`);
  assert(JSON.stringify(group.order) === JSON.stringify(['mythic', 'mod_gem_maxroll_row', 'gemfilter_row', 'gemfilter_panel']),
    `the group holds ${JSON.stringify(group.order)}`);
  assert(group.rows.every(Boolean), `a gem row lacks feature-row: ${JSON.stringify(group.rows)}`);
  passed.push('gems-group-in-qol');

  assert(!await checked(page, 'mod_gem_mythic') && !await checked(page, 'mod_gem_maxroll'), 'a gem switch starts checked');
  assert(await text(page, 'mgmval') === 'off' && await text(page, 'mgrval') === 'off', 'a gem value box does not read off');
  assert(!(await listed(page)).some((e) => /gem/.test(e.id)), 'Enabled mods lists a gem entry at the defaults');
  passed.push('gems-default-off');

  for (const id of ['mod_gem_mythic', 'mod_gem_maxroll']) {
    await page.click(`#${id}`);
    await settled(page);
  }
  const cfg = (await ctx.sandbox.state()).cfg;
  assert(cfg.mod_gem_mythic === true && cfg.mod_gem_maxroll === true, 'checking both switches did not save them on');
  const entries = await listed(page);
  for (const [id, title] of Object.entries(TITLES)) {
    const entry = entries.find((e) => e.id === id);
    assert(entry && entry.name === title, `no entry named ${title} for ${id}: ${JSON.stringify(entries)}`);
  }
  assert(!entries.some((e) => FILTER_CONTROLS.includes(e.id)), `a filter control is an entry: ${JSON.stringify(entries)}`);
  assert(entries.filter((e) => /gem/.test(e.id)).length === 2, `not exactly two gem entries: ${JSON.stringify(entries)}`);
  passed.push('gems-in-enabled-mods');

  await page.click('#enabledMods .quick-disable[data-for="mod_gem_mythic"]');
  await settled(page);
  await page.waitForFunction(() => !document.querySelector('#enabledMods li.enabled-mod[data-for="mod_gem_mythic"]'),
    null, { timeout: FREEZE_MAX_MS + 5000 });
  assert(!await checked(page, 'mod_gem_mythic'), 'Turn off left the Mythic switch checked');
  assert(await text(page, 'mgmval') === 'off', 'Turn off left #mgmval reading on');
  assert((await ctx.sandbox.state()).cfg.mod_gem_mythic === false, 'Turn off did not save Mythic off');
  assert((await listed(page)).some((e) => e.id === 'mod_gem_maxroll'), 'Turn off on Mythic also removed Max-roll');
  passed.push('gems-turn-off');
}

async function filter(ctx) {
  const { page, sandbox, passed } = ctx;
  const state = await sandbox.state();
  const affixes = state.gemAffixes;
  await openQol(page);
  if (SHOTS) {
    await $(page, () => document.getElementById('gemfilter_row').scrollIntoView({ block: 'center' }));
    await page.screenshot({ path: join(SHOTS, 'gems-closed-1280.png') });
  }

  assert(await text(page, 'gemfilter_summary') === `all ${affixes.length}` && affixes.length === 36,
    `the summary reads ${await text(page, 'gemfilter_summary')} for ${affixes.length} mods`);
  passed.push('gemfilter-summary-all');

  assert(!await panelOpen(page), 'the filter list is open before Filter… is pressed');
  await page.click('#gemfilter_toggle');
  await settled(page);
  assert(await panelOpen(page), 'Filter… did not open the list');
  const drawn = await $(page, () => {
    const box = document.getElementById('gemfilter_panel');
    return {
      boxes: [...box.querySelectorAll('input[data-gfstat]')].map((i) => ({ stat: +i.dataset.gfstat, checked: i.checked })),
      cats: [...box.querySelectorAll('.gf-cat')].map((c) => c.firstChild.textContent.trim()),
    };
  });
  assert(drawn.boxes.length === 36 && drawn.boxes.every((b) => b.checked), `the list draws ${drawn.boxes.length} boxes, ${drawn.boxes.filter((b) => b.checked).length} ticked`);
  assert(JSON.stringify(drawn.cats) === JSON.stringify(state.gemCategories) && drawn.cats.length === 6,
    `the headings are ${JSON.stringify(drawn.cats)}, not ${JSON.stringify(state.gemCategories)}`);
  const order = state.gemCategories.flatMap((cat) => affixes.filter((a) => a[1] === cat).map((a) => a[0]));
  assert(JSON.stringify(drawn.boxes.map((b) => b.stat)) === JSON.stringify(order), 'the boxes are not in category, then list, order');
  passed.push('gemfilter-lists-36-in-6');
  if (SHOTS) {
    await $(page, () => document.getElementById('gemfilter_row').scrollIntoView({ block: 'start' }));
    await page.screenshot({ path: join(SHOTS, 'gems-open-1280.png') });
  }

  await page.click('#gemfilter_panel [data-gf="none"]');
  const refused = await postsDuring(page, () => page.click('#gemfilter_panel [data-gf="save"]'));
  assert(refused.length === 0, `saving nothing ticked posted ${JSON.stringify(refused)}`);
  assert(await text(page, 'toast') === 'Gem filter: tick at least one mod', `the toast reads ${await text(page, 'toast')}`);
  assert((await sandbox.state()).cfg.gem_filter === 'all', 'the refused save changed the saved filter');
  passed.push('gemfilter-refuses-none');

  const statOf = (words) => affixes.find((a) => a[2].includes(words))[0];
  const two = [statOf('Attack Speed'), statOf('Magic Find')];
  for (const stat of two) await page.click(`#gemfilter_panel input[data-gfstat="${stat}"]`);
  const saved = await postsDuring(page, () => page.click('#gemfilter_panel [data-gf="save"]'));
  assert(saved.length === 1 && JSON.stringify(JSON.parse(saved[0].body)) === JSON.stringify({ key: 'gem_filter', value: two }),
    `the save posted ${JSON.stringify(saved)}`);
  assert(await text(page, 'gemfilter_summary') === '2 of 36', `the summary reads ${await text(page, 'gemfilter_summary')} after saving two`);
  assert(JSON.stringify((await sandbox.state()).cfg.gem_filter) === JSON.stringify(two), 'the saved filter is not the two mods');
  await page.reload();
  await waitBooted(page);
  await openQol(page);
  assert(await text(page, 'gemfilter_summary') === '2 of 36', `after a reload the summary reads ${await text(page, 'gemfilter_summary')}`);
  passed.push('gemfilter-saves-two');

  await page.click('#gemfilter_toggle');
  await settled(page);
  assert(await panelOpen(page), 'Filter… did not reopen the list');
  const reopened = await $(page, () => [...document.querySelectorAll('#gemfilter_panel input[data-gfstat]')].filter((i) => i.checked).map((i) => +i.dataset.gfstat));
  assert(JSON.stringify(reopened) === JSON.stringify(two), `the reopened list ticks ${JSON.stringify(reopened)}`);
  await page.click('#gemfilter_toggle');
  await settled(page);
  assert(!await panelOpen(page), 'Filter… did not close the list');
  passed.push('gemfilter-closes');
}

const GROUPS = [
  ['switches', switches],
  ['filter', filter],
];

const browser = await launchBrowser();
const passedAll = [];
let failures = 0;
try {
  for (const [name, fn] of GROUPS) {
    const sandbox = await startSandbox({ dist: typeof args.dist === 'string' ? args.dist : null });
    const ctx = { sandbox, passed: [] };
    try {
      ctx.page = await openPanel(browser, sandbox);
      await fn(ctx);
      console.log(`ok   ${name}: ${ctx.passed.length} checks`);
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
if (SHOTS) console.log(`shots: ${join(SHOTS, 'gems-closed-1280.png')} ${join(SHOTS, 'gems-open-1280.png')}`);
const missing = EXPECTED.filter((label) => !passedAll.includes(label));
for (const label of missing) console.log(`FAIL not passed: ${label}`);
console.log(`e2e-gems: ${passedAll.length}/${EXPECTED.length} checks passed`);
process.exitCode = failures || missing.length ? 1 : 0;
