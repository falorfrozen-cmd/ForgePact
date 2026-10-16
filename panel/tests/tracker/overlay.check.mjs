// The Tracker's overlay window (#185): the page pywebview opens on top of the
// game (`/overlay.html`, src/overlay/), and its settings in the Tracker tab
// (`OverlaySettings.svelte`). Run by tests/tracker.e2e.mjs
// (`npm run e2e:tracker -- --only overlay`).
//
// The overlay page is opened in a plain browser tab here: the window itself
// (frameless, on top, no focus, its place saved) is src/tracker_overlay.py's,
// tested with a fake pywebview in tests/test_tracker_overlay.py. Every
// expected value is read from the API in the check. The switch stays off
// throughout: the overlay page polls on its own, and with the switch off and
// the Tracker tab closed the main page claims nothing, so a claim or a play
// seen here can only be the overlay's.

import { fmt, rate, RARITIES } from '../../src/tracker/format.js';

export const EXPECTED = [
  'overlay-page-is-served',
  'counters-equal-the-api',
  'drop-row-keeps-every-count-whole',
  'ticker-shows-the-newest-journal-entries',
  'appended-flourish-drop-raises-the-card',
  'drop-card-rises-in-and-fades-out',
  'overlay-never-claims-or-plays',
  'settings-write-their-fields',
  'open-button-disabled-with-the-reason-while-unavailable',
  'overlay-endpoint-answers-409-in-the-sandbox',
];

const wait = (ms) => new Promise((r) => setTimeout(r, ms));

function assert(ok, message) {
  if (!ok) throw new Error(message);
}

// Poll `read` until it answers what `want()` answers now (both re-read each
// round), or fail naming both.
async function until(read, want, what, timeout = 10000) {
  const end = Date.now() + timeout;
  let got;
  let expected;
  for (;;) {
    got = await read();
    expected = await want();
    if (JSON.stringify(got) === JSON.stringify(expected)) return got;
    if (Date.now() > end) throw new Error(`${what}: the page shows ${JSON.stringify(got)}, expected ${JSON.stringify(expected)}`);
    await wait(150);
  }
}

let nextEvent = 1;
function groundDrop(item) {
  return { kind: 'ground_drop', event_id: `fp-777-overlay-${nextEvent++}`, source: 'game_ground_item_create', room: 'Act_01_02', item };
}

// Counts every way a page could make a sound: a Web Audio buffer started, a
// media element played, an AudioContext made. No hook ships in the product.
function COUNT_PLAYS() {
  window.__plays = [];
  const start = AudioBufferSourceNode.prototype.start;
  AudioBufferSourceNode.prototype.start = function (...args) {
    window.__plays.push('buffer');
    return start.apply(this, args);
  };
  const play = HTMLMediaElement.prototype.play;
  HTMLMediaElement.prototype.play = function (...args) {
    window.__plays.push('media');
    return play.apply(this, args);
  };
  const Real = window.AudioContext;
  window.AudioContext = class extends Real {
    constructor(...args) {
      super(...args);
      window.__plays.push('context');
    }
  };
}

// The overlay page in a fresh context, with the play counter in when asked.
async function openOverlay(t, { countPlays = false, routes } = {}) {
  const page = await t.openPanel({ routes });
  if (countPlays) await page.addInitScript(COUNT_PLAYS);
  const res = await page.goto(new URL('/overlay.html', t.sandbox.url).href);
  assert(res && res.ok(), `/overlay.html answered ${res && res.status()}`);
  await page.waitForFunction(() => document.getElementById('ovl')?.dataset.polled === '1', null, { timeout: 15000 });
  return page;
}

// The overlay has drawn the snapshot the API answers now, and read the journal.
async function settled(t, page) {
  return until(async () => page.evaluate(() => {
    const root = document.getElementById('ovl');
    return { revision: root?.dataset.revision ?? '', journal: root?.dataset.journal !== '' };
  }), async () => ({ revision: String((await t.api('/api/tracker')).snapshot.revision), journal: true }), 'the overlay\'s snapshot');
}

async function writeSettings(t, change) {
  const read = await t.api('/api/tracker/settings');
  const res = await t.api('/api/tracker/settings', { body: { ...read.settings, ...change } });
  assert(!res.err, `the settings write was refused: ${res.err}`);
  return res.settings;
}

