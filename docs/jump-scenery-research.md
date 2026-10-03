# Jumping through scenery (ForgePact #16): research phase

**Question.** How does Hero Siege decide that the player's universal jump is
blocked by scenery? And if the player's own collision queries answer "no
collision" while the player is in the air, does the jump cross a prop, and what
does the game then do with a landing point inside one?

**Why it matters.** Issue #16 asks for a mod that lets the jump pass over
scenery props while still landing on a valid spot. Nothing in this repository
knew how the jump is blocked. Elsewhere in `docs/`, "jump" means a player warp
(`far-sleep-research.md`), a trampoline (`frame-profiler.md`), the pet "jumping
around" an item (`pet-loot-stuck-research.md`) or a dispatcher
(`pet-quest-collector-c-research.md`); none of them is the player's jump. So this
phase builds one research-build instrument (`jumpprobe`) that hooks every
candidate in one build, runs one live session, and records a `## Decision`. The
mod itself (a player command, a panel switch that is off by default, the
`NATIVE_BOOLEANS` oracle entry and a release-notes line) is phase 2, written
against that decision.

**Posture.** Everything here is measured runtime behaviour, a reading written in
our own words, or our own code. Game objects and scripts are named by their
`hs-game-sdk` names and indices. No game script text, no address and no struct
offset appears. A reading is labelled as a reading, and it is not a fact until a
live session records it.

**Out of scope for this phase.** Enemy jumps (`CA_enemyJump` is a negative
control only); changing props (deactivating, destroying or moving a
`Collision_Parent_obj` instance is "walk through walls", the far-sleep class of
change); Jump Power and the jump's arc; co-op; anything that suspends the game's
own loop.

## Status

- **Phase:** research (phase 1 of 2). Nothing in this phase is player-visible,
  and the player build is free of it.
- **Instrument:** `jumpprobe` (§ Instrument), research build only, on the
  ForgePact branch `16-jump-through-scenery-research`.
- **Live procedure 1:** run on 2026-10-03, slot 14 "Sorak", in `Town_01_rm`
  (§ Results › Live 1 results). § Decision: `finding: builtins` (the jump
  crossed the prop only when the lever answered the player's builtin queries
  against the whole `Collision_Parent_obj` family) and `valid-landing:
  not-observed` (a jump aimed into a prop did not start, so nothing landed
  inside one).
- **Static reading:** done on 2026-10-03 for the scripts that have a name and
  for the player's per-step update, read as a list of the scripts it calls (§
  Static reading). The function that turns the jump key into a jump is still
  unnamed; Live 1 measured what the jump does from the outside.
- **Arc:** `arc: not-established`. The reading did not establish the jump's
  arithmetic (§ Not established), so this phase has no jump-arc model or spec.

## Static search

Searched on 2026-10-03 over `hs-game-sdk/cpp/include/hs_game_sdk/scripts.hpp`,
`objects.hpp`, the Python hierarchy (`hs_game_sdk.objects`), `sprites.py`,
`sounds.py` and every `ForgePact/docs/*.md`. Code spells each name through its
SDK constant. Each index below is that constant's index.

