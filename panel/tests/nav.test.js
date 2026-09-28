// Tab and sub-tab navigation (src/nav.js) against a stub DOM. The module's
// functions are evaluated from its source with the imports and `export`
// prefixes dropped, the way tests/panel_source.py's js_for_node() does, so
// filterControlRows can be a counting stub instead of the whole panel.
// tests/test_mods_categories.py drives the same functions from Python.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const source = readFileSync(new URL('../src/nav.js', import.meta.url), 'utf8')
  .replace(/^[ \t]*import\b[^;]*;[ \t]*\n?/gm, '')
  .replace(/(^|[\s;{}])export (?=(?:async\s+)?function\b|const\b|let\b)/gm, '$1');

function stubDom() {
  const all = [];
  const make = (attrs) => {
    const el = {
      id: attrs.id || '', className: attrs.class || '', hidden: !!attrs.hidden, tabIndex: 0, value: '', textContent: '',
      dataset: {}, attrs: {}, onclick: null, onkeydown: null,
      get classList() {
        const self = this;
        const set = () => new Set(self.className.split(/\s+/).filter(Boolean));
        return {
          contains: (c) => set().has(c),
          toggle: (c, on) => { const s = set(); if (on) s.add(c); else s.delete(c); self.className = [...s].join(' '); },
        };
      },
      getAttribute(n) { return n in this.attrs ? this.attrs[n] : null; },
      setAttribute(n, v) { this.attrs[n] = String(v); },
      click() { if (this.onclick) this.onclick(); },
      focus() { document.activeElement = this; },
    };
    for (const [k, v] of Object.entries(attrs)) {
      if (k.startsWith('data-')) el.dataset[k.slice(5)] = v;
      if (!['id', 'class', 'hidden'].includes(k)) el.setAttribute(k, v);
    }
    all.push(el);
    return el;
  };
  const matches = (el, sel) => (sel.match(/\.[\w-]+|\[[^\]]+\]/g) || []).every((tok) => {
    if (tok[0] === '.') return el.classList.contains(tok.slice(1));
    const [, name, value] = tok.match(/^\[([\w-]+)="([^"]*)"\]$/);
    return el.getAttribute(name) === value;
  });
  const document = {
    activeElement: null,
    getElementById: (id) => all.find((e) => e.id === id) || null,
    querySelector: (sel) => all.find((e) => matches(e, sel)) || null,
    querySelectorAll: (sel) => all.filter((e) => matches(e, sel)),
  };
  for (const tab of ['setup', 'modifiers', 'world', 'loot', 'mods']) make({ class: 'tabbtn', id: 'nav-' + tab, 'data-tab': tab });
  make({ class: 'tab-card', id: 'setupCard', 'data-tab': 'setup' });
  make({ class: 'tab-card', id: 'dropsCard', 'data-tab': 'loot' });
  make({ class: 'tab-card', id: 'qolCard', 'data-tab': 'mods' });
  make({ class: 'tab-card', id: 'itemsCard', 'data-tab': 'mods' });
  for (const id of ['workspace', 'pageTitle', 'pageDescription', 'breadcrumbPage', 'controlToolbar', 'controlSearch']) make({ id });
  make({ id: 'modsSubtabs', hidden: true });
  make({ class: 'subtabbtn', id: 'subtab-qol', 'aria-controls': 'qolCard' });
  make({ class: 'subtabbtn', id: 'subtab-items', 'aria-controls': 'itemsCard' });
  return document;
}

function load() {
  const document = stubDom();
  const store = {};
  const sessionStorage = { getItem: (k) => (k in store ? store[k] : null), setItem: (k, v) => { store[k] = String(v); } };
  const window = { scrollTo() {} };
  let filtered = 0;
  const filterControlRows = () => { filtered++; };
  const api = new Function('document', 'sessionStorage', 'window', 'filterControlRows',
    `${source}\nreturn {openTab,openModsSubtab,bindModsSubtabs,PAGE_INFO,state:()=>({activeTab,controlFilter,modsSubtab})};`)(
    document, sessionStorage, window, filterControlRows);
  return { ...api, document, store, filtered: () => filtered };
}

test('openTab selects one tab, titles the page and remembers it', () => {
  const nav = load();
  nav.openTab('loot');
  const $ = (id) => nav.document.getElementById(id);
  assert.equal($('nav-loot').getAttribute('aria-selected'), 'true');
  assert.equal($('nav-mods').getAttribute('aria-selected'), 'false');
  assert.equal($('dropsCard').classList.contains('active'), true);
  assert.equal($('setupCard').classList.contains('active'), false);
  assert.equal($('pageTitle').textContent, nav.PAGE_INFO.loot[0]);
  assert.equal($('controlToolbar').hidden, false);
  assert.equal($('modsSubtabs').hidden, true);
  assert.equal(nav.store.forgepact_tab, 'loot');
  assert.equal(nav.filtered(), 1);
});

test('the breadcrumb names the page as its nav button reads', () => {
  const nav = load();
  const $ = (id) => nav.document.getElementById(id);
  nav.openTab('loot');
  assert.equal($('breadcrumbPage').textContent, 'Loot', 'an unlabelled button falls back to the tab name');
  $('nav-modifiers').textContent = ' Character ';
  nav.openTab('modifiers');
  assert.equal($('breadcrumbPage').textContent, 'Character');
});

test('an unknown tab falls back to Modifiers; remember=false stores nothing', () => {
  const nav = load();
  nav.openTab('nope', false);
  assert.equal(nav.state().activeTab, 'modifiers');
  assert.equal(nav.store.forgepact_tab, undefined);
});

test('the Mods sub-tabs switch cards only while Mods is open', () => {
  const nav = load();
  const $ = (id) => nav.document.getElementById(id);
  nav.openTab('mods');
  assert.equal($('qolCard').classList.contains('active'), true);
  nav.openModsSubtab('itemsCard');
  assert.equal($('itemsCard').classList.contains('active'), true);
  assert.equal($('qolCard').classList.contains('active'), false);
  assert.equal(nav.store.forgepact_mods_subtab, 'itemsCard');
  nav.openTab('loot');
  assert.equal($('itemsCard').classList.contains('active'), false);
  nav.openTab('mods');
  assert.equal($('itemsCard').classList.contains('active'), true, 'the chosen sub-tab comes back');
});

test('arrow keys wrap between the sub-tabs and move focus', () => {
  const nav = load();
  const $ = (id) => nav.document.getElementById(id);
  nav.openTab('mods');
  nav.bindModsSubtabs();
  let prevented = false;
  $('subtab-qol').onkeydown({ key: 'ArrowLeft', preventDefault() { prevented = true; } });
  assert.equal(nav.document.activeElement.id, 'subtab-items');
  assert.equal(prevented, true);
  prevented = false;
  $('subtab-items').onkeydown({ key: 'ArrowDown', preventDefault() { prevented = true; } });
  assert.equal(prevented, false, 'an unrelated key is left alone');
});
