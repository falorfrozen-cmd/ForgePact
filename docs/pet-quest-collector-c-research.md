# Pet Quest Collector — Plan C research notes

Findings log for `ForgePact/docs/pet-quest-collector-plan-c-direct-invocation.md`.
Companions: `pet-quest-collector-research.md` (B1/B2, blocked),
`pet-quest-collector-b4-research.md` (B4, disproven). Per `agents.md`: record
only names, indices and measured behavior — no decompiled script text.

**Status (2026-09-11): SHIPPED. Phase C2 is implemented and live-measured —
the mod collects.** The mechanism was confirmed first by reproduction (a
`citrace collect confirm` advanced the quest counter 7/15 → 8/15 on
"Bricks… so many Bricks"), then wired into `PetQuestCollectorTick()` and run
as the actual mod: two sessions ending `collected=3` and `collected=6`, with
`skipped(gate)=0`, `skipped(no Loot_Manager)=0`, `target lost=0` and
`travel timeouts=0`. Evidence for the mechanism is in the second-to-last
section of this file; evidence for the shipped tick is in the last.

The confirmed call: with the quest item as `self` and `Loot_Manager_obj` as
`other`, invoke the item's own `m_Questpickup` method value with one real
argument, through the runtime's call-a-method-value helper at `exe+0xB489070`
— gated, as the game gates it, on `canPickup` and `lootType == 0`.

See "The live round: mechanism CONFIRMED, and the caller I named was wrong"
for the measurement, and note the correction it carries: the caller named in
the static-reading section (`PlayerMouseAction`) is **not** on the path. The
real one is inside `Loot_Manager_obj`.

The confirmed call: with the quest item as `self` and `Loot_Manager_obj` as
`other`, the item's own `m_Questpickup` method value is invoked with one real
argument (`1` on the measured collect). It calls
`update_quest(questIndex, questObjectiveNumber, questValue)` →
`QuestSaveUpdate`, so the objective credit is inside the call.

Earlier status, kept because the reasoning still holds where it is not
corrected above:

**Phase C0 closed, negative. Phase C1's hook-and-observe half is closed,
negative. Phase C1's *read the compiled code* half found the mechanism — see
"Reading the compiled code: the collect mechanism, found". It is a static
reading; the parts of it the live round corrected are flagged in place.**

