// The run card (#185): one finished run as a picture, for pasting into a chat,
// where a table loses its shape and a screenshot drags the whole window along.
// Ported from HS Offline Tracker's src/runcard.js (MIT, Copyright (c) 2026
// falorfrozen-cmd and HS Offline Tracker contributors; the notice is in
// CREDITS.md), redrawn in ForgePact's design: the page's own tokens, read from
// its computed style when the card is drawn, so Ember, Ledger, Graphite and
// Sigil each draw the card in their own palette and no colour is a literal
// here. Numbers are in the mono family, as on the page (panel/DESIGN.md §
// Tracker; the comp is design/tracker/tab.html's "run card").
//
// Two halves. `cardLayout` is pure arithmetic: given the run and a way to
// measure text, it answers the card's height and a list of drawing operations
// (rectangles and text, with colours and fonts named by role), so
// tests/tracker-runs.test.js checks the layout under plain node. `drawRunCard`
// and `copyCard` are the browser's part: they read the palette, measure with
// the canvas, paint the operations and put the PNG on the clipboard.

import { fmt, span, RARITIES } from '../format.js';
import { drops, finds, perHour, tallies, titleLine, when, zones } from './summary.js';

/** The card's geometry, in CSS pixels; drawn at `dpr` times that. */
export const CARD = {
  width: 480,
  dpr: 2,
  pad: 24,
  gap: 16,
  top: 16, // the brand line
  title: 26, // the character and difficulty
  total: 58, // one total: its label, value and rate
  row: 22, // a loot or tally row
  subhead: 20, // a group's heading
  zoneRow: 30, // a zone's name and time, its bar under them
  line: 20, // the finds line
  zones: 3, // at most this many zones
  tallies: 4, // at most this many tallies
};

/** Each text role's font: a size, a weight and which family token it uses. */
export const FONTS = {
  brand: { size: 12, weight: 500, family: 'body' },
  meta: { size: 12, weight: 400, family: 'mono' },
  title: { size: 18, weight: 600, family: 'body' },
  label: { size: 12, weight: 400, family: 'body' },
  value: { size: 22, weight: 400, family: 'mono' },
  rate: { size: 12, weight: 400, family: 'mono' },
  subhead: { size: 12, weight: 500, family: 'body' },
  row: { size: 13, weight: 400, family: 'body' },
  num: { size: 13, weight: 400, family: 'mono' },
};

// The tokens the card is drawn with, by the role the layout names them.
const COLOR_TOKENS = {
  bg: '--color-bg-raised',
  sunken: '--color-bg-sunken',
  primary: '--color-text-primary',
  muted: '--color-text-muted',
  'rarity-satanic': '--color-rarity-satanic',
  'rarity-set': '--color-rarity-set',
  'rarity-heroic': '--color-rarity-heroic',
  'rarity-angelic': '--color-rarity-angelic',
  'rarity-unholy': '--color-rarity-unholy',
};
const FONT_TOKENS = { body: '--font-body-family', mono: '--font-mono-family' };

/**
 * The page's palette, from a computed style (anything with getPropertyValue).
 * A token the page does not define is named in `missing` and drawn in the
 * system's own colours (Canvas, CanvasText) rather than in a colour of ours.
 */
export function readPalette(style) {
  const missing = [];
  const read = (token) => {
    const value = String(style?.getPropertyValue?.(token) ?? '').trim();
    if (!value) missing.push(token);
    return value;
  };
  const colors = {};
  for (const [role, token] of Object.entries(COLOR_TOKENS)) {
    colors[role] = read(token) || (role === 'bg' || role === 'sunken' ? 'Canvas' : 'CanvasText');
  }
  const fonts = {};
  for (const [role, token] of Object.entries(FONT_TOKENS)) fonts[role] = read(token);
  return { colors, fonts, missing };
}

/** A role's font as the canvas takes it: weight, size, the page's family, then the system's. */
export function fontString(font, fonts = {}) {
  const family = fonts[font.family];
  return `${font.weight} ${font.size}px ${family ? `${family}, ` : ''}system-ui, sans-serif`;
}

/** `text` whole when it fits `max`, else its longest start that fits with an ellipsis, else nothing. */
export function fit(text, max, role, measure) {
  const s = String(text ?? '');
  if (measure(s, role) <= max) return s;
  for (let n = s.length - 1; n > 0; n--) {
    const cut = `${s.slice(0, n).trimEnd()}…`;
    if (measure(cut, role) <= max) return cut;
  }
  return measure('…', role) <= max ? '…' : '';
}

