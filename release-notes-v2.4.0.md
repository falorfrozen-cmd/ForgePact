# ForgePact 2.4.0

Release date: 2026-10-23

## Fixed

- **The game no longer runs out of room for monsters at high density (#232).**
  With Density or a Special Content spawner turned far up (for example
  density x5 with Battlefield, Chaos Pillars and Shadow Realm at x100), the
  game could crash to desktop while building a monster, often on entering a
  zone. The game keeps every monster's protected values in one store with
  room for 262,144 of them; once a crowded session filled it, the game used
  the "store is full" answer as if it were a real slot, and crashed. ForgePact
  already had extra room for that store, but in 2.3.1 and earlier nothing ever
  switched it on. Now, as soon as you raise Density, a Special Content
  spawner or the map reveal's spawn pass, ForgePact adds more room behind the
  game's own store, and the game uses it only once its own is full. You still
  get every monster you asked for: nothing is capped or held back, so a very
  crowded zone still costs frame rate, and lowering the multipliers is how to
  get it back. With every population setting at its normal value, nothing
  changes. Tested in a game session on 2026-10-20 at the crash report's
  settings and beyond them (density x5, those three spawners at x100, every
  monster x10 and the map's spawn pass on). The game held over a million of
  those values, four times its own limit, with no crash and a clean close.
  At that extreme, leaving a zone of 28,000 monsters still took the game
  half a minute to unload.

## How to update

Download and extract the complete release, then reopen ForgePact and press
**Install Mod Plugin** once. Only the plugin changed. Your existing settings
are retained. Start the game after installing; a game that was already open
keeps the old plugin until it is restarted.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
