# Pet Quest Collector — Plan B4 research notes

Companion to `ForgePact/docs/pet-quest-collector-plan-b4-input-simulation.md`
(the plan this Phase 0 research pass belongs to) and
`ForgePact/docs/pet-quest-collector-research.md` (the original plan's B1/B2
research, concluded blocked). Per `agents.md`: record only names, indices and
measured behavior - no decompiled script text.

**Status (2026-09-10): Phase 0 complete. B4b-i is ruled out on both halves.**
Every item in the plan's §5 Phase 0 checklist needs a real player in a real
game session (hovering an item, pressing F, watching what the game does) -
there is no way to produce or observe that from outside the running game.
This session built the plugin-side tooling those rounds need and then ran
them live with the tester. Item 1 (find the `inputState` F-index) succeeded
cleanly. Items 2 and 3 both came back **negative**, for the same underlying
reason: the GML-visible input state is a downstream *record* of real input,
not the state the game reads. Item 4 is moot as a result. See "Conclusion" at
the bottom for the evidence table and what remains if this mod is picked up
again. The four numbered subsections below are the exact checklist from §5.

---

## Tooling built this session

All of it lives in the existing `citrace` research command (dev build only -
`plugin_build\build.bat dev`, never reachable from the player/ship build; see
"Build note" below for how that guarantee was actually re-verified this
session). Nothing here installs a hook, spawns anything, or is reachable from
`petquest`/any panel toggle - these are hand-run, one-shot commands typed into
`<game>\bin\bp_ipc\cmd.txt`.

### `citrace snap1` / `citrace snap2` (already existed - plan §5 item 1)

No new code needed. The existing whole-state snapshot/diff tooling (built
during the original plan's Phase 0, see `pet-quest-collector-research.md`
sessions 6-7) already captures every instance variable on the resolved
`Profile_Manager_obj` instance, including a full expansion of its `inputState`
array (`CiExpandContainer`, up to 100 elements) - so a clean, isolated
`citrace snap1` / press-and-release F once / `citrace snap2` round already
gives a full before/after dump of `inputState` to diff by eye. `citrace
snap2`'s room-change guard (added in the original session) also still applies,
so a contaminated round (a zone transition or death between snap1 and snap2)
is flagged rather than misread as evidence.

**How to run it:** `citrace snap1`, then in-game hover a quest item and press
F exactly once (no other input in between), then `citrace snap2`. Read the
`[ProfileManager] ~ inputState : array[78]={...} -> array[78]={...}` diff line
and find the one element whose value changed - that index is the F key's
tracked slot in the plan's hypothesis.

### `citrace pokekey <index> [value=1]` (new - plan §5 item 2)

Writes one element of `Profile_Manager_obj.inputState` to `value` (default
`1`) via `array_set` on the array returned by `variable_instance_get` (the
existing YYTK `CallBuiltin` chokepoint every other citrace command already
uses - not a new access path), then **explicitly restores** the previous
value on the very next `FrameCallback` tick regardless of whether the array
would have self-healed on its own. The plan's §3a treats "does it self-heal
or need an explicit restore" as a live-only open question; this always
restores explicitly, which is strictly safer (a self-healing array just gets
restored to the value it would have held anyway - no functional difference)
and removes the "stuck F held" failure mode named in the plan's §7 risk
table. The restore log line still reports whether the array had already
changed back on its own before the explicit restore ran, so the self-heal
question gets answered as a side effect rather than given up.

**How to run it:** once `citrace snap1`/`snap2` has identified the candidate
index (call it `N`), hover a quest item and run `citrace pokekey N 0` with no
real key press. Watch the game, not the log: does the interact prompt appear,
or does the item respond, exactly as if F had really been pressed? That
visible in-game reaction (or lack of one) is the actual pass/fail signal the
plan's item 2 asks for - the log line only confirms the poke and restore
themselves executed.

> Value `0`, not the command's default of `1`: a real press clears the slot
> rather than setting it (item 1 below). **This was run and the answer was
> no** - see item 2. The command is left in place because it is a generic
> one-element `inputState` poke with a safe auto-restore, useful for any
> future question about that array, not because this particular test is
> still open.

