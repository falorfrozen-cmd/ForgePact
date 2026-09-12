# Menu Pause — implementation plan

Status (2026-09-11): **planned and NOT RECOMMENDED — do not build this without
a deliberate decision to accept §0's risks.** The design below is complete and
the research behind it is real, but the conclusion of writing it is that this
class of feature does not belong in ForgePact. Nothing here has been measured
in-game: every fact marked "static" comes from `hs-game-sdk`'s object/script
tables and from this repository's own code, and every fact the mechanism
actually depends on is a Phase 0 measurement that has not been taken. Kept as
the record of what it would take, and of why the answer is no. Target branch
would have been `release/v1.3.17`, shipping as 1.3.19.

---

## 0. Recommendation: don't build this

Pausing the game from outside it means suspending the game's own runtime state
and then restoring it exactly. Every other ForgePact feature reads the game's
state or nudges one value inside a call the game itself is making; this one
takes the world away from the game and promises to give it back unchanged.
That is a different category of risk, and five specific things make it a bad
trade here:

1. **The failure mode is a soft-lock, not a no-op.** Every other feature in
   this plugin fails by doing nothing — a dead `relicgate`, a collect that does
   not fire. This one fails by leaving the player's world frozen, or the player
   instance deactivated while they press "Save and Exit". §5.5's fail-open
   rules, the F4 panic thaw and the watchdog exist precisely because the
   ordinary failure is unacceptable, and safety machinery that elaborate is
   itself a signal that the feature is fighting the platform.
2. **The two mechanisms that would make it safe are both unavailable on this
   build.** YYToolkit's per-event hook (`EVENT_OBJECT_CALL`) is deliberately
   disabled here because it crash-looped on Season 10, and named-script hooks
   are structurally blind against this YYC build's direct calls (§2,
   constraints 1-2). What is left — deactivating instances wholesale — is the
   crudest of the three options, and the one with the widest blast radius.
3. **It cannot be verified the way this toolkit verifies things.** "All timers
   freeze" is a claim about every timer in the game, including the ones nobody
   has enumerated. The contract tests of §8 can pin the mod's own structure,
   but they cannot establish that claim; only open-ended live play can, and a
   miss shows up as a buff that quietly expired or a cooldown that quietly
   advanced — the kind of wrongness a player reports as "the mod broke my
   character", months later.
4. **It carries a permanent maintenance tax.** Detection rests on a curated
   list of the game's UI objects (§5.1). Every game patch that adds a window
   adds a menu that does not pause, and every patch that renumbers assets
   invalidates the list. That is upkeep on someone else's release schedule, for
   a convenience feature.
5. **It is visibly wrong even when it works.** With Mechanism A the world's
   characters vanish while the menu is open (§7). The honest version of this
   mod needs the snapshot blit, which is more surface area again — a captured
   surface, a draw hook, and a lifetime to manage — on top of everything above.

The general form of this, and the reason it is worth writing down rather than
re-deriving: **do not add features that suspend or take over the game's own
runtime loop — pause, time scaling, save-state/rewind, forced state restore.**
Prefer features that read game state, or that change one value inside a call
the game is already making. See `agents.md`, "Don't Suspend the Game's Own
Runtime".

If the pause is wanted anyway, the cheapest honest alternatives are outside the
plugin entirely: play windowed and let the OS do it, or accept that a menu does
not stop the world (which is what the game itself decided). If it is still
wanted *inside* the plugin after that, Phase 0 (§6) is the right first step and
it costs one session and no rebuild — take its measurements before writing any
of Phase 2.

---

## 1. What the mod does

**Menu Pause**: while a menu that covers the play field is on screen, the world
stops. Close the menu and it resumes exactly where it was.

In scope, from the request:

