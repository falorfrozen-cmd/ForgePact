# Bigger prospect window — research log

phase0-status: complete
phase1-status: complete

Issue: [ForgePact #9](https://github.com/falorfrozen-cmd/ForgePact/issues/9),
"[QoL] Bigger prospect window" — *"Currently prospect window is way too small
for the amount of items players can hold in their inventory."* Opened
2026-09-15, label `enhancement`.

Status (2026-09-18): **Phase 0 complete; the gate is inconclusive, and the human chose
auto-prospect on insert.** Phase 0c ran live (§ Results, Phase 0c column). A written
save plus a relaunch destroyed everything left in the vanilla prospect grid (R7 = lost).
idcheck could not see inside the instance the window's profile getter returns
(`not observed (scan incomplete)`), so whether the grid is save data stays unknown.
The human decided (2026-09-18) against enlarging the grid and for **auto-prospect on
insert** (§ Decision gate, last paragraph). It needs no save-shape change, and R12
measured its call shapes. Stage B is that mod. Before anything ships, the one
unproven step, invoking the Prospect handler ourselves, still needs its own measured
invoke with a control. The feature must be off by default, behind a panel toggle.

Status (2026-09-18, later): **Stage B Phase 1 instrument built; live session pending**
(`phase1-status`). `prospectprobe contents`, `button` and `press` (research build only)
measure whether ForgePact can run the Prospect handler itself, beside a real press as the
positive control (§ Stage B Phase 1 live procedure). The decision core
(`plugin/include/ForgePact/AutoProspectMod.hpp`) and its baseline/target harness
(`tests/test_auto_prospect_behavior.py`) exist; nothing player-visible does. Nothing ships
unless a shape the player build can produce on its own is recorded in § Stage B results.

Status (2026-09-18, Phase 1 live): **Phase 1 ran live (research DLL from commit
`bbcdf53`); one short re-run remained**, and it has run (next paragraph). The positive control
passed, and five `self=captured` invoke shapes each printed `prospected` with
`invoked=yes` and `inner=yes` and were seen turning the item into materials
(§ Stage B results). No `self=found` shape could run: the button finder chose by the
window link and every small button is linked, so it printed `ambiguous (3)`. The finder
now chooses by the handler variable; a re-run of `button` and one `self=found` shape
completes Phase 1.

Status (2026-09-18, Stage B built): **Phase 1 is complete (`phase1-status: complete`)
and Stage B is built; its Phase 3 live check is recorded** (§ Stage B results, the `S-*`
rows: drag-in, click-in, rearrangement, off, grid-full and the player DLL all passed by
eye, with one unexplained `ran-no-effect=1` on the player DLL). The re-run (research DLL from
commit `cf451b3`) chose the button by its handler variable (`chosen=@261471`, the only
one of three with `activationFunc`), and `press exec-index button:activationArgs
self=found confirm` printed `prospected` with `invoked=yes` and `inner=yes`, seen by eye
(§ Stage B results, P-shapes). That is the one shape the ship rule accepts, and the only
one the player build uses. The mod is `autoprospect 1|0` (a player command; the panel's
**Auto-prospect** switch in Gameplay Mods, off by default): a both-route hook on
`m_MoveItemToGrid` tells the decision core an insert happened, and `FrameCallback`
re-finds the window, the grid and the button and invokes the handler once per landed
insert (§ Stage B ship design). `autoprospect stat` is research-build only.

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
  `on` itself releases anything kept before (and zeroes the counters) and
  starts over, so running it again mid-session throws away earlier captures.
  While on, the detours on the four getter rows (`GetProfileInventoryData`,
  `GetPlayerItemOwner`, `GetInventoryArray`, `GetPlayerProfileObj`; they must be
  detoured — `prospectprobe hook` first, and `on` says how many of the four
  are) keep the value each call returned, after the game's function returned.
  **Calls whose `self` is the `UI_Prospect_obj` window are kept** with that
  window's `@id`: the window holds two grids and every open calls the getters
  again, so "the last call" is not necessarily the one this ProspectGrid was
  built from. **The first 8 window returns per getter are kept and never
  overwritten**; every later one is counted as not kept (`dump` and `idcheck`
  print the count). The build-time returns come first after `backing on`, so a
  ring that dropped the oldest would drop exactly those; and because a return
  that was not kept might be the one this grid was built from, **any window
  return not kept rules out `copy`**. The latest call from any other `self` is
  kept apart and never replaces a window return. A kept value is the runtime's own
  array or struct, not a serialised copy: our copy of the `RValue` holds a
  counted reference for an array, but not for a struct, so each kept value is
  also assigned to a research global (`__pp_backing_<getter>_window<k>` /
  `_other`) that the collector sees; release clears those globals. The global
  is set first, and the slot's value, call number, `self` and `@id` only after
  it succeeded, so a slot never holds an unrooted value or another call's
  details (a window return lost that way is counted as not kept). The first 6
  calls of each getter per `on` are logged as
  `prospectprobe backing <getter> #n self=… result=<array len=N len0=M|struct members=N|kind> kept as window return <k> of 8|kept as the latest non-window return|NOT kept (the first 8 window returns are kept; …)`.
  **Nothing is serialised inside the getter's call** — a cyclic profile struct
  would overflow `json_stringify` there, at character load — only the shallow
  shape is read. Builtins only, never nested, never on the frame path. `off`
  stops capturing and keeps what was kept for `dump` and `idcheck`.
- **`prospectprobe backing dump`** — read-only. Prints the live grid snapshot
  and the open window's `@id`; the live `nodeGrid` (rows, the column count
  every row shares, row 0's cells) into `pp_backing_nodegrid.json`; per getter
  its call count and how many window returns are `kept=` and `not kept=`; and for every kept
  return its call number, `self`, whether that `self` is `(the open window)`,
  `(a window that is not open now)` or `(not the window)`, its shape, and
  `pp_backing_<getter>_<window<k>|other>.json`. The json files are written
  here, and only for a value whose depth-capped walk finished without hitting
  the cap (a struct cycle lands there); otherwise the line says
  `not written (walk …)`. Then it walks each kept value (arrays and plain
  structs, depth ≤ 10, at most 200000 values) for a sub-array shaped like
  `nodeGrid` (`rows` arrays of `cols`) or a flat array of `rows × cols`, and
  prints each match's path with `non-empty nodeGrid cells agreeing K/<non-empty>`
  — the **structural** comparison. Only `nodeGrid`'s non-empty cells are
  counted (an empty 6×9 agrees with any 6×9 of empties), and a cell agrees only
  when it holds an equal number, bool or string, or the same runtime object —
  two structs that merely print alike do not. With no item in the grid it
  prints `nodeGrid holds no item - nothing to compare`. **Structural agreement
  is a lead only and never picks a gate branch**: a copy of the profile data
  agrees with it by construction. A `K/N` below N is not evidence of a copy
  either: an instance reference compared by its bits, or an item struct the UI
  copied from the storage, can disagree with the storage it came from, so a
  shortfall is a lead too and never picks a branch. A walk that stopped early,
  hit the depth cap or met a value it could not look inside says so; its
  "no sub-array" is `not observed`, not absent.
- **`prospectprobe backing idcheck`** — the **reference-identity** probe, in
  one handler so no Draw runs in between. Run it **before any item is moved**
  after the open (§ Phase 0c live procedure, C3).
  1. **Positive control first**, on arrays the instrument builds itself: a
     kept reference to a nested array must read back a sentinel written
     afterwards through the live array, the walk must find the sentinel
     through the kept reference, and it must *not* find it in a separately
     built array. Any of those failing prints
     `control: not observed (stash does not track live arrays on this runner …)`
     (or which half of the scanner failed) and nothing else is done — no write
     reaches the game.
  2. **Refusals, nothing written:** no getter return kept at all; no open
     `UI_Prospect_obj` with a readable `@id`; **no kept window return whose
     `@id` is the open window's** (the refusal lists the window returns it
     holds — a return from an earlier open could be a dead array this grid was
     never built from, and would read as `copy`); no ProspectGrid node;
     `nodeGrid` missing or not an array; **no empty cell** (empty = `undefined`
     or the number 0; the refusal lists the cells it saw) — an item is never
     written over. An `@id` that cannot be read never matches anything.
  3. **The one write:** the sentinel `-7654321.25` into the first empty
     `nodeGrid[r][c]`, then a fresh read of the node's own `nodeGrid` to prove
     it landed (a write that did not land is `not observed`, never `copy`),
     the same cell of the first `nodeGrid`-shaped sub-array of the open
     window's `GetProfileInventoryData` return (else its `GetPlayerItemOwner`)
     read before and after, and a walk of **every** kept return of every
     getter for the sentinel.
  4. **Restore before any verdict:** the original cell value is written back
     and read back (`now=… (restored)`, or `NOT restored` — close the window
     without moving items and record it).
  5. **Verdict**, from all the walks together (each is printed with its
     `visited`/`depth-capped`/`unwalked` counts and, when incomplete, why —
     e.g. `incomplete: 1 unwalked (first: root VALUE_REF (an instance or other runtime reference), nothing to walk)`).
     The `kept returns from the open window` line first prints
     `window returns dropped (not kept past the first 8 per getter): N`, per
     getter. Then, in this order: `reference-identical (via <getter> <slot>
     call #n … self=… at <path>; sentinel in N calls of <getter>: #a #b -
     outlived one call)` when the sentinel is found in the kept returns of
     **two distinct calls of the same profile getter** (`GetProfileInventoryData`
     or `GetPlayerProfileObj`) — two separate executions returning the same
     array is what proves the array outlives a single call, so `nodeGrid`
     shares its array with the profile's own data and changing that storage
     changes the grid live — the sentinel found on a path through no
     UI-looking field; else `reference-identical (via <getter> <slot> call #n
     … self=… at <path>; sentinel in N calls of <getter>: #a #b but only M of them
     reached on a path with no UI-looking field, fewer than 2 - reached
     through a UI-looking field (<member>) - the array may be the window's
     own, a lead that decides no gate branch)` when fewer than two of them
     reached it on a path with no UI-looking field — a struct member on
     the `at <path>` whose name starts with `ui` or contains `window`, `node`,
     `panel` or `menu`, case-insensitive (the walk never descends into an
     instance, so a member name is the only UI state a path can show); the
     decided line's own `at <path>` list belongs to one kept return, the one
     its `via` clause names, and lists every path on which the sentinel was
     found in that return, not every path across the deciding calls; it may
     still contain a UI-looking one — the branch turns on how many calls
     were clean, not on what the line prints. The UI-looking-field check
     is a name rule standing in for "reached through the window's own state",
     not a measurement, and it can only demote a would-be save-backed identity
     to a lead, never promote one: a decided save-backed still rests on the
     two-call rule plus R7, not on the name rule. Two calls prove the array
     outlives one call, not that it is saved data, and an identity reached
     only through the window's own state may be the window's own
     array; else `reference-identical (via <getter> <slot> call #n … self=… at <path>; one call only - the getter
     may build this array per call, a lead that decides no gate branch)` when
     only **one** distinct call of a profile getter holds it — a single
     return proves only that `nodeGrid` is that call's own array, not that the
     array outlives the call, and it names every profile getter that hit;
     else `reference-identical (via <getter> …; not a profile getter - …)`
     when only a `GetPlayerItemOwner` or `GetInventoryArray` return holds it —
     recorded with its getter and `self`, a lead that decides no gate branch
     (neither getter's return has been measured to be profile storage; it could
     be a UI array); else `not observed (scan incomplete: N of M walks; <each
     incomplete walk and why>)` when any walk was incomplete; else
     `not observed (N window returns not kept …) - never copy` when any window
     return was not kept; and `copy` **only** when every walk completed, no
     window return was dropped, and none holds it. A walk is
     complete only if it was not truncated, hit no depth cap, and left nothing
     unwalked: **every value that is not a number, bool, string, `undefined`,
     `null` or unset and not a walkable array or plain struct — an instance
     reference (`VALUE_REF`, how this runner hands out instances), a method
     value (its bound `self` may hold the storage), a pointer — counts as
     unwalked.** A getter whose return is itself an instance reference counts against `copy`
     the same way (`root VALUE_REF …, nothing to walk`): the instance may hold
     the storage, and nothing looked inside it. So `copy` needs every kept
     return, of every getter, to be an array or plain struct walked to the
     end; if `GetPlayerItemOwner` or `GetPlayerProfileObj` hands back an
     instance, idcheck reads `not observed`, never `copy`, and the gate is
     inconclusive unless identity or R7 decides it. A `ds_*` id held as a
     plain number cannot be told from a number, so storage behind one is not
     seen; that limit stands beside every `copy`.

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
   Load a character, then `prospectprobe backing dump`. Record whether any
   getter fired at load, each kept return's `self` and shape, and the
   `bp_ipc\pp_backing_*.json` files the dump wrote (or its `not written (walk …)`
   line) — this is the "where the store is sized" read. Do not run
   `prospectprobe backing on` again this session: `on` releases everything
   kept.
