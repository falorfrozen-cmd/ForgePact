# ForgePact 2.4.0

Release date: 2026-10-23

A new item set of ForgePact's own, **Mark 1**: four Satanic pieces, like the
game's own set pieces, that, worn together with the new **Mark 1 set** switch
on (off by default), cast the skills on your Q, E, R and Y slots for you and
send your jump three times as far over scenery, at the price of walking at
half speed. A second switch, **Mark 1 drops** (also off by default), lets the
pieces drop from the monsters you kill anywhere, like the game's own set
items, and from vaults.

## New

- **Mark 1 set (#166).** Mark 1 is an item set of ForgePact's own: the
  **Mark 1 Helmet**, **Mark 1 Body Armor**, **Mark 1 Boots** and **Mark 1
  Gloves**, four Satanic pieces like the game's own set pieces, each with its
  own icon in the bag and on the ground and a gold **Mark 1** line in its
  tooltip, and no stats of their own. The set's bonus works only while you wear all four and the new switch
  (Mods → Items → **Mark 1 set**, off by default) is on.
  - **Your skills cast themselves.** The skills on your Q, E, R and Y slots
    are cast for you whenever the game would let you cast them: the game still
    checks the cooldown, the mana and every other requirement, and aims each
    one where it aims that key press. The skills take turns: one is pressed
    at a time, and once it is cast the next press goes to the skill that has
    waited longest, so a skill with no cooldown, such as Shadow Bolt, cannot
    keep the one beside it from ever being cast, and a skill that cannot be
    cast yet gives up its turn for a short while instead of holding up the
    others. Each of those slots is outlined. A
    toggle skill is switched on and left on, so you cannot switch it off while
    the bonus is active; an aura slot and an empty slot are left alone.
  - **Your jump goes three times as far.** The universal jump always goes
    three times your normal reach toward your cursor, passes over rocks,
    fences, carts and other scenery, and lands only where you could walk.
    When that spot is blocked it lands on the farthest walkable spot that is
    still past your normal reach, and when there is none you get your normal
    jump. It never crosses a zone gate or a locked door. Your normal reach is
    learned from your own jumps on open ground, shared with Jump through
    scenery; until it is learned, or while a reach learned on a character who
    jumps farther is in use, a jump can stop short of its landing, somewhere
    along the way that was not checked.
  - **In exchange, you walk at half speed.** The set halves your walk through
    your movement speed, so what else reads your movement speed, such as the
    character sheet, can show the changed value while the bonus is on.
    Casting a skill still slows your walk for about a second, as it does
    without the set, on top of the half speed.
  - **Its own art** shows once you have switched either Mark 1 switch on in
    that game session; until then a piece looks like the base item it is
    built on.
  - **Finding the pieces.** Switch on **Mark 1 drops** (Mods → Items, off by
    default; it works with or without the set's own switch). The pieces then
    drop from the monsters you kill anywhere, among the game's own items, the
    way the game's own set items such as White Mage's Sacred set and Zealot's
    Prophecy do, each about as likely as any single unique the game can drop
    from a kill. They also drop from the game's own Heroic roll, the one that
    runs when you open a vault from your bag, each about as likely as any
    single Heroic unique that roll can give you. The rates have not been
    measured yet. A dropped piece shows its own art. With the switch off,
    kills and vaults drop exactly what they always did.

  The set was checked in play on a research build of the plugin: the
  pieces, their art, the outlines and the jump worked, and in that session,
  with every slot pressed at once, Shadow Bolt was cast 20 times and Heavenly
  Fire beside it not once; the turns make sure no one skill is pressed every
  time. A second session checked the turns on that same bar: in about 32
  seconds with no key pressed, Shadow Bolt was cast 31 times, Heavenly Fire 30
  and Healing Zone 3. It also measured the walk at half speed with the set on,
  and a cast's slowdown on top of that: while a cast slows you, you walk at
  about a third of your normal speed. A toggle skill on the bar, and the
  pieces dropping from kills and vaults, have not been checked in play yet.

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
