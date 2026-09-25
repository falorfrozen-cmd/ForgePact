// Shared plumbing for the panel's browser tools (oracle.mjs, screens.mjs,
// ui.e2e.mjs): start the sandbox server, launch the installed Edge headless
// through playwright-core, open the panel and wait for its own DOM to say it
// has settled.
//
// Nothing here downloads a browser: `channel: 'msedge'` drives the Edge that
// is already installed. And nothing here reaches into page globals - the
// Svelte build has none - so every wait reads state the page already shows.

import { spawn } from 'node:child_process';
import { mkdtempSync, readFileSync, rmSync, writeFileSync } from 'node:fs';
import { get } from 'node:http';
import { tmpdir } from 'node:os';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { chromium } from 'playwright-core';

export const PANEL_DIR = resolve(dirname(fileURLToPath(import.meta.url)), '..', '..');
export const SANDBOX = join(PANEL_DIR, '..', 'tests', 'panel_sandbox_server.py');

export const VIEWPORTS = {
  1280: { width: 1280, height: 800 },
  900: { width: 900, height: 700 },
};

export const TABS = ['setup', 'modifiers', 'world', 'loot', 'mods'];

// `--name value` / `--flag` into an object; bare words land in `_`.
export function parseArgs(argv) {
  const out = { _: [] };
  for (let i = 0; i < argv.length; i++) {
    const a = argv[i];
    if (!a.startsWith('--')) { out._.push(a); continue; }
    const name = a.slice(2);
    const next = argv[i + 1];
    if (next === undefined || next.startsWith('--')) out[name] = true;
    else { out[name] = next; i++; }
  }
  return out;
}

// Start tests/panel_sandbox_server.py and wait for its `port=` and `cmds=`
// lines. `stop()` closes its stdin, which is how the server knows to exit,
// and kills only the process this call started if it has not gone by then.
// `seed`, an object of forgepact.json keys, is written to a temp file and
// passed as `--seed` (the server refuses a key it does not know, exit 2, on
// the inherited stderr); the file goes when the server does.
export async function startSandbox({ legacy = false, dist = null, offline = false, satanicMinimum = false, seed = null } = {}) {
  const args = ['-3', SANDBOX];
  if (legacy) args.push('--legacy');
  if (dist) args.push('--dist', dist);
  if (offline) args.push('--offline');
  if (satanicMinimum) args.push('--satanic-minimum');
  let seedDir = null;
  if (seed) {
    seedDir = mkdtempSync(join(tmpdir(), 'forgepact-seed-'));
    const file = join(seedDir, 'seed.json');
    writeFileSync(file, JSON.stringify(seed));
    args.push('--seed', file);
  }
  const dropSeed = () => { if (seedDir) rmSync(seedDir, { recursive: true, force: true }); seedDir = null; };
  const child = spawn('py', args, { stdio: ['pipe', 'pipe', 'inherit'], windowsHide: true });
  const info = await new Promise((resolveInfo, reject) => {
    let buffer = '';
    const found = {};
    const timer = setTimeout(() => reject(new Error('sandbox server did not report its port within 30 s')), 30000);
    child.on('exit', (code) => { clearTimeout(timer); reject(new Error(`sandbox server exited early (code ${code})`)); });
    child.stdout.on('data', (chunk) => {
      buffer += chunk.toString();
      for (const line of buffer.split(/\r?\n/)) {
        const m = line.match(/^(port|cmds)=(.+)$/);
        if (m) found[m[1]] = m[2];
      }
      if (found.port && found.cmds) { clearTimeout(timer); resolveInfo(found); }
    });
  }).catch((e) => { dropSeed(); throw e; });
  child.removeAllListeners('exit');
  const port = Number(info.port);
  const cmds = info.cmds.trim();
  const url = `http://127.0.0.1:${port}/`;
  return {
    port,
    url,
    cmds,
    // The temp directory the sandbox owns; paths inside it differ per run, so
    // recorded bodies and commands carry `<sandbox>` in its place.
    root: dirname(cmds),
    truncateCmds() { writeFileSync(cmds, ''); },
    // send_cmds writes in text mode, so on Windows the lines end in CRLF.
    readCmds() { return readFileSync(cmds, 'utf8').split(/\r?\n/).filter(Boolean); },
    async state() { return getJson(`${url}api/state`); },
    async stop() {
      if (child.exitCode !== null) { dropSeed(); return; }
      const exited = new Promise((r) => child.once('exit', r));
      child.stdin.end();
      const timeout = new Promise((r) => setTimeout(r, 5000, 'timeout'));
      if (await Promise.race([exited, timeout]) === 'timeout') child.kill();
      dropSeed();
    },
  };
}

export function getJson(url) {
  return new Promise((resolveJson, reject) => {
    get(url, (res) => {
      let body = '';
      res.on('data', (c) => { body += c; });
      res.on('end', () => { try { resolveJson(JSON.parse(body)); } catch (e) { reject(e); } });
    }).on('error', reject);
  });
}

export async function launchBrowser() {
  return chromium.launch({ channel: 'msedge', headless: true });
}

// A fresh context per page: sessionStorage (the remembered tab) never leaks
// from one run into the next.
export async function openPanel(browser, sandbox, viewport = VIEWPORTS[1280], { routes } = {}) {
  const context = await browser.newContext({ viewport });
  const page = await context.newPage();
  if (routes) await routes(page);
  await page.goto(sandbox.url);
  await waitBooted(page);
  return page;
}

// boot() writes "Settings loaded" as its very last statement.
export async function waitBooted(page) {
  await page.waitForFunction(() => document.getElementById('saveIndicator')?.textContent === 'Settings loaded',
    null, { timeout: 15000 });
}

// The save indicator is the page's own statement that no write is pending:
// j() sets `saving`/"Saving..." for as long as any write is queued. The
// Satanic card says the same with its own line while a bulk save runs.
export async function waitSaved(page, timeout = 10000) {
  await page.waitForFunction(() => {
    const s = document.getElementById('saveIndicator');
    const sat = document.getElementById('satSaveState');
    return s && !s.classList.contains('saving') && s.textContent !== 'Saving...' &&
      !(sat && sat.textContent === 'Saving changes...');
  }, null, { timeout, polling: 20 });
}

export async function openTab(page, name) {
  await page.click(`.tabbtn[data-tab="${name}"]`);
}
