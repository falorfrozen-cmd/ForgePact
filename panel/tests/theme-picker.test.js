// src/lib/theme-picker.js's pure half: the swatch colours read from
// tokens.css's rules, and the listbox's arrow, Home and End moves. The picker
// itself (trigger, list, the native select it drives) is e2e:finish's.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { SWATCH_TOKENS, nextIndex, swatchesFromRules } from '../src/lib/theme-picker.js';
import { THEMES } from '../src/theme.js';

// A stand-in for a CSSStyleRule: its selector and getPropertyValue().
const rule = (selectorText, values) => ({ selectorText, style: { getPropertyValue: (name) => values[name] ?? '' } });

// tokens.css's own blocks, parsed the plain way: `:root { ... }` and
// `:root[data-theme="<name>"] { ... }`, custom properties only.
function tokenRules() {
  const css = readFileSync(new URL('../src/tokens.css', import.meta.url), 'utf8');
  return [...css.matchAll(/(:root(?:\[[^\]]+\])?)\s*\{([^}]*)\}/g)].map(([, selector, body]) =>
    rule(selector, Object.fromEntries([...body.matchAll(/(--[\w-]+)\s*:\s*([^;]+);/g)].map(([, k, v]) => [k, v.trim()]))));
}

test('every palette in THEMES gets its three swatch colours from tokens.css, and no two palettes share all three', () => {
  const got = swatchesFromRules(tokenRules());
  assert.deepEqual(Object.keys(got), THEMES.map((t) => t.value));
  for (const [name, colours] of Object.entries(got)) {
    assert.equal(colours.length, SWATCH_TOKENS.length, name);
    for (const c of colours) assert.match(c, /^#[0-9a-f]{3,8}$/i, `${name}: ${c}`);
  }
  assert.equal(new Set(Object.values(got).map((c) => c.join())).size, THEMES.length);
});

test('the default palette is :root, each other one its data-theme block; a later rule wins', () => {
  const got = swatchesFromRules([
    rule(':root', { '--color-border-strong': '#111', '--color-text-muted': '#222', '--color-accent': '#333' }),
    rule(':root', { '--color-accent': '#334' }),
    rule(':root[data-theme="graphite"]', { '--color-border-strong': '#444', '--color-text-muted': '#555', '--color-accent': '#666' }),
    rule(':root[data-theme=sigil]', { '--color-border-strong': '#777', '--color-text-muted': '#888', '--color-accent': '#999' }),
  ]);
  assert.deepEqual(got, { ledger: ['#111', '#222', '#334'], graphite: ['#444', '#555', '#666'], sigil: ['#777', '#888', '#999'] });
});

test('a palette with no block gets empty swatches, never another palette\'s colours (negative control)', () => {
  const got = swatchesFromRules([
    rule(':root', { '--color-border-strong': '#111', '--color-text-muted': '#222', '--color-accent': '#333' }),
    rule('.card', { '--color-accent': '#f00' }),
    rule(':root[data-theme="graphite"] .card', { '--color-accent': '#0f0' }),
  ]);
  assert.deepEqual(got.graphite, ['', '', '']);
  assert.deepEqual(got.sigil, ['', '', '']);
  assert.deepEqual(got.ledger, ['#111', '#222', '#333']);
});

test('arrows move one and stop at the ends; Home and End jump; any other key moves nothing', () => {
  assert.equal(nextIndex('ArrowDown', 0, 3), 1);
  assert.equal(nextIndex('ArrowDown', 2, 3), 2);
  assert.equal(nextIndex('ArrowUp', 1, 3), 0);
  assert.equal(nextIndex('ArrowUp', 0, 3), 0);
  assert.equal(nextIndex('Home', 2, 3), 0);
  assert.equal(nextIndex('End', 0, 3), 2);
  for (const key of ['Enter', ' ', 'Escape', 'Tab', 'a', 'ArrowLeft']) assert.equal(nextIndex(key, 1, 3), null, key);
});
