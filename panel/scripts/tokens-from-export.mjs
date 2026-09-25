// tokens.css from the Figma export (panel/design/figma-export.json).
//
//   node scripts/tokens-from-export.mjs <export.json> [--out <file>]
//
// Writes the CSS to stdout, or to --out. The default palette (the one with
// `default: true`) is `:root`, every variable of its mode, sorted by name;
// each other palette follows, in export order, as a
// `:root[data-theme="<name>"]` block holding only that mode's COLOR
// variables. A custom property is `--` plus the variable name with `/` turned
// into `-` (`color/bg/base` -> `--color-bg-base`). Units:
//
//   COLOR                                   the hex as exported
//   FLOAT space/, radius/, font/size/       <n>px
//   FLOAT font/weight/, line-height/,       <n>, unitless
//         font/line-height/
//   FLOAT motion/duration/                  <n>ms
//   STRING font/<role>/family               quoted: the first family of the
//                                           stack is quoted, the rest kept
//   STRING anything else (motion/easing/,   raw
//          shadow/)
//
// A FLOAT under any other prefix has no unit rule and is an error rather than
// a guess. Nothing here knows a palette by name: names, their number and
// every value are the export's data.
//
// The module also exports what tests/tokens.test.js, scripts/contrast-check.mjs
// and tests/design-match.mjs share: the value rules, the palette lookup and a
// parser/comparator for a tokens.css.

