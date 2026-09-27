// The panel's themes and the one function that paints one. The chosen name
// is saved in forgepact.json (`theme`, by src/panel.js's change handler like
// every other setting - pywebview's page storage is private to it) and painted as
// data-theme on the root element, where tokens.css picks it up. Pure enough to
// import under plain node: tests/theme.test.js and tests/oracle-derive.mjs
// both read THEMES from here.
//
// The list is the design's palettes, in the order of design/figma-export.json's
// `palettes` (values and labels). The first is the default: its colours are
// tokens.css's `:root` block, so it needs no `data-theme` block of its own,
// and the backend's `"default"` (a config that never chose a theme) paints it
// through themeName()'s fallback. Renaming one is a change to this file and a
// re-run of `npm run oracle:derive`.
export const THEMES = [
  { value: 'ledger', label: 'Ledger' },
  { value: 'graphite', label: 'Graphite' },
  { value: 'sigil', label: 'Sigil' },
];

// A saved name this build does not know (the backend's `default`, a newer
// panel's palette, a hand edit) paints as the default rather than as no theme
// at all.
export function themeName(name) {
  return THEMES.some((theme) => theme.value === name) ? name : THEMES[0].value;
}

// Sets document.documentElement.dataset.theme - always, the default included -
// and returns the name it painted.
export function applyTheme(name) {
  const painted = themeName(name);
  if (typeof document !== 'undefined' && document.documentElement) document.documentElement.dataset.theme = painted;
  return painted;
}