// The drop the API's journal holds under this name, once it is there. Only a
// `GET /api/tracker` reads the session file, so each round asks it first.
async function journalEntry(t, name) {
  const end = Date.now() + 10000;
  for (;;) {
    await t.api('/api/tracker');
    const extra = await t.api('/api/tracker/extra');
    const found = (extra.drops || []).find((d) => d.name === name);
    if (found) return found;
    if (Date.now() > end) throw new Error(`the journal never held ${name}: ${JSON.stringify((extra.drops || []).map((d) => d.name))}`);
    await wait(200);
  }
}

const cardName = (page) => page.evaluate(() => document.querySelector('#ovlFlourish .ovl-flourish-name')?.textContent.trim() ?? null);

// The motion recorder motion.e2e.mjs uses: every `transitionrun`, with the
// CSSTransition's own duration and easing, so no check races a 120 ms fade.
async function record(page) {
  await page.evaluate(() => {
    window.__motion = [];
    document.addEventListener('transitionrun', (e) => {
      const a = e.target.getAnimations().find((x) => x.transitionProperty === e.propertyName);
      const timing = a?.effect?.getTiming();
      window.__motion.push({ el: e.target, prop: e.propertyName, state: e.target.dataset.state ?? '', duration: timing ? Number(timing.duration) : null, easing: timing ? String(timing.easing) : null });
    }, true);
  });
}
const seenOn = (page, selector) => page.evaluate((s) => window.__motion.filter((m) => m.el.matches(s))
  .map(({ prop, state, duration, easing }) => ({ prop, state, duration, easing })), selector);
