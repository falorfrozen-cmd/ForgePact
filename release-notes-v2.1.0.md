# ForgePact 2.1.0

A new **Extra packs as you approach** switch, off by default, makes Monster
Density's extra spawners only as you come near them, so high density costs the
game less every frame.

## New

- **Extra packs as you approach.** A new switch in Mods → Quality of Life, off
  by default; it matters only with Monster Density above 1x. Monster Density's
  extra spawners are made within about 3,000 px of you, and ahead of you as
  you move, instead of across the whole zone the moment you arrive. What you
  meet is the same: in Act 1's first zone at 5x, the spawners and monsters
  near you were identical, while the zone held 430 spawners instead of 1,570
  and the game's own work per frame fell from 84% to 70% of a 60 fps frame.
  With Reveal full map's **Really spawn every pack on arrival (heavy)**, every
  spawner is made at once as before, and while the Beacon or Tyrant's Crown
  has monsters hunting you, the switch reaches as far as the hunt does.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. The new switch is in the
plugin, so press **Install Mod Plugin** once after updating - updating only the
panel leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
