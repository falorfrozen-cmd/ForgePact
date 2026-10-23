# ForgePact 2.4.0

Release date: 2026-10-23

A new item set of ForgePact's own, **Mark 1**: four Satanic pieces, like the
game's own set pieces, that, worn together with the new **Mark 1 set** switch
on (off by default), cast the skills on your Q, E, R and Y slots for you and
send your jump three times as far over scenery, at the price of walking at
half speed. A second switch, **Mark 1 drops** (also off by default), lets
the pieces drop from the monsters you kill, among the game's own SS unique
items, and adds them to the Heroic roll a vault runs. See "Finding the
pieces" below.

## New

- **Mark 1 set (#166).** Mark 1 is an item set of ForgePact's own: the
  **Mark 1 Helmet**, **Mark 1 Body Armor**, **Mark 1 Boots** and **Mark 1
  Gloves**, four Satanic pieces like the game's own set pieces, each with its
  own icon in the bag and on the ground.
  - **Their own stats.** Each piece rolls its own stats within a range, like
    any item: Defense and Enhanced Defense on every piece, and All
    Resistances, All Skills, Life, Life after each Kill, Vitality, Faster Hit
    Recovery or All Attributes depending on the piece; the helmet has 1 or 2
    sockets and the body armor 3 or 4. A piece's stats count whatever the
    switches say and stay the same after saving and loading. Wherever the
    game shows its own items' roll ranges, each rolled line shows its range
    in grey after it, as theirs do.
  - **The set bonus, by pieces worn**, while the new switch (Mods → Items →
    **Mark 1 set**, off by default) is on: with 2 pieces `+500 Defense`; with
    3 also `+35% to All Resistances` and `Replenish Life 25%`; with all 4
    also `+1 to Automation`, `+5 to All Skills`, `+750 to Life`, `Physical
    Damage Taken Reduced by 20%` and `-50% Movement Speed`. Each piece's
    tooltip reads `Satanic Set <type>` under its name and shows its own
    stats, then the set's bonus lines the way the game shows its own sets,
    each lit while you wear enough pieces for it, with what Automation does
    underneath in the game's smaller grey description text, then the set's
    name and its four pieces, the ones you wear in green. Your character
    sheet (the talent screen, T) shows the set's Life, Defense and its other
    lines too; your mercenary and monsters never get the set bonus, and this
    version keeps the set's numbers off the sheet's Mercenary tab, which
    shows your mercenary's own numbers.
  - **Your skills cast themselves.** With all four worn, the skills on your
    Q, E, R and Y slots are cast for you. A skill that is not a toggle fires
    the way an item's proc fires a skill: at once, with no cast time, so a
    button you hold yourself, such as the right mouse button, never keeps it
    from firing. Each fires on its own timer, its cooldown (shortened by
    your Skill Haste) plus its cast time (shortened by your Faster Cast
    Rate), so a skill with no cooldown, such as Shadow Bolt, fires
    continuously; when two are due at once they go in Q, E, R, Y order.
    These casts cost no mana, so instead **each automated skill drains about
    1% of your maximum mana a second** while the bonus is on, taken off your
    mana regeneration: how many automated skills your regeneration keeps
    going depends on how much you have, and a drain larger than your
    regeneration empties your mana. A skill under automation cannot be
    cast by hand, and its icon on the skill bar is drawn grey, the way the
    game greys a skill you cannot use; each of those slots is also outlined.
    A toggle skill is still pressed for you and switched on and left on, so
    you cannot switch it off while the bonus is active; an aura slot and an
    empty slot are left alone.
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
  - **In exchange, you walk at half speed** (`-50% Movement Speed`). The set halves your walk through
    your movement speed, so what else reads your movement speed, such as the
    character sheet, can show the changed value while the bonus is on.
    Casting a skill still slows your walk for about a second, as it does
    without the set, on top of the half speed.
  - **Its own art** shows from the moment you have a piece, whatever the
    Mark 1 switches say: a piece loaded from your save, one that drops and
    one placed for a test alike. If its art cannot load, a piece looks like
    the base item it is built on.
  - **Finding the pieces.** Switch on **Mark 1 drops** (Mods → Items, off by
    default; it works with or without the set's own switch). The pieces then
    drop from kills, checked in Hell (zone levels 243 to 276); whether
    lower-level zones and the other difficulties drop them is not yet known.
    When a kill drops a unique item, the game draws
    it from one of its unique lists; each piece stands in on those lists for
    one of the game's own SS Satanic uniques of its kind, the way the game's
    own SS set pieces drop. SS uniques are rare, and how often a kill gives a
    piece at the normal setting has not been measured. The pieces also join
    the game's own Heroic roll, the one that runs when you open a vault from
    your bag, each about as likely as any single Heroic unique that roll can
    give you; a piece dropping from a vault has not been seen in play yet. A
    dropped piece shows its own art. The pieces are **SS tier**, so a loot
    filter that shows SS items shows them. With the switch off, kills and vaults
    drop exactly what they always did.

  The set was checked in play on a research build of the plugin: the
  pieces, their art, the outlines and the jump worked, and in that session,
  with every slot pressed at once, Shadow Bolt was cast 20 times and Heavenly
  Fire beside it not once, which is why the skills were then pressed one at
  a time. A
  second session checked the earlier rule, the skills taking turns, on that
  same bar: in about 32
  seconds with no key pressed, Shadow Bolt was cast 31 times, Heavenly Fire 30
  and Healing Zone 3. It also measured the walk at half speed with the set on,
  and a cast's slowdown on top of that: while a cast slows you, you walk at
  about a third of your normal speed. A third session checked the drops from
  kills: with the pieces' share raised 400-fold and magic find raised, so
  that they would show up within one session, kills in Hell dropped all four
  pieces, 8 in all among the 67 unique items the game's draw built. Two more
  sessions checked the set display in the tooltip, the pieces' `Satanic Set
  <type>` line and the art: the pieces showed their own art with every Mark
  1 switch off, on the ground, in the bag and worn, and again after saving
  and loading the character; and each piece's tooltip showed the set the way
  the game shows its own, on the ground, in the bag and worn, the names of
  the pieces you wear in green and `+1 to Automation` lit only with the set's
  switch on, every line inside the tooltip. A later session checked that a
  skill fired the way an item's proc fires one is cast at the skill's own
  level, the grey icons in the game's own dark look, and the 20% lower
  maximum mana with three automated skills (that build's cost; the mana
  drain replaced it). A second session that day checked the roll ranges,
  the set's bonus lines lit by pieces worn and their values added to your
  stats (your maximum life on the HUD went from 4901 to 5651 with all four),
  half speed, the skills firing in Q, E, R, Y order with the toggle skill
  on R pressed only while it was off, a skill with no
  cooldown firing about as often as holding its key casts it, the skills
  firing while the right mouse button was held with that button's own skill
  still casting, an automated skill's key casting nothing, and the grey
  icons; those counts are the times the set fired a skill, not casts seen
  landing. A third session checked the mana drain, 1% of your maximum mana
  per automated skill taken off your mana replenish, which the game turned
  into about three-quarters of 1% a second (about 1.5% of your maximum a
  second with two automated skills in that session; this version scales the
  drain by the replenish rate that session measured, aiming at about 1% a
  second per skill), and that a drain larger than your regeneration empties
  your mana;
  and the character sheet
  showing the set's Life, Defense and Physical Damage Taken Reduced with
  all four worn, the same numbers the game's own stat calls return for
  you, while your mercenary and summons got none of the bonus; it also
  found the sheet's Mercenary tab showing the set bonus and the drain on
  your mercenary's numbers. A fourth session checked the fix: the
  Mercenary tab now shows your mercenary's own numbers and your own tab
  still shows the set's; and with all four worn, each of four times a
  monster hit you the game asked for your Physical Damage Taken Reduced,
  which the set answers with its 20% added (the session read the ask, not
  the number the game got back), and once, not at a hit, for your Defense.
  A fifth session measured this version's drain on the test character,
  with two automated skills: about 1.1% of your maximum mana a second per
  automated skill at about 144 frames a second,
  and a little less, about 0.96%, at 60 frames a second.
  Not checked in play yet: whether the set's Defense and Physical Damage
  Taken Reduced make a hit do less damage (your life stayed full), whether
  every fired skill casts and a piece dropping from a vault.

- **Loot announcements show the item in its rarity's colour (#228).** With
  Loot announcements on, the chat line for a Heroic, Angelic or Unholy item
  used to be all red, `SERVER: <your character> found <item>`. Now it looks
  like the game's online loot announcement, for example `[22:51] Sorak:
  found Headhunter`: your character's name in orange, and "found" with the
  item's name in the colour the online announcement uses for its rarity,
  green for Heroic, pale yellow for Angelic and crimson for Unholy. If the
  game ever refuses the coloured line, you get the old red line instead.
  Which items are announced is unchanged. Tested in game sessions on
  2026-10-23 with a development build of the plugin: a Heroic, an Angelic
  and an Unholy item placed with a development command each got a line with
  the name in orange and "found" and the item's name drawn in exactly its
  rarity's colour; a Satanic item got no line, and with the switch off
  neither did a Heroic one. The red fallback line was not seen
  in play (it shows only if the coloured line is refused, which did not
  happen), and neither was a coloured line for an item dropped by a kill.
  As before, the line goes to the chat's general channel, so with the
  game's general chat switched off it may not appear (not tried in play).

## Fixed

- **Pocket Elf did not drop from kills (#166).** Since 2.3.0, switching
  on **Pocket Elf drops** added Pocket Elf only to a unique roll that kills
  seldom run, so in practice no kill dropped one: in a 30-minute test of
  kills in Act 1 on Hell, that roll did not run once. Now Pocket Elf joins
  the draw the game itself uses for a kill's unique items, standing in for
  one SS Satanic unique, the rarity and tier of Aimbot.exe, so it drops from
  kills, checked in Hell (zone levels 243 to 276); whether lower-level zones
  and the other difficulties drop it is not yet known. SS uniques are rare, and how often a kill gives a Pocket
  Elf at the normal setting has not been measured. Tested in a game session
  on 2026-10-21 with Pocket Elf's share raised 400-fold and magic find
  raised: a Pocket Elf dropped from a kill in Hell. With the switch off,
  kills drop exactly what they always did.
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

- **Loot announcements now work in the Chaos Tower (#227).** With Loot
  announcements on, a Heroic, Angelic or Unholy item that dropped on a Chaos
  Tower floor got no chat line, while the same items were announced
  everywhere else. The tower keeps a floor's drops off the ground until the
  floor is cleared and then puts them all down at once, and ForgePact took
  that late landing for an item you had dropped from your bag, which it never
  announces. Now an item held back on a tower floor is announced when the
  floor's loot lands at its end, with the usual loot announcement line (now
  in the item's rarity colour, see #228 above). Tested in a game session on 2026-10-23 with a
  development build of the plugin, on the first floor of the tower: a Heroic
  item put on the floor with a development command was announced when the
  floor was cleared, and with the fix switched off another one got no line,
  as in the report. The floor's own drops were seen held back and landing at
  the end, but none of them was a Heroic, Angelic or Unholy item, so a line
  for a naturally dropped one was not seen in that session. Angelic and
  Unholy items in the tower were not tried. An item you drop from your bag
  on a tower floor is meant to stay unannounced, as it does everywhere else;
  that case is covered by ForgePact's own tests but was not tried in a game
  session yet.

## How to update

Download and extract the complete release, then reopen ForgePact and press
**Install Mod Plugin** once. Both the plugin and the panel changed: the panel
has the new **Mark 1 set** and **Mark 1 drops** switches under Mods → Items.
Your existing settings are retained. Start the game after installing; a game
that was already open keeps the old plugin until it is restarted.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
