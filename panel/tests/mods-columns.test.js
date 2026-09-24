// The Mods tab's column balancer (src/mods-columns.js) against a stub DOM:
// cards keep their order and the left column is the taller one.
// tests/test_mods_columns.py runs the same function through node from Python.
import test from 'node:test';
import assert from 'node:assert/strict';
import { setupModsColumns } from '../src/mods-columns.js';

let observerCallback = null;
globalThis.ResizeObserver = class { constructor(cb) { observerCallback = cb; } observe() {} };
const el = (height = 0) => ({
  children: [], height, hidden: false, offsetParent: {}, parent: null, className: '',
  append(...nodes) {
    for (const n of nodes) {
      if (n.parent) n.parent.children.splice(n.parent.children.indexOf(n), 1);
      n.parent = this;
      this.children.push(n);
    }
  },
  getBoundingClientRect() { return { height: this.height }; },
});
globalThis.document = { createElement: () => el() };

function layout(heights, { width = 900, hidden = [] } = {}) {
  const grid = el();
  grid.clientWidth = width;
  const items = heights.map((h, i) => {
    const it = el(h);
    it.id = i;
    if (hidden.includes(i)) { it.hidden = true; it.offsetParent = null; }
    return it;
  });
  grid.append(...items);
  setupModsColumns(grid);
  observerCallback();
  return grid.children.map((c) => c.children.map((x) => x.id));
}

test('equal cards split evenly, an odd one goes left', () => {
  assert.deepEqual(layout([100, 100, 100, 100]), [[0, 1], [2, 3]]);
  assert.deepEqual(layout([100, 100, 100]), [[0, 1], [2]]);
});

test('the left column is never the shorter one', () => {
  assert.deepEqual(layout([100, 100, 100, 400]), [[0, 1, 2, 3], []]);
  assert.deepEqual(layout([400, 100, 100, 100]), [[0], [1, 2, 3]]);
});

test('hidden cards weigh nothing and an unlaid grid is left alone', () => {
  assert.deepEqual(layout([100, 100, 500, 100, 100], { hidden: [2] }), [[0, 1], [2, 3, 4]]);
  assert.deepEqual(layout([100, 100, 100, 100], { width: 0 }), [[0, 1, 2, 3], []]);
});