2. **C2 (open + capture, with no item moved).** Walk to the Prospect Cube,
   open the window. **Move no item until C3 has run** — moving or prospecting
   can replace the arrays the getters returned, which would leave a dead
   capture that reads as `copy`. `prospectprobe grid` → confirm
   `w=9 h=6 rows=6 cols0=9` (**C-grid** control: `citrace dumpobj` agrees).
   `prospectprobe backing dump` → record the open window's `@id`; per getter
   how many window returns were kept, how many were not kept (only the first 8
   per getter are), and which say `(the open window)`; their
   shapes; the live `nodeGrid` shape; and any `nodeGrid`-shaped sub-array (with
   the grid empty the dump prints `nodeGrid holds no item - nothing to compare`
   — the structural lead is taken in C2b). If no kept return says
   `(the open window)`, stop: `not observed (backing never captured the open
   window's getter calls)`.
3. **C3 (reference identity, with its control — still with no item moved).**
   `prospectprobe backing idcheck`. Record: the **positive control** verdict
   first (if it fails, the whole probe is `not observed`); any refusal
   verbatim (a `no kept return came from the open window` refusal makes the
   probe `not observed`, never `copy`); the `kept returns from the open
   window` line with its `window returns dropped` count; every walk line with
   its `unwalked` count and, for an incomplete walk, its reason; for **every**
   walk that found the sentinel, its `at <path>` and call number (a path
   through a UI-looking field — defined in § Instrument — is itself a lead);
   the verdict
   (`reference-identical (via …)` with the getter, `self`, `at <path>` and
   call number it names, and whether it decided — **two distinct calls of the
   same profile getter**, `outlived one call` — or is a `one call only` lead /
   `copy` / `not observed (scan incomplete …)` / `not observed (N window
   returns not kept …)`);
   both cell values; and confirm the chosen cell was empty and was restored
   (re-run `prospectprobe grid` / `dumpobj` to confirm no item moved). This is
   the one bounded write Phase 0c makes.

   **C2b (auto-prospect measurement and the structural lead — only after C3,
   because it moves items).** With the window open, `prospectprobe watch on`,
   `prospectprobe arm budget=20 UiAProspectButton anon@15345 anon@8881`
   (`arm` filters by label substring; the grid closures' labels are
   `UI_Inventory_Grid_obj anon@15345` = `m_MoveItemToGrid` and
   `UI_Inventory_Grid_obj anon@8881` = `m_DropItem`, so the `anon@N` substrings
   are what select them). The tester **places one item into the prospect grid
   by dragging it in**, then runs `prospectprobe backing dump` and records the
   **structural lead**: each sub-array's `non-empty nodeGrid cells agreeing K/N`
   (a lead only — a copy agrees by construction — and it never picks a gate
   branch). Then the tester places a second item by **clicking it in** (the
   game's click-to-move), and presses Prospect. Record: the `self`/`other`/args
   and `object_index` the insert closure and `UiAProspectButton` were called
   with; which insert closure fired for the drag-in and which for the
   click-in; the `grid-post` snapshots; which `UI_Prospect_obj` instance
   variable holds the button's handler as a method value (`prospectprobe grid`'s
   `m_*` list, `citrace dumpobj UI_Prospect_obj`); and whether the items left
   the grid inside the logged `UiAProspectButton` call (its `grid-post` says
   `CHANGED`) or later. `UiAProspectButton` has never been seen firing through
   this route (nobody pressed the button in Phase 0a/0b), so it has no positive
   control: `calls=0` after a press is `not observed`, never "the button does
   not call it". What C2b records is a call shape seen, not an invoke proven —
   Stage B needs its own measured invoke of the handler, with a control,
   before anything ships. Then **verify the leftover-material claims live**
   (record as claims, not facts): after one prospect, does the result material
   sit in the grid; can a further item still be inserted and prospected; is
   the material itself ever taken as prospect input or does it block the next
   insert. **R12** = the button call shape + the insert closures + the handler
   variable + the leftover-material observations.
4. **C4 (R7, controlled save).** Place a junk item (e.g. a spare ore/ring) in
   the prospect grid. Note the `.hss` mtime. Close the window; **cause the game
   to write a save** (a zone change / save point / return to menu — whatever
   writes `%LOCALAPPDATA%\Hero_Siege\*.hss`); confirm the `.hss` mtime
   **advanced** after the item was placed. Only then relaunch. Reopen and read
   where the item is: **R7** = `returned to inventory` / `kept in the prospect
   grid` / `lost`, with both mtimes. (The Phase 0b Molten Ring was void because
   the save predated placing it — do not repeat that; the mtime check is the
   gate.)
5. **C5 (decision-gate inputs).** From C3 and C4 state which gate branch
   holds, reading the decision gate's rules in their order: **save-backed** if
   idcheck = `reference-identical` via `GetProfileInventoryData` or
   `GetPlayerProfileObj` found in **two distinct calls of the same profile
   getter** at a path through no **UI-looking field** (control passing) or R7
   = kept in the prospect grid across a written save; otherwise **not
   save-backed** if idcheck = `copy`
   (control passing, a kept return from the open window, no window return
   dropped, every walk complete — so every kept return an array or struct,
   none an instance) and R7 = returned to inventory; otherwise
   **inconclusive**. An identity through a profile getter found in **one call
   only** decides nothing by itself — a single return proves only that
   `nodeGrid` is that call's own array, not that the array outlives the call —
   and neither does an identity where fewer than two of the calls
   holding it reached it on a path with no **UI-looking field** — the
   mixed case counts, one clean call among several UI-reached ones still
   decides nothing — nor an identity through `GetPlayerItemOwner` or
   `GetInventoryArray` only; all are recorded in § Results with their getter,
   `self` and call number. The structural agreement from
   C2/C2b is recorded as a lead and decides nothing.
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

The rules are read **in this order, and the first that holds decides**:

- **Save-backed** — `backing idcheck` = `reference-identical (via …)` through
  **two distinct calls of the same profile getter** — via
  `GetProfileInventoryData` or `GetPlayerProfileObj` — each holding the
  sentinel at a path through no **UI-looking field** (with its control
  passing), **or** R7 = kept in the prospect grid across a *written* save.
  Either one decides it, whatever the other says. An
  identity through the same profile getter found in **one call only** does
  not decide it — a single return proves only that `nodeGrid` is that call's
  own array, not that the array outlives the call — and neither does identity
  through `GetPlayerItemOwner` or `GetInventoryArray`: neither return has been
  measured to be profile storage (it could be a UI array), and a `one call
  only` identity proves nothing about persistence either way. Both are
  recorded in § Results with the getter, `self` and call number idcheck
  names, as a lead. Then enlarging the grid
  changes save-data shape, and the risks are:
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
- **Not save-backed** — only when save-backed does not hold: `idcheck` =
  `copy` (control passing, a kept return from the open window, no window
  return dropped, every walk complete — so every kept return of every getter
  an array or plain struct, none an instance reference) **and** R7 = returned
  to inventory. The store is a transient copy
  of the profile data, and the next experiment (not run in Phase 0c) is
  **H2''**: grow the `GetProfileInventoryData` return array inside its detour,
  before `m_SetInventoryLocalPlayer` consumes it, and see whether `nodeGrid` is
  then built larger. The human approves that as the Stage-B-bound design or
  asks for more measurement.
- **Inconclusive** — everything else: a control failed, no getter return from
  the open window was kept, `idcheck` refused or read
  `not observed (scan incomplete …)` (including a getter that returned an
  instance) or `not observed (N window returns not kept …)`, identity through
  a profile getter found in **one call only** (with R7 not kept — a `one call
  only` identity with R7 = returned is inconclusive, not not-save-backed:
  `copy` is never printed once a sentinel was found), identity where fewer
  than two of the calls holding it reached it on a path with no
  **UI-looking field** (the mixed case counts, one clean call among several
  UI-reached ones still decides nothing; with R7 not kept —
  with R7 = returned it is inconclusive too, not not-save-backed), identity
  through `GetPlayerItemOwner` or `GetInventoryArray` only (with R7 not kept), R7 was
  not measured across a written save, or `copy` with R7 = lost. H stays `not observed`; the fallback is a
  local Ghidra read of the profile storage's *construction* path (paraphrase
  only, C7), which informs a further replan and never closes the issue.

**Structural agreement is a lead, never a branch.** A copy of the profile data
mirrors it by construction, so `backing dump` finding a `nodeGrid`-shaped
sub-array — even with every non-empty cell agreeing — cannot tell the storage
from a copy of it. It is recorded in § Results (taken with at least one junk
item in the grid, counting only agreeing non-empty cells, C2b) and may inform
a replan; it never picks save-backed, not save-backed or inconclusive.

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
**Phase 0b** (2026-09-17, build `ef8d54f`) is filled; its `not observed` fields name their reason. **Phase 0c** (2026-09-18, build `c091f65`, the second launch) is filled. Its gate branch is **inconclusive**, and `phase0-status` stays `pending` until the human decides.

