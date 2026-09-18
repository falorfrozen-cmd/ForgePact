# ForgePact 1.4.3

Bosses stay normal with the Monster Rarity sliders up, ForgePact's log file no
longer grows without limit, and ForgePact's own kill drops are placed more
safely.

## Fixed

- **Monster Rarity could turn a boss Rare or Ancient.** The Rare and Ancient
  sliders are meant for ordinary monsters, but they had no check for bosses,
  so a boss could be raised like any other monster, with that tier's extra
  health and affixes on top of its own. Bosses are now always left alone;
  ordinary monsters are raised exactly as before. (A player reported an
  Anubis at about 4.5M HP instead of about 500k with 20% Rare and 20% Ancient
  set. We have not reproduced that number, but a boss could clearly be
  raised when it should not have been.)
- **`bp_ipc\out.txt` grew forever.** ForgePact only ever added to its log, so
  it could reach several megabytes. Now, when you start the game and the log
  is over 2 MB, the old one is kept as `out.prev.txt` and a fresh `out.txt`
  begins. The log never grows during a session, and the previous session is
  always kept, which is the one you need after a crash.
- **The panel could miss a quick restart.** With auto-apply on, the panel
  re-sends your saved settings whenever the game starts. It noticed a restart
  by counting startup lines in `out.txt`, which a fresh log could fool if you
  closed and reopened the game within a few seconds. It now also checks
  whether the log file itself changed.

## Changed

- **ForgePact's own kill drops are placed before the game finishes the kill.**
  The Angelic / Unholy drop and the Tyrant's Crown / Headhunter drop used to be
  placed after the game had already handled the monster's death, still using
  that monster. They are now rolled and placed while the monster is still
  there. Drop rates and what can drop are unchanged. This removes a possible
  source of crashes; it is not a confirmed fix. Players reported crashes with
  the Angelic / Unholy slider at x100 on 1.4.1, but we could not reproduce
  them: 1.4.1 and 1.4.3 each gave 30 drops from 30 kills without crashing.

## Reporting a problem

Please attach both `bp_ipc\out.txt` and `bp_ipc\out.prev.txt`, and say which
ForgePact settings you had on.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This release changes both the
plugin and the panel, so pressing **Install Mod Plugin** matters - updating only
the panel leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
