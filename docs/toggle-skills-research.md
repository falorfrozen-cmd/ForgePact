# Toggle skills — research log (issue #11)

Status (2026-09-18): **Track B shipped (off by default).** Four live
sessions run across the indicator workorder and this research doc. Ownership
is read from each `White_Mage_Soul_Spurn_AOE_obj` instance's own
`isMyClient` (no local-player read: `Player_obj` has no `playerNumber`,
session 3), the Purgatory marker (`purgatory` numeric > 0) is required
(session 4 measured a plain, non-Purgatory cast otherwise flashes the
outline for ~146 draws), the slot is `UI_Hud_Talent_obj`'s own `row0[5]`
(`talentId` 240) at its own `navBboxX`/`navBboxY`/`navBboxWidth`/
`navBboxHeight` (session 3), and the OFF lag is ~19 frames from the press
(session 4). `toggleborder 1|0` ships in `kPlayerCommands`, off by default,
panel key `mod_toggle_indicator`. Co-op is inferred, not measured against a
second real player, and not a shipping concern (ForgePact is offline-only;
see the hub guide's Known Limitations). Track A (re-cast guard) is
unaffected by any of this and remains **BLOCKED on Q2**.

- **Measured:** the cast path (Q1), draw order (Q4), three sources of
  accidental re-casts (Q5, one of them the double-cast proc, which bypasses
  `TalentUse`), where the ON/OFF state lives (Q3-D, session 2) and is read
  from the draw hook (session 3/4), ownership by `isMyClient` from the draw
  hook (session 4), the Purgatory marker as the plain-cast-flash
  discriminator (session 4), the slot geometry (session 3) and the OFF lag
  (session 4).
- **Not observed:** what in the call trace discriminates a Purgatory-toggled
  cast of Soul Spurn from a non-toggle cast at the call-trace level (Q2 -
  Track A only, not needed for Track B); *which* sub-index in
  `global.subTalentMap[1].t240` (candidates s2/s6/s7/s9/s10/s12, all found) is
  Purgatory's level; `isMyClient`'s co-op meaning against a second real
  player (checked only in the harness and by `tgprobe spurn as foreign`).
- **Blocked:** every object-event row, which does not resolve by name.

See Results and Decision. Statements in the sections before Results are
static-search facts or labelled inference, written before the sessions.

## The issue

[ForgePact#11](https://github.com/falorfrozen-cmd/ForgePact/issues/11),
"[QoL] Better toggle skills", verbatim:

> Toggle skills can be double casted by accident and don't have any indicator
> if they are on. Adding small cooldown and an animated (or not animated)
> border could make this easier.

The author's clarification (2026-09-17), verbatim:

> White mage's soul spurn skill has an upgrade (skill tree) called purgatory.
> This upgrade turns skill into a toggled skill meaning it doesn't have a
> duration anymore or its duration is infinite. Doesn't look like there's a
> special tag for it so maybe skills which apply their effect continuously
> when cast (toggled) til other are casted again could be "toggle skills".
> Definitely not auras since they have a special tag and can only be equipped
> in a special "active aura" slot in character skills.

And:

> Toggle skills cast their effect continuously when cast (toggled) and stop
> only when casted again or on zone change.

Two mods are wanted, both off by default: a **re-cast guard** (a second press
of a toggle skill within a short window after switching it on is dropped) and
an **active indicator** (a border over every skill-bar slot whose toggle is
currently on, gone on the first frame after a zone change). Neither can be
designed until the questions in Results are answered.

## Static search

Skills are **talents** throughout `hs-game-sdk`: the hotbar slot object is
`UI_Hud_Talent_obj`, the cast routines are `TalentUse*`, and skill-tree
upgrades are **sub-talents** (`GetSubTalentInfo`, `ReturnSubTalentLevel`).
Searched with `grep -i` over `hs-game-sdk/cpp/include/hs_game_sdk/scripts.hpp`,
`objects.hpp`, `python/hs_game_sdk/sprites.py` and the object hierarchy. The
SDK tables are complete for names, so a negative here is *not present*, not
merely *not observed* — but the absence of a name says nothing about behaviour.

| Looking for | Result | Label |
|---|---|---|
| any script named `*Purgat*` | none | not present (static, complete) |
| any script named `*Spurn*` / `*SoulSpurn*` | none — the White Mage's per-talent bodies are not separate scripts | not present (static, complete) |
| objects named `*Soul_Spurn*` | `White_Mage_Soul_Spurn_obj` (5761) → parent `Player_Ability_Parent_obj` (3536); `White_Mage_Soul_Spurn_AOE_obj` (5759) → parent `Player_Damage_Parent_obj`; `White_Mage_Soul_Spurn_Damaging_obj` (5760) → parent `Player_Damage_Parent_obj` | found (SDK parent table) |
| anonymous closures on any Soul Spurn object, `Skill_Controller_obj`, `Player_Buff_Parent_obj`, `Player_Ability_Parent_obj` | none | not present (static, complete) |
| the White Mage talent dispatch | `gml_Script_TalentsWhiteMage` (3797), `gml_Script_TalentsUniversal` (3825); `PopulateTalentStructMapWhiteMage` fills the talent table at load, not at cast | found |
| scripts/objects/sprites named `*Toggle*` relating to skills | only `QuestToggleVisibility`, `UiABloodPactCharacterToggle`, `UiAMarketFilterToggleStat` | not present |
| a persist / infinite / duration family | `ClearPersistSkill`, `StepAbilityParentDestroyTimer`, `PlayerUpdateTimers`; nothing named `*Infinite*` / `*Duration*` / `*Continuous*` | found (names only; behaviour not observed) |
| skill tags | `LoadSkillTagsString` is the only tag script; its output for Soul Spurn is not observed | found |
| room / zone change routines | `RoomGoto`, `NetworkRoomGoto`, `NetworkRoomSetupDone`, `CA_playerRoomSetupDone`, `SetupRoomEffects`; no script named `*ZoneChange*` / `*RoomStart*` / `*RoomEnd*` | found |
| sprites | `Ability_Indicator_Border_spr` (21), `Hud_Talents_spr` | found |
| the aura family | `LoadAura*`, `skillsAura` | found, and ruled out as the centre by the author (auras are tagged and slot-restricted); two rows stay in the probe as negative controls |

**Working hypothesis (inference, not observed).** `White_Mage_Soul_Spurn_obj`
is a `Player_Ability_Parent_obj` descendant, and that parent has a named
per-step destroy timer. "No duration" is then most simply the Purgatory
sub-talent leaving the effect instance alive with its destroy timer disabled,
and "stops on zone change" needs no cleanup routine at all, because GameMaker
destroys every non-persistent instance when the room changes. That would make
the toggle's state *the existence of a live effect instance*, readable at any
moment and automatically correct on the first frame of a new zone. The
alternatives the instrument must also be able to see: a `global.playerBuff`
slot with no expiry, or a scalar on `Player_obj`.

### Candidate scripts hooked by `tgprobe`

Every row is an `hs-game-sdk` constant; all of them are hooked in one build
behind one command.

| Group | Constant | Logged |
|---|---|---|
| cast entry | `gml_Script_TalentUse` | args + ret (ret native only) |
| cast entry | `gml_Script_TalentUseClass` | args |
| cast entry | `gml_Script_CheckTalentUse` | args + ret |
| cast entry | `gml_Script_TalentUseSetSpeed` | count |
| cast entry | `gml_Script_NetworkSendClientTalentUse` | args |
| cast entry | `gml_Script_CA_playerTalentActive`, `gml_Script_CA_playerTalentUpdate` | args |
| class body | `gml_Script_TalentsWhiteMage`, `gml_Script_TalentsUniversal` | args |
| talent lookup | `gml_Script_GetTalentInfo`, `gml_Script_GetTalentId`, `gml_Script_ReturnTalentLevel`, `gml_Script_ReturnTalentValue`, `gml_Script_GetTalentCooldown` | args (hot; first 3 calls) |
| sub-talent lookup (the Purgatory read) | `gml_Script_GetSubTalentInfo`, `gml_Script_ReturnSubTalentLevel` | args + ret |
| tags (confirms "no tag") | `gml_Script_LoadSkillTagsString` | args + ret |
| persist / timers | `gml_Script_ClearPersistSkill`, `gml_Script_StepAbilityParentDestroyTimer` | args |
| persist / timers | `gml_Script_PlayerUpdateTimers` | count (hot) |
| buff state | `gml_Script_BuffAdd`, `gml_Script_BuffRemove` | args |
| buff state | `gml_Script_GetBuff` | count (hot) |
| zone change | `gml_Script_RoomGoto`, `gml_Script_NetworkRoomGoto`, `gml_Script_NetworkRoomSetupDone`, `gml_Script_CA_playerRoomSetupDone`, `gml_Script_SetupRoomEffects` | args |
| HUD | `gml_Script_DrawHudAbilityButtons`, `gml_Script_DrawHudBuffs`, `gml_Script_DrawHud`, `gml_Script_GetPlayerTalentHudObj`, `gml_Script_UiHudTalentNavigation`, `gml_Script_DrawKeyBindSprites` | args |
| HUD closures (every closure split out of the slot object's Create event) | `gml_Script_anon_1233_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_2503_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_10745_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_11619_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_12025_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_12449_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_12916_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_13435_gml_Object_UI_Hud_Talent_obj_Create_0` — the same eight closures at the offsets of the `hs-game-sdk` regenerated at hub `4539e68`. Sessions 1 and 2 ran against the previous game build, where they were `anon@1183`, `@2413`, `@10430`, `@11283`, `@11677`, `@12084`, `@12530` and `@13033`; a closure's name carries its offset in the Create event, so it moves whenever that event's code changes | args |
| positive control | `gml_Script_CheckPlayerInteraction` | count |
| input | `gml_Script_InputPressed` | count (hot) |
| negative control | `gml_Script_LoadAura`, `gml_Script_skillsAura` | count; must not move on a Soul Spurn cast |

Object events, by runtime name `gml_Object_<Obj>_<Event>` built from the SDK's
object name. The SDK has no event table, so `not found` on an event row means
only that the name did not resolve through `GetNamedRoutinePointer` — a
statement about the lookup, not about the object. That lookup is not proven
for object events: `pet-quest-collector-research.md` (session 7) records 22 raw
`gml_Object_*` names, `Player_obj_Step_0` among them, all failing it.
`Player_obj` `Step_0` is therefore in the set as the event rows' positive
control — the player steps every frame.

| Object | Events |
|---|---|
| `Player_obj` (3553) | `Step_0` (positive control) |
| `White_Mage_Soul_Spurn_obj` (5761) | `Create_0 Step_0 Destroy_0 CleanUp_0 Alarm_0` |
| `Player_Ability_Parent_obj` (3536) | `Create_0 Step_0 Destroy_0 CleanUp_0` |
| `Draw_Player_Buff_obj` (1362) | `Create_0 Destroy_0` |
| `UI_Hud_Talent_obj` (5099) | `Create_0 Step_0 Draw_0 Draw_64` |
| `Skill_Controller_obj` (4606) | `Create_0 Step_0` |

`Step_0` rows are count only.

**No Room Start or Room End row, on any object.** Hooking the room start
event has crashed the game before, and `tests/test_est_force_behavior.py`
(`test_room_start_is_not_hooked`) keeps any such token out of the plugin
source; `tests/test_toggle_skill_contract.py` additionally forbids the two
GameMaker event suffixes for Room Start and Room End inside the `tgprobe`
block. That guard is left exactly as it is. What a zone change does (Q6) is
read instead from the room key changing (`tgprobe show`'s `room=` and
`hudSinceRoomChange=`) together with the Soul Spurn object's `Destroy_0` /
`CleanUp_0` rows.

## Instrument

`tgprobe` is **research build only** (`plugin_build\build.bat dev`,
`BloodPactPlugin_rel.dll`); it is not in `kPlayerCommands` and every trace of
it, including the three entry notes below, is inside `#ifndef
FORGEPACT_RELEASE`. It is read-only: nothing it does writes game state. Its
shape is `citrace nativetrace`'s — a native detour per row that counts,
optionally logs and calls through its trampoline — with its own resolver.
Pinned by `tests/test_toggle_skill_contract.py`.

### How a row attaches

Four rows are already hooked by ForgePact before `tgprobe hook` can run:
`DrawHudBuffs` (the head-label hook, installed at init in every build),
`BuffAdd` (the buff logger, installed by the research build's setup at frame
300), `TalentUse` (the co-op skill block, only if co-op rendering was turned
on) and `CheckPlayerInteraction` (only if a `citrace` command ran). ForgePact's
installer puts its hook body in the script table and keeps a trampoline as the
"original"; neither is code inside `Hero_Siege.exe`, and the game's own bytes
already carry a patch, so such a row cannot be detoured a second time. The
resolver, per row, in fixed order:

- (a) resolve the runtime name; nothing there → `not found`.
- (b) the table entry is code inside `Hero_Siege.exe` → nobody in this module
  holds it → native detour → `native`.
- (c) a ForgePact hook holds the table and its saved original is code inside
  `Hero_Siege.exe` → that hook is table-only, so its original is the game
  body → native detour on it → `native (under table-only <hook>)`.
- (d) a ForgePact hook holds the table with a trampoline, and that hook's body
  carries an entry note → no detour; counted by the note →
  `via <hook> (native)`. If (c)'s detour fails on a note row, the note is the
  fallback and the mode reads `via <hook> (TABLE-ONLY; …)`.
- (e) anything else → `blocked: table entry is not code inside Hero_Siege.exe`
  (for example `CheckPlayerInteraction` after `citrace nativetrace`).
- (f) the hooking library refuses → `blocked: MmCreateHook st=<n>`.

Every pointer handed to the hooking library has just been checked with
`AddrIsExecutableInModule`, and the hooking library is called from one place.
The entry notes are one research-only line at the top of `Hook_DrawHudBuffs`,
`HookTalentUse` (before the co-op puppet early return, so a puppet's calls
would be counted too; a single-player session has no puppet) and
`HookBuffAdd`. Each does nothing unless its row attached `via` that hook, so a
row with its own detour is never counted twice. A note cannot see the return
value, so a `via` row prints `ret=n/a (via hook)`.

**Consequence for the session:** a row attached natively by `tgprobe` makes a
*later* ForgePact install on the same routine degrade to table-only. Do not
turn co-op rendering on, and run no `citrace` command, after `tgprobe hook`.

### Subcommands

| Command | Prints |
|---|---|
| `tgprobe hook [substr…]` | Attaches every row whose label contains any given substring (none = all); rows already attached are left alone. First line `tgprobe hook: N native, P via hook, B blocked, F not found`, then one line per row that is not plain `native`, naming its mode and reason. |
| `tgprobe show` | One line per row: `<label> mode=<mode> calls=N lastFrame=F lastGap=G`. `lastGap` is the number of frames between the last two calls (0 = same frame). A row that is not attached (unhooked, `blocked`, `not found`) prints `calls=n/a`, never `0`. A `via … (TABLE-ONLY…)` row is flagged: direct calls bypass it, so a 0 there is *not observed*. Then `room=<key> name=<room name> hudSinceRoomChange=<n> hudRoomUnreadable=<n>`, and last `firstHud=<zone-change\|attach (not a zone change)> room=<key> frame=<F> firstHudSpurnInstances=<n\|unreadable> firstHudAbilityInstances=<n\|unreadable>`. |
| `tgprobe reset` | Zeroes every row's calls, lastFrame, lastGap and log budget. `hudSinceRoomChange` keeps counting and the `firstHud` snapshot is kept. |
| `tgprobe verbose on\|off` | While on, each row flagged for logging writes its first 3 calls since the last `reset` to `out.txt`: `tgprobe <label> #n frame=F self=<Obj#idx@id> other=… argc=N a0=… a1=…` (struct arguments as JSON, each capped at 200 characters), plus `tgprobe <label> #n ret=<value>` for rows that log a return. Count-only rows never log. |
| `tgprobe slots` | Every `UI_Hud_Talent_obj` instance (cap 16): id, `x`, `y`, `sprite_index` and its sprite name, `sprite_width/height`, `image_xscale/yscale`, then every custom variable; the GUI size. |
| `tgprobe buffs` | `global.playerBuff[1][0]`, walked the way `HhBuffAlive` reads it: every non-empty slot's index, value kind, `instance_exists`, object name, and for up to 8 live instances their custom variables. |
| `tgprobe abilities` | Every live `Player_Ability_Parent_obj` instance including descendants (cap 64): object name, id, `alarm[0..11]`, then every custom variable. The Q2/Q3/Q6 read for the working hypothesis. |
| `tgprobe vars <Obj\|global>` | Scalar variables (real/int/bool/string) of the first instance of `<Obj>`, or of the global scope. |
| `tgprobe snap <Obj\|global>` / `tgprobe diff` | Snapshot those scalars, then print changed / added / removed keys (cap 200 lines). A change inside an array or struct is invisible here; that is what the walkers are for. |
| `tgprobe room` | The room key, whether it is readable, and the room name. |
| `tgprobe spurn [log on\|off\|as foreign\|slots\|fields]` | Samples the production `ToggleIndicatorRead` on every `DrawHudBuffs` draw. Bare `spurn` prints the last sample (`n=`, `mine=`, `others=`, `unattributed=`, `capped=`, `state=`) plus running `samples=`/`on=`/`off=`/`unreadable=`/`maxN=`/`transitions=`/`lastTransitionFrame=` counters, the marker-required counters `markedOn=`/`markedOff=`/`markedUnreadable=`, and `firstAfterRoomChange=`. `spurn log on\|off` logs each instance's `playerNumber`/`isMyClient` on every state change (budgeted). `spurn as foreign` is the non-mutating negative control (P1b: there is no local player number left to override): the same enumeration and decision with every own instance re-interpreted as foreign, reported separately and never touching the real counters. `spurn slots` prints every `UI_Hud_Talent_obj` `row0`/`row1`/`playerSlot.bind_skill`/`global.mySkills` entry whose value is talent 240. `spurn fields` prints the latched per-appearance snapshot (`isMyClient`, `playerNumber`, `targetNumber`, `purgatory`, `purgatoryTimer`, `destroyTimer`) taken on the appearance's first draw and refreshed on every draw while it is present. |
| `tgprobe mark <x> <y> <w> <h>\|off` | Draws (or clears) a static outline rectangle at GUI coordinates from the draw hook, to find which candidate slot rectangle sits on Soul Spurn's button; saves and restores `draw_get_colour`/`draw_get_alpha`. Prints `draws=`/`drawExc=` so "never drew" is separable from "drew in the wrong place". |

`hudSinceRoomChange` counts `DrawHudBuffs` calls since the room key last
changed, including the first call in the new room. An unreadable room key is
never stored — it is counted in `hudRoomUnreadable` instead — so "unreadable"
cannot compare equal to anything.

**The first draw after a zone change is read where it happens.** Every
command reaches the plugin through `cmd.txt`, so a `tgprobe show` typed after
arriving lands tens of frames into the new zone — too late to say what the
first draw saw. So on the `DrawHudBuffs` call where the room key changes, the
instrument itself counts the live `White_Mage_Soul_Spurn_obj` instances and
the live `Player_Ability_Parent_obj` instances (descendants included), both
looked up by name, and keeps them with that room key and frame; `tgprobe show`
prints them as `firstHud=`. The key also counts as "changed" the first time it
is seen after `tgprobe hook` attaches: that snapshot is labelled
`attach (not a zone change)` and says nothing about Q6. A failed lookup prints
`unreadable`, never a count.

**Positive controls, one per attach mode, same session:** native →
`CheckPlayerInteraction(control)` (every interactable's step runs it; thousands
in 2 s under `citrace nativetrace`); via hook → `DrawHudBuffs`, whose count
over a window must equal (±2) the `hudCalls=` delta of two `hhlabel` replies
bracketing that window (`hhlabel` works in both builds). A native `0` with the
native control at 0, or a via `0` with the `hhlabel` cross-check failing,
measures the instrument, not the game. Object-event rows have their own
control, `Player_obj.Step_0` (thousands in 2 s if the lookup works for events
at all).

### tgprobe deep — the non-scalar read (session 2)

Session 1 left Q3 `not observed` at scalar depth only, and that negative is
weaker than it reads. `tgprobe snap`/`diff` compare real/int/bool/string
members, so a change inside an array, struct, ds_map or ds_list is invisible by
design. And the scalar reader wraps its **whole** enumeration loop in one
error handler: if reading member N throws, members N+1… are dropped without a
trace and the printed `scalars=` count says nothing about it. No control ever
showed that enumeration reached the end, so "not in any `Player_obj` or global
scalar" is *not observed at unknown coverage*, not "not present".

`tgprobe deep` is the read for everything else. Like the rest of `tgprobe` it
is research build only, installs no hook of any kind, and writes no game state
except inside its own `selftest` fixture. All of its work happens at command
time, on the frame that consumes `cmd.txt`.

**What it reads.** One snapshot captures up to seven scopes; each leaf's path
starts with a root that `deep get` accepts, so a path from any diff can be
pasted straight into `deep get`:

| Scope | Root in paths | What |
|---|---|---|
| `player` | `Player_obj` | the first `Player_obj` instance: every instance variable, containers expanded, plus builtin `alarm[0..11]` |
| `talent` | `talent:<id>` | `global.talentStructMap{<id>}` for the ids given (default `240,243,252`: Soul Spurn, the chained crow talent, Healing Zone as the non-toggle control — session-1 measurements), read through the same validated readers the Blood Pact feature uses |
| `controller` | `Controller_obj` | the first `Controller_obj` instance — `self` of the whole HUD draw chain, never read in session 1 |
| `hud` | `UI_Hud_Talent_obj` | the slot object, with `row0`/`row1`/`grid` expanded |
| `skillctl` | `Skill_Controller_obj#<k>` | every `Skill_Controller_obj` instance, descendants included (cap 8) |
| `global` | `global` | every global, enumerated **both** with the YYTK member enumerator and with `variable_instance_get_names(-5)`, unioned by name; the summary prints both counts as `globalNames=<enum>/<names>` so a short enumeration is visible |
| `census` | `census` | `instance_number` of every existing object index from 0 to the highest SDK `GameObject` enumerator + 512, non-zero rows only, as `census.<ObjectName>` |

**Walker rules.** Arrays → `[i]`; structs → `.name`; a `ds_map` → `{key}`
and a `ds_list` → `[i]`, each recognised from the value's own `ref ds_map` /
`ref ds_list` description and confirmed with `ds_exists`. The description must
*start* with that text (after the runtime's `kind=N str=` prefix): a string
value describes itself as `string:"…"`, and one whose text merely contains
`ref instance ` or `ref ds_map ` — a game-built `string(id)`, say — is a string
leaf, never handed to `instance_exists` or `ds_exists`. A non-struct object
(a method value) is a leaf naming the method; one that names no method is
counted per scope as `objNonStruct=` (see below). Everything else is a leaf holding
the kind-tagged value (strings capped at 120 characters). Caps: **depth 3**
(a root member's own value is depth 1, and containers at depth 1, 2 and 3 are
expanded, so `Player_obj.arr[3].field` and `global.playerBuff[1][0][86]` are
both leaves; a container below that is the leaf `<container n=N>`, so a size
change still shows), **200 elements per container** (then one
`…(+N more)` leaf), **250,000 leaves per scope** (then that scope stops and
reports `truncated=1`). Every member is read inside its own error handler, and
so is every builtin call inside the walk: a throw becomes the leaf
`<unreadable>` and is counted, never an aborted walk. That is the fix for
session 1's defect, and each scope's summary says so in numbers:
`names=N read=M unreadable=U`.

**Instance handles are followed one level.** A value whose own description is
`ref instance N` (how this runner hands out instances — a buff slot, a target,
an owner) is not only a leaf. The handle stays a leaf, so a slot that starts
pointing at a different instance still diffs; and if `instance_exists` says it
is live, its instance variables are read under the handle's path — for example
`global.playerBuff[1][0][86].destroyTimer` — each member in its own error
handler, one level deeper, with the same container caps. This applies wherever
the walker meets the handle, including below depth 3, since the handle is a
leaf there anyway. Without it, a toggle kept as a variable on an instance that
exists both ON and OFF would be one unchanging leaf, and the census would not
move either. Two limits keep this bounded, and both are visible:

- **One level.** A live handle met *while reading a followed instance* is a
  leaf and is not followed. The scope line counts these as the second number of
  `instRefs=<followed>/<unfollowed>`; anything non-zero there is state this
  snapshot did not read. The unfollowed number is an **upper bound** on that
  unread state (a handle can point at an instance whose variables hold nothing
  that changes), not a disqualifier: a negative quotes it, it does not void it.
- **Its own leaf budget.** Leaves read through a followed handle are charged
  to a separate budget of 250,000 per scope, kept apart from the scope's own
  250,000, and counted on the scope line as `followLeaves=`. A scope that
  exhausts it stops following and says `followTruncated=1`, so a handle with a
  large instance behind it cannot cut a scope short without the line saying
  why. The follow budget is as large as the scope budget on purpose: a single
  followed handle can hold around 200 × 200 leaves, and because the budget is
  per scope, taking scoped snapshots cannot recover a smaller one — a
  `followTruncated=1` on `player` or `global` would rule out a Q3 negative for
  the whole session.
- **Once per instance per snapshot.** An instance already read — a scope root
  (`Player_obj`, `Controller_obj`, …) or one reached earlier by another path —
  is a leaf the second time and counts as neither; its members are in the
  snapshot under the first path. Which path is first can change between
  snapshots if the set of handles changes, so a member moving from one path to
  another shows as removed + added, not as nothing.

A non-struct object value is never asked whether it is an instance (the
runtime's answer for a method value is not known to be safe), and an instance
id stored as a plain number is a number: neither is followed. A non-struct
object whose method does not resolve is the case an instance held as a plain
object would fall into, so the scope line counts those as `objNonStruct=`;
non-zero is state that may not have been read.

**The leaf budget is per scope.** One shared budget let `global`, walked
last, be cut off by whatever the earlier scopes used, at a point that moved
from one snapshot to the next. Each scope now has its own 250,000 leaves and
its own `truncated=` on its line (and `followTruncated=` for the follow
budget); the summary lists `truncatedScopes=`, naming a follow-budget cut as
`<scope>(follow)`, and its `truncated=1` covers either. A truncated scope's
diff is not evidence of anything: every snapshot compared for Q3 must read
`truncated=0` and `followTruncated=0` (see "What a negative means").

**Cost.** A full snapshot is a deliberate one-shot stall (the `citrace symdump`
precedent). The research build's stall watchdog may print a `STALL` line while
it runs — expected and harmless. If `ms=` is above about 15 s, or any scope
reports `truncated=1` or `followTruncated=1`, take scoped snapshots instead.
Snapshots are kept in memory until the plugin unloads; `tgprobe deep drop
<name>` frees one that is no longer needed.

| Command | Does / prints |
|---|---|
| `tgprobe deep snap <name> [scope…] [talent=240,243,252]` | Captures the listed scopes (none = all seven) under `<name>`; re-using a name overwrites it. One line per scope `tgprobe deep snap <name> scope=<scope> names=N read=M unreadable=U leaves=L instRefs=F/U objNonStruct=O followLeaves=FL truncated=0\|1 followTruncated=0\|1` (the global line adds `globalNames=<enum>/<names>`), then `tgprobe deep snap <name>: scopes=… leaves=… unreadable=… truncated=0\|1 truncatedScopes=none\|<list> ms=<elapsed> room=<name>`. `leaves=` on a scope line excludes its `followLeaves=`; the summary's `leaves=` counts both, and `truncatedScopes=` names a follow-budget cut as `<scope>(follow)`. |
| `tgprobe deep diff <a> <b> [substr]` | Leaves changed / added / removed between two snapshots, one line each (`~ <path>: <old> -> <new>`, `+ <path>=<value>`, `- <path> (was <value>)`), in path order, capped at 300 lines, then per-scope counts with each scope's `truncated=`. With `substr`, only paths containing it (case-insensitive) print, and the header adds `filter=<substr> matching=N`; the counts stay totals. Path order puts `global.` after every object root and `census.`, so under per-frame churn a known global line can fall past 300 — check a specific leaf with the filter, never by scrolling the unfiltered diff. |
| `tgprobe deep flip <base> <on> <off>` | Bucket **A**: leaves with `on != base` and `off == base` — flipped and reverted, the shape a toggle state has. Bucket **B**: `on != base`, `off != on`, `off != base` — changed twice, the shape of a cast counter or timestamp. Each capped at 200 lines; both counts printed, and `truncated=1` if any of the three snapshots was. Frame timers and positions churn in every diff; `flip` is what makes the session readable. |
| `tgprobe deep find <substr> [name]` | Case-insensitive search over paths and leaf values of a snapshot (default: the last one taken), cap 200 lines. `find sub`, `find 240`, `find purg` are the Purgatory sub-talent reads. |
| `tgprobe deep get <path>` | Resolves the path **live** and prints the value (and `n=` for a container). Roots: `Player_obj`, `Controller_obj`, `UI_Hud_Talent_obj`, `Skill_Controller_obj#<k>`, `global`, `talent:<id>`; then any of `.name` (a struct field, or a variable of a live instance handle), `[i]`, `{key}` (numeric keys as reals, else strings). A failing segment is named, including a non-numeric index (`bad index`). A global is read the way the snapshot reads it, so a name the snapshot listed is not refused just because `variable_global_exists` says false. Every step is a builtin called by name, so this is the read an indicator can copy from any `self`. |
| `tgprobe deep census` | The current non-zero `instance_number` rows, `<ObjectName>=<n>` (cap 300), with the SDK's name beside the runtime's when they differ. |
| `tgprobe deep selftest` | Builds a fixture from builtins alone (a parsed JSON struct with a nested array and struct, plus a created `ds_map` and a created `ds_list` attached as fields), walks it, checks the five expected leaves, changes three of them (the nested struct field, the map entry, the list element), walks again and expects exactly those three in the diff, then destroys the map and the list. Prints `tgprobe deep selftest: OK leaves=5 changed=3` or `FAIL <which check>`. Then, on its own line, the instance-handle check: a struct holding the live `Controller_obj` instance's handle twice (read only; nothing is written to the instance) must be followed exactly once — both handles leaves, members under exactly one of the two paths, `instFollowed=1`, `instUnfollowed=0`. Prints `tgprobe deep selftest instance: OK followed=1 members=N`, `FAIL <which check>`, or `SKIP no Controller_obj instance`. |
| `tgprobe deep drop <name>` | Frees a snapshot (`tgprobe deep drop <name>: dropped, N snapshot(s) kept`). Snapshots otherwise stay in memory until the plugin unloads, and a full one can hold several hundred thousand leaves. |

**Positive controls.** The deep diff is a new instrument, and a zero from it
is worth nothing until it has produced a non-zero on something known. Five,
all in session 2, three of them inside the measurement's own diffs:

- **C1, mechanics:** `tgprobe deep selftest` → `OK leaves=5 changed=3` **and**
  `tgprobe deep selftest instance: OK followed=1 members=N` with `N > 0`. A
  `FAIL` on the first line means the walker or diff is broken for arrays,
  structs, ds_maps or ds_lists; on the second, that instance handles are not
  followed. `SKIP` on the second line is not OK. Nothing after a failed or
  skipped C1 counts.
- **C2, a nested-array leaf on a known action:** while Purgatory drains HP,
  buff 86 (Martyr) appears in `global.playerBuff[1][0]` (session 1). So
  `deep diff base on playerBuff[1][0][86]` must print a
  `global.playerBuff[1][0][86]` line. Check it with that filter, not in the
  unfiltered diff, where path order can push it past the 300-line cap. If the
  leaf reads `ref instance …`, the same filtered diff must also print
  `global.playerBuff[1][0][86].<member>` lines — the in-game proof that
  handles are followed. If `tgprobe buffs` shows slot 86 alive and the filtered
  diff lists nothing, the walker is blind to nested arrays on this runner:
  stop, fix, rebuild, and record no Q3 negative.
- **C3, the census on a known action:** a Healing Zone cast creates instances
  that live about 700–1150 frames. `deep diff hz0 hz1 census.` must show a
  `census.` row for the object(s) that cast creates (record the name). If not,
  the census scope is blind.
- **C4, instance-member enumeration:** `deep diff base on Player_obj.` must
  print at least one changed leaf that is a **direct** member of the player:
  `Player_obj.<name>` or `Player_obj.alarm[i]`, with no second `.` after the
  member name (session 1's scalar diff saw `Player_obj` variables change
  across a cast). The filter also matches `Player_obj.<handle>.<member>` —
  variables of an instance the player merely points at, read by the follow —
  and those do not count. Zero direct members means the `player` scope read
  nothing that changes, and it contributes no negative.
- **C5, the talent scope:** the `talent` line of every compared snapshot reads
  `read=3`, and `tgprobe deep find <absent: <snapshot>` reports `hits=0`.
  Otherwise the talent structs were not read, and that scope contributes no
  negative.

**What a negative means.** A Q3 `not observed` is a result only if C1–C5 all
fired **and** every compared snapshot (`base`, `on`, `on2`, `off`, `hz0`,
`hz1`, or their scoped splits) reports `truncated=0` and `followTruncated=0`
on every scope line. It
means exactly: *the ON/OFF state is not a leaf reachable from the seven scopes
at depth ≤ 3 (one level deeper under a `talent:<id>` root, whose struct is
itself depth 0) with ≤ 200 elements per container, including one level of
variables on every live instance handle met there, and no live instance count
changes with it* — quoted together with each snapshot's `instRefs=` second
number, the handles that were not followed, and its `objNonStruct=`. A non-zero
unfollowed count is an upper bound on state this snapshot did not read, not a
disqualifier. What it does **not** cover, by
construction: the variables behind an instance handle met while reading a
followed instance (counted in `instRefs=`'s second number); a non-struct object
value that names no method (counted as `objNonStruct=`), which is how an
instance held as a plain object would appear — its variables are not read,
since no instance builtin is called on such a value; a ds_map or ds_list whose handle is stored as a plain number
(only values that describe themselves as `ref ds_map` / `ref ds_list` are
expanded), or an instance id stored as a plain number; a ds_map's entries past
the first 200 in the map's own iteration order, which is not sorted and may
differ between snapshots; a string's content past 120 characters; a real that
changes below the sixth decimal (numbers print with six); and anything inside a
`<container n=N>` leaf beyond its size. A `not observed` with any control
missing, or with any compared scope truncated, is `blocked`.

**Order: runtime read first, Ghidra second.** The deliverable is a path an
indicator can read by name at runtime, and a Ghidra read would still need
`deep get` to confirm its variable live — so running the deep read first can
make the decompiler read unnecessary, while the reverse cannot. The local
Ghidra read of `TalentsWhiteMage`'s talent-240 branch (one native function for
every White Mage talent, variables behind name-slot helpers) is the fallback,
run in the same session only if Q3-D comes back `not observed` with all five
controls fired and no compared scope truncated; only a paraphrase of what it finds may be written here.

## Live procedure

### Session 1

Research DLL in `mods/aurie/`, driven with `ForgePact/tools/ipc.ps1`.
Character: White Mage, Soul Spurn with Purgatory allocated on the hotbar, one
non-toggle skill on the hotbar. Start in town or a cleared zone.

1. **Fresh launch** of the research DLL, with no `bp_ipc\coop.ini` (or one
   with `enabled=0`) and **no `citrace` command at any point in the
   session** (see "How a row attaches": both take rows `tgprobe` needs).
   `hhlabel` → record `hudCalls=`. `tgprobe hook` → expect `N native, 2 via
   hook, 0 blocked, F not found` with `DrawHudBuffs` = `via Hook_DrawHudBuffs
   (native)`, `BuffAdd` = `via HookBuffAdd (native)`, `TalentUse` = `native`,
   `CheckPlayerInteraction` = `native`; record every non-native line verbatim.
   If `TalentUse` reads `via HookTalentUse`, co-op was on — say so in Results
   (its counts are still valid). If any `via` row reads `TABLE-ONLY`, or
   `CheckPlayerInteraction` is not `native`, stop and relaunch: the controls
   must attach. An object-event row that is `not found` means its name did
   not resolve by lookup — not that the object lacks the event. Check
   `Player_obj.Step_0` (the event control): if it is also `not found`, record
   **every event row as `blocked`** (never `not observed`), and Q6 and the
   step 7 negative control rest on the `tgprobe abilities` snapshots and the
   `firstHud=` line only. If co-op rendering or (later) the re-cast guard is
   to be on in this session, turn it on **before** `tgprobe hook`: a
   `TalentUse` install after the probe attached degrades to table-only.
2. Stand still 2 s. Send `hhlabel` and `tgprobe show` together in **one**
   `cmd.txt` write (`tools/ipc.ps1 -Lines`; and `hhlabel` alone at the start
   of the window), so both replies come from one consume rather than two
   polls some frames apart → one control per attach
   mode: `CheckPlayerInteraction` (native) in the thousands;
   `DrawHudBuffs` (via hook) ≈ frames elapsed **and equal (±2) to the
   `hudCalls=` delta between the two `hhlabel` replies** — that agreement is
   what proves the entry-note path counts; `Player_obj.Step_0` in the
   thousands if event rows attached; the two `negctl` rows ideally 0. If
   either control is 0, nothing measured afterwards counts as a negative. Draw
   order (Q4): `tgprobe verbose on`, `tgprobe reset`, wait one frame,
   `tgprobe show`; the log order of the first verbose lines is the draw order.
   `tgprobe verbose off`.
3. Baseline snapshots: `tgprobe slots`, `tgprobe abilities`, `tgprobe buffs`,
   `tgprobe snap Player_obj`, `tgprobe room`.
4. `tgprobe reset`; `tgprobe verbose on`; press Soul Spurn **once**. Within a
   second: `tgprobe show`, `tgprobe abilities`, `tgprobe buffs`,
   `tgprobe diff`, `tgprobe slots` → Q1 (which rows moved +1 with `self` =
   player; what `TalentUse`/`TalentsWhiteMage`/`GetSubTalentInfo` args were),
   Q2/Q3 (a `White_Mage_Soul_Spurn_obj` alive? its alarms and timer-like
   variables? a buff slot? a `Player_obj` scalar?). Wait 10 s, repeat
   `tgprobe abilities` → still alive = toggled ON persists.
5. `tgprobe snap Player_obj`; press Soul Spurn again (off). Same reads → the
   Q3 signal must flip back (effect gone / buff gone / scalar reset); note
   which of `Destroy_0`, `ClearPersistSkill`, `BuffRemove` moved.
6. `tgprobe reset`; press the **non-toggle** skill once → the Q1 row must also
   move (it is a cast) and the Q2/Q3 signals must **not** appear, or appear
   and vanish within its own effect life (record how long).
7. **Negative control, Purgatory off**: switch loadout / reset sub-skill
   points / swap to the second character; `tgprobe reset`; press Soul Spurn
   once; `tgprobe abilities` at 1 s, 3 s, 10 s → the effect must end on its
   own; record its life in seconds and which row (`Destroy_0`,
   `StepAbilityParentDestroyTimer` args, `Alarm_0`) ended it. Restore
   Purgatory. If not feasible, write the reason in Results.
8. `tgprobe reset`; press Soul Spurn (Purgatory) twice as fast as possible →
   Q5: the Q1 row reads 2 with `lastGap` of a few frames, and the toggle ends
   off. Then press once to turn it on again.
9. Hold the Soul Spurn key for 1 s → Q5: does the Q1 row stream per frame?
10. **Zone change (Q6)**: with the toggle ON, `tgprobe room` (record the old
    key), `tgprobe reset`, take a waypoint/portal. As soon as the new zone is
    playable: `tgprobe show` — read the `room=` key (must differ from the
    recorded one), `hudSinceRoomChange=`, the `firstHud=` line, and which of
    `White_Mage_Soul_Spurn_obj` `Destroy_0` / `CleanUp_0`,
    `ClearPersistSkill`, `BuffRemove`, `RoomGoto` moved — then
    `tgprobe abilities`, `tgprobe buffs`, `tgprobe room`. The first-draw
    answer is `firstHud=`: it must read `zone-change` with `room=` equal to
    the new key, and `firstHudSpurnInstances=0` is "OFF on the first draw"
    (a non-zero count there sets Track B's BLOCKED condition; `unreadable`
    makes Q6 `blocked`). If the room key did not change, or `firstHud=` still
    reads `attach` or an older key, the portal did not change the room key
    and Q6 is *not observed*. There is no Room End row (see "Static
    search"); do not add one. Press Soul Spurn once → it must turn ON as a
    fresh cast.
11. If Q2 and Q3 are both empty after 4–7: `tgprobe snap global`, repeat 4–5
    with `tgprobe diff`; read the verbose `TalentUse` / `CheckTalentUse` /
    `GetSubTalentInfo` return values. If still empty: **Ghidra fallback** —
    `citrace symdump` (in a separate session, since `citrace` is excluded from
    this one), `tools/ghidra/ImportSymbols.java`, read `TalentsWhiteMage`'s
    Soul Spurn branch locally for the sub-talent check and what it sets, and
    write only the *paraphrased* mechanism and the variable/argument name it
    reads into this doc.

Paste every quoted line into Results; fill status per Q; write
Decision. Stop the game; nothing else is left running.

### Session 2

Q3 only. Research DLL (`plugin_build\build.bat dev`) in `mods/aurie/`; no
`coop.ini` (or `enabled=0`); **no `citrace` command until step 10**. White
Mage with Soul Spurn + Purgatory and Healing Zone on the hotbar, in town. The
tester reports the toggle's state by eye at every snapshot; Purgatory's HP
drain can turn it off during a 10 s wait, and if it did, the snapshot is
retaken.

1. Fresh launch, town. `hhlabel`; `tgprobe hook` — expect the session-1 shape
   (`36 native, 2 via hook, 0 blocked, 26 not found`, event rows `not found`);
   record the summary line. Stand 2 s; `hhlabel` + `tgprobe show` in one
   `ipc.ps1 -Lines` write; the two controls must hold as in session 1.
2. `tgprobe deep selftest` → must print both `tgprobe deep selftest: OK
   leaves=5 changed=3` and `tgprobe deep selftest instance: OK followed=1
   members=N` with `N > 0` (C1). Record both lines. If either reads `FAIL` or
   the second reads `SKIP`, stop: rebuild (or, for `SKIP`, find out why no
   `Controller_obj` exists in town) before anything else.
3. Toggle OFF (by eye). `tgprobe deep snap base` → record every scope line and
   the summary line. If `unreadable=` is non-zero, note it. **Every scope line
   must read `truncated=0` and `followTruncated=0`** (the summary's
   `truncatedScopes=none`). If a scope reads either as 1, or `ms=` is above
   ~15 s, switch to scoped snapshots for
   the rest (`snap base player talent controller hud skillctl census`, then
   `snap baseg global`, and diff each pair; say which in Results). A scope still
   `truncated=1` when snapshotted alone cannot support a negative: record it,
   and Q3-D can be at best `blocked` for that scope. Check C5 here and on every
   later snapshot: the `talent` line reads `read=3`, and `tgprobe deep find
   <absent: base` reports `hits=0`.
4. `tgprobe reset`; press Soul Spurn **once** (ON by eye). Wait 3 s.
   `tgprobe show` (`TalentUse` +1, `TalentUseClass` ≥ 2). `tgprobe deep snap
   on`. Wait 10 s (still ON by eye; if it self-cancelled, turn it on and
   retake). `tgprobe deep snap on2`. Then the in-diff controls, each through
   the path filter (the unfiltered diff prints 300 lines in path order, and
   `global.` sorts last, so a line missing from it proves nothing):
   - `tgprobe deep diff base on playerBuff[1][0][86]` — must print a
     `global.playerBuff[1][0][86]` line (C2); if that leaf is `ref instance …`,
     it must also print `global.playerBuff[1][0][86].<member>` lines. If
     nothing prints, `tgprobe buffs`: slot 86 alive ⇒ the walker is blind ⇒
     stop.
   - `tgprobe deep diff base on Player_obj.` — must print at least one changed
     direct member, `Player_obj.<name>` or `Player_obj.alarm[i]` with no
     second `.` after the member name (C4); a `Player_obj.<handle>.<member>`
     line is a followed instance's variable and does not count.
   Then `tgprobe deep diff base on` and `tgprobe deep diff on on2` unfiltered —
   leaves that changed base→on but **not** on→on2 are the candidates; if either
   prints `... (N more)`, go through it by scope root with the filter
   (`Controller_obj`, `UI_Hud_Talent_obj`, `Skill_Controller_obj`, `talent:`,
   `global.`) so no line is left unread.
5. Press Soul Spurn until OFF by eye (count presses; session 1 needed up to
   3). Wait 3 s. `tgprobe deep snap off` (every scope `truncated=0` and
   `followTruncated=0`, C5).
   `tgprobe deep flip base on off` → record bucket A and B counts and lines; the
   header must read `truncated=0`. `tgprobe deep diff on2 off`.
6. For each candidate path (bucket A first, cap 10): `tgprobe deep get <path>`
   now (OFF); turn ON; `get` three times over 10 s; turn OFF; `get` three
   times. Record all reads with the by-eye state.
7. Non-toggle control: `tgprobe deep snap hz0`; cast Healing Zone; wait 3 s;
   `tgprobe deep snap hz1` (both `truncated=0` and `followTruncated=0`, C5);
   `tgprobe deep diff hz0 hz1
   census.` — must show a `census.` row for the Healing Zone object(s) (C3);
   and `tgprobe deep diff hz0 hz1 <candidate path>` must **not** show the
   candidate path.
8. `tgprobe deep find sub`, `find 240`, `find purg`, `find toggle`,
   `find active` on `on` — record the Purgatory sub-talent location if it
   appears, and anything the names suggest that `flip` did not surface;
   `deep get` it ON/OFF if so.
9. If a candidate survived 6–7: `Q3-D = measured`. Otherwise `not observed`
   (with C1–C5 quoted as fired, every compared snapshot's `truncated=0` and
   `followTruncated=0`, and each snapshot's `instRefs=` second number and
   `objNonStruct=` — an upper bound on unread state, not a disqualifier) or
   `blocked` (naming the control that did not fire or the scope that
   truncated). Fill the row. Snapshots no longer needed can be freed with
   `tgprobe deep drop <name>`.
10. Only if `Q3-D` is not `measured`: `naddr TalentsWhiteMage` → compare its
    `rva=` with the `TalentsWhiteMage` row of the local `symbols.csv` that the
    Ghidra project was imported from; record `match` or `mismatch` — never the
    address. Keep the game running in town and go to step 11. On mismatch the
    exe changed: `citrace symdump` now (the last command of the session), copy
    the CSV out of `bp_ipc`, and re-import per `ImportSymbols.java`'s header.
11. **Ghidra fallback (local only, only if reached).** In the local project,
    decompile `TalentsWhiteMage` with a local script (kept outside this
    repository). Find the talent-240 branch by its measured fingerprint — it
    is the branch that also dispatches talent 243 with 57, the crows — then
    the Purgatory sub-talent check and what is written on either side of it
    and compared against HP in the step path. Write **only** a paraphrase into
    `Q3-G`: which lookup, which sub-index, which root/variable/field and its
    ON/OFF values. No listing, pseudo-code, screenshot or decompiler function
    address. While the game is still up, confirm with `tgprobe deep get
    <path>` ON and OFF; otherwise the row says `confirmation pending`.
12. Paste everything into Results → Session 2; write Decision → After
    session 2; update the status lines. Stop the game; nothing else left
    running.

### Session 3

The indicator workorder's own live session (issue #11, Track B, P1-LIVE).
Research DLL (`BloodPactPlugin_rel.dll`, `plugin_build\build.bat dev`) copied
over `<game>\mods\aurie\BloodPactPlugin.dll`, previous file backed up; driven
with `ForgePact/tools/ipc.ps1`. White Mage with Soul Spurn + Purgatory and
Healing Zone (the non-toggle skill) on the hotbar, in town. No `bp_ipc\coop.ini`
(or `enabled=0`), no `cooprender`, no `citrace` command at any point in the
session. The tester reports ON/OFF by eye and, for R5, whether a marked
rectangle sits on Soul Spurn's slot.

R1–R10 below are the rows Results → Session 3 fills; each status is exactly
one of `measured`, `not observed` or `blocked` — a row not run is `blocked`,
never `not observed`.

1. **Instrument control (before R1).** `hhlabel` → record `hudCalls=`.
   `tgprobe spurn` → record the summary line (`samples=`, `on=`, `off=`,
   `unreadable=`, `maxN=`, `transitions=`, the last `n=`/`mine=`/`others=`/
   `unattributed=`/`state=`). Stand 2 s; send `hhlabel` and `tgprobe spurn`
   together in one `ipc.ps1 -Lines` write, so both replies come from one
   consume. The `samples=` delta across that window must match the `hhlabel`
   `hudCalls=` delta (±2); otherwise every `spurn` answer in the session is
   `blocked` — quote both deltas in the session preamble.
2. **R1 (by eye OFF).** With Soul Spurn/Purgatory off by eye, `tgprobe spurn`
   → record `state=` and `n=`. Expect `off` with `n=0`.
3. **R2 (by eye ON).** Press Soul Spurn once (ON by eye). Wait 2 s. `tgprobe
   spurn` → record `state=`, `n=`, `mine=`. Expect `on` with `n>=1`,
   `mine>=1`. Cross-check with `tgprobe deep census` (from session 2): the AOE
   object reads `=1` in the same window.
4. **R3 (co-op negative control).** Still ON. `tgprobe spurn as 2` → record
   the printed override line (`state=`, `n=`, `mine=`, `others=`). Expect
   `off` with `others>=1`. This must not change `tgprobe spurn`'s own
   counters — confirm the plain `tgprobe spurn` summary immediately after
   still reads the same `samples=`/`on=`/`off=` it read before this step, plus
   only the draws elapsed since.
5. **R4 (local playerNumber scope).** `tgprobe spurn log on`; wait for the
   next state change (or force one with a press) → record the logged line's
   `playerNumber=`/`isMyClient=` per instance. If the local player's own
   `playerNumber` is readable and equal to the AOE's, `scope: playerNumber`;
   otherwise `scope: BLOCKED` and this plan's `## State` gate is set that way
   (return `PLAN-DEFECT` if `Player_obj` has no `playerNumber` member at all).
   `tgprobe spurn log off`.
6. **R5 (slot location).**
   1. **Draw control, before any candidate.** `tgprobe mark 100 100 400 200`
      (or another obviously-visible GUI spot, clear of any other HUD element)
      → the tester confirms by eye that a red outline rectangle is actually on
      screen at that spot. `tgprobe mark off` or the next `tgprobe spurn` →
      record the printed `markDraws=`/`markDrawExc=` pair. If the rectangle is
      not seen, or `markDraws=0`, or `markDrawExc>0`: **R5 is `blocked (draw
      control)`, not `slotgeom: none`** — drawing from this hook has not been
      shown to work at all, so no candidate result means anything yet. Do not
      continue to step 6.2.
   2. **Candidates.** `tgprobe spurn slots` → record every printed
      `talentId=240` line and its members. For each candidate array/element,
      `tgprobe mark <x> <y> <w> <h>` using that element's own position-shaped
      members (by eye, on the GUI) → the tester says whether the rectangle sits
      on Soul Spurn's button, and record `markDraws=`/`markDrawExc=` for that
      candidate too (a candidate whose own draw threw or never drew is
      `blocked`, not a plain "did not land on the button" negative). Record the
      winning array, id field and position fields as `Slot geometry fields:` in
      "After session 3", or `none` — only once the draw control (6.1) passed
      and at least one candidate actually drew — if no candidate lands on the
      button. `tgprobe mark off` when done.
7. **R6 (OFF lag, Known Limitation).** Note the `TalentUse` press frame from
   `tgprobe hook`'s existing controls (or a fresh `tgprobe show`). Press Soul
   Spurn to turn it OFF; immediately and every ~10 draws until `tgprobe
   spurn` reads `off`, record `state=` and the frame. Record the draw count
   from the press to the On→Off transition.
8. **R7 (zone change).** With the toggle ON, `tgprobe spurn` (record `room=`).
   Take a waypoint/portal. As soon as the new zone is playable: `tgprobe
   spurn` → read the `firstAfterRoomChange:` line (`state=`, `n=`,
   `drawsToOff=`) and keep polling every ~10 draws until it reads `off` or
   600 draws have elapsed. By eye, say whether the toggle actually ended on
   the zone change. If it reads `on` past 600 draws while the tester reports
   the toggle ended, `read: BLOCKED (zone)`; otherwise this row feeds
   `read: GO` alongside R1–R3.
9. **R8 (HP self-cancel, Known Limitation).** With the toggle ON, let
   Purgatory's health drain run until it self-cancels (or trigger it by
   taking damage). Poll `tgprobe spurn` until the state reads `off`; record
   the transition.
10. **R9 (maxN, Known Limitation).** After R2–R9, `tgprobe spurn` → record
    `maxN=` for the session. A double-cast proc (Track A Q5) may have pushed
    it to 2; record whatever it reads.
11. **R10 (optional; Soul Spurn without Purgatory).** Only if the tester can
    respec: remove Purgatory, cast Soul Spurn, `tgprobe spurn` → record
    `state=`/`n=`. Restore Purgatory afterward. If infeasible, record R10 as
    `blocked` and say why.
12. **Gate values.** `read: GO` needs R1, R2 and R3 measured with the
    instrument control (step 1) matched; `read: BLOCKED (zone)` if R7 alone
    fails as described above. `scope: playerNumber` or `scope: BLOCKED` per
    R4. `slotgeom:` the fields recorded in R5.2, or `none` if the draw control
    (R5.1) passed but no candidate landed on the button, or `blocked (draw
    control)` if R5.1 itself did not pass — these are different outcomes and
    must not be collapsed into each other. Paste every quoted
    line into Results → Session 3; write Decision → After session 3; set this
    plan's `## State` gates. Stop the game; nothing else left running.

### Session 4

Replan 1's short session (P1b/P1b-LIVE, issue #11, Track B). Session 3 proved
the read's shape from the draw hook (`n=1` while ON, `0` while OFF), but
never proved `on` there: the per-instance loop only ran from the command
handler (`spurn as 1/2`), because the old `playerNumber` design returned at
"local number unreadable" first. P1b drops that design for the AOE's own
`isMyClient` ("Co-op / ownership after session 3: isMyClient"); this session
proves it from the draw hook, measures the plain-cast `purgatory` value
("Plain-cast flash (R10) and the Purgatory marker") on both sides, and
re-measures R6's OFF lag through `lastTransitionFrame`. One research build
(the P1b `BloodPactPlugin_rel.dll`), one short session, S0–S5 batched.

**Setup.** The P1b research DLL, in town, with no `bp_ipc\coop.ini` (or
`enabled=0`), no `cooprender`, and no `citrace` command at any point.
**Purgatory re-allocated first** — session 3 ended with it respecced out for
R10. Healing Zone stays on the hotbar as the non-toggle control. Run `tgprobe
hook` then `tgprobe reset` before S2.

S0–S5 below are the rows Results → Session 4 fills; each status is exactly
one of `measured`, `not observed` or `blocked` — a row not run is `blocked`,
never `not observed`.

1. **S0 (instrument control, gates every other row).** `hhlabel` → record
   `hudCalls=`. `tgprobe spurn` → record the summary line. Stand a few
   seconds; `hhlabel` and `tgprobe spurn` again, in one `ipc.ps1 -Lines`
   write. The `samples=` delta across that window must match the `hhlabel`
   `hudCalls=` delta (±2) — quote both deltas in the session preamble.
   Otherwise every row below is `blocked`.
2. **S1 (by eye OFF, scope).** With Soul Spurn/Purgatory off by eye, `tgprobe
   spurn` → record `state=` and `n=`. Expect `off` with `n=0`. If this
   contradicts `read: GO` from session 3, record it and set `scope: BLOCKED`.
3. **S2 (by eye Purgatory ON, scope + flash).** Press Soul Spurn once (ON by
   eye, Purgatory active). Wait 2 s. `tgprobe spurn` → record the last sample
   (`state=`, `n=`, `mine=`) and the running `on=`/`markedOn=` counters.
   Expect `on` with `mine>=1`, and both `on=` and `markedOn=` risen since S1.
   `tgprobe spurn fields` → record the snapshot; expect `isMyClient=bool:true`
   and `purgatory` numeric and greater than 0.
4. **S3 (co-op negative control, scope).** Still ON. `tgprobe spurn as
   foreign` → record the printed override line (`state=`, `n=`, `mine=`,
   `others=`). Expect `off` with `others>=1`, `mine=0`. This must not change
   `tgprobe spurn`'s own counters — confirm the plain `tgprobe spurn` summary
   read just before and just after this step is otherwise identical (only the
   draws elapsed between them differ).
5. **S4 (OFF lag, offlag).** Note the `TalentUse`/`TalentsWhiteMage` press
   frames from `tgprobe show`. Press Soul Spurn to turn it OFF; `tgprobe
   spurn` → record `lastTransitionFrame=` and confirm `state=off`. `offlag:`
   is `lastTransitionFrame` minus the `TalentUse` press `lastFrame`, in
   frames, with the `TalentsWhiteMage` frame recorded beside it. If this row
   is not run, `offlag:` is `not isolated (<= ~4560 draws, session 3 R6)`.
6. **S5 (plain-cast flash, flash).** Respec Purgatory out. Cast Soul Spurn
   once; wait at least 3 s. `tgprobe spurn` → record the state and counters.
   `tgprobe spurn fields` → record the new appearance's snapshot, both
   "first" and "last". `flash: purgatory` needs S2's snapshot `purgatory` > 0,
   this row `measured` with the new appearance's `purgatory` numeric and ≤ 0
   in both "first" and "last", and `on=` risen across this row while
   `markedOn=` did not. Anything else — including this row `blocked` — is
   `flash: KL`; record which case applied. Restore Purgatory afterward if
   practical.
7. **Gate values.** `scope: isMyClient` needs S0 matched and S1, S2 and S3 all
   `measured` as described; otherwise `scope: BLOCKED`, which stops P2 (`##
   Needs human judgement`). `flash:` and `offlag:` per steps 6 and 5 above.
   `read: GO` and `slotgeom:` are not re-decided here. Paste every quoted line
   into Results → Session 4; write Decision → After session 4; set this
   plan's `## State` gates. Stop the game; nothing else left running.

## Results

### Session 1


Session 1, 2026-09-17: research build `81c0f67` (`BloodPactPlugin_rel.dll`),
White Mage, Soul Spurn + Purgatory on the hotbar, Healing Zone (E) as the
non-toggle skill, town (`Town_05_rm`), no `coop.ini`, no `citrace`. The tester
reported the on/off state by eye after every press. Status is exactly one of
`measured`, `not observed`, `blocked`.

**Attach and controls.** `tgprobe hook: 36 native, 2 via hook, 0 blocked, 26
not found`, with `BuffAdd: via HookBuffAdd (native)` and `DrawHudBuffs: via
Hook_DrawHudBuffs (native)`. All 8 `UI_Hud_Talent_obj` closures and **all 18
object-event rows were `not found` (st=14), including the `Player_obj.Step_0`
control**, so every event row is `blocked`. This repeats the pet-quest session-7
result: raw `gml_Object_*` event names do not resolve by name on this build.
Script controls held throughout. In the same `tgprobe show` (frame=36120),
`CheckPlayerInteraction(control) calls=43200` and `DrawHudBuffs calls=4320`,
and `DrawHud`/`DrawHudAbilityButtons` also read 4320. After a
`hhlabel`+`tgprobe reset` batch, a 3 s window read `DrawHudBuffs calls=570`,
`CheckTalentUse calls=570`, `CheckPlayerInteraction calls=5700` (10 per frame).

| Q | Question | status | Evidence |
|---|---|---|---|
| Q1 | Which routine runs once per toggle press, with what `self` and args | measured | Each tap of Soul Spurn is one `TalentUse` with `self=Player_obj`, `argc=5 a0=<player ref> a1=240 a2=1 a3=false a4=true`. About 19 frames later comes one `TalentUseClass` with `self=Player_obj a0=240 a4=true a5=0 a6=-1 a7=-1`, then `TalentsWhiteMage` (argc=0) and `GetTalentCooldown(240,1)`. Healing Zone takes the same path with talent **252**. Every player cast of 240 also chains `TalentUseClass a0=243 a4=false a5=57` in the same frame, which spawns 3 `White_Mage_Malediction_Crow_obj`. One cast also chained `a0=737 a5=10` (a Healing Zone proc). |
| Q2 | What distinguishes a Purgatory-toggled Soul Spurn from the same skill without Purgatory, and from a non-toggle skill | not observed | The cast path is identical for 240 and 252, and the 243 chain plus the crows appear on casts that turned the toggle ON and on casts that turned it OFF (frames 102993 ON and 107147 OFF). So no routine, argument or spawned object seen here marks "toggle". The only discriminator in hand is the talent id (240) plus the Purgatory sub-talent, which was not read. The Purgatory-off control was not run. Controls in the same show: `CheckPlayerInteraction calls=49800`, `DrawHudBuffs calls=4980` (frame 110040). |
| Q3 | Where the on/off state lives, whether it stays flipped while on, and whether one read from the draw hook can see it | not observed | Read in both directions (tester-confirmed OFF→ON with 1 press, ON→OFF with 3 presses) and found in none of these: `Player_obj` scalars (`tgprobe diff` changed only `depthUpdater`, `hpArrayPos`, `manaArrayPos`, `socketValidateTimer`, `updateMinimap`, `yDepthSet`); global scalars (253; `diff` changed only `cpr_seed`, `current_frames`, `deltaSpd`, `repeatGravity`); `UI_Hud_Talent_obj` instance variables (no change); `Player_Ability_Parent_obj` descendants (`instances=0` while ON, because the crow and Healing Zone objects expire after about 700–1150 frames). `playerBuff[1][0]` gains **buff 86** while ON, with `destroyTimer` held near 537–563, but the tester identified it as the **Martyr** passive, which any damage triggers (including Purgatory's health drain). So it follows the drain, not the toggle. **Not examined:** arrays, structs and ds_maps (the snapshot compares scalars only), for example the player's talent data. Controls in the same show: `CheckPlayerInteraction calls=47100`, `DrawHudBuffs calls=4710` (frame 81540). |
| Q4 | Where the slots draw: whether `DrawHudBuffs` runs after `DrawHudAbilityButtons`, slot geometry, which slot variable names the talent | measured | Each frame is ordered `DrawHud` → `DrawHudAbilityButtons(1, 1986.5)` → `DrawHudBuffs(1, 1)`, all with `self=Controller_obj`, so drawing after `DrawHudBuffs` lands on top of the buttons. There is one `UI_Hud_Talent_obj` (x=0, y=0, `width=2035.8`, `height=232`, `buttonXOffset=43`, `buttonYOffset=48`, `row0X=104`, `row1X=5`, `rows=2`, `buttonScale=1`), on a GUI of 3840×2088. Buttons live in the `row0`/`row1`/`grid` arrays and were not expanded, so it is not observed which slot variable names the talent. |
| Q5 | Whether an accidental double-press is two cast calls, and whether a held key streams calls | measured | There are **three** sources. (a) **The double-cast proc:** `TalentUseClass` from `self=Universal_Double_Cast_obj`, `a0=240 a4=false a6/a7=<world x,y>`, 36–56 frames after the player's cast, **with no `TalentUse` call**. It was seen on 3 casts (frames 33519, 78330, 107183), and the tester twice reported the toggle ending in the wrong state after a proc. (b) **A held key auto-repeats** `TalentUse` every 57 frames (4 calls in about 1 s: frames ~94859, 94916, 94972, 95029), so holding flips the toggle repeatedly. (c) Real re-presses: 3 separate `TalentUse` calls were needed to turn it OFF once, with gaps of 308–354 frames. Whether a proc re-cast always flips the state is not observed: at frame 107183 the tap plus the proc ended OFF. |
| Q6 | What a zone change does to the state and in what order; is the state OFF on the first `DrawHudBuffs` in the new zone | blocked | Not run. The first-draw read counts `White_Mage_Soul_Spurn_obj` instances, but Q3 showed the toggle keeps no such instance (`abilities instances=0` while ON), so a 0 there would measure nothing (see the N-a note in the workorder). The tester reports that the toggle ends on zone change **and when health falls too low** (observed: the first cast of the session turned itself off at low health). |

### Session 2

Session 2, 2026-09-17: research build `fbb9873` (v1.4.1 base,
`BloodPactPlugin_rel.dll`), White Mage, Soul Spurn + Purgatory and Healing
Zone on the hotbar, town (`Town_05_rm`), no `coop.ini`, no `citrace` (step
5.10 was not reached — see Q3-G). The tester reported the on/off state by eye
at every snapshot. Status is exactly one of `measured`, `not observed`,
`blocked`; a `not observed` quotes C1–C5 as fired and every compared snapshot
as `truncated=0` and `followTruncated=0`, otherwise it is `blocked`.

**Attach and controls.** `tgprobe hook: 36 native, 2 via hook, 0 blocked, 26
not found` — the session-1 shape, with `CheckPlayerInteraction(control)
mode=native` and `DrawHudBuffs mode=via Hook_DrawHudBuffs (native)` both
holding, cross-checked against the `hhlabel` `hudCalls=` delta across the two
replies in the same batch (`3869` → `4049`).

**C1 (mechanics).** `tgprobe deep selftest: OK leaves=5 changed=3` — FIRED.
`tgprobe deep selftest instance: FAIL instFollowed=1 instUnfollowed=3 (want
1/0)` — **not fired**: the check expects 0 unfollowed instance handles, but
`Controller_obj` holds handles one level too deep for
`TgProbeDeepSelfTestInstance` to follow by design (`ModuleMain.cpp`,
`TgProbeDeepSelfTestInstance`). This is a check-expectation defect in the
selftest, not a walker defect — it does not affect the measured Q3-D row
below, but it would block any future `not observed` verdict until fixed (not
done in this round; a follow-up).

**Snapshots.** Every `deep snap` summary line (all eight compared snapshots
read `truncated=0`, `truncatedScopes=none`, `unreadable=0`; `followTruncated=0`
on every per-scope line, `ms=` 281–328):

```
tgprobe deep snap base: scopes=player,talent,controller,hud,skillctl,census,global leaves=125577 unreadable=0 truncated=0 truncatedScopes=none ms=281 room=Town_05_rm
tgprobe deep snap on: scopes=player,talent,controller,hud,skillctl,census,global leaves=125635 unreadable=0 truncated=0 truncatedScopes=none ms=297 room=Town_05_rm
tgprobe deep snap on2: scopes=player,talent,controller,hud,skillctl,census,global leaves=125637 unreadable=0 truncated=0 truncatedScopes=none ms=297 room=Town_05_rm
tgprobe deep snap off: scopes=player,talent,controller,hud,skillctl,census,global leaves=125600 unreadable=0 truncated=0 truncatedScopes=none ms=328 room=Town_05_rm
tgprobe deep snap hz0: scopes=player,talent,controller,hud,skillctl,census,global leaves=125599 unreadable=0 truncated=0 truncatedScopes=none ms=297 room=Town_05_rm
tgprobe deep snap hz1: scopes=player,talent,controller,hud,skillctl,census,global leaves=125594 unreadable=0 truncated=0 truncatedScopes=none ms=297 room=Town_05_rm
tgprobe deep snap hz2: scopes=player,talent,controller,hud,skillctl,census,global leaves=125593 unreadable=0 truncated=0 truncatedScopes=none ms=297 room=Town_05_rm
tgprobe deep snap hz3: scopes=player,talent,controller,hud,skillctl,census,global leaves=125601 unreadable=0 truncated=0 truncatedScopes=none ms=313 room=Town_05_rm
```

Each snapshot's `global` scope line carries `globalNames=` (enumerated/named),
e.g. `base` and `off`: `globalNames=3555/3553` and `globalNames=3556/3554`;
`on`, `on2`, `hz0`–`hz3`: `globalNames=3556/3554` throughout.

**C2 (container leaf on Purgatory's drain, in the ON diff itself).**
`tgprobe deep diff base on playerBuff[1][0][86]` → summary
`tgprobe deep diff base on: changed=247 added=143 removed=85 truncated=0
filter=playerBuff[1][0][86] matching=34`, first line `~
global.playerBuff[1][0][86]: real:-4.000000 -> kind=15 str=ref instance
261681` — FIRED (plus 33 `+` lines for the buff instance's own followed
members, e.g. `buffType=int64:86`, `destroyTimer=real:455.161248`).

**C3 (census on a known action).** First try weak: `tgprobe deep diff hz0
hz1 census.` → `tgprobe deep diff hz0 hz1: changed=122 added=3 removed=8
truncated=0 filter=census. matching=1`, only line `+
census.Cooldown_Over_obj=1` — not a Healing Zone object, so **C3 not fired**
on this pair (the hz0/hz1 window was timed too late relative to the cast).
Retaken as `hz2`/`hz3`: `tgprobe deep diff hz2 hz3 census.` → `tgprobe deep
diff hz2 hz3: changed=133 added=51 removed=43 truncated=0 filter=census.
matching=42`, lines `+ census.White_Mage_Healing_Zone_obj=1` and `+
census.Player_Ability_Parent_obj=1` — **FIRED** on the retake.

**C4 (direct member changed by a known action).** `tgprobe deep diff base on
Player_obj.` → summary `tgprobe deep diff base on: changed=247 added=143
removed=85 truncated=0 filter=Player_obj. matching=147`, line `~
Player_obj.hpArrayPos: real:1.000000 -> real:2.000000` — FIRED.

**C5 (positive control on talent scope and the absence sentinel).** Every
`deep snap` talent scope line reads `scope=talent names=3 read=3
unreadable=0` (no `<absent:` leaves in the read set). `tgprobe deep find
'<absent:' in on: hits=0` — FIRED.

| Q | Question | status | Evidence |
|---|---|---|---|
| Q3-D | Where the ON state lives, read from non-scalar runtime storage (`tgprobe deep`) | measured | Path: `census.White_Mage_Soul_Spurn_AOE_obj` (`GameObject::White_Mage_Soul_Spurn_AOE_obj` = SDK index 5759; the runtime object index itself was never printed this session). Found by `tgprobe deep flip base on off` → `tgprobe deep flip base on off: A(flipped and reverted)=41 B(changed twice)=221 truncated=0`, bucket-A line `census.White_Mage_Soul_Spurn_AOE_obj: base=<absent> on=1 off=<absent>` (bucket A also holds `census.Player_Damage_Parent_obj: base=<absent> on=1 off=<absent>`, the object's parent class — not a distinct signal). Since the measured path is a `census.<Object>` leaf, `tgprobe deep get` cannot read it (`deep get` resolves scoped struct/array/ds paths, not the census map); per the driver amendment, three `tgprobe deep census` reads stand in for it on each side — **the six quoted reads below are excerpts of the log, not the full census output.** **ON** (by eye ON, 10 s window; each read's own full `nonzero=` count — 181/161/179 — covers every nonzero object, but only the two relevant rows are shown): frame 34740 → `White_Mage_Soul_Spurn_AOE_obj=1` (`Player_Damage_Parent_obj=1` alongside); frame 35430 → `=1`; frame 36150 → `=1`. **OFF** (by eye OFF after 1 press): frame 39540, frame 40260 and frame 40950 each print only the header line `nonzero=177`, with no object rows in the log; absence of the row is *inferred* from that count matching the fully-listed baseline census at frame 27210 (177 rows enumerated in full, `White_Mage_Soul_Spurn_AOE_obj` not among them), not read directly at those three frames. Non-toggle control: `tgprobe deep diff hz2 hz3 Soul_Spurn` → `tgprobe deep diff hz2 hz3: changed=133 added=51 removed=43 truncated=0 filter=Soul_Spurn matching=0` — hz2/hz3 is the pair where C3 (the census positive control) fired, so it is the valid non-toggle control; the path does not change when Healing Zone is cast in that pair. (`tgprobe deep diff hz0 hz1 Soul_Spurn` also read `matching=0`, but hz0/hz1 is the pair where C3 did **not** fire, so it is not used as a control here — see C3 above.) Neither pair shows the AOE instance exists *only* because of the toggle; both show only that casting Healing Zone does not itself change this path. Read = `instance_number(asset_get_index(GetObjectName(GameObject::White_Mage_Soul_Spurn_AOE_obj))) > 0`; ON=1, OFF=0. The census reads above use a loop index over the object range plus `object_get_name`, not this shape directly on `White_Mage_Soul_Spurn_AOE_obj`. But the shape itself — `CallBuiltin("asset_get_index", …)` then `CallBuiltin("instance_number", …)`, by name (`TgProbeCountByName`, `ModuleMain.cpp:14916-14925`, invoked at `:14941-14942` from `TgProbeHudRoomTick`) — **did run this session**, on `White_Mage_Soul_Spurn_obj` and `Player_Ability_Parent_obj` (not the AOE object). No `self` was supplied to either call: `CallBuiltin` is the two-argument form YYTK documents as running in **the global context**, takes no `CInstance*` and never forwards the detour's `self` (`YYTK_Shared_Interface.hpp:66-74`; the `self`-taking form is the separate `CallBuiltinEx` at `:85`, not used here). So the only context this exercised is the global instance — no `self` value, `Controller_obj` or otherwise, has been supplied to this read yet, even though the call happens inside a `DrawHudBuffs` detour whose own GML `self` is `Controller_obj` (Q4). Timing: `TgProbeCountByName` runs only from the key-change branch of `TgProbeHudRoomTick` (`:14931-14943`; the per-frame part is just an interlocked counter at `:14945`), and the room key never changed this session — `firstHud=attach (not a zone change) room=4131119309451652171 frame=5400 firstHudSpurnInstances=0 firstHudAbilityInstances=0` is one snapshot taken once, at room attach, printed unchanged in three later `tgprobe show` replies (session2.log lines 127, 224, 1255; the rising `hudSinceRoomChange` in the same replies — 30, 11550, 43470 — is the separate per-frame counter, not a re-sample). So the shape ran **exactly twice**, once per object, at frame 5400. No cast had happened yet at that point: the same `tgprobe show` reply that samples `firstHud` also reads `TalentUse mode=native calls=0 lastFrame=0` at `frame=5430` (session2.log line 62); the first Soul Spurn cast counted after `tgprobe reset` (log line 150) is later — `TalentUse mode=native calls=1 lastFrame=14796` / `TalentsWhiteMage mode=native calls=1 lastFrame=14815` (log lines 159, 166). The tester's by-eye OFF note (log line 136) comes before both the reset and that first counted cast, so it corroborates OFF after frame 5400 and before the first counted cast, not exactly at frame 5400. `0` is consistent with that: `White_Mage_Soul_Spurn_obj` never appears as a non-zero census row anywhere in this session, and `Player_Ability_Parent_obj`'s absence rests on the fully enumerated census at frame 27210 (session2.log 757-935, `Player_Ability_Parent_obj` not among its 177 rows) and on its absence from both `deep flip base on off` buckets (log 490-737, A=41/B=221) — it is absent from all three ON census reads (34740/35430/36150) and all three OFF reads (39540/40260/40950), first showing up as `+ census.Player_Ability_Parent_obj=1` only much later, during the `hz2`/`hz3` Healing Zone cast (evidence about that cast, not about frame 5400). But neither object's own instance count was independently checked at frame 5400 itself, so `0` is consistent with, not proof of, the state at that moment. Both calls returned a number rather than the bare `unreadable` token (or `n/a` when no snapshot has been taken yet — `ModuleMain.cpp:15176`, `:15181-15182`), so the shape resolves in the global context at that one sample point; it shows nothing more than that. What it never returned this session, on any object, in any context, is a **non-zero**. The indicator workorder's positive control therefore cannot be "does the call return" — it must be an ON=1 read through this exact shape on `White_Mage_Soul_Spurn_AOE_obj`, in whatever context (global or a specific `self` via `CallBuiltinEx`) the indicator actually uses, which this session never obtained. Not the toggle: `Player_obj.playerEffect[182]` went `real:0.000000 -> int64:2` (`~ Player_obj.playerEffect[182]: real:0.000000 -> int64:2`, `deep diff base on Player_obj.`) on the first cast and stayed `2` after turning OFF (`tgprobe deep get Player_obj.playerEffect[182] = int64:2 frame=20550` while ON, `= int64:2 frame=25290` while OFF) — stays `2` while OFF; what it means is not established. The AOE instance's own variables while ON (`tgprobe vars White_Mage_Soul_Spurn_AOE_obj`): `activated=bool:true`, `purgatory=real:0.090000`, `purgatoryTimer=real:105.73`, `tick_frequency=180`, `tickNumber=19`. |
| Q3-G | The same, from the local Ghidra read of `TalentsWhiteMage`'s talent-240 branch, paraphrased | not run — Q3-D measured | Step 5.10 (`naddr TalentsWhiteMage` / Ghidra fallback) was not reached: Q3-D came back `measured` with all controls fired, so per §5 the Ghidra pass is skipped. No `citrace` command was sent this session. |

**Purgatory sub-talent level:** the storage exists, but which field is
Purgatory's level is **not identified**. `tgprobe deep find purg on` →
`hits=1`, the only match a quest string
(`Controller_obj.questlogDescription[5]=string:"Purge Tarethiel of evil for
good.."`), unrelated. `tgprobe deep find sub on` (hits=223, first 200 shown)
and `tgprobe deep find 240 on` (hits=317, first 200 shown, repeats the same
values) both surfaced two candidate paths for talent 240's sub-talents:
`global.subTalentMap[1].t240.s2=real:2.000000`,
`global.subTalentMap[1].t240.s6=real:2.000000`,
`global.subTalentMap[1].t240.s7=real:5.000000`,
`global.subTalentMap[1].t240.s9=real:5.000000`,
`global.subTalentMap[1].t240.s10=real:3.000000`,
`global.subTalentMap[1].t240.s12=real:3.000000` (six sub-index slots, values
2/2/5/5/3/3), and
`UI_Hud_Talent_obj.playerSlot{subTalentMap}[0].t240=<container n=6>` (the
same six-entry map, mirrored on the HUD slot object). Neither `deep find`
nor anything else run this session names which `sN` corresponds to Purgatory
— the map has no key-to-ability-name lookup in the scopes read, and
`ReturnSubTalentLevel`'s own index argument (session-1 Decision item 2) was
not called this session. `tgprobe deep find toggle on` (hits=0) and `tgprobe
deep find active on` (hits=374, first 200 shown) did not add a candidate.
`talent:240` itself resolves to the static talent-definition struct (no
level field) — the *per-player* level lives in `subTalentMap`, not there.
The live `White_Mage_Soul_Spurn_AOE_obj` instance separately carries
`purgatory=real:0.090000` while ON (see the Q3-D row); whether that value is
read from one of the six `subTalentMap` slots is not established.

**By-eye caveats:** the `on2` state was not explicitly re-confirmed by the
tester before the OFF presses began; the first OFF attempt took 2 presses,
the second took 1.

### Session 3

Session 3, 2026-09-18: research build `d64ff41` (`BloodPactPlugin_rel.dll`,
built 20:08:19, hash-verified in `mods\aurie`), White Mage "Sorak" with Soul
Spurn + Purgatory and Healing Zone on the hotbar; town (`Town_05_rm`), then a
waypoint to `Act_05_01`. No `bp_ipc\coop.ini`, no `cooprender`, no `citrace`
command at any point in the session. Driven with `ForgePact/tools/ipc.ps1`;
the tester reported ON/OFF and slot placement by eye. Full verbatim output:
`.claude/workorders/forgepact-toggle-indicator-session3.log` (a hub workorder
artefact, not part of this submodule).

**Instrument control (before R1).** Two `hhlabel` + `tgprobe spurn` pairs, 2 s
apart:

```
hhlabel -> ON (0 active, callback ok) hudCalls=2728 draws=0 playerId=-1 offset=150 lastErr=
tgprobe spurn: frame=3870 room=4131119309451652171 n=0 mine=0 others=0 unattributed=0 capped=0 localNumber=unreadable state=off samples=2728 on=0 off=2728 unreadable=0 maxN=0 transitions=0 markDraws=0 markDrawExc=0
```
```
hhlabel -> ON (0 active, callback ok) hudCalls=3148 draws=0 playerId=-1 offset=150 lastErr=
tgprobe spurn: frame=4290 room=4131119309451652171 n=0 mine=0 others=0 unattributed=0 capped=0 localNumber=unreadable state=off samples=3148 on=0 off=3148 unreadable=0 maxN=0 transitions=0 markDraws=0 markDrawExc=0
```

`hudCalls` `2728`→`3148` (+420) matches `samples` `2728`→`3148` (+420)
exactly — the sampler control passes for this session.

| Row | Status | Evidence |
|---|---|---|
| R1 | measured | By eye OFF (confirmed before the session). `tgprobe spurn: frame=4290 room=4131119309451652171 n=0 mine=0 others=0 unattributed=0 capped=0 localNumber=unreadable state=off samples=3148 on=0 off=3148 unreadable=0 maxN=0 transitions=0 markDraws=0 markDrawExc=0`. |
| R2 | measured | By eye ON (tester pressed Soul Spurn once). `tgprobe spurn: frame=9750 room=4131119309451652171 n=1 mine=0 others=0 unattributed=0 capped=0 localNumber=unreadable state=unreadable samples=8608 on=0 off=6854 unreadable=1754 maxN=1 transitions=3 markDraws=0 markDrawExc=0`; `tgprobe deep census: objects=6017 nonzero=164 unreadable=0 frame=9750` lists `White_Mage_Soul_Spurn_AOE_obj=1`. The row's expected outcome ("`spurn` On, n≥1, mine≥1") did not hold: the sampler reached `n=1` and the census confirms the AOE instance at `1`, but `localNumber=unreadable`, so `state=unreadable`, never `on` — see R4. |
| R3 | measured | `tgprobe spurn as 1 -> on n=1 mine=1 others=0 unattributed=0`; `tgprobe spurn as 2 -> off n=1 mine=0 others=1 unattributed=0`. The plain counters are unchanged by the override: the `tgprobe spurn` reads immediately before and after both read `samples=15448 on=0 off=6854 unreadable=8594 maxN=1 transitions=3` (identical). |
| R4 | measured | `tgprobe deep get Player_obj.playerNumber: FAILED segment .playerNumber: no such instance variable`. Full player-scope walk: `tgprobe deep snap p1 scope=player names=116 read=116 unreadable=0 leaves=1092 instRefs=21/1 objNonStruct=0 followLeaves=673 truncated=0 followTruncated=0 note=instances=1`. Substring search: `tgprobe deep find 'playernumber' in p1: hits=1` → `Player_obj.myHealthBar.playerNumber=real:1.000000` (a followed instance handle, not a direct `Player_obj` field). `tgprobe vars White_Mage_Soul_Spurn_AOE_obj` reads `playerNumber=real:1.000000` and `isMyClient=bool:true` on the AOE instance. `Player_obj` has no `playerNumber` member at all; the only hit for the ownership key names a different field, `myHealthBar.playerNumber`, one level removed from `Player_obj` itself. `tgprobe spurn log on` was also issued (during R6 setup) but its per-instance log lines were not observed in the replies read; R4's evidence above comes from `tgprobe vars`/`tgprobe deep` instead. |
| R5 | measured | Draw control: `tgprobe mark -> x=100.000000 y=100.000000 w=400.000000 h=200.000000 (watch \`tgprobe spurn\` or the next \`tgprobe mark off\` for draws=/drawExc=)`; tester: rectangle SEEN by eye; the next `tgprobe spurn` read `markDraws=270 markDrawExc=0`; `tgprobe mark -> off draws=4470 drawExc=0`. Candidates: `tgprobe spurn slots: row0[5] talentId=240 members: ... drawButton=bool:true hidden=bool:false ... navBboxHeight=real:139.200000 navBboxWidth=real:124.700000 navBboxX=real:385.700006 ... navBboxY=real:1711.000000 ...` and `row1[5] talentId=240 members: ... hidden=bool:true drawButton=bool:false ...`; `tgprobe spurn slots: playerSlot.bind_skill is undefined`; `tgprobe spurn slots: global.mySkills[3]=240`. Candidate A: `tgprobe mark -> x=385.700000 y=1711.000000 w=124.700000 h=139.200000 ...`; tester: the rectangle SITS ON Soul Spurn's button (by eye). `Slot geometry fields: row0[5].talentId + navBboxX/navBboxY/navBboxWidth/navBboxHeight`. |
| R6 | measured | `tgprobe show` at the press: `TalentUse mode=native calls=1 lastFrame=31247 lastGap=0`, `TalentUseClass mode=native calls=1 lastFrame=31266 lastGap=0`, `TalentsWhiteMage mode=native calls=1 lastFrame=31266 lastGap=0`. Bracketing `tgprobe spurn` reads: `frame=28500 ... state=unreadable ... off=6854 ... transitions=3` (before the press resolved) and `frame=33060 room=4131119309451652171 n=0 mine=0 others=0 unattributed=0 capped=0 localNumber=unreadable state=off samples=31918 on=0 off=8648 unreadable=23270 maxN=1 transitions=4 markDraws=4530 markDrawExc=0` (after). The On→Off transition landed somewhere inside that ~4560-draw window, which contains both the press (`lastFrame=31247`) and the cast resolving (`lastFrame=31266`); no `tgprobe spurn` sample was taken between those two frames, so the exact draw count from press to OFF is not isolated further than the window itself. |
| R7 | measured | `tgprobe show: RoomGoto mode=native calls=1 lastFrame=49084 lastGap=0`; `firstHud=zone-change room=9148364097822447407 frame=49106 firstHudSpurnInstances=0 firstHudAbilityInstances=0`; `tgprobe spurn: frame=51270 room=9148364097822447407 n=0 mine=0 others=0 unattributed=0 capped=0 localNumber=unreadable state=off samples=50106 on=0 off=26272 unreadable=23834 maxN=1 transitions=6 markDraws=4530 markDrawExc=0` with `firstAfterRoomChange: state=off n=0 drawsToOff=0`; `tgprobe room: key=9148364097822447407 readable=yes name=Act_05_01`. Tester: the toggle ended on the zone change (by eye). `drawsToOff=0` is well under the 600-draw cap, so this row feeds `read: GO` alongside R1–R3. |
| R8 | measured | Tester turned Soul Spurn ON in `Act_05_01`, let the drain run to self-cancel (no press). `tgprobe spurn: frame=60030 room=9148364097822447407 n=0 mine=0 others=0 unattributed=0 capped=0 localNumber=unreadable state=off samples=58866 on=0 off=33164 unreadable=25702 maxN=1 transitions=8 markDraws=4530 markDrawExc=0`. Compared with R7's read: `unreadable` rose `23834`→`25702` (+1868) and `transitions` rose `6`→`8` (+2, one On, one Off) while `TalentUse mode=native calls=3 lastFrame=56729 lastGap=8228` recorded only the single ON press (up from `calls=2` at R7) — no second `TalentUse` call for the Off side, consistent with a self-cancel rather than a press. |
| R9 | measured | `maxN=1` on every `tgprobe spurn` read across the whole session, from the first (`frame=3870 ... maxN=0`, before any cast) through the last (`frame=72660 ... maxN=1`); no double-cast proc pushed it to 2 this session. |
| R10 | measured | Tester removed Purgatory (respec) and cast Soul Spurn once: `TalentUseClass mode=native calls=6 lastFrame=69364 lastGap=0`, up from `calls=4 lastFrame=56748` at R8. `tgprobe spurn: frame=72660 room=9148364097822447407 n=0 mine=0 others=0 unattributed=0 capped=0 localNumber=unreadable state=off samples=71496 on=0 off=45653 unreadable=25843 maxN=1 transitions=10 markDraws=4530 markDrawExc=0`. Compared with R8's read: `unreadable` rose `25702`→`25843` (+141) and `transitions` rose `8`→`10` (+2). Since `localNumber` stayed `unreadable` for the whole session, a non-Purgatory cast of Soul Spurn creates the same `White_Mage_Soul_Spurn_AOE_obj` instance the toggled cast does — counted as `unreadable`, not `on`, for about 141 of the polled draws before the instance goes away again. |

### Session 4

Session 4, 2026-09-18: the P1b research build (`BloodPactPlugin_rel.dll`,
built 21:54:09 from ForgePact `8fcf2fe`, hash-verified), town
(`Town_05_rm`, room key `4131119309451652171`, held for the whole
session — no zone change this time), White Mage with Soul Spurn +
Purgatory (re-allocated before S1) and Healing Zone on the hotbar. No
`bp_ipc\coop.ini`, no `cooprender`, no `citrace` command at any point. Driven
with `ForgePact/tools/ipc.ps1`; the tester reported ON/OFF by eye for S0/S1/
S2/S4. Full verbatim output:
`.claude/workorders/forgepact-toggle-indicator-session4.log` (a hub workorder
artefact, not part of this submodule).

**Instrument control (S0, before S1).** Two `hhlabel` + `tgprobe spurn`
pairs, 450 draws apart:

```
hhlabel -> ON (0 active, callback ok) hudCalls=3386 draws=0 playerId=-1 offset=150 lastErr=
tgprobe spurn: frame=4920 room=4131119309451652171 n=0 mine=0 others=0 unattributed=0 capped=0 state=off samples=3386 on=0 off=3386 unreadable=0 maxN=0 transitions=0 lastTransitionFrame=-1 markedOn=0 markedOff=3386 markedUnreadable=0 markDraws=0 markDrawExc=0
```
```
hhlabel -> ON (0 active, callback ok) hudCalls=3836 draws=0 playerId=-1 offset=150 lastErr=
tgprobe spurn: frame=5370 room=4131119309451652171 n=0 mine=0 others=0 unattributed=0 capped=0 state=off samples=3836 on=0 off=3836 unreadable=0 maxN=0 transitions=0 lastTransitionFrame=-1 markedOn=0 markedOff=3836 markedUnreadable=0 markDraws=0 markDrawExc=0
```

`hudCalls` `3386`→`3836` (+450) matches `samples` `3386`→`3836` (+450)
exactly — the sampler control passes for this session.

| Row | Status | Evidence |
|---|---|---|
| S0 | measured | See the control pair above: `hudCalls` and `samples` both rose `3386`→`3836` (+450). |
| S1 | measured | By eye OFF (Purgatory just re-allocated, Soul Spurn not yet pressed). `tgprobe spurn: frame=5370 ... n=0 mine=0 others=0 unattributed=0 capped=0 state=off samples=3836 on=0 off=3836 unreadable=0 maxN=0 transitions=0 lastTransitionFrame=-1 markedOn=0 markedOff=3836 markedUnreadable=0`. |
| S2 | measured | Tester pressed Soul Spurn once (ON by eye, Purgatory active), waited 2 s. `tgprobe spurn: frame=10920 ... n=1 mine=1 others=0 unattributed=0 capped=0 state=on samples=9386 on=1793 off=7593 unreadable=0 maxN=1 transitions=1 lastTransitionFrame=9127 markedOn=1793 markedOff=7593 markedUnreadable=0` — both `on=` and `markedOn=` rose from S1's `0`. `tgprobe spurn fields: appearance=1` first: `frame=9127 isMyClient=bool:true playerNumber=real:1.000000 targetNumber=real:1.000000 purgatory=real:0.090000 purgatoryTimer=real:14.400000 destroyTimer=real:144.000000`; last: `frame=10919 isMyClient=bool:true ... purgatory=real:0.090000 purgatoryTimer=real:84.588192 destroyTimer=real:-1.000000` — `isMyClient=bool:true` and `purgatory` numeric and greater than 0, both readings. |
| S3 | measured | Still ON. `tgprobe spurn: frame=12270 ... n=1 mine=1 others=0 unattributed=0 capped=0 state=on samples=10736 on=3143 off=7593 ... transitions=1 lastTransitionFrame=9127 markedOn=3143 markedOff=7593`; `tgprobe spurn as foreign -> off n=1 mine=0 others=1 unattributed=0`; the next plain `tgprobe spurn` read immediately after is identical to the one just before (`frame=12270 ... samples=10736 on=3143 off=7593 ... transitions=1 lastTransitionFrame=9127 markedOn=3143 markedOff=7593` — every field unchanged), confirming the override never touched the real counters. |
| S4 | measured | `tgprobe show` before the OFF press: `TalentUse mode=native calls=2 lastFrame=13791 lastGap=4683`; `TalentsWhiteMage mode=native calls=2 lastFrame=13810 lastGap=4683`. `tgprobe spurn: frame=15840 ... n=0 mine=0 others=0 ... state=off samples=14306 on=4683 off=9623 unreadable=0 maxN=1 transitions=2 lastTransitionFrame=13810 markedOn=4683 markedOff=9623`. `offlag:` = `lastTransitionFrame` (`13810`) minus the `TalentUse` press `lastFrame` (`13791`) = **19 frames**; the `TalentsWhiteMage` frame (`13810`) sits beside it, equal to `lastTransitionFrame` itself (0 frames after the cast resolves). |
| S5 | measured | Tester respecced Purgatory out, cast Soul Spurn once, waited ≥3 s. `tgprobe spurn: frame=21450 ... n=0 mine=0 others=0 ... state=off samples=19916 on=4829 off=15087 unreadable=0 maxN=1 transitions=4 lastTransitionFrame=19802 markedOn=4683 markedOff=15233`. `tgprobe spurn fields: appearance=2` first: `frame=19656 isMyClient=bool:true playerNumber=real:1.000000 targetNumber=real:1.000000 purgatory=real:0.000000 purgatoryTimer=real:14.400000 destroyTimer=real:144.000000`; last: `frame=19801 isMyClient=bool:true ... purgatory=real:0.000000 purgatoryTimer=real:14.400000 destroyTimer=real:-0.737424`. The new appearance's `purgatory` reads `0` (numeric, ≤ 0) in both "first" and "last"; `on=` rose `4683`→`4829` (+146) across this row while `markedOn=` stayed at `4683` — exactly the case the row's gate rule calls `flash: purgatory`. |

## Decision

**Track A (re-cast guard): BLOCKED on Q2/Q3, redesign required.** Q1 and Q5
are measured, and they change the design:

- **`TalentUse` is the wrong hook.** The double-cast proc re-casts through
  `TalentUseClass` directly, so a `TalentUse` guard would miss the case the
  issue most likely describes.
- **The press guard must allow for auto-repeat.** A held key re-fires every
  57 frames, and that is also an "accidental double cast".
- **Refusing the proc's re-cast is enough for Soul Spurn.** For talent 240,
  when `self` is a `Universal_Double_Cast_obj`, no time window is needed. This
  is keyed on the talent id, not on a measured "is a toggle" property, and
  whether the Purgatory sub-talent should gate it is not established.

**Track B (active indicator): BLOCKED on Q3 (state not found) and Q6.**
Q4 is measured: `DrawHudBuffs` is a valid anchor.

**Next research step:**
1. Read the state from non-scalar storage. Either extend `tgprobe` with a
   struct/array/ds_map diff of `Player_obj` and the talent data, or use the
   Ghidra fallback (step 11) to read `TalentsWhiteMage`'s talent-240 branch
   locally and record, paraphrased, what it sets and reads.
2. Read the Purgatory sub-talent level
   (`ReturnSubTalentLevel(1, 240, <index>)`).
3. Expand the `row0`/`row1` button arrays for Q4's talent key.

### After session 2

**Track B (active indicator) is UNBLOCKED. Q3 read, as input to the
indicator workorder:**

- **Root/object:** `White_Mage_Soul_Spurn_AOE_obj` (`GameObject::White_Mage_Soul_Spurn_AOE_obj`
  = SDK index 5759, the runtime index was never printed) — a live-instance
  count, not a member of `Player_obj`, `global` or any struct.
- **Read:** `instance_number(asset_get_index(GetObjectName(GameObject::White_Mage_Soul_Spurn_AOE_obj))) > 0`.
  The ON/OFF evidence above is six `tgprobe deep census` reads (a loop index
  over the object range plus `object_get_name`), not a direct call of this
  expression on `White_Mage_Soul_Spurn_AOE_obj`. The **shape** — `CallBuiltin`
  of `asset_get_index` then `instance_number`, by name — **did run this
  session**, via `TgProbeCountByName` (`ModuleMain.cpp:14916-14925`, called
  at `:14941-14942` from `TgProbeHudRoomTick`), but **with no `self` supplied
  at all**: `CallBuiltin` is the two-argument form YYTK documents as running
  in the global context and never takes or forwards a `CInstance*`
  (`YYTK_Shared_Interface.hpp:66-74`; the `self`-taking form is the separate
  `CallBuiltinEx` at `:85`). It ran on `White_Mage_Soul_Spurn_obj` and
  `Player_Ability_Parent_obj`, not the AOE object, and it ran **exactly
  twice**, not every frame — `TgProbeHudRoomTick` only calls it from its
  key-change branch (`:14931-14943`; the per-frame part is just a counter
  increment at `:14945`), and the room key never changed this session. Both
  calls landed at room attach, frame 5400 (`firstHudSpurnInstances=0
  firstHudAbilityInstances=0`, printed unchanged in three later `tgprobe
  show` replies — session2.log lines 127, 224, 1255, alongside a rising
  `hudSinceRoomChange` that is a separate per-frame counter, not a
  re-sample). No cast had happened yet: the same reply samples `TalentUse
  mode=native calls=0 lastFrame=0` at `frame=5430` (log line 62), and the
  first cast counted after `tgprobe reset` (log line 150) is later
  (`TalentUse ... lastFrame=14796` / `TalentsWhiteMage ... lastFrame=14815`,
  log lines 159, 166). The tester's by-eye OFF note (log line 136) comes
  before both the reset and that first counted cast, so it corroborates OFF
  after frame 5400 and before the first counted cast, not exactly at frame
  5400. `0` is consistent with that — `White_Mage_Soul_Spurn_obj` never appears as a non-zero census
  row anywhere this session, and `Player_Ability_Parent_obj`'s absence rests
  on the fully enumerated census at frame 27210 (session2.log 757-935,
  `Player_Ability_Parent_obj` not among its 177 rows) and on its absence
  from both `deep flip base on off` buckets (log 490-737, A=41/B=221) — it
  is absent from all three ON census reads and all three OFF reads, first
  appearing only much later (`+ census.Player_Ability_Parent_obj=1` during
  the `hz2`/`hz3` Healing Zone cast) — but neither object's own count was
  independently checked at frame 5400, so `0` is consistent with, not proof
  of, the state at that moment. See the Q3-D row for the full comparison.
- **ON value:** `1` (instance exists). **OFF value:** `0` / absent (instance
  does not exist).
- **`self` dependency: untested —** the shape is
  confirmed readable **in the global context** (above — both calls returned a
  number, not the bare `unreadable` token or `n/a`; `ModuleMain.cpp:15176`,
  `:15181-15182`). **No `self` value has been supplied to this read at all** —
  not `Controller_obj`, not any other — so nothing is established yet about
  whether or how `self` matters to it. What is also genuinely missing is a
  **non-zero** result through this shape: both calls this session read `0`,
  on other objects, before any cast, never `1` on the AOE object. **The
  indicator workorder must run its own ON=1 positive control with this exact
  read on `White_Mage_Soul_Spurn_AOE_obj`, in whatever context (global via
  `CallBuiltin`, or a specific `self` via `CallBuiltinEx`) it actually plans
  to use — neither "does it return" nor "it ran once" is evidence the read
  distinguishes ON from OFF in that context.**
- **Not the toggle, seen along the way:** `Player_obj.playerEffect[182]`
  (`real:0.000000 -> int64:2` on first cast, stays `2` after OFF — what this
  value means is not established) and `Player_Damage_Parent_obj`'s census
  count (the object's own parent class, same bucket-A shape, not a distinct
  signal).
- **Observed alongside, not required for the read:** the AOE instance's own
  `purgatory`/`purgatoryTimer`/`activated`/`tickNumber` variables while ON
  (`tgprobe vars White_Mage_Soul_Spurn_AOE_obj`), which the indicator
  workorder may use for richer state but does not need for a plain on/off
  read.
- **Not established — the read above is untested on all of the following,
  and the indicator workorder must not assume it holds:**
  - **Per-player / co-op scoping.** The AOE instance carries `playerNumber=1`,
    and `instance_number` counts every player's instances of the object, not
    just the local player's. Only single-player town was measured this
    session; a co-op session with another player's Soul Spurn active is
    untested and could read `> 0` while the local player's is OFF.
  - **A Soul Spurn cast without Purgatory.** Never tested — Purgatory was on
    the hotbar throughout. The AOE instance's `destroyTimer=-1` while ON (a
    value that, on other objects in this snapshot, marks "not scheduled to
    expire") hints that the toggled instance may differ from a plain,
    non-Purgatory cast of the same skill, which this session cannot rule out.
  - **Zone change.** The attach-time `firstHud=` read (session 1's
    `tgprobe show`) counts `White_Mage_Soul_Spurn_obj` instances, a different
    object from the `White_Mage_Soul_Spurn_AOE_obj` measured here — zone
    change behavior for the actual Q3-D path is untested.
  - **OFF lag.** The first OFF census read (frame 39540) is at least 3390
    frames after the last confirmed-ON read (frame 36150); how long the AOE
    instance persists after the OFF press, and whether the read momentarily
    reads stale-ON during that window, is unmeasured.
  - **HP self-cancel.** Session 1 observed Purgatory's health drain
    self-cancelling the toggle at low HP; that path was not exercised in
    session 2, so whether the AOE instance is torn down the same way is
    untested.
  - **More than one instance.** Every ON read this session showed exactly
    `=1`; the read's behavior with 0, 2 or more simultaneous instances
    (e.g. from a proc re-cast, Track A Q5) is unobserved.
  - **The Purgatory sub-talent level.** Two candidate storage locations
    exist (`global.subTalentMap[1].t240.{s2,s6,s7,s9,s10,s12}` and
    `UI_Hud_Talent_obj.playerSlot{subTalentMap}[0].t240`, a six-entry map with
    the same values), but *which* `sN` holds Purgatory's level was not
    identified this session (see the Purgatory sub-talent note in
    Results → Session 2). If the indicator or a future design needs the
    sub-talent's configured level (as opposed to whether Soul Spurn is
    currently toggled on), that is a separate, still-open read.

Track A (re-cast guard) is unchanged from after session 1: still **BLOCKED on
Q2** (what in the call trace distinguishes a toggle cast from a non-toggle
cast). Session 2 did not attempt Q2; the next research step for Track A, if
picked up, is unchanged from the session-1 Decision.

The non-blocking instrument findings deferred at the round-3 cap (C1's
`selftest instance` check-expectation defect recorded above; N1–N4 from the
round-3 verify log) are a follow-up to the instrument itself, not to this
result — Q3-D does not depend on them.

### After session 3

**Read: GO.** R1, R2 and R3 (Results → Session 3) are all `measured`, and the
instrument control matched (`hudCalls` `2728`→`3148` (+420) = `samples`
`2728`→`3148` (+420)). R7 is also `measured`, with `drawsToOff=0`, well under
the 600-draw cap, so `read: BLOCKED (zone)` does not apply. R2's own reading
diverged from the row's original expectation (`n=1`, `mine=0`,
`state=unreadable`, never `on`, instead of "On, n≥1, mine≥1") because of the
Scope finding below — the gate rule in `## Live procedure` → `### Session 3`
only requires R1/R2/R3 `measured` with the control matched, so that divergence
does not itself change `read:`.

**Scope: BLOCKED.** `Player_obj` has no `playerNumber` member: `tgprobe deep
get Player_obj.playerNumber: FAILED segment .playerNumber: no such instance
variable`, and the full player-scope walk (`tgprobe deep snap p1 scope=player
names=116 read=116 unreadable=0 ... note=instances=1`) read all 116 named
leaves without one of them being `playerNumber`. The only hit for the
ownership key, from a substring search across the whole player scope
(`tgprobe deep find 'playernumber' in p1: hits=1`), is `Player_obj.myHealthBar.playerNumber=real:1.000000`
— a field on a *followed instance handle* (`myHealthBar`) one level removed
from `Player_obj`, not a direct member of the local player instance the
production read (Context "Co-op / ownership: the answer") assumed it could
read `playerNumber` off of directly. The AOE instance's own `playerNumber`
(`real:1.000000`) and `isMyClient` (`bool:true`) are both readable and would
match `myHealthBar.playerNumber`'s value, but that is a different ownership
key than the one the P1 design built against. Per `## Steps` step 7, R4
naming a different ownership key is a `PLAN-DEFECT`.

**Slot geometry fields: `row0[5].talentId` + `navBboxX`/`navBboxY`/`navBboxWidth`/`navBboxHeight`.**
R5.1's draw control passed (`markDraws=270 markDrawExc=0`, rectangle SEEN by
eye at an arbitrary GUI spot before any candidate was tried), and R5.2's
candidate A — `row0[5]` (`talentId=240`, `drawButton=bool:true
hidden=bool:false`), marked at its own `navBboxX=385.700006
navBboxY=1711.000000 navBboxWidth=124.700000 navBboxHeight=139.200000` —
SAT ON Soul Spurn's button, by eye. `row1[5]` (`hidden=bool:true
drawButton=bool:false`) was not marked; its own `hidden`/`drawButton` values
already explain why it is not the drawn slot.

### After session 4

**Scope: isMyClient.** S0's control matched (`hudCalls`/`samples` both
`3386`→`3836`, +450), and S1, S2 and S3 (Results → Session 4) are all
`measured` exactly as the row describes: S1 read `off n=0` by eye OFF; S2
read `on n=1 mine=1` from the draw hook itself, with `isMyClient=bool:true`
confirmed by `spurn fields` (the first time this session's read answered
`on` from `Hook_DrawHudBuffs`, not from a command-line override); S3's
`spurn as foreign` read `off n=1 mine=0 others=1` and left the real counters
byte-identical before and after. Per Context "Session 4", `scope: isMyClient`
follows.

**Flash: purgatory.** S5 respecced Purgatory out and cast Soul Spurn once.
The new appearance's `spurn fields` snapshot read `purgatory=real:0.000000`
in both "first" (`frame=19656`) and "last" (`frame=19801`) — numeric and ≤ 0
throughout, never the `>0` S2 measured for a Purgatory-toggled cast. `on=`
rose `4683`→`4829` (+146) across the row while `markedOn=` stayed at `4683`:
the plain ownership read (no marker) counts the flash as ON for ~146 draws,
exactly the plain-cast flash D-R2 anticipated, and the marker-required
decision (`markedOn`) does not. `flash: purgatory` follows: P2 requires the
Purgatory marker.

**Off lag: 19.** S4's `tgprobe show` before the press read `TalentUse
... lastFrame=13791`; the following `tgprobe spurn` read
`lastTransitionFrame=13810`, matching `TalentsWhiteMage ... lastFrame=13810`
exactly (0 frames between the cast resolving and the ownership-only read
flipping to `off`). `offlag:` = `13810 − 13791` = **19 frames** from the
button press to the read leaving `on`.

### P1: the indicator's read, control and slot design

What the P1 build (`ToggleIndicatorRead`/`ToggleIndicatorModel`, plus the
`tgprobe spurn`/`tgprobe mark` control) actually does with the evidence above,
and what it still leaves open for session 3.

#### The read, and exactly what has been proven

`### After session 2` above measured the read **shape** —
`CallBuiltin("asset_get_index", …)` then `CallBuiltin("instance_number", …)`,
two-argument, global-context, no `self` — running twice this session, on the
wrong objects, before any cast, always returning `0`. It never ran on
`White_Mage_Soul_Spurn_AOE_obj` itself and never returned a non-zero. So P1's
first job is to run that exact shape, through the production function, from
where the indicator will actually call it: inside `Hook_DrawHudBuffs`,
immediately after the trampoline and `HhDrawHeadLabels()` (the frame's GML
`self` there is `Controller_obj`, though the read itself never uses `self`).
`HhResolveLocalPlayer` is already called every frame from that same hook by
`HhDrawHeadLabels`, so calling it again here leans on a proven call, not a
cold one; `HhResolveLocalPlayer` can hand back `VALUE_REF` rather than
`VALUE_OBJECT` (Known Limitations item 7), so the read must not gate on the
player `RValue`'s own kind. `draw_rectangle` is not used anywhere in
`ModuleMain.cpp` before this change, so whether this hook can draw at all is
unproven until `tgprobe mark` shows it (Section 3's fix below).

#### Co-op / ownership: the answer

*(P1, superseded by session 3's R4 — kept for history; the current design is
below, "Co-op / ownership after session 3: isMyClient".)* `instance_number`
counts every player's instances of the AOE object, not just the local one's;
the measured AOE instance carries `playerNumber=1`, `isMyClient=true`,
`targetNumber=1` and `myCaster=-4`. The design P1 shipped as its research
control: an AOE instance lights the indicator only if its own `playerNumber`
(read with `variable_instance_get`) matches the local player's own
`playerNumber`, read via `HhResolveLocalPlayer`. Whether `Player_obj` actually
carries a `playerNumber` member at all was not printed by either session 1 or
2; session 3's R4 measured it directly. It does not: `Player_obj` has no
`playerNumber`, only `Player_obj.myHealthBar.playerNumber` (a followed
instance handle) and a struct field `pNm`, neither of which this design read.
So the local-number half of the comparison always failed, and the read could
never answer `on` from anywhere it was actually exercised as a comparison.
`isMyClient` was logged for the record only and used nowhere in this design's
decision.

#### Co-op / ownership after session 3: isMyClient

**Decision:** an AOE instance lights the indicator if its own `isMyClient`
(read with `variable_instance_get`, no local-player read at all) is true. A
`VALUE_BOOL` gives its truth directly; a numeric kind counts nonzero as true;
anything else — undefined, a string, or a throw — is *unattributed* and never
lights it. An own instance whose own `purgatory` reads numeric greater than
zero is additionally *marked*; see "Plain-cast flash (R10) and the Purgatory
marker" for when the marker is required. If every instance present is
unattributed, the answer is `Unreadable` — nothing is drawn, and a counter is
raised — never guessed either way. A foreign AOE therefore fails toward
"absent", never toward "wrong".

Why `isMyClient` over `playerNumber` (or `targetNumber`, `myCaster`, a game
script call): it is one read on the instance being judged, at the point of
use, with no second object whose shape has to be assumed — the exact failure
mode that sank the P1 design above. It also drops `HhResolveLocalPlayer`, and
with it a per-draw player lookup, from the read entirely. The game ships the
same concept by name (`gml_Script_IsMyClient`, SDK `scripts.hpp` index 2070,
beside `IsMyPlayer`/`GetMyPlayer`, which ForgePact already calls or hooks) —
that is consistency, not proof of co-op meaning by itself
(`AGENTS.md` "Identify a thing by what it is"). What is proven is narrower: it
reads `true` on the local player's own AOE, twice, in session 3's R2/R3.
Session 4 proves it from the draw hook itself (S2/S3).

What stays inferred, and cannot be produced offline: that a co-op partner's
AOE reads `false` on this client. A second real player cannot be produced in
this toolkit's offline setting (see "Anti-Cheat & Offline Enforcement"), so
the live negative control is non-mutating instead: `tgprobe spurn as foreign`
runs the identical enumeration and decision with every own instance
re-interpreted as foreign, and must answer `off` with `others>=1` while the
real (un-overridden) answer is `on`. That proves the `others` branch runs on
live values; it does not prove what `isMyClient` means for a real partner,
which stays a Known Limitation. No natural `isMyClient=false` instance was
looked for, and none is planned.

#### Research-build control (P1/P1b): `tgprobe spurn` and `tgprobe mark`

Both commands live inside the existing `tgprobe` research block, dispatched
from `TgProbeCommand`. One research-only line follows `HhDrawHeadLabels();` in
`Hook_DrawHudBuffs`, in its own `#ifndef FORGEPACT_RELEASE` pair: it samples
the production read on every draw (`tgprobe spurn`'s running counters and last
sample) and draws the `mark` rectangle if one is armed. `spurn as foreign` is
the non-mutating negative control described above; `spurn log on|off` logs
each instance's `playerNumber`/`isMyClient` on a budgeted number of state
changes; `spurn slots` runs read-only from the command handler; `spurn fields`
prints the latched per-appearance snapshot (see "Session 4" below). `mark <x>
<y> <w> <h>` saves `draw_get_colour`/`draw_get_alpha` before its first
`draw_set_` and restores both after its last draw — the same pattern
`HhDrawHeadLabels` uses — and now counts `draws=`/`drawExc=` so a tester (and
`tgprobe spurn`'s own `markDraws=`/`markDrawExc=` line) can tell "never drew"
apart from "drew somewhere the tester didn't see" (the fix for the
instrument-blindness review below). The sampler's own instrument control: the
`samples=` delta across a window must match the `hhlabel` `hudCalls=` delta
over the same window (±2), or every `spurn` answer in that window is
`blocked`, not a game finding.

#### Slot location (Q4): what is known

`UI_Hud_Talent_obj` (`GameObject` 5099) has `x=0 y=0 width=2035.8 height=232
buttonXOffset=43 buttonYOffset=48 row0X=104 row1X=5 rows=2 buttonScale=1` on a
3840×2088 GUI (session 1). Session 2's non-scalar search found talent 240 in
four places: `UI_Hud_Talent_obj.row0[5].talentId`, `.row1[5].talentId`,
`.playerSlot{bind_skill}[0][4]`, and `global.mySkills[3]`. `row0`/`row1` hold
structs whose elements also carry `refreshInfoTimer`. Not established by any
static source: which array is the drawn hotbar, why both rows hold 240 at
index 5, or which members hold the button's actual screen position — this is
a by-eye measurement only. `tgprobe spurn slots` prints every member of each
`talentId == 240` element; `tgprobe mark x y w h`, once its own draw control
has passed, draws a candidate outline from those members at GUI coordinates,
and the tester says which one (if any) sits on Soul Spurn's slot. The winning
array, id field and position fields are recorded as `Slot geometry fields:` in
`### After session 3`; if the draw control itself never produced a visible
rectangle, that is recorded separately (`blocked (draw control)`) rather than
folded into "no candidate matched" — see `## Live procedure` → `### Session 3`
step 6.

#### Section 3 fix: a draw control before any candidate (instrument-blindness review)

`TgProbeDrawMark` previously swallowed every exception from its own
`draw_rectangle`/`draw_set_*` calls uncounted, so a broken draw (wrong GUI
layer, a bad colour/alpha call, coordinates in the wrong space) and a working
draw at the wrong coordinates would have looked identical to the tester —
both print nothing extra, and the session-3 procedure would have recorded
`slotgeom: none` either way, closing the feature on an instrument artifact
rather than a real negative. `TgProbeDrawMark` now counts `draws=` (a
completed pass, including both restores) and `drawExc=` (a threw pass)
separately, both printed by `tgprobe mark`/`tgprobe mark off` and by `tgprobe
spurn`'s `markDraws=`/`markDrawExc=`. Session 3 step 6 now runs a draw control
first — `tgprobe mark` at an obviously visible GUI spot, confirmed by eye —
before touching any candidate slot rectangle, and records `blocked (draw
control)` rather than `slotgeom: none` if that control fails.
