// scripts/tokens-from-export.mjs: tokens.css from the Figma export. The
// fixture export (tests/fixtures/design/export-mini.json) carries one variable
// per unit rule and three palettes; the expected CSS below is written out by
// hand from the rules, not produced by the generator, so the round trip tests
// the rules rather than the generator agreeing with itself. A planted-
// difference control proves the comparator reports what it should, and the
// real-export test compares the committed src/tokens.css with what the export
// generates - skipped only while panel/design/figma-export.json is absent.
//
// Every path comes from import.meta.url: the criteria run this from the hub
// root, `npm test` from panel/.
import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { existsSync, mkdtempSync, readFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';
import {
  compareTokens, cssValue, defaultPalette, generateTokensCss, loadExport, parseTokensCss, paletteVariables,
} from '../scripts/tokens-from-export.mjs';

const path = (rel) => fileURLToPath(new URL(rel, import.meta.url));
const SCRIPT = path('../scripts/tokens-from-export.mjs');
const MINI = path('./fixtures/design/export-mini.json');
const REAL_EXPORT = path('../design/figma-export.json');
const REAL_TOKENS = path('../src/tokens.css');

// What the unit rules make of export-mini.json, by hand.
const EXPECTED = new Map([
  [':root', new Map([
    ['--color-accent', '#4c9be8'],
    ['--color-accent-ink', '#0a0a0a'],
    ['--color-bg-base', '#101214'],
    ['--color-bg-raised', '#ffffff14'],
    ['--color-text-muted', '#a8a8a8'],
    ['--color-text-primary', '#f0f0f0'],
    ['--font-body-family', '"Fixture Sans", "Segoe UI", system-ui, sans-serif'],
    ['--font-line-height-tight', '1.2'],
    ['--font-mono-family', '"Fixture Mono"'],
    ['--font-size-md', '14px'],
    ['--font-weight-semibold', '600'],
    ['--line-height-base', '1.5'],
    ['--motion-duration-fast', '120ms'],
    ['--motion-easing-standard', 'cubic-bezier(0.2, 0, 0, 1)'],
    ['--radius-md', '6px'],
    ['--shadow-raised', '0 1px 2px #00000066'],
    ['--space-2', '8px'],
  ])],
  [':root[data-theme="beta"]', new Map([
    ['--color-accent', '#1f5fa8'],
    ['--color-accent-ink', '#ffffff'],
    ['--color-bg-base', '#fafafa'],
    ['--color-bg-raised', '#0000000d'],
    ['--color-text-muted', '#595959'],
    ['--color-text-primary', '#1a1a1a'],
  ])],
  [':root[data-theme="gamma"]', new Map([
    ['--color-accent', '#d98cff'],
    ['--color-accent-ink', '#1b1024'],
    ['--color-bg-base', '#1b1024'],
    ['--color-bg-raised', '#ffffff1a'],
    ['--color-text-muted', '#b9a9c9'],
    ['--color-text-primary', '#f4eefa'],
  ])],
]);

test('the fixture export generates every variable with its unit, one block per palette', () => {
  const css = generateTokensCss(loadExport(MINI));
  const parsed = parseTokensCss(css);
  assert.deepEqual([...parsed.keys()], [...EXPECTED.keys()], 'blocks, in export order, the default as :root');
  assert.deepEqual(compareTokens(EXPECTED, parsed), []);
  // Exact text, not only equal after parsing: the family keeps its quotes.
  for (const [prop, value] of EXPECTED.get(':root')) assert.ok(css.includes(`  ${prop}: ${value};\n`), prop);
  // Sorted by variable name within a block.
  for (const decls of parsed.values()) {
    const names = [...decls.keys()];
    assert.deepEqual(names, [...names].sort(), 'block not sorted');
  }
  // The default palette has no data-theme block of its own.
  assert.equal(defaultPalette(loadExport(MINI)).name, 'alpha');
  assert.ok(!css.includes('data-theme="alpha"'));
});

test('a palette resolves to its own colours over the default palette, and keeps the default non-colours', () => {
  const beta = paletteVariables(loadExport(MINI), 'beta');
  assert.equal(beta.get('color/bg/base').value, '#fafafa');
  assert.equal(beta.get('space/2').value, 8);
  assert.equal(paletteVariables(loadExport(MINI), 'alpha').get('color/bg/base').value, '#101214');
});

test('values with no rule, bad colours and a missing or doubled default are errors, not guesses', () => {
  assert.throws(() => cssValue({ name: 'opacity/disabled', type: 'FLOAT', value: 0.4 }), /no unit rule/);
  assert.throws(() => cssValue({ name: 'color/bg/base', type: 'COLOR', value: 'red' }), /not #rrggbb/);
  assert.throws(() => cssValue({ name: 'space/1', type: 'FLOAT', value: 'x' }), /not a number/);
  assert.equal(cssValue({ name: 'line-height/tight', type: 'FLOAT', value: 1.2000000476837158 }), '1.2');
  assert.equal(cssValue({ name: 'font/mono/family', type: 'STRING', value: 'Mono One, monospace' }), '"Mono One", monospace');
  const exp = loadExport(MINI);
  assert.throws(() => generateTokensCss({ ...exp, palettes: exp.palettes.map((p) => ({ ...p, default: true })) }), /exactly one default/);
  assert.throws(() => generateTokensCss({ ...exp, palettes: exp.palettes.map((p) => ({ ...p, default: false })) }), /exactly one default/);
});

// The buildout export names its line heights font/line-height/<step>, since its
// name pattern allows no hyphen in the first segment; line-height/ stays for
// any export that uses it. Near misses still have no rule.
test('a FLOAT under font/line-height/ is unitless like line-height/, and a near miss is still an error', () => {
  assert.equal(cssValue({ name: 'font/line-height/tight', type: 'FLOAT', value: 1.2000000476837158 }), '1.2');
  assert.equal(cssValue({ name: 'font/line-height/base', type: 'FLOAT', value: 1.4500000476837158 }), '1.45');
  assert.equal(cssValue({ name: 'line-height/tight', type: 'FLOAT', value: 1.2000000476837158 }), '1.2');
  for (const name of ['font/line-height', 'font/line-heights/tight', 'font/lineheight/tight', 'type/font/line-height/base']) {
    assert.throws(() => cssValue({ name, type: 'FLOAT', value: 1.2 }), /no unit rule/, name);
  }
});

test('negative control: the comparator reports every planted difference and nothing else', () => {
  const css = generateTokensCss(loadExport(MINI));
  const planted = css
    .replace('--space-2: 8px;', '--space-2: 9px;')
    .replace('  --radius-md: 6px;\n', '')
    .replace('--font-mono-family: "Fixture Mono";', '--font-mono-family: "Other Mono";')
    // A stack that only grew fallbacks is the same family: not a difference.
    .replace('--font-body-family: "Fixture Sans", "Segoe UI", system-ui, sans-serif;', '--font-body-family: "Fixture Sans", sans-serif;')
    // Colours compare case-insensitively.
    .replace('--color-accent: #4c9be8;', '--color-accent: #4C9BE8;')
    .replace(':root[data-theme="beta"] {\n', ':root[data-theme="beta"] {\n  --color-extra: #123456;\n')
    .replace(':root[data-theme="gamma"]', ':root[data-theme="delta"]');
  assert.notEqual(planted, css);
  const diffs = compareTokens(parseTokensCss(css), parseTokensCss(planted));
  const key = (d) => [d.kind, d.selector, d.property].filter(Boolean).join(' ');
  assert.deepEqual(diffs.map(key).sort(), [
    'different :root --font-mono-family',
    'different :root --space-2',
    'extra :root[data-theme="beta"] --color-extra',
    'extra-block :root[data-theme="delta"]',
    'missing :root --radius-md',
    'missing-block :root[data-theme="gamma"]',
  ]);
  assert.equal(diffs.find((d) => d.property === '--space-2').actual, '9px');
});

test('the CLI writes the same CSS to --out as it prints', () => {
  const dir = mkdtempSync(join(tmpdir(), 'tokens-'));
  try {
    const out = join(dir, 'tokens.css');
    const run = spawnSync(process.execPath, [SCRIPT, MINI, '--out', out], { encoding: 'utf8' });
    assert.equal(run.status, 0, run.stderr);
    assert.equal(readFileSync(out, 'utf8'), generateTokensCss(loadExport(MINI)));
    const bad = spawnSync(process.execPath, [SCRIPT, join(dir, 'no-such-export.json')], { encoding: 'utf8' });
    assert.equal(bad.status, 1);
  } finally {
    rmSync(dir, { recursive: true, force: true });
  }
});

test('the committed src/tokens.css is what the real export generates',
  { skip: !existsSync(REAL_EXPORT) && 'panel/design/figma-export.json is not there yet; the restyle copies it in' },
  () => {
    assert.ok(existsSync(REAL_TOKENS), 'panel/design/figma-export.json exists but src/tokens.css does not');
    const expected = parseTokensCss(generateTokensCss(loadExport(REAL_EXPORT)));
    assert.deepEqual(compareTokens(expected, parseTokensCss(readFileSync(REAL_TOKENS, 'utf8'))), []);
  });
