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

  **Checked so far (2026-09-19, one machine, one game build):** with the new
  loader by itself, the game starts, and a roughly two-minute Chaos Tower
  session that raised 133 of the game's own caught errors ran with no lag
  seen, where the previous loader lagged heavily in the same content. With
  ForgePact 1.4.4's plugin and the HS Offline Tracker producer also loaded on
  the new loader, the game starts, the plugin loads and installs its hooks,
  and a handful of panel commands — a ping, and the globe/orb pickup option
  switched on, read back, and switched off again — answered normally.

  **Not yet checked:** playing with mods switched on for any real length of
  time on the new loader; whether the lag improvement holds once the plugin
  is loaded (the lag observation above was made with the loader alone); and
  other machines or game builds. This file will be corrected if any of that
  turns up a problem.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This change is in the mod
loader, so pressing **Install Mod Plugin** matters — updating only the panel
leaves the old `YYToolkit.dll` in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
