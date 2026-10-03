# ForgePact 2.2.0

Release date: 2026-10-09

A new **Mining Ore Extra Rolls** slider, off by default, lets one mining node
pay out up to ten times, including more chances at the rare finds a dig can
give. A new **Sleep loot your filter hides** switch, off by default, puts the
loot your filter hides to sleep so the game stops updating it every frame, and
shows it again while you hold a key. A new **Pet collects relics** switch,
also off by default, has your pet pick up the relics lying around you. A new
**Jump through scenery** switch, off by default, lets your jump carry you over
the rocks, fences and carts that stop it, when it would land on open ground.
And when the game crashes, freezes or drops frames badly, ForgePact now saves a
report you can attach to a bug report, without a notification: you find it on
the panel's Setup tab.

**Headhunter** and **Tyrant's Crown** now drop the way the game's own Angelic
items do: from the game's Angelic roll, as one more entry in its list, and only
while that item's switch is on. The **Angelic / Unholy Drops** slider no longer
drops them. With both switches off, the default, neither one ever drops, even
if you have forged it.

A new **Bosses** setting, off by default, on a new **Gameplay** page of the Mods
tab, makes every boss come as a Rare ("uber") or Ancient ("uber uber") boss.
Measured in a live game on one boss, a Karp King spawned from the research
console: set to Ancient, with the rarity and the affixes the mod added, it had
about five times its usual health, about twice its damage and 6.25 times its
experience.

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
- **Bosses: "uber" and "uber uber" bosses (#44).** The Mods tab has a third
  page, **Gameplay**, after Quality of Life and Items, with one setting:
  **Bosses**. Pick **Rare — "uber" boss** or **Ancient — "uber uber" boss** and
  the plugin asks the game, through the hook the Monster Rarity sliders already
  use, to set up every boss that spawns while it is on as a rare or an ancient
  one, and asks for the same extra affixes the sliders give a monster they raise
  (up to two on a rare, three on an ancient). It is off by default (**Normal
  (the game's own)**): choose a setting to use it. Bosses the game itself
  already made champion, rare or ancient are left alone, and so are the
  monsters and phases a boss creates during its fight; that is how the
  setting is built, and neither case has come up in a live game yet. Ordinary
  monsters are not affected, and the Monster Rarity sliders on the World tab
  still leave bosses alone. With Tyrant's Crown also on, a boss this setting
  raised to Rare can also get the crown's extra affix. If the plugin cannot
  set it up on your game, choosing Rare or Ancient is refused and bosses stay
  as the game makes them: the log shows `bossrarity: refused` with
  `hook=failed`. If `bossrarity status` shows `hook=table-only`, bosses the
  game creates through its compiled code's direct calls are not raised.
  What was measured in a live game, on 2026-10-02: on the research build, a
  Karp King set to Rare and to Ancient, and Damien, Uber Damien and Uber Anubis
  set to Ancient, each came out at the rarity chosen with its extra affixes. On
  an Ancient Karp King, spawned from the research console, the game then built
  a stronger boss from the rarity and the affixes the mod added: about 4.7 to
  5.7 times its usual health (two sessions), about 2.1 times its damage and
  6.25 times its experience, and its death rolled its loot at the ancient rank
  instead of the normal one. Those numbers come from that one boss. Not seen in those sessions: an ancient look (its
  name bar looked the same), more or better loot (one kill at each rank, too
  few to tell), or extra boss gems, runes or parts; and what Rare changes on a
  boss beyond its rarity was not measured. On the release plugin, the panel
  turned the setting on and off in a running game and the hook went in on
  both routes; no boss was fought on that build, so the raise itself was
  measured on the research build.
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
- **Jump through scenery (#16).** A new switch in Mods → Quality of Life,
  off by default. Until now a jump aimed across a rock, a fence, a cart or
  other scenery did not move you at all. With this on, the jump carries you
  over it, but only when it would land you on open ground inside the area;
  when the landing spot is blocked or outside the room's outer bounds, the
  jump stays blocked as it always was. Those bounds are wider than the part
  of a zone you can walk in, so a jump towards the edge of the walkable map
  is not held back by this check. Locked doors and zone gates are meant to
  keep blocking the jump. The mod learns how far your jump goes from a jump you make in the
  open, so after loading a character, make one jump on open ground first:
  until then, a jump into scenery stays blocked. Make one again after a big
  change to your Jump Power, because until then the mod checks the landing
  at your old jump distance. Some jumps the game itself refuses to start,
  such as one aimed to land inside a carriage, stay refused: the mod cannot
  change those. Only your universal jump is affected, not leap, dash or
  charge skills, and in co-op only your own character. Checked in play on
  2026-10-03: with the switch off a jump at a prop in the Town of Inoya did
  not move the character, and with it on the same jump carried them over
  it, never leaving them inside scenery. The checks on the landing spot, on
  the room's bounds and on locked doors and zone gates were not reached in
  that test (a jump aimed into a carriage did not move the character at all,
  and no edge of the area could be reached), so they have been tested
  outside the game only, and what the game does at the edge of the walkable
  map has not been observed.

## Changed

- **Headhunter and Tyrant's Crown drop only from the game's own Angelic roll.**
  - Before, they were two more items in the **Angelic / Unholy Drops** slider's
    pool: ForgePact's own die dropped them whatever their switches said, and
    the game's own Angelic roll never did.
  - Now the game decides. When a Blood Pact or dungeon "Angelic item drop
    chance" effect is active, the game's Angelic roll picks one unique from its
    own list and rolls that unique's drop rate. While a switch is on, its item
    is one more entry in that list for the length of each roll, and is taken
    out again as soon as the roll is over, so merchants, shrines and crafting
    never see it.
  - Each item stands in through a real Angelic unique of the same kind and is
    exactly as rare as it: Headhunter as rare as **Liquor Holster**, Tyrant's
    Crown as rare as the more common of **Lucifer's Crown** and **Mask of the
    Celestial**, the one with the lower drop-rate number (the log names which
    one when you turn the switch on; in our testing it was Mask of the Celestial). Those
    uniques keep their own chance to drop.
  - When the game's roll lands on the item, the game itself builds it and drops
    it where the monster died: one item per hit, in place of what that roll
    would otherwise have dropped.
  - Only while that item's switch (**Mods → Items → Headhunter** or
    **Tyrant's Crown**) is on. Forging the item in the Custom Forge still turns
    its mechanic on, as in earlier versions, but not this drop: a forged
    Headhunter or Tyrant's Crown with its switch off never drops from the
    game's Angelic roll.
  - Both switches are off by default, so a default install drops neither item,
    forged or not, and the game's roll and its list are left as they are.
  - ForgePact adds no chance of its own for these two and does not change the
    game's Angelic chance. If the plugin cannot find the game's list on your
    game, turning a switch on says so in the log and the roll stays the game's
    own.
  - Checked in a live session on 2026-10-02, with the game's Angelic chance
    raised so that hits came quickly and the item given many entries instead
    of one so that hits would fall to it: every hit that fell to the item
    became the item, 48 of 48 for Headhunter and 11 of 11 for Tyrant's Crown,
    each built by the game where the monster died, one per hit, and with both
    switches off every hit stayed the game's own. Not yet watched: a hit at the
    game's normal Angelic chance, which is about one in several thousand rolls.
    How often the item drops with the single entry a normal install adds is
    worked out from that session, not measured.
- **The Angelic / Unholy Drops slider's pool is the game's real Angelic and
  Unholy uniques again**, with no signature items in it.
- `sigdrop crown|belt|off|status` is still a test command that makes every kill
  drop the named item.

## How to update

Download and extract the complete release, then reopen ForgePact: the panel
gained a slider, three switches, an Incident reports card and a Gameplay page,
so updating only the plugin leaves them out. Your existing settings are
retained. Source users can run `Prepare-Plugin.bat` if plugin files are missing
before using **Install Mod Plugin**. The plugin changed too, so **Launch Modded
Game** brings it up to date for you. If you start the game from Steam instead,
or ForgePact's warning asks for it, press **Install Mod Plugin** once after
updating.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
