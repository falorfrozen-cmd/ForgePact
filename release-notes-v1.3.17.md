# ForgePact 1.3.17

Works together with Hero Siege Item Editor 2.15.3.

## New

- **Mods tab: Gameplay Mods.** Remove owned relics from drop pool — a relic already
  at maximum level (10 out of 10) in your equipped slots, backpack, or inventory
  stops dropping again. Experience and Magic Find orb pickup radius — 10x the normal
  range; matching orbs inside it glide toward you at a steady pace once they are close
  enough, instead of needing to walk up to them.
- **Mods tab: Items.** Headhunter, Tyrant's Crown, and Beacon moved here from World,
  since they are mechanics tied to items forged in the Item Editor rather than
  standalone world settings. Nothing about how they work changed, only where you
  find them.

## Changed

- **Reveal full map** moved from the World tab into Mods > Gameplay Mods, alongside
  the other panel-level toggles.

## Fixed

- **A rare hard crash while adjusting drop rates.** If an item's index had drifted
  after a game update, printing its drop rate to the log could read a corrupted,
  astronomically large number from game memory. Formatting that number crashed the
  whole game. Bad reads are now shown as a safe placeholder instead of crashing it.

## How to update

Download the zip, unzip it, run ForgePact and press **Install** once. Start the game after
that; a game that was already open keeps the old plugin until it is restarted.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
