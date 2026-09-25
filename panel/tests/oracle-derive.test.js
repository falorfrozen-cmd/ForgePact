// tests/oracle-derive.mjs: the derived oracle is exactly what the generator
// makes from the legacy recording (so nobody hand-edits it), it carries no
// recorded values, every `same` looks back, and the step counts are the
// contract's: three switch clicks per switched slider, one Turn off per mod
// the list can show, one theme step per THEMES entry.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { fileURLToPath } from 'node:url';
import { derive, derivedFromPath, quickDisable, serialise, switchIdOf } from './oracle-derive.mjs';
import { BOOLEAN_MODS } from '../src/enabled-mods.js';
import { THEMES } from '../src/theme.js';

const read = (name) => readFileSync(new URL(name, import.meta.url), 'utf8');
const LEGACY_TEXT = read('./behaviour-oracle.json');
const LEGACY = JSON.parse(LEGACY_TEXT);
const COMMITTED_TEXT = read('./behaviour-oracle-derived.json');
const DERIVED = JSON.parse(COMMITTED_TEXT);
const SUPPLEMENT = JSON.parse(read('./behaviour-oracle-gems.json'));
const SLIDERS = LEGACY.controls.filter((c) => switchIdOf(c));

test('the committed file is byte-identical to a fresh derivation', () => {
  const fresh = serialise(derive(JSON.parse(LEGACY_TEXT), 'tests/behaviour-oracle.json', SUPPLEMENT, 'tests/behaviour-oracle-gems.json'));
  assert.equal(fresh, COMMITTED_TEXT);
  assert.ok(!COMMITTED_TEXT.includes('\r'), 'LF only');
});

test('the source path is named relative to panel/, wherever it was run from', () => {
  assert.equal(derivedFromPath(fileURLToPath(new URL('./behaviour-oracle.json', import.meta.url))), 'tests/behaviour-oracle.json');
  assert.equal(DERIVED.derivedFrom, 'tests/behaviour-oracle.json');
  assert.equal(DERIVED.legacyRecordedAt, LEGACY.recordedAt);
  assert.equal(DERIVED.supplementFrom, 'tests/behaviour-oracle-gems.json');
});

test('the supplement\'s boolean mods get on, off, on and Turn off, on the tab it reached them on', () => {
  const gems = SUPPLEMENT.controls.filter((c) => BOOLEAN_MODS.includes(c.slice(1)));
  assert.deepEqual(gems, ['#mod_gem_mythic', '#mod_gem_maxroll']);
  for (const control of gems) {
    const at = DERIVED.steps.findIndex((s) => s.control === control);
    assert.deepEqual(DERIVED.steps.slice(at, at + 4).map((s) => s.control), [control, control, control, quickDisable(control.slice(1))]);
    assert.deepEqual(DERIVED.steps[at + 3].expect, { posts: { same: at + 1 }, cmds: { same: at + 1 } });
    const before = DERIVED.steps.slice(0, at).map((s) => s.control);
    assert.equal(before.filter((c) => c.startsWith('tab:')).at(-1), 'tab:mods');
    assert.equal(before.filter((c) => c.startsWith('subtab:')).at(-1), 'subtab:qol');
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

test('the counts: 120 switch clicks, 56 Turn off buttons, one theme step per theme', () => {
  const steps = DERIVED.steps;
  const switches = steps.filter((s) => s.control.startsWith('#sw_'));
  const quick = steps.filter((s) => s.control.startsWith('#enabledMods .quick-disable[data-for='));
  const theme = steps.filter((s) => s.control === '#theme');
  assert.equal(switches.length, 3 * SLIDERS.length);
  assert.equal(quick.length, SLIDERS.length + BOOLEAN_MODS.length + 2);
  assert.equal(switches.length, 120);
  assert.equal(quick.length, 56);
  assert.equal(theme.length, THEMES.length);
  assert.deepEqual(theme.map((s) => s.value), THEMES.map((t) => t.value));
  for (const s of theme) {
    assert.deepEqual(s.expect.posts.is, [{ url: '/api/set', body: { key: 'theme', value: s.value } }]);
    assert.deepEqual(s.expect.cmds.is, []);
  }
  for (const s of switches) assert.ok('is' in s.expect.posts && 'same' in s.expect.cmds, s.control);
  for (const s of quick) assert.ok('same' in s.expect.posts && 'same' in s.expect.cmds, s.control);
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

test('every control is covered: the switches in legacy order, then the theme', () => {
  assert.deepEqual(DERIVED.controls, [...SLIDERS.map((c) => '#sw_' + switchIdOf(c).replace('.', '_')), '#theme']);
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
