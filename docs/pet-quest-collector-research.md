# Pet Quest Collector — research notes

> **CORRECTION (2026-09-11, later session). Every "0 calls" result below that
> came from a named-script hook is an artefact of the hook, not a measurement
> of the game.** `HookOneScript`/`HookRawNamedRoutine` install by swapping a
> function pointer inside the script-table entry, and this build's compiled
> GML calls another script with a direct `call rel32` bound at compile time —
> it never reads that table, so the detour never runs however often the game
> executes the code. Item 1 below is a concrete casualty:
> `CheckPlayerInteraction` is called from the Step event of every shrine, NPC,
> pile and quest object in the room, and was "ruled out" on a 0 that only
> meant the hook could not see it. B1's headline ("no named script ever
> fires — 34 hooked call sites, 0 calls") has to be re-read the same way: it
> established that the interaction does not go through the *script table*, not
> that it does not go through a named script.
>
> **Unaffected:** everything hooked with `HookBuiltin` or `MmCreateHook`,
> which patch the function's own bytes — that includes the whole
> `citrace dispatchtrace` builtin-surface result, whose negative still stands.
>
> Full explanation, and the read-only `citrace nativetrace` command that
> re-hooks the same targets properly, in
> `pet-quest-collector-c-research.md`, "The hooks were blind: why '0 calls'
> was never evidence". Nothing below has been deleted — the measurements are
> real, only their interpretation changes.

Template for the Phase 0 measurements in
`ForgePact/docs/pet-quest-collector-plan.md` §5, filled in as measured against
a live game session through `<game>\bin\bp_ipc\cmd.txt`. Item 1 (the gating
measurement) is done as of 2026-09-10 — see its conclusion below. Per
`agents.md`, record only names, indices, and observed behavior — no
decompiled script bodies.

The scaffolding already in the plugin (`PetQuestCollectorMod.hpp`,
`PetQuestCollectorTick()`/`PetQuestCollectorStats()` in `ModuleMain.cpp`)
gives `petquest stat` real numbers for two of the seven items below before
any research happens — start a session, toggle `petquest 1`, and read those
counters alongside the manual checks.

---

## 1. `CheckPlayerInteraction` — call frequency, arguments, Self-context

**MEASURED 2026-09-10, session 1.** `CheckPlayerInteraction` (the plan's
assumed name) is a false lead: hooked successfully (`HOOK INSTALLED`), but a
live collect (hover + press F on a quest item, confirmed picked up) produced
**0 calls**. Ruled out.

**MEASURED 2026-09-10, session 2.** Broadened the trace to every other
UseKey-/Interact-/Pickup-shaped script name already indexed in hs-game-sdk,
plus every script-table entry found anywhere inside
`Quest_Object_Parent_obj`'s own Create event (its 7 anonymous closures - the
only candidates the SDK's static name search turned up for a "shared
chokepoint" per plan §3). All hooked successfully. A second live collect (a
different item) still produced **0 calls across all 12 hooked scripts**:
`CheckPlayerInteraction`, `CheckUseKey`, `PlayerInteracting`,
`LootBlocksUseKey`, `PickupLoot`, and `anon@{1400,1584,2113,2786,3858,4737,5164}
@gml_Object_Quest_Object_Parent_obj_Create_0`. Also checked
`Pickup_Parent_obj`'s own named scripts: only `s_lootDrawData` and its three
`updateDrawY`/`updatePosition`/`updateScaling` closures exist there, and
those are visual (loot ground-sprite draw), not interaction - not hooked.

**Caveat on session 2's test item:** the collected item was
`Quest_Toy_Bear_obj` (Act 8, Zone 8-4 "Camp of Souls" per the tester), which
the tester independently confirmed stays collectible even after its quest is
already complete - i.e. it may not route through the normal
advance-quest-progress path at all, so this result may not generalize to a
quest item whose collection is actually required to progress.

**MEASURED 2026-09-10, session 3 (progress-required retest, same build/session
as session 2).** Fresh account, Act 1 Zone 1, collected both
`Quest_Act_01_Coffee_Beans_obj` and `Quest_Act_01_Body_Part_obj` (the latter
also auto-collects nearby body parts, per the tester). Same all-12-hooks-still-
armed session as before. Result: still **0 calls across all 12 hooked
scripts.** This removes session 2's caveat (that item may have been a
non-progress-gated edge case) - two ordinary, progress-required Act 1 items
show the identical result. Tester confirmed both collects were genuine: the
Coffee Beans quest changed state to "return" (turn-in), and the Body Part
quest's counter advanced (0 -> 8/50, since that item auto-collects nearby
duplicates).

