// Proves the bundled faces load in the built panel, served the way the
// packaged exe serves it (the sandbox's real HTTP handler).
//
//   node tests/fonts-load.mjs [--dist <dir>]
//
// A declared @font-face that no rule uses is never downloaded, so opening the
// page proves nothing. This asks `document.fonts.load()` for each bundled
// family, which fetches and decodes the file, and reads the status of every
// face it returns: `loaded` means the browser decoded the woff2 it was served.
// The control asks the same for a family nobody declares, which must come
// back with no face at all. `document.fonts.check()` would not do for the
// control: it answers true for a family with no declared face.
//
// Each family's line names the face files the page fetched and how they were
// served; the last line is `fonts-load: <n> loaded, control <m>`, and the exit
// code is 0 only for 2 loaded and control 0.

import { resolve } from 'node:path';
import { launchBrowser, openPanel, parseArgs, startSandbox } from './lib/browser.mjs';

const FAMILIES = ['IBM Plex Sans', 'IBM Plex Mono'];
const CONTROL = 'ForgePact Undeclared Control';

const args = parseArgs(process.argv.slice(2));
const dist = typeof args.dist === 'string' ? resolve(args.dist) : null;

// One load() per family, in the page. A face that fails to fetch or decode
// makes load() reject, so a rejection is reported rather than thrown.
async function loadFamily(page, family) {
  return page.evaluate(async (fam) => {
    try {
      const faces = await document.fonts.load(`16px "${fam}"`);
      return { faces: faces.map((f) => ({ family: f.family.replace(/^["']|["']$/g, ''), status: f.status, weight: f.weight })) };
    } catch (e) {
      return { faces: [], error: String(e && e.message || e) };
    }
  }, family);
}

const sandbox = await startSandbox({ dist });
const browser = await launchBrowser();
let loaded = 0;
let control = -1;
let failed = false;
try {
  const page = await openPanel(browser, sandbox);
  const fetched = [];
  page.on('response', (r) => {
    if (/\.woff2(\?|$)/.test(r.url())) fetched.push(`${new URL(r.url()).pathname} ${r.status()} ${r.headers()['content-type'] || '-'}`);
  });
  for (const family of FAMILIES) {
    const before = fetched.length;
    const result = await loadFamily(page, family);
    const ok = result.faces.filter((f) => f.family === family && f.status === 'loaded');
    loaded += ok.length;
    if (!ok.length || ok.length !== result.faces.length) failed = true;
    const files = fetched.slice(before).join(', ') || 'no file fetched';
    const faces = result.faces.map((f) => `${f.status} ${f.weight}`).join('; ') || 'no face';
    console.log(`${ok.length && !result.error ? 'ok  ' : 'FAIL'} ${family}: ${faces}${result.error ? ' (' + result.error + ')' : ''} [${files}]`);
  }
  const ctl = await loadFamily(page, CONTROL);
  control = ctl.faces.length;
  console.log(`${control === 0 ? 'ok  ' : 'FAIL'} control "${CONTROL}": ${control} face(s)${ctl.error ? ' (' + ctl.error + ')' : ''}`);
  await page.context().close();
} finally {
  await browser.close();
  await sandbox.stop();
}
console.log(`fonts-load: ${loaded} loaded, control ${control}`);
process.exitCode = !failed && loaded === FAMILIES.length && control === 0 ? 0 : 1;
