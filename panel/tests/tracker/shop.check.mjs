// The Tracker tab's Shopping list (#185): Shop.svelte against
// `/api/tracker/shopping`. Run by tests/tracker.e2e.mjs
// (`npm run e2e:tracker -- --only shop`).
//
// What the page shows is compared with what the API answers, never with
// literals: the list is seeded through the API, read back through it, and an
// add or a remove made in the page is read back through it too. Each refusal
// has its control beside it (a blank entry next to a real one, a full list next
// to one with room), so a check that passes cannot be a page that does nothing.

export const EXPECTED = [
  'the-list-equals-the-api',
  'add-and-remove-persist',
  'a-blank-entry-is-refused',
  'a-refusal-is-shown-and-the-list-kept',
  'copy-puts-an-entry-on-the-clipboard',
];

const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const isShoppingPost = (req) => req.method() === 'POST' && new URL(req.url()).pathname === '/api/tracker/shopping';

function assert(ok, message) {
  if (!ok) throw new Error(message);
}

const same = (a, b) => JSON.stringify(a) === JSON.stringify(b);

async function seed(t, items) {
  const res = await t.api('/api/tracker/shopping', { body: { items } });
  assert(!res.err && t.lastStatus === 200, `seeding the list was refused: ${t.lastStatus} ${JSON.stringify(res)}`);
  const read = await t.api('/api/tracker/shopping');
  assert(Array.isArray(read.items), `GET /api/tracker/shopping answered ${JSON.stringify(read)}`);
  return read.items;
}

// A fresh page with the Tracker tab's Shopping list open.
async function openShop(t) {
  const page = await t.openPanel();
  await page.click('.tabbtn[data-tab="tracker"]');
  await page.waitForFunction(() => document.getElementById('nav-tracker')?.getAttribute('aria-selected') === 'true');
  await page.click('#trk-tab-shopping');
  await page.waitForSelector('[data-section="shopping"][data-loaded]', { state: 'attached', timeout: 5000 });
  return page;
}

const shown = (page) => page.$$eval('[data-section="shopping"] .trk-shop li .trk-copy', (els) => els.map((e) => e.textContent.trim()));

async function shownEquals(page, items) {
  await page.waitForFunction((want) => {
    const got = [...document.querySelectorAll('[data-section="shopping"] .trk-shop li .trk-copy')].map((e) => e.textContent.trim());
    return JSON.stringify(got) === JSON.stringify(want);
  }, items, { timeout: 5000 }).catch(() => {});
  const got = await shown(page);
  assert(same(got, items), `the page shows ${JSON.stringify(got)}, the API holds ${JSON.stringify(items)}`);
}

async function add(page, text) {
  await page.fill('#trkShopField', text);
  await page.click('[data-section="shopping"] .trk-entry-line button[type="submit"]');
}

