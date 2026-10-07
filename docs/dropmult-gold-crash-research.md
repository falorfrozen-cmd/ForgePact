# Gold at x100 - what ended the game on 2026-10-04 (ForgePact #173)

This doc carries no decompiled script text: the static reading below is the
mechanism in words, and every quoted log line is ForgePact's own output.

## Status

2026-10-07: **`crash-route: not-reproduced`.** Both sessions ran, and nothing
died during play in either. The research build ran x100 gold for about 9
minutes and 100 coins, with and without `gambaprobe hook`'s detours armed and
across five room changes; the player build ran x100 for 5 coins. The only
deaths were the deliberate crash (the instrument's positive control, which
every instrument caught) and the two graceful closes, each of which exited
`0xC0000409` after `==== clean shutdown ====` (guide Known Limitations item
25). What ended the game on 2026-10-04 is still not established. #173 stays
open: a game hs-drive launched that dies now leaves its exit code, and an
access violation also leaves a dump, an Application Error record and, on the
research build, a `crashwatch` trap line (measured once, for one access
violation in our DLL). A fast-fail (`0xC0000409`) or an external termination
never reaches the trap, so it would leave the heartbeat trail and the exit
code only. The panel's launch takes the same `launch_game` route but was not
exercised in these sessions. Details: "Results".

2026-10-06: the research phase. The game vanished during a research-build
session on 2026-10-04, seconds after `dropmult gold 100`, and left no dump, no
event-log record and no exit code. This phase added the instruments that would
have answered "what ended it" and planned one research-build session (Live
procedure 1) and one player-build session (Live procedure 2) that separate the
candidates. What was built:

- `launch_game` (`src/offline_launcher.py`) now starts the game with
  `CREATE_DEFAULT_ERROR_MODE`, so a crash gets Windows' own dump and
  Application Error record whoever launched it.
- hs-drive (the hub's `tools/hs_drive_mcp`) holds a handle on every game it
  launches and reports each one's exit code (hub `docs/tools/hs-drive-mcp.md`
  § "Exit codes").
- `crashwatch` (research build only): a heartbeat, enter/done breadcrumbs
  around both gold drop originals, a logging-only exception trap, and a
  deliberate crash as the instrument's positive control (see "Instrument").

The fix, if the sessions find one is needed, is a separate workorder planned
from the route the record round writes.

## Evidence

The owner's live session for #134 on 2026-10-04: research DLL `aa5f7d03...ef2f`,
slot 14 "Sorak", launched by hs-drive with no panel running.

- **What ran, in order.** In town: `debuglog`, `gambaprobe hook` (37 rows: the
  gold scripts `GetGoldAmount`, `GoldOperationPending`, `GetGoldCounterHash`
  and `PickUpGoldCheck`, the variable scripts, the slot machine's events, and
  the builtins `irandom`, `irandom_range`, `random`, `random_range`, `choose`
  and `instance_destroy`), then two `gambaprobe spawn`s. Then a Hell zone of
  Outskirts of Inoya / Misty Swamp, zone level 243: `dropmult gold 10` logged
  `first coin 31 -> 310` and coins 1/8 to 8/8 (310 to 1160). Then
  `dropmult gold 100` (`dropmult gold -> 100`), a `satmods POLL` line, and
  `ping` -> `pong (YYTK 4.0.1)` at 17:31:14 local. Then the game was gone.
- **What the plugin wrote after the ping:** nothing. No x100 coin line, no
  `==== clean shutdown ====`, and no `STALL` line (the research build's
  watchdog prints one once a frame has stalled 3000 ms).
- **File times.** `itemdrops.jsonl` (every hooked drop, research build) was
  last written 17:30:55 and `keychoice.txt` 17:30:53, so no hooked drop
  happened for at least 34 s before the death. `itemtruth\status.json` was
  written 17:31:25, its 30 s heartbeat. The game rewrote `herosiege13.hss` at
  17:31:29. The session's only other save, 17:28:37, came two seconds before
  its last `Room Start` line in the game log (17:28:39); no `Room Start`
  followed the last save.
- **What Windows kept:** no dump in `%LOCALAPPDATA%\CrashDumps` and no
  Application Error event. The next launch's incident monitor printed
  `CRASH previous session ended without a clean shutdown | exit unknown |
  module unknown`; that bundle has since been pruned (ten are kept). The
  plugin's own lines of the session are still in the game's `bin\bp_ipc\out.txt`.

