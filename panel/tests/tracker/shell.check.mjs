// The Tracker tab's shell (#185): its nav entry in both layouts, the
// `mod_tracker` switch heading it, when the page polls `/api/tracker`, and the
// character the API names. Run by tests/tracker.e2e.mjs
// (`npm run e2e:tracker -- --only shell`).
//
// The baseline comes first and must not change: with the switch off and
// another tab open, the page sends no `/api/tracker` request at all, which is
// what keeps `e2e:perf` and the behaviour oracle as they were.

export const EXPECTED = [
  'off-and-elsewhere-sends-no-tracker-request',
  'nav-opens-the-tab-in-ember',
  'nav-opens-the-tab-in-ledger',
  'switch-off-by-default-sends-tracker-1-and-0',
  'polls-while-the-tab-is-open-at-most-once-a-second',
  'polls-on-any-tab-once-the-switch-is-on',
  'shows-the-character-the-api-names',
  'enabled-mods-lists-the-switch-while-on',
];

const wait = (ms) => new Promise((r) => setTimeout(r, ms));
const isTrackerGet = (req) => req.method() === 'GET' && new URL(req.url()).pathname.startsWith('/api/tracker');

function assert(ok, message) {
  if (!ok) throw new Error(message);
}

// Every `/api/tracker*` GET the page sends from now on, with its time.
function countTrackerGets(page) {
  const seen = [];
  page.on('request', (req) => { if (isTrackerGet(req)) seen.push({ at: Date.now(), path: new URL(req.url()).pathname }); });
  return seen;
}

async function openTab(page, name) {
  await page.click(`.tabbtn[data-tab="${name}"]`);
  await page.waitForFunction((n) => document.querySelector(`.tabbtn[data-tab="${n}"]`)?.getAttribute('aria-selected') === 'true', name);
}

async function setSetting(t, key, value) {
  const res = await t.api('/api/set', { body: { key, value } });
  assert(!res.err, `setting ${key}=${JSON.stringify(value)} was refused: ${res.err}`);
}

async function tabOpens(t, theme) {
  await setSetting(t, 'theme', theme);
  const page = await t.openPanel();
  assert(await page.locator('html').getAttribute('data-theme') === theme, `the page did not take the ${theme} palette`);
  const nav = page.locator('#nav-tracker');
  assert(await nav.count() === 1, 'there is not exactly one #nav-tracker');
  assert(await nav.getAttribute('data-tab') === 'tracker', '#nav-tracker does not name the tracker tab');
  assert(await nav.isVisible(), `#nav-tracker is not visible in ${theme}`);
  // The rail order: Tracker follows Mods.
  const order = await page.$$eval('.tabbtn', (buttons) => buttons.filter((b) => b.checkVisibility()).map((b) => b.dataset.tab));
  assert(order.indexOf('tracker') === order.indexOf('mods') + 1, `the rail reads ${order.join(', ')}`);
  await nav.click();
  await page.waitForFunction(() => document.getElementById('nav-tracker').getAttribute('aria-selected') === 'true');
  const shown = await page.evaluate(() => ({
    card: document.querySelector('.tab-card[data-tab="tracker"]')?.checkVisibility() || false,
    others: [...document.querySelectorAll('.tab-card.active')].map((c) => c.dataset.tab),
    title: document.getElementById('pageTitle').textContent,
    labelled: document.getElementById('workspace').getAttribute('aria-labelledby'),
    switchShown: document.getElementById('mod_tracker')?.closest('.row')?.checkVisibility() || false,
  }));
  assert(shown.card, `the Tracker tab card is not shown in ${theme}`);
  assert(shown.others.every((tab) => tab === 'tracker'), `other tabs stayed active: ${shown.others}`);
  assert(shown.title === 'Tracker', `the page title reads ${shown.title}`);
  assert(shown.labelled === 'nav-tracker', `#workspace is labelled by ${shown.labelled}`);
  assert(shown.switchShown, 'the switch row is not shown on the Tracker tab');
}

