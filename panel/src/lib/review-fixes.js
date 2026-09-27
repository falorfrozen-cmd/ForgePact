// The review's code-side fixes that are not the list's own (those are in
// enabled-mods-form.js and enabled-mods-undo.js). Each reacts to the DOM the
// unchanged handlers in panel.js produce and changes only attributes and
// classes; none posts or stores anything.
//   - idleSwitch: `data-live` on a `label.slider-switch` exactly while its
//     mod is listed in #enabledMods, so an ON switch whose value sits at its
//     default draws neutral (app.css) instead of claiming to do something;
//   - finding 13: each list entry's `title` is its section and its name (the
//     card's heading, or on Mods the sub-tab's label);
//   - finding 9: on Setup, Install Mod Plugin is the primary button while the
//     mod chain is incomplete (#chainnote says so), Launch once it is not;
//   - finding 15: a Satanic pool list with more below carries
//     `data-more-below`, drawn as a fade;
//   - the page scrolls in #wrap now, so a tab change starts it at the top, as
//     openTab()'s window.scrollTo did when the window scrolled;
//   - no card inside a card: preparePanelUI() marks each Mods row
//     `feature-card`, and the design has no card in a card, so the rows are
//     named `feature-row` once it has run;
//   - a disabled child row shows the `title` mods-sync.js gives it
//     ("Enable Reveal full map first.") under the row, as the design draws it.
// Findings 18, 19 and 24 are CSS only (app.css).
import { ENTRY_TITLE_JOINER } from './enabled-mods-copy.js';

// Which Setup button is the primary one.
export function setupEmphasis(chainIncomplete) {
  return { installmod: !!chainIncomplete, launchgame: !chainIncomplete };
}

// "Monster Rarity › Rare", or the name alone when there is no heading or it
// says the same.
export function entryTitle(heading, name) {
  const section = (heading || '').trim();
  return section && section !== name ? section + ENTRY_TITLE_JOINER + name : name;
}

// A heading's first piece of text, past the icon the panel prepends.
function firstText(el) {
  if (!el) return '';
  const walker = document.createTreeWalker(el, NodeFilter.SHOW_TEXT);
  for (let node = walker.nextNode(); node; node = walker.nextNode()) {
    const text = node.textContent.trim();
    if (text) return text;
  }
  return '';
}

// The section an entry's control sits in: its card's heading, or, in a card
// with none (the Mods panels, which no longer repeat their sub-tab's name),
// the label of the sub-tab that shows that panel.
function sectionOf(control) {
  const card = control?.closest('.card');
  const heading = firstText(card?.querySelector('h2'));
  if (heading || !card?.matches('[role="tabpanel"][aria-labelledby]')) return heading;
  return firstText(document.getElementById(card.getAttribute('aria-labelledby')));
}

function markMoreBelow(list) {
  list.toggleAttribute('data-more-below', list.scrollHeight - list.scrollTop - list.clientHeight > 1);
}

export function installReviewFixes() {
  const box = document.getElementById('enabledMods');
  const workspace = document.getElementById('workspace');
  const wrap = document.getElementById('wrap');

  const afterRender = () => {
    const listed = new Set();
    for (const entry of box.querySelectorAll(':scope > ul > li.enabled-mod')) {
      listed.add(entry.dataset.for);
      const control = document.getElementById(entry.dataset.for);
      const name = entry.querySelector('.enabled-mod-name')?.textContent.trim() || '';
      entry.title = entryTitle(sectionOf(control), name);
    }
    for (const label of document.querySelectorAll('label.slider-switch')) {
      label.toggleAttribute('data-live', listed.has(label.querySelector('input')?.id));
    }
    for (const row of document.querySelectorAll('.feature-card')) row.classList.replace('feature-card', 'feature-row');
    for (const list of document.querySelectorAll('.sat-list')) markMoreBelow(list);
    paintHints();
  };
  if (box) new MutationObserver(afterRender).observe(box, { childList: true });

  const install = document.getElementById('installmod');
  const launch = document.getElementById('launchgame');
  const chain = document.getElementById('chainnote');
  if (install && launch && chain) {
    const paint = () => {
      const primary = setupEmphasis(chain.textContent.trim() !== '');
      install.classList.toggle('primary', primary.installmod);
      launch.classList.toggle('primary', primary.launchgame);
    };
    new MutationObserver(paint).observe(chain, { childList: true, characterData: true, subtree: true });
    paint();
  }

  // A child row whose parent is off says which switch it waits for, in the
  // words mods-sync.js already puts in the row's `title` - drawn under the
  // row as the design's disabled state shows it (aria-hidden: the title is
  // already the row's accessible description).
  const paintHints = () => {
    for (const row of document.querySelectorAll('.feature-row, .feature-card')) {
      const box = row.querySelector(':scope > .switch input');
      const text = box?.disabled && row.title ? row.title : '';
      let hint = row.querySelector(':scope > .feature-hint');
      if (!text) { hint?.remove(); continue; }
      if (!hint) {
        hint = document.createElement('span');
        hint.className = 'feature-hint';
        hint.setAttribute('aria-hidden', 'true');
        row.append(hint);
      }
      if (hint.textContent !== text) hint.textContent = text;
    }
  };
  if (workspace) new MutationObserver(paintHints).observe(workspace, { subtree: true, attributes: true, attributeFilter: ['title', 'disabled'] });

  if (workspace) {
    new MutationObserver(() => {
      if (wrap) wrap.scrollTop = 0;
      for (const list of document.querySelectorAll('.sat-list')) markMoreBelow(list);
    }).observe(workspace, { attributes: true, attributeFilter: ['aria-labelledby'] });
    // The pool lists fill from the saved state and refilter as the search changes.
    for (const list of workspace.querySelectorAll('.sat-list')) {
      list.addEventListener('scroll', () => markMoreBelow(list), { passive: true });
      new MutationObserver(() => markMoreBelow(list)).observe(list, { childList: true, subtree: true, attributes: true, attributeFilter: ['hidden'] });
      new ResizeObserver(() => markMoreBelow(list)).observe(list);
    }
  }
}
