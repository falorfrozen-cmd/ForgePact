// The derived behaviour oracle: the steps that prove the controls added after
// the legacy page was recorded - every slider's on/off switch, the "Enabled
// mods" list's Turn off buttons and the theme choice (on the Setup tab, which
// one `tab:setup` step opens before the theme steps).
//
//   node tests/oracle-derive.mjs --from tests/behaviour-oracle.json --out tests/behaviour-oracle-derived.json
//                                [--supplement tests/behaviour-oracle-gems.json]
//                                [--key-supplement tests/behaviour-oracle-primeevil.json]
//
// `--supplement` names a recording of controls the legacy page gained later
// (tests/behaviour-oracle-gems.json, the Gems of Incarnation switches): every
// boolean mod in its `controls` gets the same on/off/on/Turn off steps a
// legacy boolean does, entered on the tab the supplement first reached it on
// once its navigation steps are relocated to where the controls sit now
// (tests/lib/oracle-relocate.mjs, SUPPLEMENT_RELOCATION: the Loot tab), and
// the derived file names it as `supplementFrom`.
//
// `--key-supplement` names a recording of a key slider a later origin/main
// added to KEYS (tests/behaviour-oracle-primeevil.json, Prime Evil Parts):
// every switched slider in its `controls` gets the same eight steps a legacy
// slider does, entered on the tab the recording reached it on (the Loot tab,
// no relocation). They come after the theme steps, so no existing step's index
// moves, and the derived file names the recording as `keySupplementFrom`.
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
// NATIVE_BOOLEANS are boolean mods no recorded page ever had (Far scenery
// sleep, the Pet moves on switch of forgepact-pet-loot-stuck, Move all into
// the stash, Extra packs as you approach, and Sleep loot your filter hides,
// whose show-key select is derived after them): the same on, off, on and Turn
// off shape the legacy recording holds for #mod_pet_quest_pickup, but nothing
// recorded stands for them, so their
// contract is written out here as literals - on posts the mod's key with true and sends its plugin
// verb with 1, off posts false and sends the verb with 0, on again repeats the
// first, and its Turn off button repeats the off - entered on the tab and Mods
// sub-tab they sit on. They come last, so no earlier step's index moves, and
// each is in `controls`, so the replay's coverage check counts it.
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
import { SUPPLEMENT_RELOCATION, relocate } from './lib/oracle-relocate.mjs';

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

// Boolean mods added after every recording (see the header): the config key,
// where the switch sits, and the plugin verb src/forgepact.py sends for it.
// `restate` is a line the backend sends before the verb's `1` when the switch
// turns on (Sleep loot your filter hides restates its show key, at its
// default in a fresh sandbox, as map reveal restates its child).
export const NATIVE_BOOLEANS = [
  { key: 'mod_far_sleep', tab: 'tab:mods', sub: 'subtab:qol', verb: 'farsleep' },
  { key: 'mod_pet_loot_unstick', tab: 'tab:mods', sub: 'subtab:qol', verb: 'petunstick' },
  { key: 'mod_stash_move_all', tab: 'tab:mods', sub: 'subtab:qol', verb: 'stashmoveall' },
  { key: 'density_rolling', tab: 'tab:mods', sub: 'subtab:qol', verb: 'densityroll' },
  { key: 'mod_hidden_loot', tab: 'tab:mods', sub: 'subtab:qol', verb: 'hiddenloot', restate: 'hiddenloot key 164' },
];
// The show key's select (#mod_hidden_loot_key, Sleep loot your filter hides'
// child row), derived as #mod_skill_timer_style is but with literals, since no
// recording has it: each code posts itself as an integer and sends
// `hiddenloot key <code>`. Ctrl, None, then Left Alt, the default, again. The
// select is disabled while its switch is off, and the native booleans leave
// the switch off, so the switch is turned on around them (on repeats the
// switch's first on, off its off).
export const HIDDEN_LOOT_KEY_PARENT = 'mod_hidden_loot';
export const HIDDEN_LOOT_KEY_CODES = [17, 0, 164];
// Switched sliders no recorded page ever had (Skill Haste and All Skills,
// ForgePact#114): the section and key, the tab they sit on (their neighbour
// Faster Cast Rate's, which the legacy walk reached on the Modifiers tab),
// their range, and the line src/forgepact.py sends at each end. Their
// contract is written out as literals, as NATIVE_BOOLEANS' is, and they come
// after everything else (the show key's select included, which needs the tab
// the native booleans left open), so no earlier step's index moves.
export const NATIVE_SLIDERS = [
  { section: 'percent_stats', key: 'skillhaste', tab: 'tab:modifiers', min: 0, max: 200,
    atMin: 'statadd skillhaste 0', atMax: 'statadd skillhaste 200' },
  { section: 'percent_stats', key: 'allskills', tab: 'tab:modifiers', min: 0, max: 100,
    atMin: 'statadd allskills 0', atMax: 'statadd allskills 100' },
];
export const tableRange = (section, key) => `input[type=range][data-sec="${section}"][data-key="${key}"]`;
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

