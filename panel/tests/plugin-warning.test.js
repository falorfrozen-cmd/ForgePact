// Where the plugin warning's tooltips go (src/lib/plugin-warning.js).
// tooltipPosition is pure: an icon box, the tooltip's size and the window in,
// a box out.
import test from 'node:test';
import assert from 'node:assert/strict';
import { tooltipPosition } from '../src/lib/plugin-warning.js';

const viewport = { width: 1280, height: 800 };
const size = { width: 320, height: 77 };
const box = (left, top, side = 24) => ({ left, top, right: left + side, bottom: top + side });
const overlaps = (icon, at) => at.left < icon.right && at.left + size.width > icon.left &&
  at.top < icon.bottom && at.top + size.height > icon.top;

test('baseline: the status icon at the bottom opens its tooltip above, as the flat themes place it', () => {
  const icon = box(916, 760);
  const at = tooltipPosition(icon, size, viewport, true);
  assert.equal(at.upward, true);
  assert.equal(at.top, 760 - 8 - 77);
  assert.equal(overlaps(icon, at), false);
});

test('baseline: the page-actions icon opens below', () => {
  const icon = box(1200, 600);
  const at = tooltipPosition(icon, size, viewport, false);
  assert.equal(at.upward, false);
  assert.equal(at.top, 624 + 8);
  assert.equal(overlaps(icon, at), false);
});

test('an upward tooltip with no room above flips below instead of covering its icon (Ember status bar at the top)', () => {
  // Measured in Ember at 1280x800: the icon at (916, 13). Clamped upward, the
  // tooltip sat at (916, 12), over the icon, and took its pointer.
  const icon = box(916, 13);
  const at = tooltipPosition(icon, size, viewport, true);
  assert.equal(at.upward, false);
  assert.equal(at.top, 37 + 8);
  assert.equal(overlaps(icon, at), false);
});

test('a downward tooltip with no room below flips above (Ember footer)', () => {
  const icon = box(1220, 760);
  const at = tooltipPosition(icon, size, viewport, false);
  assert.equal(at.upward, true);
  assert.equal(overlaps(icon, at), false);
});

test('with room on neither side it keeps the preferred side, clamped into the window', () => {
  const tiny = { width: 400, height: 100 };
  const icon = box(100, 40);
  const up = tooltipPosition(icon, size, tiny, true), down = tooltipPosition(icon, size, tiny, false);
  assert.equal(up.upward, true);
  assert.equal(down.upward, false);
  for (const at of [up, down]) {
    assert.ok(at.left >= 12 && at.top >= 12);
  }
});
