// A switched-off slider row still shows the value it will return to: the
// design's `switch-off` state draws it faintly beside the value box, which
// reads `off`. Presentational only. Each slider row with a switch (every
// `input[data-switch]`, plus Monster Density's #den_on) gets one
//   <span class="val-remembered" aria-hidden="true">
// as a sibling of its `.val`, never inside it: typable() empties `.val` to put an
// editor there, and `.val` stays the first `.val` of the row for every lookup
// that takes the first one. `.val`'s text and class stay whatever the
// unchanged handlers write.
//
// The span holds what `.val` shows for the range's current value when the
// switch is on (`x3`, `+25%`, `30%`), and nothing while the switch is on or
// the value is the default. It follows the range while the switch is off,
// and every repaint of `.val`. It posts and stores nothing.
import { sliderText } from '../panel.js';

// The rows the page draws by hand format their own values; these are the
// same formats their painters in panel.js use (angelicPaint, rarityPaint,
// the enemy speed and density handlers), each returning '' at the default.
const HAND_DRAWN = {
  angelic_items: (v) => (v > 1 ? 'x' + v : ''),
  rarity_rare: (v) => (v > 0 ? v + '%' : ''),
  rarity_ancient: (v) => (v > 0 ? v + '%' : ''),
  enemy_speed: (v) => (v > 0 ? '+' + v + '%' : ''),
  density: (v) => (v > 1 ? 'x' + v : ''),
};

function rowsOf(root) {
  const rows = [];
  for (const box of root.querySelectorAll('input[data-switch]')) {
    const row = box.closest('.row');
    const range = row?.querySelector('input[type=range]');
    if (!range) continue;
    const format = range.dataset.sec
      ? (v) => { const text = sliderText(range.dataset.sec, v); return text === 'off' ? '' : text; }
      : HAND_DRAWN[box.dataset.switch];
    if (format) rows.push({ box, row, range, format });
  }
  const den = document.getElementById('den');
  const denOn = document.getElementById('den_on');
  if (den && denOn) rows.push({ box: denOn, row: den.closest('.row'), range: den, format: HAND_DRAWN.density });
  return rows;
}

export function installRememberedValues() {
  const root = document.getElementById('workspace');
  if (!root) return;
  let queued = false;

  function update() {
    queued = false;
    for (const { box, row, range, format } of rowsOf(root)) {
      const val = row?.querySelector('.val');
      // Only once preparePanelUI() has put `.val` in its value editor, the
      // span's place: the editor is built in boot(), after these rows exist.
      if (!val?.parentElement.classList.contains('value-stepper')) continue;
      let span = val.parentElement.querySelector(':scope > .val-remembered');
      if (!span) {
        span = document.createElement('span');
        span.className = 'val-remembered';
        span.setAttribute('aria-hidden', 'true');
        // Between the step buttons, so `−` stays the editor's first child and
        // `+` its last (tests and the oracle find them that way); app.css
        // draws it outside the box, on its left.
        val.before(span);
      }
      const v = parseFloat(range.value);
      const text = box.checked || !Number.isFinite(v) ? '' : format(v);
      if (span.textContent !== text) span.textContent = text;
    }
  }
  // After every listener has run (the handlers snap the range and repaint
  // `.val` in their own `input`/`change` handlers).
  const schedule = () => {
    if (queued) return;
    queued = true;
    queueMicrotask(update);
  };

  root.addEventListener('input', schedule, true);
  root.addEventListener('change', schedule, true);
  new MutationObserver((records) => {
    if (records.every((r) => (r.target.nodeType === 1 ? r.target : r.target.parentElement)?.closest('.val-remembered'))) return;
    schedule();
  }).observe(root, { subtree: true, childList: true, characterData: true, attributes: true, attributeFilter: ['class'] });
  schedule();
}
