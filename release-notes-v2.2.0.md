# ForgePact 2.2.0

Release date: 2026-10-09

A new **Mining Ore Extra Rolls** slider, off by default, lets one mining node
pay out up to ten times, including more chances at the rare finds a dig can
give. A new **Sleep loot your filter hides** switch, off by default, puts the
loot your filter hides to sleep so the game stops updating it every frame, and
shows it again while you hold a key. A new **Pet collects relics** switch,
also off by default, has your pet pick up the relics lying around you. And
when the game crashes, freezes or drops frames badly, ForgePact now saves a
report you can attach to a bug report, without a notification: you find it on
the panel's Setup tab.

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
- **Incident reports (#76).** When the game crashes or freezes, ForgePact
  saves a report folder under `bp_ipc\reports\` in the game's `bin` folder.
  A significant FPS drop (a single frame that takes over a quarter of a
  second, or play running two and a half times slower than usual for a
  couple of seconds) is saved as a report too. Every report is saved
  without a notification: nothing pops up while you play, and the reports
  are listed on the panel's Incident reports card. A freeze report names the
  ForgePact hook the game was inside when it stopped, and the mod as well
  when it is one of the mods that keep time for these reports, including
  when the mod had handed over to the game's own work; an FPS-drop report
  shows which mods were on and how much of each frame the mods that keep
  time for these reports took. Some mods keep no time (Sleep loot your
  filter hides, for one), so their time has no row of its own, and a mod
  missing from the table has not been cleared. Both reports say which
  room you were in. A mod's time counts
  only ForgePact's own code: when a mod's hook lets the game do its normal work (drawing the HUD,
  dropping an item, spawning a monster, and the extra drops or monsters a
  multiplier asks for), that game work is not added to the mod's time, and
  the setup ForgePact does once when the game starts shows as its own
  `setup` row in the per-mod table. Not
  every ForgePact hook can be
  named, so a report that says `none` does not clear ForgePact: it means
  only that none of the hooks it can name was running. A crash report cannot say what was running, because the crash
  is found after the game has closed. A zone or character load that stops
  the game for a few seconds is not reported as a freeze. Every report also
  holds your ForgePact settings, the list of installed mod files and your
  Windows, processor, graphics card and memory. Your Windows user name is
  replaced in every path. Nothing is uploaded: the report stays
  on your PC until you attach it to a bug report yourself. While the panel is
  open, a crash is noticed as soon as the game closes and shown on the card.
  A crash's report folder is saved the next time the game starts with
  ForgePact, and holds what Windows recorded about the crash when the panel
  was open to read it. If another mod file fails while the game is closing,
  after ForgePact has already shut down cleanly, ForgePact notes it in its
  log and on the card instead of reporting a crash. FPS drops
  are not reported in the first few seconds after a zone change, while the
  game window is in the background, and at most one FPS-drop report folder
  is saved every five minutes, ten a session. The
  new **Incident reports** card on the Setup tab lists the latest reports,
  shows how the game last closed and opens the reports folder. No report, of
  any kind, comes with a notification or a message box, and there is no
  setting for one. The last ten reports are kept. This is always on and
  changes nothing in the game.
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
- **Satanic Zone: choose the zone (#157).** Two new switches in
  World → Satanic Zone, both off by default. **Keep the zone you are in
  satanic** makes wherever you go count as the Satanic Zone, so its modifiers
  and its drops follow you; towns and sub-areas are left alone, and the plugin
  keeps the game's own value in step about four times a second because the
  game re-rolls it on its own. **Every zone counts as satanic** makes the
  game's own "is this a Satanic Zone?" answer yes wherever you are. The
  `satzone` command pins one exact zone instead (`satzone pin here`,
  `satzone pin <index>`, `satzone off`, `satzone stat`). With both switches
  off the game rolls its zone exactly as before. The zone the game keeps is a
  protected value: the plugin reads and writes it through the game's own
  `GPV`/`SPV`, never a fixed number. Measured on the research build
  (2026-10-03): the game asks `LoadSatanicZone` about 150 times a second with
  the resolved zone's room index, and that value can be written, sticking
  until the game's next roll. **Not yet confirmed in a live game**: forcing
  the answer true changed nothing visible in town, and a real zone where a
  kill can drop a relic has not been tested yet.

## How to update

Download and extract the complete release, then reopen ForgePact: the panel
gained a slider, two switches and an Incident reports card, so updating only
the plugin leaves them out. Your existing settings are retained. Source users
can run `Prepare-Plugin.bat` if plugin files are missing before using **Install
Mod Plugin**. The plugin changed too, so press **Install Mod Plugin** once
after updating - updating only the panel leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
