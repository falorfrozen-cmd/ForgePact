// The Tracker tab's Session section (#185): the totals and rates, the clock,
// loot, notable counts and grades, the tallies, the Satanic zone, the series,
// the journal, pause and End run, catching up and the empty state. Run by
// tests/tracker.e2e.mjs (`npm run e2e:tracker -- --only session`).
//
// Every expected value is read from `/api/tracker`, `/api/tracker/extra` or
// `/api/state` in the check, never written here: the page must show what the
// API answered. Where the page writes a value in the game's dialect, the
// check says it the same way through src/tracker/session/view.js, whose
// formatting tests/tracker-session.test.js pins on its own.
//
// One answer the sandbox cannot give: tallies. They come only from a
// character save, and the sandbox reads none. So the tallies check takes the
// real answer first (no tallies, and the card says so), then serves the
// page that same answer with tallies added and checks it shows those.

import { chartPoints, journalRows, modRows, notableRows, totals, zoneName } from '../../src/tracker/session/view.js';
import { clock, fmt, RARITIES } from '../../src/tracker/format.js';

export const EXPECTED = [
  'totals-match-the-api',
  'clock-and-character-match-the-api',
  'zone-chip-names-the-zone-and-its-mods',
  'series-draws-every-sample',
  'tallies-show-what-the-api-gives',
  'loot-notable-and-grades-follow-new-drops',
  'journal-is-newest-first-and-equals-extra',
  'nothing-a-poll-repaints-animates',
  'pause-and-resume-post-pause',
  'end-run-asks-first-then-posts-reset',
  'catching-up-is-shown',
  'empty-state-with-no-session',
];

const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const isPost = (path) => (r) => r.method() === 'POST' && new URL(r.url()).pathname === path;

function assert(ok, message) {
  if (!ok) throw new Error(message);
}

async function setSetting(t, key, value, box) {
  const res = await t.api('/api/set', { body: { key, value } }, box);
  assert(!res.err, `setting ${key}=${JSON.stringify(value)} was refused: ${res.err}`);
}

// The Tracker tab open on its Session section, once the first answer is drawn.
async function openSession(t, options = {}) {
  const page = await t.openPanel(options);
  await page.click('.tabbtn[data-tab="tracker"]');
  await page.waitForFunction(() => document.getElementById('trk-tab-session')?.getAttribute('aria-selected') === 'true');
  await page.waitForFunction(() => ['session', 'none'].includes(document.querySelector('[data-section="session"]')?.dataset.state),
    null, { timeout: 10000 });
  return page;
}

// Wait until `read` (run in the page) returns what `want` is, then compare.
async function until(page, read, want, what, arg = null, timeout = 6000) {
  const end = Date.now() + timeout;
  let got;
  for (;;) {
    got = await page.evaluate(read, arg);
    if (JSON.stringify(got) === JSON.stringify(want)) return got;
    if (Date.now() > end) throw new Error(`${what}: the page shows ${JSON.stringify(got)}, the API says ${JSON.stringify(want)}`);
    await wait(150);
  }
}

// The page has drawn the snapshot the API answers now.
async function settled(t, page) {
  const body = await t.api('/api/tracker');
  await until(page, () => document.querySelector('[data-section="session"]')?.dataset.revision, String(body.snapshot.revision),
    'the snapshot revision');
  return body;
}

const readTotals = () => [...document.querySelectorAll('#trkSessionCard .trk-total')].map((el) => ({
  key: el.dataset.key,
  earned: Number(el.querySelector('.trk-total-value').dataset.value),
  value: el.querySelector('.trk-total-value').textContent.trim(),
  per_hour: Number(el.querySelector('.trk-total-rate').dataset.value),
  rate: el.querySelector('.trk-total-rate').textContent.trim(),
  total: el.querySelector('.trk-total-note').dataset.total,
  note: el.querySelector('.trk-total-note').textContent.trim(),
}));

const expectTotals = (snap) => totals(snap).map((r) => ({
  key: r.key, earned: r.earned, value: r.value, per_hour: r.per_hour, rate: r.rate, total: r.total == null ? '' : String(r.total), note: r.note,
}));

const readLoot = () => [...document.querySelectorAll('#trkLoot tbody tr')].map((tr) => ({
  name: tr.dataset.rarity,
  total: Number(tr.children[1].dataset.value),
  mf: Number(tr.children[2].dataset.value),
  per_hour: Number(tr.children[3].dataset.value),
}));