export function derive(legacy, derivedFrom, supplement = null, supplementFrom = null, keySupplement = null, keySupplementFrom = null) {
  const steps = [];
  const push = (control, action, extra = {}) => {
    steps.push({ step: steps.length, control, action, ...extra });
    return steps.length - 1;
  };
  const controls = [];
  let open = { tab: null, sub: null };
  const enter = (control, recorded = legacy.steps) => {
    const { tab, sub } = contextOf(recorded, control);
    if (tab && tab !== open.tab) { push(tab, 'click'); open = { tab, sub: null }; }
    if (sub && sub !== open.sub) { push(sub, 'click'); open.sub = sub; }
  };
  const booleanMod = (selector, recorded) => {
    // A boolean mod: on, off (the reference), on, then its Turn off button.
    enter(selector, recorded);
    push(selector, 'click');
    const off = push(selector, 'click');
    push(selector, 'click');
    push(quickDisable(selector.slice(1)), 'click', { expect: { posts: { same: off }, cmds: { same: off } } });
  };
  const switchedSlider = (selector, switchId, recorded) => {
    // A switched slider: its own minimum and maximum first, as references.
    const sw = '#' + switchControlId(switchId);
    controls.push(sw);
    enter(selector, recorded);
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
  };
  let densityDone = false;
  for (const selector of legacy.controls) {
    const switchId = switchIdOf(selector);
    if (switchId) {
      switchedSlider(selector, switchId, legacy.steps);
    } else if (BOOLEAN_MODS.includes(selector.slice(1))) {
      booleanMod(selector, legacy.steps);
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
  // The supplement's boolean mods: their own recording covers them, and their
  // Turn off buttons are derived exactly as a legacy boolean's are, on the tab
  // the relocated supplement reaches them on (the Loot tab; see
  // tests/lib/oracle-relocate.mjs).
  const relocated = supplement ? relocate(supplement, SUPPLEMENT_RELOCATION) : null;
  for (const selector of relocated ? relocated.controls : []) {
    if (BOOLEAN_MODS.includes(selector.slice(1))) booleanMod(selector, relocated.steps);
  }
  // The theme sits on the Setup tab (its Appearance card; it was in the
  // status bar, on every tab, until the owner's polish pass), so one
  // navigation step opens Setup first - a click with no expectation, like
  // every `tab:` step here. It is a panel setting: saved, and never a
  // command. These steps are last, so no earlier step's index moves.
  controls.push('#theme');
  push('tab:setup', 'click');
  open = { tab: 'tab:setup', sub: null };
  for (const { value } of THEMES) {
    push('#theme', 'select', { value, expect: { posts: { is: setPost({ key: 'theme', value }) }, cmds: { is: [] } } });
  }
  // The key supplement's switched sliders (a key a later main added to KEYS):
  // the same eight steps a legacy slider gets, on the tab its recording
  // reached it on, after everything above, so no earlier step's index moves.
  for (const selector of keySupplement ? keySupplement.controls : []) {
    const switchId = switchIdOf(selector);
    if (switchId) switchedSlider(selector, switchId, keySupplement.steps);
  }
  // The boolean mods no recording has: their literal contract, last.
  const nativeAt = {};
  for (const { key, tab, sub, verb, restate } of NATIVE_BOOLEANS) {
    const selector = '#' + key;
    controls.push(selector);
    if (tab !== open.tab) { push(tab, 'click'); open = { tab, sub: null }; }
    if (sub && sub !== open.sub) { push(sub, 'click'); open.sub = sub; }
    const onCmds = [...(restate ? [restate] : []), `${verb} 1`];
    const on = push(selector, 'click', { expect: { posts: { is: setPost({ key, value: true }) }, cmds: { is: onCmds } } });
    const off = push(selector, 'click', { expect: { posts: { is: setPost({ key, value: false }) }, cmds: { is: [`${verb} 0`] } } });
    push(selector, 'click', { expect: { posts: { same: on }, cmds: { same: on } } });
    push(quickDisable(key), 'click', { expect: { posts: { same: off }, cmds: { same: off } } });
    nativeAt[key] = { on, off };
  }
  // The show key's select, after everything above, on the tab the native
  // booleans left open (see HIDDEN_LOOT_KEY_CODES).
  const parent = '#' + HIDDEN_LOOT_KEY_PARENT;
  const { on: parentOn, off: parentOff } = nativeAt[HIDDEN_LOOT_KEY_PARENT];
  controls.push('#mod_hidden_loot_key');
  push(parent, 'click', { expect: { posts: { same: parentOn }, cmds: { same: parentOn } } });
  for (const code of HIDDEN_LOOT_KEY_CODES) {
    push('#mod_hidden_loot_key', 'select', {
      value: String(code),
      expect: { posts: { is: setPost({ key: 'mod_hidden_loot_key', value: code }) }, cmds: { is: [`hiddenloot key ${code}`] } },
    });
  }
  push(parent, 'click', { expect: { posts: { same: parentOff }, cmds: { same: parentOff } } });
  // The switched sliders no recording has: a legacy slider's eight steps,
  // with the two ends' posts and lines written out, last. The range and its
  // switch are both controls here, since no recording lists the range.
  for (const { section, key, tab, min, max, atMin, atMax } of NATIVE_SLIDERS) {
    const selector = tableRange(section, key);
    const switchId = `${section}.${key}`;
    const sw = '#' + switchControlId(switchId);
    controls.push(selector, sw);
    if (tab !== open.tab) { push(tab, 'click'); open = { tab, sub: null }; }
    const top = push(selector, 'max', { expect: { posts: { is: setPost({ section, key, value: max }) }, cmds: { is: [atMax] } } });
    const atMinStep = push(selector, 'min', { expect: { posts: { is: setPost({ section, key, value: min }) }, cmds: { is: [atMin] } } });
    const atMaxStep = push(selector, 'max', { expect: { posts: { same: top }, cmds: { same: top } } });
    const off = push(sw, 'click', {
      expect: { posts: { is: setPost({ section: 'switches', key: switchId, value: false }) }, cmds: { same: atMinStep } },
    });
    const on = push(sw, 'click', {
      expect: { posts: { is: setPost({ section: 'switches', key: switchId, value: true }) }, cmds: { same: atMaxStep } },
    });
    push(quickDisable(switchControlId(switchId)), 'click', { expect: { posts: { same: off }, cmds: { same: off } } });
    push(sw, 'click', {
      expect: { posts: { is: setPost({ section: 'switches', key: switchId, value: true }) }, cmds: { same: on } },
    });
    push(selector, 'min', { expect: { posts: { same: atMinStep }, cmds: { same: atMinStep } } });
  }
  return {
    derivedFrom, legacyRecordedAt: legacy.recordedAt, ...(supplement ? { supplementFrom } : {}),
    ...(keySupplement ? { keySupplementFrom } : {}), controls, steps,
  };
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
    console.error('usage: oracle-derive.mjs --from <legacy oracle> --out <derived oracle> [--supplement <recording>] [--key-supplement <recording>]');
    process.exitCode = 2;
  } else {
    const legacy = JSON.parse(readFileSync(resolve(args.from), 'utf8'));
    const supplement = args.supplement ? JSON.parse(readFileSync(resolve(args.supplement), 'utf8')) : null;
    const keySupplement = args['key-supplement'] ? JSON.parse(readFileSync(resolve(args['key-supplement']), 'utf8')) : null;
    const derived = derive(legacy, derivedFromPath(args.from), supplement, args.supplement ? derivedFromPath(args.supplement) : null,
      keySupplement, args['key-supplement'] ? derivedFromPath(args['key-supplement']) : null);
    writeFileSync(resolve(args.out), serialise(derived));
    const switches = derived.steps.filter((s) => s.control.startsWith('#sw_')).length;
    console.log(`oracle-derive: ${derived.steps.length} steps (${switches} switch, ` +
      `${derived.steps.filter((s) => s.control.startsWith('#enabledMods ')).length} quick-disable, ` +
      `${derived.steps.filter((s) => s.control === '#theme').length} theme) to ${args.out}`);
  }
}
