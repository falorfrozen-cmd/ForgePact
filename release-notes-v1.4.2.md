# ForgePact 1.4.2

Chaos Tower and Shadow Realm rate settings above x1 work again on the current game version. This release requires the current game version.

## Fixed

- **Chaos Tower and Shadow Realm multipliers above x1 had no effect on the
  current game version.** Each could still appear only once per run, exactly
  as if the setting were left at x1. A game update renamed the internal names
  ForgePact uses to reset each mechanic's once-per-run limit, so the reset
  could no longer attach and the game's own limit took over. ForgePact 1.4.2
  uses the current game version's names. Confirmed in-game: with the rates
  raised, one run showed many Chaos Towers and at least two Shadow Realms.

  This fix only works on the current game version. If you are still on the
  previous game version, 1.4.2 cannot attach the reset there, so Chaos Tower
  and Shadow Realm rates above x1 have no effect. Stay on 1.4.1 until you
  update the game.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This change is in the plugin,
so pressing **Install Mod Plugin** matters - updating only the panel leaves the
old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
