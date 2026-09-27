// Proves tests/lib/design-flatness.mjs on static pages whose answer is known,
// before design-match.mjs trusts it on the panel: a rule that cannot report
// a border it should, or reports one it should not, would make the panel's
// `flatness: 0 violations` mean nothing.
//
//   node tests/flatness-selftest.mjs
//
// Each case is a page set with page.setContent (no sandbox, no dist) and the
// count the rule gives for it, worked out by hand from the rule and written
// beside it. Prints `ok|WRONG <case>: expected <n>, got <m>` per case and
// `flatness-selftest: <n> cases, <k> wrong` last; exits 1 when k > 0. Needs
// the installed Edge, as the other browser tools do.

import { flatness } from './lib/design-flatness.mjs';
import { launchBrowser } from './lib/browser.mjs';

const B = 'border:1px solid #888';

// [name, body html, borders list, expected count]
const CASES = [
  ['flat page', `<div id="workspace"><section><h2>Heading</h2><p>Row one</p><p>Row two</p></section></div>`, [], 0],
  // The div's border, clause 1.
  ['one unlisted border', `<div id="workspace"><div id="edge" style="${B}">x</div></div>`, [], 1],
  ['the same border, allowlisted', `<div id="workspace"><div id="edge" style="${B}">x</div></div>`, [{ selector: '#edge', why: 'test' }], 0],
  // Neither has a border: clause 2 alone, once for the inner card.
  ['a .card inside a .card', `<div id="workspace"><div class="card"><div class="card">x</div></div></div>`, [], 1],
  ['a class*=card inside another', `<div class="card tab-card"><div class="feature-card">x</div></div>`, [], 1],
  // Two borders (clause 1) plus the inner one inside a bordered box below
  // #workspace (clause 3).
  ['a border inside a bordered box below #workspace', `<div id="workspace"><div style="${B}"><div style="${B}">x</div></div></div>`, [], 3],
  // Both allowed by the list, but the nesting still counts.
  ['nested borders, both allowlisted', `<div id="workspace"><div class="a" style="${B}"><div class="b" style="${B}">x</div></div></div>`, [{ selector: '.a' }, { selector: '.b' }], 1],
  // #workspace itself is not "below #workspace": two borders, no nesting.
  ['a border directly inside a bordered #workspace', `<div id="workspace" style="${B}"><div style="${B}">x</div></div>`, [], 2],
  // Outside #workspace nesting is not counted: two borders only.
  ['nested borders outside #workspace', `<div id="workspace"></div><div style="${B}"><div style="${B}">x</div></div>`, [], 2],
  ['one side only', `<div id="workspace"><div style="border-bottom:1px solid #888">x</div></div>`, [], 1],
  ['zero width, and a style of none', `<div id="workspace"><div style="border:0 solid #888">x</div><div style="border:3px none #888">y</div></div>`, [], 0],
  // Exempt, with their descendants: UA borders on the controls, an explicit
  // one on each, and a bordered span inside a .switch and a button.
  ['bordered input, button, select, textarea and .switch', `<div id="workspace"><input style="${B}"><button style="${B}"><span style="${B}">b</span></button><select style="${B}"><option>o</option></select><textarea style="${B}"></textarea><label class="switch" style="${B}"><span class="sl" style="${B}"></span></label></div>`, [], 0],
];

const browser = await launchBrowser();
let wrong = 0;
try {
  const page = await browser.newPage();
  for (const [name, html, borders, expected] of CASES) {
    await page.setContent(`<!doctype html><html><head><style>body{margin:0}</style></head><body>${html}</body></html>`);
    const got = await page.evaluate(flatness, borders);
    const bad = got.count !== expected;
    if (bad) wrong++;
    console.log(`${bad ? 'WRONG' : 'ok'} ${name}: expected ${expected}, got ${got.count}` +
      (bad ? ` (${got.violations.map((v) => `${v.rule} ${v.element}`).join('; ')})` : ''));
  }
} finally {
  await browser.close();
}
console.log(`flatness-selftest: ${CASES.length} cases, ${wrong} wrong`);
process.exitCode = wrong ? 1 : 0;