// The motion tokens as the page computes them.
const motionTokens = (page) => page.evaluate(() => {
  const cs = getComputedStyle(document.documentElement);
  const v = (n) => cs.getPropertyValue(n).trim();
  const ms = (n) => { const s = v(n); return /ms$/.test(s) ? parseFloat(s) : parseFloat(s) * 1000; };
  return { fast: ms('--motion-duration-fast'), base: ms('--motion-duration-base'), standard: v('--motion-easing-standard'), emphasized: v('--motion-easing-emphasized') };
});
// The page writes a token's curve back as `.23`, a timing as `0.23`: compare the numbers.
const curve = (s) => (String(s).match(/-?\d*\.?\d+/g) || []).map(Number);
const sameCurve = (a, b) => /^cubic-bezier\(/.test(String(a)) && curve(a).length === 4 && curve(a).every((n, i) => Math.abs(n - curve(b)[i]) < 1e-6);
function timed(m, duration, easing, what) {
  assert(m, `${what}: no transition`);
  assert(m.duration === duration, `${what}: ${m.prop} runs ${m.duration} ms, not the token's ${duration}`);
  assert(sameCurve(m.easing, easing), `${what}: ${m.prop} eases ${m.easing}, not the token's ${easing}`);
}

// Each drop cell's box against the row's and the plate's, and whether any
// count is cut inside its cell.
const dropBoxes = (page) => page.evaluate(() => {
  const plate = document.getElementById('ovl');
  const row = document.querySelector('.ovl-drops');
  const inner = plate.getBoundingClientRect();
  const pad = parseFloat(getComputedStyle(plate).paddingRight);
  const box = row.getBoundingClientRect();
  return {
    plateRight: inner.right - pad,
    rowLeft: box.left,
    rowRight: box.right,
    cells: [...row.querySelectorAll('.ovl-drop')].map((cell) => {
      const r = cell.getBoundingClientRect();
      const value = cell.querySelector('strong');
      return { key: cell.dataset.rarity, text: cell.textContent.trim(), left: r.left, right: r.right, top: r.top, cut: cell.scrollWidth > cell.clientWidth + 0.5 || value.scrollWidth > value.getBoundingClientRect().width + 0.5 };
    }),
  };
});

async function openOverlaySettings(t, options) {
  const page = await t.openPanel(options);
  await page.click('.tabbtn[data-tab="tracker"]');
  await page.click('#trk-tab-overlay');
  await page.waitForSelector('#trkOverlayCard', { timeout: 10000 });
  // the section has the document once its controls carry the saved values
  await page.waitForFunction(() => document.getElementById('trkOverlaySettings')?.dataset.loaded === '1', null, { timeout: 10000 });
  return page;
}

export default async function (t) {
  await t.check('overlay-page-is-served', async () => {
    const res = await fetch(new URL('/overlay.html', t.sandbox.url));
    const html = await res.text();
    assert(res.status === 200, `GET /overlay.html answered ${res.status}`);
    assert(/text\/html/.test(res.headers.get('content-type') || ''), `/overlay.html is served as ${res.headers.get('content-type')}`);
    assert(html.includes('id="overlay"'), 'the page has no #overlay mount');
    const page = await openOverlay(t);
    assert(await page.locator('#ovl').count() === 1, 'the page drew no overlay plate');
    assert(await page.title() === 'ForgePact Tracker', `the page is titled ${await page.title()}`);
  });

  await t.check('counters-equal-the-api', async () => {
    const page = await openOverlay(t);
    await settled(t, page);
    const read = () => page.evaluate(() => ({
      metrics: [...document.querySelectorAll('.ovl-metric')].map((m) => ({ key: m.dataset.key, value: m.querySelector('strong').textContent.trim(), rate: m.querySelector('em').textContent.trim() })),
      drops: [...document.querySelectorAll('.ovl-drop')].map((d) => ({ key: d.dataset.rarity, value: d.querySelector('strong').textContent.trim() })),
    }));
    const want = async () => {
      const snap = (await t.api('/api/tracker')).snapshot;
      return {
        metrics: ['gold', 'xp', 'kills'].map((key) => ({ key, value: fmt(snap[key].earned), rate: rate(snap[key].per_hour) })),
        drops: [...RARITIES.map((key) => ({ key, value: fmt(snap.items?.[key]?.total ?? 0) })), { key: 'SS', value: fmt(snap.ss ?? 0) }],
      };
    };
    const shown = await until(read, want, 'the counters');
    const snap = (await t.api('/api/tracker')).snapshot;
    assert(snap.gold.earned > 0 && snap.kills.earned > 0, `the fixture earned nothing: ${JSON.stringify(snap.gold)}`);
    assert(shown.drops.find((d) => d.key === 'Heroic').value !== '0', 'the fixture\'s Heroic is not counted');
    const zone = snap.satanic_zone?.zone;
    assert(zone, 'the fixture has no Satanic zone');
    const top = await page.locator('.ovl-zone').textContent();
    assert(/Act 1/.test(top), `the top line does not name the fixture's zone: ${top}`);
  });

  await t.check('drop-row-keeps-every-count-whole', async () => {
    // The control: the fixture's own counts sit on one line, as the plate's
    // 147 px assumes.
    let page = await openOverlay(t);
    await settled(t, page);
    const small = await dropBoxes(page);
    assert(small.cells.length === 6, `the row has ${small.cells.length} cells`);
    assert(new Set(small.cells.map((c) => Math.round(c.top))).size === 1, `the fixture's counts do not sit on one line: ${JSON.stringify(small.cells)}`);
    await page.context().close();
    // Six of the widest counts fmt writes below a million ("123.5k"), through the
    // page's own poll: every count shows whole, inside the plate.
    const big = 123456;
    page = await openOverlay(t, {
      routes: async (p) => {
        await p.route('**/api/tracker', async (route) => {
          const res = await route.fetch();
          const body = await res.json();
          for (const name of RARITIES) body.snapshot.items = { ...body.snapshot.items, [name]: { ...(body.snapshot.items?.[name] ?? {}), total: big } };
          body.snapshot.ss = big;
          await route.fulfill({ response: res, json: body });
        });
      },
    });
    await until(() => page.evaluate(() => [...document.querySelectorAll('.ovl-drop strong')].map((s) => s.textContent.trim())),
      () => Array(6).fill(fmt(big)), 'the large counts');
    const wide = await dropBoxes(page);
    for (const c of wide.cells) {
      assert(!c.cut, `${c.key}'s count is cut: ${c.text}`);
      assert(c.left >= wide.rowLeft - 0.5 && c.right <= wide.rowRight + 0.5, `${c.key} (${c.text}) runs out of its row: ${c.left}-${c.right} in ${wide.rowLeft}-${wide.rowRight}`);
      assert(c.right <= wide.plateRight + 0.5, `${c.key} (${c.text}) runs past the plate's edge: ${c.right} > ${wide.plateRight}`);
    }
  });

  await t.check('ticker-shows-the-newest-journal-entries', async () => {
    // Off by default, as in the Tracker: the control first.
    assert((await t.api('/api/tracker/settings')).settings.ticker === false, 'the ticker is not off by default');
    let page = await openOverlay(t);
    await settled(t, page);
    assert(await page.locator('#ovlTicker').count() === 0, 'the ticker shows while it is off');
    await page.context().close();
    // Four journal entries, so the strip has to pick.
    for (const [name, rarity] of [['Ticker One', 'Set'], ['Ticker Two', 'Angelic'], ['Ticker Three', 'Unholy']]) {
      t.appendSession([groundDrop({ name, rarity, item_type: 0, item_id: 900 + nextEvent, weapon_type: 1, amount: 1 })]);
    }
    await journalEntry(t, 'Ticker Three');
    await writeSettings(t, { ticker: true });
    try {
      page = await openOverlay(t);
      await settled(t, page);
      const shown = await until(
        () => page.evaluate(() => [...document.querySelectorAll('#ovlTicker li .trk-item')].map((el) => el.textContent.trim())),
        async () => (await t.api('/api/tracker/extra')).drops.slice(0, 3).map((d) => d.name),
        'the ticker');
      const all = (await t.api('/api/tracker/extra')).drops;
      assert(all.length > 3 && shown.length === 3, `the journal holds ${all.length} entries and the ticker shows ${shown.length}`);
      assert(shown[0] === 'Ticker Three', `the newest entry is not first: ${shown}`);
    } finally {
      await writeSettings(t, { ticker: false });
    }
  });

  await t.check('appended-flourish-drop-raises-the-card', async () => {
    const page = await openOverlay(t);
    await settled(t, page);
    assert(await page.locator('#ovlFlourish').count() === 0, 'a card shows before anything dropped');
    // The card is announced through a status line that is there before it.
    const announce = page.locator('#ovlAnnounce');
    assert(await announce.count() === 1 && await announce.getAttribute('role') === 'status', 'no status line waits for the card');
    assert((await announce.textContent()).trim() === '', `the status line says ${await announce.textContent()} before any card`);
    const name = 'Overlay Check Covenant';
    t.appendSession([groundDrop({ name, rarity: 'Angelic', item_type: 0, item_id: 1200 + nextEvent, weapon_type: 1, amount: 1 })]);
    const entry = await journalEntry(t, name);
    assert(entry.flourish === true, `the journal entry does not ask for a card: ${JSON.stringify(entry)}`);
    await until(() => cardName(page), () => name, 'the drop card', 8000);
    const card = await page.evaluate(() => ({
      meta: document.querySelector('#ovlFlourish .ovl-flourish-meta')?.textContent.trim(),
      cls: document.getElementById('ovlFlourish').className,
      said: document.getElementById('ovlAnnounce')?.textContent.trim() ?? '',
    }));
    assert(card.meta.startsWith('Angelic'), `the card's line reads ${card.meta}`);
    assert(/\br-angelic\b/.test(card.cls), `the card is not in Angelic's colour: ${card.cls}`);
    assert(card.said.startsWith(`${name}, Angelic`), `the status line says ${card.said}`);
    // It leaves after the drop card's time (4 s by default).
    const secs = (await t.api('/api/tracker/settings')).settings.flourish_secs;
    await until(() => cardName(page), () => null, 'the card leaving', secs * 1000 + 3000);
  });

  await t.check('drop-card-rises-in-and-fades-out', async () => {
    // DESIGN.md § Tracker: in over base on the emphasized easing (opacity and
    // an 8px rise), out over fast on the standard easing (opacity only), and
    // under reduced motion a fade with no rise.
    const secs = (await t.api('/api/tracker/settings')).settings.flourish_secs;
    let page = await openOverlay(t);
    await settled(t, page);
    await record(page);
    const tok = await motionTokens(page);
    let name = 'Overlay Motion Covenant';
    t.appendSession([groundDrop({ name, rarity: 'Angelic', item_type: 0, item_id: 1300 + nextEvent, weapon_type: 1, amount: 1 })]);
    await until(() => cardName(page), () => name, 'the drop card', 8000);
    await until(() => cardName(page), () => null, 'the card leaving', secs * 1000 + 3000);
    let seen = await seenOn(page, '.ovl-flourish');
    const enter = seen.filter((m) => m.state === 'enter');
    const leave = seen.filter((m) => m.state === 'leave');
    timed(enter.find((m) => m.prop === 'opacity'), tok.base, tok.emphasized, 'the card entering');
    timed(enter.find((m) => m.prop === 'transform'), tok.base, tok.emphasized, 'the card rising');
    timed(leave.find((m) => m.prop === 'opacity'), tok.fast, tok.standard, 'the card leaving');
    assert(!leave.some((m) => m.prop === 'transform'), `the card moves as it leaves: ${JSON.stringify(leave)}`);
    await page.context().close();

    // Reduced motion, proven on before it is relied on: a fade, no rise.
    page = await t.openPanel();
    const before = await page.evaluate(() => matchMedia('(prefers-reduced-motion: reduce)').matches);
    await page.emulateMedia({ reducedMotion: 'reduce' });
    const after = await page.evaluate(() => matchMedia('(prefers-reduced-motion: reduce)').matches);
    assert(before === false && after === true, `reduced-motion emulation did not take: before ${before}, after ${after}`);
    const res = await page.goto(new URL('/overlay.html', t.sandbox.url).href);
    assert(res && res.ok(), `/overlay.html answered ${res && res.status()}`);
    await page.waitForFunction(() => document.getElementById('ovl')?.dataset.polled === '1', null, { timeout: 15000 });
    await settled(t, page);
    await record(page);
    name = 'Overlay Still Covenant';
    t.appendSession([groundDrop({ name, rarity: 'Angelic', item_type: 0, item_id: 1400 + nextEvent, weapon_type: 1, amount: 1 })]);
    await until(() => cardName(page), () => name, 'the drop card under reduced motion', 8000);
    await until(() => cardName(page), () => null, 'the card leaving under reduced motion', secs * 1000 + 3000);
    seen = await seenOn(page, '.ovl-flourish');
    assert(seen.some((m) => m.state === 'enter' && m.prop === 'opacity'), `the card does not fade in under reduced motion: ${JSON.stringify(seen)}`);
    assert(seen.some((m) => m.state === 'leave' && m.prop === 'opacity'), `the card does not fade out under reduced motion: ${JSON.stringify(seen)}`);
    assert(!seen.some((m) => m.prop !== 'opacity'), `the card moves under reduced motion: ${JSON.stringify(seen)}`);
  });

  await t.check('overlay-never-claims-or-plays', async () => {
    const page = await openOverlay(t, { countPlays: true });
    const claims = [];
    page.on('request', (req) => { if (new URL(req.url()).pathname === '/api/tracker/announce') claims.push(req.method()); });
    await settled(t, page);
    const name = 'Overlay Silent Blade';
    t.appendSession([groundDrop({ name, rarity: 'Heroic', item_type: 0, item_id: 1500 + nextEvent, weapon_type: 1, amount: 1 })]);
    const entry = await journalEntry(t, name);
    assert(entry.announce === true && entry.flourish === true, `the drop is not one an alert and a card are for: ${JSON.stringify(entry)}`);
    // The page saw the drop (the card is its positive control), and then had time to claim it.
    await until(() => cardName(page), () => name, 'the drop card');
    await wait(2500);
    assert(claims.length === 0, `the overlay sent ${claims.length} claims`);
    assert((await page.evaluate(() => window.__plays.length)) === 0, `the overlay played: ${await page.evaluate(() => window.__plays.join(','))}`);
    assert((await journalEntry(t, name)).announced === false, 'the entry was claimed');
    // The instruments, through the same page: a claim and a play are seen.
    await page.evaluate(() => fetch('/api/tracker/announce', { method: 'POST', headers: { 'Content-Type': 'application/json' }, body: '{"id":"zone:0"}' }));
    await page.evaluate(() => { const c = new AudioContext(); const s = c.createBufferSource(); s.buffer = c.createBuffer(1, 1, 22050); s.start(); });
    assert(claims.length === 1, `the claim counter missed a claim: ${claims.length}`);
    assert((await page.evaluate(() => window.__plays.length)) === 2, `the play counter reads ${await page.evaluate(() => window.__plays.join(','))}`);
  });

  await t.check('settings-write-their-fields', async () => {
    await writeSettings(t, { zz_overlay_check: 'kept' });
    const page = await openOverlaySettings(t);
    const settings = async () => (await t.api('/api/tracker/settings')).settings;
    const saved = async (key, value, what) => until(async () => (await settings())[key], () => value, what);
    const before = await settings();

    for (const [id, key] of [['#trkOvlAutoShow', 'auto_show'], ['#trkOvlLocked', 'locked'], ['#trkOvlTicker', 'ticker'],
      ['#trkOvlCompact', 'compact'], ['#trkFlourishListed', 'flourish_listed'], ['#trkFlourishZone', 'flourish_zone'], ['#trkFlourishOn', 'flourish']]) {
      const box = page.locator(id);
      assert(await box.isChecked() === !!before[key], `${id} shows ${await box.isChecked()}, the document says ${before[key]}`);
      await box.click();
      await saved(key, !before[key], `${key} after ${id}`);
      await box.click();
      await saved(key, !!before[key], `${key} back after ${id}`);
    }

    // A rarity's card switch edits the list, keeping the others in order.
    const rarities = before.flourish_rarities;
    assert(rarities.includes('Set'), `the default rarities lack Set: ${rarities}`);
    await page.locator('#trkFlourish-Set').click();
    await saved('flourish_rarities', rarities.filter((r) => r !== 'Set'), 'flourish_rarities without Set');
    await page.locator('#trkFlourish-Set').click();
    await until(async () => [...(await settings()).flourish_rarities].sort(), () => [...rarities].sort(), 'flourish_rarities with Set again');

    await page.locator('#trkFlourishTier').selectOption('5');
    await saved('flourish_tier', 5, 'flourish_tier');
    await page.locator('#trkFlourishSecs').fill('7');
    await saved('flourish_secs', 7, 'flourish_secs');
    await page.locator('#trkOvlScale').fill('125');
    await saved('scale', 1.25, 'scale');
    await page.locator('#trkOvlScaleMore').click();
    await saved('scale', 1.3, 'scale after the stepper');

    // A solid window has no see-through plate: opacity is not offered, and the note says why.
    const overlay = (await t.api('/api/tracker')).overlay;
    assert(overlay.transparent === false, `the sandbox reports a transparent overlay: ${JSON.stringify(overlay)}`);
    assert(await page.locator('#trkOvlOpacity').isDisabled(), 'opacity is offered on a solid window');
    const note = await page.locator('#trkOvlSolidNote').textContent();
    assert(/solid/i.test(note), `the solid note reads ${note}`);

    const after = await settings();
    assert(after.zz_overlay_check === 'kept', 'a field the page does not know was lost');
    // put back what this check changed
    await writeSettings(t, { flourish_tier: before.flourish_tier, flourish_secs: before.flourish_secs, scale: before.scale });
  });

  await t.check('open-button-disabled-with-the-reason-while-unavailable', async () => {
    let page = await openOverlaySettings(t);
    const button = page.locator('#trkOverlayOpen');
    assert((await t.api('/api/tracker')).overlay.available === false, 'the sandbox offers an overlay window');
    await until(() => button.isDisabled(), () => true, 'the Open button disabled');
    const reason = await page.locator('#trkOverlayReason').textContent();
    assert(/ForgePact's own window/.test(reason), `the reason reads ${reason}`);
    assert((await button.getAttribute('aria-describedby') || '').split(' ').includes('trkOverlayReason'), 'the button does not point at its reason');
    await page.context().close();

    // The control: the same section beside a panel that has its own window
    // (the API's answer changed in flight) offers the button and posts the open.
    const posts = [];
    page = await openOverlaySettings(t, {
      routes: async (p) => {
        await p.route('**/api/tracker', async (route) => {
          const res = await route.fetch();
          const body = await res.json();
          body.overlay = { ...body.overlay, available: true, open: posts.length ? posts.at(-1).on : false };
          await route.fulfill({ response: res, json: body });
        });
        await p.route('**/api/tracker/overlay', async (route) => {
          posts.push(JSON.parse(route.request().postData()));
          await route.fulfill({ json: { ok: true, open: posts.at(-1).on } });
        });
      },
    });
    const open = page.locator('#trkOverlayOpen');
    await until(() => open.isDisabled(), () => false, 'the Open button beside a window');
    assert(await page.locator('#trkOverlayReason').count() === 0, 'the reason shows while the overlay is available');
    await open.click();
    await until(() => open.textContent().then((s) => s.trim()), () => 'Close overlay', 'the button after opening');
    assert(JSON.stringify(posts) === JSON.stringify([{ on: true }]), `the button posted ${JSON.stringify(posts)}`);
  });

  await t.check('overlay-endpoint-answers-409-in-the-sandbox', async () => {
    const body = await t.api('/api/tracker/overlay', { body: { on: true } });
    assert(t.lastStatus === 409, `POST /api/tracker/overlay answered ${t.lastStatus}`);
    assert(typeof body.err === 'string' && body.err, `the refusal says nothing: ${JSON.stringify(body)}`);
    assert((await t.api('/api/tracker')).overlay.open === false, 'the overlay reads open after a refusal');
  });
}
