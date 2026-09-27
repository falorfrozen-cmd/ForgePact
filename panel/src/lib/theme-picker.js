// The Setup tab's theme choice, drawn as the design's ThemePicker (owner,
// 2026-09-26, finish review F2: "Build the ThemePicker"): a compact trigger
// with the palette's name, and a listbox of the palettes, each with three
// swatches of its own colours. The native select #theme stays underneath as the
// control of record, hidden by app.css but never removed: the derived oracle,
// the browser suites and panel.js's change handler all drive it. Choosing a
// palette here sets the select's value and dispatches `change`, only when the
// value really changed, as a native select does; panel.js then posts and
// paints exactly as before. Nothing here posts or stores anything.
//
// Keyboard (the WAI-ARIA listbox pattern): Enter, Space, ArrowDown and
// ArrowUp on the trigger open the list; inside it the arrows, Home and End
// move, a letter jumps to the palette it starts, Enter and Space choose,
// Escape closes with no change, and Tab closes and moves on. Choosing or
// Escape returns focus to the trigger. A pointer outside closes the list, and
// so does a tab change. Opened or closed from the keyboard it appears and goes
// at once; the pointer's open scales it in from the trigger (app.css, motion
// row M5), which is why the list carries data-instant except during a
// pointer's own open or close.
//
// The trigger follows the select: its `change` events, and the root's
// data-theme, which applyTheme() writes at boot and after every save. Both
// write only a change, so a repaint that paints the same palette mutates
// nothing.
import { THEMES } from '../theme.js';

// The three swatch colours, in the order the export's theme-picker state
// names them.
export const SWATCH_TOKENS = ['--color-border-strong', '--color-text-muted', '--color-accent'];

