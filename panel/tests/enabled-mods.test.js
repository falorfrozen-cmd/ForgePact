// enabledControls() (src/enabled-mods.js): which controls the "Enabled mods"
// list shows for a saved config. The fixture is src/forgepact.py's DEFAULTS
// as /api/state returns it (tests/fixtures/defaults-cfg.json;
// tests/test_enabled_mods_panel.py keeps it equal to the real DEFAULTS), so
// the baseline is the config a new player starts with: nothing is on.
import test from 'node:test';
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import {
  BOOLEAN_MODS, SWITCH_SECTIONS, TOP_LEVEL_SWITCHES, enabledControls, sliderSwitchIds, switchControlId,
} from '../src/enabled-mods.js';

const DEFAULTS = JSON.parse(readFileSync(new URL('./fixtures/defaults-cfg.json', import.meta.url), 'utf8'));
const cfg = (patch = {}) => ({ ...structuredClone(DEFAULTS), ...patch });
const withSection = (section, key, value, patch = {}) => {
  const c = cfg(patch);
  c[section] = { ...c[section], [key]: value };
  return c;
};

test('baseline: the defaults turn nothing on', () => {
  assert.deepEqual(enabledControls(cfg()), []);
});

test('an empty or missing config is not an error', () => {
  assert.deepEqual(enabledControls({}), []);
  assert.deepEqual(enabledControls(null), []);
  assert.deepEqual(enabledControls(undefined), []);
});

test('every boolean mod is an entry when true, keyed by its own checkbox', () => {
  assert.equal(BOOLEAN_MODS.length, 23);
  for (const key of BOOLEAN_MODS) {
    assert.deepEqual(enabledControls(cfg({ [key]: true })), [key], key);
    assert.deepEqual(enabledControls(cfg({ [key]: false })), [], key);
  }
});

test('the two Gems of Incarnation switches are entries; their mod filter never is', () => {
  assert.deepEqual(BOOLEAN_MODS.slice(-4, -2), ['mod_gem_mythic', 'mod_gem_maxroll']);
  assert.equal(DEFAULTS.mod_gem_mythic, false);
  assert.equal(DEFAULTS.mod_gem_maxroll, false);
  assert.equal(DEFAULTS.gem_filter, 'all');
  assert.deepEqual(enabledControls(cfg({ mod_gem_mythic: true, mod_gem_maxroll: true })), ['mod_gem_mythic', 'mod_gem_maxroll']);
  // The filter is an option of the Mythic entry: narrowed, with both off, it
  // is still nothing on; narrowed with Mythic on, still one entry.
  assert.deepEqual(enabledControls(cfg({ gem_filter: [68, 284] })), []);
  assert.deepEqual(enabledControls(cfg({ gem_filter: [68, 284], mod_gem_mythic: true })), ['mod_gem_mythic']);
});

test('the two Satanic Zone control switches are entries while on, and off by default', () => {
  assert.deepEqual(BOOLEAN_MODS.slice(-2), ['satanic_follow', 'satanic_everywhere']);
  assert.equal(DEFAULTS.satanic_follow, false);
  assert.equal(DEFAULTS.satanic_everywhere, false);
  assert.deepEqual(enabledControls(cfg({ satanic_follow: true })), ['satanic_follow']);
  assert.deepEqual(enabledControls(cfg({ satanic_everywhere: true })), ['satanic_everywhere']);
  assert.deepEqual(enabledControls(cfg({ satanic_follow: true, satanic_everywhere: true })),
    ['satanic_follow', 'satanic_everywhere']);
});

test('far scenery sleep is an entry while on, and off by default', () => {
  assert.ok(BOOLEAN_MODS.includes('mod_far_sleep'));
  assert.equal(DEFAULTS.mod_far_sleep, false);
  assert.deepEqual(enabledControls(cfg({ mod_far_sleep: true })), ['mod_far_sleep']);
});

