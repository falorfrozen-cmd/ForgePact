// The "Enabled mods" list's two forms and the remembered value on a
// switched-off slider, in a real (headless) Edge against the sandbox server.
//
//   node tests/enabled-mods-form.e2e.mjs [--dist <dir>]      (npm run e2e:form)
//
// The form rule is the design's (`amendments.enabledMods.threshold` in
// design/figma-export.json): the inline row while the entries fit on one
// line, the tray once they overfill it, back to the row only past the
// hysteresis band. src/lib/enabled-mods-form.js decides; this drives it
// through the window's width and the saved config, and reads the answer only
// from the DOM (`data-form`, ids, classes, aria-*, visibility) and from
// sandbox.state() - never from a computed style.
//
// Setup goes through the sandbox's own /api/set and a reload, the way a saved
// config reaches the page; the checks themselves use the page's controls.

import { readFileSync } from 'node:fs';
import { launchBrowser, openPanel, parseArgs, startSandbox, waitBooted, waitSaved } from './lib/browser.mjs';
import { BOOLEAN_MODS } from '../src/enabled-mods.js';

const args = parseArgs(process.argv.slice(2));
const EXPORT = JSON.parse(readFileSync(new URL('../design/figma-export.json', import.meta.url), 'utf8'));
const HYSTERESIS = EXPORT.amendments.enabledMods.threshold.hysteresisPx;
const HEIGHT = 800;
const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const EXPECTED = [
  'form-inline-few-1280',
  'form-tray-many-1280',
  'form-tray-narrow',
  'form-hysteresis-no-flicker',
  'turn-off-inline',
  'turn-off-tray',
  'remembered-value-when-off',
  'remembered-value-cleared-when-on',
];

// Three ordinary entries, the most the features' own e2e ever shows at 1280.
const THREE = ['map_reveal', 'headhunter', 'mod_orb_pickup_radius'];

function assert(ok, message) { if (!ok) throw new Error(message); }
const $ = (page, fn, arg) => page.evaluate(fn, arg);

const post = (page, body) => $(page, (b) => fetch('/api/set', { method: 'POST', body: JSON.stringify(b) }).then((r) => r.json()), body);

// Every boolean mod off but `on`, then a reload: the list renders from the
// saved config in boot().
async function only(page, on) {
  for (const key of BOOLEAN_MODS) await post(page, { key, value: on.includes(key) });
  await page.reload();
  await waitBooted(page);
  await frames(page);
}

// Two animation frames: the form module's ResizeObserver has run and the
// attribute it set has been painted.
const frames = (page) => $(page, () => new Promise((r) => requestAnimationFrame(() => requestAnimationFrame(r))));
const formOf = (page) => $(page, () => document.getElementById('enabledMods').dataset.form);
const listed = (page) => $(page, () => [...document.querySelectorAll('#enabledMods li.enabled-mod')].map((li) => li.dataset.for));

async function resize(page, width) {
  await page.setViewportSize({ width, height: HEIGHT });
  await frames(page);
}

// POSTs to /api/set, in order.
function capturePosts(page) {
  const posts = [];
  page.on('request', (r) => { if (r.method() === 'POST' && r.url().endsWith('/api/set')) posts.push(r.postData()); });
  return posts;
}

async function settled(page) {
  await wait(30);
  await waitSaved(page);
  await wait(30);
}

async function forms(ctx) {
  const { page, sandbox, passed } = ctx;
  const read = async () => (await sandbox.state()).cfg;
  const posts = capturePosts(page);

  await only(page, THREE);
  assert(await formOf(page) === 'inline', 'Three entries at 1280 are not inline');
  assert(await $(page, () => [...document.querySelectorAll('#enabledMods li.enabled-mod')].filter((li) => li.checkVisibility()).length) === 3,
    'Three entries are not all visible');
  passed.push('form-inline-few-1280');

  await only(page, BOOLEAN_MODS);
  assert((await listed(page)).length >= 12, 'Fewer than twelve entries on');
  assert(await formOf(page) === 'tray', 'Twelve entries at 1280 are not in the tray');
  passed.push('form-tray-many-1280');

  await only(page, THREE);
  assert(await formOf(page) === 'inline', 'Back to three: not inline at 1280');
  let W = 0;
  for (let width = 1280; width >= 360; width -= 10) {
    await resize(page, width);
    if (await formOf(page) === 'tray') { W = width; break; }
  }
  assert(W > 0, 'Narrowing to 360px never moved three entries into the tray');
  passed.push('form-tray-narrow');

  await $(page, () => {
    window.__formChanges = 0;
    new MutationObserver((records) => { window.__formChanges += records.length; })
      .observe(document.getElementById('enabledMods'), { attributes: true, attributeFilter: ['data-form'] });
  });
  const half = W + Math.floor(HYSTERESIS / 2);
  await resize(page, half);
  await wait(50);
  assert(await formOf(page) === 'tray' && await $(page, () => window.__formChanges) === 0,
    `Inside the band (${W} -> ${half}) the form changed`);
  let back = 0;
  for (let width = half + 10; width <= W + HYSTERESIS + 200; width += 10) {
    await resize(page, width);
    if (await formOf(page) === 'inline') { back = width; break; }
  }
  assert(back > half, `Widening from ${W} never returned inline above ${half} (turned at ${back})`);
  passed.push('form-hysteresis-no-flicker');
  ctx.note = `tray at ${W}px, inline again at ${back}px, band ${HYSTERESIS}px`;

  await resize(page, 1280);
  await only(page, THREE);
  assert(await formOf(page) === 'inline', 'Not inline before the inline Turn off');
  posts.length = 0;
  await page.click('#enabledMods .quick-disable[data-for="headhunter"]');
  await settled(page);
  await page.waitForFunction(() => !document.querySelector('#enabledMods li.enabled-mod[data-for="headhunter"]'));
  assert(!(await read()).headhunter, 'Inline Turn off left the mod on');
  assert(posts.length === 1, `Inline Turn off sent ${posts.length} POSTs`);
  passed.push('turn-off-inline');

  await page.mouse.move(1, HEIGHT / 2);
  await only(page, BOOLEAN_MODS);
  assert(await formOf(page) === 'tray', 'Not in the tray before the tray Turn off');
  if (await $(page, () => document.querySelector('.enabled-mods-toggle')?.getAttribute('aria-expanded')) === 'false') {
    await page.click('.enabled-mods-toggle');
  }
  assert(await $(page, () => document.querySelectorAll('.quick-disable[data-for="headhunter"]').length) === 1,
    'The tray does not hold exactly one Turn off for the entry');
  posts.length = 0;
  await page.click('#enabledMods .quick-disable[data-for="headhunter"]');
  await settled(page);
  await page.waitForFunction(() => !document.querySelector('#enabledMods li.enabled-mod[data-for="headhunter"]'));
  assert(!(await read()).headhunter, 'Tray Turn off left the mod on');
  assert(posts.length === 1, `Tray Turn off sent ${posts.length} POSTs`);
  passed.push('turn-off-tray');
}

