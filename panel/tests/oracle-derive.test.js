// tests/oracle-derive.mjs: the derived oracle is exactly what the generator
// makes from the legacy recording (so nobody hand-edits it), it carries no
// recorded values, every `same` looks back, and the step counts are the
// contract's: three switch clicks per switched slider, one Turn off per mod
// the list can show, one theme step per THEMES entry, after the one step that
// opens Setup, where the theme is, then the key supplement's slider (Prime
// Evil Parts), entered on the Loot tab, then the boolean mods no recording
// has (NATIVE_BOOLEANS - Mods › Quality of Life, with the two Satanic Zone
// control switches entering the World tab and back), then the show
// key's select of Sleep loot your filter hides, then the switched sliders no
// recording has (NATIVE_SLIDERS), entered on Modifiers, the Loot tab's after
// them and the skill sliders back on Modifiers, then the panel's own Incident reports controls (PANEL_BOOLEANS,
// PANEL_BUTTONS), entered on Setup, and last of all the selects no recording
// has (NATIVE_SELECTS, the Bosses select), entered again on Mods › Gameplay.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import {
  HIDDEN_LOOT_KEY_CODES, HIDDEN_LOOT_KEY_PARENT, NATIVE_BOOLEANS, NATIVE_SELECTS, NATIVE_SLIDERS, PANEL_BOOLEANS, PANEL_BUTTONS, derive,
  derivedFromPath, quickDisable, serialise, switchIdOf, tableRange,
} from './oracle-derive.mjs';
import { HIDDEN_LOOT_KEYS, HIDDEN_LOOT_KEY_DEFAULT } from '../src/hidden-loot-keys.js';
import { BOOLEAN_MODS } from '../src/enabled-mods.js';
import { THEMES } from '../src/theme.js';

const read = (name) => readFileSync(new URL(name, import.meta.url), 'utf8');
const LEGACY_TEXT = read('./behaviour-oracle.json');
const LEGACY = JSON.parse(LEGACY_TEXT);
const COMMITTED_TEXT = read('./behaviour-oracle-derived.json');
const DERIVED = JSON.parse(COMMITTED_TEXT);
const SUPPLEMENT = JSON.parse(read('./behaviour-oracle-gems.json'));
const KEY_SUPPLEMENT = JSON.parse(read('./behaviour-oracle-primeevil.json'));
const SLIDERS = LEGACY.controls.filter((c) => switchIdOf(c));
const KEY_SLIDERS = KEY_SUPPLEMENT.controls.filter((c) => switchIdOf(c));
// The native booleans' steps close the file: one tab step each time the tab
// changes (the first opens Mods) and one sub-tab step each time the sub-tab
// changes on it (Quality of Life opens with Mods; the World-tab pair makes
// Jump through scenery re-enter both), then on, off, on and Turn off for each.
const NATIVE_NAV = (() => {
  let count = 0;
  let tab = null;
  let sub = null;
  for (const n of NATIVE_BOOLEANS) {
    if (n.tab !== tab) { count += 1; tab = n.tab; sub = null; }
    if (n.sub && n.sub !== sub) { count += 1; sub = n.sub; }
  }
  return count;
})();
const NATIVE_STEPS = NATIVE_NAV + 4 * NATIVE_BOOLEANS.length;
// Then the show key's select: its switch on, one select per code, its switch off.
const KEY_STEPS = 2 + HIDDEN_LOOT_KEY_CODES.length;
// Then the native sliders': one tab step each time the tab changes (Modifiers,
// then Loot, then Modifiers again for the skill sliders), then a slider's
// eight steps each.
const NATIVE_SLIDER_TABS = NATIVE_SLIDERS.filter((n, i) => i === 0 || n.tab !== NATIVE_SLIDERS[i - 1].tab).length;
const NATIVE_SLIDER_STEPS = NATIVE_SLIDER_TABS + 8 * NATIVE_SLIDERS.length;
// Then the panel's own controls: one tab step each time the tab changes
// (from the last native slider's), two clicks per switch, one per button.
const PANEL_CONTROLS = [...PANEL_BOOLEANS, ...PANEL_BUTTONS];
const PANEL_TABS = PANEL_CONTROLS.filter((n, i) => n.tab !== (i === 0 ? NATIVE_SLIDERS.at(-1).tab : PANEL_CONTROLS[i - 1].tab)).length;
const PANEL_STEPS = PANEL_TABS + 2 * PANEL_BOOLEANS.length + PANEL_BUTTONS.length;
// Then, last, the native selects': the Mods tab and its Gameplay sub-tab once
// (the panel's own controls left Setup open), then raised, off, raised and
// Turn off for each.
const NATIVE_SELECT_STEPS = 2 + 4 * NATIVE_SELECTS.length;
// Everything after the key supplement's slider.
const TAIL = NATIVE_STEPS + KEY_STEPS + NATIVE_SLIDER_STEPS + PANEL_STEPS + NATIVE_SELECT_STEPS;

