// The pure decisions behind the restyle's review fixes, run without a
// browser (tests/review-fixes.e2e.mjs drives them on the page):
//   - where focus goes after a Turn off (src/lib/enabled-mods-undo.js),
//   - when the inline/tray switch is held (src/lib/enabled-mods-form.js),
//   - which Setup button is the primary one (src/lib/review-fixes.js),
//   - an entry's tooltip (src/lib/review-fixes.js).
// Each has its ordinary case and the outliers beside it.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { focusAfterTurnOff, FREEZE_MAX_MS, UNDO_VISIBLE_MS } from '../src/lib/enabled-mods-undo.js';
import { HOLD_IDLE_MS, holdActive } from '../src/lib/enabled-mods-form.js';
import { entryTitle, setupEmphasis } from '../src/lib/review-fixes.js';
import { ENTRY_TITLE_JOINER } from '../src/lib/enabled-mods-copy.js';

const exported = JSON.parse(readFileSync(new URL('../design/figma-export.json', import.meta.url), 'utf8'));
const ORDER = ['sw_stats_exp', 'headhunter', 'map_reveal', 'den_on'];

test('the three timings are the export\'s', () => {
  const m = exported.amendments.enabledMods;
  assert.equal(HOLD_IDLE_MS, m.hold.idleMs);
  assert.equal(FREEZE_MAX_MS, m.reflowFreeze.maxMs);
  assert.equal(UNDO_VISIBLE_MS, exported.amendments.undoToast.visibleMs);
});

test('after a Turn off, focus goes to the next entry', () => {
  const remaining = ORDER.filter((id) => id !== 'headhunter');
  assert.deepEqual(focusAfterTurnOff({ order: ORDER, removed: 'headhunter', remaining, form: 'inline' }), { to: 'entry', id: 'map_reveal' });
  // The next one that is still listed, not merely the next one that was.
  assert.deepEqual(focusAfterTurnOff({ order: ORDER, removed: 'headhunter', remaining: ['sw_stats_exp', 'den_on'], form: 'inline' }),
    { to: 'entry', id: 'den_on' });
});

test('the last entry hands focus to the previous one', () => {
  assert.deepEqual(focusAfterTurnOff({ order: ORDER, removed: 'den_on', remaining: ORDER.slice(0, 3), form: 'inline' }),
    { to: 'entry', id: 'map_reveal' });
});

test('the only entry hands focus to the heading, or to the count control in the tray', () => {
  assert.deepEqual(focusAfterTurnOff({ order: ['headhunter'], removed: 'headhunter', remaining: [], form: 'inline' }), { to: 'heading' });
  assert.deepEqual(focusAfterTurnOff({ order: ['headhunter'], removed: 'headhunter', remaining: [], form: 'tray' }), { to: 'toggle' });
  // An entry the list still shows wins over the fallback in either form.
  assert.deepEqual(focusAfterTurnOff({ order: ORDER, removed: 'den_on', remaining: ['sw_stats_exp'], form: 'tray' }),
    { to: 'entry', id: 'sw_stats_exp' });
});

test('the hold: while a pointer button or a key is down, while the popover is open, then HOLD_IDLE_MS', () => {
  const now = 10000;
  // Baseline: nothing held, nothing released lately.
  assert.equal(holdActive({}, now), false);
  assert.equal(holdActive({ releasedAt: now - HOLD_IDLE_MS }, now), false);
  assert.equal(holdActive({ pointerDown: true }, now), true);
  assert.equal(holdActive({ keysDown: 1 }, now), true);
  assert.equal(holdActive({ popoverOpen: true }, now), true);
  assert.equal(holdActive({ releasedAt: now - HOLD_IDLE_MS + 1 }, now), true);
  assert.equal(holdActive({ keysDown: 0, pointerDown: false, popoverOpen: false, releasedAt: now - 5 * HOLD_IDLE_MS }, now), false);
});

test('Setup: Install Mod Plugin is primary while the chain is incomplete, Launch once it is complete', () => {
  assert.deepEqual(setupEmphasis(true), { installmod: true, launchgame: false });
  assert.deepEqual(setupEmphasis(false), { installmod: false, launchgame: true });
});

test('an entry\'s tooltip names its section, unless there is none or it says the same', () => {
  assert.equal(entryTitle('Monster Rarity', 'Rare'), `Monster Rarity${ENTRY_TITLE_JOINER}Rare`);
  assert.equal(entryTitle('Monster Density', 'Monster Density'), 'Monster Density');
  assert.equal(entryTitle('', 'Headhunter buffs on rare kills'), 'Headhunter buffs on rare kills');
  assert.equal(entryTitle(null, 'Gold'), 'Gold');
});
