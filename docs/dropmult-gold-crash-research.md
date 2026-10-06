# Gold at x100 - what ended the game on 2026-10-04 (ForgePact #173)

This doc carries no decompiled script text: the static reading below is the
mechanism in words, and every quoted log line is ForgePact's own output.

## Status

2026-10-06: **research phase under way; no route is known yet.** The game
vanished during a research-build session on 2026-10-04, seconds after
`dropmult gold 100`, and left no dump, no event-log record and no exit code.
This phase adds the instruments that would have answered "what ended it" and
plans one research-build session (Live procedure 1) and one player-build
session (Live procedure 2) that separate the candidates. What was built:

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
- **None of its named callees ends the game.** They are `GetCounterHash`,
  `ReportClient`, `DecryptStringApi`, `string_sha256`, `ShowDebug`,
  `GetCurrenciesSetLogParams` and `ApiRequest`. Its 58 unnamed callees were not
  read.
- **`ReportClient` does not end the game on its named paths.** Its named
  callees are `GetGameStateReport`, `ApiRequestRegion`, `string_sha256` and
  `DecryptStringApi`: it builds a state report and sends it online (hub
  `docs/RUNTIME_DATA_MODELS.md` § 7.5). It has about 200 direct call sites,
  many of them in `CheatDetection`.
- **So a pickup that fails the game's own check is reported, not fatal**, on
  the named paths read. For the unnamed callees this is not established.
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
in player build: crashwatch status`. Every form but `crash` replies with the
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
  thread=<game|other> in-hook=<id|none> in-mod=<mod|none>`, the tags taken
  from the incident monitor's accounting. It always continues the search, so
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
   BloodPactPlugin_rel.dll+0x... thread=game`; a new dump in `CrashDumps` and an
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
build: crashwatch status`.

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

The record round writes this section from the two sessions' captures.

## Not established

- **What ended the game on 2026-10-04.** Every candidate under "What the
  evidence rules in and out" is open; none has been observed.
- **Whether x100 coins crash the game at a drop or a pickup.** Not observed on
  2026-10-04 (no x100 coin existed) nor in Live 2 of #77 (x100 coins dropped
  and were picked up, player build, no crash observed).
- **Whether `gambaprobe`'s armed detours, or a room change under them, can end
  the game.** Not observed.
- **hs-drive's own error mode** (whether the games it launched before this
  phase inherited `SEM_NOGPFAULTERRORBOX`). Not measured; the launch flag makes
  it irrelevant from this phase on.
- **What `PickUpGoldCheck`'s second `ReportClient` call site checks**, and what
  its 58 unnamed callees do. Not read.
- **Where a `Coin_obj` hands its value to `PickUpGoldCheck`**, and which in-game
  sources call `DropGold` directly (chests, breakables: Known Limitations item
  12 counts 12 direct callers, unnamed). Not found in the shared references or
  research docs.
- **Whether the game saves at a room change.** The 2026-10-04 save times are an
  observation, not a mechanism.