const rarityColor = (rarity) => (RARITIES.includes(rarity) ? `rarity-${rarity.toLowerCase()}` : 'primary');

/** The card in words, for the canvas's accessible name. */
export function cardLabel(run, now = Date.now()) {
  return `Run card: ${titleLine(run)}, ${when(run?.started_ms, now)}, ${span(run?.secs)}. `
    + `${fmt(run?.gold ?? 0)} gold, ${fmt(run?.xp ?? 0)} experience, ${fmt(run?.kills ?? 0)} kills, ${fmt(drops(run))} drops.`;
}

/**
 * The card's layout: `{ width, height, ops }`. An op is
 * `{ kind: 'rect', x, y, w, h, color }` or
 * `{ kind: 'text', text, x, y, font, color, align }`, where `y` is the text's
 * baseline, `font` a FONTS role and `color` a palette role. The first op is
 * the background, the whole card. `measure(text, role)` answers a text's
 * width in that role's font; `now` decides "Today" and "Yesterday".
 */
export function cardLayout(run, { measure, now = Date.now() } = {}) {
  const { width, pad, gap } = CARD;
  const content = width - 2 * pad;
  const col = (content - gap) / 2;
  const right = width - pad;
  const ops = [];
  const text = (t, x, y, font, color, align = 'left') => ops.push({ kind: 'text', text: t, x, y, font, color, align });
  const secs = typeof run?.secs === 'number' && run.secs > 0 ? run.secs : 0;

  // Two columns of label / number rows, the number at each column's right edge.
  const pairs = (items, top, color) => {
    items.forEach((item, i) => {
      const x = pad + (i % 2) * (col + gap);
      const y = top + Math.floor(i / 2) * CARD.row + 15;
      const count = fmt(item.count);
      const room = col - measure(count, 'num') - 8;
      text(fit(item.label, room, 'row', measure), x, y, 'row', item.color ?? color);
      text(count, x + col, y, 'num', 'primary', 'right');
    });
    return Math.ceil(items.length / 2) * CARD.row;
  };

  let y = pad;

  // The brand at the left, when the run started and how long it ran at the right.
  const meta = `${when(run?.started_ms, now)} · ${span(secs)}`;
  text(meta, right, y + 12, 'meta', 'muted', 'right');
  text(fit('ForgePact Tracker', content - measure(meta, 'meta') - gap, 'brand', measure), pad, y + 12, 'brand', 'muted');
  y += CARD.top + 8;

  // Whose run, on which difficulty.
  text(fit(titleLine(run), content, 'title', measure), pad, y + 19, 'title', 'primary');
  y += CARD.title + gap;

  // The four totals, two by two, each with its rate under it.
  const totals = [
    ['Gold', run?.gold],
    ['Experience', run?.xp],
    ['Kills', run?.kills],
    ['Drops', drops(run)],
  ];
  totals.forEach(([label, value], i) => {
    const x = pad + (i % 2) * (col + gap);
    const top = y + Math.floor(i / 2) * (CARD.total + gap);
    const v = typeof value === 'number' && Number.isFinite(value) ? value : 0;
    text(label, x, top + 12, 'label', 'muted');
    text(fit(fmt(v), col, 'value', measure), x, top + 38, 'value', 'primary');
    text(fit(`${fmt(perHour(v, secs))}/h`, col, 'rate', measure), x, top + 54, 'rate', 'muted');
  });
  y += 2 * CARD.total + gap + gap;

  // The loot: the rarities that dropped, in their own colours; all five when none did.
  const got = RARITIES.filter((name) => (run?.items?.[name] ?? 0) > 0);
  y += pairs((got.length ? got : RARITIES).map((name) => ({ label: name, count: run?.items?.[name] ?? 0, color: rarityColor(name) })), y);

  // The bosses and chests the run put down, when the run says.
  const done = [...tallies(run, 'boss'), ...tallies(run, 'chest')].slice(0, CARD.tallies);
  if (done.length) {
    y += gap;
    text('Killed and opened', pad, y + 13, 'subhead', 'muted');
    y += CARD.subhead;
    y += pairs(done.map((t) => ({ label: t.label, count: t.total })), y, 'muted');
  }

  // Where the time went, longest first, each with its share of the run as a bar.
  const where = zones(run).slice(0, CARD.zones);
  if (where.length) {
    y += gap;
    text('Where it happened', pad, y + 13, 'subhead', 'muted');
    y += CARD.subhead;
    for (const zone of where) {
      const len = span(zone.secs);
      text(len, right, y + 14, 'num', 'muted', 'right');
      text(fit(zone.label, content - measure(len, 'num') - 12, 'row', measure), pad, y + 14, 'row', 'primary');
      ops.push({ kind: 'rect', x: pad, y: y + 20, w: content, h: 4, color: 'sunken' });
      ops.push({ kind: 'rect', x: pad, y: y + 20, w: Math.min(content, Math.max(2, Math.round(zone.share * content))), h: 4, color: 'muted' });
      y += CARD.zoneRow;
    }
  }

  // The finds, one line along the bottom: as many names as fit, then how many more.
  const found = finds(run);
  if (found.length) {
    y += gap;
    const base = y + 14;
    const label = 'Finds:';
    text(label, pad, base, 'row', 'muted');
    let x = pad + measure(`${label} `, 'row');
    const sep = measure(', ', 'row');
    const space = measure(' ', 'row');
    let shown = 0;
    for (let i = 0; i < found.length; i++) {
      const rest = found.length - i - 1;
      const more = rest > 0 ? space + measure(`+${rest} more`, 'row') : 0;
      const lead = i > 0 ? sep : 0;
      const name = found[i].name;
      const room = right - x - lead - more;
      let shownName = name;
      if (measure(name, 'row') > room) {
        // The first name is cut rather than dropped; a later one waits in "+N more".
        if (i > 0) break;
        shownName = fit(name, room, 'row', measure);
        if (!shownName) break;
      }
      if (i > 0) { text(',', x, base, 'row', 'muted'); x += sep; }
      text(shownName, x, base, 'row', rarityColor(found[i].rarity));
      x += measure(shownName, 'row');
      shown++;
    }
    if (shown < found.length) text(`+${found.length - shown} more`, x + space, base, 'row', 'muted');
    y += CARD.line;
  }

  const height = y + pad;
  ops.unshift({ kind: 'rect', x: 0, y: 0, w: width, h: height, color: 'bg' });
  return { width, height, ops };
}

