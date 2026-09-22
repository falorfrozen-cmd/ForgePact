# ForgePact 1.4.5

The Prospect Cube can now prospect each item the moment you put it in, so you
no longer have to stop every 54 items to press Prospect and start again.

Three new, off-by-default Gameplay Mods for toggle skills: mark the skill-bar
slot of a toggle you have running, stop a double cast proc from flipping a
toggle straight back, and draw a countdown over a plain (non-toggled) cast so
you can see how much time is left before it ends.

A fixed set of toggle skills, each measured in-game, is covered by the toggle
marker and the double-cast guard. The countdown below covers a different set
of its own - see that bullet for what it draws on.

## New

- **"Reveal full map" now marks every monster pack instead of spawning it.**
  Before: the map's monster sub-toggle created every pack of the zone as you
  arrived, so that distant monsters would show on the revealed map. Filling a
  zone that way put a whole zone's monsters into the game at once, and at high
  density the game stayed slow for the whole zone even after the spawning had
  finished, because the game keeps working on every living monster every
  frame. Now the sub-toggle (**Show every monster pack on the map**) draws one
  marker per pack the moment you arrive, by pack kind (normal, champion,
  ancient, legion, mini boss), including density copies, without creating a
  single monster. Each pack is still born by the game when you walk near it,
  exactly as without the mod, and its real minimap dots replace the marker at
  that moment. What a marker cannot show is the rarity roll of the monsters
  inside the pack, which the game only decides when the pack is born. Each pack
  kind has its own icon: a white dot for a normal pack, a white ring for an
  ambush, a magenta diamond for ancient, a cyan star for champion, a gold chest
  for a colossal chest, an orange triangle for a legion and a red bullseye for a
  mini boss. Packs that sit close together, such as the copies Monster Density
  adds, show as one icon with a small count. The icons are plain PNG files in
  `bin\bp_ipc\packmarks\` next to your game; replace any of them with your own
  picture of the same name and it is used instead (`packmarks reload` in a
  running game, or the next launch). The `packmarks` plugin command changes
  size, grouping distance, the count badge and, for the fallback dots, colour
  and outline, live. Confirmed in the map screen on 2026-09-22; the first build's icon-based
  markers drew nothing visible, which is why the dots replaced them.
  The old behaviour is still available as a second, off-by-default
  sub-toggle, **Really spawn every pack on arrival (heavy)**, for comparison; it
  still lags at high density.
- **Tyrant's Crown and Beacon no longer scan every monster for nothing.** While
  either was worn, ForgePact walked every living monster every sixth frame to
  wake sleeping ones, but the game never puts monsters to sleep, so the walk
  only cost frame time. It now checks whether waking changed anything before
  walking, and walks only if it did.
- **Local early-population candidate (failed performance testing; not release-ready).**
  Filling dense maps could exhaust the game's fixed monster-stat storage and
  crash during enemy creation. On the supported game library, ForgePact now
  reserves additional storage and starts groups in short batches, preserving density.
  The local follow-up spreads additional density copies across frames as well
  as pack births and prioritizes nearby copies. The latest local adjustment
  targets five seconds instead of slowing almost to a stop during ordinary
  frame-time fluctuations. The panel reports when that target is exceeded;
  this target is not evidence that every group spawned.
  The latest 4x-density test missed that target and caused severe lag even after
  the queue counters emptied. No crash was reported in that run; the performance
  issue remains unresolved. It cannot interrupt a single expensive game call.
  A subsequent capture found silent groups expiring from those counters without
  admission. They now remain separately reported as unconfirmed, rather than
  implying that the map finished populating. Unsupported storage or insufficient
  reserve is reported beside the option. Real monsters still cost frame time;
  this is not a promise of unchanged FPS. Standalone native and queue tests pass;
  they do not override the failed live performance check.
  A local follow-up also fixes abandoning maps whose creators load after the
  minimap, and avoids redundant Tyrant/Beacon scans of already-active monsters.
  Automated behavior checks pass; this follow-up still needs live verification.
  Further local refinements reduce repeated monster-ID lookups, temporary memory
  allocations and repeated waiting-list scans. Enemy density, hunting behavior
  and enabled features are preserved. The cause of silent groups and the actual
  FPS gain still require in-game verification.
  The next local candidate reduces repeated searches when selecting nearby
  density copies and avoids rewriting idle population status unnecessarily.
  A waiting group that starts spawning naturally can now be recognized, instead
  of remaining marked as unconfirmed. This does not guarantee every group member
  has spawned; live completion and performance testing remain outstanding.
  The latest local refinement removes a repeated creator lookup during enemy
  births. It preserves density and existing features; reduced stutter has not
  yet been established in-game.

- **Local Miner's Helmet prototype (not release-ready).** The helmet grants
  exactly four times the ore while equipped; removing it restores normal ore
  rewards. The gold mining pulse is cosmetic. The local follow-up clears stale
  equipped status when returning to menus, rejects malformed equipment before
  the game's item lookup, and checks mining ownership before reading gear.
  An in-game crash investigation remains open; these checks are not a crash fix.

- **Mining Ore Amount (experimental, not yet verified in-game).** A 1–10× slider
  under Loot changes the quantity in a mining ore reward. x1 keeps normal mining.
  The new adapter, panel persistence and native Release build have been checked;
  the real game's mining and pickup behavior still needs verification. This is
  independent of Gold/drop-rate controls and does not install hooks at its default.

- **Auto-prospect items put in the Prospect Cube.** The cube's 9×6 prospect
  grid fills long before a full inventory is through it: you fill it, press
  Prospect, and fill it again, over and over. With the new **Auto-prospect**
  switch in Gameplay Mods, every item you drag or click into the grid is
  prospected straight away by the game's own Prospect, exactly as if you had
  pressed the button, so the grid never fills with items waiting their turn.
  It is **off by default**.
  - **The previous batch goes to your materials tab.** Each prospect leaves
    its materials in the grid, one stack per material type, as a normal
    Prospect does. When you put the next item in, ForgePact first moves the
    materials that the previous prospect made to your materials tab - the
    same move the game makes when you click a material - and then prospects,
    so the newest batch stays in the grid where you can see it and the grid
    does not fill up with materials. A material that is the first of its kind
    - one with no stack of it yet in your materials tab - is moved too, the
    way a click on it moves it: the game puts it in your bag rather than the
    materials tab. Only that batch moves. The item you put
    in is prospected and not moved, even when it is itself a material such
    as ore, and materials you put in the grid yourself stay where they are -
    with one exception nobody has run into yet: if you swap an item onto one
    of those batch materials and drop that material straight back in, it
    still counts as part of the batch and goes to your tab instead of being
    prospected. Prospecting an ore does not always give materials: the
    game's own Prospect only has a chance to, with or without ForgePact, so
    an ore that is used up with nothing in its place is the game, not the
    mod. After you reopen the cube, turn Auto-prospect off and on,
    or take something out of the grid, the next prospect moves nothing, and
    moving starts again with the batch that prospect makes. This is its own
    switch under Auto-prospect, **Move the previous materials to your
    materials tab**, and it is **on by default** whenever Auto-prospect is
    on; turn it off to keep every material in the grid as before. If the
    game will not take a material, it stays in the grid and
    `bp_ipc\out.txt` says so once (a full bag or materials tab was not
    tested); if ForgePact could not make the move at all, that line also
    says which step failed, so a bug report can say where it stopped. ForgePact only clears a
    material from the grid after the game confirms it reached your materials
    tab or your bag; if one ever leaves the grid without that confirmation, the switch
    turns itself off for the rest of the session and `out.txt` says so. When fewer than 6 cells are free, auto-prospect holds
    back and leaves your item in the grid (the first time in a session, it
    writes a line to `bp_ipc\out.txt` saying so); empty some of the grid and
    carry on.
  - **Anything still in the prospect grid when the game saves is lost.** This
    is how the game itself treats that grid, with or without ForgePact: items
    left in it across a save are gone when you load again. With
    auto-prospect on, the newest batch of materials sits in the grid, so
    empty it before you leave the cube, and especially before you quit.
  - ForgePact runs the Prospect, and moves the materials, for you at a moment
    the game did not choose. The Prospect and the move to the materials tab
    were tested with junk items; the move of a first-of-its-kind material to
    your bag has not been seen in a real session yet. Keep a
    backup of your save before you try either with anything you care about.
  - If ForgePact cannot attach to the game's insert step on your copy of the
    game, the switch turns itself off for that session and `bp_ipc\out.txt`
    says why, instead of pretending to work. `out.txt` also records the first
    item it prospects and the first materials it moves each session, and says
    so once if a Prospect it ran left the grid unchanged, or could not check
    the grid afterwards.
- **Mark a running toggle skill on the skill bar.** A toggle you have switched
  on has no on-screen sign that it is still running - you had to remember
  whether you pressed it, or watch your health bar or the ground for the
  effect. With this mod turned on (Mods tab, off by default), a soft red
  outline appears around that skill's slot on the skill bar the whole time it
  is running, and disappears the moment it stops - on a re-press, on a zone
  change, or if the game cancels it. It covers a fixed set of toggle skills,
  each measured in-game. A plain cast - one made without the sub-talent that
  turns the skill into a toggle - does not light the outline, and neither
  does a skill outside that set.
- **Stop double cast re-casting a toggle skill.** With a double cast effect
  equipped, a double cast proc could cast one of that same fixed set of
  toggle skills a second time on its own, a moment after your press - which
  flipped the toggle straight back,
  so it ended off when you had just turned it on, or on when you had just
  turned it off. With this mod turned on (Mods tab, off by default), that
  extra cast is skipped and the toggle stays the way your press left it. It
  only steps in when you actually have the sub-talent that makes the skill a
  toggle, or the skill is a toggle on its own; otherwise the double cast's
  extra cast goes through exactly as it does in the unmodded game. Your own
  presses, and double casts of every other skill, are not affected.
- **Timed skill countdown.** A new **Timed skill countdown** dropdown in Mods
  (off by default) draws how much of a timed cast is left over that skill's
  slot on the skill bar, and it disappears the moment the cast ends. A small
  set of timed skills were measured in-game and are tested; most other
  skills with both a duration and a real cooldown are also covered, by rule,
  and are untested. A few skills are left out where a measurement showed the
  timer on the skill's own object is not the skill's duration, and companion
  skills (turrets, totems, hydra) are not covered. A few skills whose
  duration is a buff on you, measured in-game, are covered too, and other
  buff-only skills are not. A skill switched on as a toggle never gets a
  countdown. In a fight, hits can add a little time to some skills (roughly
  0.2 s each in our test) and the countdown rises slightly to match. Pick
  one of four looks: **Arc** (a ring that empties clockwise),
  **Bar** (a shrinking bar above the icon), **Number** (a shrinking
  percentage above the icon) or **Fade** (a soft outline that dims as time
  runs out). If you turn this on partway through a cast that is already
  running, that one cast shows as full from the moment you turned it on
  rather than its true remaining time; the next cast on that skill reads
  correctly.
- **Mods tab columns.** The Gameplay and Items mod lists are split into two
  columns, and the right column could end up longer than the left, leaving a
  gap under the left one. The left column is now always the longer one, and
  the cards still read top to bottom, left column first. On a narrow window
  the lists are a single column, as before.
- **A new read-only `menulayout` command lists where the menu buttons
  are.** A tool that plays through the main menu for you - such as the
  toolkit's `hs-drive` helper, which opens a save for testing - used to
  click fixed spots on the screen, and a game patch or a different screen
  layout could silently move a button out from under that spot. ForgePact
  now answers `menulayout` with the live instances of a set of menu objects
  it looks for, and the interface pieces under them, each with its position
  on the game window. `hs-drive` now clicks the positions it lists for
  `Play local`, the save slot and `PLAY`, and stops with a reason instead of
  guessing when a button is not listed. In a test run, clicks at the listed
  `Play local`, first- and second-save-slot and `PLAY` positions all worked
  and loaded the chosen character. Nothing in play changes, and there is no
  new switch in the panel.

## Changed

- **Headhunter and Tyrant's Crown now drop like an Angelic / Unholy item.** They used to drop
  on their own, in every session, about once every 15,000 kills each - far more often than any
  Angelic / Unholy item such as **Liquor Holster** - with no panel control for it. They
  are now two more items in the Angelic / Unholy Drops pool, so they drop exactly as often as
  Liquor Holster does from that slider: never while it is off (the default), more often as it is raised,
  on the very same roll as every other item in the pool. With two more items sharing each hit,
  every other Angelic / Unholy item is very slightly rarer. The game's own Angelic drops (from a
  Blood Pact or dungeon "Angelic item drop chance" modifier) still do not include either item -
  only ForgePact's own die does.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This release changes both the
plugin and the panel, so pressing **Install Mod Plugin** matters - updating only
the panel leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.

### Local experiment — not yet release-verified

- Miner's Helmet prototype: high defense, 4x mining ore while equipped, and a golden pulse. A test-item button and an Item Editor signature template are available in the experiment checkout. Equipment detection, the final tooltip and the pulse still require live confirmation. Nearby-vein harvesting remains disabled.
- The Mining Ore Amount slider works again on a character that has loaded the helmet: a worn helmet replaces the slider with x4, and with the helmet off the slider applies. Before, arming the helmet mechanic forced x1 whenever the helmet was not worn.
- Vein Resonance (helmet worn): a finished dig also finishes the two nearest eligible veins within 192 px by queuing them through the game's own dig, so they drop ore at 4x with normal mining XP; depleted, busy, over-level or out-of-reach veins are skipped, a finished vein never chains, and `minerhelm veins 0|1` switches it. Verified live on 2026-09-23: two veins 154 and 186 px from the dug node completed with 4x ore.
- The helmet's ore bonus no longer depends on the node naming its miner: on the installed build an ordinary dig leaves the node's miner field at `noone`, which the old check refused. The reward now also accepts a dig whose node has no named miner when the local player stands beside it; a node that names another player is still refused. Refusal reasons include the distance and the raw miner value.
- The experimental helmet no longer requires an extra pointer conversion to recognize the miner. Failed bonus checks now record a reason instead of remaining silent. Automated reward/pulse checks pass; the reported in-game no-bonus issue is not yet confirmed resolved.


## AFK FARM compatibility

AFK FARM can now use its own reward settings while ForgePact is loaded. Its MF,
XP, Gold and loot bonuses apply once during delivery. Your normal ForgePact
settings still apply to active play and are not overwritten.

Pack markers now use distinct fantasy icons: skull, hood, horned mask, helmet,
chest, skull group and crowned skull. Each kind has its own shape and colour,
with dark outlines to help it stand out on the minimap. Existing custom PNGs
are still preserved.
