# ForgePact 1.4.8

## New

- **`menulayout` now also lists the town stash and the bag.** `menulayout` is
  the read-only command a tool such as the toolkit's `hs-drive` helper uses
  to find buttons on the game window instead of clicking fixed spots. It now
  also lists the stash window and the stash in town, the stash and bag tab
  buttons, the item grids with their size in grid cells, the split-stack
  dialog, the inventory's drag object (`UI_Inventory_Drag_obj`) and your
  character, and prints a few more of each one's own settings
  beside its position - among them which stash tab and which bag tab are on
  show. Under each item grid it adds one line per filled grid cell, naming
  the item there. Nothing in play changes, and there is no new switch in the
  panel.
- **`menulayout` now also lists your skill bar and the talent screen.** The
  same read-only command now lists the skill bar, the talent screen and its
  buttons, with one line per skill slot naming the skill bound there and
  where its button is on the window. Nothing in play changes, and there is no
  new switch in the panel.
- **Two commands for tools that test your skills: `skillstate` and
  `talentalloc`.** A tool such as `hs-drive` can now read your skill bar,
  the talents you have learned and their sub-talent nodes with `skillstate`
  (it also counts how many of a skill's effect objects are alive, which a
  tool compares before and after a key press), and put a point into a talent you have not learned yet, or into
  one of its sub-talent nodes, with `talentalloc`. `talentalloc` presses the
  talent's own button on the talent screen the way the game does, so the
  game checks and records the point itself, and reports whether your learned
  talents changed. Neither reads your points left or a talent's level, and
  there is no command to change which skill sits in a bar slot or to reset
  your talents. Nothing in play changes unless a tool sends one of them, and
  there is no new switch in the panel.
- **Five commands for tools that set up a test at the stash: `playerwarp`,
  `stashtab`, `bagtab`, `stashclose` and `giveitem`.** They let a tool such
  as `hs-drive` get your character ready for a test without anyone at the
  keyboard. `playerwarp` moves your character to a spot in the room (the
  tool uses it to stand you next to the town stash before it presses the
  interact key). `stashtab` switches the open stash to another tab, and
  `bagtab` switches the bag beside it to its Materials or Socket tab, each
  through the game's own tab handler. `stashclose` closes the stash through
  its own close button's handler, which is what saves the stash.
  `giveitem` gives your character one more unit of a material already in
  your bag (the only kind confirmed so far; other items, such as a
  non-stackable, may be refused), made by the game's own item loader. Each
  says what it changed, or why it changed nothing. None of them runs unless
  a tool sends it, none has a switch in the panel, and none acts on its own
  during play. Whether the game treats an item made by `giveitem` exactly
  like a dropped one in every check it runs has not been fully confirmed
  yet.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This change is in the plugin,
so pressing **Install Mod Plugin** matters - updating only the panel leaves the
old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
