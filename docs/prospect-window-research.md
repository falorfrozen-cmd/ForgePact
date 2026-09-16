# Bigger prospect window — research log

phase0-status: pending

Issue: [ForgePact #9](https://github.com/falorfrozen-cmd/ForgePact/issues/9),
"[QoL] Bigger prospect window" — *"Currently prospect window is way too small
for the amount of items players can hold in their inventory."* Opened
2026-09-15, label `enhancement`.

Status (2026-09-17): **research stage, Phase 0 not yet run.** Nothing
player-visible exists. What controls the window's size is **not known**: the
`hs-game-sdk` tables name the objects, scripts and closures involved (below,
complete) but carry no instance variable, argument shape or constant for any
UI object. So the work is staged:

- **Stage A (done in this change):** a research instrument, `prospectprobe`
  (research build only), that native-detours every static-search candidate in
  one build; a game-independent sizing core
  (`plugin/include/ForgePact/ProspectWindowMod.hpp`) with its baseline/target
  harness (`tests/test_prospect_window_behavior.py`); this document; and
  contract tests (`tests/test_prospect_window_contract.py`) pinning all of it.
- **Live session (human):** one research build, one relaunch — § Live
  procedure — with the numbers written into § Results.
- **Stage B (only once § Results names H1 or H2):** the mod itself, a panel
  toggle, docs and release notes.

What "bigger" means here: **capacity** — more cells per prospect operation —
not the same cells drawn larger. The complaint is relative to the inventory's
size, so the window's own input grid is what is too small.

## Static search

Run 2026-09-17 over `hs-game-sdk/cpp/include/hs_game_sdk/{scripts,objects}.hpp`,
`hs-game-sdk/python/hs_game_sdk/{objects,sprites,sounds,scripts}.py`, the
hierarchy helpers, and every submodule's sources. Names and indices only.

### Objects

| Object | Index | Ancestors | Note |
|---|---|---|---|
| `UI_Prospect_obj` | 5220 | `UI_Inventory_Parent_obj` (5115) → `UI_Parent_obj` (5205) | the window |
| `UI_Journal_Prospecting_obj` | 5126 | `UI_Parent_obj` | the journal *tab* (recipe list), not the window |
| `UI_Button_Journal_Prospect_obj` | 4994 | `UI_List_Item_Parent_obj` (5130) | journal list row |
| `Prospect_Cube_obj` | 3725 | `Quest_NPC_Parent_obj` → `Collision_Parent_obj` → `Avoidable_Parent_obj` | the world object the player interacts with |
| `UI_Inventory_Grid_obj` | 5112 | `UI_Node_Parent_obj` (5186) | an item-grid *node* — the likely carrier of the input grid |
| `UI_Grid_obj` | — | `UI_Node_Parent_obj` | generic grid node |
| `UI_Container_obj` | 5050 | `UI_Node_Parent_obj` | generic container node |

`UI_Inventory_Parent_obj` has exactly ten children, and they are the windows
that show the player's inventory beside their own content:
`UI_Angelic_Upgrade_obj`, `UI_Craft_obj`, `UI_Incarnation_Socket_obj`,
`UI_Inventory_obj`, `UI_Inventory_Trade_obj`, `UI_Mailbox_Message_Send_obj`,
`UI_Market_Add_Item_obj`, `UI_Merchant_obj`, `UI_Prospect_obj`,
`UI_Stash_obj`. The mod may only ever touch a call or instance whose owner is
`UI_Prospect_obj`; the instrument may observe the siblings.

No sprite is named for the prospect window (`sprites.py` has only
`Prospect_Cube_Idle_spr` 17412 and `Craft_Animation_Prospect_Copper_spr` 5449),
and `UI_Prospect_obj` sets no mask sprite.

Object *event* code (Create/Step/Draw) is not in any table, and raw
`gml_Object_<Obj>_<Event>_<N>` names were measured **not to resolve** through
`GetNamedRoutinePointer` — session 7 of
`docs/pet-quest-collector-research.md` on three other objects, and again for
`objMinimap` on 2026-09-16. So if the size is a literal inside
`UI_Prospect_obj`'s Create event, the only windows into it are (a) the named
scripts that event calls and (b) the instance variables it leaves behind. The
instrument covers both.

### Scripts

Every row below is a row of `prospectprobe`'s target table. The runtime name
is the `HeroSiege::Scripts` constant's value, used as-is.

*Prospect family:*

| Probe label | Runtime name | SDK index |
|---|---|---|
| `UiAProspectButton` | `gml_Script_UiAProspectButton` | 975 |
| `___struct___120@UiAProspectButton` | `gml_Script____struct___120@UiAProspectButton@DefineProspectCombos` | 970 |
| `___struct___122@___struct___121` | `gml_Script____struct___122@___struct___121@UiAProspectButton@DefineProspectCombos` | 971 |
| `___struct___121@UiAProspectButton` | `gml_Script____struct___121@UiAProspectButton@DefineProspectCombos` | 972 |
| `___struct___124@___struct___123` | `gml_Script____struct___124@___struct___123@UiAProspectButton@DefineProspectCombos` | 973 |
| `___struct___123@UiAProspectButton` | `gml_Script____struct___123@UiAProspectButton@DefineProspectCombos` | 974 |
| `UI_Prospect_obj anon@1038` | `gml_Script_anon@1038@gml_Object_UI_Prospect_obj_Create_0` | 6067 |
| `UI_Prospect_obj anon@2729` | `gml_Script_anon@2729@gml_Object_UI_Prospect_obj_Create_0` | 6068 |
| `UI_Prospect_obj anon@3551` | `gml_Script_anon@3551@gml_Object_UI_Prospect_obj_Create_0` | 6069 |
| `Prospect_Cube_obj anon@320` | `gml_Script_anon@320@gml_Object_Prospect_Cube_obj_Create_0` | 5198 |
| `UI_Button_Journal_Prospect_obj anon@324` | `gml_Script_anon@324@gml_Object_UI_Button_Journal_Prospect_obj_Create_0` | 5461 |
| `UI_Journal_Prospecting_obj anon@1003` | `gml_Script_anon@1003@gml_Object_UI_Journal_Prospecting_obj_Create_0` | 5690 |
| `UI_Node_Parent_obj anon@1508` | `gml_Script_anon@1508@gml_Object_UI_Node_Parent_obj_Create_0` | — |
| `UI_Node_Parent_obj anon@1909` | `gml_Script_anon@1909@gml_Object_UI_Node_Parent_obj_Create_0` | — |

`UiAProspectButton` is the prospect executor / button action.
`DefineProspectCombos` (969), `InventoryGrid` (1958), `InventoryGridHelperFuncs`
(1987) and `UiFuncs` (4459) are script-file *containers*, not routines — not
hookable, and not in the table.

*UI framework (`UiFuncs`, 4460–4488):*

| Probe label | Runtime name |
|---|---|
| `UiCreate` | `gml_Script_UiCreate` |
| `UiCreateNode` | `gml_Script_UiCreateNode` |
| `UiSetGrid` | `gml_Script_UiSetGrid` |
| `UiSetGridArray` | `gml_Script_UiSetGridArray` |
| `UiResetGrid` | `gml_Script_UiResetGrid` |
| `UiMoveNode` | `gml_Script_UiMoveNode` |
| `UiCreateSameLevel` | `gml_Script_UiCreateSameLevel` |
| `UiCreateContainer` | `gml_Script_UiCreateContainer` |
| `UiContainerChange` | `gml_Script_UiContainerChange` |
| `UiChangeVisibility` | `gml_Script_UiChangeVisibility` |
| `UiSetRef` | `gml_Script_UiSetRef` |
| `UiSetNodeScale` | `gml_Script_UiSetNodeScale` |
| `UiRemoveNode` | `gml_Script_UiRemoveNode` |
| `___struct___408@UiCreate` | `gml_Script____struct___408@UiCreate@UiFuncs` |
| `___struct___409@UiCreateNode` | `gml_Script____struct___409@UiCreateNode@UiFuncs` |
| `___struct___410@UiCreateContainer` | `gml_Script____struct___410@UiCreateContainer@UiFuncs` |

`UiSetGrid` / `UiSetGridArray` are the strongest candidates for "the call
that sizes a grid node"; `s_ItemGridInfo` below is the other. None of this is
measured — it is a candidate list.

*Inventory-grid family (1954–1998):*

| Probe label | Runtime name |
|---|---|
| `InventoryResetTabs` | `gml_Script_InventoryResetTabs` |
| `InventoryInitGrids` | `gml_Script_InventoryInitGrids` |
| `GetInventoryGridNode` | `gml_Script_GetInventoryGridNode` |
| `UiResizeInventoryNodes` | `gml_Script_UiResizeInventoryNodes` |
| `s_ItemOperation` | `gml_Script_s_ItemOperation` |
| `s_InvNode` | `gml_Script_s_InvNode` |
| `InventoryGridAddItem` | `gml_Script_InventoryGridAddItem` |
| `GridHasSpace` | `gml_Script_GridHasSpace` |
| `InventoryGridHasSpace` | `gml_Script_InventoryGridHasSpace` |
| `GridAddItem` | `gml_Script_GridAddItem` |
| `GetGridTypeName` | `gml_Script_GetGridTypeName` |
| `GridClear` | `gml_Script_GridClear` |
| `ParseItemToGrid` | `gml_Script_ParseItemToGrid` |
| `s_ItemGridInfo` | `gml_Script_s_ItemGridInfo` |
| `GetItemPreferredGrid` | `gml_Script_GetItemPreferredGrid` |
| `InvGridClearItemNode` | `gml_Script_InvGridClearItemNode` |
| `GetStackOpLocationFromGridType` | `gml_Script_GetStackOpLocationFromGridType` |

The `s_` prefix is this game's struct-constructor naming, so `s_ItemGridInfo`
and `s_InvNode` are very likely the per-grid and per-node records.

*Controls:*

| Probe label | Runtime name | Why |
|---|---|---|
| `CheckPlayerInteraction` | `gml_Script_CheckPlayerInteraction` | proven to fire from every interactable's Step event (`citrace nativetrace`); must count |
| `PlayerMouseAction` | `gml_Script_PlayerMouseAction` | fires on a click |

Both already have `citrace nativetrace` rows; `prospectprobe` hooks them
again under its own ids so the control runs through the *same* installer as
the targets.

### Negative results, sourced

- No name in any SDK table contains "prospect" beyond the ones above
  (case-insensitive substring search over scripts, objects, sprites, sounds).
- HSSaveEditor, hero-siege-item-editor, HS-ValueEditor, HS-Offline-Tracker and
  Hs-Offline-Loot-Forge contain no "prospect". Whether items left in the
  prospect grid persist in the save is therefore **not observed in the editors'
  models** — not "does not persist". R7 measures it.
- HSCraftSim (`RESEARCH.md`) has the 48-recipe prospect table (each prospect
  item mapped to its result with a tier requirement) and lists the prospect
  executor as still to extract. It says nothing about the window.

## Hypotheses

| | Mechanism | Shipped design it implies | Required evidence (all of it) |
|---|---|---|---|
| **H1** | A named script receives the input grid's dimensions as arguments when the window opens (`UiSetGrid`, `UiCreateNode`, `s_ItemGridInfo`, `InventoryInitGrids`, …). | **Design A**: `HookOneScript` (both routes) on that script; when the call belongs to the prospect window, scale the dimension arguments before the trampoline. | (a) R4: a row fired between `arm` and `show` on a window open whose logged args carry R3's numbers, with `self`/`other`/an argument identifying the prospect window or its node; (b) **positive control on the same row: `prospectprobe override <row> <argIndex> <value> 1` followed by a reopen draws a grid of the overridden size** (R5a = yes) — a row that carries the numbers but whose override changes nothing is recorded and does not count; (c) R6: items placed in the new cells are consumed by the prospect button; (d) `CheckPlayerInteraction` counted > 0 in the same session (C). |
| **H2** | The dimensions live in instance variables on the window (or its grid node) that the draw and the cell store follow. | **Design B**: a frame-driven pass that, once per new `UI_Prospect_obj` instance, writes the variables the game left, validated numeric and read back. | (a) R2: numeric variables on R1 whose vanilla values equal R3; (b) **`prospectprobe set` on them, without reopening, changes the drawn grid *and* accepts an item dropped into a new cell with no error** (R5b = yes) — a bigger frame that refuses items proves the store did not follow; (c) R6 as H1; (d) the `x` write control moved the window (C) and (e) a reopen restores vanilla (the write was per-instance). |
| **H3** | The size is fixed (literals in event code with a fixed store), and no named call or variable governs it. | **No one-value mod exists.** Blocked, with the numbers; follow-up below. | Every H1 row's override and every H2 candidate's write measured with its control passing, none positive; C passing. |
| **not observed** | Any control failed, the window object never resolved, the enumeration printed nothing, or the session ended on a GML error before a control ran. | No hypothesis is concluded; Stage B does not start. | Record each R-field as `not observed (<which instrument, which control>)`. |

Both designs stay in the "change one value inside a call the game is already
making" class (`AGENTS.md`, "Don't Suspend the Game's Own Runtime"): Design A
rewrites two arguments inside the game's own sizing call, Design B writes two
variables the game already reads. Neither adds a call the game is not making,
and nothing pauses, re-creates or re-draws the window.

**Follow-up if H3 or not observed:** read, locally in Ghidra, the code that
calls the three `UI_Prospect_obj` Create closures (found through
`citrace symdump` and `tools/ghidra/ImportSymbols.java`), to learn which callee
sizes the store. What is learned is paraphrased into § Results; nothing
decompiled enters this or any tracked file, and that read never closes the
issue by itself.

## Instrument

The sizing decision itself is `ForgePact::ProspectWindowMod`
(`plugin/include/ForgePact/ProspectWindowMod.hpp`): off returns the vanilla
size and applies nothing; on scales each axis by `kProspectColsFactor` /
`kProspectRowsFactor`, clamps to `kProspectMaxCols` / `kProspectMaxRows`, never
shrinks, applies once per window instance id (a latch of 64 ids), refuses a
vanilla size with an axis `<= 0`, and prints a stat line naming what it did.
The factors and caps are **placeholders** until R3 and R8 are measured (default
decision: the inventory grid's size, capped at 2x vanilla per axis). Nothing
calls it yet, and no command exposes it.

### Hook-free instruments (existing, research build)

These come first in the live procedure, because they cost no hook and cannot
be blind in the way a detour can.

- `citrace dumpobj <Obj> [nth]` — every instance variable (name and value) of
  the nth live instance of a named object, method values resolved. The
  enumeration instrument for `UI_Prospect_obj` and its grid nodes
  (`UI_Inventory_Grid_obj`, `UI_Grid_obj`, `UI_Container_obj`).
  Control: a name it printed must read back through `oget`, a different route.
- `inames <Obj> [filter]`, `oget <Obj> <var>`, `oset <Obj> <var> <num>` (first
  instance only, no kind check — which is why `prospectprobe set` exists),
  `ojson <Obj> <var>`, `gnames [filter]`, `gjson <global>`,
  `cb <builtin> [args]`.
- `tools/ipc.ps1` sends one command and prints only its reply
  (`.\ForgePact\tools\ipc.ps1 "citrace dumpobj UI_Prospect_obj"`).

### `prospectprobe` (research build only)

Not in `kPlayerCommands`; dispatched from `HandleProspectCommand`. Bare
`prospectprobe` prints the usage.

- **`prospectprobe set <Obj> <nth> <var> <number>`** — the bounded write.
  Refuses, with no write made: an object `asset_get_index` does not know
  (`unknown object`); `nth` outside `instance_number` (`no such instance`); a
  variable `variable_instance_exists` denies (`no such variable` — it never
  creates one), except the fixed built-ins `x`, `y`, `depth`, `visible`,
  `image_xscale`, `image_yscale`, `image_alpha`, which always exist and cannot
  be created, and are accepted either way because whether that builtin answers
  true for a built-in is unmeasured on this runner (the output prints
  `exists=`); and a current value that is not a finite real/int32/int64
  (`<var> is <kind>, not a number`). A string, `undefined` or method value is
  refused before any write, because a GML type error inside a builtin is the
  runner's fatal dialog. The instance from `instance_find` is passed through
  with whatever kind it has (`VALUE_REF` on this runner). Prints
  `prospectprobe set <Obj>[nth].<var>: was=<v> now=<read-back> (readback ok|MISMATCH) exists=…`.
  **Write control:** `prospectprobe set UI_Prospect_obj 0 x <x+60>` must move
  the window by eye before any other write is believed.
- **`prospectprobe hook [substr ...]`** — native-detours every row of the
  table in § Static search (or only rows whose label contains one of the
  substrings, to bisect a crash). Each name resolves through
  `GetNamedRoutinePointer` to the compiled function, and the address is
  **refused before `MmCreateHook`** unless it is committed, executable code
  inside `Hero_Siege.exe`'s own image — which also refuses a table entry some
  table hook already swapped for a plugin detour. Prints, per row,
  `detoured <label> at exe+0x…`, `not found st=…`, `refused (<why>)`, or
  `MmCreateHook failed st=…`, then `N detoured, M failed`. Idempotent per row.
  Do not run `citrace nativetrace` in the same session: it detours two of the
  same addresses, and the second hook on an address fails.
- **`prospectprobe arm`** — zeroes every counter and log budget and arms
  logging: the next 6 calls of each row are written to `out.txt` as
  `prospectprobe <label> #n self=… other=… argc=… a0=… a1=…`. A `self` or
  `other` without a numeric `object_index` (a struct, as constructors and
  struct closures receive) is printed as `(not an instance: …)` and never
  handed to `object_get_name`.
- **`prospectprobe show`** — per row: `calls`, `since` the previous show, and
  whether the log budget is spent; `(not detoured)` for a row that did not
  install. The last line is the control, `CheckPlayerInteraction: calls=N`:
  **`0` voids every row above**, and a control that did not install says so.
- **`prospectprobe reset`** — zeroes counters and disarms.
- **`prospectprobe override <label> <argIndex> <number> [calls=1]`** — for the
  next `calls` calls of an already-detoured row, if argument `argIndex` exists
  and is numeric, replace it before forwarding and log
  `override <label> a<i>: was=<v> now=<value>`. Refuses a label that is not a
  row, and a row that is not detoured (`hook it first`). A call whose argument
  is missing or not numeric is logged as `not applied` (up to 6 lines) and does
  not use up the count. `prospectprobe override clear` cancels. Labels may
  contain a space (`UI_Prospect_obj anon@1038`); everything before the trailing
  numbers is the label. This is H1's positive control.

Counting is unconditional; logging is budgeted per row so a hot row
(`GridHasSpace`, the controls) cannot drown `out.txt`. Every detour forwards to
the game's own function through the trampoline.

## Live procedure

One research build, one relaunch. Back up the save first
(`%LOCALAPPDATA%\Hero_Siege`), use junk items in the grid, and expect a GML
runtime error at step L8 to end the session — that is itself a recorded
result, not a failure of the procedure.

1. **L1.** `plugin_build\build.bat dev`; with the game closed copy
   `plugin_build\BloodPactPlugin_rel.dll` over
   `<game>\mods\aurie\BloodPactPlugin.dll`. (Pressing **Install** in the panel
   afterwards restores the ship DLL.) Launch, load a character,
   `.\ForgePact\tools\ipc.ps1 ping`.
2. **L2 (hook-free).** Before opening the window:
   `citrace dumpobj UI_Prospect_obj` → expect `no live instances`. If it
   prints an instance while the window is closed, record `R1-note: window is
   resident while closed` — Design B must then key on visibility, not
   existence.
3. **L3.** Walk to the Prospect Cube (mining site) and open the prospect
   window. By eye, count the input grid's columns × rows (**R3**) and the
   inventory grid's (**R8**). Note whether the two are drawn by the same
   window.
4. **L4 (enumeration).** `citrace dumpobj UI_Prospect_obj`, then
   `cb instance_number <index of UI_Inventory_Grid_obj>` and
   `citrace dumpobj UI_Inventory_Grid_obj <nth>` for each nth (likewise
   `UI_Grid_obj`, `UI_Container_obj`). Identify the instance whose numeric
   variables equal R3 (**R1** = object + nth; **R2** = the variable names and
   vanilla values). Control: `oget <R1 object> <one R2 name>` (first instance
   only — if R1's nth ≠ 0, use `inames` count instead) returns the same value
   `dumpobj` printed; `inames <R1 object>` total equals `dumpobj`'s count. If
   no variable equals R3, R2 = `not observed (dumpobj: no variable equals R3
   on UI_Prospect_obj or any grid node; enumeration control <passed|failed>)`.
5. **L5 (write control).** `oget UI_Prospect_obj x`, then
   `prospectprobe set UI_Prospect_obj 0 x <x+60>` → the window moves right by
   eye (**C-write = yes/no**). Restore with the original value.
6. **L6.** Close and reopen the window: R2's values are vanilla again
   (Create re-ran) — record yes/no.
7. **L7 (H2 experiment).** For each R2 candidate: `prospectprobe set <R1 obj>
   <nth> <var> <vanilla×2>`. By eye: does the drawn grid change? Drag an item
   into a cell that did not exist in vanilla: accepted, refused, or GML error
   (**R5b** per variable). Note whether R8's size would fit on screen at this
   position (for D2).
8. **L8.** If R5b = accepted for some variables: press the prospect button
   with items in both vanilla and new cells — are the new cells' items
   consumed (**R6**)? Then put junk items in the grid, close the window, and
   record where they went; reopen — still there? (**R7**: `returned to
   inventory` / `kept in grid` / `lost`).
9. **L9 (hooks).** Close the window. `prospectprobe hook` → record
   `N detoured, M failed` and every `not found`/`refused` row.
   `prospectprobe show` → `CheckPlayerInteraction: calls=` must already be
   climbing (**C-hook**); if 0, stop and record `not observed (control)`.
10. **L10.** `prospectprobe arm`, open the window, `prospectprobe show` and
    read the logged lines in `bp_ipc\out.txt`: rows that fired between arm
    and show, with `self`, `other`, args (**R4** = every row whose args carry
    R3's numbers, with the arg indices; else `not observed among detoured
    rows (list the rows that did fire)`).
11. **L11 (H1 experiment).** For each R4 row: close the window,
    `prospectprobe override <label> <argIndex> <vanilla×2> 1`, reopen → is
    the drawn grid the overridden size (**R5a** per row)? If yes, repeat L8's
    R6/R7 checks against this grid. `prospectprobe override clear`.
12. **L12.** Fill `## Results`: R1–R8, C (C-write, C-hook, enumeration
    control, R6), H per § Deciding the hypothesis. Add the log line
    `phase0: complete` to this workorder **only** if H reads H1 or H2; an H3
    or not-observed H is reported to the human with the numbers (`BLOCKED`).
13. **L13 (fallback, only if H = H3 / not observed).** `citrace symdump`,
    `tools/ghidra/ImportSymbols.java` headless, read the callers of the three
    `UI_Prospect_obj` Create closures locally; paraphrase what sizes the store
    into `## Results` under `Ghidra read (paraphrase)`; nothing decompiled in
    any tracked file. This informs a replan; it never closes the issue.

## Deciding the hypothesis

A row whose arguments carry the vanilla numbers is a candidate, not a result; only an override on that row that changes the drawn grid counts for H1.

A variable write that enlarges the drawn frame but not the cells the game accepts items into does not count for H2.

Read § Hypotheses' required-evidence column as a conjunction: every item of a
row must be present, with its control passing, in the same session.

- **H1** needs R4 (a row carrying R3's numbers on a window open, identifying
  the prospect window), R5a = yes on that same row, R6 = consumed, and
  C-hook > 0.
- **H2** needs R2 (numeric variables equal to R3), R5b = accepted for them
  without a reopen, R6 = consumed, C-write = yes, and L6 = vanilla again on
  reopen.
- **H3** needs every H1 override and every H2 write measured with its control
  passing, and none positive.
- Anything else is **not observed**, naming the instrument and the control
  that failed. A zero from a detour without a climbing `CheckPlayerInteraction`,
  or an empty enumeration without a passing `oget` read-back, is a statement
  about the instrument, not the game.

If R7 reads `kept in grid`, an enlarged grid would hold items in cells vanilla
does not have, and turning the mod off later could strand them. Stage B then
ships only with a Known Limitations entry, panel copy telling the player to
empty the grid first, and an explicit human acceptance — or does not ship.

## Results

Not yet measured. Every cell reads `unknown` until the live session fills it.

| Field | Meaning | Value |
|---|---|---|
| R1 | object + nth of the instance carrying the grid size (L4) | unknown |
| R1-note | window resident while closed? (L2) | unknown |
| R2 | variable names and vanilla values equal to R3 (L4) | unknown |
| R3 | vanilla input grid, columns × rows, by eye (L3) | unknown |
| R4 | detoured rows whose args carry R3 on a window open, with arg indices and identification (L10) | unknown |
| R5a | override on each R4 row changes the drawn grid (L11) | unknown |
| R5b | `set` on each R2 variable changes the drawn grid and accepts an item in a new cell (L7) | unknown |
| R6 | items in new cells consumed by the prospect button (L8/L11) | unknown |
| R7 | items left in the grid on close: returned to inventory / kept in grid / lost (L8) | unknown |
| R8 | inventory grid, columns × rows, by eye (L3) | unknown |
| C | C-write (L5), C-hook (L9), enumeration control (L4), L6 reopen restores vanilla | unknown |
| H | H1 / H2 / H3 / not observed | unknown |
