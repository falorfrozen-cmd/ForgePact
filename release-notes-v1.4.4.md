# ForgePact 1.4.4

The Prospect Cube can now prospect each item the moment you put it in, so you
no longer have to stop every 54 items to press Prospect and start again.

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
    in is always prospected and never moved, even when it is itself a
    material such as ore, and materials you put in the grid yourself stay
    where they are. Prospecting an ore does not always give materials: the
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
    tested). ForgePact only clears a
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
    the game did not choose. The Prospect was tested with junk items; the
    move to the materials tab, and the move of a first-of-its-kind material
    to your bag, are still being tested on this build. Keep a
    backup of your save before you try either with anything you care about.
  - If ForgePact cannot attach to the game's insert step on your copy of the
    game, the switch turns itself off for that session and `bp_ipc\out.txt`
    says why, instead of pretending to work. `out.txt` also records the first
    item it prospects and the first materials it moves each session, and says
    so once if a Prospect it ran left the grid unchanged, or could not check
    the grid afterwards.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This release changes both the
plugin and the panel, so pressing **Install Mod Plugin** matters - updating only
the panel leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
