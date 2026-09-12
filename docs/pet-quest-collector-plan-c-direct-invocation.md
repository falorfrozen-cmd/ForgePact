# Pet Quest Collector — Plan C: direct invocation, then native decompilation

Status (2026-09-11, final session): **MECHANISM CONFIRMED LIVE. Phase C0's
exit criterion is met on the positive branch; Phase C2 is unblocked.** On a
real `Quest_Act_01_Brick_obj` collect, with read-only detours installed at
the functions' own addresses:

> with the quest item as `self` and `Loot_Manager_obj` as `other`, the item's
> own **`m_Questpickup`** method value is invoked with **one** real argument
> (`1` as measured). It calls
> `update_quest(questIndex, questObjectiveNumber, questValue)` — measured
> `(1002, 0, 1)` — and `QuestSaveUpdate`, so the objective credit is inside
> the call, not something we have to add.

Two results came with it, both recorded in
`ForgePact/docs/pet-quest-collector-c-research.md`:

- **The script hooks were blind.** `CheckPlayerInteraction` measured
  `native=3840, table=0` in one session. Every "0 calls" this investigation
  ever got from `HookOneScript`/`HookRawNamedRoutine` measured the
  instrument, not the game — see the B1/B2 row below.
- **The caller named in the static reading was wrong.** `PlayerMouseAction`
  measured 0 calls; the real path runs inside `Loot_Manager_obj`
  (RVA `0x86E0AC0`), which also calls `ClearSpecificInput` — the `inputState`
  `1 -> 0` flip B4 measured and could not account for.

