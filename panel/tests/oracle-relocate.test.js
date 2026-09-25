// tests/lib/oracle-relocate.mjs: the Gems supplement's navigation relocation
// moves its two navigation steps to the Loot tab and nothing else, keeps what
// every step sent exactly as recorded, refuses every map that could hide a
// behaviour, and never touches its input.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { SUPPLEMENT_RELOCATION, relocate } from './lib/oracle-relocate.mjs';

const TEXT = readFileSync(new URL('./behaviour-oracle-gems.json', import.meta.url), 'utf8');
const SUPPLEMENT = JSON.parse(TEXT);
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
