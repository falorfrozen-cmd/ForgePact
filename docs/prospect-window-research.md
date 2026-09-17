# Bigger prospect window — research log

phase0-status: pending

Issue: [ForgePact #9](https://github.com/falorfrozen-cmd/ForgePact/issues/9),
"[QoL] Bigger prospect window" — *"Currently prospect window is way too small
for the amount of items players can hold in their inventory."* Opened
2026-09-15, label `enhancement`.

Status (2026-09-17): **research stage.** Phase 0a and Phase 0b both ran
2026-09-17 and both recorded H = not observed. Phase 0a was blind (stale SDK
closure names; SDK regenerated, hub `4539e68`). Phase 0b was not: it hooked
every regenerated closure, snapshotted the grid around every logged call and
ran both write experiments, and found that the node's cell store (`nodeGrid`)
is built inside `m_SetInventoryLocalPlayer` (`anon@1065`) and does not follow
`nodeGridWidth`/`nodeGridHeight` — a width written before that builder ran
crashed the game, and re-running every probed builder changed nothing
(§ Results, Phase 0b). A local Ghidra read (paraphrased in § Results) points at
the player's **profile inventory** data as what the store comes from; that is
a lead, **not established**. Phase 0c (instrument built, live session pending)
tests it read-only, and its answer is a decision gate for the human (§ Deciding
the hypothesis → Decision gate), because a profile-backed store would make
"bigger" a save-data change. Nothing player-visible exists. So the work is
staged:

- **Stage A (done):** a research instrument, `prospectprobe` (research build
  only), that native-detours every static-search candidate in one build; a
  game-independent sizing core
  (`plugin/include/ForgePact/ProspectWindowMod.hpp`) with its baseline/target
  harness (`tests/test_prospect_window_behavior.py`); this document; and
  contract tests (`tests/test_prospect_window_contract.py`) pinning all of it.
- **Phase 0a (live, done):** § Results, Phase 0a column.
- **Phase 0b (live, done):** the target table re-derived from the regenerated
  SDK (45 closure rows, enforced by a test), a grid snapshot around every
  logged call (`watch`), a hook-free read of the window and node (`grid`), and
  the two write experiments a positive result needs (`setat`, `resize … via`).
  § Live procedure; § Results, Phase 0b column.
- **Phase 0c (instrument done, live session pending):** four profile/inventory
  getter rows, and `prospectprobe backing on|off` / `dump` / `idcheck`, which
  keep what the game's *own* getter calls return and test whether `nodeGrid`
  is that storage — no getter is ever invoked by the instrument. Plus a
  controlled save test (R7) and the auto-prospect call-shape measurement
  (R12). § Phase 0c live procedure.
- **Stage B (only once the human has chosen a build-bearing branch of the
  decision gate):** the mod itself, a panel toggle, docs and release notes.

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

*Profile inventory getters (Phase 0c):*

| Probe label | Runtime name | SDK index |
|---|---|---|
| `GetProfileInventoryData` | `gml_Script_GetProfileInventoryData` | 1738 |
| `GetPlayerItemOwner` | `gml_Script_GetPlayerItemOwner` | 2030 |
| `GetInventoryArray` | `gml_Script_GetInventoryArray` | 1913 |
| `GetPlayerProfileObj` | `gml_Script_GetPlayerProfileObj` | 1723 |

`m_SetInventoryLocalPlayer` reads the first two before the ProspectGrid store
exists (Ghidra read, § Results). The other two are rows so the same capture
also sees character load, where the store may be sized. Plain script names, so
not moved by a game patch the way closure numbers are. **These rows are
detoured to observe the game's own calls and are never invoked by the
instrument**: a blind `callnum GetProfileInventoryData` in Phase 0b, without the
`self` the game passes, threw twice and then crashed the game.

*Control, and one input candidate:*

| Probe label | Runtime name | Why |
|---|---|---|
| `CheckPlayerInteraction` | `gml_Script_CheckPlayerInteraction` | **the control**: proven to fire from every interactable's Step event (`citrace nativetrace`); must count |
| `PlayerMouseAction` | `gml_Script_PlayerMouseAction` | a candidate, not a control: expected to fire on a click, but its one native measurement (pet quest research, 2026-09-11) read 0 |

Both already have `citrace nativetrace` rows; `prospectprobe` hooks them
again under its own ids so the control runs through the *same* installer as
the targets. 91 rows in all (87 in Phase 0b, plus the four getters).

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
| **H2''** | Phase 0c's branch if the store is a transient copy of profile data: `nodeGrid` is built from the array `GetProfileInventoryData` returns, so a larger returned array builds a larger store. | Not designed yet: a hook on `GetProfileInventoryData` that grows its **return array** before `m_SetInventoryLocalPlayer` consumes it — the same "one value inside a call the game is making" class as Design B', aimed at the getter's result. A fresh implement round, only after the human picks the "not save-backed" gate branch. | The decision gate reading **not save-backed** (idcheck `copy` with its control passing, and R7 = returned to inventory across a written save), then its own experiment. Never run in Phase 0c. |
| **H3** | Fixed literals: no variable or argument governs the size. | **No one-value mod exists.** Blocked, with the numbers. | R2-window, R4' and R9 all measured with every fired row fully logged; `setat` tried against the R9 row and applied to the R9 call, negative; `resize via` tried against the R9 method, grown (`resize 18 6`), and `reverted` with `invoked=yes` and the `self`/argument shape L5 logged for the game's own call of that row — a shrink's `reverted` never counts; a local Ghidra read, paraphrased, naming the literal. Never from an empty field. |
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
  call whose `grid-post` says `CHANGED` brackets the builder (**R9**). `same`
  and `CHANGED` are printed only when both reads resolved — a node
  (`@<id> …`) or a definite `none`; a failed or nested read on either side
  prints `UNREADABLE`, so two failed reads never look like "nothing changed".
  `none` on both sides prints `same (no node)`, because `none` means only that
  no node carrying `"ProspectGrid"` in `uiNodeCallstack` was found — a node
  that exists but has not set that variable yet reads `none` too — so it
  brackets nothing either.
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
- **`prospectprobe reset`** — zeroes counters, disarms, turns `watch` off,
  clears a pending `setat`, turns `backing` off and releases what it kept.
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
  (`CallBuiltinEx`). Refuses, with no call made: an `override` or `setat` is
  pending (it would fire on a matching call inside the invoke and rewrite what
  the method received while `args=` printed what was supplied — clear it
  first); no such instance; the variable does not exist
  (`variable_instance_exists`); the value is not a method value.
  Prints the resolution (`->method:<name>#<index>`, or `unresolvable` — an
  unresolvable method value is still invoked, and its outcome is
  `invoked=unproven`) before calling, a snapshot before,
  `st=<status> res=<value> invoked=… self=… args=(…)`, and a snapshot after
  (`same|CHANGED|UNREADABLE`). Never reads the value's `CScriptRef` and never
  calls an address.
  **`invoked=`** is what says whether the method's own body ran:
  `script_execute` returning success proves the dispatch, not the body. The
  resolved closure name is matched to its row in the probe table; if that row is
  detoured, its call count is read before and after the invoke —
  `invoked=yes (<label> +N)` or `invoked=NO (<label> +0)`. With no detoured
  row for the closure it prints `invoked=unproven (…)`, and the outcome proves
  nothing about the body. `self=` and `args=(none|…)` name what was supplied,
  so a call shape can be compared with the game's own call of that row (L5).
- **`prospectprobe resize <cols> <rows> via <m_Method> [number ...]`** (or
  `via window:<m_Method>`, which invokes the window's method with the window as
  `self`) — Phase 0b, H2' experiment 2. **`via` is required**: a bare `resize`
  is refused with the R5b reason. Trailing numbers are passed to the method as
  arguments, as `call` does. In one handler, so no Draw can run in between:
  refuses (nothing written) when an `override` or `setat` is pending (as
  `call` refuses), there is no ProspectGrid node, the method
  value is missing or not a method value (as `call` refuses), or
  `nodeGridWidth`/`nodeGridHeight` are missing or not numbers; an unresolvable
  method is invoked like `call` and reads `invoked=unproven`. Otherwise writes
  both, invokes the method, then measures `nodeGrid` over **every** row
  (`rows`, `cols0`, `cols=<min>..<max>`). Every outcome line carries
  `probe=shrink|grow|same|mixed` (the request against the size the node carried
  before the write) and `invoked=… self=… args=(…) st=…`:
  - `kept (nodeGrid now rows=… cols0=… cols=…) size=<w>x<h>` — the call
    succeeded and every row now measures `<cols>`, with `<rows>` rows. `size=`
    is `nodeGridWidth`x`nodeGridHeight` as read after the call (a method may
    write them itself); a width past the shortest row or a height past the row
    count adds `size exceeds store` — the R5b crash on the next Draw, so close
    the window before anything else;
  - `reverted (builder did not resize nodeGrid to <cols>x<rows>: …)` or
    `reverted (call failed; …)` — the size is written back, **capped per axis
    at what the store now covers** (width at the shortest row, height at the
    row count). When that cap is below the vanilla values, or `nodeGrid` is no
    longer an array, the line adds `restore unsafe: the store no longer covers
    the vanilla …` — close the window before anything else. (With no array
    no size is safe; the vanilla size is written back and the line says so.)
    A `probe=shrink` line ends by saying it never counts toward H3;
  - `rebuilt (the call replaced the ProspectGrid node; new node …, rows=… - matches|does not match the request …) size=…`
    — the method destroyed the node the write was on and a new one exists;
    nothing is restored, and the replacement's shape and `size=` (with
    `size exceeds store` as for `kept`) are the outcome;
  - `destroyed (… no new one exists …)` — the node is gone and nothing replaced it.

  A snapshot follows either way. A `reverted` is `not observed for that
  method`, never "the builder does not read them", and counts toward H3 only
  under § Deciding's rule — only on a grow: GML's element assignment grows an
  array and never truncates it, so an assignment builder answers a shrink with
  an unchanged store. A method with side effects beyond `nodeGrid` cannot
  be undone (workorder D7) — run it last, with the save backed up.
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

