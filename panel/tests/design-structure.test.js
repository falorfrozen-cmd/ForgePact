// lib/design-structure.mjs: design-match's structure rules, without a
// browser. `runtimeValues` is proven on each kind of value the redesign's
// export carried by mistake (a version, a path, a port) with a clean text
// beside each, and on the real export, which must hold none. `scopedEntries`
// is proven on a component that exists, one that does not, and one without a
// selector. `placeEntries` runs in the page and is proven by
// tests/structure-selftest.mjs through design-match itself.
import test from 'node:test';
import assert from 'node:assert/strict';
import { existsSync, readFileSync } from 'node:fs';
import { panelRuntime, runtimeValues, scopedEntries } from './lib/design-structure.mjs';

const KNOWN = { version: '2.0.0', ports: ['8780', '8801'] };
const kinds = (text) => runtimeValues(text, KNOWN).map((r) => r.kind);

test('a version string is a runtime value, the panel\'s own or any x.y.z', () => {
  assert.deepEqual(kinds('ForgePact 2.0.0'), ['version']);
  assert.deepEqual(kinds('Updated to v1.4.7'), ['version']);
  // control: numbers that are settings, not versions
  assert.deepEqual(kinds('Monster density x3'), []);
  assert.deepEqual(kinds('Drop rate 1.5'), []);
  assert.deepEqual(kinds('0.97 scale'), []);
});

test('a filesystem path is a runtime value; a bare file name is not', () => {
  for (const text of ['C:\\Program Files (x86)\\Steam\\steamapps\\common\\Hero Siege',
    'D:/games/hero siege', '%LOCALAPPDATA%\\Hero_Siege\\forgepact.json',
    'reads bin\\bp_ipc\\cmd.txt', '\\\\nas\\share\\hs', 'font at /usr/share/fonts/plex.woff2']) {
    assert.ok(kinds(text).includes('path'), text);
  }
  // control: the real export's setup text names files without a path
  assert.deepEqual(kinds('mod chain incomplete: exe not patched, AurieCore.dll, YYToolkit.dll, mod plugin - click'), []);
  assert.deepEqual(kinds('Turn off / on'), []);
});

test('a port is a runtime value', () => {
  assert.ok(kinds('Open http://127.0.0.1:8780').includes('port'));
  assert.ok(kinds('localhost').includes('port'));
  assert.deepEqual(kinds('Serving on 8801'), ['port']);
  assert.deepEqual(kinds('panel port 9133'), ['port']);
  // control: a number that is not one of the panel's ports
  assert.deepEqual(kinds('8780000 gold'), []);
  assert.deepEqual(kinds('Stack of 8'), []);
});

test('the panel\'s version and ports are read from its source', () => {
  const src = '__version__ = "2.0.0"\nPORT = 8780\nPORT_CANDIDATES = [8780, 8801, 8899]\n';
  assert.deepEqual(panelRuntime(src), { version: '2.0.0', ports: ['8780', '8801', '8899'] });
  assert.deepEqual(panelRuntime(''), { version: '', ports: [] });
  const real = new URL('../../src/forgepact.py', import.meta.url);
  const got = panelRuntime(readFileSync(real, 'utf8'));
  assert.match(got.version, /^\d+\.\d+\.\d+$/);
  assert.ok(got.ports.includes('8780'), got.ports.join(','));
});

test('an entry is scoped by its component\'s selector; a bad component is an error', () => {
  const exp = {
    components: [{ name: 'EnabledMods', selector: '#enabledMods' }, { name: 'RailTab' }],
    selectorTokens: [
      { selector: '.enabled-mod', property: 'color', variable: 'v', component: 'EnabledMods' },
      { selector: '.enabled-mod', property: 'background-color', variable: 'w', component: 'EnabledMods' },
      { selector: '.tabbtn', property: 'color', variable: 'v', component: 'RailTab' },
      { selector: '.val', property: 'color', variable: 'v', component: 'NoSuch' },
      { selector: 'body', property: 'color', variable: 'v' },
    ],
  };
  const { scoped, unscoped, errors } = scopedEntries(exp);
  assert.deepEqual(scoped, [{ selector: '.enabled-mod', component: 'EnabledMods', within: '#enabledMods' }]);
  assert.equal(unscoped, 1);
  assert.equal(errors.length, 2);
  assert.match(errors[0], /RailTab" has no selector/);
  assert.match(errors[1], /NoSuch" is not in the export's components/);
});

test('the real export\'s texts hold no runtime value', { skip: !existsSync(new URL('../design/figma-export.json', import.meta.url)) }, () => {
  const exp = JSON.parse(readFileSync(new URL('../design/figma-export.json', import.meta.url), 'utf8'));
  const known = panelRuntime(readFileSync(new URL('../../src/forgepact.py', import.meta.url), 'utf8'));
  const hits = exp.screens.flatMap((s) => [...(s.texts || []), ...(s.placeholders || [])]
    .flatMap((t) => runtimeValues(t, known).map((h) => `${s.tab}-${s.width}: ${JSON.stringify(t)} ${h.kind}`)));
  assert.deepEqual(hits, []);
});