**Phase 0c, first launch (2026-09-18, build `723cc52`): the instrument was blind, so nothing below comes from it.** `prospectprobe hook` detoured 91/91 rows, and the controls were live (`CheckPlayerInteraction` 192,220 calls; the idcheck sentinel control passed). With the window open, `prospectprobe grid` read `w=9 h=6 rows=6 cols0=9`. But `backing dump` printed `open window=none` and kept **0** window returns for every getter, even though `m_SetInventoryLocalPlayer` (`anon@1065`) ran once and the getters ran about 285,000 times. idcheck then refused: `no open UI_Prospect_obj window with a readable id … no write made`. The cause was the probe, not the game. On this runner `id` and `object_index` come back as `VALUE_REF`, and `PpInstanceId` / `PpObjectName` / `PpDescribeSelf` accepted numbers only. So the window had no readable id, and every getter call's `self` was filed as `not the window`. They now read the index the way `N1ObjectIndex` does, and a refused `self` prints its `object_index`, so a real struct can be told apart from a ref (`test_probe_self_and_window_helpers_accept_value_ref`). **Not established by that launch:** whether the getters' `self` is the window. That is measured on the next launch. Load-time returns, recorded only as shapes: `GetProfileInventoryData` and `GetPlayerProfileObj` returned instance references, `GetInventoryArray` an 18-string array, `GetPlayerItemOwner` `int64 0`.