Phase 0c adds one capture mode and two reads. None of them invokes a game
script, by any route (no `callnum`, no `script_execute`, no `call`/`resize`):
every value the game computes about the store is taken from the game's **own**
call, inside the getter's detour, after the game's function returned. That is
the whole design, because a blind `callnum GetProfileInventoryData` (no correct
`self`) crashed the game in Phase 0b.

- **`prospectprobe backing on|off`** — off by default; `reset` turns it off.
  `on` releases anything kept before and starts over. While on, the detours on
  the four getter rows (`GetProfileInventoryData`, `GetPlayerItemOwner`,
  `GetInventoryArray`, `GetPlayerProfileObj`; they must be detoured —
  `prospectprobe hook` first, and `on` says how many of the four are) keep the
  value the call returned: a counted reference to the runtime's own array or
  struct, not a serialised copy. A call whose `self` is the `UI_Prospect_obj`
  window is held once kept (that is the call the ProspectGrid is built from);
  otherwise the latest call replaces it. The first 6 calls of each getter per
  `on` are logged as
  `prospectprobe backing <getter> #n self=… result=<array len=N len0=M|struct members=N|kind> kept|not kept … -> pp_backing_<getter>.json (<bytes> bytes)`,
  and the file (`bp_ipc\`) holds `{"getter","call","self","shape","value"}`
  with `value` from `json_stringify` (only an array or plain struct is
  stringified; anything else is `null`, its kind in `shape`). Builtins only,
  never nested, never on the frame path. `off` stops capturing and keeps what
  was kept for `dump` and `idcheck`.
- **`prospectprobe backing dump`** — read-only. Prints the live grid snapshot;
  the live `nodeGrid` (rows, the column count every row shares, row 0's cells)
  into `pp_backing_nodegrid.json`; and for each getter either
  `never captured (calls while backing was on: N)` or the kept call number,
  its `self` (and whether that was the window), its shape, and
  `pp_backing_<getter>_kept.json`. Then it walks each kept value (arrays and
  plain structs, depth ≤ 10, at most 200000 values) for a sub-array shaped like
  `nodeGrid` (`rows` arrays of `cols`) or a flat array of `rows × cols`, and
  prints each match's path with `cells agreeing with nodeGrid K/<rows×cols>`
  — the **structural** comparison. A walk that stopped early, hit the depth cap
  (a struct cycle lands there) or met an object that answered neither
  `is_method` nor `is_struct` says so; its "no sub-array" is `not observed`,
  not absent. Structural agreement is a lead, not identity.
- **`prospectprobe backing idcheck`** — the **reference-identity** probe, in
  one handler so no Draw runs in between:
  1. **Positive control first**, on arrays the instrument builds itself: a
     kept reference to a nested array must read back a sentinel written
     afterwards through the live array, the walk must find the sentinel
     through the kept reference, and it must *not* find it in a separately
     built array. Any of those failing prints
     `control: not observed (stash does not track live arrays on this runner …)`
     (or which half of the scanner failed) and nothing else is done — no write
     reaches the game.
  2. **Refusals, nothing written:** `GetProfileInventoryData` never captured;
     no ProspectGrid node; `nodeGrid` missing or not an array; **no empty
     cell** (empty = `undefined` or the number 0; the refusal lists the cells
     it saw) — an item is never written over.
  3. **The one write:** the sentinel `-7654321.25` into the first empty
     `nodeGrid[r][c]`, then a fresh read of the node's own `nodeGrid` to prove
     it landed (a write that did not land is `not observed`, never `copy`),
     the same cell of the first `nodeGrid`-shaped sub-array of the kept
     profile return read before and after, and a walk of every kept value for
     the sentinel.
  4. **Restore before any verdict:** the original cell value is written back
     and read back (`now=… (restored)`, or `NOT restored` — close the window
     without moving items and record it).
  5. **Verdict** (from the `GetProfileInventoryData` walk; the other getters
     are reported beside it): `reference-identical` (the sentinel was found in
     the kept return, with its path — `nodeGrid` shares its array with the
     profile data, so changing that storage changes the grid live);
     `copy` only when the walk was complete; otherwise
     `not observed (scan incomplete: …)`.

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
L4's `detoured` on a closure row proves only that the name resolved, not that
the detour sees the closure's calls. A count on a closure row is controlled once
a `call` or `resize via` on that object's method printed `invoked=yes` for that
row; until then a zero on it is `not observed`.

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
   **The four closures Phase 0a read off the live instances —
   `UI_Prospect_obj anon@1065`, `anon@2806`, `anon@3657` and
   `UI_Inventory_Grid_obj anon@36159` — must print `detoured`**: closures
   resolve by name when the name is current (session 7), so a `not found` on
   one of them means the stale-name reading was wrong; stop and record
   `not observed (closure rows not resolvable: <rows>)`. Every other closure
   row, including the nested `___struct___K@anon@N` rows (never measured
   resolving through this route), may fail; record them and go on.
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
   `not observed (budget spent)`. A `grid-post` line reading `UNREADABLE` is
   not a `same`: it brackets nothing. Nor does `same (no node)`: `none` on both
   sides also covers a node that exists but has not set its `uiNodeCallstack`
   yet. If no line says `CHANGED`: when at least
   one `grid-post=@…` line was logged during the open (the snapshot resolved the
   node inside a logged extent), R9 =
   `not observed (change outside every logged extent)`; when none was, R9 =
   `not observed (snapshot never resolved the node during the open)`. Neither
   says where the size is set.
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
   the window open and the grid empty. Methods, in order: the grid node's own
   `m_RefreshNode`, each other `m_*` method L2 listed on the node, then
   `window:m_Resize` and `window:m_UpdateInventoryGrid`. For each, pass the
   argument shape L5 logged when the game itself called that method's row
   (numbers only, appended after the method name; with no logged call, pass
   none and record the shape as unknown). If that logged call carried a
   non-numeric argument (a string, an instance, a struct), `resize` cannot
   supply it: R10 for that method is `not observed (call shape unknown)`, and
   its probes are still run and recorded beside that verdict. Before each
   `resize`, `prospectprobe arm budget=10 <the method's row label>` so the
   detour logs the `self` and arguments the method actually received; compare
   them with L5's call of that row (L6's `@id`-ignored rule) and record the
   verdict beside R10. **Shrink every method first**, in the order above:
   `prospectprobe resize 8 5 via m_RefreshNode`, then `resize 8 5 via <method>`
   for each of the others. A builder that ignores a shrink leaves a store the
   smaller size still fits. Then grow, in two passes. First every method whose
   shrink printed `kept`: `prospectprobe resize 9 6 via <method>` back to
   vanilla, then `prospectprobe resize 18 6 via <method>`. Then every method
   whose shrink printed `reverted` with `invoked=yes` and the store unchanged
   (`rows=6 cols0=9 cols=9..9`): `prospectprobe resize 18 6 via <method>`.
   A method whose shrink printed `reverted` with `invoked=yes` and an
   unchanged store is still grown: an assignment-built store grows and never
   truncates, so a shrink alone cannot show that the builder reads the size.
   D7 already accepts that this grow can crash the session. A method whose
   shrink printed `rebuilt`, `destroyed` or `restore unsafe`, or `reverted`
   with `invoked=NO`, `invoked=unproven` or a changed store, is not grown.
   If an invoke of a method whose closure is
   a row of the table (`m_RefreshNode`, `window:m_Resize`,
   `window:m_UpdateInventoryGrid`) prints
   `invoked=unproven (no detoured row for …)` while that row printed
   `detoured` at L4, the closure-name matcher failed on this runner, and
   nothing from `resize` counts this session: record every R10 probe as
   `not observed (invoked= matcher failed)`. Never a bare `set` of
   the size followed by a separate `call` — a Draw can run between two IPC
   polls, and that is the R5b crash. Record **R10** per method and per probe:
   `kept`/`reverted`/`rebuilt`/`destroyed`, the `probe=`, `invoked=` and
   `size=` values, `self=` and `args=` as printed, the received-call verdict,
   the snapshot, and on `kept` (or a `rebuilt` that matches the request) the
   drawn grid, an item in a new cell (R5c), the button (R6), close/reopen
   (R7). A line carrying `restore unsafe` or `size exceeds store`: close the
   window at once and record it. If a `kept` result is followed by a GML error
   on the next frame, that is recorded as
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

