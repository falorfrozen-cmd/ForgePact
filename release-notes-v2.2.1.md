# ForgePact 2.2.1

A quick fix on 2.2.0: Goburin's Head pity goes up to 200, and Jump through
scenery is reworked so that it can do what it says.

## Changed

- **Goburin's Head pity now goes up to 200 (#207).** The slider used to stop at
  20 machine explosions; it now runs from 1 to 200, still 10 by default, and
  `gambapity <n>` accepts the same range. A number you saved earlier stays as
  it was. The count is still kept between game sessions, in
  `forgepact_gamba_pity.json`, so quitting the game does not start it over.

## Fixed

- **Jump through scenery (#206).** With the switch on before the game started,
  jumps were still stopped by rocks, fences, trees and carts, in town and
  outside it. ForgePact itself refused those jumps, and that is what changed:
  - ForgePact checked for a landing spot at the length of your last jump, but
    a jump goes to where your mouse cursor is. So a jump over scenery was
    almost always checked at the wrong spot, inside the scenery, and refused.
    ForgePact now checks the spot your cursor is on, or, when the cursor is
    farther away than it has seen your jump reach, the farthest spot your jump
    can reach. Aim at open ground past the rock or tree and the jump
    carries you over it; aim into the scenery itself and it stays blocked, as
    in the game.

  Also, with the switch on before the game starts, ForgePact now waits for
  your character to be loaded before it hooks into the game, instead of doing
  it at the character screen; the first jump after loading a character no
  longer has to be a practice jump in the open, and walking into another zone
  no longer makes ForgePact forget what it learned about your jump.

  **Not yet confirmed in a live game.** These changes pass ForgePact's own
  tests, but the version of the mod in this release has not yet been tried in
  a game session. Locked doors and zone gates still block the jump, and a jump
  the game itself refuses (one aimed to land inside a carriage or a building,
  for example) stays refused.

## How to update

Download and extract the complete release, then reopen ForgePact and press
**Install Mod Plugin** once. Both parts changed: the plugin (the jump and the
pity range) and the panel (the Goburin's Head pity slider now reaches 200), so
updating only one of them leaves the other behind. Your existing settings are
retained. Start the game after installing; a game that was already open keeps
the old plugin until it is restarted.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
