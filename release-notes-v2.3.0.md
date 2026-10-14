# ForgePact 2.3.0

Release date: 2026-10-16

A new **Always show Chaos Pillar mods** switch, off by default, shows each
Chaos Pillar's mods while the pillar is on screen, without hovering over it.
A new **Boss soul pieces** switch, also off by default, lets monsters drop a
piece of their act boss's soul that takes you straight to that boss for a
fresh fight.

Three new charms, **Pocket Elf**, **Gabriel Wings** and **Witch's
Broomstick**, let you walk through scenery and monsters while you wear one,
and each can join the game's own drops behind its own switch, off by default.

HS Offline Tracker now lives inside ForgePact: a new **Tracker** tab counts
your session, keeps a journal of your best drops, plays an alert for each one
and files your runs, with a small always-on-top overlay window beside the
game. It is off by default.

## New

- **HS Offline Tracker is now part of ForgePact (#185).** Until now, keeping
  count of a farming session meant running a second program, HS Offline
  Tracker, beside ForgePact. Its offline features are now ForgePact's
  **Tracker tab**, with one switch, **Session tracker**, off by default; with
  it off, ForgePact tracks nothing. With it on, ForgePact's own plugin counts
  while you play; it only reads the game and changes nothing in it.
  - **Session:** the gold, experience and kills of this run, their rates per
    hour, the Satanic zone you are in with its buffs and debuffs, and a
    journal of the drops it records.
  - **Five rarities:** it records Set, Satanic, Angelic, Heroic and Unholy
    items that drop on the ground. Lower rarities, and items you drop
    yourself from the bag, are not recorded.
  - **Runs:** a session is filed as a run when the game closes (or when you
    end the run yourself), with its own run card you can copy.
  - **Alerts:** a sound plays once for each recorded drop of a rarity you
    turn on and, if you turn it on too, once when your Satanic zone changes.
    The bundled sounds are
    HS Offline Tracker's own; you can use your own sound files instead. The
    panel plays them from its own window, whichever tab is open; if it was
    started in a browser, click anywhere in it once to let it play sounds.
  - **Shopping list and Item codex:** a list of items you are hunting, which
    always plays its alert when one drops, and a book of every named item
    with its drop chance and where to farm it.
  - **Overlay:** a small window that stays on top of the game, with your
    counters, the newest drops and a card for a great find. It is a solid
    plate, not see-through, and it needs the game in windowed or borderless
    mode: a game in exclusive fullscreen draws over it. It opens only from
    ForgePact's own window, not from a browser tab.
  - **Your HS Offline Tracker data comes with you:** the first time you turn
    the tracker on, ForgePact copies the Tracker's runs, settings, shopping
    list and your own alert sounds. It only reads them; HS Offline Tracker's
    files are left as they are.
  - **Offline play only.** The Tracker's online-only parts (mail, purses,
    pickups, magic find) and its Game Link installer are not carried over,
    and neither is its keys and resources counter, which never counted
    offline. Discord status is built in but not offered yet: it needs a
    Discord application of ForgePact's own.

  HS Offline Tracker itself takes no new features from now on; its page
  points to ForgePact's Tracker tab.

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
- **Flight charms: Pocket Elf, Gabriel Wings and Witch's Broomstick (#158).**
  Three charms of ForgePact's own, each with its own icon in the bag and on
  the ground and a gold **Flight** line in its tooltip. While you wear one in
  your charm slots, you walk through rocks, fences, carts, trees and other
  scenery, and your character is drawn in front of it instead of hidden
  behind it, tree canopies included. A room's colour filter (its tint or
  lighting overlay) still covers you as it does on the ground, so in the few
  rooms that put scenery in front of their filter, that scenery stays in front
  of you. You also walk through monsters instead of being held up by them, but
  they still hit you and you still hit them (owner-confirmed in play,
  2026-10-13). Walls (zone gates count as walls), locked doors and the edge of
  the map still stop you. A few stone walls are built by the game as scenery
  rather than as walls, such as the town church's, and those you would walk
  through: a game session measured that the church wall is scenery, not a
  wall, but nobody has walked it with a charm on yet. Take the charm
  off while you stand inside a prop or a monster and you can still walk out;
  nothing moves you on its own. The game
  itself counts only one flight charm at a time: wear two and only one of
  them works.
  - **Pocket Elf** (Satanic, one cell) can drop among the game's own unique
    items. It has no other stats, and its tooltip still shows the gold
    **Flight** line.
  - **Gabriel Wings** (Angelic, 2x2 cells) also lowers the physical and the
    magic damage you take, each by 1 to 12%. It can drop from the game's own
    Angelic roll.
  - **Witch's Broomstick** (Unholy, one cell wide and five tall) also adds 10
    to 100% movement speed. It can drop from the game's own Angelic roll.

  Each charm's drop has its own switch in Mods → Items, all off by default. A
  switch only lets that charm drop, about as rarely as one particular unique
  of the game's own does; it changes no drop chance. Gabriel Wings and Witch's
  Broomstick each come as about 1 in 50 of the Angelic roll's drops, the
  same share as Headhunter and Tyrant's Crown, and Pocket Elf on about 1 in
  51 to 1 in 222 of the ordinary unique roll's draws. A charm you already own
  works whether its switch is on or off.
  If a charm's full size no longer fits where it was saved, it loads at one
  cell in its own place instead of disappearing, and is full size again the
  next time it fits. It also loads at one cell when its full size would cover
  the place of another flight charm in the same bag, instead of pushing that
  charm out.
  Offline play only. Checked in three game sessions on a test build, plus the
  owner's own play check: you walk
  through scenery and stop at walls, you are drawn over tree canopies in town
  and in the Outskirts of Inoya, all three charms show their icons and the
  **Flight** line, the sizes survive a reload, and with each switch on Pocket
  Elf came once in 100 unique drops and Witch's Broomstick twice and Gabriel
  Wings never in 102 Angelic drops, as the shares above predict (the test
  raised the Angelic roll's chance to get that many drops, so how often that
  roll happens in normal play was not measured). Walking through monsters
  came last: the third session showed walls still stop a flying walk, and
  that monsters let you through while they still hit you and you still hit
  them is owner-confirmed in play, 2026-10-13 (the owner's report from
  playing with the charm, not a measured test).

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
  (`HSOfflineTrackerProducer.dll`) that ForgePact up to 2.2.x installed beside
  its plugin, an older build that aborted while it unloaded. ForgePact no
  longer ships that sensor at all: its own Tracker tab does the counting from
  inside ForgePact's plugin (see New). When **Install Mod Plugin** finds the
  old build in your game, it moves it out of the game's mods folder into
  ForgePact's backup folder (`%LOCALAPPDATA%\Hero_Siege\backup\`), checks the
  copy there, and tells you what it moved and why; if the file cannot be
  moved (HS Offline Tracker still running, for example), it says so and
  installs everything else. Tested on 2026-10-13: with the old sensor a close
  from the main menu still crashed, and with HS Offline Tracker 0.1.4's fixed
  sensor in its place a close from the world, a close after Save & Exit, and
  a close from the world with Loot announcements on all exited cleanly with
  no crash report; HS Offline Tracker's own checks of its fixed sensor, on a
  test build with ForgePact loaded, closed cleanly from the main menu three
  times out of three. With no sensor at all, as 2.3.0 leaves your game, a
  close from the main menu, from the world, after Save & Exit, and from the
  world with Loot announcements on all exited cleanly with no crash report,
  tested on a ForgePact build from before 2.3.0.