## Phase 0c live procedure

One research build, one launch (a second only if a crash forces it). **Back up
the save first** (`%LOCALAPPDATA%\Hero_Siege`), junk items only, no blind
invoke of any getter. The Phase 0b procedure above is kept as the record of
what Phase 0b ran.

1. **C1.** `plugin_build\build.bat dev`; game closed; copy
   `plugin_build\BloodPactPlugin_rel.dll` over
   `<game>\mods\aurie\BloodPactPlugin.dll` (Install in the panel restores the
   ship DLL). Launch; **before loading a character**, `prospectprobe hook`,
   `prospectprobe backing on`, `prospectprobe arm budget=6 GetInventoryArray GetPlayerProfileObj GetProfileInventoryData GetPlayerItemOwner`.
   Load a character. Record whether any getter fired at load and its captured
   shape (`bp_ipc\pp_backing_*.json`) — this is the "where the store is sized"
   read.
2. **C2 (open + capture).** Walk to the Prospect Cube, open the window.
   `prospectprobe grid` → confirm `w=9 h=6 rows=6 cols0=9` (**C-grid** control:
   `citrace dumpobj` agrees). `prospectprobe backing dump` → record the
   captured `GetProfileInventoryData` / `GetPlayerItemOwner` shapes and the
   live `nodeGrid` shape, and note whether `nodeGrid`'s 6×9 appears as a
   sub-structure of the profile return (**structural** identity). If no getter
   was captured, stop: `not observed (backing never captured — getters not
   called on this open)`.
   **C2b (auto-prospect measurement — for the auto-prospect alternative under
   § Decision gate).** With the window open and `prospectprobe watch on`,
   `prospectprobe arm budget=20 UiAProspectButton anon@15345 anon@8881`
   (`arm` filters by label substring; the grid closures' labels are
   `UI_Inventory_Grid_obj anon@15345` = `m_MoveItemToGrid` and
   `UI_Inventory_Grid_obj anon@8881` = `m_DropItem`, so the `anon@N` substrings
   are what select them). The tester **places one item into the
   prospect grid, then presses Prospect.** Record: the `self`/`other`/args and
   `object_index` the insert closure and `UiAProspectButton` were called with
   (so the shipped design can invoke the button's own handler with the measured
   shape, no blind invoke), which insert closure actually fired for a
   drag-in vs a click-in, and the `grid-post` snapshots. Then **verify the
   leftover-material claims live** (record as claims, not facts): after
   one prospect, does the result material sit in the grid; can a second item
   still be inserted and prospected; is the material itself ever taken as
   prospect input or does it block the next insert. **R12** = the button call
   shape + the insert closure + the leftover-material observations.
