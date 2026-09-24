# Early population capacity experiment

> **Read first (2026-09-22):** [population-performance-analysis.md](population-performance-analysis.md)
> explains why the lag survives an empty queue: the game walks every living
> monster several times per frame (minimap layer, health bars, draw, the
> 30-frame box rebuild), so the cost scales with the number of monsters kept
> alive, not with how they were born. Further pacing work cannot fix that.

## Latest capture and local refinement (2026-09-22)

The `f2b0444b...` profile finished its 90.004660-second capture with all 13
script timing hooks attached through both routes. The player reports some
improvement; this was not a controlled same-map comparison. No allocation or
invalid-handle failures were recorded. Peak frame interval was 309.603ms.
Measured same-room interval estimates reached 4.31ms/frame for hunt activation,
4.20ms for native enemy stepping and 4.14ms for health-bar drawing. These are
inclusive wall-time samples, not additive/exclusive CPU percentages.

First-room pending groups fell 71 -> 25 -> 0 while admissions stayed 489.
Second-room pending groups fell 42 -> 0 while admissions stayed 326. The old
240-frame pruning erased callers that stopped polling. **This does not prove
they never spawned:** they could have spawned naturally, become inactive or
been removed. None of those outcomes was measured. Earlier queue-zero snapshots
in this document must not be read as complete-population evidence.

The current refinement:

- Keeps expired identities as `unconfirmedPacks` until a later admission or
  zone/toggle reset. The panel and profile expose this independently of the
  active queue. Unknown results do not extend the population window forever;
  readiness/capacity limits and actual native spawning are unchanged. This
  fixes accounting, **not** the still-unproven cause of stopped native polls.
- Maintains expiry order, so frame housekeeping inspects only expired entries
  plus the first live deadline rather than every pending caller each frame.
- Immediately admitted packs allocate no temporary queue nodes. The small
  per-frame identity vector reserves once and keeps repeated-call semantics.
  The allocation-count target failed at 97 allocations for 32 fresh admissions
  before this fast path; it passes with zero afterwards.
- Hunt activation reads numeric/REF identities directly through the runner's
  RValue conversion, retaining the variable lookup for object-valued handles.
  Native-active identities use a sorted contiguous snapshot instead of a
  hash node per enemy. In the production-body test, 4096 already-active enemies
  still require 4096 ordinal lookups, but redundant ID-variable reads fall from
  4096 to zero. Mixed rarity/range, unsorted IDs and all handle kinds preserve
  the original active set. This is a call/allocation result, not an FPS result.

Five-second completion, improved live FPS and the reason for silent groups
remain unverified. The candidate adds no watcher/thread or forced spawn/step
event. Player builds still omit all profile-only AI/draw measurement hooks.
Focused loop: `test_beacon_wake_behavior.py`, `test_population_capacity.py`,
`test_map_reveal*.py`, `test_adaptive_population.py` and
`test_population_profile_summary.py`. All original readiness, native-active,
capacity, density and deferred-call invariants remain required.

## Local v3 candidate: selection cost and native birth evidence

The v2 player run (`428dcffc...`, 2026-09-22) recorded its last scheduling
progress at 10.807 seconds, a 309.311ms peak frame, a 123.579ms peak outer
creation, 399 admissions and 29 unconfirmed groups. No allocation or invalid
handle failures were recorded. The closed-window frame value was latched, not
current FPS. This run does not meet the five-second target.

`DeferredDensityCopies` now uses a distance heap for ready jobs and a deadline
heap for retrying jobs. Distances rebuild when the player's position changes;
all selections in a batch reuse them. New placements join at their proper
distance and due retries rejoin before selection. Equal-distance ties use
insertion order. Generation checks and completed-copy bits still prevent stale
work and cumulative density growth on revisits. The production helper fixture
with 4096 placements and 32 selections performed 261152 coordinate reads before
the change and 8192 afterwards. This measures queue work, not game FPS or native
construction speed. Pending counts include both heaps.

