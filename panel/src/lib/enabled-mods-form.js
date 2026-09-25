// Which form the "Enabled mods" list takes: the inline row under the rail
// while its entries fit on one line, the tray (a popover under the count in
// the rail) once they overfill it. One list, two presentations: #enabledMods
// is the single section src/lib/enabled-mods-list.js fills, and the form is
// only `data-form="inline"|"tray"` on it, set here at runtime and drawn by
// app.css. The rule and its numbers are the design's
// (`amendments.enabledMods` in design/figma-export.json):
//   - to the tray when the row's natural single-line width is more than the
//     width it has; back to the row only once it fits with HYSTERESIS_PX to
//     spare, so a window resized across the line does not flicker;
//   - the switch waits while a pointer button or a key is down or the tray is
//     open, and then for HOLD_IDLE_MS, so the list never changes shape under
//     a hand that is using it. A resize alone is not an interaction.
// The tray's popover (closed on load; Escape, an outside click or a tab
// change close it) lives here too, because it is the tray form's own state.
//
// Nothing here posts or stores anything: the list's buttons reach a setting
// only through the control's own handler (enabled-mods-list.js). The decision
// and the hold are pure and import under plain node (tests/*.test.js).

export const HYSTERESIS_PX = 160;
export const HOLD_IDLE_MS = 400;

// natural: the row's single-line width; available: the width it has;
// current: the form now ('inline', 'tray', or null before the first call).
export function nextForm(natural, available, current) {
  if (current === 'tray') return natural <= available - HYSTERESIS_PX ? 'inline' : 'tray';
  return natural > available ? 'tray' : 'inline';
}

// Is a form change held back at `now`? releasedAt is when the last pointer
// button, key or open tray was let go.
export function holdActive({ pointerDown = false, keysDown = 0, popoverOpen = false, releasedAt = -Infinity } = {}, now) {
  if (pointerDown || keysDown > 0 || popoverOpen) return true;
  return now - releasedAt < HOLD_IDLE_MS;
}

const CHEVRON = '<svg viewBox="0 0 12 12" aria-hidden="true" focusable="false"><path d="m3 4.5 3 3 3-3"/></svg>';

