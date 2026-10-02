# ForgePact 2.2.0

Release date: 2026-10-09

A new **Mining Ore Extra Rolls** slider, off by default, lets one mining node
pay out up to ten times, including more chances at the rare finds a dig can
give. And a new **Pet collects relics** switch, also off by default, has your
pet pick up the relics lying around you.

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
- **Pet collects relics (#124).** A new switch on the Mods tab, under Quality
  of Life, right after Pet collects quest items. Until now a relic on the
  ground waited for you to click it: the game's own pet never takes relics.
  With this on, while your pet is out it walks to the relics lying on screen
  and picks them up for you, one at a time, through the game's own pickup, so
  each one raises the relic you own by one level, the same as picking it up
  yourself. A relic you already have at 10/10 is left where it is, since the
  game will not let it be picked up: with a 10/10 relic and a lower one on the
  ground the pet takes the lower one, and with only 10/10 relics around it
  stays put instead of going back and forth. If the game turns a pickup down,
  the relic stays on the ground and the pet moves on to the next one. It is
  separate from Pet collects quest items; with both on, the pet fetches one
  thing at a time. It is off by default: turn it on in the panel. It has not
  been checked in play yet; the in-game check, when it runs, is recorded in
  ForgePact's `docs/pet-relic-collector-research.md`.

## How to update

Download and extract the complete release, then reopen ForgePact: the panel
gained a slider and a switch, so updating only the plugin leaves them out. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. The plugin changed too, so
press **Install Mod Plugin** once after updating - updating only the panel
leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
