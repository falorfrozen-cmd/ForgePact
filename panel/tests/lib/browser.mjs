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

// How long a sandbox may take to print its port (`startTimeoutMs` overrides
// it). Starting one cost about 4.5 s on an idle machine while importing
// hs_game_sdk built every table, and up to 41 s beside three other browser
// suites on four cores; since the SDK loads its tables on first use (hub
// PR #286) and the panel imports only the Satanic pools, it takes about
// 0.45 s. The limit is only there to name a sandbox that hangs.
export const SANDBOX_START_TIMEOUT_MS = 120000;
// How many of a sandbox's last stderr lines an error quotes.
const STDERR_TAIL = 20;

// Start tests/panel_sandbox_server.py and wait for its `port=` and `cmds=`
// lines. `stop()` closes its stdin, which is how the server knows to exit,
// and kills only the process this call started if it has not gone by then.
// `seed`, an object of forgepact.json keys, is written to a temp file and
// passed as `--seed` (the server refuses a key it does not know, exit 2);
// the file goes when the server does, and never before: a sandbox still
// starting would otherwise fail on the missing file instead of the reason it
// was late.
// `src`, a directory holding another tree's forgepact.py, is passed as
// `--src`: the server imports that module instead of this checkout's (with
// `legacy`, it serves that tree's embedded page).
//
// The sandbox's stderr reaches this process's stderr line by line, each line
// prefixed with the sandbox's port (its pid until it has one), so a suite's
// output says which sandbox printed what. `describe()` says whether the
// sandbox is still running and quotes its last stderr lines; a failed start
// and a failed `state()` carry that text in their message, which is how a
// refused connection says whether anything was still serving.
export async function startSandbox({ legacy = false, dist = null, offline = false, satanicMinimum = false, seed = null, src = null,
  startTimeoutMs = SANDBOX_START_TIMEOUT_MS } = {}) {
  const args = ['-3', SANDBOX];
  if (legacy) args.push('--legacy');
  if (src) args.push('--src', src);
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
  const child = spawn('py', args, { stdio: ['pipe', 'pipe', 'pipe'], windowsHide: true });
  // Closing the stdin of a sandbox that already died fails with EPIPE; that
  // death is what describe() reports, so the pipe error itself is dropped.
  child.stdin.on('error', () => {});
  const started = Date.now();
  let label = `sandbox pid ${child.pid}`;
  let exited = null;
  // 'close', not 'exit': it comes after the stderr pipe has drained, so the
  // tail a report quotes is complete.
  const closed = new Promise((resolveClosed) => child.once('close', (code, signal) => {
    exited = { code, signal, after: Date.now() - started };
    resolveClosed();
  }));
  const tail = [];
  let partial = '';
  const printLine = (line) => {
    tail.push(line);
    if (tail.length > STDERR_TAIL) tail.shift();
    process.stderr.write(`[${label}] ${line}\n`);
  };
  child.stderr.on('data', (chunk) => {
    const lines = (partial + chunk.toString()).split(/\r?\n/);
    partial = lines.pop();
    for (const line of lines) printLine(line);
  });
  child.stderr.on('end', () => { if (partial) printLine(partial); partial = ''; });
  const describe = () => {
    const after = exited && `, ${(exited.after / 1000).toFixed(1)} s after it started`;
    const status = exited === null ? `${label} is still running`
      : exited.signal ? `${label} was stopped by ${exited.signal}${after}`
        : `${label} exited with code ${exited.code}${after}`;
    return tail.length ? `${status}; its last stderr lines:\n${tail.join('\n')}` : status;
  };
  const info = await new Promise((resolveInfo, reject) => {
    let buffer = '';
    const found = {};
    let late = false;
    const timer = setTimeout(async () => {
      // Ended before this rejects: a late sandbox must not start serving once
      // its caller has given up, nor fail on the seed dropped below. Killing
      // the `py` launcher ends the python.exe it started as well.
      late = true;
      child.kill();
      await within(closed, 5000);
      reject(new Error(`sandbox server did not report its port within ${startTimeoutMs / 1000} s and was stopped; ${describe()}`));
    }, startTimeoutMs);
    closed.then(() => {
      clearTimeout(timer);
      if (!late) reject(new Error(`sandbox server exited early; ${describe()}`));
    });
    child.stdout.on('data', (chunk) => {
      buffer += chunk.toString();
      for (const line of buffer.split(/\r?\n/)) {
        const m = line.match(/^(port|cmds)=(.+)$/);
        if (m) found[m[1]] = m[2];
      }
      // Not once the start timer has fired: that sandbox is being stopped, and
      // a port line still on the pipe must not win over the timeout's reject.
      if (!late && found.port && found.cmds) { clearTimeout(timer); resolveInfo(found); }
    });
  }).catch((e) => { dropSeed(); throw e; });
  const port = Number(info.port);
  label = `sandbox :${port}`;
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
    describe,
    async state() {
      try {
        return await getJson(`${url}api/state`);
      } catch (e) {
        throw new Error(`${e.message}; ${describe()}`, { cause: e });
      }
    },
    // Waits for 'close', so whatever the sandbox printed while stopping has
    // been passed on before this returns.
    async stop() {
      if (child.exitCode === null && child.signalCode === null) {
        child.stdin.end();
        if (!await within(closed, 5000)) child.kill();
      }
      await within(closed, 5000);
      dropSeed();
    },
  };
}

// Whether `promise` settles within `ms`; the timer never outlives the answer,
// so a finished suite does not wait it out before exiting.
async function within(promise, ms) {
  let timer;
  const expired = new Promise((r) => { timer = setTimeout(r, ms, false); });
  try {
    return await Promise.race([promise.then(() => true), expired]);
  } finally {
    clearTimeout(timer);
  }
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
