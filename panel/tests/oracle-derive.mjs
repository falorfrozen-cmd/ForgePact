// The derived behaviour oracle: the steps that prove the controls added after
// the legacy page was recorded - every slider's on/off switch, the "Enabled
// mods" list's Turn off buttons and the theme choice.
//
//   node tests/oracle-derive.mjs --from tests/behaviour-oracle.json --out tests/behaviour-oracle-derived.json
//
// tests/behaviour-oracle.json was recorded from the legacy page and is never
// re-recorded: it is the proof that the port changed nothing. The new controls
// have no legacy recording to compare against, so this file holds no recorded
// values at all. It is generated from the legacy file's control list and the
// panel's own contract, and each step's expectation is either another step of
// the same replay run (`same: <step>`) or a literal the contract fixes
// (`is: [...]`): a switch turned off must send what its slider sends at its
// minimum (every slider's minimum is its default), turned on again what it
// sends at its maximum, and a Turn off button exactly what the control it
// stands for sends. `oracle.mjs replay --derived` runs these steps on a fresh
// sandbox after the legacy ones.
//
// Deterministic: the same legacy file and the same THEMES give the same bytes,
// and tests/oracle-derive.test.js holds the committed file to that. A theme
// renamed in src/theme.js is a re-run of `npm run oracle:derive`, never an
// edit to the derived file.

import { readFileSync, writeFileSync } from 'node:fs';
import { dirname, relative, resolve, sep } from 'node:path';
import { fileURLToPath, pathToFileURL } from 'node:url';
import { BOOLEAN_MODS, switchControlId } from '../src/enabled-mods.js';
import { THEMES } from '../src/theme.js';

export const PANEL_DIR = resolve(dirname(fileURLToPath(import.meta.url)), '..');

