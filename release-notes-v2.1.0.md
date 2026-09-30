# ForgePact 2.1.0

Release date: 2026-10-02

Emptying your backpack into the stash used to take one Ctrl + click per item.
A new **Move all into the stash** switch, off by default, does it with one
button or one key.

A new **Extra packs as you approach** switch, off by default, makes Monster
Density's extra spawners only as you come near them, so high density costs the
game less every frame.

**Remove owned relics from drop pool** now really keeps your 10/10 relics from
dropping.

## New

- **Move all into the stash** (Mods → Quality of Life, off by default). With the
  stash open, click the new **Move all** button, just left of the backpack's
  **Sort** button and level with it, or press **F4**, and every item on the
  backpack tab you are looking at moves into the stash tab you are looking at,
  one item at a time, by the game's own move for that item - the same one a
  Ctrl + click makes - so each item lands where a hand move would have put it.
  - The **Move all** button is there only while the switch is on and the stash
    is open; switching off removes it. One click is one move-all, exactly like
    F4. If the button cannot be shown, F4 still works.
  - **When the stash tab fills up**, the items that fit move and the rest stay
    in your backpack. They never spill onto another stash tab or page, and
    nothing already in the stash moves.
  - Works on any Personal or Shared stash page, and on the **Materials** tab
    from your backpack or its Materials view. A stash stack holds up to 999: a
    material (or any other stackable) joins a stack of its kind that still has
    room for all of it, and when every stack of its kind is too full, or there
    is none, it starts a new stack in a free cell of the same tab. It is never
    split, and with no free cell it stays in your backpack. An item the tab
    does not take stays in your backpack.
  - On the **Socketable** tab, from the backpack's Socket view, a socketable
    whose kind is already on the tab joins that stack, a single one or a whole
    stack of them. The tab keeps one stack per kind, so a kind the tab does not
    have yet stays in your backpack.
  - **Not supported yet:** the Unique tab, and the backpack's Key, Tarot and
    Relic views. The button and F4 move nothing there.
  - If a move cannot be confirmed afterwards, the run stops, the item is taken
    back out of the stash tab where possible, and the switch turns itself off
    until you restart the game; the panel shows `off (this session)` beside it.
  - The button and F4 do nothing unless the switch is on, the game is the window
    in front and the stash is open, and nothing with Alt, Ctrl or Shift held, so
    Alt + F4 still only closes the game. The stash is saved when you close it, as
    usual.
- **Extra packs as you approach.** A new switch in Mods → Quality of Life, off
  by default; it matters only with Monster Density above 1x. Monster Density's
  extra spawners are made within about 3,000 px of you, and ahead of you as
  you move, instead of across the whole zone the moment you arrive. What you
  meet is the same: in Act 1's first zone at 5x, the spawners and monsters
  near you were identical, while the zone held 430 spawners instead of 1,570
  and the game's own work per frame fell from 84% to 70% of a 60 fps frame.
  With Reveal full map's **Really spawn every pack on arrival (heavy)**, every
  spawner is made at once as before, and while the Beacon or Tyrant's Crown
  has monsters hunting you, the switch reaches as far as the hunt does.
- **ForgePact now notices an old mod plugin in your game.** Updating ForgePact
  never replaced the plugin inside the game, so after an update the game kept
  the previous version's plugin: new switches showed ON and did nothing, and
  nothing said why. ForgePact now compares the plugin in your game with the one
  it ships. When yours is older, Setup, the status bar and the warning icon say
  so, and **Launch Modded Game** puts the new plugin in place before the game
  starts. It does this only while the game is closed. When an update also
  changes YYToolkit or AurieCore, it leaves them alone and asks you to press
  **Install Mod Plugin** instead.
  - **Install Mod Plugin** now checks again that the game is closed right before
    it copies anything, and a file that is in use is reported in plain words
    instead of a raw Windows error.

## Fixed

- **Remove owned relics from drop pool now works.**
  - Before, a relic you already had at 10/10 kept dropping even with the switch on,
    while ForgePact's log said it was holding that relic back.
    - The switch changed a value the game does not use when it picks a relic.
    - Relics from Satanic zone kills never went through it at all.
    - Relics in your backpack's relic tab were not checked.
  - Now, when the game picks a relic you own at 10/10, worn or in the relic tab, it
    picks again. Another relic drops in its place, and every other relic keeps its
    usual odds.
  - If every relic that can drop is already at 10/10, the switch stands down.
  - `relicfilter status` in the log shows how many maxed relics it has skipped.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. Move all into the stash,
Extra packs as you approach and the relic fix are in the plugin. **Launch Modded Game** brings
the plugin up to date for you. If you start the game from Steam instead, or
ForgePact's warning asks for it,
press **Install Mod Plugin** once after updating.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
