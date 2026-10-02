# Incident reports - crash, freeze and FPS-drop detection (issue #76)

Status (2026-10-02): **built for 2.2.0 and verified against a stand-in frame
thread** (`tests/incident_monitor_harness.cpp`, `tests/incident_shutdown_probe.cpp`)
and by contract tests, and **run in the game in Live 1** (see "Live
results": density's row in a report and a freeze at a zone load were not
observed live). Since the owner's two later decisions (2026-10-02),
no report notifies anyone, and a mod's time counts only ForgePact's own code
("Decisions"). The player-facing description is the README's
[Incident reports](../README.md#incident-reports-crash-freeze-and-fps-drop-reports)
section.

## Why

A player's "the game crashed" or "it got laggy" arrives with no evidence, and
the first question, whether ForgePact had anything to do with it, could only be
answered by asking for `out.txt` and guessing. The player build now notices a
crash, a freeze or a significant FPS drop on its own and writes a folder the
player can attach as it is, without a notification of any kind: the panel
lists every report on its Incident reports card. Each folder answers three
questions: which of our hooks the game was inside, which mod was on or busy,
and how much frame time our own code was taking (never the game work a hook
wraps). The first answer has limits (a crash cannot
know it, and a few hooks are untagged; see "Known limits"), so `none` is a
narrow statement, not an exoneration.

## What it does

- **On the game's frame thread**, `IncidentFrameTick()` runs as the second
  statement of `FrameCallback` (the frame profiler's tick stays first). It
  takes a `QueryPerformanceCounter` reading, writes the frame time into a
  600-frame ring and does atomic stores; nothing else. Once a second it also
  samples the room key and the context counts (room name, instance count,
  monster count) the frame profiler's adapter already reads by name, and
  stores whether that room is a menu room (`IncidentIsMenuRoom`).
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
  of each mod's hook body takes the clock on entry and exit, adds that mod's
  own time to its per-frame counter, and sets the in-mod tag for the
  duration (restoring the previous one on exit). Mods: `density`,
  `mapreveal`, `drops`, `autoprospect`, `hudlabels`, `farsleep`, `gems`,
  `miner`, `stashmoveall`, `ipc`, `setup` for the one-time setup at start-up
  (`LoadConfig` and `InstallHook`, run once at frame 300; a 2-4 s frame on
  the test machine in Live 1 and Live 2), and `frame` for `FrameCallback`'s
  own code outside the named mods. The density hook on every created instance is
  sampled one call in 16 and scaled by 16. Every row is self time: each call
  a scoped body makes into the game's original (`g_Orig_DrawHudBuffs`,
  `g_Orig_DropRelic`, the `DropManager.hpp` drop hooks' `m_Orig_*`,
  `DoMultiCreate`'s `orig`) sits inside a
  guard in `IncidentMonitor.hpp` that pauses whichever ForgePact clock is
  running on the frame thread and restarts it after, and a scope nested in
  another mod's scope pauses the outer clock while it runs. Inside the
  guard the in-mod tag carries a game-original mark, so a freeze there reads
  `in-mod hudlabels (game original)`.
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
  the next load, with no report folder.
- **No notice**: every report, PERF, FREEZE or CRASH, is written without a
  notification from either side (the owner's decision of 2026-10-02, "No
  notice at all"). The panel lists every report folder on the Incident
  reports card and starts no process to tell anyone; the plugin shows no
  window and calls no message-box API; no setting changes that. The panel
  still writes `bp_ipc\panel.json`, now only so a report's `panelVersion`
  can name the running panel.
- **The `incident stat` command** prints the monitor's current view and the
  per-mod table; it is the live control that the monitor is counting. Its
  first line starts `incident: frames ` and carries the worst frame overall
  (and whether it was judged), the worst judged frame, the count of judged
  frames of 250 ms or more, `window yes|no` and `menu yes|no` (whether the
  room the frame thread last sampled is a menu room); a third line carries
  the tagged and untagged hook counts and the report write errors.

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
  no switch for the monitor.
- **No report notifies anyone** (owner decisions, 2026-10-02). After Live 1,
  where the panel's FPS-drop notification had landed in Windows'
  notification center while the game ran fullscreen, the owner first decided
  that a PERF report is written without a notification, and the panel's
  FPS-drop switch that 2.2.0's development builds carried was removed. After
  the next code review the owner widened that to "No notice at all": a freeze or
  crash report, like an FPS drop's, is written and listed in the panel, with
  no toast and no message box of any kind. So the panel's toast path, its
  counters and the plugin's message-box thread are gone, and there is no
  setting for a notice. Keeping the crash and freeze notices for a windowed
  player was rejected: the owner said no notice at all.
- **A mod is charged only for its own code** (owner decision, 2026-10-02:
  "Only our own work"). Until then each `IncidentScope` started its clock
  before the hook called the game's original, so the original's time was
  charged to the mod, and `frame` was inclusive, so it always ranked first
  and could not be compared with the rows below it (Live 1's per-mod table
  started with `frame`). Now:
  - every game-original call in a scoped body (`Hook_DrawHudBuffs`,
    `DoMultiCreate`, `Hook_DropRelic`, `DropManager.hpp`'s `FP_DROP_HOOK`
    bodies, `Hook_DropGold`, `Hook_DropMonsterGold`, `Hook_DropKeys`) runs
    inside a guard that pauses whatever ForgePact clock is running on the
    frame thread, whichever scope started it, and restarts it after; it reads
    the clock only when one is running, so `DoMultiCreate`'s untimed calls
    stay at a couple of relaxed stores. `DropManager.hpp` has a guard macro
    beside `FP_DROP_INCIDENT_SCOPE` that compiles to the bare call where
    `IncidentMonitor.hpp` is absent. The tick scopes (`DensityCopiesTick`,
    `GemsTick`, `AutoProspectTick`, `StashMoveAllTick`, `FarSleepTick`,
    miner, mapreveal, ipc, frame) wrap no original and have no guard: the
    built-ins and scripts they call are their own work.
    `test_every_scoped_original_call_is_guarded` finds every scoped body and
    fails on an original call outside the guard;
  - every row is self time: a scope nested in another mod's scope pauses the
    outer clock while it runs, so `frame` is `FrameCallback`'s own code
    outside the named mods, the rows add up to ForgePact's total, and
    `TopMod` ranks `frame` like any other row;
  - same-mod nesting is counted once while that mod's clock runs; a paused
    clock is not running, so a hook the game calls from inside a wrapped
    original (a drop hook inside `DropRelic`'s original) times its own code;
  - a sampled scope's untimed call still pauses another mod's running
    clock, and its own time counts only when sampled, times `kSampleEvery`;
  - the extra originals a multiplier calls (the drop loops, density's extra
    copies) are game work too and are excluded, since the owner asked that a
    mod count "only ForgePact's own code";
  - inside the guard the in-mod channel names the mod with the mark
    `(game original)`: `FreezeLine` and `StatLines` print, for example,
    `in-mod hudlabels (game original)`, and a freeze's `report.json` says the
    same; the mark is gone after the guard. `none` and a crash's `unknown`
    are unchanged, and the installer's in-hook channel still names the hook
    inside the original.

  Timing the original as a separate `game` row was rejected: it would bring
  back the reading the decision removes.
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
  would read as a crash, with a crash report at every launch. So
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
- **A gap that begins in a menu room is a load, never a freeze** (D17,
  replan 5, after Live 2; the owner: "If it wasn't high then fix it"). Live
  2 wrote a FREEZE report for a 3.53 s gap in `Chose_rm` right after the
  character screen's slot click: the game loads the save there, and the
  screen after the click is the character panel in the same room, so no
  room change follows the gap and the room-change lead above has nothing to
  see. So the frame thread's once-a-second context tick also stores whether
  the room is a menu room, and the detector keeps that flag from the moment
  a gap crosses 3 s. A gap that began in a menu room is a load on both
  paths, when it ends and when it reaches `kFreezeHoldMs`: it is counted as
  quiet, `FreezeEndedLine` logs `in a menu room: a load, not reported`, and
  no episode is written. The menu rooms are a table in the adapter
  (`ModuleMain.cpp`, beside `IncidentFrameTick`), each entry spelled from
  the SDK's `HeroSiege::Rooms::GameRoom` enum through one macro, so a name
  the SDK lacks fails the compile: `Init_rm`, `Game_Start_rm`, `Login_rm`,
  `Login_Valhalla_rm`, `Main_Menu_rm`, `Main_Menu_Valhalla_rm`,
  `Char_Select_rm` and `Chose_rm`. The string compared is the identifier
  after the enum's prefix, which is what `room_get_name` answers; the
  header stays game-independent and takes only the flag. `incident stat`'s
  `menu yes|no` is the live control that the flag reads the room. Rejected:
  comparing the room index (the plugin reads the room's name, never its
  index); reporting a menu-room freeze at `kFreezeHoldMs` anyway (no
  ForgePact mod runs menu code, so the report would name nothing, and a
  save load or cloud sync at the character screen is the game's normal
  work); a separate rule for "the first in-world room after character
  select" (a gap that begins in `Chose_rm` and ends in town is a menu-room
  gap whether or not the room change is seen in time). The harness pins it
  with `freeze-menu-room` and `freeze-menu-room-never-ends`, each beside a
  control with the flag off that reports the freeze as before.
- **The one-time start-up setup is its own row, `setup`, and prints its
  cost** (D18, replan 5, after Live 2). Live 2's bundle read `frame` at
  4084.41 ms worst over the last minute beside a gap in which the frame
  thread ran no ForgePact code, which reads as our code at the load. The
  row was the one-time setup: `LoadConfig` and `InstallHook` run once, at
  `fc > 300` in the main menu about 5-10 s after launch, inside
  `FrameCallback`'s `frame` scope and with no scope of their own, so the
  whole 2-4 s setup frame was `frame`'s self time, and for a minute after
  it the row's worst named the wrong thing. So the setup block opens
  `IncidentScope incidentSetup(IncidentMod::setup)` (a row like any mod's:
  self time, ranked with the others) and prints one line, `incident: setup
  <total> ms at frame <fc>: config <ms> ms, hooks <ms> ms (<the three
  slowest installers, name and ms>)`, which lands in every later bundle's
  `out-tail.txt`, so a report's reader sees what the row was. `InstallHook`
  marks a lap after each installer of its normal path to supply the three
  slowest (the shipping build's normal path stops after the custom-item,
  item-truth, auto-arm and Headhunter installers; the development build's
  continues through every research installer). The clock readings go
  through `ForgePact::Incident::Qpc()`. Bounding or moving the setup is not
  decided here; Live 3 measures which installer costs what first.
- **The panel's route leaves a trace.** `/api/state`'s `incidents` carries
  `reports`, `lastExit` and `exitWatch` (`pidHeld`, `exitsSeen`,
  `lastCode`), so "the game exited cleanly" is told apart from "no exit was
  watched". When `out.txt`'s last session already ends in a clean-shutdown
  line, a non-zero exit is written to `exit.json` with
  `after_clean_shutdown: true` for the plugin to note rather than report:
  item 25's abort would otherwise read as a crash at every exit. The Setup
  card's last-exit line says the exit came after ForgePact's clean shutdown.
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
  only the named mods; any other hook's time shows under `frame` only when it
  runs inside `FrameCallback`'s own code, and in no row otherwise.
- **A mod is not charged for the game work it causes.** The guard excludes
  every call into a game original, the extra ones a multiplier makes
  included, so a drop or density multiplier that makes the game do ten times
  the work shows only its own bookkeeping in its row. A freeze inside that
  work still names the mod, as `in-mod <mod> (game original)`.
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
  report folder follows. Before any marker: if the game's close did
  not pass through the hooked `ExitProcess` export and the producer's
  destructors aborted before ours ran, no marker would be written and every
  exit would read as a crash, so a tracker user would get a crash report at
  every launch. The `ExitProcess`
  route is there to prevent exactly that; this order is not observed live
  (in Live 1 the `ExitProcess` route fired and the producer's abort came
  after the marker).
- **A freeze is reported only when it ends, or at `kFreezeHoldMs`.** A gap of
  3 s or more that ends with a room change within 1.5 s of frames resuming is
  taken as a load and never reported, so a real freeze that happens to end in
  a zone change is missed. A freeze that never ends is reported after 15 s;
  one that the process does not survive for 15 s leaves only the crash path.
  A frozen game's report is therefore written at 15 s, not at 3 s. A gap
  that begins in a menu room is never reported, at either point: a hang at
  the main menu or the character screen (`Main_Menu_rm`, `Chose_rm` and the
  other menu rooms) leaves no freeze report, only the log line
  `in a menu room: a load, not reported` when it ends; a crash there is
  still found at the next load. The menu rooms are a list of names, so a
  menu room the list lacks is judged like any other room.
- **The start-up setup frame is slow, and shows as `setup`.** The one-time
  setup (`LoadConfig` and `InstallHook`, at frame 300 in the main menu)
  takes one frame of about 2.5-2.7 s on the test machine: Live 3 measured
  `incident: setup 2491.1 ms at frame 301` (config 0.3 ms, hooks 2490.8
  ms) and 2701.2 ms at the relaunch, and nearly all of it is one
  installer pair, `InstallCustomForgeItemHooks+InstallItemTruth` at 2486.2
  ms. It falls in the room-change grace, so it is never judged or
  reported, but for a minute after it the `setup` row's worst in
  `incident stat` and in any report written in that minute is that frame;
  the `incident: setup` line in the log says what it cost and names the
  three slowest installers. Making it faster or spreading it over frames
  is not part of 2.2.0; it is left for a follow-up with these numbers.
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
- **Nobody is told a report was written.** The player finds a report only on
  the panel's Incident reports card or in the `reports\` folder; a player
  who never opens either does not know one exists. That is the owner's
  choice (see "Decisions"), not an oversight.

## Live results

Live 1, 2026-10-02 (capture: the workorder's
`forgepact-76-incident-report-live-1.md`, kept with the hub's local workorder
files). The 2.2.0 shipping DLL (SHA-256 `0c208d37...b61b9`) ran in the game
with the tracker producer (`HSOfflineTrackerProducer.dll`) installed beside
it, and the panel running. The panel was the build from before the FPS-drop
toast was removed, and both were from before the owner's later decisions
(no notice of any kind; a mod's time is its own code only): this session's
per-mod rows were inclusive, game originals and `frame`'s nested mods
included. Ten of the thirteen checks passed; the other three are
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
  decided that an FPS drop's report is saved without a notification, and
  later the same day that no report of any kind notifies anyone (see
  "Decisions"), so 2.2.0 shows no notice at all. The crash and freeze
  notices this session did not produce were removed before any session
  could observe them.

Live 2, 2026-10-02 (capture: the workorder's
`forgepact-76-incident-report-live-2.md`, kept with the hub's local workorder
files). The shipping DLL built from ForgePact `c674757` (SHA-256
`5dd7cbf6...8ca8990`, equal to the lease's hash) ran after the owner's later
decisions: no notice of any kind, and every per-mod row, `frame` included,
is ForgePact's own code only. Eight checks were recorded: six passed, one
is values only, and `no-report-normal` failed. That fail is the finding
below, and the fix for it is in the code Live 3 runs.

- **Installed and answering.** `ping` answered `pong (YYTK 4.0.1)`, and
  the first `incident stat` line read `incident: frames 3570 | baseline 6.9
  ms | worst 4100.7 ms (not judged) | worst judged 64.2 ms | slow judged
  frames 0 | watching yes | grace no | focus yes | window yes | in-hook none
  | in-mod ipc`. Observed live.
- **The installer tag.** `hooks tagged 19, untagged 0 | report write errors
  0`. Observed live.
- **A mod switched off is not charged: not observed live; harness evidence
  only.** The values the session gave: the `DrawHudBuffs` hook kept running
  with the HUD mods off, `hudCalls` 1988 before the 70 s wait and 14198
  after it, while the `hudlabels` row read 0.00 / 0.02 ms after the wait
  (0.00 / 0.05 before it), beside Live 1's step-0 `hudlabels 0.08 / 1.34`
  when the row still included the game original. The label was on with no
  draws and the border and the skill timer were already off with none, so
  no draw of our own was charged either way, and the release DLL has no
  readout of the original's own time to compare against. The claim rests on
  the harness's `game-original-excluded` and `game-original-in-mod`
  scenarios.
- **The clean-shutdown marker, and no crash after it.** An ordinary close
  left `==== clean shutdown ====` after the session's banner, and at the
  next load the plugin logged `incident: the previous session shut down
  cleanly; the panel recorded exit 0xC0000409 after it`, with no `CRASH`
  line and no crash folder. Observed live.
- **A normal session writes nothing: failed (`no-report-normal`).** The
  second line read `episodes 1, held back 0, ignored near a room change or
  unfocused 5 | reports written 1 | last reports\20261002-190757_freeze`,
  unchanged across the 70 s wait. The bundle `20261002-190757_freeze` says
  what happened: a 3.53 s gap without a frame (`worstMs 3527.96`) in
  `Chose_rm`, right after the slot click of `hs_select_character` (the last
  lines before the `FREEZE` line in its `out-tail.txt` are the `Chose_rm`
  menu listing the click reads, and the Play click came about three seconds
  after the report), with `inHook none`, `inMod none` and `modsEpisode []`:
  no ForgePact code ran on the frame thread during the gap. RAM was 6578 MB
  free of 32689 MB. The slot click loads the save and opens the character
  panel in the same room, so no room change followed the gap and the
  room-change lead that keeps a zone load from being a freeze had nothing to
  see. The game was loading, and the monitor called it a freeze.
- **The `frame` row's 4084 ms was our start-up setup, not the load.** The
  bundle's `modsLastMinute` read `frame 2.302 / 4084.412` and step 0's
  `incident stat` `frame 1.24 / 4084.41`; by step 2 the row was `frame 0.00
  / 0.19`, outside the minute window. That 4084 ms is the one-time setup at
  start-up (`LoadConfig` and `InstallHook`, which installs every mod's hooks),
  which ran inside `frame`'s scope with no row of its own, so a minute later
  it read as the cause of a load it had nothing to do with. The bundle's
  `out-tail.txt` shows the installers still logging `HOOK INSTALLED` lines
  after the monitor's 10 s previous-session check. Which installer takes
  the time, and whether the 4100.7 ms worst frame is the setup frame or the
  town load (both fall in a room change's grace), was not established.
- **The fix.** A gap that begins in a menu room (the login, main menu and
  character rooms, `Chose_rm` and `Main_Menu_rm` among them, named from the
  SDK) is a load, never a freeze; and the start-up setup is timed under its
  own `setup` row and prints one `incident: setup` line with its parts'
  milliseconds. Both are under "Decisions". Live 3 runs the same character
  load again to check that no report is written, and measures the setup.

Live 3, 2026-10-02 (capture: the workorder's
`forgepact-76-incident-report-live-3.md`, kept with the hub's local workorder
files). The shipping DLL built from ForgePact `092229a`, the commit that
added the menu-room rule and the `setup` row (SHA-256 `a2a7fa59...cc2b9d097`,
equal to the lease's hash and to the installed copy), ran the same character
load as Live 2: launch, main menu, slot 2, then 60 s in town with no input,
an ordinary close and a relaunch without a character. All eight required
checks passed; `char-load-gap` records values only, and its load path was
not exercised.

- **Installed and answering.** `ping` answered `pong (YYTK 4.0.1)`, and
  after the load the first `incident stat` line read `incident: frames
  4696 | baseline 6.9 ms | worst 2838.7 ms (not judged) | worst judged 31.1
  ms | slow judged frames 0 | watching yes | grace no | focus yes | window
  yes | menu no | in-hook none | in-mod none`. Observed live.
- **The installer tag.** `hooks tagged 19, untagged 0 | report write errors
  0` at the main menu, after the load and after the wait. Observed live.
- **The menu flag.** At the main menu, before any click, the first line
  ended `| focus no | window yes | menu yes | in-hook none | in-mod none`;
  after the load it read `| menu no |`. Observed live.
- **The start-up setup has its own row and its own line.** The log carried
  `incident: setup 2491.1 ms at frame 301: config 0.3 ms, hooks 2490.8 ms
  (InstallCustomForgeItemHooks+InstallItemTruth 2486.2 ms,
  CaptureAngelicScriptCode 3.2 ms, LoadCustomForgeEntries 1.2 ms)`, and the
  relaunch's line read `incident: setup 2701.2 ms at frame 301`. At the
  main menu the first line's worst was `2507.5 ms (not judged)`, the setup
  frame, and the fourth line began `setup 2.44 / 2491.30, frame 0.01 /
  8.10`; after the load it began `setup 0.53 / 2491.30, frame 0.02 / 8.10`.
  The `frame` row no longer carries the setup: Live 2's `frame 1.24 /
  4084.41` is now `setup` at 2491.30 and `frame` at 8.10. After the minute
  had passed the row was gone from the table (`frame 0.00 / 0.16, ipc 0.00
  / 0.14, ...`). Observed live.
- **A normal session writes nothing.** After the load and again after the
  62 s wait the second line read `incident: episodes 0, held back 0,
  ignored near a room change or unfocused 4 | reports written 0`, with
  worst judged 31.1 ms and slow judged frames 0, and `reports\` held only
  the two folders from Live 1 and Live 2 (`20261002-172932_perf`,
  `20261002-190757_freeze`). No `FREEZE` and no `CRASH` line followed the
  session's banner. Observed live.
- **A character-load gap in a menu room: not observed live; harness
  evidence only.** The largest gap of the session was the 2838.7 ms worst
  frame after the load, under the 3 s freeze threshold, so the log carried
  no `FREEZE` line and no `in a menu room: a load, not reported` line. Live
  2's 3.53 s gap in `Chose_rm` did not recur at that length, so the menu
  rule's load path was not exercised live; it rests on the harness's
  `freeze-menu-room` and `freeze-menu-room-never-ends` scenarios.
- **The clean-shutdown marker, and no crash after it.** The ordinary close
  left `==== clean shutdown ====` as the last line of `out.txt`, and the
  relaunch logged `incident: the previous session shut down cleanly`, with
  no `CRASH` line and no crash folder. Observed live.
