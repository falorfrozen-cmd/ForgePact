// The plugin warning as two icons, each with a tooltip (owner, 2026-09-25,
// replacing the full-width band): #pluginWarning beside Apply all, whose
// visibility and message status() in panel.js owns, and a second icon beside
// the status bar's "Settings loaded", kept in step with it here - never by
// editing status(). Each icon is the control: it opens Setup (App.svelte).
// Its tooltip only describes, since a tooltip's content is not interactive
// (WAI-ARIA tooltip pattern, WCAG 1.4.13): a link inside it would vanish as
// the pointer travelled to it and could never be reached by Tab.
//
// A tooltip opens on hover after OPEN_DELAY_MS, at once on focus (with no
// animation when the focus came from the keyboard), and at once on hover when
// the other one closed less than INSTANT_MS ago (no delay, no animation: the
// second feels as fast as the first was deliberate). It stays
// open while the pointer is on the icon or on the tooltip, and closes when the
// pointer leaves both (after CLOSE_GRACE_MS, so the gap between them can be
// crossed), on blur, and on Escape, which leaves focus where it was. It is
// position: fixed, placed from the icon's box whenever it opens and clamped
// into the window: below the icon in the page actions, above it in the status
// bar, which is fixed at the bottom of the window.
// Nothing here posts or stores anything.

export const OPEN_DELAY_MS = 300;
export const CLOSE_GRACE_MS = 120;
const INSTANT_MS = 400;
const EDGE = 12;
const GAP = 8;

let lastClosed = -Infinity;
const tooltips = [];

// Where a tooltip goes: its box beside the icon's, inside the window.
export function tooltipPosition(icon, size, viewport, upward) {
  const left = upward ? icon.left : icon.right - size.width;
  const top = upward ? icon.top - GAP - size.height : icon.bottom + GAP;
  return {
    left: Math.max(EDGE, Math.min(left, viewport.width - EDGE - size.width)),
    top: Math.max(EDGE, Math.min(top, viewport.height - EDGE - size.height)),
  };
}

function wire(host, upward) {
  const button = host?.querySelector('.plugin-warning-button');
  const tip = host?.querySelector('.plugin-warning-tooltip');
  if (!button || !tip) return;
  let openTimer = 0;
  let closeTimer = 0;
  let dismissed = false;
  let hovered = false;

  const place = () => {
    tip.style.left = '0px';
    tip.style.top = '0px';
    const at = tooltipPosition(button.getBoundingClientRect(), { width: tip.offsetWidth, height: tip.offsetHeight },
      { width: innerWidth, height: innerHeight }, upward);
    tip.style.left = at.left + 'px';
    tip.style.top = at.top + 'px';
    tip.style.setProperty('--tooltip-origin', upward ? 'bottom left' : 'top right');
  };
  const isOpen = () => !tip.hidden;
  const open = (instant) => {
    clearTimeout(openTimer);
    clearTimeout(closeTimer);
    if (isOpen() || dismissed || host.hidden) return;
    for (const other of tooltips) if (other.tip !== tip) other.close();
    tip.toggleAttribute('data-instant', !!instant);
    // Clearing `hidden` is the entrance: app.css's @starting-style scales and
    // fades it in from display:none, so no attribute has to wait a frame.
    tip.hidden = false;
    place();
  };
  const close = () => {
    clearTimeout(openTimer);
    clearTimeout(closeTimer);
    if (!isOpen()) return;
    tip.hidden = true;
    lastClosed = performance.now();
  };
  tooltips.push({ tip, close });

  host.addEventListener('pointerenter', () => {
    hovered = true;
    clearTimeout(closeTimer);
    if (isOpen()) return;
    if (performance.now() - lastClosed < INSTANT_MS) open(true);
    else openTimer = setTimeout(() => open(false), OPEN_DELAY_MS);
  });
  host.addEventListener('pointerleave', () => {
    hovered = false;
    clearTimeout(openTimer);
    if (document.activeElement === button) return;
    dismissed = false;
    closeTimer = setTimeout(close, CLOSE_GRACE_MS);
  });
  // Focus from the keyboard shows it at once: a keyboard action never animates.
  button.addEventListener('focus', () => open(button.matches(':focus-visible')));
  button.addEventListener('blur', () => {
    dismissed = false;
    if (!hovered) close();
  });
  document.addEventListener('keydown', (e) => {
    if (e.key !== 'Escape' || !isOpen()) return;
    dismissed = true;
    close();
  });
  // The page scrolls in #wrap and the window can be resized while it is open.
  const replace = () => { if (isOpen()) place(); };
  document.getElementById('wrap')?.addEventListener('scroll', replace, { passive: true });
  addEventListener('resize', replace);
  // status() hides the warning: its tooltip goes with it.
  new MutationObserver(() => { if (host.hidden) { dismissed = false; close(); } }).observe(host, { attributes: true, attributeFilter: ['hidden'] });
}

export function installPluginWarning() {
  const warning = document.getElementById('pluginWarning');
  const text = document.getElementById('pluginWarningText');
  const status = document.querySelector('#statusbar .status-warning');
  const statusText = document.getElementById('statusWarningText');
  if (warning && text && status && statusText) {
    // The status bar's icon shows exactly when #pluginWarning does, saying the same.
    const sync = () => {
      status.hidden = warning.hidden;
      if (statusText.textContent !== text.textContent) statusText.textContent = text.textContent;
    };
    new MutationObserver(sync).observe(warning, { attributes: true, attributeFilter: ['hidden'] });
    new MutationObserver(sync).observe(text, { childList: true, characterData: true, subtree: true });
    sync();
  }
  wire(warning, false);
  wire(status, true);
}
