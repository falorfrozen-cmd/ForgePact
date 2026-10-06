# Setup stall - what the one-time start-up setup spends its 2.5 s on (ForgePact #151)

Status (2026-10-06): **static reading and offline measurement written; the
instrument is built; Live 1 has not run.** No fix is chosen. The fix route is
the owner's decision after Live 1, and it goes to a follow-up workorder.

## Question

The plugin's one-time setup (the `if (!g_Setup && fc > 300)` block in
`FrameCallback`: `LoadConfig` and `InstallHook`) holds the main menu's frame
for about 2.5 s on every launch. Which part of it takes that time: resolving
the hooked names, the detour each hook install makes, writing the log lines,
or something outside the hook installers? The answer decides what a fix can
change, and so has to be measured in the game before anything is changed.

## Existing measurements

All from the #76 (incident reports) live sessions on the owner's machine,
recorded in [incident-report.md](incident-report.md) § "Live results". Each is
measured.

| Session | What was measured | Source |
| --- | --- | --- |
| Live 1 of #76, 2026-10-02 | `PERF hitch 508 ms frame \| baseline 6.9 ms \| room Town_01_rm \| top ipc 495.0 ms/frame`, right after the panel's batch of settings commands; the bundle's `modsEpisode` started with `frame` 495.161 ms and `ipc` 495.036 ms | incident-report.md, Live 1, "A heavy scene produces a PERF report" and "Density attribution" |
| Live 2 of #76, 2026-10-02 | the `frame` row's worst over the last minute was 4084.41 ms, which was the setup running inside `frame`'s scope (`frame 2.302 / 4084.412` in the bundle, `frame 1.24 / 4084.41` in `incident stat`); first `incident stat` line `worst 4100.7 ms (not judged)` | incident-report.md, Live 2, "The `frame` row's 4084 ms was our start-up setup" |
| Live 3 of #76, 2026-10-02 | `incident: setup 2491.1 ms at frame 301: config 0.3 ms, hooks 2490.8 ms (InstallCustomForgeItemHooks+InstallItemTruth 2486.2 ms, CaptureAngelicScriptCode 3.2 ms, LoadCustomForgeEntries 1.2 ms)`; 2701.2 ms at the relaunch; `hooks tagged 19, untagged 0` at the main menu | incident-report.md, Live 3, "The start-up setup has its own row and its own line" and "The installer tag" |

So the time is in one lap: `InstallCustomForgeItemHooks+InstallItemTruth`,
2486.2 ms of the 2490.8 ms `hooks` value in Live 3. Name resolution alone
(`CaptureAngelicScriptCode`, which resolves two scripts by name) took 3.2 ms in
the same setup.

## Static reading: what one MmCreateHook costs

Static reading, in our own words, of Aurie v2.0.2 (`AurieFramework/Aurie`,
tag `v2.0.2`, which is the version `tools/fetch_toolchain.py` pins), files
`Aurie/source/framework/Memory Manager/memory.cpp` and
`Aurie/source/framework/Early Launch/early_launch.cpp`. No newer Aurie tag
existed when this was written (checked 2026-10-06). Not yet measured in the
game; Live 1 below is the measurement.

