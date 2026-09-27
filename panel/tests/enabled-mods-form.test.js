// src/lib/enabled-mods-form.js: when the "Enabled mods" list shows as the
// inline row and when as the tray. The rule is the design's
// (`amendments.enabledMods.threshold` in design/figma-export.json): to the
// tray once the row's natural single-line width is more than the width it
// has, back to the row only once it fits with HYSTERESIS_PX to spare, so a
// window resized across the line does not flicker between the two. Pure, so
// it runs here without a browser; tests/enabled-mods-form.e2e.mjs drives the
// live measurement.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { HYSTERESIS_PX, nextForm } from '../src/lib/enabled-mods-form.js';

const exported = JSON.parse(readFileSync(new URL('../design/figma-export.json', import.meta.url), 'utf8'));
const AVAILABLE = 1216;

test('the band is the export\'s hysteresisPx', () => {
  assert.equal(HYSTERESIS_PX, exported.amendments.enabledMods.threshold.hysteresisPx);
  assert.ok(HYSTERESIS_PX > 0);
});

test('a row that fits stays inline, and so does one that has never been measured', () => {
  assert.equal(nextForm(912, AVAILABLE, 'inline'), 'inline');
  assert.equal(nextForm(912, AVAILABLE, null), 'inline');
  assert.equal(nextForm(0, AVAILABLE, 'inline'), 'inline');
});

test('a row that overfills goes to the tray, and on load too', () => {
  assert.equal(nextForm(AVAILABLE + 200, AVAILABLE, 'inline'), 'tray');
  assert.equal(nextForm(912, 836, null), 'tray');
});

test('in the tray it stays there while inside the band', () => {
  assert.equal(nextForm(AVAILABLE, AVAILABLE, 'tray'), 'tray');
  assert.equal(nextForm(AVAILABLE - 1, AVAILABLE, 'tray'), 'tray');
  assert.equal(nextForm(AVAILABLE - HYSTERESIS_PX + 1, AVAILABLE, 'tray'), 'tray');
  // The same widths from the row keep the row: only the tray has the band.
  assert.equal(nextForm(AVAILABLE - 1, AVAILABLE, 'inline'), 'inline');
});

test('it returns to inline only past the band', () => {
  assert.equal(nextForm(AVAILABLE - HYSTERESIS_PX - 1, AVAILABLE, 'tray'), 'inline');
  assert.equal(nextForm(400, AVAILABLE, 'tray'), 'inline');
});

test('both edges exactly: to the tray on >, back to the row on <=', () => {
  // natural == available fits (toTray is natural > available).
  assert.equal(nextForm(AVAILABLE, AVAILABLE, 'inline'), 'inline');
  assert.equal(nextForm(AVAILABLE + 1, AVAILABLE, 'inline'), 'tray');
  // natural == available - band comes back (toInline is natural <= available - band).
  assert.equal(nextForm(AVAILABLE - HYSTERESIS_PX, AVAILABLE, 'tray'), 'inline');
  assert.equal(nextForm(AVAILABLE - HYSTERESIS_PX + 1, AVAILABLE, 'tray'), 'tray');
});
