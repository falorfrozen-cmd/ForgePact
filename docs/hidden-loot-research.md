# #95 part 2: what a filter-hidden ground item costs, and how long it lives

Issue #95 asks whether ground items the loot filter hides cost anything, and
whether they should be dropped. Part 1 (in
[dev2-bug-batch-research.md](dev2-bug-batch-research.md#95-part-1-what-a-hidden-ground-item-still-costs))
established that a hidden item stays a live, invisible `Loot_Ground_obj` that
re-checks its visibility every 0.3 s, but it measured no frame cost, and it did
not measure whether a hidden item runs its Draw event, outlives the zone or
reaches the save. Part 2a, the workorder `forgepact-issue-95`, answers those
questions: a static reading of the object first (below), a cost model built
from far scenery sleep's measurements, four research-build instruments
(`lootspawn`, `lootsleep`, `loothide`, `lootshow`) plus a wider `lootcensus`, and
one live session that puts about 3,000 hidden items in a zone and compares the
frame thread with them awake and with them asleep. Whether a mod is built at all
(part 2b, the workorder `forgepact-issue-95-mod`) was decided from those
numbers: the owner decided to build it, and [The mod](#the-mod) describes what
was built.

Everything below is labelled. **Measured** means observed on the running game,
and each measurement names the check of the session that took it. **Static
reading** means read from the game's compiled code in a local decompiler and
written here in our own words; no game code is quoted (AGENTS.md § Legal).
**Model** means arithmetic from earlier measurements, not a measurement of its
own. A **reading of our own code** is ForgePact's source. A thing that was looked
for and not seen is written "not observed", never "does not happen". The game
facts established here are also recorded in the hub's
`docs/RUNTIME_DATA_MODELS.md` §18.5, the shared record every module reads.

## Status

| Question | What is established | By what |
|---|---|---|
| What a hidden item is | A filtered item stays a live, invisible instance; 281 of 291 ground items were hidden at a strict filter | **Measured**, part 1 ([dev2 bug batch, #95 part 1](dev2-bug-batch-research.md#95-part-1-what-a-hidden-ground-item-still-costs)) |
| The object's events, its parent, where the filter verdict is computed, and that Alarm 9 never re-runs the filter | See [Static reading](#static-reading) | **Static reading**, 2026-09-28 |
| Whether a hidden item runs its Draw event | No. 2,736 hidden items ran Draw 0 times in 10 s; the same items, shown, ran it 1,477,440 times | **Measured**, Live 1's `hidden-draw` with its positive control `visible-draw` ([Live 1 results](#live-1-results-2026-09-28)) |
| What a hidden item costs per frame | About 2.4-5.6 µs of frame time per hidden item per frame at 2,736 items, three to seven times the model's 0.4-0.8 µs | **Measured**, Live 1's `cost-hidden-working` and its `perf` annex `cost-hidden-perf`; the prediction was a **Model** ([below](#model-what-a-hidden-item-should-cost)) |
| Whether hidden items outlive the zone | No. The zone's end ran Clean Up on 809 of 809 hidden items, and none were left | **Measured**, Live 1's `zone-end-cleanup` and `zone-change-gone` |
| Whether hidden items reach the save | **Not observed.** `saves-diff`, read with 2,736 hidden items still on the ground and before any zone change, is the check that answers it; its control failed. `reload-none` landed in town | Live 1's `saves-diff` and `reload-none`, both `not-observed` |
| A mod that hides, sleeps or refuses filtered items | **Sleep loot your filter hides** (`hiddenloot`, part 2b, workorder `forgepact-issue-95-mod`), off by default: a drop the game's filter hides is put to sleep at the end of its frame, and shown while a key is held. It refuses nothing and decides nothing itself. Part 2a (`forgepact-issue-95`) shipped no mod | **Reading of our own code**; harness-verified 2026-09-28 (`tests/test_hidden_loot_behavior.py`); **not observed live** yet: Live procedure 2 of `forgepact-issue-95-mod` is pending ([The mod](#the-mod)) |
| That all three ground-drop entry points reach `LootGroundInit` | `LootGroundCreateFromItem` and `LootGroundDrop` call it; `LootGroundCreate` names it as a callee | **Static reading**, 2026-09-28 ([The mod](#the-mod)); whether the hook sees the game's own calls is for Live procedure 2 |

Live 1 of `forgepact-issue-95` ran on 2026-09-28; its results are in
[Live 1 results](#live-1-results-2026-09-28). Part 1's own record stays in the
[dev2 bug batch](dev2-bug-batch-research.md#95-part-1-what-a-hidden-ground-item-still-costs);
this document is where it continues.

## Static reading

Read 2026-09-28 in the local, named Ghidra project (AGENTS.md § "Check for a
Named Ghidra Project"). The compiled-code table rows for `Loot_Ground_obj` and
its parent were found by searching the image for each event's name string and
the table row that points at it. Every fact below is a **static reading**, in
our own words; what the decompiler showed stays on the researcher's machine.

- **`Loot_Ground_obj` (SDK index 2513) owns five events: Create, Destroy,
  Alarm 9, Draw and Clean Up.** It has no Step event and no Alarm 4 event. Its
  parent is `Pickup_Parent_obj` (3421), which owns Create, Alarm 9 and Clean
  Up, and no Step either. The Create event sets `alarm[4]` to 1 (part 1's
  "alarm 4"), but neither the object nor its parent handles that alarm, so it
  counts down and nothing runs. **Static reading.**
- **Create binds three closures as methods and runs none of them.** By role:
  a rare-drop announcement closure (it calls `GetRareDropAnnouncement` and
  sends to chat); the **loot-filter closure**, bound as `m_LootFilter`, which
  reads `global.loot_filter_new` and calls `LootFilterAffixTierVisible` and
  `LootFilterAffixTierHighlight`; and a small dispatcher that calls
  `LootGroundRelicStep` or `LootGroundDeActiveStep`. Their `anon@N` numbers
  move with every game patch (hub guide §5.3), so they are named here by role,
  not number. **Static reading.**
- **The filter verdict is computed inside `LootGroundInit`.**
  `LootGroundCreateFromItem(x, y, item)` calls `CreateLootInFreePos` to make the
  instance and then `LootGroundInit(instance, item)`. `LootGroundDrop`, the
  path of a player dropping an item from the bag, calls `LootGroundInit` too.
  `LootGroundInit` reads the bound `m_LootFilter` method off the instance and
  calls it, behind a guard whose condition was not read (`skipLootFilter` is
  the obvious candidate). So by the time `LootGroundCreateFromItem` returns,
  the game's own `lootFilterVisible` verdict is already on the instance.
  `LootGroundCreate`, the other entry point, also calls `CreateLootInFreePos`;
  whether it reaches `LootGroundInit` was not read to the end of its long body.
  **Static reading.**
- **Alarm 9 never re-runs the filter.** It reads `lootFilterVisible`, calls
  `OnScreen`, sets the built-in `visible` from the two, re-arms itself for
  0.3 s of game speed, and counts `itemCompanionTimer` down. It calls no
  method. So once an item is hidden, nothing in the item itself asks the
  filter again. **Static reading.**
- **The game re-evaluates items already on the ground from its own menu
  path.** `LootFilterImport` references the `m_LootFilter` slot (one site),
  and the loot filter's option-screen closures call `LootFilterItemTypeCheck`
  for the menu. That is the route by which part 1's Live 1 saw `hidden` fall
  from 281 to 0 when the owner turned the filter off. How that pass finds the
  items was not read. If it walks them the way GameMaker's `with`,
  `instance_number`, `instance_find` and `instance_exists` do, it cannot reach
  a deactivated instance, since the runner skips those; that matters to any
  mod that puts hidden items to sleep. **Static reading** for the two
  references; the walk is not established.
- **Draw.** The Draw event tests one instance variable through one built-in
  and then calls `LootGroundDraw`. **Static reading** of the event body only.
  That a GameMaker runner does not run the Draw event of an instance whose
  `visible` is false is the engine's documented behaviour, not a reading of
  this build's runner; that the draw pass still walks such an instance is far
  sleep's finding
  ([far-sleep-research.md](far-sleep-research.md#what-sleeps-and-what-never-does):
  hidden is not asleep). Live 1 measured the first half on this build: a
  hidden item's Draw did not run (`hidden-draw`, with `visible-draw` as its
  positive control; [Live 1 results](#live-1-results-2026-09-28)).
- **Save.** `SaveSlotFunc` is a one-line wrapper around `SaveSlot`, whose body
  was not read. Whether ground items reach the save was left to Live 1's
  `saves-diff` and `reload-none`, and both read not observed. **Static
  reading** of the wrapper only.
- **Our own hooks.** The research build, not the player build, table-hooks
  `LootGroundCreate` and `LootGroundCreateFromItem` (`InstallItemInspectHooks`,
  through `HookOneScriptTable`, both inside `#ifndef FORGEPACT_RELEASE`); the
  player build has neither hook. The angelic probe reports both rows as under a
  table-only hook, so they are blind to this build's direct calls. Live 1's
  research DLL installed both (its boot log printed `HOOK INSTALLED` on each),
  and their bodies re-run the original once per drop multiplier, so whether
  `lootspawn`'s calls pass through them matters only above x1; the session read
  every drop multiplier at x1 (`dropstats`) before spawning, and each
  `lootspawn 1000` answered `made=1000`. **Reading of our own code**, with the
  session's `dropstats` line.

## Model: what a hidden item should cost

**Model**, not a measurement. Far scenery sleep measured the runtime's
per-instance walk cost on this build: putting 4,282 hidden props to sleep saved
1.6-2.4 ms of a 16.7 ms frame
([far-sleep-research.md](far-sleep-research.md#what-sleeping-the-far-props-saves)),
which is **0.37-0.56 µs per instance per frame** for the alarm pass, the step
upkeep and the layer walks together. A hidden ground item is the same kind of
instance: hidden, owning an alarm, no Step event. On top of that it runs its
own Alarm 9 every 0.3 s, about every 18 frames at 60 fps; an `OnScreen` call
and a few variable reads and writes, say 2-5 µs a run, adds 0.1-0.3 µs per
frame once spread over those 18 frames.

**The model: 0.4-0.8 µs per hidden item per frame.** It assumes the cost is
linear in the number of items, which the session does not test; the write-up
scales the measured figure back to realistic counts on that assumption, and
says so.

- **Part 1's 281 hidden items:** 0.1-0.2 ms per frame, under 1.5% of a 60 fps
  frame. That is inside the capture-to-capture noise of `frameprof`: far
  sleep's A/B captures varied from 52.8% to 59.4% `working` with nothing
  changed. A window of 281 items, however clean, would have answered nothing,
  which is why part 1's "isolated strict-filter window" is not repeated as
  such.
- **3,000 hidden items:** 1.2-2.4 ms, 7-14 points of `working`, the same order
  as the far-sleep effect, which showed clearly in two alternating pairs.

So Live 1 spawns about 3,000 hidden items on the back end and compares them
awake and asleep in two alternating pairs, on the same instances. The per-item
cost is the awake-minus-asleep delta divided by the count.

Live 1 measured three to seven times the model's upper bound; the likely gap,
the `Loot_Manager_obj` Begin Step the model left out, is discussed in
[Live 1 results](#live-1-results-2026-09-28).

## Live procedures

Each procedure's step-by-step lives in the workorder that runs it
(`forgepact-issue-95`); this is the summary. The session runs on slot 14
("Sorak", the slot every #95 session used), backs the saves up before the
launch and restores them after, and must pass `dll-hash`, `marker` (the boot
line `==== BloodPact plugin loaded ==== v2.0.1`) and `control` (`ping` answers
`pong`, and `lootcensus` answers a line with a `walked=` field) before anything
else it records counts. Person actions are short hand-backs: walking into a
zone and standing still at the owner's usual strict filter, and, after the
reload, walking into a zone again and later taking an exit.

**Live 1, the cost and lifetime round** (the research DLL,
`plugin_build\BloodPactPlugin_rel.dll` from `plugin_build\build.bat dev`; run
2026-09-28). Far sleep is off for the session. After a baseline `frameprof`
capture, three `lootspawn 1000` calls drop one equipment template 3,000 times
around the player; `loothide` is the fallback if the owner's filter happens to
show the template. `evcount` counts the Alarm 9, Draw, Clean Up and Destroy
runs of `Loot_Ground_obj`. Two alternating pairs of 15 s `frameprof` captures
with `perf` averages then compare the items awake and put to sleep by
`lootsleep 1` / `lootsleep 0`, followed by one capture with the items made
visible by `lootshow`, the same-session control `cost-visible-working`. The
items are hidden again and the game is stopped with them still on the ground,
so the save it writes on the way out is the one `saves-diff` reads, before any
zone change; the character is then reloaded (`reload-none`). The zone exit is a
step of its own: in a zone, a fresh `lootspawn 1000` is hidden, and the owner
takes an exit while `evcount` counts Clean Up and Destroy. Its checks, in
order: `dll-hash`, `marker`, `control`, `spawn-count`, `spawn-hidden`,
`alarm9-runs`, `hidden-draw`, `visible-draw`, `cost-hidden-working`,
`cost-hidden-perf`, `cost-visible-working`, `sleep-wake-roundtrip`,
`zone-end-cleanup`, `zone-change-gone`, `reload-none`, `saves-diff`. The first
three, `spawn-count`, `alarm9-runs` and `sleep-wake-roundtrip` have to pass for
the session to count; the rest are research checks, and their verdict is the
finding.

The workorder sets its `cost:` route from `cost-hidden-working` alone: pass,
`cost: measured`; fail, `cost: none`; anything else, `cost: not-observed`. A
fail needs the control `cost-visible-working` to have resolved a delta on
`working` in the same session, so a flat result beside a flat control reads not
observed rather than "no cost". `cost-hidden-perf` does not route: `perf` reads
the game's frame interval, which the 60 fps limiter holds at about 16.67 ms
whenever the frame has time to spare, so it is recorded as an annex and scored
only when the frame is over budget. The per-item cost by `working` is
`(delta points / 100) × 16.67 ms / N`.

## Live 1 results (2026-09-28)

Slot 14 ("Sorak"), the research DLL with sha256 `1298187c...3364` (ForgePact
`034aef7`, `build.bat dev`, boot line `v2.0.1`), at the owner's strict filter.
The saves were backed up before the launch and restored afterwards, and the
restored set inspected identical to the backup. The six checks the session
needed all passed; of the ten research checks, six passed, two failed and two
read not observed. The workorder's route is `cost: measured`.

| Check | Verdict | What was seen |
|---|---|---|
| `dll-hash` | pass | The lease hashed the installed DLL as `1298187c...3364`, the hash the session was dispatched with |
| `marker` | pass | `==== BloodPact plugin loaded ==== v2.0.1` on both launches |
| `control` | pass | `pong (YYTK 4.0.1)`, and `lootcensus: ground=0 hidden=0 invisible=0 coins=0 walked=0 ...` before any count was recorded |
| `spawn-count` | pass | Three `lootspawn 1000` calls, each `made=1000`, left `ground=2736`: 914, 912 and 910 new ground items per call, with `return-unreadable=` 86, 88 and 90 |
| `spawn-hidden` | fail | The game's filter showed the copies (`hidden-now=0`); the fallback `loothide` answered `set=2736`, and `lootcensus` then read `hidden=2736 invisible=2736`. N = 2,736 from here on |
| `alarm9-runs` | pass | Alarm 9 ran 51,552 times in 10 s, above 10 × N (27,360) |
| `hidden-draw` | pass | Draw ran 0 times in the same 10 s. No other item was on the ground (`n0 = 0`) |
| `visible-draw` | pass | With the same items shown by `lootshow`, Draw ran 1,477,440 times in 10 s: the counter reaches this event, so the zero above is the game's |
| `cost-hidden-working` | pass | `working` awake vs asleep: 100% vs 90% (pair 1) and 100% vs 88% (pair 2), +10 and +12 points |
| `cost-hidden-perf` | pass | Scored on the over-budget branch: the frame was not capped (asleep it ran at about 140 fps) and `working` awake was 100%. 18.30 vs 7.73 ms and 14.20 vs 7.54 ms, +10.57 and +6.66 ms |
| `cost-visible-working` | fail | `working` was already 100% with the items hidden, so showing them could not raise it: 100 vs 100, +0 points. Its `perf` annex resolved a delta: 15.41 vs 14.20 ms, +1.21 ms. The control gates only a fail of `cost-hidden-working`, so this does not change that pass |
| `sleep-wake-roundtrip` | pass | `lootsleep 1`: `asleep=2736 ... ground-after=0`; `lootsleep 0`: `woken=2736 exist-after=2736 ... ground-after=2736`, and `lootcensus` read 2,736 again. The same round trip ran twice |
| `zone-end-cleanup` | pass | In a second zone, a fresh `lootspawn 1000` (809 ground items, then `loothide`); on the owner's exit Clean Up ran 809 times and Destroy 0 |
| `zone-change-gone` | pass | `lootcensus` after the exit: `ground=0` |
| `reload-none` | not observed | The reload put the character in town (`Town_01_rm`), where `ground=0` says nothing about the zone the save was written in |
| `saves-diff` | not observed | The game was stopped with 2,736 hidden items on the ground. Its exit rewrote three files and none grew (`herosiege13.hss` -8 bytes, `inventory_order_13.hss` -16, `shop.ini` 0). The check's positive control, the template's key in the decoded backup, failed: the key sits one encoding layer deeper (inside the `[fortune]` section's `potions=` field) than the single-pass decode reaches, so the check is not scored |

### The frame captures

Each capture is 15 s of `frameprof`; `perf` is the average frame interval since
the previous `perf reset`.

| Capture | Items on the ground | `working` | `perf` average (max) | `frameprof` frame time |
|---|---|---|---|---|
| Baseline | none | 90% | 7.11 ms (14.4) | median 6.9 ms, 139.4 fps |
| Pair 1, awake | 2,736 hidden, awake | 100% | 18.30 ms (2,482.4)* | median 22.2 ms, 45.7 fps |
| Pair 1, asleep | 2,736 asleep | 90% | 7.73 ms (214.8) | median 6.9 ms, 142.5 fps |
| Pair 2, awake | 2,736 hidden, awake | 100% | 14.20 ms (67.9) | not recorded |
| Pair 2, asleep | 2,736 asleep | 88% | 7.54 ms (51.5) | not recorded |
| Shown (control) | 2,736 visible, awake | 100% | 15.41 ms (78.6) | not recorded |

\* Pair 1's `perf` window ran from the baseline's reset, so it holds the frames
before the spawn and the spawn's own stall as well; its average is mixed, and
its maximum is the spawn.

The heaviest event in pair 1's awake capture was `Loot_Manager_obj`'s Begin
Step, at 42.7% of the frame; with the items shown, `Loot_Ground_obj`'s Draw
came in at 25.8%.

### What a hidden item costs

- **By frame time, pair 2** (the cleanest window): 6.66 ms / 2,736 = **2.4 µs
  per hidden item per frame**.
- **By frame time, pair 1:** 10.57 ms / 2,736 = 3.9 µs from the mixed `perf`
  window, and (22.2 - 6.9) ms / 2,736 = 5.6 µs from `frameprof`'s medians.
- **By `working`,** the workorder's conversion gives (11 / 100) × 16.67 ms /
  2,736 = 0.67 µs. That is a floor, not an estimate: the awake captures sat at
  100% and could not rise further, and the conversion assumes a 16.67 ms frame
  while this zone ran at about 7 ms with the items asleep.

So a hidden item costs **about 2.4-5.6 µs of frame time per frame** here, three
to seven times the model's upper bound of 0.8 µs. **Measured.** The model
counted the runtime's per-instance upkeep and the item's own Alarm 9. The
awake profile's heaviest event, `Loot_Manager_obj`'s Begin Step, is not in the
model, and its share with the items asleep was not recorded; that the
manager does work per ground item in that event is what the profile suggests,
not something read or measured. Showing the items added about 1.21 ms more
(0.44 µs per item), their Draw.

Scaled back linearly (an assumption the session did not test), part 1's 281
hidden items would cost 0.7-1.6 ms a frame, about 4-9% of a 60 fps frame.

### What the session established about lifetime

- **A hidden item does not outlive its zone.** The zone's end ran each one's
  Clean Up (809 of 809) and no Destroy, and none were left after the exit.
  **Measured**, with the items awake.
- **Sleep is reversible within a zone.** `instance_activate_object` brought
  back every item `instance_deactivate_object` had put to sleep, twice, with
  the ground count restored. **Measured.**
- **Whether hidden items reach the save, and whether they come back after a
  reload, was not observed.** The save the game wrote at exit with 2,736 hidden
  items on the ground did not grow, but `saves-diff`'s control failed, so that
  size is a record, not a verdict; and the reload landed in town. If either is
  measured later, the finding covers the save written at exit, not a save
  written mid-zone.

## The mod

Part 2b, the workorder `forgepact-issue-95-mod` (2026-09-28): a Quality of Life
switch, **Sleep loot your filter hides** (`mod_hidden_loot`, off by default,
plugin verb `hiddenloot 1|0|stat|key <vk>`), with a child row, **Show hidden
loot while held**, that picks the show key (`mod_hidden_loot_key`, Left Alt,
virtual key 164, by default). The class is
`plugin/include/ForgePact/HiddenLootMod.hpp`; the adapter is the "Hidden loot
sleep" section of `plugin/ModuleMain.cpp`. Everything in this section is a
**reading of our own code** unless it is labelled otherwise. The mod was built
to the owner's answers of 2026-09-28: build it (Q1), a key held to show the
items (Q2), sleep rather than destroy (Q3), the hook on `LootGroundInit` with
the sleep at the end of the drop's frame (Q-deferral), and Left Alt as the
default key (Q-key).

**It decides nothing.** The verdict is the game's: the mod acts on the
`lootFilterVisible` the game's own filter left on the item, and never
evaluates the filter itself or refuses a drop.

**The hook point, and why there.** All three ground-drop entry points reach
`LootGroundInit`, which runs the item's bound filter closure and leaves the
verdict on the new instance: `LootGroundCreateFromItem` (monster drops) calls
it once after making the instance, `LootGroundDrop` (an item dropped from the
bag) calls it at two sites, and `LootGroundCreate`'s body names it as a callee
once. **Static reading**, 2026-09-28, in the local Ghidra project; the third
is a listing of callees, not a traced path. So one hook, on `LootGroundInit`,
covers every drop path. Hooking the entry points instead would take three
hooks, and two of them already carry one of ours (`MiningOre` hooks
`LootGroundCreate` in the player build; the research build's
`InstallItemInspectHooks` table-swaps `LootGroundCreate` and
`LootGroundCreateFromItem` at start-up), on which `HookOneScript` would go
table-only and miss the game's compiled calls. Nothing else hooks
`LootGroundInit` in either build, so the install, `HookOneScript` by the SDK's
name with both routes, gets the inline detour on both builds. It is attempted
once, on the first switch-on after setup.

**Where the sleep happens, and why there.** The hook calls the game first and
then only records what the call carried (argument 0, argument 1 and `self`),
with no runner call. The arguments are read as `(instance, item)`, but that is
a reading, not a measurement, so at the end of the frame (`EVENT_FRAME`, after
every step event) the class takes the first of the three that is a live
instance whose `object_index` is `Loot_Ground_obj`'s, whatever its value kind
(a number, a reference or an instance pointer are all asked; the kind never
decides, since an item struct is an object too). It then reads the verdict
there, after `variable_instance_exists`, and deactivates the item only if the
verdict reads hidden. It does not deactivate inside the call: the rest of the
entry point, and whoever called `LootGroundCreateFromItem` with its return
value, may still address the new instance, and a deactivated instance is
absent to them; whether this runtime would then error or silently skip is not
established, and neither is acceptable. The price is that a hidden drop stays
awake for at most one frame. Reading the verdict at the tick is also where the
decision is used (AGENTS.md § "Check a Permission Where It Is Used"). A call
none of whose three values is a ground item counts `unidentified=`, and an item
with no verdict counts `no-filter-var=`; neither is acted on.

**The show key and its guard.** The class polls the key once a frame with
`GetAsyncKeyState` (the route the research build's F5-F11 hotkeys already use),
finds its edges, and counts it only while the foreground window belongs to the
game's process (`GetWindowThreadProcessId(GetForegroundWindow())` against
`GetCurrentProcessId()`), so a key held in another window shows nothing. These
are Win32 calls, not game addresses. On a key-down edge every slept item is
woken and both its verdict and the built-in `visible` are written visible, so
it shows at once rather than at Alarm 9's next refresh (if the built-in refuses
the write, the verdict alone is left to that refresh and the log says so once).
A hidden drop made while the key is held is shown instead of slept. On release,
every shown item that still exists is written hidden again and slept; one
picked up meanwhile is forgotten and counted `gone=`, asked nothing but whether
it exists. `GetAsyncKeyState(VK_LMENU)` reads the left Alt only. Codes 1 and 2
(the mouse buttons the game plays with) are refused, 0 means no key is polled,
and the panel does not offer generic Alt, Right Alt or F10. A lone Alt press
can put a Win32 window into its menu mode; whether it does for this game's
window is **not established**, and Live procedure 2's `alt-no-menu` measures it
with the default key. If it does, the player picks another key.

**Switching, rooms.** Switching on walks the ground items already there once,
capped at 8,192 like `lootcensus`, and sleeps those the filter hides.
Switching off wakes every item the mod put to sleep, with the game's hidden
verdict in place, which leaves them exactly as the game keeps a hidden item.
A room change forgets every handle without a runner call (the room's end took
the instances), and in a persistent room the mod sleeps nothing, read the way
far sleep reads it (`room_persistent`).

**The fallback.** If the `LootGroundInit` hook is table-only or not installed,
the game's compiled drop calls may pass it by, so a pass over the awake ground
items every 18 frames (Alarm 9's 0.3 s at 60 fps) takes its place, and the
switch-on line (`route=table-only` / `route=none`) and a log line say so. With
both routes in place the mod never walks, apart from the switch-on walk.

**What it cannot do**, known and accepted rather than fixed: loosening the
filter does not reveal slept items. A deactivated instance is not drawn,
whatever its verdict, and whether the game's menu re-evaluation even reaches
one is not established (see [Static reading](#static-reading) and
[Not established](#not-established)); the player holds the key or switches the
mod off, and an item woken by the switch keeps the hidden verdict it had until
the filter is next applied. Only the verdict at drop time counts: an item a later,
stricter filter hides stays awake. The pet collects only filter-visible items
([pet-loot-stuck-research.md](pet-loot-stuck-research.md#the-scan)), so it ignores slept
items as it ignored hidden ones, and while the key is held the shown items are
targets it can pick up.

**Verified.** The harness (`tests/hidden_loot_harness.cpp`, run by
`tests/test_hidden_loot_behavior.py`) compiles the real class against a
controlled runner: off asks the game nothing, not even the key; a hidden drop
sleeps at the next frame's end and a visible one is never touched; the hold,
the drop while held and the foreground guard; off, the switch-on walk, room
changes and persistent rooms; and the fallback pass. The wiring and the rules
are pinned by `tests/test_hidden_loot_mod_contract.py`, the panel by
`tests/test_hidden_loot_panel_contract.py`. **Not observed live:** everything
the harness cannot see (that the detour sees the game's own drop calls, the key
under `SendInput`, whether Clean Up runs at the zone's end for sleeping loot)
is Live procedure 2 of `forgepact-issue-95-mod`, not yet run.

## Not established

- **Why 95 items stayed invisible with the filter off** in part 1's Live 1.
  Alarm 9's `OnScreen` half is the likely cause (items off screen read
  `visible` false whatever the filter says), but nothing measured it, and this
  workorder does not.
- **What the menu path does to a deactivated item.** Turning the filter off
  re-evaluates the items on the ground; how that pass walks them was not read,
  so whether it can reach an item a mod put to sleep is not established (see
  [Static reading](#static-reading)).
- **Whether `LootGroundCreate` reaches the same filter call on every path.** It
  calls `CreateLootInFreePos` like `LootGroundCreateFromItem`, and a listing of
  its callees names `LootGroundInit` once (**static reading**, 2026-09-28,
  [The mod](#the-mod)), but no path through its long body was traced, so which
  of the items it makes get their verdict from `LootGroundInit` is not
  established.
- **The guard in front of the filter call** inside `LootGroundInit` was not
  read; `skipLootFilter` is a candidate, not a finding.
- **Whether ground items reach the save.** `SaveSlot`'s body was not read, and
  Live 1's `saves-diff` and `reload-none` both read not observed
  ([Live 1 results](#live-1-results-2026-09-28)).
- **Where the per-item cost goes.** The awake profile's heaviest event was
  `Loot_Manager_obj`'s Begin Step; what it does per ground item was not read,
  and its share with the items asleep was not recorded.
- **Whether the cost is linear in the number of items.** Live 1 measured one
  count, 2,736; the scaling to part 1's 281 assumes linearity.
- **Whether a sleeping ground item is cleaned up at the zone's end.** Live 1's
  zone exit ran with the items awake; far sleep's props are cleaned up asleep,
  but that was not measured for loot.
- **Why some `LootGroundCreateFromItem` calls made no ground item.** 86-90 of
  each 1,000 `lootspawn` drops in the first zone, and 191 of 1,000 in the
  second, returned no live instance, and the ground count rose only by the
  instances returned. **Measured**; the cause is not established.