**Jump scripts.** The game has a universal player jump, separate from skills.

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_CA_playerJump` | 348 | `CA_*` is the network "client action" family (`gml_Script_CA_playerMove` 352, `gml_Script_CA_playerSetMoveDirection` 364, `CA_enemyCreate`), so this is most plausibly the co-op relay of a jump, not the local keypress (**static reading**) |
| `gml_Script_PlayerForceJump` | 2770 | starts a jump on its `self` (§ Static reading) |
| `gml_Script_playerJumpGravity` | 2764 | a getter-sized script with one call site |
| `gml_Script_StatJumpPower` | 3391 | the Jump Power stat. Like Skill Haste, it is reached from `ReturnSpecificStat` (3344), the dispatcher `docs/models/skill-stat-spec.md` in the toolkit describes. The Dexterity sheet draws it (`Dexterity_Jump_Power_spr`) |
| `gml_Script_skillsLeap` | 3664 | skill-shaped |
| `gml_Script_skillsBlink` | 3660 | skill-shaped |
| `gml_Script_skillsCharge` | 3662 | skill-shaped |
| `gml_Script_CA_enemyJump` | 273 | negative control (an enemy jump's relay) |
| `gml_Script_CheckTalentUse` | 539 | own-detour control: runs once per frame (measured, `docs/RUNTIME_DATA_MODELS.md` § 7.2 in the toolkit) |
| `gml_Script_CA_playerMove` | 352 | the movement relay of the same family |
| `gml_Script_CA_playerSetMoveDirection` | 364 | the movement relay of the same family |

**Collision scripts.**

| Name | SDK index | Note |
| --- | --- | --- |
| `gml_Script_CanMove` | 467 | yes/no gate on a position (§ Static reading) |
| `CanMoveFuncs` | 466 | bare name. The setup script that binds `CanMove` as a method value |
| `gml_Script_InstancePlaceTallest` | 599 | `CanMove`'s one named callee (**static reading**) |
| `gml_Script_TilePlaceMeeting` | 3866 | suggests a tile collision route |
| `gml_Script_CheckCollisionLine` | 523 | a long line probe in one direction |
| `gml_Script_CheckPath` | 534 | |
| `getClosestCollisionDir` | 1613 | bare name |
| `CollisionsFunc` | 595 | bare name |
| `gml_Script_collision_normal` | 594 | |

**Builtins.** The player's own Step code is measured to call these every step
against `Enemy_Parent_obj` and `Collision_Parent_obj`, for ordinary movement
collision. They go through the builtin table, where `HookBuiltin` sees them
(2026-09-10, `pet-quest-collector-research.md` sessions 5-6, the `citrace`
`CITRACE_BUILTIN_PM` hooks in `ModuleMain.cpp`): `position_meeting`,
`instance_place`, `instance_position` and `collision_point`. `citrace` hooks
`point_in_rectangle`, `distance_to_point`, `instance_nearest` and
`point_in_circle` there too. No ForgePact code hooks the following standard GML
collision builtins yet, so they are candidates: `place_meeting`, `place_free`,
`collision_line`, `collision_rectangle`, `collision_circle` and
`tilemap_get_at_pixel` (the tile route `TilePlaceMeeting` suggests). If
`HookBuiltin` cannot resolve one of these names, the instrument reports that row
`failed`, and the failure is not fatal.

**Objects.**

- `Collision_Parent_obj` (957) has the parent `Avoidable_Parent_obj` (433) and
  1,824 descendants. Its direct children are:
  - `Collision_Prop_obj` (959), with 1,676 descendants. This is the scenery,
    including `Destructible_Parent_obj` (1325) and `Secret_Jump_obj` (4339).
  - `Block_obj` (609), with one child, `Block_Dynamic_Sprite_obj` (608).
    `ReplaceBlockObjWithDynamics` exists. These are most plausibly the level's
    wall blocks.
  - `Wall_Parent_obj` (5701), with the children `Invisible_Wall_obj` (2269),
    `Invisible_Wall_Not_Minimap_obj` (2268) and `Gate_Parent_obj` (1881). These
    are the map's edges and the zone gates.
  - `Chaos_Tower_Collision_obj` (871), `Lock_obj`, `Lever_Parent_obj`,
    `Quest_NPC_Parent_obj`, `Rock_Pillar_obj`, `Valve_Platform_obj`,
    `Angelic_Room_Platform_Mask_obj` and `Cult_Dungeon_Bridge_Block_obj`.
- **Outside the family, with no parent:** `Boss_Block_obj` (682),
  `Dungeon_Boss_Blocker_obj` (1365), `Bridge_Block_obj` (695),
  `Cthulhu_Wall_Block_obj` (1050), `Autotile_Minimap_Wall_obj` (418),
  `Path_Blocker_obj` (3401) and `Shr_Blocker_obj` (4587). A
  `Collision_Parent_obj` query never sees them, so how they block, if they do,
  is **not established**.
- **Player side:** `Player_obj` (3553, parent `Enemy_Aggroable_obj`) and
  `Universal_Jump_Land_Stun_obj` (5343, the landing effect of a universal
  talent). Also `Quest_Jump_Spot_obj` (3966) and `Monster_Jump_Trigger_obj`
  (2966), the sounds `Player_Jump_01..03_snd` and the sprite
  `Effect_Jump_Landing_spr`.

**Negative results**, recorded so nobody repeats them:

- `docs/RUNTIME_DATA_MODELS.md`, `hs-game-sdk/curated/` and `stats.py` hold no
  `*Jump*` variable, stat id or measurement.
- No ForgePact command touches the jump.
- Object events do not resolve by name on this build
  (`docs/RUNTIME_DATA_MODELS.md` § 5.1). So the instrument's rows are scripts
  and builtins only.
- `far-sleep-research.md` says "a `playerwarp` to the map's edge snapped back".
  That is the one hint that the game corrects an out-of-bounds position, and it
  is a note, not a measurement.

## Static reading

Every claim carries one of four labels, plus a source:

- **Static reading**: read locally from the compiled game and written here in
  our own words. No script text, address or struct offset is quoted or
  transcribed (`AGENTS.md` § "Legal" in the toolkit). Unless a claim says
  otherwise, the reading is of the build current on 2026-10-03, read that day.
- **Measured**: observed in a running game.
- **Our code**: what ForgePact does.
- **Not established**: not known yet. § Not established lists what Live 1 is
  meant to settle.

### Who calls the candidates

- **`CA_playerJump` has exactly one direct call site**, inside an unnamed
  compiled function (**static reading**). That function calls well over a
  hundred `CA_*` client-action handlers (zone state, the player list,
  inventory updates and more), so it is the network message dispatcher, and
  `CA_playerJump` is the co-op relay of a jump, not the local keypress.
- **`PlayerForceJump` has two direct call sites** (**static reading**). One is
  in `PlayerTakeDamage` (2786), so a hit can start the same jump. Under which
  condition it does so is **not established**. The other is in an unnamed
  function shaped like a world object that launches whatever it catches:
  it tests for instances at its own position and starts a forced jump on
  each one it finds. Which object it is is **not established**.
- **`playerJumpGravity` has one direct call site**, inside a very large unnamed
  function (**static reading**). The size and the missing name fit an object's
  Step event. Its own body is a few instructions, a getter. The value it
  returns is **not established**: the local reading of it did not render.
- **`StatJumpPower` is reached only from `ReturnSpecificStat`** (**static
  reading**). Every read of Jump Power through the dispatcher therefore passes
  it, as with Skill Haste (`docs/models/skill-stat-spec.md` in the toolkit).
- **`CanMove` and `CheckCollisionLine` have no direct call site** (**static
  reading**). They are reached through the script table or a method value.
  `CanMoveFuncs` is a short setup script that does exactly that: it binds
  `CanMove` as a method value and stores it. So a table hook would see these
  calls, and the instrument's native detour sees them by either route.
- **None of the named candidates is the local keypress** (**static reading**).
  Neither jump script is called from anywhere that looks like input handling.
  So the code that turns the jump key into a jump lives in an unnamed compiled
  function: either one of the hosts above or something else. Which one is
  **not established**. Live 1's J1 checks which of `CA_playerJump` and
  `PlayerForceJump` fires with the local player as `self`.

### What the jump scripts do

- **`PlayerForceJump` starts a jump towards a target point computed from a
  distance and a direction it is given, and tests no scenery on the way**
  (**static reading**). It records the jump on `self` (as entries of an
  array-valued variable, name not established), announces it to the network
  when playing co-op, and calls no collision script. The target is offset
  from `self`'s own position; that the position it starts from is `x`/`y`
  fits what `CanMove` reads, but is **not established**. **So nothing in
  `PlayerForceJump` tests the target against scenery.** If a blocked test
  exists, it runs somewhere else: per frame while airborne, or in whatever
  calls the jump.
- **`CA_playerJump` replays another player's jump from the sender's numbers**
  (**static reading**), looking the player up by network slot, and calls no
  collision script.
- **The jump state is held as entries of one array-valued variable, not as
  named variables** (**static reading**). Variable-name recovery found no named
  variable for the jump's state. The name of the array variable itself
  is **not established**. A consequence for the instrument: `jumpprobe state`
  finds variables by name pattern (`jump`, `air`, `grav` ...). It shows this
  state only if that array's name matches, or if the jump also moves a named
  variable. If not, Live 1 records `airborne-state: not-observed` and takes the
  airborne length from the trace's moving `x`/`y` run, as its procedure says.

### What the collision scripts do

- **`CanMove` is a yes/no gate on a position, built on builtin collision
  tests plus one instance lookup** (**static reading**). A hit in any of the
  tests answers "blocked"; the lookup goes through `InstancePlaceTallest`, and
  what the instance it finds is like can also answer "blocked". Which
  builtins the tests are is **not established**, because the compiled code
  does not name them. The instrument's builtin rows count them by name.
- **`CheckCollisionLine` is a long line probe in one direction, built on
  builtin calls** (**static reading**). Which builtins they are is **not
  established**.
- **`getClosestCollisionDir` does nothing in this build** (**static reading**):
  its compiled body returns an empty value at once.
- **`CollisionsFunc` has the same shape as `CanMoveFuncs`** (**static
  reading**): a short setup script that binds method values, with no
  collision call of its own.
- **`CA_enemyJump`** is the enemy counterpart of `CA_playerJump`. Its body was
  not needed beyond confirming that it is a separate script, the instrument's
  negative control.
- **Not read:** `skillsLeap`, `skillsBlink`, `skillsCharge`,
  `TilePlaceMeeting`, `InstancePlaceTallest`, `CheckPath` and
  `collision_normal` were not read for this phase (the local reading of the
  first two did not render). Whether a leap or dash skill shares the jump's path
  is **not established**.

### The player's per-step update

The large unnamed function that calls `playerJumpGravity` was read after the
round-0 reading, as a list of the scripts it calls rather than as a body: a
full rendering of it did not finish in the time the local tooling allows. The
listing covers the function and may run a little past its end.

- **It is the player's own per-step update** (**static reading**). Besides
  `playerJumpGravity` it calls the player's movement, timer, interaction,
  minimap, town-portal, disabled-state and in-water scripts
  (`PlayerMovement`, `PlayerUpdateTimers`, `PlayerInteracting`,
  `PlayerUpdateMinimap`, `PlayerCastTownPortal`, `PlayerDisabled`,
  `IsInWater`), the life and mana timers, and the skill scripts
  `skillsBasic`, `skillsLeap`, `skillsCharge`, `skillsBlink` and `skillsAura`.
- **Of the instrument's collision-script rows it calls only
  `InstancePlaceTallest` directly**, from several sites (**static reading**).
  It makes no direct call to `CanMove`, `CanMoveFuncs`, `TilePlaceMeeting`,
  `CheckCollisionLine`, `CheckPath`, `getClosestCollisionDir`,
  `CollisionsFunc`, `collision_normal`, `CA_playerJump` or `PlayerForceJump`.
  A call to `CanMove` or `CheckCollisionLine` through the script table or a
  method value is still possible (neither has a direct site anywhere): **not
  established**.
- **Which collision builtins it calls is not established** statically.
  Builtins are reached through runtime function pointers, so a list of call
  targets cannot name them. Live 1 named them from the outside (§ Results).
- **Whether the blocked decision is read every frame or once at take-off is
  not established** statically: the calls sit in a per-step function, so they
  can run every frame, but what gates them was not read. Live 1 measured that
  the jump runs through `skillsLeap` and `playerJumpGravity` once per frame
  for its whole length, and that a blocked jump does not move the player at
  all (§ Results).

### What this means for the lever

The reading puts no collision test in either jump script, and it places the
per-frame jump update in a large unnamed function that object events would
host. Object events do not resolve by name on this build, so that function
cannot be hooked by name. What it calls can be: the builtins (through the
builtin table) and the collision scripts (through the script table or a method
value). That is why the lever answers the player's own builtin collision
queries, and optionally three script rows, instead of hooking the jump itself
(**our code**, § Instrument). And because the reading predicts that neither
jump script runs on the local keypress, the lever can also hold its window
open with no jump script at all (`hold`), and Live 1 proves it engages before
any negative is read (`lever-control`).

## Instrument

`jumpprobe` is research build only (`#ifndef FORGEPACT_RELEASE`). It is never
in `kPlayerCommands`, and nothing of it reaches the player build. It lives in
`plugin/ModuleMain.cpp`, with its decision core in
`plugin/include/ForgePact/JumpSceneryProbe.hpp`. `skillprobe`
(`skill-actions-research.md` § Instrument) is its model, and `skillprobe`'s code
is the reference for the detours, the `held` rule, the armed lines and the
refusals. The rows are the twenty scripts and the ten builtins of § Static
search, each through its `hs-game-sdk` constant or builtin name.