3. **C3 (reference identity, with its control).** `prospectprobe backing idcheck`.
   Record: the **positive control** verdict first (stash tracks a live array —
   if it fails, the whole probe is `not observed`), then the test verdict
   (`reference-identical` / `copy`), both cell values, and confirm the chosen
   cell was empty and was restored (re-run `prospectprobe grid` / `dumpobj` to
   confirm no item moved). This is the one bounded write Phase 0c makes.
4. **C4 (R7, controlled save).** Place a junk item (e.g. a spare ore/ring) in
   the prospect grid. Note the `.hss` mtime. Close the window; **cause the game
   to write a save** (a zone change / save point / return to menu — whatever
   writes `%LOCALAPPDATA%\Hero_Siege\*.hss`); confirm the `.hss` mtime
   **advanced** after the item was placed. Only then relaunch. Reopen and read
   where the item is: **R7** = `returned to inventory` / `kept in the prospect
   grid` / `lost`, with both mtimes. (The Phase 0b Molten Ring was void because
   the save predated placing it — do not repeat that; the mtime check is the
   gate.)
5. **C5 (decision-gate inputs).** From C2/C3/C4 state which gate branch holds
   (§ Deciding the hypothesis → Decision gate): **save-backed** (idcheck
   reference-identical, or structural match, or R7 kept across a written save),
   **not save-backed** (idcheck copy and R7 returned), or **inconclusive**.
