// An idle slider's note as a tooltip (owner, 2026-09-25). A slider row is
// idle while its switch is on and its mod is not listed (the switch has no
// data-live, src/lib/review-fixes.js); app.css hides an idle row's note. Here,
// hovering or focusing an idle row shows that note as a tooltip above the row,
// by setting data-tooltip on it; app.css draws [data-tooltip] out of the flow
// (position: absolute, pointer-events: none), so no row moves when it shows or
// hides. Round 0 showed the note in the flow instead: hiding it on pointer
// leave moved the rows below up under the pointer, and the behaviour oracle's
// next click missed its switch (90 mismatches). A turned-on or switched-off
// row's note stays in the flow and is not touched here.
//
// A slider at its default has no note (owner, 2026-09-26: panel.js writes an
// empty one, and idleNote() skips it), and an idle row is exactly a row at its
// saved default. So a settled panel has no idle row with words to show: the
// tooltip opens only while an idle row's range holds a value not yet saved (a
// drag between its input events and its change), and that drag's own input
// closes it again.
//
// Opens on pointer enter after OPEN_DELAY_MS, at once and with no animation
// when another note tooltip closed less than INSTANT_MS ago (moving down a
// column), and at once when focus enters the row other than by a press.
// Closes at once on pointer
// leave, on focus leaving the row, on Escape (focus stays; the row stays
// quiet until the pointer or focus leaves it), and on any input or change in
// the row, which may stop it being idle. The tooltip holds nothing to reach, so
// there is no close grace. It measures nothing and listens to no scroll or
// resize: it is placed by CSS inside its .setting-entry and scrolls with it.
// Nothing here posts or stores anything, and it adds no element and no id.

export const OPEN_DELAY_MS = 300;
export const INSTANT_MS = 400;

const ENTRY = '.setting-entry';

// Mirrors app.css's hide rule: a drop row's note describes the row, never its
// state, and is always shown.
function idleNote(entry) {
  if (!entry || entry.querySelector('[data-sec="drops"]')) return null;
  if (!entry.querySelector(':scope .slider-switch:not([data-live]) > input:checked')) return null;
  const note = entry.querySelector(':scope > .note[data-note]');
  return note && note.textContent.trim() !== '' ? note : null;
}

export function installSliderNotes(root = document) {
  let shown = null;       // the note drawn as a tooltip now
  let hovered = null;     // the entry under the pointer
  let dismissed = null;   // the entry Escape quieted
  let openTimer = 0;
  let lastClosed = -Infinity;

  const hide = () => {
    clearTimeout(openTimer);
    if (!shown) return;
    shown.removeAttribute('data-tooltip');
    shown.removeAttribute('data-instant');
    shown.removeAttribute('role');
    shown = null;
    lastClosed = performance.now();
  };
  const show = (entry, instant) => {
    clearTimeout(openTimer);
    const note = idleNote(entry);
    if (!note || entry === dismissed) return;
    if (shown === note) return;
    hide();
    note.toggleAttribute('data-instant', !!instant);
    note.setAttribute('role', 'tooltip');
    note.setAttribute('data-tooltip', '');
    shown = note;
  };

  root.addEventListener('pointerover', (e) => {
    if (e.pointerType === 'touch') return;
    const entry = e.target.closest?.(ENTRY) || null;
    if (entry === hovered) return;
    hovered = entry;
    if (!entry) return;
    if (shown && !entry.contains(shown)) hide();
    if (!idleNote(entry) || entry === dismissed) return;
    if (performance.now() - lastClosed < INSTANT_MS) show(entry, true);
    else openTimer = setTimeout(() => { if (hovered === entry) show(entry, false); }, OPEN_DELAY_MS);
  });
  root.addEventListener('pointerout', (e) => {
    const entry = e.target.closest?.(ENTRY);
    if (!entry || entry.contains(e.relatedTarget)) return;
    if (hovered === entry) hovered = null;
    if (dismissed === entry && !entry.contains(document.activeElement)) dismissed = null;
    if (entry.contains(document.activeElement) && shown && entry.contains(shown)) return;
    if (!shown || entry.contains(shown)) hide();
  });
  // Focus from a press (a click on the switch or the range) is the pointer's
  // business: the hover path already has it, and a change closes it.
  let pressing = false;
  root.addEventListener('pointerdown', () => { pressing = true; }, true);
  root.addEventListener('pointerup', () => { pressing = false; }, true);
  root.addEventListener('pointercancel', () => { pressing = false; }, true);
  root.addEventListener('focusin', (e) => {
    const entry = e.target.closest?.(ENTRY);
    if (!entry || pressing) return;
    if (shown && !entry.contains(shown)) hide();
    show(entry, true);
  });
  root.addEventListener('focusout', (e) => {
    const entry = e.target.closest?.(ENTRY);
    if (!entry || entry.contains(e.relatedTarget)) return;
    if (dismissed === entry && hovered !== entry) dismissed = null;
    if (hovered === entry || !shown || !entry.contains(shown)) return;
    hide();
  });
  root.addEventListener('keydown', (e) => {
    if (e.key !== 'Escape' || !shown) return;
    dismissed = shown.closest(ENTRY);
    hide();
  });
  const changed = (e) => { if (shown && e.target.closest?.(ENTRY)?.contains(shown)) hide(); };
  root.addEventListener('input', changed, true);
  root.addEventListener('change', changed, true);
}