test('the committed file is byte-identical to a fresh derivation', () => {
  const fresh = serialise(derive(JSON.parse(LEGACY_TEXT), 'tests/behaviour-oracle.json', SUPPLEMENT, 'tests/behaviour-oracle-gems.json',
    KEY_SUPPLEMENT, 'tests/behaviour-oracle-primeevil.json'));
  assert.equal(fresh, COMMITTED_TEXT);
  assert.ok(!COMMITTED_TEXT.includes('\r'), 'LF only');
});

test('the source path is named relative to panel/, wherever it was run from', () => {
  assert.equal(derivedFromPath(fileURLToPath(new URL('./behaviour-oracle.json', import.meta.url))), 'tests/behaviour-oracle.json');
  assert.equal(DERIVED.derivedFrom, 'tests/behaviour-oracle.json');
  assert.equal(DERIVED.legacyRecordedAt, LEGACY.recordedAt);
  assert.equal(DERIVED.supplementFrom, 'tests/behaviour-oracle-gems.json');
  assert.equal(DERIVED.keySupplementFrom, 'tests/behaviour-oracle-primeevil.json');
});

test('the supplement\'s boolean mods get on, off, on and Turn off, on the tab it reached them on', () => {
  const gems = SUPPLEMENT.controls.filter((c) => BOOLEAN_MODS.includes(c.slice(1)));
  assert.deepEqual(gems, ['#mod_gem_mythic', '#mod_gem_maxroll']);
  // The supplement was recorded on Mods › Quality of Life; relocated
  // (tests/lib/oracle-relocate.mjs), it reaches the gems on the Loot tab.
  const first = DERIVED.steps.findIndex((s) => s.control === gems[0]);
  const lastNav = DERIVED.steps.slice(0, first).findLastIndex((s) => /^(tab|subtab):/.test(s.control));
  assert.equal(lastNav, first - 1);
  assert.equal(DERIVED.steps[lastNav].control, 'tab:loot');
  for (const control of gems) {
    const at = DERIVED.steps.findIndex((s) => s.control === control);
    assert.deepEqual(DERIVED.steps.slice(at, at + 4).map((s) => s.control), [control, control, control, quickDisable(control.slice(1))]);
    assert.deepEqual(DERIVED.steps[at + 3].expect, { posts: { same: at + 1 }, cmds: { same: at + 1 } });
    const before = DERIVED.steps.slice(0, at).map((s) => s.control);
    assert.equal(before.filter((c) => c.startsWith('tab:')).at(-1), 'tab:loot');
    assert.ok(!before.slice(lastNav).some((c) => c.startsWith('subtab:')), `${control}: a sub-tab step after tab:loot`);
  }
  // Without a supplement, nothing of it is derived.
  const bare = derive(LEGACY, 'tests/behaviour-oracle.json');
  assert.ok(!('supplementFrom' in bare));
  assert.ok(!bare.steps.some((s) => gems.includes(s.control)));
});

test('forty switched sliders, never density', () => {
  assert.equal(SLIDERS.length, 40);
  assert.ok(!SLIDERS.includes('#den'));
  assert.equal(switchIdOf('#enemyspeed'), 'enemy_speed');
  assert.equal(switchIdOf('input[type=range][data-sec="keys"][data-key="relic"]'), 'keys.relic');
  assert.equal(switchIdOf('#enemyspeed_ct'), null);
});

