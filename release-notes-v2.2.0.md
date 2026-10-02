# ForgePact 2.2.0

Release date: 2026-10-09

A new **Mining Ore Extra Rolls** slider, off by default, lets one mining node
pay out up to ten times, including more chances at the rare finds a dig can
give. A new **Sleep loot your filter hides** switch, off by default, puts the
loot your filter hides to sleep so the game stops updating it every frame, and
shows it again while you hold a key. And when the game crashes, freezes or
drops frames badly, ForgePact now tells you and saves a report you can attach
to a bug report.

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
- **Incident reports (#76).** When the game crashes, freezes, or has a
  significant FPS drop (a single frame that takes over a quarter of a second,
  or play running two and a half times slower than usual for a couple of
  seconds), ForgePact notices and saves a report folder under
  `bp_ipc\reports\` in the game's `bin` folder. Each report says whether
  ForgePact's own code was running when it happened, which of its mods were
  on or busy, how much of each frame ForgePact's mods took, and which room you
  were in, together with your ForgePact settings, the list of installed mod
  files and your Windows, processor, graphics card and memory. Your Windows
  user name is replaced in every path. Nothing is uploaded: the report stays
  on your PC until you attach it to a bug report yourself. While the panel is
  open, a Windows notification tells you a report was saved, and a crash is
  noticed as soon as the game closes. A crash's report folder is saved the
  next time the game starts with ForgePact, and holds what Windows recorded
  about the crash when the panel was open to read it. Without the panel, a
  freeze or a crash shows a message box instead, the crash one the next time
  the game starts. FPS drops
  are not reported in the first few seconds after a zone change, while the
  game window is in the background, or more than once every 30 seconds. The
  new **Incident reports** card on the Setup tab lists the latest reports,
  shows how the game last closed, opens the reports folder, and has a switch,
  **Tell me about FPS drops**, that turns off the FPS-drop notifications
  only: reports are always saved, and crashes and freezes are always shown.
  The last ten reports are kept. This is always on and changes nothing in
  the game.

## How to update

Download and extract the complete release, then reopen ForgePact: the panel
gained a slider, a switch and an Incident reports card, so updating only the
plugin leaves them out. Your existing settings are retained. Source users can
run `Prepare-Plugin.bat` if plugin files are missing before using **Install Mod
Plugin**. The plugin changed too, so press **Install Mod Plugin** once after
updating - updating only the panel leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
