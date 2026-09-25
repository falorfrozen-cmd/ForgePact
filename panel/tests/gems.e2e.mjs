// The Gems of Incarnation controls on the Loot tab, in a real (headless) Edge
// against the sandbox server at the product defaults: the DOM facts the
// behaviour oracle cannot see. What the controls send is proven by the
// supplementary oracle (tests/behaviour-oracle-gems.json, recorded from
// origin/main's legacy page when the controls sat on Mods › Quality of Life,
// and replayed through tests/lib/oracle-relocate.mjs's navigation relocation
// by `npm run oracle:replay`); this suite checks where the controls sit (their
// own #gemsCard, the Loot tab's last card), how they start, how the Enabled
// mods list treats them, what the mod filter's list draws with the Satanic
// pool's classes, what its search and All mods / Enabled / Disabled filter
// show and never change, what Save refuses and sends, the unsaved-changes cue,
// and how the card follows the Loot tab's own search and Modified filter.
//
//   node tests/gems.e2e.mjs [--dist <dir>] [--shots <dir>]    (npm run e2e:gems)
//
// Asserts through the DOM (ids, classes, checked, text, checkVisibility()),
// sandbox.state() and the POSTs the page sends - never through a computed
// style. Every expected list is computed from sandbox.state().gemAffixes,
// never written out here. --shots <dir> also writes gems-closed-1280.png and
// gems-open-1280.png (the Loot tab with the filter closed and open) for the
// owner.

import { mkdirSync } from 'node:fs';
import { join } from 'node:path';
import { launchBrowser, openPanel, parseArgs, startSandbox, waitBooted, waitSaved } from './lib/browser.mjs';
import { FREEZE_MAX_MS } from '../src/lib/enabled-mods-undo.js';

const args = parseArgs(process.argv.slice(2));
const SHOTS = typeof args.shots === 'string' ? args.shots : null;
if (SHOTS) mkdirSync(SHOTS, { recursive: true });
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const EXPECTED = [
  'gems-group-on-loot',
  'gems-not-on-mods',
  'gems-default-off',
  'gems-in-enabled-mods',
  'gems-turn-off',
  'gemfilter-summary-all',
  'gemfilter-lists-36-in-6',
  'gemfilter-refuses-none',
  'gemfilter-saves-two',
  'gemfilter-closes',
  'gems-pool-reuses-satanic-classes',
  'gemfilter-search',
  'gemfilter-enabled-disabled',
  'gemfilter-filters-never-untick',
  'gemfilter-select-all-is-vanilla',
  'gemfilter-refuses-none-by-category',
  'gems-card-follows-loot-search',
  'gems-unsaved-cue',
];
const TITLES = { mod_gem_mythic: 'Mythic Gems of Incarnation', mod_gem_maxroll: 'Max-roll Gems of Incarnation' };
const FILTER_CONTROLS = ['gemfilter_toggle', 'gemfilter_row', 'gemfilter_panel', 'gem_filter'];
// The World tab's pool classes the gem list draws with, each on the same part.
const POOL_CLASSES = ['sat-toolbar', 'sat-search', 'sat-filters', 'sat-list', 'sat-option', 'sat-name', 'sat-count', 'sat-button', 'sat-empty'];
// The Satanic pool's own strings, which the gem list reuses.
const COPY = {
  placeholder: 'Search by name or effect...',
  filters: ['All mods', 'Enabled', 'Disabled'],
  empty: 'No matching modifiers.<br>Try another search or filter.',
};

function assert(ok, message) { if (!ok) throw new Error(message); }
const $ = (page, fn, arg) => page.evaluate(fn, arg);

async function settled(page) {
  await wait(30);
  await waitSaved(page);
  await wait(30);
}

async function openLoot(page) {
  await page.click('.tabbtn[data-tab="loot"]');
  await settled(page);
}

const listed = (page) => $(page, () => [...document.querySelectorAll('#enabledMods li.enabled-mod')]
  .map((li) => ({ id: li.dataset.for, name: li.querySelector('.enabled-mod-name')?.textContent.trim() })));
