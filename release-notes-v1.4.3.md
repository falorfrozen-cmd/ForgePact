# ForgePact 1.4.3

Bosses are left alone by the Monster Rarity sliders, the log file no longer grows without limit, and ForgePact's own kill drops are placed more carefully.

## Fixed

- **The Monster Rarity sliders (Rare / Ancient) could raise a boss the same
  way they raise an ordinary monster.** A boss already builds its own health
  and affixes when it spawns; the sliders had no check for that and could
  raise it a second time on top of it. A player reported an Anubis boss
  going from ~500k to ~4.5M HP with 20% Rare and 20% Ancient set - we have
  not reproduced that number ourselves, but the sliders clearly could reach
  a boss when they should not have. Bosses are now left alone by both
  sliders; ordinary monsters raise exactly as before.
- **`bp_ipc\out.txt` no longer grows without limit.** The plugin only ever
  appended to it, and nothing trimmed it - one player's copy reached 7.8 MB.
  Now, once it passes 2 MB, the plugin rotates it to `out.prev.txt` the next
  time the game starts (never mid-session), so the total stays around 4 MB and
  the previous session's log is never lost - the one you'd actually want after
  a crash, since you relaunch before you can report it. If a bug report needs
  a log, please attach both `out.txt` and `out.prev.txt`.
- **The panel now notices a restart across that rotation.** ForgePact's panel
  detects a game restart by counting startup lines in `out.txt`; a rotated log
  starts a fresh count that could coincidentally match the old one, which
  could make the panel miss a quick close-and-relaunch and skip reapplying
  your saved settings. The panel now also checks that it is reading the same
  log file, not just the same count.

## Changed

- **ForgePact's own kill drops are now placed before the game finishes handling the kill.**
  When a monster dies, ForgePact rolls its own Angelic / Unholy drop and its
  Tyrant's Crown / Headhunter drop and places the item where the monster
  fell. Until now it did this after the game's own kill handling had run, and
  still used the monster to place the item. With 1.4.3 both drops are rolled
  and placed while the monster is still there, and ForgePact no longer
  touches the monster after that. Drop rates and what can drop are unchanged.

  Players reported crashes in fights with the Angelic / Unholy slider at x100
  on 1.4.1. We could not reproduce that crash: in testing, 1.4.1 and 1.4.3
  each dropped 30 angelic items from 30 kills without crashing. This change
  removes a possible risk, it is not a confirmed fix for that report. If you
  still get crashes, please report them with the last lines of
  `bp_ipc\out.txt` and which ForgePact settings you had on.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This release changes both the
plugin and the panel, so pressing **Install Mod Plugin** matters - updating only
the panel leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
