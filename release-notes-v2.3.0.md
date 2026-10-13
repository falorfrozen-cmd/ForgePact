# ForgePact 2.3.0

Release date: 2026-10-16

A new **Always show Chaos Pillar mods** switch, off by default, shows each
Chaos Pillar's mods while the pillar is on screen, without hovering over it.
A new **Boss soul pieces** switch, also off by default, lets monsters drop a
piece of their act boss's soul that takes you straight to that boss for a
fresh fight.

## New

- **Always show Chaos Pillar mods (#7).** A Chaos Pillar shows its box of
  mods (pack size, magic find, experience, extra loot and the rest) only while
  your mouse cursor is on it, so choosing which pillar to activate means
  pointing at each one in turn. With this switch on (Mods → Quality of Life,
  off by default), every Chaos Pillar on screen shows its box on its own, like
  a strongbox in Path of Exile. It is the game's own box, with the game's own
  text and colours; a pillar that goes off screen hides it again. A pillar you
  have activated stops showing its box; in our session the game then replaced
  it with its fight within a few seconds. Switching it off returns every
  pillar to hover-only at once. It works with mouse play; gamepad is not
  supported. Checked in a game session with mouse play: boxes showed without
  hovering, hid when the pillars went off screen and when one was activated,
  and switching off brought back the hover-only boxes.
- **Boss soul pieces (#197).** Fighting an act's boss again means making your
  way back through the act to its lair. With
  this switch on (Loot → Boss soul pieces, off by default), monsters in an
  act's zones sometimes drop a Piece of that act's boss's Soul (Gurag's in Act
  1, the Grim Reaper's in Act 2, Mevius' in Act 7, and so on for all nine
  acts): 1 in 7,200 whenever the game rolls a gem for a dying monster (five
  quarters of the Gem of Incarnation's listed 1 in 9,000; how often pieces
  turn up next to the gem in play has not been measured). Right-click a piece in your bag
  and confirm, and you are taken straight to that act's boss lair for a fresh
  fight, even if you beat the boss before; no portal opens. It works from a
  town or any zone of an act, and is refused (you keep the piece) inside a
  boss lair or an uber realm. In your bag a piece looks like a Gem of
  Incarnation, and pieces of the same boss stack into one cell, with the count
  drawn on it (Gurag's pieces in one stack, the Grim Reaper's in another, never
  with a real item); right-click a stack and confirm, and one piece is used and
  the rest stay. Pieces stack only while the switch is on. A stack keeps its
  count when the game saves your bag, which it does when you close the stash or
  save and exit to the main menu (closing the game does not always save it).
  Using a piece makes the game forget what it kept of that lair, which may
  include loot left on its floor (not tested). A piece is an Edible PAS Logo
  underneath, so your save needs no new item from ForgePact to load; what a
  piece shows and does with ForgePact removed has not been checked. Checked in
  play on 2026-10-12 with a player build from before the stacks were added, in
  Acts 1 and 2: monsters dropped named
  pieces, and none with the switch off (those drops were forced with the
  `soulpiece force on` command, so the 1 in 7,200 chance itself was not seen in
  play); a piece used in a zone or in town took the character straight to the
  Act 1 or Act 2 lair and was used up; Gurag came out, was beaten and came out
  again on the next piece; and the Grim Reaper came out in his lair. The gem
  icon and the stacks were checked in research sessions with a test build:
  pieces of one boss stacked on pickup, the bag drew the count, each use took
  one piece from the stack, and stacks kept their counts through a save and a
  restart. A player build with the stacks was then checked on 2026-10-12 in
  Acts 1 and 2: two pieces of each boss made one stack per act with the gem
  icon and the count 2, using the Gurag stack took the character to the Act 1
  lair and left one piece, and both stacks were back with their counts after
  the stash was closed and the game restarted. The lairs of Acts 3 to 9 were
  not visited.

## Fixed

- **Closing the game while playing, or after Save & Exit, no longer leaves a
  crash report.** With ForgePact installed, Windows reported Hero Siege as
  crashed (`ucrtbase.dll`, `0xc0000409`) on nearly every close, even ordinary
  ones from the main menu or after Save & Exit. The report came after the game
  had finished its own shutdown. The cause was the HS Offline Tracker sensor
  that ForgePact installs beside its plugin, an older build that aborted while
  it unloaded. ForgePact now installs HS Offline Tracker 0.1.4's sensor, which
  carries the Tracker's own fix. Tested on 2026-10-13: with the old sensor a
  close from the main menu still crashed, and with the new one a close from
  the world, a close after Save & Exit, and a close from the world with Loot
  announcements on all exited cleanly with no crash report. We did not repeat
  a close from the main menu with the new sensor; HS Offline Tracker's own
  checks of the same fix, on a test build with ForgePact loaded, closed
  cleanly from the main menu three times out of three. ForgePact and HS
  Offline Tracker 0.1.4 now install the same sensor and loader files (checked
  file by file against the Tracker's 0.1.4 download).
- **HS Offline Tracker 0.1.1 to 0.1.3 can put the old sensor back.** If you
  also use HS Offline Tracker, update it to 0.1.4 or later. Versions 0.1.1 to
  0.1.3 offer to update the live sensor when they see ForgePact's, and
  pressing that puts their own old sensor back, and the crash at close with
  it. ForgePact replaces it again the next time you start the game from
  ForgePact, so the two keep swapping the file until the Tracker is updated.
- **Install Mod Plugin no longer overwrites a sensor it does not recognise.**
  If your game has a sensor from a newer HS Offline Tracker, Install leaves
  it as it is and says so, instead of replacing it with ForgePact's copy.

## How to update

Download and extract the complete release, then reopen ForgePact and press
**Install Mod Plugin** once. Both parts changed: the plugin (the Chaos Pillar
and Boss soul pieces switches) and the panel (their switches on the Mods and
Loot tabs), so updating only one of them leaves the other behind. Pressing **Install Mod
Plugin** also replaces an old HS Offline Tracker sensor with the fixed one.
If you start the game from ForgePact without pressing Install, ForgePact
replaces an old sensor itself when it can, as it does the plugin, and when it
cannot, its launch message asks you to close the game and press Install Mod
Plugin; until you do, the crash at close stays. Your existing settings are retained. Start the
game after installing; a game that was already open keeps the old plugin
until it is restarted.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
