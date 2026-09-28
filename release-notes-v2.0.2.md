# ForgePact 2.0.2

Emptying your backpack into the stash used to take one Ctrl + click per item.
A new **Move all into the stash** switch, off by default, does it with one key.

## New

- **Move all into the stash** (Mods → Quality of Life, off by default). With the
  stash open, press **F4** and every item on the backpack tab you are looking at
  moves into the stash tab you are looking at, one item at a time, by the game's
  own move for that item - the same one a Ctrl + click makes - so each item
  lands where a hand move would have put it.
  - **When the stash tab fills up**, the items that fit move and the rest stay
    in your backpack. They never spill onto another stash tab or page, and
    nothing already in the stash moves.
  - Works on any Personal or Shared stash page, and on the **Materials** tab
    from your backpack or its Materials view: a material joins the stack of its
    kind there whole, or takes a free cell if the tab has none of that kind yet.
    An item the tab does not take stays in your backpack.
  - **Not supported yet:** the **Socketable** tab and the backpack's Socket
    view, the Unique tab, and the backpack's Key, Tarot and Relic views. F4 moves
    nothing there.
  - If a move cannot be confirmed afterwards, the run stops, the item is taken
    back out of the stash tab where possible, and the switch turns itself off
    until you restart the game; the panel shows `off (this session)` beside it.
  - F4 does nothing unless the switch is on, the game is the window in front and
    the stash is open, and nothing with Alt, Ctrl or Shift held, so Alt + F4 still
    only closes the game. The stash is saved when you close it, as usual.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. Move all into the stash is in
the plugin, so press **Install Mod Plugin** once after updating - updating only
the panel leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