- **`jumpprobe`** (bare) prints the build marker
  `jumpprobe: rows=<n> hooked=<k> held=<h> builtins=<b>`, then the usage.
- **`jumpprobe hook`** installs one native detour per script row
  (`MmCreateHook` behind `AddrIsExecutableInModule`, no table-only swap) and one
  `HookBuiltin` per builtin row.
  - A row another instrument already holds is reported `held` and is never
    hooked twice.
  - `hook` refuses outright, naming the hook, while any `citrace` spatial
    builtin hook is installed: a builtin can be detoured only once.
  - It answers `jumpprobe hook: <n> detoured, <f> failed, <h> held, <b> builtins`.
  - Counts run from then on. Nothing is logged until `arm`.
- **`jumpprobe arm [n]`** / **`arm off`** logs the next `n` calls per row
  (default 5, at most 500) **whose `self` is the local player**, one line each:
  `jumpprobe <Row> #<k> self=<Obj#index@id> argc=<a> a0=... ret=... x=<x> y=<y> frame=<f>`.
  - The local player is identified by the instance-handle rule:
    `VALUE_REF` is accepted (`docs/RUNTIME_DATA_MODELS.md` § 1 in the toolkit).
  - Builtin rows also print the name of the object argument.
  - Calls from any other `self` are counted under `other-self=` and not logged.
