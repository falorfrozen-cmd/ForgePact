// Screenshots of every tab at two widths, and a pixel comparison between two
// sets of them.
//
//   node tests/screens.mjs --out <dir> [--legacy] [--dist <dir>]
//                          [--compare <dir> --max-mismatch <ratio>]
//
// For each tab (setup, modifiers, world, loot, and the Mods tab's two
// sub-tabs, mods-qol and mods-items) at 1280x800 and 900x700 this writes a
// full-page `<tab>-<w>.png` and the page's accessibility tree as
// `<tab>-<w>.txt`. With --compare it runs pixelmatch on each pair against the
// same names in <dir>, prints `<name> mismatch=<ratio>` per pair, and exits 1
// if any ratio exceeds --max-mismatch (default 0) or a pair is missing.

import { existsSync, mkdirSync, readFileSync, writeFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import pixelmatch from 'pixelmatch';
import { PNG } from 'pngjs';
import { VIEWPORTS, launchBrowser, openPanel, parseArgs, startSandbox, waitSaved } from './lib/browser.mjs';

// Relative paths are the caller's: `npm --prefix ForgePact/panel run screens`
// runs this with the panel directory as cwd, and INIT_CWD is where npm was run.
const here = (p) => resolve(process.env.INIT_CWD || process.cwd(), p);

const SHOTS = [
  ['setup', 'setup', null],
  ['modifiers', 'modifiers', null],
  ['world', 'world', null],
  ['loot', 'loot', null],
  ['mods-qol', 'mods', 'qol'],
  ['mods-items', 'mods', 'items'],
];

async function capture(args) {
  const out = here(args.out);
  mkdirSync(out, { recursive: true });
  const sandbox = await startSandbox({ legacy: !!args.legacy, dist: args.dist });
  const browser = await launchBrowser();
  const written = [];
  try {
    for (const [width, viewport] of Object.entries(VIEWPORTS)) {
      const page = await openPanel(browser, sandbox, viewport);
      for (const [name, tab, sub] of SHOTS) {
        await page.click(`.tabbtn[data-tab="${tab}"]`);
        if (sub) await page.click(`#subtab-${sub}`);
        // Park the pointer where nothing reacts to hover, let the column
        // balancer and fonts settle, and wait out any poll-driven repaint.
        await page.mouse.move(0, 0);
        await waitSaved(page);
        await page.evaluate(() => document.fonts.ready);
        await page.waitForTimeout(400);
        const file = join(out, `${name}-${width}.png`);
        await page.screenshot({ path: file, fullPage: true, animations: 'disabled', caret: 'hide' });
        writeFileSync(join(out, `${name}-${width}.txt`), await page.locator('body').ariaSnapshot() + '\n');
        written.push(file);
      }
      await page.context().close();
    }
  } finally {
    await browser.close();
    await sandbox.stop();
  }
  console.log(`screens: ${written.length} captured in ${out}`);
  return written;
}

function compare(outDir, againstDir, maxMismatch) {
  let failed = 0;
  for (const [name] of SHOTS) {
    for (const width of Object.keys(VIEWPORTS)) {
      const file = `${name}-${width}.png`;
      const a = join(againstDir, file);
      const b = join(outDir, file);
      if (!existsSync(a) || !existsSync(b)) {
        console.log(`${name}-${width} mismatch=missing`);
        failed++;
        continue;
      }
      const base = PNG.sync.read(readFileSync(a));
      const next = PNG.sync.read(readFileSync(b));
      // Pages of different heights differ by at least the rows one has and
      // the other lacks: compare the shared area and count the rest as
      // mismatched.
      const width2 = Math.min(base.width, next.width);
      const height = Math.min(base.height, next.height);
      const crop = (png) => {
        const buf = Buffer.alloc(width2 * height * 4);
        for (let y = 0; y < height; y++) png.data.copy(buf, y * width2 * 4, y * png.width * 4, y * png.width * 4 + width2 * 4);
        return buf;
      };
      // threshold 0: any colour change counts. Both runs use the same browser
      // and fonts, so an unchanged page matches exactly; pixelmatch's default
      // 0.1 let a whole-page background change on this dark palette through.
      const diff = pixelmatch(crop(base), crop(next), null, width2, height, { threshold: 0 });
      const total = Math.max(base.width * base.height, next.width * next.height);
      const ratio = (diff + (total - width2 * height)) / total;
      const bad = ratio > maxMismatch;
      if (bad) failed++;
      console.log(`${name}-${width} mismatch=${ratio.toFixed(5)}${bad ? ' FAIL' : ''}`);
    }
  }
  return failed;
}

const args = parseArgs(process.argv.slice(2));
if (!args.out || args.out === true) {
  console.error('usage: screens.mjs --out <dir> [--legacy] [--dist <dir>] [--compare <dir> --max-mismatch <ratio>]');
  process.exitCode = 2;
} else {
  await capture(args);
  if (args.compare) {
    const failed = compare(here(args.out), here(args.compare), Number(args['max-mismatch'] ?? 0));
    console.log(`screens: ${failed} pair(s) over the limit`);
    process.exitCode = failed ? 1 : 0;
  }
}
