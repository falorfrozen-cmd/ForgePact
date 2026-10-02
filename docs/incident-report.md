# Incident reports - crash, freeze and FPS-drop detection (issue #76)

Status (2026-10-02): **built for 2.2.0 and verified against a stand-in frame
thread** (`tests/incident_monitor_harness.cpp`, `tests/incident_shutdown_probe.cpp`)
and by contract tests; **not yet run in the game** (see "Live results",
pending Live 1). The player-facing description is the README's
[Incident reports](../README.md#incident-reports-crash-freeze-and-fps-drop-reports)
section.

## Why

A player's "the game crashed" or "it got laggy" arrives with no evidence, and
the first question, whether ForgePact had anything to do with it, could only be
answered by asking for `out.txt` and guessing. The player build now notices a
crash, a freeze or a significant FPS drop on its own, says so, and writes a
folder the player can attach as it is. Each folder answers three questions:
was it our code, which mod was on or busy, and how much frame time our hooks
were taking.

## What it does

- **On the game's frame thread**, `IncidentFrameTick()` runs as the second
  statement of `FrameCallback` (the frame profiler's tick stays first). It
  takes a `QueryPerformanceCounter` reading, writes the frame time into a
  600-frame ring and does atomic stores; nothing else. Once a second it also
  samples the room key and the context counts (room name, instance count,
  monster count) the frame profiler's adapter already reads by name.
- **Per-mod accounting**: an `IncidentScope` at the top of each ForgePact hook
  body takes the clock on entry and exit, adds the difference to that mod's
  per-frame counter, and sets an "in hook" tag to the mod for the duration
  (restoring the previous one on exit). Mods: `density`, `mapreveal`, `drops`,
  `autoprospect`, `hudlabels`, `farsleep`, `gems`, `miner`, `stashmoveall`,
  `ipc`, and `frame` for the whole `FrameCallback` body. The density hook on
  every created instance is sampled one call in 16 and scaled by 16.
- **On the monitor thread** (an `ExitSafeThread` inside the plugin, woken every
  250 ms): the analysis, rate limiting, report building and every file write.
  It reads only ForgePact's own atomics and ring, and the Win32 focus state of
  the game window; it never calls YYToolkit or touches a game instance.
- **Crash detection** happens after the fact. A namespace-scope object's
  destructor writes `==== clean shutdown ====` to `out.txt` when the DLL is
  unloaded on a normal exit. On its first wake the monitor looks at the
  previous session's text in `out.txt` (or `out.prev.txt`); a session that
  ends without that line is a crash. The panel, when it is running, holds a
  handle to the game process, reads its exit code when it closes, queries the
  Windows Application log for the matching `Application Error` record (event
  1000: faulting module, offset and exception code), and writes
  `bp_ipc\exit.json`; the plugin folds it into the crash report at the next
  load and deletes it.
- **Notifications**: the panel shows a Windows toast for each new report folder
  (PERF only while `notify_lag` is on). Without a live panel (`bp_ipc\panel.json`
  names no running pid), the plugin shows a `MessageBoxW` for a freeze and for
  a crash found at load, never for an FPS drop.
- **The `incident stat` command** prints the monitor's current view and the
  per-mod table; it is the live control that the monitor is counting.

## The report

`bp_ipc\reports\<yyyymmdd-HHMMSS>_<perf|freeze|crash>\` holds `report.json`,
`out-tail.txt` and `out-prev-tail.txt` (the last 500 lines of each),
`forgepact.json`, `modstate.json`, `mods.txt` (size, file version and SHA-256 of
each file in `mods\aurie\` and of `AurieCore.dll`) and `system.txt` (Windows
build, CPU, GPU and driver, memory). Every path has the user's profile folder
replaced with the literal `%USERPROFILE%`. The ten newest folders are kept.
The plugin is the only writer under `reports\`; the panel only reads it.

## Decisions

- **The monitor is a diagnostic, not a mod, and is always on.** The rule that
  no mod is on by default covers things that change the game; the monitor
  installs no hook and writes nothing into the game. A report missing because
  a switch was off is the outcome the feature exists to prevent, so there is
  no switch for the monitor. The one setting, the panel's `notify_lag`
  (default on), silences PERF notifications only.
- **One writer for the report folder: the plugin.** The panel contributes
  `exit.json` and `panel.json` and reads `reports\`; two writers of one format
  in two languages would drift, and a plugin-only install would get no crash
  report at all.
- **The clean-shutdown marker is a static destructor using Win32 file calls
  only** (`CreateFileA`/`WriteFile`/`CloseHandle`). It is the only ForgePact
  code that runs on a normal `ExitProcess`; a stream, `Out()` or a library
  load there would be the exit-time crash class the guide already records.
  `tests/incident_shutdown_probe.cpp` proves both halves: `ExitProcess`
  leaves the marker, `TerminateProcess` does not.
- **The thread is an `ExitSafeThread` on a heap-held singleton**, the frame
  profiler's shape, started from `ModuleInitialize` after the frame callback
  registers. It appends to `out.txt` through `OutRaw`, which moved out of the
  research block so both builds have it. No thread suspension anywhere.
- **Thresholds are named constants in `IncidentMonitor.hpp`**: hitch 250 ms;
  sustained 2.5 times the baseline for 2000 ms; freeze 3000 ms; 5000 ms grace
  after a room change; a 600-frame ring whose median, taken before the
  episode window, is the baseline; at most one episode per 30 s and 50 a
  session; at most one report folder per 5 minutes and 10 a session; 10
  folders kept. No command changes them.
- **The room-change signal is `CurrentRoomKey()`**, sampled once a second,
  rather than map reveal's zone generation, which only advances while map
  reveal is on. Focus is decided on the monitor thread from
  `GetForegroundWindow` and `IsIconic`, with no game state.
- **No new built-in name in the incident code.** The context counts come
  from the frame profiler's existing adapter, so the set of names the player
  build resolves does not grow.
- **The panel reads the exit code from a handle it holds**, opened when it
  first sees the game running, so a crash is read even when the panel started
  after the game. An exit code of 0 writes nothing.
- **Panel toasts go through PowerShell's own app identity** with the message
  passed as an argument, never interpolated into the script, and every failure
  swallowed; the toast carries no button (the Open folder button is in the
  panel). The plugin's message box appears only when no panel is running.
- **The PDB comes from the release workflow's environment, not from
  `build.bat`.** `forgepact-release.yml`'s compile step sets `CL=/Zi` and
  `_LINK_=/DEBUG:FULL /OPT:REF /OPT:ICF /PDBALTPATH:BloodPactPlugin.pdb`, so
  `build.bat`'s `cl` line, which every tag's compile-line check compares,
  stays verbatim. `/OPT:REF /OPT:ICF` are named because `/DEBUG` turns them
  off by default. `plugin_build/BloodPactPlugin_ship.pdb` is kept as the
  `forgepact-plugin-pdb-<tag>` artifact for 90 days and never enters the zip.
  `tests/test_release_pdb_symbols.py` compiles `tests/pdb_codegen_probe.cpp`
  plain and under exactly those values and finds the compiler's code and
  relocations identical and the linked `.text` at the same address with the
  same size. The shipped DLL is this symbols build, so the PDB matches it.

## Known limits

- **A crash during exit can read as a clean session.** Another DLL's exit-time
  abort (the guide's Known Limitations item on the 0xC0000409 exit crash, for
  instance `HS-Offline-Tracker`'s producer) may run after our destructor wrote
  the marker.
- **No function names without the PDB.** A report records the faulting module
  and offset only. Mapping an offset inside `BloodPactPlugin.dll` to one of our
  functions is a maintainer step with that tag's PDB artifact, which expires
  after 90 days.
- **A symbols build's `.rdata` layout differs from a plain build's.** `/DEBUG`
  grows the debug directory inside `.rdata`, which moves the data after it, so
  the RIP-relative displacements pointing there differ (on the probe, 14 bytes
  in 7 fields of `.text`). The instructions and every function's address do
  not change. Proven on the probe; for the whole plugin it is UNVERIFIED until
  a dry-run release build.
- **FPS drops near a room change are not reported**: the first 5 seconds after
  the room changes are a grace period, because loading a zone is slow by
  design. Drops while the game is in the background or minimised are not
  reported either, and at most one episode is reported every 30 seconds.
- **The Application log may hold no record**, or be unreadable; `exit.json`'s
  `event_probe` (`queried`, `records_seen`) says which, so "no record" is not
  confused with "could not read".
- **Without the panel**, a crash's report has no exit code or faulting module,
  only the missing clean-shutdown line.
- **The toast is not tested automatically** beyond its command builder; it
  is checked by eye in Live 1.

## Live results

Pending Live 1: the session installs the 2.2.0 shipping DLL, checks that a
normal session writes no report and ends with the clean-shutdown line, that a
heavy scene produces a PERF report whose per-mod table is filled, and that the
panel's toast appears. Results will be recorded here as observed or not
observed live, with the numbers.
