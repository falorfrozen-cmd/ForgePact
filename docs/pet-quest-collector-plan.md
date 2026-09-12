# Pet Quest Collector — implementation plan

Status (2026-09-10): **scaffolding implemented; Phase 0's exit criterion
reached and exhausted on the "blocked" side - re-scoping needed.** Phases 1
(contract tests) and 4 (panel) are done; Phase 2's read-only enumeration
(steps 1-3: camera bounds, family enumeration, static exclusion filter) is
done and wired into `FrameCallback`. Phase 0 item 1 (the gating measurement)
is done, live, across many confirmed collects spanning 13 rebuild/relaunch
cycles: **B1 is blocked** and **B2 is disproven**, both as originally scoped
(`CheckPlayerInteraction`, its target gate function, is never called) and
through every builtin-hook variant tried in its place. 34 distinct hooked
call sites total - every named script `hs-game-sdk`'s static search could
find, every spatial/collision builtin plausibly involved, and every
object-lifecycle/variable-write builtin that could explain a collected item
vanishing from `instance_number`/`instance_find` (confirmed to happen, with
room-change artifacts specifically ruled out) - all measured zero calls.
Native decompilation was also attempted (Ghidra, against this YYC-compiled
build - see `ForgePact/docs/pet-quest-collector-research.md`'s "Native
decompilation" and "Final status" sections): real infrastructure and 16
decompiled functions now exist locally for a future session to continue
from, and the general shape of the mechanism was found, but not a confirmed
field/script name, and it was stopped deliberately after one misread was
caught before it reached any conclusion or code - the risk of an uncaught
misread feeding a wrong collect call (corrupting quest state) outweighed
pushing further this session. This mod needs the re-scoping §11's first risk
row calls for before Phase 2 step 4-5 (the actual collect call) or Phase 3
(pet movement) can be implemented. **That re-scoping was drafted and has now also been tested to a conclusion**:
see `ForgePact/docs/pet-quest-collector-plan-b4-input-simulation.md` (Plan B4
- fake the player's own hover+keypress input instead of finding the internal
mechanism). Its Phase 0 ran live on 2026-09-10 and **B4's central premise is
disproven**: the GML-visible input state (`inputState`, `mouse_x_prev`, and
every other candidate on `Profile_Manager_obj`) is a downstream *record* of
real input that nothing reads back, so poking it fakes nothing - on either
the keypress half or the hover half. Full evidence in
`ForgePact/docs/pet-quest-collector-b4-research.md`. B1, B2 and B4 are now all
closed. **The active plan is now Plan C**
(`ForgePact/docs/pet-quest-collector-plan-c-direct-invocation.md`, drafted
2026-09-10), which attacks the assumption all three shared - that the code must
be reached by *name* - via the quest item's own `m_Quest*` bound methods and
`event_perform`, with Ghidra as an explicit, anchored fallback. **Its Phase C0
tooling was implemented 2026-09-11** (all seven checklist items, one dev build,
one relaunch); no live round has run yet, so nothing is measured. Commands,
safety rules and the suggested running order are in
`ForgePact/docs/pet-quest-collector-c-research.md`.
Target branch `release/v1.3.17`.

Panel toggle: "Pet collects quest items" (Mods tab).
Config key: `mod_pet_quest_pickup`. Plugin command: `petquest 1` / `petquest 0`
(+ `petquest stat` in the research build only).

---

## 1. What the mod does

While the toggle is on and the player has a pet out, the pet collects quest
items anywhere on screen, without the player hovering each one and pressing the
interact key.

Decisions taken 2026-09-10, before implementation:

| Question | Decision |
| --- | --- |
| Only items for quests the player has accepted? | **Yes** — gate on accepted quests. |
| Who gets credit for the pickup? | **The player.** The pet is the trigger, not the acting instance. |
| Collection radius | **Screen-wide** (the current camera view), not a leash around the player. |

Off by default. Installs no hooks and runs no tick while off, matching the
zero-overhead all-off baseline that `tests/test_release_hook_contract.py`
already guards.

---

## 2. The constraint that shapes everything

Quest items are **not** walk-over pickups. In essentially all cases the player
must hover the mouse over the item and press the interact key (F by default).

That rules out the mechanism `orbpickup` uses. Orb pickup never collects
anything itself — it moves the globe toward the player and lets the game's own
proximity pickup fire (`PullOneGlobe`, `ModuleMain.cpp:4680`). There is no
proximity pickup to trigger here, so moving a quest item next to the player
accomplishes nothing. The interaction has to be produced, not provoked.

**Consequence for build order:** because the interaction is mouse-driven rather
than position-driven, the pet does not need to physically reach an item for the
mechanic to work. The pet walking over is cosmetic. So the collect is built and
proven *first*, headless and instant, and the pet movement is layered on after.
If the collect turns out to be unreachable, nothing is spent on animation.

---

## 3. Structural finding: the game defines the item set for us

The corrected `hs-game-sdk` object hierarchy (see
`tests/test_object_hierarchy.py` at repo root) gives a three-level chain:

```
Pickup_Parent_obj (3421)              <- root, 110 descendants
├── Coin_obj
├── Loot_Ground_obj
├── Player_Item_Drop_obj
└── Quest_Object_Parent_obj (3979)    <- 106 descendants
    ├── Quest_Act_01_Coffee_Beans_obj
    ├── Quest_Act_02_Ginseng_obj
    └── ... 104 more
```

Two things follow.

**No hand-built allowlist.** An earlier draft of this plan proposed regex-
filtering the 135 `Quest_*` object names to guess which are carriable versus
which are anchored scenery. That is unnecessary and worse: the 32 `Quest_*`
objects that fall *outside* the `Quest_Object_Parent_obj` family are exactly the
props such a filter was trying to exclude — `Quest_Potion_Cauldron_obj`,
`Quest_Naga_Temple_Pedestal_obj`, `Quest_Monster_Spawner_obj`,
`Quest_Manager_obj`, `Quest_Object_Collision_obj`. The game's own hierarchy
draws the line more accurately than a name pattern can, and it survives a patch
that adds new quest items.

**Quest items are loot-family.** Sitting under the same parent as `Coin_obj` and
`Loot_Ground_obj` is strong evidence that the interact path is *shared* across
pickups rather than reimplemented per object. One chokepoint to find, not 106.

Enumeration should still be done at runtime — GameMaker's `instance_number` /
`instance_find` accept a parent object and return its children — with the SDK's
`HeroSiege::Objects::IsDescendantOf(idx, GameObject::Quest_Object_Parent_obj)`
as the compile-time-checked classifier and cross-check. Prefer the runtime
parent walk over baking the 106-entry table into the plugin.

> Note: `hs-game-sdk/data/objects.json` has not been regenerated from the exe,
> so its raw `parent_index` / `mask_index` *field labels* still carry the old
> mislabeling. Read hierarchy through the generated bindings
> (`OBJECT_PARENT_INDEX`, `IsDescendantOf`, …), which are correct, not through
> the json fields directly.

---

## 4. Mechanism: two candidates, one measurement decides

### Plan B1 — call the pickup path directly (preferred)

Bypass the input gate entirely: invoke whatever `gml_Script_CheckPlayerInteraction`
invokes on a successful interaction, with the quest instance as the target and
the **player** as the acting/crediting context.

ForgePact already has this machinery well-worn —
`CallGameScriptEx(res, "gml_Script_X", self, other, args)` appears throughout
(`ModuleMain.cpp:4543`, `:4778`, `:5049`). No hot-path hooks, no input spoofing,
and the player-as-Self choice directly satisfies the credit decision.

### Plan B2 — satisfy the gate rather than bypass it (fallback)

Hook `CheckPlayerInteraction` and make it evaluate as hovered-and-pressed for
one specific instance, only while the mod is on and only for a chosen target.

This is the Beacon's proven lie-to-the-game pattern — `Hook_distance_to_object`
returns 0.0 to convince spawners the player is adjacent (`ModuleMain.cpp:4605`),
narrowly gated so the builtin passes through natively otherwise.

Riskier than B1: it likely requires hooking mouse/keyboard builtins, which are
far hotter paths than `distance_to_object`, and it reproduces the input gate
rather than the effect.

### Plan B3 — not pursued

Synthesising real mouse movement and keypresses (moving the OS cursor onto each
item and injecting F) is rejected: it fights the player for the cursor, is
frame-rate dependent, and would be unusable while the player is doing anything
else.

---

## 5. Phase 0 — research (dev build, before any implementation)

Build with `plugin_build\build.bat dev`, drive by hand through
`<game>\bin\bp_ipc\cmd.txt`, log findings to
`ForgePact/docs/pet-quest-collector-research.md`.

Per `agents.md`: record measured behavior, names and indices only — no
decompiled script bodies in any tracked file.

1. **`CheckPlayerInteraction`** — hook it, log call frequency, arguments and
   Self-context. *This single measurement decides B1 vs B2 and is the gate on
   the whole feature.*
2. **Is there a hold/channel?** If the interaction runs a progress bar rather
   than completing instantly, the synthetic call must hit the *completion* path,
   not the start. Determines whether one call per item is enough.
3. **`gml_Script_LootBlocksUseKey`** — the name suggests ground loot blocks the
   interact key. Measure whether a pile of loot underfoot suppresses the fetch.
4. **Accepted-quest gate, cheap path first.** Before building an object→quest-id
   mapping, measure whether quest item instances exist in the room *at all*
   before their quest is accepted. If the game only spawns them on acceptance,
   the gate is free and needs no code. If they pre-exist, fall back to
   `quest_exists` / `GetQuestObjectives` / `IsQuestCompleted` and find where the
   instance carries its quest id.
5. **Parent enumeration through YYTK** — confirm `instance_number` /
   `instance_find` on `Quest_Object_Parent_obj` (3979) return children through
   `CallBuiltin`, not just exact-object matches.
6. **The pet at runtime** — confirm `Companion_obj` (977) is the player's pet,
   and whether `Companion_Pickup_obj` (978) instantiates alongside it and what
   it already absorbs. If the pet already has a working collector collider, part
   of Phase 3 may collapse into widening its reach.
7. **Screen-wide radius** — determine how to read the current view
   (`camera_get_view_x/y/w/h` on `view_camera[0]`, or the `view_*` fallbacks)
   and whether that is stable across zoom levels and zone types.

**Exit criterion:** B1 or B2 is chosen with evidence, or the feature is reported
as blocked. If neither mechanism is reachable, this stops being an
`orbpickup`-shaped mod and needs re-scoping before any code is written.

---

## 6. Phase 1 — contract tests

New `ForgePact/tests/test_pet_quest_collector_contract.py`, modeled on
`test_relic_filter_contract.py`. Pure Python, no game process, per `agents.md`'s
baseline/target rule.

**Baseline (mod off):**
- `forgepact.DEFAULTS["mod_pet_quest_pickup"]` exists and is `False`.
- `build_cmds` omits `petquest` when off.
- No eager hook is added to the release path (the invariant
  `test_release_hook_contract.py` already enforces).

**Target (mod on):**
- `build_cmds` emits `petquest 1`.
- `"petquest"` is present in the plugin's `kPlayerCommands` set
  (`ModuleMain.cpp:10510`). **Omitting this is the classic failure mode here** —
  the player build silently rejects unknown commands, so the toggle would appear
  to work and do nothing.
- The new header declares its `std::atomic<bool>` enabled flag.
- The Mods-tab row and its label exist in `forgepact.HTML`.

**Hierarchy:**
- Assert the collector targets `Quest_Object_Parent_obj` descendants and that
  the excluded props (`Quest_Potion_Cauldron_obj`,
  `Quest_Naga_Temple_Pedestal_obj`, `Quest_Monster_Spawner_obj`) are *not* in
  that family — so a future SDK regeneration that breaks the hierarchy fails
  here rather than in-game.

---

## 7. Phase 2 — headless collect

Prove the mechanic before any animation exists.

New `ForgePact/plugin/include/ForgePact/PetQuestCollectorMod.hpp`, following the
class-split direction set by `ModManager.hpp` / `RelicFilterMod.hpp`: the class
owns only its own new state and leans on ModuleMain's existing `Out()`,
`g_Yytk`, and `HhResolveLocalPlayer` rather than adding more globals to an
11.4k-line file.

Tick called from `FrameCallback` beside the orb block (`ModuleMain.cpp:11221`),
reusing the once-per-frame cached player position already computed there:

1. Read the current camera view → screen-wide bounds.
2. Enumerate `Quest_Object_Parent_obj` instances within those bounds, under the
   same 64-instance budget the orb tick uses, so a runaway count cannot cost a
   frame.
3. Drop targets whose quest is not accepted (or skip entirely if Phase 0.4 shows
   the game never spawns them early).
4. Invoke the collect with the player as acting context.
5. Pace it — at most one item per N frames. Instant bulk collection is both
   more likely to trip a scripted quest state and unreadable to the player.

**Verify with `GetQuestProgress` that the objective actually advances.** A
pickup that removes the instance without incrementing progress is worse than no
mod at all — it destroys quest items. This check is the real pass/fail for the
phase.

No hooks are installed by this mod, so unlike `relicfilter` it needs none of the
arm/defer/pending machinery, and `build_cmds` can safely emit it at launch.

**`petquest stat`** (research build only), built as the same decision tree
`OrbPickupStats` uses (`ModuleMain.cpp:4736`): separate counters for *no pet
resolved* / *pet found, no items on screen* / *items seen but quest not
accepted* / *collect attempted, progress unchanged* / *collected*. Those
distinguish five very different bugs that all present as "it did nothing."

---

## 8. Phase 3 — pet movement (cosmetic)

Only once Phase 2 collects and credits correctly.

Steer `Companion_obj` toward the pending target and fire the collect on arrival.
Constant px/frame, **not** an accelerating ramp — there is a logged user
complaint from 2026-09-10 about the orb pull, that speeding up as it closed in
"read as an unnatural teleport right before pickup"
(`ModuleMain.cpp:4600`). The same mistake is available here and should not be
repeated.

With no pet summoned, the mod does nothing. It deliberately does **not** fall
back to collecting for the player directly — that is a different mod (a quest
item magnet), and silently becoming one would surprise anyone who enabled a
*pet* feature.

---

## 9. Phase 4 — panel

Five edits in `ForgePact/src/forgepact.py`, mirroring `mod_orb_pickup_radius`
exactly:

| What | Anchor |
| --- | --- |
| `"mod_pet_quest_pickup": False` | `DEFAULTS`, `:180` |
| emit `petquest 1` when on | `build_cmds`, `:588` |
| add key to the bool-toggle tuple | `:1394` |
| live `send_cmds([f"petquest {1 if ... else 0}"])` | `:1430` |
| Mods-tab row + JS load/onchange | `:1806`, `:2009`, `:2143` |

---

## 10. Phase 5 — build, verify, document

1. `plugin_build\build.bat release`, then copy to
   `modfiles_shipped\BloodPactPlugin.dll` — `build_release.py` refuses to
   package if those two differ, so a stale plugin cannot ship by accident.
2. Live confirmation in-game, reserved for last per `agents.md`'s
   limit-rebuilds rule.
3. Docs: `pet-quest-collector-research.md`, the Mods section of
   `ForgePact/README.md`, the module guide at
   `docs/submodules/ForgePact/instructions.md`, and release notes.

**Iteration cost.** The targeting and pacing math is pure geometry and quest-
state logic. Per `agents.md`, extract it into a harness under `tools/` (the
`tests/headhunter_dispatch_harness.cpp` precedent) so it can be tuned without a
rebuild-and-relaunch cycle, reserving live sessions for final confirmation.

---

## 11. Risks

| Risk | Mitigation |
| --- | --- |
| **Phase 0.1 fails** — the interaction resists both B1 and B2 | Feature is re-scoped, not forced. This is the single largest risk and it is front-loaded by design. |
| Collect removes the item without crediting progress | Phase 2's `GetQuestProgress` check is the phase's pass/fail gate, not an afterthought. |
| Screen-wide auto-collect trips a scripted quest state (objects meant to be taken in order, or during a specific step) | Accepted-quest gate; one item per N frames; per-object exclusions added only from observed breakage, not guessed in advance. |
| A quest object in the parent family is not a carriable item — the 106 include `Civilian_NPC_obj`, `Boat_Quest_obj`, `Black_Hole_Quest_obj` | Runtime sanity filter on top of the family; these three specifically checked during Phase 0. |
| Frame budget | 64-instance cap, reusing the cached per-frame player position, as `orbpickup` does. |
| Online quest sync | Offline only by design. `CA_questUpdate` and `NetworkSendClientQuestUpdate` are untouched and out of scope. |
| Legal | Research notes carry measured behavior, names and indices only — no decompiled script bodies in tracked files (`agents.md`). |

---

## 12. Files touched

New:
- `ForgePact/plugin/include/ForgePact/PetQuestCollectorMod.hpp`
- `ForgePact/tests/test_pet_quest_collector_contract.py`
- `ForgePact/docs/pet-quest-collector-research.md`

Modified:
- `ForgePact/plugin/ModuleMain.cpp` — include anchor, `RunCommand` branch,
  `kPlayerCommands` entry, `FrameCallback` tick
- `ForgePact/src/forgepact.py` — the five anchors in §9
- `ForgePact/modfiles_shipped/BloodPactPlugin.dll` — rebuilt
- `ForgePact/README.md`, `docs/submodules/ForgePact/instructions.md`,
  release notes
