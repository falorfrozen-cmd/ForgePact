# Skill actions: the bar, a cast, a binding and a talent allocation

**Question.** For the character's skill bar and talent tree, as the running
game reports them: which routines and which state does the bar, a cast from
the bar, a binding of a skill to a bar slot, a talent allocation (and a
sub-talent node), and a reset of the tree go through - and which of those
routines can be called by name, with which self, other and arguments, so that
the game does its own bookkeeping?

**Why it matters.** The hub's `hs-drive` MCP server (`docs/tools/hs-drive-mcp.md`
in the toolkit) is gaining five tools - `hs_skills_status`, `hs_skill_cast`,
`hs_skill_bind`, `hs_talent_allocate` and `hs_talent_reset` - so a live test
session can read and change the character's skills with no one at the
keyboard (toolkit issue #147). Binding, allocating and resetting are *setup*
for other tests, so they go through the game's own routines by name, through
player-build verbs (`skillbind`, `talentalloc`, `talentreset`), and are
confirmed by re-reading the game's state through a read verb (`skillstate`).
The cast is the thing a test exercises, so it presses the slot's own key
through the game's input path and proves the cast from game state. None of
these mechanisms is measured for this purpose yet. This document records the
static search, what a local static reading suggests, the instrument, the one
research launch that measures them, and the decision the hub tools and the
player verbs are written against.

**Why not write the state directly.** A raw write to the binding, the talent
levels, the sub-talent levels or the points would be the shortest route, and
it is ruled out: the static reading below finds the allocation handlers
hashing the talent state and reporting the client on a mismatch, and the
bar's arrays are derived from a store elsewhere, so a raw write either flags
the client or drifts from the store. The game's own handlers, called by name
with the real button instance as self, keep the game in charge of its own
hashing - the same conclusion the crafting-materials research reached for
items. A click on a listed `menulayout` row stays only as the fallback a
§ Decision line may record.

**Posture.** Everything here is measured runtime behaviour, a reading written
in our own words, or our own code. Game objects and scripts are named by their
`hs-game-sdk` names and indices; no game script text appears, and the
procedure is written as prose. A reading is labelled as a reading and is not
a fact until phase 0 records it.

**Status.** The instrument (`skillprobe` in the research build, and
`menulayout`'s talent rows and HUD slot rows) is built on this branch. Phase
0, the research launch below, has **not run yet**: § Results is empty and
every § Decision line reads `pending`. What § Instrument lists as hypotheses,
and what § Static readings suggests, are not facts until phase 0 records
them.

## Static search

Source: `hs-game-sdk`'s object and script tables and its parent hierarchy
(the Python binding's parent lookup and the C++ `objects.hpp` and
`scripts.hpp`), searched on 2026-09-25 over every name matching a talent,
skill, spell, hotbar, bind or controls concept; plus
`toggle-skills-research.md` (§ Static search, § Toggle skill table) in this
repository and the toolkit's `docs/RUNTIME_DATA_MODELS.md` § 6.3, § 7 and
§ 8. Throughout the game and the SDK a skill is a **talent**: the bar is
`UI_Hud_Talent_obj`, the cast routines are `TalentUse*`, and the upgrades in
the tree are sub-talents.

The command's candidate table is nine objects, each spelled through the SDK's
`GameObject` enum (index and parent from `hs_game_sdk.objects`):

| Object | Index | Parent |
| --- | --- | --- |
| `UI_Hud_Talent_obj` (the bar) | 5099 | `UI_Parent_obj` |
| `UI_Talent_Screen_obj` (the tree window) | 5278 | `UI_Parent_obj` |
| `UI_Talent_Button_obj` | 5273 | `UI_Button_obj` |
| `UI_Button_Talent_Player_obj` | 5016 | `UI_Button_obj` |
| `UI_Button_Subtalent_obj` | 5013 | `UI_Button_obj` |
| `UI_Talent_Screen_Allocate_obj` | 5275 | `UI_Button_obj` |
| `UI_Sub_Talents_obj` | 5272 | `UI_Parent_obj` |
| `UI_Talent_Node_Tree_Parent_obj` | 5274 | `UI_Node_Parent_obj` |
| `UI_Button_Sub_Skill_obj` | 5012 | `UI_Button_obj` |

Their Create-event closures are twenty-seven `HeroSiege::Scripts` constants,
each named `gml_Script_anon_<n>_gml_Object_<Obj>_Create_0` (runtime name
`gml_Script_anon@<n>@gml_Object_<Obj>_Create_0`); the contract test derives
this set from `scripts.hpp`, never from a hand list:

| Object | Closures (offset: SDK index) |
| --- | --- |
| `UI_Hud_Talent_obj` | `anon@1233`: 5603, `anon@2503`: 5604, `anon@10745`: 5605, `anon@11619`: 5606, `anon@12025`: 5607, `anon@12449`: 5608, `anon@12916`: 5609, `anon@13435`: 5610 |
| `UI_Talent_Screen_obj` | `anon@4498`: 6177, `anon@9936`: 6178 |
| `UI_Talent_Button_obj` | `anon@1009`: 6166, `anon@1751`: 6167, `anon@2387`: 6168, `anon@2973`: 6169, `anon@3562`: 6170, `anon@4068`: 6171 |
| `UI_Button_Talent_Player_obj` | `anon@650`: 5472, `anon@23088`: 5473, `anon@23904`: 5474 |
| `UI_Button_Subtalent_obj` | `anon@535`: 5470, `anon@2428`: 5471 |
| `UI_Talent_Screen_Allocate_obj` | `anon@643`: 6174 |
| `UI_Sub_Talents_obj` | `anon@6264`: 6163, `anon@8060`: 6164, `anon@8767`: 6165 |
| `UI_Talent_Node_Tree_Parent_obj` | `anon@1596`: 6172, `anon@2306`: 6173 |
| `UI_Button_Sub_Skill_obj` | none in the SDK |

Scripts, thirty-three, each a `HeroSiege::Scripts` constant. Where the SDK
carries both a bare and a `gml_Script_` form the row uses `gml_Script_<Name>`
(the form `tgprobe` and `craftprobe` already use) and the index below is that
constant's; the six constructor-style names exist only in the bare form and
are spelled as the SDK spells them.

| Group | Scripts (SDK index) |
| --- | --- |
| Cast | `CheckTalentUse` (539), `TalentUse` (3829), `TalentUseClass` (3833), `TalentRequirement` (3769), `TalentRequirementFunc` (3768, bare), `GetTalentCooldown` (1782), `PlayerManaUpdate` (2768) |
| Talent reads | `GetTalentInfo` (1788), `ReturnTalentLevel` (2297), `GetTalentLevelReq` (1790), `ReturnSubTalentLevel` (3453), `GetSubTalentInfo` (1778) |
| Bar and tree UI | `UiActivateTalents` (4378, bare), `UiActivateHudTalentButtonFuncs` (4103, bare), `UiHudTalentNavigationFunc` (4491, bare), `UiTalentNavigationFunc` (4543, bare), `UiHideTalentsWithNoBind` (4108), `UiSetActivationFunc` (4463), `UiCreate` (4467), `UiSetFocus` (4476) |
| Network and consistency | `NetworkSendTalentUpdate` (2594), `NetworkSendClientAllTalents` (2457), `NetworkSendClientTalentUse` (2523), `CA_playerTalentUpdate` (370), `CA_playerExtraTalentUpdate` (333), `CA_playerTalentActive` (368), `ClearPersistSkill` (2390), `ReportClient` (566) |
| Controls and owners | `LoadControls` (2165), `SaveControlsFunc` (3490, bare), `GetPlayerTalentHudObj` (1725), `GetPlayerProfileObj` (1723) |
| Session control | `CheckPlayerInteraction` (544) |

`KeyboardMouseInput` is deliberately **not** a row: it is a per-frame input
dispatcher, hooking it costs every frame, and it answers nothing the hook-free
`keys` reader does not.

Several of these rows are also rows of other research instruments in this
plugin, which matters for the `held` rule in § Instrument: `tgprobe`'s table
names `CheckTalentUse`, `TalentUse`, `TalentUseClass`,
`NetworkSendClientTalentUse`, `CA_playerTalentActive`,
`CA_playerTalentUpdate`, `GetTalentInfo`, `ReturnTalentLevel`,
`GetTalentCooldown`, `GetSubTalentInfo`, `ReturnSubTalentLevel`,
`ClearPersistSkill`, `GetPlayerTalentHudObj`, the eight `UI_Hud_Talent_obj`
closures and `CheckPlayerInteraction`; `craftprobe`'s names `ReportClient`,
`UiCreate` and `CheckPlayerInteraction`; `restartprobe`'s names
`UiSetActivationFunc`, `UiCreate` and `UiSetFocus`. Outside the probes,
`toggleguard` installs on `TalentUseClass` when it is armed, and the co-op
render path installs on `TalentUse` when it is on.

Negative results of the search, recorded so nobody repeats it:

- Object-event rows do not resolve by name on this build (toggle research),
  so the rows are scripts and Create closures only, never an object's event.
- The on/off state of a toggle skill lives in no scalar of `Player_obj`, the
  globals or the HUD object (toggle research); nothing here depends on it.
- No per-skill script exists for any talent (no `*Purgat*`, no `*Spurn*`): a
  talent's body lives in one of the 26 `Talents<Class>` scripts.
- `UI_Button_Sub_Skill_obj` has no Create closure in the SDK; its row is an
  object row only.
- `UI_Talent_Screen_Attributes_Container_obj` has two Create closures (SDK
  6175 and 6176) beside the talent screen's; it is not one of the nine objects
  and its closures are not rows.
- Mercenary talents (`UI_Mercenary_Talents_obj`,
  `UI_Button_Mercenary_Talent_obj`, `TalentsMercenary`), the mobile and
  gamepad talent UI (`Mobile_Talent_Direction_obj`,
  `UI_Mobile_Talent_Round_Button_obj`) and the relic, augment and universal
  talent groups are out of scope: the class tree and the bar only.
- `DebugItemSpawning` resolves to a 512-byte stub in the static project: *not
  observed*, not absent.

What prior measurement established (the toggle-skills research in this
repository, folded into the toolkit's `docs/RUNTIME_DATA_MODELS.md` § 7), in
our own words:

- **The cast pipeline** (measured). One key press makes one `TalentUse` call
  with self `Player_obj` and the arguments (player ref, talent id, 1, false,
  true); a held key repeats it every 57 frames. About 19 frames later
  `TalentUseClass` runs with self `Player_obj`, a0 the talent id and a4 true,
  and inside it the class body (`TalentsWhiteMage` and the like) and
  `GetTalentCooldown(id, 1)`. A double-cast proc is a `TalentUseClass` from
  `Universal_Double_Cast_obj`. `CheckTalentUse` runs once per frame. The
  cooldown and mana handling is not inside the `TalentUse` call (see
  § Static readings).
- **The bar** (measured, toggle session 2). One `UI_Hud_Talent_obj` at x 0,
  y 0, with `rows=2`, `row0X=104`, `row1X=5`, `buttonXOffset=43`,
  `buttonYOffset=48` and `buttonScale=1` on a 3840x2088 GUI. The bound talent
  240 was found in four places: `UI_Hud_Talent_obj.row0[5].talentId`,
  `.row1[5].talentId`, `.playerSlot{bind_skill}[0][4]` and
  `global.mySkills[3]`. The `row0`/`row1` elements are structs carrying
  `talentId`, `refreshInfoTimer`, `navBboxX` and `navBboxY` - the last two
  are where the slot's button draws, the position the toggle marker uses.
  Which array is the drawn bar, why both rows held 240 at index 5, and which
  of the four places is the *store* the others derive from is **not
  established** (`bindWriteRule` measures it).
- **Static talent data** (measured). `global.talentStructMap` is a ds_map
  from numeric talent id to a struct with `abilityId` (a string),
  `abilityAura`, `abilityDuration`, `abilityCooldown` (0.25 means none),
  `abilityLength` and `abilityTags`; 817 ids; no level and no toggle flag.
  The `abilityId` to id mapping is resolved at run time by walking that map,
  which the shipped toggle mods already do (`talentIdRule`).
- **Allocation state** (measured). `global.subTalentMap` is an array of 6;
  index 1 held the local player's sub-talent levels, shaped `t<talentId>` to
  `s<NN>` to a level (0, with the key present, when unallocated; a base-form
  talent has no `t<id>` node). `toggleguard` reads it at the call. A talent's
  own level has a reader, `ReturnTalentLevel` (a script), and the level
  readers call `ReturnSpecificStat`. Where the *points available* live is
  **not measured** (`pointsReader`).
- **Keys** (measured by the hub's `hs_input` tool). Its `send_input` reaches
  both `keyboard_check` and `keyboard_check_direct`; a click must be held
  (`hold_ms` 120); `E` interacts (the owner). The key per bar slot is
  **unmeasured** (`castKeyRule`). The game names its controls - `input_talents`,
  `input_skill_bind`, `tutorial_bind_skill`, `bind_skill`, `talent_loadout`,
  `talent_reset` and `all_talents` exist in its name table, a static reading -
  and loads them through `LoadControls` and the per-control
  `s_MultiBindStruct` (`AddMultiBind`).

## Static readings

A local static reading (2026-09-25) of the named routines, the bar's Create
closures, the talent screen's and the allocation buttons' closures, the
constructors and the controls loader, in our own words. Nothing from it is
copied here, and every item below is a reading to be confirmed by phase 0,
never a fact. The static project gives call graphs, constants and control
flow, not member names; the phase 0 dumps supply the names live.

- **Between the key and the cast.** `CheckTalentUse` is the routine between
  the input and the cast. It looks up the player's HUD talent object
  (`GetPlayerTalentHudObj`), the input device and whether the mouse is
  disabled, reads the talent's info (`GetTalentInfo`), checks
  `TalentRequirement`, and only then calls `TalentUse` with five arguments;
  after it, it updates mana and life (`PlayerManaUpdate`, `PlayerHpUpdate`,
  with `ReturnSpecificStat`). So the mana cost and the requirement gate sit
  *above* `TalentUse`: a by-name `TalentUse` skips them. That is recorded as a
  research fact (`castByNameRoute`), and it is why the shipped cast presses
  the key instead.
- **The two cast entry points.** The `TalentUse` and `TalentUseClass` symbols
  are short thunks of about 300 bytes with no body a static reading reaches;
  the real bodies are reached through the name table, which is why a by-name
  hook sees them. That is a limit of the reading, not an absence.
- **The bar is built from the profile.** The bar's Create closure `anon@2503`
  finds the local player and its profile object (`GetLocalPlayerObj`,
  `GetPlayerProfileObj`), builds its buttons (`UiCreateNode`,
  `UiSetGridArray`), gives each button its activation function
  (`UiSetActivationFunc`), hides the slots with no binding
  (`UiHideTalentsWithNoBind`), and reads each talent's info and cooldown. The
  binding store is therefore expected on the profile object, mirrored into
  `row0`/`row1`/`mySkills`.
- **Allocation is hash-protected.** The allocation click on a talent button
  (`UI_Button_Talent_Player_obj` `anon@23904`) decrypts a stored string,
  recomputes a SHA-256 of the talent state, compares the two, and calls
  `ReportClient` when they differ. The sub-talent button's click
  (`UI_Button_Subtalent_obj` `anon@2428`) recomputes the SHA-256 and stores
  it, copies a struct and calls `ClearPersistSkill`. Consequence: a raw write
  to the talent or sub-talent state would trip the hash and report the
  client, so the only safe back-end route is the game's own handler, by
  name, with the real button instance as self.
- **The other talent closures.** On `UI_Button_Talent_Player_obj`, `anon@650`
  reads the talent's info and description (a tooltip) and `anon@23088` is a
  small enable or draw helper. On `UI_Talent_Button_obj`, `anon@1009` clears
  one input and `anon@1751` searches an array. The `UI_Talent_Screen_obj`
  closures build the tree (grid, focus, nodes, talent and sub-talent info);
  the `UI_Sub_Talents_obj` closures read sub-talent info and move nodes;
  `UI_Talent_Screen_Allocate_obj` `anon@643` is small and its role is left to
  measurement.
- **Network and consistency.** `NetworkSendTalentUpdate` (which sends a
  packet) and `CA_playerTalentUpdate` (which writes members) sit beside the
  handlers. Offline they may still run; the armed rows record whether an
  allocation calls them.
- **Constructors.** `UiActivateTalents`, `UiActivateHudTalentButtonFuncs`,
  `UiHudTalentNavigationFunc`, `UiTalentNavigationFunc`,
  `TalentRequirementFunc`, `SaveControlsFunc`, `LoadControlsFunc` and
  `s_MultiBindStruct` build structs of bound methods whose descriptors hold
  the script's *name*; the bodies resolve by name at run time. So the
  activation function a button runs is a named script a row can hook once
  its name is logged: `activationFunc` is a method value, and `craftprobe
  find` prints what it names.
- **Keys.** `LoadControls` queues the controls load and writes one member;
  `s_MultiBindStruct` is the per-control struct. `KeyboardMouseInput` and
  `PlayerMouseAction` are large input dispatchers and were not read further.

## Instrument

Four readers already in the research (dev) build, and one new command,
`skillprobe`, used together with them. The existing ones:

- **`menulayout`** is the player-build reader documented in
  `menu-layout-research.md` and extended by `stash-bag-layout-research.md`
  (29 objects, array values printed as `[a,b,...]`). It is the reader the hub
  tools' proofs rest on. This branch adds two things, pinned by
  `tests/test_menu_layout_contract.py` and `tests/test_skill_actions_contract.py`:
  1. **The nine talent objects** of § Static search join its candidate table
     (38 names), with these optional fields, printed only when the instance
     carries the variable, as for every other optional field: `talentId`,
     `subTalentId`, `talentLevel`, `treeIndex`, `slotIndex`, `allocated` and
     `pointsAvailable`. They are hypotheses; phase 0 prunes any name no dump
     showed.
  2. **Slot rows.** After a `UI_Hud_Talent_obj` row it prints one row per
     element of that instance's `row0` and `row1` arrays:
     `  slot=<row>,<i> talent=<talentId|none> gui=<navBboxX>,<navBboxY> win=<cx>,<cy>`,
     with `none` for an absent field, and one `  slot=<row>,* absent` row
     for an array the instance does not have (`  slot=<row>,* empty` for an
     empty one), so no answer is ever silence. The values are read only through
     `variable_instance_get`, `array_length`, `array_get` and
     `variable_struct_get`. That `navBboxX`/`navBboxY` is where the button
     draws was measured by the toggle research; that it is the right point to
     click, and how `win=` maps from it, is `slotRule`'s to confirm.
  Every existing format string, the 200-row cap and the one-name form
  (`menulayout <ObjectName>`) are unchanged, and it stays a reader: it
  clicks, hooks, writes and runs no game script.
- **`craftprobe find <Obj> <nth> [from=<i>] <text>`** (research build,
  `crafting-materials-research.md` § Instrument) walks any instance's
  variables into arrays and structs and prints the path of every match. It is
  how the binding store and the controls struct are located.
- **`tgprobe`** (research build, `toggle-skills-research.md` § Instrument):
  `deep` snapshots and diffs `Player_obj`, the talent structs,
  `Controller_obj`, the HUD object, `Skill_Controller_obj`, every global and
  a live-instance census (how the mana member that fell on a cast is named);
  `talents` walks `global.talentStructMap` (ids, `abilityId`,
  `abilityCooldown`).
- **`citrace dumpobj <Obj> [nth]`** (research build) prints every instance
  variable.

**`skillprobe`** (research build only, inside `#ifndef FORGEPACT_RELEASE`;
never in `kPlayerCommands`, and `strip_research_blocks` leaves nothing of it
in the player build). It follows `craftprobe`'s shapes - its code is the
reference for the detours, the armed lines, `call` and the refusal lines. One
row table: the thirty-three scripts and the twenty-seven closures of
§ Static search, sixty rows, each by its `HeroSiege::Scripts` constant.

- **`skillprobe`** (bare): `skillprobe: rows=<n> hooked=<k> held=<h>` - the
  build marker.
- **`hook`**: one `MmCreateHook` per row, resolved by name and attached only
  behind `AddrIsExecutableInModule`; no table swap and no address constant.
  It answers `<n> detoured, 0 failed, <h> held`. A row whose function another
  install in this plugin already detours (`tgprobe`, `toggleguard`,
  `craftprobe` - the rows § Static search lists) is reported `held`, neither
  detoured nor failed, and is never hooked twice; its counts are read from
  that probe's own `stat`/`show`. In the launch shared with the stash
  research, `craftprobe hook` has already run, so its `ReportClient`,
  `UiCreate` and `CheckPlayerInteraction` rows are expected `held`, and
  `TalentUseClass` too when `toggleguard` is armed. A row held by a ForgePact
  hook that went in inline (toggleguard's `HookTalentUseClass` installs both
  routes) has no counter anywhere and no argument log until `tgprobe`
  attaches that row through the hook's own entry note, so `hook`, `arm` and
  `show` all name the fix on that row's line: `tgprobe hook TalentUseClass`
  and `tgprobe verbose on`, then `skillprobe hook TalentUseClass`, after which
  the row reads `held by tgprobe hook` with a count and `tgprobe`'s verbose
  lines log its calls (three per `tgprobe reset`).
- **`arm <Row> [n]`** and **`show`**: per row, `calls=`; for armed calls one
  line per call with self (object and id), argc, and every argument through
  the existing value formatter (instance refs by id, structs by kind), plus
  `ret=`. A row nobody counts prints `calls=n/a (<why>)` on every `show`,
  never a 0 and never nothing. The instrument's own `ReturnTalentLevel` calls
  from `state` are neither counted nor logged.
- **`call <Row> <Obj> <nth>|id:<n> [other:<id>] [args ...] confirm`**: one
  by-name dispatch of the row's script with that instance as self and
  `other` as given (default: the self). The arguments are parsed as
  `craftprobe call` parses them - numbers, strings, `id:<n>` refs, `true` and
  `false`. The reply prints self and other in the armed lines' `Obj#index@id`
  form, and the return. It is refused without `confirm`, when the instance
  does not exist, or when the row has no resolved pointer. `other:` is parsed
  by `craftprobe`'s own `CpResolveOther` and the call goes through
  `craftprobe`'s `CpDispatchScript` (both from the stash research's
  additions), called from `skillprobe`, never copied. A closure row (a
  runtime name containing `@`) is refused the way `craftprobe call` refuses
  one, naming the route that works: `craftprobe methods <Obj> <nth>` to find
  the variable holding the closure, then `craftprobe callm <Obj> <nth> inst
  <member> [other:<id>] … confirm`.
- **`state`** (hook-free): prints, by name, each `row0`/`row1` element's
  `talentId`; `global.mySkills`; the profile object's binding array once
  `find` has located it (the `bindWriteRule` path, printed as a path plus its
  values); for every talent on the bar, `global.subTalentMap[1]`'s `t<id>`
  key with each `s<NN>` level; `ReturnTalentLevel` by name for each bar
  talent; and the points-available candidates - every numeric member of the
  profile or the player whose name contains `point`, `talent` or `skill`,
  printed with its path. Every read has its own `try` and prints
  `unreadable` for that item, never a default.
- **`keys`** (hook-free): the members of the controls struct (found by
  `craftprobe find` on `Controller_obj` or the profile for `bind_skill` or
  `input_skill_bind`) whose names contain `skill`, `talent` or `bind`, with
  their key codes.
- **`slots`** (hook-free): the same `slot=` rows `menulayout` prints, plus
  every member of one element (a dump).

`state`, `keys` and `slots` install no hook. `skillprobe` is dispatched from
`RunCommand` by its own handler as a standalone early return, like the other
research commands.

**Recording rule**, for every by-name call in the procedure (S3, S5, S6): the
stash research's rule applies verbatim. The capture and § Results carry the
*logged* shape (self, other, argument count and each argument, from the same
routine's armed line in S2 or S4 of this session) beside the *supplied* shape
(the call's own reply line). Fields a step changes on purpose (the original
talent id instead of the new one, another button) are listed as `intended:`
before the call and are not a mismatch. The outcome is exactly one of:

- `reproduced`: the shapes are equal, so the game's answer - success or
  refusal - is a measured result;
- `shape not reproduced (<field>: logged <x>, supplied <y>)`;
- `not-run (instrument: <why>)`: the instrument refused or could not reach
  the routine (a closure with no holder, a row `held` or `failed`, a row
  whose control stayed at 0 calls).

The last two are never a route negative. A § Decision line that falls back
to a UI route after either says "by-name not tested with the logged shape"
and quotes that shape.

**Positive control, every session.** `ping` answers `pong (YYTK 4.0.1)`;
`menulayout UI_Hud_Talent_obj` prints one row followed by `slot=` rows;
`skillprobe hook` answers `<n> detoured, 0 failed, <h> held`; `skillprobe
show` twice, 2 s apart, reports a non-zero and climbing `calls=` for the
`CheckPlayerInteraction` row (the control the crafting-materials research
used; when the row is `held` by `craftprobe`, the climbing count is the one
that probe reports for the same row) **and** for the `CheckTalentUse` row,
printed as the *own-detour control*. The second one is needed because a held
`CheckPlayerInteraction` proves only its holder's detours: in the launch
shared with the stash research it is `craftprobe`'s. `CheckTalentUse` runs
once per frame (`toggle-skills-research.md` Session 1) and only `tgprobe`
tables it besides this probe, so its line carries `logged=`, not `held by`,
and its climbing count proves `skillprobe`'s own detour bodies, trampolines
and counters - the plumbing every S2 and S4 row reads. If that row is held
or not climbing, `show` says `INSTRUMENT-BLIND` itself. When `hook` reports
`TalentUseClass` held by `HookTalentUseClass (toggleguard 1) (inline
detour)`, the control also includes running the fix that line names -
`tgprobe hook TalentUseClass`, `tgprobe verbose on`, then `skillprobe hook
TalentUseClass` - and seeing the row read `held by tgprobe hook`. Only that
row name goes to `tgprobe hook`: an unfiltered `tgprobe hook` would take
`CheckTalentUse` too and leave no own-detour control. Any of these failing
makes the session instrument-blind. A bare `skillprobe` answering
`skillprobe: rows=<n>` is the build marker.

**Hypotheses phase 0 tests** (none is a fact yet):

- **Cast by key** (the shipped route). The slot's key, taken from `keys`,
  pressed through `hs_input` (`hold_ms` 120) makes one `CheckTalentUse`, then
  `TalentUse` (player ref, id, 1, false, true), then `TalentUseClass` about
  19 frames later. `castProof` is whichever of these changes and is readable
  in the player build: the slot's `refreshInfoTimer`, the player's mana (a
  `tgprobe deep` diff of `Player_obj` before and after), a
  `Player_Ability_Parent_obj` descendant appearing in the census, or the
  skill timer's own remaining-time read. The cast tool ships the first that
  `skillstate` can read by name.
- **Cast by name** (a research fact only). `skillprobe call TalentUse
  Player_obj 0 id:<player> <id> 1 false true confirm` is expected to give the
  same downstream signals without the mana cost, because `CheckTalentUse` is
  skipped.
- **Bind.** The owner assigns a skill to a slot once by hand; the armed rows
  show which handler ran (the bar button's activation function, a talent
  screen button, `SaveControlsFunc`), and `state` before and after shows which
  store changed first (`bindWriteRule`; expected the profile's
  `bind_skill[player][slot]`, with `row0`/`row1`/`mySkills` refreshed from
  it). The replay calls that handler by name with its logged self, other and
  arguments (`bindRoute: byname`); else the UI route the owner described
  (`bindRoute: ui`, with the `menulayout` rows it clicks); else
  `not-observed`.
- **Talent screen open.** The owner's key (asked in the hand-back), or the
  closure or `UiCreate` arguments the rows log when the screen opens
  (`talentScreenOpenRoute`). The allocation and reset handlers need the
  screen's button instances as self, so the player verbs open the screen by
  that route first and close it with `Esc` after.
- **Allocate.** The owner allocates one point once by hand; the rows show the
  handler (`UI_Button_Talent_Player_obj` `anon@23904` expected, with
  `ReportClient` at 0 calls, and `NetworkSendTalentUpdate` or
  `CA_playerTalentUpdate` perhaps); `state` shows the points down by one and
  the talent's level up by one; the replay by name with the button instance
  as self reproduces it (`allocRoute`). A sub-talent node likewise
  (`UI_Button_Subtalent_obj` `anon@2428`; `subTalentMap[1].t<id>.s<NN>` up
  by one; `subAllocRoute`).
- **Reset.** The owner resets once by hand if the game offers it (the
  `talent_reset` name suggests a control); the rows show the handler, and the
  points come back (`resetRoute`); `not-observed` if the game offers no reset
  in town for this character.
- **Points.** `pointsReader` is the path `state` prints whose value moved by
  exactly -1 on the allocation and back on the reset.

## Live procedure

Operator-run through the hub's `hs-drive` tools, the research (dev) build of
this branch (`plugin_build\BloodPactPlugin_rel.dll`, its SHA-256 named in the
dispatch). The owner installs it; the game lease records the installed DLL's
hash. This procedure runs inside the stash and bag research launch (toolkit
workorder `hs-drive-stash-bag-actions`), after its P0-9 (stash closed) and
before its P0-10, under its own capture file: that procedure owns the lease,
the backup, the launch, the character load, and the final stop, inspect and
restore, and its `dll-hash` is this capture's too (the same lease line is
quoted). The person is needed only for one hand-back, S4. Every reply is
captured verbatim, and every check is named exactly as below. Every by-name
call follows § Instrument's recording rule: its capture line quotes the
logged armed line and the call's own reply, and its outcome is one of
`reproduced`, `shape not reproduced (…)` or `not-run (instrument: …)`.

**Character.** Save slot 14 (Sorak; the owner confirmed it has a free talent
point or a reset). It needs at least two learned skills so that a slot can be
re-bound. S1 reads the point count and the bar and records them; a zero there
is a `fail` on S1 with the value quoted, not a hand-back. Cases: one
non-toggle skill with a cooldown (the ordinary cast), one bind of an
already-learned skill into one slot, one point into one talent plus one
sub-talent node (the outlier: the hashed, screen-bound handlers), and one
reset if the game offers it. No others.

- **dll-hash.** The lease's DLL hash equals the dispatch's research-build
  hash.
- **marker.** A bare `skillprobe` answers `skillprobe: rows=<n>`.
- **control.** § Instrument's positive control: `ping` answers
  `pong (YYTK 4.0.1)`; `menulayout UI_Hud_Talent_obj` lists one row followed
  by `slot=` rows; `skillprobe hook` answers `<n> detoured, 0 failed, <h>
  held`; if its reply names `tgprobe hook TalentUseClass` on the
  `TalentUseClass` line (toggleguard's hook went in inline), run
  `tgprobe hook TalentUseClass`, `tgprobe verbose on` and `skillprobe hook
  TalentUseClass`, which must answer that row `held by tgprobe hook`;
  `skillprobe show` twice, 2 s apart, shows the `CheckPlayerInteraction`
  count non-zero and climbing, and the own-detour control `CheckTalentUse`
  (its line carries `logged=`, not `held by`) non-zero and climbing. Any of
  these failing, or `show` printing `INSTRUMENT-BLIND`, is
  `INSTRUMENT-BLIND`.
- **Running alone.** If this procedure ever runs outside the shared launch,
  the standing steps come first - a manual copy of the saves,
  `hs_saves_backup`, `hs_launch`, `hs_select_character(14)` - and S8 stops
  and restores.
- **S1, state and keys.** `skillprobe state`, `skillprobe keys` and
  `skillprobe slots`; then `craftprobe find UI_Hud_Talent_obj 0 <the
  talentId of slot 0,0 as state printed it>`, and the same `find` on the
  profile object that `GetPlayerProfileObj` names (by that instance's object
  name). Expected: every bar slot's talent id, a key code per slot, the paths
  holding the slot-0 id (the store candidates), and a points value. Record the
  slot chosen for S2: the bar talent with the largest `abilityCooldown` in
  `tgprobe talents` that is not a toggle (`toggle-skills-research.md`
  § Toggle skill table). Fixture: `menulayout UI_Hud_Talent_obj`.
- **S2, cast by key.** First `skillprobe arm TalentUse 3`, `arm
  TalentUseClass 3` and `arm CheckTalentUse 3` (and `tgprobe reset` when the
  control put `TalentUseClass` on `tgprobe`, whose verbose lines are then its
  log), and a `tgprobe deep` snapshot
  of the `Player_obj` scalars. Then `hs_input` key <that slot's key code>,
  held 120 ms. Expected: `show` gives `CheckTalentUse`, then `TalentUse`
  (self `Player_obj`, a1 the slot's talent id), then `TalentUseClass` within
  a second; `state` shows the slot's `timer` changed; a `tgprobe deep` diff
  names the mana member that fell; a screenshot shows the effect. Prior
  measurement found `CheckTalentUse` running once per frame, so its three
  armed lines may be spent on the frames before the press; the order is then
  read from its count and the other two rows' armed lines, and the capture
  says which. Record `castProof` as the readable signal or signals.
- **S3, cast by name.** (`tgprobe reset` first when `TalentUseClass` is
  `tgprobe`'s.) `skillprobe call TalentUse Player_obj 0 id:<player
  id> <id> 1 false true confirm`. Expected: `TalentUseClass` fires, the
  effect shows, and mana does not fall (`castByNameRoute`). The outcome is
  recorded under the recording rule against S2's logged `TalentUse` line: a
  `reproduced` refusal, or no effect, is `not-observed` and not a defect;
  `shape not reproduced` and `not-run` are written as such.
- **S4, hand-arming** (NEEDS-HUMAN, the one hand-back). Every row armed with
  `skillprobe arm <Row> 5` first, and `state` and `keys` read before. The
  request to the owner: "1. Assign a different skill to bar slot <the S2
  slot> the way you normally do (say which key or menu you used). 2. Open
  the talent screen (say which key opens it), allocate one point into <a
  talent with a free point - name it>, and allocate one sub-talent node if
  one is available (name it). 3. If the game lets you reset or unallocate
  here, do it once and say what you pressed. Reply with what you did." Then
  `skillprobe show` (every armed line, in order), `skillprobe state`,
  `skillprobe keys`, and - if the screen is open - `menulayout` of
  `UI_Talent_Screen_obj`, `UI_Button_Talent_Player_obj`,
  `UI_Button_Subtalent_obj` and `UI_Talent_Screen_Allocate_obj` (fixture).
  Expected: one handler per action, each with a logged self (object and id),
  other and arguments; `state` shows the slot's new id, the points down by
  one, the sub-node up by one, and after the reset the points back. Record
  `bindWriteRule` (the path that changed first), the three shapes, and
  `talentScreenOpenRoute` (the key, and any closure or `UiCreate` row that
  fired on the open).
- **S5, bind by name.** Replay S4's bind shape to put the original skill
  back in the slot: `skillprobe call <Row> <Obj> <nth>|id:<n> [other:<id>]
  <args as logged, with the original id> confirm`. Expected: `state` shows
  the original id in the slot and the store path agrees (`bindRoute:
  byname`). Otherwise record the refusal with both shapes and its outcome
  under the recording rule, and `bindRoute: ui` with the owner's described
  steps and the rows `menulayout` lists for them. A closure in S4's log is
  replayed through `craftprobe methods` and `callm … inst`, never through
  `skillprobe call`.
- **S6, allocate, sub-allocate and reset by name.** With the screen open by
  `talentScreenOpenRoute` and its button instances listed by `menulayout`,
  replay S4's allocation shape with the same talent's button instance as
  self. Expected: `state` shows the points down by one and the level up by
  one, with `ReportClient` at 0 calls (`allocRoute: byname`). The sub-node
  shape likewise (`subAllocRoute`), and the reset shape (`resetRoute`),
  expected to restore the points. If the game offered no reset,
  `resetRoute: not-observed`, and the allocation stays (the session's restore
  undoes it). `Esc` closes the screen; `state` is unchanged by the close.
- **S7, close-state.** `menulayout UI_Talent_Screen_obj` lists nothing after
  `Esc`, and `skillprobe state` equals the S6 end state.
- **S8, hand-over.** In the shared launch: `skillprobe state` one last time,
  then hand back to the stash procedure's P0-10, whose stop, inspect, restore
  and lease release cover this session; that capture's P0-10 line is quoted
  here as `observed:`. Running alone: `hs_stop_game` (`exited: true`,
  `forced: false`); `hs_saves_inspect` (the character's save among the
  changed files); `hs_saves_restore` with the pre-launch backup; inspect
  again, clean; `hs_lease_release`; the DLL hash after equals the hash
  before.

A `not-observed` on S2 (the cast by key) or on S4 (the hand-arming) sends the
plan back for revision rather than into an implementation round. A single
by-name replay going unobserved (S5, S6) does not: that verb is left out of
the player build and its hub tool refuses `route_not_measured`. Nor does a
replay recorded `shape not reproduced` or `not-run (instrument …)`, which is
never written as `not-observed`: the § Decision line records the UI route,
names the by-name route as not tested with the logged shape, and quotes that
shape. A logged shape that was reproduced and that the game refused is a
measured negative, and is recorded as one, with both shapes quoted.

## Results

Phase 0 has not run. One dated row per check, filled from the live capture.
For a by-name call, *Logged shape* is the self, other, argument count and
arguments of the game's own call (its armed line) and *Supplied shape* those
of the replay (its reply line), with the outcome under § Instrument's
recording rule in *Observation*; `-` where a check makes no by-name call.

| Check | Observation | Logged shape | Supplied shape | Control | Date |
| --- | --- | --- | --- | --- | --- |
| dll-hash | pending | - | - | - | - |
| marker | pending | - | - | - | - |
| control | pending | - | - | pending | - |
| S1 | pending | - | - | - | - |
| S2 | pending | pending | - | - | - |
| S3 | pending | pending | pending | - | - |
| S4 | pending | pending | - | - | - |
| S5 | pending | pending | pending | - | - |
| S6 | pending | pending | pending | - | - |
| S7 | pending | - | - | - | - |
| S8 | pending | - | - | - | - |

## Decision

Twelve lines, each `pending` until phase 0 has measured it. The player verbs
and the hub's skill tools are written against these lines and against the
verbatim replies phase 0 records, not against the hypotheses above. A route
line names the by-name shape (script or closure, self, other, arguments) or
the UI route it fell back to; `not-observed` only when neither was observed.
A line that fell back after a by-name call recorded `shape not reproduced` or
`not-run (instrument …)` says "by-name not tested with the logged shape" and
quotes that shape.

slotRule: pending
castKeyRule: pending
castProof: pending
castByNameRoute: pending
bindWriteRule: pending
bindRoute: pending
talentScreenOpenRoute: pending
allocRoute: pending
subAllocRoute: pending
resetRoute: pending
pointsReader: pending
talentIdRule: pending

What each line records:

- **slotRule**: how a bar slot's window point follows from its element's
  `navBboxX`/`navBboxY` (and the bar's own offsets and scale), and which of
  `row0`/`row1` is the drawn bar (S1).
- **castKeyRule**: where a slot's key code is read - the controls member and
  its path - and the code per slot (S1).
- **castProof**: the signal or signals a cast by key changed that the player
  build can read by name (S2).
- **castByNameRoute**: what a by-name `TalentUse` did - the effect, and
  whether mana fell - with its outcome under the recording rule; a research
  fact, not a shipped route (S3).
- **bindWriteRule**: the store a hand binding changed first, as a path, and
  which of the other three places were refreshed from it (S4).
- **bindRoute**: the handler called by name with its self, other and
  arguments, or `ui` with the owner's steps and the rows clicked (S4, S5).
- **talentScreenOpenRoute**: the key that opens the talent screen, and any
  closure or `UiCreate` call that fired on the open (S4).
- **allocRoute** and **subAllocRoute**: the handler called by name with the
  button instance as self, its other and its arguments, and whether
  `ReportClient`, `NetworkSendTalentUpdate` or `CA_playerTalentUpdate` ran
  (S4, S6).
- **resetRoute**: the reset handler by name with its shape, or
  `not-observed` if the game offered no reset (S4, S6).
- **pointsReader**: the path whose value moved by exactly -1 on the
  allocation and back on the reset (S4, S6).
- **talentIdRule**: how an `abilityId` resolves to a numeric talent id -
  expected the walk of `global.talentStructMap` the toggle mods already make
  (S1).