`citrace pokekey2 <index1> <index2> [value=0]` (added mid-session) does the
same thing for two indices in the *same* frame, which is what a real press
does to indices 30 and 60 - built once it turned out that poking either alone
did nothing, to rule out "the check needs both at once."

### `citrace mouse` / `citrace mousewrite <x> <y>` (new - plan §5 item 3)

- `citrace mouse` reads `mouse_x`/`mouse_y` via `variable_instance_get` on the
  resolved player instance - exactly the technique the plan's §5 item 3 names
  - plus `Profile_Manager_obj.mouse_x_prev`/`mouse_y_prev` if present (both
  were seen as real instance variables on that object in the original
  session's var dump). Meant to be called repeatedly while the tester moves
  the real cursor, to confirm by eye that the read values track it.
- `citrace mousewrite <x> <y>` is a cheap write-then-readback test of the same
  two variables: writes, reads back immediately, and restores the original
  values regardless of outcome. Neither command touches OS input in any way -
  this is a read/write probe through YYTK only, not B4b-ii's OS cursor move.

> **Measured correction (item 3 below): `mouse_x`/`mouse_y` do not exist as
> runtime variables at all**, so `citrace mousewrite` has nothing to write to
> and `citrace mouse` reports them as `undefined`. What `citrace mouse` is
> actually useful for is the second half of its output -
> `ProfileManager.mouse_x_prev`/`mouse_y_prev`, which *do* track the real
> cursor 1:1 in screen space. To write those, use the pre-existing generic
> `oset Profile_Manager_obj mouse_x_prev <value>` - no new command needed
> (and it was tried; see item 3).

**What this does not do:** locate the actual backing memory storage a native
plugin could write directly, now that the GML-level write is confirmed to be
a dead end. The plan's §5 item 3 names that as open-ended, unbounded-until-
tried effort - it remains unattempted, and is one of the two paths listed
under "What is left" in the Conclusion.

---

## Build note: a pre-existing release-build compile gap, found and fixed

While adding the tooling above next to `CiSnapTake`/`CiSnapDiff`, actually
building the **release/ship** configuration (`plugin_build\build.bat`, no
`dev` argument - the one that produces `modfiles_shipped\BloodPactPlugin.dll`,
the file players get) failed: `CiSnapTake`, `CiSnapDiff`, and
`CiFindNearestQuestItem` were defined *outside* the `#ifndef
FORGEPACT_RELEASE` guard that protects the rest of the citrace machinery
(`CiGetProfileManagerObjIdx`, `CiSnapshotInstance`, `CiSnapshotGlobals`,
`CiDiffSnapshot`, `CiDescribeInstance`, etc.), even though they call directly
into it. That's a pre-existing gap from whichever earlier session added
`snap1`/`snap2` (not introduced by this session's additions), just never
caught because `build.bat dev` - the configuration used for every prior
citrace research round - doesn't define `FORGEPACT_RELEASE` and so never hit
the missing symbols.

Fixed by moving the `#ifndef FORGEPACT_RELEASE` guard to cover
`CiFindNearestQuestItem` through this session's new `CiMouseWriteTest`, all in
one block (see `ModuleMain.cpp` around `CiFindNearestQuestItem`). Both
`plugin_build\build.bat dev` and `plugin_build\build.bat` (release/ship) were
rebuilt clean after the fix, and the full Python contract-test suite (100
tests, `tests/`) still passes unchanged. This also means
`modfiles_shipped\BloodPactPlugin.dll` is now rebuilt from source that
actually compiles as release - it was silently impossible to produce a fresh
one from this branch before this fix.

---

## Phase 0 checklist status (plan §5)

1. **Confirm the `inputState` F-index precisely. MEASURED 2026-09-10,
   live session.** Clean, isolated round: `citrace snap1` while hovering
   `Quest_Act_01_Body_Part_obj#3880` (173 px away), tester pressed F once
   (nothing else in between), `citrace snap2`. Result: exactly two of the 78
   elements changed, both from `1` to `0` - **index 30 and index 60**, both
   in the same direction on the same press. Everything else in the array
   (all 78 elements were `1` in the "before" snapshot) stayed `1`.

   **Correction to the plan's own working hypothesis:** §3a's text assumed
   writing `1` would simulate a press ("write that index to `1` for exactly
   one frame"). The measured real press does the opposite - it flips the
   tracked slot(s) from `1` to `0`. Every rest-state element in the "before"
   snapshot being uniformly `1` is consistent with this array tracking
   something closer to "available"/"not yet consumed this press" than a
   literal "is held" boolean - a real press clears it, not sets it. This
   matters directly for item 2 below: the poke must write `0`, not the
   `citrace pokekey`'s documented default of `1`.

   Also matches the original research doc's earlier noisy finding
   ("index 30, and separately 30+60 in a longer, contaminated window",
   `pet-quest-collector-research.md` §1 session 6) - this clean round
   confirms both indices precisely, with no contamination.

   **Corroborating evidence the press was genuinely registered**, independent
   of the array question: `citrace snap2`'s "nearest quest item" resolved to
   a *different* instance (`#3880@305139`, 511 px away) than snap1's
   (`#3880@305042`, 173 px away) - consistent with the original session's
   documented pattern (`pet-quest-collector-research.md` §1 session 6) of a
   collected item being deactivated and dropping out of
   `instance_number`/`instance_find` enumeration entirely, so the *next*
   nearest instance of the same object type (this item auto-collects nearby
   duplicates, per the tester) took its place as "nearest." `citrace`
   tracing itself was not armed this round, so `instance_destroy`'s call
   count was not independently checked this time - not needed, since the
   original session already established that counter stays 0 on a genuine
   collect either way.

2. **Confirm a poke to that index is sufficient. MEASURED 2026-09-10, live
   session - NEGATIVE.** Tested all three ways, each while genuinely hovering
   a quest item, no real F press: `citrace pokekey 30 0` alone - nothing.
   `citrace pokekey 60 0` alone - nothing. `citrace pokekey2 30 60 0`
   (added mid-session specifically to test both simultaneously in the same
   frame, since a real press flips both together) - still nothing. All three
   pokes executed and restored cleanly (confirmed via the restore log line;
   `inputState` does not self-heal on its own - the explicit restore was
   needed every time, answering that sub-question too).

   **Conclusion: `inputState` is not what the interact/collect check reads.**
   It reliably records a real press (item 1) but writing it directly produces
   no visible reaction, in any combination tried. Most consistent explanation:
   `inputState` is a downstream record Profile_Manager_obj keeps for its own
   bookkeeping (gamepad/keyboard normalization), not a live input source
   anything else - including, plausibly, `keyboard_check_pressed` itself,
   which is a native builtin and not obviously wired to read a plain GML
   array - consults. This falsifies §3a's original premise for `inputState`
   specifically. B4b-i (a GML-array poke) is not dead - see the `pressedArray`
   lead below - but `inputState` alone is ruled out as the mechanism.

   **`pressedArray`/`mousePressedArray` checked and ruled out, MEASURED
   2026-09-10, same live session.** Armed `citrace 1` (full trace, all 34
   hooks) and captured `Hook_Ci_KeyboardCheckPressed`'s unfiltered `ALLVARS`
   dump at the exact instant of a real F press (`keyboard_check_pressed #8253
   self=Profile_Manager_obj key=70 result=1`) - this is a precise, same-frame
   capture, not human-reaction-time gated like `snap1`/`snap2`, so it doesn't
   have that technique's risk of missing a transient flag. Result:
   `pressedArray=array[3]={-1,-1,-1}` at that exact moment - unchanged from
   its rest state, and (from other lines in the same session's log)
   `mousePressedArray=array[3]={666,-1,-1}` - both 3 elements, both shaped
   like mouse-button state (left/middle/right), not a 78-slot keycode array.
   Neither reacts to F at all. `inputState` remains the only array on
   `Profile_Manager_obj` that is both keycode-shaped (78 slots, one per
   `key_bind`/scancode) and confirmed to move on a real F press.

   **Revised conclusion for item 2.** `inputState` is very likely a
   *downstream* record Profile_Manager_obj keeps for its own bookkeeping
   (gamepad/keyboard normalization into `inputToGP`/`inputToKB`, both also
   present on this object as 78-entry array-of-arrays remap tables - config-
   shaped, not live state), not something anything - `keyboard_check_pressed`
   very much included, since it is a native builtin with no obvious reason to
   read back a GML instance variable - actually consults to decide "was F
   pressed." No other candidate array was found among Profile_Manager_obj's
   ~50 instance variables in this dump. This means **B4b-i-for-keys (a plain
   GML-array poke) does not currently have a known viable target** - the
   same category of result §3b already anticipated for `mouse_x`/`mouse_y`,
   now also true for the key-press half §3a had hoped would be simpler.
   Not yet tried: whether the raw engine-level key-down state
   `keyboard_check_pressed` actually reads has any GML-level handle at all
   (unlikely, symmetric with §3b's `mouse_x`/`mouse_y` finding), or whether a
   native memory location for it can be found the way the original session
   found `HhDrawHeadLabels`'s screen-projection math - open-ended, not
   pre-built, a deliberate stopping point pending the tester's direction
   rather than more unscoped live digging (`agents.md`'s "limit rebuilds and
   reruns" / checkpoint-before-open-ended-work guidance).
3. **Locate (or rule out) direct mouse-position storage (B4b-i). MEASURED
   2026-09-10, live session - NEGATIVE, and it rules the sub-strategy out
   rather than leaving it open.** Three separate findings:

   a. **`mouse_x`/`mouse_y` have no runtime handle at all.**
   `variable_instance_get` on the resolved player instance returns
   `undefined` for both, and `get mouse_x`/`get mouse_y` (the pre-existing
   global-variable reader) answers "does not exist". They are compile-time
   engine built-ins with no entry in either the instance variable map or the
   global table - so §5 item 3's own stated technique ("compare `mouse_x`/
   `mouse_y` read via `variable_instance_get` against real cursor movement")
   cannot be performed as written, and `citrace mousewrite`, which was built
   to write exactly those two names, has nothing to write to. §3b's guess
   that this "may be plain engine-internal state with no GML-level handle at
   all" is now measured rather than assumed.

   b. **`Profile_Manager_obj.mouse_x_prev`/`mouse_y_prev` DO track the real
   cursor, 1:1, in screen/GUI-space** - the only GML-reachable cursor handle
   found anywhere in this investigation. Confirmed by two readings at known
   cursor positions: real cursor at the top-left corner read
   `(30, 32)`; moved to the bottom-right corner it read `(2673, 1736)`.
   (Screen-space, not world-space: the values span the window, and did not
   shift with the camera.)

   c. **...but writing them does nothing, for the same reason `inputState`
   does nothing.** `oset Profile_Manager_obj mouse_x_prev 500` (plus the same
   for `mouse_y_prev`) lands - a read-back in the same command batch shows
   `500` - but the value is overwritten from the real device within a frame or
   two; three seconds later it read the true cursor position again, unchanged,
   with the real mouse never having moved. Forcing it repeatedly (~10x/second
   for ~4 seconds, so the fake position was being reasserted continuously
   rather than surviving a single frame) produced **no visible in-game effect
   whatsoever** - no hover highlight, no tooltip, no cursor artifact anywhere
   near the faked coordinates, confirmed by the tester watching for it.

   `mouse_x_prev` is, as its name suggests, Profile_Manager_obj's own record
   of where the cursor was - written *from* the real device for that object's
   movement-detection bookkeeping (`disableMouseMove`, `RefreshMouseMove` and
   friends are on the same object), and read by nothing that decides what the
   cursor is hovering. Exactly the same downstream-record trap as `inputState`
   in item 2.
4. **Test the full sequence on one item.** Moot - items 2 and 3 between them
   leave no confirmed way to fake either half of the interaction through GML,
   so there is no sequence left to test. Not attempted.

**Exit criterion: reached, on the "B4b-i is unreachable" branch.** The plan's
§5 exit criterion offers three outcomes; this is the middle one, arrived at
with live evidence on both halves rather than by running out of ideas.

## Conclusion (2026-09-10, live session with the tester)

**B4b-i - faking either half of the interaction by poking GML-visible state -
is ruled out, on both halves, for symmetric and well-understood reasons.**

| Half | GML-visible candidate | Tracks real input? | Writable? | Any effect when written? |
| --- | --- | --- | --- | --- |
| Key press (§3a) | `Profile_Manager_obj.inputState[30]`, `[60]` | Yes, reliably | Yes | **No** - single, both, one frame or held |
| Key press (§3a) | `pressedArray`, `mousePressedArray` | No - mouse buttons, 3 slots | - | - |
| Hover (§3b) | `mouse_x` / `mouse_y` | n/a - **no runtime handle at all** | No | - |
| Hover (§3b) | `Profile_Manager_obj.mouse_x_prev`/`_y_prev` | Yes, 1:1, screen-space | Yes | **No** - even forced ~10x/s for 4s |

The pattern is the same on both halves, and it is the thing that actually
matters: **everything reachable from GML is a downstream record of real
input, not the state anything reads to decide what happened.** The game's own
interact and hover checks consult engine-internal input state
(`keyboard_check_pressed` is a native builtin; `mouse_x`/`mouse_y` are
compile-time built-ins with no variable-table entry), and
`Profile_Manager_obj` mirrors that state into its own instance variables
afterwards for its own bookkeeping. Writing the mirror changes nothing,
which is exactly what was observed, twice, for two independent subsystems.

This falsifies the specific premise that made B4 look more promising than the
original plan's B3 - §3a's "a plugin can flip one element without ever
touching the real keyboard, so **half of B3's objection may not apply at
all**". Measured: it does apply. Both halves now sit behind the same wall
B3's objection described.

### What is left, if this mod is picked up again

1. **B4b-ii, now for both halves** (OS-level `SendInput` for the cursor move
   *and* the keypress, not just the cursor as §3b assumed). This is the full
   original Plan B3 that `pet-quest-collector-plan.md` §4 rejected outright,
   with every downside it named - fights the player for the cursor *and* the
   keyboard, frame-rate dependent, misfires during combat - and now twice the
   surface area. If it ships at all it should be opt-in and clearly labeled,
   per B4's own §5 exit criterion.
2. **Native (non-GML) input-state writes** - locate the engine's own raw
   key-down and cursor buffers in `Hero_Siege.exe` and write them from the
   plugin's native code, bypassing GML entirely. Unscoped, Ghidra-class
   effort in the same category where the original B1/B2 investigation
   deliberately stopped (see `pet-quest-collector-research.md`'s "Native
   decompilation" and its documented misread-risk), and with the same
   consequence if a read is wrong.
3. **Re-scope the mod**, as `pet-quest-collector-plan.md` §11's first risk row
   has called for since the B1/B2 result.

Nothing in the shipped build is affected either way: `petquest` remains a
read-only counting tick, off by default, and every tool built for this
investigation is dev-build-only.

### Successor: Plan C

`ForgePact/docs/pet-quest-collector-plan-c-direct-invocation.md` (drafted
2026-09-10; Phase C0 tooling implemented 2026-09-11, live round pending - see
`ForgePact/docs/pet-quest-collector-c-research.md`) picks this up from a
different angle: every plan so
far assumed the code had to be reached *by name*, and all three died on that
assumption. Plan C's Phase C0 goes after the two name-free doors nobody has
tried - **invoking the `m_Quest*` bound methods sitting on the item instance
directly** (the resolver for these was built in session 7 and, checked against
the full log history, has never once been run) and **`event_perform` by
numeric event id**, which reaches exactly the object-event code session 7
proved unreachable by name. Only if all of C0 fails does it go to Ghidra, and
it does so with a new anchor the previous attempt lacked: a stack walk from
inside the already-firing `keyboard_check_pressed(70)` hook.