const readNotable = () => [...document.querySelectorAll('#trkNotable dd')].map((dd) => ({ label: dd.dataset.label, total: Number(dd.dataset.value) }));

const readJournal = () => [...document.querySelectorAll('#trkJournal .trk-entry')].map((li) => ({
  id: li.dataset.id,
  time: li.querySelector('.trk-time').textContent.trim(),
  rarity: li.querySelector('.trk-rarity').textContent.trim(),
  name: li.querySelector('.trk-item').textContent.trim(),
}));

// A ground drop as the sensor writes it, appended to the live session.
let nextEvent = 1;
function groundDrop(item) {
  return { kind: 'ground_drop', event_id: `fp-777-check-${nextEvent++}`, source: 'game_ground_item_create', room: 'Act_01_02', item };
}

export default async function (t) {
  // The switch on, as a player who wants the session counted has it.
  await setSetting(t, 'mod_tracker', true);

  await t.check('totals-match-the-api', async () => {
    const page = await openSession(t);
    const body = await settled(t, page);
    const snap = body.snapshot;
    assert(snap.gold.earned > 0 && snap.xp.earned > 0 && snap.kills.earned > 0, `the fixture earned nothing: ${JSON.stringify(snap.gold)}`);
    const shown = await until(page, readTotals, expectTotals(snap), 'the totals');
    // The lines are the API's own numbers: earned, per hour, and the lifetime total where there is one.
    for (const key of ['gold', 'xp', 'kills']) {
      const row = shown.find((r) => r.key === key);
      assert(row.earned === snap[key].earned, `${key} earned reads ${row.earned}, the API says ${snap[key].earned}`);
      assert(row.per_hour === snap[key].per_hour, `${key} per hour reads ${row.per_hour}, the API says ${snap[key].per_hour}`);
      assert(row.total === (snap[key].total == null ? '' : String(snap[key].total)), `${key} total reads ${row.total}`);
      assert(row.value === fmt(snap[key].earned), `${key} is written ${row.value}`);
    }
  });

  await t.check('clock-and-character-match-the-api', async () => {
    const page = await openSession(t);
    const body = await settled(t, page);
    const snap = body.snapshot;
    const shown = await page.evaluate(() => ({
      secs: Number(document.getElementById('trkClock').dataset.secs),
      clock: document.getElementById('trkClock').textContent.trim(),
      paused: document.getElementById('trkClock').classList.contains('paused'),
      name: document.getElementById('trkSessionTitle').textContent.trim(),
      pause: document.getElementById('trkPause')?.textContent.trim(),
    }));
    assert(shown.secs === snap.session_secs, `the clock holds ${shown.secs} s, the API says ${snap.session_secs}`);
    assert(shown.clock === clock(snap.session_secs), `the clock reads ${shown.clock}`);
    assert(shown.paused === snap.paused, `the clock shows paused=${shown.paused}, the API says ${snap.paused}`);
    assert(shown.name === snap.character.name, `the card is titled ${shown.name}, the API names ${snap.character.name}`);
    assert(shown.pause === (snap.paused ? 'Carry on' : 'Pause clock'), `the pause button reads ${shown.pause}`);
  });

  await t.check('zone-chip-names-the-zone-and-its-mods', async () => {
    const state = await t.api('/api/state');
    const page = await openSession(t);
    const body = await settled(t, page);
    const zone = body.snapshot.satanic_zone;
    assert(zone && zone.zone, `the fixture has no Satanic zone: ${JSON.stringify(zone)}`);
    assert(zone.buffs.length && zone.debuffs.length, `the fixture's zone has no buffs or no debuffs: ${JSON.stringify(zone)}`);
    const buffs = modRows(zone.buffs, state.satanicBuffs).map((m) => m.name);
    const debuffs = modRows(zone.debuffs, state.satanicDebuffs).map((m) => m.name);
    // Named from /api/state's lists, never by the number the record carries.
    for (const name of [...buffs, ...debuffs]) assert(!/^(Buff|Debuff) \d+$/.test(name), `${name} is not in /api/state's lists`);
    const read = () => ({
      code: document.getElementById('trkZoneName')?.dataset.zone,
      name: document.querySelector('#trkZoneName > span')?.textContent.trim(),
      here: !!document.querySelector('#trkZoneName .trk-here'),
      buffs: [...document.querySelectorAll('#trkZone .trk-mod.buff .trk-mod-name')].map((e) => e.textContent.trim()),
      debuffs: [...document.querySelectorAll('#trkZone .trk-mod.debuff .trk-mod-name')].map((e) => e.textContent.trim()),
    });
    await until(page, read, { code: zone.zone, name: zoneName(zone.zone), here: !!body.snapshot.satanic_here, buffs, debuffs }, 'the zone chip');
  });

  await t.check('series-draws-every-sample', async () => {
    const extra = await t.api('/api/tracker/extra');
    assert(extra.series.length > 0, 'the fixture has no series point');
    const page = await openSession(t);
    for (const key of ['gold', 'xp']) {
      const want = chartPoints(extra.series, key);
      const read = new Function(`return (() => {
        const fig = document.querySelector('.trk-chart[data-key="${key}"]');
        const line = fig?.querySelector('polyline');
        return { samples: Number(fig?.dataset.samples), points: line ? line.getAttribute('points').trim().split(/\\s+/).length : 0,
          latest: fig?.querySelector('figcaption b')?.textContent.trim() };
      })()`);
      await until(page, read, { samples: extra.series.length, points: want.points.length, latest: fmt(want.latest) }, `the ${key} chart`);
    }
  });

  await t.check('tallies-show-what-the-api-gives', async () => {
    const body = await t.api('/api/tracker');
    assert(Array.isArray(body.snapshot.tallies), 'the API has no tallies list');
    const page = await openSession(t);
    await settled(t, page);
    const readTallies = () => ({
      boss: [...document.querySelectorAll('#trkTallies [data-group="boss"] dd')].map((d) => [d.dataset.label, Number(d.dataset.value)]),
      chest: [...document.querySelectorAll('#trkTallies [data-group="chest"] dd')].map((d) => [d.dataset.label, Number(d.dataset.value)]),
      empty: !!document.querySelector('#trkTallies .trk-empty'),
    });
    const pick = (list, group) => list.filter((x) => x.group === group).map((x) => [x.label, x.total]);
    const asApi = (list) => ({ boss: pick(list, 'boss'), chest: pick(list, 'chest'), empty: list.length === 0 });
    await until(page, readTallies, asApi(body.snapshot.tallies), 'the tallies (the real answer)');
    // The same answer, with the tallies a save would have given.
    let served = null;
    const routed = await openSession(t, {
      routes: (p) => p.route('**/api/tracker', async (route) => {
        const res = await route.fetch();
        const json = await res.json();
        json.snapshot.tallies = [
          { label: 'Uber Phantom Leviathan', group: 'boss', total: 4 },
          { label: 'Crystal', group: 'chest', total: 23 },
          { label: 'Chaos Tower floors', group: 'boss', total: 18 },
        ];
        served = json.snapshot.tallies;
        await route.fulfill({ response: res, json });
      }),
    });
    const deadline = Date.now() + 5000;
    while (!served && Date.now() < deadline) await wait(100);
    assert(served, 'the routed page never asked for /api/tracker');
    await until(routed, readTallies, asApi(served), 'the tallies (a save that counted some)');
  });

  await t.check('loot-notable-and-grades-follow-new-drops', async () => {
    const before = (await t.api('/api/tracker')).snapshot;
    t.appendSession([
      groundDrop({ name: 'Fixture SS Blade', rarity: 'Satanic', item_type: 3, item_id: 201, weapon_type: 1, tier: 6, magic_find_drop: true, amount: 1 }),
      groundDrop({ name: 'Angelic Key', rarity: 'Angelic', item_type: 12, item_id: 7, amount: 1 }),
    ]);
    const page = await openSession(t);
    const snap = (await settled(t, page)).snapshot;
    // The control: the drops moved the API's own counts, so a page that only drew zeros would fail below.
    assert(snap.ss === before.ss + 1, `ss went from ${before.ss} to ${snap.ss}`);
    assert(snap.items.Satanic.total === before.items.Satanic.total + 1, `Satanic went from ${before.items.Satanic.total} to ${snap.items.Satanic.total}`);
    const key = snap.notable.find((n) => n.label === 'Angelic Key');
    assert(key && key.total > 0, `Angelic Key is not counted: ${JSON.stringify(snap.notable)}`);
    await until(page, readLoot, RARITIES.map((name) => ({ name, total: snap.items[name].total, mf: snap.items[name].mf, per_hour: snap.items[name].per_hour })), 'the loot table');
    await until(page, readNotable, notableRows(snap), 'the notable counts');
    await until(page, readTotals, expectTotals(snap), 'the totals after the drops');
  });

  await t.check('journal-is-newest-first-and-equals-extra', async () => {
    const page = await openSession(t);
    const rows = (drops) => journalRows(drops).map((r) => ({ id: r.id, time: r.time, rarity: r.rarity, name: r.name }));
    let extra = await t.api('/api/tracker/extra');
    assert(extra.drops.length > 0, 'the journal is empty');
    await until(page, readJournal, rows(extra.drops), 'the journal');
    // A new drop lands first, and the list is still /extra's.
    t.appendSession(groundDrop({ name: 'Fixture Unholy Ring', rarity: 'Unholy', item_type: 7, item_id: 301, amount: 1 }));
    // `/extra` reads what the last `/api/tracker` ingested; ask that first, as the page's poll does.
    await t.api('/api/tracker');
    extra = await t.api('/api/tracker/extra');
    const first = extra.drops[0];
    assert(first.name === 'Fixture Unholy Ring', `the newest entry in /extra is ${first.name}`);
    for (let i = 1; i < extra.drops.length; i++) {
      assert(extra.drops[i - 1].ts_ms >= extra.drops[i].ts_ms, '/extra is not newest first');
    }
    const shown = await until(page, readJournal, rows(extra.drops), 'the journal after a new drop');
    assert(shown[0].name === 'Fixture Unholy Ring', `the journal's first row is ${shown[0].name}`);
    const count = await page.locator('#trkJournalTitle + .trk-count').textContent();
    assert(count.trim() === `${extra.drops.length} drops`, `the journal's count reads ${count}`);
  });

  await t.check('nothing-a-poll-repaints-animates', async () => {
    const page = await openSession(t);
    await settled(t, page);
    const moving = await page.evaluate(() => {
      const els = document.querySelectorAll('#trkClock, .trk-total *, #trkLoot td, #trkNotable dd, #trkZoneName, .trk-chart *, #trkJournal .trk-entry, #trkJournal .trk-entry *');
      const out = [];
      for (const el of els) {
        const cs = getComputedStyle(el);
        const durations = `${cs.transitionDuration},${cs.animationDuration}`.split(',').map((s) => parseFloat(s) || 0);
        if (cs.animationName !== 'none' || durations.some((d) => d > 0)) out.push(`${el.tagName}.${el.className?.baseVal ?? el.className}`);
      }
      return { count: els.length, out };
    });
    assert(moving.count > 20, `only ${moving.count} elements were looked at`);
    assert(moving.out.length === 0, `these move: ${moving.out.join(', ')}`);
  });

  await t.check('pause-and-resume-post-pause', async () => {
    const page = await openSession(t);
    const start = (await settled(t, page)).snapshot;
    assert(!start.paused, 'the session starts paused');
    for (const on of [true, false]) {
      const [req] = await Promise.all([page.waitForRequest(isPost('/api/tracker/pause')), page.click('#trkPause')]);
      const posted = JSON.parse(req.postData());
      assert(posted.on === on, `pause posted ${req.postData()}`);
      const body = await t.api('/api/tracker');
      assert(body.snapshot.paused === on, `the API says paused=${body.snapshot.paused} after posting on=${on}`);
      const read = () => ({
        paused: document.getElementById('trkClock').classList.contains('paused'),
        button: document.getElementById('trkPause')?.textContent.trim(),
        note: document.getElementById('trkSessionNote')?.textContent.includes('Clock paused') || false,
      });
      await until(page, read, { paused: on, button: on ? 'Carry on' : 'Pause clock', note: on }, `the card after pause on=${on}`);
    }
  });

  await t.check('end-run-asks-first-then-posts-reset', async () => {
    const page = await openSession(t);
    const before = (await settled(t, page)).snapshot;
    assert(before.gold.earned > 0, 'nothing to end: the run earned no gold');
    const resets = [];
    page.on('request', (r) => { if (isPost('/api/tracker/reset')(r)) resets.push(r); });
    // Asked first: nothing is posted until the run is confirmed.
    await page.click('#trkEndRun');
    await page.waitForSelector('#trkEndConfirm', { state: 'visible' });
    const focused = await page.evaluate(() => document.activeElement?.id);
    assert(focused === 'trkEndNo', `the confirmation focused ${focused}, not Keep going`);
    await wait(700);
    assert(resets.length === 0, 'End run posted before it was confirmed');
    await page.click('#trkEndNo');
    await page.waitForSelector('#trkEndConfirm', { state: 'detached' });
    await wait(500);
    assert(resets.length === 0, 'Keep going posted a reset');
    assert((await t.api('/api/tracker')).snapshot.gold.earned === before.gold.earned, 'Keep going ended the run');
    // Confirmed: one reset, and the page shows the API's fresh counters.
    await page.click('#trkEndRun');
    await Promise.all([page.waitForRequest(isPost('/api/tracker/reset')), page.click('#trkEndYes')]);
    await page.waitForSelector('#trkEndConfirm', { state: 'detached' });
    assert(resets.length === 1, `${resets.length} resets posted`);
    const after = (await settled(t, page)).snapshot;
    assert(after.gold.earned === 0, `the API still has ${after.gold.earned} gold earned after End run`);
    await until(page, readTotals, expectTotals(after), 'the totals after End run');
  });

  await t.check('catching-up-is-shown', async () => {
    const page = await openSession(t);
    await settled(t, page);
    const answers = [];
    page.on('response', async (res) => {
      if (new URL(res.url()).pathname !== '/api/tracker') return;
      try { answers.push(!!(await res.json()).catching_up); } catch {}
    });
    assert(!await page.locator('#trkSessionNote').isVisible() || !(await page.locator('#trkSessionNote').textContent()).includes('Catching up'),
      'the card says it is catching up before anything was appended');
    // Several read budgets of records (256 KiB each) at the session's last
    // moment: the panel reads them over several polls, catching up meanwhile.
    const lines = Array.from({ length: 16000 }, () => ({ kind: 'heartbeat', emitted_at_unix_ms: 1700000090000 }));
    t.appendSession(lines);
    await page.waitForFunction(() => (document.getElementById('trkSessionNote')?.textContent || '').includes('Catching up'), null, { timeout: 15000 });
    // The page draws an answer before the listener has read its body back, so give the listener a moment.
    const heard = async (ok) => {
      const end = Date.now() + 3000;
      while (!ok() && Date.now() < end) await wait(100);
      return ok();
    };
    assert(await heard(() => answers.includes(true)), `the page showed catching up, but no answer it got said so: ${JSON.stringify(answers)}`);
    // And it stops saying so once the API does.
    await page.waitForFunction(() => !(document.getElementById('trkSessionNote')?.textContent || '').includes('Catching up'), null, { timeout: 30000 });
    assert(await heard(() => answers.at(-1) === false), `the note went while the API still said catching_up: ${JSON.stringify(answers)}`);
  });

  await t.check('empty-state-with-no-session', async () => {
    const empty = await t.startSandbox({ trackerSession: null });
    await setSetting(t, 'mod_tracker', true, empty);
    const body = await t.api('/api/tracker', {}, empty);
    assert(!body.session?.file && !body.snapshot?.character, `the empty sandbox has a session: ${JSON.stringify(body.session)}`);
    const page = await openSession(t, { sandbox: empty });
    await page.waitForFunction(() => document.querySelector('[data-section="session"]')?.dataset.state === 'none', null, { timeout: 5000 });
    const shown = await page.evaluate(() => ({
      title: document.getElementById('trkNoSessionTitle')?.textContent.trim(),
      buttons: document.querySelectorAll('#trkPause, #trkEndRun').length,
      journal: !!document.getElementById('trkJournal'),
      totals: document.querySelectorAll('.trk-total').length,
    }));
    assert(shown.title === 'No session yet', `the empty tab is titled ${shown.title}`);
    assert(shown.buttons === 0 && !shown.journal && shown.totals === 0, `the empty tab shows controls or numbers: ${JSON.stringify(shown)}`);
    // The control: the fixture's sandbox, the same page, is not empty.
    const full = await openSession(t);
    assert(await full.evaluate(() => document.querySelector('[data-section="session"]').dataset.state) === 'session',
      'the sandbox with a session shows the empty state too');
  });
}
