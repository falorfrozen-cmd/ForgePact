# ForgePact 2.2.0

Release date: 2026-10-09

A new **Mining Ore Extra Rolls** slider, off by default, lets one mining node
pay out up to ten times, including more chances at the rare finds a dig can
give.

**Headhunter** and **Tyrant's Crown** now drop the way the game's own Angelic
items do: from the game's Angelic roll, and only while that item's switch is
on. The **Angelic / Unholy Drops** slider no longer drops them. With both
switches off, the default, neither one ever drops, even if you have forged it.

## New

- **Mining Ore Extra Rolls (#36).** A new slider on the Loot tab, right under
  Mining Ore Multiplier, from 1 to 10. At 3, every node you finish mining pays
  out three times: three sets of its ore, and three chances at the bonus finds
  a dig can roll on top of the ore. Those bonus finds still depend on your own
  character's stats, so a character that never gets them from a normal dig
  will not get them this way either; the extra rolls then only give more ore.
  Character and guild experience still count once per node; a test dig showed
  that. Mining experience, quest progress and the floating experience text may
  not: the plugin tries to hold them back on the extra rolls, but in a test dig
  the game never handed them out at the point where the plugin holds them back.
  Until a dig shows otherwise, they may come once per roll: ten times at 10
  rolls (the 2026-10-02 check could not tell, because its character's mining
  level was already at the cap). It works together with
  Mining Ore Multiplier: at 3 rolls with the multiplier at 5, you get three sets
  of ore, each five times as large. A worn Miner's Helmet is built to give each
  set its 4x in place of the multiplier; that combination has not been measured
  in play. It is off by default (1 is the game's normal
  dig): move the slider to use it. If the plugin cannot set it up on your game,
  the slider stays at 1 and says so in the log, and Mining Ore Multiplier keeps
  working. Checked in play on 2026-10-02 with this release's plugin, through
  the panel: at 3 rolls a Copper Vein dropped three stacks of ore (14 ore in
  all), and with the slider back at 1 the next Copper Vein dropped one stack
  (3 ore). How much ore is in each stack still varies from dig to dig.

## Changed

- **Headhunter and Tyrant's Crown drop only from the game's own Angelic roll.**
  - Before, they were two more items in the **Angelic / Unholy Drops** slider's
    pool: ForgePact's own die dropped them whatever their switches said, and
    the game's own Angelic roll never did.
  - Now the game decides. When a Blood Pact or dungeon "Angelic item drop
    chance" effect is active and the game's Angelic roll hits, the game drops
    its own Angelic or Unholy item as usual, and ForgePact rolls the signature
    items' share of that hit: the share of one item in the pool, about 1 hit in
    48 with one switch on and 2 in 49 with both. On a success, the item lands
    beside the game's own item, where the monster died. With both switches on,
    each success is one or the other at even odds.
  - This was verified in a live game session, using a test build that made the
    game's Angelic roll hit far more often than it normally does, with the
    switches set from the console: with a switch on, its item dropped beside
    the game's own Angelic items; with both off, neither ever did.
  - Only while that item's switch (**Mods → Items → Headhunter** or
    **Tyrant's Crown**) is on. Forging the item in the Custom Forge still turns
    its mechanic on, as in earlier versions, but not this drop: a forged
    Headhunter or Tyrant's Crown with its switch off never drops from the
    game's Angelic roll.
  - Both switches are off by default, so a default install drops neither item,
    forged or not, and the game's roll is left as it is.
  - ForgePact adds no chance of its own for these two and does not change the
    game's Angelic chance.
- **The Angelic / Unholy Drops slider's pool is the game's real Angelic and
  Unholy uniques again**, with no signature items in it.
- `sigdrop crown|belt|off|status` is still a test command that makes every kill
  drop the named item. `sigdrop status` now also counts the game's Angelic
  rolls and hits, and the signature items they dropped.

## How to update

Download and extract the complete release, then reopen ForgePact: the panel
gained a slider, so updating only the plugin leaves it out. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. The plugin changed too, so
**Launch Modded Game** brings it up to date for you. If you start the game from
Steam instead, or ForgePact's warning asks for it, press **Install Mod Plugin**
once after updating.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
