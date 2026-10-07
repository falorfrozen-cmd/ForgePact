# Main-thread offload - which part of the frame thread's time could move

Status (2026-10-07): **research, nothing offloaded.** The runner's own threads
are a static reading (below); the frame thread's split into the runner's
phases and the live session's captures are added to this document as they
are made. Nothing here is a player-visible change.

## Question

[ForgePact #183](https://github.com/falorfrozen-cmd/ForgePact/issues/183)
asks to move some per-frame work off Hero Siege's main thread, starting with
one small, measurable part. The game runs on GameMaker's YYC runner, and all of
its game logic (every GML step and draw event) runs on that one thread. Before
anything is offloaded, the question is which part of the frame thread's time an
offload could win at all.

The issue lists four candidates:

1. **Measure first.** Sort the frame thread's time, over a heavy room, into
   game code, runtime code, graphics driver and waits, so the measurement
   decides which of the others pays. The issue proposed building a sampler
   from the stall watchdog. That sampler already exists: `frameprof`
   ([frame-profiler.md](frame-profiler.md)) has shipped in both builds since
   v2.0.0 and buckets the frame thread's time this way. What it could not yet
   say is where its largest bucket goes - 57% of Act_01_01's frame was
   "GameMaker runtime with no game code on the stack" (frame-profiler.md,
   "First captures") - so this research splits that bucket by the runner's
   phases rather than building a second sampler.
2. **Render submission off the frame thread, with no game code changed:**
   DXVK as a drop-in `d3d11.dll`/`dxgi.dll`, whose own thread does the
   translation and driver submission. It can only win the graphics-driver
   share.
3. **One pure, heavy GML computation as snapshot, worker, apply:** copy a
   script's inputs on the main thread, compute on our own worker thread, and
   apply the result on the main thread where the game reads it. It can only
   win game code that is a self-contained computation, not the reads of live
   instance state around it.
4. **Child game processes,** in the manner of tinkerer-red/MultiProcessing:
   extra copies of the game that run GML in parallel, at the cost of
   serializing every input and result and of a whole game instance per child.

## Static reading: threads the runner starts

**Static reading, 2026-10-06 (ForgePact #183),** made in the local Ghidra
project and paraphrased here; not measured live. On 2026-10-07 the installed
exe was checked to contain the strings "GC Thread", "Job Worker Thread",
"JobManager", "GameMaker HTTP", "THREAD SAFETY ERROR", "MultithreadGCOn",
"MultithreadGCOff" and "GroundGeneration time"; nothing else below was
re-read.

- **Garbage collection** runs on a thread of its own, the "GC Thread". The
  runner has a `MultithreadGCOn`/`MultithreadGCOff` debug switch, and its flag
  byte reads 1 in the image, so the multithreaded collector is the default.
- **A job pool,** "JobManager", runs up to 8 "Job Worker Thread"s. Its jobs are
  image and texture-page decoding (PNG, GIF, JPEG, QOI and external
  `texture_%d.yytex` files), `buffer_save_async` and `buffer_load_async`, and
  zip and HTTP texture loads.
- **Audio** has its own thread, and so does HTTP ("GameMaker HTTP").
- **Direct3D 11** is created with device flags 0x820: BGRA_SUPPORT (0x20) plus
  VIDEO_SUPPORT (0x800). SINGLETHREADED (0x1) is not set, and neither is
  PREVENT_INTERNAL_THREADING_OPTIMIZATIONS (0x8), so the driver may use its own
  threads.
- **Saving.** The game has `SaveFileGMAsync` and `SaveCommit` scripts. Whether
  they reach `buffer_save_async`, and so the job pool, is not established. A
  `HookBuiltin` call counter on `buffer_save_async` during one save would
  settle it.
- **A main-thread guard.** The runner refuses some operations when they are
  called off the main thread, with the error "THREAD SAFETY ERROR, this code
  can only be executed on the main thread". A worker thread of ours that calls
  into the runner can therefore fail by design, not only by racing.

**Measured, beside the readings** (2026-09-28 `frameprof` captures, each
capture's `.txt` summary and JSON):

- **CPU per thread.** The frame thread used 93-99% of one core, and every other
  game thread together used 4-10% of one core. So the threads above exist, but
  together they do little of the work in these captures.
- **No thread names.** `GetThreadDescription` was empty for every game thread
  (the JSON's `threads.top[].name`). The runner gives Windows no thread names,
  so the names above are strings in the exe, not descriptions a tool can see on
  a live thread.
- **The collector still costs the frame.** `docs/RUNTIME_DATA_MODELS.md` § 5.9
  measured `gc_collect`'s walk landing one frame later and taking 12.7-31.7 ms.
  The reading says a GC thread exists; it does not say collections leave the
  frame thread alone. Whether the frame thread waits on the GC thread is not
  established.

### Research addresses, never called from player code

These are absolute addresses in the local Ghidra project (image base
0x140000000, so RVA = address - 0x140000000), for this build only. They are
research findings, not an interface: per AGENTS.md § "Never Call an Address You
Resolved by Hand" none of them is ever called or read from player code, and
the next game build moves every one.

Build `pe-6aaa6779-0cad4fc8` (§ 16.8 of `docs/RUNTIME_DATA_MODELS.md`).

- Read from the copy imported into the Ghidra project: SHA-256
  `498d588550d80d0d100a4ce6958bcc044e6b79339920d3d8a931bca9a1b142c0`.
- The installed exe on 2026-10-07 has SHA-256
  `dab03f134327d9b682c67f1dc461822fb822a7e535009636624bb1454f498e2b`.
- The two are the same size (281,751,552 bytes). All 4,049 bytes that
  differ lie in the `.aurie` section the patcher appends, so the game's code,
  and every address below, is the same in both.

| What | Address | Module offset |
|---|---|---|
| Job submit | 0x14b696960 | `Hero_Siege.exe+0xb696960` |
| Worker spawn | 0x14b696ac0 | `Hero_Siege.exe+0xb696ac0` |
| `D3D11CreateDevice` call site | 0x14b61443f | `Hero_Siege.exe+0xb61443f` |
| GC thread start | 0x14b486950 | `Hero_Siege.exe+0xb486950` |
| Multithreaded-GC flag byte (reads 1) | 0x1505ad00b | `Hero_Siege.exe+0x105ad00b` |

The facts above, without the addresses, are folded into
[`docs/RUNTIME_DATA_MODELS.md` § 5.11](../../docs/RUNTIME_DATA_MODELS.md#511-threads-the-runner-starts).

## Runner phases in the 2026-09-28 captures

**Measured, 2026-09-28 captures, re-read offline 2026-10-07** with
`py tools/frameprof_report.py --no-html <capture>.json` (its `runner phases:`
block). The input is the six 15 s Act_01_01 captures of the hidden-loot A/B
that are still on disk (`bin\bp_ipc\perf\frameprof-20260928-<stem>.*`). The
"First captures" of [frame-profiler.md](frame-profiler.md) (Town, and
Act_01_01 for 30 s) are gone, so they are not re-read here.

The tool finds the phases from each capture's own stacks, not from an
address: a runtime frame is a phase's dispatcher when at least 95% of the
event-bearing samples beneath it are that phase's event kinds (Begin Step,
Step, End Step, Alarm and Collision for the step phase; the Draw family,
Pre-Draw and Post-Draw for the draw phase). On all six captures it found the
same structure:

- the frame loop `Hero_Siege.exe!0xB56F4D0` has two branches: the per-frame
  function `Hero_Siege.exe!0xB56E300`, and the presentation branch
  `Hero_Siege.exe!0xB55E900`;
- under the per-frame function, the **step dispatcher** is
  `Hero_Siege.exe!0xB56E4B0` and the **draw dispatcher** is
  `Hero_Siege.exe!0xB60F590`;
- "outside the phases" is everything else: the presentation branch, which
  holds the frame limiter `Hero_Siege.exe!0xB55BDC0`, and the per-frame
  function's own work outside both dispatchers.

The phases' shares of all samples. Loot is the hidden-loot A/B's state: asleep
gives about 140 fps, awake 30-46 fps. Parity is the tool's check that its
per-stack buckets add up to the plugin's own bucket counts.

| Capture | Loot | fps | Step phase | Draw phase | Outside | Parity |
|---|---|---|---|---|---|---|
| 170330 | asleep | 139.4 | 47.3% | 37.7% | 15.0% | agree |
| 170624 | awake | 45.6 | 72.0% | 25.5% | 2.4% | agree |
| 170718 | asleep | 142.5 | 46.3% | 37.8% | 15.9% | agree |
| 170816 | awake | 44.9 | 72.2% | 25.3% | 2.5% | agree |
| 170859 | asleep | 143.1 | 45.2% | 37.1% | 17.7% | agree |
| 171044 | awake | 29.8 | 52.9% | 44.9% | 2.2% | agree |

Each phase split by bucket, in percent of all samples, as
game / runtime / graphics / mods / waits. Waits are the GPU wait, game code
waiting, idle sleeping and the spinning limiter together.

| Capture | Step phase | Draw phase | Outside the phases |
|---|---|---|---|
| 170330 | 22.4 / 24.6 / 0 / 0.2 / 0 | 9.3 / 24.8 / 1.5 / 2.2 / 0 | 0.1 / 3.6 / 1.3 / 0.4 / 9.5 (spin) |
| 170624 | 58.6 / 13.3 / 0 / 0.1 / 0 | 9.3 / 9.3 / 2.6 / 4.3 / 0 | 0.1 / 1.6 / 0.6 / 0.1 / 0 |
| 170718 | 21.0 / 25.0 / 0 / 0.2 / 0 | 9.7 / 24.7 / 1.6 / 1.8 / 0 | 0.1 / 3.4 / 1.8 / 0.5 / 10.1 (spin) |
| 170816 | 59.5 / 12.6 / 0 / 0.1 / 0 | 8.8 / 9.3 / 2.7 / 4.5 / 0 | 0 / 1.6 / 0.5 / 0.3 / 0.1 (GPU) |
| 170859 | 21.4 / 23.7 / 0 / 0.1 / 0 | 8.6 / 24.4 / 2.2 / 1.9 / 0 | 0 / 3.5 / 1.5 / 0.5 / 12.3 (spin 12.2, GPU 0.1) |
| 171044 | 44.3 / 8.5 / 0 / 0.1 / 0 | 27.7 / 7.4 / 7.1 / 2.7 / 0 | 0 / 1.4 / 0.7 / 0.1 / 0 |

The dispatchers' heaviest direct callees, by runtime-only share (no game,
graphics or mod frame anywhere on the stack), as the range over the three
~140 fps captures and then the three slower ones:

| Phase | Callee | Runtime-only, ~140 fps | Runtime-only, 30-46 fps | Read below as |
|---|---|---|---|---|
| step | `Hero_Siege.exe!0xB57C200` | 7.4-7.9% | 1.5-2.7% | the collision pass |
| step | `Hero_Siege.exe!0xB6C5BF0` | 6.0-6.4% | 2.8-4.1% | the alarm pass |
| step | `Hero_Siege.exe!0xB5E0D80` | 2.1-2.6% | 0.9-1.4% | a room element pass |
| step | `Hero_Siege.exe!0xB56F180` | 2.1-2.3% | 0.7-1.1% | the animation frame advance |
| step | `Hero_Siege.exe!0xB6C7350` | 1.3-1.6% | 0.4-0.7% | the Step-family event pass |
| draw | `Hero_Siege.exe!0xB60E880` | 11.3-12.7% | 4.2-4.5% | the room draw, per view |
| draw | `Hero_Siege.exe!0xB610330` | 9.3-10.1% | 2.1-3.3% | the Post-Draw and GUI event passes |
| draw | `Hero_Siege.exe!0xB610B30` | 2.3-2.7% | 1.1-1.7% | the Pre-Draw event pass |
| draw | `Hero_Siege.exe!0xB60D070` | 0.2-0.3% | 0.1% | the main layer pass |

What these show, all measured on these six captures:

- **At about 140 fps the "GameMaker runtime" bucket splits almost evenly
  between the phases**: 23.7-25.0% of samples are runtime in the step phase,
  24.4-24.8% in the draw phase and 3.4-3.6% outside them.
- **The step phase's runtime time is mostly fixed passes over instances,** the
  collision and alarm passes above all, not the dispatch of game events: the
  Step-family event pass carries 22-24% of samples but only 1.3-1.6%
  runtime-only.
- **The draw phase's runtime time is in the room draw and the GUI-side event
  passes,** and graphics-driver frames are only 1.5-2.2% of samples there.
- **Outside the phases is 15-18% at about 140 fps, and most of it is the
  limiter spinning** (9.5-12.2%). At 30-46 fps the frame has no time to spare
  and outside drops to 2.2-2.5%.
- **With the loot awake, game code takes over:** 58.6-59.5% of samples are
  game code in the step phase at 45 fps, and at 30 fps the draw phase grows to
  44.9%, 27.7% of it game code (`Loot_Ground_obj Draw`).
- **Parity:** the tool's bucket totals agreed with the plugin's on all six
  captures.

## The per-frame functions (static reading)

**Static reading, 2026-10-07,** in the local Ghidra project (the build whose
SHA-256 is given above), through the `ghidra` MCP server's decompiler, in our
own words; nothing here is measured beyond the shares quoted from the section
above. None of these was in the decompile index before. Each is named by its
module offset; the absolute address is 0x140000000 more (for example
0x14b56e300).

- **Per-frame function** (`Hero_Siege.exe+0xb56e300`). If an override
  callback is installed it hands the whole frame to it and returns; what
  installs one is not established. Otherwise it runs the frame in order,
  marking regions with a colour and a name in the shape of the runner's
  debug-overlay timing bars: a "Garbage Collector" region, some input and
  housekeeping calls, an "IO&YoYo" region, and then the step dispatcher. It
  returns before the step dispatcher when the game window is not the active
  window and a pause-when-unfocused switch is set. After the step, and only
  when no room change is pending, it opens a "Draw" region, prepares the
  views, runs the draw dispatcher when drawing is enabled, counts the frame,
  and ends with a second "Garbage Collector" region around one call. That the
  call is the collector's per-frame work is not established.
- **Step dispatcher** (`Hero_Siege.exe+0xb56e4b0`), in an "Update" region.
  It first walks every active instance once, copying its position into its
  previous-position fields and advancing its animation frame. It then runs, in
  order: Begin Step events; a series of further event passes, among them the
  alarm pass (what the others dispatch is not established); a room element
  pass; Step events; motion, where each instance moves by its speed, or a
  physics world steps instead when the room has one; two more event passes
  (not established); the collision pass, skipped when the physics world
  stepped; and End Step events. After each pass it checks whether a room
  change is pending and stops early if one is.
- **Draw dispatcher** (`Hero_Siege.exe+0xb60f590`). It creates or resizes
  the application surface when needed, runs Pre-Draw events, and clears the
  screen when no instance drew in them. It then draws the room once per
  enabled view (up to eight, or once with views off), setting each view's port
  and camera first. Then it runs Post-Draw events and, when the application
  surface is in use, draws that surface to the screen. Then, in GUI space, it
  runs Draw GUI Begin, Draw GUI and Draw GUI End. These are interleaved with a
  second Draw Begin, Draw and Draw End pass over layers that a different
  layer mask selects; what that mask selects is not established.
- **The frame loop's other branch** (`Hero_Siege.exe+0xb55e900`). It does
  nothing when no display device exists. Otherwise it timestamps the frame and
  calls one of two routes chosen by its argument, timing the route when the
  argument is set. In the captures no sample under it went through the
  untimed route; nearly all went through the timed one, which reads DWM composition timing and monitor information and,
  through one callee, reaches both the limiter and Discord's overlay hook
  (`DiscordHook64.dll`). This is the presentation step; which call is the
  swap chain's Present is not established.
- **Frame limiter** (`Hero_Siege.exe+0xb55bdc0`). It waits out a deadline
  given in microseconds, capped at 3 s. In one mode it sleeps the whole wait
  on a high-resolution waitable timer. In the other it sleeps on that timer
  for the wait minus a margin, unless a switch skips the sleep, and then spins
  on `QueryPerformanceCounter` (or `GetTickCount64`) until the deadline. The
  captures' spinning samples are this loop. Which setting makes this build
  spin through the whole wait is not established.

The step dispatcher's five heaviest callees by runtime-only share:

- **Collision pass** (`Hero_Siege.exe+0xb57c200`). It gathers candidate
  instance pairs from a spatial structure, tests each pair's overlap
  precisely, runs the Collision event on both instances, and moves solid
  instances back to their previous position. Its runtime-only share is the
  pairing and overlap testing, before any game event runs.
- **Alarm pass** (`Hero_Siege.exe+0xb6c5bf0`). For each of the twelve alarm
  slots it walks the instances of every object that has that alarm event,
  counts each instance's alarm down by one, and runs the Alarm event when it
  reaches zero. Its runtime-only share is that walk and countdown.
- **Room element pass** (`Hero_Siege.exe+0xb5e0d80`). It walks a list of
  element ids the room keeps, updates each element it finds, and then calls a
  member named `event` on the struct entries that qualify. That fits the
  room's sequence elements and their event callbacks, but it is not
  established.
- **Animation frame advance** (`Hero_Siege.exe+0xb56f180`), called once per
  instance from the dispatcher's first walk. It advances the instance's image
  index by its image speed, scaled by the sprite's own speed and the game
  speed, wraps it at the frame count, and on a wrap runs an Other-type event
  (Animation End by its placement; the subtype is not established).
- **Step-family event pass** (`Hero_Siege.exe+0xb6c7350`). It runs one of
  Begin Step, Step or End Step, chosen by its argument, for every instance of
  every object that defines it, skipping deactivated instances and instances
  created after the pass began. Nearly all of its time is the game's own event
  code.

The draw dispatcher's heaviest callees by runtime-only share (the fifth,
`Hero_Siege.exe+0xb4a2410`, is under 0.1% and was not read):

- **Room draw, per view** (`Hero_Siege.exe+0xb60e880`). It stores the view
  rectangle, clears it when the room asks for that, sets the view's camera, and
  runs Draw Begin events, the main layer pass and Draw End events.
- **Layer-by-layer event pass** (`Hero_Siege.exe+0xb610330`). It runs one
  Draw-family event, chosen by its arguments, for every instance that defines
  it, layer by layer in depth order with each layer's begin and end hooks
  around it; one instance alone takes a shorter path. The draw dispatcher
  calls it for Post-Draw and the three GUI events.
- **The same pass, reporting** (`Hero_Siege.exe+0xb610b30`). It is the
  layer-by-layer pass above, used for Pre-Draw, and it reports whether any
  instance ran the event; the dispatcher clears the screen when none did.
- **Main layer pass** (`Hero_Siege.exe+0xb60d070`). It walks the room's
  layers in depth order and draws each element according to its kind (the
  body switches over nine kinds); an instance element runs its Draw event.

Addresses, absolute, for the same build (research addresses, never called
from player code):

| What | Address | Module offset |
|---|---|---|
| Frame loop (seen in the stacks, not read) | 0x14b56f4d0 | `Hero_Siege.exe+0xb56f4d0` |
| Per-frame function | 0x14b56e300 | `Hero_Siege.exe+0xb56e300` |
| Step dispatcher | 0x14b56e4b0 | `Hero_Siege.exe+0xb56e4b0` |
| Draw dispatcher | 0x14b60f590 | `Hero_Siege.exe+0xb60f590` |
| Frame loop's other branch (presentation) | 0x14b55e900 | `Hero_Siege.exe+0xb55e900` |
| Frame limiter | 0x14b55bdc0 | `Hero_Siege.exe+0xb55bdc0` |
| Collision pass | 0x14b57c200 | `Hero_Siege.exe+0xb57c200` |
| Alarm pass | 0x14b6c5bf0 | `Hero_Siege.exe+0xb6c5bf0` |
| Room element pass | 0x14b5e0d80 | `Hero_Siege.exe+0xb5e0d80` |
| Animation frame advance | 0x14b56f180 | `Hero_Siege.exe+0xb56f180` |
| Step-family event pass | 0x14b6c7350 | `Hero_Siege.exe+0xb6c7350` |
| Room draw, per view | 0x14b60e880 | `Hero_Siege.exe+0xb60e880` |
| Layer-by-layer event pass | 0x14b610330 | `Hero_Siege.exe+0xb610330` |
| The same pass, reporting | 0x14b610b30 | `Hero_Siege.exe+0xb610b30` |
| Main layer pass | 0x14b60d070 | `Hero_Siege.exe+0xb60d070` |

The measured split and these readings, without addresses, are folded into
[`docs/RUNTIME_DATA_MODELS.md` § 5.12](../../docs/RUNTIME_DATA_MODELS.md#512-frame-thread-time-by-phase).

## Not done here

- **Candidates 2-4 (DXVK, a snapshot-worker-apply GML offload, child
  processes).** Nothing is installed, offloaded or spawned here. This research
  only states the most each one could win; each gets its own workorder, written
  against that record.
- **The working set of an idle child process** (asked for under candidate 4).
  Launching a second game instance risks Steam's single-instance check and the
  real saves, and `docs/RUNTIME_DATA_MODELS.md` § 5.9 already measures the
  game's own private bytes. Candidate 4's workorder decides whether to measure
  it.
- **Each thread's start address, measured live.** Recording it in `frameprof`'s
  thread table would turn the thread readings above into measurements, but it
  is a plugin change, and this research changes no plugin code. It goes with
  the first later workorder that changes `frameprof`.
- **Names beyond the listed functions.** Only the per-frame function, its other
  branch, the two dispatchers and five runtime-only callees of each are named.
  Naming more of the runtime goes to whichever candidate's workorder needs it.
- **Whether `SaveFileGMAsync` reaches `buffer_save_async`.** It needs the
  `HookBuiltin` counter described above, in a live session; no candidate here
  depends on it.