### What the evidence rules in and out

- **No x100 coin reached the hook.** `Hook_DropGold` logs the coin line
  (`LogGoldCoin`) before it calls the original, the first coin after any
  multiplier change always logs (the coin-log count resets), and `Out` opens,
  appends and closes `out.txt` for each line, so a written line survives a
  crash. The harness case `baseline/coin_line_precedes_original` pins that
  ordering. So the scaled-coin path did not run before this death: "an x100
  coin crashed the game" is not supported by this session. It is not excluded
  for a session where x100 coins exist.
- **The owner was not killing** (no drop for 34 s). Picking up x10 coins
  already on the ground, or changing zones (the save timing), are possible;
  neither is established.
- **No frame stall of 3 s or more preceded the death**, unless the game died
  within 3 s of one.
- **x100 has run without a crash before:** Live 2 of #77 (player DLL 2.0.0,
  same character) dropped and picked up x100 coins (44 -> 4400, +12870 gold).
  That was the player build with no research hooks armed.
- **The candidates** Live procedure 1 separates:
  1. the x100 path, at a drop or at a pickup;
  2. `gambaprobe`'s armed detours: gold scripts on the pickup path, and the
     builtin `instance_destroy`, which a room's end reaches for every instance;
  3. a room change under either;
  4. a game crash unrelated to ForgePact;
  5. external termination (nothing points to it).

  The research build's always-on extras (`BP_LOGDROP`, the stall watchdog) are
  absent from the player build, which Live procedure 2 covers.

### Why the last crash left no dump

The guide's Known Limitations item 25, "A missing dump is not a clean exit": a
game started by a process whose error mode has `SEM_NOGPFAULTERRORBOX`
inherits it, and Windows then writes no dump and logs no Application Error
event, even for a `0xC0000409` abort. hs-drive launches through
`launch_game`, whose game `Popen` passed no creation flags until this phase.
Measured while planning: a Python started from the agent's shell reads an
error mode of `0x3`. hs-drive's own error mode was not measured (Claude Code
starts it from `.mcp.json`). This machine does write Hero Siege dumps to
`%LOCALAPPDATA%\CrashDumps` without a `LocalDumps` registry key (two exist,
from 2026-10-02 and 2026-10-03, and the WER policy `Disabled` is 0), so WER
works once the error mode lets it.

The exit code was lost because nothing held a handle on the process: the
panel's exit watch (`open_exit_handle`, `exit_code_of`, `exit_code_text` in
`src/forgepact.py`) runs only while the panel does, and this session had no
panel. The precedent for the flag is `tools/itemtruth_memrun.py` (PR #97),
whose game spawn already passed `CREATE_DEFAULT_ERROR_MODE`.

## Static reading

Read locally on 2026-10-06 in the named Ghidra project (hub `AGENTS.md`
§ "Check for a Named Ghidra Project"); the output stays on that machine and is
findable with the hub's `py -3 tools/decomp_index.py has PickUpGoldCheck`.
Each point is a static reading, in my own words.

- **`PickUpGoldCheck` checks its hash first.** It compares its first argument
  with the game's current counter hash (`GetCounterHash`). When they differ it
  reports the client (`ReportClient`) and returns false, without touching the
  balance. A second `ReportClient` call site exists in it; the condition that
  reaches it was not read.
- **The bodies of `PickUpGoldCheck` and `ReportClient` themselves make no
  call that ends the game.** `PickUpGoldCheck`'s named callees are
  `GetCounterHash`, `ReportClient`, `DecryptStringApi`, `string_sha256`,
  `ShowDebug`, `GetCurrenciesSetLogParams` and `ApiRequest`; its 58 unnamed
  callees were not read. `ReportClient`'s named callees are
  `GetGameStateReport`, `ApiRequestRegion`, `string_sha256` and
  `DecryptStringApi`: it builds a state report and sends it online (hub
  `docs/RUNTIME_DATA_MODELS.md` § 7.5). It has about 200 direct call sites,
  many of them in `CheatDetection`.
- **Of those named callees, only `GetCounterHash` and `ShowDebug` were read.**
  `ApiRequest`, `ApiRequestRegion`, `GetGameStateReport`, `DecryptStringApi`,
  `GetCurrenciesSetLogParams` and `string_sha256` were not, so whether a
  pickup that fails the game's own check can end the game through them is not
  established.
- **`DropGold` and `DropMonsterGold` are not named in this project** (their
  `symbols.csv` rows point outside the exe, because that dump ran with table
  hooks in place), so #77's reading of them (hub `RUNTIME_DATA_MODELS.md`
  § 13.10) stands without a re-read. `Coin_obj`'s two Create closures were
  decompiled but not read.
