# ForgePact 2.3.0

Release date: 2026-10-16

A new **Fill the map as you approach** switch, off by default, makes Reveal
full map's **Really spawn every pack on arrival** lighter: only the packs near
you are born when you arrive, and the rest as you come near them.

## New

- **Fill the map as you approach.** A new switch in Mods → Quality of Life,
  off by default, right under Extra packs as you approach. It only matters
  while Reveal full map's **Really spawn every pack on arrival** is on. That
  option makes every pack in the zone at once when you arrive, and at high
  density a filled zone then holds thousands of monsters, each with its
  shadow and health bar, which the game keeps updating every frame however
  far away they are. Our frame profile found that work to be the largest part
  of a filled zone's frame. With this switch on, only the packs within about
  3,000 px of you are made when you arrive; the rest are made as you come
  near them, so the zone holds far fewer living monsters at any one time.
  Nothing is hidden, paused or put to sleep, and the packs around you are the
  ones you would meet anyway. Turning the switch off fills the rest of the
  zone you are in. With Extra packs as you approach on as well, Monster
  Density's extra packs are made as you approach under the fill too. Only
  your own character counts; it has not been tried in co-op. This version has
  been tested outside the game; how much it saves in play is not measured yet.

## How to update

Download and extract the complete release, then reopen ForgePact: the panel
gained a switch, so updating only the plugin leaves it out. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. The new switch is in the
plugin too, so press **Install Mod Plugin** once after updating - updating only
the panel leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