- **ForgePact leaves other sensors alone.** A sensor ForgePact does not know
  as the crashing build, such as HS Offline Tracker 0.1.4's fixed one or a
  newer one, stays where it is, without a word. **Remove Plugin** no longer
  deletes any HS Offline Tracker sensor, and starting the game from ForgePact
  no longer replaces one.

## Known issues

- **HS Offline Tracker 0.1.1 to 0.1.3 can put the old sensor back.** If you
  still use HS Offline Tracker, update it to 0.1.4 or later, or use the
  Tracker tab instead. Versions 0.1.1 to 0.1.3 offer to update the live
  sensor, and pressing that puts their own old sensor back, and the crash at
  close with it. When you start the game from ForgePact with that sensor in
  place, its launch message says so, and pressing **Install Mod Plugin** moves
  it aside again.

## How to update

Download and extract the complete release, then reopen ForgePact and press
**Install Mod Plugin** once. Both parts changed: the plugin (the Chaos Pillar
switch, the Boss soul pieces switch, the three flight charms and the tracker)
and the panel (their switches on the Mods and Loot tabs, and the new Tracker
tab), so updating only one of them leaves the other behind. Pressing **Install
Mod Plugin** also moves HS Offline Tracker's old, crashing sensor out of the
game, if it is there; ForgePact installs no sensor in its place. If you start
the game from ForgePact without pressing Install, ForgePact still replaces an
older plugin itself, but it leaves the old sensor where it is: its launch
message tells you, and until you press Install Mod Plugin with the game
closed, the crash at close stays. Your existing settings are retained. Start
the game after installing; a game that was already open keeps the old plugin
until it is restarted.

If you used HS Offline Tracker, turn on the Tracker tab's **Session tracker**
switch once: ForgePact then copies your Tracker runs, settings, shopping list
and sounds, and leaves HS Offline Tracker's own files untouched.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