// ---- The browser's part ------------------------------------------------------

/**
 * Draw `run` on `canvas` in the page's palette, at `dpr` times its CSS size.
 * Loads the fonts the card uses first, so the first card is not measured in a
 * fallback face. Answers the layout and the palette's missing tokens, or
 * null when `isCurrent` says a newer drawing was asked for while the fonts
 * loaded (that one paints instead).
 */
export async function drawRunCard(canvas, run, { style, now = Date.now(), dpr = CARD.dpr, isCurrent = () => true } = {}) {
  const palette = readPalette(style ?? getComputedStyle(document.documentElement));
  const fonts = Object.values(FONTS).map((font) => fontString(font, palette.fonts));
  if (document.fonts?.load) await Promise.all(fonts.map((f) => document.fonts.load(f).catch(() => null)));
  if (!isCurrent()) return null;
  const ctx = canvas.getContext('2d');
  const measure = (t, role) => {
    ctx.font = fontString(FONTS[role], palette.fonts);
    return ctx.measureText(t).width;
  };
  const layout = cardLayout(run, { measure, now });
  canvas.width = layout.width * dpr;
  canvas.height = layout.height * dpr;
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.textBaseline = 'alphabetic';
  for (const op of layout.ops) {
    ctx.fillStyle = palette.colors[op.color] ?? palette.colors.primary;
    if (op.kind === 'rect') {
      if (op.color === 'bg' || !ctx.roundRect) ctx.fillRect(op.x, op.y, op.w, op.h);
      else { ctx.beginPath(); ctx.roundRect(op.x, op.y, op.w, op.h, op.h / 2); ctx.fill(); }
      continue;
    }
    ctx.font = fontString(FONTS[op.font], palette.fonts);
    ctx.textAlign = op.align;
    ctx.fillText(op.text, op.x, op.y);
  }
  return { layout, missing: palette.missing };
}

/** The canvas as a PNG. */
export function cardBlob(canvas) {
  return new Promise((resolve, reject) => {
    canvas.toBlob((blob) => (blob ? resolve(blob) : reject(new Error('the card could not be made into a picture'))), 'image/png');
  });
}

/** Put the drawn card on the clipboard as an image/png; throws when the page cannot. */
export async function copyCard(canvas) {
  if (!navigator.clipboard?.write || typeof ClipboardItem === 'undefined') throw new Error('no clipboard');
  await navigator.clipboard.write([new ClipboardItem({ 'image/png': cardBlob(canvas) })]);
}
