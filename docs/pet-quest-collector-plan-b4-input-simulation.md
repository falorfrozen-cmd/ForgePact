# Pet Quest Collector — Plan B4: simulate the player's own interaction

Status (2026-09-10): **Phase 0 complete, live. B4b-i is disproven on both
halves - this plan's central premise does not hold.** Companion to
`ForgePact/docs/pet-quest-collector-plan.md` (the original plan, whose B1/B2
are conclusively blocked/disproven - see
`ForgePact/docs/pet-quest-collector-research.md`'s "Final status"). Target
branch `release/v1.3.17`, same as the original plan.

§5's Phase 0 research commands (`citrace pokekey`/`pokekey2`, `citrace
mouse` - `citrace snap1`/`snap2` already existed and needed no changes) are
implemented, dev-build-only, and were run live with the tester. Results, in
full detail in `ForgePact/docs/pet-quest-collector-b4-research.md`:

- **Item 1 succeeded:** a real F press flips `Profile_Manager_obj.inputState`
  indices **30 and 60** together, `1 -> 0` (the opposite direction from §3a's
  assumption).
- **Item 2 is negative:** poking those indices - either alone, or both in the
  same frame - produces no in-game reaction at all. `pressedArray` /
  `mousePressedArray` turned out to be unrelated 3-slot mouse-button state.
- **Item 3 is negative:** `mouse_x`/`mouse_y` have no runtime variable-table
  entry at all (neither instance nor global), and
  `mouse_x_prev`/`mouse_y_prev`, which *do* track the cursor 1:1 in screen
  space and *are* writable, are overwritten from the real device every frame
  and had no visible effect even when forced ~10x/second for four seconds.
- **Item 4 is moot:** no confirmed way to fake either half, so no sequence to
  test.

The common cause: everything reachable from GML is a *downstream record* of
real input, not the state the game reads. §3a's hope that "half of B3's
objection may not apply at all" is measured false - it applies to both
halves. What remains (OS-level injection for both halves, i.e. full Plan B3;
or native non-GML input-buffer writes; or re-scoping the mod) is set out in
the research doc's Conclusion. Phase 1+ (§6) is not started and no longer has
a mechanism to be built on.

**Superseded by Plan C**
(`ForgePact/docs/pet-quest-collector-plan-c-direct-invocation.md`, drafted
2026-09-10; Phase C0 tooling implemented 2026-09-11, live round pending - see
`ForgePact/docs/pet-quest-collector-c-research.md`): rather than simulate the
player's input at all, invoke the quest
item's own `m_Quest*` bound methods directly and/or drive its events with
`event_perform` - both name-free, and so unaffected by the named-routine-table
wall that closed B1/B2 - with Ghidra as the explicit fallback if that fails.

---

## 1. The idea

Every approach tried under the original plan (B1: call whatever the
interaction invokes on success; B2: hook the interaction gate and lie to it)
required *finding* the game's internal collect mechanism. 34 hooked call
sites across 13 live sessions and a first pass at native decompilation
(Ghidra) never found it - see the research doc's "Final status".

B4 sidesteps that entirely: **don't find the mechanism - reproduce the two
conditions a real player produces, and let the game's own logic (whichever
unidentified path it runs through) do the rest.**

A real collect happens when, on the same frame (or a short window):
1. The player is hovering the mouse over the quest item, and
2. The interact key (F) reads as freshly pressed.

If a pet-controlled system can make both of those true for a specific item -
without needing to know how the game responds to them - the existing,
already-correct, already-tested game logic handles hover validation, the
accepted-quest gate, progress crediting, and removal, exactly as it does for
a real player. This is not a new mechanism to discover; it is **synthetic
input**, aimed at the same targets a mouse and keyboard would be.

### Why this used to be rejected, and why it's being reconsidered now

The original plan named this "Plan B3" (§4) and rejected it outright:
> "Synthesising real mouse movement and keypresses... is rejected: it fights
> the player for the cursor, is frame-rate dependent, and would be unusable
> while the player is doing anything else."

That objection is still correct for *moving the real OS cursor*. What's
different now is concrete, measured information Plan B3 didn't have:
- The interact-key check reads from a plain, writable GML array
  (`Profile_Manager_obj.inputState`, confirmed live, `array[78]`), not raw OS
  keyboard state - a plugin can flip one element without ever touching the
  real keyboard, so **half of B3's objection (fighting the player for input)
  may not apply to the key-press half at all.**
- The screen-bounds/camera-transform math needed to place a fake cursor over
  a specific world-space item already exists (`HhDrawHeadLabels`'s
  world→GUI projection, reusable).
- The plan's own accepted-quest-gate research question (Phase 0 item 4) is
  moot under B4: the game's real gate runs unchanged, so nothing has to be
  replicated.

The mouse-position half is the part still closest to the original B3
objection - see §3.

---

## 2. Decisions carried over from the original plan

Unchanged from `pet-quest-collector-plan.md` §1: gate on accepted quests
(free under B4, not something to implement), **the player gets credit**
(automatic under B4 - the interaction is genuinely the player's), screen-wide
collection radius (unchanged - `PetQuestCollectorTick`'s existing camera-view
enumeration already finds "closest visible collectible item"; that part of
Phase 2 from the original plan needs no rework). Off by default, matching
every other mod toggle.

---

## 3. The two halves, separately

### 3a. Faking "F was just pressed" - low risk, cheaply testable

**Hypothesis:** one specific index of `Profile_Manager_obj.inputState`
(seen flip in two prior sessions, noisily - `citrace snap2` diffs showed
index 30, and separately 30+60 in a longer, contaminated window) is the F
key's tracked state. A clean, isolated single-press diff (`citrace snap1`
immediately before a press, `citrace snap2` immediately after, no other
input in between - the room-change guard already built this session
protects the timing) would very likely confirm the exact index in one round.

**Once known:** write that index to `1` for exactly one frame from
`PetQuestCollectorTick` (or a dedicated tick) via `variable_instance_set` on
the resolved `Profile_Manager_obj` instance, the same mechanism already used
throughout this session's research tooling (`variable_instance_set` is
already a confirmed-safe, already-hooked-and-measured builtin). Whether this
needs an explicit restore or self-heals next frame (the array is plausibly
refreshed from the real input device every frame regardless of our poke) is
itself a one-line thing to observe live, not a design risk.

**This half, if confirmed, has none of Plan B3's downsides**: no OS input
synthesized, nothing visible to the player, no shared input state fought
over.

### 3b. Faking "hovering the item" - the part that inherits B3's risk

Two sub-strategies, in order of preference:

**B4b-i (direct-state poke, preferred, unconfirmed):** find whatever memory
location the game reads for mouse position during its (still unidentified)
hover check, and write to it directly for the same one-frame window as the
key-press poke, bypassing GML's normal read-only restriction on
`mouse_x`/`mouse_y` the way a native plugin can bypass any GML-level
restriction. Avoids OS cursor movement entirely - if it works, B4 has *none*
of B3's rejected downsides. Not yet confirmed reachable: unlike `inputState`
(a whole GML array, directly writable through ordinary `variable_instance_set`
on a resolved instance), `mouse_x`/`mouse_y`'s storage is not yet known to be
directly writable this way - it may be plain engine-internal state with no
GML-level handle at all, closer in kind to the "no separate script-table
entry" problem this session hit repeatedly. Locating it is a **narrower**
search than the collect mechanism, though: a single value that changes 1:1
with real OS cursor movement, discoverable by moving the mouse and watching
what changes, rather than a whole subsystem's dispatch logic. Still real,
unbounded-until-tried research effort - budget accordingly (see §5).

**B4b-ii (real OS cursor move, fallback, exactly Plan B3):** `display_mouse_set(x,y)`-equivalent
or raw `SendInput`, moving the actual OS cursor to the item's screen
position for one frame, then back. Inherits the original rejection reasons
in full:
- Visibly yanks the player's cursor, however briefly - jarring even if
  harmless.
- A single wrong-moment frame during active aiming/movement-click could
  misfire a real game action in the wrong direction (Hero Siege is
  real-time; the player's mouse routinely drives aim/movement).
- Frame-rate dependent timing (the "move, poke, restore" sequence needs to
  land within one frame's window reliably).

If B4b-i turns out unreachable, B4b-ii is still strictly better than doing
nothing, but should ship (if at all) opt-in and clearly labeled, with the
UX risk stated plainly to whoever enables it - this is a materially
different risk profile than every other toggle in the Mods tab, none of
which touch the player's real input devices.

---

## 4. Open question: timing / continuity

Real hovering is not instantaneous - the player's cursor is already over the
item for some number of frames before they press F. Whether the game's
(still unidentified) hover check requires that continuity, or re-evaluates
fresh every frame from raw position alone, is unknown. Safest design: hold
the faked hover state (whichever half-3b strategy is used) for a handful of
frames *before* injecting the faked key-press, rather than setting both on
the same frame - cheap to build in regardless of which hover strategy is
chosen, and removes one variable from the first live test.

---

## 5. Phase 0 — research (dev build, live session required)

Same tooling posture as the original plan's Phase 0: `citrace`-style
research commands in the dev build, findings logged to
`ForgePact/docs/pet-quest-collector-b4-research.md` (created 2026-09-10;
measured behavior and our own code only - no decompiled script text, per
`agents.md`). That doc records what the tooling below does and how to run
it; none of the four items has been run live yet.

1. **Confirm the `inputState` F-index precisely.** One clean, isolated
   `citrace snap1`/press F/`citrace snap2` round, nothing else happening in
   between. Exit signal: the exact index, confirmed reproducible.
2. **Confirm a poke to that index is sufficient.** Arm a tick that writes
   `1` to the confirmed index without any real key event, and check whether
   `keyboard_check_pressed(70)`-driven behavior (visible in-game: does the
   interact prompt / the item respond?) fires. This is the single load-bearing
   test for all of §3a - a clean pass or fail, decidable in one live round.
3. **Locate (or rule out) direct mouse-position storage (B4b-i).** Compare
   `mouse_x`/`mouse_y` read via `variable_instance_get` against real cursor
   movement to confirm the correlation, then attempt to find the backing
   storage a native plugin could write directly (scope: much narrower than
   the collect-mechanism hunt, but still open-ended - timebox it, and fall
   back to B4b-ii's OS-cursor approach if it doesn't resolve quickly).
4. **Test the full sequence on one item**, end to end, starting with B4b-ii
   (OS cursor move) if B4b-i isn't ready in time, to get one full proof of
   concept before deciding which hover strategy to keep.

**Exit criterion:** either poke-based hover (B4b-i) works, in which case B4
is a clean, low-risk implementation with none of Plan B3's downsides; or only
OS-cursor-move (B4b-ii) works, in which case implementation should be gated
behind an explicit, clearly-labeled opt-in given the UX/safety tradeoff; or
neither half works, in which case B4 joins B1/B2 as blocked and this mod
needs the re-scoping the original plan's §11 already calls for.

---

## 6. Phase 1+ — implementation (not started, blocked on Phase 0)

Sketch only, pending Phase 0's outcome:
- Reuse `PetQuestCollectorTick`'s existing screen-view enumeration
  (`ForgePact/plugin/include/ForgePact/PetQuestCollectorMod.hpp`,
  `PetQuestCollectorTick()` in `ModuleMain.cpp`) to find the nearest visible
  collectible item - already built, already tested, needs no rework.
- Add the confirmed hover-fake (whichever half-3b strategy Phase 0 selects)
  and the confirmed key-press-fake (§3a), sequenced per §4's continuity
  finding.
- Pace it the same way the original plan's Phase 2 step 5 already specifies
  (at most one item per N frames) - unchanged rationale (readability,
  avoiding tripping scripted quest state).
- Phase 3 (pet movement, cosmetic) from the original plan is unaffected by
  which mechanism triggers the collect and can be built independently.

---

## 7. Risks

| Risk | Mitigation |
| --- | --- |
| Faked hover doesn't match what the real check requires (screen vs. world coords, single-frame vs. continuity) | Phase 0.3/0.4 test end-to-end on one item before any broader implementation; §4's continuity hold as the safer default. |
| B4b-ii (OS cursor move) misfires a real game action during combat | Ship opt-in only if B4b-i is unreachable; state the risk plainly in the panel copy; consider gating on "no enemies nearby" as a cheap partial mitigation. |
| `inputState` poke doesn't self-heal and leaves a stuck "F held" state | Phase 0.2 explicitly checks this; add an explicit one-frame restore if it doesn't self-heal. |
| Legal / `agents.md` | Same posture as the rest of this mod's research: measured behavior and our own code only, no decompiled script text, in any tracked file. |

---

## 8. Files touched (once implementation starts)

New (created 2026-09-10, Phase 0 tooling only):
- `ForgePact/docs/pet-quest-collector-b4-research.md`

Modified this session (Phase 0 tooling only - not the collect mechanism
itself, which stays blocked on live results):
- `ForgePact/plugin/ModuleMain.cpp` - `citrace pokekey`/`citrace mouse`/
  `citrace mousewrite`, plus a pre-existing release-build compile gap around
  `CiSnapTake`/`CiSnapDiff` found and fixed along the way (see the research
  doc's "Build note"). `PetQuestCollectorMod.hpp`/`PetQuestCollectorTick`
  untouched - still no collect call, per Phase 1+'s gate below.

Still to modify, once Phase 0's live rounds pick a strategy (Phase 1+, §6):
- `ForgePact/plugin/include/ForgePact/PetQuestCollectorMod.hpp`
- `ForgePact/plugin/ModuleMain.cpp` (`PetQuestCollectorTick`, plus the new
  hover/key-press fake once Phase 0 selects a strategy)
- `ForgePact/tests/test_pet_quest_collector_contract.py`
- `ForgePact/docs/pet-quest-collector-plan.md`,
  `ForgePact/docs/pet-quest-collector-research.md` (cross-links to this
  document)