| Frozen | Meaning |
| --- | --- |
| Monsters | no movement, no attacks, no spell casts, no spawn/aggro progress |
| Player | no movement, no attacks, no ability use |
| Mercenary (companion) | same |
| Pet (`Companion_obj`) | same |
| Damage | nothing resolves — no contact damage, no projectile hits, no DoT ticks |
| Timers | buff and debuff durations, ability cooldowns, internal per-entity timers, projectile lifetimes |
| **Not** frozen | the menu itself: it draws, animates and responds to mouse and keyboard exactly as now |

Out of scope for 1.3.19 (say so in the release notes rather than half-doing
them): the Chaos Tower run clock and other **zone-level** timers owned by
manager objects (§5.3 keeps the hook point but defaults it off), online play of
any kind, and any pause the player triggers by hand rather than by opening a
menu.

Offline single-player only, like the rest of ForgePact. The mod must refuse to
arm when the session is not offline — a frozen local world in a networked
session is desync, not a pause.

---

## 2. The constraints that shape everything

These are already-established facts about this game build and this plugin, not
assumptions. They rule out the three obvious implementations before any code is
written.

1. **There is no per-event hook to turn off.** YYToolkit's `EVENT_OBJECT_CALL`
   (the `Code_Execute()` callback) is *deliberately disabled* in the modified
   YYToolkit this project ships — it crash-looped and corrupted instance
   lookups on Season 10 (`yytoolkit-modified/NOTICE.md`). The plugin gets
   `EVENT_FRAME` (a `Present()` hook) and nothing finer. So "intercept every
   step event and skip it" is not available.
