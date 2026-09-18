# ForgePact 1.4.3

Bosses are no longer raised to Rare/Ancient by the rarity sliders.

## Fixed

- **The Monster Rarity sliders (Rare / Ancient) could raise a boss the same
  way they raise an ordinary monster.** A boss already builds its own health
  and affixes when it spawns; the sliders had no check for that and could
  raise it a second time on top of it. A player reported an Anubis boss
  going from ~500k to ~4.5M HP with 20% Rare and 20% Ancient set - we have
  not reproduced that number ourselves, but the sliders clearly could reach
  a boss when they should not have. Bosses are now left alone by both
  sliders; ordinary monsters raise exactly as before.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This change is in the plugin,
so pressing **Install Mod Plugin** matters - updating only the panel leaves the
old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
