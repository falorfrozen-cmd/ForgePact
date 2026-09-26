# Item Truth evaluations and the game's memory (2026-09-26)

**Question.** On 2026-09-26 a Hero Siege session that had evaluated about 200,000
items for the Item Editor's seed table ended with a Windows Error Reporting crash
(`ucrtbase.dll`, `0xc0000409`) at 01:36:27. A restarted session evaluated about
40,000 more and sat at about 3.85 GB of private memory. The conclusion drawn at
the time was that the game keeps every item an evaluation request builds, about
95 KB each, and the seed-table tool was limited to 30,000 evaluations per game
session. This document asks what keeps the evaluated items alive, so it can be
released.

**Answer: nothing does.** Measured on build `pe-6aaa6779-0cad4fc8` (the
AnkerGames copy), at the main menu:

- 20,000 evaluations with the released ForgePact 1.4.6 moved private bytes by
  +6.7 MB (white bases) and +10.2 MB (white, unique, socketed and runeword
  items), 0.3-0.5 KB per item. The level then stayed flat for three minutes.
- The player build of the change that added `tools/itemtruth_memrun.py`, measured
  through the tool itself: +10.7 MB over the same mix, flat afterwards.
- The positive control in the same harness: the same 20,000 items, held on
  purpose where the game's collector sees them, grew private bytes by 106-117 MB
  (5.4-6.0 KB per item) and the collector's objects by 100,324-114,325 (5.0-5.7
  per item). Without the hold, 20,000 evaluations left the object count where it
  was (+20 objects in the generation that holds them, once collected).
- The crashed session's own crash dump records its peak commit charge as
  **3.14 GB** - the same peak as a fresh launch reaching the menu (3.03-3.15 GB
  here). 197,704 evaluations never grew the process.
- The crash itself is the process exiting: `ExitProcess` ran the DLLs' exit-time
  destructors, and one of `HSOfflineTrackerProducer.dll`'s called `std::terminate`
  (fast-fail code 7, `abort`). It is not a memory failure, and not ForgePact's
  or the game's code. Nine of the ten dumps Windows kept, 2026-09-25 13:17 to
  2026-09-26 01:36, show the same stack; one is a session that lived 70 seconds.
- The 95 KB figure divides the process's whole private memory (3.85 GB) by the
  number of items (40,000). The game uses about 2.8 GB at the menu before it
  evaluates anything. That restarted session also built 538 items of its own,
  which the bare menu never does, so it was not at the bare menu throughout.

So there is nothing to release, and no fix in this change: the evaluation path of
the player build is unchanged. What the investigation added is the instrument, a
research-build positive control, and the facts below.

## Instruments

- **`tools/itemtruth_memrun.py`** (standard library, Windows). It launches the
  game minimised to the main menu with no character, waits for the menu's
  one-time release (below), queues one evaluation request the way the Item Editor
  does and samples the game's private bytes and working set once a second from
  outside the process. At the end it closes the game with `CloseMainWindow`, as a
  player would, and reports the exit code (`0xC0000409` is an abort). It starts
  the game with Windows' default error mode, so a crash is reported as it would
  be for a player (below, "A plain close aborts too"). `run` is the measurement
  and `control` the positive control (research build). The run's own journal
  files are moved out of the Item Editor's folder afterwards, but only when
  every line in them belongs to the run's requests.
- **`truthmem`** (research build only). `truthmem stat` prints private bytes, the
  runtime's collector (`gc_is_enabled`, `gc_get_target_frame_time`,
  `gc_get_stats`) and the instance count. `truthmem hold on` keeps every item an
  evaluation request builds in `global.fp_truthmem_hold`, a struct the collector
  walks, so none can be freed; `truthmem release` drops it. `truthmem gc` calls
  `gc_collect` and reads again 1, 60 and 600 frames later.
- **The crash dumps** in `%LOCALAPPDATA%\CrashDumps` (Windows keeps the last ten),
  read with the minidump parser from the Miner's Helmet crash research: the
  process's own memory counters, the system's commit, and the faulting thread's
  stack, with each return address matched to a module and an export.

A negative needs a positive control in the same instrument (hub `AGENTS.md`,
"Prove the Instrument Before Trusting a Negative Result"). That is what `control`
is for: it holds the same kind of items in the same launch and shows that the
harness sees them.

## Measurements

All at the main menu, no character loaded, 2026-09-26.

