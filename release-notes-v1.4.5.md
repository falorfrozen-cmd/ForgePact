# ForgePact 1.4.5

The Prospect Cube can now prospect each item the moment you put it in, so you
no longer have to stop every 54 items to press Prospect and start again.

Three new, off-by-default Gameplay Mods for toggle skills: mark the skill-bar
slot of a toggle you have running, stop a double cast proc from flipping a
toggle straight back, and draw a countdown over a plain (non-toggled) cast so
you can see how much time is left before it ends.

A fixed set of toggle skills, each measured in-game, is covered by the toggle
marker and the double-cast guard. The countdown below covers a different set
of its own - see that bullet for what it draws on.

## New

- **Auto-prospect items put in the Prospect Cube.** The cube's 9×6 prospect
  grid fills long before a full inventory is through it: you fill it, press
  Prospect, and fill it again, over and over. With the new **Auto-prospect**
  switch in Gameplay Mods, every item you drag or click into the grid is
  prospected straight away by the game's own Prospect, exactly as if you had
  pressed the button, so the grid never fills with items waiting their turn.
  It is **off by default**.
  - **The previous batch goes to your materials tab.** Each prospect leaves
    its materials in the grid, one stack per material type, as a normal
    Prospect does. When you put the next item in, ForgePact first moves the
    materials that the previous prospect made to your materials tab - the
    same move the game makes when you click a material - and then prospects,
    so the newest batch stays in the grid where you can see it and the grid
    does not fill up with materials. A material that is the first of its kind
    - one with no stack of it yet in your materials tab - is moved too, the
    way a click on it moves it: the game puts it in your bag rather than the
    materials tab. Only that batch moves. The item you put
    in is prospected and not moved, even when it is itself a material such
    as ore, and materials you put in the grid yourself stay where they are -
    with one exception nobody has run into yet: if you swap an item onto one
    of those batch materials and drop that material straight back in, it
    still counts as part of the batch and goes to your tab instead of being
    prospected. Prospecting an ore does not always give materials: the
    game's own Prospect only has a chance to, with or without ForgePact, so
    an ore that is used up with nothing in its place is the game, not the
    mod. After you reopen the cube, turn Auto-prospect off and on,
    or take something out of the grid, the next prospect moves nothing, and
    moving starts again with the batch that prospect makes. This is its own
    switch under Auto-prospect, **Move the previous materials to your
    materials tab**, and it is **on by default** whenever Auto-prospect is
    on; turn it off to keep every material in the grid as before. If the
    game will not take a material, it stays in the grid and
    `bp_ipc\out.txt` says so once (a full bag or materials tab was not
    tested); if ForgePact could not make the move at all, that line also
    says which step failed, so a bug report can say where it stopped. ForgePact only clears a
    material from the grid after the game confirms it reached your materials
    tab or your bag; if one ever leaves the grid without that confirmation, the switch
    turns itself off for the rest of the session and `out.txt` says so. When fewer than 6 cells are free, auto-prospect holds
    back and leaves your item in the grid (the first time in a session, it
    writes a line to `bp_ipc\out.txt` saying so); empty some of the grid and
    carry on.
  - **Anything still in the prospect grid when the game saves is lost.** This
    is how the game itself treats that grid, with or without ForgePact: items
    left in it across a save are gone when you load again. With
    auto-prospect on, the newest batch of materials sits in the grid, so
    empty it before you leave the cube, and especially before you quit.
  - ForgePact runs the Prospect, and moves the materials, for you at a moment
    the game did not choose. The Prospect and the move to the materials tab
    were tested with junk items; the move of a first-of-its-kind material to
    your bag has not been seen in a real session yet. Keep a
    backup of your save before you try either with anything you care about.
  - If ForgePact cannot attach to the game's insert step on your copy of the
    game, the switch turns itself off for that session and `bp_ipc\out.txt`
    says why, instead of pretending to work. `out.txt` also records the first
    item it prospects and the first materials it moves each session, and says
    so once if a Prospect it ran left the grid unchanged, or could not check
    the grid afterwards.