// Each palette's swatch colours, from tokens.css's own rules: `:root` holds
// the default palette (THEMES[0]) and `:root[data-theme="<name>"]` every
// other one. `rules` is a list of { selectorText, style } (CSSStyleRule, or a
// test's stand-in); a later rule wins, as in the cascade. A palette whose
// value is missing gets '' for it, never another palette's colour.
export function swatchesFromRules(rules, themes = THEMES) {
  const found = new Map();
  for (const rule of rules) {
    const selector = String(rule?.selectorText || '').replace(/\s+/g, '');
    const m = /^:root(?:\[data-theme=(["']?)([\w-]+)\1\])?$/.exec(selector);
    if (!m) continue;
    const name = m[2] || themes[0].value;
    const got = found.get(name) || {};
    for (const token of SWATCH_TOKENS) {
      const value = rule.style?.getPropertyValue?.(token)?.trim();
      if (value) got[token] = value;
    }
    found.set(name, got);
  }
  return Object.fromEntries(themes.map((t) => [t.value, SWATCH_TOKENS.map((token) => found.get(t.value)?.[token] || '')]));
}

// The option the arrows, Home and End land on; null for any other key.
// Clamped at both ends, as a listbox does.
export function nextIndex(key, index, count) {
  if (key === 'ArrowDown') return Math.min(count - 1, index + 1);
  if (key === 'ArrowUp') return Math.max(0, index - 1);
  if (key === 'Home') return 0;
  if (key === 'End') return count - 1;
  return null;
}

function styleRules() {
  const out = [];
  for (const sheet of document.styleSheets) {
    let rules;
    try { rules = sheet.cssRules; } catch { continue; }
    for (const rule of rules) if (rule.selectorText) out.push(rule);
  }
  return out;
}

export function installThemePicker() {
  const select = document.getElementById('theme');
  const host = select?.closest('.theme-picker');
  const trigger = host?.querySelector('.theme-picker-trigger');
  const list = host?.querySelector('.theme-picker-list');
  const value = host?.querySelector('.theme-picker-value');
  if (!select || !host || !trigger || !list || !value) return null;
  const options = [...list.querySelectorAll('.theme-picker-option')];
  let active = 0;
  let typed = '';
  let typedAt = 0;

  const isOpen = () => host.hasAttribute('data-open');
  const labelOf = (name) => (THEMES.find((t) => t.value === name) || THEMES[0]).label;

  function paint(name) {
    const label = labelOf(name);
    if (value.textContent !== label) value.textContent = label;
    for (const option of options) {
      const on = String(option.dataset.value === name);
      if (option.getAttribute('aria-selected') !== on) option.setAttribute('aria-selected', on);
    }
  }

  // Filled on the first open (the stylesheets are certainly applied by then),
  // and again if a palette came back empty.
  let filled = false;
  function fillSwatches() {
    if (filled) return;
    const swatches = swatchesFromRules(styleRules());
    filled = Object.values(swatches).every((colours) => colours.every(Boolean));
    for (const option of options) {
      const chips = option.querySelectorAll('.theme-picker-swatches > span');
      (swatches[option.dataset.value] || []).forEach((colour, i) => { if (chips[i] && colour) chips[i].style.setProperty('--swatch', colour); });
    }
  }

  function activate(index) {
    active = Math.max(0, Math.min(options.length - 1, index));
    options.forEach((option, i) => option.toggleAttribute('data-active', i === active));
    list.setAttribute('aria-activedescendant', options[active].id);
    if (isOpen() && document.documentElement.dataset.theme === 'ember') {
      options[active].scrollIntoView({ block: 'nearest', behavior: 'instant' });
    }
  }

  function setOpen(open, instant) {
    if (open === isOpen()) return;
    list.toggleAttribute('data-instant', instant);
    if (!instant) {
      const settle = (e) => {
        if (e.target !== list || e.propertyName !== 'opacity') return;
        list.removeEventListener('transitionend', settle);
        list.removeEventListener('transitioncancel', settle);
        list.setAttribute('data-instant', '');
      };
      list.addEventListener('transitionend', settle);
      list.addEventListener('transitioncancel', settle);
    }
    host.toggleAttribute('data-open', open);
    trigger.setAttribute('aria-expanded', String(open));
    if (open) {
      // Ember's menu participates in the content pane's scroll flow. Even a
      // short window can show and scroll it without crossing the action bar.
      const pane = document.getElementById('wrap');
      list.style.maxHeight = document.documentElement.dataset.theme === 'ember' && pane
        ? `${Math.max(32, pane.clientHeight - 16)}px` : '';
      fillSwatches();
      activate(Math.max(0, options.findIndex((o) => o.dataset.value === select.value)));
      list.focus({ preventScroll: true });
      if (document.documentElement.dataset.theme === 'ember') {
        list.scrollIntoView({ block: 'nearest', behavior: 'instant' });
      }
    } else {
      list.removeAttribute('aria-activedescendant');
    }
  }

  function choose(name, instant) {
    setOpen(false, instant);
    trigger.focus({ preventScroll: true });
    if (name === select.value) return;
    select.value = name;
    select.dispatchEvent(new Event('change', { bubbles: true }));
  }

  // A click from the keyboard (Enter, Space) has detail 0 and opens at once.
  trigger.addEventListener('click', (e) => setOpen(!isOpen(), e.detail === 0));
  trigger.addEventListener('keydown', (e) => {
    if (e.key !== 'ArrowDown' && e.key !== 'ArrowUp') return;
    e.preventDefault();
    setOpen(true, true);
  });
  list.addEventListener('keydown', (e) => {
    const next = nextIndex(e.key, active, options.length);
    if (next !== null) { e.preventDefault(); activate(next); return; }
    if (e.key === 'Enter' || e.key === ' ') { e.preventDefault(); choose(options[active].dataset.value, true); return; }
    if (e.key === 'Escape') { e.preventDefault(); e.stopPropagation(); setOpen(false, true); trigger.focus({ preventScroll: true }); return; }
    if (e.key === 'Tab') { setOpen(false, true); return; }
    if (e.key.length === 1 && /\S/.test(e.key) && !e.ctrlKey && !e.metaKey && !e.altKey) {
      const now = performance.now();
      typed = (now - typedAt < 700 ? typed : '') + e.key.toLowerCase();
      typedAt = now;
      const hit = options.findIndex((o) => o.textContent.trim().toLowerCase().startsWith(typed));
      if (hit >= 0) activate(hit);
    }
  });
  list.addEventListener('pointermove', (e) => {
    const option = e.target.closest?.('.theme-picker-option');
    if (option) activate(options.indexOf(option));
  });
  list.addEventListener('click', (e) => {
    const option = e.target.closest?.('.theme-picker-option');
    if (option) choose(option.dataset.value, e.detail === 0);
  });
  // Focus leaving the picker (Tab, a click elsewhere that takes focus) closes it.
  host.addEventListener('focusout', (e) => {
    if (isOpen() && !host.contains(e.relatedTarget)) setOpen(false, true);
  });
  document.addEventListener('pointerdown', (e) => {
    if (isOpen() && !host.contains(e.target)) setOpen(false, false);
  }, true);
  const workspace = document.getElementById('workspace');
  if (workspace) new MutationObserver(() => setOpen(false, true)).observe(workspace, { attributes: true, attributeFilter: ['aria-labelledby'] });

  select.addEventListener('change', () => paint(select.value));
  const root = document.documentElement;
  new MutationObserver(() => paint(root.dataset.theme || select.value)).observe(root, { attributes: true, attributeFilter: ['data-theme'] });
  paint(root.dataset.theme || select.value);
  return { open: (instant = true) => setOpen(true, instant), close: (instant = true) => setOpen(false, instant), isOpen };
}