test('no step carries a recorded value; every expectation is same-earlier or a literal', () => {
  DERIVED.steps.forEach((s, i) => {
    assert.equal(s.step, i);
    assert.ok(!('posts' in s) && !('cmds' in s), `step ${i} carries a recorded value`);
    for (const field of ['posts', 'cmds']) {
      const e = s.expect?.[field];
      if (!e) continue;
      assert.notEqual('same' in e, 'is' in e, `step ${i} ${field}`);
      if ('same' in e) {
        assert.ok(e.same < i, `step ${i} ${field} points forward`);
        assert.ok(!DERIVED.steps[e.same].control.startsWith('tab:'), `step ${i} ${field} points at a tab switch`);
      }
    }
  });
});

test('the counts: 141 switch clicks, 73 Turn off buttons, one theme step per theme', () => {
  const steps = DERIVED.steps;
  const switches = steps.filter((s) => s.control.startsWith('#sw_'));
  const quick = steps.filter((s) => s.control.startsWith('#enabledMods .quick-disable[data-for='));
  const theme = steps.filter((s) => s.control === '#theme');
  assert.equal(switches.length, 3 * (SLIDERS.length + KEY_SLIDERS.length + NATIVE_SLIDERS.length));
  assert.equal(quick.length, SLIDERS.length + KEY_SLIDERS.length + NATIVE_SLIDERS.length + BOOLEAN_MODS.length + 2 + NATIVE_SELECTS.length);
  assert.equal(switches.length, 141);
  assert.equal(quick.length, 73);
  assert.equal(theme.length, THEMES.length);
  assert.deepEqual(theme.map((s) => s.value), THEMES.map((t) => t.value));
  for (const s of theme) {
    assert.deepEqual(s.expect.posts.is, [{ url: '/api/set', body: { key: 'theme', value: s.value } }]);
    assert.deepEqual(s.expect.cmds.is, []);
  }
  for (const s of switches) assert.ok('is' in s.expect.posts && 'same' in s.expect.cmds, s.control);
  for (const s of quick) assert.ok('same' in s.expect.posts && 'same' in s.expect.cmds, s.control);
  // Three clicks each, and two more on the show key's switch around its select.
  assert.equal(steps.filter((s) => NATIVE_BOOLEANS.some((n) => s.control === '#' + n.key)).length, 3 * NATIVE_BOOLEANS.length + 2);
  assert.equal(steps.filter((s) => s.control === '#mod_hidden_loot_key').length, HIDDEN_LOOT_KEY_CODES.length);
  assert.ok(LEGACY.steps.length + steps.length >= 600);
});

test('a switch off compares with its slider at minimum, on with its slider at maximum', () => {
  const steps = DERIVED.steps;
  const off = steps.find((s) => s.control === '#sw_stats_exp' && s.expect.posts.is[0].body.value === false);
  const reference = steps[off.expect.cmds.same];
  assert.equal(reference.control, 'input[type=range][data-sec="stats"][data-key="exp"]');
  assert.equal(reference.action, 'min');
  const on = steps[off.step + 1];
  assert.equal(on.control, '#sw_stats_exp');
  assert.equal(steps[on.expect.cmds.same].action, 'max');
  const quick = steps[off.step + 2];
  assert.equal(quick.control, quickDisable('sw_stats_exp'));
  assert.equal(quick.expect.posts.same, off.step);
});