**Conclusion (session 1-3 combined).** Three live collects, two full plugin
relaunches, 12 distinct script-table entries hooked (every UseKey-/Interact-/
Pickup-shaped name in hs-game-sdk, plus the complete set of anonymous
closures inside `Quest_Object_Parent_obj`'s Create event - the only entries a
static name search can find), zero calls on every single one, on both a
non-progress-gated item and two ordinary progress-required Act 1 items. This
is no longer "not yet measured" - it is a measured, repeated negative result.

- **B1 is blocked**: there is no discoverable named-script call target to
  invoke directly.
- **B2 as originally scoped is also disproven**, not just untried:
  `CheckPlayerInteraction` was the candidate gate function B2 planned to hook
  and lie to, and it is never called during a real interaction, so hooking it
  changes nothing.
- The interaction (mouse-hover check, key-press check, and the collect
  itself) is most likely compiled inline into per-object event bytecode with
  no separate script-table entry - comparable to `orbpickup`'s own dead end
  with the globe step scripts (`ModuleMain.cpp`, `OrbPickupTick` comments).
  Per `agents.md`, this cannot be confirmed by decompiling to look at that
  bytecode.
- **One measurement path remains, untried**: hook the *builtin* functions
  (`keyboard_check_pressed`, `mouse_check_button_pressed`, or similar) that
  inline hover/press logic would plausibly call, filtered immediately to
  instances whose Self is a `Quest_Object_Parent_obj` descendant so the hot-
  path cost stays near zero for every other call in the game. This is a
  variant of plan B2, not a new mechanism, and carries the risk the plan
  itself already named ("far hotter paths than `distance_to_object`").

**MEASURED 2026-09-10, session 4 (builtin variant, HIT).** Hooked
`keyboard_check_pressed` and `mouse_check_button_pressed` directly (not
filtered by Self this time - filtered by key/button argument plus an
unconditional first-5-calls sample instead, since the object driving the
check was unknown). Collected another Act 1 item. Result: a real hit -
`keyboard_check_pressed(70)` (`70` = `ord("F")`), called with
**`Self = Profile_Manager_obj#3673`**, returned `1` (pressed) on its first
occurrence, then `0` on every subsequent per-frame recheck (consistent with
`_pressed` being edge-triggered). Neither the quest item instance nor the
player instance is Self at the point the key is read - it's a dedicated
manager object. This is a new, more specific target: whatever
`Profile_Manager_obj` does next with that `1` is the real dispatch point, and
it happens inline in that object's own event code (still no named-script
call - see below). Followed up by hooking `Profile_Manager_obj`'s own 9 Create-event anon
closures the same way (still all 0 calls) - that manager object's F-press
detection does not dispatch through any of its own script-table entries
either, reinforcing the inline-bytecode theory.

**MEASURED 2026-09-10, session 4 continued - no-rebuild win.** Per the
updated `agents.md` guidance (exhaust cheap static/live checks before another
rebuild), queried the *already-running* game's global variables directly with
the pre-existing `dump <substr>` command - no rebuild needed. `dump target`
turned up `global.PlayerGetMouseTarget` and `global.GetMouseTarget`, names
that hadn't appeared in any prior search (which had focused on
"interact"/"usekey"/"pickup"/object-hierarchy, not "mouse"/"target").
`dump hover` also turned up `global.GetQuestHoverDescription` and a live
`global.hoverTooltip = 1` flag. Broadened the static script-name search with
"Mouse" and found `PlayerMouseAction`, `GetPlayerMouseDisabled`,
`KeyboardMouseInput`, `RefreshMouseMove`, `GetMouseDisabledTarget`,
`CanISeeTarget` alongside them - none tied to `Quest_Object_Parent_obj`'s or
`Profile_Manager_obj`'s hierarchies, which is why the earlier per-object
searches missed them. All 9 added to `citrace` in one batch and rebuilt once
(rather than one hook per relaunch). Retested live: all 9 still 0 calls.

**MEASURED 2026-09-10, session 5.** Two more additions in one batch (per the
updated `agents.md` guidance): (a) `Hook_Ci_KeyboardCheckPressed` now dumps
every instance variable on Self, unfiltered, the moment key 70 (F) returns
pressed - since every keyword-filtered guess (`CiQuestVars`) had missed
whatever the real field is named; (b) six spatial/collision builtins
(`instance_position`, `instance_place`, `collision_point`,
`point_in_rectangle`, `distance_to_point`, `instance_nearest`), filtered to
Self == `Profile_Manager_obj` (the object confirmed in session 4 to be where
the F-press is read), as the next-cheapest lead after another named-script
guess would have been.