export default async function (t) {
  await t.check('the-list-equals-the-api', async () => {
    const items = await seed(t, ['Zed Rune', 'Angelic Key x3', 'Wyrmhide Gauntlets']);
    assert(items.length === 3, `the API kept ${JSON.stringify(items)}`);
    const page = await openShop(t);
    await shownEquals(page, items);
    const count = (await page.locator('[data-section="shopping"] .trk-count').textContent()).trim();
    assert(count === `${items.length} items`, `the count reads ${count}`);
    // The control: an empty list shows no entry and says it is empty.
    const none = await seed(t, []);
    assert(none.length === 0, `the API still holds ${JSON.stringify(none)}`);
    const other = await openShop(t);
    await shownEquals(other, none);
    assert(await other.locator('[data-section="shopping"] .trk-empty').isVisible(), 'the empty list says nothing');
  });

  await t.check('add-and-remove-persist', async () => {
    const before = await seed(t, ['Zed Rune', 'Socket stones']);
    const page = await openShop(t);
    await shownEquals(page, before);
    const [req] = await Promise.all([page.waitForRequest(isShoppingPost), add(page, '  Angelic Key x3  ')]);
    assert(same(JSON.parse(req.postData()).items, [...before, 'Angelic Key x3']), `Add posted ${req.postData()}`);
    await page.waitForFunction(() => document.getElementById('trkShopField')?.value === '', null, { timeout: 5000 });
    const added = (await t.api('/api/tracker/shopping')).items;
    assert(same(added, [...before, 'Angelic Key x3']), `after Add the API holds ${JSON.stringify(added)}`);
    await shownEquals(page, added);
    // Enter adds too.
    await page.fill('#trkShopField', 'Wyrmhide Gauntlets');
    await Promise.all([page.waitForRequest(isShoppingPost), page.press('#trkShopField', 'Enter')]);
    await page.waitForFunction(() => document.getElementById('trkShopField')?.value === '', null, { timeout: 5000 });
    const entered = (await t.api('/api/tracker/shopping')).items;
    assert(same(entered, [...added, 'Wyrmhide Gauntlets']), `after Enter the API holds ${JSON.stringify(entered)}`);
    // Remove the first entry; the rest keep their order.
    await Promise.all([
      page.waitForRequest(isShoppingPost),
      page.click(`[data-section="shopping"] .trk-shop li button[aria-label="Remove ${entered[0]}"]`),
    ]);
    await shownEquals(page, entered.slice(1));
    const removed = (await t.api('/api/tracker/shopping')).items;
    assert(same(removed, entered.slice(1)), `after Remove the API holds ${JSON.stringify(removed)}`);
    // A fresh page reads the saved list.
    const fresh = await openShop(t);
    await shownEquals(fresh, removed);
  });

  await t.check('a-blank-entry-is-refused', async () => {
    const before = await seed(t, ['Zed Rune']);
    // The API refuses a blank entry and keeps the list.
    const res = await t.api('/api/tracker/shopping', { body: { items: [...before, '   '] } });
    assert(t.lastStatus === 400 && typeof res.err === 'string', `a blank entry answered ${t.lastStatus} ${JSON.stringify(res)}`);
    assert(same((await t.api('/api/tracker/shopping')).items, before), 'the refused write changed the list');
    // The page sends nothing for a blank entry.
    const page = await openShop(t);
    await shownEquals(page, before);
    const posts = [];
    page.on('request', (r) => { if (isShoppingPost(r)) posts.push(r.postData()); });
    await add(page, '    ');
    await page.press('#trkShopField', 'Enter');
    await wait(1000);
    assert(posts.length === 0, `a blank entry posted ${JSON.stringify(posts)}`);
    await shownEquals(page, before);
    // The control: the same field with a name posts at once.
    await Promise.all([page.waitForRequest(isShoppingPost), add(page, 'Socket stones')]);
    await shownEquals(page, [...before, 'Socket stones']);
    assert(same((await t.api('/api/tracker/shopping')).items, [...before, 'Socket stones']), 'the control did not save');
  });

  await t.check('a-refusal-is-shown-and-the-list-kept', async () => {
    // A full list: the API holds at most 200 entries.
    const full = await seed(t, Array.from({ length: 200 }, (_, i) => `Item ${i + 1}`));
    assert(full.length === 200, `the API kept ${full.length} entries`);
    const over = await t.api('/api/tracker/shopping', { body: { items: [...full, 'One more'] } });
    assert(t.lastStatus === 400 && typeof over.err === 'string', `a 201st entry answered ${t.lastStatus} ${JSON.stringify(over)}`);
    const page = await openShop(t);
    await shownEquals(page, full);
    await Promise.all([page.waitForResponse((r) => isShoppingPost(r.request())), add(page, 'One more')]);
    const error = page.locator('[data-section="shopping"] .trk-error');
    await error.waitFor({ state: 'visible', timeout: 5000 });
    const text = (await error.textContent()).trim();
    assert(text.includes(over.err), `the page shows "${text}", the API said "${over.err}"`);
    assert(await page.inputValue('#trkShopField') === 'One more', 'the refused entry was cleared from the field');
    await shownEquals(page, full);
    assert((await t.api('/api/tracker/shopping')).items.length === 200, 'the refused add changed the list');
    // The control: with room again, the same entry saves and the message goes.
    await Promise.all([
      page.waitForRequest(isShoppingPost),
      page.click(`[data-section="shopping"] .trk-shop li button[aria-label="Remove ${full[0]}"]`),
    ]);
    await shownEquals(page, full.slice(1));
    await Promise.all([page.waitForRequest(isShoppingPost), page.click('[data-section="shopping"] .trk-entry-line button[type="submit"]')]);
    await shownEquals(page, [...full.slice(1), 'One more']);
    assert(await error.count() === 0, 'the refusal stayed after a save');
    await seed(t, []);
  });

  await t.check('copy-puts-an-entry-on-the-clipboard', async () => {
    const items = await seed(t, ['Zed Rune', 'Angelic Key x3']);
    const page = await openShop(t);
    await page.context().grantPermissions(['clipboard-read', 'clipboard-write'], { origin: new URL(t.sandbox.url).origin });
    await shownEquals(page, items);
    await page.evaluate(() => navigator.clipboard.writeText('before'));
    // Each entry copies its own text, not its neighbour's.
    for (const [i, item] of items.entries()) {
      await page.locator('[data-section="shopping"] .trk-shop li .trk-copy').nth(i).click();
      const copied = page.locator('[data-section="shopping"] .trk-shop li').nth(i).locator('.trk-copied');
      await copied.waitFor({ state: 'visible', timeout: 800 }).catch(() => {});
      assert(await copied.isVisible(), `entry ${i} does not say it was copied`);
      await page.waitForFunction(async (want) => (await navigator.clipboard.readText()) === want, item, { timeout: 3000 }).catch(() => {});
      const got = await page.evaluate(() => navigator.clipboard.readText());
      assert(got === item, `copying entry ${i} put ${JSON.stringify(got)} on the clipboard, not ${JSON.stringify(item)}`);
    }
    // Copying changes nothing on the list.
    assert(same((await t.api('/api/tracker/shopping')).items, items), 'copying changed the saved list');
    await seed(t, []);
  });
}
