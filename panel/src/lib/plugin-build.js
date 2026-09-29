// What the panel says about the plugin in the game when it is not the one this
// ForgePact ships (issue #123). /api/state's `pluginBuild` is src/forgepact.py's
// plugin_build_state(), read from the DLL's own bytes: updating ForgePact never
// touched the copy in the game, so an old plugin went on loading while the
// panel showed switches it does not know. status() in panel.js decides where
// these words go; nothing here posts or stores anything.

// The states in which the panel asks for Install Mod Plugin: PLUGIN_STALE_STATES
// in src/forgepact.py. `missing` is mod_chain's to report, and `newer`,
// `current` and `no-bundle` need nothing from the player.
export const PLUGIN_STALE_STATES = ['older', 'different', 'unknown'];

const FIX = ' Close the game, then click Install Mod Plugin in Setup.';

// { chip, chain, warning } for a stale plugin, or null. `chip` follows
// "Game open · " in the status bar, `chain` leads Setup's #chainnote (which adds
// " - click Install Mod Plugin (game must be closed)"), and `warning` is the
// plugin warning's tooltip.
export function pluginBuildNotice(build) {
  const state = build?.state;
  if (!PLUGIN_STALE_STATES.includes(state)) return null;
  const game = build.installed ? 'v' + build.installed : 'an old version';
  const shipped = build.bundled ? 'v' + build.bundled : 'a newer one';
  if (state === 'older') {
    return {
      chip: 'plugin out of date',
      chain: `mod plugin out of date: the game has ${game}, this ForgePact ships ${shipped}`,
      warning: `The mod plugin in the game is ${game}, older than this ForgePact's ${shipped}. `
        + 'Your settings are saved, but mods added since then will not work.' + FIX,
    };
  }
  if (state === 'different') {
    return {
      chip: 'plugin build differs',
      chain: `mod plugin differs: the game's ${game} is not the build this ForgePact ships`,
      warning: `The mod plugin in the game is a different ${game} build from the one this ForgePact ships. `
        + 'Some mods may not work as described.' + FIX,
    };
  }
  return {
    chip: 'plugin not recognised',
    chain: 'mod plugin not recognised: the game\'s BloodPactPlugin.dll is not one this ForgePact can read',
    warning: 'The mod plugin file in the game is not one this ForgePact can read, so its mods may not work.' + FIX,
  };
}
