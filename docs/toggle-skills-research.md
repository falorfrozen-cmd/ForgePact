# Toggle skills — research log (issue #11)

Status (2026-09-17): **one live session run; both tracks BLOCKED pending a
second research round.**

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

## Live procedure

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

## Results

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