6. **C6.** Fill § Results' **Phase 0c** column (R7, R12 button-call-shape +
   leftover-material claims, backing structural, backing idcheck + its
   control, the load-time capture, the gate branch, H). **Do not** change
   `phase0-status: pending` — that waits on the human's gate decision.
7. **C7 (fallback, only if inconclusive).** A local Ghidra read of the
   profile-storage *construction* path (found via `citrace symdump` +
   `tools/ghidra/ImportSymbols.java`), paraphrased into § Results; nothing
   decompiled in any tracked file. Informs a further replan; never closes #9.
8. **C8.** Report the gate branch and the R7 fate; the human's decision on the
   gate is recorded before any Stage B build.

## Deciding the hypothesis

A row whose arguments carry the vanilla numbers is a candidate, not a result; only an override on that row that changes the drawn grid counts for H1.

A variable write that enlarges the drawn frame but not the cells the game accepts items into does not count for H2.

A snapshot that changes inside a call's extent names the builder's extent, not the builder; only a write the builder then follows counts for H2'.

A `reverted` from `resize via` counts toward H3 only when it printed `invoked=yes` and supplied the `self` and argument shape L5 logged when the game itself called that row; if L5 never logged the game calling it, it is `not observed (call shape unknown)`.

Only a grow's `reverted` from `resize via` counts toward H3; a shrink's `reverted` is `not observed (shrink only — an assignment-built store never truncates)`.

Read § Hypotheses' required-evidence column as a conjunction: every item of a
row must be present, with its control passing, in the same session.

- **H1** needs R4' (a row carrying 9/6 on a window open, identifying the
  prospect window), R5a = yes on that same row with its applied override line
  matching the R4' call under L6's rule (`self` and `other` by object name and
  `#object_index` with `@id` ignored, the other numeric arguments equal except
  instance ids or handles), R5c = accepted, R6 = consumed, and C-hook > 0.
- **H2' needs all of:** R9 (a logged call whose `grid-post` says `CHANGED`,
  naming the builder's extent); R11 = applied on the R9 call with `rows`/`cols0`
  following **or** R10 = `kept` (with `invoked=yes`; a `kept` with
  `invoked=unproven` still needs the drawn grid and R5c, and is written down
  as unproven); R5c = accepted in a new cell; R6 = consumed;
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
  override applied to the R4' call, `setat` applied to the R9 call and
  negative, `resize via` on the R9 method grown (`resize 18 6`) and `reverted`
  with `invoked=yes` and the call shape L5 logged for it (the rules above — a
  shrink's `reverted` never counts), none positive, and a local Ghidra read
  (paraphrased) naming the literal. An empty field, a row left `UNLOGGED`, a
  closure row `not found`, an override or `setat` that landed elsewhere, or a
  `resize via` whose `invoked=` is `NO` or `unproven`, whose call shape is
  unknown, or whose only `reverted` was a shrink makes H **not observed**,
  never H3: a hypothesis that nothing
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

### Decision gate (Phase 0c → Stage B)

Phase 0b's lead is that the ProspectGrid store is, or is copied from, the
player's profile inventory data. If it is the storage itself, "bigger" is a
**save-data** change — a different risk class from "one value inside a call
the game is making". Phase 0c writes the gate's inputs into § Results and
names which branch holds; **the human picks the branch** before any Stage B
build. Unanswered, nothing is built.

- **Save-backed** — `backing idcheck` = `reference-identical` (with its control
  passing), **or** `backing dump` shows `nodeGrid` mirrors a profile sub-array,
  **or** R7 = kept in the prospect grid across a *written* save. Then enlarging
  the grid changes save-data shape, and the risks are:
  - **Stranded items.** An enlarged grid holds items in cells vanilla does not
    have; turning the mod off, or opening the save in a vanilla or online
    client, leaves those items stranded in cells nothing else can address.
  - **Save compatibility.** The game ships `ValidateInventory`,
    `DetectInventoryModifications` and `DetectInventoryDuplicates`
    (`hs-game-sdk` script names) — an anti-tamper surface. A save with a
    non-vanilla prospect store may be rejected, "repaired", or flagged. Not
    measured; a risk to state, not to wave away.
  - **The save editors cannot see it.** HSSaveEditor and
    hero-siege-item-editor model inventory and stash tabs but carry no
    "prospect" anywhere (§ Static search, Negative results), so an item parked
    in an enlarged prospect store could not be recovered through them.

  The human's alternatives to a save-shape change: (a) a **prospect all**
  batching helper — feed inventory ore through the vanilla 9×6 grid in
  successive automated fills, so a full inventory takes fewer *manual* trips
  (its own feasibility unproven; a separate research round); (b)
  **auto-prospect on insert**, below; (c) close #9 as not feasible, with this
  finding recorded so it is not re-investigated.
- **Not save-backed** — `idcheck` = `copy` (control passing, walk complete)
  **and** R7 = returned to inventory. The store is a transient copy of the
  profile data, and the next experiment (not run in Phase 0c) is **H2''**:
  grow the `GetProfileInventoryData` return array inside its detour, before
  `m_SetInventoryLocalPlayer` consumes it, and see whether `nodeGrid` is then
  built larger. The human approves that as the Stage-B-bound design or asks
  for more measurement.
- **Inconclusive** — a control failed, a getter was never captured, `idcheck`
  refused, or its walk was incomplete. H stays `not observed`; the fallback is a
  local Ghidra read of the profile storage's *construction* path (paraphrase
  only, C7), which informs a further replan and never closes the issue.