// One ordinary row and the outliers: a table row built by row() (x<n>), a
// percent row (+<n>%), a static Svelte row (bare <n>%), and Monster Density
// with its own switch and handler.
const CASES = [
  { name: 'stats.exp', range: 'input[type=range][data-sec="stats"][data-key="exp"]', sw: '#sw_stats_exp', value: 5, other: 8 },
  { name: 'percent_stats.damage', range: 'input[type=range][data-sec="percent_stats"][data-key="damage"]', sw: '#sw_percent_stats_damage', value: 25, other: 50 },
  { name: 'rarity_rare', range: '#rarity_rare', sw: '#sw_rarity_rare', value: 25, other: 40 },
  { name: 'density', range: '#den', sw: '#den_on', value: 3, other: 4 },
];

async function remembered(ctx) {
  const { page, passed } = ctx;
  await only(page, []);
  const row = (c) => ({
    val: () => $(page, (s) => document.querySelector(s).closest('.row').querySelector('.val').textContent.trim(), c.range),
    span: () => $(page, (s) => {
      const r = document.querySelector(s).closest('.row');
      const span = r.querySelector('.val-remembered');
      return span && { text: span.textContent.trim(), hidden: span.getAttribute('aria-hidden'), first: r.querySelector('.val') !== span && !span.classList.contains('val'),
        gone: span.textContent.trim() === '' || !span.checkVisibility() };
    }, c.range),
  });
  const slide = (c, v) => $(page, ([s, value]) => {
    const r = document.querySelector(s);
    r.value = value;
    r.dispatchEvent(new Event('input', { bubbles: true }));
    r.dispatchEvent(new Event('change', { bubbles: true }));
  }, [c.range, v]);
  const tap = (s) => $(page, (sel) => document.querySelector(sel).click(), s);
  const checked = (s) => $(page, (sel) => document.querySelector(sel).checked, s);

  const last = {};
  for (const c of CASES) {
    await slide(c, c.value);
    await settled(page);
    if (!await checked(c.sw)) { await tap(c.sw); await settled(page); }
    const reading = await row(c).val();
    assert(reading && reading !== 'off', `${c.name}: no reading with the switch on`);
    await tap(c.sw);
    await settled(page);
    let span = await row(c).span();
    assert(await row(c).val() === 'off', `${c.name}: .val does not read off with the switch off`);
    assert(span && span.text === reading, `${c.name}: remembered ${JSON.stringify(span?.text)}, the row read ${reading}`);
    assert(span.hidden === 'true' && span.first, `${c.name}: the span is not aria-hidden, or it is the row's .val`);
    await slide(c, c.other);
    await settled(page);
    span = await row(c).span();
    assert(span.text !== reading && span.text !== '' && await row(c).val() === 'off',
      `${c.name}: moving the range while off left the span at ${span.text} or changed .val`);
    last[c.name] = span.text;
  }
  passed.push('remembered-value-when-off');

  for (const c of CASES) {
    await tap(c.sw);
    await settled(page);
    const span = await row(c).span();
    assert(await row(c).val() === last[c.name], `${c.name}: switched on, .val reads ${await row(c).val()}, not ${last[c.name]}`);
    assert(span.gone, `${c.name}: the remembered value still shows with the switch on`);
  }
  passed.push('remembered-value-cleared-when-on');
}

const GROUPS = [
  ['forms', forms],
  ['remembered', remembered],
];

const browser = await launchBrowser();
const passedAll = [];
let failures = 0;
try {
  for (const [name, fn] of GROUPS) {
    const sandbox = await startSandbox({ dist: typeof args.dist === 'string' ? args.dist : null });
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
console.log(`e2e-form: ${passedAll.length}/${EXPECTED.length} checks passed`);
process.exitCode = failures || missing.length ? 1 : 0;
