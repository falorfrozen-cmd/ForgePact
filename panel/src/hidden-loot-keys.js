// "Show hidden loot while held" (Sleep loot your filter hides): the keys and
// mouse buttons the select offers, as [Windows virtual-key code, name], in the
// order it lists them. src/forgepact.py's HIDDEN_LOOT_KEYS is the same list,
// code for code and name for name (tests/test_hidden_loot_panel_contract.py),
// and /api/set refuses any other code. Left out: the left and right mouse
// buttons (the game's own), and generic Alt, Right Alt and F10, which can put
// the game window into its menu mode. Plain JS, so the parity test runs it
// under node.

const LETTERS = 'ABCDEFGHIJKLMNOPQRSTUVWXYZ';
const DIGITS = '0123456789';

export const HIDDEN_LOOT_KEYS = [
  [0, 'None'],
  [164, 'Left Alt'],
  [17, 'Ctrl'], [16, 'Shift'], [9, 'Tab'], [20, 'Caps Lock'], [32, 'Space'], [192, 'Backquote'],
  [4, 'Middle mouse'], [5, 'Mouse 4'], [6, 'Mouse 5'],
  ...Array.from({ length: 9 }, (_, i) => [112 + i, 'F' + (i + 1)]), [122, 'F11'], [123, 'F12'],
  ...[...LETTERS].map((c) => [c.charCodeAt(0), c]),
  ...[...DIGITS].map((c) => [c.charCodeAt(0), c]),
];

// The saved default, Left Alt (the owner's choice, 2026-09-28).
export const HIDDEN_LOOT_KEY_DEFAULT = 164;
