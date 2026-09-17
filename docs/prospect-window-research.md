# Bigger prospect window — research log

phase0-status: pending

Issue: [ForgePact #9](https://github.com/falorfrozen-cmd/ForgePact/issues/9),
"[QoL] Bigger prospect window" — *"Currently prospect window is way too small
for the amount of items players can hold in their inventory."* Opened
2026-09-15, label `enhancement`.

Status (2026-09-17): **research stage.** Phase 0a ran 2026-09-17 and recorded
H = not observed (instrument blind: stale SDK closure names; SDK regenerated,
hub `4539e68`). Phase 0b pending. Nothing player-visible exists. What controls
the window's size is **not yet established**: Phase 0a measured *where* the
size lives (on the window's `"ProspectGrid"` node, § Results R1/R2) and that a
bare write of it crashes the node's draw (R5b), but not *which call builds the
node's cell store* — and the rows most likely to answer that were never hooked,
because the `hs-game-sdk` closure names the instrument used had been
renumbered by a game patch. So the work is staged:

- **Stage A (done):** a research instrument, `prospectprobe` (research build
  only), that native-detours every static-search candidate in one build; a
  game-independent sizing core
  (`plugin/include/ForgePact/ProspectWindowMod.hpp`) with its baseline/target
  harness (`tests/test_prospect_window_behavior.py`); this document; and
  contract tests (`tests/test_prospect_window_contract.py`) pinning all of it.
- **Phase 0a (live, done):** § Results, Phase 0a column.
- **Phase 0b (instrument done, live session pending):** the target table
  re-derived from the regenerated SDK (45 closure rows, enforced by a test), a
  grid snapshot around every logged call (`watch`), a hook-free read of the
  window and node (`grid`), and the two write experiments a positive result
  needs (`setat`, `resize … via`). One research build, one relaunch unless a
  crash forces a second — § Live procedure.
- **Stage B (only once § Results names H1 or H2'):** the mod itself, a panel
  toggle, docs and release notes.

What "bigger" means here: **capacity** — more cells per prospect operation —
not the same cells drawn larger. The complaint is relative to the inventory's
size, so the window's own input grid is what is too small.

## Static search

Run 2026-09-17 over `hs-game-sdk/cpp/include/hs_game_sdk/{scripts,objects}.hpp`,
`hs-game-sdk/python/hs_game_sdk/{objects,sprites,sounds,scripts}.py`, the
hierarchy helpers, and every submodule's sources, and re-run for the closure
rows against the SDK as regenerated on 2026-09-17 (hub `4539e68`). Names and
indices only.

### Objects

| Object | Index | Ancestors | Note |
|---|---|---|---|
| `UI_Prospect_obj` | 5220 | `UI_Inventory_Parent_obj` (5115) → `UI_Parent_obj` (5205) | the window |
| `UI_Journal_Prospecting_obj` | 5126 | `UI_Parent_obj` | the journal *tab* (recipe list), not the window |
| `UI_Button_Journal_Prospect_obj` | 4994 | `UI_List_Item_Parent_obj` (5130) | journal list row |
| `Prospect_Cube_obj` | 3725 | `Quest_NPC_Parent_obj` → `Collision_Parent_obj` → `Avoidable_Parent_obj` | the world object the player interacts with |
| `UI_Inventory_Grid_obj` | 5112 | `UI_Node_Parent_obj` (5186) | an item-grid *node*; Phase 0a measured the input grid on one (R1) |
| `UI_Grid_obj` | 5081 | `UI_Node_Parent_obj` | generic grid node |
| `UI_Container_obj` | 5050 | `UI_Node_Parent_obj` | generic container node |

`UI_Inventory_Parent_obj` has exactly ten children, and they are the windows
that show the player's inventory beside their own content:
`UI_Angelic_Upgrade_obj`, `UI_Craft_obj`, `UI_Incarnation_Socket_obj`,
`UI_Inventory_obj`, `UI_Inventory_Trade_obj`, `UI_Mailbox_Message_Send_obj`,
`UI_Market_Add_Item_obj`, `UI_Merchant_obj`, `UI_Prospect_obj`,
`UI_Stash_obj`. The mod may only ever touch a call or instance whose owner is
`UI_Prospect_obj`; the instrument may observe the siblings.

No sprite is named for the prospect window (`sprites.py` has only
`Prospect_Cube_Idle_spr` and `Craft_Animation_Prospect_Copper_spr`), and
`UI_Prospect_obj` sets no mask sprite. Phase 0a read the node's background as
`Craft_Grid_Large_spr`, shared with the craft cube.

Object *event* code (Create/Step/Draw) is not in any table, and raw
`gml_Object_<Obj>_<Event>_<N>` names were measured **not to resolve** through
`GetNamedRoutinePointer` — session 7 of
`docs/pet-quest-collector-research.md` on three other objects, and again for
`objMinimap` on 2026-09-16. So if the size is a literal inside an object's
Create event, the only windows into it are (a) the named scripts and closures
that event calls, (b) the instance variables it leaves behind, and (c) the
shape of those variables before and after each call (Phase 0b's snapshot). The
instrument covers all three. *Closures* defined inside an event (`anon@N@…`)
are different: session 7 hooked them by name successfully, so they resolve
when the name is current.

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

`UiAProspectButton` is the prospect executor / button action.
`DefineProspectCombos` (969), `InventoryGrid` (1958), `InventoryGridHelperFuncs`
(1987) and `UiFuncs` (4459) are script-file *containers*, not routines — not
hookable, and not in the table.

*Object Create-event closures (regenerated SDK, 45 rows).* Every constant the
SDK defines whose value names the Create event of one of ten objects: the
window, its grid node's object, their parents and grandparent, the two sibling
node types, and the three other prospect objects.
`test_target_table_covers_every_sdk_closure_of_the_ui_objects` fails, naming
the missing constants, if a future regeneration adds one the table lacks.
The Stage A table's closure names were read from the pre-patch `data.win` and are stale on the current game: `anon@1038/2729/3551` (`UI_Prospect_obj`), `anon@320` (`Prospect_Cube_obj`), `anon@324`, `anon@1003`, `anon@1508` and `anon@1909` all printed `not found` in Phase 0a.

*UI_Prospect_obj (the window):*

| Probe label | Runtime name | SDK index |
|---|---|---|
| `UI_Prospect_obj anon@1065` (`m_SetInventoryLocalPlayer`, measured live) | `gml_Script_anon@1065@gml_Object_UI_Prospect_obj_Create_0` | 6067 |
| `UI_Prospect_obj anon@2806` (`m_Resize`, measured live) | `gml_Script_anon@2806@gml_Object_UI_Prospect_obj_Create_0` | 6068 |
| `UI_Prospect_obj anon@3657` (`m_UpdateInventoryGrid`, measured live) | `gml_Script_anon@3657@gml_Object_UI_Prospect_obj_Create_0` | 6069 |

*UI_Inventory_Grid_obj (the ProspectGrid node's object):*

| Probe label | Runtime name | SDK index |
|---|---|---|
| `UI_Inventory_Grid_obj anon@2143` | `gml_Script_anon@2143@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5627 |
| `UI_Inventory_Grid_obj anon@2971` | `gml_Script_anon@2971@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5628 |
| `UI_Inventory_Grid_obj anon@4064` | `gml_Script_anon@4064@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5629 |
| `UI_Inventory_Grid_obj ___struct___517@anon@8881` | `gml_Script____struct___517@anon@8881@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5630 |
| `UI_Inventory_Grid_obj ___struct___519@anon@8881` | `gml_Script____struct___519@anon@8881@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5631 |
| `UI_Inventory_Grid_obj anon@8881` | `gml_Script_anon@8881@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5632 |
| `UI_Inventory_Grid_obj ___struct___522@anon@15345` | `gml_Script____struct___522@anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5633 |
| `UI_Inventory_Grid_obj ___struct___529@anon@15345` | `gml_Script____struct___529@anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5634 |
| `UI_Inventory_Grid_obj ___struct___532@anon@15345` | `gml_Script____struct___532@anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5635 |
| `UI_Inventory_Grid_obj ___struct___536@anon@15345` | `gml_Script____struct___536@anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5636 |
| `UI_Inventory_Grid_obj ___struct___538@anon@15345` | `gml_Script____struct___538@anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5637 |
| `UI_Inventory_Grid_obj ___struct___541@anon@15345` | `gml_Script____struct___541@anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5638 |
| `UI_Inventory_Grid_obj anon@15345` | `gml_Script_anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5639 |
| `UI_Inventory_Grid_obj anon@34555` | `gml_Script_anon@34555@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5640 |
| `UI_Inventory_Grid_obj anon@36159` (`m_RefreshNode`, measured live) | `gml_Script_anon@36159@gml_Object_UI_Inventory_Grid_obj_Create_0` | 5641 |

*UI_Inventory_Parent_obj (the window's parent):*

| Probe label | Runtime name | SDK index |
|---|---|---|
| `UI_Inventory_Parent_obj anon@1621` | `gml_Script_anon@1621@gml_Object_UI_Inventory_Parent_obj_Create_0` | 5649 |
| `UI_Inventory_Parent_obj anon@4028` | `gml_Script_anon@4028@gml_Object_UI_Inventory_Parent_obj_Create_0` | 5650 |
| `UI_Inventory_Parent_obj anon@6164` | `gml_Script_anon@6164@gml_Object_UI_Inventory_Parent_obj_Create_0` | 5651 |
| `UI_Inventory_Parent_obj anon@6272` | `gml_Script_anon@6272@gml_Object_UI_Inventory_Parent_obj_Create_0` | 5652 |
| `UI_Inventory_Parent_obj anon@6754` | `gml_Script_anon@6754@gml_Object_UI_Inventory_Parent_obj_Create_0` | 5653 |
| `UI_Inventory_Parent_obj anon@7597` | `gml_Script_anon@7597@gml_Object_UI_Inventory_Parent_obj_Create_0` | 5654 |
| `UI_Inventory_Parent_obj anon@8615` | `gml_Script_anon@8615@gml_Object_UI_Inventory_Parent_obj_Create_0` | 5655 |
| `UI_Inventory_Parent_obj anon@11250` | `gml_Script_anon@11250@gml_Object_UI_Inventory_Parent_obj_Create_0` | 5656 |
| `UI_Inventory_Parent_obj anon@19615` | `gml_Script_anon@19615@gml_Object_UI_Inventory_Parent_obj_Create_0` | 5657 |
| `UI_Inventory_Parent_obj anon@22055` | `gml_Script_anon@22055@gml_Object_UI_Inventory_Parent_obj_Create_0` | 5658 |

*UI_Node_Parent_obj (every node's parent), UI_Grid_obj and UI_Container_obj (sibling node types):*

| Probe label | Runtime name | SDK index |
|---|---|---|
| `UI_Node_Parent_obj anon@1577` | `gml_Script_anon@1577@gml_Object_UI_Node_Parent_obj_Create_0` | 6008 |
| `UI_Node_Parent_obj anon@1997` | `gml_Script_anon@1997@gml_Object_UI_Node_Parent_obj_Create_0` | 6009 |
| `UI_Grid_obj anon@933` | `gml_Script_anon@933@gml_Object_UI_Grid_obj_Create_0` | 5569 |
| `UI_Grid_obj anon@1307` | `gml_Script_anon@1307@gml_Object_UI_Grid_obj_Create_0` | 5570 |
| `UI_Grid_obj anon@1577` | `gml_Script_anon@1577@gml_Object_UI_Grid_obj_Create_0` | 5571 |
| `UI_Grid_obj anon@2086` | `gml_Script_anon@2086@gml_Object_UI_Grid_obj_Create_0` | 5572 |
| `UI_Grid_obj anon@2244` | `gml_Script_anon@2244@gml_Object_UI_Grid_obj_Create_0` | 5573 |
| `UI_Grid_obj anon@2416` | `gml_Script_anon@2416@gml_Object_UI_Grid_obj_Create_0` | 5574 |
| `UI_Container_obj anon@197` | `gml_Script_anon@197@gml_Object_UI_Container_obj_Create_0` | 5525 |

`anon@1577` exists on both `UI_Node_Parent_obj` and `UI_Grid_obj`; the labels
carry the object name, so the two rows are distinct.

*UI_Parent_obj (the window's grandparent, where a generic resize would live) and the other prospect objects:*

| Probe label | Runtime name | SDK index |
|---|---|---|
| `UI_Parent_obj anon@255` | `gml_Script_anon@255@gml_Object_UI_Parent_obj_Create_0` | 6041 |
| `UI_Parent_obj anon@874` | `gml_Script_anon@874@gml_Object_UI_Parent_obj_Create_0` | 6042 |
| `UI_Parent_obj anon@2234` | `gml_Script_anon@2234@gml_Object_UI_Parent_obj_Create_0` | 6043 |
| `UI_Parent_obj anon@8601` | `gml_Script_anon@8601@gml_Object_UI_Parent_obj_Create_0` | 6044 |
| `UI_Parent_obj anon@10129` | `gml_Script_anon@10129@gml_Object_UI_Parent_obj_Create_0` | 6045 |
| `Prospect_Cube_obj anon@337` | `gml_Script_anon@337@gml_Object_Prospect_Cube_obj_Create_0` | 5198 |
| `UI_Button_Journal_Prospect_obj anon@342` | `gml_Script_anon@342@gml_Object_UI_Button_Journal_Prospect_obj_Create_0` | 5461 |
| `UI_Journal_Prospecting_obj anon@1039` | `gml_Script_anon@1039@gml_Object_UI_Journal_Prospecting_obj_Create_0` | 5690 |

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
| `___struct___411@UiContainerChange` | `gml_Script____struct___411@UiContainerChange@UiFuncs` (4480; missed by the Stage A search, added in Phase 0b) |

Phase 0a measured `UiSetGrid` carrying a row index and instance references
(navigation links), and `UiCreateNode` creating the ProspectGrid node with no
size among its arguments (§ Results R4).

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
Deliberately not in the table: the item-operation scripts in the same file
(`InventoryGridAddItemPos`, `InventoryGridRemoveItem`, `InventoryGridAddToStack`,
`InventoryGridCanAddToStack`, `InventoryGridHasSpaceMulti`,
`InventoryGridAddItemToTab`, `___struct___187@InventorySortTab@InventoryGrid`,
`ProcessInventoryGridInput` and its two structs) — they run when items move,
not when the grid is built; if R6 needs them each is one row.

*Control, and one input candidate:*

| Probe label | Runtime name | Why |
|---|---|---|
| `CheckPlayerInteraction` | `gml_Script_CheckPlayerInteraction` | **the control**: proven to fire from every interactable's Step event (`citrace nativetrace`); must count |
| `PlayerMouseAction` | `gml_Script_PlayerMouseAction` | a candidate, not a control: expected to fire on a click, but its one native measurement (pet quest research, 2026-09-11) read 0 |

Both already have `citrace nativetrace` rows; `prospectprobe` hooks them
again under its own ids so the control runs through the *same* installer as
the targets. 87 rows in all.

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
| **H1** | A named script or closure receives the input grid's dimensions as arguments when the window opens. | **Design A**: `HookOneScript` (both routes) on that row; when the call belongs to the prospect window, scale the dimension arguments before the trampoline. | (a) R4': a row whose logged args on a window open carry 9/6, with `self`/`other`/an argument identifying the prospect window or its node; (b) **positive control on the same row: `prospectprobe override <row> <argIndex> <value> 1` followed by a reopen draws a grid of the overridden size**, the applied line matching the R4' call with `@id` ignored (R5a = yes) — a row that carries the numbers but whose override changes nothing is recorded and does not count; (c) R6: items placed in the new cells are consumed by the prospect button; (d) C-hook: `CheckPlayerInteraction` counted > 0 in the same session. |
| **H2** | The dimensions live in variables the draw *and the cell store* follow at write time. | **Design B**: a frame-driven write. | **Ruled out for a bare write by R5b**: writing `nodeGridWidth` 9 → 18 on the open node crashed its Draw within one frame (`index out of bounds request 9 maximum size is 9`) — the draw followed the write, the store (`nodeGrid`) did not. Kept in the table with that verdict. |
| **H2'** | The dimensions live in variables (on the window or the node) that the game's own builder reads when it builds `nodeGrid`; writing them before the builder runs, or re-running the builder after the write, resizes the store. | **Design B'**: `HookOneScript` (both routes) on the R9 row; when the call belongs to the prospect window (the `self`/`other`/argument identification R9 recorded), write the scaled values into the recorded variables before the trampoline — or, if only `resize via` was positive, write + invoke the recorded method by name once per new window instance from a lazily installed hook on the R9 row's *post* side. | R9 identifies the builder's extent; R11 (`setat`) or R10 (`resize via`) = the drawn grid is the new size **and** an item dropped into a new cell is accepted (R5c); R6 = consumed; C-grid passing; C-hook passing. |
| **H3** | Fixed literals: no variable or argument governs the size. | **No one-value mod exists.** Blocked, with the numbers. | R2-window, R4' and R9 all measured with every fired row fully logged; `setat` and `resize via` both tried against the R9 row/method and negative, each with its control passing; a local Ghidra read, paraphrased, naming the literal. Never from an empty field. |
| **not observed** | Any control failed; a closure row printed `not found`; a row stayed `UNLOGGED`; an override or `setat` landed on a different call; the session ended before an experiment ran. | No hypothesis is concluded; Stage B does not start. | Record each field as `not observed (<which instrument, which control>)`. |

All three designs stay in the "change one value inside a call the game is
already making" class (`AGENTS.md`, "Don't Suspend the Game's Own Runtime"):
Design A rewrites two arguments inside the game's own sizing call, Design B'
writes two variables the game's builder is about to read. The one exception
is B' via `resize`, which invokes the game's own builder by name once per new
window — an extra call the game is not making at that instant. The human
accepted that class (workorder D8) on condition of a Known Limitations entry;
nothing pauses, re-creates or suspends the window.

**Follow-up if H3 or not observed:** read, locally in Ghidra, the R9 row's
body or `UI_Inventory_Grid_obj`'s Create event (found through
`citrace symdump` and `tools/ghidra/ImportSymbols.java`), to learn what sizes
the store. What is learned is paraphrased into § Results; nothing decompiled
enters this or any tracked file, and that read never closes the issue by
itself.

## Instrument

The sizing decision itself is `ForgePact::ProspectWindowMod`
(`plugin/include/ForgePact/ProspectWindowMod.hpp`): off returns the vanilla
size and applies nothing; on scales each axis by `kProspectColsFactor` /
`kProspectRowsFactor`, clamps to `kProspectMaxCols` / `kProspectMaxRows`, never
shrinks, applies once per window instance id (a latch of 64 ids), refuses a
vanilla size with an axis `<= 0`, and prints a stat line naming what it did.
The factors and caps are **placeholders** until Stage B sets them from R3 and
R8 (default decision: the inventory grid's size, capped at 2x vanilla per
axis). Nothing calls it yet, and no command exposes it.

### Hook-free instruments (existing, research build)

These come first in the live procedure, because they cost no hook and cannot
be blind in the way a detour can.

- `citrace dumpobj <Obj> [nth]` — every instance variable (name and value) of
  the nth live instance of a named object, method values resolved to
  `->method:<closure name>#<index>` (how Phase 0a read the live closure names).
  Control: a name it printed must read back through `oget`, a different route.
- `inames <Obj> [filter]`, `oget <Obj> <var>`, `oset <Obj> <var> <num>` (first
  instance only, no kind check — which is why `prospectprobe set` exists),
  `ojson <Obj> <var>`, `gnames [filter]`, `gjson <global>`,
  `cb <builtin> [args]`.
- `tools/ipc.ps1` sends one command and prints only its reply
  (`.\ForgePact\tools\ipc.ps1 "citrace dumpobj UI_Prospect_obj"`); `-Lines "a","b"`
  sends several lines that run back-to-back in the same poll.

### `prospectprobe` (research build only)

Not in `kPlayerCommands`; dispatched from `HandleProspectCommand`. Bare
`prospectprobe` prints the usage. Hook-free subcommands first.

- **`prospectprobe grid`** — hook-free read, Phase 0b. Finds the ProspectGrid
  node by what it is: the `UI_Inventory_Grid_obj` (object index resolved
  through the SDK name and `asset_get_index`) whose `uiNodeCallstack` names
  `"ProspectGrid"`. Prints one snapshot line,
  `grid=@<id> w=<nodeGridWidth> h=<nodeGridHeight> rows=<array_length(nodeGrid)|not-array> cols0=<array_length(nodeGrid[0])|-> cell=<nodeWidth>x<nodeHeight> bbox=<navBboxWidth>x<navBboxHeight> scale=<gridScale>`
  (a variable that does not exist prints `?`, and is never created), or
  `grid=none`. Then, for the live `UI_Prospect_obj` (instance 0), every
  variable whose numeric value is exactly 9 or 6 (**R2-window**) and every
  `m_*` variable with the closure it resolves to; then the node's `m_*`
  variables likewise. Read-only. Its control (**C-grid**) is `citrace dumpobj`
  on the same node printing the same `nodeGridWidth`/`nodeGridHeight`.
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
  An asset name that is not an object (a sprite, a sound) is refused as
  `unknown object` too (`object_exists`).
  **Write controls.** Phase 0a: `x` and `image_alpha` on the window and
  `gridScale` on the node all read back and changed nothing visible (C-write =
  no) — those are not fields the node draws from. The grid-node write route is
  nonetheless proven by R5b: a `nodeGridWidth` write changed what the node's
  next Draw did. That is why no `x` control is required for the node route;
  Phase 0b's by-eye control on it is `nodeWidth` (cell size) and then
  `navBboxWidth` (**C-write2**). **Never write `nodeGridWidth` or
  `nodeGridHeight` bare** — that is the R5b crash; use `resize … via`.
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
- **`prospectprobe arm [budget=N] [substr ...]`** — zeroes every counter and
  arms logging: the next `N` calls (default 6, at most 5000; anything else is
  refused and nothing is armed) of each *selected* row are written to
  `out.txt` as `prospectprobe <label> #n self=… other=… argc=… a0=… a1=…`.
  With no substrings every row except the `CheckPlayerInteraction` control is
  selected (the control fires every frame from every interactable; its count
  is the measurement — name it to log it); with substrings, only rows whose
  label contains one of them. Unselected rows still count. A `self` or `other`
  without a numeric `object_index` (a struct, as constructors and struct
  closures receive) is printed as `(not an instance: …)` and never handed to
  `object_get_name`.
- **`prospectprobe watch on|off`** — Phase 0b. While on, **and only for a call
  that is being logged** (the same budget and selection as its log line),
  the detour reads the grid snapshot before the game's function runs and
  appends ` grid-pre=<snapshot>` to the log line, then after it runs logs
  `prospectprobe <label> #n grid-post=<snapshot> same|CHANGED`. The snapshot
  calls builtins only, never nests inside another snapshot, and is never taken
  on the frame path. Off by default; `reset` turns it off. The first logged
  call whose `grid-post` says `CHANGED` brackets the builder (**R9**).
- **`prospectprobe show`** — per row: `calls` since `arm`, `since` the
  previous show, and, while armed, `logged=L` and — when the row made more
  calls than it logged — `UNLOGGED=K (budget spent - not observed)`; a row the
  last `arm` did not select says `UNLOGGED=<calls> (not selected - not observed)`
  if it fired and `(not selected for logging)` if it did not; `(not detoured)`
  for a row that did not install. The first line names a pending override
  with its `left` and `notApplied` counts, a pending `setat` with its
  `notApplied` count, and whether `watch` is on. The last line is the control,
  `CheckPlayerInteraction: calls=N`: **`0` voids every row above**, and a
  control that did not install says so.
- **`prospectprobe reset`** — zeroes counters, disarms, turns `watch` off and
  clears a pending `setat`.
- **`prospectprobe override <label> <argIndex> <number> [calls=1] [self=<Obj>] [other=<Obj>] [when=<number>]`**
  — for the next `calls` calls of an already-detoured row, if argument
  `argIndex` exists, is numeric, and the call matches every selector given
  (`self=` / `other=`: the object name of `self` / `other`, written as the
  name alone — `self=UI_Prospect_obj`, never with the `#object_index@id`
  suffix the log lines print — which a struct never matches; `when=`: argument
  `argIndex` currently equals that number), replace it before forwarding and log
  `override <label> #n a<i>: was=<v> now=<value> (left=…) self=… other=… argc=… a0=… …`
  — the call's own `self`, `other` and every argument, so the line can be
  checked against the R4' call. Refuses a label that is not a row, and a row
  that is not detoured (`hook it first`). A call whose argument is missing, not
  numeric, or that fails a selector is logged as `not applied (…)` with the
  reason (up to 6 lines), counted in `show`'s `notApplied`, and does not use
  up the count. `prospectprobe override clear` cancels. Labels may contain a
  space (`UI_Prospect_obj anon@2806`); the `key=value` selectors are taken off
  the end first, then everything before the trailing numbers is the label.
  This is H1's positive control.
- **`prospectprobe call window|grid <m_Method> [number ...]`** — Phase 0b.
  Invokes a method value stored on the live window (instance 0 of
  `UI_Prospect_obj`) or on the ProspectGrid node, with that instance as `self`
  and `other`, through the runtime's own `script_execute` reached by name
  (`CallBuiltinEx`). Refuses, with no call made: no such instance; the variable
  does not exist (`variable_instance_exists`); the value is not a method value.
  Prints the resolution (`->method:<name>#<index>` or `unresolvable`) before
  calling, a snapshot before, `st=<status> res=<value>`, and a snapshot after
  (`same|CHANGED`). Never reads the value's `CScriptRef` and never calls an
  address.
- **`prospectprobe resize <cols> <rows> via <m_Method>`** (or
  `via window:<m_Method>`, which invokes the window's method with the window as
  `self`) — Phase 0b, H2' experiment 2. **`via` is required**: a bare `resize`
  is refused with the R5b reason. In one handler, so no Draw can run in
  between: refuses (nothing written) when there is no ProspectGrid node, the
  method does not resolve as `call` would, or `nodeGridWidth`/`nodeGridHeight`
  are missing or not numbers; otherwise writes both, invokes the method, then
  reads `array_length(nodeGrid)` and `array_length(nodeGrid[0])`. If the call
  failed or the store does not now measure `<rows>` × `<cols>`, both variables
  are written back and it prints
  `reverted (builder did not resize nodeGrid: rows=… cols0=…)`; else
  `kept (nodeGrid now rows=… cols0=…)`. A snapshot after either way. A
  `reverted` is `not observed for that method`, never "the builder does not
  read them". A method with side effects beyond `nodeGrid` cannot be undone
  (workorder D7) — run it last, with the save backed up.
- **`prospectprobe setat <label> pre|post window|grid <var> <number> [self=<Obj>] [other=<Obj>] [arg<i>=<text>]`**
  — Phase 0b, H2' experiment 1: a one-shot write at a hook point. Refuses a
  label that is not a row and a row that is not detoured (`hook it first`);
  `when=` is refused (not a `setat` selector). On the next call of the row, in
  the given phase (`pre` = before the game's function runs, `post` = after),
  that matches every selector (`self=`/`other=` as `override`;
  `arg<i>=<text>`: argument `i` exists and its printed value contains `<text>`,
  case-insensitive — `arg4=ProspectGrid` singles out the `UiCreateNode` call
  that creates the node): resolves the target (`window` = instance 0 of
  `UI_Prospect_obj`, `grid` = the snapshot's node) and applies `set`'s checks
  (existing variable, finite number, read back). A call that fails a selector,
  or finds no target instance (`not applied (no grid instance)`), or a
  variable that is missing or not a number, is logged as `not applied (…)` (up
  to 6 lines), counted in `show`, and the write stays pending. Applied:
  `prospectprobe setat <label> #n pre|post <target>.<var>: was=… now=… (readback ok|MISMATCH) self=… other=… argc=… a0=…`,
  and for `post` a snapshot line. `prospectprobe setat clear` cancels. One
  pending `setat` at a time.

Counting is unconditional; logging is budgeted per row and per `arm` so a hot
row cannot drown `out.txt`, and `show` reports every call the budget hid, so a
spent budget is visible rather than read as silence. Every detour forwards to
the game's own function through the trampoline.

What a count through this route can and cannot prove: `CheckPlayerInteraction`
is the one row with a positive native measurement (`citrace nativetrace`,
and again in Phase 0a: 10200 → 14400 in two seconds). `PlayerMouseAction` is a
**candidate, not a control** — its only earlier native measurement read 0 (it
counted 1 in Phase 0a). No constructor (`s_*`) or `___struct___` row other than
`___struct___408/409/410` has counted through a native detour, so a zero on one
of those is uncontrolled: record it as `not observed`, never as "not called".
The closure rows' own positive control is L4: they must install.

## Live procedure

Phase 0b. One research build, one relaunch unless a crash forces a second.
Back up the save first (`%LOCALAPPDATA%\Hero_Siege`; Phase 0a's crash wrote no
save), junk items only. A GML runtime error at L9 is a recorded result, not a
failure of the procedure (workorder D7).

1. **L1.** `plugin_build\build.bat dev`; game closed; copy
   `plugin_build\BloodPactPlugin_rel.dll` over `<game>\mods\aurie\BloodPactPlugin.dll`.
   (Pressing **Install** in the panel afterwards restores the ship DLL.)
   Launch, load a character, `.\ForgePact\tools\ipc.ps1 ping`.
2. **L2 (hook-free enumeration).** `citrace dumpobj UI_Prospect_obj` with the
   window closed → `no live instances` (re-confirms R1-note). Walk to the
   Prospect Cube, open the window. `prospectprobe grid` → record the
   snapshot line (expect `w=9 h=6 rows=6 cols0=9`), **R2-window** (every
   `UI_Prospect_obj` variable equal to 9 or 6, by name), and every `m_*`
   method on the window and on the ProspectGrid node with its resolved
   closure. Control (**C-grid**): `citrace dumpobj UI_Inventory_Grid_obj <nth>`
   for the nth whose `uiNodeCallstack` is `"ProspectGrid"` prints the same
   `nodeGridWidth`/`nodeGridHeight`, and `inames UI_Inventory_Grid_obj` total
   equals `dumpobj`'s count. If `grid=none` while the window is open, stop:
   `not observed (snapshot resolver)`.
3. **L3 (write controls).** `prospectprobe set UI_Inventory_Grid_obj <nth> nodeWidth 46.4`
   → do the drawn cells shrink? Restore 92.8. If nothing changed,
   `navBboxWidth` to 417.6, observe, restore. Record **C-write2** =
   `nodeWidth: <changed|no change>; navBboxWidth: <…>`. Also repeat
   `set UI_Prospect_obj 0 x <x+60>` once and restore (expected: no change).
   Do **not** write `nodeGridWidth`/`nodeGridHeight` here (R5b).
4. **L4 (hooks + positive controls).** Close the window. `prospectprobe hook`
   → record `N detoured, M failed` and every `not found`/`refused` row.
   **Every `UI_Prospect_obj anon@…` and `UI_Inventory_Grid_obj …` row
   must print `detoured`** — closures resolve by name when the name is current
   (session 7), so a `not found` on one of them means the stale-name reading
   was wrong; stop and record `not observed (closure rows not resolvable:
   <rows>)`. Other objects' closure rows may fail; record them.
   `prospectprobe show` → `CheckPlayerInteraction: calls=` climbing (**C-hook**).
5. **L5 (the logged open with snapshots).** Stand next to the cube, window
   closed. `prospectprobe watch on`, `prospectprobe arm budget=500`, open the
   window at once, `prospectprobe show`. From `out.txt`: **R4'** = rows whose
   logged args carry 9 or 6 (now including closure rows), with indices and
   `self`/`other`; **R9** = the first logged call whose `grid-post` line says
   `CHANGED`, with its pre and post snapshots, plus every later `CHANGED` line
   (the builder extent and the sequence, e.g. `none → w=9 h=6 rows=0 → rows=6`).
   Re-arm on `UNLOGGED` rows exactly as before: close the window, re-arm with a
   larger budget restricted to those rows
   (`prospectprobe arm budget=<calls+50> <label substr> ...`), reopen, and
   `show` again, until every row that fired has had one pass with no
   `UNLOGGED`. (A row fully logged in an earlier pass and left out of a later
   filter prints `UNLOGGED=<calls> (not selected - not observed)` there; its
   earlier pass stands. A row that never had a clean pass does not; nor does a
   previously `UNLOGGED` row that did not refire on the re-arm pass — that is
   `not observed (did not refire)`.) A row still `UNLOGGED` is
   `not observed (budget spent)`. If no line says `CHANGED` while the snapshot
   went from `none` (L2 closed) to `9×6` (L2 open), R9 =
   `not observed (change outside every logged extent)` and the size is set by
   code no hooked row brackets.
6. **L6 (H1 experiment, only if R4' non-empty).** For each R4' row: close the
   window, `prospectprobe override <label> <argIndex> <vanilla×2> 1 when=<vanilla>`
   plus `self=<Obj>` (or `other=<Obj>`), where `<Obj>` is the object name
   alone that the R4' call's `self` (or `other`) printed —
   `self=UI_Prospect_obj`, not `self=UI_Prospect_obj#5220@100456`. Omit a
   selector only when that side printed `(not an instance: …)`. If L5 logged
   more than one call on a single open that these selectors would all accept,
   use that number instead of `1` and note it beside R5a. Reopen. Record both,
   always, for this row:
   - **The match verdict.** Compare the
     `prospectprobe override <label> #n a<i>: was=… now=…` line in `out.txt`
     with the R4' call logged in L5. It matches when `self` and `other` each
     name the same object with the same `#object_index` — **ignore `@id`**:
     a reopen re-runs Create (Phase 0a L6), so the window and its nodes are new
     instances and the right call always carries a new id; a
     `(not an instance: …)` matches `(not an instance: …)`; and every numeric
     argument other than `a<i>` is equal, except arguments that are instance
     ids or handles (a value equal to an `@id` printed in that same call, or
     one that differed between L5's opens), which are ignored like `@id`.
     No applied line at all (`show` still lists the override pending,
     `notApplied` counting) is a mismatch.
   - **The grid observation.** Is the drawn grid the overridden size — yes
     or no? Written down whatever the match verdict was.

   **R5a** for the row: match and yes = `yes`; match and no = `no`; mismatch
   = `not observed (override landed elsewhere; grid <changed|unchanged>)` —
   not "no". On yes, drop an item into a new cell (accepted / refused / error
   → **R5c**) and press the prospect button (**R6**).
   `prospectprobe override clear`.
7. **L7 (H2' experiment, write before the builder — only if R9 names a row
   and R2-window or the R9 pre-snapshot names the variables).** Close the
   window. If R2-window is non-empty and R9 = `UiCreateNode #k` with
   `a4="ProspectGrid"`:
   `prospectprobe setat UiCreateNode pre window <var> <2×> arg4=ProspectGrid`
   for each R2-window variable (one at a time — the instrument holds one
   pending `setat` — so width and height take two opens).
   If R9 is a closure row without size args: `setat <R9 label> pre window|grid <var> <2×> self=<Obj>`.
   Reopen. Record **R11** per variable: the applied line (self/other/args,
   matching R9's call under L6's `@id`-ignored rule), the `grid-post` snapshot
   (`rows`/`cols0` must equal the new size), the drawn grid by eye, and an
   item dropped into a new cell (accepted / refused / error → **R5c**). On
   accepted, press the button (**R6**), then close/reopen for **R7**
   (`returned to inventory` / `kept in grid` / `lost`). `prospectprobe setat clear`.
8. **L8 (fallback for R9 = UiCreateNode with empty R2-window).**
   Record R11 = `not applicable (no window variable carries the size)`; go
   to L9.
9. **L9 (H2' experiment, write then re-run the builder — last, D7).** With
   the window open and the grid empty:
   `prospectprobe resize 18 6 via m_RefreshNode` (the grid node's own method).
   If it prints `reverted`, try `prospectprobe resize 18 6 via <m_*>` for each
   other method L2 listed on the node, then
   `prospectprobe resize 18 6 via window:m_Resize` and
   `... via window:m_UpdateInventoryGrid`. Never a bare
   `set` of the size followed by a separate `call` — a Draw can run between
   two IPC polls, and that is the R5b crash. Record **R10** per method:
   `kept`/`reverted` with the snapshot, and on `kept` the drawn grid, an item
   in a new cell (R5c), the button (R6), close/reopen (R7). If a `kept`
   result is followed by a GML error on the next frame, that is recorded as
   `kept but draw failed: <crash.txt line>` — a result, and the relaunch is
   the second one this procedure allows.
10. **L10.** Fill § Results' Phase 0b column: R2-window, R4', R5a, R5c, R6,
    R7, R9, R10, R11, C-grid, C-write2, C-hook, H per § Deciding the
    hypothesis. Add `phase0: complete` to the workorder's log **only** if H
    reads H1 or H2'; otherwise report `BLOCKED` with the numbers.
11. **L11 (fallback, only if H = H3 / not observed).** `citrace symdump`,
    `tools/ghidra/ImportSymbols.java`, read locally the R9 row's body (or
    `UI_Inventory_Grid_obj`'s Create event via the OBJT event pointer) for what
    sizes the store; paraphrase into § Results under `Ghidra read
    (paraphrase)`; nothing decompiled in any tracked file. This informs a
    replan; it never closes the issue.

## Deciding the hypothesis

A row whose arguments carry the vanilla numbers is a candidate, not a result; only an override on that row that changes the drawn grid counts for H1.

A variable write that enlarges the drawn frame but not the cells the game accepts items into does not count for H2.

A snapshot that changes inside a call's extent names the builder's extent, not the builder; only a write the builder then follows counts for H2'.

Read § Hypotheses' required-evidence column as a conjunction: every item of a
row must be present, with its control passing, in the same session.

- **H1** needs R4' (a row carrying 9/6 on a window open, identifying the
  prospect window), R5a = yes on that same row with its applied override line
  matching the R4' call under L6's rule (`self` and `other` by object name and
  `#object_index` with `@id` ignored, the other numeric arguments equal except
  instance ids or handles), R5c = accepted, R6 = consumed, and C-hook > 0.
- **H2' needs all of:** R9 (a logged call whose `grid-post` says `CHANGED`,
  naming the builder's extent); R11 = applied on the R9 call with `rows`/`cols0`
  following **or** R10 = `kept`; R5c = accepted in a new cell; R6 = consumed;
  C-grid passing; C-hook > 0. A `CHANGED` snapshot alone is an extent, not a
  builder. A `reverted` on `resize via` is `not observed for that method`, never
  "the builder does not read them"; a `setat` that never applied, or applied to
  a call that does not match R9's under L6's rule, is
  `not observed (setat landed elsewhere)`.
- **H2** stays ruled out for a bare write by R5b; nothing in Phase 0b re-tries
  it.
- **A row whose calls during the open exceed its logged lines is
  `not observed (budget spent)`** — `show` prints it as `UNLOGGED=K`. Its
  arguments and snapshots were never seen, so it is neither in R4'/R9 nor
  evidence against them. A row whose calls on one open exceed the 5000 maximum
  budget stays `not observed (budget spent)` by design: the cap keeps
  `out.txt` readable, and no decision rule may read past it.
- **An override whose applied line does not match the R4' call** under L6's
  rule (a different object or `#object_index` on `self`/`other`, or a
  different non-id argument), or that never applied, is
  `not observed (override landed elsewhere; grid <changed|unchanged>)` — not
  R5a = no. The grid observation is recorded beside the verdict every time,
  so a mismatch that still changed the grid stays on the page as a lead for a
  re-run with tighter selectors. `@id` differing is never a mismatch.
- **H3** needs **R2-window, R4' and R9 all measured**, every row that fired
  during the L5 open **fully logged** (no `UNLOGGED` left), every R4' row's
  override applied to the R4' call, `setat` and `resize via` both tried against
  the R9 row and method, each with its control passing, none positive, and a
  local Ghidra read (paraphrased) naming the literal. An empty field, a row
  left `UNLOGGED`, a closure row `not found`, or an override or `setat` that
  landed elsewhere makes H **not observed**, never H3: a hypothesis that nothing
  governs the size cannot be concluded from candidates that were never seen or
  never tried.
- Anything else is **not observed**, naming the instrument and the control
  that failed. A zero from a detour without a climbing `CheckPlayerInteraction`,
  a `not found` on a regenerated closure row, or an empty enumeration without a
  passing C-grid read-back, is a statement about the instrument, not the game.

If R7 reads `kept in grid`, an enlarged grid would hold items in cells vanilla
does not have, and turning the mod off later could strand them. Stage B then
ships only with a Known Limitations entry, panel copy telling the player to
empty the grid first, and an explicit human acceptance — or does not ship.

## Results

Two sessions. **Phase 0a** (2026-09-17, research build from ForgePact
`cb77ad4`, Stage A instrument) is filled; every `not observed` in it is an
instrument failure — the stale SDK closure names — and never a negative.
**Phase 0b** reads `unknown` until its live session fills it.

| Field | Meaning | Phase 0a | Phase 0b |
|---|---|---|---|
| R1 | object + nth of the instance carrying the grid size | `UI_Inventory_Grid_obj` instance nth **5 of 6** while the window is open — the one whose `uiNodeCallstack` reads `"ProspectGrid"` (`gridName` `"Prospectron RX9000"`, `masterUi`/`parent` = the `UI_Prospect_obj` instance). `UI_Prospect_obj.prospectGrid` references it. | unknown |
| R1-note | window resident while closed? | window **not** resident while closed: `UI_Prospect_obj` has no live instance until opened; closing destroys it and its grid nodes (`UI_Inventory_Grid_obj` count drops to 1, the HUD `"PotionGrid"`). | unknown |
| R2 | size variables on R1 and their vanilla values | on the ProspectGrid node: `nodeGridWidth` = 9, `nodeGridHeight` = 6, `nodeGrid` = array[6] of arrays (rows × cols), `nodeWidth` = `nodeHeight` = 92.8, `navBboxWidth` = 835.2 (= 9 × 92.8), `navBboxHeight` = 556.8 (= 6 × 92.8), `gridBackground` = `Craft_Grid_Large_spr`, `gridScale` = 1. The window's own 100 variables were dumped but **not searched for 9/6** — Phase 0b step L2 does. | unknown |
| R2-window | `UI_Prospect_obj` variables equal to 9 or 6 (L2) | not run (added in Phase 0b) | unknown |
| R3 | vanilla input grid, columns × rows, by eye | 9 columns × 6 rows (by eye, and equal to R2). | unknown |
| R4 | detoured rows whose args carry R3 on a window open (Stage A table) | **not observed among detoured rows.** 41 rows detoured, 8 `not found st=14` (all eight object-Create closure rows: `UI_Prospect_obj anon@1038/2729/3551`, `Prospect_Cube_obj anon@320`, `UI_Button_Journal_Prospect_obj anon@324`, `UI_Journal_Prospecting_obj anon@1003`, `UI_Node_Parent_obj anon@1508/1909` — stale names). `arm budget=500`, one open, no row `UNLOGGED`. Fired: `UiCreate` 1 (`a0`=object `UI_Prospect_obj`, `a1`=1, `a2`=1), `UiCreateNode` 42 (#41 creates `UI_Inventory_Grid_obj` `"ProspectGrid"`: `a0`=0 `a1`=0 `a2`=object `UI_Inventory_Grid_obj` `a3`=script `UiAActivate` `a4`=`"ProspectGrid"` — no size), `UiSetGrid` 10 (`a0` = row index 0..5 / 0..3, `a1..a7` = instance refs, navigation links), `UiMoveNode` 131, `UiCreateContainer` 1, `UiSetRef` 2, `___struct___408/409/410` 1/42/1, `InventoryResetTabs` 1, `InventoryInitGrids` 1 (`a0`=true), `GetInventoryGridNode` 1, `UiResizeInventoryNodes` 2 (`a0`=2332 `a1`=1168.7), `PlayerMouseAction` 1. Zero: `UiAProspectButton` and its structs, `UiSetGridArray`, `UiResetGrid`, `UiContainerChange`, `UiChangeVisibility`, `UiSetNodeScale`, `UiRemoveNode`, `s_ItemGridInfo`, `s_InvNode`, `GridHasSpace`, `GridAddItem`, `ParseItemToGrid`, the rest. **No logged call carried 9 or 6 as a numeric argument.** | superseded by R4' |
| R4' | detoured rows (regenerated table) whose logged args carry 9/6 on a window open, with identification (L5) | not observed (closure rows not detoured) | unknown |
| R5a | override on each R4/R4' row changes the drawn grid (L6) | not run (R4 empty). | unknown |
| R5b | bare `set` of a size variable changes the drawn grid and accepts an item | `nodeGridWidth` 9 → 18 on the open ProspectGrid node (`set`, readback ok): **fatal GML error within one frame, before any drag** — `crash.txt`: "ERROR in action number 1 of Draw Event for object UI_Inventory_Grid_obj: index out of bounds request 9 maximum size is 9", trace `gml_Object_UI_Inventory_Grid_obj_Draw_64` line 124, last UI node ProspectGrid; WER APPCRASH c0000005. The draw iterates `nodeGridWidth` over `nodeGrid` rows that were sized at build time. Save files were last written before the write; no save corruption. `nodeGridHeight` not tried (same class of crash expected: the array has 6 rows). | not re-run (R5b is the reason for `resize … via`) |
| R5c | an item dropped into a new cell after a positive R5a/R10/R11: accepted / refused / error | not run | unknown |
| R6 | items in new cells consumed by the prospect button | not observed (no accepted cell). | unknown |
| R7 | items left in the grid on close: returned to inventory / kept in grid / lost | not observed. | unknown |
| R8 | inventory grid, columns × rows | inventory grid (`"InventoryGrid"`, `UI_Inventory_Grid_obj` nth 2): 15 columns × 6 rows (`nodeGridWidth` 15, `nodeGridHeight` 6, `nodeGrid` array[6]). | unknown |
| R9 | first logged call whose `grid-post` says `CHANGED`, with pre/post snapshots, and every later `CHANGED` (L5) | not run (no snapshot instrument) | unknown |
| R10 | `resize … via <method>`: `kept`/`reverted` per method, with snapshot (L9) | not run | unknown |
| R11 | `setat` before the R9 builder: applied line, `grid-post`, drawn grid (L7/L8) | not run | unknown |
| C | write, hook and enumeration controls | **C-write = no**: `set UI_Prospect_obj 0 x` 60/600/1500 and `image_alpha` 0.3 (readback ok / float MISMATCH) and `set UI_Inventory_Grid_obj 5 gridScale 0.5` all landed and read back, none changed anything visible. **C-hook passed** (`CheckPlayerInteraction` 10200 → 14400 in two seconds). **Enumeration control passed** (`oget UI_Inventory_Grid_obj nodeGridWidth` = 4 = nth 0's dumped value; `inames` total 124 = `dumpobj` count). **L6 = yes** (reopen: new instances, vanilla values). | C-hook (L4): unknown |
| C-grid | `prospectprobe grid` agrees with `citrace dumpobj` on the ProspectGrid node (L2) | not run | unknown |
| C-write2 | `nodeWidth` / `navBboxWidth` by-eye write control on the node (L3) | not run (R5b already proves the node route reaches the draw) | unknown |
| H | H1 / H2' / H3 / not observed | **not observed (instrument blind: stale SDK closure names).** The live window's method values named its Create closures `m_SetInventoryLocalPlayer` = `anon@1065`, `m_Resize` = `anon@2806`, `m_UpdateInventoryGrid` = `anon@3657` (all `@gml_Object_UI_Prospect_obj_Create_0`), and the grid node's `m_RefreshNode` = `anon@36159@gml_Object_UI_Inventory_Grid_obj_Create_0`; the stale SDK tables carried `anon@1038/2729/3551` and no `36159`. | unknown |
