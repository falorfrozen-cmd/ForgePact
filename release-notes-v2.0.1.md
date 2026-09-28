# ForgePact 2.0.1

Four fixes from the bug reports, a new switch that keeps your pet from
getting stuck on loot it cannot pick up, and a Pet Collects Quest Items that
copes better with several quest items at once. The new switch is off until you
turn it on.

## New

- **Pet moves on from loot it cannot pick up (#94).** With a lot of loot on
  the ground, the pet could stay on one item, hopping around it without
  picking it up or going on to the rest. A new switch on the Mods tab, under
  Quality of Life, makes the pet give up an item it has stayed on for about a
  second and a half and go for the others; an item on the ground is then left
  alone for about ten seconds before the pet tries it again. Gold is different:
  the switch can only turn the pet away from a coin, not hold it back, so the
  pet may go for a coin it gave up again sooner. The switch picks nothing up
  itself and does not change which items the pet collects. It is off by
  default: turn it on to use it. Not yet confirmed in a live game.

## Changed

- **Pet Collects Quest Items works through several quest items.** The pet
  now takes the quest items on screen one at a time, and when it could not
  collect one, it goes for another and comes back to that one about ten
  seconds later. With a lot of quest objects around, it now also looks at
  all of them, not only the first few. Not yet confirmed in a live game.

## Fixed

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

Download and extract the complete release, then reopen ForgePact: the panel
gained a switch, so updating only the plugin leaves it out. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. The plugin changed too, so
press **Install Mod Plugin** once after updating - updating only the panel
leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