const text = (page, id) => $(page, (i) => document.getElementById(i).textContent.trim(), id);
const checked = (page, id) => $(page, (i) => document.getElementById(i).checked, id);
const panelOpen = (page) => $(page, () => document.getElementById('gemfilter_panel').checkVisibility());
const cueShown = (page) => $(page, () => document.getElementById('gemfilter_unsaved').checkVisibility());
// The list as drawn: every box in drawing order, and which rows and headings show.
const pool = (page) => $(page, () => {
  const box = document.getElementById('gemfilter_panel');
  return {
    boxes: [...box.querySelectorAll('input[data-gfstat]')].map((i) => ({ stat: +i.dataset.gfstat, cat: i.dataset.gfc, checked: i.checked, shown: i.closest('.sat-option').checkVisibility() })),
    heads: [...box.querySelectorAll('.gf-cat')].map((c) => ({ name: c.firstChild.textContent.trim(), shown: c.checkVisibility(), count: c.querySelector('.sat-count')?.textContent.trim() })),
    empty: box.querySelector('.sat-empty')?.checkVisibility() ?? null,
    pressed: [...box.querySelectorAll('[data-gf-filter]')].filter((b) => b.getAttribute('aria-pressed') === 'true').map((b) => b.dataset.gfFilter),
  };
});
const shownStats = (p) => p.boxes.filter((b) => b.shown).map((b) => b.stat);
const tickedStats = (p) => p.boxes.filter((b) => b.checked).map((b) => b.stat);
const box = (stat) => `#gemfilter_panel input[data-gfstat="${stat}"]`;

// POSTs sent while `fn` runs.
async function postsDuring(page, fn) {
  const posts = [];
  const on = (r) => { if (r.method() === 'POST') posts.push({ url: new URL(r.url()).pathname, body: r.postData() }); };
  page.on('request', on);
  try { await fn(); await settled(page); } finally { page.off('request', on); }
  return posts;
}
const bodies = (posts) => posts.map((p) => JSON.parse(p.body));

// The list redrawn from the saved filter: closed if open, then opened.
async function reopen(page) {
  if (await panelOpen(page)) { await page.click('#gemfilter_toggle'); await settled(page); }
  await page.click('#gemfilter_toggle');
  await settled(page);
  assert(await panelOpen(page), 'Filter… did not open the list');
}

async function search(page, query) {
  await page.fill('#gemSearch', query);
  await settled(page);
}

// The drawing order: category by category, then the list's own order.
const drawingOrder = (state) => state.gemCategories.flatMap((cat) => state.gemAffixes.filter((a) => a[1] === cat).map((a) => a[0]));