`PopulationBirthScope` observes only existing native `instance_create_*` calls,
with no additional hooks. For an enemy created by a tracked creator it captures
the numeric caller identity before the call, then resolves the waiting record
only after the original path returned successfully with a valid numeric/REF
instance identity. Caller pointers are not read after creation. Unknown callers,
non-enemies, failed returns, missing/throwing originals and reset generations do
not resolve anything. The manager also checks the original full map identity.
`observedNativeBirthPacks` counts these resolved waiting groups separately from
admissions. One observed birth does **not** establish a complete pack, and groups
without evidence stay unconfirmed. This addresses false unknowns from vanilla
near-player spawning; it does not prove that the previous 29 groups took that path.

The spawn-window clock stops when scheduling closes or a pending pass times out,
without clearing unresolved identities. Previously `passElapsedMs` alone changed
the serialized state every 30 frames even after scheduling ended, defeating the
unchanged-file check. The production-manager fixture reproduced the moving
closed-window clock before the fix. This is an avoidable disk-write reduction,
not attribution of the previously observed large stall to disk I/O.

`peakNativeCreateObject` and `peakNativeWasDensityCopy` accompany the existing
peak duration. They are taken from the outer call's scalar argument, never an
extra world scan. Nested child creation cannot overwrite its parent's identity.
The duration includes plugin work within the creation hook and nested calls; it
must not be described as exclusive native CPU time. No per-call log, extra
watcher/thread or event replay was added. The player DLL still omits the bounded
profile recorder. New checks are in `density_queue_cost.cpp`,
`test_population_birth.py`, the admission/manager harnesses and the nested timing
fixture. Live five-second completion, all-pack completion and FPS gain are pending.

## Earlier measurements

Local candidate, 2026-09-21. The first live zone completed without reported
allocation failures, but population took 15-20 seconds according to the player.
The faster follow-up admitted 730 groups over 4479ms in a later live snapshot,
used 20832 overflow records without allocation/handle failures, and still caused
a player-reported 2-3 second entry hitch. Its between-admission time limit never
fired. These are snapshots, not a controlled before/after benchmark. The adaptive
follow-up later recorded 56.1 seconds between first/latest admission, 350 admitted
groups, 216 waiting and a 250us work budget. It was too conservative. The current
local revision targets five seconds but FAILED live performance acceptance on
2026-09-21/22; it does not promise zero hitches or a hardware-independent deadline.

The candidate DLL with SHA-256
`10cc32793be59e7322338594b8152572c5ebf221818812acf489024aca3d10c9`
was tested at 4x density. The player reported severe lag without a crash.
At 14.231 seconds from pass start, 573 groups still waited and the latest frame
interval was 147.382ms. Later samples had both queues empty, 1047 admitted groups,
1902 cumulative density copies and 85885 live overflow records, yet frame
intervals were 346.814-410.922ms. Allocation and invalid-handle failures stayed
zero. Admission ended 40.151 seconds after the first admission; these counters
are not a complete enemy census. A later cleared/new pass had zero admissions
and overflow records and must not be compared as if it were the same map.

The persistent lag is not explained by construction timing alone. The distance
hook stays active for a frame-based window even after known queues empty; both
that path and ongoing native AI/render work remain candidates, not proven causes.
An attempted external WPR CPU capture failed before recording with policy error
`0xc5585011`; no CPU sample attribution exists from that attempt. No toggles or
game binaries were changed during the follow-up diagnosis. The development stall
watchdog is compiled out of this player build, but functional hooks remain active.
Do not present the synthetic five-second test below as live acceptance or raise
the creation budget again as if queue throughput alone explained this failure.