| Field | Meaning | Phase 0a | Phase 0b | Phase 0c |
|---|---|---|---|---|
| R1 | object + nth of the instance carrying the grid size | `UI_Inventory_Grid_obj` instance nth **5 of 6** while the window is open — the one whose `uiNodeCallstack` reads `"ProspectGrid"` (`gridName` `"Prospectron RX9000"`, `masterUi`/`parent` = the `UI_Prospect_obj` instance). `UI_Prospect_obj.prospectGrid` references it. | unchanged: the `UI_Inventory_Grid_obj` whose `uiNodeCallstack` reads `"ProspectGrid"` (nth 5, `@265938`; `@261558` after the relaunch), found by `prospectprobe grid`'s resolver rather than by nth. | not run (not in the Phase 0c procedure) |
| R1-note | window resident while closed? | window **not** resident while closed: `UI_Prospect_obj` has no live instance until opened; closing destroys it and its grid nodes (`UI_Inventory_Grid_obj` count drops to 1, the HUD `"PotionGrid"`). | unchanged: `citrace dumpobj UI_Prospect_obj` with the window closed → no live instances (L2). | not run (not in the Phase 0c procedure) |
| R2 | size variables on R1 and their vanilla values | on the ProspectGrid node: `nodeGridWidth` = 9, `nodeGridHeight` = 6, `nodeGrid` = array[6] of arrays (rows × cols), `nodeWidth` = `nodeHeight` = 92.8, `navBboxWidth` = 835.2 (= 9 × 92.8), `navBboxHeight` = 556.8 (= 6 × 92.8), `gridBackground` = `Craft_Grid_Large_spr`, `gridScale` = 1. The window's own 100 variables were dumped but **not searched for 9/6** — Phase 0b step L2 does. | unchanged: `prospectprobe grid` = `w=9 h=6 rows=6 cols0=9 cell=92.8x92.8 bbox=835.2x556.8 scale=1`. **`nodeGrid` is not built from `nodeGridWidth`/`nodeGridHeight`** (R9, R10, R11); the Ghidra read below points at the player's profile inventory data instead — a lead, not established. | not run (not in the Phase 0c procedure) |
| R2-window | `UI_Prospect_obj` variables equal to 9 or 6 (L2) | not run (added in Phase 0b) | **empty** — the live `UI_Prospect_obj` has 100 variables, **0** equal to 9 or 6 (and 15 `m_*` methods). The window carries no variable the size could be written into before the node is built. | **empty**: 100 variables on `UI_Prospect_obj`, 0 equal to 9 or 6 (unchanged from Phase 0b) |
| R3 | vanilla input grid, columns × rows, by eye | 9 columns × 6 rows (by eye, and equal to R2). | 9 columns × 6 rows (unchanged). | `w=9 h=6 rows=6 cols0=9` by `prospectprobe grid` on both launches; not re-read by eye |
| R4 | detoured rows whose args carry R3 on a window open (Stage A table) | **not observed among detoured rows.** 41 rows detoured, 8 `not found st=14` (all eight object-Create closure rows: `UI_Prospect_obj anon@1038/2729/3551`, `Prospect_Cube_obj anon@320`, `UI_Button_Journal_Prospect_obj anon@324`, `UI_Journal_Prospecting_obj anon@1003`, `UI_Node_Parent_obj anon@1508/1909` — stale names). `arm budget=500`, one open, no row `UNLOGGED`. Fired: `UiCreate` 1 (`a0`=object `UI_Prospect_obj`, `a1`=1, `a2`=1), `UiCreateNode` 42 (#41 creates `UI_Inventory_Grid_obj` `"ProspectGrid"`: `a0`=0 `a1`=0 `a2`=object `UI_Inventory_Grid_obj` `a3`=script `UiAActivate` `a4`=`"ProspectGrid"` — no size), `UiSetGrid` 10 (`a0` = row index 0..5 / 0..3, `a1..a7` = instance refs, navigation links), `UiMoveNode` 131, `UiCreateContainer` 1, `UiSetRef` 2, `___struct___408/409/410` 1/42/1, `InventoryResetTabs` 1, `InventoryInitGrids` 1 (`a0`=true), `GetInventoryGridNode` 1, `UiResizeInventoryNodes` 2 (`a0`=2332 `a1`=1168.7), `PlayerMouseAction` 1. Zero: `UiAProspectButton` and its structs, `UiSetGridArray`, `UiResetGrid`, `UiContainerChange`, `UiChangeVisibility`, `UiSetNodeScale`, `UiRemoveNode`, `s_ItemGridInfo`, `s_InvNode`, `GridHasSpace`, `GridAddItem`, `ParseItemToGrid`, the rest. **No logged call carried 9 or 6 as a numeric argument.** | superseded by R4' | superseded by R4' |
| R4' | detoured rows (regenerated table) whose logged args carry 9/6 on a window open, with identification (L5) | not observed (closure rows not detoured) | **not observed among detoured rows** — no logged call in either L5 pass (budgets 500 and 5000) carried a real 9 or 6 as an argument. `UiCreateNode #41` creates the node with `a4="ProspectGrid"` and no size. | not run (not in the Phase 0c procedure) |
| R5a | override on each R4/R4' row changes the drawn grid (L6) | not run (R4 empty). | not run (R4' empty). | not run (not in the Phase 0c procedure) |
| R5b | bare `set` of a size variable changes the drawn grid and accepts an item | `nodeGridWidth` 9 → 18 on the open ProspectGrid node (`set`, readback ok): **fatal GML error within one frame, before any drag** — `crash.txt`: "ERROR in action number 1 of Draw Event for object UI_Inventory_Grid_obj: index out of bounds request 9 maximum size is 9", trace `gml_Object_UI_Inventory_Grid_obj_Draw_64` line 124, last UI node ProspectGrid; WER APPCRASH c0000005. The draw iterates `nodeGridWidth` over `nodeGrid` rows that were sized at build time. Save files were last written before the write; no save corruption. `nodeGridHeight` not tried (same class of crash expected: the array has 6 rows). | not re-run (R5b is the reason for `resize … via`) | not re-run |
| R5c | an item dropped into a new cell after a positive R5a/R10/R11: accepted / refused / error | not run | not run (no accepted enlarged cell). | not run (not in the Phase 0c procedure) |
| R6 | items in new cells consumed by the prospect button | not observed (no accepted cell). | not observed (no accepted enlarged cell). | not run (not in the Phase 0c procedure) |
| R7 | items left in the grid on close: returned to inventory / kept in grid / lost | not observed. | **not observed (void attempt)** — a junk Molten Ring placed in the grid was back in the inventory after a crash and relaunch, but the last save (`herosiege13.hss` 16:40:00) predated placing it (crash 16:56:43), so that shows only that the crash lost an unsaved move, nothing about the grid. Phase 0c's C4 measures R7 across a written save. | **lost**. A Sash of the Magi plus 3 prospect materials were left in the grid, and the window was closed. A save to the main menu rewrote `herosiege13.hss` and `inventory_order_13.hss` at 20:38:15, after placement (the baseline was 20:25:29), and a full relaunch followed. Neither the sash nor the materials were in the grid, any inventory tab, or the stash. Nothing returned them. A vanilla prospect grid left holding items across a written save is emptied, not kept |
| R8 | inventory grid, columns × rows | inventory grid (`"InventoryGrid"`, `UI_Inventory_Grid_obj` nth 2): 15 columns × 6 rows (`nodeGridWidth` 15, `nodeGridHeight` 6, `nodeGrid` array[6]). | 15 columns × 6 rows (unchanged). | not run (not in the Phase 0c procedure) |
| R9 | first logged call whose `grid-post` says `CHANGED`, with pre/post snapshots, and every later `CHANGED` (L5) | not run (no snapshot instrument) | the open is bracketed by `Prospect_Cube_obj anon@337`; inside `UiCreate(UI_Prospect_obj,1,1)`: `UiCreateNode #41` creates the node `w=0 h=0 rows=not-array`; **`nodeGridWidth`/`Height` become 9/6 before `UiCreateNode #42`, with no hooked call bracketing that write** (the window Create event's own body, which is not in the script table); **`nodeGrid` becomes a 6×9 array inside `m_SetInventoryLocalPlayer` (`anon@1065`) with no logged sub-call bracketing it**; `m_RefreshNode` (`anon@36159`) only sets `navBbox`. `anon@2143`/`anon@255` stayed `UNLOGGED` even at budget 5000 (they fire more than 17k times per open cycle) → `not observed (budget spent)` by design. | not observed: every logged call's `grid-post` reads `same`, including drag-in, click-in and both Prospect presses. `grid-post` compares the grid's size, not its contents, so it cannot show when items entered or left |
| R10 | `resize … via <method> [args]`: per method and probe (every shrink, then grow every method whose shrink `kept` or `reverted` with `invoked=yes` and an unchanged store; only a grow's `reverted` counts toward H3), `kept`/`reverted`/`rebuilt`/`destroyed`, the `probe=`, `invoked=` and `size=` values, `self=` and the args supplied, the self/args the detour logged as received, the snapshot, and any `restore unsafe` / `size exceeds store` (L9) | not run | every probed method — `m_RefreshNode`, `m_SetPosition`, `m_MouseInGrid`, `m_MouseInAnyGrid`, `window:m_Resize`, `window:m_UpdateInventoryGrid`, and (added) `window:m_SetInventoryLocalPlayer` — shrink (8×5) and grow (18×6) both **`reverted` with `invoked=yes`**, args none (argc=0, matching L5's game calls), store unchanged 6×9; only a grow's `reverted` counts toward H3, and every grow reverted. `m_RefreshNode`/`m_SetPosition`/`m_Resize`/`m_SetInventoryLocalPlayer` move only `navBbox`. Not probed (deviation: item-moving/buying methods, no known args): `m_MoveItemToGrid`, `m_StartInvDragging`, `m_DropItem`, `m_BuyItemConfirmed` → not observed. **No probed method resizes `nodeGrid` from `nodeGridWidth`/`Height`.** | not run (not in the Phase 0c procedure) |
| R11 | `setat` before the R9 builder: applied line, `grid-post`, drawn grid (L7/L8) | not run | `setat UI_Prospect_obj anon@1065 pre grid nodeGridWidth 18` **applied on the matching call** (`was=9 now=18`, self/other/argc matching L5) → game crash: `Step Event0` of `UI_Inventory_Grid_obj` line 75, `index out of bounds request 15 maximum size is 9`. The store built inside `anon@1065`'s extent **did not follow** the width written at its entry. Height not tried (the crash ended that launch). | not run (not in the Phase 0c procedure) |
| C | write, hook and enumeration controls | **C-write = no**: `set UI_Prospect_obj 0 x` 60/600/1500 and `image_alpha` 0.3 (readback ok / float MISMATCH) and `set UI_Inventory_Grid_obj 5 gridScale 0.5` all landed and read back, none changed anything visible. **C-hook passed** (`CheckPlayerInteraction` 10200 → 14400 in two seconds). **Enumeration control passed** (`oget UI_Inventory_Grid_obj nodeGridWidth` = 4 = nth 0's dumped value; `inames` total 124 = `dumpobj` count). **L6 = yes** (reopen: new instances, vanilla values). | C-hook (L4): **passed** — `CheckPlayerInteraction` 1500 → 5700, then 1800 → 6000 on the relaunch; `hook` 87 detoured / 0 failed, including the four live-read closures `UI_Prospect_obj anon@1065/2806/3657` and `UI_Inventory_Grid_obj anon@36159`. Phase 0a's stale-name blindness is closed. | **live**: 91/91 rows detoured, 0 failed, on both launches. `CheckPlayerInteraction` climbed (18,860 at load, 182,100 by the end). idcheck's sentinel control passed on both launches |
| C-grid | `prospectprobe grid` agrees with `citrace dumpobj` on the ProspectGrid node (L2) | not run | **passed** — `prospectprobe grid` = `w=9 h=6 rows=6 cols0=9`; `citrace dumpobj` of the ProspectGrid node agreed; `inames` total 124 = `dumpobj` count. | **half**: `prospectprobe grid` = `w=9 h=6 rows=6 cols0=9`; the `citrace dumpobj` of the ProspectGrid node was not run |
| C-write2 | `nodeWidth` / `navBboxWidth` by-eye write control on the node (L3) | not run (R5b already proves the node route reaches the draw) | **`nodeWidth` changed live** (46.4: cells drawn half-width, same frame; restored 92.8). The background sprite `Craft_Grid_Large_spr` did **not** move (see Background sprite). Window `x` write: no visible change (as Phase 0a). | not run (not in the Phase 0c procedure) |
| Background sprite | `gridBackground` of the ProspectGrid node, and whether it follows the grid | `Craft_Grid_Large_spr` (R2) | `Craft_Grid_Large_spr` is a **fixed 9×6 image**: with cells drawn half-width (C-write2) it did not change, so a larger grid would draw cells beyond it and needs its own background handling (a Stage B note). | not run (not in the Phase 0c procedure) |
| Ghidra read (paraphrase) | local read of the open path on the current exe, paraphrased; nothing decompiled is in this repository | not run | `m_SetInventoryLocalPlayer` (`anon@1065`) begins by fetching the player's **profile inventory** data (`GetProfileInventoryData`) and the item owner (`GetPlayerItemOwner`), passing its own self/other and a profile reference, then wires the grid nodes from what those return; both getters take the window instance as their self. Resolving every numeric constant referenced by `anon@1065`, `InventoryInitGrids`, `GetProfileInventoryData`, `GetPlayerItemOwner`, `m_UpdateInventoryGrid`, `m_RefreshNode` and the cube's closure found no 9 or 6 (`m_Resize` references 18 once; layout, unverified). The window's Create event, where `nodeGridWidth`/`Height` are set, is not in the script table and was not read. **Reading, not live-confirmed:** `nodeGrid` is, or is copied from, per-profile inventory storage whose 9×6 shape is fixed where that data is created — consistent with R11 and R10. If it is the storage itself, "bigger" is a save-data change (§ Decision gate). Phase 0c tests it. | not run. C7 is the fallback for the inconclusive gate below; not started |
| Identity attempt (`callnum`) | calling a getter directly to compare its result with `nodeGrid` | not run | **instrument misuse, not a result**: `callnum GetProfileInventoryData` with no arguments, `0` and `1` — without the window as self, which the getter needs — threw twice and then crashed the game (`Controller_obj` Step: `array_get :: Index [-1] out of range [1]` in `EnemyStepHandleNew`). It establishes nothing about the store. Phase 0c never invokes a getter; it keeps what the game's own call returned (`prospectprobe backing`). | not run (not in the Phase 0c procedure) |
| R12 | auto-prospect: `UiAProspectButton` call shape (self/other/args/`object_index`; no positive control, so 0 after a press is `not observed`), which insert closure fired for the drag-in and for the click-in, `grid-post` snapshots, the window variable holding the handler's method value, whether items left the grid inside that call or later, and the leftover-material claims checked live (C2b, after C3) | not run | not run | Drag-in and click-in both go through `m_MoveItemToGrid` (`anon@15345`); `m_DropItem` (`anon@8881`) never fired. Drag-in: `self` = `other` = the ProspectGrid node, 4 args `(undefined, 0, 0, 0)`. Click-in: `self` = the ProspectGrid node, `other` = the source inventory grid node, 0 args. Prospect: `UiAProspectButton` with `self` = the button (`UI_Button_Small_obj`) and `other` = the window, 1 arg (an array); the handler lives on the button, not among the window's `m_*` methods. Inside it, `___struct___123` ran with a real struct `self` (`object_index=undefined`) and args `(73|72, 0, 14)`; the one-item presses here gave one call each, which read as once per item, but Stage B Phase 1 corrected that (P-shapes / P-free-cells: 13 items in one press gave +3, one per material stack produced — measured, not proven). Claims checked by eye: the materials **stay** in the grid, a further item **can** still be inserted and prospected, and the materials were **not** taken as input — see Stage B's `P-materials-only`, where a press with only materials in the grid ran no inner call and changed nothing (the once-per-item reading of the inner count is withdrawn above). Materials stack down column 0 |
| load-time capture | getters that fired at character load, each kept return's `self` and shape, and the `pp_backing_*.json` the C1 `backing dump` wrote (C1) | not run | not run (no `backing` instrument) | Launch 2 (launch 1 was blind; see the note above the table). All four getters fired at load: `GetProfileInventoryData` ~31,700 calls, latest from an `UI_Inventory_Grid_obj` node, returning an **instance** reference; `GetInventoryArray` ~12,900 calls, latest from `Player_obj`, returning an 18-string array; `GetPlayerItemOwner` ~1,800 calls, from `Flask_Controller_obj`, returning `int64 0`; `GetPlayerProfileObj` 13 calls, from `Achievement_Controller_obj`, returning an instance reference |
| backing structural | `backing dump`: the open window's `@id`, kept returns per getter and which came from the open window, their shapes, live `nodeGrid` shape, and any `nodeGrid`-shaped sub-array with `non-empty nodeGrid cells agreeing K/N` (taken with a junk item in the grid) and walk completeness (C2, C2b). A lead only; never picks a gate branch | not run | not run | Open window `@261486`. At open, `UI_Prospect_obj` itself called `GetProfileInventoryData` once and `GetPlayerItemOwner` once, and never called `GetInventoryArray` or `GetPlayerProfileObj`. With an item in the grid, the window calls `GetProfileInventoryData` continuously (8 kept, 867 not kept). Every window return of it is the same **instance** reference that every other caller gets, and the walk does not enter instances (`unwalked=1 … references=1`), so the structural lead was **not observed**. Each `nodeGrid` cell with an item holds a node struct (`nodeStartX`, `nodeStartY`, `nodeLocked`, `nodeIsPermanent`, `nodeFingerprint` ending in `-14`); inventory fingerprints end in `-0` |
| backing idcheck | `backing idcheck`, run before any item is moved: the control verdict first, any refusal, `kept returns from the open window` with its `window returns dropped` count, every walk with its `unwalked` count and reason, then `reference-identical (via …)` with the getter, `self` and call number it names — deciding only through two distinct calls of the same profile getter at a path through no UI-looking field, else a lead reached through a UI-looking field, else a `one call only` lead naming every profile getter that hit — / `copy` / `not observed`, the cell, both values, and `restored` (C3) | not run | not run | Control **passed**. The one write went to cell `[0][0]`: `was=undefined`, the sentinel read back, then `now=undefined (restored)`; no item was moved. Verdict: **not observed (scan incomplete: 3 of 6 walks; root VALUE_REF, an instance, nothing to walk)**. The window's `GetProfileInventoryData` return is an instance the walk cannot enter. Only one window call per getter was kept before any item moved, so a decided verdict was out of reach anyway |
| gate branch | save-backed / not save-backed / inconclusive, per § Decision gate read in its order (C5); the human picks the branch | not run | not run | **inconclusive**. idcheck read `not observed (scan incomplete …)`, a getter returned an instance, and R7 = lost. Save-backed does not hold (no two-call identity; R7 is not kept), and not save-backed needs idcheck = `copy`. **Human decision (2026-09-18): auto-prospect on insert**, not a bigger grid |
| H | H1 / H2' / H3 / not observed | **not observed (instrument blind: stale SDK closure names).** The live window's method values named its Create closures `m_SetInventoryLocalPlayer` = `anon@1065`, `m_Resize` = `anon@2806`, `m_UpdateInventoryGrid` = `anon@3657` (all `@gml_Object_UI_Prospect_obj_Create_0`), and the grid node's `m_RefreshNode` = `anon@36159@gml_Object_UI_Inventory_Grid_obj_Create_0`; the stale SDK tables carried `anon@1038/2729/3551` and no `36159`. | **not observed** — H1 unsupported (R4' empty); H2' no positive (R11 applied and matched but the store stayed 9 wide → crash; every R10 grow `reverted` with `invoked=yes`); H3 not concludable (R2-window is an empty field, two rows stayed `UNLOGGED`, four grid methods unprobed, and the Ghidra read above is not live-confirmed). | not observed |

## Stage B Phase 1 live procedure

The human chose auto-prospect on insert (§ Decision gate, last paragraph). One step of it
has never been observed: the Prospect handler (`UiAProspectButton`) run by anything other
than the game's own press. Phase 0c R12 measured that press — `self` = the
`UI_Button_Small_obj` button, `other` = the `UI_Prospect_obj` window, one argument, an
array — but not which button variable holds the handler, what the array holds, whether the
insert closure (`m_MoveItemToGrid`, `UI_Inventory_Grid_obj anon@15345`) fires during a
press or for every item at load, or what a press does with only materials in the grid.
Phase 1 measures all of it in one research build, with a real press as the positive
control in the same session. It follows the pet quest precedent (the guide's Known
Limitations item 11): resolve by name only, record what was supplied beside every result,
and say why every refusal happened. Nothing ships unless a shape is recorded here.

### Instrument (research build only, under `prospectprobe`)

- **`contents`** (hook-free): the ProspectGrid node, found the way `grid` finds it, as
  `contents=@<id> filled=<K> empty=<E> uiNodeCallstack=<kind>`, then the filled column
  indexes per row, then the distinct `nodeFingerprint` values of the filled cells. A cell
  is empty when it is `undefined` or 0 (idcheck's rule). A read that fails says
  `unreadable`, never an empty grid.
- **`watch`**: a logged call's `grid-post=` line now ends in ` contents=<K>-><K'>`, the
  filled count before and after the game's function. That shows whether an insert fills
  the cells inside the call or later. `?` means not read.
- **Press capture**: while `UiAProspectButton` is detoured, the game's own call keeps its
  `self` and `other` ids and argument 0. The argument is rooted through the research
  global `__pp_press_arg` and logged shallowly expanded before and after the call.
  `press show` prints it, or `none captured`. ForgePact's own `press` never replaces it.
- **`button`** (hook-free): every `UI_Button_Small_obj` with its `@id`, the variables whose
  value is the open window's id, every variable that resolves to `UiAProspectButton` with
  its kind, `index-match=` (the value against `asset_get_index`) and `method-index-match=`
  (`method_get_index` of a method value against the same index), and its arrays, marked
  when one is the captured argument or equal to it. A button **qualifies** when it is
  linked to the open window **and** one of its variables is the handler itself — a method
  value whose `method_get_index` is the handler's index, or one resolving to the handler's
  row; a plain number equal to the index is printed as a lead and never counted. It ends
  `chosen=@id` (exactly one button qualifies), `none` or `ambiguous (N)`, then
  `captured-self=@id same` or `DIFFERENT` when a press was captured — the control on the
  finder. The first build chose by the window link alone and printed `ambiguous (3)` live:
  all three small buttons link to the window through `masterUi`/`parent`, and only the
  Prospect button carries the handler (`activationFunc`). `press … self=found` uses the
  same finder.
- **`press <route> <argsrc> [self=found|captured] confirm`**: one invoke per command, by
  name, `self` = the button and `other` = the window. Routes: `exec-index` hands
  `script_execute` the handler's own asset index; `exec-var:<var>` hands it the button
  variable's value; `scriptex` calls the script by its SDK name. Argument sources:
  `captured` (the rooted array itself), `copy` (a new array with the same elements),
  `button:<var>` (the button's own array variable), `empty` (a new empty array). Every
  refusal prints `no call made` and comes before any call: no `confirm`, a pending
  override or setat, no open window, no button or an ambiguous one, an unavailable
  argument source, a grid with no filled cell, or a `UiAProspectButton` row that is not
  detoured (run `prospectprobe hook` first — without it `invoked=` could not be proven,
  so no verdict could tell "nothing ran" from "ran and did nothing"). The outcome line
  carries `st=`, `(threw)`, `res=`, `invoked=` (the `UiAProspectButton` row's count
  across the call), `inner=` (the `___struct___123@UiAProspectButton` row's), `self=`,
  `other=`, `route=` and `args=`, then the contents after and a verdict decided by the
  `invoked=` count, never by `st=` alone:
  - `prospected (filled K->K', fingerprints changed)` — the grid changed **and** the
    handler was entered (invoked delta ≥ 1);
  - `grid changed but handler not entered (invoked=NO) - not a prospect by this call`;
  - `handler entered, grid unchanged` — invoked delta ≥ 1, no grid change;
  - `dispatched but handler not entered (invoked=NO)` — `script_execute` reported
    success, but the handler's body never ran;
  - `not dispatched` — the call failed or threw and the handler was not entered.

### Shapes, in order

1. `press exec-index captured confirm`
2. `press exec-index button:<var> confirm`, if `button` marked a variable as the captured
   argument
3. `press exec-var:<var> captured confirm`, if `button` found a handler variable
4. `press exec-index copy confirm` (does the array's content matter, or its identity?)
5. `press exec-index empty confirm`
6. `press scriptex captured confirm`
7. `press exec-index captured self=captured confirm`, only if `captured-self=` printed
   `DIFFERENT`

To ship, a shape must print `prospected` **and** `invoked=yes` (delta ≥ 1) **and**
`inner=yes` (delta ≥ 1) — the same two counters `P-control` needs, so a grid change that
came from something other than the handler's body is never recorded as a prospect — with
`self=found` and an argument source the player build can produce on its own —
`button:<var>` or `empty` — and the item must be seen turning into materials. If only `captured` or `copy` worked, the recorded elements
decide how a player build could construct the array, and that is a replan, not a ship.

### Conflicts

- `prospectprobe hook` detours `anon@15345` at the same address the Stage B hook will
  patch. Phase 1 runs `prospectprobe hook` and has no Stage B hook; the later Phase 3 must
  not run `prospectprobe hook`. Do not run `citrace nativetrace` in either session.
- A faulting shape crashes the game: the build uses `/EHsc`, so `catch (...)` does not
  catch an access violation. Relaunch the same build, record the crash in `P-shapes`, and
  go on with the next shape.

### Procedure

- **P0.** Build dev (`plugin_build\build.bat dev`). With the game closed, copy
  `plugin_build\BloodPactPlugin_rel.dll` over `<game>\bin\mods\aurie\BloodPactPlugin.dll`
  (the player DLL stays backed up as `BloodPactPlugin.dll.ship-backup-20260918_201853`).
  Back up `%LOCALAPPDATA%\Hero_Siege`. Use junk items only. Record the build's commit in
  § Stage B results.
- **P1.** Before loading a character, run `prospectprobe hook`. The rows
  `UiAProspectButton`, `___struct___123@UiAProspectButton` and
  `UI_Inventory_Grid_obj anon@15345` must print `detoured`; if one does not, stop — every
  count below would be blind. Load the character, then run `prospectprobe show` and record
  the `anon@15345` calls made at load (`P-load-calls`).
- **P2.** Open the cube. Run `prospectprobe grid`, `prospectprobe contents`,
  `prospectprobe button` and `prospectprobe watch on`.
- **P3, the positive control.** Run
  `prospectprobe arm budget=50 UiAProspectButton ___struct___123 anon@15345`. Click one
  item in, then run `contents` (and note the logged `anon@15345` line's
  `contents=K->K'`). Run `show`, press Prospect by hand, then run `contents`, `show`,
  `press show` and `button`. `button`'s `captured-self=` goes to `P-button`; the
  `anon@15345` `since=` across the press goes to `P-reentry`. Press Prospect again with
  only materials in the grid, and record what `show` and `contents` print
  (`P-materials-only`).
- **P4.** For each shape in the order above: insert one item, run the `press … confirm`
  command, then `show`, and note by eye whether the item became materials. Record the
  verdict word as printed together with `invoked=` and `inner=`: only `prospected` with
  `invoked=yes` and `inner=yes` counts toward shipping; `handler entered, grid unchanged`,
  `dispatched but handler not entered (invoked=NO)` and `not dispatched` are three
  different results and are never merged. A `refused: … is not detoured` means P1 was
  skipped or failed — go back to P1. If the game crashes, relaunch the same build, record
  the crash and go on (`P-shapes`).
- **P5.** Keep inserting and prospecting (by hand, or with a working shape), running
  `contents` after each prospect and recording `empty=`, until column 0 is full of
  materials or the junk runs out (`P-free-cells`).
- **P6.** Close the window with materials in it, before any save. Reopen it and record
  whether they are back in the inventory, still in the grid, or gone (`P-close`).
- **P7.** Fill the rows of § Stage B results and set `phase1-status: complete`.

`P-control` passes only if the hand press shows `UiAProspectButton +1`,
`___struct___123 +N` and a `contents` change. If it fails, every `P-shapes` verdict is
`not observed`: an instrument that cannot see the game's own press has said nothing
about ours.

## Stage B results

Phase 1 (research build, commit recorded at P0) fills the `P-*` rows; Phase 3 (after the
Stage B adapter is built) fills the `S-*` rows. A row that could not be measured says
`not observed (<why>)`; no row is left empty once its phase is complete.

| Row | What fills it | Result |
|---|---|---|
| P-control | P3's hand press: `UiAProspectButton +1`, `___struct___123 +N`, and the `contents` change (filled and fingerprints), all three required; plus the click-in's `anon@15345` `contents=K->K'` | **PASS.** P1: `prospectprobe hook` detoured 91 rows, 0 failed, the three required rows among them (the install stalled frames for about 9 s in all). Hand press: `UiAProspectButton +1`, `___struct___123 +1`, `contents` 6->1 with the fingerprints changed, and the human saw materials appear. Click-in: the `anon@15345` line read `self=@261557` (the ProspectGrid node) `other=@261539 argc=0` and **`contents=6->6`** — the cells were already filled when `m_MoveItemToGrid` was entered (see the note under this table) |
| P-button | P3's `button` after the press: `chosen=`, `captured-self=` (`same`/`DIFFERENT`), each handler variable with its kind, `index-match=` and `method-index-match=`, and any array marked as the captured argument | P2/P3 (grid @261557, 9×6): **`ambiguous (3)`** — all three `UI_Button_Small_obj` link to the window through `masterUi` and `parent`. Only @261558 carries a handler variable: `activationFunc`, a method value resolving to `UiAProspectButton`, `index-match=no method-index-match=yes`. `press show`: `self=@261558 other=@261511 argc=1`, and arg 0 is the same array as @261558's `activationArgs` (one element, an empty struct). `captured-self=@261558 DIFFERENT` only because nothing was chosen. The finder now keys on the handler variable (§ Instrument, `button`). Re-run (`cf451b3`, a new session, so new ids): 3 buttons, all linked to window @261424; only @261471 has a handler variable (`activationFunc`, `method-index-match=yes`) and it prints `qualifies`; verdict **`chosen=@261471`** |
| P-shapes | P4, per shape in order: the outcome line (`st=`, `res=`, `invoked=`, `inner=`, `self=`, `other=`, `route=`, `args=`), the verdict, and the item by eye; or the crash | All with `self=captured` (@261558), `other` = the window, `st=0`, `invoked=yes`, `inner=yes`, verdict `prospected`, each seen by eye turning into materials: `exec-index button:activationArgs` (filled 5->2); `exec-index captured` (8->3); `exec-var:activationFunc captured` (7->4); `exec-index copy` (10->5); `exec-index empty` (7->6). `scriptex captured`: not observed (skipped by the tester; `exec-index` already qualifies). No crash. First session: `self=found` not observed (the finder printed `ambiguous (3)`). **Re-run (research DLL from `cf451b3`): `exec-index button:activationArgs self=found`** — `st=0`, `invoked=yes (+1)`, `inner=yes (+1)`, `self=@261471` (the found button), `other=@261424` (the window), `route=exec-index`, `args=(button:activationArgs=array[0]={})`, contents 8->1 with the fingerprints changed, verdict `prospected`, and the human saw the item become materials. It meets the ship rule: **`invoke-shape: exec-index button:activationArgs self=found`**. `exec-index empty self=found`: not observed (not run; the first shape already qualifies) |
| P-reentry | P3: `anon@15345` calls during the hand press (`since=` across it), i.e. whether the handler moves materials in through the insert closure | not observed (one hand press): `anon@15345` stayed at 1 across it, so that press put no materials in through the insert closure. The ship adapter still ignores and counts any insert made inside its own call (`while-invoking=`) |
| P-load-calls | P1: `anon@15345` calls at character load, before the cube was opened | 0 across the character load. Controls in the same `show`: `CheckPlayerInteraction` 9950, `anon@2143` 2985, `ParseItemToGrid` 33 — the instrument was counting |
| P-materials-only | P3: a hand press with only materials in the grid — `UiAProspectButton` / `___struct___123` counts and the `contents` change | One materials-only press observed as a no-op: `UiAProspectButton +1`, `___struct___123 +0`, filled 1->1, fingerprints unchanged. One press with one set of materials, not every material |
| P-close | P6: materials left in the grid when the window closes, before any save — back in the inventory, still in the grid, or gone | Still in the grid, not returned to the inventory. The reopened window has a new grid node (@263885) holding the same 9 fingerprints |
| P-free-cells | P5: `empty=` after each prospect; the fewest free cells that still took an insert, which sets `kAutoProspectMinFreeCells` (6, one column, if not measured) | Materials were not observed to merge: each press added one single-cell stack per material type, filling column 0 rows 0–5, then column 1. The fewest free cells that still took an insert and prospected was 9 (45 of 54 filled); 6–8 free cells are not measured (not observed either way), so `kAutoProspectMinFreeCells` stays 6 and Phase 3's `S-grid-full` step measures 6–8. In that press (13 items, 6 material stacks already there) `___struct___123` ran +3 (args 73/72/61), all 13 items went, and 3 new stacks appeared with quantities that looked right to the tester — one inner call per material stack produced, not per item (measured, not proven) |
| S-drag | Phase 3 (S2): a drag-in with `autoprospect 1` is prospected once; plus the drag-in's own `contents=K->K'`, bracketed in S1 (drag-in timing is not observed yet) | **PASS.** S1 (research DLL `3fc595c`, auto-prospect off, `prospectprobe hook` 91 detoured, 0 failed): the drag-in logged `anon@15345 #1` with `self` = `other` = the ProspectGrid node (@261415), `argc=4`, and **`contents=0->0`**; `contents` straight afterwards read `filled=6` (one 2×3 item). So a drag-in fills its cells *after* `m_MoveItemToGrid` returns, the opposite order to the click-in (`6->6`); the settled-count core handles both. S2 (a fresh launch, no `prospectprobe hook`, `autoprospect 1`): the drag-in was prospected with no press, materials seen by eye, and the player line read `autoprospect: first prospect - invoked=1 prospected=1 … inserts=1 not-landed=0` |
| S-click | Phase 3 (S3): a click-in is prospected once | **PASS.** Prospected, seen by eye; `autoprospect stat` went to `invoked=2 prospected=2 inserts=2` - one invoke for the click-in |
| S-rearrange | Phase 3 (S4): moving an item (a material) inside the grid invokes nothing | **PASS.** Moving a material from row 1 column 0 to column 5: `invoked=2` unchanged, `inserts=3`, `not-landed=1`. Also observed before `autoprospect 0`: one more insert and not-landed (`inserts=4`, `not-landed=2`) with `invoked` unchanged; what the player did to cause it was not observed, and it did not invoke |
| S-off | Phase 3 (S6): after `autoprospect 0`, an insert stays in the grid | **PASS.** `autoprospect 0` printed `off - items put in the prospect grid stay there`. The grid was then filled to 48 of 54 cells with items and nothing was prospected: `while-off=24`, `invoked=2` unchanged. Reopening the window gave a new grid node (@263453) |
| S-grid-full | Phase 3 (S5): filling the grid logs `grid-full` once and stops invoking; plus what an insert does with 6, 7 and 8 free cells (not measured in Phase 1) | **PASS.** `autoprospect 1` on its own invoked nothing. A 1-cell item into 5 free cells printed one line, `autoprospect: grid-full - holding back - 5 free cells, needs 6; …`, with `grid-full=1`, `invoked=2`, and the item stayed. A second insert into 4 free cells gave `grid-full=2` and no new line (once per reason holds). With items taken out to 16 free cells, one insert prospected everything in the grid at once (seen by eye): `invoked=3 prospected=3 elsewhere=4`, and the grid went to 2 material cells. 6, 7 and 8 free cells: not observed (the inserts fell at 5, 4 and 16), so `kAutoProspectMinFreeCells` stays 6. The grid was full of items, not materials, when the line first printed, so its advice now reads `empty some of the grid` instead of `take the materials out` |
| S-player-dll | Phase 3 (S7): the player DLL with the panel toggle on prospects one insert (seen by eye), and `out.txt` shows a line naming work done, not armed state. `autoprospect stat` is research-only, so the player build logs `autoprospect: first prospect - invoked=<n≥1> prospected=<n≥1> …` (`StatLine()`'s fields) once, on the first prospect of a session; that line, after `autoprospect: hook installed -> ON`, is acceptance - a line saying only that the hook is installed or the mod is on is not | **PASS for both routes.** Player DLL `BloodPactPlugin_ship.dll` from `3fc595c` (sha256 `89B05DC2…DE61AB89`), the panel started from this checkout with Auto-prospect on: auto-apply sent `autoprospect 1`, which printed `armed`, then `HOOK INSTALLED on anon@15345@…` and `autoprospect: hook installed -> ON`. A click-in and a drag-in both became materials, seen by eye, and `out.txt` read `autoprospect: first prospect - invoked=2 prospected=1 ran-no-effect=1 … inserts=2 (… elsewhere=1 …) not-landed=0`. One invoke ran with no effect (`ran-no-effect=1`), which the research DLL did not show in S2/S3: observed, cause not observed, and harmless (nothing was lost). The player build logged no line of its own for it - it was visible only because the first-prospect line happened to carry the count - so it now logs `autoprospect: the Prospect ran but the grid did not change - …` once per session, and `… could not be read afterwards - …` for `unverified` (built after this run, not yet seen live) |

**Insert timing — an input for the ship adapter.** A click-in's `watch` line read
`contents=6->6`: the grid's filled count was the same before and after `m_MoveItemToGrid`
(`anon@15345`), and the new item was already counted before the closure was entered. So
for a click-in the cell is filled before the insert closure runs, not inside it (one
click-in observed). Whether that fill can fall in an earlier frame than the hook is not
measured, and a drag-in's timing is not observed at all (Phase 3 S1 brackets one).

The ship core therefore never takes its baseline at hook time, and does not re-read it
every frame either. It keeps a **settled** count: what the grid held when the core last
accounted for all of it (first sight of the node, the grid re-read straight after an
invoke, a refused insert whose item stays, or a pending insert that expired). Between
those it only follows removals down. An insert invokes when the filled count is above the
settled count, so a click-in filled before its hook - in the same frame or an earlier
one - still prospects once, and moving something already settled (a material, a refused
item) never does. The earlier core re-read the count every frame and kept the pre-invoke
count after an invoke; against it, the harness recorded
`FAIL target/click_in_filled_a_frame_before_the_hook_invokes_once invokes=0 notLanded=1`,
and a rearrangement straight after an invoke fired the handler
(`FAIL target/rearrangement_right_after_an_invoke_never_invokes invokes=2`). The measured
same-frame click-in passed against both cores (`tests/auto_prospect_harness.cpp`).

What is still unproven: an insert whose cell is filled and emptied again before any frame
reads it, and a drag that empties the source cell while the item is held and fills it
again on the drop (which would read as an insert, and prospect the dragged item). Phase 3
S4 checked the second by moving a material, and it did not invoke (`S-rearrange`); one
materials-only press was observed as a no-op (`P-materials-only`).

## Stage B ship design

- **Core:** `plugin/include/ForgePact/AutoProspectMod.hpp`, game-independent (it names no
  runtime interface), pinned by `tests/test_auto_prospect_behavior.py` +
  `tests/auto_prospect_harness.cpp` (baseline, target and `adapter/` scenarios, each
  target's failing line recorded) and `tests/test_auto_prospect_contract.py`.
- **Hook:** `HookOneScript` on `m_MoveItemToGrid` through the SDK constant, both routes,
  installed once from `FrameCallback` after setup. A `TABLE-ONLY` or failed install turns
  the mod off with a line saying so (a table swap never sees compiled GML's direct call).
  The body runs the game's function first, and only while the mod is on checks that
  `self` is the ProspectGrid node (`UI_Inventory_Grid_obj` by `object_index`, and a
  `uiNodeCallstack` naming `"ProspectGrid"`). It never invokes.
- **Invoke, at the point of use:** `AutoProspectTick`, from `FrameCallback` only while
  the mod is on, re-finds the window, the grid and - only while an insert is pending -
  the button (the `UI_Button_Small_obj` linked to the window through `masterUi`/`parent`
  whose `activationFunc` is a method of `UiAProspectButton`, by `method_get_index`
  against `asset_get_index`; exactly one must qualify), then runs the recorded shape once:
  `script_execute` through `CallBuiltinEx`, the handler's asset index and the button's
  own `activationArgs` array, `self` = the button, `other` = the window. It re-reads the
  grid straight after the call and hands it to the core.
- **Refusals**, each logged once per session in both builds: `no-window`, `no-grid`,
  `unreadable`, `node-changed`, `no-button`, `no-args`, `grid-full` (fewer than
  `kAutoProspectMinFreeCells` = 6 free cells: `holding back - N free cells, needs 6; empty
  some of the grid`). A failed dispatch is logged once too. The player build also logs
  `autoprospect: first prospect - …` once, and once each the first invoke that ran but
  left the grid unchanged (`autoprospect: the Prospect ran but the grid did not change -
  …`) and the first whose effect could not be read (`… could not be read afterwards - …`).
- **Not built, on purpose:** returning materials to the inventory or clearing the grid (a
  second unmeasured game operation; `grid-full` refuses instead), and any bigger grid.

## Stage B Phase 3 live procedure

Research DLL first, then the player DLL. Back up `%LOCALAPPDATA%\Hero_Siege`, junk items
only. `prospectprobe hook` detours the same address as the Stage B hook, so one of the two
would go table-only: it runs only in S1's own launch, with auto-prospect off, and never in
a launch that runs `autoprospect 1` (nor does `citrace nativetrace`).

- **S1, drag-in timing (its own launch, auto-prospect off).** `build.bat dev`; with the
  game closed, copy `plugin_build\BloodPactPlugin_rel.dll` over
  `<game>\bin\mods\aurie\BloodPactPlugin.dll` (the panel's Auto-prospect switch off).
  Before loading a character run `prospectprobe hook`; `UI_Inventory_Grid_obj anon@15345`
  must print `detoured`. Open the cube, run `prospectprobe watch on` and
  `prospectprobe arm budget=5 anon@15345`, drag one item in, and record the logged
  `anon@15345` line's `contents=K->K'` in `S-drag` (`K` = `K'` means the cell was filled
  before the closure ran, as for the click-in). Close the game.
- **S0.** Relaunch the same research DLL; do **not** run `prospectprobe hook`. Load a
  character, run `autoprospect 1`; `out.txt` must show `autoprospect: hook installed -> ON`.
- **S2 / S3.** A drag-in, then a click-in: each prospected once (seen by eye; `autoprospect
  stat` shows `invoked=` and `prospected=` up by one each).
- **S4.** Move one material to another cell inside the grid: `invoked=` unchanged,
  `not-landed=` up by one.
- **S5.** Keep inserting until `autoprospect: grid-full - holding back …` is logged, once.
  Record what the inserts at 8, 7 and 6 free cells did (prospected, or refused), then
  confirm a further insert stays put and `invoked=` stops rising.
- **S6.** `autoprospect 0`; an insert stays in the grid.
- **S7, the player DLL.** `build.bat release`; copy `plugin_build\BloodPactPlugin_ship.dll`
  over `mods\aurie\BloodPactPlugin.dll`; turn the panel's **Auto-prospect** switch on;
  prospect one insert (by eye) and confirm `out.txt` shows `autoprospect: hook installed
  -> ON` and then `autoprospect: first prospect - invoked=1 prospected=1 …`.
- Fill the `S-*` rows. The human decides which DLL stays installed.

## Stage C: materials to the bag

stage-c-status: pending

The human asked (2026-09-18) for each prospect to first move the previous prospect's
materials from the grid to the player's inventory ("bag"). The newest batch then stays
visible until the next insert, the 9×6 grid stops filling with one-cell material stacks
(today `grid-full` holds back), and only one batch is exposed to the save-time loss (R7).
Moving a material is a **second game operation** nobody has observed: which routine runs
when a player moves a material from the ProspectGrid to the bag, with what `self`, `other`
and arguments, whether it merges into an existing stack, and what a full bag does. Stage C
records all of it in one research build, with the player's own hand move as the positive
control, before anything is built. Nothing ships unless a by-name shape qualifies (the rule
at the end of § Stage C live procedure), and a move that empties a grid cell without the bag
gaining it is a loss, never a success.

What is already measured (§ Stage B results): both insert routes go through
`m_MoveItemToGrid` (`UI_Inventory_Grid_obj anon@15345`); a click-in's `other` was an
unnamed source grid (`@261539`), so which grid node is the bag is **not recorded**;
materials are single-cell stacks, one per material type per prospect, not observed to
merge; closing the window leaves them in the grid; moving a material inside the grid fires
`m_MoveItemToGrid` and was not observed to invoke (Stage B, S4 plus one unexplained case).

### Stage C static search

Done over `hs-game-sdk/cpp/include/hs_game_sdk/scripts.hpp` and `objects.hpp`: every
script name containing move, transfer, stack, add, remove, space, quick, shift, take, loot,
send, bag, inventory, grid, material or prospect (case-insensitive), then every Create-event
closure of the bag window's object. 52 names not already rows were added to
`prospectprobe`'s target table; each is the `HeroSiege::Scripts` constant's value, used
as-is. Already rows, and not repeated: `InventoryGridAddItem`, `GridAddItem`,
`GridHasSpace`, `InventoryGridHasSpace`, `GridClear`, `InvGridClearItemNode`,
`GetStackOpLocationFromGridType`, `GetItemPreferredGrid`, `GetInventoryGridNode`,
`ParseItemToGrid`, `s_ItemOperation`, `s_InvNode`, `s_ItemGridInfo`, every
`UI_Inventory_Grid_obj`/`UI_Inventory_Parent_obj` closure, `PlayerMouseAction` and the
`CheckPlayerInteraction` control. The hot rows (`ProcessInventoryGridInput` and its two
structs, `UiDrawInventoryMaterialTab`) are ordinary rows: `arm`'s budget and label filters
bound what they log. The table now has 143 rows, so `prospectprobe hook` stalls frames for
longer than Phase 1's 91 rows did (about 9 s then).

*Item operations the Stage A search left out:*

| Probe label | Runtime name |
|---|---|
| `InventoryGridAddItemPos` | `gml_Script_InventoryGridAddItemPos` |
| `InventoryGridRemoveItem` | `gml_Script_InventoryGridRemoveItem` |
| `InventoryGridAddToStack` | `gml_Script_InventoryGridAddToStack` |
| `InventoryGridCanAddToStack` | `gml_Script_InventoryGridCanAddToStack` |
| `InventoryGridHasSpaceMulti` | `gml_Script_InventoryGridHasSpaceMulti` |
| `InventoryGridAddItemToTab` | `gml_Script_InventoryGridAddItemToTab` |
| `InventorySortTab` | `gml_Script_InventorySortTab` |
| `___struct___187@InventorySortTab` | `gml_Script____struct___187@InventorySortTab@InventoryGrid` |
| `ProcessInventoryGridInput` | `gml_Script_ProcessInventoryGridInput` (hot) |
| `___struct___305@ProcessInventoryGridInput` | `gml_Script____struct___305@ProcessInventoryGridInput@ProcessInventoryGridInputFunc` (hot) |
| `___struct___308@ProcessInventoryGridInput` | `gml_Script____struct___308@ProcessInventoryGridInput@ProcessInventoryGridInputFunc` (hot) |

*Grid and stack primitives:*

| Probe label | Runtime name |
|---|---|
| `GridAddToStack` | `gml_Script_GridAddToStack` |
| `GridRemoveItem` | `gml_Script_GridRemoveItem` |
| `GridSettle` | `gml_Script_GridSettle` |
| `InventorySwapItemsNew` | `gml_Script_InventorySwapItemsNew` |
| `InventoryStackHandler` | `gml_Script_InventoryStackHandler` |
| `InventoryStackUpdateAndRemove` | `gml_Script_InventoryStackUpdateAndRemove` |
| `___struct___161@InventoryStackUpdateAndRemove` | `gml_Script____struct___161@InventoryStackUpdateAndRemove@InventoryFuncs` |
| `InventoryStackUpdateAndEdit` | `gml_Script_InventoryStackUpdateAndEdit` |
| `InventorySplitOperation` | `gml_Script_InventorySplitOperation` |
| `InventorySplitDrop` | `gml_Script_InventorySplitDrop` |
| `UiASplitStack` | `gml_Script_UiASplitStack` |
| `ItemsAreStackable` | `gml_Script_ItemsAreStackable` |
| `IsStackable` | `gml_Script_IsStackable` |
| `IsItemTypeStackable` | `gml_Script_IsItemTypeStackable` |
| `GetMaxStack` | `gml_Script_GetMaxStack` |

*Add-to-inventory family:*

| Probe label | Runtime name |
|---|---|
| `AddToInventory` | `gml_Script_AddToInventory` |
| `___struct___13@AddToInventory` | `gml_Script____struct___13@AddToInventory@AddToInventoryFunc` |
| `___struct___16@OnlineAddToStack` | `gml_Script____struct___16@OnlineAddToStack@AddToInventoryFunc` |
| `FindInventoryItemOperation` | `gml_Script_FindInventoryItemOperation` |
| `___struct___152@FindInventoryItemOperation` | `gml_Script____struct___152@FindInventoryItemOperation@InventoryFuncs` |
| `s_PendingStackOperation` | `gml_Script_s_PendingStackOperation` |
| `s_InventoryDrag` | `gml_Script_s_InventoryDrag` (the held-item record's constructor) |
| `GetItemOwnerFromStackOpLocation` | `gml_Script_GetItemOwnerFromStackOpLocation` |
| `GetInventorySlotType` | `gml_Script_GetInventorySlotType` |
| `GetItemFingerprint` | `gml_Script_GetItemFingerprint` |
| `GetItemFromFingerprint` | `gml_Script_GetItemFromFingerprint` |
| `InventoryUpdateExt` | `gml_Script_InventoryUpdateExt` |
| `InventoryUpdateExtNoQue` | `gml_Script_InventoryUpdateExtNoQue` |
| `InvGridEquipV2` | `gml_Script_InvGridEquipV2` |

*Materials tab and command-arg pickup:*

| Probe label | Runtime name |
|---|---|
| `UiAInventoryMaterialTabClick` | `gml_Script_UiAInventoryMaterialTabClick` |
| `UiDrawInventoryMaterialTab` | `gml_Script_UiDrawInventoryMaterialTab` (hot) |
| `CA_playerItemPickup` | `gml_Script_CA_playerItemPickup` |
| `CA_playerItemPickupAccept` | `gml_Script_CA_playerItemPickupAccept` |
| `CA_playerItemDrop` | `gml_Script_CA_playerItemDrop` |

*The bag window's (`UI_Inventory_obj`) Create-event closures.* `UI_Inventory_obj` joined
the object list of `test_target_table_covers_every_sdk_closure_of_the_ui_objects`, so the
next SDK regeneration that renumbers these fails that test by name:

| Probe label | Runtime name |
|---|---|
| `UI_Inventory_obj anon@495` | `gml_Script_anon@495@gml_Object_UI_Inventory_obj_Create_0` |
| `UI_Inventory_obj anon@2261` | `gml_Script_anon@2261@gml_Object_UI_Inventory_obj_Create_0` |
| `UI_Inventory_obj anon@2364` | `gml_Script_anon@2364@gml_Object_UI_Inventory_obj_Create_0` |
| `UI_Inventory_obj anon@4391` | `gml_Script_anon@4391@gml_Object_UI_Inventory_obj_Create_0` |
| `UI_Inventory_obj anon@5590` | `gml_Script_anon@5590@gml_Object_UI_Inventory_obj_Create_0` |
| `UI_Inventory_obj anon@7874` | `gml_Script_anon@7874@gml_Object_UI_Inventory_obj_Create_0` |
| `UI_Inventory_obj anon@14458` | `gml_Script_anon@14458@gml_Object_UI_Inventory_obj_Create_0` |

**Negative results, sourced.** No script name in the SDK contains `Quick`, `ShiftClick`,
`Transfer`, `TakeAll`, `LootAll`, `MoveAll`, `SendTo`, `ToInventory`, `ToBag` or
`ToStash` (case-insensitive). `StashTakeItemOnline`/`StashAddItemOnline` exist but are the
*online* stash routes and are not rows. `UI_Inventory_Drag_obj` has no Create-event closure
in the SDK (grep of `scripts.hpp`); the held item is `s_InventoryDrag`. `hs-game-sdk` has
no item-class constant in any binding (a grep of `hs-game-sdk/python/hs_game_sdk` and
`player.hpp` for "material" finds sounds and sprites only). No ForgePact research doc has
measured an inventory-add operation; the pet quest collector invoked `m_Questpickup` on a
ground item, not an inventory move. These are "not found by name", not "does not exist":
a quick-move the game offers under another name is what M3's right-click and shift-click
test looks for.

**Material identity, the expected signal.** The documented item-class table is
`HSCraftSim/RESEARCH.md` § 2 ("Item types (= catalog `cls`)": 14 material, 15 socketable,
16 relic, …), and the item instance struct carries an `itemType` field (same section). So a
material is expected to be the cell's item instance with `itemType == 14`. Unverified:
which member of a `nodeGrid` cell struct holds the item instance (only `nodeFingerprint`
has been read) and that the field is spelled `itemType` there - `M-cell` and `M-identity`
record both. The `-14` ending of a material's `nodeFingerprint` is a lead only, never the
check: it is a field a material happens to carry, not what makes it a material.

### Stage C hypotheses

The positive control (M2/M3) decides between these, and `move` can express each:

- **H-A** the bag grid node's own `m_MoveItemToGrid` (`anon@15345`) with `self` = the bag
  node and `other` = the ProspectGrid, with the arguments the game passed. A click-in was
  `argc=0`, so the item may come from the held-item state (`s_InventoryDrag`) rather than
  an argument; if so, a by-name invoke with nothing held is recorded as it is, expected to
  refuse or do nothing. Evidence: `anon@15345` logged on the bag node's `self` during the
  hand move, with its `other` and `argc`.
- **H-B** `m_DropItem` (`anon@8881`) on the bag node, the drop half of a drag. Evidence:
  `anon@8881` logged on the bag node during M3's drag.
- **H-C** a named inventory script those closures call, handed the item instance and a
  destination (`InventoryGridAddItem`, `AddToInventory`, `InventoryGridAddToStack`,
  `InventorySwapItemsNew`, or `GridRemoveItem` + `InvGridClearItemNode`) - the shape a
  player build can produce from values read off the cell and the node. Evidence: which of
  those rows fired during the hand move, and each logged argument's kind and value.
- **H-D** a quick-move the game itself offers (a right-click or shift-click, keyed in
  `ProcessInventoryGridInput`). If the human finds such a gesture, it is the preferred
  control. Evidence: the gesture moves a material by eye, and which rows fired.

Every hypothesis also needs: the ProspectGrid's `contents` before and after, the bag grid's
filled count (or the stack count) before and after, and `CheckPlayerInteraction` non-zero
with `anon@15345` counting in the same session - otherwise the instrument is not seeing
calls and every row is `not observed`.

### Stage C instrument (research build only, under `prospectprobe`)

- **`grids`** (hook-free): every `UI_Inventory_Grid_obj` instance (object index by the SDK
  name), as `bag:<k>` in `instance_find` order - its `@id`, its `uiNodeCallstack` printed
  in full, `nodeGridWidth`/`nodeGridHeight`, `filled=`/`empty=` by `contents`'s rule, the
  ids in `masterUi`/`parent`, and each `m_*` method with the closure it resolves to. The bag
  grid is then named by its callstack, the way the ProspectGrid is by `"ProspectGrid"`.
- **`cell <grid> <row> <col>`** (hook-free, read-only): `<grid>` is `prospect` or
  `bag:<k|text>` (the k-th node `grids` listed, or the one node whose callstack contains
  the text); row and col are indexed as `contents` prints them. It prints the cell's kind;
  for a struct, every member with a shallow value, then each member that is itself a
  struct one level down, so the item instance's type, fingerprint and stack count are
  visible. `empty` and `unreadable, not empty` are never confused. Nothing is written.
- **`move <self> <callable> [arg ...] [other=<sel>] [member=<name>] [bag=<k|text>]
  confirm`**: one invoke per command, by name. `self`/`other` are `prospect`,
  `bag:<k|text>` or `window` (`other` defaults to `self`). The callable is `m_<Method>` (a
  method value read off `self`) or `script:<Name>` (its asset index); either goes to
  `script_execute` through `CallBuiltinEx`, never an address. Arguments are `prospect`,
  `bag:<k|text>`, `window`, `cell:<grid>,<r>,<c>` (the cell struct), `item:<grid>,<r>,<c>`
  (the cell's `member=` member - M-cell's recorded item member; there is no default until
  it is recorded), `n:<number>`, `undef` or `str:<text>`, each re-read at the command.
  `bag=` names the bag grid (else the first `bag:` among self, other and the arguments).
  Every refusal comes before any call and says `no call made`: no `confirm`; a callable
  that is not `m_…`/`script:…`; no bag grid named; a pending `override`/`setat`; no open
  window; no ProspectGrid; an unresolvable selector; a missing variable or one that is not
  a method value; a script name with no asset index; the callable's row not detoured
  (`invoked=` could not be proven - the rule `press` follows); an unreadable grid. The
  outcome line carries `st=`, `(threw)`, `res=`, `invoked=` (the row's count across the
  call), `self=`, `other=` and `args=` (each shallowly expanded); then the prospect
  `contents K->K'` with its fingerprints and `prospect-changed-cells=`, the bag's
  `filled B->B'`, `changed-cells=` and `new-fingerprints=`, and a verdict decided from those
  deltas and `invoked=`, never from `st=` alone. Both grids are digested per cell before and
  after the call: a material cell is a stack, so a shape that takes part of one leaves the
  prospect grid's filled count and fingerprints unchanged and shows only as a changed cell.
  Which member holds the stack count is not recorded until `M-cell`, so any change to a cell
  that was filled counts as the prospect grid losing something:
  - `the prospect grid lost a cell, a fingerprint or stack count but the bag did not gain
    it - POSSIBLE LOSS` - and the bag gained no cell, no fingerprint and no changed cell.
    Decided first, whatever else happened;
  - `moved (…)` - the prospect grid lost it, the bag gained it, and the callable's body
    ran. `moved (partial)` when no whole cell left the prospect grid and only a filled
    cell's digest changed (part of a stack moved); it asks for that cell's stack count by
    `cell`. When the bag gained no new cell or fingerprint, only a changed cell, the verdict
    says so and asks for the stack's count by `cell` (a merge, or a POSSIBLE LOSS);
  - `grids changed but handler not entered (invoked=NO) - not a move by this call`;
  - `handler entered, nothing moved`;
  - `dispatched but handler not entered (invoked=NO)`;
  - `not dispatched`.

### Stage C live procedure

Research DLL, auto-prospect OFF for the whole session (`prospectprobe hook` and
`autoprospect 1` detour the same `anon@15345` address), junk materials only,
`%LOCALAPPDATA%\Hero_Siege` backed up, driven through `tools/ipc.ps1`.

- **M0** build dev, install `plugin_build\BloodPactPlugin_rel.dll`, back up the save.
- **M1** before loading a character: `prospectprobe hook` (rows must print `detoured`; record
  `N detoured, M failed` and any `refused`). Load, open the cube, `prospectprobe grids`
  (→ `M-grids`: every grid, its callstack name, which one is the bag), `contents`,
  `cell prospect <r> <c>` on a material cell, the item's cell and an empty cell (→ `M-cell`,
  `M-identity`). Prospect one junk item by hand first if the grid holds no material.
- **M2, control by click.** `watch on`; `arm budget=40` (all rows). Click a material in the
  ProspectGrid, click it into the bag. `show`, `contents`, `grids`, then `cell bag:<k> <r> <c>`
  on the landed cell (→ `M-control-click`: every row that fired with `self`/`other`/args, the
  two grids' deltas; `CheckPlayerInteraction` and `anon@15345` must be non-zero or every row
  is `not observed`). **If the hand move leaves every node `grids` lists unchanged while the
  material is in the bag by eye, the bag's material container is not a grid node**: `M-grids`
  records that (the materials tab - the `UiDrawInventoryMaterialTab` and
  `UiAInventoryMaterialTabClick` rows - is the likely container, and whichever of them fired
  in this control is the lead), and no M7 verdict about a loss or "nothing moved" is evidence,
  and no shape is recorded as a negative, until an instrument can read that container. M7
  then records outcome lines and by-eye results only.
- **M3, control by drag.** `reset`, `arm budget=40`; drag a material to the bag; same
  records (→ `M-control-drag`). Also try a right-click and a shift-click on a material and
  record whether the game itself quick-moves it (H-D).
- **M4, stacking.** With one stack of a material type already in the bag, move another of the
  same type by hand: merged (stack count up, bag filled unchanged) or a new cell; which stack
  rows fired (→ `M-stack`). This defines "the bag gained it" for the adapter.
- **M5, bag full.** Fill the bag (junk), move a material by hand: refused, and where (which
  has-space row returned what), material still in the grid (→ `M-bagfull`). A hand move that
  *loses* the material stops the session: record it, nothing ships.
- **M6, re-entry.** Across M2 and M3, `anon@15345` calls on the ProspectGrid `self`
  (→ `M-reentry`).
- **M7, shapes.** In the order the controls suggest (H-A, H-B, H-C, then anything else the
  rows showed), `prospectprobe move ... confirm` with every value from a selector (no captured
  value). Record each outcome line, the verdict and the by-eye result (→ `M-shapes`). Repeat
  the qualifying shape once with the bag full (must refuse or leave the material in place).
  A crash: relaunch the same build, record it, continue. If M2 found the bag's container is
  not a grid node, a `POSSIBLE LOSS` or `nothing moved` here is the instrument's blindness,
  not the shape's result: go by the by-eye check and leave the shape open.
- **M8** fill the rows, `stage-c-status: complete`.

**Qualifying rule for `move-shape`:** the shape printed `moved`, with `invoked=yes` on the
row the control identified, every value from a selector a player build can produce by name
(instances found by what they are, cell/item structs read off the node, numbers, `undef`),
the material seen in the bag by eye, and its bag-full repeat left the material in the grid
without a `POSSIBLE LOSS`. Otherwise `move-shape: none`.

If nothing qualifies, Stage C stops there (`move-shape: none`): the next step is a
paraphrased local read of the routine the control identified, then a second batched
research build - never a guessed shape in a player build.

## Stage C results

Filled by the Stage C live session (research build, commit recorded at M0). A row that
could not be measured says `not observed (<why>)`; no row is left empty once
`stage-c-status` is `complete`.

| Row | What fills it | Result |
|---|---|---|
| M-grids | M1's `grids`: every `UI_Inventory_Grid_obj` with its callstack name and size, and which one is the bag (the name the ship adapter finds it by) - or, when M2's hand move changed no listed node while the material is in the bag by eye, that the bag's container is not a grid node | |
| M-cell | M1's `cell prospect` on a material cell: which cell-struct member holds the item instance, and that member's own fields (type, fingerprint, stack count) | |
| M-identity | M1/M2's `cell`: the item-type value on a material cell (expected 14), on the inserted item's cell (expected not 14), and on the bag cell the material landed in (expected 14) | |
| M-control-click | M2: every row that fired on a click-move of a material to the bag, with `self`/`other`/args, the two grids' deltas, and `CheckPlayerInteraction`/`anon@15345` non-zero | |
| M-control-drag | M3: the same for a drag, plus whether a right-click or shift-click quick-moves a material (H-D) | |
| M-stack | M4: a second material of a type already in the bag - merged (stack count up, bag filled unchanged) or a new cell, and which stack rows fired; defines "the bag gained it" | |
| M-bagfull | M5: a hand move into a full bag - refused or not, which has-space row returned what, and the material still in the grid | |
| M-reentry | M6: `anon@15345` calls on the ProspectGrid `self` across M2 and M3 | |
| M-shapes | M7: per shape, the outcome line (`st=`, `res=`, `invoked=`, `self=`, `other=`, `args=`), the verdict, the by-eye result, or the crash; and the qualifying shape's bag-full repeat | |
