# Frame profiler (`frameprof`) - where the game's frame time goes

Status (2026-09-28): **built, verified against a stand-in frame thread**
(`tests/frame_profiler_harness.cpp`), **run in the game** (see "First
captures"), and **crash-tested** (see "Crash tests"): the live runs found and
fixed a ForgePact bug that could end the game while a summary printed, and a
separate crash at close did not repeat in six A/B closes.

## Why

Every performance question so far was answered one hook at a time: the
population profile build (`build.bat profile`, [population-capacity.md](population-capacity.md))
timed a hand-picked list of scripts, and
[population-performance-analysis.md](population-performance-analysis.md)
named the passes that scale with the monster count by reading the scripts.
That works when the suspect is known. A general question - "what does the
game spend a slow frame on?" - needs a tool that looks at everything without
a list, including the GameMaker runtime and the graphics driver, which no
script hook sees. The analysis document already named the shape: sample the
frame thread's instruction pointer from inside the plugin and bucket it by the
game's own script table.

## What it does

`frameprof start [seconds] [rate]` starts a background thread that, `rate`
times a second (default 250), pauses the game's frame thread, reads its
registers, copies the live part of its stack, and lets it go. After each
pause it walks the copied stack with the x64 unwind tables and counts the
call stack. When the capture ends it names every function it saw and writes
the report.

Names come from three places:

- **Game code:** the game's compiled-code table (YYToolkit's `YYGMLFuncs`: one
  24-byte row per script and object event - name, function, variables - in
  the image's `.data`). The adapter hands over one row, the one the first
  script's `CScript::m_Functions` points at inside the game image, and the
  profiler walks the table both ways from there while rows still look like
  rows: a `gml_` name inside the image and a function that is null or inside
  any loaded module. The installed build's table has 20,951 rows (14,694
  object events, 4,955 scripts, 1,278 script files, 28 room creation codes).
  Mods that hook a script through the table swap that row's function for
  their own; such a row no longer says where the game's function is, so it is
  read again from the exe file on disk, where it still does (moved by the
  image's load offset). In the game on 2026-09-28, 15 rows were swapped and
  all 15 came back from disk. The first version of the walk demanded a
  function inside the image and stopped at the first swapped row: 680 rows,
  and almost no game code named. `gml_Object_<obj>_Draw_64` becomes
  `<obj> Draw GUI`, `gml_Script_X` becomes `X`.
- **Built-ins:** 429 GameMaker built-in names (`plugin/include/ForgePact/FrameProfilerBuiltins.hpp`) the adapter resolves with
  YYToolkit's `GetNamedRoutinePointer`; the entry point it returns is the
  function a sampled frame starts at. A name this runner lacks is skipped.
- **System DLLs:** each module's export table. A thread inside a system call is
  named by the syscall stub it sits in (`ntdll.dll!NtDelayExecution`).

Everything else is `module!0x<rva>` - the GameMaker runtime's own functions
have no names in the image.

Each sample then falls in one bucket, decided from the innermost frame
outwards by the first frame that says what kind of work it is: a graphics
module (the driver, `d3d11`, `dxgi`) makes it **graphics driver**, game code
makes it **game code**, a mod's DLL makes it **mods**; runtime and system
frames are passed through, and a stack with none of those is **GameMaker
runtime**. A blocking system call at the innermost frame (a wait, a sleep, a
yield) turns graphics into **waiting for the GPU / display**, game code into
**game code waiting**, and a runtime stack into **idle (frame limiter,
sleeping)**. Game code under a mod's hook still counts as game code: the
hook's frame is outside the game function, not inside it.

**The frame limiter spins.** Hero Siege's runner does not sleep between
frames: it busy-waits on the clock. In town at 60 fps the frame thread used
94-96% of a core, and 38-45% of its samples sat in one runtime function
(`Hero_Siege.exe!0xB55BDC0`) or the `QueryPerformanceCounter` calls it made.
Counted as runtime, that read as a thread 99.7% busy. So the report finds the
limiter from the data - among runtime-only stacks whose innermost frame is a
clock read, the calling function that holds at least half of them and at
least 1% of all samples - and puts it, and the clock reads it makes, in its
own bucket, **idle (frame limiter, spinning)**, counted as waiting (JSON
`spinWait`). A clock read under game code is never counted there.

## The report

Three files in `<game>\bin\bp_ipc\perf\`, and the summary in `out.txt`:

| File | What it holds |
| --- | --- |
| `frameprof-<date>-<time>.json` | `capture` (length, rate, samples, what the pauses cost, how walks ended), `frames` (fps, median/p90/p95/p99/worst, frames over 33/50/100/250 ms), `time` (working vs waiting), `buckets`, the tables `events`, `gmlTotal`, `gmlSelf`, `builtins`, `leafFunctions`, `leafModules` (40 rows each, share of all samples and of working samples), `hitches` (the ten slowest frames over 50 ms, each with what ran during it), `timeline` (one row per second: frames, mean and worst frame, working share, room, instance count, monsters alive) and `threads` (CPU per thread over the capture, as a share of one core) |
| `.stacks.txt` | one line per distinct call stack, outermost frame first, `;`-separated, with its sample count last - the collapsed format flame-graph tools read |
| `.txt` | the summary lines |

`py tools/frameprof_report.py` (newest capture of the configured game, or a
path) prints a fuller summary and writes `<stem>.html` beside the capture: a
self-contained page with the same numbers, the tables with bars, the slow
frames, a per-second chart with the monster count, an icicle chart of the
stacks, and the CPU per thread.

After the summary it prints a `runner phases:` block, and the page has the
same as its Runner phases section. It answers where the runtime's share goes
inside a frame, by splitting the frame thread's samples into the runner's
**step phase** (Begin Step, Step, End Step, alarms, collisions), its **draw
phase** and what lies outside both (presenting the frame, the frame limiter):

- **How a phase is found.** From the `.stacks.txt` alone, never from an
  address: a phase's dispatcher is the outermost runtime function under which
  at least 95% of the object events are that phase's, provided those events are
  at least 1% of all samples. A stack beneath both dispatchers is counted once.
- **`step phase:` / `draw phase:`** - the dispatcher, the function it is
  called from, its share of all samples split by bucket, and its eight
  heaviest callees, each with its share and its runtime-only share (no game
  code, graphics driver or mod anywhere on the stack). `not found` when no
  function qualifies.
- **`outside the phases:`** - the rest of the samples, split by bucket.
- **The parity line.** The tool derives each stack's bucket again from its
  labels, the way the plugin's `Classify` and `IsSpinSample` do, and compares
  the totals with the capture's `buckets`: "buckets agree with the capture",
  or the samples by which they differ, bucket by bucket. A stacks file keeps no
  module paths, so a fixed list of Windows DLLs stands in for the plugin's
  `\windows\` rule; a difference shows up here rather than silently.

The research this split serves (ForgePact #183, which part of the frame
thread's time an offload could win) is
[main-thread-offload-research.md](main-thread-offload-research.md).

How to read the tables:

- **Heaviest events** - each object event with everything it calls. This is
  where a frame's time goes, event by event.
- **Heaviest game code** - every script and event, each counted once per
  sample it is anywhere in. Nested code is inside its caller's share, so the
  rows add up to more than 100%.
- **Game code's own time** - each game function's own code, plus the runtime
  calls it makes directly (variable reads, built-ins), but not other game
  functions it calls. This is where to look for a function that is slow in
  itself.
- **Heaviest built-ins** - built-in functions with what they call.
- **Innermost functions / by module** - where the CPU actually was.

## Cost, measured

One sample pauses the frame thread for `SuspendThread` + `GetThreadContext` +
`ResumeThread`. Against a spinning thread on the development PC
(2026-09-28, `THREAD_PRIORITY_HIGHEST` sampler, high-resolution timer):
suspend 10 us, get-context 41 us, resume 7 us, **61 us median, 138 us p95**;
most of it is `GetThreadContext` waiting for the suspension to land. With the
same PC busy compiling in the background the pause grew to about **500 us**.

So the default rate is 250 a second (1.5-2.5% of the frame thread's time at
the quiet-machine pause), and the sampler watches its own cost: every half
second, if the pauses added up to more than 3% of that time, it halves its
rate (down to 20 a second); when they fall under 0.75% it doubles back, up to
the requested rate. The report carries `pausePercentOfTime` (an upper bound -
it times the whole pause, not just the part the frame thread was stopped),
`hzLowest` and `rateCuts`. The walking and naming run on the sampler's own
thread, whose CPU the report lists separately.

While no capture runs, the plugin's per-frame cost is two atomic loads
(`FrameProfilerTick`).

## Safety

- **Nothing that can take a lock runs while the frame thread is suspended.**
  `CaptureOnce` does `SuspendThread`, `GetThreadContext`, a guarded `memcpy`
  of the stack into a buffer allocated before sampling started,
  `ResumeThread`, and two `QueryPerformanceCounter` reads - no heap, no
  logging, no game call. The frame thread may be holding the heap, CRT or
  loader lock when it is paused; a sampler that needed one would freeze the
  game. This is the stall watchdog's rule (ModuleMain.cpp), and
  `tests/test_frame_profiler.py` pins it textually.
- **The walk reads the copy, not the live stack.** After `ResumeThread` the
  frame thread moves on; pointers into the old stack range, in the copy and in
  the registers, are moved into the copy first, and `RtlVirtualUnwind` then
  reads only the snapshot. SEH guards it against a bad one.
- **No loader-lock lookups.** The walk finds unwind entries by binary search in
  each module's own `.pdata`, from a module list taken at the start of the
  capture; `RtlLookupFunctionEntry` is never called.
- **Exit-safe.** The profiler is a heap-held singleton that is never freed and
  its thread is an `ExitSafeThread`, so `ExitProcess` finds nothing to destroy
  (Known Limitation 26 in the module guide).
- **It changes nothing.** No hook, no write into the game. The adapter reads
  one table row, looks up built-in names, and once a second reads `room`,
  `instance_count` and `instance_number(Enemy_Parent_obj)` on the frame thread.
- **The heap-churn test.** The harness samples, at 2000 a second, a thread
  that allocates and frees in a tight loop - the case a lock taken while
  suspended would deadlock. It has to finish; a watchdog ends the harness if it
  does not.

It is in the player build because it is inert until started and a player
reporting "this area is slow" can send a capture; the stall watchdog stays
research-only because it was a thread that ran all the time.

## First captures (2026-09-28)

Installed build (exe 281,751,552 bytes, PE stamp `0x6AAA6779`), Suh (level
100) standing still, the player's own ForgePact settings (Monster Density 5x,
map reveal with pack markers), game window restored without focus, Intel Xe
graphics. The ForgePact build was this branch's player build.

| | Town_02_rm, 15 s | Act_01_01, 30 s |
| --- | --- | --- |
| fps / median / 95% under / worst | 58.9 / 16.7 / 16.7 / 158 ms | 51.5 / 18.7 / 25.2 / 58 ms |
| frame thread working / waiting | 54% / 46% | 95% / 5% |
| game code | 17.6% | 26.3% |
| GameMaker runtime (no game code on the stack) | 24.8% | 57.2% |
| graphics driver / waiting for GPU or display | 8.8% / 1.8% | 9.0% / 1.6% |
| mods | 2.6% | 2.8% |
| frame limiter, spinning | 44.5% | 3.1% |
| instances / monsters alive | 858 / 3 | 6,072 / 78 |
| frame thread CPU, % of one core | 94% | 93% |
| profiler cost, upper bound | 1.9% | 2.0% (rate cut to 125/s at times) |

Read so far:

- Act_01_01 at 5x misses 60 fps with the frame thread working 95% of the
  time: it is CPU-bound on one core. More than half of that core goes to the
  GameMaker runtime itself, not to any game script, and the zone holds 6,072
  instances of which only 78 are monsters: the runtime's per-frame work is
  mostly about everything else in the zone. The heaviest game code is small
  by comparison (Player_obj Step 4.5%, Menu_Controller_obj Step 3.4%, of which
  `timer_system_update` 3.3%; Controller_obj Draw GUI 3.0%).
- Below the main loop the runtime-only time splits into two branches, about
  27% (with one function at 13% self time) and about 17% of all samples;
  naming them is the next research step, which
  [main-thread-offload-research.md](main-thread-offload-research.md) takes
  with the report's runner phases (see "The report").
- The town's two slow frames (158 and 116 ms) were 36 of 39 and 27 of 29
  samples waiting for the GPU or display: a graphics stall, not game code.
- The frame thread keeps a whole core busy even when idle, because the frame
  limiter spins.

## Crash tests (2026-09-28)

Two different crashes turned up around the live runs; each close below is a
normal window close from Act_01_01 after about 150 s there, driven by the AFK
repo's `tools/game_session.py` (never forced), with Windows Error Reporting
read after each.

| Run | ForgePact build | Captures | Result |
| --- | --- | --- | --- |
| first live session, 09:53 | first test build | town + zone | `0xC0000005` at `Hero_Siege.exe+0xB488F3E` on close |
| C1, 10:08 | first test build | one, while the game was still starting | `0xC0000409` in `ucrtbase.dll`, exception data 5, while the summary printed (before any zone) |
| early capture, 10:17 | first test build | one, 31 s after launch | alive, all 12 summary lines printed |
| C2 | final build | town + zone | clean exit (0) |
| B1 | final build | none | clean exit (0) |
| OLD1 | first test build | town + zone | clean exit (0) |
| A1 | the player's own ForgePact | none | clean exit (0) |
| C3 | final build | town + zone | clean exit (0) |

**`0xC0000409` - a ForgePact bug, fixed.** `Out()` wrote each line to
`out.txt` and passed it to YYToolkit's `PrintInfo("[BP] %s", line)`. At the
pinned YYToolkit commit, `PrintInfo` formats that into a 4096-byte buffer with
`vsprintf_s` and hands the result to `CmWriteInfo`, which runs it through
`vsprintf_s` again as the format string. A `%` in the line is then a
conversion specifier; an invalid one ("54%, waiting" gives `%,`) or a line too
long for the buffer makes `vsprintf_s` call the C runtime's invalid-parameter
handler, and with the default handler that ends the process with a fast fail:
`0xC0000409`, exception data 5 (`FAST_FAIL_INVALID_ARG`). In C1 the first two
summary lines, which hold no `%`, reached `out.txt`; the third, the first with
a `%`, did not - `Out()`'s stream was still open when `PrintInfo` ended the
process. The same lines printed without harm in the other sessions, so
whether the handler is fatal depends on the game's state at the time (not
pinned down). The fix, in `Out()`: the line is written and the stream closed
first, every `%` is doubled for the console copy (the second pass turns `%%`
back into `%`), and the console copy is cut at 1800 characters. Every `Out()`
caller gets it, not only the profiler; the other `Print*` calls in the plugin
use constant formats.

**`0xC0000005` at `+0xB488F3E` - not reproduced.** The faulting instruction
is an indirect call in the runtime's call-a-built-in-by-index helper
(function `+0xB488E50`, used from over a thousand places), reading the
built-in table's entry for the call - an invalid table or index at the time.
Another session recorded the same address for `goto Act_01_06` without these
builds. It did not repeat in the six later closes above, including the same
build and scenario that produced it (OLD1), so it is read as an intermittent
crash in the game's room teardown, not the profiler's. Of the test-session
tool's earlier recorded closes from Act_01_01, it is the only crash.

## Limits

- It sees one thread, the frame thread. The report's CPU-per-thread table shows
  how busy the others were, not what they did.
- A frame in generated code (no module), such as a hook's trampoline, ends
  the walk. That is rare: a trampoline runs a few instructions and jumps on,
  and a hook's own function lives in its DLL like any other frame.
- The GameMaker runtime's functions are unnamed (`Hero_Siege.exe!0x...`). The
  offline Ghidra workflow in the hub can name the few that matter.
- A capture holds up to 500 frames a second of frame times and every sample's
  stack; 600 s at 2000 a second is the ceiling the limits allow.
- Windows charges thread CPU time in clock ticks, so the per-thread percentages
  of a short capture read a few percent off.

## Considered and not used

- **Timing every object event** through YYToolkit's `EVENT_OBJECT_CALL`: that
  callback is off in the toolkit's YYToolkit build (the `ExecuteIt` hook,
  patch `0003` in the hub's series), and project notes record the per-event
  hook crash-looping on Season 10.
- **Hooking a list of scripts** - the population profile build does this, and
  it only sees what is on the list; direct calls from compiled code are blind
  to a table hook.
- **An external profiler process** (sampling through `ReadProcessMemory`):
  it would need no plugin at all, but it would have to parse unwind codes
  itself for a remote image and find the table from outside. Worth it only if
  a capture ever has to be taken without ForgePact installed.
- **ETW sampled profiling**: needs administrator rights.