**Auto-prospect on insert** (the human's last-resort design; independent of
what backs the store). Instead of a *bigger* grid, make one insert do more:
each time an item is moved into the prospect grid, run the game's own
prospect operation at once, so the grid clears after every item and 9×6 is
never the limit. It changes what one insert does, not save-data shape, so it
sidesteps the save-backed risks on either branch. Shape, subject to R12 and the
human's approval: a lazily installed `HookOneScript` (both routes) on the insert
closure R12 confirms (`m_MoveItemToGrid` `anon@15345` or `m_DropItem`
`anon@8881`); when the item landed in the **prospect** grid (identified by the
node's `uiNodeCallstack` naming `"ProspectGrid"` / its `object_index`, as
measured — never by a variable's mere presence), invoke `UiAProspectButton`'s
own handler once, with the `self` and arguments R12 measured when the tester
pressed Prospect. No blind invoke. Two statements about it are **claims to
verify** at C2b, not facts: that the prospected materials left in the grid
cannot themselves be prospected, and that they do not block the next insert.
If a leftover material blocks the next insert or is taken as input, the design
needs a clear-or-relocate step, and that is a new finding. The risk the human
weighs: prospecting stops being a batched, click-once action, and the mod fires
the game's own operation at a moment the game was not calling it (the same
class the human accepted for the pet quest collector). Off by default; a
toggle.

## Results

Three sessions. **Phase 0a** (2026-09-17, research build from ForgePact
`cb77ad4`, Stage A instrument) is filled; every `not observed` in it is an
instrument failure — the stale SDK closure names — and never a negative.
**Phase 0b** (2026-09-17, build `ef8d54f`) is filled; its `not observed` fields name their reason. **Phase 0c** reads `unknown` until its live session (§ Phase 0c live procedure) fills it.