test('the theme steps come after one tab:setup step, and only the key supplement\'s and the native controls\' steps follow them', () => {
  // The theme moved from the status bar to the Setup tab's Appearance card:
  // one navigation step with no expectation opens Setup, then one step per
  // theme; only the key supplement's slider, the native booleans, the
  // native sliders, the panel's Incident reports controls and the native
  // selects (appended later) come after them, so no earlier step moved.
  const steps = DERIVED.steps;
  const first = steps.findIndex((s) => s.control === '#theme');
  assert.equal(steps[first - 1].control, 'tab:setup');
  assert.equal(steps[first - 1].action, 'click');
  assert.ok(!('expect' in steps[first - 1]), 'the Setup step carries an expectation');
  // Setup is opened once for the theme and once more for the Incident
  // reports card at the very end.
  assert.equal(steps.filter((s) => s.control === 'tab:setup').length, 2);
  assert.equal(steps.findIndex((s) => s.control === 'tab:setup'), first - 1);
  assert.deepEqual(steps.slice(first, first + THEMES.length).map((s) => s.control), THEMES.map(() => '#theme'));
  assert.equal(first, steps.length - THEMES.length - 1 - 8 * KEY_SLIDERS.length - TAIL);
  assert.ok(!DERIVED.controls.includes('tab:setup'), 'a navigation step is not a control');
  // Without the key supplement, only the native booleans' and sliders' steps
  // follow the theme, and everything before them is the same as with it.
  const bare = derive(LEGACY, 'tests/behaviour-oracle.json', SUPPLEMENT, 'tests/behaviour-oracle-gems.json');
  assert.equal(bare.steps.findIndex((s) => s.control === '#theme'), bare.steps.length - THEMES.length - TAIL);
  assert.ok(!('keySupplementFrom' in bare));
  const head = bare.steps.length - TAIL;
  assert.deepEqual(bare.steps.slice(0, head), DERIVED.steps.slice(0, head));
});

test('the key supplement\'s slider gets the eight slider steps, entered on the Loot tab after the theme', () => {
  assert.deepEqual(KEY_SLIDERS, ['input[type=range][data-sec="keys"][data-key="primeevil"]']);
  const steps = DERIVED.steps;
  const at = steps.length - 9 - TAIL;
  // The theme left Setup open, so the Loot tab is entered again first.
  assert.equal(steps[at - 1].control, '#theme');
  assert.deepEqual(steps[at], { step: at, control: 'tab:loot', action: 'click' });
  const [range] = KEY_SLIDERS;
  const sw = '#sw_keys_primeevil';
  assert.deepEqual(steps.slice(at + 1, at + 9).map((s) => [s.control, s.action]), [
    [range, 'max'], [range, 'min'], [range, 'max'], [sw, 'click'], [sw, 'click'],
    [quickDisable('sw_keys_primeevil'), 'click'], [sw, 'click'], [range, 'min'],
  ]);
  const off = steps[at + 4];
  assert.deepEqual(off.expect, {
    posts: { is: [{ url: '/api/set', body: { section: 'switches', key: 'keys.primeevil', value: false } }] },
    cmds: { same: at + 2 },
  });
  assert.equal(steps[at + 5].expect.cmds.same, at + 3);
});

test('every control is covered: the switches in legacy order, the theme, the key supplement\'s switch, the native booleans, the show key, each native slider and its switch, the panel\'s own controls, then the native selects', () => {
  assert.deepEqual(DERIVED.controls, [...SLIDERS.map((c) => '#sw_' + switchIdOf(c).replace('.', '_')), '#theme', '#sw_keys_primeevil',
    ...NATIVE_BOOLEANS.map((n) => '#' + n.key), '#mod_hidden_loot_key',
    ...NATIVE_SLIDERS.flatMap((n) => [tableRange(n.section, n.key), `#sw_${n.section}_${n.key}`]),
    ...PANEL_BOOLEANS.map((n) => '#' + n.key), ...PANEL_BUTTONS.map((b) => '#' + b.id),
    ...NATIVE_SELECTS.map((n) => '#' + n.key)]);
});