| Run | DLL | Items | Private at the request | Peak while building | After | Per item |
|---|---|---|---|---|---|---|
| `run`, white bases | 1.4.6 release | 20,000 | 2,783.7 MB | 2,792.0 MB | 2,790.4 MB, flat 180 s | +0.34 KB |
| `run --mix` | 1.4.6 release | 20,000 | 2,782.9 MB | 2,794.8 MB | 2,793.1 MB, flat 180 s | +0.52 KB |
| `run --mix` (the committed tool) | player build with this tool's change | 20,000 | 2,783.3 MB | 2,795.6 MB | 2,794.0 MB, flat 180 s | +0.55 KB |
| `control` #1, first half | research, first draft | 20,000 | 3,031.6 MB (before the release) | 3,044.9 MB | 2,811.8 MB, flat 60 s | see below |
| `control` #1, held | research, first draft | 20,000 | 2,811.8 MB | 2,930.2 MB | 2,929.1-2,930.1 MB | **+6.0 KB** |
| `control` #2, first half (the committed tool) | research build with this tool's change | 20,000 | 2,784.9 MB | - | 2,804.5 MB | +1.0 KB |
| `control` #2, held | research build with this tool's change | 20,000 | 2,804.5 MB | - | 2,910.5 MB | **+5.4 KB** |

- **The mix** is the seed-table sessions' own shapes (the journal's evaluated
  definitions): 11,005 white bases, 4,947 uniques and 4,048 socketed items,
  including runewords with their runes in `s1..s6`, whose build makes a rune item
  inside the item.
- **Building speed.** 20,000 items took 41-49 s: about 430 a second at
  ForgePact's 4 ms and 200 items per frame.
- **The collector** (`truthmem stat`) is on, with the default 100 µs frame
  target and five generations. The menu holds about 577,600 objects, 482,474 of
  them in the oldest generation.
- **The first half keeps nothing.** In control #2 the generation that holds
  evaluated items went from 94,568 to 94,588 objects over 20,000 evaluations, and
  the whole heap was back at 577,064 once `gc_collect` ran. In control #1 the
  count went from 577,627 to 579,121 and `gc_collect` found only 1,569 objects to
  free: the items had been collected while the request ran. Control #2's
  +1.0 KB per item in private bytes is the runtime's high-water mark (below),
  not items: none of them survived.
