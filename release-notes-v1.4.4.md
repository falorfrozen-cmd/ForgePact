# ForgePact 1.4.4

The mod loader (`YYToolkit.dll`) that ForgePact installs is rebuilt from source
this project can fully account for, instead of a binary whose original source
was lost.

## Fixed

- **The mod loader is rebuilt from source this project can fully account
  for.** Every ForgePact release from 1.3.1 through 1.4.3 shipped
  a `YYToolkit.dll` built by a previous maintainer whose original source was
  never kept — the notice that shipped with it described two changes, but the
  binary itself contained more than that, and a fresh build of the two files
  that *were* kept crashed during startup and never got the game running.
  1.4.4 instead ships a `YYToolkit.dll` built from a documented set of changes
  to the published YYToolkit project that anyone can read and rebuild
  themselves; a `YYToolkit-BUILD-INFO.json` file installed beside the DLL
  records exactly what went into the copy you have.

  **What this has been checked against so far:** launched once, by itself with
  no ForgePact plugin loaded, for about two minutes of play in Chaos Tower — it
  started normally, handled 133 of the game's own caught errors without the
  heavy lag an earlier report described for the previous file, and the player
  reported no lag at all. **It has not yet been launched with the ForgePact
  plugin loaded**, which is the next check before this build is considered
  safe to release, and this file will be corrected if that check finds a
  problem.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This change is in the mod
loader, so pressing **Install Mod Plugin** matters — updating only the panel
leaves the old `YYToolkit.dll` in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
