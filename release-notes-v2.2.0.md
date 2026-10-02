# ForgePact 2.2.0

Release date: 2026-10-09

A new **Mining Ore Extra Rolls** slider, off by default, lets one mining node
pay out up to ten times, including more chances at the rare finds a dig can
give. A new **Sleep loot your filter hides** switch, off by default, puts the
loot your filter hides to sleep so the game stops updating it every frame, and
shows it again while you hold a key. And a new **Pet collects relics** switch,
also off by default, has your pet pick up the relics lying around you.

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
- **Sleep loot your filter hides.** A new switch in Mods → Quality of Life,
  off by default. Items your loot filter hides are not gone: they lie on the
  ground unseen, and the game keeps updating every one of them every frame.
  With this on, an item your filter hides is put to sleep as soon as it
  drops, so the game stops updating it. Items your filter shows, and gold,
  are never touched. Hold **Left Alt** to see the hidden items
  and pick them up; let go and the rest are hidden and asleep again. Under
  **Show hidden loot while held** you can pick another key, the middle mouse
  button, mouse button 4 or 5, or none; the left and right mouse buttons
  can't be used. Turning the switch on also puts to sleep the hidden items
  already on the ground, and turning it off wakes them all. If you loosen
  your filter later, items already asleep stay hidden: hold the key, or turn
  the switch off. How much it saves comes from a research session on the
  research build, which put hidden items to sleep with the same call before
  this switch existed: with about 2,700 hidden items lying around one spot,
  the game took about 7.5 ms a frame with them asleep, against 14 to 18 ms
  with them awake. This version of the switch has been tested both outside
  the game and in a live game.
- **Pet collects relics (#124).** A new switch on the Mods tab, under Quality
  of Life, right after Pet collects quest items. Until now a relic on the
  ground waited for you to click it: the game's own pet never takes relics.
  With this on, while your pet is out it walks to the relics lying on screen
  and picks them up for you, one at a time, through the game's own pickup, so
  each one raises the relic you own by one level, the same as picking it up
  yourself. A relic you already have at 10/10 is left where it is, since
  picking it up cannot raise it any further: with a 10/10 relic and a lower
  one on the ground the pet takes the lower one, and with only 10/10 relics
  around it stays put instead of going back and forth. If the game turns a
  pickup down,
  the relic stays on the ground and the pet moves on to the next one. It is
  separate from Pet collects quest items; with both on, the pet fetches one
  thing at a time. It is off by default: turn it on in the panel. Checked in
  play on 2026-10-02: the pet picked up 31 relics, each one raising the owned
  relic by one level, left 10/10 relics alone and stayed put when only those
  were on screen. The relics in that check were placed by a test command, so
  a relic the game itself drops has not been watched being collected yet.

## How to update

Download and extract the complete release, then reopen ForgePact: the panel
gained a slider and two switches, so updating only the plugin leaves them out. Your
existing settings are retained. Source users can run `Prepare-Plugin.bat` if
plugin files are missing before using **Install Mod Plugin**. The plugin changed
too, so press **Install Mod Plugin** once after updating - updating only the
panel leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