Result: the full variable dump is a real finding, but not the one hoped for.
`Profile_Manager_obj` is the game's **generic input/profile abstraction
layer** - `gamePadEnabled`, `keyBinds`, `inputState`, `inputToGP`,
`inputToKB`, `gamePadAxisLH/LV/RH/RV`, `mouse_x_prev`/`mouse_y_prev`,
`autoAim`, `localSaveControlsString` (a saved keybind-profile config),
`profileNumber`, `profilePublicName`, `platformOnlineAccountId`. **None of
its ~50 variables mention quest, target, hover, or nearest anything.** It
normalizes raw keyboard/gamepad/mouse input into `pressedArray`/`inputState`
for other objects to read - it is not itself deciding what F does. Consistent
with that: all 6 spatial/collision builtins filtered to it stayed at 0 calls
too - it does no position/hover checking of its own.

**Conclusion (sessions 1-5 combined).** 36 hooks across 6 relaunches, zero
calls on every named-script and quest-object-hierarchy candidate, and the one
real hit (`keyboard_check_pressed`) leads to a generic input layer with no
quest-specific state and no further calls out of it. The object that actually
reads `Profile_Manager_obj`'s processed input state and decides "F pressed +
hovering a quest item -> collect" has not been identified - the next
candidate by the same logic would be the **player's own object** (reading
`Profile_Manager_obj.pressedArray`/`inputState` from its own Step event,
inline, the same way Profile_Manager_obj itself has no separate script-table
entry for this). This is now a materially deeper, better-evidenced result
than the original Phase 0 exit criterion asked for, and a natural point to
check in with the tester on whether to keep going (player-object-filtered
spatial builtins) or stop and re-scope, given the live-session cost so far.

**MEASURED 2026-09-10, session 6 (in progress).** Tester's suggestion:
rather than keep guessing individual functions, capture the *entire*
observable game state right before the F-press and again right after, and
diff them - whatever actually changed shows up regardless of which
un-hooked call path did it. Added `citrace snap1` (captures every global
variable plus every instance variable on the player and on
`Profile_Manager_obj`) and `citrace snap2` (re-captures the same three and
prints only additions/removals/changed values, capped at 60 lines per group
so unrelated churn can't bury the signal). Also added, in the same rebuild:
6 spatial/collision builtins (`instance_position`, `instance_place`,
`collision_point`, `point_in_rectangle`, `distance_to_point`,
`instance_nearest`, `point_in_circle`, `position_meeting`) filtered to
Self being either `Profile_Manager_obj` or the player instance.

Result: `citrace snap1` captured 3571 globals, 116 player instance variables,
56 `Profile_Manager_obj` instance variables. After a real collect,
`citrace snap2`'s diff showed only generic frame-timing churn
(`current_frames`, `deltaSpd`, `repeatGravity`, `updateMinimap`,
`socketValidateTimer`) - **nothing quest-related, and zero changes on
`Profile_Manager_obj`.** Side finding: the player-filtered `position_meeting`
hook fires continuously during ordinary movement (collision against
`Enemy_Parent_obj`/`Collision_Parent_obj` every step), unrelated to
interaction - confirms the player object's own Step code calls builtins
constantly for reasons that have nothing to do with quest items, and ate most
of the shared 300-line trace budget as noise.

**Methodological gap identified, not yet fixed:** the diff compares each
variable's `Describe()` string, which for a container-typed variable (e.g.
`global.questDataRepo`/`global.questDailyOffline`, both `ds_map` references
seen in earlier `dump quest` output) is just a stable reference id - it does
not change even if the map's *contents* do. Quest progress most plausibly
lives inside such a map's keys/values, not in a reassigned variable, which
would make it invisible to this diff technique as built. Fixing this means
snapshotting known quest-related ds_map/ds_list contents specifically (e.g.
`ds_map_keys`/`ds_map_find_value` on `global.questDataRepo`), not just
top-level variable identity - another rebuild, not yet done.