test('a native boolean\'s contract is literal: on sends its verb with 1, off with 0, its Turn off repeats the off', () => {
  assert.deepEqual(NATIVE_BOOLEANS.map((n) => n.key),
    ['mod_far_sleep', 'mod_pet_loot_unstick', 'mod_stash_move_all', 'density_rolling', 'mod_pet_relic_pickup', 'mod_hidden_loot',
      'satanic_follow', 'satanic_everywhere', 'mod_jump_scenery']);
  // Only Sleep loot your filter hides restates a child when it turns on: its
  // show key, at the default a fresh sandbox holds.
  assert.deepEqual(NATIVE_BOOLEANS.filter((n) => n.restate).map((n) => [n.key, n.restate]),
    [['mod_hidden_loot', `hiddenloot key ${HIDDEN_LOOT_KEY_DEFAULT}`]]);
  for (const n of NATIVE_BOOLEANS) {
    assert.ok(BOOLEAN_MODS.includes(n.key), `${n.key}: the Enabled mods list shows it, so it has a Turn off button`);
    const cb = '#' + n.key;
    assert.ok(!LEGACY.controls.includes(cb) && !SUPPLEMENT.controls.includes(cb) && !KEY_SUPPLEMENT.controls.includes(cb),
      `${n.key}: a recording lists it: derive it from there instead`);
  }
  // The legacy walk reached the Pet moves on switch's neighbour, Pet collects
  // quest items, on the same tab and sub-tab the native booleans are entered on.
  const neighbour = LEGACY.steps.findIndex((s) => s.control === '#mod_pet_quest_pickup');
  const nav = LEGACY.steps.slice(0, neighbour).filter((s) => /^(tab|subtab):/.test(s.control)).map((s) => s.control);
  assert.equal(nav.findLast((c) => c.startsWith('tab:')), 'tab:mods');
  assert.equal(nav.at(-1), 'subtab:qol');
  const steps = DERIVED.steps;
  const at = steps.length - TAIL;
  // Every earlier step (the key supplement's last one included) comes first,
  // so none of their indexes moved.
  assert.equal(steps[at - 1].control, KEY_SLIDERS[0]);
  assert.equal(steps[at - 1].action, 'min');
  for (const n of NATIVE_BOOLEANS) assert.ok(!steps.slice(0, at).some((s) => s.control.includes(n.key)), n.key);
  assert.deepEqual(steps.slice(at, at + 2).map((s) => [s.control, s.action]), [['tab:mods', 'click'], ['subtab:qol', 'click']]);
  assert.ok(!('expect' in steps[at]) && !('expect' in steps[at + 1]), 'a navigation step carries an expectation');
  // Each entry's four steps follow in list order, after the navigation steps
  // the loop emits whenever the tab or sub-tab changes: the World-tab pair
  // moves the loop to World and back (Jump through scenery re-enters Mods >
  // Quality of Life).
  let cursor = at + 2;
  let tab = 'tab:mods';
  let sub = 'subtab:qol';
  for (const { key, tab: entryTab, sub: entrySub, verb, restate } of NATIVE_BOOLEANS) {
    if (entryTab !== tab) {
      assert.deepEqual([steps[cursor].control, steps[cursor].action], [entryTab, 'click'], key);
      cursor += 1; tab = entryTab; sub = null;
    }
    if (entrySub && entrySub !== sub) {
      assert.deepEqual([steps[cursor].control, steps[cursor].action], [entrySub, 'click'], key);
      cursor += 1; sub = entrySub;
    }
    const first = cursor;
    const cb = '#' + key;
    assert.deepEqual(steps.slice(first, first + 4).map((s) => [s.control, s.action]),
      [[cb, 'click'], [cb, 'click'], [cb, 'click'], [quickDisable(key), 'click']]);
    const on = steps[first];
    const off = steps[first + 1];
    assert.deepEqual(on.expect, { posts: { is: [{ url: '/api/set', body: { key, value: true } }] },
      cmds: { is: [...(restate ? [restate] : []), `${verb} 1`] } });
    assert.deepEqual(off.expect, { posts: { is: [{ url: '/api/set', body: { key, value: false } }] }, cmds: { is: [`${verb} 0`] } });
    assert.deepEqual(steps[first + 2].expect, { posts: { same: on.step }, cmds: { same: on.step } });
    assert.deepEqual(steps[first + 3].expect, { posts: { same: off.step }, cmds: { same: off.step } });
    cursor += 4;
  }
  // The block ends exactly where the show key's select begins.
  assert.equal(cursor, steps.length - TAIL + NATIVE_STEPS);
});

