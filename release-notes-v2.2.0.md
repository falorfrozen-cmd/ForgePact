# ForgePact 2.2.0

Release date: 2026-10-09

A new **Sleep loot your filter hides** switch, off by default, puts the loot
your filter hides to sleep so the game stops updating it every frame, and
shows it again while you hold a key.

## New

- **Sleep loot your filter hides.** A new switch in Mods → Quality of Life,
  off by default. Items your loot filter hides are not gone: they lie on the
  ground unseen, and the game keeps updating every one of them every frame.
  With this on, an item your filter hides is put to sleep as soon as it
  drops, so the game stops updating it. Items your filter shows, and gold,
  are never touched. Hold **Left Alt** to see the hidden items
  and pick them up; let go and the rest are hidden and asleep again. Under
  **Show hidden loot while held** you can pick another key, the middle mouse
  button, mouse button 4 or 5, or none; the left and right mouse buttons
  can't be used. Turning the switch on also puts to sleep the hidden items
  already on the ground, and turning it off wakes them all. If you loosen
  your filter later, items already asleep stay hidden: hold the key, or turn
  the switch off. How much it saves comes from a research session on the
  research build, which put hidden items to sleep with the same call before
  this switch existed: with about 2,700 hidden items lying around one spot,
  the game took about 7.5 ms a frame with them asleep, against 14 to 18 ms
  with them awake. This version of the switch has been tested both outside
  the game and in a live game.

## How to update

Download and extract the complete release, then reopen ForgePact: the panel
gained a switch, so updating only the plugin leaves it out. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. The new switch is in the
plugin too, so press **Install Mod Plugin** once after updating - updating only
the panel leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