- **`jumpprobe show`** prints `calls=` per row since the last `show`, and the
  cumulative `total=`, with the two controls first:
  - `CheckTalentUse`, the own-detour control, which climbs once per frame;
  - `position_meeting`, the builtin control, which climbs while the player
    walks.

  A row with neither a detour nor a holder prints `calls=n/a (<why>)`, never `0`.
- **`jumpprobe state`** installs no hook. It prints the local player's `x`, `y`
  and `sprite_index` name, and every instance variable whose name contains
  `jump`, `air`, `grav`, `land`, `fall`, `height`, `zpos`, `hover` or `fly`
  (case-insensitive, found through `variable_instance_get_names`), with its
  value. An item it cannot read prints `unreadable`, never a default.
- **`jumpprobe trace 1|0`**: while on, it prints one
  `jumpprobe trace frame=<f> x=<x> y=<y> <name>=<value>...` line for each frame
  in which `x`, `y` or any `state` variable changed, at most 600 lines a
  session. While off, the per-frame tick returns at once.
- **`jumpprobe pass 1 [frames] [props|all] [scripts] [hold]`** / **`pass 0`** /
  **`pass stat`** is **the one research lever**.
  - While it is on, a `CA_playerJump` or `PlayerForceJump` detour entry with the
    local player as `self` opens a window of `frames` frames. The default is 90;
    Live 1 sets the real value from its measured airborne length.
  - **`hold`** keeps the window open for as long as the lever is on, jump or no
    jump, until `pass 0`. § Static reading predicts that neither window opener
    runs on the local keypress (`CA_playerJump` is the co-op relay,
    `PlayerForceJump` is reached from a hit and a launcher). Without `hold`
    the lever would then answer nothing while reporting itself ON, and every
    later check would read as a negative about the game. With `hold` the lever
    does not depend on either script, and it is the lever's own positive
    control: walking into a prop with it on shows whether an answered builtin
    changes the player's movement at all. A jump-script entry under `hold` is
    still counted in `windows-opened=`.
  - **An inert lever is named.** While the lever is on without `hold`, if the
    local player's calls arrive (`outside-window=` climbs) but no window has
    opened (`windows-opened=0`), `pass stat` and `show` print a
    `jumpprobe pass: INERT - ...` line: every call ran the game's own function,
    and nothing from that run says what blocks the jump. `pass 1` also warns
    when no opener is detoured (without `hold`), when no collision builtin is
    hooked, and when no local player resolved.
  - Inside the window, a hooked builtin call with the local player as `self` is
    answered without running the original if its object argument is one of
    these:
    - `Collision_Prop_obj` or a descendant (`props`, the default);
    - `Collision_Parent_obj` or a descendant (`all`).

    The family is decided through `object_is_ancestor` by name, never from a
    hand-written list. The answer matches what the builtin returns when
    nothing is there: `noone` for `instance_place`, `instance_position`,
    `collision_point`, `collision_line`, `collision_rectangle` and
    `collision_circle` (each returns an instance or `noone`); `false` for
    `position_meeting` and `place_meeting`.
  - `place_free` and `tilemap_get_at_pixel` take no object argument, so the
    family rule cannot decide them. They are answered only under `all` (window
    open, the player as `self`): `true` for `place_free` and `0`, an empty
    tile, for `tilemap_get_at_pixel`. Under `props` they run the original and
    count under `other-family=`.
  - With `scripts`, the window also rewrites the returns of three script rows:
    `CanMove` to true, `InstancePlaceTallest` to `noone` and
    `TilePlaceMeeting` to false.
  - Everything else runs the original: calls outside the window, from another
    `self`, against `Enemy_Parent_obj` or against any other family.
  - The counters per row are `passed=`, `passthrough=`, `outside-window=`,
    `other-self=` and `other-family=`. `pass stat` prints them with the window
    state, `hold=` and `windows-opened=`. `pass 0` closes any window and ends
    `hold`.
  - The family rule reads the query's object argument, not the instance the
    query hits. A query against `Collision_Parent_obj` itself is outside
    `props`, so under `props` it runs the original and counts
    `other-family=`. The armed lines print `object=`, so a `props` run whose
    `other-family=` climbed on `Collision_Parent_obj` queries is a run where
    `props` never saw the query that blocks.
  - The lever is deliberately blunt. It skips the landing check too, so Live 1
    can measure what the game itself does with a landing point inside a prop.
    That is the question phase 2's "valid target" rule has to answer.

