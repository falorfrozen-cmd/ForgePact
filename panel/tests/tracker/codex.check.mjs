// The Tracker tab's Item codex (#185): Codex.svelte against
// `/api/tracker/codex`. Run by tests/tracker.e2e.mjs
// (`npm run e2e:tracker -- --only codex`).
//
// What the page shows is compared with what the API answers, never with
// literals: the rows, the counts, a search and a filter are worked out here
// from the API's own answer, then read off the page. Each narrowing has its
// control beside it (the full list before it, the list again after it is
// cleared), and the request count has one too (the tracker's own polls, seen
// by the same listener), so a check that passes cannot be a page that does
// nothing or a listener that sees nothing.

export const EXPECTED = [
  'fetched-only-when-the-section-opens',
  'the-row-count-equals-the-api',
  'a-name-search-narrows-the-list',
  'a-rarity-filter-narrows-the-list',
  'a-kind-filter-narrows-the-list',
  'a-tied-item-shows-its-places-and-better-chance',
];

const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const pathOf = (req) => new URL(req.url()).pathname;
const SECTION = '[data-section="codex"]';
const ROW = `${SECTION} .trk-codex tbody tr`;

function assert(ok, message) {
  if (!ok) throw new Error(message);
}

const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);
const grouped = (n) => n.toLocaleString('en-US');
const chance = (n) => (n > 0 ? `1/${grouped(n)}` : '—');

async function codexRows(t) {
  const res = await t.api('/api/tracker/codex');
  assert(t.lastStatus === 200 && Array.isArray(res.items) && res.items.length > 0,
    `GET /api/tracker/codex answered ${t.lastStatus} ${JSON.stringify(res).slice(0, 200)}`);
  return res.items;
}

// The page's own order, worked out independently: rarest first (the longest
// odds), the unpriced last, equal odds by name.
const byName = (a, b) => a.name.localeCompare(b.name, 'en');
const rarest = (rows) => [...rows].sort((a, b) => (b.rate > 0) - (a.rate > 0) || (b.rate || 0) - (a.rate || 0) || byName(a, b));

// A fresh page on the Tracker tab, its Session section open; `seen` collects
// every `/api/tracker*` request the page sends from its first load on.
async function openTracker(t) {
  const seen = [];
  const page = await t.openPanel({
    routes: async (p) => { p.on('request', (req) => { if (pathOf(req).startsWith('/api/tracker')) seen.push(pathOf(req)); }); },
  });
  await page.click('.tabbtn[data-tab="tracker"]');
  await page.waitForFunction(() => document.getElementById('nav-tracker')?.getAttribute('aria-selected') === 'true');
  return { page, seen };
}

async function openCodex(page) {
  await page.click('#trk-tab-codex');
  await page.waitForSelector(`${SECTION}[data-loaded]`, { state: 'attached', timeout: 10000 });
}

async function freshCodex(t) {
  const { page, seen } = await openTracker(t);
  await openCodex(page);
  return { page, seen };
}

const shownNames = (page) => page.$$eval(`${ROW} td.trk-c-name`, (els) => els.map((e) => e.textContent.trim()));
const countText = async (page) => (await page.locator(`${SECTION} .trk-count`).textContent()).trim();

// Wait until the page shows `want` rows' names, then compare.
async function shownEquals(page, want, what) {
  await page.waitForFunction(({ sel, names }) => {
    const got = [...document.querySelectorAll(sel)].map((e) => e.textContent.trim());
    return JSON.stringify(got) === JSON.stringify(names);
  }, { sel: `${ROW} td.trk-c-name`, names: want }, { timeout: 5000 }).catch(() => {});
  const got = await shownNames(page);
  assert(same(got, want), `${what}: the page shows ${got.length} rows (${JSON.stringify(got.slice(0, 5))}...), the API calls for ${want.length} (${JSON.stringify(want.slice(0, 5))}...)`);
}

// How many rows the page draws at once, read off its own note ("Showing N of M").
async function shownCap(page, matches) {
  const note = page.locator(`${SECTION} .trk-codex-more`);
  if (!(await note.count())) return matches;
  const m = /Showing ([\d,]+) of ([\d,]+)/.exec(await note.textContent());
  assert(m, `the note reads "${await note.textContent()}"`);
  assert(Number(m[2].replace(/,/g, '')) === matches, `the note says ${m[2]} match, the API ${matches}`);
  return Number(m[1].replace(/,/g, ''));
}