import { readFileSync, writeFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const HEX = /^#[0-9a-f]{6}([0-9a-f]{2})?$/i;
const PALETTE_NAME = /^[a-z][a-z0-9-]{0,31}$/;

// Relative paths are the caller's, as in tests/screens.mjs.
export const here = (p) => resolve(process.env.INIT_CWD || process.cwd(), p);

export function loadExport(file) {
  return JSON.parse(readFileSync(file, 'utf8'));
}

export function propertyName(name) {
  return '--' + String(name).replace(/\//g, '-');
}

// Figma stores FLOATs as float32, so 1.2 arrives as 1.2000000476837158.
function number(value, name) {
  const n = Number(value);
  if (!Number.isFinite(n)) throw new Error(`${name}: FLOAT value ${JSON.stringify(value)} is not a number`);
  return String(Math.round(n * 1e4) / 1e4);
}

export function isFamilyName(name) {
  return /^font\/[^/]+\/family$/.test(name);
}

// `Family, "Segoe UI", sans-serif` -> `"Family", "Segoe UI", sans-serif`. A
// value that already carries quotes is taken as a finished stack.
export function quoteFamily(value) {
  const text = String(value).trim();
  if (/["']/.test(text)) return text;
  const [first, ...rest] = text.split(',').map((s) => s.trim()).filter(Boolean);
  if (!first) throw new Error('empty font family');
  return [`"${first}"`, ...rest].join(', ');
}

// The first family of a stack, unquoted: what the browser reports first and
// what a comparison of families compares.
export function firstFamily(stack) {
  const first = String(stack).split(',')[0] || '';
  return first.trim().replace(/^["']|["']$/g, '').trim();
}

export function cssValue({ name, type, value }) {
  if (type === 'COLOR') {
    if (!HEX.test(String(value))) throw new Error(`${name}: COLOR value ${JSON.stringify(value)} is not #rrggbb or #rrggbbaa`);
    return String(value);
  }
  if (type === 'FLOAT') {
    if (/^(space|radius|font\/size)\//.test(name)) return `${number(value, name)}px`;
    if (/^(font\/weight|line-height|font\/line-height)\//.test(name)) return number(value, name);
    if (/^motion\/duration\//.test(name)) return `${number(value, name)}ms`;
    throw new Error(`${name}: no unit rule for a FLOAT under this name`);
  }
  if (type === 'STRING') return isFamilyName(name) ? quoteFamily(value) : String(value).trim();
  throw new Error(`${name}: unknown variable type ${JSON.stringify(type)}`);
}

export function defaultPalette(exp) {
  const palettes = exp?.palettes;
  if (!Array.isArray(palettes) || palettes.length === 0) throw new Error('the export has no palettes');
  for (const p of palettes) {
    if (!PALETTE_NAME.test(String(p?.name))) throw new Error(`palette name ${JSON.stringify(p?.name)} is not a lowercase theme name`);
  }
  const defaults = palettes.filter((p) => p.default === true);
  if (defaults.length !== 1) throw new Error(`the export needs exactly one default palette, found ${defaults.length}`);
  return defaults[0];
}

// One mode's variables by name; a name listed twice in one mode is an error.
export function modeVariables(exp, mode) {
  const out = new Map();
  for (const v of exp.variables || []) {
    if (v.mode !== mode) continue;
    if (out.has(v.name)) throw new Error(`${v.name} is listed twice in mode ${mode}`);
    out.set(v.name, v);
  }
  return out;
}

// What a palette resolves to: the default palette's variables, with this
// palette's COLOR variables in place of the default's of the same name.
export function paletteVariables(exp, paletteName) {
  const base = defaultPalette(exp);
  const merged = new Map(modeVariables(exp, base.name));
  if (paletteName !== base.name) {
    for (const [name, v] of modeVariables(exp, paletteName)) if (v.type === 'COLOR') merged.set(name, v);
  }
  return merged;
}

const byName = (a, b) => (a.name < b.name ? -1 : a.name > b.name ? 1 : 0);

function block(selector, variables) {
  const lines = variables.sort(byName).map((v) => `  ${propertyName(v.name)}: ${cssValue(v)};`);
  return `${selector} {\n${lines.join('\n')}\n}\n`;
}

export function generateTokensCss(exp) {
  const base = defaultPalette(exp);
  const rootVars = [...modeVariables(exp, base.name).values()];
  if (rootVars.length === 0) throw new Error(`the default palette ${base.name} has no variables`);
  const parts = [
    '/* Generated by scripts/tokens-from-export.mjs from the Figma export; regenerate it, do not edit it. */\n',
    block(':root', rootVars),
  ];
  for (const p of exp.palettes) {
    if (p === base) continue;
    const colors = [...modeVariables(exp, p.name).values()].filter((v) => v.type === 'COLOR');
    parts.push(block(`:root[data-theme="${p.name}"]`, colors));
  }
  return parts.join('\n');
}

// A tokens.css as Map(selector -> Map(property -> value)). Selectors have
// their whitespace collapsed and single quotes made double, so
// `:root[data-theme='x']` and `:root[data-theme="x"]` are one block.
export function parseTokensCss(css) {
  const blocks = new Map();
  const text = String(css).replace(/\/\*[\s\S]*?\*\//g, '');
  for (const m of text.matchAll(/([^{}]+)\{([^{}]*)\}/g)) {
    const selector = m[1].trim().replace(/\s+/g, ' ').replace(/'/g, '"');
    const decls = blocks.get(selector) || new Map();
    for (const decl of m[2].split(';')) {
      const colon = decl.indexOf(':');
      if (colon < 0) continue;
      const prop = decl.slice(0, colon).trim();
      const value = decl.slice(colon + 1).trim().replace(/\s+/g, ' ');
      if (prop) decls.set(prop, value);
    }
    blocks.set(selector, decls);
  }
  return blocks;
}

function sameValue(prop, a, b) {
  if (/^--font-.+-family$/.test(prop)) return firstFamily(a).toLowerCase() === firstFamily(b).toLowerCase();
  if (HEX.test(a) && HEX.test(b)) return a.toLowerCase() === b.toLowerCase();
  return a === b;
}

// Every difference between two parsed tokens.css files, as
// { kind, selector, property?, expected?, actual? }: kind is missing-block,
// extra-block, missing, extra or different. Font families compare by their
// first family, so a stack may grow fallbacks without being a difference.
export function compareTokens(expected, actual) {
  const out = [];
  for (const [selector, decls] of expected) {
    const got = actual.get(selector);
    if (!got) { out.push({ kind: 'missing-block', selector }); continue; }
    for (const [property, value] of decls) {
      if (!got.has(property)) out.push({ kind: 'missing', selector, property, expected: value });
      else if (!sameValue(property, value, got.get(property))) out.push({ kind: 'different', selector, property, expected: value, actual: got.get(property) });
    }
    for (const [property, value] of got) if (!decls.has(property)) out.push({ kind: 'extra', selector, property, actual: value });
  }
  for (const selector of actual.keys()) if (!expected.has(selector)) out.push({ kind: 'extra-block', selector });
  return out;
}

function main(argv) {
  const positional = [];
  let out = null;
  for (let i = 0; i < argv.length; i++) {
    if (argv[i] === '--out') out = argv[++i];
    else positional.push(argv[i]);
  }
  if (positional.length !== 1 || out === undefined) {
    console.error('usage: tokens-from-export.mjs <export.json> [--out <file>]');
    return 2;
  }
  let css;
  try {
    css = generateTokensCss(loadExport(here(positional[0])));
  } catch (e) {
    console.error(`tokens-from-export: ${e.message}`);
    return 1;
  }
  if (out) {
    writeFileSync(here(out), css);
    console.log(`tokens-from-export: wrote ${here(out)}`);
  } else {
    process.stdout.write(css);
  }
  return 0;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  process.exitCode = main(process.argv.slice(2));
}
