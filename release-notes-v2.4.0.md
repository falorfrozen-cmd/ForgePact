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
- **Very high damage no longer crashes the game when damage numbers are
  shown (#230).** With damage raised very high, hitting a training dummy
  could close the game to desktop with no error, when the game's damage
  numbers were switched on and shortened (the "12,3K" / "5M" style). The
  crash report had Total Damage and Critical Strike Damage both at 1000% and
  Critical Strike Chance at 500%. The game's own number shortener cannot write a number of 100
  trillion or more, and ends the game when it is handed one. Now, while any
  Modifiers slider that can raise a damage number is on, ForgePact hands it
  the largest number it can show instead, so a damage number that large
  reads `99999B`. The damage you deal is unchanged: only the number written
  on screen stops at that limit.
  With every Modifiers slider at its normal value, nothing changes. Tested
  in a game session on 2026-10-20 with a development build of the plugin, in
  Act 1's town at the crash report's settings and then with Total Damage
  raised in steps: with this fix turned off the game closed with the crash
  report's error code once a damage number reached about 102 trillion (the
  session's own figure; the report names no number); with it on, damage
  numbers up to about 100 quintillion (1e20) all read `99999B`, the game
  kept running through every step and closed cleanly.

## How to update

Download and extract the complete release, then reopen ForgePact and press
**Install Mod Plugin** once. Only the plugin changed. Your existing settings
are retained. Start the game after installing; a game that was already open
keeps the old plugin until it is restarted.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