test('the show key\'s select follows the native booleans: its switch on, Ctrl, None, Left Alt, its switch off', () => {
  // Codes the key list offers, the last one the saved default.
  const offered = HIDDEN_LOOT_KEYS.map(([code]) => code);
  assert.deepEqual(HIDDEN_LOOT_KEY_CODES, [17, 0, 164]);
  for (const code of HIDDEN_LOOT_KEY_CODES) assert.ok(offered.includes(code), code);
  assert.equal(HIDDEN_LOOT_KEY_CODES.at(-1), HIDDEN_LOOT_KEY_DEFAULT);
  const steps = DERIVED.steps;
  // The native sliders come after it, on the Modifiers tab, then the panel's
  // own controls, then the native selects.
  const at = steps.length - NATIVE_SELECT_STEPS - PANEL_STEPS - NATIVE_SLIDER_STEPS - KEY_STEPS;
  const parent = '#' + HIDDEN_LOOT_KEY_PARENT;
  const nativeAt = steps.length - TAIL + 2 + 4 * NATIVE_BOOLEANS.findIndex((n) => n.key === HIDDEN_LOOT_KEY_PARENT);
  const [on, off] = [nativeAt, nativeAt + 1];
  assert.equal(steps[on].control, parent);
  // The switch's own steps left it off, and the select is disabled while it
  // is; the native booleans after it (the Satanic Zone switches and Jump
  // through scenery) leave it alone.
  assert.equal(steps[nativeAt + 3].control, quickDisable(HIDDEN_LOOT_KEY_PARENT));
  assert.equal(steps[at - 1].control, quickDisable(NATIVE_BOOLEANS.at(-1).key));
  for (let i = nativeAt + 4; i < at; i++) assert.notEqual(steps[i].control, parent, i);
  assert.deepEqual(steps[at], { step: at, control: parent, action: 'click', expect: { posts: { same: on }, cmds: { same: on } } });
  HIDDEN_LOOT_KEY_CODES.forEach((code, i) => {
    assert.deepEqual(steps[at + 1 + i], {
      step: at + 1 + i, control: '#mod_hidden_loot_key', action: 'select', value: String(code),
      expect: { posts: { is: [{ url: '/api/set', body: { key: 'mod_hidden_loot_key', value: code } }] }, cmds: { is: [`hiddenloot key ${code}`] } },
    });
  });
  const last = at + KEY_STEPS - 1;
  assert.deepEqual(steps[last], { step: last, control: parent, action: 'click', expect: { posts: { same: off }, cmds: { same: off } } });
});

test('each control runs on the tab the legacy walk first reached it on', () => {
  let tab = null;
  let sub = null;
  const tabOf = {};
  for (const s of LEGACY.steps) {
    if (s.control.startsWith('tab:')) { tab = s.control; sub = null; } else if (s.control.startsWith('subtab:')) sub = s.control;
    else if (!(s.control in tabOf)) tabOf[s.control] = [tab, sub];
  }
  tab = null; sub = null;
  for (const s of DERIVED.steps) {
    if (s.control.startsWith('tab:')) { tab = s.control; sub = null; continue; }
    if (s.control.startsWith('subtab:')) { sub = s.control; continue; }
    const want = tabOf[s.control];
    if (!want) continue;   // switches, Turn off buttons and the theme: not in the legacy walk
    assert.equal(tab, want[0], s.control);
    if (want[1]) assert.equal(sub, want[1], s.control);
  }
});

