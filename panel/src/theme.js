// The panel's themes and the one function that paints one. The chosen name
// is saved in forgepact.json (`theme`, by src/panel.js's change handler like
// every other setting - pywebview's page storage is private to it) and painted as
// data-theme on the root element, where tokens.css picks it up. Pure enough to
// import under plain node: tests/theme.test.js and tests/oracle-derive.mjs
// both read THEMES from here.
//
// The first three palettes retain their Figma export order. Ledger supplies
// tokens.css's base :root colours. Ember adds a scoped local palette and is
// the fallback for configurations that have not explicitly chosen a theme.
// Changing this list also requires `npm run oracle:derive`.
export const THEMES = [
  { value: 'ledger', label: 'Ledger' },
  { value: 'graphite', label: 'Graphite' },
  { value: 'sigil', label: 'Sigil' },
  { value: 'ember', label: 'Ember Forge' },
];

// A saved name this build does not know (the backend's `default`, a newer
// panel's palette, a hand edit) paints as the default rather than as no theme
// at all.
export const DEFAULT_THEME = 'ember';

export function themeName(name) {
  return THEMES.some((theme) => theme.value === name) ? name : DEFAULT_THEME;
}

// Sets document.documentElement.dataset.theme - always, the default included -
// and returns the name it painted.
export function applyTheme(name) {
  const painted = themeName(name);
  if (typeof document !== 'undefined' && document.documentElement && document.documentElement.dataset.theme !== painted) {
    document.documentElement.dataset.theme = painted;
    document.dispatchEvent?.(new Event('forgepact:theme'));
  }
  return painted;
}
