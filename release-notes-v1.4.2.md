# ForgePact 1.4.2

Chaos Tower and Shadow Realm rate settings above x1 are meant to work again on the current game version - not yet confirmed in-game, and this release requires the current game version specifically.

## Fixed

- **Chaos Tower and Shadow Realm multipliers above x1 could only appear once
  per run on the current game version**, exactly as if the setting were left
  at x1. The internal names ForgePact used to reset each mechanic's
  once-per-run flag right before it re-activates no longer matched the game
  after an update, so the reset found nothing to reset and the game's own
  vanilla limit took over instead. ForgePact 1.4.2 fixes the internal names
  to the current game version specifically, so a rate above x1 can let either
  mechanic appear again within the same run on that version.

  This fix is version-specific, not adaptive: if you are still on the
  previous game version, installing 1.4.2 makes Chaos Tower and Shadow Realm
  rates above x1 stop working the same way they did before this fix - stay
  on 1.4.1 until you update the game.

  This fix is not yet confirmed in-game - it corrects the names to what they
  should be on the current game version, but nobody has yet reproduced either
  mechanic appearing more than once in a run with it installed.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This change is in the plugin,
so pressing **Install Mod Plugin** matters - updating only the panel leaves the
old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