2. **Named-script hooks are structurally blind here.** `HookOneScript` swaps a
   pointer inside the script-table entry, and this YYC build's compiled GML
   calls another script with a direct `call rel32` bound at compile time that
   never reads that table (`docs/pet-quest-collector-c-research.md`, "The hooks
   were blind"). Hooks on the *step funnels* (`EnemyParentBeginStepMain`,
   `ClassStepEvent`, …) may therefore do nothing at all, silently. Where a hook
   is needed, it must be `MmCreateHook` on the address resolved **by name**
   through the script table (`citrace nativetrace`'s route), and it must be
   proved with a positive control before any "it isn't called" conclusion.
3. **No global time scale exists in the game to turn down.** Static search over
   all 6,254 script names finds zero `delta`, `timescale`, `gamespeed` or
   `pause`-shaped entries — the only `Pause` hits are `UI_Pause_obj`'s own
   drawing helpers and its Create-event closures. Whatever ticks, ticks in
   ordinary step events. `game_set_speed` is not an option either: it would
   slow the menu and the renderer with everything else.
4. **No hand-resolved addresses.** `agents.md`, "Never Call an Address You
   Resolved by Hand", and `tests/test_release_hook_contract.py` enforce it.
   Everything here resolves by name (`asset_get_index`, `CallBuiltin`,
   `GetNamedRoutinePointer`).
5. **A bug in this mod hard-locks the player's session.** Every other ForgePact
   feature fails by doing nothing. This one can leave a world permanently
   frozen, or leave the player deactivated while they press "Save and Exit".
   Fail-open behaviour (§5.5) is a requirement, not polish.

---

## 3. What the static search already gives us

Run before any live session, per `agents.md` ("Limit Rebuilds & Reruns"). All
indices from `hs-game-sdk/data/objects.json` (Season 10 tables).

### 3.1 The actors are reachable through a handful of parents

GameMaker counts and addresses a parent object as the whole subtree, so the
entire world can be named in ~12 asset indices:

| Object | Index | Descendants | Covers |
| --- | ---: | ---: | --- |
| `Enemy_Parent_obj` | 1429 | 336 | every monster |
| `Player_obj` | 3553 | 0 | the player |
| `Mercenary_obj` | 2696 | 0 | the hired companion |
| `Companion_obj` | 977 | 0 | the pet (the same object the Pet Quest Collector drives) |
| `Summon_Parent_obj` | 4742 | 37 | summons, incl. `Necro_Summon_Parent_obj` (3034) |
| `Player_Damage_Parent_obj` | 3543 | 653 | everything the player/mercenary throws that can hit |
| `Enemy_Damage_Parent_obj` | 1416 | 165 | everything monsters throw |
| `Player_Ability_Parent_obj` | 3536 | 239 | player ability objects |
| `Enemy_Ability_Parent_obj` | 1400 | 41 | monster ability objects |
| `Chain_Lightning_Parent_obj` | 856 | 44 | travelling chain effects |
| `Orbit_Parent_obj` | 3332 | 25 | orbiting effects |
| `Player_Sentry_Parent_obj` / `Shaman_Totem_Parent_obj` | 3557 / 4443 | 21 / 15 | placed turrets/totems |
| `Pickup_Parent_obj` | 3421 | 110 | ground loot (has its own despawn timers) |

Useful hierarchy fact: `Enemy_Aggroable_obj` (1401) is the parent of exactly
`Player_obj`, `Mercenary_obj` and `Summon_Parent_obj` — the "friendly actors"
subtree, 40 descendants, one index.

### 3.2 Buff and debuff timers are *instances*

`ModuleMain.cpp`'s Headhunter code already reads
`global.playerBuff[player][0][buffId]` and finds a **`Draw_Player_Buff_obj`
instance** (1362) there — `HhBuffAlive` tests it with `instance_exists`. There
is a matching `Draw_Enemy_Buff_obj` (1361). That is the single most useful
finding for this mod: buff/debuff duration is almost certainly counted down in
those instances' own events, which means **freezing instances freezes buff
timers** — no separate mechanism needed, and no reaching into a timer field
whose layout we would have to guess.

It also produces the sharpest Phase 0 test available (§6, item 5): take a
Headhunter 20-second buff, pause 60 seconds, and read the remaining time.

### 3.3 "A menu is displayed" is already a hierarchy question

`UI_Parent_obj` (5205) has 199 descendants and 178 direct children, and reading
the list shows it is *almost exactly* the set of blocking windows —
`UI_Pause_obj` (5206), `UI_Options_*`, `UI_Inventory_Parent_obj` (5115),
`UI_Talent_Screen_obj`, `UI_Journal_*`, `UI_Map_Screen_obj`, `UI_Stash_*`,
`UI_Yes_No_Prompt_obj`, and so on.

The exceptions — the objects under `UI_Parent_obj` that are HUD or ambient, not
a menu — are few, and they are what a curated list has to carve out:

`DPS_Meter_obj` (1357), `Boss_Cooldown_Meter_obj` (684), `UI_Potion_Floating_obj`
(5213), `UI_Hud_Talent_obj` (5099), `UI_Ingame_Chat_obj` (5107),
`Joystickman_obj` (2318, mobile stick), `objSteveFighter`, `UI_Dropdown_Parent_obj`
(5067), `UI_Ready_Parent_obj` (5227), `UI_Circle_Menu_obj` (5043, the radial
emote wheel — a menu, but it does not cover the screen), plus the login/loading
screens, which are not in-world situations at all.

Two more that need a decision rather than a default: `UI_You_Died_obj` (5286)
and `UI_Error_Prompt_obj` / `UI_Network_Error_Prompt_obj`.

### 3.4 The step funnels exist, if the fallback mechanism is needed

Static search finds the named entry points a per-system freeze would target:
`EnemyParentBeginStepMain`, `enemyParentBeginStepFunc`, `EnemyStepHandlerFunc`,
`EnemyChildStepGlobalTimers`, `ClassStepEvent`, `SummonStepLoop` (+ four
siblings), `stepMercenaryTimers`, `stepMercenaryGetNearestEnemy`,
`ProjectileStepFuncs`, `ProjectileRunStepFuncs`, `playerAbilityParentStep`,
`StepAbilityParent*` (5), `LootGroundActiveStep`, `npcParentStepEvent`,
`PlayerBuffFuncs`, `EnemyBuffFuncs`. Recorded here so Mechanism B (§4) starts
from the full list rather than a guess — but see constraint 2 before trusting
any of them.

---

## 4. Mechanism: one preferred, one fallback, one rejected

### Mechanism A — instance deactivation (preferred)

The GameMaker-native pause. On menu open, deactivate every instance in the
world set (§3.1); on menu close, reactivate exactly those. A deactivated
instance runs no events at all: no step, no alarm, no collision, no timer. That
is every requirement in §1 satisfied by one primitive, with **zero hooks into
game code** — which matters, because constraint 2 says hooks into game code are
exactly what cannot be trusted here.

`instance_deactivate_object` / `instance_activate_object` are ordinary
builtins, so they go through `CallBuiltin`: name-resolved, no addresses, no
struct layouts. The research build can already call them today with the
existing `cb` command — which is why Phase 0 needs **no rebuild at all**.

Two known costs, both handled:

- **Deactivated instances are not drawn.** The world's entities disappear while
  the menu is open. §7 covers this; the mod is usable without solving it.
- **`instance_exists` returns false for a deactivated instance, and `with()`
  skips it.** Any menu that reads the player this way would misbehave. This is
  the single biggest open question and it is Phase 0's first job.

### Mechanism B — native hooks on the step funnels (fallback)

If Phase 0 shows deactivating the player breaks the menus (or, worse, breaks
the *closing* of a menu), keep Mechanism A for everything else and freeze the
player — and only the player — by early-returning `ClassStepEvent` and the
player ability/damage step funnels while paused, hooked with `MmCreateHook` on
the name-resolved address. Same for any other actor that turns out to be
undeactivatable.

This is the fallback and not the default because it needs a positive control
per hook, it leaves per-object event code (`gml_Object_*_Step_0`, reachable
only through `HookRawNamedRoutine`) unaddressed, and "which function ticks this
particular timer" is a research project of unknown size, whereas deactivation
is one call per subtree.

### Mechanism C — per-entity neutralisation (rejected, recorded so it is not re-proposed)

Writing `speed = 0` / `path_speed = 0` / `image_speed = 0` onto every actor
each frame stops *motion* but not step code, so cooldowns, buff durations, DoT
ticks and attack windups all keep running. It cannot satisfy "all timers
freeze" in principle, not just in practice. It is also the shape most likely to
leave the world subtly wrong on resume (half-written state on hundreds of
instances).

---

## 5. Design

### 5.1 Detection

Per frame, one cheap gate, then a confirm:

1. **Gate**: `instance_number(UI_Parent_obj)` — one builtin call, counts the
   whole subtree.
2. If the count exceeds the ambient baseline (the HUD objects of §3.3 that are
   legitimately present in-world), **confirm** by walking the curated blocking
   set with `instance_number` per entry, and take the first hit.

The curated set lives where the repo already keeps curated SDK data:
`hs-game-sdk/curated/menu_pause.json` (alongside `satanic_zone.json`), with
both lists — `blocking` (opt-in menu objects) and `ambient` (never counts) —
and a contract test that re-derives it against
`hs-game-sdk`'s real hierarchy so a future SDK regeneration cannot silently
widen it. Same guardrail as the Pet Quest Collector's target set.

Deliberately **not** hooking the `UiActivate*` openers
(`UiActivateInventory`, `UiActivateOptions`, `UiActivateMainMenu`, … — 16 of
them exist): constraint 2 says a table hook may never fire, and a missed open
means a menu that does not pause, while a missed *close* means a world that
never resumes. Polling cannot miss an edge in either direction.

An **allowlist, not a denylist**, is the default: an unknown new UI object must
mean "no pause", never "freeze the world".

### 5.2 Freeze and thaw

On the rising edge of "menu open":

1. Snapshot the frozen set: for each parent index in the world set, walk
   `instance_find(idx, n)` and record every instance id.
2. Deactivate **by instance id**, one call each, recording exactly what was
   deactivated.
3. Store the set and the room index.

On the falling edge (or any fail-open trigger):

1. Reactivate exactly the recorded ids, then clear the set.

**Never `instance_activate_all()`.** If the game culls off-screen instances
with `instance_deactivate_region` — unverified, and a Phase 0 item — a blanket
activate would wake everything the game had deliberately put to sleep. Only
what we deactivated gets reactivated, which is correct whether or not the game
culls.

Cost: one pass of `instance_find` + one `instance_deactivate_object` per
instance, at menu open and menu close only — not per frame. With high density
settings this is a few thousand `CallBuiltin`s in one frame; if that shows as a
visible hitch, spread it over frames the way the density queue already spreads
spawner creation, freezing the highest-value subtrees (enemies, damage objects)
first.

### 5.3 What stays running, on purpose

- Every `UI_*` instance — the menu must work.
- `Camera_obj` (755) and the renderer.
- Sound, input, `Loot_Manager_obj` (2514) and the game's own managers.
- Zone-level manager timers (Chaos Tower's run clock, spawner cadence). A
  `menupause managers 1` switch is reserved for this and **defaults off** in
  1.3.19: the manager objects also drive UI and saving, so freezing them is its
  own research item, not a free extra.

### 5.4 ForgePact's own ticks must pause too

`FrameCallback` keeps running (it is a `Present` hook), and several of its ticks
*write to the world*: `OrbPickupTick` pulls globes toward the player,
`PetQuestCollectorTick` walks the pet and collects, `HeadhunterActivityTick`,
`SatanicPollTick`, the density spawn queue, and `EstForceApply`. A paused world
that still has globes flying and the pet walking is not paused.

So the mod publishes one flag — `MenuPauseMod::Instance().IsFrozen()` — and each
of those ticks takes an early return on it. Two extra details:

- The Headhunter head labels count down in **wall-clock** time
  (`HhNowMs`), so while the buff instance is frozen the label would keep
  ticking and expire a buff that is still there. The label clock must have the
  paused duration added back to it (or be frozen the same way).
- `HhBuffAlive` tests `instance_exists` on the buff instance, which returns
  false while deactivated — a second reason those ticks must not run while
  frozen.

### 5.5 Fail-open safety

Non-negotiable, in priority order:

1. **Any exception, anywhere in the freeze/thaw path → thaw everything and
   disarm the mod for the session**, with one line in `out.txt` saying so.
2. **Room change while frozen → thaw first.** The plugin already hooks
   `ZoneStateResetAll` / `ZoneStateResetSingle` and already reads `global.room`
   each frame; either edge triggers a thaw. This is also what protects "Save and
   Exit" pressed from the pause menu.
3. **Watchdog**: frozen for more than N seconds (default 15 min) with no
   blocking menu instance found → thaw, log, disarm.
4. **Panic key**: a hotkey (F4, unused) thaws and disarms immediately,
   available in the player build, so a stuck world is recoverable without the
   panel.
5. **`menupause 0` thaws**, whatever state the detector thinks it is in.
6. **Refuse to arm at all** outside a real in-world room (character select, main
   menu, loading) and in any non-offline session.

### 5.6 Command surface

| Command | Build | Meaning |
| --- | --- | --- |
| `menupause 1` / `0` | player | arm / disarm (and thaw) |
| `menupause status` | player | armed?, frozen?, last trigger object, instances frozen, thaw reason |
| `menupause managers 1/0` | player | §5.3, default off |
| `menupause scan` | research | list every `UI_Parent_obj` descendant with a live instance right now — how the curated list gets built and re-checked |
| `menupause force 1/0` | research | freeze/thaw by hand, no detector |

`menupause` joins `kPlayerCommands` in `RunCommand`; `scan`/`force` stay behind
`#ifndef FORGEPACT_RELEASE`, and the contract test pins both facts.

---

## 6. Phase 0 — research (no rebuild, no new code)

Everything here runs against the **existing research build** through
`tools/ipc.ps1` and the `cb` command, which calls any builtin by name
(`CallBuiltinCmd`). This is the whole point of doing it first: the mechanism is
either viable or not before a line of C++ is written, at the cost of one game
session.

```powershell
.\ForgePact\tools\ipc.ps1 -Lines "cb instance_number 5205","cb asset_get_index Enemy_Parent_obj"
```

| # | Question | How | Exit criterion |
| ---: | --- | --- | --- |
| 1 | **Baseline**: what does the game do *today* with the ESC menu open? | Open ESC, watch a monster pack, a DoT, a buff timer, a cooldown | Written down as the "before" half of the before/after the workflow rule requires |
| 2 | Does a blocking menu create and destroy an instance? | `cb instance_number 5206` (`UI_Pause_obj`) with the menu open, then closed; repeat for inventory (5115) and options (5203) | Count goes 0 → ≥1 → 0. If a menu instead persists with `visible = false`, detection switches to reading `visible`/`active` and §5.1 changes |
| 3 | What is the ambient `UI_Parent_obj` count in-world with no menu open? | `cb instance_number 5205` while walking a zone, in town, in Chaos Tower | A stable baseline, and the `menupause scan` list to seed `menu_pause.json` |
| 4 | Does deactivating the world actually freeze it? | `cb instance_deactivate_object 1429` (enemies) mid-fight; observe; `cb instance_activate_object 1429` | Monsters stop dead and resume on reactivate, in position, with no error spam |
| 5 | **Do timers freeze?** | Take a Headhunter 20 s buff (`headhunter force`), `cb instance_deactivate_object 1362`, wait 60 s, reactivate, read the buff | Buff still present with ~its remaining time. This is the measurement the whole mod rests on — if buff duration is wall-clock rather than instance-counted, §3.2's assumption is wrong and buffs need their own mechanism |
| 6 | **Can the player be deactivated?** | `cb instance_deactivate_object 3553`, then: open and close each menu, click things, press ESC | Menus open, draw the right values, and **close**. A menu that cannot be closed with the player deactivated sends the player half to Mechanism B, and nothing else changes |
| 7 | Does the game cull with `instance_deactivate_region`? | Count enemies via `instance_find` walk vs the game's own count (`census` / `enemystats`) at distance | Answer recorded; §5.2's "never activate_all" stands either way |
| 8 | Does anything get upset by a missing player? | Leave the player deactivated 60 s, watch `out.txt` and the game | No error prompt, no respawn, no save corruption, no stuck state |
| 9 | Pet, mercenary, summons, loot | Repeat 4 for 977, 2696, 4742, 3421 | Each freezes and resumes cleanly, or is recorded as an exception |
| 10 | Cost of a full freeze | Time a whole-set deactivate at density x3 | Under one frame, or §5.2's spread-over-frames path is needed |

Write the results into `docs/menu-pause-research.md` as they are taken, and
label anything unobserved **"not observed"**, never "does not happen"
(`agents.md`, "Prove the Instrument Before Trusting a Negative Result"). A
positive control is easy here and must be used: item 4 is itself the control
for items 5-9 — if `instance_deactivate_object` visibly stops monsters, the
instrument works.

---

## 7. The visual question (decide after Phase 0 item 4)

Deactivated instances are not drawn, so with Mechanism A the world empties out
while the menu is open: background and tiles remain, monsters and the player
vanish, everything returns on close.

Three options, in order of preference:

1. **Ship it plain (1.3.19).** Document it in the release notes in one line
   ("while a menu is open the world is frozen and its characters are hidden").
   Functionally complete, zero risk.
2. **Snapshot blit (1.3.20).** On the freeze edge, capture
   `application_surface` into a sprite (`sprite_create_from_surface`, a
   builtin — name-resolved) and draw it over the empty world underneath the
   menu each frame while frozen, freeing it on thaw. The draw hook the head
   labels already use (`DrawHudBuffs`, which is confirmed to fire) is a known-good
   place to blit from, and the projection work for GUI vs world space is
   already written. The one ordering subtlety: capture must happen on the frame
   the menu opens but *before* the menu itself has drawn, or the snapshot
   contains a frame of menu.
3. Hybrid (freeze everything but leave the actors' draw running) — not
   possible in GameMaker; recorded so it is not re-proposed.

Option 1 is the plan of record. Option 2 only starts once the pause itself is
confirmed working in a real session.

---

## 8. Phase 1 — contract tests (before implementation)

Per `agents.md`'s before/after rule, and matching the existing suites'
style. New file `tests/test_menu_pause_contract.py`:

**Baseline (mod off):**
- `build_cmds` emits no `menupause` line when `mod_menu_pause` is false, and
  the default in `DEFAULTS` is false.
- No `instance_deactivate*` call site in `ModuleMain.cpp` is reachable without
  the mod's own enabled flag (source-level assertion, same technique
  `test_release_hook_contract.py` uses).

**Target (mod on):**
- `build_cmds` emits `menupause 1` when the toggle is on.
- `menupause` is in `kPlayerCommands`; `menupause scan` and `menupause force`
  are release-guarded and absent from it.

**Invariants that keep it safe:**
- `instance_activate_all` appears nowhere in the plugin source.
- Every deactivate path has a matching recorded-id reactivate (structural
  assertion on the freeze/thaw helpers).
- The curated `menu_pause.json` `blocking` list contains only
  `UI_Parent_obj` descendants, per `hs-game-sdk`'s hierarchy, and contains
  **none** of the ambient HUD objects of §3.3 — re-derived from the SDK, not
  copied.
- The world set contains only descendants of the §3.1 parents, and contains no
  `UI_*` object.
- Every ForgePact tick listed in §5.4 checks the frozen flag.

**Fixture, not a live game**: the hierarchy assertions run against
`hs-game-sdk`'s tables, so the whole suite runs with no game process — same as
every other suite here.

---

## 9. Phase 2 — implementation

New `plugin/include/ForgePact/MenuPauseMod.hpp` (the ModManager-split shape the
other mods use: a singleton, its own atomics, leaning on ModuleMain's `Out`,
`g_Yytk`, `asset_get_index` helpers), holding:

- armed / frozen flags, the frozen-id vector, the room index at freeze time,
  the thaw reason and counters for `status`.
- `ResolveAssets()` — every index by name, once, with a refusal if any fails.
- `DetectBlockingMenu()` — §5.1.
- `Freeze()` / `Thaw(reason)` — §5.2, both `try`-wrapped, thaw always reachable.
- `Tick()` — edge detection + watchdog, called from `FrameCallback`.

`ModuleMain.cpp` changes:

1. `MenuPauseMod::Instance().Tick()` in `FrameCallback`, after the setup gate.
2. The §5.4 early returns in the other ticks.
3. `menupause` in `RunCommand`, and in `kPlayerCommands`.
4. Thaw on the room-change edge and from the existing `ZoneState*` hooks.
5. F4 panic thaw, in both builds.

Ordering note: `FrameCallback` is a `Present` hook, so a freeze issued on frame
N takes effect for frame N+1's step. That is correct — the menu's first drawn
frame is the last frame the world moves — and it is worth one comment in the
source so nobody "fixes" it later.

---

## 10. Phase 3 — panel

`src/forgepact.py`, following `mod_pet_quest_pickup` exactly:

- `DEFAULTS["mod_menu_pause"] = False`.
- `build_cmds`: append `menupause 1` when on. Safe to send at launch — no hook
  is installed, and the mod refuses to arm outside an in-world room (§5.5.6).
- The `/api/set` live-dispatch branch (`send_cmds([f"menupause {0|1}"])`) and
  the key list at line ~1413.
- A row in the **Mods → World** card, with copy that names the limitation:
  *"Pause the game while a menu is open — monsters, your character, your
  mercenary and your pet all stop, and buff, debuff and cooldown timers stop
  with them. The menu itself works normally. While paused the world's
  characters are hidden; they come back the moment you close the menu."*

---

## 11. Phase 4 — build, verify, document

1. `plugin_build\build.bat release`, copy to `modfiles_shipped\`.
2. `py -m unittest discover -s tests -v` — all suites, not just the new one.
3. **Live verification checklist** (the "after" half of the workflow rule):
   pause mid-pack-fight; pause with a DoT on the player; pause with a 20 s
   Headhunter buff and wait a minute; pause with an ability on cooldown; pause
   with the pet fetching a quest item; pause in Chaos Tower (documented as
   *not* stopping the run clock in 1.3.19); open every menu class — ESC,
   inventory, talents, journal, map, stash, a yes/no prompt; close each one and
   confirm resume; **Save and Exit from the pause menu**; zone change with the
   menu open; F4 panic thaw; `menupause 0` while frozen.
4. `release-notes-v1.3.19.md` — `## New` with the mod and its limitation, and
   `## How to update` boilerplate.
5. `docs/submodules/ForgePact/instructions.md`: the new test file, the new
   command, the new curated data file, and any Known Limitation Phase 0 turns
   up.
6. `README.md`: one row in the feature table, one short section if the
   limitation needs explaining.
7. `docs/menu-pause-research.md` committed alongside, with the measurements.

---

## 12. Risks

| Risk | Why it matters | Mitigation |
| --- | --- | --- |
| Deactivating the player breaks a menu, or the menu can no longer be closed | Soft-lock — the worst outcome this mod can produce | Phase 0 item 6 decides before implementation; Mechanism B for the player half; F4 panic thaw ships regardless |
| Buff durations turn out to be wall-clock, not instance-counted | §3.2's central assumption; buffs would keep expiring while paused | Phase 0 item 5 measures it directly; if wrong, buffs get their own mechanism and the release notes say what does not freeze |
| The game culls with `instance_deactivate_region` | A blanket reactivate would wake culled instances | Never `instance_activate_all`; reactivate only recorded ids (already the design) |
| Save/exit pressed while frozen | Deactivated instances are invisible to `with()`; a save that walks the player could write wrong state | Thaw on room change and before exit (§5.5.2); Phase 0 item 8 watches for it |
| Freeze cost at high density | A visible hitch on every menu open | Phase 0 item 10 measures; spread over frames if needed |
| A new UI object in a game patch is not in the curated list | A menu that does not pause | Allowlist semantics: unknown = no pause, never a stuck freeze. `menupause scan` re-derives the list in one session |
| ForgePact's own ticks keep moving a frozen world | Pet walks, globes fly, labels expire | §5.4 — one flag, enforced by a contract test |
| Online session | Desync | Refuse to arm (§5.5.6) |

---

## 13. Files touched

```
hs-game-sdk/curated/menu_pause.json          (new - blocking + ambient lists)
ForgePact/plugin/include/ForgePact/MenuPauseMod.hpp   (new)
ForgePact/plugin/ModuleMain.cpp              (tick, command, thaw edges, tick guards)
ForgePact/src/forgepact.py                   (default, build_cmds, /api/set, Mods tab row)
ForgePact/tests/test_menu_pause_contract.py  (new)
ForgePact/docs/menu-pause-plan.md            (this file)
ForgePact/docs/menu-pause-research.md        (new - Phase 0 log)
ForgePact/release-notes-v1.3.19.md           (new)
ForgePact/README.md                          (feature row)
docs/submodules/ForgePact/instructions.md    (test, command, curated file)
```

---

## 14. Open questions for the owner

1. **Which menus count?** The plan's default is "every window under
   `UI_Parent_obj` except the HUD/ambient objects of §3.3". Three need a call:
   the radial emote wheel (`UI_Circle_Menu_obj` — a menu, but it does not cover
   the screen; default **no pause**), the death screen (`UI_You_Died_obj`;
   default **no pause**, the game is already resolving a death), and the
   in-game chat box (`UI_Ingame_Chat_obj`; default **no pause**).
2. **Hidden-while-paused, or wait for the snapshot?** §7 ships option 1 now and
   option 2 in a follow-up. The alternative is holding the whole mod until the
   snapshot works.
3. **Zone-level timers** (Chaos Tower run clock): out of scope for 1.3.19 per
   §5.3, switch reserved. Worth pulling in, or leave it?
