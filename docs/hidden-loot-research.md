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
(part 2b, the draft workorder `forgepact-issue-95-mod`) is decided from those
numbers.

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
| Whether a hidden item runs its Draw event | Not by the runner's rule for `visible = false` | **Static reading**; Live 1's `hidden-draw` and its control `visible-draw` measure it |
| What a hidden item costs per frame | 0.4-0.8 µs per item per frame, predicted | **Model** ([below](#model-what-a-hidden-item-should-cost)); Live 1's `cost-hidden-working` and `cost-hidden-perf` measure it |
| Whether hidden items outlive the zone or reach the save | Nothing read | Live 1's `zone-end-cleanup`, `zone-change-gone`, `reload-none` and `saves-diff` |
| A mod that hides, sleeps or refuses filtered items | None. **No mod ships from this workorder**; part 2b stays a draft until Live 1's numbers and the owner's answers exist | - |

Live 1 of `forgepact-issue-95` has not run yet. Its results go in a
`## Live 1 results` section of this document when it has.

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
  and then calls `LootGroundDraw`. The GameMaker runner does not run the Draw
  event of an instance whose `visible` is false, so a hidden item's Draw
  should never run, but the runner's draw pass still walks the instance
  ([far-sleep-research.md](far-sleep-research.md#what-sleeps-and-what-never-does):
  hidden is not asleep). **Static reading**; Live 1's `hidden-draw` measures it,
  with `visible-draw` as its positive control.
- **Save.** `SaveSlotFunc` is a one-line wrapper around `SaveSlot`, whose body
  was not read. Whether ground items reach the save is left to Live 1's
  `reload-none` and `saves-diff`. **Static reading** of the wrapper only.
- **Our own hooks.** The player build already hooks `LootGroundCreate` and
  `LootGroundCreateFromItem` (`InstallItemInspectHooks`, table-only through
  `HookOneScriptTable`), and the angelic probe reports both rows as under a
  table-only hook, so they are blind to this build's direct calls. Nothing in
  this workorder uses them. **Reading of our own code.**

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

## Live procedures

Each procedure's step-by-step lives in the workorder that runs it
(`forgepact-issue-95`); this is the summary. The session runs on slot 14
("Sorak", the slot every #95 session used), backs the saves up before the
launch and restores them after, and must pass `dll-hash`, `marker` (the boot
line `==== BloodPact plugin loaded ==== v2.0.1`) and `control` (`ping` answers
`pong`, and `lootcensus` answers a line with a `walked=` field) before anything
else it records counts. Person actions come in two short hand-backs: walking
into a zone and standing still at the owner's usual strict filter, and later
taking an exit.

**Live 1, the cost and lifetime round** (the research DLL,
`plugin_build\BloodPactPlugin_rel.dll` from `plugin_build\build.bat dev`; not
run yet). Far sleep is switched off for the session and put back afterwards.
After a baseline `frameprof` capture, three `lootspawn 1000` calls drop one
equipment template 3,000 times around the player; `loothide` is the fallback if
the owner's filter happens to show the template. `evcount` counts the Alarm 9,
Draw, Clean Up and Destroy runs of `Loot_Ground_obj`. Two alternating pairs of
15 s `frameprof` captures with `perf` averages then compare the items awake and
put to sleep by `lootsleep 1` / `lootsleep 0`, followed by one capture with the
items made visible by `lootshow`. The owner then takes an exit, the game is
stopped, the saves are inspected, and the character is reloaded. Its checks,
in order: `dll-hash`, `marker`, `control`, `spawn-count`, `spawn-hidden`,
`alarm9-runs`, `hidden-draw`, `visible-draw`, `cost-hidden-working`,
`cost-hidden-perf`, `cost-visible-working`, `sleep-wake-roundtrip`,
`zone-end-cleanup`, `zone-change-gone`, `reload-none`, `saves-diff`. The first
three, `spawn-count`, `alarm9-runs` and `sleep-wake-roundtrip` have to pass for
the session to count; the rest are research checks, and their verdict is the
finding. From `cost-hidden-working` and `cost-hidden-perf` the workorder sets
its route: both pass, a cost was measured; both fail, a cost the instrument
cannot see at 3,000 items; anything else, not observed.

## Not established

- **Why 95 items stayed invisible with the filter off** in part 1's Live 1.
  Alarm 9's `OnScreen` half is the likely cause (items off screen read
  `visible` false whatever the filter says), but nothing measured it, and this
  workorder does not.
- **What the menu path does to a deactivated item.** Turning the filter off
  re-evaluates the items on the ground; how that pass walks them was not read,
  so whether it can reach an item a mod put to sleep is not established (see
  [Static reading](#static-reading)).
- **Whether `LootGroundCreate` reaches the same filter call.** It calls
  `CreateLootInFreePos` like `LootGroundCreateFromItem`, but its body was not
  read to the end, so whether items it makes get their verdict from
  `LootGroundInit` is not established.
- **The guard in front of the filter call** inside `LootGroundInit` was not
  read; `skipLootFilter` is a candidate, not a finding.
- **What `SaveSlot` writes** was not read; Live 1 measures only whether ground
  items survive a reload.