The subsequent local profile DLL (`5fe5b8f9...`) completed a 90.207489-second
capture. While packs remained queued, interval estimates were 13.47-82.98ms per
frame in outer native creation, 0.87-58.05ms in protected allocation (nested
inside creation), 0.25-0.88ms in the extra distance-hook path, 0.10-0.45ms in the
plugin frame callback, and at most 0.019ms in the helmet tick. Later same-room
intervals with no pending packs/copies and a closed distance window still took
63.28-99.12ms per frame, while these measured paths were individually below 1ms.
This rules out attributing all lag to these measured paths; it does not separate
native AI/rendering from unmeasured hooks or GPU/OS waits. Costs overlap and are
wall-time estimates, not exclusive CPU percentages.

The player also reported that the first map did not populate, but the next did.
The working map had already admitted 1002 creators before a manual `reveal 1`
refresh was sent, so that refresh did not prove a fix. Initial-map arming remains
an open issue. A separate-process native-library benchmark rejected one proposed
explanation for allocation cost: full-bank allocation failure returned in about
0.286us versus 2.06us for an ordinary allocation, including Python call overhead.
Do not optimize a supposed repeated full-bank scan on that disproved assumption.

## Failure addressed

A preserved Hero Siege 7.0.13 crash, with density 2 and early population enabled,
exhausted the native protected-variable store: 262144 live records. Allocation
returned -1, and enemy construction passed that invalid handle to the native
setter. The fault occurred while a creator constructed a monster. Pacing alone
cannot fit more simultaneously living monsters into the same fixed store.

This evidence identifies a storage failure. It does not verify the separate
Miner's Helmet reward or cosmetic-pulse experiment.

## Capacity and pacing

`ProtectedPoolRuntime.hpp` recognizes one exact installed native-library hash:
`ea33261a54ba922b4074ae211990087c3a74fa840ead44ab30f0c74e504c505a`.
It resolves and validates the ten named exports, copies that local library to
`bp_ipc/population-cache/<hash>/`, and loads independent instances. No game
library, copied game implementation, or decompiler output belongs in the source
repository or release package. The copies are created only from the user's
installed library at runtime. Unknown versions refuse early population explicitly.

`ProtectedPoolRouter.hpp` preserves the original handle space as bank zero and
routes additional numeric handles to the new pools. Allocation, reads, writes,
freeing, undefined values, key changes, protection and native checks all use the
original functions belonging to the correct bank. Checks are never replaced by
success constants. Existing live bank-zero values are not reset at installation.

All consumer detours must attach before the allocator can issue extended handles.
A partial installation stays an inert forwarding layer. The existing drop-rate
getter remains the one getter detour: it calls the router before applying its
usual global-key overrides. An overflow handle whose local slot happens to be
175, 177 or 178 must not receive those global drop modifiers.

Two additional pools are prepared before activation. Later reserve growth happens
in the existing frame callback (or on allocation if needed), up to 16 pools total.
Allocation/free/growth metadata is serialized; ordinary reads do not acquire that
lock. There is no background watcher or whole-world population census. Pools stay
loaded until process exit even if the map toggle is disabled, because existing
monsters may still hold their handles. Overflow frees reuse storage across zones.

The native reset entry retains its original bank-zero behavior. Overflow reset
frees the adapter's live entries instead of repeatedly allocating new backing
pages. The native initializer does not rewind a spent allocation cursor, so a
post-reset handle need not be zero; tests check valid reusable storage instead.

The native memory check requests a 4096-byte query buffer despite reserving less
in its own frame. A shallow C++ caller reproduced ERROR_NOACCESS even for correctly
protected pages; a caller stack reservation makes the same native check succeed.
`NativeMemoryCheck` supplies committed caller stack space and preserves the native
return value. This behavior was found by the compiled native harness; the earlier
Python probe alone did not expose it.

`PackAdmissionQueue.hpp` stores numeric creator identities, never retained game
pointers or deferred native events. Ready callers do not wait behind absent
callers. `AdaptivePopulationBudget` uses a five-second target measured from the
first readable minimap for the pass, not from the loading-screen transition.
Remaining original/density groups, queued work and typical frame duration choose
a 4-8ms work budget. A one-second allowance leaves space for staggered native
polls; the last 1.5 seconds use the upper budget. There are at most 32 admissions
and 32 density-copy attempts per frame. Copies share at most one third of the
work budget while the early-population pass is active.

