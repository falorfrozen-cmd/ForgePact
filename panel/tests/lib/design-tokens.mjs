// The token half of tests/design-match.mjs: what each selectorTokens entry
// should compute to in each palette, the in-page measurement, and the
// comparison.
//
// The expected value is not compared as text against the computed one. The
// token's CSS (scripts/tokens-from-export.mjs's unit rules) is set on a hidden
// probe element and the browser's own computed value for it is read back, so
// both sides come out of the same serialiser: `#100d0bf7` and the page's
// `var(--bg)` both read `rgba(16, 13, 11, 0.97)`, a unitless line height is
// turned into px at the element's own font size, a box-shadow is normalised
// the same way on both sides. Font families compare by their first family.

import { cssValue, firstFamily, isFamilyName, paletteVariables } from '../../scripts/tokens-from-export.mjs';

// For each entry, the token's CSS text per palette (export order, default
// first), or an error saying why there is none.
export function expectedCss(exp, entries, paletteNames) {
  return entries.map((entry) => paletteNames.map((name) => {
    const v = paletteVariables(exp, name).get(entry.variable);
    if (!v) return { error: `${entry.variable} is not a variable of palette ${name}` };
    try { return { css: cssValue(v), type: v.type, name: v.name }; } catch (e) { return { error: e.message }; }
  }));
}

// Runs in the page; must stay self-contained (page.evaluate serialises it).
// `list`: [{ selector, property, css: [text|null per theme] }]. `themes`:
// null for "as the page painted it", then a data-theme name per other
// palette. With `qualify`, a selector that does not match but is an
// existing element plus a trailing .class or [attr] / [attr=value] gets that
// qualifier applied to the element for its measurement, and taken off after.
// Returns per entry null (not found in this state) or { actual[], expected[],
// qualifier }.
export function measureTokens({ list, themes, qualify }) {
  const root = document.documentElement;
  const hadTheme = root.hasAttribute('data-theme');
  const wasTheme = root.getAttribute('data-theme');
  const settle = () => { for (const a of document.getAnimations()) { try { a.finish(); } catch { /* an infinite animation */ } } };
  const paint = (theme) => {
    if (theme !== null) root.setAttribute('data-theme', theme);
    else if (hadTheme) root.setAttribute('data-theme', wasTheme);
    else root.removeAttribute('data-theme');
    settle();
  };
  const PROBE = 'position:absolute;left:-10000px;top:0;visibility:hidden;pointer-events:none';
  const probe = document.createElement('div');
  probe.setAttribute('aria-hidden', 'true');
  document.body.append(probe);

  // "#id.error" -> { base: "#id", apply, undo }; null when not of that shape
  // or when the base matches nothing.
  const qualifierOf = (selector) => {
    const m = /^(.*[^\s>+~,])(\.[A-Za-z_][\w-]*|\[[^\]]+\])$/.exec(selector);
    if (!m) return null;
    let el;
    try { el = document.querySelector(m[1]); } catch { return null; }
    if (!el) return null;
    if (m[2].startsWith('.')) {
      const cls = m[2].slice(1);
      if (el.classList.contains(cls)) return null;
      return { text: m[2], apply: () => el.classList.add(cls), undo: () => el.classList.remove(cls) };
    }
    const a = /^\[\s*([\w-]+)\s*(?:=\s*(["']?)(.*?)\2)?\s*\]$/.exec(m[2]);
    if (!a) return null;
    const had = el.hasAttribute(a[1]);
    const was = el.getAttribute(a[1]);
    return {
      text: m[2],
      apply: () => el.setAttribute(a[1], a[3] ?? ''),
      undo: () => (had ? el.setAttribute(a[1], was) : el.removeAttribute(a[1])),
    };
  };

  const measureOne = (entry) => {
    let el;
    try { el = document.querySelector(entry.selector); } catch { return null; }
    if (!el) return null;
    const result = { actual: [], expected: [], qualifier: null };
    themes.forEach((theme, t) => {
      paint(theme);
      const cs = getComputedStyle(el);
      result.actual[t] = cs.getPropertyValue(entry.property).trim();
      const css = entry.css[t];
      if (css === null) { result.expected[t] = null; return; }
      probe.style.cssText = PROBE;
      if (entry.property !== 'font-size') probe.style.fontSize = cs.fontSize;
      probe.style.setProperty(entry.property, css);
      // An invalid value is dropped by the parser, and the probe would then
      // report whatever it inherited: say so instead.
      result.expected[t] = probe.style.getPropertyValue(entry.property) === ''
        ? { invalid: css }
        : getComputedStyle(probe).getPropertyValue(entry.property).trim();
    });
    paint(null);
    return result;
  };

  try {
    return list.map((entry) => {
      if (!qualify) return measureOne(entry);
      const q = qualifierOf(entry.selector);
      if (!q) return null;
      q.apply();
      try {
        const r = measureOne(entry);
        if (r) r.qualifier = q.text;
        return r;
      } finally {
        q.undo();
        settle();
      }
    });
  } finally {
    paint(null);
    probe.remove();
  }
}

const squash = (s) => String(s).trim().replace(/\s+/g, ' ');

// { ok, expected, actual, why } for one entry in one palette.
export function compareToken(entry, want, actual, expected) {
  if (want.error) return { ok: false, expected: '-', actual, why: want.error };
  if (expected && typeof expected === 'object') return { ok: false, expected: want.css, actual, why: `${want.css} is not a valid ${entry.property}` };
  if (entry.property === 'font-family' || isFamilyName(want.name)) {
    const ok = firstFamily(actual).toLowerCase() === firstFamily(expected).toLowerCase();
    return { ok, expected: firstFamily(expected), actual: firstFamily(actual), why: ok ? '' : 'first family differs' };
  }
  const ok = squash(actual) === squash(expected);
  return { ok, expected, actual, why: ok ? '' : 'differs' };
}
