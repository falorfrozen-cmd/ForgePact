// src/theme.js: the theme list and applyTheme(), which paints the name as
// data-theme on the root element. Runs without a DOM first (the module must
// import under plain node, as tests/oracle-derive.mjs imports it), then
// against a stub document.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { THEMES, applyTheme, themeName } from '../src/theme.js';

test('without a document, applyTheme still answers the name and throws nothing', () => {
  assert.equal(typeof globalThis.document, 'undefined');
  assert.equal(applyTheme('alt'), 'alt');
});

test('THEMES: default first, every value a name the backend accepts, labels unique', () => {
  assert.equal(THEMES[0].value, 'default');
  assert.ok(THEMES.length >= 2);
  for (const { value, label } of THEMES) {
    assert.match(value, /^[a-z][a-z0-9-]{0,31}$/, value);
    assert.ok(label, value);
  }
  assert.equal(new Set(THEMES.map((t) => t.value)).size, THEMES.length);
  assert.equal(new Set(THEMES.map((t) => t.label)).size, THEMES.length);
});

test('an unknown or missing name falls back to the default', () => {
  assert.equal(themeName('no-such-theme'), 'default');
  assert.equal(themeName(undefined), 'default');
  assert.equal(themeName(''), 'default');
  for (const { value } of THEMES) assert.equal(themeName(value), value);
});

test('applyTheme sets data-theme on the root every time, default included', () => {
  const root = { dataset: {} };
  globalThis.document = { documentElement: root };
  try {
    // Baseline: the page starts with no attribute; the first paint sets it.
    assert.equal(root.dataset.theme, undefined);
    assert.equal(applyTheme('default'), 'default');
    assert.equal(root.dataset.theme, 'default');
    for (const { value } of THEMES) {
      assert.equal(applyTheme(value), value);
      assert.equal(root.dataset.theme, value);
    }
    assert.equal(applyTheme('gone'), 'default');
    assert.equal(root.dataset.theme, 'default');
  } finally {
    delete globalThis.document;
  }
});

test('app.css carries a block for every theme but the default', () => {
  const css = readFileSync(new URL('../src/app.css', import.meta.url), 'utf8');
  for (const { value } of THEMES.slice(1)) {
    const block = css.match(new RegExp(`:root\\[data-theme="${value}"\\]\\{([^}]*)\\}`));
    assert.ok(block, value);
    assert.ok((block[1].match(/--[\w-]+:/g) || []).length >= 2, `${value} overrides fewer than two custom properties`);
  }
});