Frame feedback uses a recent median rather than the old minimum, with an explicit
threshold so ordinary 50fps jitter does not repeatedly halve the budget. A native
burst exceeding 100ms gets one recovery frame; there is no permanent cooldown.
Actual native work and pack reservations overlap instead of charging the same
work twice. Copy cost remains separate. The 32-group/8ms between-grant outer guard
is retained. At least one indivisible pack may progress on an otherwise busy
frame, so the budget is not a hard wall-clock frame-duration guarantee.

`PopulationNativeScope` measures the outermost native `instance_create_*` call
while population work is active, including nested construction exactly once.
Actual work and pack reservations share one budget. This also sees delayed
construction and its effect on later frames, which a clock checked only between
permission requests missed. Native events still run in their normal context;
none is interrupted, manually replayed or moved to a worker thread. Near-player
natural spawning remains native. There is no new watcher or object-event hook.

Density's extra creator copies used to run synchronously in the original
creation call, outside the pack-admission guard. Four-argument, scalar-only
calls now queue copies; the original still returns synchronously, unchanged.
`DeferredDensityCopies` retains a placement plan plus completed-copy bits.
Ready-map processing prioritizes the player's nearby placements, leaving budget
for pack admission. Layer/depth and object values are retained only after
rejecting object/array/struct arguments; caller contexts are stored as numeric
IDs (or global/null), re-resolved immediately before each native call.

Runnable density jobs are cleared on room/lifecycle changes. Incomplete plans
resume only when the game restores the original placement, and completed copies
are not repeated. Full ZoneState reset clears both plans and placement guards.
An unavailable caller waits rather than being replaced with a different caller.
Uncertain native-call failures are recorded and not retried. Unknown call shapes
(including optional initialization structs), oversized multipliers and unsupported
contexts retain the prior synchronous path and increment a fallback counter.
Those paths can still cause a hitch; live validation must check the counter.

The 900-frame reveal window extends while pack or density work remains. A
completed density plan records native calls completed, not a live enemy census.

Early admission waits if the reserve is below 16384 slots. A failed native
allocation stops further early admission for the session. Finite memory, very
large packs, natural spawning and native AI/rendering still have limits. The
controller checks between indivisible native calls; it cannot enforce a hard
frame duration or constant FPS. A single expensive base-game call or the ongoing
AI cost of four times as many monsters can still exceed the frame budget.
The five-second target never bypasses readiness or storage safety, cancels groups,
or causes an unlimited catch-up loop. A miss is latched and reported. Admission
and density-copy timing is not proof that every native enemy finished its own
initialization, nor a census of every monster visible on the minimap.

## Verification and diagnostics

### Bounded local performance capture

`plugin_build\build.bat profile` produces `BloodPactPlugin_profile.dll` in its
own object/output directory. It defines `FORGEPACT_RELEASE` plus the explicit
local-only `FORGEPACT_POPULATION_PROFILE` switch. It never copies this DLL into
`modfiles_shipped` or `dist`; the usual release build contains no profile recorder.
The recorder itself does not change the spawn policy. The current local source
also contains the readiness and redundant-walk fixes described below.

The first ready population window starts one 90-second capture per launch.
Sampling and logging stop automatically on the existing frame callback. There
are no new threads, commands or stall watchdogs. The local profile build adds
13 named-script timing hooks for enemy steps/timers, pathfinding and minimap/
enemy drawing. Each delegates its original call exactly once, unchanged.
`scriptCoverage` records table and native-detour attachment separately: an
unresolved or table-only path with zero calls is **not** proof of zero cost.
Existing protected reads
and writes are timed 1/64 calls, distance paths 1/16, and callback substeps each
time. Counts and sampled microseconds go to `bp_ipc/population-profile.jsonl`
at most once a second plus the final record. A closed or stalled process can
leave an incomplete capture; a missing final record must not be called complete.