test('move all into the stash is an entry while on, and off by default', () => {
  assert.ok(BOOLEAN_MODS.includes('mod_stash_move_all'));
  assert.equal(DEFAULTS.mod_stash_move_all, false);
  assert.deepEqual(enabledControls(cfg({ mod_stash_move_all: true })), ['mod_stash_move_all']);
});

test('extra packs as you approach is an entry while on, and off by default', () => {
  assert.ok(BOOLEAN_MODS.includes('density_rolling'));
  assert.equal(DEFAULTS.density_rolling, false);
  assert.deepEqual(enabledControls(cfg({ density_rolling: true })), ['density_rolling']);
});

test('pet collects relics is an entry while on, and off by default', () => {
  assert.ok(BOOLEAN_MODS.includes('mod_pet_relic_pickup'));
  assert.equal(DEFAULTS.mod_pet_relic_pickup, false);
  assert.deepEqual(enabledControls(cfg({ mod_pet_relic_pickup: true })), ['mod_pet_relic_pickup']);
  // Its own switch: Pet collects quest items on does not list it, nor the reverse.
  assert.deepEqual(enabledControls(cfg({ mod_pet_quest_pickup: true })), ['mod_pet_quest_pickup']);
});

test('sleep loot your filter hides is an entry while on, off by default, and its show key never is one', () => {
  assert.ok(BOOLEAN_MODS.includes('mod_hidden_loot'));
  assert.ok(!BOOLEAN_MODS.includes('mod_hidden_loot_key'));
  assert.equal(DEFAULTS.mod_hidden_loot, false);
  assert.equal(DEFAULTS.mod_hidden_loot_key, 164);
  assert.deepEqual(enabledControls(cfg({ mod_hidden_loot: true })), ['mod_hidden_loot']);
  // The key rides on the switch's entry: another key with the switch off is
  // still nothing on, and with it on still one entry.
  assert.deepEqual(enabledControls(cfg({ mod_hidden_loot_key: 17 })), []);
  assert.deepEqual(enabledControls(cfg({ mod_hidden_loot_key: 0, mod_hidden_loot: true })), ['mod_hidden_loot']);
});

test('dungeon chest\'s countdown form is never an entry: it rides on the switch, head by default', () => {
  assert.ok(!BOOLEAN_MODS.includes('dungeon_chest_countdown'));
  assert.equal(DEFAULTS.dungeon_chest_countdown, 'head');
  // Another form with the switch off is still nothing on, and with it on
  // still the switch's one entry.
  for (const form of ['head', 'chat', 'both']) {
    assert.deepEqual(enabledControls(cfg({ dungeon_chest_countdown: form })), [], form);
    assert.deepEqual(enabledControls(cfg({ dungeon_chest_countdown: form, mod_dungeon_chest: true })), ['mod_dungeon_chest'], form);
  }
});

test('jump through scenery is an entry while on and off by default', () => {
  assert.ok(BOOLEAN_MODS.includes('mod_jump_scenery'));
  assert.equal(DEFAULTS.mod_jump_scenery, false);
  assert.deepEqual(enabledControls(cfg({ mod_jump_scenery: true })), ['mod_jump_scenery']);
});

test('the skill timer is an entry for any style but off', () => {
  for (const style of ['arc', 'bar', 'number', 'fade']) {
    assert.deepEqual(enabledControls(cfg({ mod_skill_timer_style: style })), ['mod_skill_timer_style'], style);
  }
  assert.deepEqual(enabledControls(cfg({ mod_skill_timer_style: 'off' })), []);
});

test('Bosses is an entry while rare or ancient, and off by default', () => {
  assert.equal(DEFAULTS.boss_rarity, 'off', 'off by default');
  for (const mode of ['rare', 'ancient']) {
    assert.deepEqual(enabledControls(cfg({ boss_rarity: mode })), ['boss_rarity'], mode);
  }
  assert.deepEqual(enabledControls(cfg({ boss_rarity: 'off' })), []);
  const { boss_rarity, ...older } = cfg();
  assert.deepEqual(enabledControls(older), [], 'an older config without the key reads as off');
});