export function installEnabledModsForm() {
  const box = document.getElementById('enabledMods');
  const wrap = document.getElementById('wrap');
  if (!box || !wrap) return null;
  const heading = box.querySelector(':scope > h2');
  const count = document.getElementById('enabledModsCount');
  const hold = { pointerDown: false, keys: new Set(), releasedAt: -Infinity };
  const listeners = { render: [] };
  let timer = 0;
  let toggle = null;

  const isOpen = () => box.hasAttribute('data-open');
  const list = () => box.querySelector(':scope > ul');
  const holdState = () => ({ pointerDown: hold.pointerDown, keysDown: hold.keys.size, popoverOpen: isOpen(), releasedAt: hold.releasedAt });

  // The row's natural single-line width and the width it has, both read with
  // the inline layout forced for one synchronous moment (data-measure), so
  // the tray still measures the row it would be. Nothing paints in between.
  function measure() {
    box.setAttribute('data-measure', '');
    const style = getComputedStyle(box);
    const available = box.clientWidth - parseFloat(style.paddingLeft) - parseFloat(style.paddingRight);
    let left = Infinity;
    let right = -Infinity;
    for (const el of [heading, count, box.querySelector(':scope > .enabled-mods-empty'), ...box.querySelectorAll(':scope > ul > li')]) {
      if (!el || !el.getClientRects().length) continue;
      const rect = el.getBoundingClientRect();
      left = Math.min(left, rect.left);
      right = Math.max(right, rect.right);
    }
    box.removeAttribute('data-measure');
    return { natural: right > left ? right - left : 0, available };
  }

  function schedule(delay) {
    clearTimeout(timer);
    timer = setTimeout(decide, Math.max(0, delay));
  }

  function decide() {
    clearTimeout(timer);
    const current = box.dataset.form || null;
    const now = performance.now();
    if (current && holdActive(holdState(), now)) {
      // Held: a release (pointer, key, closing the tray) decides again.
      const s = holdState();
      if (!s.pointerDown && !s.keysDown && !s.popoverOpen) schedule(hold.releasedAt + HOLD_IDLE_MS - now + 1);
      return;
    }
    const { natural, available } = measure();
    if (!(available > 0)) return;
    const next = nextForm(natural, available, current);
    if (next !== current) setForm(next);
  }

  function setForm(form) {
    if (form === 'tray') {
      if (!toggle) {
        // No id (the oracle walks `button[id]`) and no words of its own: its
        // name is the count it holds.
        toggle = document.createElement('button');
        toggle.type = 'button';
        toggle.className = 'enabled-mods-toggle';
        toggle.setAttribute('aria-labelledby', 'enabledModsCount');
        toggle.setAttribute('aria-expanded', 'false');
        toggle.addEventListener('click', (e) => {
          const opening = !isOpen();
          setOpen(opening);
          // Opened from the keyboard (a click with no pointer behind it):
          // straight to the first Turn off.
          if (opening && e.detail === 0) list()?.querySelector('.quick-disable')?.focus();
        });
        count.before(toggle);
        toggle.append(count);
        toggle.insertAdjacentHTML('beforeend', CHEVRON);
      }
      box.dataset.form = 'tray';
    } else {
      setOpen(false);
      if (toggle) {
        const hadFocus = toggle.contains(document.activeElement);
        toggle.before(count);
        toggle.remove();
        toggle = null;
        if (hadFocus) heading?.focus({ preventScroll: true });
      }
      box.dataset.form = 'inline';
    }
    labelList();
  }

  function setOpen(open) {
    if (open === isOpen()) return;
    box.toggleAttribute('data-open', open);
    toggle?.setAttribute('aria-expanded', String(open));
    if (!open) {
      hold.releasedAt = performance.now();
      schedule(HOLD_IDLE_MS + 1);
    }
    fitTray();
  }

  // The list the toggle controls, and the tray's height: it scrolls after
  // the design's scrollsAfter (8) entries.
  function labelList() {
    const ul = list();
    if (!ul) return;
    ul.id = 'enabledModsList';
    toggle?.setAttribute('aria-controls', ul.id);
    fitTray();
  }

  function fitTray() {
    const ul = list();
    if (!ul) return;
    ul.style.maxHeight = '';
    if (box.dataset.form === 'tray' && isOpen()) {
      const entries = ul.querySelectorAll(':scope > li');
      if (entries.length > 8) {
        const last = entries[7];
        ul.style.maxHeight = `${last.offsetTop + last.offsetHeight + parseFloat(getComputedStyle(ul).paddingBottom)}px`;
      }
    }
    markMoreBelow(ul);
  }

  // A list with more below the fold says so (app.css draws a fade).
  function markMoreBelow(ul) {
    ul.toggleAttribute('data-more-below', ul.scrollHeight - ul.scrollTop - ul.clientHeight > 1);
  }

  // Interaction that holds the form: pointer buttons and keys, anywhere.
  document.addEventListener('pointerdown', (e) => {
    hold.pointerDown = true;
    if (isOpen() && !box.contains(e.target)) setOpen(false);
  }, true);
  const pointerUp = () => {
    if (!hold.pointerDown) return;
    hold.pointerDown = false;
    hold.releasedAt = performance.now();
    schedule(HOLD_IDLE_MS + 1);
  };
  document.addEventListener('pointerup', pointerUp, true);
  document.addEventListener('pointercancel', pointerUp, true);
  document.addEventListener('keydown', (e) => {
    hold.keys.add(e.code || e.key);
    if (e.key === 'Escape' && isOpen()) {
      setOpen(false);
      toggle?.focus();
    }
  }, true);
  document.addEventListener('keyup', (e) => {
    hold.keys.delete(e.code || e.key);
    if (!hold.keys.size) {
      hold.releasedAt = performance.now();
      schedule(HOLD_IDLE_MS + 1);
    }
  }, true);
  // A key let go in another window never reaches this one.
  window.addEventListener('blur', () => {
    if (!hold.keys.size && !hold.pointerDown) return;
    hold.keys.clear();
    hold.pointerDown = false;
    hold.releasedAt = performance.now();
    schedule(HOLD_IDLE_MS + 1);
  });

  // A tab change closes the tray (the tab's state, not nav.js, is watched).
  const workspace = document.getElementById('workspace');
  if (workspace) new MutationObserver(() => setOpen(false)).observe(workspace, { attributes: true, attributeFilter: ['aria-labelledby'] });

  // The renderer replaced the list (boot(), and after every drained save).
  new MutationObserver((records) => {
    if (!records.some((r) => [...r.addedNodes, ...r.removedNodes].some((n) => n.nodeType === 1 && n.matches('ul, .enabled-mods-empty')))) return;
    const ul = list();
    if (ul) ul.addEventListener('scroll', () => markMoreBelow(ul), { passive: true });
    labelList();
    for (const fn of listeners.render) fn();
    decide();
  }).observe(box, { childList: true });

  new ResizeObserver(() => decide()).observe(wrap);
  // The tray sits left of #chipGame in the rail; the chip's width follows its
  // text ("Game offline", "Game open · plugin missing").
  const chip = document.getElementById('chipGame');
  if (chip) new ResizeObserver(() => document.documentElement.style.setProperty('--chip-game-w', `${chip.offsetWidth}px`)).observe(chip);
  window.addEventListener('resize', decide);
  document.fonts?.ready?.then(decide);
  decide();

  return {
    get form() { return box.dataset.form || null; },
    get toggle() { return toggle; },
    isOpen,
    decide,
    // Called after every render of the list, once the list is labelled.
    onRender(fn) { listeners.render.push(fn); },
  };
}
