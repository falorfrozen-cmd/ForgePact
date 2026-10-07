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