// A word in some names that picks out a few rows, worked out from the API.
function searchTerm(rows) {
  const counts = new Map();
  for (const r of rows) for (const w of new Set(r.name.toLowerCase().split(/[^a-z]+/).filter((w) => w.length >= 5))) counts.set(w, (counts.get(w) || 0) + 1);
  const pick = [...counts].filter(([, n]) => n >= 3 && n <= 25).sort((a, b) => b[1] - a[1] || a[0].localeCompare(b[0]))[0];
  assert(pick, 'no word in the codex names picks out 3 to 25 rows');
  return pick[0];
}

export default async function (t) {
  await t.check('fetched-only-when-the-section-opens', async () => {
    const { page, seen } = await openTracker(t);
    // The Session section is open and the tab polls `/api/tracker`: the
    // control that the listener sees the page's tracker requests at all.
    await page.waitForFunction(() => document.getElementById('trk-tab-session')?.getAttribute('aria-selected') === 'true');
    await wait(1500);
    assert(seen.includes('/api/tracker'), `the listener saw no tracker poll: ${JSON.stringify(seen)}`);
    assert(!seen.includes('/api/tracker/codex'), `the codex was fetched before its section opened: ${JSON.stringify(seen)}`);
    const before = await page.evaluate(() => performance.getEntriesByType('resource').filter((e) => new URL(e.name).pathname === '/api/tracker/codex').length);
    assert(before === 0, `the page's own record shows ${before} codex requests before the section opened`);
    // Opening the section fetches it, once.
    await openCodex(page);
    await wait(500);
    const fetched = seen.filter((p) => p === '/api/tracker/codex').length;
    assert(fetched === 1, `opening the section sent ${fetched} codex requests`);
    assert((await page.locator(ROW).count()) > 0, 'the section opened with no rows');
  });

  await t.check('the-row-count-equals-the-api', async () => {
    const rows = await codexRows(t);
    const { page } = await freshCodex(t);
    const total = rows.length;
    assert((await countText(page)) === `${grouped(total)} of ${grouped(total)}`, `the count reads "${await countText(page)}", the API has ${total} rows`);
    const cap = await shownCap(page, total);
    assert(cap > 0 && cap <= total, `the page draws ${cap} of ${total}`);
    // Rarest first by default: the page's first rows are the API's rarest.
    await shownEquals(page, rarest(rows).slice(0, cap).map((r) => r.name), 'rarest first');
    // By name: the API's first names.
    await page.selectOption('#trkCodexOrder', 'name');
    await shownEquals(page, [...rows].sort(byName).slice(0, cap).map((r) => r.name), 'by name');
  });

  await t.check('a-name-search-narrows-the-list', async () => {
    const rows = await codexRows(t);
    const term = searchTerm(rows);
    const want = rarest(rows.filter((r) => r.name.toLowerCase().includes(term)));
    const { page } = await freshCodex(t);
    // The control: the whole codex first.
    assert((await countText(page)) === `${grouped(rows.length)} of ${grouped(rows.length)}`, `before the search the count reads "${await countText(page)}"`);
    // Typed in another case, with spaces around it.
    await page.fill('#trkCodexSearch', `  ${term.toUpperCase()} `);
    await shownEquals(page, want.map((r) => r.name), `the search "${term}"`);
    assert((await countText(page)) === `${grouped(want.length)} of ${grouped(rows.length)}`, `after the search the count reads "${await countText(page)}"`);
    assert(await page.locator(`${SECTION} .trk-codex-more`).count() === 0, 'a list shown whole still says there is more');
    // A search that finds nothing says so.
    await page.fill('#trkCodexSearch', 'zzqx no such item');
    await shownEquals(page, [], 'a search for nothing');
    assert(await page.locator(`${SECTION} .trk-empty`).isVisible(), 'a search that found nothing says nothing');
    // Cleared, the whole codex is back.
    await page.fill('#trkCodexSearch', '');
    const cap = await shownCap(page, rows.length);
    await shownEquals(page, rarest(rows).slice(0, cap).map((r) => r.name), 'the cleared search');
  });

  await t.check('a-rarity-filter-narrows-the-list', async () => {
    const rows = await codexRows(t);
    const counts = new Map();
    for (const r of rows) if (r.rarity) counts.set(r.rarity, (counts.get(r.rarity) || 0) + 1);
    // The rarest rarity the codex lists: few enough to be shown whole.
    const [rarity] = [...counts].sort((a, b) => a[1] - b[1])[0];
    const want = rarest(rows.filter((r) => r.rarity === rarity));
    const { page } = await freshCodex(t);
    const offered = await page.$$eval('#trkCodexRarity option', (os) => os.map((o) => o.value).filter(Boolean));
    assert(same([...offered].sort(), [...counts.keys()].sort()), `the rarity filter offers ${JSON.stringify(offered)}, the API's rows carry ${JSON.stringify([...counts.keys()])}`);
    await page.selectOption('#trkCodexRarity', rarity);
    const cap = await shownCap(page, want.length);
    await shownEquals(page, want.slice(0, cap).map((r) => r.name), `the rarity ${rarity}`);
    assert((await countText(page)) === `${grouped(want.length)} of ${grouped(rows.length)}`, `with ${rarity} the count reads "${await countText(page)}"`);
    // The rarity's word stands beside its colour on every row.
    const words = await page.$$eval(`${ROW} td.trk-c-rarity`, (els) => els.map((e) => e.textContent.trim()));
    assert(words.length && words.every((w) => w === rarity), `the rarity column reads ${JSON.stringify([...new Set(words)])}`);
    // The control: any rarity again.
    await page.selectOption('#trkCodexRarity', '');
    assert((await countText(page)) === `${grouped(rows.length)} of ${grouped(rows.length)}`, `back to any rarity the count reads "${await countText(page)}"`);
  });

  await t.check('a-kind-filter-narrows-the-list', async () => {
    const rows = await codexRows(t);
    const counts = new Map();
    for (const r of rows) counts.set(r.group, (counts.get(r.group) || 0) + 1);
    const [group] = [...counts].filter(([, n]) => n >= 2).sort((a, b) => a[1] - b[1] || a[0].localeCompare(b[0]))[0];
    const want = rarest(rows.filter((r) => r.group === group));
    const { page } = await freshCodex(t);
    const offered = await page.$$eval('#trkCodexKind option', (os) => os.map((o) => o.value).filter(Boolean));
    assert(same([...offered].sort(), [...counts.keys()].sort()), `the kind filter offers ${JSON.stringify(offered)}, the API's rows carry ${JSON.stringify([...counts.keys()])}`);
    await page.selectOption('#trkCodexKind', group);
    const cap = await shownCap(page, want.length);
    await shownEquals(page, want.slice(0, cap).map((r) => r.name), `the kind ${group}`);
    assert((await countText(page)) === `${grouped(want.length)} of ${grouped(rows.length)}`, `with ${group} the count reads "${await countText(page)}"`);
    // A search stacks on the filter.
    const one = want[0];
    await page.fill('#trkCodexSearch', one.name);
    const both = rarest(rows.filter((r) => r.group === group && r.name.toLowerCase().includes(one.name.toLowerCase())));
    await shownEquals(page, both.map((r) => r.name), `the kind ${group} and the search "${one.name}"`);
  });

  await t.check('a-tied-item-shows-its-places-and-better-chance', async () => {
    const rows = await codexRows(t);
    const unique = (r) => rows.filter((o) => o.name.toLowerCase().includes(r.name.toLowerCase())).length === 1;
    const tied = rows.find((r) => r.chase > 0 && r.rate > 0 && r.places.length && r.zones.length && unique(r));
    assert(tied, 'the API has no item tied to a place with both chances');
    // The control: an item tied nowhere, with a chance anywhere.
    const free = rows.find((r) => !r.chase && r.rate > 0 && !r.places.length && !r.zones.length && unique(r));
    assert(free, 'the API has no item that drops anywhere');
    const { page } = await freshCodex(t);
    const cells = async (name) => {
      await page.fill('#trkCodexSearch', name);
      await shownEquals(page, [name], `the search "${name}"`);
      return page.$eval(ROW, (tr) => Object.fromEntries([...tr.cells].map((td) => [td.className.match(/trk-c-(\w+)/)?.[1], td.textContent.trim()])));
    };
    const a = await cells(tied.name);
    assert(a.where === tied.places.join(' · '), `${tied.name}: its places read "${a.where}", the API says ${JSON.stringify(tied.places)}`);
    assert(a.tied === chance(tied.chase), `${tied.name}: its tied chance reads "${a.tied}", the API says ${tied.chase}`);
    assert(a.anywhere === chance(tied.rate), `${tied.name}: its chance anywhere reads "${a.anywhere}", the API says ${tied.rate}`);
    assert(a.rarity === tied.rarity && a.kind === tied.type, `${tied.name}: reads ${JSON.stringify(a)}`);
    const b = await cells(free.name);
    assert(b.where === 'anywhere', `${free.name}: its places read "${b.where}"`);
    assert(b.tied === '—', `${free.name}: its tied chance reads "${b.tied}"`);
    assert(b.anywhere === chance(free.rate), `${free.name}: its chance anywhere reads "${b.anywhere}", the API says ${free.rate}`);
  });
}
