// lib/plugin-build.js: what the panel says about a plugin in the game that is
// not the one this ForgePact ships (issue #123). The baseline half is the
// states that must stay silent, so an up-to-date install, a missing plugin
// (mod_chain's to report) and a payload from before the field existed look
// exactly as they did; the target half is each stale state's words.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { PLUGIN_STALE_STATES, pluginBuildNotice } from '../src/lib/plugin-build.js';

test('nothing is said when the plugin is current, missing, newer or unknown to compare', () => {
  for (const state of ['current', 'missing', 'newer', 'no-bundle']) {
    assert.equal(pluginBuildNotice({ state, installed: '2.1.0', bundled: '2.1.0' }), null, state);
  }
  // A payload with no pluginBuild at all: an older backend, or a test's patch.
  assert.equal(pluginBuildNotice(undefined), null);
  assert.equal(pluginBuildNotice(null), null);
  assert.equal(pluginBuildNotice({}), null);
});

test('an older plugin names both versions and the fix', () => {
  const notice = pluginBuildNotice({ state: 'older', installed: '2.0.1', bundled: '2.1.0' });
  assert.equal(notice.chip, 'plugin out of date');
  assert.equal(notice.chain, 'mod plugin out of date: the game has v2.0.1, this ForgePact ships v2.1.0');
  assert.match(notice.warning, /v2\.0\.1, older than this ForgePact's v2\.1\.0/);
  assert.match(notice.warning, /click Install Mod Plugin in Setup\.$/);
});

test('a plugin from before 1.3.20 has no version and still reads as older', () => {
  const notice = pluginBuildNotice({ state: 'older', installed: null, bundled: '2.1.0' });
  assert.equal(notice.chain, 'mod plugin out of date: the game has an old version, this ForgePact ships v2.1.0');
});

test('a different build of the same version and an unreadable file each say so', () => {
  const different = pluginBuildNotice({ state: 'different', installed: '2.1.0', bundled: '2.1.0' });
  assert.equal(different.chip, 'plugin build differs');
  assert.match(different.chain, /the game's v2\.1\.0 is not the build this ForgePact ships/);
  const unknown = pluginBuildNotice({ state: 'unknown', installed: null, bundled: '2.1.0' });
  assert.equal(unknown.chip, 'plugin not recognised');
  assert.match(unknown.warning, /click Install Mod Plugin in Setup\.$/);
});

test('the stale states are the backend\'s', () => {
  // src/forgepact.py's PLUGIN_STALE_STATES decides the launch advice; the two
  // lists must name the same states or the panel and the launch disagree.
  const source = readFileSync(new URL('../../src/forgepact.py', import.meta.url), 'utf8');
  const declared = source.match(/^PLUGIN_STALE_STATES = \(([^)]*)\)/m);
  assert.ok(declared, 'PLUGIN_STALE_STATES is declared in src/forgepact.py');
  assert.deepEqual(declared[1].split(',').map((s) => s.trim().replace(/^"|"$/g, '')).filter(Boolean), PLUGIN_STALE_STATES);
});

test('every notice leads Setup\'s chain note without markup', () => {
  // status() writes `chain` into #chainnote's innerHTML, so no state's words
  // may carry markup of their own.
  for (const state of PLUGIN_STALE_STATES) {
    const notice = pluginBuildNotice({ state, installed: '2.0.1', bundled: '2.1.0' });
    for (const text of Object.values(notice)) assert.doesNotMatch(text, /[<>&"]/, `${state}: ${text}`);
  }
});