- **Why no more reading now.** #173's session had no x100 coin, so `DropGold`'s
  body cannot explain that death. If a session's crash names an offset inside
  `Hero_Siege.exe`, reading the function there is the next workorder's first
  step.

## Instrument

Three pieces, each answering a part of "what ended it" that the 2026-10-04
session could not.

### The launch: the default error mode

`launch_game` starts the game with `subprocess.CREATE_DEFAULT_ERROR_MODE`, so
neither the panel's launch nor hs-drive's passes a parent's suppressed error
mode on. A crash then gets a dump in `%LOCALAPPDATA%\CrashDumps` and an
Application Error record, as it would for a player who started the game from
Steam. Tests: `tests/test_offline_launcher.py`.

### hs-drive: the exit code of every game it launched

`hs_launch` opens a handle on the PID it started (query-limited-information
plus synchronize, the panel's `open_exit_handle` shape) and reports
`exit_watch` (`held`, or `unavailable (<reason>)`). `hs_status` and
`hs_stop_game` report `exits`, oldest first, one `{"pid", "exit_code"}` per
launched PID that has ended, the code written `0x%08X`. "Ended" is asked of the
handle (signalled), never read from the code, since 259 is also `STILL_ACTIVE`.
The launch change reaches hs-drive through `launcher_bridge`, which imports
`offline_launcher` when the server starts, so the server has to be reconnected
before a live session. Detail: hub `docs/tools/hs-drive-mcp.md` § "Exit codes".

### `crashwatch` (research build only)

`crashwatch on|off|status|crash confirm`, dispatched from
`HandleLiveOneResearchCommand`; the player build answers `command unavailable
in player build: crashwatch` (the refusal names the verb only, measured Live 2,
2026-10-07). Every form but `crash` replies with the
status line: `crashwatch: on|off, heartbeats=<n> gold-crumbs=<n>
exceptions=<n> lines=<n> of 50000 (<k> after the cap) -> bp_ipc\crashwatch.txt`.
Any other argument replies with the usage.

- **The file.** The session's first `on` truncates `bp_ipc\crashwatch.txt` and
  writes a header line (`crashwatch on <UTC time> pid=<pid> ...`). Every line
  is on disk before game code runs again, so a death straight after loses no
  written line. At most 50,000 lines a session, and 32 exception lines; later
  ones are counted, not written.
- **The heartbeat**, every 15 frames while on, starts `hb `: the UTC time to
  the millisecond, the frame count, the room name, the `Coin_obj` count
  (resolved by SDK name once, as `lootcensus` does; `?` when unresolved), the
  gold multiplier in force, and the `DropGold` / `DropMonsterGold` hook call
  counts. The last heartbeat before a death bounds when it happened and in
  which room.
- **The gold breadcrumbs**: `gold DropGold enter #<n> x<m> a4 <handed> ->
  <passed>` or `gold DropMonsterGold enter #<n> x<m>` immediately before the
  original, and `gold <script> done #<n>` after it (`DropManager.hpp`'s
  `FP_GOLD_CRUMB_*` points, compiled out of the player build). A game that
  dies inside a gold original leaves an `enter` with no `done`. They fire only
  while ForgePact's drop hooks are installed, which a gold multiplier above 1
  (or `goldtrace on`) does.
- **The exception trap**: a first-registered vectored exception handler that
  logs only the fatal kinds (access violation, stack overflow, illegal or
  privileged instruction, integer divide by zero, array bounds, heap
  corruption) as `exception 0x<code> at <module>+0x<offset>
  thread=<game|other> in-hook=<id|none> in-mod=<mod|none>
  game-original=<yes|no>`, the tags taken from the incident monitor's
  accounting (they describe the game thread, whichever thread faulted).
  `<module>` is the file name the DLL was loaded under: an installed research
  DLL reads `BloodPactPlugin.dll`, not `_rel`. It always continues the search, so
  it changes nothing about how the game handles the exception. It writes with
  Win32 file calls from a fixed buffer, with no allocation, no C++ stream, no
  shared log lock and no YYToolkit call. `off` removes it.
- **The positive control.** `crashwatch crash` without `confirm` refuses,
  naming the word, and does nothing. `crashwatch crash confirm` writes
  `crashwatch: crashing on purpose (positive control)` to `out.txt` and to the
  file, then causes an access violation on the game thread outside every
  `try`. It does not need `on`. The crash proves, in one session, that the trap
  logs, that Windows dumps and records it, and that hs-drive reads the exit
  code: an instrument that has produced those once is one whose silence later
  means something.

Tests: `tests/test_crash_watch_contract.py` (the command's shape and that it
stays out of the player build) and `tests/test_drop_gold_behavior.py` with
`drop_gold_harness.cpp` (where the breadcrumbs sit around each original, and
that the coin line precedes the original).

## Live procedure 1

Research build (`plugin_build/BloodPactPlugin_rel.dll`), one sitting, every
candidate in one session, slot 14 "Sorak". The owner reconnects hs-drive after
the workorder's last commit and is asked before the DLL is installed. Control:
`ping` -> `pong (YYTK 4.0.1)`; marker: `crashwatch status` -> a line starting
`crashwatch: off`.

1. **Setup.** Take the hs-drive lease, self-check, back the saves up (a manual
   copy and `hs_saves_backup`). `hs_status` carries `exits`, empty. Launch:
   plugin ready, `exit_watch` held. Load slot 14. Control and marker.
2. **Positive control, in town.** `crashwatch on`; after 2 s the status shows
   heartbeats above 0. `crashwatch crash` refuses, naming `confirm`, and `ping`
   still answers. `crashwatch crash confirm`: the game is gone within 10 s.
   Expected: `hs_status` lists that PID with exit code `0xC0000005`; the file
   ends with the announcement followed by `exception 0xC0000005 at
   BloodPactPlugin.dll+0x... thread=game` (the installed name; the procedure
   as first written said `_rel`); a new dump in `CrashDumps` and an
   Application Error record for `Hero_Siege.exe`, code `0xc0000005`; the
   relaunch reports `CRASH previous session ended without a clean shutdown`.
3. **Phase A, x10, no research hook armed.** A Hell combat zone near level
   243, as in #173. `crashwatch on`, `dropmult gold 10`. Kill and walk over
   every coin for at least 3 minutes and 8 coins, then take the zone's exit
   once. Expected: the eight `dropmult gold coin <i>/8 at x10` lines, at least
   8 `gold DropGold enter` lines each followed by its `done`, `coins=` falling
   after pickups, `room=` changing at the exit, and the HUD gold rising. This
   is the session's positive control for the gold path.
4. **Phase B, x100, no research hook armed.** `dropmult gold 100`, `ping` (the
   #173 sequence). At least 3 minutes and 8 coins, a chest or breakable if one
   is in reach, one zone exit. Recorded: coin lines at x100 and a HUD rise of
   roughly 100 times the coins' amounts; whether the game stays alive; a
   `gold DropGold enter` without a `DropMonsterGold` one before it (a direct
   `DropGold`).
5. **Phase C, the #173 conditions**, only if Phase B ended alive. `dropmult gold
   1`, portal to town, `debuglog`, `gambaprobe hook`, two `gambaprobe spawn`s,
   back to the Hell zone; `dropmult gold 10` for at least 1 minute and 4 coins,
   then `dropmult gold 100`, `ping`, at least 3 minutes and 8 coins and one zone
   exit. Recorded: whether the game stays alive.
6. **Whenever the game dies in steps 3-5**, at once: `hs_status`'s `exits`, the
   last 40 lines of `crashwatch.txt` and of `out.txt`, the newest dump and
   Application Error record, and what the owner was doing, in their words.
   Then relaunch and go to teardown.
7. **Teardown.** `dropmult gold 1`, `crashwatch off`, a graceful
   `hs_stop_game` (its `exits` carries the code; a normal close can read
   `0xC0000409`, guide item 25), inspect and restore the session's save backup
   (the gold earned is undone), release the lease.

Checks: dll-hash, marker, control, hsdrive-current, crash-exitcode,
crash-trap, crash-dump, x10-control, x100-scaled, x100-no-hooks, direct-gold,
hooks-x100, crash-evidence, close-exitcode. The session is valid only if
dll-hash, marker, control, hsdrive-current and x10-control pass; the
instrument is accepted only if crash-exitcode and crash-trap pass. The rest are
research: a fail or a "not observed" there is the finding.

## Live procedure 2

Player build (`plugin_build/BloodPactPlugin_ship.dll`), the same sitting,
after Live procedure 1's teardown, from the save backup it restored; slot 14.
Control: `ping`; marker: `crashwatch status` -> `command unavailable in player
build: crashwatch` (amended after the session, which showed the refusal names
the verb only).

1. **Setup** as Live procedure 1 step 1; control, marker.
2. **x10**: a Hell combat zone, `dropmult gold 10`, kill and pick up for at
   least 2 minutes and 8 coins. Expected: the eight coin lines at x10 in
   `out.txt` and the HUD gold rising.
3. **x100**: `dropmult gold 100`, `ping`, at least 3 minutes and 8 coins, a
   chest or breakable if in reach, one zone exit. Recorded: coin lines at x100,
   the HUD rise, and whether the game stays alive.
4. **If it dies**: `hs_status`'s `exits`, the last 40 `out.txt` lines, the
   newest dump and Application Error record, the owner's words; relaunch and
   read the incident line.
5. **Teardown**: `dropmult gold 1`, a graceful `hs_stop_game` (`exits`), then
   inspect, restore and release as in Live procedure 1.

Checks: dll-hash, marker, control, x10-control, x100-scaled, x100-player,
crash-evidence, close-exitcode. Must pass: dll-hash, marker, control,
x10-control; the rest are research.

## Results

Both sessions ran on 2026-10-07, one sitting, slot 14 "Sorak", launched by
hs-drive (with `CREATE_DEFAULT_ERROR_MODE`, after the server was reconnected).
Each claim below is labelled measured, not observed, or not run.

### Live 1 results

Research DLL, SHA-256 `cbe36c96...79bb`, installed and hashed by the lease.
All 14 checks recorded: 12 pass, 2 not observed (`direct-gold`,
`crash-evidence`). Session validity and instrument acceptance both hold.

- **Setup (measured).** The first `hs_status` had no `exits` field: the
  server was still the pre-change one. After the owner reconnected it,
  `exits` was present and empty (`hsdrive-current`). `hs_launch` reported
  `exit_watch: held`. Control `pong (YYTK 4.0.1)`; marker `crashwatch: off`.
- **The positive control (measured; every instrument fired).** `crashwatch
  crash` refused, naming `confirm`, and `ping` still answered. `crashwatch
  crash confirm` ended the game at once. `hs_status`'s `exits` read the PID
  with `0xC0000005`. `crashwatch.txt` ended with the announcement, then
  `exception 0xC0000005 at BloodPactPlugin.dll+0x1A90DA thread=game
  in-hook=none in-mod=ipc game-original=no`. The module is named by the file it
  was installed as (`BloodPactPlugin.dll`), not `BloodPactPlugin_rel.dll` as
  the procedure's text had it. Windows wrote `Hero_Siege.exe.28660.dmp`
  (63 MB) and an Application Error 1000 record (faulting module
  `BloodPactPlugin.dll`, `0xc0000005`). So the launch flag reached the game,
  and a crash under hs-drive now leaves a dump. On the relaunch the incident
  monitor's `CRASH previous session ended without a clean shutdown` line
  appeared in `out.txt` after `hs_launch`'s reply, not in it, and read `exit
  unknown | module unknown` although hs-drive held the code and a dump existed.
- **Phase A, x10, no research hook (measured; the gold path's positive
  control).** About 196 s in Outskirts of Inoya (`Act_01_01`) over two owner
  rounds. Coin lines 1/8 to 8/8, each `<a> -> <10a>`. 45 `DropMonsterGold
  enter` / `DropGold enter` / `DropGold done` / `DropMonsterGold done`
  quartets, properly nested. `coins=` rose with each drop (up to 3) and fell
  back to 0 within about 0.3-2 s; `room=` changed at each exit. HUD gold
  +27,508 for 21,160 scaled.
- **Phase B, x100, no research hook (measured).** About 3 minutes
  (07:49:31-07:52:30 UTC), 39 coins at x100 (coin lines `<a> -> <100a>`, for
  example `34 -> 3400`, `51 -> 5100`), scaled sum 185,900; HUD gold +234,260.
  Four room changes at x100 (`Act_01_01` -> `Town_01_rm` -> `Act_05_02` ->
  `Town_05_rm`). The game stayed alive, with no exception line.
- **Phase C, the #173 conditions (measured).** `debuglog` and `gambaprobe
  hook` armed in town, two `gambaprobe spawn`s (each machine cleaned up on its
  first frame, as on 2026-10-04). Then x10 for about 97 s and 13 coins, and
  x100 for about 6 minutes and 61 coins (scaled sum 346,500, HUD +444,990),
  with one room change at x100 (`Act_01_01` -> `Town_01_rm`, 08:01:57 UTC).
  The game stayed alive, with no exception line.
- **`gambaprobe hook` installed its known set (measured).** Its summary read
  `44 rows, 3 missing, 0 table-only (32 detoured, 1 detoured-under, 8
  shared)`: the builtins `GetVariable`, `SetVariable` and
  `SetVariableToUndefined` were not found by name. That is already measured
  (`gamba-machine-research.md`, Live 3 and 4; hub
  `docs/RUNTIME_DATA_MODELS.md`): they do not resolve by name on this runner,
  so the procedure's expected "0 missing" was the wrong expectation, not a
  deviation of this session. Phase C ran without those three builtin
  detours. The 2026-10-04 session listed 37 rows; whether that list included
  the three is not established.
- **The install froze the game for 6 s (measured).** `gambaprobe hook: took
  6020 ms` (5835 ms of it in the table phase), then a `STALL 3422 ms` line, a
  `FREEZE 6 s without a frame | in-hook none | in-mod ipc` and its incident
  report; the game recovered by itself.
- **Frame cost (measured; cause not established).** During Phase B, `PERF
  sustained 2.9x for 2 s | baseline 6.9 ms | during 19.9 ms` in `Act_01_01`;
  during Phase C at x100, one `PERF hitch 266 ms frame`. During Phase A's
  first round (x10, `crashwatch` on) the owner remarked that the FPS was low.
  None of these was attributed to a cause.
- **Every gold call was monster gold (measured).** 158 `DropGold` hook calls,
  158 `DropMonsterGold`, and every `DropGold enter` directly preceded by a
  `DropMonsterGold enter`; at `off` the file held 632 breadcrumbs, one `enter`
  and one `done` for each. A direct `DropGold` at x100 was not observed; the
  owner did not report opening a chest or breakable (`direct-gold`).
- **Nothing died in Phases A-C** (`crash-evidence` not observed): `exits`
  carried only the deliberate crash's PID throughout.
- **Teardown (measured).** The graceful `hs_stop_game` closed the game in
  6.6 s; `exits` read `0xC0000409` for it, with `==== clean shutdown ====` the
  last `out.txt` line. Live 2's capture lists `Hero_Siege.exe.14736.dmp`
  (10:02:33 local) as the dump before its own, so this close also left a dump.
  Saves restored and inspected clean.

### Live 2 results

Player DLL, SHA-256 `f40b0162...2dbb`, hashed by the lease. Checks: dll-hash,
control, x10-control, x100-scaled, x100-player and close-exitcode pass;
`crash-evidence` recorded the close-time death (nothing died during play).
`marker` was recorded fail against the procedure's old text, because the
refusal reads `command unavailable in player build: crashwatch`, naming the
verb only; that refusal is itself the evidence that the player build has no
`crashwatch`, and the procedure was amended to it. The owner dropped the play
minimums for this session.

- **x10 (measured).** In Satanic The Depths of Hell, zone level 514: three
  coin lines (`48 -> 480`, `59 -> 590`, `36 -> 360`), HUD gold +1,859 for
  1,430 scaled.
- **x100 (measured).** `dropmult gold 100`, `ping`, then five coin lines
  (`44 -> 4400` to `53 -> 5300`), HUD gold +29,900 for 23,000 scaled, in the
  same zone, about a minute, no zone change. The game stayed alive and
  answered `ping` afterwards (`x100-player`).
- **The close (measured).** The graceful `hs_stop_game` closed the game in
  7.2 s; `exits` read `0xC0000409`, after `==== clean shutdown ====`. Windows
  logged an Application Error 1000 (faulting module `ucrtbase.dll`, exception
  `0xc0000409`), a Windows Error Reporting `BEX64` event, and wrote
  `Hero_Siege.exe.34868.dmp` (60 MB). That is the signature of the tracker
  producer's exit-time abort (guide Known Limitations item 25); the dump's
  stack was not read in this round, so the attribution rests on that
  signature. The relaunch to read the incident line was not run.

### What the two sessions settle

- x100 gold, at the drop and at the pickup, did not end the game on either
  build in about 10 minutes and 105 coins, with five room changes at x100 on
  the research build and none on the player build. Not observed is not "does
  not happen": one sitting, one character, monster gold only.
- `gambaprobe`'s armed detours (minus the three missing builtins) with x100
  and a room change did not end the game either: not observed.
- What a scaled coin credits (measured, a game fact, folded into hub
  `docs/RUNTIME_DATA_MODELS.md` § 13.10): the HUD rise was exactly 1.3 x the
  coins' summed scaled amounts in three of the five play windows (Phase A, and
  both of Live 2's), and 1.26 x and 1.28 x in Phases B and C, whose readings
  may have left coins on the ground (Phase C's was taken at `coins=1`). That is
  #77's 1.3 x, now over the 153 coins of those windows at x10 and x100, on
  both builds, mostly at zone level 243 (Live 1) and at 514 (Live 2).
- The instruments work, as far as the positive control reaches: one access
  violation in our DLL under hs-drive left its exit code, a dump, an
  Application Error record and a trap line naming our module and offset. A
  fast-fail (`0xC0000409`) or an external termination never reaches the
  vectored trap; for those the heartbeat trail and `exits` are the evidence.
  The panel's launch route (the same `launch_game`) was not exercised.

## Not established

- **What ended the game on 2026-10-04.** Every candidate under "What the
  evidence rules in and out" is open. Live 1 and Live 2 reproduced none of
  them: no death during play was observed.
- **Whether x100 coins crash the game at a drop or a pickup.** Not observed on
  2026-10-04 (no x100 coin existed), in Live 2 of #77, nor in 2026-10-07's
  Live 1 (100 coins, research build) and Live 2 (5 coins, player build).
  Monster gold only: a direct `DropGold` (chests, breakables) at x100 was not
  exercised.
- **Whether `gambaprobe`'s armed detours, or a room change under them, can end
  the game.** Not observed in Live 1's Phase C (61 coins at x100, one room
  change), which ran without three builtin detours its list names
  (`GetVariable`, `SetVariable`, `SetVariableToUndefined`, which do not
  resolve by name on this runner, as `gamba-machine-research.md` already
  measured). Whether the 2026-10-04 session's 37-row list included them is not
  established.
- **What made the frames slow** in Live 1 (`PERF sustained 2.9x` in Phase B, a
  266 ms hitch in Phase C, the owner's low FPS in Phase A) and why `gambaprobe
  hook` took 6 s to install. Not attributed.
- **Why the incident monitor's relaunch line read `exit unknown | module
  unknown`** after the deliberate crash, when hs-drive held the code and
  Windows had written a dump. Not investigated.
- **Whether the close-time `0xC0000409` dumps of 2026-10-07 are the tracker
  producer's abort.** The signature matches item 25 (`ucrtbase.dll`, after the
  clean-shutdown line); the dumps' stacks were not read.
- **hs-drive's own error mode** (whether the games it launched before this
  phase inherited `SEM_NOGPFAULTERRORBOX`). Not measured; the launch flag makes
  it irrelevant from this phase on (Live 1's deliberate crash dumped).
- **What `PickUpGoldCheck`'s second `ReportClient` call site checks**, and what
  its 58 unnamed callees do. Not read.
- **Where a `Coin_obj` hands its value to `PickUpGoldCheck`**, and which in-game
  sources call `DropGold` directly (chests, breakables: Known Limitations item
  12 counts 12 direct callers, unnamed). Not found in the shared references or
  research docs.
- **Whether the game saves at a room change.** The 2026-10-04 save times are an
  observation, not a mechanism.