- **The held half.** Generation 2 gained 100,324 objects (control #2) and 114,325
  (control #1), 5.0-5.7 per item, and they survived `gc_collect`. After
  `truthmem release` the collector freed them (generation 2 back to exactly
  94,588 in control #2), but private bytes stayed at 2,910-2,930 MB. The runtime
  keeps memory it has freed for reuse, so private bytes show the most the game
  ever held, never less.
- **The two controls** ran two builds of the same research command: control #1
  a first draft (the same hold, reached through a differently shaped
  `TruthEvalOne`), through the scratch harness this tool grew from; control #2
  the change that added the tool, through the tool itself. Control #1's menu
  release landed inside its first half (below), so that half has no per-item
  figure; the collector's object count is its measure.
- **`gc_collect` runs at the end of the frame.** Read in the same frame, nothing
  had changed. Read one frame later, it had walked the heap: 12.7 ms with about
  578,000 objects, 31.7 ms with about 692,000.

### The menu's one-time release

About 45-126 s after launch, whether or not anything was being evaluated, the
game's private bytes fell once, by 240-370 MB (3,154 → 2,784 MB, 3,153 → 2,801 MB,
3,045 → 2,805 MB), and stayed down. Before that, brief dips of up to 133 MB came
back. A request queued before the release has the release subtract from whatever
it keeps. In control #1 the release landed inside the first half. So
`itemtruth_memrun.py` waits until the last 30 samples are steady and at least
150 MB below the peak before it queues anything. In its two live runs it
queued at 113 s and 138 s.

## The crash of 2026-09-26 01:36:27

Session `8836` (journal `live-pe-6aaa6779-0cad4fc8-20260926-003416-8836-*`)
evaluated 197,704 items in 17 requests between 00:35:23 and 01:36:26: 147,157
white bases, 41,633 uniques and 8,914 socketed items. It ended during the 17th
request, at item 13,836 of 20,000. Its dump,
`%LOCALAPPDATA%\CrashDumps\Hero_Siege.exe.8836.dmp`:

- **The process's own counters.** Peak commit charge 3.14 GB, commit at the dump
  1.92 GB, peak working set 3.35 GB. The system's commit stood at 40.3 of 44.7 GB,
  and its peak since boot was 44.9 GB; what used the rest is not in a
  process dump.
- **The thread** (the dump holds only the one that failed) is inside
  `RtlExitUserProcess` → `LdrShutdownProcess`, running the exit-time destructors
  of `HSOfflineTrackerProducer.dll`. From there `ucrtbase`'s `terminate` →
  `abort` raised the fast fail (parameter 7, `FAST_FAIL_FATAL_APP_EXIT`), which
  WER reports as `0xc0000409`. Below `ExitProcess` are the game's own frames.
- **The same exit as an ordinary one.** Dumps `39980` (a session that lived about
  70 s), `16016` and `46152` have the identical stack, down to the same game
  frames under `ExitProcess`. Nine of the ten dumps kept show the tracker DLL's
  abort; the tenth (`44152`) failed somewhere else.
- **Why it aborts.** HS-Offline-Tracker's producer keeps its publisher in a
  global `std::thread` (`aurie-producer/src/module.cpp`, `g_publish_worker`) and
  joins it only in `StopPublishWorker`. When the game exits through
  `ExitProcess`, the other threads are gone before the DLL's globals are
  destroyed, and a `std::thread` still joinable at destruction calls
  `std::terminate`. `ItemTruth.hpp` avoids the same trap for its own writer
  thread by never destroying the `Journal`. ForgePact ships this DLL
  (`modfiles_shipped/HSOfflineTrackerProducer.dll`, pinned from the 1.3.16
  package, a 2026-09-07-or-earlier build).
- **A plain close aborts too; a missing dump hid it.** None of the five games
  this investigation closed with `CloseMainWindow` left a dump, nor did another
  session's close at about 02:15. That does not show a clean exit. The games
  were started by Python run from Git Bash. That Python reports
  `GetErrorMode()` = `0x3` (`SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX`,
  measured afterwards), and a child inherits it. With it, Windows writes no
  dump and no Application Error event for the fast fail; the exit code (not
  read then) is still `0xC0000409`. HS-Offline-Tracker PR #8's live check
  launched with `CREATE_DEFAULT_ERROR_MODE` instead: the old producer aborted
  on a `CloseMainWindow` at the main menu (exit `0xC0000409`, dump `56756`
  with this stack), and the fixed one exited `0` three times out of three. So
  the old producer most likely aborts every exit once its publisher runs.
  `itemtruth_memrun.py` now launches with the default error mode and reports
  the exit code.

**Not established:** why session 8836 exited at 01:36:27. Its exit path is the
game's ordinary one, and the dump cannot tell a close by a person or a tool from
the game ending itself. It does rule out the process running out of memory: it
never held more than 3.14 GB.

## What this changes

- **No limit on evaluations per session is needed for memory.** The Item
  Editor 2.16.3 seed-table work first stated the 95 KB figure and stopped
  `build_game_seed_table.py` after 30,000 evaluations per session. Both were
  corrected on 2026-09-26, before that work merged: the tool now builds the
  whole table in one run (`--max-per-run` defaults to no limit), and its notes
  point here (hero-siege-item-editor#11, hub #206).
- **The WER reports at game close** come from the tracker producer's shutdown,
  not from the mod or the game; the fix belongs in HS-Offline-Tracker. A crash
  report that names `ucrtbase.dll` with `0xc0000409` needs its dump's stack read
  before it is blamed on anything.
- **Research builds** no longer copy Item Truth's own builds into `LogDrop`'s
  session-long `g_SeenDrop` set and `itemdrops.jsonl`. A seed-table session would
  have put every one of its hundreds of thousands of items there. The player
  build never runs `LogDrop`.

## How to repeat it

```powershell
# the released player build installed, Item Truth on (the editor's Game truth)
py -3 tools/itemtruth_memrun.py run --items 20000 --mix --out <folder>
# the research build (plugin_build\build.bat dev) installed in its place
py -3 tools/itemtruth_memrun.py control --items 20000 --out <folder>
```

Each run takes 5-8 minutes and writes `samples.csv` and `summary.json` (with
the game's `exit_code`); `control` also writes the `truthmem` lines as
`truthmem.txt`. Keep the installed DLL's backup outside `mods\aurie`: Aurie
loads every `.dll` there.