| Field | Meaning | Phase 0a | Phase 0b | Phase 0c |
|---|---|---|---|---|
| R1 | object + nth of the instance carrying the grid size | `UI_Inventory_Grid_obj` instance nth **5 of 6** while the window is open — the one whose `uiNodeCallstack` reads `"ProspectGrid"` (`gridName` `"Prospectron RX9000"`, `masterUi`/`parent` = the `UI_Prospect_obj` instance). `UI_Prospect_obj.prospectGrid` references it. | unchanged: the `UI_Inventory_Grid_obj` whose `uiNodeCallstack` reads `"ProspectGrid"` (nth 5, `@265938`; `@261558` after the relaunch), found by `prospectprobe grid`'s resolver rather than by nth. | unknown |
| R1-note | window resident while closed? | window **not** resident while closed: `UI_Prospect_obj` has no live instance until opened; closing destroys it and its grid nodes (`UI_Inventory_Grid_obj` count drops to 1, the HUD `"PotionGrid"`). | unchanged: `citrace dumpobj UI_Prospect_obj` with the window closed → no live instances (L2). | unknown |
| R2 | size variables on R1 and their vanilla values | on the ProspectGrid node: `nodeGridWidth` = 9, `nodeGridHeight` = 6, `nodeGrid` = array[6] of arrays (rows × cols), `nodeWidth` = `nodeHeight` = 92.8, `navBboxWidth` = 835.2 (= 9 × 92.8), `navBboxHeight` = 556.8 (= 6 × 92.8), `gridBackground` = `Craft_Grid_Large_spr`, `gridScale` = 1. The window's own 100 variables were dumped but **not searched for 9/6** — Phase 0b step L2 does. | unchanged: `prospectprobe grid` = `w=9 h=6 rows=6 cols0=9 cell=92.8x92.8 bbox=835.2x556.8 scale=1`. **`nodeGrid` is not built from `nodeGridWidth`/`nodeGridHeight`** (R9, R10, R11); the Ghidra read below points at the player's profile inventory data instead — a lead, not established. | unknown |
| R2-window | `UI_Prospect_obj` variables equal to 9 or 6 (L2) | not run (added in Phase 0b) | **empty** — the live `UI_Prospect_obj` has 100 variables, **0** equal to 9 or 6 (and 15 `m_*` methods). The window carries no variable the size could be written into before the node is built. | unknown |
| R3 | vanilla input grid, columns × rows, by eye | 9 columns × 6 rows (by eye, and equal to R2). | 9 columns × 6 rows (unchanged). | unknown |
| R4 | detoured rows whose args carry R3 on a window open (Stage A table) | **not observed among detoured rows.** 41 rows detoured, 8 `not found st=14` (all eight object-Create closure rows: `UI_Prospect_obj anon@1038/2729/3551`, `Prospect_Cube_obj anon@320`, `UI_Button_Journal_Prospect_obj anon@324`, `UI_Journal_Prospecting_obj anon@1003`, `UI_Node_Parent_obj anon@1508/1909` — stale names). `arm budget=500`, one open, no row `UNLOGGED`. Fired: `UiCreate` 1 (`a0`=object `UI_Prospect_obj`, `a1`=1, `a2`=1), `UiCreateNode` 42 (#41 creates `UI_Inventory_Grid_obj` `"ProspectGrid"`: `a0`=0 `a1`=0 `a2`=object `UI_Inventory_Grid_obj` `a3`=script `UiAActivate` `a4`=`"ProspectGrid"` — no size), `UiSetGrid` 10 (`a0` = row index 0..5 / 0..3, `a1..a7` = instance refs, navigation links), `UiMoveNode` 131, `UiCreateContainer` 1, `UiSetRef` 2, `___struct___408/409/410` 1/42/1, `InventoryResetTabs` 1, `InventoryInitGrids` 1 (`a0`=true), `GetInventoryGridNode` 1, `UiResizeInventoryNodes` 2 (`a0`=2332 `a1`=1168.7), `PlayerMouseAction` 1. Zero: `UiAProspectButton` and its structs, `UiSetGridArray`, `UiResetGrid`, `UiContainerChange`, `UiChangeVisibility`, `UiSetNodeScale`, `UiRemoveNode`, `s_ItemGridInfo`, `s_InvNode`, `GridHasSpace`, `GridAddItem`, `ParseItemToGrid`, the rest. **No logged call carried 9 or 6 as a numeric argument.** | superseded by R4' | superseded by R4' |
| R4' | detoured rows (regenerated table) whose logged args carry 9/6 on a window open, with identification (L5) | not observed (closure rows not detoured) | **not observed among detoured rows** — no logged call in either L5 pass (budgets 500 and 5000) carried a real 9 or 6 as an argument. `UiCreateNode #41` creates the node with `a4="ProspectGrid"` and no size. | unknown |
| R5a | override on each R4/R4' row changes the drawn grid (L6) | not run (R4 empty). | not run (R4' empty). | unknown |
| R5b | bare `set` of a size variable changes the drawn grid and accepts an item | `nodeGridWidth` 9 → 18 on the open ProspectGrid node (`set`, readback ok): **fatal GML error within one frame, before any drag** — `crash.txt`: "ERROR in action number 1 of Draw Event for object UI_Inventory_Grid_obj: index out of bounds request 9 maximum size is 9", trace `gml_Object_UI_Inventory_Grid_obj_Draw_64` line 124, last UI node ProspectGrid; WER APPCRASH c0000005. The draw iterates `nodeGridWidth` over `nodeGrid` rows that were sized at build time. Save files were last written before the write; no save corruption. `nodeGridHeight` not tried (same class of crash expected: the array has 6 rows). | not re-run (R5b is the reason for `resize … via`) | not re-run |
| R5c | an item dropped into a new cell after a positive R5a/R10/R11: accepted / refused / error | not run | not run (no accepted enlarged cell). | unknown |
| R6 | items in new cells consumed by the prospect button | not observed (no accepted cell). | not observed (no accepted enlarged cell). | unknown |
| R7 | items left in the grid on close: returned to inventory / kept in grid / lost | not observed. | **not observed (void attempt)** — a junk Molten Ring placed in the grid was back in the inventory after a crash and relaunch, but the last save (`herosiege13.hss` 16:40:00) predated placing it (crash 16:56:43), so that shows only that the crash lost an unsaved move, nothing about the grid. Phase 0c's C4 measures R7 across a written save. | unknown |
| R8 | inventory grid, columns × rows | inventory grid (`"InventoryGrid"`, `UI_Inventory_Grid_obj` nth 2): 15 columns × 6 rows (`nodeGridWidth` 15, `nodeGridHeight` 6, `nodeGrid` array[6]). | 15 columns × 6 rows (unchanged). | unknown |
| R9 | first logged call whose `grid-post` says `CHANGED`, with pre/post snapshots, and every later `CHANGED` (L5) | not run (no snapshot instrument) | the open is bracketed by `Prospect_Cube_obj anon@337`; inside `UiCreate(UI_Prospect_obj,1,1)`: `UiCreateNode #41` creates the node `w=0 h=0 rows=not-array`; **`nodeGridWidth`/`Height` become 9/6 before `UiCreateNode #42`, with no hooked call bracketing that write** (the window Create event's own body, which is not in the script table); **`nodeGrid` becomes a 6×9 array inside `m_SetInventoryLocalPlayer` (`anon@1065`) with no logged sub-call bracketing it**; `m_RefreshNode` (`anon@36159`) only sets `navBbox`. `anon@2143`/`anon@255` stayed `UNLOGGED` even at budget 5000 (they fire more than 17k times per open cycle) → `not observed (budget spent)` by design. | unknown |
| R10 | `resize … via <method> [args]`: per method and probe (every shrink, then grow every method whose shrink `kept` or `reverted` with `invoked=yes` and an unchanged store; only a grow's `reverted` counts toward H3), `kept`/`reverted`/`rebuilt`/`destroyed`, the `probe=`, `invoked=` and `size=` values, `self=` and the args supplied, the self/args the detour logged as received, the snapshot, and any `restore unsafe` / `size exceeds store` (L9) | not run | every probed method — `m_RefreshNode`, `m_SetPosition`, `m_MouseInGrid`, `m_MouseInAnyGrid`, `window:m_Resize`, `window:m_UpdateInventoryGrid`, and (added) `window:m_SetInventoryLocalPlayer` — shrink (8×5) and grow (18×6) both **`reverted` with `invoked=yes`**, args none (argc=0, matching L5's game calls), store unchanged 6×9; only a grow's `reverted` counts toward H3, and every grow reverted. `m_RefreshNode`/`m_SetPosition`/`m_Resize`/`m_SetInventoryLocalPlayer` move only `navBbox`. Not probed (deviation: item-moving/buying methods, no known args): `m_MoveItemToGrid`, `m_StartInvDragging`, `m_DropItem`, `m_BuyItemConfirmed` → not observed. **No probed method resizes `nodeGrid` from `nodeGridWidth`/`Height`.** | unknown |
| R11 | `setat` before the R9 builder: applied line, `grid-post`, drawn grid (L7/L8) | not run | `setat UI_Prospect_obj anon@1065 pre grid nodeGridWidth 18` **applied on the matching call** (`was=9 now=18`, self/other/argc matching L5) → game crash: `Step Event0` of `UI_Inventory_Grid_obj` line 75, `index out of bounds request 15 maximum size is 9`. The store built inside `anon@1065`'s extent **did not follow** the width written at its entry. Height not tried (the crash ended that launch). | unknown |
| C | write, hook and enumeration controls | **C-write = no**: `set UI_Prospect_obj 0 x` 60/600/1500 and `image_alpha` 0.3 (readback ok / float MISMATCH) and `set UI_Inventory_Grid_obj 5 gridScale 0.5` all landed and read back, none changed anything visible. **C-hook passed** (`CheckPlayerInteraction` 10200 → 14400 in two seconds). **Enumeration control passed** (`oget UI_Inventory_Grid_obj nodeGridWidth` = 4 = nth 0's dumped value; `inames` total 124 = `dumpobj` count). **L6 = yes** (reopen: new instances, vanilla values). | C-hook (L4): **passed** — `CheckPlayerInteraction` 1500 → 5700, then 1800 → 6000 on the relaunch; `hook` 87 detoured / 0 failed, including the four live-read closures `UI_Prospect_obj anon@1065/2806/3657` and `UI_Inventory_Grid_obj anon@36159`. Phase 0a's stale-name blindness is closed. | unknown |
| C-grid | `prospectprobe grid` agrees with `citrace dumpobj` on the ProspectGrid node (L2) | not run | **passed** — `prospectprobe grid` = `w=9 h=6 rows=6 cols0=9`; `citrace dumpobj` of the ProspectGrid node agreed; `inames` total 124 = `dumpobj` count. | unknown |
| C-write2 | `nodeWidth` / `navBboxWidth` by-eye write control on the node (L3) | not run (R5b already proves the node route reaches the draw) | **`nodeWidth` changed live** (46.4: cells drawn half-width, same frame; restored 92.8). The background sprite `Craft_Grid_Large_spr` did **not** move (see Background sprite). Window `x` write: no visible change (as Phase 0a). | unknown |
| Background sprite | `gridBackground` of the ProspectGrid node, and whether it follows the grid | `Craft_Grid_Large_spr` (R2) | `Craft_Grid_Large_spr` is a **fixed 9×6 image**: with cells drawn half-width (C-write2) it did not change, so a larger grid would draw cells beyond it and needs its own background handling (a Stage B note). | unknown |
| Ghidra read (paraphrase) | local read of the open path on the current exe, paraphrased; nothing decompiled is in this repository | not run | `m_SetInventoryLocalPlayer` (`anon@1065`) begins by fetching the player's **profile inventory** data (`GetProfileInventoryData`) and the item owner (`GetPlayerItemOwner`), passing its own self/other and a profile reference, then wires the grid nodes from what those return; both getters take the window instance as their self. Resolving every numeric constant referenced by `anon@1065`, `InventoryInitGrids`, `GetProfileInventoryData`, `GetPlayerItemOwner`, `m_UpdateInventoryGrid`, `m_RefreshNode` and the cube's closure found no 9 or 6 (`m_Resize` references 18 once; layout, unverified). The window's Create event, where `nodeGridWidth`/`Height` are set, is not in the script table and was not read. **Reading, not live-confirmed:** `nodeGrid` is, or is copied from, per-profile inventory storage whose 9×6 shape is fixed where that data is created — consistent with R11 and R10. If it is the storage itself, "bigger" is a save-data change (§ Decision gate). Phase 0c tests it. | unknown |
| Identity attempt (`callnum`) | calling a getter directly to compare its result with `nodeGrid` | not run | **instrument misuse, not a result**: `callnum GetProfileInventoryData` with no arguments, `0` and `1` — without the window as self, which the getter needs — threw twice and then crashed the game (`Controller_obj` Step: `array_get :: Index [-1] out of range [1]` in `EnemyStepHandleNew`). It establishes nothing about the store. Phase 0c never invokes a getter; it keeps what the game's own call returned (`prospectprobe backing`). | unknown |
| R12 | auto-prospect: `UiAProspectButton` call shape (self/other/args/`object_index`), which insert closure fired (drag-in vs click-in), `grid-post` snapshots, and the leftover-material claims checked live (C2b) | not run | not run | unknown |
| load-time capture | getters that fired at character load, with their kept shapes and `pp_backing_*.json` (C1) | not run | not run (no `backing` instrument) | unknown |
| backing structural | `backing dump`: kept `GetProfileInventoryData` / `GetPlayerItemOwner` shapes, live `nodeGrid` shape, and any `nodeGrid`-shaped sub-array with its agreeing-cell count and walk completeness (C2) | not run | not run | unknown |
| backing idcheck | `backing idcheck`: the control verdict first, then `reference-identical` / `copy` / `not observed`, the cell, both values, and `restored` (C3) | not run | not run | unknown |
| gate branch | save-backed / not save-backed / inconclusive, per § Decision gate (C5); the human picks the branch | not run | not run | unknown |
| H | H1 / H2' / H3 / not observed | **not observed (instrument blind: stale SDK closure names).** The live window's method values named its Create closures `m_SetInventoryLocalPlayer` = `anon@1065`, `m_Resize` = `anon@2806`, `m_UpdateInventoryGrid` = `anon@3657` (all `@gml_Object_UI_Prospect_obj_Create_0`), and the grid node's `m_RefreshNode` = `anon@36159@gml_Object_UI_Inventory_Grid_obj_Create_0`; the stale SDK tables carried `anon@1038/2729/3551` and no `36159`. | **not observed** — H1 unsupported (R4' empty); H2' no positive (R11 applied and matched but the store stayed 9 wide → crash; every R10 grow `reverted` with `invoked=yes`); H3 not concludable (R2-window is an empty field, two rows stayed `UNLOGGED`, four grid methods unprobed, and the Ghidra read above is not live-confirmed). | unknown |
