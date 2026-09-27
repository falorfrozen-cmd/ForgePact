// The flatness rule the design asks for ("no unnecessary borders or multiple
// levels of element containers"), as one function that runs inside the page:
// tests/design-match.mjs hands it to page.evaluate against the panel, and
// tests/flatness-selftest.mjs against static pages with known answers.
//
// Over every element under <body> except input, button, select, textarea,
// .switch and their descendants:
//
//   border          an element with a border on any side - computed
//                   border-<side>-style not `none` and border-<side>-width
//                   above 0 - counts one, unless it matches a selector in the
//                   export's `borders` list;
//   card-in-card    every `[class*=card] [class*=card]` match counts one;
//   border-in-border every element with a border whose ancestor, below
//                   #workspace (not #workspace itself), also has one counts
//                   one - listed in `borders` or not, since the list allows a
//                   border, not a box inside a box.
//
// It must stay self-contained: page.evaluate serialises the function's text,
// so it can use nothing from this module's scope.
export function flatness(borders) {
  const EXEMPT = 'input, button, select, textarea, .switch';
  const SIDES = ['top', 'right', 'bottom', 'left'];
  const invalid = [];
  const allow = [];
  for (const entry of borders || []) {
    const selector = entry && entry.selector;
    if (!selector) continue;
    try { document.querySelector(selector); allow.push(selector); } catch { invalid.push(selector); }
  }
  const hasBorder = (el) => {
    const cs = getComputedStyle(el);
    return SIDES.some((side) => cs.getPropertyValue(`border-${side}-style`) !== 'none' &&
      parseFloat(cs.getPropertyValue(`border-${side}-width`)) > 0);
  };
  const describe = (el) => {
    let s = el.tagName.toLowerCase() + (el.id ? `#${el.id}` : '');
    for (const c of [...el.classList].slice(0, 3)) s += `.${c}`;
    const owner = el.parentElement && el.parentElement.closest('[id]');
    return owner && owner !== document.body ? `#${owner.id} ${s}` : s;
  };
  const workspace = document.getElementById('workspace');
  const violations = [];
  for (const el of document.body.querySelectorAll('*')) {
    if (el.closest(EXEMPT)) continue;
    const bordered = hasBorder(el);
    if (bordered && !allow.some((selector) => el.matches(selector))) violations.push({ rule: 'border', element: describe(el) });
    if (el.matches('[class*=card] [class*=card]')) violations.push({ rule: 'card-in-card', element: describe(el) });
    if (bordered && workspace && workspace.contains(el)) {
      for (let up = el.parentElement; up && up !== workspace; up = up.parentElement) {
        if (hasBorder(up)) { violations.push({ rule: 'border-in-border', element: `${describe(el)} in ${describe(up)}` }); break; }
      }
    }
  }
  return { count: violations.length, violations, invalid };
}
