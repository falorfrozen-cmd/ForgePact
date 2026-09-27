// tests/lib/oracle-relocate.mjs: the Gems supplement's navigation relocation
// moves its two navigation steps to the Loot tab and nothing else, keeps what
// every step sent exactly as recorded, refuses every map that could hide a
// behaviour, and never touches its input. insertAddedKeys gives each full key
// reset of a recording made before a key existed exactly that key's reset
// line, after its anchor, and nothing else, and refuses a recording made after.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { ADDED_KEYS, FULL_RESET, SUPPLEMENT_RELOCATION, insertAddedKeys, relocate } from './lib/oracle-relocate.mjs';

const TEXT = readFileSync(new URL('./behaviour-oracle-gems.json', import.meta.url), 'utf8');
const SUPPLEMENT = JSON.parse(TEXT);
const LEGACY_TEXT = readFileSync(new URL('./behaviour-oracle.json', import.meta.url), 'utf8');
const LEGACY = JSON.parse(LEGACY_TEXT);
const PRIMEEVIL = JSON.parse(readFileSync(new URL('./behaviour-oracle-primeevil.json', import.meta.url), 'utf8'));
const kept = (s) => JSON.stringify([s.step, s.action, s.value, s.posts, s.cmds]);

test('the map is exactly Mods and its Quality of Life sub-tab to Loot', () => {
  assert.deepEqual({ ...SUPPLEMENT_RELOCATION }, { 'tab:mods': 'tab:loot', 'subtab:qol': 'tab:loot' });
  assert.ok(Object.isFrozen(SUPPLEMENT_RELOCATION));
});

test('the committed supplement moves steps 0 and 1 only, every post and command as recorded', () => {
  const moved = relocate(SUPPLEMENT, SUPPLEMENT_RELOCATION);
  assert.equal(moved.steps.length, SUPPLEMENT.steps.length);
  moved.steps.forEach((s, i) => assert.equal(kept(s), kept(SUPPLEMENT.steps[i]), `step ${i}`));
  const changed = moved.steps.filter((s, i) => s.control !== SUPPLEMENT.steps[i].control);
  assert.deepEqual(changed.map((s) => [s.step, s.relocatedFrom, s.control]), [[0, 'tab:mods', 'tab:loot'], [1, 'subtab:qol', 'tab:loot']]);
  assert.ok(moved.steps.slice(2).every((s) => !('relocatedFrom' in s)));
  // Everything else of the recording is carried over untouched.
  const { steps: _a, ...rest } = moved;
  const { steps: _b, ...want } = SUPPLEMENT;
  assert.deepEqual(rest, want);
});

test('refuses a map whose key is not a navigation step', () => {
  assert.throws(() => relocate(SUPPLEMENT, { '#mod_gem_mythic': 'tab:loot' }), /not a navigation step/);
});

test('refuses a map whose value is not a navigation step', () => {
  assert.throws(() => relocate(SUPPLEMENT, { 'tab:mods': '#gemsCard' }), /not a navigation step/);
});

test('refuses to move a step that sent anything', () => {
  const tampered = structuredClone(SUPPLEMENT);
  tampered.steps[0].posts = [{ url: '/api/set', body: { key: 'mod_gem_mythic', value: true } }];
  assert.throws(() => relocate(tampered, SUPPLEMENT_RELOCATION), /only a step that sent nothing may move/);
  const commanded = structuredClone(SUPPLEMENT);
  commanded.steps[1].cmds = ['gemmythic 1'];
  assert.throws(() => relocate(commanded, SUPPLEMENT_RELOCATION), /only a step that sent nothing may move/);
});

test('refuses a key that matches no step (a stale map)', () => {
  assert.throws(() => relocate(SUPPLEMENT, { ...SUPPLEMENT_RELOCATION, 'subtab:items': 'tab:loot' }), /subtab:items matches no step/);
});

test('never mutates its input', () => {
  const input = JSON.parse(TEXT);
  const out = relocate(input, SUPPLEMENT_RELOCATION);
  assert.equal(JSON.stringify(input), JSON.stringify(SUPPLEMENT));
  out.steps[2].posts.push('x');
  out.viewport.width = 1;
  assert.equal(JSON.stringify(input), JSON.stringify(SUPPLEMENT));
});

test('insertAddedKeys: the one added key is Prime Evil Parts at x1, after Colosseum Fragments', () => {
  assert.deepEqual(ADDED_KEYS.map((k) => ({ ...k })), [{ key: 'primeevil', line: 'droprate group primeevil 1', after: 'droprate group colosfrag ' }]);
  assert.ok(Object.isFrozen(ADDED_KEYS) && Object.isFrozen(ADDED_KEYS[0]));
  assert.equal(FULL_RESET, 'dungeonkey del 12');
});

test('insertAddedKeys: 70 legacy full resets gain the line after colosfrag; every other step and field as recorded', () => {
  const out = insertAddedKeys(LEGACY);
  assert.equal(out.steps.length, LEGACY.steps.length);
  let changed = 0;
  out.steps.forEach((s, i) => {
    const was = LEGACY.steps[i];
    assert.deepEqual([s.step, s.control, s.action, s.value, s.posts], [was.step, was.control, was.action, was.value, was.posts], `step ${i}`);
    if (!was.cmds.includes(FULL_RESET)) {
      assert.deepEqual(s.cmds, was.cmds, `step ${i}`);
      return;
    }
    changed++;
    const k = was.cmds.findIndex((c) => c.startsWith('droprate group colosfrag '));
    assert.deepEqual(s.cmds, [...was.cmds.slice(0, k + 1), 'droprate group primeevil 1', ...was.cmds.slice(k + 1)], `step ${i}`);
  });
  assert.equal(changed, 70);
  const { steps: _a, ...rest } = out;
  const { steps: _b, ...want } = LEGACY;
  assert.deepEqual(rest, want);
});

test('insertAddedKeys: the Gems supplement holds no full reset, so nothing changes', () => {
  assert.equal(JSON.stringify(insertAddedKeys(SUPPLEMENT)), JSON.stringify(SUPPLEMENT));
});

test('insertAddedKeys: refuses a recording made after the key existed (the Prime Evil supplement), and a second pass', () => {
  assert.ok(PRIMEEVIL.steps.some((s) => s.cmds.includes(FULL_RESET)), 'positive control: the supplement has a full reset step');
  assert.throws(() => insertAddedKeys(PRIMEEVIL), /already sends "droprate group primeevil 1"/);
  assert.throws(() => insertAddedKeys(insertAddedKeys(LEGACY)), /already sends/);
});

test('insertAddedKeys: refuses a full reset without exactly one anchor line', () => {
  const at = LEGACY.steps.findIndex((s) => s.cmds.includes(FULL_RESET));
  const none = structuredClone(LEGACY);
  none.steps[at].cmds = none.steps[at].cmds.filter((c) => !c.startsWith('droprate group colosfrag '));
  assert.throws(() => insertAddedKeys(none), /0 lines starting "droprate group colosfrag "/);
  const two = structuredClone(LEGACY);
  two.steps[at].cmds.push('droprate group colosfrag 3');
  assert.throws(() => insertAddedKeys(two), /2 lines starting/);
});

test('insertAddedKeys: never mutates its input', () => {
  const input = JSON.parse(LEGACY_TEXT);
  const out = insertAddedKeys(input);
  assert.equal(JSON.stringify(input), JSON.stringify(LEGACY));
  out.steps[0].cmds.push('x');
  out.viewport.width = 1;
  assert.equal(JSON.stringify(input), JSON.stringify(LEGACY));
});
