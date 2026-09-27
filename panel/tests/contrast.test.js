// scripts/contrast-check.mjs: WCAG 2.x contrast over the export's
// contrastPairs. The maths against known values (black on white is 21,
// #777777 on white is 4.48 and still a fail at 4.5), compositing for a
// translucent fg and bg, the refusal when a translucent bg has no surface,
// and the CLI against the two fixture exports: one that passes in all three
// palettes and one planted to fail.
import test from 'node:test';
import assert from 'node:assert/strict';
import { spawnSync } from 'node:child_process';
import { fileURLToPath } from 'node:url';
import { checkContrast, composite, contrastRatio, parseHex } from '../scripts/contrast-check.mjs';

const path = (rel) => fileURLToPath(new URL(rel, import.meta.url));
const SCRIPT = path('../scripts/contrast-check.mjs');

const exportOf = (colours, pairs) => ({
  palettes: [{ name: 'one', default: true }],
  variables: Object.entries(colours).map(([name, value]) => ({ collection: 'tokens', mode: 'one', name, type: 'COLOR', value })),
  contrastPairs: pairs,
});

test('known ratios: black on white is 21, #777777 on white is 4.48', () => {
  assert.equal(contrastRatio('#000000', '#ffffff'), 21);
  assert.equal(contrastRatio('#ffffff', '#000000'), 21, 'order does not matter');
  assert.equal(contrastRatio('#777777', '#ffffff').toFixed(2), '4.48');
  assert.ok(contrastRatio('#777777', '#ffffff') < 4.5);
  assert.equal(contrastRatio('#123456', '#123456'), 1);
});

test('an fg with alpha is composited over its bg first', () => {
  // 0x80 black over white leaves 255 * (1 - 128/255) = 127 = 0x7f per channel.
  const blended = composite(parseHex('#00000080'), parseHex('#ffffff'));
  assert.deepEqual([blended.r, blended.g, blended.b].map((c) => Math.round(c * 1e6) / 1e6), [127, 127, 127]);
  assert.ok(Math.abs(contrastRatio('#00000080', '#ffffff') - contrastRatio('#7f7f7f', '#ffffff')) < 1e-9);
  assert.equal(contrastRatio('#00000080', '#ffffff').toFixed(2), '4.00');
});

test('the unrounded ratio decides: 4.48 at 2 dp still fails a 4.48 minimum', () => {
  const report = checkContrast(exportOf({ 'color/text/primary': '#777777', 'color/bg/base': '#ffffff' },
    [{ fg: 'color/text/primary', bg: 'color/bg/base', minRatio: 4.48, where: 'rounding' }]));
  assert.equal(report.results[0].ratio.toFixed(2), '4.48');
  assert.equal(report.results[0].pass, false);
  assert.equal(report.failing, 1);
});

test('a bg with alpha is composited over color/bg/base, and is an error without it', () => {
  const pair = [{ fg: 'color/text/primary', bg: 'color/bg/raised', minRatio: 4.5, where: 'raised' }];
  const withBase = checkContrast(exportOf({ 'color/text/primary': '#000000', 'color/bg/raised': '#00000080', 'color/bg/base': '#ffffff' }, pair));
  assert.ok(Math.abs(withBase.results[0].ratio - contrastRatio('#000000', '#7f7f7f')) < 1e-9);
  const without = checkContrast(exportOf({ 'color/text/primary': '#000000', 'color/bg/raised': '#00000080' }, pair));
  assert.equal(without.results[0].ratio, null);
  assert.match(without.results[0].error, /color\/bg\/base is absent/);
  assert.equal(without.failing, 1);
});

test('a palette uses its own colours over the default palette', () => {
  const exp = exportOf({ 'color/text/primary': '#000000', 'color/bg/base': '#ffffff' },
    [{ fg: 'color/text/primary', bg: 'color/bg/base', minRatio: 4.5, where: 'body' }]);
  exp.palettes.push({ name: 'two', default: false });
  exp.variables.push({ collection: 'tokens', mode: 'two', name: 'color/text/primary', type: 'COLOR', value: '#fefefe' });
  const report = checkContrast(exp);
  assert.deepEqual(report.results.map((r) => [r.palette, r.pass]), [['one', true], ['two', false]]);
});

test('CLI: the passing fixture exits 0, the planted one exits 1 with FAIL lines', () => {
  const ok = spawnSync(process.execPath, [SCRIPT, path('./fixtures/design/export-mini.json')], { encoding: 'utf8' });
  assert.equal(ok.status, 0, ok.stdout + ok.stderr);
  const okLines = ok.stdout.trim().split(/\r?\n/);
  assert.ok(!okLines.some((l) => l.startsWith('FAIL')));
  assert.match(okLines.at(-1), /^contrast: \d+ pairs x 3 palettes, 0 failing$/);

  const bad = spawnSync(process.execPath, [SCRIPT, path('./fixtures/design/export-wrong.json')], { encoding: 'utf8' });
  assert.equal(bad.status, 1);
  const badLines = bad.stdout.trim().split(/\r?\n/);
  assert.ok(badLines.some((l) => l.startsWith('FAIL')));
  assert.match(badLines.at(-1), /^contrast: \d+ pairs x \d+ palettes, [1-9][0-9]* failing$/);

  const missing = spawnSync(process.execPath, [SCRIPT, path('./fixtures/design/no-such-export.json')], { encoding: 'utf8' });
  assert.equal(missing.status, 1);
});