Run `py -3 tools/summarize_population_profile.py <path-to-jsonl>` to calculate
interval estimates and show zone transitions. Measurements are inclusive wall
time, not sampled CPU stacks. `distance_extra` starts after the original native
distance call; `distance_native` measures that original separately. Protected
operations include the original native operation; nested protected/native work
also occurs inside callback/creation metrics, so categories must never be summed
or subtracted from frame time to claim exact native AI cost. `native_create`
reuses the existing outer-creation timing while the population budget is active.
Sampling can be biased and affects execution slightly; use it to locate large
costs, not to claim zero overhead or exact percentages. These are profiling-only
artifacts, and no live result is implied by their successful build or unit tests.

The expanded capture also times existing Tyrant/Beacon scan, leash, activation
and path-start hooks. Frequent per-enemy paths are sampled 1/16. Once per report,
Windows process and frame-thread CPU counters are captured separately from wall
time; the summary preserves unknown/missing counters and thread changes. This
helps distinguish CPU work from waits without claiming which unmeasured subsystem
caused a wait. Those calls require no external profiler privilege.

### Local follow-up: late creators and repeated activation walks

The production-manager regression reproduced two failures before the change:
an empty initial creator list permanently cancelled the pass, and an unready
first creator blocked ready siblings. The pending pass now survives an empty
poll until its existing bounded timeout. Up to 32 candidates are checked per
poll with a rotating cursor. Each admitted creator still passes its own readiness
and capacity checks; no create, alarm or step event is replayed manually.

Tyrant/Beacon activation keeps the same active set. If activation added no
instances, the second full lookup walk is redundant and is skipped. Unlimited
all-enemy activation skips both filter walks. The actual production function's
harness verifies mixed rarities, range boundaries, native-active preservation,
repeat calls and empty rooms. With 4096 already-active synthetic enemies, the
old implementation did 8192 ordinal lookups plus 8192 ID reads; the new one does
4096 of each. This is a call-count reduction in that case, **not an FPS claim**.
No feature, monster or density copy is removed to achieve it.

The completed earlier 90-second capture showed 63.28–99.12ms frame intervals
after both queues emptied in the same room. Individually measured native creation,
allocation, distance-hook and frame-callback paths were each below 1ms/frame
there. Native AI/draw and hunt activation were not covered, so attributing the
remaining lag exclusively to native AI or declaring all hooks cheap was unwarranted.
This is why the expanded capture measures those paths together instead of
disabling gameplay features for successive user tests. Live acceptance remains open.

- Compiled small-pool baseline/overflow/free/reset/failure tests and admission
  test cover the production helpers. Failed native checks propagate unchanged.
- The optional real-library harness opens 786449 simultaneous records across four
  pools, reads every value, exercises boundary operations, performs two full
  teardown/reuse cycles and reset, and rejects malformed handles.
- Ten injected hook-install failures prove no extended handles escape incomplete
  setup. Optional real MinHook tests call the patched exported addresses and the
  verbatim production drop getter, both with and without a preinstalled getter.
- The production MapRevealManager + distance-hook harness covers unavailable and
  recovering capacity, continued fog reveal, transitions/readiness, 2100 groups
  surviving a capacity pause beyond 900 frames, and eventual window closure.
- The helper's old fast-policy fixture remains an outer-guard regression; it
  is no longer a claim about production timing. The real manager/hook fixture
  checks eventual completion of 448 staggered groups under the adaptive policy.
- `test_adaptive_population.py` compiles both production helpers and the actual
  density adapter/DoMultiCreate bodies. It covers pacing, shared budgets, slow
  frame recovery, nested timing, nearby priority, original-call preservation,
  missing callers, map/capacity gates, transitions/resumption, no cumulative
  density growth, and native fallback for unknown initialization arguments.
- Player DLL build and general regressions must pass before local installation.
  These tests do not substitute for a live run through the previously failing
  map at the same settings, followed by ore/reward verification.
