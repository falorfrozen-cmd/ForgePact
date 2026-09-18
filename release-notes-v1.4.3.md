# ForgePact 1.4.3

ForgePact's log file no longer grows without limit.

## Fixed

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

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This change is in the plugin,
so pressing **Install Mod Plugin** matters - updating only the panel leaves the
old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
