// Turn off's safety net, around the unchanged route in enabled-mods-list.js
// (quickDisable sets the control off and dispatches `change`, so the
// control's own handler posts). Four things, all from the design's
// `amendments` (design/figma-export.json):
//   - the undo toast: "<name> turned off" with an Undo button, for
//     UNDO_VISIBLE_MS, paused while hovered, focused or the window is hidden,
//     replaced by the next Turn off. Undo is the same route in reverse: it
//     sets the control back the way it was and dispatches `change` on it, so
//     the control's own handler posts exactly what turning it on does. It
//     does nothing if the control has changed since;
//   - a polite announcement in a live region of its own (not #toast, not
//     #saveIndicator);
//   - focus: a Turn off that had focus hands it to the next entry's Turn off,
//     else the previous one's, else the heading (the count control in the
//     tray), once the list has been rebuilt;
//   - the reflow freeze: while the pointer stays in the list after a Turn
//     off, an invisible placeholder keeps the removed entry's place, so no
//     other Turn off slides under it, until the pointer leaves or
//     FREEZE_MAX_MS pass.
// The toast sits in a band app.css reserves above the status bar, outside
// .page-actions, #statusbar and every .tab-card, and never over the
// scrolling content. Nothing here posts or stores anything.
import { UNDO_TEXTS, withName } from './enabled-mods-copy.js';

export const FREEZE_MAX_MS = 2500;
export const UNDO_VISIBLE_MS = 8000;

// Where focus goes after the Turn off for `removed` was used with focus on
// it. order: the listed ids before; remaining: the ids listed now.
export function focusAfterTurnOff({ order, removed, remaining, form }) {
  const left = new Set(remaining);
  const at = order.indexOf(removed);
  for (let i = at + 1; i < order.length; i++) if (left.has(order[i])) return { to: 'entry', id: order[i] };
  for (let i = at - 1; i >= 0; i--) if (left.has(order[i])) return { to: 'entry', id: order[i] };
  return { to: form === 'tray' ? 'toggle' : 'heading' };
}

// The state a control is in, as quickDisable changes it.
function stateOf(control) {
  return control.matches('select') ? control.value : control.checked;
}

function setState(control, state) {
  if (control.matches('select')) control.value = state;
  else control.checked = state;
}

// What quickDisable leaves a control at.
function isOff(control) {
  return control.matches('select') ? control.value === 'off' : control.checked === false;
}

