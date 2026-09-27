// The words the "Enabled mods" list gained with the restyle, and the only
// place they live: the undo toast after a Turn off, what the polite live
// region says, and the joiner in an entry's tooltip. Taken as they are from
// design/figma-export.json (`amendments.undoToast.texts` and
// `amendments.enabledMods.entryTitle.joiner`), which carries the wording the
// owner approved. `{name}` is the entry's name, as the list shows it.
export const UNDO_TEXTS = {
  turnedOff: '{name} turned off',
  undo: 'Undo',
  undoLabel: 'Undo turning off {name}',
  announceOff: '{name} turned off. Undo is available.',
  announceUndo: '{name} turned back on',
};

// "Monster Rarity › Rare": the section's heading, then the entry's name.
export const ENTRY_TITLE_JOINER = ' › ';

export function withName(text, name) {
  return text.replaceAll('{name}', name);
}
