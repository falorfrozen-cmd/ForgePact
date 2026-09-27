// WCAG AA contrast over the Figma export's contrastPairs, in every palette.
//
//   node scripts/contrast-check.mjs <export.json>
//
// For each palette (export order) and each contrastPairs entry, resolves fg
// and bg to that palette's colours - its own COLOR variables over the default
// palette's of the same name - and computes the WCAG 2.x contrast ratio from
// relative luminance. A bg with alpha is composited over the palette's
// `color/bg/base` first (an error if that is absent or itself translucent);
// an fg with alpha is then composited over the resolved bg. The unrounded
// ratio is compared with minRatio, so 4.499 fails a 4.5. Prints
//
//   PASS|FAIL <palette> <fg> on <bg> <ratio to 2 dp> (min <n>) — <where>
//
// per pair and palette, then `contrast: <pairs> pairs x <palettes> palettes,
// <k> failing` last. Exits 1 when k > 0, when the export has no pairs, or when
// it does not parse. Nothing here knows a palette by name.

import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { here, loadExport, paletteVariables } from './tokens-from-export.mjs';

const BASE = 'color/bg/base';

// '#rrggbb' / '#rrggbbaa' -> { r, g, b } in 0..255 and a in 0..1.
export function parseHex(hex) {
  const m = /^#([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})([0-9a-f]{2})?$/i.exec(String(hex));
  if (!m) throw new Error(`${JSON.stringify(hex)} is not #rrggbb or #rrggbbaa`);
  return { r: parseInt(m[1], 16), g: parseInt(m[2], 16), b: parseInt(m[3], 16), a: m[4] === undefined ? 1 : parseInt(m[4], 16) / 255 };
}

// Source-over: `top` painted on an opaque `bottom`. Channels stay unrounded.
export function composite(top, bottom) {
  const a = top.a;
  return {
    r: top.r * a + bottom.r * (1 - a),
    g: top.g * a + bottom.g * (1 - a),
    b: top.b * a + bottom.b * (1 - a),
    a: 1,
  };
}

const linear = (c) => {
  const s = c / 255;
  return s <= 0.04045 ? s / 12.92 : ((s + 0.055) / 1.055) ** 2.4;
};

export function relativeLuminance({ r, g, b }) {
  return 0.2126 * linear(r) + 0.7152 * linear(g) + 0.0722 * linear(b);
}

// Ratio of two colours (hex or parsed). A translucent fg is composited over
// bg; bg must be opaque here - resolve it first.
export function contrastRatio(fg, bg) {
  const back = typeof bg === 'string' ? parseHex(bg) : bg;
  if (back.a < 1) throw new Error('bg is translucent; composite it over its surface first');
  let front = typeof fg === 'string' ? parseHex(fg) : fg;
  if (front.a < 1) front = composite(front, back);
  const [hi, lo] = [relativeLuminance(front), relativeLuminance(back)].sort((x, y) => y - x);
  return (hi + 0.05) / (lo + 0.05);
}

function colour(vars, name) {
  const v = vars.get(name);
  if (!v) throw new Error(`${name} is not a variable of this palette`);
  if (v.type !== 'COLOR') throw new Error(`${name} is ${v.type}, not COLOR`);
  return parseHex(v.value);
}

// Every pair in every palette: { palette, fg, bg, minRatio, where, ratio,
// pass, error }. `ratio` is null when the pair could not be resolved, and
// such a pair fails.
export function checkContrast(exp) {
  const pairs = Array.isArray(exp.contrastPairs) ? exp.contrastPairs : [];
  const results = [];
  for (const palette of exp.palettes) {
    const vars = paletteVariables(exp, palette.name);
    for (const pair of pairs) {
      const row = { palette: palette.name, fg: pair.fg, bg: pair.bg, minRatio: Number(pair.minRatio), where: pair.where || '', ratio: null, pass: false, error: null };
      try {
        if (!Number.isFinite(row.minRatio)) throw new Error(`minRatio ${JSON.stringify(pair.minRatio)} is not a number`);
        let bg = colour(vars, pair.bg);
        if (bg.a < 1) {
          if (!vars.has(BASE)) throw new Error(`${pair.bg} has alpha and ${BASE} is absent`);
          const base = colour(vars, BASE);
          if (base.a < 1) throw new Error(`${pair.bg} has alpha and ${BASE} is itself translucent`);
          bg = composite(bg, base);
        }
        row.ratio = contrastRatio(colour(vars, pair.fg), bg);
        row.pass = row.ratio >= row.minRatio;
      } catch (e) {
        row.error = e.message;
      }
      results.push(row);
    }
  }
  return { pairs: pairs.length, palettes: exp.palettes.length, results, failing: results.filter((r) => !r.pass).length };
}

export function formatResult(r) {
  const ratio = r.ratio === null ? `error: ${r.error}` : r.ratio.toFixed(2);
  return `${r.pass ? 'PASS' : 'FAIL'} ${r.palette} ${r.fg} on ${r.bg} ${ratio} (min ${r.minRatio}) — ${r.where}`;
}

function main(argv) {
  if (argv.length !== 1) {
    console.error('usage: contrast-check.mjs <export.json>');
    return 2;
  }
  let report;
  try {
    report = checkContrast(loadExport(here(argv[0])));
  } catch (e) {
    console.error(`contrast: cannot check ${argv[0]}: ${e.message}`);
    return 1;
  }
  for (const r of report.results) console.log(formatResult(r));
  console.log(`contrast: ${report.pairs} pairs x ${report.palettes} palettes, ${report.failing} failing`);
  if (report.pairs === 0) {
    console.error('contrast: the export lists no contrastPairs, so nothing was checked');
    return 1;
  }
  return report.failing > 0 ? 1 : 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exitCode = main(process.argv.slice(2));
}