export default async function (t) {
  await t.check('off-and-elsewhere-sends-no-tracker-request', async () => {
    const state = await t.sandbox.state();
    assert(state.cfg.mod_tracker === false, 'the sandbox does not start with the switch off');
    const page = await t.openPanel();
    const gets = countTrackerGets(page);
    await openTab(page, 'loot');
    await wait(2500);
    await openTab(page, 'world');
    await wait(2500);
    assert(gets.length === 0, `the page sent ${gets.length} tracker requests: ${gets.map((g) => g.path).join(', ')}`);
    // The control: the same page polls as soon as the tab opens.
    await openTab(page, 'tracker');
    await page.waitForFunction(() => true);
    const until = Date.now() + 3000;
    while (!gets.length && Date.now() < until) await wait(100);
    assert(gets.length > 0, 'the control failed: opening the Tracker tab sent no request either');
  });

  await t.check('nav-opens-the-tab-in-ember', () => tabOpens(t, 'ember'));
  await t.check('nav-opens-the-tab-in-ledger', () => tabOpens(t, 'ledger'));

  await t.check('switch-off-by-default-sends-tracker-1-and-0', async () => {
    assert((await t.sandbox.state()).cfg.mod_tracker === false, 'mod_tracker is not off by default');
    const page = await t.openPanel();
    await openTab(page, 'tracker');
    const box = page.locator('#mod_tracker');
    assert(!await box.isChecked(), 'the switch shows on with the setting off');
    for (const value of [true, false]) {
      t.sandbox.truncateCmds();
      const [req] = await Promise.all([
        page.waitForRequest((r) => r.method() === 'POST' && new URL(r.url()).pathname === '/api/set'),
        box.click(),
      ]);
      const posted = JSON.parse(req.postData());
      assert(posted.key === 'mod_tracker' && posted.value === value, `the switch posted ${req.postData()}`);
      await page.waitForFunction(() => !document.getElementById('saveIndicator').classList.contains('saving'));
      assert((await t.sandbox.state()).cfg.mod_tracker === value, `the saved setting is not ${value}`);
      const cmds = t.sandbox.readCmds();
      assert(cmds.includes(`tracker ${value ? 1 : 0}`), `cmd.txt reads ${JSON.stringify(cmds)}`);
      assert(await box.isChecked() === value, `the switch does not show ${value}`);
      const shown = await page.locator('#mtrval').textContent();
      assert(shown.trim() === (value ? 'on' : 'off'), `the switch's value reads ${shown}`);
    }
  });

  await t.check('polls-while-the-tab-is-open-at-most-once-a-second', async () => {
    const page = await t.openPanel();
    const gets = countTrackerGets(page);
    await openTab(page, 'tracker');
    await wait(5000);
    const polls = gets.filter((g) => g.path === '/api/tracker');
    assert(polls.length >= 3, `only ${polls.length} polls in 5 s with the tab open`);
    assert(polls.length <= 6, `${polls.length} polls in 5 s: more than one a second`);
    for (let i = 1; i < polls.length; i++) {
      const gap = polls[i].at - polls[i - 1].at;
      assert(gap >= 900, `two polls ${gap} ms apart`);
    }
    // Leaving the tab with the switch off stops them.
    await openTab(page, 'mods');
    await wait(1500);
    const after = gets.length;
    await wait(3000);
    assert(gets.length === after, `${gets.length - after} tracker requests after the tab closed`);
  });

  await t.check('polls-on-any-tab-once-the-switch-is-on', async () => {
    const page = await t.openPanel();
    const gets = countTrackerGets(page);
    await openTab(page, 'loot');
    await wait(1500);
    assert(gets.length === 0, 'the page polled before the switch was on');
    await openTab(page, 'tracker');
    await Promise.all([
      page.waitForRequest((r) => r.method() === 'POST' && new URL(r.url()).pathname === '/api/set'),
      page.locator('#mod_tracker').click(),
    ]);
    await openTab(page, 'loot');
    const start = gets.length;
    await wait(3500);
    assert(gets.length - start >= 2, `${gets.length - start} tracker requests in 3.5 s on Loot with the switch on`);
    // A reload on another tab polls too: the saved setting, not the click, decides.
    await page.reload();
    const reloaded = countTrackerGets(page);
    await page.waitForFunction(() => document.getElementById('saveIndicator')?.textContent === 'Settings loaded');
    await openTab(page, 'world');
    await wait(3000);
    assert(reloaded.length >= 2, `${reloaded.length} tracker requests after a reload with the switch on`);
    await setSetting(t, 'mod_tracker', false);
  });

  await t.check('shows-the-character-the-api-names', async () => {
    const body = await t.api('/api/tracker');
    const name = body.snapshot?.character?.name;
    assert(typeof name === 'string' && name.length > 0, `the API names no character: ${JSON.stringify(body.snapshot?.character)}`);
    const page = await t.openPanel();
    await openTab(page, 'tracker');
    await page.waitForFunction((n) => document.getElementById('trackerState')?.textContent.includes(n), name, { timeout: 5000 });
    // The control: a sandbox with no session names nobody, and the tab says so.
    const empty = await t.startSandbox({ trackerSession: null });
    const none = await t.api('/api/tracker', {}, empty);
    assert(!none.snapshot?.character, `the empty sandbox names ${JSON.stringify(none.snapshot?.character)}`);
    const other = await t.openPanel({ sandbox: empty });
    await openTab(other, 'tracker');
    await other.waitForFunction(() => (document.getElementById('trackerState')?.textContent || '').trim().length > 0);
    await wait(1500);
    const text = await other.locator('#trackerState').textContent();
    assert(!text.includes(name), `the tab with no session still names ${name}: ${text}`);
  });

  await t.check('enabled-mods-lists-the-switch-while-on', async () => {
    const page = await t.openPanel();
    assert(await page.locator('#enabledMods [data-for="mod_tracker"]').count() === 0, 'listed while off');
    await openTab(page, 'tracker');
    await Promise.all([
      page.waitForRequest((r) => r.method() === 'POST' && new URL(r.url()).pathname === '/api/set'),
      page.locator('#mod_tracker').click(),
    ]);
    await page.waitForSelector('#enabledMods li.enabled-mod[data-for="mod_tracker"]', { state: 'attached', timeout: 5000 });
    const name = await page.locator('#enabledMods li.enabled-mod[data-for="mod_tracker"] .enabled-mod-name').textContent();
    assert(name.trim() === 'Tracker', `the entry is named ${name}`);
    // Its Turn off is the switch's own handler.
    t.sandbox.truncateCmds();
    await Promise.all([
      page.waitForRequest((r) => r.method() === 'POST' && new URL(r.url()).pathname === '/api/set'),
      page.locator('#enabledMods .quick-disable[data-for="mod_tracker"]').click(),
    ]);
    await page.waitForFunction(() => !document.querySelector('#enabledMods li.enabled-mod[data-for="mod_tracker"]'));
    assert((await t.sandbox.state()).cfg.mod_tracker === false, 'Turn off left the setting on');
    assert(t.sandbox.readCmds().includes('tracker 0'), `Turn off sent ${JSON.stringify(t.sandbox.readCmds())}`);
  });
}
