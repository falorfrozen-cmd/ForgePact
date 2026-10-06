# Setup stall - what the one-time start-up setup spends its 2.5 s on (ForgePact #151)

Status (2026-10-06): **measured in Live 1.** The detour is 97.5% of the
setup's `hooks` time, and each detour costs about two system-wide thread
snapshots, as the static reading predicted. The owner chose fix route (a),
patching Aurie's freeze; it goes to a follow-up workorder (see "Fix routes").

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
existed when this was written (checked 2026-10-06). Live 1 below measured
the cost this reading predicts; the freeze's internals (which calls it makes,
in what order) remain a reading.

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
  Live 3 counted `hooks tagged 19` at the main menu, one more than this
  reading; Live 1 measured both counts (18 and 7 setup installs) and showed
  the 19th is `DrawHudBuffs`, installed at load (see "Live 1").
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

Live 1 answered it: in the game one snapshot took a median 32.8 ms (not 47.8;
the offline figure includes Python's loop), a detour 68.7 ms, and the whole
setup 1268.3 ms rather than Live 3's 2490.8 ms. The detour made up 97.5% of
it, and every other part together 31.3 ms, so no part of the installers is
missing from the reading. Why Live 3's setup took twice as long is not
established; the setup's length differs between sessions (see "Live 1").

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
  `DrawHudBuffs` through `HookOneScript` (`fp_hh_hudlabels`) at load. That
  is the 19th tagged hook Live 3 counted beside the 18 the setup installs:
  measured in Live 1, where `installs since load` read 19 against the setup
  line's 18 at the main menu, with `fp_hh_hudlabels` as the session's worst
  detour.
- **The clock** is `ForgePact::Incident::Qpc()`, on the frame thread only.
- **The setup line**, printed once per launch right after the existing
  `incident: setup` line:
  `incident: setup installs <n>, detours <d>: resolve <ms> ms, detour <ms> ms (worst <ms> ms <hook id>), log <ms> ms, rest <ms> ms, outside installers <ms> ms`.
  In the player build the five parts add up to the `hooks` value (Live 1:
  0.1 + 1237.0 + 4.9 + 20.9 + 5.4 = 1268.3 ms). In the research build they
  can add up to more: an install made by the co-op auto-start or the
  population profile falls inside the window but after `hooks` was measured,
  and `outside installers` is then held at 0 rather than going negative. The
  same holds when `InstallHook` throws before its end is recorded.
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

2026-10-06, the owner's machine, the player build
(`BloodPactPlugin_ship.dll`, SHA-256 `c700a482...0efc85`, equal to the
installed DLL), slot 14 for Case C. Capture:
`.claude/workorders/forgepact-151-setup-stall-live-1.md` in the hub (a local
workorder file, not committed). The validity checks passed: the DLL hash,
`pong (YYTK 4.0.1)` in both launches, the existing `incident: setup` line once
per launch with its usual shape, the new line right after it, and
`reports written 0` at the main menu in both launches. Every number below is
measured, read from `out.txt` and the commands' replies.

**Case A: item truth requested (launch 1).** `capture.request` existed.

| Value | Measured |
| --- | --- |
| existing line | `incident: setup 1268.7 ms at frame 301: config 0.3 ms, hooks 1268.3 ms (InstallCustomForgeItemHooks+InstallItemTruth 1264.5 ms, CaptureAngelicScriptCode 3.3 ms, LoadCustomForgeEntries 0.3 ms)` |
| installs, detours | 18, 18 |
| resolve | 0.1 ms |
| detour | 1237.0 ms, worst 80.0 ms (`fp_customforge_new`) |
| log | 4.9 ms |
| rest | 20.9 ms |
| outside installers | 5.4 ms |
| detour share of `hooks` | 97.5% |
| per detour | 68.7 ms |
| session at the main menu | `installs since load 19, detours 19, detour 1324.9 ms total, worst 87.9 ms fp_hh_hudlabels` |
| thread snapshot, 3 runs | 33.5, 32.8, 32.6 ms (median 32.8); 6012 threads system-wide, 76 in this process |
| per detour / median snapshot | 2.10x |

**Case B: item truth not requested (launch 2).** `capture.request` renamed
aside; no `item truth: capturing` line and no `draw_text*` hooks.

| Value | Measured |
| --- | --- |
| existing line | `incident: setup 483.4 ms at frame 301: config 0.4 ms, hooks 483.0 ms (InstallCustomForgeItemHooks+InstallItemTruth 479.4 ms, CaptureAngelicScriptCode 3.1 ms, LoadCustomForgeEntries 0.3 ms)` |
| installs, detours | 7, 7 |
| resolve | 0.0 ms |
| detour | 463.0 ms, worst 67.1 ms (`fp_customforge_stats`) |
| log | 1.9 ms |
| rest | 14.2 ms |
| outside installers | 3.9 ms |
| per detour | 66.1 ms |
| `hooks` against Case A | 38.1% |
| session at the main menu | `installs since load 8, detours 8, detour 539.6 ms total, worst 76.6 ms fp_hh_hudlabels` |
| thread snapshot, 3 runs | 31.9, 32.0, 38.5 ms (median 32.0); about 5733 threads system-wide, 77-78 in this process |
| per detour / median snapshot | 2.07x |

**Case C: installs after the setup (launch 1, in town).** Before:
`installs since load 19, detours 19` (D0 = 19). `dropmult relic 2` installed
20 drop hooks (`DropRelic` through `DropOreMaterials`, 20 `HOOK INSTALLED`
lines). After: `installs since load 39, detours 39, detour 2777.2 ms total,
worst 103.4 ms bp_citemd`, so the 20 cost 1452.3 ms, 72.6 ms each. The setup
line was unchanged. The monitor judged that frame 1546.5 ms, attributed it to
`ipc` (`ipc 0.23 / 1542.58`), and wrote one PERF report
(`20261006-133656_perf`). `dropmult relic 1` answered `dropmult relic -> 1`.

What this shows:

- **The detour is the setup's time** (measured): 97.5% of `hooks` with item
  truth on. Name resolution (0.1 ms), the log (4.9 ms), the rest of the
  installers (20.9 ms) and everything outside them (5.4 ms) are small.
- **One detour costs about two system-wide thread snapshots** (measured):
  68.7 ms against a 32.8 ms snapshot in Case A, 66.1 against 32.0 in Case B.
  The ratio matches the static reading's two snapshots per `MmCreateHook`.
  The walk inspects about 6000 entries to act on the game's 76.
- **Item truth's 11 text hooks are about 62% of the setup** (measured): 18
  detours and 1268.3 ms with it, 7 and 483.0 ms without.
- **The setup's length varies between sessions** (measured): Live 3 read
  2490.8 ms, the session just before this one in the same `out.txt` (an
  older build) 1533.7 ms, this one 1268.3 ms, on the same machine with the
  same installs. That the variation is the snapshot's cost, which grows with
  the machine's thread count and load, is an inference: no earlier session
  timed a snapshot.
- **On-demand installs pay the same price in play** (measured): 20 hooks at
  `dropmult relic 2` held one frame 1.5 s in town and produced a PERF
  report. #76 Live 1's `top ipc 495.0 ms/frame` hitch after the panel's
  batch of settings commands is very likely the same cost; it was not
  measured by part at the time.
- **Not observed:** the freeze's own breakdown inside `MmCreateHook`
  (snapshot against suspend and resume). The instrument times the call as a
  whole; the per-snapshot attribution rests on the ratio and the static
  reading.

## Fix routes

The measured cost each route works against: **one detour costs 66-73 ms on
the owner's machine** (68.7 ms at the setup with item truth on, 66.1 ms
without, 72.6 ms for the 20 on-demand drop hooks in town), about two
system-wide thread snapshots of about 32 ms each. Everything else an install
does costs about 1.4 ms (Case A: resolve, log and rest came to 25.9 ms
over 18 installs, most of it the rest and the log).

