// The Tracker tab's browser checks (#185), in a real (headless) Edge against
// the sandbox server: every `tests/tracker/<name>.check.mjs`, or the named
// ones.
//
//   node tests/tracker.e2e.mjs [--only <name>[,<name>...]] [--dist <dir>]    (npm run e2e:tracker)
//
// A check module exports `EXPECTED`, the names of its checks, and a default
// `async (t) => ...` that calls `t.check(name, fn)` once per check. Each module
// gets its own sandbox, started with the tracker fixture
// (tests/fixtures/tracker/session-fixture.ndjson: pid 777, Sorak, a Satanic
// zone, deltas, a ground drop) as its live session, so the tracker store, the
// session folder and the settings are the sandbox's own, never the
// developer's. What `t` offers:
//
// - `t.sandbox`: that sandbox (lib/browser.mjs's `startSandbox` result; its
//   `trackerSession` is the session file the panel reads);
// - `t.openPanel({ sandbox, viewport, routes })`: a fresh page on the panel,
//   booted; closed when the check ends;
// - `t.appendSession(lines)`: appends records to the session file. A string is
//   written as it is; an object is given the envelope the fixture's records
//   carry (protocol, v, process_id, build_id), a `sequence` one past the
//   file's last and, unless it names one, an `emitted_at_unix_ms` a second
//   past the file's last;
// - `t.api(path, init)`: the sandbox's JSON answer to a request made from this
//   process (`init` as fetch's; an object `body` is sent as JSON). The answer's
//   HTTP status is on `t.lastStatus`;
// - `t.startSandbox(options)`: a second sandbox (for example one with no
//   session: `{ trackerSession: null }`), stopped when the module ends;
// - `t.args`: this run's arguments.
//
// Expected values come from the API in the check, never from literals. A check
// that counts alert plays wraps the Web Audio API in a page init script; no
// test hook ships in the product.
//
// Prints one line per check and, last, `e2e-tracker: <passed>/<total> checks
// passed`, where total is the sum of the modules' EXPECTED. Exits 1 on a
// failed check, on an expected check that never ran, or when --only names no
// module.

import { appendFileSync, readdirSync, readFileSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { pathToFileURL } from 'node:url';
import { PANEL_DIR, VIEWPORTS, launchBrowser, openPanel, parseArgs, startSandbox } from './lib/browser.mjs';

const args = parseArgs(process.argv.slice(2));
const DIST = typeof args.dist === 'string' ? args.dist : null;
const CHECKS_DIR = join(PANEL_DIR, 'tests', 'tracker');
export const FIXTURE = resolve(PANEL_DIR, '..', 'tests', 'fixtures', 'tracker', 'session-fixture.ndjson');
const SUFFIX = '.check.mjs';

const available = readdirSync(CHECKS_DIR).filter((f) => f.endsWith(SUFFIX)).map((f) => f.slice(0, -SUFFIX.length)).sort();
const wanted = typeof args.only === 'string' ? args.only.split(',').map((s) => s.trim()).filter(Boolean) : null;
const modules = wanted ? available.filter((name) => wanted.includes(name)) : available;
const unknown = wanted ? wanted.filter((name) => !available.includes(name)) : [];
if (wanted && (!modules.length || unknown.length)) {
  console.log(`FAIL --only names ${unknown.length ? 'no module ' + unknown.join(', ') : 'no module'}; the modules are: ${available.join(', ')}`);
  console.log('e2e-tracker: 0/0 checks passed');
  process.exit(1);
}

// The records the fixture's lines carry besides their own fields.
const ENVELOPE = ['protocol', 'v', 'process_id', 'build_id'];

function appendTo(file, lines) {
  const existing = readFileSync(file, 'utf8').split(/\r?\n/).filter(Boolean).map((l) => JSON.parse(l));
  const last = existing.at(-1) || {};
  let sequence = last.sequence || 0;
  let at = last.emitted_at_unix_ms || Date.now();
  const out = [];
  for (const line of lines) {
    if (typeof line === 'string') { out.push(line); continue; }
    const record = {};
    for (const key of ENVELOPE) if (key in last) record[key] = last[key];
    sequence += 1;
    at = line.emitted_at_unix_ms ?? at + 1000;
    Object.assign(record, { sequence, emitted_at_unix_ms: at }, line);
    out.push(JSON.stringify(record));
  }
  if (out.length) appendFileSync(file, out.map((l) => l + '\n').join(''));
}

const browser = await launchBrowser();
let total = 0;
let passed = 0;
let failed = 0;
try {
  for (const name of modules) {
    const mod = await import(pathToFileURL(join(CHECKS_DIR, name + SUFFIX)).href);
    const expected = Array.isArray(mod.EXPECTED) ? mod.EXPECTED : [];
    total += expected.length;
    const ran = new Set();
    const extraSandboxes = [];
    let sandbox = null;
    try {
      sandbox = await startSandbox({ dist: DIST, trackerSession: FIXTURE });
      let contexts = [];
      const t = {
        args,
        sandbox,
        lastStatus: null,
        async openPanel({ sandbox: box = sandbox, viewport = VIEWPORTS[1280], routes } = {}) {
          const page = await openPanel(browser, box, viewport, { routes });
          contexts.push(page.context());
          return page;
        },
        appendSession(lines) {
          if (!sandbox.trackerSession) throw new Error('this sandbox has no session file');
          appendTo(sandbox.trackerSession, Array.isArray(lines) ? lines : [lines]);
        },
        async api(path, init = {}, box = sandbox) {
          const opts = { ...init };
          if (opts.body !== undefined && typeof opts.body !== 'string') {
            opts.body = JSON.stringify(opts.body);
            opts.headers = { 'Content-Type': 'application/json', ...(opts.headers || {}) };
            opts.method = opts.method || 'POST';
          }
          const res = await fetch(new URL(path, box.url), opts);
          t.lastStatus = res.status;
          const text = await res.text();
          try { return JSON.parse(text); } catch {
            throw new Error(`${opts.method || 'GET'} ${path}: HTTP ${res.status}, not JSON: ${text.slice(0, 200)}`);
          }
        },
        async startSandbox(options = {}) {
          const box = await startSandbox({ dist: DIST, ...options });
          extraSandboxes.push(box);
          return box;
        },
        async check(label, fn) {
          if (ran.has(label)) throw new Error(`check ${label} ran twice`);
          ran.add(label);
          if (!expected.includes(label)) {
            failed++;
            console.log(`FAIL ${name}/${label}: not in the module's EXPECTED`);
            return;
          }
          try {
            await fn();
            passed++;
            console.log(`ok   ${name}/${label}`);
          } catch (e) {
            failed++;
            console.log(`FAIL ${name}/${label}: ${e.stack || e.message}`);
          } finally {
            for (const context of contexts) await context.close().catch(() => {});
            contexts = [];
          }
        },
      };
      await mod.default(t);
    } catch (e) {
      failed++;
      console.log(`FAIL ${name}: ${e.stack || e.message}`);
    } finally {
      for (const box of extraSandboxes) await box.stop();
      if (sandbox) await sandbox.stop();
    }
    for (const label of expected) if (!ran.has(label)) console.log(`FAIL ${name}/${label}: never ran`);
  }
} finally {
  await browser.close();
}
console.log(`e2e-tracker: ${passed}/${total} checks passed`);
process.exitCode = failed || passed !== total ? 1 : 0;