The short version: `PlayerMouseAction` (#102772, RVA `0x4E3A010`) runs
`with (mouseTarget) { if (canPickup) m_Questpickup(other.id) }` for
`lootType 0` quest objects, and `m_Questpickup` calls `update_quest` →
`QuestSaveUpdate`. The one argument, and the `self`/`other` context, are what
every C0.2 call shape was missing.

**And the reason nobody saw this live: the script hooks were blind.** Three of
those four functions were already hooked and all recorded 0 calls.
`HookOneScript` installs by swapping a pointer *inside the script-table
entry*, and this build's compiled GML calls another script with a direct
`call rel32` that never reads that table. So **every "0 calls" result from a
named-script hook in these notes — B1's headline "34 hooked call sites, 0
calls" included — measured the instrument, not the game.** See "The hooks
were blind" at the end of this file. `citrace nativetrace` (new, read-only)
re-hooks the same targets with `MmCreateHook`, which patches the address
itself, and prints both counters side by side so one live round settles it.
The `dispatchtrace` and `HookBuiltin` results are unaffected — those already
used `MmCreateHook`.

Earlier status, still accurate for the hook-and-observe work:

- A full runtime symbol table applied to the Ghidra project (6,167 functions
  named, 5,334 newly created).
- A clean 15-frame native call chain from a real F press, decompiled and
  cross-confirming the generic script/builtin dispatcher a prior session
  found by hand — plus its static table base address, which that session
  did not have.
- **The dispatcher table read live and resolved with no ID-scheme decoding
  needed** (session 7's own "most promising next thread"), producing a
  complete 2867-entry builtin table.
- **A full builtin-call trace across 118+ real frames of a genuine collect —
  zero calls to anything interact/pickup/lifecycle-shaped.** This is the
  most thorough negative result of the whole investigation: not five guessed
  names, the complete builtin surface.

See "Phase C1 anchors", "Phase C1 tooling", and "`citrace dispatchdump` /
`citrace dispatchtrace`" below.

| Item | Result |
| --- | --- |
| C0.1 resolve the `m_Quest*` methods | **Done.** All six resolve — to the same Create-event closures already measured at 0 calls. Seventh method found. |
| C0.2 invoke a method directly | **Done, negative - fully closed.** All nine call shapes tested (direct/indexed/method-call/script, with and without arguments, plus `InvokeWithObject` after three correction rounds). Every one faults or cannot reach the target instance. Plan C's central premise is disproven. |
| C0.3 `event_perform` | **Done, negative.** Fires correctly; 12 events tried, every one a no-op, including with genuine hover. |
| C0.4 dumps | **Done.** `Quest_Manager_obj` is online-only plumbing. Player quest-cast state found, then ruled out by the tester. |
| C0.5 hover globals | **Done (read half).** All ten hold real bound methods - moot now that C0.2 is closed. |
| C0.6 deactivation model | **Done, negative — and it contradicts session 6's inference.** |
| C0.7 `petquest stat` | **Done, positive.** Also confirms the *original* plan's Phase 0 items 5 and 7. |

**All seven items are now measured. C0 is closed on the negative branch** - no
mechanism was confirmed, and nothing has been implemented in
`PetQuestCollectorTick()`. Per the plan's own §2 exit criterion, this routes to
**Phase C1 (Ghidra)**, already underway - see below. The save was backed up
before the first mutating command (plan §4 rule 1) to
`%LOCALAPPDATA%\Hero_Siege_backup_planC_20260911-005030`.

**Phase C1 started the same day** (see "Phase C1 tooling" and "Phase C1
anchors" below): `citrace symdump` produced a complete runtime symbol table
(6,254/6,254 resolved) and it was applied to the Ghidra project — **6,167
functions named, 5,334 newly created, 0 failed.** `citrace stackwalk` then
captured a clean 15-frame native chain from a real F press, decompiled
offline, and **independently confirmed** the generic script/builtin dispatcher
(`0x14b488f40`) the 2026-09-10 session found by reading disassembly by hand -
plus its static table base address, which that session did not have.

One deviation from the plan's safety order, taken deliberately and with the
tester informed: §4 rule 2 asks for a non-progress-gated item first
(`Quest_Toy_Bear_obj`), but testing ran on Act 1 bricks because the tester was
already positioned there and the save backup made it recoverable. No quest
damage was observed — every mutating command was a no-op.

Phase C1 (Ghidra, incl. the `keyboard_check_pressed` stack walk) is **not**
built: the plan gates it on C0 coming back negative, and its own §7 lists the
stack-walk capture as "only in Phase C1". Phase C2 (the actual collect in
`PetQuestCollectorTick`) is likewise untouched — the tick still counts and
never mutates, and `tests/test_pet_quest_collector_contract.py` fails if that
changes before a mechanism is confirmed.

---

## Why this phase can exist at all

B1 needed a script name to call, B2 needed a name to hook, and session 7
established that the runtime's named-routine table indexes only callable
scripts and the closures GameMaker splits out of them — 22 raw object-event
names all returned "not found" (status 14) at hook-install time, before any
live test was needed. Plan C's two doors don't use names:

1. **the callable is already in a variable.** Session 6's item dump found
   `m_QuestActivate`, `m_QuestActive`, `m_QuestDestructible`,
   `m_QuestInteract`, `m_Questpickup`, `m_QuestUseKey` on the quest item
   instance, none matching any script name in `hs-game-sdk`. A method *value*
   can be invoked without being found by name, because we already hold it.
2. **events can be performed by number.** `event_perform(type, number)` runs
   an object's own event code through the engine with no name lookup in the
   path.

**And one already-built measurement has never been run.** Session 7 added
`CiTryResolveMethod` (`method_get_index()` + `script_get_name()` — resolves
what script a bound method wraps *without calling it*) specifically to
identify those six methods. Checked against the full `out.txt` history: zero
`->method:` lines have ever been logged. `CiExpandContainer` calls the
resolver on every variable it expands, and `citrace snap1` already expands the
item's ~59 variables — it just prints a count instead of the dump. So C0.1
needed a command that *prints*, not new resolution logic, and that is exactly
what was added.

### A precedent the plan didn't have

`event_perform` through YYTK is **already proven to work in this plugin**. The
Density research command `spawnforce` (`ModuleMain.cpp`) has been calling
`CallBuiltinEx(res, "event_perform", spawnerInstance, ..., { ev_alarm, n })`
against live `Enemy_Creator_obj` instances for several sessions, and it
demonstrably makes those spawners run their own alarm event code and spawn
enemies. C0.3 uses the identical call shape, so the mechanism is not in
question — only whether the *quest item's* events do anything useful.

---

## Tooling built this session

All of it lives in the existing `citrace` research command, inside the same
`#ifndef FORGEPACT_RELEASE` block as every other citrace tool — dev build only
(`plugin_build\build.bat dev`), and `citrace` is not in `kPlayerCommands`, so
a shipped build rejects the command outright even from a hand-written
`cmd.txt`. Both configurations were rebuilt clean and the full Python contract
suite passes (109 tests, up from 100 — the nine new ones are described under
"Safety, and what enforces it").

Drive it with `ForgePact\tools\ipc.ps1` (added 2026-09-11), which writes
`<game>\bin\bp_ipc\cmd.txt`, waits for the game to consume it, and prints only
the lines that command appended to `out.txt` — instead of the hand-driven
"edit cmd.txt, then scroll a multi-megabyte out.txt" loop every prior session
used:

```powershell
.\ForgePact\tools\ipc.ps1 citrace methods
.\ForgePact\tools\ipc.ps1 -Lines "petquest 1","petquest stat"
.\ForgePact\tools\ipc.ps1 -Tail 40
```

A timeout means the game is not running or the plugin did not load. Writing
`cmd.txt` by hand still works exactly as before. `citrace help` prints the
whole subcommand list in-game.

### Read-only (safe to run any time, no confirmation)

| Command | Checklist item | What it does |
| --- | --- | --- |
| `citrace item` | C0.1, C0.4 | Nearest quest item: every instance variable, container-expanded and method-resolved, sorted so two dumps are diffable by eye. |
| `citrace methods` | C0.1, C0.2 step 1 | Just the six `m_Quest*` variables and what each resolves to, **plus** which invocation builtins this runtime actually exposes (`script_execute`, `method_call`, `method_get_index`, `method_get_self`, `script_get_name`). |
| `citrace dumpobj <ObjectName> [nth]` | C0.4 | Full method-resolved dump of any object instance. `Quest_Manager_obj` has never been dumped at all; `Controller_obj` is known to hold live game state. |
| `citrace player` | C0.4 | The same for the local player. |
| `citrace globals` | C0.5 | The hover/target globals read as *values*: `GetMouseTarget`, `PlayerGetMouseTarget`, `GetQuestHoverDescription`, `GetMouseDisabledTarget`, `CanISeeTarget`, `hoverTooltip`, `PlayerMouseAction`, `GetPlayerMouseDisabled`, `KeyboardMouseInput`, `RefreshMouseMove`. Session 4 hooked these (0 calls) but never read them. |
| `citrace sweep [extra ...]` | C0.5 | The plan's broadened keyword sweep (`use`, `key`, `press`, `activate`, `collect`, `pickup`, `interactable`, `focus`, `selected`, `target`, `hover`, `quest`, `interact`) in one pass, rendered through `CiExpandContainer` — so an array, ds_map or bound method shows its contents/target instead of the opaque handle the pre-existing `dump` prints. That opacity is exactly how the `m_Quest*` methods stayed invisible for six sessions. |

### Mutating (require a literal `confirm` token)

| Command | Checklist item | What it does |
| --- | --- | --- |
| `citrace invoke item <varName> confirm [path]` | C0.2 | Invokes a bound method held in one of the item's instance variables, item as `self`, player as `other` (the original plan §1's credit decision). |
| `citrace invoke global <name> confirm [path]` | C0.2 + C0.5 | The same for a method value held in a global — the direct follow-up if `citrace globals` shows one. |
| `citrace event <type> <number> confirm` | C0.3 | `event_perform` with the nearest quest item as `self`. |
| `citrace eventobj <ObjectName> <type> <number> confirm` | C0.3 | `event_perform_object` — another object's event in the item's context. |
| `citrace activate [ObjectName] confirm` | C0.6 | `instance_activate_object`, defaulting to the `Quest_Object_Parent_obj` family, reporting the family count before and after. |

C0.7 needed no new code: `petquest 1` then `petquest stat` already counts
family enumeration and camera-bounds stability, and has simply never been read
in a session with quest items on screen.

### How the invoke path decides what to call

The plan's §2 C0.2 lists four candidate ways a runtime may accept a method
value and says explicitly to settle which one applies rather than assume.
`citrace methods` answers that read-only, by probing the name table. The
invoke command then tries them in the plan's stated order and **stops at the
first that reports success** — they are fallbacks for each other, never a
sequence, because a method that ran twice would break the plan's "one item,
one call, one observation" rule as surely as a loop would:

1. `script_execute(methodValue)` — keeps the method's own bound self.
2. `method_call(methodValue, [])`.
3. `CallGameScriptEx("gml_Script_<resolved name>", item, player, {})`, using
   the name `CiTryResolveMethod` recovered. Preferred over a bare
   `method_get_index()` → `script_execute(index)` because that loses the bound
   self, which is the whole point of a bound method here.

Whichever path ran is printed with the result, so the answer to "how does this
runtime accept a method value" comes out of the first invoke either way.

**The optional `[path]` argument** (`script_execute`, `method_call`, `script`;
default is the fallback order above) exists because *"the call succeeded"* and
*"the call did something"* are different questions here. `CallBuiltinEx`
reports success as long as the builtin exists and ran — so if this runtime's
`script_execute` quietly no-ops on a method value instead of rejecting it,
path 1 claims the invoke, paths 2-3 never run, and "the method does nothing"
becomes indistinguishable from "the invocation path was wrong". Forcing one
path separates those two readings in one extra live round rather than a
rebuild. It is still one call per command, so it does not weaken §4 rule 4.
**If an invoke reports success but nothing visible changes, re-run it with
each path forced before concluding the method is inert.**

### Event-number labels

`citrace event`/`eventobj` take numbers, as the plan asks and as the engine
requires. The log line annotates them with the standard GameMaker event
constant names (`ev_mouse/ev_mouse_enter` for `6 10`, etc.) purely so the log
reads clearly and the tester can follow the plan's "read-shaped events before
press-shaped ones" ordering without a lookup table. **The labels are standard
GM naming, not measured for this build** — a surprising label is a wrong
label, never evidence about what the event does.

---

## Safety, and what enforces it

Phase C0.2 onward is the first code in this entire investigation that
*invokes* rather than *observes*, so it is the first that can remove a quest
item without crediting its objective — which the original plan's §11 calls
worse than no mod at all. The plan's §4 rules are enforced as follows:

| §4 rule | How |
| --- | --- |
| 1. Back up the save first | Cannot be enforced from inside the plugin. A banner listing all five preconditions prints to `out.txt` once per session, immediately before the first mutating command runs. |
| 2. Non-progress-gated item first | Same banner names `Quest_Toy_Bear_obj` (Act 8, Zone 8-4) explicitly. |
| 3. Verify progress, not disappearance | Every mutating command prints, **before and after**: the item's marker flags (`canPickup`, `isActive`, `active`, `questIndex`, `questObjectType`, `questObjectiveNumber`, `distanceForPickup`, `activateQuestObjectWithMouse`, `itemActive`, `x`, `y`, `visible`), the `Quest_Object_Parent_obj` family instance count, and a best-effort `gml_Script_GetQuestProgress`/`GetQuestMaxProgress` read keyed on the item's own `questIndex`. A family-count drop prints an explicit warning that a vanish is **not** by itself a successful collect. |
| 4. One item, one call, one observation | No command loops. The invoke paths guard each other with a `called` flag; nothing iterates the six `m_Quest*` names except the read-only reporter. |
| 5. Fresh character where practical | Named in the banner. |

The `confirm` token is a literal word rather than a numeric flag deliberately:
a mis-parsed or half-pasted line in `cmd.txt` then fails closed instead of
firing. The read-only commands deliberately do **not** require it — gating
them too would train the tester to type `confirm` reflexively, which is
exactly how such a gate stops working.

Nine contract tests in `tests/test_pet_quest_collector_contract.py` pin all of
this statically: every mutating subcommand routes through the confirm gate,
the gate fails closed and prints the banner, the read-only ones don't, every
helper sits inside the release guard, `citrace` is absent from
`kPlayerCommands`, both mutating helpers report flags/family-count/progress on
both sides, and none of it has leaked into `PetQuestCollectorTick`.

### The one unverified read

`GetQuestProgress`'s signature has never been measured. `hs-game-sdk` indexes
`gml_Script_GetQuestProgress` (and `gml_Script_GetQuestMaxProgress`), so the
names are real, but nothing measured what they take. The report passes the
item's own `questIndex` and prints whatever comes back, labelled
`(unverified sig)`. Both are read-shaped (`Get…`) and the call is wrapped, so
a wrong arity degrades to `undefined` rather than mutating anything — but
**treat the number as unconfirmed until the tester cross-checks it against the
quest counter in the UI**, exactly as they did during the B1/B2 sessions.

---

## Suggested running order for the live round

Read-only first, so the mutating commands are aimed rather than guessed:

1. `petquest 1`, walk to a zone with quest items on screen, then
   `petquest stat` — C0.7 for free, and confirms family enumeration and camera
   bounds while the rest runs.
2. `citrace methods` — the highest-value single measurement in the plan, and
   the one already-built resolver that has never been run.
3. `citrace item` — the full item dump, for anything the six methods miss.
4. `citrace dumpobj Quest_Manager_obj`, `citrace dumpobj Controller_obj`,
   `citrace player`.
5. `citrace globals`, then `citrace sweep`.
6. **Back up the save**, switch to `Quest_Toy_Bear_obj` (Act 8, Zone 8-4) or a
   fresh character, then the mutating commands — one at a time, reading the
   before/after block after each:
   - `citrace invoke item m_Questpickup confirm` (then `m_QuestInteract`,
     then `m_QuestUseKey`, one live observation apiece),
   - `citrace event 6 10 confirm` / `citrace event 6 11 confirm`
     (hover enter/leave — read-shaped, per the plan's §6 risk row) before
     `citrace event 6 4 confirm` (left press) and `citrace event 3 0 confirm`
     (step),
   - `citrace activate confirm`, ideally immediately after a *real* collect,
     which is what makes C0.6 decisive.

---

## Why the game's scripts cannot be called cold (MEASURED 2026-09-11)

> **Correction.** An earlier version of this section claimed the fault line was
> `CallGameScriptEx`/`CallBuiltinEx` versus their non-`Ex` forms, inferred from
> two data points. **That was wrong, and the second half of the same session
> disproved it.** It is corrected here rather than deleted because a future
> session could otherwise rediscover the wrong rule from the raw `out.txt`.

The control test that settles it, run back to back in one command file:

```
call GetQuestProgress   ->  real:-1.000000      (works)
call GetMouseTarget     ->  EXCEPTION           (faults)
```

Both go through the **pre-existing** `call` command — `DoCall` →
`CallGameScript("gml_Script_<name>", {})` — which predates all of this work and
is identical in shape for both. One works and one faults, so the variable is
**the script being called, not the call mechanism**, and the plugin's own
plumbing is exonerated: `citrace invoke`'s paths fault in exactly the same
places `call` does.

What actually happens: these YYC-compiled scripts dereference their arguments
and `self` without validating them. Called cold from the plugin — global
context, no arguments, no captured scope — most of them fault inside the
game's own code. `GetQuestProgress` is simply defensive enough to return `-1`
instead. The faults are access violations caught by the plugin's `catch (...)`
guards; the game survived every one, and `call GetQuestProgress` still worked
afterwards, so the process tolerates them — but they are real AVs inside the
runtime and should not be triggered casually.

So the working rule is narrower and duller than the discarded one:

- **Builtins are fine** through `CallBuiltin` *and* `CallBuiltinEx` — including
  with method-value arguments (`method_get_index`) and with instance self/other
  (`event_perform`). Nothing here is Ex-specific.
- **Game scripts called cold are not**, by any route, unless the script happens
  to tolerate a global `self` and missing arguments.

The rewritten `citrace invoke` reports which call faulted and `what()` where
the runtime provides one, and offers all seven call shapes as separately
selectable paths
(`auto|builtin|builtinex|index|indexex|methodcall|script|scriptex`). The first
version printed a bare `EXCEPTION` for every failure, which cost a live round.

> Every `gml_Script_GetQuestProgress=EXCEPTION` / `threw (non-std)` line in
> this session's `out.txt` is that script faulting on a 1-argument cold call,
> **not** a reading about quest state. Progress verification stayed on the
> tester's eyes for the whole session.

---

## Phase C0 checklist status

### C0.1 — resolve the six `m_Quest*` methods

**MEASURED 2026-09-11, live session — succeeded, and decisive.** On
`Quest_Act_01_Brick_obj#3881` (the "so many bricks" Act 1 quest), all six
resolved, plus a seventh method the plan did not know about:

| Instance variable | Wraps |
| --- | --- |
| `m_QuestUseKey` | `anon@1400@gml_Object_Quest_Object_Parent_obj_Create_0` (#105270) |
| `m_QuestActivate` | `anon@1584@…` (#105271) |
| `m_QuestDestructible` | `anon@2113@…` (#105272) |
| `m_Questpickup` | `anon@2786@…` (#105273) |
| `m_QuestInteract` | `anon@3858@…` (#105274) |
| `m_QuestActive` | `anon@4737@…` (#105275) |
| **`m_LootGroundDeActiveStep`** | `anon@5164@…` (#105276) |

This is the plan's own first exit branch: the six map **1:1 onto the seven
Create-event closures session 2 hooked and measured at 0 calls**. So they are
real, callable, and *not* on the path a genuine collect takes.

`m_LootGroundDeActiveStep` (`anon@5164`) closes a loose end: session 2 hooked
seven closures but could only account for six methods. Its name matches
session 6's deactivation inference — though see C0.6, which contradicts that
inference by measurement.

All five invocation builtins are present: `script_execute`, `method_call`,
`method_get_index`, `method_get_self`, `script_get_name`. The plan's "open
question to settle in step 1, not assume" is settled — the runtime exposes
every candidate.

### C0.2 — invoke a resolved method directly

**MEASURED 2026-09-11 — NEGATIVE, and fully closed.** This is the plan's "main
event", and it does not work. All nine call shapes the tooling offers were
tested (direct/indexed/method-call/script forms, with and without arguments,
plus a real `with`-context after three correction rounds - see "Why the game's
scripts cannot be called cold" above for the full evidence); every one either
faults or cannot even reach the target instance. The `InvokeWithObject` thread
ended in a *different* kind of negative (a tooling enumeration gap) than the
eight direct-call faults - both are detailed above.

First attempt (three paths) threw, which looked like a plugin bug. The command
was rewritten with seven separately selectable call shapes and real fault
reporting, redeployed, and retried against a live brick. Result: **all seven
paths fault with access violations**, for both a method wrapping an anon
closure (`m_QuestActive` → `anon@4737`) and one wrapping an ordinary named
script (`global.GetMouseTarget` → `GetMouseTarget#102419`):

```
CallBuiltin script_execute(method)      -> access violation
CallBuiltinEx script_execute(method)    -> access violation
CallBuiltin script_execute(index)       -> access violation
CallBuiltinEx script_execute(index)     -> access violation
CallBuiltin method_call(method, [])     -> access violation
CallGameScript gml_Script_<name>        -> access violation
CallGameScriptEx gml_Script_<name>      -> access violation
```

The control test in the section above proves this is **not** the plugin's
plumbing: the pre-existing `call` command faults on `GetMouseTarget` in exactly
the same way, while `call GetQuestProgress` works. The scripts fault inside the
game's own code because a cold call supplies neither the arguments nor the
`self`/captured scope they dereference.

**So Plan C's central premise — "a method value can be invoked directly
because we already hold it" — is disproven.** Holding the callable turns out
not to be sufficient; the call context is what is missing, and that is exactly
what a real call site provides and a plugin cannot synthesize this way.

**Two things remained untried, and both were built, tested, and closed** across
several live rounds on 2026-09-11:

1. **Arguments.** Every call in the first round passed *none* — the single
   most likely reason a script dereferencing `arg[0]` would fault. Tested
   directly (`citrace invoke item m_Questpickup confirm builtin player`):
   **still an access violation.** Arguments alone do not change the outcome.
2. **`InvokeWithObject`** — YYTK's real `with`-style instance context, rather
   than passing a self *pointer* into a call. This took three rounds to test
   correctly, and each round's failure was itself informative:

   | Round | What was passed | Result |
   | --- | --- | --- |
   | 1 | the item's own `id`, raw (`VALUE_REF` on this runner) | `AURIE_NOT_IMPLEMENTED` — never entered the callback |
   | 2 | `id` coerced to a plain real | `AURIE_OBJECT_NOT_FOUND` — revealed that `InvokeWithObject`'s parameter is an object **type**, not an instance id (matches its own doc, and both pre-existing call sites in this file) |
   | 3 | the item's `object_index`, raw (also `VALUE_REF` on this runner) | `AURIE_NOT_IMPLEMENTED` again — the same coercion gap, just on a second value |
   | 4 | `object_index` coerced to a plain real (`real:3881`, matching the item's own `#3881` tag exactly) | **`AURIE_OBJECT_NOT_FOUND` — zero instances found, despite this exact instance being resolved via `instance_number`/`instance_find` moments earlier in the same command, with `familyCount=1` confirming it was on screen** |

   Round 4 is the decisive one: the argument was provably correct, and
   `InvokeWithObject` still could not locate an instance every other command
   in this file sees without difficulty. That means it walks a **narrower or
   different instance enumeration** than `instance_number`/`instance_find` -
   a real, specific limitation of the tool, not a coercion mistake to fix
   further. No further hypothesis was chased past this point.

`auto` **stops at the first access violation** rather than walking all nine
shapes, since every direct-call shape is measured to fault and nine AVs per
command buys nothing; an explicit `all` sweeps past them when that is actually
wanted. The AVs are caught and survivable (the game kept running and ordinary
calls still worked afterwards), but they are real faults inside the runtime.

**Both untried approaches are now closed, and neither changes the answer.**
C0.2 is negative.

### C0.3 — `event_perform` on the item's own events

**MEASURED 2026-09-11 — the mechanism works, every event tried is a no-op.**
`event_perform` fires correctly (`performed -> bool:true`, matching
`spawnforce`'s precedent), with the quest item as `self` and the player as
`other`. Tried, each with before/after marker flags and family count:

`ev_mouse` 10 (enter), 11 (leave), 4 (left press), 0 (left button);
`ev_keypress` 70 (F); `ev_step` 0; `ev_other` 10-15 (user 0-5).

**Every one left `canPickup`, `isActive`, `active`, `questObjectiveNumber`,
`itemActive`, `visible`, position and family count completely unchanged.**

The last four were fired while the tester was *genuinely hovering* a brick at
138 px — so the game's own hover gate was satisfied and only the trigger was
synthetic. That is the strongest form of this test available and it still did
nothing.

> `event_perform`'s `bool:true` return is only "the builtin ran". It is **not**
> a signal that the object has that event. Most likely reading: these events
> do not exist on `Quest_Object_Parent_obj` at all, and `event_perform` on an
> absent event is a silent no-op — consistent with session 7, where all 22 raw
> object-event names failed to resolve.

Not yet tried: `ev_alarm` 0-3, remaining `ev_other` subtypes, `ev_keyboard`
(5)/`ev_keyrelease` (10) with key 70, and `event_perform_object`.

### C0.4 — dumps of the objects never inspected

**MEASURED 2026-09-11.** `Quest_Manager_obj` — dumped for the first time in
this investigation — is **thin and not the local collect path**: 6 variables,
all community-quest/online plumbing (`httpCommunityQuestGet`,
`m_CommunityQuestProgressGet`, `m_CommunityQuestProgressUpdate`,
`m_SavePendingQuestProgress`, `m_PlaceSecurityBeacon`, `questItemDropAmount`).

**The player carries a quest-cast state machine** never seen before:
`questCastInstance`, `questCastItem`, `questCastText`, `questCastTime`,
`totalQuestCastTime` (144), plus `interacting`, `mouseTarget`,
`itemPickupCooldown`, `lootPickedThisFrame`. The item carries
`questCastItem`/`questCastTime` (60) of its own.

This looked like the answer to the original plan's never-measured Phase 0
item 2 ("is there a hold/channel?"). **The tester settled it against that
reading: quest items collect instantly, with no cast bar.** So the cast
machinery exists but is not the path for `questObjectType = 23` pickups —
presumably it belongs to channeled quest objects. Recorded because it is real
and may matter for other quest object types; **not** a lead for this mod.

Full brick field set worth keeping: `questIndex = 1002`,
`questObjectType = 23`, `questValue = 1`, `questObjectiveNumber = 0`,
`canPickup = true`, `isActive = 0`, `active = 0`, `itemActive = true`,
`distanceForPickup = 0`, `activateQuestObjectWithMouse = false`,
`interactText = ""`, `noDestroy = false`, and a **`deleteTimer` counting
down from ~33** — these bricks despawn on their own, which is why the family
count churns between readings.

`activateQuestObjectWithMouse = false` is worth noting on its own: whatever
"activate with mouse" means, bricks do not use it.

### C0.5 — the hover-target globals, resolved and called

**MEASURED 2026-09-11 (read half).** All ten hold genuine bound method values
resolving to named scripts: `GetMouseTarget#102419`,
`PlayerGetMouseTarget#102758`, `GetQuestHoverDescription#103207`,
`GetMouseDisabledTarget#102420`, `CanISeeTarget#100465`,
`PlayerMouseAction#102772`, `GetPlayerMouseDisabled#101726`,
`KeyboardMouseInput#101900`, `RefreshMouseMove#103252`, and
`global.hoverTooltip = 1`. Calling any of them waits on C0.2's fix.

### C0.6 — confirm the deactivation model

**MEASURED 2026-09-11 — NEGATIVE, and decisive.** Run twice:

- Minutes after a collect: family count `4 -> 4`, no change (inconclusive —
  `deleteTimer` may have destroyed the item by then).
- **Seconds after a fresh collect**: family count had dropped `4 -> 3`, and
  `instance_activate_object(Quest_Object_Parent_obj)` returned it to `3` —
  **the collected item did not come back.**

So session 6's inference is **contradicted, not confirmed**. A collected item
leaves `instance_number`/`instance_find` in a way that is neither a caught
`instance_destroy` (measured 0 across many sessions) *nor* reversible
deactivation. Either the removal uses a path with no hookable builtin, or the
item is destroyed through something session 6's filter could not see.

### C0.7 — `petquest stat`'s own counters

**MEASURED 2026-09-11 — POSITIVE.** With bricks on screen and a pet out:
`family objs seen=11640 excluded=0 on-screen=11640 | pet-seen ticks=11640 |
no-camera ticks=0`.

That confirms two items of the **original** plan's Phase 0 that had sat
unverified since day one: **item 5** (parent enumeration through YYTK returns
descendants, not just exact matches) and **item 7** (camera-bounds reading is
stable — zero failed ticks). The scaffolding's targeting is sound; only the
collect was ever missing.

### Side result: a clean `snap1`/`snap2` round

Run over one real collect. Confirms and adds nothing new:

- `ProfileManager.inputState[30]` and `[60]` flipped `1 -> 0` — reproduces the
  B4 finding exactly and proves F genuinely registered.
- **No quest-related change in globals or on the player.** Only frame-timing
  churn (`deltaSpd`, `current_frames`, `updateMinimap`, `socketValidateTimer`)
  and one `playerEffect` timer slot.
- The `[questItem]` diff hit **session 6's known confound again**: nearest went
  `@305355 -> @305801`, two *different* bricks, so those deltas are
  meaningless. All marker fields are identical across instances anyway. This
  technique cannot work while more than one item of the same type is in range.

**Phase C0 exit criterion (plan §2): reached on the negative branch, fully.**
No mechanism was confirmed. C0.2 (including its `InvokeWithObject` sub-thread,
closed after three correction rounds), C0.3 and C0.6 are all negative with
live evidence; C0.1, C0.4, C0.5 and C0.7 succeeded as measurements but none
yields a collect. **This routes to Phase C1, Ghidra** - already underway, see
below.

---

## Phase C1 anchors, captured live 2026-09-11

RVAs are only obtainable from a running process, so they were pulled while the
session was still up. Ghidra address = `0x140000000 + rva` (the imported image
base, per the plan's §3.1); the live `modbase` was `0x7FF75A420000` this run
and will differ next launch — the RVAs will not.

| Target | Method it backs | RVA |
| --- | --- | --- |
| `anon@1400@gml_Object_Quest_Object_Parent_obj_Create_0` | `m_QuestUseKey` | `0x9865C50` |
| `anon@2786@…` | `m_Questpickup` | `0x98732C0` |
| `anon@3858@…` | `m_QuestInteract` | `0x9879B30` |
| `anon@5164@…` | `m_LootGroundDeActiveStep` | `0x987DDC0` |
| `GetMouseTarget` | — | `0x4CE93B0` |
| `PlayerMouseAction` | — | `0x4E3A010` |
| `GetQuestProgress` | — | `0x5304FA0` |

These are better anchors than the previous Ghidra attempt had: that pass picked
16 closures because they were *hookable*, not because they were *involved*.
These four closures are now known to be the exact bodies behind the item's own
`m_Quest*` methods, and `GetQuestProgress`'s RVA gives a cheap way to settle
its signature by reading its prologue instead of guessing it live (which is
what faulted repeatedly this session).

**Confirmed stable across launches.** These RVAs match, exactly, the address
list the 2026-09-10 session baked into `DecompileTargets.java` a day earlier —
so `decomp_output.txt` (705 KB, local) *already contains* the decompiled bodies
of all four. That session decompiled them without knowing which was which,
because it chose targets for being hookable. C0.1 now names them, which makes
existing output interpretable with no new decompilation:

| Method | Closure | Decompiled size |
| --- | --- | --- |
| `m_QuestActive` | `anon@4737` | 2,658 bytes (smallest — consistent with a predicate) |
| `m_QuestUseKey` | `anon@1400` | 3,695 bytes |
| `m_LootGroundDeActiveStep` | `anon@5164` | 5,062 bytes |
| `m_QuestInteract` | `anon@3858` | 14,360 bytes |
| `m_QuestActivate` | `anon@1584` | 25,504 bytes |
| `m_QuestDestructible` | `anon@2113` | 25,690 bytes |
| `m_Questpickup` | `anon@2786` | **26,723 bytes (largest)** |

---

## Phase C1 tooling built 2026-09-11 (and why it outlives this mod)

### `citrace symdump` — a symbol table for a stripped 280 MB binary

§3.2's anchor problem, solved from a different direction than the plan
proposed. The plan's step 3 wanted to reverse-engineer the dispatcher's
ID→name table (`base + id*0x18` at `0x14b488f40`) so call-site IDs in
decompiled output become readable — real work with an uncertain payoff.

But the runtime already exposes the same mapping through a builtin nobody
thought to iterate. `script_get_name(i)` answers it for any index, and
`GetNamedRoutinePointer` turns the name into a live address. Walking the index
space produces the whole table directly — no disassembly, no struct-layout
guessing:

```
citrace symdump [start=100000] [end=110000]   ->  bp_ipc\symbols.csv
```

It also covers ground `hs-game-sdk` cannot: the SDK's generated table is a
static extract of 6,254 names, while this reads whatever the running build
actually has — **including the `anon@N@gml_Object_..._Create_0` closures that
only exist at runtime**, which is precisely the category this investigation
spent three sessions chasing by hand.

Range is capped and pageable because it runs inside one `FrameCallback`; the
default range covers the observed script index space (`CanISeeTarget` #100465
at the low end, the quest closures at #105270-105276).

### `ForgePact/tools/ghidra/ImportSymbols.java` — apply it to Ghidra

Reads that CSV, disassembles and creates a function at each RVA if one is not
already there, then names it (`@` → `_`, since Ghidra rejects it). Run headless
with `-noanalysis`, so it never triggers a whole-binary analysis pass on a
280 MB image.

**Pre-flight tested 2026-09-11** against a 7-row CSV of the verified RVAs
above: *7 rows | named 6 (created 2 new functions) | skipped 1 | failed 0* —
the skip being the deliberate blank-address row. The pipeline works.

**Run for real, same session:** `citrace symdump` over the default range
(100000-110000) resolved **6,254 of 6,254** — every script in that range got a
live address, a 100% hit rate. Copied to
`C:\Users\Administrator\ghidra_projects\symbols.csv` (local, outside `bp_ipc`,
so it survives the game closing) and **applied to the Ghidra project**:
*6254 rows | named 6167 (created 5334 new functions) | skipped 87 | failed 0*.

**The stripped 280 MB binary is now a named one.** 5,334 functions that had no
Ghidra entry at all now exist and are named; 6,167 total carry a real name
instead of `FUN_14xxxxxxx`. This is permanent (saved in the Ghidra project)
and available to every future session and every other tool in this toolkit —
not just this mod.

Together these turn `FUN_14xxxxxxx` into real names across the whole binary,
permanently, for **every** future session and every other tool in this toolkit
— not just this mod. That is worth more than the single answer Phase C1 was
opened to get, which is why it was built first.

### `citrace stackwalk [n]` — who called this builtin

§3.2 step 1, and the plan is right that it is the highest-value item *for this
mod*: it converts the investigation's one confirmed live signal — F is read by
`keyboard_check_pressed` with `Self = Profile_Manager_obj` — into the actual
chain of native functions that read the interact key, which is exactly the set
`DecompileTargets.java` should be pointed at.

Armed explicitly and counted down, so it fires on the real keypress edge and
nothing else. It sits *ahead* of the `g_CiTraceOn`/log-budget gate in the hook,
so an armed request cannot be silently swallowed by tracing being toggled off
or the shared 300-line budget being spent; idle cost is one atomic load.
Requires `citrace 1` first — that is what installs the hook it fires from.
Frames are reported as `module+RVA` with `Hero_Siege.exe` rows marked, and with
`symbols.csv` imported those RVAs come back named.

**MEASURED 2026-09-11, live session — succeeded cleanly, first try.** Armed
with `citrace 1` + `citrace stackwalk 3` while hovering a brick, then a real F
press captured 18 frames. Frames `[00]`-`[01]` are the plugin's own hook
trampoline (`BloodPactPlugin.dll`, expected — that's this plugin calling the
original builtin). **Frames `[02]`-`[16]`, 15 in total, are a clean, unbroken
chain inside `Hero_Siege.exe`** — the actual native call path from wherever the
game decided to check F down to `keyboard_check_pressed` itself:

```
[02] 0xB489032   [07] 0xB6A5DDA   [11] 0xB56E6D0   [15] 0xB8C2816
[03] 0x6BA45A    [08] 0xB672CC2   [12] 0xB56E476   [16] 0x11180239  (nearest to the OS/engine boundary)
[04] 0x952E2FF   [09] 0xB672A7E   [13] 0xB56FBB4
[05] 0xB49AA62   [10] 0xB6C7537   [14] 0xB51DD84
[06] 0xB49A413
```

`[17]` was `?+0x0` — the unwind ran out of frame info at the top, which is
normal this deep. More than double the anchor count the previous Ghidra pass
had (which picked 16 targets by hookability, not involvement), and these are
anchored by construction: every one of them is provably on the path from a
real F press to the moment it registers.

**Decompiled the same session, offline (no game needed for this part).**
`symbols.csv` was imported into the Ghidra project first (see below), then all
15 frames decompiled via a new local script,
`ghidra_scripts\DecompileStackWalk.java` (not committed — local Ghidra
scripts, per `agents.md`). Two results stand out:

- **`[03]` resolved to a real, non-`FUN_` name: `PollKeyboardInputs`.** This
  came directly from the symbol import — the dispatcher table walk (`citrace
  symdump`) had already resolved it as a named GameMaker script, and Ghidra
  picked that name up automatically once imported. Confirms the hunch that
  its unusually low RVA (`0x6BA45A`, versus the `0x9-0xB` range everything
  else clusters in) meant "generic low-level helper", not quest-specific code.
- **`[02]` is `FUN_14b488f40` — the exact address of the generic
  script/builtin dispatcher already identified in the 2026-09-10 Ghidra
  session** by manually reading helper bodies (`ForgePact/docs/pet-quest-collector-research.md`'s
  "Native decompilation"). Independent confirmation from a completely
  different technique (a live stack capture vs. reading disassembly by hand)
  landing on the same function is a strong cross-check that this is
  understood correctly.

**And its decompile answers session 7's own "most promising next thread"
directly**, without needing to resolve the ID scheme by hand as that thread
proposed:

Described rather than quoted, per `agents.md` — these are the interoperability
facts, not the decompiler's text: the dispatcher indexes a table whose static
base pointer lives at `exe+0x1081B410`, with a **stride of `0x18` bytes per
entry** and the **callable function pointer at offset `+8`** within an entry.
It then calls through that pointer with the argument order
**`(result, self, other, argc)`**.

This matches the `{ptr, funcptr}` entry shape and calling convention the prior
session inferred from reading the helper cold, confirms it precisely, and adds
**the dispatcher table's static base pointer: `exe+0x1081B410`** (the value
`kCiDispatchTablePtrRvaDefault` carries).

**Resolved the same session, live.** Reading the table at that address (via
the pre-existing `readmem`, no rebuild needed) showed its entries need no ID
scheme decoded at all — see "`citrace dispatchdump` / `citrace
dispatchtrace`" below for the full result, including a builtin-call trace
that turned out to close off the entire remaining "observe it through a
dispatched call" category for this mod.

`[04]` (`FUN_14952e2ff`, 12,734 bytes — by far the largest frame) is the
function that calls into `PollKeyboardInputs` and immediately after it, the
dispatcher directly, twice, each preceded by a `_DAT_...` constant load — the
same call-then-dispatch shape repeated across a huge raw function body.
Consistent with session 7's finding that object-event code compiles inline
with no separate script-table entry: this is plausibly `Profile_Manager_obj`'s
own (or a caller's) compiled Step event, not a callable script. Not fully
read - the size and lack of names beyond the dispatcher make it a genuinely
open-ended reading task, which is exactly the point at which the prior Ghidra
pass stopped once already; left here as an anchor for a future continuation
rather than pushed through in this session.

---

## `citrace dispatchdump` / `citrace dispatchtrace` — the builtin call surface

Built 2026-09-11, after the stack-walk result above. Frame02 of that walk
decompiled to the generic dispatcher (`0x14b488f40`), whose body indexes a
table by ID at `base + id*0x18` and calls the function pointer at `+8`.
Rather than decode the ID scheme by hand (the plan's own §3.2 step 3), the
table was read live — and its layout turned out to need no decoding at all:

```
+0x00  const char*  name       e.g. "camera_create"
+0x08  void*        function
+0x10  int32        argument count
```

Verified against real GameMaker signatures on the first ten entries
(`camera_create`/0, `camera_create_view`/4, `camera_destroy`/1, …) — every
argc matched. **This closes session 7's "most promising next thread"
completely**: every dispatcher call site in every decompiled function now
resolves to a name, mechanically, with no scheme to reverse-engineer.

### `citrace dispatchdump` — read the whole table

`citrace dispatchdump [tablePtrRvaHex] [maxEntries]` walks the table until an
entry's name stops being printable ASCII, and writes `bp_ipc\builtins.csv`
(id, name, argc, RVA).

**MEASURED 2026-09-11, live session, two rounds.** Round 1 stopped at id 174
— the name-plausibility check required identifier characters only, and entry
174 is `@@array_get@@`: GameMaker's internal builtins legitimately use `@` in
their names (the same convention as `@@GetInstance@@`, which this plugin
already calls elsewhere). Widened to "printable ASCII, no spaces" and
re-run: **2867 builtins**, matching a count field sitting immediately after
the table pointer in the same memory region (`0x0B33` = 2867) — two
independent methods, exact agreement.

Cross-checked against every keyword this investigation has ever used
(`instance_*`, `*destroy*`, `*deactivate*`, `*remove*`, `*delete*`): **no
`quest`/`interact`/`pickup`/`loot`-shaped builtin exists anywhere in the
engine's ~2867-entry table.** Two `instance_*` builtins turned up that no
prior session had thought to hook — `instance_deactivate_region`,
`instance_deactivate_layer` — see `dispatchtrace` below for why hooking them
individually turned out to be unnecessary.

### `citrace dispatchtrace` — trace every builtin call, by name, in one hook

The real payoff. Sessions 4-7 hooked builtins one guessed name at a time — 34
hooked call sites, 13 rebuild cycles, all zero. With the whole table
readable, **one hook on the dispatcher itself sees every builtin call the
game makes**, resolved to a name, with no prior guessing.

`citrace dispatchtrace arm` installs the hook (lazily, dev build only) and
arms a capture on the next real F-press edge — the same edge the stack walk
uses, so both techniques share one trigger. Only the numeric builtin ID is
recorded on the hot path (one relaxed atomic load, one store) to keep the
cost on the game's single hottest function negligible; names are resolved
against the table afterward, off the hot path. `citrace dispatchtrace show`
writes the capture to `bp_ipc\dispatchseq.csv` (ordered) and
`dispatchsummary.csv` (counts).

**MEASURED 2026-09-11, live session — thorough, and negative.** Armed on a
real F-press over a quest item; captured **60,000 calls before hitting its
cap**, spanning **118+ real frames** (counted via the
`mouse_check_button_released` burst that recurs at each frame's start) —
several seconds of real play following the press, not a truncated instant.

The complete distinct-name list is 112 builtins, and every one of them is
mundane: collision/movement (`collision_circle`, `place_meeting`,
`distance_to_object`), rendering (`draw_*`, `sprite_*`, `surface_*`,
`layer_*`), UI/HUD bookkeeping (`ds_list_*`, `ds_map_*`, `string_*`), input
polling, and generic utility calls (`array_*`, `typeof`, `real`). **Not one
call resembling an interact, pickup, or instance-lifecycle operation
appears.** Specifically absent across all 118+ frames:
`event_perform`/`event_perform_async`, `script_execute`/`script_execute_ext`,
`method_call`, and every instance-lifecycle builtin —
`instance_destroy`/`instance_deactivate_object`/`instance_deactivate_all`/
`instance_deactivate_region`/`instance_deactivate_layer`/
`instance_activate_object`/`instance_change` — the exact set every prior
session already measured at zero with individually guessed hooks, now
confirmed against the **complete** builtin call surface instead of five names
picked by hand.

**This closes off the entire remaining category of "observe it through some
dispatched call."** Two invocation mechanisms exist in this runtime — GML
builtins (indexed through this exact dispatcher) and compiled-script-to-
compiled-script native calls (a direct x86 `CALL` to a compile-time-resolved
address, needing no ID dispatch at all, per session 7's finding that
object-event code compiles inline with no separate script-table entry). The
dispatcher hook is complete for the first category and structurally blind to
the second. Combined with the absence of any lifecycle builtin, the most
consistent reading is that the actual collect and removal are done through
**inlined field writes (`self->field = value`, not a call in any observable
sense) and direct native-to-native calls** — invisible to any hook this
toolkit can build at the GML or numeric-dispatch level, full stop.

### What is left after this result

Every technique that observes *calls* — named-script hooks (B1/B2), builtin
hooks filtered by guess (sessions 4-7), a whole-state snapshot diff (B4),
direct invocation (C0.2), `event_perform` (C0.3), and now the complete
builtin dispatch surface — has come back with nothing. The only category not
yet tried is reading the compiled code itself, byte by byte:

1. **Finish reading `[04]`** (`FUN_14952e2ff`, 12.7 KB, plausibly
   `Profile_Manager_obj`'s own Step body) for the actual field-write pattern
   — an inlined `self->canPickup = 0`-shaped write would show up directly in
   the decompile even though it triggers no hook.
2. **A parallel stack walk armed on instance removal**, if a signal can be
   found to hook it from — none has been identified; every candidate builtin
   is now confirmed uncalled.
3. **Accept this as the practical ceiling** for hook-and-observe technique
   and treat "read the compiled bodies by hand" as its own bounded,
   deliberately scoped follow-up rather than something to push through in
   the same session — the same discipline the prior Ghidra pass's stopping
   point, and this plan's own §3.3, already called for.

---

## Reading the compiled code: the collect mechanism, found (2026-09-11, later session)

**This is the branch the section above left open — item 1, "finish reading
`[04]`" — and it closes positively.** Reading `[04]` itself came back
negative (it is input polling, see below), but the technique built to read it
made the whole binary readable, and two further reads found the actual
mechanism: the exact call the game makes to collect a quest item, the gate in
front of it, and the script that credits the objective.

**Everything in this section is a *static reading*, not a measurement.** Per
the plan's §3.3 it is a hypothesis until a live hook agrees with it, and
nothing here has been invoked. `PetQuestCollectorTick()` is untouched.

### 1. `[04]` was decompiled from the wrong address

`DecompileStackWalk.java` created a Ghidra function at each captured **return
address**. A return address is mid-body, so for `[04]` Ghidra invented a
function starting at `0x952e2ff` and decompiled only the tail from there —
12,734 bytes of a function that is really 20,832, with the prologue and every
frame-pointer-relative local missing. That is why the earlier read described
it as "a huge raw function body" with no structure: a third of it was gone
and the rest had no frame.

The PE's own exception directory (`.pdata`, 315,994 `RUNTIME_FUNCTION`
entries) gives exact bounds for every function in the image. `[04]`'s real
extent is **RVA `0x952c5d0` .. `0x9531730`**, and it takes `(self, other)` —
the signature every compiled GML event body in this build has. Decompiled at
that entry it is well-formed.

**Anyone continuing this work should resolve a captured return address
through `.pdata` before decompiling it.** The other 14 stack-walk frames were
created the same wrong way; their real bounds are now known too (they were
all 57-2,211 bytes, so the distortion was smaller, but it was there).

### 2. The name-slot table — every `_DAT_` in this binary resolves to a name

The single most useful thing found this session, and it is not specific to
this mod.

Compiled GML in this build never names anything inline. Every variable
access, every builtin call and every script reference goes through a 4-byte
global that the decompiler shows as `_DAT_1506xxxxx` / `_DAT_1507xxxxx`.
Those globals are what the previous Ghidra pass gave up on ("the call-site
IDs inside them, `_DAT_150740ee0` and friends, were opaque without a name
table").

They are not opaque. `.data` holds a flat array of 16-byte slots:

| offset | contents |
| --- | --- |
| `+0x00` | `const char*` — the name, as a plain ASCII C string |
| `+0x08` | `int32` — the id, cached at runtime (`-1` = not yet resolved) |

and the address the code references is the **`+0x08` half**. So the name for
any `_DAT_<addr>` is the string pointed to at `<addr> - 8`. Walking `.data`
for slots shaped this way yields **38,234 named slots**, covering instance
variable names, builtin names and script names alike.

Checked against things already known independently: `_DAT_150740ee0`, the
example the prior session called opaque, is `interactText`; `_DAT_1506fdc98`
is `inputState`, the array B4 measured flipping on F. Every builtin name
recovered this way matched `builtins.csv` from `citrace dispatchdump`, which
came from a completely different source (a live table read) — two independent
methods, exact agreement.

**Consequence: the 705 KB of decompiled output the prior sessions produced,
plus anything decompiled from now on, can be annotated with real names
mechanically, offline, with no game running.** That is what made the rest of
this section possible.

### 3. A whole-image cross-reference scan, from the file alone

The second reusable piece. To ask "which functions touch `canPickup`?" needs
cross-references, and a `-noanalysis` Ghidra project only has them where it
happens to have disassembled.

x86-64 encodes these as RIP-relative displacements, so the naive search is
212 MB of `.text` with one check per byte offset — far too slow in Python.
Inside any 64 KB window, though, the top 16 bits of the displacement a given
target requires take at most two values, so `bytes.find` can do the scanning
at C speed and only the handful of candidates it returns need checking.
**A full-image cross-reference scan for one target runs in about half a
second.** The same formula finds direct `call rel32` sites, so it answers
"who calls this?" and "what does this call?" as well.

Combined with `.pdata` bounds and `symbols.csv`, each hit is reported as
`address -> enclosing function -> nearest script symbol`.

### 4. `[04]` itself: negative, and now definitively so

With names resolved, `[04]` (`Profile_Manager_obj`, RVA `0x952c5d0`) reads
unambiguously. Every name it touches is input plumbing:

`inputState`, `keyBinds`, `inputToKB`, `inputToGP`, `newToKB`, `inputSimMap`,
`pressedArray`, `mousePressedArray`, `actionArray`, `multiBindArray`,
`ignoreMultiBind`, `menuNav`, `prevMenuNav`, `m_CheckInputArrayPressed`,
`m_ClearInputArrayPressed`, `m_CheckMultiBindInput`, `gamePadInfo`,
`padIndex`, `padStatus`, `gamePadAxis*`, `gamePadVibrationTimer`,
`useRightStick`, `window_has_focus`, `device_mouse_raw_x/y`.

**Not one quest-, pickup-, interact- or instance-lifecycle-shaped name
anywhere in the function.** It calls `PollKeyboardInputs` and
`PollGamepadInputs` and writes the results into the input arrays; that is all
it does. The stack walk anchored on `keyboard_check_pressed` reached the
*producer* of the key state, never a consumer — which in hindsight is what a
keyboard hook was always going to find.

That closes the plan's §3.2 step 1 thread. What actually mattered was the
name-slot table it forced us to build.

### 5. The field-write shape, since the earlier note asked for it

A GML `self.field = value` compiles to a short sequence, not one call:

1. `self->vtable[+0x10](self, <name-slot addr>)` — returns a pointer to the
   member's `RValue` (the write accessor). The read accessor is
   `vtable[+0x08]`, same argument.
2. a write-barrier open call on `self` (`0x14b486200`),
3. `0x140189e60(dest, src)` — `RValue` copy-assign, the actual store,
4. a write-barrier close call (`0x14b486150`).

So "a call through a runtime helper" was right, but the helper that carries
the *name* is the vtable accessor, and the one that carries the *value* is
the copy-assign. Both were already in the helper map under other
descriptions. `RValue` itself is 16 bytes: value at `+0x00`, type/flags at
`+0x0C` (`0xffffff` undefined, `0` real, `5` unset, `6` instance ref, `0xd`
bool).

### 6. The mechanism

> **CORRECTED by the live round — read this before the rest of §6.**
> `PlayerMouseAction` measured **0 calls** during a real brick collect with a
> detour on its own address, so it is **not** the caller. The branch described
> below is real code and its `canPickup` gate, `lootType` mapping and `with`
> are all genuine, but the path a brick collect actually takes runs inside
> `Loot_Manager_obj` instead, and passes a different `other` and a different
> argument. See the final section of this file. Kept unedited because the
> callee half (`m_Questpickup`, the one argument, the `lootType 0` case) was
> right and the mistake — treating a cross-reference as proof of the caller —
> is worth not repeating.

Chasing `canPickup` with the cross-reference scan produced 37 references
across 34 functions — every quest-object flavour in the game, plus
`Loot_Manager_obj`, the outline managers, and one entry that is the whole
answer: **`PlayerMouseAction`**, RVA `0x4E3A010`, 17,410 bytes.

That RVA is already in this document's own anchor table, captured live via
`naddr` a session earlier. It is script **#102772**, one of the ten hover
globals C0.5 read and never called. The static symbol and the live address
agree exactly, which is a useful cross-check that the file being read is the
process being played.

`PlayerMouseAction` runs with the **player** as `self`. Its quest branch,
described rather than transcribed (`agents.md`: interoperability facts, not
the script body):

The branch is taken when the player's `mouseTarget` is a descendant of
`Quest_Object_Parent_obj` (index **3979**) — the ancestry test is
`object_is_ancestor`, i.e. exactly the family membership `hs-game-sdk`'s
hierarchy already models. Inside it, execution switches context to the
`mouseTarget` instance, sets that item's `activateQuestObjectWithMouse` to
`true`, and — **only if the item's `canPickup` is true** — dispatches on the
item's `lootType` to one of the item's own bound methods, then sets
`activateQuestObjectWithMouse` back to `false`.

The `lootType` → method mapping, which is the part this mod needs:

| `lootType` | method invoked on the item |
| --- | --- |
| 0 | `m_Questpickup` |
| 1 | `m_QuestInteract` |
| 2 | `m_QuestActive` |
| 4 | `m_QuestActivate` |
| 5 | `m_QuestDestructible` |

(`lootType 3` has no entry.) Each is called with exactly one argument.

Notes on the shape, all read directly:

- The `with` is a real GML `with` — the runtime's with-begin/next/end helpers
  (`0x14b489710` / `0x14b489e50` / `0x14b4893d0`) take the *addresses* of the
  caller's `self` and `other` locals and swap them for the duration, so
  inside the block `self` is the item and `other` is the player. That is the
  same credit context the original plan §1 chose, arrived at independently by
  the game.
- The `lootType` switch is a lazily-built 5-entry table of
  `{RValue key, int case}` pairs, keys `4, 5, 0, 1, 2` mapping to cases
  `0..4`. The mapping above is that table read out.
- **The method takes exactly one argument: `other.id`** — the player's
  instance id, as a plain real, built by making an instance `RValue` from
  `other` and reading its `id` member.
- The call goes through the runtime's call-a-method-value helper at
  `0x14b489070`, whose signature is
  `(self, other, RValue* result, int argc, RValue* methodValue, RValue** args)`
  — the same shape as the builtin dispatcher, with a method value in place of
  a builtin id.
- `activateQuestObjectWithMouse` is set true only for the duration of the
  call and reset to false straight after. Bricks read `false` in C0.4 because
  every read happened outside that window — the flag is not "this item is
  mouse-activatable", it is "a mouse activation is in flight right now".

**This explains every C0.2 access violation.** The method wants one argument
and a `self` that is the item; C0.2's nine call shapes supplied no argument
(or the wrong kind) and could not set `self`. The bodies dereference
`argument0` without validating it, exactly as the "cannot be called cold"
section predicted — we simply never had the signature.

### 7. What `m_Questpickup` does, and why it is the right target

`m_Questpickup` is `anon@2786@...`, RVA `0x98732C0`, 26,723 bytes — the
largest of the seven closures, as this document already noted. The names it
touches and the scripts it calls were read without decompiling it:

- fields: `questIndex`, `questObjectType`, `questObjectiveNumber`,
  `questValue`, `lootName`, `lootSound`, `lootVolume`, `id`, `object_index`
- builtins: `instance_nearest`, `instance_exists`, `collision_circle_list`,
  `point_distance`, `ds_list_*`, `audio_play_sound_at`, `floor`
- **scripts, exactly once each: `GetQuestProgress`, `GetQuestMaxProgress`,
  `update_quest`, `InventoryLogAddItem`, `choose_array`**

`update_quest` (#103199, RVA `0x52FCF30`) in turn touches `questlogId`,
`questlogProgress`, `questlogDifficulty`, `onlineQuestUpdatePool` and calls
`GetQuestObjectives`, `GetQuestProgress`, `GetQuestMaxProgress`,
`LoadQuestlog` and **`QuestSaveUpdate`**.

So `m_Questpickup` is not "make the item disappear" — it is the objective
credit and the save. That is precisely the distinction the original plan's
§11 pass/fail rests on, and it means this target satisfies the plan's
"verify progress, not disappearance" requirement by construction.

Notably it calls **no** instance-lifecycle builtin. Whatever removes the item
is downstream of the credit, which is consistent with `dispatchtrace` seeing
no lifecycle builtin during a real collect.

### 8. A second, simpler path that does not apply to bricks

The quest item's own Step event (an unnamed body at RVA `0x9883210`, inside
the `Quest_Object_Parent_obj` region — no script-table entry, which is why
seven sessions could not hook it by name) does the same thing by proximity.
Described rather than transcribed: gated on the item's own `canPickup`, it
dispatches on `lootType` to `CheckPlayerInteraction`, handing it the matching
bound method and the item's `distanceForPickup` as the range. The four
entries it has:

| `lootType` | method handed to `CheckPlayerInteraction` |
| --- | --- |
| 1 | `m_QuestInteract` |
| 2 | `m_QuestActive` |
| 4 | `m_QuestActivate` |
| 5 | `m_QuestDestructible` |

`CheckPlayerInteraction` (#100544, RVA `0x436380`, 2,328 bytes) is a small
generic helper used by NPCs, piles, shrines, stones and the chaos tower as
well; it calls `GetLocalPlayerObj` and uses `instance_exists`,
`distance_to_object` and `place_meeting`. It touches **no input or key
name** — it is a pure range check that fires the method when the player is
close enough.

**`lootType 0` — `m_Questpickup` — is absent from this table.** Pickup-type
quest items have no proximity path at all; the mouse-target path in §6 is the
only one. And bricks carry `distanceForPickup = 0` (C0.4), so even for the
other types the range path is inert on them.

This matters because "set `distanceForPickup` high and let the game collect
it" is the tempting shortcut, and for the Act 1 bricks this mod targets it
cannot work. It may still be worth knowing for other quest object types.

### 9. The gate in front of it

`PlayerMouseAction` has exactly two callers: a 249-byte stub immediately
after it, and **`PlayerMovement`** (#102773/#102774, RVA `0x4E414A0`) — a
single 199,478-byte function, the player's compiled movement/step body — at
RVA `0x4E67B64`.

Ghidra's decompiler **cannot render a function that size**: it times out at
600 s and returns nothing. Recorded so nobody spends another session on it.
The call site was read from disassembly instead (a range disassembler needs
no `Function` and takes seconds), annotated with the name-slot table from §2.

The immediate gate on the call, described rather than transcribed: the caller
first requires `instance_exists(mouseTarget)`; it then computes a distance to
that target, choosing `distance_to_object` when the target is a
`Quest_Object_Parent_obj` (3979) descendant and `point_distance` against the
target's `x`/`y` otherwise; and it calls `PlayerMouseAction` when that
distance is within a threshold **and** `mouseTarget` still exists on the
re-check.

Two things worth having from that:

- **`PlayerMouseAction` is called with `argc = 0`.** The call passes
  `(self, other, &result, 0, args)` — the ordinary compiled-script signature
  with no arguments. So it is the one script in this chain that *cannot*
  fault on a missing `argument0`, which is what the "scripts cannot be called
  cold" section above measured as the cause of every other fault. It needs a
  real `self` and nothing else.
- The same `object_is_ancestor(..., 3979)` test appears here and again inside
  `PlayerMouseAction` — the caller uses it to pick the distance function, the
  callee to pick the branch.

**Correction to an earlier draft of this section: the `inputState` read in
this window is not the interact gate.** It resolves to
`Input_Device_Manager_obj.playerProfileSlot[<slot>].inputState[18]`, and it
selects between a `Mouse_Move_obj` branch and a `disableMouseMove` branch —
both of which converge on the common continuation that reaches the quest
code. The flag tested immediately before the call is
`instance_exists(mouseTarget)`, not a key. **Where the interact key is
actually tested was not located**; it is further up the same 199 KB body
(five more `inputState` references sit between RVA `0x4E5A6B8` and
`0x4E63B5F`), or in whatever assigns `mouseTarget` in the first place.

That gap does not affect the mod: a plugin calling `PlayerMouseAction`
directly bypasses the gate entirely. It is left open honestly rather than
guessed at.

So the chain from a key press to a collect, with that one link unread, is:

```
Profile_Manager_obj event body (RVA 0x952C5D0)  ->  writes inputState[]
            ... unread link: where the interact key is tested / mouseTarget assigned ...
Player PlayerMovement        (RVA 0x4E414A0)    ->  mouseTarget exists, ancestry, range
PlayerMouseAction            (RVA 0x4E3A010)    ->  invokes the item's m_Questpickup, item as self, one arg
m_Questpickup                (RVA 0x98732C0)    ->  update_quest -> QuestSaveUpdate
```

`inputState` is the same array B4 measured flipping `1 -> 0` on F
(`[30]`/`[60]`), so B4's observation and the top of this chain are the same
event seen from the two ends. B4's conclusion still stands: writing that
array from outside did nothing, because the *producer* rewrites it every
frame. Nothing here suggests otherwise.

### 10. What this changes, and what it does not

It gives the investigation its first concrete call shape:

> with the quest item as `self` and the player as `other`, invoke the item's
> own `m_Questpickup` method value with **one** argument — the player's `id`
> as a plain real.

Three ways to reach that, in increasing order of how much they lean on the
plugin rather than the game:

1. **Set `mouseTarget` on the player to the item instance and call
   `PlayerMouseAction` with the player as `self`.** Uses the game's own
   branch verbatim, including the `canPickup`/`lootType` gate and the `with`.
   `PlayerMouseAction` is a named script (#102772), so the existing
   `CallGameScriptEx` path can reach it, and §9 measured its real call site
   passing **`argc = 0`** — so unlike every script C0.2 tried, it has no
   `argument0` to fault on. Everything it reads is a player field, so a real
   `self` should be all it needs. Cheapest to test, closest to what the game
   does, and the one to try first.
2. **Reproduce the `with` ourselves** and invoke the method value with the
   one argument. `InvokeWithObject` is measured not to find instances
   (C0.2), so this would mean assembling `self`/`other` by hand.
3. **Call the runtime's method-value helper at `0x14b489070` directly**
   through a function pointer at `modbase + 0xB489070`, passing
   `(item, player, &result, 1, methodValue, &args)`. Most direct, least
   forgiving, and the one most likely to crash if any assumption above is a
   misread.

**None of this has been run.** Per the plan's §3.3 and §4:

- the reading is a hypothesis until a **hook on `PlayerMouseAction` fires
  during a genuine player collect** — that is the cheap, non-mutating
  confirmation, it is a named script so it is hookable today, and it should
  happen before anything is invoked. The handle to hook it by is the name the
  binary itself carries for it, `gml_Script_PlayerMouseAction`, which is the
  form `HookOneScript` already takes;
- a hook at `m_Questpickup`'s address (`0x98732C0`, hookable by address even
  though its name is an `anon@` one) firing in the same collect would confirm
  the inner call too, and its argument can be read at the same time, which
  settles `other.id` by measurement rather than by reading;
- only then the §4 order: save backup, `Quest_Toy_Bear_obj` first, one call,
  progress checked against the quest counter in the UI, not against
  disappearance.

The two things that would have falsified this reading were checked before
writing it up, and both hold — statically, and against numbers C0.4 had
already measured live:

- **`Quest_Act_01_Brick_obj`'s own Create event** is the unnamed body at RVA
  `0x977ED10` (1,695 bytes — it has no script-table symbol, which is why no
  prior session could reach it by name). It was identified by its own
  constants: it writes `questIndex = 1002` and `questObjectType = 23`, the
  exact pair C0.4 read off a live brick. It sets only `questIndex`,
  `questObjectType`, `randomizePos`, `lootName`, `lootSound` and the image
  fields — **it never touches `lootType`**, so a brick inherits the parent
  default.
- **`Quest_Object_Parent_obj`'s Create event** (RVA `0x987F190`) sets that
  default: `lootType = 0`, `canPickup = true`, `distanceForPickup = 0`. The
  last two match C0.4's live readings exactly.

So a brick is `lootType 0` → case 2 of §6's table → **`m_Questpickup`**, with
`canPickup` true, and with no proximity path (§8) because `lootType 0` is not
in that table *and* `distanceForPickup` is 0. Three independent numbers
(`questIndex`, `questObjectType`, `distanceForPickup`) read the same
statically and live, which is the strongest cross-check available without
running anything.

`lootType` is still worth adding to the next `citrace item` dump as a
belt-and-braces confirmation, since it is free.

### 11. Local tooling (not committed, per `agents.md`)

All under `C:\Users\Administrator\ghidra_scripts\` and the session scratchpad:

- `DecompileTrueBounds.java` — decompiles at real `.pdata` entry points,
  deleting the mid-body function artefacts the earlier script created.
- `DisasmRange.java` — disassembles an address range with no `Function`
  needed, for the bodies the decompiler times out on (§9).
- a PE reader that resolves `.pdata` function bounds, walks the name-slot
  table, and annotates either a decompiled listing or a raw disassembly with
  real names.
- the whole-image cross-reference / callee scanner described in §3.

One known gap in the name-slot walker: it skips single-character names, so
`x` and `y` come back unannotated. Harmless here (they were identified by
hand) but worth fixing if these tools are ever promoted.

These are our own code and touch no game text; the decompiler output they
consume stays local. They are generic enough to be worth promoting into
`ForgePact/tools/` alongside `ImportSymbols.java` if a future session wants
them — flagged rather than done, since that is a repo decision.

---

## The hooks were blind: why "0 calls" was never evidence

Built 2026-09-11, same session, immediately after the section above. Written
before any live round, because it came out of reading this repository's own
code rather than the game's.

### What happened

The static read named four functions on the collect path: `PlayerMouseAction`,
`m_Questpickup` (`anon@2786`), `update_quest`, and `CheckPlayerInteraction`.
Three of those four are **already hooked** in `ModuleMain.cpp`, and every one
of them is recorded in these notes as measuring **0 calls** on confirmed
collects. Taken at face value that refutes the whole reading.

It does not. The hooks cannot see those calls.

### The mechanism

`HookOneScript` and `HookRawNamedRoutine` — which between them install every
script hook this plugin has ever used, all 34 of the call sites B1/B2 counted
— install like this:

```
sc = (CScript*)GetNamedRoutinePointer("gml_Script_<name>");
*origOut = sc->m_Functions->m_ScriptFunction;      // save
sc->m_Functions->m_ScriptFunction = detour;        // install
```

That is a **function-pointer swap inside the script-table entry**. It
intercepts a call only if the caller reads that table entry at call time.

This build's compiled GML does not. Read straight off the disassembly of the
real call site (RVA `0x4E67B64`), `PlayerMovement` reaches `PlayerMouseAction`
with a direct `call rel32` to the function body — the address is bound at
compile time and the table is never consulted. Swapping the table pointer
cannot affect it, so the hook never fires no matter how many times the game
runs the code.

`CheckPlayerInteraction` is called the same way, from the compiled Step event
of every shrine, NPC, pile, stone and quest object — so its 0 is explained by
the same thing.

For the `m_Quest*` closures the answer is likely but **not proven**. The
runtime's call-a-method-value helper (`0x14b489070`) reads a function pointer
out of the method object itself (`+0x90`) and calls through it when it is
non-null, which a table swap would also not touch; only if that pointer is
null does it fall back to an index-based path that might be table-mediated.
Which branch these closures take is a live question, not a readable one.

### Why this matters beyond one mod

**Every "0 calls" result in `pet-quest-collector-research.md` that came from a
named-script hook has to be re-read as "the instrument could not see this",
not "the game did not do this".** That includes B1's headline — "no named
script ever fires (34 hooked call sites, 0 calls)" — and B2's. Those sessions
concluded the interaction does not go through any named script; what they
actually measured is that it does not go through the *script table*, which is
a different and much weaker statement.

It also explains a smaller puzzle these notes never resolved: session 7 found
22 raw object-event names that resolved but still never fired. Same cause —
object event code is invoked directly too.

The `citrace dispatchtrace` result is **unaffected**: that one hooks the
builtin dispatcher with `MmCreateHook`, which patches the function's own
bytes, so it genuinely does see every builtin call. Its negative stands, and
so does the reasoning built on it. The same is true of every `HookBuiltin`
hook, which uses `MmCreateHook` as well. The blindness is specific to the two
*script* hook installers.

### `citrace nativetrace` — the same targets, detoured properly

Read-only. Installs `MmCreateHook` detours at the real addresses of the ten
functions on and around the collect path: `PlayerMouseAction`,
`CheckPlayerInteraction`, `update_quest`, and all seven `m_Quest*` /
`m_LootGroundDeActiveStep` closures. Each detour counts the call, logs up to
six of them with `self`, `other`, `argc` and the argument values, and
tail-calls the trampoline.

```
citrace nativetrace          install the detours (prints each resolved RVA)
citrace nativetrace show     native vs table call counts, side by side
citrace nativetrace reset    zero the counters between runs
```

Three things about the design are deliberate:

- **Both hooks stay installed on the same functions, and `show` prints both
  counters.** The comparison *is* the measurement. Asserting that a table
  swap is blind is worth much less than a line reading
  `native=1  table=0` next to the function name.
- **`CheckPlayerInteraction` is the control.** The static read has it running
  from every interactable's Step event every frame, so its native counter must
  climb into the thousands within seconds of standing in a populated room. If
  it does not, the new instrument is as suspect as the old one and nothing
  else in the run should be believed.
- **The address is taken from the existing hook's saved original when there is
  one.** If `citrace 1` ran first, the table entry holds this plugin's own
  table-swap detour rather than the game's function; patching that would
  produce a detour that fires only when the blind hook fires — silently
  reproducing the exact artefact being tested for. A contract test pins the
  ordering.

Per-target log budgets (six lines each) rather than the shared 300-line one,
so the hot control target cannot drown out the one-shot `m_Questpickup` line
that is the actual payload.

Seven contract tests were added (125 in the suite, up from 118): the
subcommand exists and is *not* behind the `confirm` gate (it mutates nothing),
all three helpers sit inside the release guard, the installer uses
`MmCreateHook` and never writes `m_ScriptFunction`, the address resolver
prefers the saved original, the report prints both counters, the control
target is present, and the logging is bounded per target.

### What the live round should show

Install, then collect one quest item normally — no mutating command, no
`confirm`, nothing invoked:

```
citrace 1
citrace nativetrace
... walk to a brick, hover it, press F, watch it collect ...
citrace nativetrace show
```

Predictions, stated before the run so the result can falsify them:

| Target | native | table | meaning if it holds |
| --- | --- | --- | --- |
| `CheckPlayerInteraction` | thousands | 0 | the control: the instrument works, the old one was blind |
| `PlayerMouseAction` | >= 1 per collect | 0 | the reading's outer call confirmed |
| `m_Questpickup` | 1 per collect | 0 | the inner call confirmed, **with its argument logged** |
| `update_quest` | 1 per collect | 0 | the objective credit confirmed |
| the other six closures | 0 | 0 | bricks are `lootType 0`, so only pickup should fire |

The `m_Questpickup` line is the one that matters most: it prints `argc` and
the argument, so it settles by measurement what the static read inferred —
one argument, the player's `id`. If `argc` comes back 0, or the argument is
not the player's id, §6 of the previous section is misread and the call shape
has to be rebuilt from what the log actually shows.

If `PlayerMouseAction` fires but `m_Questpickup` does not, the item is not
`lootType 0` or `canPickup` was false at that moment — both readable from the
same `citrace item` dump.

**Only after that** does anything get invoked, and then in the plan's §4
order: save backup, `Quest_Toy_Bear_obj` first, one call, progress checked
against the quest counter in the UI.

### Build state

Dev and release both rebuilt clean; the full Python suite passes (125 tests).
The dev DLL is deployed to `<game>\bin\mods\aurie\BloodPactPlugin.dll`, with
the previous one kept alongside as `BloodPactPlugin.dll.prev-<timestamp>`.
`PetQuestCollectorTick()` is still untouched and still only counts.

---

## The live round: mechanism CONFIRMED, and the caller I named was wrong

**MEASURED 2026-09-11, live session, on a real `Quest_Act_01_Brick_obj`
collect.** Read-only throughout — `citrace nativetrace` only counts, logs and
tail-calls. Nothing was invoked.

### The counters

```
PlayerMouseAction:         native=0     table=0
CheckPlayerInteraction:    native=3840  table=0     <-- control
update_quest:              native=1     table=(not hooked)
m_Questpickup:             native=1     table=0
m_QuestInteract:           native=0     table=0
m_QuestActivate:           native=0     table=0
m_QuestDestructible:       native=0     table=0
m_QuestActive:             native=0     table=0
m_QuestUseKey:             native=0     table=0
m_LootGroundDeActiveStep:  native=1     table=0
```

All ten detours installed, 0 failed, and **every RVA the plugin printed
matched the static read exactly** — `PlayerMouseAction 0x4E3A010`,
`CheckPlayerInteraction 0x436380`, `update_quest 0x52FCF30`,
`m_Questpickup 0x98732C0`. The file being read is the process being played.

### 1. The hooks were blind — proven, not argued

`CheckPlayerInteraction`: **native=3840, table=0.** The control fired 3,840
times in one short session while the hook that has been installed on it since
session 1 recorded nothing.

That settles it. **Every "0 calls" result in this investigation that came from
`HookOneScript` or `HookRawNamedRoutine` measured the instrument.** Session
1's "`CheckPlayerInteraction` is a false lead — ruled out" was wrong about a
function that runs thousands of times a minute. B1's headline stands
withdrawn.

`m_Questpickup` and `m_LootGroundDeActiveStep` show the same signature —
native=1, table=0 — so the seven `anon@` closures were blind too. The open
question from the previous section (whether the method-value helper dispatches
through the table or through the pointer in the method object) is answered:
**through the method object.** A table swap never touches it.

#### This was also a shipping bug, not only a research one (2026-09-12)

Everything above was written about *instruments*. Origin's review of PR #2
made the obvious next point, which nobody here had made: the shipped gameplay
hooks install the same way, so they were blind in the same places — reporting
`HOOK INSTALLED` and then changing nothing on the paths the game actually
uses. Read-only inspection of the shipped executable found direct native
callers for `StatMovementSpeed`, `StatAttackSpeed`, `DropRelic`,
`DropMonsterGold` and `DropGold` (`StatMovementSpeed` caller RVA `0x59914ed` →
target `0x5b06870`; `DropGold` with 12 verified direct callers), i.e. stat
scaling, the drop multipliers and the max-level relic filter.

`HookOneScript` now installs both routes and hands the hook body the
trampoline; `HookOneScriptTable` keeps the old behaviour for the 58
research-only installs, because `citrace nativetrace`'s table-vs-native
comparison — the measurement on this very page — needs one side to really be
table-only. Details in `docs/submodules/ForgePact/instructions.md`, Known
Limitations item 12.

The lesson worth carrying: this page had the finding for a day and drew only
the research conclusion from it. **When an instrument turns out to be blind,
check whether anything shipped is built the same way.**

### 2. The collect, measured

In order, from one F press:

```
m_LootGroundDeActiveStep  self=Quest_Act_01_Brick_obj#3881@297149  other=Loot_Manager_obj#2514@262158  argc=0
keyboard_check_pressed(70) -> 1
m_Questpickup             self=Quest_Act_01_Brick_obj#3881@297149  other=Loot_Manager_obj#2514@262158  argc=1  a0=real:1
update_quest              self=Quest_Act_01_Brick_obj#3881@297149  other=Loot_Manager_obj#2514@262158  argc=3  a0=int64:1002  a1=real:0  a2=real:1
```

The item's own fields at that instant, logged on the same lines:
`questIndex=1002`, `questObjectType=23`, `questValue=1`,
`questObjectiveNumber=0`, `canPickup=true`, `distanceForPickup=0`,
`activateQuestObjectWithMouse=false`.

**`update_quest`'s signature is now measured, not inferred:**
`update_quest(questIndex, questObjectiveNumber, questValue)` — `(1002, 0, 1)`,
each argument matching the item's own field of that name.

Only `m_Questpickup` fired among the seven closures, which is exactly what
`lootType 0` predicts.

### 3. What I got wrong

**`PlayerMouseAction` never ran.** `native=0`. The previous section named it
as the caller and it is not on this path at all.

The code I read in it is real — that branch exists, and `object_is_ancestor`,
the `with`, the `canPickup` gate and the `lootType` switch are all genuinely
there. But it is **not the path a brick collect takes**, and I treated "this
function contains a matching branch" as "this function is the caller" without
anything forcing that conclusion. The `canPickup` cross-reference list in §6
had `Loot_Manager_obj` sitting in it the whole time; I passed over it because
`PlayerMouseAction` looked like the obvious answer.

Two smaller errors follow from the same mistake:

- **`other` is `Loot_Manager_obj`, not the player.** I predicted the player.
- **The argument is not the player's `id`.** It is `real:1`. The player's
  instance id would be a six-figure number; this is 1.

### 4. The real caller, read after the fact

The function is an unnamed body inside `Loot_Manager_obj`'s region, RVA
`0x86E0AC0` (20,099 bytes), the one the §6 cross-reference list showed as
`anon@11280@gml_Object_Loot_Manager_obj_Create_0`'s neighbour. Described
rather than transcribed, its quest branch switches context to the loot
instance it currently has in focus — so inside the branch `self` is the item
and `other` is `Loot_Manager_obj` — checks that the focused instance is
defined, then requires the item's `canPickup` and `lootType == 0` before
invoking the item's own `m_Questpickup`. The single argument it passes is
this function's own `argument0`, defaulting to `undefined` when it was called
with none.

Points worth keeping:

- **The `canPickup` gate and the `lootType == 0` test are both here**, at the
  real call site, comparing against a literal int64 `0`. So the case mapping
  in §6 was right about *which* method a `lootType 0` item gets, and the
  brick's `lootType` is confirmed to be 0 by the path it actually took —
  which is what the parent Create default said it would be.
- **The argument is a pass-through of this function's own `argument0`**, with
  a literal `undefined` RValue as the default when it is called with no
  arguments. So the game itself is prepared for `m_Questpickup(undefined)`;
  the `real:1` seen live is whatever its caller handed down, and `questValue`
  on the item is also 1.
- The same body calls `ClearSpecificInput` — which is the `inputState` flip
  from `1 -> 0` that B4 measured and could not account for. It is the interact
  key being *consumed* by the loot system.
- Its other named callees line the rest of the pickup up: `PickupLoot`,
  `InventoryLogAddItem`, `PlayLootPickupSound`, `CreatePickupEffect`,
  `GetPlayerDevice`, `CheckInterfacesOpen`, `CreateItemSaveStruct`.

So the item is selected through the loot system's focus
(`lootBoxInFocus` / `playerLootTarget` / `lootInstance` all appear in this
body), not through `mouseTarget`. That is a different subsystem from the one
§6 described, and it is the one that runs.

### 5. Where this leaves the mod

The confirmed call, every part of it now measured rather than inferred:

> with the quest item as `self` and `Loot_Manager_obj` as `other`, invoke the
> item's own `m_Questpickup` method value with one real argument (`1` on a
> real collect; the game's own call site also admits `undefined`).

`m_Questpickup` then calls `update_quest(questIndex, questObjectiveNumber,
questValue)` and `QuestSaveUpdate`, so the objective credit is inside the
call — the original plan's §11 "progress, not disappearance" requirement is
satisfied by the target itself, not by anything we have to add.

Plan C's Phase C0 exit criterion is met on the positive branch at last: a
mechanism is confirmed, by live measurement, on a real collect. **Phase C2 is
now unblocked** — but §4 binds from the first invoke:

1. back up the save;
2. `Quest_Toy_Bear_obj` (Act 8, Zone 8-4) or a fresh character first;
3. one item, one call, one observation;
4. verify the quest counter in the UI advances — not that the item vanished.

The invoke path itself still has to be built: C0.2's `citrace invoke` could
not set `self` to the item, and that is exactly what this call needs. The two
routes left are the runtime's `with` helpers (`0x14b489710` / `0x14b489e50` /
`0x14b4893d0`) or calling the method-value helper `0x14b489070` directly with
`(item, lootManager, &result, 1, method, &args)` — now known to be reachable,
since `citrace nativetrace` just trampolined through that exact function ten
times without incident.

### 6. Method note

Two readings in a row put a plausible caller in front of a correctly-read
callee. The callee was right both times; the caller was guessed from a
cross-reference list and confirmed nothing. **A cross-reference proves a
function mentions a name, never that it is the one that runs.** The cheap fix
is the one that worked here: detour every candidate at its real address and
let the counters say which. That took one live round and no argument.

---

## Phase C2: `citrace collect` — the measured call, wired up

Built 2026-09-11 immediately after the confirming round. **Not yet run.** This
is the first code in the whole investigation that can credit a quest
objective, so plan §4 binds in full from its first use.

### What it does

```
citrace collect confirm [path] [args...]
```

Takes the nearest quest item and makes the call the live round measured,
with nothing inferred:

| part | value | where it came from |
| --- | --- | --- |
| `self` | the quest item | measured |
| `other` | the live `Loot_Manager_obj` instance | measured — **not** the player, which is what the static read guessed |
| method | the item's own `m_Questpickup` variable | measured |
| args | one real, default `1` | measured (`a0=real:1`) |
| call shape | the runtime helper at `exe+0xB489070` | measured, and its signature read off its own body |

The helper's signature, for the record:
`void(CInstance* self, CInstance* other, RValue* result, int argc, RValue* method, RValue** args)`
— the same shape as the builtin dispatcher with a method value in place of a
numeric id. `citrace nativetrace` trampolined through that exact function ten
times in the confirming round, so it is known-good on this build; the address
is printed on every call so a build mismatch shows up as a wrong address
rather than as a crash.

### The new `native` invoke path

`kCiInvokePaths` gains `native` **at the front**, so an `auto` run reaches the
shape known to work before spending faults on the nine that are measured
negative. Those nine are demoted, not deleted — a future session can still
name one and re-run it without a rebuild. `citrace collect` defaults to
`native` explicitly rather than relying on the ordering.

### It reproduces the game's own gates, deliberately

The real call site (RVA `0x86E0AC0`) tests `canPickup` and `lootType == 0`
before invoking. `citrace collect` re-reads both from the live item and
**refuses** if either fails.

That is not belt-and-braces. Calling `m_Questpickup` on an item the game
would have skipped is precisely how an objective gets credited for something
that should not have been collectable — the failure mode the original plan's
§11 calls worse than no mod at all. Refusing is the whole point; the gates
are a feature of the mechanism, not an obstacle to it.

It also refuses when no `Loot_Manager_obj` instance is live, rather than
substituting something else for `other`, since that would no longer be the
measured call.

### Safety

Unchanged from the C0 mutating commands, and it reuses their machinery:
literal `confirm` token, the §4 banner once per session, and
`CiInvokeMethodValue`'s before/after block — item marker flags, family count,
and the quest progress read — on both sides of the call. One item, one call,
no loop. It ends by printing the §4 rule 3 reminder in full: *an item
vanishing is not the pass condition; check the quest counter in the UI.*

Six contract tests were added (131 in the suite, up from 125): the confirm
gate, the measured context (with `other=player` pinned *out*, since that was
the wrong guess), the measured defaults, both of the game's gates present and
refusing, the release guard, and the helper address.

One existing test was rewritten rather than deleted:
`test_with_context_path_is_offered_first` pinned `InvokeWithObject` first
because it was the last *untried* shape. It has since been tried and is
negative, so the invariant it protected is gone; the replacement,
`test_the_measured_call_shape_is_offered_first`, pins the measured shape and
also asserts the disproven ones are still selectable.

### Running it — the §4 order, not a shortcut

```
1. Back up %LOCALAPPDATA%\Hero_Siege\  (and any cloud-sync copy).
2. Fresh character if practical; otherwise Quest_Toy_Bear_obj (Act 8, Zone
   8-4), which the tester confirmed stays collectible after its quest
   completes - the safest possible target.
3. citrace 1
   citrace item                 <- read lootType/canPickup before anything
   citrace nativetrace          <- so the detours log what our own call does
4. citrace collect confirm      <- ONE item, ONE call
5. Read the before/after block, then look at the quest counter in the UI.
```

Step 3's `nativetrace` is worth keeping armed: it means our own invoke is
observed by the same instrument that watched the real collect, so the two can
be compared line for line. If `citrace collect` works, the log should show
`m_Questpickup` and then `update_quest` firing with the same argument shape
the real collect produced — `update_quest(questIndex, questObjectiveNumber,
questValue)`.

**Pass condition:** the quest counter advances by the item's `questValue`.
**Not** the item disappearing, and **not** `m_Questpickup` merely returning
without faulting.

If the objective does not advance, the most likely cause is the argument:
`1` is what the real collect passed, but what `m_Questpickup` does with it
has not been read, and the game's own call site also admits `undefined`. Retry
with an explicit argument before concluding the mechanism is wrong.

### Build state

Dev and release rebuilt clean; 131 tests pass; `citrace collect` and
`Loot_Manager_obj` are present in the dev DLL and absent from the ship DLL.
Dev build deployed, previous kept alongside as
`BloodPactPlugin.dll.prev-<timestamp>`. At the time of writing,
`PetQuestCollectorTick()` was still untouched and still only counted —
nothing went in there until a live run showed the objective advancing. That
run is the next section; C2 landed immediately after it.

---

## `citrace collect` RUN LIVE — the objective advanced. Mechanism works.

**MEASURED 2026-09-11. The quest counter for "Bricks… so many Bricks" went
7/15 → 8/15 on a plugin-invoked collect.** This is the pass condition the
original plan's §11 set and that every prior session failed to reach: not an
item disappearing, but the objective being credited.

### Setup

Save backed up first (plan §4 rule 1) to
`%LOCALAPPDATA%\Hero_Siege_backup_C2-20260911-131445`.

**Deviation from §4 rule 2, taken knowingly:** the tester ran on an Act 1
brick with the brick quest *in progress*, not on `Quest_Toy_Bear_obj`. Rule 2
asks for the non-progress-gated item first precisely because a mis-credit on
a live quest chain is the expensive failure. The tester chose the brick
because that quest was already active and set up; the backup made it
recoverable. Noted rather than smoothed over — the rule was right and it was
not followed.

### The item, read before the call (`citrace item`)

`Quest_Act_01_Brick_obj#3881@301975`, 59 variables. The two gates:

- **`lootType = int64:0`** — the belt-and-braces check flagged as the single
  cheapest falsifier of the whole model. It holds. The value was derived
  statically from `Quest_Object_Parent_obj`'s Create default (the brick's own
  Create never sets it); now it is measured.
- **`canPickup = bool:true`**

plus `questIndex=1002`, `questObjectType=23`, `questValue=1`,
`questObjectiveNumber=0`, `distanceForPickup=0`, `lootName="Brick"`,
`deleteTimer=16.7` and falling (these do despawn on their own, which is worth
knowing when timing a live round).

### The call

`citrace collect confirm` — one item, one call, default path `native`,
default argument `1`.

```
m_Questpickup   self=Quest_Act_01_Brick_obj#3881@301975  other=Loot_Manager_obj#2514@262063  argc=1  a0=real:1
update_quest    same self/other                          argc=3  a0=int64:1002  a1=real:0  a2=real:1
```

**Those two lines are identical in shape to the ones the real F-press collect
produced** — same `self`, same `other`, same argument, and `update_quest`
receiving the same `(questIndex, questObjectiveNumber, questValue)` triple.
The call returned `undefined` and did not fault.

Family count `1 -> 0`; the item left the enumeration. And the counter moved.

### What this settles

- **The mechanism is confirmed end to end**, by reproduction rather than
  observation: we make the call the game makes, and the game credits it.
- **The call shape is right in every part** — including the two the static
  read got wrong and the live round corrected (`other = Loot_Manager_obj`,
  argument `= 1` rather than the player's `id`).
- **`m_Questpickup` removes the item itself.** `m_LootGroundDeActiveStep`
  measured `native=0` on our call — it fired during the *real* collect but
  not during ours, and the item vanished anyway. So the removal is inside
  `m_Questpickup` (or downstream of `update_quest`), not in the
  loot-deactivation step, and our path does not need it.
- The control held throughout: `CheckPlayerInteraction` finished the session
  at **native=8310, table=0**.

### The one known difference from a real collect

Our path skips whatever `Loot_Manager_obj` does *around* the call — the real
collect also ran `m_LootGroundDeActiveStep`, and the loot body calls
`ClearSpecificInput`, `PickupLoot`, `InventoryLogAddItem`,
`PlayLootPickupSound` and `CreatePickupEffect`. None of that is required for
the objective credit, which is what this mod needs, but it is why a
plugin-driven collect is silent and effectless on screen where a real one
plays a sound and shows a pickup effect.

**Worth checking before Phase C2 ships**, since neither is quest state and
neither was observed this round:

- whether the item is logged to the inventory/loot log the way a real collect
  logs it (`InventoryLogAddItem` did not fire on our call);
- whether anything in the loot system is left in a stale state by collecting
  an item it never had in focus.

### Phase C2 proper is now unblocked

*(Written before C2 landed; the section after this one is what actually
shipped, and answers each bullet.)*

`PetQuestCollectorTick()` is still untouched and still only counts — the
contract test enforcing that is still green, deliberately. Wiring the
confirmed call into it is the next piece of work, and it is a design step,
not a copy-paste of `citrace collect`:

- **pacing** — one item per N frames rather than every item in one tick;
- **the accepted-quest gate** — only collect items belonging to a quest the
  player actually has;
- **the `Loot_Manager_obj` dependency** — `citrace collect` refuses when no
  instance is live; the tick needs to decide the same way rather than
  substituting something;
- **the two gates stay** — `canPickup` and `lootType == 0` are the game's own
  and must be re-read per item, not assumed from the family membership;
- **`questValue` as the argument** rather than a hardcoded `1` — they were
  equal on this item, so this round cannot tell them apart, and an item with
  `questValue != 1` would settle it.

---

## Phase C2 SHIPPED — the mod collects, live-measured

**MEASURED 2026-09-11.** The confirmed call is now wired into
`PetQuestCollectorTick()` and was run as the mod, not as a research command.
Two `petquest stat` readings from the same session, taken on `petquest 0`:

```
petquest stat: family objs seen=2491 excluded=0 on-screen=3  | pet-seen ticks=39105 | no-camera ticks=0
  collected=3 | skipped(gate)=0 skipped(no Loot_Manager)=0 | target lost=0 travel timeouts=0 | phase=idle arg=1.00

petquest stat: family objs seen=4988 excluded=0 on-screen=19 | pet-seen ticks=41745 | no-camera ticks=0
  collected=6 | skipped(gate)=0 skipped(no Loot_Manager)=0 | target lost=0 travel timeouts=0 | phase=idle arg=1.00
```

Every failure counter the tick can raise stayed at zero across both runs: no
item was refused by the game's own gates, no collect was skipped for a missing
`Loot_Manager_obj`, no target was lost while the pet walked, and the travel
timeout never fired — so the pet reached every target under its own straight-
line movement rather than being collected at the timeout fallback.

### How each open design question from the previous section was answered

- **Pacing** — one target at a time. The tick is a two-phase state machine
  (`Idle` → `Travel`); `Idle` picks the single nearest eligible item and
  `Travel` walks the pet to it, so there is no sweep. `kPetQuestCooldownFrames`
  (24, ~0.4 s) sits between collects so it reads as fetching.
- **The `Loot_Manager_obj` dependency** — the tick refuses exactly as
  `citrace collect` did: no live instance, no call, and the skip is counted
  (`skipped(no Loot_Manager)`) rather than substituted for.
- **The two gates** — `PetQuestItemIsCollectable()` re-reads `canPickup` and
  `lootType == 0` from the live instance both at selection and again inside
  `PetQuestCollectOne()`, never caching them from selection. An item can stop
  being collectable while the pet is walking.
- **`questValue` vs a hardcoded `1`** — still reproduced rather than
  understood, and still `1`. It is now adjustable live (`petquest arg <n>`)
  so an item with `questValue != 1` can settle it without a rebuild; no such
  item has been tested yet.
- **The accepted-quest gate** — *not implemented.* The tick collects any
  `lootType == 0` family member on screen, whether or not the player has that
  quest. The game's own gates are what stand between the mod and a bad
  collect. See the open items below.

### Behaviour deliberately not reproduced

`m_Questpickup` removes the item and credits the objective; everything the
real collect does *around* the call is skipped — `ClearSpecificInput`,
`PickupLoot`, `InventoryLogAddItem`, `PlayLootPickupSound`,
`CreatePickupEffect`. So a pet collect is silent and has no pickup effect
where a player's own collect plays one. Cosmetic, and known.

### Open items

- **The pickup is silent** (above). Adding the sound/effect means invoking
  more of the loot body than the objective credit needs, so it was left out.
- **`InventoryLogAddItem` does not fire** on our path, so a pet-collected
  item is not written to the inventory/loot log the way a hand-collected one
  is. Not quest state, so it does not affect the objective.
- **No accepted-quest gate**, as above.
- ~~**`kQuestPickupCallFnRva` is a hardcoded address** (`exe+0xB489070`) and it
  ships in the player build.~~ **CLOSED 2026-09-11, second pass** — see below.

---

## Second pass: the collect call, minus the address

The open item above was the right one to close first. `PetQuestCollectOne`
shipped a call through `GetModuleHandleA(nullptr) + 0xB489070` that validated
nothing — not the module, not the bytes, not the build. Everything else in the
plugin resolves by name and fails harmlessly; this one would have transferred
control into whatever a future build put at that offset. It is `relicgate`'s
defect (a fixed RVA inside `DropItem`, long since pointing elsewhere, feature
silently dead) with a worse failure mode, because `relicgate` only ever read.

### The attempt: read the callable off the value itself

`exe+0xB489070` is the runtime's call-a-method-value dispatcher. What it does
is look up the callable behind a method value and call it with the supplied
self/other. A GML method value normally **is** a `CScriptRef`, and YYToolkit
defines that struct: it carries the compiled function (`m_CallScript`'s script
function, or `m_CallYYC`) and the instance the method is bound to
(`m_BoundThis`). So the thing the dispatcher goes to find looked like it was
already in our hand — no address needed.

`InvokeMethodValue` was written to read it directly. The call shape was
unchanged and still the measured one — `self` = the item, `other` =
`Loot_Manager_obj`, one real argument. Only the route changed, moving the
dependency from *this month's `Hero_Siege.exe`* to *YYToolkit's struct
layout*.

Requires `/DYYTK_DEFINE_INTERNAL=1` on the compile line (the opaque
`CScriptRef` stand-in has no members) — see `plugin/BUILD.md`.

**This did not work on this runner** — see "Third pass" below. It is kept as a
validated fallback, not as the shipped mechanism, and the reason it failed is
the most useful thing in this whole section.

### Checks instead of a jump

Before anything is called: the value is `VALUE_OBJECT` and
`OBJECT_KIND_SCRIPTREF`; it is readable as a `CScriptRef`; and a callable
field is committed, executable, and inside `Hero_Siege.exe`'s image
(`AddrIsExecutableInModule`, via `VirtualQuery`). Every layout read is
`IsBadReadPtr`-guarded. Any failure returns `false`, bumps a counter, and logs
one line naming the field — so a build where this does not hold says so
instead of crashing or going quiet.

That design is what made the next two rounds cheap: the wrong hypothesis
produced 309 counted refusals and a field dump, not 309 jumps into zero.

### What survives of the old path

`citrace collect`'s `native` path, dev build only, now itself address-checked.
It exists so a session can run the old shape and the shipped one against the
same item and confirm they are one mechanism. `scriptref` is the default and
is what the mod runs, so a green `collect` remains evidence about the mod.

### Third pass: the first build of this did not collect

Reported live the same day: **the pet selects and flies to the nearest quest
item correctly, and then nothing happens.** Travel working narrows it to
`PetQuestCollectOne` → `InvokeMethodValue`; the deployed DLL was confirmed by
hash to be that build.

One flaw found by re-reading, and it is mine, not the runtime's:
`MethodValueFunction` took `m_CallYYC` first and consulted `m_CallScript` only
when `m_CallYYC` was **null** — so a non-null `m_CallYYC` that failed the
executable-in-image check refused the whole call rather than falling through
to the other field. The preference was also backwards. `m_CallScript`'s script
function is the field this plugin already proves on this runtime:
`HookOneScript` swaps exactly that pointer, and this document's own
`nativetrace` round resolved `m_Questpickup` to `exe+0x98732C0` through it.
That is a positive control; `m_CallYYC` has none here. Fixed: both candidates
are validated independently, proven field first.

Two other changes, both about not repeating the original mistake in a new
shape:

- **The bind check is counted, not enforced.** "`m_BoundThis` is this exact
  `CInstance`" was written as a *gate* on an assumption never measured on this
  runtime, while the game's own call site supplies `self` explicitly anyway.
  The check that protects the process is the executable-in-image one, and that
  still gates. Refusing a measured-correct call over an unmeasured assumption
  is the same class of error as trusting an address.
- **A refusal now names itself.** The first structural refusal of a session
  logs one line to `out.txt` with `m_Kind`, `m_ObjectKind`, `m_CallScript`,
  the resolved script function, `m_CallYYC`, whether each is in-image, and the
  `m_BoundThis` kind — in **both** builds, because this is the failure a
  future game or YYToolkit update will produce and the player's own log should
  explain it. Every layout read is `IsBadReadPtr`-guarded, so a wrong layout
  refuses instead of faulting inside a frame callback.

That fix was real but it was **not** the cause. Measured, one launch later:

```
petquest stat: family objs seen=2590 excluded=0 on-screen=615 | pet-seen ticks=12046
  collected=0 | skipped(gate)=0 skipped(no Loot_Manager)=0 | target lost=0 travel timeouts=0
  REFUSED (structural): no callable on the method value=309 (collect did NOT run)

petquest: COLLECT REFUSED - the object is not OBJECT_KIND_SCRIPTREF.
  [kind=6 objectKind=0 m_CallScript=0000000000000000 scriptFn=0000000000000000(inImage=0)
   m_CallYYC=0000000000000000(inImage=0) boundKind=496]
```

**On this runner the `m_Quest*` values are not `CScriptRef`s.** `m_Questpickup`
is `VALUE_OBJECT` with `m_ObjectKind = 0` (`OBJECT_KIND_YYOBJECTBASE`, not
`SCRIPTREF`), both callable fields read as 0, and `method_get_index` returns
nothing for it — while a sibling variable on the same instance,
`s_lootDrawData`, *does* resolve, to script `#105134` (that contrast is
visible in this document's own `citrace item` dump). This game boxes these
values in a shape YYToolkit's `CScriptRef` does not describe. Reading the
callable out of the struct cannot work here, and no reordering of the fields
would have changed that.

Note what the first and second attempts have in common: **a layout somebody
wrote down was trusted without a positive control on this target.** An address
is the loud version of that mistake. A struct field is the quiet one. Both
were wrong for the same reason, and the second one was written *while fixing*
the first.

## Fourth pass: `script_execute` — CONFIRMED

What ships. The route that assumes nothing about the value's shape is to let
the runtime dispatch it:

```cpp
g_Yytk->CallBuiltinEx(res, "script_execute", item, lootManager, { methodValue, arg })
```

A builtin resolved **by name**, with `self` and `other` supplied by
`CallBuiltinEx`. No address, and no struct layout either — strictly more
version-proof than both earlier attempts. The call shape is the one measured
on a real collect and never changed across any of the three rounds: `self` =
the item, `other` = `Loot_Manager_obj`, one real argument.

Confirmed live 2026-09-11, plan §4 rule 3 satisfied — **the quest counter
advanced**, not merely the item vanishing:

```
collected=1 | skipped(gate)=0 skipped(no Loot_Manager)=0 | target lost=0 travel timeouts=0
call route=script_execute (name-resolved, no layout) | dispatched-but-item-remained=0
```

Zero structural refusals. `dispatched-but-item-remained=0` is a new counter
added for this round: §2 of the plan warned `script_execute` may *quietly
no-op* on a method value rather than reject it, and since `m_Questpickup`
removes the item, an instance still present after the call would have caught
exactly that. It is not proof of credit — rule 3 still stands, and the UI
counter is what confirmed this — it only separates "nothing ran" from "ran and
did nothing", which are identical from outside.

The `CScriptRef` route remains as a fallback for a runner where that layout
holds, reached only when `script_execute` fails to dispatch at all — never as
a retry of a call that already ran, so "one item, one call" holds.

### Irony worth recording

`script_execute` was one of the nine shapes `kCiInvokePaths` had written off
as "measured negative (C0.2)". It was on the list the whole time. See the
loose end below for why those negatives were never fair tests — the same
`argc = 0` / no-`self` problem §7 identified — and note that closing this took
*three* rounds of chasing call machinery when the working answer was sitting in
a table of already-implemented paths.

### The loose end, now partly pulled

The nine name-resolved shapes in `kCiInvokePaths` were labelled "measured
negative (C0.2)". They were swept with **no argument and no way to set
`self`** — the same conditions §7 above later explains away ("This explains
every C0.2 access violation"). One of them, `CallBuiltinEx` +
`script_execute`, is now the shipped mechanism, which retroactively proves the
label wrong for at least that one.

The other eight remain **not observed**, not **does not work**. `citrace
collect confirm <path> 1` supplies an argument and a real `self` to every one
of them, so any of the eight can be re-tested in one line with no rebuild —
§4 rules still bind if anyone runs them.

**The lesson that cost three rounds:** a negative result is only as good as
the conditions it was measured under, and those conditions have to be written
down *next to* the result. "Measured negative" sat in a table for weeks while
the thing it described was the answer. This document's own "Prove the
Instrument Before Trusting a Negative Result" rule (now in the repo-root
`agents.md`) was written about hooks that could not fire; it applies just as
much to call shapes that were never given their arguments.