async function switches(ctx) {
  const { page, passed } = ctx;
  await openLoot(page);

  const group = await $(page, () => {
    const card = document.getElementById('gemsCard');
    const angelic = document.getElementById('angelicCard');
    const parts = [document.getElementById('mod_gem_mythic').closest('.row'), document.getElementById('mod_gem_maxroll_row'),
      document.getElementById('gemfilter_row'), document.getElementById('gemfilter_panel')];
    const lootCards = [...document.querySelectorAll('.tab-card[data-tab="loot"]')];
    return {
      isLootCard: !!card?.matches('.card.tab-card[data-tab="loot"]'),
      afterAngelic: !!card && !!(angelic.compareDocumentPosition(card) & Node.DOCUMENT_POSITION_FOLLOWING),
      last: lootCards.at(-1) === card,
      visible: !!card?.checkVisibility(),
      heading: card?.querySelector('h2')?.textContent.trim(),
      inCard: !!card && parts.every((p) => card.contains(p)),
      ordered: parts.every((p, i) => i === 0 || !!(parts[i - 1].compareDocumentPosition(p) & Node.DOCUMENT_POSITION_FOLLOWING)),
      rows: parts.slice(0, 3).map((r) => r.classList.contains('feature-row')),
    };
  });
  assert(group.isLootCard && group.afterAngelic && group.last && group.visible,
    `#gemsCard is not the Loot tab's last card, after #angelicCard and visible on Loot: ${JSON.stringify(group)}`);
  assert(group.heading === 'Gems of Incarnation', `the card's heading reads ${group.heading}`);
  assert(group.inCard && group.ordered, `the card does not hold Mythic, Max-roll, the filter row and its list in order: ${JSON.stringify(group)}`);
  assert(group.rows.every(Boolean), `a gem row lacks feature-row: ${JSON.stringify(group.rows)}`);
  passed.push('gems-group-on-loot');

  await page.click('.tabbtn[data-tab="mods"]');
  await settled(page);
  const onMods = await $(page, () => ['qolCard', 'itemsCard'].map((id) => {
    const card = document.getElementById(id);
    return {
      id,
      controls: [...card.querySelectorAll('[id^="mod_gem_"],[id^="gemfilter"],[data-gfstat]')].map((el) => el.id || el.outerHTML.slice(0, 60)),
      rows: [...card.querySelectorAll('.row')].filter((r) => /Gems of Incarnation|Mods on Mythic gems/.test(r.textContent)).length,
    };
  }));
  assert(onMods.every((c) => c.controls.length === 0 && c.rows === 0), `a gem control or row is still on Mods: ${JSON.stringify(onMods)}`);
  passed.push('gems-not-on-mods');
  await openLoot(page);

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
  await openLoot(page);
  if (SHOTS) {
    await $(page, () => document.getElementById('gemfilter_row').scrollIntoView({ block: 'center' }));
    await page.screenshot({ path: join(SHOTS, 'gems-closed-1280.png') });
  }

  assert(await text(page, 'gemfilter_summary') === `all ${affixes.length}` && affixes.length === 36,
    `the summary reads ${await text(page, 'gemfilter_summary')} for ${affixes.length} mods`);
  passed.push('gemfilter-summary-all');

  const expanded = () => $(page, () => {
    const t = document.getElementById('gemfilter_toggle');
    return [t.getAttribute('aria-expanded'), t.getAttribute('aria-controls')];
  });
  assert(!await panelOpen(page), 'the filter list is open before Filter… is pressed');
  assert(JSON.stringify(await expanded()) === '["false","gemfilter_panel"]', `closed, Filter… reads ${JSON.stringify(await expanded())}`);
  await page.click('#gemfilter_toggle');
  await settled(page);
  assert(await panelOpen(page), 'Filter… did not open the list');
  const drawn = await pool(page);
  assert(drawn.boxes.length === 36 && drawn.boxes.every((b) => b.checked), `the list draws ${drawn.boxes.length} boxes, ${drawn.boxes.filter((b) => b.checked).length} ticked`);
  assert(JSON.stringify(drawn.heads.map((h) => h.name)) === JSON.stringify(state.gemCategories) && drawn.heads.length === 6,
    `the headings are ${JSON.stringify(drawn.heads.map((h) => h.name))}, not ${JSON.stringify(state.gemCategories)}`);
  assert(JSON.stringify(drawn.boxes.map((b) => b.stat)) === JSON.stringify(drawingOrder(state)), 'the boxes are not in category, then list, order');
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
  for (const stat of two) await page.click(box(stat));
  const saved = await postsDuring(page, () => page.click('#gemfilter_panel [data-gf="save"]'));
  assert(saved.length === 1 && JSON.stringify(JSON.parse(saved[0].body)) === JSON.stringify({ key: 'gem_filter', value: two }),
    `the save posted ${JSON.stringify(saved)}`);
  assert(await text(page, 'gemfilter_summary') === '2 of 36', `the summary reads ${await text(page, 'gemfilter_summary')} after saving two`);
  assert(JSON.stringify((await sandbox.state()).cfg.gem_filter) === JSON.stringify(two), 'the saved filter is not the two mods');
  await page.reload();
  await waitBooted(page);
  await openLoot(page);
  assert(await text(page, 'gemfilter_summary') === '2 of 36', `after a reload the summary reads ${await text(page, 'gemfilter_summary')}`);
  passed.push('gemfilter-saves-two');

  await page.click('#gemfilter_toggle');
  await settled(page);
  assert(await panelOpen(page), 'Filter… did not reopen the list');
  assert(JSON.stringify(await expanded()) === '["true","gemfilter_panel"]', `open, Filter… reads ${JSON.stringify(await expanded())}`);
  const reopened = tickedStats(await pool(page));
  assert(JSON.stringify(reopened) === JSON.stringify(two), `the reopened list ticks ${JSON.stringify(reopened)}`);
  await page.click('#gemfilter_toggle');
  await settled(page);
  assert(!await panelOpen(page), 'Filter… did not close the list');
  assert(JSON.stringify(await expanded()) === '["false","gemfilter_panel"]', `closed again, Filter… reads ${JSON.stringify(await expanded())}`);
  passed.push('gemfilter-closes');
}

