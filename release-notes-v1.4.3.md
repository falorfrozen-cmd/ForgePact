# ForgePact 1.4.3

A precaution in how ForgePact places its own kill drops.

## Changed

- **ForgePact's own kill drops are now placed before the game finishes handling the kill.**
  When a monster dies, ForgePact rolls its own Angelic / Unholy drop and its
  Tyrant's Crown / Headhunter drop and places the item where the monster
  fell. Until now it did this after the game's own kill handling had run, and
  still used the monster to place the item. With 1.4.3 both drops are rolled
  and placed while the monster is still there, and ForgePact no longer
  touches the monster after that. Drop rates and what can drop are unchanged.

  Players reported crashes in fights with the Angelic / Unholy slider at x100
  on 1.4.1. We could not reproduce that crash: in testing, 1.4.1 and 1.4.3
  each dropped 30 angelic items from 30 kills without crashing. This change
  removes a possible risk, it is not a confirmed fix for that report. If you
  still get crashes, please report them with the last lines of
  `bp_ipc\out.txt` and which ForgePact settings you had on.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This change is in the plugin,
so pressing **Install Mod Plugin** matters - updating only the panel leaves the
old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