**Phase C2's invoke path is now built, tested and deployed: `citrace collect
confirm`.** It reproduces the measured call exactly — item as `self`,
`Loot_Manager_obj` as `other`, the item's own `m_Questpickup`, one real
argument defaulting to the measured `1`, through the runtime helper at
`exe+0xB489070` — and it reproduces the game's own `canPickup` /
`lootType == 0` gates, refusing rather than proceeding if either fails.
A new `native` invoke path fronts `kCiInvokePaths`; the nine measured-negative
shapes are demoted, not removed. 131 contract tests pass.

**RUN LIVE 2026-09-11 — it works. The quest counter went 7/15 → 8/15.** Save
was backed up first; the call reproduced the real collect's inner calls
exactly (`m_Questpickup` with `argc=1 a0=real:1`, then
`update_quest(1002, 0, 1)`), the item left the family count, and the
objective was credited. That is the original plan's §11 pass condition, met
for the first time in the investigation.

One deviation to record: it ran on a live Act 1 brick rather than
`Quest_Toy_Bear_obj`, so §4 rule 2 was not followed. No damage resulted, but
the rule was right.

**Remaining before Phase C2 ships:** wiring the call into
`PetQuestCollectorTick()` (still untouched, still counting only) with pacing,
the accepted-quest gate, per-item re-reads of `canPickup`/`lootType`, and
`questValue` as the argument instead of the hardcoded `1` — plus two
unobserved side-effects worth checking, since a plugin collect skips what
`Loot_Manager_obj` does around the call (no `InventoryLogAddItem`, no pickup
sound/effect). See the research notes' closing section.

Earlier status (2026-09-11): **Phase C0 closed, negative — no collect
mechanism found. Phase C1 underway and already productive.** All seven C0 checklist
items were measured live; none yields a collect. C0.2 (the plan's "main
event" — invoking a resolved `m_Quest*` method directly) is the most
thoroughly tested: nine call shapes across several live rounds, all faulting
or unable to reach the target instance, including a real `with`-context
attempt via `InvokeWithObject` that took three correction rounds to run
correctly and still could not locate an instance every other command in the
same file sees without difficulty. Full evidence, including the exact
`AurieStatus` codes at each stage, in
`ForgePact/docs/pet-quest-collector-c-research.md`.

**Phase C1 (§3, Ghidra) has started**, per the plan's own gate now that C0 is
negative, and has already produced results independent of any specific
mechanism: `citrace symdump` (new) walks the runtime's own script table and
produced a complete symbol file (6,254/6,254 resolved), applied to the Ghidra
project (6,167 functions named, 5,334 newly created, 0 failed) — the stripped
280 MB binary is now a named one, reusable by any future tool in this
toolkit, not just this mod. `citrace stackwalk` (new) captured a clean
15-frame native call chain from a real F press; decompiling it independently
confirmed the generic script/builtin dispatcher (`0x14b488f40`) a prior
session found by reading disassembly by hand, and recovered its static table
base address, which that session did not have.

**§3.2 step 3 (the dispatcher-table thread) is now also closed, live, the
same session.** The table's entries needed no ID-scheme decoding: each holds
a plain `const char*` name, a function pointer, and an argument count,
directly. `citrace dispatchdump` (new) reads the whole table — 2867 builtins.
`citrace dispatchtrace` (new) hooks the dispatcher once and, unlike every
prior name-guessed hook, sees **every** builtin call the game makes. Armed on
a real F-press collect, it captured 118+ frames and found **zero** calls to
anything interact/pickup/lifecycle-shaped — the most thorough negative result
in the whole investigation, closing off "observe it through a dispatched
call" as a category entirely, not just the names anyone happened to guess.
Full detail in the research doc. §3.2 step 4 (a stack walk on the removal
path) remains open, but has no known signal left to hook from. Phase C2 (§5)
is
untouched: `PetQuestCollectorTick()` still only counts, and a contract test
fails if a collect call appears there before a mechanism is confirmed.

Third plan for this mod, and the first one written with all three prior
mechanisms already closed by measurement:

| Plan | Idea | Outcome |
| --- | --- | --- |
| B1 (`pet-quest-collector-plan.md` §4) | Call the script the interaction invokes | ~~**Blocked** - no named script ever fires (34 hooked call sites, 0 calls)~~ **Result withdrawn 2026-09-11**: those hooks swap a script-table pointer that compiled GML never reads, so the zeroes measured the instrument. See "The hooks were blind" in the C research notes. |
| B2 (same §4) | Hook the interaction gate and lie to it | ~~**Disproven** - the gate it named is never called~~ **Partly withdrawn, same reason.** The gate it named (`CheckPlayerInteraction`) is real and does run; only the *builtin* variants (hooked with `MmCreateHook`) keep their 0. |
| B4 (`pet-quest-collector-plan-b4-input-simulation.md`) | Fake the player's hover + keypress | **Disproven** - GML-visible input state is a downstream mirror; writing it does nothing (`pet-quest-collector-b4-research.md`) |

Target branch `release/v1.3.17`, same as the original plan. Findings go to a
new `ForgePact/docs/pet-quest-collector-c-research.md`. Per `agents.md`:
measured behavior, names and indices only in tracked files - decompiler output
stays local.

---

## 1. Why there is anything left to try

Every prior attempt shares one assumption that is now worth attacking
directly: **that the code we want must be reached by *name*.** B1 needed a
script name to call. B2 needed a name to hook. Session 7 established that the
runtime's named-routine table only indexes callable scripts and the closures
GameMaker splits out of them - object event code is not in it under any naming
convention (22 raw names, all "not found" at install time, before any live
test was needed). That is a hard wall for anything name-based, and all three
plans hit it.

But two doors into that same code do not use names at all, and neither has
been tried:

1. **The callable is sitting in a variable.** Session 6 found the quest item
   instance carries six bound-method-shaped instance variables -
   `m_QuestActivate`, `m_QuestActive`, `m_QuestDestructible`, `m_QuestInteract`,
   `m_Questpickup`, `m_QuestUseKey` - none of which match any script name in
   `hs-game-sdk`. A GameMaker method *value* can be invoked directly; it does
   not need to be found in the name table, because we already hold it.
2. **Events can be performed by number.** `event_perform(type, number)` runs
   an object's own event code through the engine, keyed by numeric event id -
   no name lookup anywhere in the path. The exact code that
   `HookRawNamedRoutine` could not resolve by name (`..._Mouse_0..11`,
   `..._Step_0`, the alarms) is reachable this way.

Both are ordinary GML, callable through the `CallBuiltin`/`CallGameScriptEx`
chokepoints this plugin already uses everywhere. Neither needs a
disassembler.

**And one piece of already-built tooling has never been run.** Session 7 added
`CiTryResolveMethod` (`method_get_index()` + `script_get_name()` - resolves
what script a bound method wraps *without calling it*) specifically to answer
what the `m_Quest*` methods are. Checked 2026-09-10 against the full 3.9 MB
`out.txt` history: **zero `->method:` lines have ever been logged.** The six
methods are still recorded only as bare `object/struct`. The single
highest-value read-only measurement available was built, documented as "not
yet tested", and then the investigation moved on to B4 without it.

---

## 2. Phase C0 — everything that does not need a decompiler

Ordered by (value / cost). C0.1-C0.2 are the reason this plan exists; the rest
are cheap enough to batch into the same build per `agents.md`'s
"hook every candidate in the same build" rule.

### C0.1 Resolve the six `m_Quest*` methods (read-only, zero risk)

Run `CiTryResolveMethod` against a live quest item and record what each of the
six methods wraps. `citrace snap1` already captures the item's ~59 variables
through `CiExpandContainer` (which calls the resolver) but prints only a
count - so this needs a command that *prints* that dump, not new resolution
logic. One small addition, e.g. `citrace item` → nearest quest item, every
variable, expanded and method-resolved.

Exit signal: six names (or six "unresolvable" results). Either is decisive:
- **Names that match `anon@N@gml_Object_Quest_Object_Parent_obj_Create_0`** -
  ties the methods to the seven closures already hooked (and already measured
  at 0 calls during a real collect, which is informative but does not stop us
  calling them ourselves).
- **Names not in `hs-game-sdk`** - new call targets B1 never had.
- **Unresolvable** - still fine: C0.2 does not need the name.

### C0.2 Invoke a resolved method directly (the main event)

This is B1's idea with B1's blocker removed. Call `m_Questpickup` /
`m_QuestInteract` / `m_QuestUseKey` on the item instance, with the item as
`self` and the player as `other` (the plan's original credit decision -
`pet-quest-collector-plan.md` §1 - is unchanged and satisfied by that
context).

**Open question to settle in step 1, not assume:** how this runtime accepts a
method value. Candidates, in order to try:
1. `script_execute(methodValue, args...)` - accepts method values in modern
   runtimes.
2. `method_call(methodValue, [args])`.
3. `method_get_index(methodValue)` → `script_execute(thatIndex, ...)` - loses
   the bound `self`, so `CallGameScriptEx` with an explicit self/other is the
   better form of this fallback.
4. `CallGameScriptEx(res, "<name from C0.1>", item, player, {})` if C0.1
   returned a real name.

**This is the first step in this entire investigation that mutates quest
state, so it is the first one that can do damage.** §4's safety rules are not
optional here.

### C0.3 `event_perform` on the item's own events

Independent of C0.1/C0.2 and equally name-free. With the quest item as
`self`, perform its own event code by number.

> **The call shape is already proven in this plugin** (found while
> implementing C0, 2026-09-11): the Density research command `spawnforce` has
> been calling `CallBuiltinEx(res, "event_perform", spawnerInstance, ...,
> { ev_alarm, n })` against live `Enemy_Creator_obj` instances for several
> sessions, and it demonstrably makes those spawners run their own alarm event
> code and spawn enemies. So `event_perform` through YYTK reaching object
> event code is not in question — only whether the *quest item's* events do
> anything useful is.

Events to perform:
- The 12 mouse events (`ev_mouse` 0-11) - especially `ev_left_press`,
  `ev_left_release`, and the "no button"/enter/leave hover subtypes, which is
  where continuous hover detection would live.
- `ev_step`, the alarms (`ev_alarm` 0-3), and `ev_other`'s user events.

`event_perform_object` is the variant worth having too, since it runs another
object's event in the current instance's context.

Exit signal: any of them produces a visible collect, a prompt, or a change in
the item's `canPickup`/`isActive`/`active` flags. This directly tests the code
`HookRawNamedRoutine` proved unreachable by name in session 7.

### C0.4 Full method-resolved dumps of the objects never inspected

`citrace snap1` captures the player (116 vars) and `Profile_Manager_obj` (56)
but the *quest manager* has never been dumped at all. Batch, in one build:
- `Quest_Manager_obj` - the obvious owner of quest state, excluded from the
  collector's *target* set by the original plan §3 and never *inspected*.
- `Controller_obj` - carries the satanic-zone state this toolkit already
  reads, so it is known to hold live game state.
- The quest item's full dump (C0.1) and the player's, both with method
  resolution applied, looking for further method values and for anything
  shaped like "what is the cursor currently over".

### C0.5 The hover-target globals, resolved and called

Session 4's `dump` sweep found `global.GetMouseTarget`,
`global.PlayerGetMouseTarget`, `global.GetQuestHoverDescription`,
`global.GetMouseDisabledTarget`, `global.CanISeeTarget`, and a live
`global.hoverTooltip = 1` flag. They were hooked (0 calls) but **never read as
values or called**. If any holds a method value, C0.2's invoke path applies
directly. Broaden the `dump` sweep in the same round: `use`, `key`, `press`,
`activate`, `collect`, `pickup`, `interactable`, `focus`, `selected`.

`global.hoverTooltip` in particular is a live, settable global that is
*already* 1 - worth knowing what writes it and what reads it.

### C0.6 Confirm the deactivation model (diagnostic)

Session 6 established that a collected item vanishes from
`instance_number`/`instance_find` while `instance_destroy` stays at 0 - the
signature of `instance_deactivate_*`. Never confirmed directly. Call
`instance_activate_object` on the item's object index after a collect: if the
item reappears, the model is confirmed and the *inverse* (deactivating an item
ourselves) becomes a known-good way to make an item disappear - which is
explicitly **not** a collect (no progress credit) and must not be mistaken for
one, but does pin down what the real mechanism's last step looks like.

### C0.7 Re-check `petquest stat`'s own counters

Free. The scaffolding's read-only tick has never had its numbers read in a
session where quest items were actually on screen (`family objs seen=`,
`on-screen=`). Confirms Phase 0 items 5 and 7 of the original plan
(family enumeration and camera-bounds stability) at zero cost while any of the
above runs.

**Phase C0 exit criterion:** either a confirmed mechanism that collects an
item *and* advances its objective (→ go to Phase C2, implementation), or all
seven come back negative (→ Phase C1, Ghidra).

---

## 3. Phase C1 — native decompilation, if and only if C0 fails

This is a YYC build: `data.win` has no `CODE` chunk, and every script and
object event is native x86-64 inside the 280 MB `Hero_Siege.exe`
(`pet-quest-collector-research.md`, "Native decompilation"). The previous
attempt got real infrastructure standing and then stopped deliberately, after
one misread was caught before it reached any conclusion. Everything it built
is still on this machine.

### 3.1 What already exists (do not rebuild it)

- Ghidra 11.3.2 at `C:\Users\Administrator\ghidra\ghidra_11.3.2_PUBLIC`,
  Temurin JDK 21.
- An imported project at `C:\Users\Administrator\ghidra_projects\HeroSiege`
  (`Hero_Siege.exe` already loaded - no re-import).
- `DecompileTargets.java` / `DecompileHelpers.java` in
  `C:\Users\Administrator\ghidra_scripts\` - both parameterized by a plain
  address list at the top; pointing them at new RVAs is a one-line edit.
  Run headless, `-noanalysis`, targeting specific addresses - never a
  whole-binary analysis pass on a 280 MB image.
- `decomp_output.txt` / `helper_output.txt` - 16 target functions and 16
  runtime helpers already decompiled.
- `HSCraftSim/tools/lift_yyc.py` - our own annotated-decompile → pseudo-GML
  lifter, with a verified YYC helper contract.
- A partial helper map for *this* build: RValue destructor `0x140189c30`,
  copy-assign `0x140189e60`/`0x140189d60`, set-undefined `0x140189e00`,
  set-real `0x140189e20`, `Truthy` `0x14b49a090`, and the generic script/builtin
  dispatcher `0x14b488f40` (indexes `base + id*0x18`, loads a function pointer,
  calls it with `(result, self, other, argc, args)`).
- `naddr`/`naddrall` plugin commands - live RVAs for any hookable name.

### 3.2 The anchor problem, and the new way to solve it

The previous pass failed for one reason worth stating plainly: **it had no
good anchors.** It decompiled 16 anon closures picked because they were
*hookable*, not because they were *involved*, and the call-site IDs inside
them (`_DAT_150740ee0` and friends) were opaque without a name table.

There is now a confirmed live signal that pointed nowhere before but is an
excellent anchor here: **`keyboard_check_pressed(70)` fires, with
`Self = Profile_Manager_obj`, returning 1 on exactly the frame F is pressed.**
That hook is already installed and already fires reliably. Whatever decides
"F + hovering a quest item → collect" runs downstream of it.

So, in order:

1. **Capture the call stack from inside the existing hook.**
   `RtlCaptureStackBackTrace` inside `Hook_Ci_KeyboardCheckPressed`, only on
   the `key==70 && Result!=0` edge (fires once per press, so the cost is
   irrelevant), logging return addresses minus the live image base = RVAs in
   `Hero_Siege.exe`. This is a small plugin addition and it is the single
   highest-value item in this phase: it converts the one confirmed live signal
   into **the exact chain of native functions that read the interact key**,
   which is precisely the set `DecompileTargets.java` should be pointed at.
   Nothing in the prior investigation attempted this.
2. **Decompile that chain**, innermost caller outward, with the helper map
   above applied and `lift_yyc.py` for the mechanical parts.
3. **Resolve the dispatcher table** (`pet-quest-collector-research.md`'s own
   "most promising next thread"): if `0x14b488f40`'s `base + id*0x18` table can
   be located statically and its ID space turns out to be the same one
   `script_get_name()` resolves at the GML level, then every call-site ID in
   every decompiled function becomes a *name*, live, via the plugin. That
   single result would retroactively make the existing 705 KB of decompiled
   output readable, and it is testable cheaply: read the table base live with
   `readmem`, pick an entry whose ID we can cross-check against a known script
   index, and see if the names line up.
4. **Same stack-walk trick for the removal step**, if needed: no builtin was
   ever caught firing during a collect, but if C0.6 confirms deactivation,
   hooking `instance_deactivate_object` *with a stack walk* would catch the
   caller even though the previous, count-only hook showed 0 calls for our
   specific filter.

### 3.3 Discipline for this phase

The prior attempt's stopping reason was sound and applies unchanged: a
confident misread of YYC output can produce a "collect" call that corrupts
quest state. Therefore:

- **Every candidate found by decompilation gets verified live before it is
  called** - hook it by whatever handle we now have, confirm it fires on a
  real player collect, and only then invoke it ourselves. A decompiled
  reading is a hypothesis, not a finding, until a live hook agrees with it.
- No conclusion goes into a tracked doc without a live measurement behind it.
- Timebox: if the stack walk (3.2 step 1) does not yield a usable chain, or
  the dispatcher table (step 3) does not resolve, stop and re-scope rather
  than grinding. Both have crisp pass/fail signals within one session each.

---

## 4. Safety rules (binding on C0.2 onward)

The moment anything is *invoked* rather than *observed*, this stops being
read-only research. Non-negotiable:

1. **Back up the save first.** `%LOCALAPPDATA%\Hero_Siege\` (and any
   cloud-sync copy) before the first invoke test of a session.
2. **Test on a non-progress-gated item first.** `Quest_Toy_Bear_obj` (Act 8,
   Zone 8-4) is confirmed by the tester to stay collectible after its quest
   completes - the safest possible target. Only after it behaves correctly
   move to an ordinary Act 1 item.
3. **Verify progress, not disappearance.** The original plan's §11 pass/fail
   still stands: an item vanishing without its objective advancing is *worse
   than no mod*. Check `GetQuestProgress` (or the quest's own counter, as the
   tester read it during the B1/B2 sessions) after every successful-looking
   invoke.
4. **One item, one call, one observation.** No loops, no batching, no "try all
   six methods in sequence" - a wrong call is easier to attribute when it is
   the only thing that happened.
5. **Fresh character for destructive testing** where practical, so a corrupted
   quest chain costs nothing.

---

## 5. Phase C2 — implementation **(DONE 2026-09-11)**

Unchanged from the original plan's Phase 2/3, which never became wrong - only
unreachable. `PetQuestCollectorTick()`'s camera-bounds enumeration, the
64-instance budget, the static exclusion list and the panel toggle were all
built and tested; the confirmed mechanism dropped into the one `TODO(Phase 0)`
site in that tick.

**Shipped and live-measured** — two sessions, `collected=3` and `collected=6`,
with every failure counter (gate refusals, missing `Loot_Manager_obj`, lost
targets, travel timeouts) at zero. What landed:

- **Pacing:** an `Idle` → `Travel` state machine, one target at a time, with a
  24-frame cooldown between collects. No sweep.
- **Pet movement** (Phase 3, promoted because it is what makes the mod read as
  a pet fetching rather than items vanishing): the pet's `x`/`y` are written
  toward the target at a constant 11 px/frame, the same technique
  `OrbPickupTick` already uses on globes. A 240-frame timeout collects anyway
  if something holds the pet, since the walk is cosmetic and the credit is the
  point.
- **The game's own gates** (`canPickup`, `lootType == 0`) re-read from the live
  instance at collect time, never cached from selection.
- **The accepted-quest gate: NOT implemented.** The tick collects any
  `lootType == 0` family member on screen. This is the one item from the
  original plan that did not ship — see the open items in
  `pet-quest-collector-c-research.md`.

Full evidence and the per-question rationale: `pet-quest-collector-c-research.md`,
"Phase C2 SHIPPED".

---

## 6. Risks

| Risk | Mitigation |
| --- | --- |
| **Invoking an unverified method corrupts quest state** - the failure mode every plan so far has avoided by staying read-only | §4 in full: save backup, non-progress item first, progress verified per call, one call at a time. This is the main new risk this plan introduces and it is introduced deliberately. |
| A method invoked out of context (no hover, no gate) half-completes an interaction | Prefer `m_Questpickup`/`m_QuestInteract` *after* C0.1 tells us what they wrap; verify item flags (`canPickup`/`isActive`/`active`) before and after, not just quest progress. |
| `event_perform` runs an event with side effects beyond collection | Test the read-shaped events (hover enter/leave, step) before the press-shaped ones; observe item flags after each. |
| Ghidra misread produces a wrong call target | §3.3: every decompiled candidate must be confirmed by a live hook firing on a *real* player collect before it is ever invoked. |
| Phase C1 becomes open-ended, as the prior attempt did | §3.3's timebox, with two crisp pass/fail gates (stack walk yields a chain; dispatcher table resolves). |
| Legal / `agents.md` | Decompiler output stays local (already `.gitignore`d); tracked docs carry measured behavior, names and indices only. |
| Live-session cost | Per `agents.md`, C0.1-C0.7 are batched into **one** build and one relaunch, not seven. |

---

## 7. Files touched

New (done 2026-09-11):
- `ForgePact/docs/pet-quest-collector-c-research.md` (findings log)

Modified (research build only, until C2) — Phase C0 done 2026-09-11:
- `ForgePact/plugin/ModuleMain.cpp` - `citrace item`/`dumpobj`/`player`
  (C0.1/C0.4), `citrace methods` (C0.1 + C0.2 step 1, read-only), `citrace
  invoke item|global` (C0.2), `citrace event`/`eventobj` (C0.3), `citrace
  globals`/`sweep` (C0.5), `citrace activate` (C0.6), plus `citrace help` and
  the `confirm` gate / safety banner §4 requires. Still to come, **only in
  Phase C1**: the stack-walk capture inside the existing
  `keyboard_check_pressed` hook.
- `ForgePact/tests/test_pet_quest_collector_contract.py` - nine tests pinning
  the C0 invariants (release-guarded, `citrace` absent from
  `kPlayerCommands`, every mutating subcommand behind the confirm gate, the
  gate failing closed, flags/family-count/progress reported on both sides of
  every mutation, and nothing leaked into `PetQuestCollectorTick`).
- `C:\Users\Administrator\ghidra_scripts\DecompileTargets.java` - local only,
  address list edited per §3.1.

Modified for C2 — done 2026-09-11:
- `ForgePact/plugin/include/ForgePact/PetQuestCollectorMod.hpp` - status
  rewritten from "pending research" to the shipped behaviour and its gates.
- `ForgePact/plugin/ModuleMain.cpp` - `InvokeMethodValueNative()` and
  `kQuestPickupCallFnRva` placed outside the `FORGEPACT_RELEASE` guard, since
  the shipped collect needs them in the player build (`citrace collect`, which
  also uses them, stays guarded); `PetQuestItemIsCollectable()`,
  `PetQuestCollectOne()`, the `Idle`/`Travel` state machine inside
  `PetQuestCollectorTick()`, the counters `PetQuestCollectorStats()` reports,
  and `petquest arg <n>`.
- `ForgePact/tests/test_pet_quest_collector_contract.py` - the
  "not yet implemented" gate replaced by the invariants that keep the live
  collect honest (one call shape and it is the measured one, gates re-read per
  item, `other` = `Loot_Manager_obj` rather than the player, one item at a
  time with a cooldown, no pet = no collecting).
- `ForgePact/src/forgepact.py` - the `mod_pet_quest_pickup` toggle, its
  `build_cmds` line and its live `petquest 0|1` dispatch.
- `ForgePact/docs/pet-quest-collector-c-research.md` - "Phase C2 SHIPPED".
