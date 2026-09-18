# ForgePact 1.4.3

A change aimed at crashes during fights with the Angelic / Unholy drop setting turned up high. The fix is not yet confirmed in-game.

## Fixed

- **Possible fix (not yet confirmed): the game could crash in fights with the Angelic / Unholy slider at x100.**
  Players reported the crash with the setting at x100 on 1.4.1; 1.4.2 did not
  change this. When a monster dies, ForgePact rolls its own Angelic / Unholy
  drop and its Tyrant's Crown / Headhunter drop and places the item where the
  monster fell. Until now it did this after the game's own kill handling had
  run, and still used the monster to place the item. With
  1.4.3 both drops are rolled and placed while the monster is still there,
  before the game's own kill handling runs, and ForgePact no longer touches
  the monster after that. Drop rates and what can drop are unchanged.

  This is our best lead, not a confirmed cause: the change is not yet
  confirmed in-game. If the game still crashes with the slider up, please
  report it with the last lines of `bp_ipc\out.txt`.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This change is in the plugin,
so pressing **Install Mod Plugin** matters - updating only the panel leaves the
old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