**Chosen: (a), patch Aurie's freeze** (the owner, 2026-10-06, after Live 1).
A follow-up workorder plans it. The options, with the risk each carries:

- **(a) Patch Aurie's freeze to walk only the game's threads.** Chosen. The
  freeze and the resume would stop paying for the whole system's threads (about
  6000 entries in Live 1, 76 of them the game's), so the cost falls at the
  root for the setup and for every on-demand install alike. It keeps
  `MmCreateHook`, both routes and every caller unchanged. It means a new
  documented patch series and a rebuilt, pinned `AurieCore.dll` that players
  install, as `third_party/yytoolkit/` does for YYToolkit, and it touches
  thread suspension, so it is planned as concurrency work.
- **(b) ForgePact's own detour installer through a vendored SafetyHook,**
  with no per-call freeze. Every hook would change how it attaches, and
  Aurie would no longer track the hooks. Also concurrency work. Not chosen.
- **(c) Install fewer hooks at the setup.** Install item truth's text hooks
  only when a capture actually starts (at the first recorded tooltip, since
  `TipCaptureBegin` starts recording only from our own `DrawInventoryItemV2`
  hook). Nothing is missed, but Live 1 puts those 11 hooks at about 785 ms
  (1268.3 against 483.0 ms), a hitch that would move to the player's first
  tooltip hover in play. It does nothing for on-demand installs. Not chosen.
- **(d) Defer or spread the installs over frames: rejected.** A hook
  installed late misses the calls made before it (`CreateItemNew` and the
  rest must be in place before the first item is built, and the save loads at
  the slot click in `Chose_rm`), and installing `DropRelic` while character
  selection runs stalled the runner for about a minute (the ForgePact guide's
  Known Limitations).
- **(e) Install from a worker thread: rejected.** `MmCreateHook` freezes the
  game thread from any thread, and table writes and YYToolkit calls off the
  frame thread are not safe.
- **(f) Leave it.** The setup's stall happens once per launch, in the main
  menu, and neither Live 3 of #76 nor Live 1 wrote a report for it
  (`reports written 0`). The player still sees the menu hold for 0.5 to
  2.5 s on every launch, and an on-demand install in play still holds a
  frame for about 70 ms a hook (1.5 s for `dropmult relic 2`). Not chosen.
