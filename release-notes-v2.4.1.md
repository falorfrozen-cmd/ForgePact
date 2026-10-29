# ForgePact 2.4.1

A quick fix on 2.4.0 for Hero Siege's 7.0.14.0 update: four mods that could no
longer find their place in the game after the update hook into it again.

## Fixed

- **Four mods could no longer hook into the game after Hero Siege's 7.0.14.0 update (#249).**
  The game's 7.0.14.0 update (2026-10-29) renamed parts of the game that
  ForgePact 2.4.0 looks up by name, so after the update these mods turned on
  in the panel but changed nothing:
  - **Craft from the stash**: recipes at the Crafting Cube no longer counted
    the materials and socketables in your stash;
  - **Auto-prospect**: items put in the Prospect Cube's grid were no longer
    prospected for you;
  - the **Shadow Realm** and **Chaos Tower** sliders (Special Content, above
    x1): a run went back to at most one of each, as without the mod.

  ForgePact 2.4.1 looks them up under their new names. Checked in a game
  session on 7.0.14.0: all four mods switch on and hook into the game again,
  and the plugin's log names no lookup that failed. 2.4.1 is built for game
  7.0.14.0; use 2.4.0 only with the earlier game version.

## How to update

Download and extract the complete release, then reopen ForgePact and press
**Install Mod Plugin** once: the game update needs the new plugin. Only the
plugin changed; the panel works as in 2.4.0. Your existing settings are
retained. Start the game after installing; a game that was already open keeps
the old plugin until it is restarted.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
