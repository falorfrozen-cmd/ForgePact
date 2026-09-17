# Toggle skills — research log (issue #11)

Status (2026-09-17): **one live session run; both tracks BLOCKED pending a
second research round. Session 2 is planned, not yet run:** it reads Q3 (where
the Soul Spurn / Purgatory ON state lives) from non-scalar storage with
`tgprobe deep` — `Player_obj`, the talent structs in
`global.talentStructMap`, `Controller_obj`, the HUD slot object,
`Skill_Controller_obj`, every global, and a live-instance census — and falls
back to a local Ghidra read of `TalentsWhiteMage` only if that comes back
empty with its controls fired.

- **Measured:** the cast path (Q1), draw order (Q4), and three sources of
  accidental re-casts (Q5), one of them the double-cast proc, which bypasses
  `TalentUse`.
- **Not observed:** where the toggle's on/off state lives (Q3). It is in no
  scalar of `Player_obj` or global, in no HUD variable, in no ability object
  and in no player buff.
- **Blocked:** every object-event row, which does not resolve by name.

See Results and Decision. Statements in the sections before Results are
static-search facts or labelled inference, written before the session.

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
| HUD closures (every closure split out of the slot object's Create event) | `gml_Script_anon_1183_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_2413_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_10430_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_11283_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_11677_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_12084_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_12530_gml_Object_UI_Hud_Talent_obj_Create_0`, `gml_Script_anon_13033_gml_Object_UI_Hud_Talent_obj_Create_0` | args |
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

Not run yet. Session 2, <date>: research build `<commit>`
(`BloodPactPlugin_rel.dll`), White Mage, Soul Spurn + Purgatory and Healing
Zone on the hotbar, town (`<room>`), no `coop.ini`, no `citrace` before step 10.
The tester reports the on/off state by eye at every snapshot. Status is exactly
one of `measured`, `not observed`, `blocked`; a `not observed` quotes C1–C5 as
fired and every compared snapshot as `truncated=0` and `followTruncated=0`,
otherwise it is `blocked`.

**Attach, controls and snapshots.** (`tgprobe hook` summary line; both C1
`selftest` lines; every `deep snap` scope line and summary line, with
`instRefs=`, `objNonStruct=`, `followLeaves=`, `truncated=` and
`followTruncated=`; the C2 `global.playerBuff[1][0][86]` line from
the filtered diff, with its `.<member>` lines if it is an instance handle, or
`C2 not fired: …`; the C3 `census.` line or `C3 not fired: …`; the C4
direct `Player_obj.<name>` line or `C4 not fired: …`; the C5 `talent` `read=` values and
`find <absent:` hit counts.)

| Q | Question | status | Evidence |
|---|---|---|---|
| Q3-D | Where the ON state lives, read from non-scalar runtime storage (`tgprobe deep`) |  |  |
| Q3-G | The same, from the local Ghidra read of `TalentsWhiteMage`'s talent-240 branch, paraphrased |  |  |

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

Not written yet: filled from Results → Session 2.