test('a native slider\'s contract is literal and last: each end posts its value and sends its line, its switch sends the ends', () => {
  assert.deepEqual(NATIVE_SLIDERS.map((n) => `${n.section}.${n.key}`),
    ['percent_stats.skillhaste', 'percent_stats.allskills', 'drops.mining_ore_rolls',
      'percent_stats.projspeed', 'percent_stats.projamount', 'percent_stats.aoesize']);
  const steps = DERIVED.steps;
  const at = steps.length - NATIVE_SLIDER_STEPS - PANEL_STEPS - NATIVE_SELECT_STEPS;
  for (const n of NATIVE_SLIDERS) {
    for (const c of [tableRange(n.section, n.key), `#sw_${n.section}_${n.key}`]) {
      assert.ok(!LEGACY.controls.includes(c) && !SUPPLEMENT.controls.includes(c) && !KEY_SUPPLEMENT.controls.includes(c),
        `${c}: a recording lists it: derive it from there instead`);
      assert.ok(!steps.slice(0, at).some((s) => s.control === c), `${c} appears before the native sliders`);
    }
  }
  // Skill Haste's and All Skills' neighbour Faster Cast Rate is where the
  // legacy walk reached the percent rows: the Modifiers tab, no sub-tab.
  const neighbour = LEGACY.steps.findIndex((s) => s.control === tableRange('percent_stats', 'castrate'));
  const nav = LEGACY.steps.slice(0, neighbour).filter((s) => /^(tab|subtab):/.test(s.control)).map((s) => s.control);
  assert.equal(nav.at(-1), 'tab:modifiers');
  // The native booleans' steps and the show key's select (closed by its
  // switch's off click) come first, so none of their indexes moved.
  assert.equal(steps[at - 1].control, '#' + HIDDEN_LOOT_KEY_PARENT);
  // Each slider's eight steps follow in list order, after one tab step
  // whenever the tab changes (Modifiers, then Loot for Mining Ore Extra Rolls,
  // then Modifiers again for the skill sliders).
  let first = at;
  let tab = null;
  for (const { section, key, tab: sliderTab, min, max, atMin, atMax } of NATIVE_SLIDERS) {
    if (sliderTab !== tab) {
      assert.deepEqual(steps[first], { step: first, control: sliderTab, action: 'click' });
      tab = sliderTab;
      first += 1;
    }
    const range = tableRange(section, key);
    const sw = `#sw_${section}_${key}`;
    const post = (value) => [{ url: '/api/set', body: { section, key, value } }];
    const switched = (value) => [{ url: '/api/set', body: { section: 'switches', key: `${section}.${key}`, value } }];
    assert.deepEqual(steps.slice(first, first + 8).map((s) => [s.control, s.action]), [
      [range, 'max'], [range, 'min'], [range, 'max'], [sw, 'click'], [sw, 'click'],
      [quickDisable(`sw_${section}_${key}`), 'click'], [sw, 'click'], [range, 'min'],
    ]);
    assert.deepEqual(steps[first].expect, { posts: { is: post(max) }, cmds: { is: [atMax] } });
    assert.deepEqual(steps[first + 1].expect, { posts: { is: post(min) }, cmds: { is: [atMin] } });
    assert.deepEqual(steps[first + 2].expect, { posts: { same: first }, cmds: { same: first } });
    assert.deepEqual(steps[first + 3].expect, { posts: { is: switched(false) }, cmds: { same: first + 1 } });
    assert.deepEqual(steps[first + 4].expect, { posts: { is: switched(true) }, cmds: { same: first + 2 } });
    assert.deepEqual(steps[first + 5].expect, { posts: { same: first + 3 }, cmds: { same: first + 3 } });
    assert.deepEqual(steps[first + 6].expect, { posts: { is: switched(true) }, cmds: { same: first + 4 } });
    assert.deepEqual(steps[first + 7].expect, { posts: { same: first + 1 }, cmds: { same: first + 1 } });
    first += 8;
  }
  // Only the panel's own controls' steps and the native selects' follow.
  assert.equal(first, steps.length - PANEL_STEPS - NATIVE_SELECT_STEPS);
  // Mining Ore Extra Rolls: max sends `miningrolls 10`, min (its default, 1)
  // `miningrolls 1`.
  const rolls = NATIVE_SLIDERS.find((n) => n.key === 'mining_ore_rolls');
  assert.deepEqual([rolls.tab, rolls.min, rolls.max, rolls.atMin, rolls.atMax], ['tab:loot', 1, 10, 'miningrolls 1', 'miningrolls 10']);
  // The last native slider's switch is listed just before the panel's own
  // controls (and the native selects after them), and its slider back at its
  // minimum is the step before theirs.
  const last = NATIVE_SLIDERS.at(-1);
  assert.equal(DERIVED.controls.at(-1 - PANEL_CONTROLS.length - NATIVE_SELECTS.length), `#sw_${last.section}_${last.key}`);
  assert.equal(steps.at(-1 - PANEL_STEPS - NATIVE_SELECT_STEPS).control, tableRange(last.section, last.key));
});

