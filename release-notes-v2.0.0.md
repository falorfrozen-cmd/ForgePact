# ForgePact 2.0.0

The panel has a new look, and three new controls: a list of the mods you have
turned on, with a button to turn each one off, an on/off switch on every
slider, and a choice of colour theme. Every control still does exactly what it
did before.

Tools that drive the game for testing, such as the toolkit's `hs-drive` helper,
also get new plugin commands for the stash, the bag and your skills.

## New

- **Enabled mods.** A new list near the top of the panel, just under the Apply
  controls, shows every mod you have turned on, with its current value, and
  says how many are on (**Nothing is on** when none are). Each entry has a
  **Turn off** button that switches that mod off exactly as its own control
  would, and the mod leaves the list. Settings that are only options of
  another mod (map population, the auto-prospect material move, the
  gem mod filter) and the Satanic Zone modifiers are not listed.
- **An on/off switch on every slider.** Every slider now has its own switch,
  like Monster Density's. Turning a slider off keeps the value you set, while
  the game plays as if the slider were at its default; turning it back on
  sends your value again. **Apply all** leaves a switched-off slider at the
  default.
- **Theme.** A new **Theme** choice, in the **Appearance** card at the end of
  the Setup tab, picks the panel's colour theme. Click it and a short list
  opens, showing a small colour preview of each theme beside its name, so you
  can see what you are picking; the keyboard works too. The panel remembers it
  with its other settings, so your choice is still there the next time you
  open ForgePact.

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

## Changed

- **A new look for the whole panel.** The tabs sit in one bar along the top of
  the window, each page is laid out in cards, and the panel comes in three
  colour themes: **Ledger** (the default), **Graphite** and **Sigil**. Its
  fonts come with ForgePact, so the panel looks the same on every PC and still
  works with no internet connection. Every control sends the same settings to
  the game as before.
- **Small touches.** The footer now reads "Created by Falor and ST4H". A light
  line separates the groups on the Modifiers tab. The Miner's Helmet card
  shows the helmet's stats in the accent colour.
- **Tooltips and menus move gently.** Tooltips, the list of enabled mods, the
  theme list and the messages at the bottom fade in; they appear at once when
  you use the keyboard, and they only fade, without moving, when your system is
  set to reduce motion. Buttons and tabs still ease into their hover colour
  with reduce motion on. A screen reader now reads the line under a slider along
  with the slider.
- **Gems of Incarnation moved to the Loot tab.** The two switches and the mod
  filter now have their own card at the end of the Loot tab, instead of sitting
  among the Mods tab's Quality of Life switches. They work as before. The mod
  filter's list can now be searched and narrowed to **Enabled** or
  **Disabled** mods, like the Satanic Zone mods on the World tab, and it says
  **Unsaved changes** until you press **Save filter**.
- **Mining Ore Amount is now called Mining Ore Multiplier.** The Loot slider
  works as before; only its name changed, and the help sentence under it is
  gone. While the game runs, the line under it still says when a worn
  Miner's Helmet replaces the slider, or when the plugin could not enable it.
- **The "Choose your Hero_Siege.exe" band is now a warning icon.** Instead of
  a band across the top of every page, a small warning icon sits beside
  **Apply all now** and another beside **Settings loaded** at the bottom.
  Point at either one, or tab to it, to read the same message; click it to go
  to Setup.

## Fixed

- **Blank panel window when ForgePact's ports were all in use.** ForgePact
  tries five ports of its own (8780, 8801, 8899, 9133 and 9777). When all five
  were taken, it let Windows pick any free port, and Windows could pick one the
  panel window is not allowed to open, such as 6000 or 10080. The window then
  stayed blank. ForgePact now skips those ports and asks for another. If it
  still cannot find one, it tells you so instead of opening an empty window.
  This was rare: it needs all five ports busy, and a PC whose free-port range
  has been changed from Windows' default.
- **Remove owned relics from drop pool did not see the relics you wear
  (#93).** It never looked in the five equipped relic slots, so a relic at
  10 out of 10 that you were wearing could still drop again. It now reads
  those five slots, and a maxed relic you wear is held back like any other.
- **A high Gold drop multiplier froze the game (#77).** The multiplier
  dropped that many coins instead of one bigger coin, and for gold from
  monsters it was applied twice: at 100x one monster's gold became 10,000
  coins, and the game stalled for several seconds when they dropped and
  again when you picked them up. The Gold multiplier now raises the amount
  of the one coin the game drops, so 100x gives one coin worth 100 times as
  much. This also means gold from monsters now comes out at the multiplier
  you set: before, 10x gave about 100 times the gold, and now it gives 10
  times.
- **Timed skill countdown showed nothing on Mana Orb (#83).** Mana Orb now
  shows its countdown over its skill-bar slot. It was checked in-game with
  the Chosen One upgrade, where the orb follows you; a Mana Orb without
  Chosen One was not checked.
- **Craft from the stash gave one vague reason for every refused craft
  (#80).** Every refusal said `unreadable`, whatever stopped it. Each reason
  now has its own name in the log: the stash counts had already been used
  for an earlier craft, could not be matched to what the recipe needs, the
  recipe could not be identified, or belonged to another recipe. A craft is
  also refused, with nothing moved from the stash, when the game's own item
  hash step did not run after a stack was changed; before, that move still
  counted as done.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. The panel changes need no new
mod plugin, but the new tool commands are in the plugin, so press **Install Mod
Plugin** once after updating - updating only the panel leaves the old plugin in
place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
