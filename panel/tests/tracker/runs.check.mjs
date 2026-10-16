// The Tracker tab's Runs section (#185): the run history, the picked run, the
// run card and its copy, and Clear history. Run by tests/tracker.e2e.mjs
// (`npm run e2e:tracker -- --only runs`).
//
// Every expected value comes from the sandbox's API, never from a literal: the
// list is compared with `/api/tracker/runs`, the imported runs with the
// fixture folder the sandbox imported them from, and the card's background
// with the page's own `--color-bg-raised`. The clipboard is the browser's
// own, with the permission granted to the page; the one refusal is a page
// init script that makes `navigator.clipboard.write` reject, the way a page
// without the permission sees it. No test hook ships in the product.

import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { fmt } from '../../src/tracker/format.js';

export const EXPECTED = [
  'empty-history-says-so',
  'list-equals-the-api-newest-first',
  'tracker-runs-show-after-the-import',
  'card-draws-on-its-canvas-in-the-palette',
  'copy-puts-a-png-on-the-clipboard',
  'copy-says-so-when-the-page-cannot',
  'clear-asks-first-then-posts',
];

const SOURCE = fileURLToPath(new URL('./fixtures/runs-source/', import.meta.url));
const SOURCE_RUNS = JSON.parse(readFileSync(new URL('./fixtures/runs-source/runs.json', import.meta.url), 'utf8'));

const wait = (ms) => new Promise((r) => setTimeout(r, ms));

function assert(ok, message) {
  if (!ok) throw new Error(message);
}

// What a run is, for comparing the page's rows with the API's runs.
const ident = (run) => `${run.started_ms}:${run.ended_ms}`;

async function openRuns(t, options = {}) {
  const page = await t.openPanel(options);
  await page.click('.tabbtn[data-tab="tracker"]');
  await page.waitForFunction(() => document.getElementById('nav-tracker')?.getAttribute('aria-selected') === 'true');
  await page.click('#trk-tab-runs');
  await page.waitForSelector('[data-section="runs"]', { state: 'attached' });
  return page;
}

// The rows the page shows, in order, once it shows `count` of them.
async function rows(page, count) {
  await page.waitForFunction((n) => document.querySelectorAll('#trkRunList .trk-run').length === n, count, { timeout: 8000 })
    .catch(async () => {
      const shown = await page.locator('#trkRunList .trk-run').count();
      throw new Error(`the list shows ${shown} runs, the API ${count}`);
    });
  return page.$$eval('#trkRunList .trk-run', (buttons) => buttons.map((b) => ({
    ident: `${b.dataset.started}:${b.dataset.ended}`,
    text: b.textContent.replace(/\s+/g, ' ').trim(),
    current: b.getAttribute('aria-current') === 'true',
  })));
}

function sameOrder(shown, runs) {
  const want = runs.map(ident);
  const got = shown.map((r) => r.ident);
  assert(JSON.stringify(got) === JSON.stringify(want), `the list reads ${got.join(', ')}, the API ${want.join(', ')}`);
  runs.forEach((run, i) => {
    const gold = `${fmt(run.gold)} gold`;
    assert(shown[i].text.includes(gold), `row ${i} does not say ${gold}: ${shown[i].text}`);
  });
}

async function endRun(t) {
  const res = await t.api('/api/tracker/reset', { body: {} });
  assert(!res.err && t.lastStatus === 200, `End run was refused: ${t.lastStatus} ${JSON.stringify(res)}`);
}

// The pixels of the run card's canvas: its size, the share of pixels that
// differ from `bg` (an [r, g, b]) by more than a rounding, and its corner.
async function cardPixels(page) {
  await page.waitForFunction(() => {
    const c = document.getElementById('trkRunCard');
    return c && c.width > 300 && c.height > 150;
  }, null, { timeout: 8000 });
  return page.evaluate(() => {
    const css = (name) => getComputedStyle(document.documentElement).getPropertyValue(name).trim();
    const hex = css('--color-bg-raised');
    const bg = [1, 3, 5].map((i) => parseInt(hex.slice(i, i + 2), 16));
    const c = document.getElementById('trkRunCard');
    const data = c.getContext('2d').getImageData(0, 0, c.width, c.height).data;
    let other = 0;
    for (let i = 0; i < data.length; i += 4) {
      const d = Math.abs(data[i] - bg[0]) + Math.abs(data[i + 1] - bg[1]) + Math.abs(data[i + 2] - bg[2]);
      if (d > 12) other++;
    }
    return { hex, width: c.width, height: c.height, share: other / (data.length / 4), corner: [...data.slice(0, 4)], bg, label: c.getAttribute('aria-label') || '' };
  });
}

