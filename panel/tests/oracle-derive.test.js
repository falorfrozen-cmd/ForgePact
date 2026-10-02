// tests/oracle-derive.mjs: the derived oracle is exactly what the generator
// makes from the legacy recording (so nobody hand-edits it), it carries no
// recorded values, every `same` looks back, and the step counts are the
// contract's: three switch clicks per switched slider, one Turn off per mod
// the list can show, one theme step per THEMES entry, after the one step that
// opens Setup, where the theme is, then the key supplement's slider (Prime
// Evil Parts), entered on the Loot tab, then the boolean mods no recording
// has (NATIVE_BOOLEANS), entered on Mods › Quality of Life, and last the
// switched sliders no recording has (NATIVE_SLIDERS), entered on Modifiers,
// and the Loot tab's after them.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { NATIVE_BOOLEANS, NATIVE_SLIDERS, derive, derivedFromPath, quickDisable, serialise, switchIdOf, tableRange } from './oracle-derive.mjs';
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
// The native booleans' steps close the file: the Mods tab and its Quality of
// Life sub-tab once, then on, off, on and Turn off for each.
const NATIVE_STEPS = 2 + 4 * NATIVE_BOOLEANS.length;
// Then the native sliders': one tab step each time the tab changes (Modifiers,
// then Loot), then a slider's eight steps each.
const NATIVE_SLIDER_TABS = NATIVE_SLIDERS.filter((n, i) => i === 0 || n.tab !== NATIVE_SLIDERS[i - 1].tab).length;
const NATIVE_SLIDER_STEPS = NATIVE_SLIDER_TABS + 8 * NATIVE_SLIDERS.length;
// Everything after the key supplement's slider.
const TAIL = NATIVE_STEPS + NATIVE_SLIDER_STEPS;

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

test('the counts: 132 switch clicks, 64 Turn off buttons, one theme step per theme', () => {
  const steps = DERIVED.steps;
  const switches = steps.filter((s) => s.control.startsWith('#sw_'));
  const quick = steps.filter((s) => s.control.startsWith('#enabledMods .quick-disable[data-for='));
  const theme = steps.filter((s) => s.control === '#theme');
  assert.equal(switches.length, 3 * (SLIDERS.length + KEY_SLIDERS.length + NATIVE_SLIDERS.length));
  assert.equal(quick.length, SLIDERS.length + KEY_SLIDERS.length + NATIVE_SLIDERS.length + BOOLEAN_MODS.length + 2);
  assert.equal(switches.length, 132);
  assert.equal(quick.length, 64);
  assert.equal(theme.length, THEMES.length);
  assert.deepEqual(theme.map((s) => s.value), THEMES.map((t) => t.value));
  for (const s of theme) {
    assert.deepEqual(s.expect.posts.is, [{ url: '/api/set', body: { key: 'theme', value: s.value } }]);
    assert.deepEqual(s.expect.cmds.is, []);
  }
  for (const s of switches) assert.ok('is' in s.expect.posts && 'same' in s.expect.cmds, s.control);
  for (const s of quick) assert.ok('same' in s.expect.posts && 'same' in s.expect.cmds, s.control);
  assert.equal(steps.filter((s) => NATIVE_BOOLEANS.some((n) => s.control === '#' + n.key)).length, 3 * NATIVE_BOOLEANS.length);
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
  // theme; only the key supplement's slider, the native booleans and the
  // native sliders (appended later) come after them, so no earlier step moved.
  const steps = DERIVED.steps;
  const first = steps.findIndex((s) => s.control === '#theme');
  assert.equal(steps[first - 1].control, 'tab:setup');
  assert.equal(steps[first - 1].action, 'click');
  assert.ok(!('expect' in steps[first - 1]), 'the Setup step carries an expectation');
  assert.equal(steps.filter((s) => s.control === 'tab:setup').length, 1);
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

test('every control is covered: the switches in legacy order, the theme, the key supplement\'s switch, the native booleans, then each native slider and its switch', () => {
  assert.deepEqual(DERIVED.controls, [...SLIDERS.map((c) => '#sw_' + switchIdOf(c).replace('.', '_')), '#theme', '#sw_keys_primeevil',
    ...NATIVE_BOOLEANS.map((n) => '#' + n.key),
    ...NATIVE_SLIDERS.flatMap((n) => [tableRange(n.section, n.key), `#sw_${n.section}_${n.key}`])]);
});

test('a native boolean\'s contract is literal: on sends its verb with 1, off with 0, its Turn off repeats the off', () => {
  assert.deepEqual(NATIVE_BOOLEANS.map((n) => n.key), ['mod_far_sleep', 'mod_pet_loot_unstick', 'mod_stash_move_all', 'density_rolling']);
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
  // All of them sit on the Quality of Life sub-tab, so it is entered once and
  // each boolean's four steps follow in turn.
  NATIVE_BOOLEANS.forEach(({ key, verb }, i) => {
    const first = at + 2 + 4 * i;
    const cb = '#' + key;
    assert.deepEqual(steps.slice(first, first + 4).map((s) => [s.control, s.action]),
      [[cb, 'click'], [cb, 'click'], [cb, 'click'], [quickDisable(key), 'click']]);
    const on = steps[first];
    const off = steps[first + 1];
    assert.deepEqual(on.expect, { posts: { is: [{ url: '/api/set', body: { key, value: true } }] }, cmds: { is: [`${verb} 1`] } });
    assert.deepEqual(off.expect, { posts: { is: [{ url: '/api/set', body: { key, value: false } }] }, cmds: { is: [`${verb} 0`] } });
    assert.deepEqual(steps[first + 2].expect, { posts: { same: on.step }, cmds: { same: on.step } });
    assert.deepEqual(steps[first + 3].expect, { posts: { same: off.step }, cmds: { same: off.step } });
  });
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
    ['percent_stats.skillhaste', 'percent_stats.allskills', 'drops.mining_ore_rolls']);
  const steps = DERIVED.steps;
  const at = steps.length - NATIVE_SLIDER_STEPS;
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
  // The native booleans' last Turn off comes first, so none of their indexes moved.
  assert.equal(steps[at - 1].control, quickDisable(NATIVE_BOOLEANS.at(-1).key));
  // Each slider's eight steps follow in list order, after one tab step
  // whenever the tab changes (Modifiers, then Loot for Mining Ore Extra Rolls).
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
  assert.equal(first, steps.length);
  // Mining Ore Extra Rolls: max sends `miningrolls 10`, min (its default, 1)
  // `miningrolls 1`.
  const rolls = NATIVE_SLIDERS.find((n) => n.key === 'mining_ore_rolls');
  assert.deepEqual([rolls.tab, rolls.min, rolls.max, rolls.atMin, rolls.atMax], ['tab:loot', 1, 10, 'miningrolls 1', 'miningrolls 10']);
  // The last control listed is the last native slider's switch, and the last
  // step is its slider back at its minimum.
  const last = NATIVE_SLIDERS.at(-1);
  assert.equal(DERIVED.controls.at(-1), `#sw_${last.section}_${last.key}`);
  assert.equal(steps.at(-1).control, tableRange(last.section, last.key));
});
