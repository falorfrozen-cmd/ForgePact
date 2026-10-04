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
// the stash, Extra packs as you approach, Pet collects relics (#124), Sleep
// loot your filter hides - whose show-key select is derived after them, so it
// stays last - the two Satanic Zone control switches (#157, on the World
// tab), and Jump through scenery): the same on, off, on and Turn off shape the
// legacy recording holds for #mod_pet_quest_pickup, but nothing recorded
// stands for them, so their contract is written out here as literals - on
// posts the mod's key with true and sends its plugin verb with 1, off posts
// false and sends the verb with 0, on again repeats the first, and its Turn
// off button repeats the off - entered on the tab and Mods sub-tab they sit
// on. They come last, so no earlier step's index moves, and each is in
// `controls`, so the replay's coverage check counts it. The zone-control pair
// sits before Jump through scenery rather than after it because the show
// key's select follows this loop without a navigation step of its own: the
// loop must end on Mods > Quality of Life.
//
// NATIVE_SLIDERS are switched table sliders no recorded page ever had (Skill
// Haste and All Skills, #114; Mining Ore Extra Rolls, issue #36; Projectile
// Speed, Projectile Amount and Area of Effect, #160): the same
// eight steps a legacy slider gets (max, min, max, switch off, on, Turn off,
// on, min), entered on the tab they sit on (one tab step whenever the tab
// changes), but with nothing recorded to compare the slider's own moves against,
// so its maximum and minimum carry literal expectations - each end posts the
// section, key and value and sends the line written out for it - and the switch steps compare with those
// the way a legacy slider's do. They come after the native booleans and the
// show key's select, so no earlier step's index moves, and both the range and its switch are in
// `controls`, since no recording lists the range either.
//
// PANEL_BOOLEANS and PANEL_BUTTONS are panel controls no recorded page ever
// had that send the plugin nothing (the Setup tab's Incident reports card,
// issue #76). The card has one, Open reports folder; PANEL_BOOLEANS is empty
// since its FPS-drop switch went (an FPS drop is recorded without a notice,
// the owner, 2026-10-02) and stays as a working list for the next panel
// switch: a switch is clicked off then on again from its default (on),
// each click posting its key with the new value and sending no command, and a
// button is clicked once, posting an empty body to its own route and sending
// no command. Neither is a mod, so neither has a Turn off button. They come
// after the native sliders, so no earlier step's index moves, and each is in
// `controls`.
//
// NATIVE_SELECTS are selects no recorded page ever had (the Bosses select of
// the Mods tab's Gameplay sub-tab, issue #44): raised, off, raised again and
// Turn off, each post and line written out as a literal, entered on their tab
// and Mods sub-tab (Mods, then Gameplay, since the panel buttons leave Setup
// open). They come after the panel controls, so no earlier step's index
// moves.
//
// NATIVE_SWITCHED_RANGES are switch-plus-range pairs no recorded page ever
// had (Dungeon chest opens early, issue #31, on Mods › Gameplay): a switch
// of their own, as Monster Density's #den_on is density's, beside a
// top-level range whose value box is typable. NATIVE_SLIDERS does not fit
// them: a table slider's switch turned off sends what its slider sends at its
// minimum, and this one sends `<verb> off`, a different line, while a range
// moved with the switch off is saved and sends nothing. So each step's post
// and line are literals: the switch on (the range at its resting value), the
// range at its maximum, at its minimum, a typed value, the switch off, the
// range at its maximum while off (posted, nothing sent), the switch on again
// (it sends the saved maximum), and the Turn off button, which repeats the
// switch's off. Entered on their tab and Mods sub-tab (the native selects
// leave them open), they come after the native selects, so no earlier step's
// index moves, and both the switch and the range are in `controls`, since no
// recording lists either. A pair with a child select (`restate`) sends that
// select's saved line after its own each time the switch turns on.
//
// NATIVE_CHILD_SELECTS are selects no recorded page ever had that sit under a
// switch-plus-range pair's switch and are disabled while it is off (where
// Dungeon chest opens early's countdown shows, the owner's choice of
// 2026-10-04), the shape of the show key's select above: the switch turned on
// around them (on repeats the pair's last on, off its off), each value posted
// and its line sent, ending on the default. Last of all, after the pairs, so
// no earlier step's index moves.
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
// default in a fresh sandbox, as map reveal restates its child). The two
// Satanic Zone control switches sit before Jump through scenery rather than
// last: the show key's select follows this loop without a navigation step of
// its own, so the loop must end on Mods > Quality of Life.
export const NATIVE_BOOLEANS = [
  { key: 'mod_far_sleep', tab: 'tab:mods', sub: 'subtab:qol', verb: 'farsleep' },
  { key: 'mod_pet_loot_unstick', tab: 'tab:mods', sub: 'subtab:qol', verb: 'petunstick' },
  { key: 'mod_stash_move_all', tab: 'tab:mods', sub: 'subtab:qol', verb: 'stashmoveall' },
  { key: 'density_rolling', tab: 'tab:mods', sub: 'subtab:qol', verb: 'densityroll' },
  { key: 'mod_pet_relic_pickup', tab: 'tab:mods', sub: 'subtab:qol', verb: 'petrelic' },
  { key: 'mod_hidden_loot', tab: 'tab:mods', sub: 'subtab:qol', verb: 'hiddenloot', restate: 'hiddenloot key 164' },
  { key: 'satanic_follow', tab: 'tab:world', verb: 'satzone follow' },
  { key: 'satanic_everywhere', tab: 'tab:world', verb: 'satzone everywhere' },
  { key: 'mod_jump_scenery', tab: 'tab:mods', sub: 'subtab:qol', verb: 'jumpscenery' },
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
// ForgePact#114; Mining Ore Extra Rolls, #36; the three skill sliders, #160):
// the section and key, the tab they sit on (Skill Haste and All Skills: their
// neighbour Faster Cast Rate's, which the legacy walk reached on the Modifiers
// tab; Mining Ore Extra Rolls: the Loot tab; the skill sliders: Modifiers,
// entered again after the Loot tab), their range, and the line
// src/forgepact.py sends at each end.
// Their contract is written out as literals, as NATIVE_BOOLEANS' is, and they
// come after everything else (the show key's select included, which needs the
// tab the native booleans left open), a newer one after an older one, so no
// earlier step's index moves.
export const NATIVE_SLIDERS = [
  { section: 'percent_stats', key: 'skillhaste', tab: 'tab:modifiers', min: 0, max: 200,
    atMin: 'statadd skillhaste 0', atMax: 'statadd skillhaste 200' },
  { section: 'percent_stats', key: 'allskills', tab: 'tab:modifiers', min: 0, max: 100,
    atMin: 'statadd allskills 0', atMax: 'statadd allskills 100' },
  { section: 'drops', key: 'mining_ore_rolls', tab: 'tab:loot', min: 1, max: 10,
    atMin: 'miningrolls 1', atMax: 'miningrolls 10' },
  { section: 'percent_stats', key: 'projspeed', tab: 'tab:modifiers', min: 0, max: 100,
    atMin: 'skillslider projspeed 0', atMax: 'skillslider projspeed 100' },
  { section: 'percent_stats', key: 'projamount', tab: 'tab:modifiers', min: 0, max: 5,
    atMin: 'skillslider projamount 0', atMax: 'skillslider projamount 5' },
  { section: 'percent_stats', key: 'aoesize', tab: 'tab:modifiers', min: 0, max: 100,
    atMin: 'skillslider aoesize 0', atMax: 'skillslider aoesize 100' },
];
// Panel settings and actions no recorded page ever had, which send the plugin
// nothing (issue #76's Incident reports card on Setup): a switch's key, tab
// and default (none now), and a button's id, tab and route. Appended after the
// native sliders; only NATIVE_SELECTS come after them.
export const PANEL_BOOLEANS = [];
export const PANEL_BUTTONS = [
  { id: 'openreports', tab: 'tab:setup', url: '/api/openreports' },
];
// Selects no recorded page ever had (the Bosses select, issue #44, the only
// control of the Mods tab's Gameplay sub-tab): the config key, where it sits,
// the value it is raised to, and the plugin verb src/forgepact.py sends with
// the chosen value. The legacy #mod_skill_timer_style branch above walks only
// the recording's controls, so these are written out as literals - raised,
// off, raised again, then the Turn off button, which sends what off sends -
// after the panel controls, so no earlier step's index moves.
export const NATIVE_SELECTS = [
  { key: 'boss_rarity', tab: 'tab:mods', sub: 'subtab:gameplay', on: 'ancient', verb: 'bossrarity' },
];
// Switch-plus-range pairs no recorded page ever had (Dungeon chest opens
// early, issue #31; see the header): the switch's config key, the range's
// config key (each control's id is its key), where they sit, the plugin verb
// src/forgepact.py sends, the range's resting value in a fresh sandbox, its
// ends, the value typed into its number input, and the child select's line
// the switch's on restates (its default in a fresh sandbox).
export const NATIVE_SWITCHED_RANGES = [
  { key: 'mod_dungeon_chest', range: 'dungeon_chest_pct', tab: 'tab:mods', sub: 'subtab:gameplay', verb: 'dungeonchest',
    rest: 75, min: 50, max: 95, typed: 80, restate: 'dungeonchest countdown head' },
];
// Child selects of a switch-plus-range pair (see the header): the config key
// (the control's id), the pair's switch, the values posted in order (the
// default last) and the line src/forgepact.py sends before each value. Last
// of all.
export const NATIVE_CHILD_SELECTS = [
  { key: 'dungeon_chest_countdown', parent: 'mod_dungeon_chest', values: ['chat', 'both', 'head'], verb: 'dungeonchest countdown' },
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
  // The panel's own switches and buttons: a switch away from its default and
  // back, a button once, each posting its own literal and sending no command.
  for (const { key, tab, initial } of PANEL_BOOLEANS) {
    const selector = '#' + key;
    controls.push(selector);
    if (tab !== open.tab) { push(tab, 'click'); open = { tab, sub: null }; }
    for (const value of [!initial, initial]) {
      push(selector, 'click', { expect: { posts: { is: setPost({ key, value }) }, cmds: { is: [] } } });
    }
  }
  for (const { id, tab, url } of PANEL_BUTTONS) {
    const selector = '#' + id;
    controls.push(selector);
    if (tab !== open.tab) { push(tab, 'click'); open = { tab, sub: null }; }
    push(selector, 'click', { expect: { posts: { is: [{ url, body: {} }] }, cmds: { is: [] } } });
  }
  // The selects no recording has: their literal contract, last of all (after
  // the panel controls, so Mods and its Gameplay sub-tab are entered again).
  for (const { key, tab, sub, on: raised, verb } of NATIVE_SELECTS) {
    const selector = '#' + key;
    controls.push(selector);
    if (tab !== open.tab) { push(tab, 'click'); open = { tab, sub: null }; }
    if (sub && sub !== open.sub) { push(sub, 'click'); open.sub = sub; }
    const on = push(selector, 'select', { value: raised, expect: { posts: { is: setPost({ key, value: raised }) }, cmds: { is: [`${verb} ${raised}`] } } });
    const off = push(selector, 'select', { value: 'off', expect: { posts: { is: setPost({ key, value: 'off' }) }, cmds: { is: [`${verb} off`] } } });
    push(selector, 'select', { value: raised, expect: { posts: { same: on }, cmds: { same: on } } });
    push(quickDisable(key), 'click', { expect: { posts: { same: off }, cmds: { same: off } } });
  }
  // The switch-plus-range pairs no recording has: their literal contract
  // (Mods › Gameplay is still open from the native selects).
  const pairAt = {};
  for (const { key, range, tab, sub, verb, rest, min, max, typed, restate } of NATIVE_SWITCHED_RANGES) {
    const sw = '#' + key;
    const rg = '#' + range;
    controls.push(sw, rg);
    if (tab !== open.tab) { push(tab, 'click'); open = { tab, sub: null }; }
    if (sub && sub !== open.sub) { push(sub, 'click'); open.sub = sub; }
    const sends = (value, cmds) => ({ posts: { is: setPost({ key: range, value }) }, cmds: { is: cmds } });
    const onCmds = (value) => [`${verb} ${value}`, ...(restate ? [restate] : [])];
    push(sw, 'click', { expect: { posts: { is: setPost({ key, value: true }) }, cmds: { is: onCmds(rest) } } });
    push(rg, 'max', { expect: sends(max, [`${verb} ${max}`]) });
    push(rg, 'min', { expect: sends(min, [`${verb} ${min}`]) });
    push(rg, 'type', { value: String(typed), expect: sends(typed, [`${verb} ${typed}`]) });
    const off = push(sw, 'click', { expect: { posts: { is: setPost({ key, value: false }) }, cmds: { is: [`${verb} off`] } } });
    push(rg, 'max', { expect: sends(max, []) });
    const on = push(sw, 'click', { expect: { posts: { is: setPost({ key, value: true }) }, cmds: { is: onCmds(max) } } });
    push(quickDisable(key), 'click', { expect: { posts: { same: off }, cmds: { same: off } } });
    pairAt[key] = { on, off };
  }
  // The child selects no recording has, last of all: their switch turned on
  // around them (it is off after the pair's Turn off), each value in turn.
  for (const { key, parent, values, verb } of NATIVE_CHILD_SELECTS) {
    const selector = '#' + key;
    const sw = '#' + parent;
    const { on: parentOn, off: parentOff } = pairAt[parent];
    controls.push(selector);
    push(sw, 'click', { expect: { posts: { same: parentOn }, cmds: { same: parentOn } } });
    for (const value of values) {
      push(selector, 'select', { value, expect: { posts: { is: setPost({ key, value }) }, cmds: { is: [`${verb} ${value}`] } } });
    }
    push(sw, 'click', { expect: { posts: { same: parentOff }, cmds: { same: parentOff } } });
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