**Session 6 broadening (per tester's "collect more data than not enough"):**
one rebuild, several additions together:
- `CiExpandContainer`: snapshots now expand one level of an array's elements
  or a ds_map/ds_list's key/value pairs (capped at 40 entries), instead of
  the opaque reference id `Describe()` alone gives. Applied unconditionally
  to the player/`Profile_Manager_obj` instance snapshots (small, bounded
  sets - catches `Profile_Manager_obj`'s `pressedArray`/`inputState`/
  `mousePressedArray` arrays, which were invisible before this), and to any
  global that is itself an array or whose name matches a broadened keyword
  list (quest/interact/collect/pickup/target/hover/usekey/accept/complete/
  select/active/current/near) - not to all ~3500 globals unconditionally,
  to avoid a real frame stall.
- `instance_destroy` hooked, filtered to Self or its first argument being a
  `Quest_Object_Parent_obj` descendant - cheap and rare, catches the exact
  moment and calling context of the item's own destruction.
- Noise cut: the `Enemy_Parent_obj`/`Collision_Parent_obj` collision-check
  spam from the player-filtered spatial builtins (session 5) is now counted
  but not logged, so it can't burn the shared trace budget before a
  genuinely new match appears.
- `citrace snap1` now also finds the nearest quest item instance and dumps
  its own variables unfiltered (not diffed - the item is expected to be
  destroyed by snap2, so there is nothing to diff against, but seeing its
  state right before collection may still be informative).
**MEASURED 2026-09-10, session 6, breakthrough.** `citrace snap1`'s nearest-
quest-item dump (added this session) found the item itself
(`Quest_Act_01_Body_Part_obj`) carries bound-method-shaped instance
variables never found by any name search in this file:
`m_QuestActivate`, `m_QuestActive`, `m_QuestDestructible`, `m_QuestInteract`,
`m_Questpickup`, `m_QuestUseKey` - plus plain data fields `questIndex=1021`,
`questObjectType=1`, `questObjectiveNumber=0`, `canPickup=true`,
`isActive=0`, `active=0`, `distanceForPickup=0`,
`activateQuestObjectWithMouse=false`. None of the `m_Quest*` names match any
script in `hs-game-sdk` (checked) - consistent with them being GameMaker
`method()` values bound at runtime, the same "no separate script-table
entry" pattern as everything else in this file, except this time the method
itself sits in an inspectable variable instead of needing to be hooked by
name.

Also notable: `citrace stat` after the collect showed **`instance_destroy=0`**
even though the item was genuinely collected (quest progressed). The plan's
assumption that quest items get destroyed on collect looks wrong for at
least this item - `canPickup`/`isActive`/`active` look far more likely to be
the real flip.

**MEASURED 2026-09-10, session 6, final round.** Added, at the tester's
explicit request after every safer mechanism above came back negative:
`instance_deactivate_object`, `instance_deactivate_all`, `instance_change`,
and (filtered by variable *name* first via a plain string compare - not by
resolving the target instance on every call - to keep the cost for the
other ~100% of the engine's instance-variable writes at one comparison, not
a second interpreter round trip) `variable_instance_set` limited to writes of
`object_index`/`canPickup`/`isActive`/`active`/`questObjectiveNumber`/
`distanceForPickup`/`activateQuestObjectWithMouse`/`itemActive`/`questIndex`
on a `Quest_Object_Parent_obj` descendant.

A room-change guard was also added (`citrace snap2` now compares the current
room against the one recorded at `snap1` and flags a mismatch) after one
round's diff showed the player's own health bar/shadow/HUD-talent instance
ids had all changed - a room transition or death/respawn between the two
snapshots, not a result of the collect. That round's data was discarded; a
clean retest with the room confirmed unchanged reproduced the same
"item vanished from the family, zero calls on every hook" result.

**Final result, all four:** `instance_deactivate_object=0`,
`instance_deactivate_all=0`, `instance_change=0`,
`variable_instance_set(marker names)=0`, confirmed on a clean, room-unchanged
collect. No freeze or stall occurred with the filtered `variable_instance_set`
hook active.

**Overall conclusion.** Across 13 rebuild/relaunch cycles: 5 named scripts, 16
anonymous closures (7 on `Quest_Object_Parent_obj`'s Create event, 9 on
`Profile_Manager_obj`'s), 8 spatial/collision builtins filtered to the player
and `Profile_Manager_obj`, and 5 object-lifecycle/variable-write builtins
filtered to the quest item itself - 34 distinct hooked call sites in total -
all measured **zero calls** on multiple genuine, confirmed collects (quest
progress verified by the tester each time), with room-change and
player-recreation artifacts specifically ruled out as confounds. The one
positive finding in the entire session (`keyboard_check_pressed` reading key
`70`/F, `Self = Profile_Manager_obj`) leads to a generic input-abstraction
object with no quest-specific state and no further outward calls. The
mechanism that actually removes a collected item from
`instance_number`/`instance_find` - real and repeatedly confirmed, since it
happens on every clean test - is not reachable through any hookable
script-table entry or engine builtin a live YYTK session can intercept. It is
most consistent with either a lower-level engine primitive with no
corresponding named/hookable builtin, or bytecode inlined in a way this
tracing approach cannot see. Per `agents.md`, going further would require
reading decompiled script bodies, which is out of scope for this toolkit.

**MEASURED 2026-09-10, session 7.** Realized every hook in this file went
through `HookOneScript`, which always prepends `"gml_Script_"` - correct for
script assets and the anonymous closures GameMaker nests inside an object's
Create event (both genuinely carry that prefix), but structurally incapable
of ever reaching an object's own built-in event code, named
`gml_Object_<ObjName>_<Event>_<N>` with no such prefix. Added
`HookRawNamedRoutine` (identical to `HookOneScript` minus the prefix) and
tried 22 raw names directly: `Quest_Object_Parent_obj`'s `Step_0`,
`Destroy_0`, `Create_0`, all 12 `Mouse_0..11` subtypes, `Alarm_0..3`, plus
`Profile_Manager_obj_Step_0`, `Player_obj_Step_0`, `Player_obj_Mouse_3`.
**All 22 returned "not found" (status 14) at hook-install time, before any
live collect was even needed.** This is a clean, cheap, structurally
different result from every prior "0 calls" measurement: those hooks
installed successfully and were confirmed live not to fire; these never
resolved to an address at all. Conclusion: this runtime's named-routine
table (`GetNamedRoutinePointer`) only indexes callable scripts and the
closures GameMaker splits out of them - it does not index built-in
object-event code under any naming convention tried. That code exists only
as raw native functions referenced from each object's own `OBJT` record
(the event-pointer lists the SDK extractor's own comment documents at
`tools/extract_and_generate_sdk.py`'s `OBJ_OFF_*` constants, just past the
fields currently read), unreachable by name at runtime. This closes the
"maybe I've been hooking the wrong prefix" possibility cleanly and confirms
native disassembly is the only remaining path - see §Native decompilation
below.

## Native decompilation (Ghidra)

**MEASURED 2026-09-10, session 7.** `data.win` has no `CODE` chunk at all,
and `Hero_Siege.exe` is 280 MB - this is a **YYC (YoYo Compiler) build**:
every GML script and object event is compiled to native x86-64 machine code
linked into the executable itself, not interpreted bytecode. "Decompiling"
here means native binary reverse engineering (Ghidra), not reading GML
pseudocode from a VM build.

This has already been done once for this exact game (a different subsystem):
`HSCraftSim/RESEARCH.md` documents a full Ghidra-headless decompile of
`DoCraftResult` (the crafting cube executor) against `Hero_Siege.exe.aurie_backup`
(build `AnkerGames S10`, SHA-256 `2034fad4…`, image base `0x140000000`).
`HSCraftSim/tools/lift_yyc.py` is a general "annotated Ghidra decompile ->
pseudo-GML" lifter with a documented, verified YYC helper-function contract
(the hardest part of reverse-engineering a YYC binary - what each
`FUN_0x0001xxxxxxxx` runtime helper does generically). The Ghidra project
itself and its raw decompile output (`research/decomp/`, a Ghidra script
`HsDecomp.java`/`HsSwitchFix.java`, and `strpool.json`) were local-only
scratchpad artifacts from a prior session and no longer exist on this
machine - only `lift_yyc.py` (our own code) and the RESEARCH.md writeup
(measured facts, not decompiled text) were committed, per `agents.md`.
Neither Ghidra nor a matching JDK is installed here yet.

**Session 7, in progress.** Installed Temurin JDK 21 (winget) and Ghidra
11.3.2 (`C:\Users\Administrator\ghidra\ghidra_11.3.2_PUBLIC`, local machine
only - not part of the repo). Imported `Hero_Siege.exe` into a new headless
project (`C:\Users\Administrator\ghidra_projects\HeroSiege`, also local-only).
Rather than run a full auto-analysis pass on a 280 MB binary (likely hours),
pulled live RVAs for the 7 `Quest_Object_Parent_obj` and 9
`Profile_Manager_obj` Create-event anon closures via the existing `naddr`
plugin command (confirmed genuine game addresses, not our own hooks, since
`citrace` was never armed this launch) and wrote a targeted Ghidra script
(`DecompileTargets.java`, local-only) that creates+decompiles just those 16
functions by address, skipping whole-binary analysis entirely.

**Progress made, then stopped deliberately.** All 16 target functions
decompiled successfully (`decomp_output.txt`, local-only, 705 KB). The raw
output is exactly as unreadable as expected without symbols - hundreds of
`func_0x0001xxxxxxxx(...)` calls per function. Identified several of the
most-called low-level runtime helpers by decompiling *them* too
(`DecompileHelpers.java`, `helper_output.txt`, both local-only) and reading
their bodies directly - the same classification technique HSCraftSim's
`RESEARCH.md` documents doing once already, redone here because the old
cheat sheet's exact addresses don't reliably carry over to this build:

- `0x140189c30` - RValue destructor/release (refcount decrement, frees on
  reaching 0). Matches `RVdtor` in HSCraftSim's documented helper contract.
- `0x140189e60` / `0x140189d60` - RValue copy-assign (destroy old value if
  refcounted, then copy the new 16-byte value+kind pair). Matches `COPY`.
- `0x140189e00` - set an RValue to `undefined`.
- `0x140189e20` - set an RValue to a real number.
- `0x14b49a090` - `Truthy(x)` (boolean-conversion check on a call result).
- `0x14b488f40` - **the generic script/builtin call dispatcher**: indexes a
  table (`base + id*0x18`) by a numeric ID, loads a function pointer from
  that entry, and calls it with `(result, self, other, argc, args)`. This is
  the mechanism `CallBuiltin`/direct GML calls compile down to.

Reading `Qo_Anon4737` (the smallest target, fully) with that map applied
showed a real, readable *shape*: something resolved off `self` through a
vtable-style indirect call, a dispatcher call (`0x14b488f40`) with a
constant ID and one argument, a `Truthy()` check on the result, and -
conditionally on that check - a call that looks exactly like a field *write*
target-based on a constant ID (matching `FUN_14b4c0db0`'s `x.f = v` pattern
from HSCraftSim's contract, including the same `0x80000000` "no index"
sentinel). That is structurally the right shape for "if some check passes,
write a field" - plausibly the collect marker being set - but the IDs
themselves (`_DAT_150740ee0`, `_DAT_1506d7a08`, `_DAT_1506e5ce8`) are opaque
numbers without a name table to resolve them against.

**Attempted a shortcut, caught the mistake before acting on it.** Rather
than reverse-engineer the ID→name table from scratch, tried reading
`_DAT_150740ee0`'s live value directly from the running game (computing the
live address from Ghidra's RVA + the live process's image base, both
already known from earlier `naddr` output) via the plugin's existing
`readmem` command. The first 8 bytes looked like a small integer (a
plausible "ID"); the second 8 bytes looked like a valid pointer into
`Hero_Siege.exe`, so it was followed - and led to unrelated string literals
(`"playerSequences"`, `"playerSequencePaused"`). That disproved the reading:
`(**(code **)(*param_1 + 8))(...)` is a **virtual call through the
instance's own vtable**, a different and more fundamental part of the YYC
object model than the `GetVar`/field-access pattern it was being matched
against. This was caught before any conclusion was drawn from it or any code
was written against it, but it demonstrates real risk of misreading this
class of code with confidence - which matters directly here, since acting on
a wrong read could mean writing a collect call that corrupts quest state
(exactly the failure mode the plan warned against from the start).

**Decision, tester's call:** stop the Ghidra investigation at this point
rather than continue accepting that risk profile. See §Final status below.

Two safe (read-only, non-invasive) follow-ups added for the next round:
- `CiTryResolveMethod`: calls GameMaker's `method_get_index()` +
  `script_get_name()` on any object-typed variable - resolves what script a
  bound method wraps *without calling it*, wired into the same
  `CiExpandContainer` path the snapshots already use.
- The nearest quest item is now snapshotted *and diffed* (not just dumped
  once), on the theory it survives the interaction rather than being
  destroyed - so whichever of `canPickup`/`isActive`/`active`/
  `questObjectiveNumber` actually flips should show up automatically.
- Container-expansion cap raised 40 -> 100 entries (missed half of
  `Profile_Manager_obj`'s 78-element `inputState` array at 40).

**MEASURED 2026-09-10, session 6, second breakthrough.** Diffing the nearest
quest item across collects was unreliable at first with multiple identical
items on the ground (the `#N` in `CiDescribeInstance`'s output is the
*object type* index, shared by every item of that kind, not a unique
instance id - two different physical items were compared as if they were
one). With exactly one item left, the same technique gave a clean result:
`citrace snap2` reported **"no quest item instance found nearby"** after a
genuine collect - the item vanished from the `Quest_Object_Parent_obj` family
enumeration entirely - while `citrace stat` in the same round still showed
**`instance_destroy=0`**. GameMaker excludes *deactivated* instances from
`instance_number`/`instance_find` while keeping them alive in memory - this
is that signature, not destruction. Added (not yet tested):
`instance_deactivate_object` (GameMaker's single-instance form - takes either
an object index, deactivating the whole type, or a specific instance
reference, distinguished in the filter) and `instance_deactivate_all`
(Self-filtered, in case it's issued as a broadcast from the item's own code),
both filtered the same way as `instance_destroy`.

- Considered and explicitly **not** added: hooking `variable_instance_set`
  filtered to quest-item targets. It would catch any write to the item right
  before destruction, but it is one of the hottest builtins in the entire
  game (every object's every per-frame field write), and this session's own
  logs already recorded one unrelated 3-second frame stall - doubling the
  interpreter cost of every instance-variable write for the game to trace a
  single feature was judged too risky for a live session.

## 2. Hold/channel vs. instant completion

*Not measured.* Determines whether one call per item is enough, or whether
the synthetic call needs to hit a *completion* path distinct from a *start*
path.

## 3. `gml_Script_LootBlocksUseKey` — does ground loot suppress the fetch?

*Not measured.* Stand near a loot pile with a quest item also on screen and
check whether the interaction fires at all.

## 4. Accepted-quest gate — do quest item instances exist before acceptance?

*Not measured.* `petquest stat`'s `family objs seen=` counter already shows
whether *any* `Quest_Object_Parent_obj` descendants are on screen at all
before a relevant quest is accepted — check that first, cheaply, before
reaching for `quest_exists` / `GetQuestObjectives` / `IsQuestCompleted`. If
instances only spawn post-acceptance, the gate is free. If they pre-exist,
this section needs to record where the instance carries its quest id.

## 5. Parent enumeration through YYTK

*Partially self-verifying.* `PetQuestCollectorTick()` already calls
`instance_number` / `instance_find` on `Quest_Object_Parent_obj`'s resolved
index, which is standard GameMaker family semantics (a parent object index
enumerates every descendant, not just exact matches) — if `petquest stat`'s
`family objs seen=` count matches what's visibly on screen during a session,
this item is confirmed. Record the session's observed count here.

## 6. The pet at runtime

*Partially self-verifying.* `petquest stat`'s `pet-seen ticks=` counter
increments whenever `instance_number(Companion_obj)` is nonzero — summon a
pet and confirm it moves. Still needs manual confirmation of whether
`Companion_Pickup_obj` (978) already absorbs anything, which would shrink
Phase 3's scope.

## 7. Screen-wide radius stability

*Partially self-verifying.* `PetQuestCollectorTick()` already reads
`view_get_camera(0)` / `camera_get_view_*` each tick and counts a `no-camera
ticks=` value in `petquest stat` for when the view size comes back ≤0.
Confirm across zoom levels and zone types that this stays 0 during normal
play.

---

## Exit criterion (plan §5)

**Reached on the "blocked" branch, 2026-09-10.** B1 is blocked (no
discoverable call target) and B2 - both as originally scoped
(`CheckPlayerInteraction`, never called) and every builtin-hook variant tried
in its place - is disproven, not just untried. 34 distinct hooked call sites
across 13 live rebuild/relaunch cycles, all zero calls on multiple confirmed
collects. See item 1's final conclusion for the full list.

**Correction to an earlier version of this note:** `agents.md`'s decompiling
rule was mis-stated in this session as putting decompilation itself out of
scope - it doesn't. The rule (now reworded in `agents.md`) only restricts
where decompiled *output* is allowed to land (never committed/pasted into a
tracked file); analyzing the game locally, including with a native
disassembler, is a legitimate technique. This session did pursue it (see
§Native decompilation above): Ghidra + a matching JDK installed, the binary
imported, 16 relevant functions decompiled, and several low-level runtime
helpers identified by reading their own bodies. It surfaced a real,
promising shape (a dispatcher call, a truthy check, a conditional field
write) but not a confirmed field/script name - and one attempted shortcut
(following a pointer live from the running game) turned out to be a
misreading, caught before it fed into any conclusion or code. Continuing
would mean more of that same open-ended, uncertain work, with real
consequences if a misread went uncaught (a wrong collect call could corrupt
quest state) - the tester's call was to stop here rather than keep absorbing
that risk.

## Final status (2026-09-10)

**Blocked, by design, not by lack of effort.** Every mechanism reachable
through live measurement (script hooks, builtin hooks, object-lifecycle
hooks, whole-state snapshot diffing) and the first substantial pass at
static decompilation all came back without a usable answer. The scaffolding,
contract tests, and panel toggle built earlier in this session are complete,
tested (100/100), and untouched by this outcome - `petquest 1` arms
correctly and the read-only tick counts real candidates - only the actual
collect call (plan Phase 2 step 4-5) and pet movement (Phase 3) remain
unimplemented, gated on this research.

**Re-scoped as Plan B4** (`ForgePact/docs/pet-quest-collector-plan-b4-input-simulation.md`):
rather than find the internal collect mechanism, simulate the two inputs a
real player produces (hover + interact keypress) and let the game's own logic
do the rest. Sidesteps everything documented above - it never needs to know
the mechanism this whole document failed to find.

**Update 2026-09-10: B4's Phase 0 ran live and its central premise is
disproven too** - see `ForgePact/docs/pet-quest-collector-b4-research.md`. A
real F press is faithfully recorded in `Profile_Manager_obj.inputState`
(indices 30 and 60, `1 -> 0`), and the cursor is faithfully recorded in that
same object's `mouse_x_prev`/`mouse_y_prev` - but writing either changes
nothing in game, because both are downstream mirrors of real device input
rather than the state the game's own checks read. `mouse_x`/`mouse_y` turn
out to have no runtime variable-table entry at all. So the same wall this
document hit from the inside (the collect mechanism is not reachable through
GML) also stands from the outside (the *inputs* to that mechanism are not
reachable through GML either).

**Left in place locally for a future continuation** (none of this is
committed, per `agents.md`):
- Ghidra 11.3.2 at `C:\Users\Administrator\ghidra\ghidra_11.3.2_PUBLIC`,
  Temurin JDK 21 (installed via winget).
- The imported project at `C:\Users\Administrator\ghidra_projects\HeroSiege`
  (already has `Hero_Siege.exe` loaded - no need to re-import).
- `C:\Users\Administrator\ghidra_scripts\DecompileTargets.java` /
  `DecompileHelpers.java` - both parameterized by a plain address list at the
  top; extending either to a new set of RVAs is a one-line edit before
  re-running `analyzeHeadless ... -process Hero_Siege.exe -noanalysis
  -scriptPath ... -postScript <name>.java`.
- `decomp_output.txt` / `helper_output.txt` under
  `C:\Users\Administrator\ghidra_projects\` - the 16 target functions and 16
  runtime helpers already decompiled this session.
- The `naddr`/`naddrall` plugin commands (already existed) remain the
  fastest way to get fresh live RVAs for any newly-hooked script name to feed
  back into these scripts as new anchors.

**Loose end, resolved.** The `data.win` STRG-chunk search for the quest
field names (`m_QuestInteract`, `canPickup`, etc.) queued earlier finished
after the investigation had already stopped: `0` hits across all 80,318
string entries. Consistent with the rest of this session's findings -
variable/field names live in a separate table this build doesn't expose
(no `VARI` chunk either), not the general string literal pool, so this
wasn't a viable shortcut to begin with. Nothing further to act on.

**Most promising next thread, if resumed:** finish classifying
`0x14b488f40`'s dispatch table (`base + id*0x18`, entries holding a pointer
and a function pointer at `+8`) - if that table's base can be located
statically and it turns out to be the same ID space `script_get_name()`
resolves at the GML level, the dispatcher's call-site IDs become resolvable
live, sidestepping the need to decode the ID scheme by hand entirely.

**Now carried forward by Plan C**
(`ForgePact/docs/pet-quest-collector-plan-c-direct-invocation.md`, drafted
2026-09-10; Phase C0 tooling implemented 2026-09-11, live round pending - see
`ForgePact/docs/pet-quest-collector-c-research.md`). It keeps the
dispatcher-table thread above as Phase C1 step 3,
adds a new anchor the attempt above lacked - a stack walk from inside the
already-firing `keyboard_check_pressed(70)` hook, turning this document's one
confirmed live signal into concrete RVAs to decompile - and, crucially, puts a
whole phase of *non*-decompiler work in front of it: the `m_Quest*` bound
methods this document discovered (session 6) can be invoked directly without
ever resolving a name, and `CiTryResolveMethod`, added at the end of session 7
to identify them, has never actually been run.