async function gemPool(ctx) {
  const { page, sandbox, passed } = ctx;
  const state = await sandbox.state();
  const affixes = state.gemAffixes;
  const order = drawingOrder(state);
  const catOf = Object.fromEntries(affixes.map(([stat, cat]) => [stat, cat]));
  const nameOf = Object.fromEntries(affixes.map(([stat, , name]) => [stat, name]));
  await openLoot(page);
  await reopen(page);

  // The pool's classes, strings and attributes.
  const classes = await $(page, (names) => names.map((c) => ({
    c,
    gems: document.querySelectorAll(`#gemfilter_panel .${c}`).length,
    satanic: document.querySelectorAll(`#satanicMods .${c}`).length,
  })), POOL_CLASSES);
  assert(classes.every((x) => x.gems > 0 && x.satanic > 0), `a pool class is missing from one of the pools: ${JSON.stringify(classes)}`);
  const own = await $(page, () => {
    const panel = document.getElementById('gemfilter_panel');
    return {
      satHooks: panel.querySelectorAll('[data-sat-filter],[data-sat-polarity],.sat-footer-note').length,
      rows: [...panel.querySelectorAll('input[data-gfstat]')].map((i) => {
        const label = i.closest('label.sat-option');
        return !!label && label.classList.contains('is-enabled') === i.checked && !!label.querySelector('.sat-name');
      }),
      placeholder: document.getElementById('gemSearch')?.getAttribute('placeholder'),
      searchType: document.getElementById('gemSearch')?.type,
      filters: [...panel.querySelectorAll('.sat-filters [data-gf-filter]')].map((b) => b.textContent.trim()),
      group: panel.querySelector('.sat-filters')?.getAttribute('role'),
      empty: panel.querySelector('.sat-empty')?.innerHTML.trim(),
      disabled: panel.querySelectorAll('button:disabled,input:disabled').length,
    };
  });
  assert(own.satHooks === 0, `the gem list carries a Satanic behaviour hook: ${own.satHooks}`);
  assert(own.rows.length === 36 && own.rows.every(Boolean), 'a box is not in a label.sat-option whose is-enabled matches it');
  assert(own.placeholder === COPY.placeholder && own.searchType === 'search', `the search reads ${own.placeholder} (${own.searchType})`);
  assert(JSON.stringify(own.filters) === JSON.stringify(COPY.filters) && own.group === 'group', `the filters read ${JSON.stringify(own.filters)} (${own.group})`);
  assert(own.empty === COPY.empty, `the empty text reads ${own.empty}`);
  assert(own.disabled === 0, `${own.disabled} of the list's controls are disabled`);
  const countsOf = (p) => p.heads.map((h) => h.count);
  const expectCounts = (ticked) => state.gemCategories.map((cat) => `${order.filter((s) => catOf[s] === cat && ticked.includes(s)).length} enabled`);
  let now = await pool(page);
  assert(JSON.stringify(countsOf(now)) === JSON.stringify(expectCounts(order)), `the heading counts read ${JSON.stringify(countsOf(now))}`);
  const first = order[0];
  const tickPosts = await postsDuring(page, () => page.click(box(first)));
  now = await pool(page);
  const followed = await $(page, (s) => document.querySelector(`#gemfilter_panel input[data-gfstat="${s}"]`).closest('.sat-option').classList.contains('is-enabled'), first);
  assert(!followed && JSON.stringify(countsOf(now)) === JSON.stringify(expectCounts(order.filter((s) => s !== first))),
    `after unticking ${first} the row reads is-enabled=${followed} and the counts ${JSON.stringify(countsOf(now))}`);
  assert(tickPosts.length === 0, `a tick posted ${JSON.stringify(tickPosts)}`);
  passed.push('gems-pool-reuses-satanic-classes');

  // Search: name or category, case-insensitive; headings with no shown row hide.
  await reopen(page);
  const searchPosts = await postsDuring(page, async () => {
    for (const query of ['LOOT', 'speed', 'resist']) {
      await search(page, query);
      const q = query.toLowerCase();
      const want = order.filter((s) => nameOf[s].toLowerCase().includes(q) || catOf[s].toLowerCase().includes(q));
      const p = await pool(page);
      assert(want.length > 0 && JSON.stringify(shownStats(p)) === JSON.stringify(want), `"${query}" shows ${JSON.stringify(shownStats(p))}, not ${JSON.stringify(want)}`);
      const heads = p.heads.filter((h) => h.shown).map((h) => h.name);
      const wantHeads = state.gemCategories.filter((cat) => want.some((s) => catOf[s] === cat));
      assert(JSON.stringify(heads) === JSON.stringify(wantHeads), `"${query}" shows the headings ${JSON.stringify(heads)}, not ${JSON.stringify(wantHeads)}`);
      assert(p.empty === false, `"${query}" shows the empty text`);
    }
    await search(page, 'nothing-matches-this');
    let p = await pool(page);
    assert(shownStats(p).length === 0 && p.heads.every((h) => !h.shown) && p.empty === true, `a no-match search shows ${JSON.stringify(shownStats(p))}, empty ${p.empty}`);
    await search(page, '');
    p = await pool(page);
    assert(shownStats(p).length === 36 && p.heads.every((h) => h.shown) && p.empty === false, `a cleared search shows ${shownStats(p).length} rows`);
    assert(tickedStats(p).length === 36, `searching changed a tick: ${tickedStats(p).length} ticked`);
  });
  assert(searchPosts.length === 0, `searching posted ${JSON.stringify(searchPosts)}`);
  passed.push('gemfilter-search');

  // All mods / Enabled / Disabled: shows and hides, never ticks.
  const three = [order[0], order[13], order[27]];
  const filterPosts = await postsDuring(page, async () => {
    for (const stat of three) await page.click(box(stat));
    const counts = {};
    for (const f of ['enabled', 'disabled', 'all']) {
      await page.click(`#gemfilter_panel [data-gf-filter="${f}"]`);
      await settled(page);
      const p = await pool(page);
      counts[f] = shownStats(p);
      assert(JSON.stringify(p.pressed) === JSON.stringify([f]), `after ${f}, aria-pressed is on ${JSON.stringify(p.pressed)}`);
    }
    assert(counts.enabled.length === 33 && counts.disabled.length === 3 && counts.all.length === 36,
      `Enabled shows ${counts.enabled.length}, Disabled ${counts.disabled.length}, All mods ${counts.all.length}`);
    assert(JSON.stringify(counts.disabled) === JSON.stringify(three), `Disabled shows ${JSON.stringify(counts.disabled)}`);
    await page.click('#gemfilter_panel [data-gf-filter="enabled"]');
    await settled(page);
    const gone = order[5];
    await page.click(box(gone));
    await settled(page);
    const p = await pool(page);
    assert(!shownStats(p).includes(gone) && shownStats(p).length === 32, `unticking ${gone} under Enabled left ${shownStats(p).length} rows shown`);
    assert(tickedStats(p).length === 32, `the filters changed a tick: ${tickedStats(p).length} ticked`);
  });
  assert(filterPosts.length === 0, `the filters posted ${JSON.stringify(filterPosts)}`);
  passed.push('gemfilter-enabled-disabled');

  // Save sends every ticked row, shown or hidden.
  await reopen(page);
  await search(page, 'resist');
  const hiding = await pool(page);
  assert(shownStats(hiding).length >= 2 && shownStats(hiding).length < 10, `"resist" shows ${shownStats(hiding).length} rows`);
  let posts = await postsDuring(page, () => page.click('#gemfilter_panel [data-gf="save"]'));
  assert(JSON.stringify(bodies(posts)) === JSON.stringify([{ key: 'gem_filter', value: 'all' }]), `Save under a search posted ${JSON.stringify(posts)}`);
  const visibleTwo = shownStats(hiding).slice(0, 2);
  for (const stat of visibleTwo) await page.click(box(stat));
  posts = await postsDuring(page, () => page.click('#gemfilter_panel [data-gf="save"]'));
  const rest = order.filter((s) => !visibleTwo.includes(s));
  assert(rest.length === 34 && JSON.stringify(bodies(posts)) === JSON.stringify([{ key: 'gem_filter', value: rest }]),
    `unticking two shown rows and saving posted ${JSON.stringify(posts)}`);
  passed.push('gemfilter-filters-never-untick');

  // Everything ticked is the game's own roll: 'all'.
  await reopen(page);
  const pair = [order[1], order[20]];
  await page.click('#gemfilter_panel [data-gf="none"]');
  for (const stat of pair) await page.click(box(stat));
  posts = await postsDuring(page, () => page.click('#gemfilter_panel [data-gf="save"]'));
  assert(JSON.stringify(bodies(posts)) === JSON.stringify([{ key: 'gem_filter', value: pair }]), `saving two posted ${JSON.stringify(posts)}`);
  await reopen(page);
  await page.click('#gemfilter_panel [data-gf="all"]');
  posts = await postsDuring(page, () => page.click('#gemfilter_panel [data-gf="save"]'));
  assert(JSON.stringify(bodies(posts)) === JSON.stringify([{ key: 'gem_filter', value: 'all' }]), `Tick all then Save posted ${JSON.stringify(posts)}`);
  assert(await text(page, 'gemfilter_summary') === 'all 36', `the summary reads ${await text(page, 'gemfilter_summary')}`);
  assert((await sandbox.state()).cfg.gem_filter === 'all', 'the saved filter is not all');
  passed.push('gemfilter-select-all-is-vanilla');

  // Every category's none, then Save: the same refusal as Untick all.
  const before = JSON.stringify((await sandbox.state()).cfg.gem_filter);
  for (const cat of state.gemCategories) await page.click(`#gemfilter_panel [data-gfcat="${cat}"][data-gfset="0"]`);
  assert(tickedStats(await pool(page)).length === 0, 'every heading\'s none left a row ticked');
  posts = await postsDuring(page, () => page.click('#gemfilter_panel [data-gf="save"]'));
  assert(posts.length === 0, `saving nothing ticked posted ${JSON.stringify(posts)}`);
  assert(await text(page, 'toast') === 'Gem filter: tick at least one mod', `the toast reads ${await text(page, 'toast')}`);
  assert(JSON.stringify((await sandbox.state()).cfg.gem_filter) === before, 'the refused save changed the saved filter');
  passed.push('gemfilter-refuses-none-by-category');
}

