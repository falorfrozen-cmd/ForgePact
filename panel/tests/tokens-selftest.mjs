// Proves the variant rule in tests/lib/design-tokens.mjs on static pages whose
// answer is known, before design-match.mjs trusts it on the panel: a bare
// selector the export lists beside a variant of it (`.val` and `.val.off`)
// must be measured on an element in none of the listed variants, or the
// default token is compared against a variant's colour and a mismatch (or a
// match) means nothing.
//
//   node tests/tokens-selftest.mjs
//
// Each case is a page set with page.setContent (no sandbox, no dist), a
// selectorTokens-shaped list with literal expected colours, and the number of
// mismatches the rule gives for it (a selector not found counts as one),
// worked out by hand and written beside it. It calls the same variantsOf,
// measureTokens and compareToken that design-match.mjs uses. Prints
// `ok|WRONG <case>: expected <n>, got <m>` per case and
// `tokens-selftest: <n> cases, <k> wrong` last; exits 1 when k > 0. Needs the
// installed Edge, as the other browser tools do.

import { launchBrowser } from './lib/browser.mjs';
import { compareToken, measureTokens, variantsOf } from './lib/design-tokens.mjs';

const VAL_PAGE = '<span class="val off" style="color:rgb(1,2,3)">off</span><span class="val" style="color:rgb(4,5,6)">4</span>';

// [name, body html, [[selector, expected colour]...], expected mismatches]
const CASES = [
  ['a variant before its bare selector', VAL_PAGE, [['.val', 'rgb(4, 5, 6)'], ['.val.off', 'rgb(1, 2, 3)']], 0],
  // The rule picks the plain .val, so each expectation is now the other one's.
  ['the same page, expectations swapped', VAL_PAGE, [['.val', 'rgb(1, 2, 3)'], ['.val.off', 'rgb(4, 5, 6)']], 2],
  ['an attribute variant before its bare selector',
    '<b class="t" aria-selected="true" style="color:rgb(7,8,9)">a</b><b class="t" style="color:rgb(10,11,12)">b</b>',
    [['.t', 'rgb(10, 11, 12)'], ['.t[aria-selected="true"]', 'rgb(7, 8, 9)']], 0],
  // Every .val is .off, so the bare .val matches no element: not found.
  ['only the variant on the page', '<span class="val off" style="color:rgb(1,2,3)">off</span>', [['.val', 'rgb(1, 2, 3)'], ['.val.off', 'rgb(1, 2, 3)']], 1],
  // `.card h2` is a descendant, not a variant, so .card is measured on the first .card.
  ['a descendant selector is not a variant',
    '<div class="card" style="color:rgb(20,21,22)"><h2 style="color:rgb(23,24,25)">h</h2></div>',
    [['.card', 'rgb(20, 21, 22)'], ['.card h2', 'rgb(23, 24, 25)']], 0],
  // No variant listed: the first match, as before the rule.
  ['no variant listed', VAL_PAGE, [['.val', 'rgb(1, 2, 3)']], 0],
  // The export's value-box shape: a child combinator and its `.off` variant.
  // The bare `.val` outside any stepper (the panel's `CT only` value) comes
  // last and non-off, and must never be the pick for `.value-stepper>.val`.
  ['a child-combinator selector and its variant',
    '<div class="value-stepper"><span class="val off" style="color:rgb(30,31,32)">off</span></div>'
      + '<div class="value-stepper"><span class="val" style="color:rgb(33,34,35)">x3</span></div>'
      + '<span class="val" style="color:rgb(36,37,38)">CT only</span>',
    [['.value-stepper>.val', 'rgb(33, 34, 35)'], ['.value-stepper>.val.off', 'rgb(30, 31, 32)']], 0],
];

const browser = await launchBrowser();
let wrong = 0;
try {
  const page = await browser.newPage();
  for (const [name, html, rows, expected] of CASES) {
    await page.setContent(`<!doctype html><html><head><style>body{margin:0}</style></head><body>${html}</body></html>`);
    const selectors = rows.map(([s]) => s);
    const list = rows.map(([selector, css]) => ({ selector, property: 'color', css: [css], variants: variantsOf(selector, selectors) }));
    const got = await page.evaluate(measureTokens, { list, themes: [null], qualify: false });
    const misses = [];
    got.forEach((r, i) => {
      const [selector, css] = rows[i];
      if (!r) { misses.push(`${selector} not found`); return; }
      const c = compareToken({ selector, property: 'color' }, { css }, r.actual[0], r.expected[0]);
      if (!c.ok) misses.push(`${selector} ${c.actual} != ${c.expected}`);
    });
    const bad = misses.length !== expected;
    if (bad) wrong++;
    console.log(`${bad ? 'WRONG' : 'ok'} ${name}: expected ${expected}, got ${misses.length}` + (misses.length ? ` (${misses.join('; ')})` : ''));
  }
} finally {
  await browser.close();
}
console.log(`tokens-selftest: ${CASES.length} cases, ${wrong} wrong`);
process.exitCode = wrong ? 1 : 0;