- **What the release setup installs** (reading `ModuleMain.cpp`).
  `InstallCustomForgeItemHooks` always runs, because the built-in signature
  entries (Miner's Helmet, Tyrant's Crown, Headhunter) are always added. It
  makes 5 `HookOneScript` calls (`CreateItemNew`, `CreateItemInit`,
  `GenerateItemSpecialStats`, `GetRuneword`, `GenerateItemRandomStats`), and
  the Miner's Helmet's affix adds 2 more through `InstallForgedTooltipHooks`.
  `InstallItemTruth` installs only while
  `%LOCALAPPDATA%\Hero_Siege\itemtruth\capture.request` exists (it has on the
  owner's machine since 2026-09-27); it then adds 3 `HookOneScript` and 8
  `HookBuiltin` calls. That is 18 installs with item truth on and 7 without.
  UNVERIFIED: Live 3 counted `hooks tagged 19` at the main menu, so one tagged
  install is not accounted for by this reading.
- **Each install calls `MmCreateHook` once** (`HookOneScript` on the first
  install only, `HookBuiltin` every time).
- **`MmCreateHook` freezes the whole process around the hook.** It suspends
  every other thread of the process, creates the hook through SafetyHook, and
  resumes the threads again. Both the freeze and the resume walk the process's
  threads the same way: they take a `CreateToolhelp32Snapshot` with
  `TH32CS_SNAPTHREAD`, then step through every entry and act only on the
  entries this process owns (open the thread, suspend or resume it, close the
  handle).
- **That snapshot covers every thread on the system, not only the game's.**
  `TH32CS_SNAPTHREAD` ignores the process id it is given, so its cost grows
  with the machine's total thread count (browsers, launchers, the IDE), not
  with the game's. Each install therefore pays for two system-wide thread
  snapshots and two walks over them.
- **SafetyHook itself freezes nothing.** Creating the inline hook does a few
  `VirtualQuery`/`VirtualProtect` calls and a near allocation; the thread
  freeze is Aurie's, around it.

## Offline measurement

Planning machine, 2026-10-06, no game running. One system-wide
`CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD)` plus a full
`Thread32First`/`Thread32Next` walk, timed 20 times through Python `ctypes`:
min 46.2 ms, median 47.8 ms, max 52.0 ms, with 5909 threads on the system.
The Python loop's overhead is included. Measured, outside the game.

The prediction Live 1 tests: about 2 x 48 = 95 ms per install, so 18 installs
come to about 1.7 s, against the 2486.2 ms Live 3 measured. The remaining gap
of about 0.8 s is not explained by this reading (the machine's load at the
time, the game's own thread count, or another part of the installers). That
gap is why the instrument below times every part of an install, not only the
detour.

## Instrument

Built in this workorder; ships in both builds. It changes no install: every
hook keeps both routes, and the order, count and frame of the installs are
unchanged.

- **What is counted.** An *install* is one call of `HookOneScript` or
  `HookBuiltin`. A *detour* is one `MmCreateHook` call inside them, counted
  whether or not it succeeds. `HookOneScriptTable`, `InstallSlotHook`, the
  population detours and the `ExitProcess` marker hook are not counted; their
  time falls into `outside installers` when they run inside the setup.
- **The parts of an install.** *resolve* is the time inside
  `GetNamedRoutinePointer`, *detour* the time inside `MmCreateHook`, *log* the
  time inside `Out(`, and *rest* the installer's remaining time (the thunk
  table, the address check, the table write, building the log text).
  *outside installers* is the setup line's existing `hooks` value minus the
  total time of the installs made in the setup window, never negative. It
  covers whatever `InstallHook` does outside the two installers.
- **The setup window** is `SetupLap`'s: from `SetupLapStart()` until
  `SetupSlowest` ends the laps. An install outside it counts only toward the
  session totals. In the research build the window also covers the co-op
  auto-start and the population profile's installs, which run after
  `InstallHook` but before the line is printed; the release build has
  neither. One install comes before the window in every build:
  `ModuleInitialize` calls `InstallHeadLabelHook`, which installs
  `DrawHudBuffs` through `HookOneScript` (`fp_hh_hudlabels`) at load. Static
  reading: that is probably the 19th tagged hook Live 3 counted beside the
  18 the setup installs; the session line's `installs since load` against
  the setup line's `installs` shows it.
- **The clock** is `ForgePact::Incident::Qpc()`, on the frame thread only.
- **The setup line**, printed once per launch right after the existing
  `incident: setup` line:
  `incident: setup installs <n>, detours <d>: resolve <ms> ms, detour <ms> ms (worst <ms> ms <hook id>), log <ms> ms, rest <ms> ms, outside installers <ms> ms`.
  The five parts add up to the `hooks` value.
- **`incident setup`** prints three lines: the setup line (or
  `incident: setup installs not measured yet` before the setup has run), the
  session's totals
  (`incident: installs since load <n>, detours <d>, detour <ms> ms total, worst <ms> ms <hook id>`),
  and one timed thread snapshot, the positive control:
  `incident: thread snapshot <ms> ms, <t> threads system-wide, <p> in this process`.
  The snapshot repeats Aurie's walk (the system-wide snapshot and every
  entry) and counts the entries the game owns; it opens, suspends and resumes
  nothing, and runs only on this command, on the frame thread.
- **Where it lives.** `plugin/include/ForgePact/IncidentMonitor.hpp`
  (`InstallParts`, `InstallCost` and its `g_InstallCost`, `InstallTimer`,
  `ThreadSnapshot()`, `ThreadSnapshotLine()`), game-independent like the rest
  of the header; `ModuleMain.cpp`'s `HookOneScript` and `HookBuiltin` (an
  `InstallTimer` per call and a clock read around the lookup, the detour and
  each log line; the guards, their order and the one detour are unchanged),
  `SetupLapStart`/`SetupSlowest` (the window), the setup block (the line) and
  `IncidentCommand` (the verb; usage `incident stat|setup`).
- **Tests.** `tests/incident_monitor_harness.cpp`'s `setup-cost-line`,
  `setup-cost-outside-setup` and `setup-cost-sums` run the accounting on the
  harness's controlled clock, so each value is exact; `thread-snapshot` runs
  the probe on the real clock with lower bounds only (four idle threads
  started raise this process's count by at least four). The first run, on
  the developer machine, read `incident: thread snapshot 30.8 ms, 5534
  threads system-wide, 5 in this process`. `tests/test_incident_monitor_contract.py`
  pins the wiring: both installers time their three parts through
  `ForgePact::Incident::Qpc()`, the cost line follows the existing one, the
  verb answers `setup`, and the probe never opens, suspends or resumes a
  thread and is called only from the verb.

## Live 1

Not run yet. The procedure is the workorder's Live procedure 1: Case A (item
truth requested), Case C (an install after the setup, same launch, through
`dropmult relic 2`) and Case B (item truth not requested, second launch).

## Fix routes

Pending the owner's decision after Live 1. The options, with the risk each
carries:

- **Install fewer hooks at the setup.** Install item truth's text hooks only
  when a capture actually starts (at the first recorded tooltip, since
  `TipCaptureBegin` starts recording only from our own `DrawInventoryItemV2`
  hook). Nothing is missed, but a hitch of roughly a second moves to the
  player's first tooltip hover in play.
- **Make each detour cheaper without changing Aurie.** For example one freeze
  around a batch of detours instead of one per detour. Any such route needs
  a hook API that Aurie v2.0.2 does not expose, so it means a change to
  Aurie or a vendored SafetyHook, which this workorder rules out and a
  follow-up would have to justify.
- **Defer or spread the installs over frames: rejected for now.** A hook
  installed late misses the calls made before it (`CreateItemNew` and the
  rest must be in place before the first item is built, and the save loads at
  the slot click in `Chose_rm`), and installing `DropRelic` while character
  selection runs stalled the runner for about a minute (the ForgePact guide's
  Known Limitations).
- **Install from a worker thread: rejected.** `MmCreateHook` freezes the game
  thread from any thread, and table writes and YYToolkit calls off the frame
  thread are not safe.
- **Leave it.** The stall happens once per launch, in the main menu, and Live 3 of #76
  wrote no report for it (`reports written 0`). The player still sees the
  menu hold for about 2.5 s on every launch.

The measured per-detour cost goes here after Live 1.
