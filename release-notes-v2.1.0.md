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

Two new sliders on the Modifiers tab, **Skill Haste** and **All Skills**, add
to your own totals the way Faster Cast Rate does.

## New

- **Move all into the stash** (Mods → Quality of Life, off by default). With the
  stash open, click the new **Move all** button, left of the backpack's **Sort**
  button, or press **F4**, and every item on the backpack tab you are looking at
  moves into the stash tab you are looking at, one item at a time, by the game's
  own move for that item - the same one a Ctrl + click makes - so each item
  lands where a hand move would have put it.
  - The **Move all** button is there only while the switch is on and the stash
    is open; switching off removes it. One click is one move-all, exactly like
    F4. If the button cannot be shown, F4 still works.
  - **When the stash tab fills up**, the items that fit move and the rest stay
    in your backpack. They never spill onto another stash tab or page, and
    nothing already in the stash moves.
  - Works on any Personal or Shared stash page, and on the **Materials** tab
    from your backpack or its Materials view: a material joins the stack of its
    kind there whole, or takes a free cell if the tab has none of that kind yet.
    An item the tab does not take stays in your backpack.
  - On the **Socketable** tab, from the backpack's Socket view, a socketable
    whose kind is already on the tab joins that stack when it is a single one.
    A stack of more than one socketable stays in your backpack, and so does a
    kind the tab does not have yet.
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
- **Skill Haste and All Skills** (Modifiers → Offense, beside Faster Cast Rate,
  both off by default). Each adds to your own total after the game has worked it
  out, so your gear and buffs still count underneath.
  - **Skill Haste** adds Skill Haste points: every skill cooldown runs down half a
    percent faster per point, so +100 makes a cooldown take 2/3 of its time. The
    game counts at most 200 Skill Haste in total, where a cooldown takes half its
    time, so the slider stops at 200.
  - **All Skills** adds levels to every skill you have put a point in, like
    "+N to All Skills" on an item, in whole levels up to 100.
  - Don't run Stat Forge's boosts of the same names at the same time: they work
    on the same game functions.

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
- **The pet no longer chases something that is not loot.** After a zone change
  its target could end up being an old item's number reused by something else
  on the map, and the pet would run at that spot and grind there while you
  walked on. A target that is not an item or a coin is now dropped the moment
  it is seen, and the pet goes back to collecting. The switch itself is still
  off by default.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. Move all into the stash,
Extra packs as you approach, Skill Haste, All Skills and the relic fix are in the plugin. **Launch Modded Game** brings
the plugin up to date for you. If you start the game from Steam instead, or
ForgePact's warning asks for it,
press **Install Mod Plugin** once after updating.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