export function installEnabledModsUndo(form) {
  const box = document.getElementById('enabledMods');
  if (!box) return;
  const heading = box.querySelector(':scope > h2');
  // The pinned markup gives the heading no tabindex; focus may land on it.
  heading?.setAttribute('tabindex', '-1');

  const announcer = document.createElement('div');
  announcer.className = 'visually-hidden';
  announcer.setAttribute('role', 'status');
  announcer.setAttribute('aria-live', 'polite');
  document.body.append(announcer);
  const announce = (text) => {
    // A repeated sentence is still spoken: clear, then set on the next frame.
    announcer.textContent = '';
    requestAnimationFrame(() => { announcer.textContent = text; });
  };

  let pending = null;     // the Turn off waiting for the list to rebuild
  let undoFocus = null;   // the id whose Turn off gets focus after an Undo
  let toast = null;
  let freeze = null;      // { index, width, height, timer }
  let pointerInside = false;

  // Record the control's state before the unchanged onclick turns it off.
  box.addEventListener('click', (e) => {
    const button = e.target.closest?.('.quick-disable');
    if (!button || !box.contains(button)) return;
    const control = document.getElementById(button.dataset.for);
    if (!control) return;
    const entry = button.closest('li');
    const entries = [...box.querySelectorAll(':scope > ul > li.enabled-mod')];
    const name = entry?.querySelector('.enabled-mod-name')?.textContent.trim() || '';
    pending = {
      id: control.id,
      hadFocus: document.activeElement === button,
      order: entries.map((li) => li.dataset.for),
    };
    if (entry && e.detail !== 0) {
      const rect = entry.getBoundingClientRect();
      startFreeze({ index: entries.indexOf(entry), width: rect.width, height: rect.height, copy: fadingCopy(entry) });
    }
    showToast({ control, prior: stateOf(control), name, instant: e.detail === 0 });
    announce(withName(UNDO_TEXTS.announceOff, name));
  }, true);

  box.addEventListener('pointerenter', () => { pointerInside = true; });
  box.addEventListener('pointerleave', () => {
    pointerInside = false;
    endFreeze();
  });

  // The pointer's Turn off: the list is rebuilt without the entry within a
  // frame or two of the click, so the entry itself has no time to fade. The
  // placeholder that keeps its place starts as a copy of it instead - not an
  // .enabled-mod, with no data-for, inert and hidden from assistive tech -
  // which app.css fades and shrinks once ([data-fading]). From the keyboard
  // there is no placeholder, and the entry simply goes.
  function fadingCopy(entry) {
    const copy = entry.cloneNode(true);
    copy.className = 'enabled-mod-ghost';
    copy.removeAttribute('data-for');
    for (const el of copy.querySelectorAll('[data-for], [aria-label]')) {
      el.removeAttribute('data-for');
      el.removeAttribute('aria-label');
    }
    copy.setAttribute('data-fading', '');
    return copy;
  }

  function startFreeze(place) {
    endFreeze();
    freeze = { ...place, timer: setTimeout(endFreeze, FREEZE_MAX_MS) };
  }

  function endFreeze() {
    if (!freeze) return;
    clearTimeout(freeze.timer);
    freeze = null;
    const ghost = box.querySelector('.enabled-mod-ghost');
    if (ghost) {
      ghost.remove();
      form?.decide();
    }
  }

  // After every render: keep the geometry, then move focus.
  const afterRender = () => {
    const ul = box.querySelector(':scope > ul');
    if (freeze && pointerInside && ul) {
      // Not an .enabled-mod, and nothing in it with an id or a data-for: tests
      // and the oracle never see it. The first one after the Turn off is the
      // fading copy; a later rebuild inside the same freeze gets a blank one.
      const ghost = freeze.copy || document.createElement('li');
      freeze.copy = null;
      ghost.className = 'enabled-mod-ghost';
      ghost.setAttribute('aria-hidden', 'true');
      ghost.inert = true;
      ghost.style.width = `${freeze.width}px`;
      ghost.style.height = `${freeze.height}px`;
      ul.insertBefore(ghost, ul.children[freeze.index] || null);
    }
    const remaining = [...box.querySelectorAll(':scope > ul > li.enabled-mod')].map((li) => li.dataset.for);
    if (pending && !remaining.includes(pending.id)) {
      const { order, id, hadFocus } = pending;
      pending = null;
      if (hadFocus) {
        const target = focusAfterTurnOff({ order, removed: id, remaining, form: form?.form });
        const el = target.to === 'entry' ? box.querySelector(`.quick-disable[data-for="${target.id}"]`)
          : target.to === 'toggle' ? (form?.toggle || heading) : heading;
        el?.focus({ preventScroll: true });
      }
    }
    if (undoFocus && remaining.includes(undoFocus)) {
      box.querySelector(`.quick-disable[data-for="${undoFocus}"]`)?.focus({ preventScroll: true });
      undoFocus = null;
    }
  };
  if (form) form.onRender(afterRender);
  else new MutationObserver(afterRender).observe(box, { childList: true });

  // The toast rises in and sinks out (app.css). It appears at once when the
  // keyboard turned the entry off or when it replaces a toast still shown,
  // and leaves at once for a keyboard Undo; one that is leaving is inert and
  // is removed when its fade ends, or at once by the next toast.
  function showToast({ control, prior, name, instant = false }) {
    const replacing = !!toast;
    hideToast(true);
    const el = document.createElement('div');
    el.className = 'undo-toast';
    if (instant || replacing) el.setAttribute('data-instant', '');
    const text = document.createElement('span');
    text.className = 'undo-toast-text';
    text.textContent = withName(UNDO_TEXTS.turnedOff, name);
    const button = document.createElement('button');
    button.type = 'button';
    button.className = 'undo-toast-button';
    button.textContent = UNDO_TEXTS.undo;
    button.setAttribute('aria-label', withName(UNDO_TEXTS.undoLabel, name));
    el.append(text, button);
    const host = document.documentElement.dataset.theme === 'ember'
      ? document.querySelector('.ember-notices') : null;
    (host || document.body).append(el);
    document.documentElement.setAttribute('data-undo-open', '');

    let left = UNDO_VISIBLE_MS;
    let startedAt = 0;
    let timer = 0;
    // Once hidden, the pointer or focus leaving this toast must not start its
    // timer again: hideToast hides whichever toast is showing by then.
    let closed = false;
    const paused = () => el.matches(':hover') || el.contains(document.activeElement) || document.hidden;
    const run = () => {
      clearTimeout(timer);
      if (closed || paused()) return;
      startedAt = performance.now();
      timer = setTimeout(hideToast, left);
    };
    const pause = () => {
      if (!timer) return;
      clearTimeout(timer);
      timer = 0;
      left = Math.max(0, left - (performance.now() - startedAt));
    };
    const resume = () => { if (!timer) run(); };
    const onVisibility = () => (document.hidden ? pause() : resume());
    el.addEventListener('pointerenter', pause);
    el.addEventListener('pointerleave', () => { if (!el.contains(document.activeElement)) resume(); });
    el.addEventListener('focusin', pause);
    el.addEventListener('focusout', (e) => { if (!el.contains(e.relatedTarget) && !el.matches(':hover')) resume(); });
    document.addEventListener('visibilitychange', onVisibility);

    button.addEventListener('click', (e) => {
      const hadFocus = document.activeElement === button;
      if (isOff(control)) {
        setState(control, prior);
        control.dispatchEvent(new Event('change', { bubbles: true }));
        announce(withName(UNDO_TEXTS.announceUndo, name));
        if (hadFocus) undoFocus = control.id;
      }
      hideToast(e.detail === 0);
    });

    toast = { el, cleanup: () => { closed = true; clearTimeout(timer); document.removeEventListener('visibilitychange', onVisibility); } };
    run();
  }

  const leaving = new Set();
  function hideToast(instant = false) {
    for (const el of leaving) el.remove();
    leaving.clear();
    if (!toast) return;
    toast.cleanup();
    const { el } = toast;
    if (instant) el.remove();
    else {
      el.inert = true;
      el.setAttribute('data-leaving', '');
      leaving.add(el);
      const gone = (e) => {
        if (e.target !== el || e.propertyName !== 'opacity') return;
        leaving.delete(el);
        el.remove();
      };
      el.addEventListener('transitionend', gone);
      el.addEventListener('transitioncancel', gone);
    }
    toast = null;
    releaseBand();
  }

  // Giving the band back grows the scrolling page at its bottom. Scrolled to
  // near the end, that would pull the content down under the pointer, so the
  // band is kept until the page is scrolled to where it can grow in place.
  const wrap = document.getElementById('wrap');
  const shell = document.getElementById('appShell');
  const statusbar = document.getElementById('statusbar');
  let waiting = false;
  const growsInPlace = () => {
    const band = parseFloat(getComputedStyle(shell).paddingBottom) - (statusbar?.offsetHeight || 0);
    return wrap.scrollTop <= Math.max(0, wrap.scrollHeight - wrap.clientHeight - band);
  };
  function releaseBand() {
    const root = document.documentElement;
    if (toast || !root.hasAttribute('data-undo-open')) return;
    // Ember's footer row reserves its own height, including a leaving toast.
    if (root.dataset.theme === 'ember' || !wrap || !shell || growsInPlace()) {
      root.removeAttribute('data-undo-open');
      return;
    }
    if (waiting) return;
    waiting = true;
    wrap.addEventListener('scroll', function onScroll() {
      if (!toast && !growsInPlace()) return;
      wrap.removeEventListener('scroll', onScroll);
      waiting = false;
      releaseBand();
    }, { passive: true });
  }
}
