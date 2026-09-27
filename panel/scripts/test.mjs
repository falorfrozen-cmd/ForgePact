// `npm test`: the panel's unit tests, `tests/*.test.js`, under node's own
// runner. They cover the plain-JS modules (poll policy, navigation, column
// balancer, enabled mods, theme) and the derived oracle against stub DOMs,
// in well under a second, with no browser.
//
// Explicit file paths, as hub/scripts/test.mjs does: supported Node 20 does
// not expand test globs, and directory arguments are not portable across Node
// versions. The browser suites are separate scripts (`oracle:replay`, `e2e`,
// `screens`) because they need Edge and a built dist/.

import { execFileSync } from 'node:child_process';
import { readdirSync } from 'node:fs';
import { dirname, join } from 'node:path';
import { fileURLToPath } from 'node:url';

const root = join(dirname(fileURLToPath(import.meta.url)), '..');
const testFiles = readdirSync(join(root, 'tests'), { withFileTypes: true })
  .filter((entry) => entry.isFile() && entry.name.endsWith('.test.js'))
  .map((entry) => join('tests', entry.name))
  .sort();
if (testFiles.length === 0) throw new Error('No unit test files found in tests/.');
execFileSync(process.execPath, ['--test', ...testFiles], { cwd: root, stdio: 'inherit' });
