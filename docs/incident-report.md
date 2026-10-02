# Incident reports - crash, freeze and FPS-drop detection (issue #76)

Status (2026-10-02): **built for 2.2.0 and verified against a stand-in frame
thread** (`tests/incident_monitor_harness.cpp`, `tests/incident_shutdown_probe.cpp`)
and by contract tests, and **run in the game in Live 1** (see "Live
results": crash and freeze notices and a freeze at a zone load were not
observed live). The player-facing description is the README's
[Incident reports](../README.md#incident-reports-crash-freeze-and-fps-drop-reports)
section.

## Why

A player's "the game crashed" or "it got laggy" arrives with no evidence, and
the first question, whether ForgePact had anything to do with it, could only be
answered by asking for `out.txt` and guessing. The player build now notices a
crash, a freeze or a significant FPS drop on its own, says so for a crash or a
freeze (an FPS drop is recorded silently), and writes a folder the player can
attach as it is. Each folder answers three questions:
which of our hooks the game was inside, which mod was on or busy, and how much
frame time our hooks were taking. The first answer has limits (a crash cannot
know it, and a few hooks are untagged; see "Known limits"), so `none` is a
narrow statement, not an exoneration.

## What it does

- **On the game's frame thread**, `IncidentFrameTick()` runs as the second
  statement of `FrameCallback` (the frame profiler's tick stays first). It
  takes a `QueryPerformanceCounter` reading, writes the frame time into a
  600-frame ring and does atomic stores; nothing else. Once a second it also
  samples the room key and the context counts (room name, instance count,
  monster count) the frame profiler's adapter already reads by name.
- **The in-hook id, set by the installer**: `HookOneScript` and `HookBuiltin`
  hand both of their routes (the script-table swap and the inline detour) a
  per-slot thunk from `TaggedThunks<Fn>` instead of the caller's function.
  The thunk sets the in-hook id to the hook's own id on entry, restores the
  previous id on exit, and calls the hook body, which still receives the
  trampoline as its original. It takes no clock reading and counts nothing,
  so the hottest hooks pay two atomic stores. Every header installer goes
  through one of those two, so a hook added later is tagged without anyone
  remembering to. `InstallSlotHook` tags its detour the same way and
  `HookProtGet` opens with an `IncidentHookTag`; the ten native population
  detours are the untagged set, counted (`incident stat` prints `hooks tagged
  N, untagged M`).
- **Per-mod accounting, set by named scopes**: an `IncidentScope` at the top
  of each mod's hook body takes the clock on entry and exit, adds the
  difference to that mod's per-frame counter, and sets the in-mod tag for the
  duration (restoring the previous one on exit). Mods: `density`,
  `mapreveal`, `drops`, `autoprospect`, `hudlabels`, `farsleep`, `gems`,
  `miner`, `stashmoveall`, `ipc`, and `frame` for the whole `FrameCallback`
  body. The density hook on every created instance is sampled one call in 16
  and scaled by 16.
- **On the monitor thread** (an `ExitSafeThread` inside the plugin, woken every
  250 ms): the analysis, rate limiting, report building and every file write.
  It reads only ForgePact's own atomics and ring, and the Win32 focus state of
  the game window; it never calls YYToolkit or touches a game instance.
- **Crash detection** happens after the fact. An inline detour on
  `ExitProcess` (id `fp_exit_marker`, resolved by name from `kernelbase.dll`)
  writes `==== clean shutdown ====` to `out.txt` when the game asks Windows to
  exit, before any DLL is detached; a namespace-scope object's destructor
  writes `==== clean shutdown (detach) ====` as the fallback when the DLL is
  unloaded. Whichever runs first writes, once. Ten seconds after load the
  monitor looks at the previous session's text in `out.txt` (or
  `out.prev.txt`); a session with no line starting `==== clean shutdown` is a
  crash. The panel, when it is running, holds a handle to the game process,
  reads its exit code when it closes, queries the Windows Application log for
  the matching `Application Error` record (event 1000: faulting module,
  offset and exception code), notes whether `out.txt`'s last session already
  ends in a clean-shutdown line (`after_clean_shutdown`), and writes
  `bp_ipc\exit.json`; the plugin folds it into the crash report at the next
  load and deletes it. A non-zero exit after a clean shutdown is logged at
  the next load, with no report folder and no toast.
- **Notifications**: the panel shows a Windows toast for each new FREEZE or
  CRASH report folder. Without a live panel (`bp_ipc\panel.json` names no
  running pid), the plugin shows a `MessageBoxW` for a freeze and for a crash
  found at load. A PERF (FPS drop) report is written without a notification
  from either side: the panel lists it on the Incident reports card and never
  toasts it, the plugin never calls `MessageBoxW` for it, and no setting
  changes that.
- **The `incident stat` command** prints the monitor's current view and the
  per-mod table; it is the live control that the monitor is counting. Its
  first line starts `incident: frames ` and carries the worst frame overall
  (and whether it was judged), the worst judged frame, the count of judged
  frames of 250 ms or more and `window yes|no`; a third line carries the
  tagged and untagged hook counts and the report write errors.

## The report

`bp_ipc\reports\<yyyymmdd-HHMMSS>_<perf|freeze|crash>\` holds `report.json`,
`out-tail.txt` and `out-prev-tail.txt` (the last 500 lines of each),
`forgepact.json`, `modstate.json`, `mods.txt` (size, file version and SHA-256 of
each file in `mods\aurie\` and of `AurieCore.dll`) and `system.txt` (Windows
build, CPU, GPU and driver, memory). Every path has the user's profile folder
replaced with the literal `%USERPROFILE%`. The ten newest folders are kept.
The plugin is the only writer under `reports\`; the panel only reads it.
`report.json`'s `frames.inHook` and `frames.inMod` depend on the kind: a
freeze gives the two names captured when the gap crossed 3 s (or `none`); a
crash gives `"unknown"` for both, because it is found at the next load, where
nothing about the crashed frame is knowable; an FPS drop gives `null` for
both, because the per-mod tables answer that question. A file of the bundle
that fails to write is counted, and the `incident: report written` line says
` (N files failed)`, so a missing file is not mistaken for empty data.

## Decisions

- **The monitor is a diagnostic, not a mod, and is always on.** The rule that
  no mod is on by default covers things that change the game; the monitor
  hooks no game script, writes nothing into the game, and changes no hook's
  behaviour (the installer's thunk only names it; its one detour is on
  Windows' `ExitProcess`, to write the marker). A report missing because
  a switch was off is the outcome the feature exists to prevent, so there is
  no switch for the monitor, and none for its notices either. An FPS drop is
  recorded without a notice of any kind: after Live 1, where the PERF toast
  had landed in Windows' notification center while the game ran fullscreen,
  the owner decided on 2026-10-02 that a PERF report is written but the
  player is not told, so the panel's FPS-drop switch that 2.2.0's
  development builds carried was removed. Crash and freeze notices are
  unchanged.
- **One writer for the report folder: the plugin.** The panel contributes
  `exit.json` and `panel.json` and reads `reports\`; two writers of one format
  in two languages would drift, and a plugin-only install would get no crash
  report at all.
- **The clean-shutdown marker is written at `ExitProcess`, with a static
  destructor as the second writer.** Aurie runs no module code at exit and the
  plugin has no `DllMain`, so round 0 relied on a namespace-scope destructor
  alone. But the tracker producer the panel installs beside the plugin aborts
  most game exits from its own exit-time destructors (the guide's Known
  Limitations item 25), and DLLs detach in reverse load order: if our
  destructor ran after that abort, every exit on a tracker user's machine
  would read as a crash, with a report and a message box at every launch. So
  the adapter installs, by name (`GetModuleHandleW(L"kernelbase.dll")` then
  `GetProcAddress`, `kernel32.dll` as the fallback; `MmCreateHook`, id
  `fp_exit_marker`), an inline detour on `ExitProcess` whose body writes the
  marker and calls the trampoline. `ExitProcess` runs on the game thread with
  every DLL still mapped, before any detach. A real crash (WER,
  `TerminateProcess`, a fast fail) never calls it. `ShutdownMarker::Write`
  is once-only and names its route: `==== clean shutdown ====` from the exit
  hook, `==== clean shutdown (detach) ====` from the destructor, and the
  crash check matches the common prefix, so `out.txt` says which writer
  fired. Both writers use `CreateFileA`/`WriteFile`/`CloseHandle` only: a
  stream, `Out()` or a library load there would be the exit-time crash class
  the guide already records. `tests/incident_shutdown_probe.cpp` proves the
  destructor's halves (`ExitProcess` leaves the marker, `TerminateProcess`
  does not) and the harness's `marker-once` proves two writes leave one line.
  Live 1 observed that the game's close reaches kernelbase's `ExitProcess`
  export (the marker carried no `(detach)`), which a static reading had
  predicted: the runner leaves through `exit()`, and the C runtime's import
  resolves to kernelbase.
- **Two tag channels: the installer tags every hook, named scopes time the
  mods** (owner decision, 2026-10-02: "Tag in the installer"). Round 0 set the
  in-hook tag only from scopes placed by hand in eleven mod bodies, while the
  player build installs about seventy hooks; a freeze in any of the others
  read `in-hook none`, a false "not ours", and every hook added later would
  have been blind the same way. The interception therefore lives in the
  installer, following the hub's rule to put it there rather than at the call
  sites someone checked. `TaggedThunks<Fn>` is specialised on the function
  pointer type and instantiated twice in `ModuleMain.cpp`
  (`TaggedThunks<PFUNC_YYGMLScript>`, `TaggedThunks<TRoutine>`), with
  `kHookSlots = 128` slots each. A slot holds the body and its own copy of
  the id (callers' ids come from tables and arrays, so the caller's pointer
  is never kept). `Tagged(id, dest)` reuses the slot of a body seen before,
  since re-installing is how the plugin keeps installs idempotent, and when
  the table is full it returns the body itself and counts an untagged
  install. The id lives in `Accounting` as an atomic pointer beside the mod
  channel. `IncidentHookTag` is the scoped form for a body no installer sees.
  The ten population detours have ten different native signatures; tagging
  them by hand was rejected, so they are counted and documented instead.
- **A freeze verdict waits for its end, so a load is not a freeze.** A zone
  load, the character load or the game's own save at exit can stop the frame
  thread for 3 s or more with the window visible. The room-change grace
  cannot cover it the way it covers a hitch: the room key is sampled on the
  frame thread, which the load is blocking, so the new key is seen only after
  the gap. So when the gap crosses 3 s the detector marks the freeze and
  captures both tags and the last frame's time, but emits nothing. When
  frames resume it waits `kRoomLeadMs` (1.5 s) for a room change: one seen
  after the freeze began means the gap was a load, counted as quiet and
  logged `after a room change: a load, not reported`. Otherwise the episode
  is emitted then, with the gap as its length. A freeze that never ends is
  emitted once the gap reaches `kFreezeHoldMs` (15 s): a 15 s load is worth a
  report, and the exit save never reaches it because the process is gone.
- **The panel's routes leave a trace.** `/api/state`'s `incidents` carries
  `exitWatch` (`pidHeld`, `exitsSeen`, `lastCode`) and `toasts` (`sent`,
  `failed`, `lastError`), so "the game exited cleanly" is told apart from "no
  exit was watched", and a missing toast from a failed one (`toasts` counts
  crash, freeze and exit-with-error toasts; a PERF report sends none). When
  `out.txt`'s last session already ends in a clean-shutdown line, a non-zero
  exit is written to `exit.json` with `after_clean_shutdown: true` for the plugin to
  note, and no toast is shown: item 25's abort would otherwise toast "closed
  with an error" at every exit. The Setup card's last-exit line says the exit
  came after ForgePact's clean shutdown.
- **The instrument reports what it did.** `incident stat`'s first line is the
  live marker and starts `incident: frames ` (the thread's start line,
  `incident: monitor running`, stays separate, because the crash check reads
  it). The worst-frame bookkeeping sits in the judging loops, where it is
  known whether a frame was quiet, so the worst judged frame (armed, not
  quiet, not a freeze frame) and the count of judged frames of 250 ms or more
  are printed apart from the worst frame overall: the overall worst is
  nearly always a load. `window yes|no` says whether the game window was
  found, without which freeze detection is off. `Out` (frame thread) and
  `OutRaw` (monitor thread) append to `out.txt` under one lock; the shutdown
  marker never takes it. The panel writes `exit.json` and `panel.json` and
  the plugin reads them, so both bindings are held to one fixture,
  `tests/fixtures/incident/exit.json` and `panel.json`: the harness's
  `exit-json-fixture` reads every key the plugin reads through the header's
  parsers, and `test_incident_panel.py` holds the Python writers' key sets
  equal to the fixture's.
- **The thread is an `ExitSafeThread` on a heap-held singleton**, the frame
  profiler's shape, started from `ModuleInitialize` after the frame callback
  registers. It appends to `out.txt` through `OutRaw`, which moved out of the
  research block so both builds have it. No thread suspension anywhere.
- **Thresholds are named constants in `IncidentMonitor.hpp`**: hitch 250 ms;
  sustained 2.5 times the baseline for 2000 ms; freeze 3000 ms, held until it
  ends or until `kFreezeHoldMs` (15000 ms); 5000 ms grace after a room change,
  reaching `kRoomLeadMs` (1500 ms) back before the change was seen; a
  600-frame ring whose median, taken before the
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

- **`none` is not an exoneration.** The in-hook id covers every hook installed
  through `HookOneScript`, `HookBuiltin` and `InstallSlotHook`, plus
  `HookProtGet`. The ten native population detours `PreparePopulationCapacity`
  installs (`fp_population_*`) are untagged: time inside them reads `in-hook
  none`, and `incident stat` prints their number as `untagged`. A hook table
  that ran out of its 128 slots would add to the same count. So `none` means
  "not inside a tagged hook", never "not ForgePact". The per-mod table covers
  only the named mods; any other hook's time shows under `frame` at most.
- **A crash report cannot say what was running.** It is written at the next
  load, so its `inHook` and `inMod` are `"unknown"`, and its room and counts
  are empty.
- **An exit-time abort, in both directions. Only the first is observed
  live.** Another DLL's exit-time abort (Known Limitations item 25 in the
  guide: the tracker producer, `0xC0000409`) can fall on either side of our
  marker. After the marker, which the `ExitProcess` route makes the likely
  order and Live 1 observed, the session reads as clean: the panel records the non-zero exit with
  `after_clean_shutdown: true`, the next load logs that the previous session
  shut down cleanly and that the panel recorded the exit after it, and no
  report folder or toast follows. Before any marker: if the game's close did
  not pass through the hooked `ExitProcess` export and the producer's
  destructors aborted before ours ran, no marker would be written and every
  exit would read as a crash, so a tracker user would get a crash report,
  and without the panel a message box, at every launch. The `ExitProcess`
  route is there to prevent exactly that; this order is not observed live
  (in Live 1 the `ExitProcess` route fired and the producer's abort came
  after the marker).
- **A freeze is reported only when it ends, or at `kFreezeHoldMs`.** A gap of
  3 s or more that ends with a room change within 1.5 s of frames resuming is
  taken as a load and never reported, so a real freeze that happens to end in
  a zone change is missed. A freeze that never ends is reported after 15 s;
  one that the process does not survive for 15 s leaves only the crash path.
  A frozen game's message box therefore appears at 15 s, not at 3 s.
- **No function names without the PDB, and the PDB needs renaming first.** A
  report records the faulting module and offset only. Mapping an offset
  inside `BloodPactPlugin.dll` to one of our functions is a maintainer step
  with that tag's `forgepact-plugin-pdb-<tag>` artifact, which expires after
  90 days. The artifact holds `BloodPactPlugin_ship.pdb`, while the DLL
  embeds the name `BloodPactPlugin.pdb` (`/PDBALTPATH`), which is what a
  debugger looks for: rename the file to `BloodPactPlugin.pdb` beside the DLL
  (or in the symbol path) before loading the dump.
- **Two `std::ofstream` writers share `out.txt`.** `Out` on the frame thread
  and `OutRaw` on the monitor thread both append in append mode, which the C
  runtime implements as a seek to the end followed by a write; across two
  handles that is not atomic, so they take one lock. A third writer that
  skips it (the shutdown marker deliberately does, since it runs during
  exit) can still interleave with them.
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
  only the missing clean-shutdown line, and an exit-time abort after the
  marker goes unrecorded.
- **The toast is not tested automatically** beyond its command builder and
  the tests that a new FREEZE or CRASH folder sends one and a new PERF folder
  sends none; it is shown for a crash or a freeze only, and a toast's actual
  appearance on screen is checked by eye in a live session.

## Live results

Live 1, 2026-10-02 (capture: the workorder's
`forgepact-76-incident-report-live-1.md`, kept with the hub's local workorder
files). The 2.2.0 shipping DLL (SHA-256 `0c208d37...b61b9`) ran in the game
with the tracker producer (`HSOfflineTrackerProducer.dll`) installed beside
it, and the panel running. The panel was the build from before the FPS-drop
toast was removed. Ten of the thirteen checks passed; the other three are
recorded below as the owner accepted them.

- **Installed and answering.** The log showed `HOOK INSTALLED on
  kernelbase.dll!ExitProcess (incident: clean-shutdown marker)` and the
  monitor's running line, and `ping` answered `pong (YYTK 4.0.1)`. The
  first `incident stat` line read `incident: frames 3503 | baseline 6.9 ms |
  worst 2798.9 ms (not judged) | worst judged 52.0 ms | slow judged frames 0
  | ...| in-hook none | in-mod none`. Observed live.
- **The installer tag.** With the usual mods on: `hooks tagged 31, untagged
  0`. With density on, the population detours are installed and counted as
  untagged: `25, untagged 5` before the dense play and `27, untagged 9` after
  it, within the ten native pool detours this doc expects. Observed live.
- **A normal session writes nothing.** About two minutes in town and one
  zone: frames 36764, worst 2798.9 ms (not judged, inside a room change's
  grace), worst judged 185.0 ms, slow judged frames 0, episodes 0, 11
  ignored near a room change or unfocused, reports written 0, and no
  `reports\` folder. Observed live.
- **The clean-shutdown marker comes from the `ExitProcess` route.** An
  ordinary close (a window close, not a forced kill) left `==== clean
  shutdown ====` as the last line of `out.txt`, without `(detach)`: the
  game's close reaches kernelbase's `ExitProcess` export under Aurie, and the
  exit hook wrote the marker before any DLL detached. Observed live.
- **The tracker producer's exit-time abort falls after the marker.** With
  the producer present the game exited `0xC0000409`. The panel's exit watch
  counted it (`exitsSeen 1`, `lastCode 0xC0000409`), wrote `exit.json` with
  `after_clean_shutdown: true` (`event_probe` queried, 20 records seen, no
  faulting module), and sent no toast (`sent 0, failed 0`). At the next
  load the plugin logged `incident: the previous session shut down cleanly;
  the panel recorded exit 0xC0000409 after it`, with no `CRASH` line and no
  crash folder. Observed live. The other order, an abort before any marker,
  was not observed live.
- **A heavy scene produces a PERF report, and its bundle is complete.**
  With density 5 and map reveal on, the log carried three episodes: `PERF
  hitch 508 ms frame | baseline 6.9 ms | room Town_01_rm | top ipc 495.0
  ms/frame`, then two `PERF sustained` episodes in the two dense zones (6.9x
  and 2.6x for 2 s, top `mapreveal` at 0.1 ms/frame). One report was
  written, `reports\20261002-172932_perf`, for the town hitch; the two
  dense-zone episodes were held back by the gap between bundles (`episodes
  3, held back 3 | reports written 1`; worst judged 1594.9 ms, slow judged
  frames 4). The folder held `report.json`, `out-tail.txt`,
  `out-prev-tail.txt`, `forgepact.json`, `modstate.json`, `mods.txt` and
  `system.txt`, and the Windows user name appeared in none of their lines.
  Observed live.
- **Density attribution in the per-mod table: not observed live.** The one
  report was the town hitch, which came right after the panel's batch of
  settings commands, with the plugin's command handling (`ipc`) taking the
  time: its `modsEpisode` rows start with
  `frame` (495.161 ms) and `ipc` (495.036 ms), with `density` last at 0.000.
  The dense-zone episodes that would have shown density's cost wrote no
  bundle. The owner accepted this as not observed live.
- **A freeze at a zone load: not observed live.** No load blocked frames for
  3 s, so there was no `a load, not reported` line and no `FREEZE` line, and
  the deferred freeze verdict went unexercised. Its behaviour rests on the
  harness's `freeze-load-room-change` and `freeze-never-ends` scenarios.
- **The FPS-drop toast reached Windows but not the player.** The panel
  counted one toast sent and none failed (`lastError` null). Windows
  delivered it to its notification center but held it there while the game
  ran fullscreen, so the owner did not see it in the game. The owner then
  decided that an FPS drop's report is saved without a notification (see
  "Decisions"), so 2.2.0 shows no notice for an FPS drop, and a crash's or
  a freeze's toast, which this session did not produce, was not observed
  live either.