test('Dungeon chest opens early is an entry while its switch is on, whatever the percentage, and off by default', () => {
  assert.equal(DEFAULTS.mod_dungeon_chest, false, 'off by default');
  assert.equal(DEFAULTS.dungeon_chest_pct, 75);
  assert.ok(!BOOLEAN_MODS.includes('mod_dungeon_chest'), 'its off is `dungeonchest off`, not a `verb 0` boolean');
  for (const pct of [50, 75, 95]) {
    assert.deepEqual(enabledControls(cfg({ mod_dungeon_chest: true, dungeon_chest_pct: pct })), ['mod_dungeon_chest'], pct);
  }
  // The percentage rides on the switch's entry: moved with the switch off,
  // it is still nothing on.
  assert.deepEqual(enabledControls(cfg({ dungeon_chest_pct: 90 })), []);
  const { mod_dungeon_chest, dungeon_chest_pct, ...older } = cfg();
  assert.deepEqual(enabledControls(older), [], 'an older config without the keys reads as off');
});

test('density is an entry only while switched on above x1, through den_on', () => {
  assert.deepEqual(enabledControls(cfg({ density_on: true, density: 3 })), ['den_on']);
  assert.deepEqual(enabledControls(cfg({ density_on: false, density: 3 })), [], 'switched off keeps its value but is not on');
  assert.deepEqual(enabledControls(cfg({ density_on: true, density: 1 })), [], 'x1 is the default');
});

test('Goburin’s Head pity is an entry while its switch is on, whatever the count, and off by default', () => {
  assert.equal(DEFAULTS.mod_gambapity, false, 'off by default');
  assert.equal(DEFAULTS.gambapity, 10);
  assert.ok(!BOOLEAN_MODS.includes('mod_gambapity'), 'its off is `gambapity off`, not a `verb 0` boolean');
  for (const count of [1, 10, 20]) {
    assert.deepEqual(enabledControls(cfg({ mod_gambapity: true, gambapity: count })), ['mod_gambapity'], count);
  }
  // The count rides on the switch's entry: moved with the switch off, it is
  // still nothing on.
  assert.deepEqual(enabledControls(cfg({ gambapity: 5 })), []);
  const { mod_gambapity, gambapity, ...older } = cfg();
  assert.deepEqual(enabledControls(older), [], 'an older config without the keys reads as off');
});

test('the switch ids: 43 table rows plus four top-level sliders, never density', () => {
  // 40 table rows before the three skill sliders (#160) joined percent_stats.
  const ids = sliderSwitchIds(cfg());
  assert.equal(ids.length, 47);
  assert.deepEqual(ids.slice(-4), TOP_LEVEL_SWITCHES);
  assert.ok(ids.includes('stats.exp') && ids.includes('percent_stats.damage') && ids.includes('keys.ruby'));
  assert.ok(!ids.some((id) => id.includes('density') || id === 'enemy_speed_ct'));
  assert.deepEqual(SWITCH_SECTIONS, ['stats', 'percent_stats', 'spawners', 'drops', 'keys']);
  assert.equal(switchControlId('stats.exp'), 'sw_stats_exp');
  assert.equal(switchControlId('percent_stats.damage'), 'sw_percent_stats_damage');
  assert.equal(switchControlId('rarity_rare'), 'sw_rarity_rare');
  assert.equal(switchControlId('enemy_speed'), 'sw_enemy_speed');
});

test('a table slider is an entry above its default while its switch is on', () => {
  assert.deepEqual(enabledControls(withSection('stats', 'exp', 5)), ['sw_stats_exp']);
  assert.deepEqual(enabledControls(withSection('spawners', 'rift', 2)), ['sw_spawners_rift']);
  assert.deepEqual(enabledControls(withSection('drops', 'mining_ore', 4)), ['sw_drops_mining_ore']);
  assert.deepEqual(enabledControls(withSection('keys', 'relic', 20)), ['sw_keys_relic']);
  assert.deepEqual(enabledControls(withSection('stats', 'exp', 1)), [], 'x1 is the default');
});