- `population_deadline.cpp` models 384 original groups and exactly 1152 density
  copies, delayed/retried native polls, 50fps cadence and native work. The old
  adaptive policy failed the five-second assertion; the adjusted policy serves
  all 1536 groups at 3.24015 simulated seconds. This is a deterministic fixture,
  not a live-game benchmark. The real manager/distance-hook harness also checks
  the five-second target without bypassing its readiness/capacity guards.

Run `py -3 -m unittest discover -s tests -p test_population_capacity.py -v`.
For local native tests, set `FORGEPACT_POOL_TEST_DLL` to the supported installed
library, and optionally `FORGEPACT_MINHOOK_SOURCE` to a local MinHook source tree.
Those tests run in separate test processes, never against the running game.
No native-library test is silently presented as executed when its input is absent.

`bp_ipc/modstate.json` includes `population`: capacity readiness, usable reserve,
bank count, overflow live records, allocation/invalid-handle failures, queue size,
admitted creators, window frames and a reason. It also records peak admissions
per frame, frames that exhausted the time budget, and milliseconds from the
pass's first admission to its latest admission (not full map loading time).
Adaptive diagnostics add queued/completed density copies, synchronous fallbacks,
copy failures/context wait reasons, current work budget, last/peak frame intervals,
peak outer native-create duration, measured call count and slow-frame count.
`targetMs` is 5000; `passElapsedMs` starts when the minimap arms the pass,
`lastProgressMs` records the latest scheduled copy/admission, and `targetExceeded`
latches an observed backlog or scheduled work past the target until the next pass.
None of these fields is called full-map completion, which has not been measured.
These are bounded counters updated from the existing callbacks, not per-call logs.
The panel shows unavailable/paused
population instead of treating the saved checkbox as proof it worked. Log entries
record setup refusal and pool growth. Mining and other plugin state stay separate.

## Local v4: caller reuse and generation measurement

Current installed-game metadata independently confirms that the v3 peak object
3259 is `objZoneGenV2`. The game image's code/data sections match the private
analysis image. This does not attribute all 89.971ms of the outer creation hook
to native generation CPU time; nested game and plugin work are included. The
inspected preset construction uses new instances immediately to assign their
properties, so postponing those creates is not a safe queueing optimization.
This candidate does not rewrite any native generation step or timer.

`CreationCallerInfo` shares one lazily read caller object index between the
existing birth observer and enemy-born guard, within one invocation and before
the original call. It keeps no identity across invocations/frames and does no
destructor work. A production-body fixture measured 2000 -> 1000 caller-object
reads per 1000 births, with nested enemy-chain flags and decoration/all-off paths
preserved. Density, copy scheduling, native arguments and readiness/capacity
guards are unchanged. No new player hook, watcher or thread was added.

The local profile has 22 named script timing hooks: the previous 13 plus preset
data, key presets, heat map, preset objects, ground presets, zone walls,
autotiling, static path blockers and zone-state restore. Table/native coverage
is explicit: verify successful attachment and actual hits before interpreting a
zero. These diagnostic hooks and the recorder are absent from player builds.

Previously capture began only when ready creators armed the spawn window. The
first `PopulatePresetData` call now starts the same bounded 90-second capture
before native work. Ready creators remain a fallback for maps that do not use
this generator. `captureStart` records which trigger won; subsequent triggers
cannot reset the clock or restart a completed capture. A new-format summary
retains work before the first Present report. This initial interval has unknown
starting room/CPU time and may be a partial frame. Old captures without start
metadata keep the previous interval behavior. Inclusive timings overlap and must
not be summed.

The inspected v3 run at 4x reported last scheduled progress 7.694s, peak frame
232.430ms, 95 tracked creators observed starting native births and zero remaining
unconfirmed tracked identities. This was a different map, not a controlled
comparison or full-pack census. V4 has no live performance acceptance yet;
the five-second target and stable frame time remain unresolved.