async function lootSearch(ctx) {
  const { page, passed } = ctx;
  await openLoot(page);
  const hidden = (id) => $(page, (i) => document.getElementById(i).hidden, id);
  const find = async (query) => { await page.fill('#controlSearch', query); await settled(page); };
  const pick = async (f) => { await page.click(`[data-control-filter="${f}"]`); await settled(page); };
  await find('incarnation');
  assert(!await hidden('gemsCard') && await hidden('emptySettings'), 'a word from the card hid it, or showed the empty state');
  await find('nothing-matches-this');
  assert(await hidden('gemsCard') && !await hidden('emptySettings'), 'a no-match search left the card, or no empty state');
  await find('');
  assert(!await hidden('gemsCard'), 'a cleared search left the card hidden');
  await pick('modified');
  assert(await hidden('gemsCard'), 'Modified shows the card at the defaults');
  await pick('all');
  await page.click('#mod_gem_mythic');
  await settled(page);
  await pick('modified');
  assert(!await hidden('gemsCard'), 'Modified hides the card with Mythic on');
  await pick('all');
  passed.push('gems-card-follows-loot-search');
}

async function unsavedCue(ctx) {
  const { page, sandbox, passed } = ctx;
  const state = await sandbox.state();
  const order = drawingOrder(state);
  await openLoot(page);
  const cue = await $(page, () => {
    const el = document.getElementById('gemfilter_unsaved');
    return { text: el?.textContent.trim(), hidden: !!el?.hidden, inRow: !!el?.closest('#gemfilter_row') };
  });
  assert(cue.text === 'Unsaved changes' && cue.hidden && cue.inRow, `the cue is ${JSON.stringify(cue)}`);
  await reopen(page);
  assert(!await cueShown(page), 'the cue shows on a list drawn from the saved filter');

  // (a) From 'all', untick one: shown, nothing sent; Save: one POST, hidden.
  const x = order[3];
  const catOfX = state.gemAffixes.find((a) => a[0] === x)[1];
  let posts = await postsDuring(page, () => page.click(box(x)));
  assert(await cueShown(page), 'unticking a row did not show the cue');
  assert(posts.length === 0, `unticking a row posted ${JSON.stringify(posts)}`);
  posts = await postsDuring(page, () => page.click('#gemfilter_panel [data-gf="save"]'));
  const rest = order.filter((s) => s !== x);
  assert(JSON.stringify(bodies(posts)) === JSON.stringify([{ key: 'gem_filter', value: rest }]), `Save posted ${JSON.stringify(posts)}`);
  assert(!await cueShown(page), 'the cue still shows after Save');

  // (b) Tick it back: shown; untick it again (the saved 35): hidden. Nothing sent.
  posts = await postsDuring(page, async () => {
    await page.click(box(x));
    assert(await cueShown(page), 'ticking the row back did not show the cue');
    await page.click(box(x));
    assert(!await cueShown(page), 'returning to the saved 35 left the cue shown');
    await page.click('#gemfilter_panel [data-gf="none"]');
    assert(await cueShown(page), 'Untick all did not show the cue');
    await page.click('#gemfilter_panel [data-gf="all"]');
    assert(await cueShown(page), 'Tick all (36, the saved filter is 35) did not show the cue');
    await page.click(`#gemfilter_panel [data-gfcat="${catOfX}"][data-gfset="1"]`);
    assert(await cueShown(page), 'a heading\'s all (nothing to tick) hid the cue');
    await page.click(box(x));
    assert(!await cueShown(page), 'back at the saved 35 through Tick all and a row, the cue still shows');
  });
  assert(posts.length === 0, `ticking and unticking posted ${JSON.stringify(posts)}`);

  // An unsaved change, then close and reopen: redrawn from the saved filter.
  const y = order[30];
  await page.click(box(y));
  assert(await cueShown(page), 'unticking a second row did not show the cue');
  await reopen(page);
  assert(!await cueShown(page), 'the redraw from the saved filter left the cue shown');
  assert(JSON.stringify(tickedStats(await pool(page))) === JSON.stringify(rest), 'the redraw did not tick the saved filter');

  // design-match measures `.hint` on its first match: never the cue.
  const firstHint = await $(page, () => {
    const h = document.querySelector('.hint');
    return { cue: h?.id === 'gemfilter_unsaved', inGems: !!h?.closest('#gemsCard') };
  });
  assert(!firstHint.cue && !firstHint.inGems, `the first .hint in the page is ${JSON.stringify(firstHint)}`);
  passed.push('gems-unsaved-cue');
}

const GROUPS = [
  ['switches', switches],
  ['filter', filter],
  ['pool', gemPool],
  ['loot-search', lootSearch],
  ['unsaved-cue', unsavedCue],
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