test('the panel\'s own Incident reports control follows the native sliders: the button once, posting its literal and sending nothing', () => {
  // The FPS-drop switch went (an FPS drop is recorded without a notice, the
  // owner, 2026-10-02), so the card has no panel switch left.
  assert.deepEqual(PANEL_BOOLEANS, []);
  assert.deepEqual(PANEL_BUTTONS, [{ id: 'openreports', tab: 'tab:setup', url: '/api/openreports' }]);
  for (const c of ['#openreports']) {
    assert.ok(!LEGACY.controls.includes(c) && !SUPPLEMENT.controls.includes(c) && !KEY_SUPPLEMENT.controls.includes(c),
      `${c}: a recording lists it: derive it from there instead`);
  }
  const steps = DERIVED.steps;
  const at = steps.length - PANEL_STEPS - NATIVE_SELECT_STEPS;
  // The last native slider left the Loot tab open, so Setup is entered again.
  assert.equal(steps[at - 1].control, tableRange(NATIVE_SLIDERS.at(-1).section, NATIVE_SLIDERS.at(-1).key));
  assert.deepEqual(steps.slice(at, at + PANEL_STEPS), [
    { step: at, control: 'tab:setup', action: 'click' },
    { step: at + 1, control: '#openreports', action: 'click',
      expect: { posts: { is: [{ url: '/api/openreports', body: {} }] }, cmds: { is: [] } } },
  ]);
  // Only the native selects come after it.
  assert.equal(DERIVED.controls.at(-1 - NATIVE_SELECTS.length), '#openreports');
  assert.equal(steps.at(-1 - NATIVE_SELECT_STEPS).control, '#openreports');
});

test('a native select\'s contract is literal and last: raised and off post the value and send the verb with it, its Turn off repeats the off', () => {
  assert.deepEqual(NATIVE_SELECTS, [{ key: 'boss_rarity', tab: 'tab:mods', sub: 'subtab:gameplay', on: 'ancient', verb: 'bossrarity' }]);
  const steps = DERIVED.steps;
  const at = steps.length - NATIVE_SELECT_STEPS;
  for (const n of NATIVE_SELECTS) {
    const sel = '#' + n.key;
    assert.ok(!LEGACY.controls.includes(sel) && !SUPPLEMENT.controls.includes(sel) && !KEY_SUPPLEMENT.controls.includes(sel),
      `${sel}: a recording lists it: derive it from there instead`);
    assert.ok(!steps.slice(0, at).some((s) => s.control.includes(n.key)), `${sel} appears before the native selects`);
  }
  // The panel's own controls' last step (Open reports folder, on Setup) comes
  // first, so none of their indexes moved, and Mods › Gameplay is entered again.
  assert.equal(steps[at - 1].control, '#openreports');
  assert.deepEqual(steps.slice(at, at + 2), [
    { step: at, control: 'tab:mods', action: 'click' }, { step: at + 1, control: 'subtab:gameplay', action: 'click' }]);
  NATIVE_SELECTS.forEach(({ key, on: raised, verb }, i) => {
    const first = at + 2 + 4 * i;
    const sel = '#' + key;
    assert.deepEqual(steps.slice(first, first + 4).map((s) => [s.control, s.action, s.value]),
      [[sel, 'select', raised], [sel, 'select', 'off'], [sel, 'select', raised], [quickDisable(key), 'click', undefined]]);
    assert.deepEqual(steps[first].expect, { posts: { is: [{ url: '/api/set', body: { key, value: raised } }] }, cmds: { is: [`${verb} ${raised}`] } });
    assert.deepEqual(steps[first + 1].expect, { posts: { is: [{ url: '/api/set', body: { key, value: 'off' } }] }, cmds: { is: [`${verb} off`] } });
    assert.deepEqual(steps[first + 2].expect, { posts: { same: first }, cmds: { same: first } });
    assert.deepEqual(steps[first + 3].expect, { posts: { same: first + 1 }, cmds: { same: first + 1 } });
  });
  assert.equal(DERIVED.controls.at(-1), '#' + NATIVE_SELECTS.at(-1).key);
  assert.equal(steps.at(-1).control, quickDisable(NATIVE_SELECTS.at(-1).key));
});