// The four top-level sliders, by the element id the legacy page gave them, to
// the config key their switch is saved under.
const TOP_LEVEL = {
  '#enemyspeed': 'enemy_speed',
  '#rarity_rare': 'rarity_rare',
  '#rarity_ancient': 'rarity_ancient',
  '#angelic_items': 'angelic_items',
};
const TABLE_RANGE = /^input\[type=range\]\[data-sec="([^"]+)"\]\[data-key="([^"]+)"\]$/;

export function switchIdOf(selector) {
  const m = selector.match(TABLE_RANGE);
  return m ? `${m[1]}.${m[2]}` : (TOP_LEVEL[selector] || null);
}

export const quickDisable = (controlId) => `#enabledMods .quick-disable[data-for="${controlId}"]`;
const setPost = (body) => [{ url: '/api/set', body }];

// The tab (and Mods sub-tab) the legacy walk had open when it first reached
// `control`: the last `tab:` step before it, and the last `subtab:` step
// after that.
function contextOf(legacySteps, control) {
  const first = legacySteps.findIndex((s) => s.control === control);
  let tab = null;
  let sub = null;
  for (let i = 0; i < first; i++) {
    const c = legacySteps[i].control;
    if (c.startsWith('tab:')) { tab = c; sub = null; } else if (c.startsWith('subtab:')) sub = c;
  }
  return { tab, sub };
}

export function derive(legacy, derivedFrom) {
  const steps = [];
  const push = (control, action, extra = {}) => {
    steps.push({ step: steps.length, control, action, ...extra });
    return steps.length - 1;
  };
  const controls = [];
  let open = { tab: null, sub: null };
  const enter = (control) => {
    const { tab, sub } = contextOf(legacy.steps, control);
    if (tab && tab !== open.tab) { push(tab, 'click'); open = { tab, sub: null }; }
    if (sub && sub !== open.sub) { push(sub, 'click'); open.sub = sub; }
  };
  let densityDone = false;
  for (const selector of legacy.controls) {
    const switchId = switchIdOf(selector);
    if (switchId) {
      // A switched slider: its own minimum and maximum first, as references.
      const sw = '#' + switchControlId(switchId);
      controls.push(sw);
      enter(selector);
      push(selector, 'max');
      const atMin = push(selector, 'min');
      const atMax = push(selector, 'max');
      const off = push(sw, 'click', {
        expect: { posts: { is: setPost({ section: 'switches', key: switchId, value: false }) }, cmds: { same: atMin } },
      });
      const on = push(sw, 'click', {
        expect: { posts: { is: setPost({ section: 'switches', key: switchId, value: true }) }, cmds: { same: atMax } },
      });
      push(quickDisable(switchControlId(switchId)), 'click', { expect: { posts: { same: off }, cmds: { same: off } } });
      push(sw, 'click', {
        expect: { posts: { is: setPost({ section: 'switches', key: switchId, value: true }) }, cmds: { same: on } },
      });
      push(selector, 'min');
    } else if (BOOLEAN_MODS.includes(selector.slice(1))) {
      // A boolean mod: on, off (the reference), on, then its Turn off button.
      enter(selector);
      push(selector, 'click');
      const off = push(selector, 'click');
      push(selector, 'click');
      push(quickDisable(selector.slice(1)), 'click', { expect: { posts: { same: off }, cmds: { same: off } } });
    } else if ((selector === '#den_on' || selector === '#den') && !densityDone) {
      // Monster Density keeps its own switch; the list turns it off through it.
      densityDone = true;
      enter(selector);
      push('#den', 'max');
      push('#den_on', 'click');
      const off = push('#den_on', 'click');
      push('#den_on', 'click');
      push(quickDisable('den_on'), 'click', { expect: { posts: { same: off }, cmds: { same: off } } });
      push('#den', 'min');
    } else if (selector === '#mod_skill_timer_style') {
      enter(selector);
      push(selector, 'select', { value: 'arc' });
      const off = push(selector, 'select', { value: 'off' });
      push(selector, 'select', { value: 'arc' });
      push(quickDisable('mod_skill_timer_style'), 'click', { expect: { posts: { same: off }, cmds: { same: off } } });
    }
  }
  // The theme sits in the status bar, on every tab. It is a panel setting:
  // saved, and never a command.
  controls.push('#theme');
  for (const { value } of THEMES) {
    push('#theme', 'select', { value, expect: { posts: { is: setPost({ key: 'theme', value }) }, cmds: { is: [] } } });
  }
  return { derivedFrom, legacyRecordedAt: legacy.recordedAt, controls, steps };
}

export function serialise(derived) {
  return JSON.stringify(derived, null, 1) + '\n';
}

// The path the derived file names its source by: relative to panel/, with
// forward slashes, wherever the command was run from.
export function derivedFromPath(fromPath) {
  return relative(PANEL_DIR, resolve(fromPath)).split(sep).join('/');
}

function parse(argv) {
  const out = {};
  for (let i = 0; i < argv.length; i++) if (argv[i].startsWith('--')) out[argv[i].slice(2)] = argv[++i];
  return out;
}

if (process.argv[1] && import.meta.url === pathToFileURL(resolve(process.argv[1])).href) {
  const args = parse(process.argv.slice(2));
  if (!args.from || !args.out) {
    console.error('usage: oracle-derive.mjs --from <legacy oracle> --out <derived oracle>');
    process.exitCode = 2;
  } else {
    const legacy = JSON.parse(readFileSync(resolve(args.from), 'utf8'));
    const derived = derive(legacy, derivedFromPath(args.from));
    writeFileSync(resolve(args.out), serialise(derived));
    const switches = derived.steps.filter((s) => s.control.startsWith('#sw_')).length;
    console.log(`oracle-derive: ${derived.steps.length} steps (${switches} switch, ` +
      `${derived.steps.filter((s) => s.control.startsWith('#enabledMods ')).length} quick-disable, ` +
      `${derived.steps.filter((s) => s.control === '#theme').length} theme) to ${args.out}`);
  }
}