export default async function (t) {
  // A sandbox with HS Offline Tracker's own run history to import, and no
  // session of its own. Started once, by the first check that needs it.
  let imported = null;
  const importedBox = async () => {
    if (!imported) imported = await t.startSandbox({ trackerSource: SOURCE });
    return imported;
  };

  await t.check('empty-history-says-so', async () => {
    const api = await t.api('/api/tracker/runs');
    assert(Array.isArray(api.runs) && api.runs.length === 0, `the sandbox starts with runs: ${JSON.stringify(api)}`);
    const page = await openRuns(t);
    await page.waitForSelector('#trkRunsEmpty', { timeout: 8000 });
    assert(await page.locator('#trkRunList .trk-run').count() === 0, 'rows are shown with no runs');
    assert(await page.locator('#trkRunsClear').count() === 0, 'Clear history is offered with nothing to clear');
    assert(await page.locator('#trkRunCard').count() === 0, 'a card is drawn with no run');
  });

  await t.check('list-equals-the-api-newest-first', async () => {
    const page = await openRuns(t);
    await endRun(t);
    const first = (await t.api('/api/tracker/runs')).runs;
    assert(first.length === 1, `End run filed ${first.length} runs`);
    sameOrder(await rows(page, 1), first);
    // A second run: the session goes on two minutes past the reset, earns
    // something, and is ended by hand again.
    const now = Date.now();
    t.appendSession([
      { kind: 'session_delta', gold: 777, xp: 55, kills: 3, source: 'forgepact_tracker', emitted_at_unix_ms: now + 120000 },
      { kind: 'heartbeat', emitted_at_unix_ms: now + 125000 },
    ]);
    // Let the poll read the records before the run is ended.
    const until = Date.now() + 8000;
    while (Date.now() < until) {
      const live = await t.api('/api/tracker');
      if (live.snapshot?.gold?.earned === 777) break;
      await wait(250);
    }
    await endRun(t);
    const both = (await t.api('/api/tracker/runs')).runs;
    assert(both.length === 2, `the second End run left ${both.length} runs`);
    assert(both[1].id === first[0].id, 'the first run is not second in the API after the next was filed');
    assert(both[0].ended_ms > both[1].ended_ms, `the API is not newest first: ${both.map((r) => r.ended_ms)}`);
    const shown = await rows(page, 2);
    sameOrder(shown, both);
    assert(shown[0].current && !shown[1].current, 'the newest run is not the picked one');
    const title = await page.locator('#trkRunTitle').textContent();
    assert(title.trim().length > 0, 'the picked run has no heading');
    const gold = await page.locator('#trkRunDetail [data-total="gold"] .trk-total-value').textContent();
    assert(gold.trim() === fmt(both[0].gold), `the picked run's gold reads ${gold}, the API ${fmt(both[0].gold)}`);
  });

  await t.check('tracker-runs-show-after-the-import', async () => {
    const box = await importedBox();
    const api = (await t.api('/api/tracker/runs', {}, box)).runs;
    assert(JSON.stringify(api) === JSON.stringify(SOURCE_RUNS),
      `the imported history is not the Tracker's runs.json: ${JSON.stringify(api).slice(0, 300)}`);
    // The control: the module's own sandbox imported nothing.
    const own = (await t.api('/api/tracker/runs')).runs;
    assert(!own.some((r) => SOURCE_RUNS.some((s) => ident(s) === ident(r))), 'the sandbox with no Tracker folder shows its runs');
    const page = await openRuns(t, { sandbox: box });
    const shown = await rows(page, api.length);
    sameOrder(shown, api);
    // Picking the older one, filed by a Tracker before 0.9.8 (no tallies),
    // shows it, and only what it has.
    await page.locator('#trkRunList .trk-run').nth(1).click();
    await page.waitForFunction(() => document.querySelectorAll('#trkRunList .trk-run')[1]?.getAttribute('aria-current') === 'true');
    const kills = await page.locator('#trkRunDetail [data-total="kills"] .trk-total-value').textContent();
    assert(kills.trim() === fmt(api[1].kills), `the older run's kills read ${kills}, the API ${fmt(api[1].kills)}`);
    assert(await page.locator('#trkRunDetail [data-group="tallies"]').count() === 0, 'a run with no tallies shows Killed and opened');
    await page.locator('#trkRunList .trk-run').nth(0).click();
    await page.waitForSelector('#trkRunDetail [data-group="tallies"]');
  });

  await t.check('card-draws-on-its-canvas-in-the-palette', async () => {
    const box = await importedBox();
    const api = (await t.api('/api/tracker/runs', {}, box)).runs;
    // Ember, then Ledger: two palettes, two backgrounds, each the page's own.
    const seen = [];
    for (const theme of ['ember', 'ledger']) {
      const set = await t.api('/api/set', { body: { key: 'theme', value: theme } }, box);
      assert(!set.err, `theme ${theme} was refused: ${set.err}`);
      const page = await openRuns(t, { sandbox: box });
      await rows(page, api.length);
      const px = await cardPixels(page);
      assert(px.width === 960, `the card is ${px.width} px wide, not 480 at twice the density`);
      assert(px.share > 0.02, `only ${(px.share * 100).toFixed(2)}% of the card differs from its background: nothing was drawn`);
      assert(px.share < 0.6, `${(px.share * 100).toFixed(2)}% of the card differs from ${px.hex}: it is not drawn on the page's background`);
      const diff = px.corner.slice(0, 3).reduce((s, v, i) => s + Math.abs(v - px.bg[i]), 0);
      assert(diff <= 6 && px.corner[3] === 255, `the card's corner is ${px.corner}, the page's --color-bg-raised ${px.hex}`);
      assert(px.label.includes(api[0].character), `the card's label does not name the run's character: ${px.label}`);
      seen.push(px.hex);
    }
    assert(seen[0] !== seen[1], `Ember and Ledger drew on the same background ${seen[0]}; the check cannot tell a palette from a literal`);
  });

  await t.check('copy-puts-a-png-on-the-clipboard', async () => {
    const box = await importedBox();
    const api = (await t.api('/api/tracker/runs', {}, box)).runs;
    const page = await openRuns(t, {
      sandbox: box,
      routes: async (p) => { await p.context().grantPermissions(['clipboard-read', 'clipboard-write'], { origin: box.url }); },
    });
    await rows(page, api.length);
    await cardPixels(page);
    await page.click('#trkRunCopy');
    await page.waitForFunction(() => (document.getElementById('trkRunCopyState')?.textContent || '').trim().length > 0, null, { timeout: 8000 });
    const said = (await page.locator('#trkRunCopyState').textContent()).trim();
    assert(/^Copied/.test(said), `Copy card said ${said}`);
    const got = await page.evaluate(async () => {
      const items = await navigator.clipboard.read();
      const item = items.find((i) => i.types.includes('image/png'));
      if (!item) return { types: items.flatMap((i) => i.types) };
      const blob = await item.getType('image/png');
      const bitmap = await createImageBitmap(blob);
      const c = document.getElementById('trkRunCard');
      return { types: item.types, size: blob.size, width: bitmap.width, height: bitmap.height, cw: c.width, ch: c.height };
    });
    assert(got.width, `the clipboard holds no image/png: ${JSON.stringify(got.types)}`);
    assert(got.width === got.cw && got.height === got.ch, `the clipboard's picture is ${got.width}x${got.height}, the card ${got.cw}x${got.ch}`);
  });

  await t.check('copy-says-so-when-the-page-cannot', async () => {
    const box = await importedBox();
    const api = (await t.api('/api/tracker/runs', {}, box)).runs;
    const page = await openRuns(t, {
      sandbox: box,
      routes: async (p) => {
        await p.addInitScript(() => {
          const refuse = () => Promise.reject(new DOMException('Write permission denied.', 'NotAllowedError'));
          Object.defineProperty(navigator.clipboard, 'write', { value: refuse, configurable: true });
        });
      },
    });
    await rows(page, api.length);
    await cardPixels(page);
    await page.click('#trkRunCopy');
    await page.waitForFunction(() => (document.getElementById('trkRunCopyState')?.textContent || '').trim().length > 0, null, { timeout: 8000 });
    const said = (await page.locator('#trkRunCopyState').textContent()).trim();
    assert(/^Could not copy the card/.test(said), `a refused copy said ${JSON.stringify(said)}`);
  });

  await t.check('clear-asks-first-then-posts', async () => {
    const before = (await t.api('/api/tracker/runs')).runs;
    assert(before.length > 0, 'nothing to clear: the list check filed no runs');
    const page = await openRuns(t);
    await rows(page, before.length);
    const clears = [];
    page.on('request', (req) => { if (req.method() === 'POST' && new URL(req.url()).pathname === '/api/tracker/runs/clear') clears.push(req); });
    // Asking first: the button opens a confirmation and posts nothing.
    await page.click('#trkRunsClear');
    await page.waitForSelector('#trkRunsConfirm', { timeout: 5000 });
    await wait(500);
    assert(clears.length === 0, 'Clear history posted before it was confirmed');
    // Keep them: nothing posted, nothing gone.
    await page.click('#trkRunsKeep');
    await page.waitForSelector('#trkRunsConfirm', { state: 'detached' });
    await wait(500);
    assert(clears.length === 0, 'Keep them posted a clear');
    assert((await t.api('/api/tracker/runs')).runs.length === before.length, 'Keep them lost runs');
    // Confirmed: one post, then an empty history in the API and on the page.
    await page.click('#trkRunsClear');
    await Promise.all([
      page.waitForRequest((r) => r.method() === 'POST' && new URL(r.url()).pathname === '/api/tracker/runs/clear'),
      page.click('#trkRunsClearConfirm'),
    ]);
    await page.waitForSelector('#trkRunsEmpty', { timeout: 8000 });
    assert(clears.length === 1, `${clears.length} clear requests`);
    const after = (await t.api('/api/tracker/runs')).runs;
    assert(after.length === 0, `the API still holds ${after.length} runs`);
  });
}
