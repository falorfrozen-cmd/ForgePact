// Which mods a saved config has switched on, for the "Enabled mods" list: one
// entry per mod whose saved state makes src/forgepact.py's build_cmds() emit
// something, named by the control that turns it off. Pure - it reads only
// the config - so tests/enabled-mods.test.js runs it under plain node.
//
// Never entries: dungeon_chest_pct (it rides on mod_dungeon_chest's entry),
// gambapity (it rides on mod_gambapity's entry),
// auto_apply (a panel setting), enemy_speed_ct (a scope, not a
// value), the child options map_reveal_packs / map_reveal_spawn /
// mod_auto_prospect_bag / mod_hidden_loot_key / dungeon_chest_countdown (they
// ride on their parent's entry), gem_filter (an
// option of the Mythic gems entry: it is sent only while that switch is on),
// game_exe, theme, and the Satanic Zone pools (all on by default, so
// "enabled" there is the default rather than something the player turned on).
// The same card's zone-control switches (#157) are ordinary entries: off by
// default, and each sends its line while on.

// Boolean mods: an entry while true; the control is the checkbox itself.
export const BOOLEAN_MODS = [
  'map_reveal', 'headhunter', 'tyrant', 'beacon', 'mod_filter_max_relics',
  'mod_orb_pickup_radius', 'mod_pet_quest_pickup', 'mod_pet_relic_pickup', 'mod_pet_loot_unstick', 'mod_auto_prospect',
  'mod_toggle_indicator', 'mod_toggle_guard', 'mod_restart_anytime', 'mod_craft_mats',
  'mod_far_sleep', 'mod_stash_move_all', 'density_rolling', 'mod_hidden_loot', 'mod_jump_scenery', 'mod_loot_announce',
  'mod_gem_mythic', 'mod_gem_maxroll',
  'satanic_follow', 'satanic_everywhere',
];

// The sliders that carry an on/off switch, in the order src/forgepact.py's
// SLIDER_SWITCH_IDS lists them: every row of these sections as
// "<section>.<key>", then the four top-level sliders by their config key.
// Monster Density is not one of them - den_on has always been its switch.
export const SWITCH_SECTIONS = ['stats', 'percent_stats', 'spawners', 'drops', 'keys'];
export const TOP_LEVEL_SWITCHES = ['rarity_rare', 'rarity_ancient', 'angelic_items', 'enemy_speed'];

// A slider's value at its default is off: 0 for these, 1 for every other one.
const ZERO_DEFAULT = new Set(['percent_stats', 'enemy_speed', 'rarity_rare', 'rarity_ancient']);

export function sliderSwitchIds(cfg) {
  const ids = [];
  for (const section of SWITCH_SECTIONS) for (const key of Object.keys(cfg?.[section] || {})) ids.push(section + '.' + key);
  return ids.concat(TOP_LEVEL_SWITCHES);
}

// The switch checkbox's id: sw_stats_exp, sw_rarity_rare.
export function switchControlId(switchId) {
  return 'sw_' + switchId.replace('.', '_');
}

// Absent or true is on; only an explicit false (the one value the panel
// saves) turns a slider off.
export function switchOn(cfg, switchId) {
  return cfg?.switches?.[switchId] !== false;
}

function sliderValue(cfg, switchId) {
  const dot = switchId.indexOf('.');
  return Number(dot < 0 ? cfg[switchId] : cfg[switchId.slice(0, dot)]?.[switchId.slice(dot + 1)]);
}

function sliderDefault(switchId) {
  return ZERO_DEFAULT.has(switchId.split('.')[0]) ? 0 : 1;
}

export function enabledControls(cfg) {
  if (!cfg) return [];
  const out = [];
  for (const key of BOOLEAN_MODS) if (cfg[key]) out.push(key);
  if ((cfg.mod_skill_timer_style || 'off') !== 'off') out.push('mod_skill_timer_style');
  if ((cfg.boss_rarity || 'off') !== 'off') out.push('boss_rarity');
  // Dungeon chest opens early: its switch decides, as den_on does density's.
  // Not in BOOLEAN_MODS, whose entries the oracle derives as `verb 1` /
  // `verb 0`: off sends `dungeonchest off`, on the saved percentage.
  if (cfg.mod_dungeon_chest) out.push('mod_dungeon_chest');
  // Goburin's Head pity: its switch decides, as dungeon chest's does. Not in
  // BOOLEAN_MODS either: off sends `gambapity off`, on the saved count.
  if (cfg.mod_gambapity) out.push('mod_gambapity');
  if (cfg.density_on && Number(cfg.density) > 1) out.push('den_on');
  for (const id of sliderSwitchIds(cfg)) {
    if (switchOn(cfg, id) && sliderValue(cfg, id) > sliderDefault(id)) out.push(switchControlId(id));
  }
  return out;
}