test('Mining Ore Extra Rolls is an entry above one roll, beside the multiplier', () => {
  assert.equal(DEFAULTS.drops.mining_ore_rolls, 1, 'off by default');
  assert.deepEqual(enabledControls(withSection('drops', 'mining_ore_rolls', 3)), ['sw_drops_mining_ore_rolls']);
  assert.deepEqual(enabledControls(withSection('drops', 'mining_ore_rolls', 1)), [], 'one roll is the default');
  const both = withSection('drops', 'mining_ore', 5);
  both.drops.mining_ore_rolls = 3;
  assert.deepEqual(enabledControls(both), ['sw_drops_mining_ore', 'sw_drops_mining_ore_rolls']);
});

test('percent stats, enemy speed and rarity are on above 0; angelic above 1', () => {
  assert.deepEqual(enabledControls(withSection('percent_stats', 'damage', 25)), ['sw_percent_stats_damage']);
  assert.deepEqual(enabledControls(withSection('percent_stats', 'damage', 0)), []);
  assert.deepEqual(enabledControls(cfg({ enemy_speed: 50 })), ['sw_enemy_speed']);
  assert.deepEqual(enabledControls(cfg({ rarity_rare: 25, rarity_ancient: 15 })), ['sw_rarity_rare', 'sw_rarity_ancient']);
  assert.deepEqual(enabledControls(cfg({ angelic_items: 2 })), ['sw_angelic_items']);
  assert.deepEqual(enabledControls(cfg({ angelic_items: 1 })), [], 'x1 is off');
});

test('a switched-off slider keeps its value but is not an entry; true or absent is on', () => {
  assert.deepEqual(enabledControls(withSection('stats', 'exp', 5, { switches: { 'stats.exp': false } })), []);
  assert.deepEqual(enabledControls(withSection('stats', 'exp', 5, { switches: { 'stats.exp': true } })), ['sw_stats_exp']);
  assert.deepEqual(enabledControls(cfg({ enemy_speed: 50, switches: { enemy_speed: false } })), []);
  const { switches, ...noSwitches } = withSection('stats', 'exp', 5);
  assert.deepEqual(enabledControls(noSwitches), ['sw_stats_exp'], 'an older config without switches reads as all-on');
});

test('never entries: panel settings, scopes, child options, the theme and the Satanic pools', () => {
  const c = cfg({
    auto_apply: true, enemy_speed_ct: false, map_reveal_packs: false, map_reveal_spawn: true,
    mod_auto_prospect_bag: false, game_exe: 'C:\\Games\\Hero_Siege.exe', theme: 'alt',
  });
  c.satanic_mods = { buff: { 1: true, 2: false }, debuff: { 1: false } };
  assert.deepEqual(enabledControls(c), []);
  // A child option rides on its parent's entry; it never adds one of its own.
  assert.deepEqual(enabledControls({ ...c, map_reveal: true }), ['map_reveal']);
});

test('entries come out in rule order: booleans, skill timer, bosses, dungeon chest, gambapity, density, sliders', () => {
  const c = withSection('stats', 'exp', 5, {
    beacon: true, map_reveal: true, mod_skill_timer_style: 'arc', boss_rarity: 'ancient', mod_dungeon_chest: true,
    mod_gambapity: true, density_on: true, density: 2, rarity_rare: 10,
  });
  assert.deepEqual(enabledControls(c),
    ['map_reveal', 'beacon', 'mod_skill_timer_style', 'boss_rarity', 'mod_dungeon_chest', 'mod_gambapity', 'den_on', 'sw_stats_exp', 'sw_rarity_rare']);
});
