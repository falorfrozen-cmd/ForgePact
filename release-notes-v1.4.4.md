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
  - **Materials stay in the grid.** Each prospect leaves its materials there,
    one stack per material type, as a normal Prospect does. Take them out as
    you go. When fewer than 6 cells are free, auto-prospect holds back and
    leaves your item in the grid (the first time in a session, it writes a
    line to `bp_ipc\out.txt` saying so); take the materials out and carry on.
  - **Anything still in the prospect grid when the game saves is lost.** This
    is how the game itself treats that grid, with or without ForgePact: items
    and materials left in it across a save are gone when you load again. With
    auto-prospect on, materials sitting in the grid become the normal state,
    so empty it before you leave the cube, and especially before you quit.
  - ForgePact runs the Prospect for you at a moment the game did not choose.
    It was tested with junk items; keep a backup of your save before you try
    it with anything you care about.
  - If ForgePact cannot attach to the game's insert step on your copy of the
    game, the switch turns itself off for that session and `bp_ipc\out.txt`
    says why, instead of pretending to work. `out.txt` also records the first
    item it prospects each session.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This release changes both the
plugin and the panel, so pressing **Install Mod Plugin** matters - updating only
the panel leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
