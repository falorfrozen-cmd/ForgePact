// design-match's structure checks, which compare an export's shape with the
// panel's page and never a pixel or a token:
//
//   placement  each `selectorTokens` entry that names a `component` (an entry
//              of the export's `components` list carrying a `selector`, the
//              element that component is drawn as) must have its first match
//              inside that element on every screen where it matches, and must
//              match on at least one screen. `placeEntries` runs in the page.
//   runtime    no screen text or placeholder may carry a value the running
//              panel supplies: a version string (the panel's own, or any
//              x.y.z), a filesystem path, or a port.
//
// Why: in the ForgePact UI redesign (2026-09-24..27) the Figma export
// disagreed with the panel's DOM seven times - value strings, 68 texts, a bare
// `.val` selector that matched outside its stepper, a font path, a baked-in
// version - and each was found only at the restyle step, two workorders later.

// Each entry's component selector, or why it has none. An entry without
// `component` is unscoped: placement does not check it.
export function scopedEntries(exp) {
  const components = new Map((Array.isArray(exp.components) ? exp.components : []).map((c) => [c.name, c]));
  const scoped = [];
  const errors = [];
  let unscoped = 0;
  for (const entry of Array.isArray(exp.selectorTokens) ? exp.selectorTokens : []) {
    if (entry.component === undefined) { unscoped++; continue; }
    const c = components.get(entry.component);
    if (!c) errors.push(`${entry.selector}: component ${JSON.stringify(entry.component)} is not in the export's components`);
    else if (typeof c.selector !== 'string' || !c.selector.trim()) errors.push(`${entry.selector}: component ${JSON.stringify(entry.component)} has no selector`);
    else scoped.push({ selector: entry.selector, component: entry.component, within: c.selector });
  }
  // The same selector and component listed for several properties is one placement.
  const seen = new Set();
  const unique = scoped.filter((s) => { const k = `${s.selector}\u0000${s.component}`; if (seen.has(k)) return false; seen.add(k); return true; });
  return { scoped: unique, unscoped, errors };
}

// In the page: for each { selector, within }, where its first match is.
// `found` false when nothing matches; `inside` whether that first match is
// the component element or inside one; `where` the match and up to three
// ancestors, for the failure line.
export function placeEntries(list) {
  const label = (el) => el.tagName.toLowerCase() + (el.id ? `#${el.id}` : '') +
    [...el.classList].slice(0, 2).map((c) => `.${c}`).join('');
  return list.map(({ selector, within }) => {
    let el;
    try { el = document.querySelector(selector); } catch { return { invalid: 'selector' }; }
    if (!el) return { found: false };
    let inside;
    try { inside = !!el.closest(within); } catch { return { invalid: 'component selector' }; }
    const chain = [];
    for (let n = el; n && n !== document.body && chain.length < 4; n = n.parentElement) chain.unshift(label(n));
    return { found: true, inside, where: chain.join(' > ') };
  });
}

// The runtime values a text carries, as [{ kind, value }]. `version` is the
// panel's own (src/forgepact.py `__version__`), `ports` the ports it may
// serve on (`PORT_CANDIDATES`); either may be empty.
export function runtimeValues(text, { version = '', ports = [] } = {}) {
  const s = String(text);
  const out = [];
  const add = (kind, re) => { for (const m of s.matchAll(re)) out.push({ kind, value: m[0].trim() }); };
  if (version && s.includes(version)) out.push({ kind: 'version', value: version });
  add('version', /\bv?\d+\.\d+\.\d+(?:-[0-9A-Za-z.]+)?\b/g);
  add('path', /[A-Za-z]:[\\/][^\s,;)]*/g); // C:\Program Files\..., D:/games
  add('path', /\\\\[^\s\\]+\\[^\s,;)]*/g); // \\server\share
  add('path', /%[A-Z_]+%[^\s,;)]*/g); // %LOCALAPPDATA%\Hero_Siege
  add('path', /(?:^|[\s(])((?:~|\.{1,2})?\/[\w.-]+\/[\w./-]*)/g); // /usr/share/x, ~/x/y, ./a/b
  add('path', /(?:^|[\s(])([\w.-]+\\[\w.-]+(?:\\[\w.-]*)*)/g); // bin\bp_ipc\cmd.txt
  add('port', /\b(?:localhost|127\.0\.0\.1)(?::\d{1,5})?\b/g);
  add('port', /\bport\s+\d{2,5}\b/gi);
  for (const p of ports) add('port', new RegExp(`(?<![\\d.])${p}(?![\\d.])`, 'g'));
  const seen = new Set();
  return out.filter((r) => { const k = `${r.kind}\u0000${r.value}`; if (seen.has(k)) return false; seen.add(k); return true; });
}

// `__version__` and `PORT_CANDIDATES` read out of src/forgepact.py's text;
// what cannot be read is empty, and the generic patterns still apply.
export function panelRuntime(sourceText) {
  const version = (/^__version__\s*=\s*['"]([^'"]+)['"]/m.exec(sourceText) || [])[1] || '';
  const list = (/^PORT_CANDIDATES\s*=\s*\[([^\]]*)\]/m.exec(sourceText) || [])[1] || '';
  const ports = list.split(',').map((p) => p.trim()).filter((p) => /^\d+$/.test(p));
  return { version, ports };
}