The decision core is game-independent: the window arithmetic, the family rule
(through a callback), the kind of answer per row and the counters.
`tests/jump_scenery_harness.cpp` compiles it whole
(`tests/test_jump_scenery_behavior.py`), and `tests/test_jump_scenery_contract.py`
pins the command on comment-stripped source. The adapter in `ModuleMain.cpp`
supplies the frame number, `object_is_ancestor`, the detours and the printing.

## Live procedure 1

The full procedure, with every command in order, is in the toolkit workorder
`forgepact-16-jump-scenery-research`, context file, § "Live procedure 1". That
is a local working note, so the setup and the checks are repeated here.

**Setup:**

- **Build:** the research build (`build.bat dev`). The owner installs it when
  asked, and its SHA-256 is recorded in the session's log.
- **Character:** save slot 14, "Sorak" (the owner's decision, 2026-10-03). Any
  character with the universal jump will do.
- **Zone:** Act 1's first outdoor zone (`Act_01_01`).
- **Jump key:** the owner presses their own binding.
- **Position reads:** `menulayout Player_obj` and `jumpprobe state`.
- **Rules:** no `citrace` command runs in this session.
- **Cases:** one ordinary jump, one jump at a low prop, and three outliers: a
  landing inside a prop, a map edge, and a leap or dash skill if the character
  has one. Before the lever's jumps, one walk into a prop with the lever held
  open proves the lever engages (`lever-control`).

**Checks**, in this order. The first four establish that the session is valid.
For the rest, a `fail` or a `not-observed` is itself the finding, except that
a negative about the game from J3, J4 or J5 counts only when the lever is
proven to engage: `lever-control` passed, and that run's `pass stat` showed
`windows-opened=` ≥ 1 or `hold=on`.

| Check | Step | Expected |
| --- | --- | --- |
| `dll-hash` | before launch | the installed DLL's SHA-256 equals the research build's recorded hash |
| `marker` | `jumpprobe` | first line starts `jumpprobe: rows=20 hooked=0 held=0 builtins=0` |
| `control` | `ping` | `pong (YYTK 4.0.1)` |
| `builtin-control` | `jumpprobe hook`, then `show`, walk, `show` 2 s later | `hook` answers `<n> detoured, 0 failed, <h> held, <b> builtins` with `n + h = 20`. `CheckTalentUse` `calls=` climbs on both reads, and `position_meeting` `calls=` climbs between them. A zero on `CheckTalentUse` is `INSTRUMENT-BLIND` |
| `jump-rows` | J1 (an ordinary jump on open ground, armed, traced); J6 (a leap or dash skill, if on the bar) | `CA_playerJump` or `PlayerForceJump` logs a call with the player as `self` (which one, and its arguments); `CA_enemyJump` stays at 0. J6 names the rows a skill fires, or reads `not-observed (no such skill)` |
| `airborne-state` | J1 trace and `state` before and after | `x`/`y` move over a run of frames, and at least one `state` variable changes at take-off and again at landing (its name, and the frame count, which is J3's `frames`). Otherwise `not-observed`, with `frames` taken from the moving-`x` run |
| `vanilla-block` | J2: lever off, the player jumps straight at a low `Collision_Prop_obj` from one step away | the player does not cross (stays within a few px, or stops at the prop's edge), and `show` names the builtin rows that counted calls with a `Collision_*` argument during the jump |
| `lever-control` | L0, the lever's own positive control: `jumpprobe pass 1 <frames> all hold`, the owner **walks** (no jump) into J2's prop, `menulayout Player_obj`, `jumpprobe pass stat`, `jumpprobe pass 0` | `passed=` > 0 on at least one builtin row: an answered builtin reaches the player's movement queries. Also record whether the player walked into the prop (`playerwarp` back to the pre-walk coordinates if so). `passed=` 0 on every row is `fail`: the lever never engaged, and J3, J4 and J5 record `lever-not-engaged`, never `not-observed` |
| `pass-crosses-prop` | J3: `jumpprobe pass 1 <frames> props`, repeat J2, `jumpprobe pass stat`. Add `hold` when J1 logged neither `CA_playerJump` nor `PlayerForceJump` with the player as `self` | the player ends on the far side and `passed=` rose on at least one row. Without `hold`, `pass stat` must show `windows-opened=` ≥ 1; `windows-opened=0` (the `INERT` line) means the run says nothing, so repeat it with `hold`. If the armed lines show `other-family=` climbing on `Collision_Parent_obj` queries, `props` never saw the blocking query: repeat with `all` before recording. If not crossed, repeat with `scripts`, then with `all`, and record which flag crossed, or that none did. `not-observed` (none crossed) needs `lever-control` passed and every run engaged (`windows-opened=` ≥ 1 or `hold=on`); otherwise record `lever-not-engaged` |
| `invalid-landing` | J4: lever on (J3's mode, `hold` included), jump so the arc ends inside a prop wider than the jump, traced, `jumpprobe pass stat` | one of: settles outside within a few frames (`game-ejects`, with the frames), stays inside (`game-stuck`), or leaves the room's bounds (`game-falls-through`). `not-observed` needs the run engaged, as for J3; otherwise `lever-not-engaged` |
| `warp-restore` | only if J4 left the player stuck or out of bounds | `playerwarp <x> <y>` with J3's post-jump coordinates, then `menulayout Player_obj` reads within 2 px. Otherwise `not-observed (not needed)` |
| `boundary-wall` | J5: `jumpprobe pass 1 <frames> all` (with `hold` if J3 needed it), jump outward at the zone's edge (`Invisible_Wall_obj`), `jumpprobe pass stat` | the player either stays inside (quote `passed=` and `windows-opened=`) or leaves the map (then `playerwarp` back). Record which. A stay-inside with the lever not engaged is `lever-not-engaged` |

**Teardown:** `jumpprobe pass 0`, `jumpprobe trace 0` and `jumpprobe arm off`.
The operator then restores the saves under their own rules.

## Results

### Live 1 results

Run on 2026-10-03 by the workorder's live operator, on the research build of
`aaf1856` (SHA-256 `6a0907ca21f1d2124dc8593e835efd3499aaa43fae7d19e97698fe9688e1e3e9`).
The capture is the toolkit workorder's
`forgepact-16-jump-scenery-research-live-1.md`, a local working note; what it
measured is restated here. Character: save slot 14, "Sorak" (level 100, Hell),
whose bar has no leap, dash, charge or blink skill.

**Where it differed from § Live procedure 1:**

- The room was `Town_01_rm` (Town of Inoya, a safe zone), not `Act_01_01`.
  The owner chose the props there. Whether an outdoor zone's props block the
  jump the same way is **not established**.
- The jump key is Space, and the jump goes **towards the mouse cursor**, not
  in the direction the player faces (the owner, 2026-10-03). The owner placed
  the cursor and the character; the operator pressed Space through the
  driving tool and never moved the cursor.
- J1 and J2 were one jump: the owner was already in front of the prop with
  the cursor beyond it, so there was no separate open-ground jump. Walking (the
  builtin control, L0) was driven by key input, not by the owner.
- J5 was aimed north, over a building and prop area, with the player 355 px
  from the room's west side and about 390 px from its top, so the nearest room edge
  was beyond the jump's reach (about 117 px). It was a second jump aimed into
  scenery, not a map-edge test.

| Check | Observed | Verdict |
| --- | --- | --- |
| `dll-hash` | the installed DLL's SHA-256 equals the recorded build | pass |
| `marker` | `jumpprobe: rows=20 hooked=0 held=0 builtins=0 ...`; `hook` answered `20 detoured, 0 failed, 0 held, 10 builtins` | pass |
| `control` | `pong (YYTK 4.0.1)` | pass |
| `builtin-control` | `CheckTalentUse` `calls=` 1230, then 570 over the walk; `position_meeting` 4920, then 1909 | pass |
| `jump-rows` | `skillsLeap` and `playerJumpGravity` logged a call with the player as `self` on every frame of the jump (104 and 103 calls); `CA_playerJump`, `PlayerForceJump` and `CA_enemyJump` 0 | pass |
| `airborne-state` | the only instance variables matching the name filter are `bufferJump` and `slopeHeight`, and both stayed 0 through every jump | not-observed |
| `vanilla-block` | lever off, a jump at the prop left the player exactly where it started | pass |
| `lever-control` | `pass 1 104 all hold`, then a walk into the prop: `position_meeting` `passed=420`, `place_meeting` 1947, `collision_line` 2021 | pass |
| `pass-crosses-prop` | `props` and `props scripts` stayed blocked with `passed=0` on every row; `all` crossed, 117 px | pass |
| `invalid-landing` | with `all hold`, a jump aimed into a horse carriage did not start; nothing landed inside a prop | not-observed |
| `boundary-wall` | with `all hold`, two jumps aimed north into a building and prop area did not move the player; the nearest room edge was 355 to 390 px away, out of reach of the 117 px jump, so no edge was tested (the operator's capture reads `pass`; this record reads it as not observed) | not-observed |
| `warp-restore` | nothing was stuck or out of bounds, so no restore was needed | not-observed |

What the session established, each item **measured** unless it says
otherwise:

- **The universal jump runs through `skillsLeap` and `playerJumpGravity`,
  once per frame, for 104 frames.** Both log a call with the player as `self`
  on every frame of the jump, whether the player moves or not; `skillsLeap`
  takes one argument close to 1 (0.996 to 1.001 across frames).
  `CA_playerJump` and `PlayerForceJump` are not called by the local jump, as
  the static reading predicted, so the lever's jump-script window never opened
  (`windows-opened=0` on every run) and every lever run used `hold`.
  `StatJumpPower`, `CanMove`, `TilePlaceMeeting`, `CheckCollisionLine` and the
  other collision-script rows logged no player call during a jump, and
  `InstancePlaceTallest`, which the player calls while walking, logged none
  during one either.
- **The jump's length is 104 frames, about 117 px on open ground for this
  character** (about 1.1 px per frame, in the `all` crossing's trace). Sorak's
  Jump Power was not read, so this is not the base jump.
- **No instance variable named like a jump state changes.** `bufferJump` and
  `slopeHeight` are the only names matching `jump`, `air`, `grav`, `land`,
  `fall`, `height`, `zpos`, `hover` or `fly`, and both stayed 0. Where the
  game keeps the jump's state is **not established** (the static reading
  points at an array-valued variable, § Static reading).
- **A blocked jump does not move the player at all.** With the lever off, a
  jump at a prop 22 to 40 px away left the position unchanged to the tenth of
  a pixel, while `skillsLeap` and `playerJumpGravity` still ran for the jump's
  whole length. The player does not stop at the prop's edge.
- **During the jump the player queries the collision family by its parent,
  not by the prop.** The calls with the player as `self` that name a
  collision object pass `Collision_Parent_obj` itself (`position_meeting`,
  `place_meeting`, `instance_position`, `collision_line`) or
  `Wall_Parent_obj` (`collision_circle`); `position_meeting` also queries
  `Enemy_Parent_obj`. None passes `Collision_Prop_obj` or a descendant of it.
  `instance_position` returned real instances at 22 to 40 px; which objects
  they were was not read.
- **Answering those queries "nothing there" lets the jump cross.** With the
  lever on under `props` (with or without `scripts`), every builtin row
  counted only `other-family=` (`position_meeting` 1413, `place_meeting` 1790,
  `collision_line` 154 in one run) and the player stayed put. Under `all`,
  `position_meeting` (556), `place_meeting` (1756), `instance_position` (46),
  `collision_line` (179) and `collision_circle` (31) were answered, and the
  same jump moved the player 117 px past the prop, over 104 frames, where it
  settled. `place_free`, `tilemap_get_at_pixel` and the three script rows
  answered nothing. Which of the five answered rows is the deciding one is
  **not established**.
- **Walking stayed blocked with the lever on.** Under `all hold`, walking
  into the same prop answered the three rows above (`lever-control`) and
  did not move the player. Two things ran unanswered during that walk:
  `InstancePlaceTallest`, 1944 calls with the player as `self` (the lever was
  not set to answer script rows), and 6252 `place_meeting` calls naming an
  object outside the collision family. Which of them, if either, holds the
  walk is **not established**.
- **A jump aimed into a prop does not start, even with the lever on.** Aimed
  at a horse carriage, under `all hold`, the jump ran (`skillsLeap` and
  `playerJumpGravity` per frame), but the player moved about 4 px once in the
  first run and not at all in the second. So the landing point is judged by
  something other than the player's answered queries. `instance_place` counted 1110 calls
  from other selves in that run and none from the player; whether one of
  those is the landing test is **not established**.
- **J5, a second jump aimed into scenery, did not carry the player either**
  (two runs, `all hold`: the per-frame rows ran, the player's x and y did
  not change). The jump was aimed north into a building and prop area, and
  the nearest room edge, 355 to 390 px away, was out of reach of the 117 px
  jump, so it says nothing about a map edge. What refused it is **not
  established**; the candidates are the same as J4's (what the lever does not
  answer, or something unhooked).
- **`skillsCharge` counted one player call** at some point after J4, with no
  charge skill on the bar. The cause is **not established**. Whether a leap or
  dash skill shares the jump's path is **not observed**: Sorak has none.

## Decision

finding: builtins

Live 1, 2026-10-03 (§ Results › Live 1 results): with the lever answering
the player's builtin collision queries, the jump crossed a prop it could not
cross with the lever off, 117 px over its 104 frames, and `passed=` climbed on
five builtin rows. The lever had to answer the whole family (`all`), because
the player's queries name `Collision_Parent_obj` itself, never a prop
object; `props` and `props scripts` changed nothing. The script rows and the
tile rows answered nothing. `lever-control` passed first, so the result is
about the game. For phase 2 this means the rule cannot tell a prop from a wall
by the query's object argument: that argument is the family's parent on every
query.

`finding:` takes exactly one of these values:

- `builtins`: with `pass` on, `passed=` climbs during a jump at a prop and the
  player crosses it. The blocked test is the player's own builtin collision
  queries.
- `scripts`: `passed=` climbs on a script row, and the crossing needs the
  `scripts` flag.
- `tiles`: the crossing needs the tile rows (`tilemap_get_at_pixel` /
  `TilePlaceMeeting`).
- `not-observed`: the player stays blocked with every lever on, and the lever
  is proven to engage (`lever-control` passed, and each J3 run showed
  `windows-opened=` ≥ 1 or `hold=on`). The decision is made upstream of every
  hooked row.
- `lever-not-engaged`: the lever never answered the player's queries during
  the jump (`lever-control` failed, or no J3 run opened a window and none ran
  with `hold`). This says nothing about the game, and the question stays open
  for a second session.

valid-landing: not-observed

Live 1, J4: the lever engaged (`lever-control` passed, and J4 ran under
`all hold` with `passed=` climbing), and a jump aimed into a horse carriage
did not start: the player stayed at the take-off point, within 4 px, in two
runs. So no landing inside a prop happened, and what the game does with one
is still open. What refused the jump is something the lever does not answer
(§ Results).

`valid-landing:` takes exactly one of these values:

- `game-ejects`: the game corrects a landing inside a prop within a few frames
  (quote the frames).
- `game-stuck`: the player stays inside and needs `playerwarp`.
- `game-falls-through`: the player ends outside the room or under the map.
- `not-observed`: the lever engaged (as for `finding:`) and the jump still did
  not land inside a prop.
- `lever-not-engaged`: J4 ran with a lever that answered nothing, so no
  landing inside a prop was attempted.

## Not established

After Live 1 (2026-10-03), these are still not established. Live 1 settled
that the local jump does not go through `CA_playerJump` or `PlayerForceJump`
but runs through `skillsLeap` and `playerJumpGravity` per frame, that it lasts
104 frames, and that answering the player's builtin queries against
`Collision_Parent_obj` lets it cross a prop (§ Results).

- **What turns the jump key into a jump.** The keypress handler is still
  unnamed. `skillsLeap` runs on every frame of the jump, so it is the jump's
  per-frame step, not necessarily its start.
- **Which of the five answered builtin rows decides the crossing**
  (`position_meeting`, `place_meeting`, `instance_position`,
  `collision_line`, `collision_circle`). Live 1 answered all five at once.
- **Whether the blocked decision is taken once at take-off or every frame.** A
  blocked jump does not move the player at all, which fits a decision made
  before the first step, but the answered builtins are called throughout the
  jump. Not measured either way.
- **What refuses a jump aimed into a prop, and what the game would do with a
  landing inside one.** Under `all`, a jump aimed into a horse carriage did not
  start. The refusal comes from something the lever does not answer:
  `instance_place` calls from other selves, `InstancePlaceTallest`, or
  something unhooked. Whether the landing test runs under another `self` (a
  landing-marker object, for instance) is a question a second session could
  answer only with a lever that answers other selves, which this build's lever
  does not.
- **Which objects the blocking queries found.** `instance_position` returned
  real instances 22 to 40 px from the player; their objects were not read.
  Whether a phase-2 rule can tell a prop from a wall therefore has to come from
  the instance a query returns, not from its object argument (§ Decision).
- **What holds a walk** with the lever answering (`InstancePlaceTallest` or
  the out-of-family `place_meeting` queries, § Results).
- **The map edge and zone gates.** Map-edge behaviour is not established: J5
  never reached an edge (it was aimed north into scenery, with the nearest
  edge out of reach of the 117 px jump), and no zone gate was tried. Whether a
  jump with the lever on stays inside the room or leaves it at an edge is not
  observed.
- **The outdoor zones.** Live 1 ran in `Town_01_rm`, not `Act_01_01`.
- **The jump's state.** No instance variable named like a jump state moved
  (`bufferJump` and `slopeHeight` stayed 0). The array-valued variable the jump
  scripts write (§ Static reading) is still unnamed.
- **The base jump's length.** 104 frames and about 117 px were measured for
  Sorak, whose Jump Power was not read.
- Whether a leap or dash skill shares the jump's path: **not observed**
  (Sorak has none on the bar). Why `skillsCharge` counted one player call is
  not established.
- Whether `Boss_Block_obj` and the other parentless blockers block a jump at
  all.
- **`arc: not-established`.** The jump's arithmetic (airborne frames or distance
  as a function of Jump Power and the base jump) was not established by the
  reading. `StatJumpPower`'s body was not read for its formula, and
  `playerJumpGravity`'s value did not render. The per-step update was read only
  as a list of the scripts it calls (§ Static reading), and `StatJumpPower`
  logged no call during a jump. So this phase has no
  `docs/models/jump-arc-spec.md`, no `jump_arc_model.py` and no
  `tests/test_jump_arc_model.py`. Live 1 measured the arc at one, unread, Jump
  Power (104 frames, about 117 px), which can seed a model in phase 2 once the
  character's Jump Power is read beside it.
- `getClosestCollisionDir` does nothing in this build (§ Static reading). If a
  game update gives it a body, the static reading has to be redone.
