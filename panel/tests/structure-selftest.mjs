// design-match's structure-only mode (--structure: texts, placement, runtime)
// run end to end on two fixture exports against the built panel:
//
//   export-structure.json        texts the seeded page shows, `.tabbtn` inside
//                                the Rail component and `.enabled-mod` and
//                                `#enabledModsCount` inside EnabledMods: exit 0.
//   export-structure-wrong.json  the same with one text renamed ("World
//                                settings" -> "World options") and
//                                `#controlSearch` placed in EnabledMods,
//                                where the page does not draw it: exit 1,
//                                both named with their screen.
//
// Each case's expected lines are written beside it. Last line
// `structure-selftest: <n> cases, <k> wrong`, exit 1 when k > 0. Needs
// `npm ci`, `npm run build` and Edge, like design-match itself.
import { spawnSync } from 'node:child_process';
import { mkdtempSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import { fileURLToPath } from 'node:url';

const TOOL = fileURLToPath(new URL('./design-match.mjs', import.meta.url));
const FIXTURES = fileURLToPath(new URL('./fixtures/design/', import.meta.url));

const CASES = [
  {
    export: 'export-structure.json',
    exit: 0,
    lines: ['texts: 0 missing', 'placement: 0 misplaced, 0 not found, 1 unscoped', 'runtime: 0 values',
      'tokens: skipped', 'flatness: skipped', 'composites: skipped'],
    absent: [/^placement \S+: /, /^texts \S+: missing/],
  },
  {
    export: 'export-structure-wrong.json',
    exit: 1,
    lines: ['texts: 1 missing', 'texts world-1280: missing "World options"',
      'placement: 2 misplaced, 0 not found, 1 unscoped', 'runtime: 0 values'],
    matching: [/^placement world-1280: #controlSearch is not inside EnabledMods \(#enabledMods\); first match: .*input#controlSearch/,
      /^placement mods-items-900: #controlSearch is not inside EnabledMods/],
  },
];

let wrong = 0;
const out = mkdtempSync(join(tmpdir(), 'structure-selftest-'));
try {
  for (const c of CASES) {
    const r = spawnSync(process.execPath, [TOOL, '--structure', '--export', join(FIXTURES, c.export), '--out', join(out, c.export)],
      { encoding: 'utf8' });
    const lines = (r.stdout || '').split(/\r?\n/);
    const problems = [];
    if (r.status !== c.exit) problems.push(`exit ${r.status}, want ${c.exit}`);
    for (const want of c.lines) if (!lines.includes(want)) problems.push(`no line ${JSON.stringify(want)}`);
    for (const re of c.matching || []) if (!lines.some((l) => re.test(l))) problems.push(`no line matching ${re}`);
    for (const re of c.absent || []) if (lines.some((l) => re.test(l))) problems.push(`unexpected line matching ${re}`);
    if (problems.length) {
      wrong++;
      console.log(`WRONG ${c.export}: ${problems.join('; ')}`);
      console.log((r.stdout || '') + (r.stderr || ''));
    } else console.log(`ok ${c.export}: exit ${r.status}`);
  }
} finally {
  rmSync(out, { recursive: true, force: true });
}
console.log(`structure-selftest: ${CASES.length} cases, ${wrong} wrong`);
process.exitCode = wrong ? 1 : 0;
