// The "Enabled mods" list under the control dock: every mod the saved config
// has on (src/enabled-mods.js decides which), in the order the controls sit
// in the page, each with a "Turn off" button.
//
// Nothing here writes a setting. "Turn off" sets the mod's own control off and
// dispatches `change` on it, so the control's own handler posts exactly what a
// click on it would have, and repaints whatever it repaints (the map-reveal
// switch greys out its children). The list is rebuilt from the saved config
// by refreshSavedControls() once that write has drained, and by boot().
//
// An entry's name is its row's label and its value the row's value box, read
// from the page rather than copied, so a label or a value format lives in one
// place.
import { enabledControls } from '../enabled-mods.js';

// Density's switch sits in a heading row of its own; its label and value are
// on the multiplier's row.
const ROW_OF = { den_on: 'den' };

function rowOf(control) {
  const anchor = ROW_OF[control.id] ? document.getElementById(ROW_OF[control.id]) : control;
  return anchor?.closest('.row') || null;
}

// The label's first piece of text: the title, before a tag or a description,
// and past the icon the panel prepends to it.
function nameOf(row) {
  const label = row?.querySelector('.lbl');
  if (!label) return '';
  const walker = document.createTreeWalker(label, NodeFilter.SHOW_TEXT);
  for (let node = walker.nextNode(); node; node = walker.nextNode()) {
    const text = node.textContent.trim();
    if (text) return text;
  }
  return '';
}

function valueOf(row, control) {
  const value = row?.querySelector('.val');
  if (value) return value.textContent.trim();
  return control.matches('select') ? (control.selectedOptions[0]?.textContent.trim() || '') : '';
}

export function quickDisable(control) {
  if (!control) return;
  if (control.matches('select')) control.value = 'off';
  else control.checked = false;
  control.dispatchEvent(new Event('change', { bubbles: true }));
}

export function renderEnabledMods(cfg) {
  const box = document.getElementById('enabledMods');
  if (!box) return;
  const controls = enabledControls(cfg)
    .map((id) => document.getElementById(id))
    .filter(Boolean)
    .sort((a, b) => (a.compareDocumentPosition(b) & Node.DOCUMENT_POSITION_FOLLOWING ? -1 : 1));
  document.getElementById('enabledModsCount').textContent = `${controls.length} on`;
  box.querySelectorAll(':scope > ul, :scope > .enabled-mods-empty').forEach((el) => el.remove());
  if (!controls.length) {
    const empty = document.createElement('p');
    empty.className = 'enabled-mods-empty';
    empty.textContent = 'Nothing is on';
    box.append(empty);
    return;
  }
  const list = document.createElement('ul');
  for (const control of controls) {
    const row = rowOf(control);
    const name = nameOf(row);
    const item = document.createElement('li');
    item.className = 'enabled-mod';
    item.dataset.for = control.id;
    const nameEl = document.createElement('span');
    nameEl.className = 'enabled-mod-name';
    nameEl.textContent = name;
    const valueEl = document.createElement('span');
    valueEl.className = 'enabled-mod-value';
    valueEl.textContent = valueOf(row, control);
    // No id: the behaviour oracle enumerates `button[id]`, and this button is
    // a second way to reach a control it already exercises.
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'quick-disable';
    button.dataset.for = control.id;
    button.setAttribute('aria-label', `Turn off ${name}`);
    button.textContent = 'Turn off';
    button.onclick = () => quickDisable(document.getElementById(button.dataset.for));
    item.append(nameEl, valueEl, button);
    list.append(item);
  }
  box.append(list);
}