- **Mark a running toggle skill on the skill bar.** A toggle you have switched
  on has no on-screen sign that it is still running - you had to remember
  whether you pressed it, or watch your health bar or the ground for the
  effect. With this mod turned on (Mods tab, off by default), a soft red
  outline appears around that skill's slot on the skill bar the whole time it
  is running, and disappears the moment it stops - on a re-press, on a zone
  change, or if the game cancels it. It covers a fixed set of toggle skills,
  each measured in-game. A plain cast - one made without the sub-talent that
  turns the skill into a toggle - does not light the outline, and neither
  does a skill outside that set.
- **Stop double cast re-casting a toggle skill.** With a double cast effect
  equipped, a double cast proc could cast one of that same fixed set of
  toggle skills a second time on its own, a moment after your press - which
  flipped the toggle straight back,
  so it ended off when you had just turned it on, or on when you had just
  turned it off. With this mod turned on (Mods tab, off by default), that
  extra cast is skipped and the toggle stays the way your press left it. It
  only steps in when you actually have the sub-talent that makes the skill a
  toggle, or the skill is a toggle on its own; otherwise the double cast's
  extra cast goes through exactly as it does in the unmodded game. Your own
  presses, and double casts of every other skill, are not affected.
- **Timed skill countdown.** A new **Timed skill countdown** dropdown in Mods
  (off by default) draws how much of a timed cast is left over that skill's
  slot on the skill bar, and it disappears the moment the cast ends. A small
  set of timed skills were measured in-game and are tested; most other
  skills with both a duration and a real cooldown are also covered, by rule,
  and are untested. A few skills are left out where a measurement showed the
  timer on the skill's own object is not the skill's duration, and companion
  skills (turrets, totems, hydra) are not covered. A few skills whose
  duration is a buff on you, measured in-game, are covered too, and other
  buff-only skills are not. A skill switched on as a toggle never gets a
  countdown. In a fight, hits can add a little time to some skills (roughly
  0.2 s each in our test) and the countdown rises slightly to match. Pick
  one of four looks: **Arc** (a ring that empties clockwise),
  **Bar** (a shrinking bar above the icon), **Number** (a shrinking
  percentage above the icon) or **Fade** (a soft outline that dims as time
  runs out). If you turn this on partway through a cast that is already
  running, that one cast shows as full from the moment you turned it on
  rather than its true remaining time; the next cast on that skill reads
  correctly.
- **Mods tab columns.** The Gameplay and Items mod lists are split into two
  columns, and the right column could end up longer than the left, leaving a
  gap under the left one. The left column is now always the longer one, and
  the cards still read top to bottom, left column first. On a narrow window
  the lists are a single column, as before.
- **A new read-only `menulayout` command lists where the menu buttons
  are.** A tool that plays through the main menu for you - such as the
  toolkit's `hs-drive` helper, which opens a save for testing - used to
  click fixed spots on the screen, and a game patch or a different screen
  layout could silently move a button out from under that spot. ForgePact
  now answers `menulayout` with the live instances of a set of menu objects
  it looks for, and the interface pieces under them, each with its position
  on the game window. `hs-drive` now clicks the positions it lists for
  `Play local`, the save slot and `PLAY`, and stops with a reason instead of
  guessing when a button is not listed. In a test run, clicks at the listed
  `Play local`, first- and second-save-slot and `PLAY` positions all worked
  and loaded the chosen character. Nothing in play changes, and there is no
  new switch in the panel.

## Changed

- **Headhunter and Tyrant's Crown now drop like an Angelic / Unholy item.** They used to drop
  on their own, in every session, about once every 15,000 kills each - far more often than any
  Angelic / Unholy item such as **Liquor Holster** - with no panel control for it. They
  are now two more items in the Angelic / Unholy Drops pool, so they drop exactly as often as
  Liquor Holster does from that slider: never while it is off (the default), more often as it is raised,
  on the very same roll as every other item in the pool. With two more items sharing each hit,
  every other Angelic / Unholy item is very slightly rarer. The game's own Angelic drops (from a
  Blood Pact or dungeon "Angelic item drop chance" modifier) still do not include either item -
  only ForgePact's own die does.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This release changes both the
plugin and the panel, so pressing **Install Mod Plugin** matters - updating only
the panel leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
