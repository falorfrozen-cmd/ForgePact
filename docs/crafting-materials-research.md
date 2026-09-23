# Crafting from the stash's special tabs (ForgePact issue #14)

phase0-status: complete
phase1-status: complete
phase1b-status: complete
phase1c-status: complete

**Status: Phase 0 done (static search, instrument, decision core); Phase 1 (the
first live session, 2026-09-22) done; Phase 1b (a widened instrument and a
second session, 2026-09-22/23) done; Phase 1c (a reader round, 2026-09-23)
done; the mechanism is not decided.** Phase 1
measured the vanilla baseline, the cube's craft route and a vanilla duplication,
but its instrument reached neither the stash's special-tab container nor the
route a hand move between a special tab and the bag takes (`## Results`).
Phase 1b saw that move route by name in both directions and located both tabs'
window objects, but no reader reproduced a special-tab count, and the window's
container was not found by `instance_exists`/`instance_number` once the window
closed (destroyed or deactivated; not distinguished) (`### Phase 1b results`).
Phase 1c read both special tabs' counts with the stash window open - the game's
own fingerprint lookup resolves a stash cell with `9` as its second argument,
not `0` - but no reader reproduced a special-tab count with the window closed
(`### Phase 1c results`); no hypothesis
is eligible yet (`## Decision gate`). Nothing player-visible changes yet: the
`craftmats` switch exists but nothing is wired to crafting, and the player
build refuses it. A result is only ever recorded as a negative with its
positive control from the same session (repo-root `AGENTS.md`, "Prove the
Instrument Before Trusting a Negative Result").

## Interpretation

The issue reads "Crafting should use player materials from stash (only from
special mats stash tabs)". That is the **game's own crafting**, not a ForgePact
feature: ForgePact has no crafting code (its Custom Forge dresses items; it does
not craft), the panel has no crafting control, and the toolkit's only crafting
tool is HSCraftSim, a standalone simulator. The game's crafting surface in
`hs-game-sdk` is the Crafting Cube - the world object `Craft_Cube_obj`, its
window `UI_Craft_obj`, the recipe rows `UI_Craft_Recipe_List_Item_obj`, the
journal's `UI_Journal_Crafting_obj` and `UI_Button_Journal_Craft_obj` - and the
scripts listed under `## Static search`. (`DefineCraftingCombos` and
`DefineCraftingFuncs` carry no `gml_Script_` prefix in the SDK: they are script
files, not routines anything can hook.)

"Special mats stash tabs" is the shared stash's purchasable **material tab** -
the tab `UiAStashTabMaterialBuy` sells and `UiAStashMaterialTabClick` opens - the
stash counterpart of the bag's materials tab (`UiAInventoryMaterialTabClick`,
`UiDrawInventoryMaterialTab`). Issue #9 measured the bag's materials tab to be a
stack container, not a grid node (`prospect-window-research.md`, § Stage C
results); the stash's material tab is expected to be the same kind of container,
and where either one lives is a Phase 1 question. Issue #9 never read the bag's
materials tab either: no reader it had found the container, so "the bag gained
it" there rests on the game's own `success` answer plus the tab count by eye
(`prospect-window-research.md`, rows `M-grids` and `M-stack`). No reader in this
toolkit has yet been shown to find either tab, which is why `## Results` gives
the readers here a positive control before a miss counts.

**Corrected during Phase 1 (owner, 2026-09-22): both special tabs are in
scope.** "Special mats stash tabs" means the stash's **Socketable** tab (tab
index `-2`) and its **Materials** tab (`-4`, the one `UiAStashTabMaterialBuy`
sells); both hold inputs of Cube recipes. The tab table lives in
`global.defaultStashTabStruct` / `global.stashTabDataStruct` (eight game modes,
each 23 `{name, tab}` entries: the personal tab `0`, shared tabs `1`-`19`,
Socketable `-2`, Materials `-4`, Unique `-5`) - names and indices only, no
contents. The Socketable tab's bag counterpart is the bag's socket tab
(`UiAInventorySocketTabClick`, `UiDrawInventorySocketTab`), a family the Phase 0
table did not carry because the second tab was only named during Phase 1.

**The goal.** With the switch on, a recipe whose inputs sit partly or wholly in
the stash's Socketable or Materials tab counts as craftable at the cube, and the
craft consumes them there, without the player first moving them to the bag.
Ordinary stash grid tabs, the Unique tab (`-5`), the guild stash, the Blood Pact
stash and the inventory's key/tarot/vault tabs are never touched. Off by
default; with the switch off the game behaves exactly as unmodded.

**The owner's decisions (2026-09-22, before Phase 0):**

- **Count and consume.** The recipe counts the stash tab's materials, and the
  craft removes them from the stash tab. The stash write at a moment the game did
  not choose is an accepted risk, to be worded in the guide's Known Limitations
  (item 22) when the mod ships.
- **Bag first, then stash.** Only the shortfall comes from the stash tab.
- **The stash need not be open** - if the stash tab's container is readable while
  the stash window is closed. If Phase 1 shows it is not, the fallback is to
  require the stash window to be open.

**What issue #9 already measured that this reuses** (`prospect-window-research.md`,
§ Stage C results and § Stage C/D ship design): a material is identified by
`itemType == 14` on the struct the game's own `GetItemFromFingerprint(fp, 0)`
returns (`HeroSiege::Items::ItemType::Material`), never by the fingerprint's
text; grid cells hold only a fingerprint, not the item; the game's own
click-move into the bag's materials tab is `InventoryGridCanAddToStack` then
`InventoryGridAddToStack` (which answers `success` and `tabType` -4 for the
materials tab) then `InvGridClearItemNode`, by name through `script_execute`,
and a first-of-type material goes `GetItemPreferredGrid` then `GridAddItem`
instead; `GetProfileInventoryData` must never be invoked blind (it crashed the
game in that work's Phase 0b); and that work's move into the save-backed
inventory at a moment the game did not choose was accepted as a risk (Known
Limitations item 19) - a stash write is the same class and needs the same
explicit acceptance, which the owner gave above. `StashAddToStack` and
`StashGridAddItem` are the stash-side names; nobody has measured them.

## Static search

Over `hs-game-sdk/cpp/include/hs_game_sdk/scripts.hpp` and `objects.hpp`, every
name containing craft, recipe, stash, material, stack, consume, count,
find-inventory, owner or fingerprint (case-insensitive), 2026-09-22. Every row
below is a `craftprobe` table row, spelled in `ModuleMain.cpp` through the
`HeroSiege::Scripts` constant whose value is shown - never retyped - so a row
that prints `not found` live is a finding about the runtime, not a typo.
`test_craft_mats_contract.py` checks that every table row's runtime name is in
this table, and that every SDK closure on the eight objects below is a row.

| Row label | Group | Runtime name (the SDK constant's value) |
|---|---|---|
| GetCraftItemsAvailable | crafting | `gml_Script_GetCraftItemsAvailable` |
| CraftFindRecipeItems | crafting | `gml_Script_CraftFindRecipeItems` |
| ___struct___68@CraftFindRecipeItems | crafting | `gml_Script____struct___68@CraftFindRecipeItems@DefineCraftingFuncs` |
| DoCraftResult | crafting | `gml_Script_DoCraftResult` |
| ___struct___86@DoCraftResult | crafting | `gml_Script____struct___86@DoCraftResult@DefineCraftingFuncs` |
| CraftEditGrid | crafting | `gml_Script_CraftEditGrid` |
| ___struct___87@CraftEditGrid | crafting | `gml_Script____struct___87@CraftEditGrid@DefineCraftingFuncs` |
| CraftEditPlayerInventory | crafting | `gml_Script_CraftEditPlayerInventory` |
| ___struct___88@CraftEditPlayerInventory | crafting | `gml_Script____struct___88@CraftEditPlayerInventory@DefineCraftingFuncs` |
| UiACraftButton | crafting | `gml_Script_UiACraftButton` |
| UiACraftMultiAmountConfirm | crafting | `gml_Script_UiACraftMultiAmountConfirm` |
| GetCraftRecipeName | crafting | `gml_Script_GetCraftRecipeName` |
| s_CraftData | crafting | `gml_Script_s_CraftData` |
| s_CraftItem | crafting | `gml_Script_s_CraftItem` |
| CountInventoryItem | count/find/consume | `gml_Script_CountInventoryItem` |
| FindInventoryItem | count/find/consume | `gml_Script_FindInventoryItem` |
| FindInventoryItemData | count/find/consume | `gml_Script_FindInventoryItemData` |
| FindInventoryItemOperation | count/find/consume | `gml_Script_FindInventoryItemOperation` |
| ___struct___152@FindInventoryItemOperation | count/find/consume | `gml_Script____struct___152@FindInventoryItemOperation@InventoryFuncs` |
| InventoryStackHandler | count/find/consume | `gml_Script_InventoryStackHandler` |
| InventoryStackUpdateAndRemove | count/find/consume | `gml_Script_InventoryStackUpdateAndRemove` |
| ___struct___161@InventoryStackUpdateAndRemove | count/find/consume | `gml_Script____struct___161@InventoryStackUpdateAndRemove@InventoryFuncs` |
| InventoryStackUpdateAndEdit | count/find/consume | `gml_Script_InventoryStackUpdateAndEdit` |
| ___struct___172@InventoryStackUpdateAndEdit | count/find/consume | `gml_Script____struct___172@InventoryStackUpdateAndEdit@InventoryFuncs` |
| s_PendingStackOperation | count/find/consume | `gml_Script_s_PendingStackOperation` |
| GetStackOpLocationFromGridType | count/find/consume | `gml_Script_GetStackOpLocationFromGridType` |
| GetItemOwnerFromStackOpLocation | count/find/consume | `gml_Script_GetItemOwnerFromStackOpLocation` |
| GetItemOwnerStr | count/find/consume | `gml_Script_GetItemOwnerStr` |
| ChangeItemOwner | count/find/consume | `gml_Script_ChangeItemOwner` |
| GetMaxStack | count/find/consume | `gml_Script_GetMaxStack` |
| IsItemTypeStackable | count/find/consume | `gml_Script_IsItemTypeStackable` |
| UiAInventoryConsumeItemConfirm | count/find/consume | `gml_Script_UiAInventoryConsumeItemConfirm` |
| GetItemFromFingerprint | count/find/consume | `gml_Script_GetItemFromFingerprint` |
| ReturnItemTypeFromFingerPrint | count/find/consume | `gml_Script_ReturnItemTypeFromFingerPrint` |
| StashAddToStack | stash | `gml_Script_StashAddToStack` |
| StashGridAddItem | stash | `gml_Script_StashGridAddItem` |
| s_StashTabData | stash | `gml_Script_s_StashTabData` |
| GetStashMaxTabs | stash | `gml_Script_GetStashMaxTabs` |
| LoadStash | stash | `gml_Script_LoadStash` |
| SaveStash | stash | `gml_Script_SaveStash` |
| ___struct___357@SaveStash | stash | `gml_Script____struct___357@SaveStash@SaveStashFunc` |
| ___struct___359@SaveStash | stash | `gml_Script____struct___359@SaveStash@SaveStashFunc` |
| ___struct___361@SaveStash | stash | `gml_Script____struct___361@SaveStash@SaveStashFunc` |
| ___struct___363@SaveStash | stash | `gml_Script____struct___363@SaveStash@SaveStashFunc` |
| ___struct___364@SaveStash | stash | `gml_Script____struct___364@SaveStash@SaveStashFunc` |
| GetStashMapPos | stash | `gml_Script_GetStashMapPos` |
| UiAStashTabClick | stash | `gml_Script_UiAStashTabClick` |
| UiAStashMaterialTabClick | stash | `gml_Script_UiAStashMaterialTabClick` |
| UiAStashTabMaterialBuy | stash | `gml_Script_UiAStashTabMaterialBuy` |
| UiAStashListBtnActivate | stash | `gml_Script_UiAStashListBtnActivate` |
| UiAStashListBtnActivate anon@1820 | stash | `gml_Script_anon@1820@UiAStashListBtnActivate@UiActivateFuncs` |
| UiDrawStashTabBuy | stash | `gml_Script_UiDrawStashTabBuy` |
| UiAInventoryMaterialTabClick | bag materials tab | `gml_Script_UiAInventoryMaterialTabClick` |
| UiDrawInventoryMaterialTab | bag materials tab (draws every frame) | `gml_Script_UiDrawInventoryMaterialTab` |
| InventoryGridAddToStack | bag materials tab | `gml_Script_InventoryGridAddToStack` |
| InventoryGridCanAddToStack | bag materials tab | `gml_Script_InventoryGridCanAddToStack` |
| InvGridClearItemNode | bag materials tab | `gml_Script_InvGridClearItemNode` |
| GetItemPreferredGrid | bag materials tab | `gml_Script_GetItemPreferredGrid` |
| GridAddItem | bag materials tab | `gml_Script_GridAddItem` |
| GetInventoryGridNode | bag materials tab | `gml_Script_GetInventoryGridNode` |
| InventoryGridRemoveItem | bag materials tab | `gml_Script_InventoryGridRemoveItem` |
| GridRemoveItem | bag materials tab | `gml_Script_GridRemoveItem` |
| GetProfileInventoryData | profile getter (capture only) | `gml_Script_GetProfileInventoryData` |
| GetPlayerItemOwner | profile getter (capture only) | `gml_Script_GetPlayerItemOwner` |
| GetInventoryArray | profile getter (capture only) | `gml_Script_GetInventoryArray` |
| GetPlayerProfileObj | profile getter (capture only) | `gml_Script_GetPlayerProfileObj` |
| UI_Craft_obj anon@1834 | closure (cube) | `gml_Script_anon@1834@gml_Object_UI_Craft_obj_Create_0` |
| UI_Craft_obj anon@4988 | closure (cube) | `gml_Script_anon@4988@gml_Object_UI_Craft_obj_Create_0` |
| UI_Craft_obj anon@6782 | closure (cube) | `gml_Script_anon@6782@gml_Object_UI_Craft_obj_Create_0` |
| UI_Craft_obj anon@7914 | closure (cube) | `gml_Script_anon@7914@gml_Object_UI_Craft_obj_Create_0` |
| UI_Journal_Crafting_obj anon@1154 | closure (cube) | `gml_Script_anon@1154@gml_Object_UI_Journal_Crafting_obj_Create_0` |
| UI_Craft_Recipe_List_Item_obj anon@840 | closure (cube) | `gml_Script_anon@840@gml_Object_UI_Craft_Recipe_List_Item_obj_Create_0` |
| Craft_Cube_obj anon@436 | closure (cube) | `gml_Script_anon@436@gml_Object_Craft_Cube_obj_Create_0` |
| UI_Button_Journal_Craft_obj anon@525 | closure (cube) | `gml_Script_anon@525@gml_Object_UI_Button_Journal_Craft_obj_Create_0` |
| UI_Stash_obj anon@1649 | closure (stash) | `gml_Script_anon@1649@gml_Object_UI_Stash_obj_Create_0` |
| UI_Stash_obj anon@2245 | closure (stash) | `gml_Script_anon@2245@gml_Object_UI_Stash_obj_Create_0` |
| UI_Stash_obj anon@2483 | closure (stash) | `gml_Script_anon@2483@gml_Object_UI_Stash_obj_Create_0` |
| UI_Stash_obj anon@3835 | closure (stash) | `gml_Script_anon@3835@gml_Object_UI_Stash_obj_Create_0` |
| UI_Stash_obj anon@4195 | closure (stash) | `gml_Script_anon@4195@gml_Object_UI_Stash_obj_Create_0` |
| UI_Stash_obj anon@4348 | closure (stash) | `gml_Script_anon@4348@gml_Object_UI_Stash_obj_Create_0` |
| UI_Stash_obj anon@5662 | closure (stash) | `gml_Script_anon@5662@gml_Object_UI_Stash_obj_Create_0` |
| UI_Stash_obj anon@6631 | closure (stash) | `gml_Script_anon@6631@gml_Object_UI_Stash_obj_Create_0` |
| UI_Stash_obj anon@7091 | closure (stash) | `gml_Script_anon@7091@gml_Object_UI_Stash_obj_Create_0` |
| UI_Stash_obj anon@7525 | closure (stash) | `gml_Script_anon@7525@gml_Object_UI_Stash_obj_Create_0` |
| UI_Stash_obj anon@8329 | closure (stash) | `gml_Script_anon@8329@gml_Object_UI_Stash_obj_Create_0` |
| UI_Stash_obj anon@8574 | closure (stash) | `gml_Script_anon@8574@gml_Object_UI_Stash_obj_Create_0` |
| UI_Stash_Tab_Bar_Container_obj anon@125 | closure (stash) | `gml_Script_anon@125@gml_Object_UI_Stash_Tab_Bar_Container_obj_Create_0` |
| UI_Stash_Tab_Bar_Container_obj anon@1018 | closure (stash) | `gml_Script_anon@1018@gml_Object_UI_Stash_Tab_Bar_Container_obj_Create_0` |
| UI_Stash_Tab_Bar_Container_obj anon@5497 | closure (stash) | `gml_Script_anon@5497@gml_Object_UI_Stash_Tab_Bar_Container_obj_Create_0` |
| UI_Stash_Tab_Bar_Container_obj anon@5778 | closure (stash) | `gml_Script_anon@5778@gml_Object_UI_Stash_Tab_Bar_Container_obj_Create_0` |
| UI_Stash_Tab_Bar_Container_obj anon@6037 | closure (stash) | `gml_Script_anon@6037@gml_Object_UI_Stash_Tab_Bar_Container_obj_Create_0` |
| UI_Stash_Tab_Bar_Container_obj anon@8477 | closure (stash) | `gml_Script_anon@8477@gml_Object_UI_Stash_Tab_Bar_Container_obj_Create_0` |
| UI_Stash_Tab_Bar_Container_obj anon@9733 | closure (stash) | `gml_Script_anon@9733@gml_Object_UI_Stash_Tab_Bar_Container_obj_Create_0` |
| UI_Stash_Tab_Bar_Container_obj sortXAscending | closure (stash) | `gml_Script_sortXAscending@gml_Object_UI_Stash_Tab_Bar_Container_obj_Create_0` |
| Town_Stash_obj anon@663 | closure (stash) | `gml_Script_anon@663@gml_Object_Town_Stash_obj_Create_0` |
| UI_Button_Stash_Tab_obj anon@503 (Step) | closure (stash) | `gml_Script_anon@503@gml_Object_UI_Button_Stash_Tab_obj_Step_0` |
| CheckPlayerInteraction | control | `gml_Script_CheckPlayerInteraction` |

The Create-event closures are every SDK constant on `UI_Craft_obj` (4),
`UI_Journal_Crafting_obj` (1), `UI_Craft_Recipe_List_Item_obj` (1),
`Craft_Cube_obj` (1), `UI_Button_Journal_Craft_obj` (1), `UI_Stash_obj` (12),
`UI_Stash_Tab_Bar_Container_obj` (8, including `sortXAscending`) and
`Town_Stash_obj` (1) - 29 constants - plus the one Step closure of
`UI_Button_Stash_Tab_obj`.

### Phase 1b additions

Phase 1's hand move between the Socketable tab and the bag fired none of the 97
rows above while the control climbed (`## Results`, `M-move-stash-to-bag`), so
the search was widened (2026-09-22) over the same two SDK files: every name
matching cursor, held, pick-up, place, drag, swap, grid node, slot, stack,
socket, tab, inventory-add or node, plus every Create closure of the grid,
inventory and stash objects. It found 53 scripts and struct methods and 52
closures the table did not carry - 105 rows, 202 in all, hooked by the same one
`craftprobe hook`. Notable among them: the whole socket family, the bag's
counterpart of the Socketable tab (missed in Phase 0 because that tab was only
named during Phase 1); `GridAddToStack` and the `InventoryGridAddItem*` adds;
and `ProcessInventoryGridInput` with the drag family, the grid's own pick-up and
place handling, which nobody had hooked.

The closures are every SDK constant on the Create event of
`UI_Inventory_Grid_obj` (15, including its eight `___struct___` members),
`UI_Grid_obj` (6), `UI_Node_Parent_obj` (2), `UI_Inventory_obj` (7),
`UI_Inventory_Parent_obj` (10), `UI_Stash_Socket_New_obj` (1),
`UI_Stash_Unique_Items_obj` (8), `Load_Inventory_obj` (2) and
`UI_Split_Stack_obj` (1) - 52 constants, which with the 29 above make the 81 the
coverage test's SDK scan must find. The Unique tab's closures are hooked only as
candidates for a code path the special tabs may share; nothing acts on that tab.
`UiDrawInventorySocketTab` draws every frame, like `UiDrawInventoryMaterialTab`.

| Row label | Group | Runtime name (the SDK constant's value) |
|---|---|---|
| ProcessInventoryGridInput | grid input and drag | `gml_Script_ProcessInventoryGridInput` |
| ___struct___305@ProcessInventoryGridInput | grid input and drag | `gml_Script____struct___305@ProcessInventoryGridInput@ProcessInventoryGridInputFunc` |
| ___struct___308@ProcessInventoryGridInput | grid input and drag | `gml_Script____struct___308@ProcessInventoryGridInput@ProcessInventoryGridInputFunc` |
| InvStartDragging | grid input and drag | `gml_Script_InvStartDragging` |
| InvCopyItemDragData | grid input and drag | `gml_Script_InvCopyItemDragData` |
| InvCopyItemToInvDragData | grid input and drag | `gml_Script_InvCopyItemToInvDragData` |
| s_InventoryDrag | grid input and drag | `gml_Script_s_InventoryDrag` |
| ResetDragState@s_InventoryDrag | grid input and drag | `gml_Script_ResetDragState@anon@47432@s_InventoryDrag@DefineStructs` |
| InventorySwapItemsNew | grid input and drag | `gml_Script_InventorySwapItemsNew` |
| InvGridEquipV2 | grid input and drag | `gml_Script_InvGridEquipV2` |
| InvGridEquipGamepad | grid input and drag | `gml_Script_InvGridEquipGamepad` |
| InventoryGridAddItem | adds and stacks | `gml_Script_InventoryGridAddItem` |
| InventoryGridAddItemPos | adds and stacks | `gml_Script_InventoryGridAddItemPos` |
| InventoryGridAddItemToTab | adds and stacks | `gml_Script_InventoryGridAddItemToTab` |
| InventoryGridHasSpace | adds and stacks | `gml_Script_InventoryGridHasSpace` |
| InventoryGridHasSpaceMulti | adds and stacks | `gml_Script_InventoryGridHasSpaceMulti` |
| GridAddToStack | adds and stacks | `gml_Script_GridAddToStack` |
| AddToInventory | adds and stacks | `gml_Script_AddToInventory` |
| ___struct___13@AddToInventory | adds and stacks | `gml_Script____struct___13@AddToInventory@AddToInventoryFunc` |
| IsStackable | adds and stacks | `gml_Script_IsStackable` |
| ItemsAreStackable | adds and stacks | `gml_Script_ItemsAreStackable` |
| UiASplitStack | adds and stacks | `gml_Script_UiASplitStack` |
| UiAInventoryMobileStackSplit | adds and stacks | `gml_Script_UiAInventoryMobileStackSplit` |
| GetItemFingerprint | adds and stacks | `gml_Script_GetItemFingerprint` |
| InventorySocketItem | socket family | `gml_Script_InventorySocketItem` |
| InventorySocketUpdateAndRemove | socket family | `gml_Script_InventorySocketUpdateAndRemove` |
| InventorySocketUpdateAndSubtract | socket family | `gml_Script_InventorySocketUpdateAndSubtract` |
| ___struct___169@InventorySocketUpdateAndSubtract | socket family | `gml_Script____struct___169@InventorySocketUpdateAndSubtract@InventoryFuncs` |
| UiAInventorySocketTabClick | socket family | `gml_Script_UiAInventorySocketTabClick` |
| UiDrawInventorySocketTab | socket family (draws every frame) | `gml_Script_UiDrawInventorySocketTab` |
| s_SocketData | socket family | `gml_Script_s_SocketData` |
| GetItemSocketDataStruct | socket family | `gml_Script_GetItemSocketDataStruct` |
| UiAStashTabSocketableBuy | stash tab buys and types | `gml_Script_UiAStashTabSocketableBuy` |
| UiAStashTabUniqueBuy | stash tab buys and types | `gml_Script_UiAStashTabUniqueBuy` |
| UiAStashTabBuy | stash tab buys and types | `gml_Script_UiAStashTabBuy` |
| UiAStashUniqueItemType | stash tab buys and types | `gml_Script_UiAStashUniqueItemType` |
| UiCreateNode | node and tab plumbing | `gml_Script_UiCreateNode` |
| ___struct___409@UiCreateNode | node and tab plumbing | `gml_Script____struct___409@UiCreateNode@UiFuncs` |
| UiRemoveNode | node and tab plumbing | `gml_Script_UiRemoveNode` |
| UiMoveNode | node and tab plumbing | `gml_Script_UiMoveNode` |
| ValidateInventoryNode | node and tab plumbing | `gml_Script_ValidateInventoryNode` |
| DetectInventoryDuplicateNode | node and tab plumbing | `gml_Script_DetectInventoryDuplicateNode` |
| s_InvNode | node and tab plumbing | `gml_Script_s_InvNode` |
| UiResizeInventoryNodes | node and tab plumbing | `gml_Script_UiResizeInventoryNodes` |
| GetInventoryMaxTabs | node and tab plumbing | `gml_Script_GetInventoryMaxTabs` |
| InventoryResetTabs | node and tab plumbing | `gml_Script_InventoryResetTabs` |
| InventorySortTab | node and tab plumbing | `gml_Script_InventorySortTab` |
| ___struct___187@InventorySortTab | node and tab plumbing | `gml_Script____struct___187@InventorySortTab@InventoryGrid` |
| UiAInventoryTabClick | node and tab plumbing | `gml_Script_UiAInventoryTabClick` |
| InventoryLogAddItem | node and tab plumbing | `gml_Script_InventoryLogAddItem` |
| InventoryUpdateExtAddItemStats | node and tab plumbing | `gml_Script_InventoryUpdateExtAddItemStats` |
| UI_Button_Inventory_Tab_obj anon@403 (Step) | node and tab plumbing | `gml_Script_anon@403@gml_Object_UI_Button_Inventory_Tab_obj_Step_0` |
| Load_Inventory_obj ___struct___448 (Other_62) | node and tab plumbing | `gml_Script____struct___448@gml_Object_Load_Inventory_obj_Other_62` |
| UI_Inventory_Grid_obj anon@2143 | closure (UI_Inventory_Grid_obj) | `gml_Script_anon@2143@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj anon@2971 | closure (UI_Inventory_Grid_obj) | `gml_Script_anon@2971@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj anon@4064 | closure (UI_Inventory_Grid_obj) | `gml_Script_anon@4064@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj ___struct___517@anon@8881 | closure (UI_Inventory_Grid_obj) | `gml_Script____struct___517@anon@8881@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj ___struct___519@anon@8881 | closure (UI_Inventory_Grid_obj) | `gml_Script____struct___519@anon@8881@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj anon@8881 | closure (UI_Inventory_Grid_obj) | `gml_Script_anon@8881@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj ___struct___522@anon@15345 | closure (UI_Inventory_Grid_obj) | `gml_Script____struct___522@anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj ___struct___529@anon@15345 | closure (UI_Inventory_Grid_obj) | `gml_Script____struct___529@anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj ___struct___532@anon@15345 | closure (UI_Inventory_Grid_obj) | `gml_Script____struct___532@anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj ___struct___536@anon@15345 | closure (UI_Inventory_Grid_obj) | `gml_Script____struct___536@anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj ___struct___538@anon@15345 | closure (UI_Inventory_Grid_obj) | `gml_Script____struct___538@anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj ___struct___541@anon@15345 | closure (UI_Inventory_Grid_obj) | `gml_Script____struct___541@anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj anon@15345 | closure (UI_Inventory_Grid_obj) | `gml_Script_anon@15345@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj anon@34555 | closure (UI_Inventory_Grid_obj) | `gml_Script_anon@34555@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Inventory_Grid_obj anon@36159 | closure (UI_Inventory_Grid_obj) | `gml_Script_anon@36159@gml_Object_UI_Inventory_Grid_obj_Create_0` |
| UI_Grid_obj anon@933 | closure (UI_Grid_obj) | `gml_Script_anon@933@gml_Object_UI_Grid_obj_Create_0` |
| UI_Grid_obj anon@1307 | closure (UI_Grid_obj) | `gml_Script_anon@1307@gml_Object_UI_Grid_obj_Create_0` |
| UI_Grid_obj anon@1577 | closure (UI_Grid_obj) | `gml_Script_anon@1577@gml_Object_UI_Grid_obj_Create_0` |
| UI_Grid_obj anon@2086 | closure (UI_Grid_obj) | `gml_Script_anon@2086@gml_Object_UI_Grid_obj_Create_0` |
| UI_Grid_obj anon@2244 | closure (UI_Grid_obj) | `gml_Script_anon@2244@gml_Object_UI_Grid_obj_Create_0` |
| UI_Grid_obj anon@2416 | closure (UI_Grid_obj) | `gml_Script_anon@2416@gml_Object_UI_Grid_obj_Create_0` |
| UI_Node_Parent_obj anon@1577 | closure (UI_Node_Parent_obj) | `gml_Script_anon@1577@gml_Object_UI_Node_Parent_obj_Create_0` |
| UI_Node_Parent_obj anon@1997 | closure (UI_Node_Parent_obj) | `gml_Script_anon@1997@gml_Object_UI_Node_Parent_obj_Create_0` |
| UI_Inventory_obj anon@495 | closure (UI_Inventory_obj) | `gml_Script_anon@495@gml_Object_UI_Inventory_obj_Create_0` |
| UI_Inventory_obj anon@2261 | closure (UI_Inventory_obj) | `gml_Script_anon@2261@gml_Object_UI_Inventory_obj_Create_0` |
| UI_Inventory_obj anon@2364 | closure (UI_Inventory_obj) | `gml_Script_anon@2364@gml_Object_UI_Inventory_obj_Create_0` |
| UI_Inventory_obj anon@4391 | closure (UI_Inventory_obj) | `gml_Script_anon@4391@gml_Object_UI_Inventory_obj_Create_0` |
| UI_Inventory_obj anon@5590 | closure (UI_Inventory_obj) | `gml_Script_anon@5590@gml_Object_UI_Inventory_obj_Create_0` |
| UI_Inventory_obj anon@7874 | closure (UI_Inventory_obj) | `gml_Script_anon@7874@gml_Object_UI_Inventory_obj_Create_0` |
| UI_Inventory_obj anon@14458 | closure (UI_Inventory_obj) | `gml_Script_anon@14458@gml_Object_UI_Inventory_obj_Create_0` |
| UI_Inventory_Parent_obj anon@1621 | closure (UI_Inventory_Parent_obj) | `gml_Script_anon@1621@gml_Object_UI_Inventory_Parent_obj_Create_0` |
| UI_Inventory_Parent_obj anon@4028 | closure (UI_Inventory_Parent_obj) | `gml_Script_anon@4028@gml_Object_UI_Inventory_Parent_obj_Create_0` |
| UI_Inventory_Parent_obj anon@6164 | closure (UI_Inventory_Parent_obj) | `gml_Script_anon@6164@gml_Object_UI_Inventory_Parent_obj_Create_0` |
| UI_Inventory_Parent_obj anon@6272 | closure (UI_Inventory_Parent_obj) | `gml_Script_anon@6272@gml_Object_UI_Inventory_Parent_obj_Create_0` |
| UI_Inventory_Parent_obj anon@6754 | closure (UI_Inventory_Parent_obj) | `gml_Script_anon@6754@gml_Object_UI_Inventory_Parent_obj_Create_0` |
| UI_Inventory_Parent_obj anon@7597 | closure (UI_Inventory_Parent_obj) | `gml_Script_anon@7597@gml_Object_UI_Inventory_Parent_obj_Create_0` |
| UI_Inventory_Parent_obj anon@8615 | closure (UI_Inventory_Parent_obj) | `gml_Script_anon@8615@gml_Object_UI_Inventory_Parent_obj_Create_0` |
| UI_Inventory_Parent_obj anon@11250 | closure (UI_Inventory_Parent_obj) | `gml_Script_anon@11250@gml_Object_UI_Inventory_Parent_obj_Create_0` |
| UI_Inventory_Parent_obj anon@19615 | closure (UI_Inventory_Parent_obj) | `gml_Script_anon@19615@gml_Object_UI_Inventory_Parent_obj_Create_0` |
| UI_Inventory_Parent_obj anon@22055 | closure (UI_Inventory_Parent_obj) | `gml_Script_anon@22055@gml_Object_UI_Inventory_Parent_obj_Create_0` |
| UI_Stash_Socket_New_obj anon@1305 | closure (UI_Stash_Socket_New_obj) | `gml_Script_anon@1305@gml_Object_UI_Stash_Socket_New_obj_Create_0` |
| UI_Stash_Unique_Items_obj anon@1081 | closure (UI_Stash_Unique_Items_obj, candidate only) | `gml_Script_anon@1081@gml_Object_UI_Stash_Unique_Items_obj_Create_0` |
| UI_Stash_Unique_Items_obj anon@3176 | closure (UI_Stash_Unique_Items_obj, candidate only) | `gml_Script_anon@3176@gml_Object_UI_Stash_Unique_Items_obj_Create_0` |
| UI_Stash_Unique_Items_obj anon@4465 | closure (UI_Stash_Unique_Items_obj, candidate only) | `gml_Script_anon@4465@gml_Object_UI_Stash_Unique_Items_obj_Create_0` |
| UI_Stash_Unique_Items_obj anon@4743 | closure (UI_Stash_Unique_Items_obj, candidate only) | `gml_Script_anon@4743@gml_Object_UI_Stash_Unique_Items_obj_Create_0` |
| UI_Stash_Unique_Items_obj anon@5014 | closure (UI_Stash_Unique_Items_obj, candidate only) | `gml_Script_anon@5014@gml_Object_UI_Stash_Unique_Items_obj_Create_0` |
| UI_Stash_Unique_Items_obj anon@5993 | closure (UI_Stash_Unique_Items_obj, candidate only) | `gml_Script_anon@5993@gml_Object_UI_Stash_Unique_Items_obj_Create_0` |
| UI_Stash_Unique_Items_obj anon@11861 | closure (UI_Stash_Unique_Items_obj, candidate only) | `gml_Script_anon@11861@gml_Object_UI_Stash_Unique_Items_obj_Create_0` |
| UI_Stash_Unique_Items_obj anon@15868 | closure (UI_Stash_Unique_Items_obj, candidate only) | `gml_Script_anon@15868@gml_Object_UI_Stash_Unique_Items_obj_Create_0` |
| Load_Inventory_obj anon@752 | closure (Load_Inventory_obj) | `gml_Script_anon@752@gml_Object_Load_Inventory_obj_Create_0` |
| Load_Inventory_obj anon@877 | closure (Load_Inventory_obj) | `gml_Script_anon@877@gml_Object_Load_Inventory_obj_Create_0` |
| UI_Split_Stack_obj anon@1285 | closure (UI_Split_Stack_obj) | `gml_Script_anon@1285@gml_Object_UI_Split_Stack_obj_Create_0` |

**Hypothesis for the Phase 1 silence** (unmeasured): the special tabs are stack
containers whose click-move runs through the socket or stack routes above, or
inside the grid-node closures, rather than through `InventoryGridAddToStack` or
`GridAddItem`. Live 2 (`### Live procedure 1b`) answers it.

### Phase 1c rows

Live 2 saw `LoadStash` run twice during character load, on `Console_Save_obj`,
and return `true` - the stash's contents were written somewhere, not handed
back - and saw neither special tab's window object with the stash closed
(`### Phase 1b results`). Phase 1c looks for that store. The static search
(2026-09-23, the same two SDK files) took every script naming an inventory map,
a map position, a load order or the stash's save constants that the table did
not carry, and every Create closure of the object `LoadStash` ran on
(`Console_Save_obj`, 6) and of `Profile_Manager_obj` (10) - 21 rows, 223 in
all, hooked by the same one `craftprobe hook`. `Town_Stash_obj`'s only Create
closure, `anon@663`, has been a row since Phase 0 (`Town_Stash_obj anon@663`
above) and is armed again in Live 1c rather than added. `Player_obj`'s Create
closures are left out on purpose: player-build mods may already detour them,
and a second `MmCreateHook` on one of those addresses would only add a failed
row. `citrace` also names the ten `Profile_Manager_obj` closures, one more
reason a session runs one instrument.

| Row label | Group | Runtime name (the SDK constant's value) |
|---|---|---|
| GetItemMap | script (by name, an item map; its returns are kept per first argument by `backing`) | `gml_Script_GetItemMap` |
| GetInventoryMapPos | script (by name, a position in an inventory map, beside the existing `GetStashMapPos` row) | `gml_Script_GetInventoryMapPos` |
| SaveInventoryMap | script (by name, the save of an inventory map) | `gml_Script_SaveInventoryMap` |
| LoadInventoryOrderNew | script (by name, the inventory's load order) | `gml_Script_LoadInventoryOrderNew` |
| s_SaveStashConstants | struct constructor (by name, the stash's save constants) | `gml_Script_s_SaveStashConstants` |
| Console_Save_obj anon@1640 | closure (Console_Save_obj, `LoadStash`'s self) | `gml_Script_anon@1640@gml_Object_Console_Save_obj_Create_0` |
| Console_Save_obj anon@1828 | closure (Console_Save_obj) | `gml_Script_anon@1828@gml_Object_Console_Save_obj_Create_0` |
| Console_Save_obj anon@2004 | closure (Console_Save_obj) | `gml_Script_anon@2004@gml_Object_Console_Save_obj_Create_0` |
| Console_Save_obj anon@2587 | closure (Console_Save_obj) | `gml_Script_anon@2587@gml_Object_Console_Save_obj_Create_0` |
| Console_Save_obj anon@3249 | closure (Console_Save_obj) | `gml_Script_anon@3249@gml_Object_Console_Save_obj_Create_0` |
| Console_Save_obj anon@3848 | closure (Console_Save_obj) | `gml_Script_anon@3848@gml_Object_Console_Save_obj_Create_0` |
| Profile_Manager_obj anon@2012 | closure (Profile_Manager_obj) | `gml_Script_anon@2012@gml_Object_Profile_Manager_obj_Create_0` |
| Profile_Manager_obj anon@2520 | closure (Profile_Manager_obj) | `gml_Script_anon@2520@gml_Object_Profile_Manager_obj_Create_0` |
| Profile_Manager_obj anon@3454 | closure (Profile_Manager_obj) | `gml_Script_anon@3454@gml_Object_Profile_Manager_obj_Create_0` |
| Profile_Manager_obj anon@5201 | closure (Profile_Manager_obj) | `gml_Script_anon@5201@gml_Object_Profile_Manager_obj_Create_0` |
| Profile_Manager_obj anon@5349 | closure (Profile_Manager_obj) | `gml_Script_anon@5349@gml_Object_Profile_Manager_obj_Create_0` |
| Profile_Manager_obj anon@5835 | closure (Profile_Manager_obj) | `gml_Script_anon@5835@gml_Object_Profile_Manager_obj_Create_0` |
| Profile_Manager_obj anon@6549 | closure (Profile_Manager_obj) | `gml_Script_anon@6549@gml_Object_Profile_Manager_obj_Create_0` |
| Profile_Manager_obj anon@6879 | closure (Profile_Manager_obj) | `gml_Script_anon@6879@gml_Object_Profile_Manager_obj_Create_0` |
| Profile_Manager_obj anon@8168 | closure (Profile_Manager_obj) | `gml_Script_anon@8168@gml_Object_Profile_Manager_obj_Create_0` |
| Profile_Manager_obj anon@9994 | closure (Profile_Manager_obj) | `gml_Script_anon@9994@gml_Object_Profile_Manager_obj_Create_0` |

Already rows, and armed again rather than added: `GetItemOwnerFromStackOpLocation`,
`GetItemOwnerStr`, `ChangeItemOwner`, `GetStashMapPos`, `LoadStash`, `SaveStash`
and its five structs, `s_StashTabData`, and `Town_Stash_obj anon@663`. The
contract test's closure scan now covers nineteen objects and finds at least 97
constants.

### Negative results, sourced

These are "not found by name" in the SDK tables above, not "does not exist":

- No script name contains `UseStash`, `FromStash`, `StashMaterial` (beyond the
  two UI names above), `CraftFromStash`, `MaterialCount`, `HasMaterial`,
  `ConsumeMaterial` or `Ingredient` (search of `scripts.hpp`, 2026-09-22).
- `ConvertOnlineStash*` and `Stash*Online` are online routes; they are not rows.
- `UI_Button_Stash_Tab_obj` and `UI_Craft_Animation_obj` have no Create
  closure in the SDK (the first has the one Step closure above).
- `objects.hpp` carries no parent table, so the cube's and the stash's UI
  hierarchy is not known statically.
- No research document under `ForgePact/docs/` has measured a craft or a stash
  read (a search of every doc); `docs/RUNTIME_DATA_MODELS.md` lists no material
  or stash container; the HSSaveEditor and hero-siege-item-editor guides mention
  `stash.hss` but no material-tab layout (both submodules are not checked out in
  the worktree this was written in).
- **Object events by their raw name: no name tried has resolved** - which is
  why Phase 1b reaches `UI_Inventory_Grid_obj`'s Step and mouse events only
  through the scripts and closures they call, and why its list is broad. 22
  `gml_Object_<Obj>_<Event>_<n>` names, `Player_obj_Step_0` among them, returned
  "not found" from `GetNamedRoutinePointer` at hook-install time
  (`pet-quest-collector-research.md`, session 7, also cited by
  `toggle-skills-research.md`), and YYToolkit's per-event callback is disabled on
  this build (repo-root `AGENTS.md`, "Don't Suspend the Game's Own Runtime").
  Phase 1b does not try the route again.
- Phase 1b's widened search (2026-09-22): no script name contains `Held`,
  `Grab`, `MouseItem`, `HandItem` or `CarryItem`; the `Cursor` names are options
  and gamepad handling, and the `Pickup` names are world loot, so neither family
  is a row.

## Baseline (vanilla) to measure

**B0 - does the unmodded game already craft from the stash tab?** Nobody has
measured it. With the research DLL loaded but **before** `craftprobe hook` (the
detours only count, but a vanilla check should not depend on that), put one
material only in the stash's material tab and none in the bag, open the cube and
select a recipe that needs it: does the recipe count as craftable, and does
pressing craft craft it? Repeat once with the stash window open beside the cube.
If the game already does this, the feature is narrower than the issue assumes (or
already the game's), and this document says so before anything is built.

## Hypotheses

The owner chooses one at `## Decision gate` from the evidence Phase 1 records.
Rejected up front, whatever Phase 1 shows: reading or writing `stash.hss` as the
mechanism (the game holds the file while running, and ForgePact's model is
in-memory only - Phase 1b's hub tool `tools/stash_tab_counts.py` reads the file
only after the game has exited, as a count cross-check, and never writes it); a
struct-layout read of the stash container (the quiet form of a hand-resolved
address - repo-root `AGENTS.md`); simulating clicks; any per-frame scan of the
stash.

- **H-A - count and consume through a hook.** A routine (`GetCraftItemsAvailable`
  or `CraftFindRecipeItems`) computes a recipe's availability from a readable
  container, and `CraftEditPlayerInventory` or `InventoryStackUpdateAndRemove`
  consumes. The mod adds the stash tab's count to the first and draws the
  shortfall from the stash stack through the game's own stack routine by name.
  **Decided by:** those rows fire on a hand craft (`M-craft`) with argument
  shapes a hook can extend without reading a struct layout, and the stash tab's
  container is readable with the stash window closed (`M-stash-closed`, which
  counts as a "no" only once its control has passed - see `## Results`).
- **H-B - pull on demand.** At the craft press (or on recipe selection) the mod
  moves the shortfall from the stash tab to the bag's materials tab through the
  game's own click-move route, by name - whichever of the `StashAddToStack` /
  `InventoryGridAddToStack` family the hand move fires - and then lets the game
  count and consume unchanged. One game operation, the risk class of issue #9's
  Stage C, no crafting hook. **Decided by:** the hand move stash-to-bag
  (`M-move-stash-to-bag`) fires a by-name shape that answers `success`, and the
  bag tab's cap (`GetMaxStack`) is not hit.
- **H-C - count only.** An availability hook only; consume stays vanilla, so the
  craft fails at the press unless it also pulls (H-B). A fallback if consume is
  unreachable. **Decided by:** H-A's availability half holds and its consume half
  does not.

Whichever wins, the arithmetic is the same and is already built and tested
without the game: `plugin/include/ForgePact/CraftMatsMod.hpp` (per material:
need N, bag count k, stash-tab count s; take min(N-k, s) from the stash tab only
when the switch is on and k < N; an unreadable count refuses the whole press and
is named once; a tab that changed without the game's success answer, or a
success the re-read cannot confirm, turns the mod off for the session), pinned by
`tests/test_craft_mats_behavior.py` + `tests/craft_mats_harness.cpp`.

## Instrument

`craftprobe` (research build only; not in `kPlayerCommands`, so the player build
answers `command unavailable`; dispatched from `HandleCraftCommand`). Every
table row above is native-detoured by one `craftprobe hook`: resolved by name
through `GetNamedRoutinePointer`, refused unless the function is executable
code inside `Hero_Siege.exe`, then `MmCreateHook` at the function's own address
- the attach route `prospectprobe` uses, because a table-only hook is blind to
this build's direct calls (guide, Known Limitations item 12).

| Subcommand | What it does | Writes? |
|---|---|---|
| `hook [substr ...]` | detours every row (or the matching ones); prints `N detoured, F failed` and one line per failure | no |
| `arm [budget=N] [substr ...]` | zeroes the counters; logs the next N calls of each selected row (default N=6; default rows: all but the control) - per call `self`, `other`, `argc`, every argument, then a `ret=` line | no |
| `show [all]` | the control's count first, then per called row `calls=`, `logged=`, `unlogged=`; `all` adds silent rows and `calls=n/a` for rows not detoured | no |
| `reset` | zeroes every counter | no |
| `bag [substr ...]` | hook-free: every matching variable of the bag window (`UI_Inventory_obj`), shallowly - kind, length, the first entries' members | no |
| `stash [substr ...]` | hook-free: the same for the stash window (`UI_Stash_obj`), the world stash (`Town_Stash_obj`) and the globals; run with the stash open **and** closed | no |
| `recipe [substr ...]` | hook-free: the same for the cube window (`UI_Craft_obj`) | no |
| `var <Obj or global> <nth> <name> [json]` | hook-free: one variable, deeper; `json` writes `bp_ipc\cp_var_<name>.json` when a depth-capped walk finds no cycle | no |
| `backing on [substr ...]`, `off`, `dump`, `clear` | keeps the latest value the game's **own** call of a row returned (default rows: the four profile getters, `s_StashTabData`, `LoadStash`, `GetStashMaxTabs`, `GetCraftItemsAvailable`, `CraftFindRecipeItems`, `CountInventoryItem`, `FindInventoryItemData`), rooted in a research global; `dump` writes `bp_ipc\cp_backing_<row>.json` | no - nothing is invoked |
| `dump` | `bp_ipc\craftprobe_rows.json` (every row's counters) and `backing dump` | no |
| `call <Row> <Obj> <nth> [args ...] confirm` | exactly one by-name call (`asset_get_index` + `script_execute`) of one plain-script row, self = other = that instance; an argument is a number, `true`/`false`, text, `fp:<fingerprint>` (the item the game's own lookup returns) or `kept:<row>` (a kept return). Refuses before the call without `confirm`, for a closure, struct method or profile getter, an instance it cannot resolve, or an argument it cannot resolve; prints what was supplied, the instance either side and what came back | **yes - one call** |

`CheckPlayerInteraction` is the positive control in the same table: it fires from
every interactable's Step event, so a 0 there voids every other row's count.
`UiDrawInventoryMaterialTab` draws every frame: leave it at the default budget or
out of `arm`'s filters, and read `show`'s `unlogged=` for it. Several rows share
an address with `prospectprobe`'s table; a second `MmCreateHook` on an address
already hooked fails for that row, so a session runs **one** instrument - no
`prospectprobe hook`, `citrace nativetrace` or `autoprospect 1` alongside
`craftprobe hook` (`hook` says so when `prospectprobe` already holds rows).

`craftmats 1|0` sets the decision core's switch and nothing else in Phase 0: no
hook, no call, nothing on the frame path; `craftmats 1` answers that crafting is
unchanged. `craftmats stat` (research build) prints the core's stat line.

### Phase 1b additions

Phase 1's instrument could reach neither the special-tab container (its `var`
printed `ref instance N` and stopped) nor the hand move's route (none of its 97
rows fired). The Phase 1b build keeps every subcommand and refusal above as it
was and adds the following. None of it writes game state; the one write is
still `call ... confirm`.

| Addition | What it prints | Cap | Control |
|---|---|---|---|
| 105 new rows (`### Phase 1b additions` under `## Static search`) | the same `hook`/`arm`/`show` lines; `hook` answers `202 detoured, 0 failed` on a clean session | one detour per row | `CheckPlayerInteraction`, as before; `UiDrawInventorySocketTab` draws every frame, so keep it out of wide `arm` filters or read its `unlogged=` |
| Marker: a bare `craftprobe` | the usage, whose first line is `craftprobe: phase1b rows=202 - ...` | - | it is the control: without `phase1b` the installed plugin is not this build, and nothing from the session counts |
| `var` follows references | `var <Obj> <nth> <name>` as before; a value that is a live `ref instance N` is then followed: `object_get_name` of its `object_index`, its id and every variable, and each of those that is itself a live instance reference one level further. `var id:<n> <name>` (or `*`) starts from an instance number; `<a.b.c>` walks a dotted path through instances and plain structs; `*` lists every variable of the root and follows its references. A `ref ds_grid`, `ref ds_list` or `ref ds_map` prints its size and first entries | depth 2 below the root; 12 distinct instances per command (a visited set names an instance already printed instead of reading it again); 80 variables per instance; 12 entries per data structure | an instance is read only after `instance_exists` answers true, and a data structure only after `ds_exists` with its own type answers true (a `ds_grid_*` call on the wrong kind is a fatal game error - `prospect-window-research.md` R5b); a reference is recognised from the runtime's own text for it, never from its bytes; a method value is described (`method_get_index` + `script_get_name`) and never called |
| `node id:<n>`, `node <Obj> <nth>`, `node stash`, `node bag` | for each instance: `nodeGridWidth`/`nodeGridHeight`, filled and empty cells, the distinct `nodeFingerprint` values; per fingerprint the item the game's own `GetItemFromFingerprint(fp, 0)` returns - its `itemType`, its `itemDefinitionStruct.b`, the fingerprint's class suffix, and every numeric member of the item, its definition struct or the cell that may be the stack count - named exactly `o` (the save's stack key: the item struct carries the save's short keys, as its `b` shows, and `tools/stash_tab_counts.py` sums `data.o`) or containing `stack`, `amount`, `count` or `qty`; when the definition struct has none of those, all its numeric members are printed instead (24 at most), so an unexpected name shows up rather than silence - then a `sum class=<c> b=<b>` line per pair with each candidate member summed. A lookup that answers nothing prints what was supplied (`returned no struct (self=<object> id=<n>, a1=0)`), so a miss reads "not resolved with that self and `a1=0`", never "not resolvable". `stash` reads every live instance reference of the open stash window (`stashGrid`, `uiStashContainer`, `invMaterialTab`, ...); `bag` reads every `UI_Inventory_Grid_obj` with its `uiNodeCallstack`. An instance without `nodeGrid` prints its variable names | 64 lookups per command (a fingerprint past the cap is summed with `b=?`); 12 instances; 40 fingerprint lines per instance; 24 definition members | `node bag` is the known-good read, for both halves of the comparison: it passes only when a `sum` line carries a member whose value equals a bag stack of two or more that the owner states by eye (or the `CountInventoryItem` answer for that item), which proves the cell read and names the count member. A special-tab sum counts only in that member, and a special-tab miss through an instance grid counts only once this control has passed in the same session (Phase 1c's `node var` paths each have their own control: `### Phase 1c readers`). `tools/stash_tab_counts.py` is the save-side cross-check |
| `backing` per first argument | for `GetInventoryArray` and `CountInventoryItem`, besides the latest return, the latest return per distinct first argument (`1`, `-2`, `-4`, ...): one line when a new signature is first seen, and `backing dump` writes `cp_backing_<row>_arg<k>.json` per signature plus a count line | 8 signatures per row; later new signatures are counted, not kept | the getters are still never invoked; only the game's own calls are kept |

The hub tool `tools/stash_tab_counts.py` (repo-root `docs/tools/stash-tab-counts.md`)
is the save-side cross-check: after the game has exited it decodes
`hs2saves\stash.hss` read-only with `hero-siege-item-editor`'s own decoder and
prints, per `socket_tab*` and `material_tab*` key, the entry count and the summed
stack per (class, b). It runs outside the game and the plugin, and never writes.

### Phase 1c readers

**Why `node-bag-control` failed in Live 2.** The reader read the grid it was
pointed at correctly; the stack the owner named was not in that grid, and the
lookup cap could not have covered the grid anyway. From the capture
(`.claude/workorders/forgepact-issue-14-phase1b-live-2.md`, cited by its step
headings and not committed):

- Step 3, "check `node-bag-control`": the owner named 173 Tor, sitting on the
  bag's Socketable tab. `node bag` read the bag's `InventoryGrid`
  (`UI_Inventory_Grid_obj` id 262489, 15x6, 73 filled cells), which held
  classes 13 and 14 only - the bag's Main tab, the one on show. No class-15 cell
  was in that read, so no sum could equal 173.
- The same step: 73 fingerprints against a cap of 64 lookups left 9 summed as
  `b=?` on every repeat, so even the right tab could have left the named stack
  unresolved.
- Step 5b, "craftprobe node bag - class=15 b=1": with the bag's Socketable tab
  on show, the same reader on the bag's `InventoryGrid` (id 262423, 21 filled
  cells) printed `sum class=15 b=1 ... def.o=2`, equal to the 2 Ol the owner
  counted - the control's shape, met on another tab. The bag's `InventoryGrid`
  held whichever bag tab was on show in both reads (Main: classes 13/14;
  Socketable: class 15). That is two observations, not an established rule.

So Live 1c asks the owner for a stack **and the bag tab it sits on**, has that
tab on show at the read, and `node` can spend its lookups on the named class.

**What the Phase 1c build adds.** All of it inside the existing research-only
`craftprobe` block; every subcommand, cap and refusal above stays; names come
through `HeroSiege::Scripts`/`HeroSiege::Objects` constants; nothing reads a
struct layout; nothing writes but `call ... confirm`; and the one game script a
reader calls is still the fingerprint lookup, now through a research-only
helper beside the player build's own that takes the self and the second
argument.

| Addition | What it prints | Cap | Control |
|---|---|---|---|
| 21 new rows (`### Phase 1c rows` under `## Static search`) | the same `hook`/`arm`/`show` lines; `hook` answers `223 detoured, 0 failed` on a clean session | one detour per row | `CheckPlayerInteraction`, as before |
| Marker: a bare `craftprobe` | the usage, whose first line is `craftprobe: phase1c rows=223 - ...` | - | it is the control: without `phase1c` the installed plugin is not this build, and nothing from the session counts |
| `backing` per argument, keyed per row | `GetItemFromFingerprint` keyed on its **second** argument (`a1`), `GetItemMap` on its first, `GetInventoryArray` and `CountInventoryItem` on their first as before. Per signature: a line the first time it is seen (with its self), and in `backing dump` one line each - `a<k>=<signature> #<call> calls=<n> a0=<latest first argument> selves=<objects>` - plus one `cp_backing_<row>_arg<k>.json` file. `GetItemFromFingerprint` and `GetItemMap` join the default `backing on` rows. The instrument's own lookups (made by `node`) are neither logged nor kept, so every signature is a call the game made | 8 signatures per row; a0 cut at 96 characters (a fingerprint keeps its class suffix); 6 distinct self objects per signature | nothing is invoked. The lookup runs whenever the bag or the stash draws (19193 calls before the first stash open in Live 2), so the game's own call shape for a stash cell - which `a1`, which self, which class in `a0` - is captured without a hand move. Keeping it at that rate may stutter; `backing off` stops the keeping |
| `node` options `a1=<v>`, `self=id:<n>`, `class=<c>` (any order after the selector) | the lookup shape first (`lookups are GetItemFromFingerprint(fp, <a1>) with self=<...>`); then as before. `a1=` is the lookup's second argument (default 0; a number, `true`/`false`, `undefined` or text); `self=id:` its self (default: the grid each fingerprint was read from), used only after `instance_exists` answers true and the id resolves by name to an instance; `class=` makes lookups only for fingerprints with that class suffix and sums the rest as `b=?`. A miss prints the self and `a1` actually supplied: `returned no struct (self=<object> id=<n>, a1=<a1>)` | 160 lookups per command (a whole bag grid is 90 cells, the Socketable tab 140) | `node bag` on the tab the owner names is still the known-good read (`node-bag-control`). A special-tab miss counts only once the control for the path it took passed in the same session: through an instance grid (`node bag`, `node id:`, `node socket`, `node stash`, or a `node var` path that lands on a live instance), `node-bag-control`; through `node var` over an array, `ds_list` or `ds_map` of fingerprint strings, `node-var-control`; through `node var` over item structs read directly, no control yet, so such a miss is "not observed (uncontrolled path)" |
| `node socket [id:<n>]` | the Socketable tab is not one grid: its window, `UI_Stash_Socket_New_obj` (the first live one, or `id:<n>`), holds `grid`, an array of one `UI_Inventory_Grid_obj` instance per cell (Live 2, step 3). This walks that array (and one level of row arrays), reads each live cell instance as a grid, and prints one summary line (entries, cell instances read, gone, without `nodeGrid`, not an instance; filled/empty; distinct fingerprints) and **one** `sum class=<c> b=<b>` table across all cells | 400 cell instances; 160 lookups | each cell is read only after `instance_exists`, once (a visited set), and no reference below it is followed |
| `node var <Obj or global> <nth> <a.b.c>`, `node var id:<n> <a.b.c>` | sums whatever the `var` path reaches (the same root and walk as `var`): a live instance with `nodeGrid` is read as `node id:`; an array, a `ds_list`, a `ds_map` or a struct is read entry by entry - a fingerprint string or a cell carrying `nodeFingerprint` gets one lookup (default self: the instance the path's last name was read from; a global path has none, so `self=id:` gives one), and an item struct carrying `itemDefinitionStruct` is read directly, no lookup: its `b` and its stack members (`def.o`), its class from its key's suffix when the key is a fingerprint, else its `itemType`. Same fingerprint, item and `sum` lines | 1000 entries; 160 lookups; 40 fingerprint/item lines | a `ds_list` or `ds_map` is read only after `ds_exists` with its own type answers true; anything else is counted as `other`, never guessed at. The fingerprint-entry path's control is `node-var-control` (`node var` over the kept `GetInventoryArray` array, self the bag grid, equals `node bag` on the same tab); a path landing on a live instance rests on `node-bag-control`; the item-struct path has no control yet |
| `store [substr ...]` | hook-free: for the first live instance of each of `Console_Save_obj`, `Profile_Manager_obj`, `Town_Stash_obj`, `Player_obj`, `New_Inventory_Data_obj`, `Inventory_Loading_obj` and `Load_Inventory_obj`, for the instance the kept `GetProfileInventoryData` return refers to (if kept and alive), and for the globals: every variable whose name matches the filters (default `stash socket material item map inv tab`), one line each with its shape - kind, array length, struct member count, a data structure's size and first entries, or `ref instance N` with its object's name - and its value shallowly | 80 variables per holder (the line says when more matched) | a holder is read only after `instance_number`/`instance_exists`; a `ds_*` only after `ds_exists`; the instrument's own `__cp_*` research globals are skipped and counted, never reported as a finding |
| `store names [substr ...]` | the matching names only, ten per line | 400 names per holder, so a search is never cut at 80 (Phase 1's globals listing was) | as `store` |

**Destroyed or deactivated.** `instance_exists` and `instance_number` answer
false and 0 for a deactivated instance too, so Live 2's `stash-closed` cannot
say "destroyed". A read-only way to separate the two: note the Socketable
window's id with the stash open, close it, reopen it, and read the new window's
id and `var id:<old id>` - the old id alive again means the window was kept, a
dead old id with a new one means a new window was made; the old one destroyed or
still deactivated (not distinguished). Either answer leaves the
closed-window store as the thing to find, since a deactivated window is not
readable by the plugin without activating it, which would be a write into the
game's loop and is not planned.

## Live procedure

Owner-run; the agent drives the command channel (`hs-drive`) and reads
`out.txt` after each step; the human opens the cube and the stash and presses
craft. Research DLL only, a fresh launch, one instrument.

1. **Back up the saves first** (`hs_saves_backup`, or a copy of
   `%LOCALAPPDATA%\Hero_Siege`), then `hs_selfcheck`, `hs_launch`,
   `hs_select_character` to town.
2. **B0 (vanilla, before any hook).** One material only in the stash tab, none in
   the bag; the cube, a recipe that needs it: craftable? craft? Again with the
   stash window open beside the cube. Fill `B0-vanilla`.
3. **Hook-free readers first.** `craftprobe bag`, `craftprobe stash` (stash
   closed), open the stash, `craftprobe stash` again, open the cube and select a
   recipe, `craftprobe recipe`. Fill `M-bag`, `M-stash-open`, `M-stash-closed`,
   `M-recipe`. Follow any promising variable with `craftprobe var ...`. Before
   either tab is written up as unreadable, note the tab counts by eye: a reader
   has only found a tab when the container it names holds the counts the human
   sees (the control for `M-bag` and `M-stash-closed`, below `## Results`).
4. `craftprobe hook`; `craftprobe arm budget=200` (the draw row stays near the
   default: narrow `arm` with substrings if it floods). Walk past anything
   interactable, then `craftprobe show`: `C-control` needs a non-zero
   `CheckPlayerInteraction` count - without it, stop: nothing below counts.
5. **Hand craft, material in the bag.** One recipe whose input is in the bag's
   materials tab; press craft once; `craftprobe show`. Fill `M-craft` with every
   row that fired, its self/other/arguments/return, and the bag tab's count
   change by eye.
6. **Hand moves.** Click-move one material stash tab -> bag, then bag -> stash
   tab; `craftprobe show` after each. Fill `M-move-stash-to-bag`,
   `M-move-bag-to-stash` (the rows, their `success` answers, the counts by eye).
7. **Open/close.** Close and reopen the stash and the cube with `bag`/`stash`
   between; `craftprobe backing on`, reopen both windows, `craftprobe dump`, then
   `craftprobe show all`. Fill `M-backing`.
8. Fill each hypothesis row from the rows above, stop the game, and restore the
   saves if anything unexpected was written.

### Live procedure 1b

Live 2 runs the Phase 1b build under `live-operator`, with the owner at the
keyboard for the hand moves; the step-by-step procedure (preconditions, the
exact commands, the six hand-backs) is `### Live procedure 2` in the workorder's
context file, `.claude/workorders/forgepact-issue-14-phase1b-context.md`, which
stays on the owner's machine. What this document fixes is its shape:

- **Before anything counts**: the installed plugin's SHA-256 equals the one under
  `### Phase 1b results` (another session may have swapped the DLL), a bare
  `craftprobe` answers `phase1b rows=202`, and `CheckPlayerInteraction` is
  non-zero once the character is loaded. The ForgePact panel's Auto-prospect is
  off for the session (its `autoprospect 1` would hold an address this table
  shares), the saves are backed up first, and no craft is pressed (the craft
  route is measured; the duplication is not to be widened).
- **Cases**: the Socketable tab (the ordinary case), the Materials tab (the
  outlier: a second tab and a different draw family), one whole-stack pick-up
  and put-back, and a one-unit move each way. The owner also states one bag
  stack of two or more by eye: `node-bag-control` passes only when a `node bag`
  sum equals it, which names the member the special-tab sums are read in.
- **The capture** is `.claude/workorders/forgepact-issue-14-phase1b-live-2.md`,
  ending with a `## Checks` section of one line per check, exactly
  `- <name> | expected: <text> | observed: <quoted, short> | pass|fail|not-observed`,
  for these checks: `dll-hash`, `marker`, `control`, `node-bag-control`,
  `counts-tool-before` (the instrument's controls: without `dll-hash`, `marker`
  and `control` nothing else in the file counts; the hook rows - the moves,
  `load-capture`, `backing-per-arg` - rest on those three and
  not on `node-bag-control`, which with `counts-tool-before` gates only the
  reader rows), then `stash-open-socket`,
  `stash-open-material`, `move-socket-to-bag`, `move-bag-to-socket`,
  `move-material-to-bag`, `stash-closed`, `load-capture`, `backing-per-arg` and
  `counts-tool-after`. A `fail` or `not-observed` there is a finding, recorded
  under `### Phase 1b results` with what the reader printed; it is never
  rewritten as "not readable" unless its control passed (rules (a)-(c) under
  `## Results`).

### Live procedure 1c

Live 1c runs the Phase 1c build under `live-operator`; the step-by-step
procedure is `### Live procedure 1` in the workorder's context file,
`.claude/workorders/forgepact-issue-14-phase1c-context.md`, which stays on the
owner's machine. It is a reader round: **no hand moves between tabs and the
bag, and no craft**. What this document fixes is its shape:

- **Controls first**: the installed plugin's SHA-256 equals the one under
  `### Phase 1c results`, a bare `craftprobe` answers `phase1c rows=`, and
  `CheckPlayerInteraction` is non-zero once the character is loaded; without
  those three nothing else in the file counts. Auto-prospect is off, the saves
  are backed up first, and one instrument runs (`craftprobe` only).
- **The decisive read comes before the stash is ever opened in the launch**:
  `backing dump`, `store`, `store names ...` and `var`/`node var` on each
  candidate, with the stash and the bag closed. A special-tab sum reproduced
  there is the load-time store by construction.
- **Then the hand-backs, reads only**: the bag open on the tab the owner names
  (`node-bag-control`, and the lookup signatures the bag's draw produced), the
  stash open on the Socketable tab (`node socket`, with the new signatures'
  `a1=` and `self=id:` if the plain lookup misses), the Materials tab, a
  close-and-reopen for the window's id, and a last closed read.
- **Cases**: the Socketable tab (the ordinary case) and the Materials tab (the
  outlier: an ordinary grid object, a different draw family).
- **The capture** is `.claude/workorders/forgepact-issue-14-phase1c-live-1.md`,
  ending with a `## Checks` section of one line per check, exactly
  `- <name> | expected: <text> | observed: <quoted, short> | pass|fail|not-observed`,
  for these sixteen checks: `dll-hash`, `marker`, `control`,
  `counts-tool-before`, `load-rows`, `store-candidates`, `store-closed`,
  `node-bag-control`, `node-var-control`, `fp-a1-bag`, `fp-a1-stash`,
  `stash-open-socket`, `stash-open-material`, `reopen-id`,
  `store-closed-after` and `counts-tool-after`. The hook rows (`load-rows`,
  `fp-a1-bag`, `fp-a1-stash`) rest on `dll-hash`, `marker` and `control`. A
  special-tab reader miss counts only once the control for the path it took
  passed in the same session: through an instance grid (`node bag`, `node
  id:`, `node socket`, `node stash`, or a `node var` path that lands on a live
  instance), `node-bag-control`; through `node var` over an array, `ds_list` or
  `ds_map` of fingerprint strings, `node-var-control`; through `node var` over
  item structs read directly, no control yet, so such a miss is recorded "not
  observed (uncontrolled path)". A `fail` or
  `not-observed` is a finding, recorded under `### Phase 1c results` as "not
  observed by <reader> (<what it printed>)"; no live outcome is an acceptance
  criterion of the build.

## Results

Research DLL: `plugin_build\BloodPactPlugin_rel.dll`, built 2026-09-22 with
`plugin_build\build.bat dev` from ForgePact `aa0c72a` (SHA-256
`6c901f742384337785d98d337f69229e4308d7439e8c55896e102e6affa56dad`), against the
toolchain `py tools/fetch_toolchain.py` placed. The owner had it installed as
`mods\aurie\BloodPactPlugin.dll` for the Phase 1 session.

**Phase 1 session, 2026-09-22.** Character slot 13 (Town of Inoya), the owner at
the keyboard and the driver on the command channel, two launches: session 1 with
`craftprobe hook` (B0 first, before any hook), session 2 opening with a
hook-free repeat of the duplication and then hooking again. The saves were backed
up first (an independent copy and an `hs-drive` backup) and restored after each
launch. The capture (`out.txt`, the `cp_*.json` dumps and the session notes)
stays on the owner's machine and is cited below by `out.txt` line; nothing of it
is committed.

| Row | What is measured | Result | Evidence (out.txt lines / by eye) |
|---|---|---|---|
| B0-vanilla | Unmodded: does a recipe whose only input is in the stash tab count as craftable and craft (stash closed; stash open)? | **Not craftable** with the only input in the stash's Socketable tab and none in the bag, stash closed. Stash open beside the cube: not measurable - the game does not let the stash and cube windows be open together. | By eye, session 1, before `craftprobe hook` |
| C-control | `CheckPlayerInteraction` count in the same session as every row below (must be non-zero) | **Non-zero in both hooked sessions**: 44520 (later 374640) in session 1, 135660 in session 2; `97 detoured, 0 failed` both times | out.txt 413, 3224, 4096 (session 1); 4312, 4436 (session 2) |
| M-bag | `craftprobe bag`: where the bag's materials tab lives, its kind and entries. **Control:** the container named holds the counts seen in the bag's materials tab by eye; a miss also needs `M-backing`'s profile-getter returns checked for the tab (rule below) | Not observed by `craftprobe bag` (`UI_Inventory_obj`, filters `mat`/`stack`/`tab`/`inv`, one level): it ran with the bag window closed (`UI_Inventory_obj has no live instance`), and the control was never run. Seen through the hooks instead: `CountInventoryItem(1, <itemType>, 1, <id>)` (self `UI_Craft_obj`, other `Player_obj`) answered plausible bag counts (153, 81, 139, ...), and `GetInventoryArray(1)` (self `Player_obj`) returned 18 fingerprint strings `0-0-<n>-<class>`, 15 non-empty - the bag's main tab, not a materials tab | out.txt 86, 3731-3789; `cp_backing_GetInvArray.json` |
| M-stash-open | `craftprobe stash` with the stash window open: where the stash's material tab lives, with a count matching the tab by eye (this is `M-stash-closed`'s control) | Not observed by `craftprobe stash` / `var` (instance refs not followed): on the Socketable tab (214 Ol by eye) `UI_Stash_obj` (125 variables) reads `stashTabSelected` = `tabSelected` = -2, and holds `stashGrid`, `uiStashContainer` and `invMaterialTab` as `ref instance` values and `stashSearchGrid` as a `ref ds_grid`; the reader printed each ref's text and stopped, so no container was reached and this row's control has not passed | out.txt 180-216; 214 Ol by eye (session 2) |
| M-stash-closed | `craftprobe stash` with the stash window closed: is the material tab readable closed? **Control:** `M-stash-open` found the tab's container with a count matching by eye, and `M-backing`'s profile and stash getter returns were checked for it (rule below) | Not observed by `craftprobe stash` (`UI_Stash_obj`/`Town_Stash_obj`/globals, filters `stash`/`mat`/`tab`, one level): `UI_Stash_obj has no live instance`, `Town_Stash_obj` matched 0 of 44 variables, and the 80 matching globals include only the tab table (`defaultStashTabStruct`, `stashTabDataStruct`: tab names and indices, no contents). Rules (a) and (b) did not pass, so this is not a "not readable" | out.txt 86-171; `cp_var_stashTabDataStruct.json` |
| M-recipe | `craftprobe recipe`: the selected recipe's needs, reachable by name? | Not observed (`craftprobe recipe` was not run in Phase 1). The recipe reached the craft route as `CraftFindRecipeItems`' arguments instead: a recipe array of length 1, the bench grid array of length 6 and a `ref ds_map` (see `M-craft`) | out.txt 4057 |
| M-craft | Hand craft from the bag: which rows fire, with what self/other/arguments/returns; bag count change | Recipe 3 Ol -> 1 Old, 3 Ol in the crafting bench, crafted twice. Craft 1: `CraftFindRecipeItems` (self `UI_Craft_Recipe_List_Item_obj`, other `UI_Grid_obj`, argc 4) -> `{a=2}`; `DoCraftResult(1, 60, undefined, ds_map, false)` -> `CraftEditGrid` -> `CraftEditPlayerInventory` -> `InventoryGridRemoveItem(1, <Ol fingerprint>)` false -> `GridRemoveItem(<grid array>, <fingerprint>)` true -> `s_CraftItem(15, 2, undefined, false)` -> `GridAddItem` -> `{tabNumber 0, x 0, y 0, tabType 0, success true}`. Craft 2 with 0 Ol left: `CraftFindRecipeItems` -> `{a=1}`, `CraftEditPlayerInventory` ran only the `___struct___88` edit, no remove and no add row fired, `s_CraftItem` ran, and a second Old appeared. By eye: 3 Ol -> 0, **2 Old produced - a vanilla duplication**, reproduced in session 2 with no hook installed (`### Constraints from Phase 1`) | out.txt 4057-4081; by eye, both sessions |
| M-move-stash-to-bag | Hand click-move stash tab -> bag: rows, `success` answer, counts by eye | Not observed by the 97-row table (control non-zero, 135660; `GridAddItem`/`GridRemoveItem` fired in the same build on the craft). The owner picked the whole 214-Ol stack out of the Socketable tab, put it back, then moved 1 Ol to the bag: no add, remove or stack row fired (`StashAddToStack`, `StashGridAddItem`, `InventoryGridAddToStack`, `InventoryGridCanAddToStack`, `GridAddItem`, `GridRemoveItem`, `InventoryGridRemoveItem`, `InvGridClearItemNode`, every `Stack*` row, `ChangeItemOwner`: 0 calls). What fired: `UiAStashTabClick` (self `UI_Button_Stash_Tab_obj`, other `UI_Stash_obj`); `GetInventoryGridNode` x3 (self `UI_Stash_obj`, a0 = `0`, `0`, `-2`, each returning `ref instance 266199`); `GetStashMaxTabs` x6 (8); the `UI_Stash_obj` closures `anon@5662`, `@8574`, `@8329`, `@3835`, `@7091`, `@2245`, `@4348` (x3, a0 = the grid refs 266217, 266278, 266199) and `@7525` (x35); and the draw rows. The move runs through code outside the table | out.txt 4312-4460 |
| M-move-bag-to-stash | Hand click-move bag -> stash tab: rows, `success` answer, counts by eye | Not observed by the 97-row table (control non-zero, 135660; `GridAddItem`/`GridRemoveItem` fired in the same build on the craft). No bag -> stash-tab move was made on its own; the nearest, the whole stack put back into the Socketable tab from the cursor, fired none of the add, remove or stack rows (`M-move-stash-to-bag`) | out.txt 4312-4460 |
| M-backing | `backing dump`: what the profile and stash getters returned (json files) | Stash open, session 2: `GetStashMaxTabs` -> 8 (self `UI_Stash_Tab_Bar_Container_obj`); `GetProfileInventoryData` -> `ref instance 257725` (self `UI_Inventory_Grid_obj`; not followed); `GetPlayerItemOwner` -> 0; `GetInventoryArray` (self `Player_obj`, a0 = 1) -> the 18 bag fingerprints of `M-bag`. `s_StashTabData`, `LoadStash` and `SaveStash`: 0 calls after the hook, across a stash open and close too - they ran at load, before the hook, or on a route outside the table. No kept return held a special tab's contents at the depth read | out.txt 4462-4467; `cp_backing_GetStashMaxTabs.json`, `cp_backing_GetProfileInv.json`, `cp_backing_GetItemOwner.json`, `cp_backing_GetInvArray.json` |
| H-A | Availability and consume rows extendable by name, and the stash tab readable closed? | Undecidable from Phase 1 (container and move route not reached). The availability and consume rows fire by name with recorded shapes (`M-craft`), but the Cube's own `a` cannot be extended (the duplication) and no special-tab container has been read | `M-craft`, `M-stash-open`, `M-stash-closed` |
| H-B | A by-name stash -> bag move with a `success` answer, bag cap not hit? | Undecidable from Phase 1 (container and move route not reached): no row of the table fired on the hand move | `M-move-stash-to-bag` |
| H-C | Availability extendable, consume not? | Undecidable from Phase 1 (container and move route not reached) | `M-craft`, `M-stash-closed` |

**A reader miss is not a negative until its control passes.** The hook-free
readers look only where they are pointed: `craftprobe stash` lists the instance
variables of `UI_Stash_obj` and `Town_Stash_obj` and the globals whose names
contain `stash`, `mat` or `tab`, one level deep, and with the stash window closed
`UI_Stash_obj` has no instance to read at all; `craftprobe bag` does the same for
`UI_Inventory_obj` with `mat`, `stack`, `tab`, `inv`. A "nothing found" from
either therefore measures where the reader looked, not where the game keeps the
tab. So:

- **(a)** `M-stash-closed` counts as a miss only after `M-stash-open` has located
  the stash tab's container, with a count matching what the human sees in the
  tab. If the reader cannot find the tab with the window open, it has no
  known-good target, and a closed-window miss says nothing about the game.
- **(b)** Before a miss counts - in `M-bag` or `M-stash-closed` - the `backing`
  returns (`M-backing`: the four profile getters, `s_StashTabData`, `LoadStash`,
  `GetStashMaxTabs`) are checked for that container too, since the game's own
  getters may hand it out where no window variable or global holds it.
- **(c)** Until both hold, the Result cell reads "not observed by `craftprobe
  stash` (`UI_Stash_obj`/`Town_Stash_obj`/globals, filters `stash`/`mat`/`tab`,
  one level)" - or the same for `craftprobe bag` with its object and filters -
  and never "not readable". Only a miss whose control passed may be written as
  "not readable closed".

### Constraints from Phase 1

These bind every hypothesis, whichever the owner picks.

- **The cube's own availability count cannot be trusted, and the mod must not
  widen the vanilla duplication.** In the unmodded game (reproduced in session 2
  with no hook installed), one craft of 3 Ol -> 1 Old consumed the 3 Ol, yet
  `CraftFindRecipeItems` still answered `a=2` beforehand and `a=1` afterwards, and
  a second craft with no Ol left produced another Old (`M-craft`). So the mod
  counts a recipe's inputs itself, from a container it has read, and never reads,
  trusts or extends the cube's `a`. A stash tab must never be able to supply an
  input the mod has not counted there, since that would turn a one-off stale
  count into a repeatable one.
- **Both special tabs are in scope**: Socketable (`-2`) and Materials (`-4`).
  The Unique tab (`-5`) and every ordinary grid tab stay untouched; the Unique
  tab's closures are hooked in Phase 1b only as candidates for a code path the
  special tabs may share, and nothing acts on them.
- **The stash and cube windows cannot be open together** (`B0-vanilla`). A
  mechanism that reads the stash tab while the cube is in use reads it with the
  stash window closed, so the owner's fallback "require the stash to be open"
  is not available as stated: the container has to be readable closed, or the
  mod has to read it at some earlier moment when the stash was open and keep
  that answer.
- **No game bug is filed.** The duplication is recorded here as a design
  constraint only (owner, 2026-09-22); this work neither fixes nor reports it.

### Phase 1b results

Research DLL: `plugin_build\BloodPactPlugin_rel.dll`, built 2026-09-22 with
`plugin_build\build.bat dev` from ForgePact `8d89622` (SHA-256
`d38586443197d2b0747d85c634aefcf2e03bf11dff4cfefaba659a895060d957`). It replaces
the `a64cdbf` build (`774b2df5...`), whose `node` reader could not see the `o`
stack member; a plugin with that older hash fails `dll-hash`. The player build
(`build.bat release`) from the same commit carries no `phase1b` string.
The owner had it installed as `mods\aurie\BloodPactPlugin.dll` for Live 2;
`### Live procedure 1b`'s `dll-hash` check compared the installed plugin against
this hash before anything else counted.

**Live 2, 2026-09-22/23.** The owner at the keyboard for the hand moves and
`live-operator` on the command channel (`hs-drive`). Character slot 14 ("Sorak");
slot 13, Phase 1's character, was loaded first by mistake and relaunched before
any hand-back counted. Three launches: slot 13 (abandoned), slot 14 (hand-backs
A to C), and, after an overnight interruption, a resumed slot-14 launch (a repeat
of C, then D to F). The saves were backed up before each night's part (an
independent copy and an `hs-drive` backup) and restored afterwards; no ForgePact
mod was toggled, and no craft was pressed. The resumed launch's starting bag
count was corrected by the owner's count (1 Ol left over from hand-back C, not
0); every count below uses the corrected figure. The capture,
`.claude/workorders/forgepact-issue-14-phase1b-live-2.md`, stays on the owner's
machine with the workorder and is cited below by its step headings; nothing of
it is committed.

Filled from that capture, one row per check it carries (the check names and
their `## Checks` line format are fixed by `### Live procedure 1b`). A `fail` or
`not-observed` verdict is a finding, not a defect; a mechanism row counts only
when `dll-hash`, `marker`, `control` and, for a special-tab miss,
`node-bag-control` passed in the same session. Coordinates `(x, y)` are the
first two arguments of the call named; `o` and `b` are the members of the
`itemDefinitionStruct` inside the item struct the call received.
The move rows and `load-capture` count although `node-bag-control` failed: they
are hook observations and rest on the hook controls (`dll-hash`, `marker`,
`control`), not on `node-bag-control`, which gates only the reader rows
(`stash-open-socket`, `stash-open-material`, `stash-closed`).

| Check | What it measures | Observed | Verdict |
|---|---|---|---|
| dll-hash | The installed plugin's SHA-256 equals the hash above | `D38586443197...060D957`, equal to the hash above, before the first launch and again before the resumed launch (another session had swapped the plugin overnight, and it was reinstalled) (capture, Step 0 and Resume) | pass |
| marker | A bare `craftprobe` answers `phase1b rows=202` (this build, not `aa0c72a`) | `craftprobe: phase1b rows=202 - research instrument ...` at each of the three launches, before the character loaded; `craftprobe hook` answered `202 detoured, 0 failed` each time | pass |
| control | `CheckPlayerInteraction` non-zero after the character loads | Non-zero at every `craftprobe show`: 14896 and 14812 after the two first-night loads, 13090 after the resumed load, 132720 to 335160 during the hand-backs | pass |
| node-bag-control | `craftprobe node bag` prints a `sum class=<c> b=<b>` line whose member value is equal to a bag stack (two or more) the owner states by eye - the known-good read of both the cells and the count member, which is named here | Scored **fail**. The owner stated Tor 173, from the bag's Socketable tab (hand-back A). `node bag` and `node id:262489` read the bag's `InventoryGrid` (`UI_Inventory_Grid_obj` id 262489, 15x6, 73 filled cells, 73 fingerprints, classes 13 and 14 only) and resolved 64 of the 73 fingerprints (the per-command cap; the same 9 were left on a repeat); every resolved item printed a `def.o` value, and no sum equalled 173 - the grid read held no socketable, so the stack stated was not in it (the grid appears to hold whichever bag tab is on show; not established). In the resumed launch, with the bag's Socketable tab on show, `node bag` read `sum class=15 b=1 ... def.o=2` on the bag's `InventoryGrid` (id 262423), equal to the 2 Ol the owner counted by eye (capture, Step 5b): the control's shape, met after the special-tab reads below had run, which were not repeated after it | fail |
| counts-tool-before | `tools/stash_tab_counts.py` before launch: exit 0, a `socket_tab` and a `material_tab` line | Exit 0: `socket_tab: entries=90`, among them `class=15 b=1 stack=217` (Ol; the owner saw 217), and `material_tab: entries=1`, `class=14 b=50 stack=13` (Unstable Dust; the owner saw 13). The same before the resumed launch | pass |
| stash-open-socket | A reader prints a per-(class, b) sum, in the member `node-bag-control` named, equal to the Socketable tab's Ol count by eye; the container's object and id | Not observed by `node`: no read printed a class-15 sum of 217 while the owner saw Ol 217 (hand-back A). Container located: the Socketable tab is `UI_Stash_Socket_New_obj` (id 262965), reached from `UI_Stash_obj.activeNode` through a `UI_Container_obj`'s `content`, and its `anon@1305` fires on the tab. Its `grid` is a 10x14 array of separate `UI_Inventory_Grid_obj` instances, one per socketable (cells tagged `StashSocketGrid`, `itemTypeExclusive` [15]; `nodeList` of 106), not one fingerprint-keyed `nodeGrid`. On two of those cells (ids 262970, 262971, both class 15) `GetItemFromFingerprint` was not resolved with self = the `StashSocketGrid` cell (`UI_Inventory_Grid_obj`) and `a1=0`, with a fresh lookup budget, so no count member was read. `UI_Stash_obj.stashGrid` (id 262533) held 1 filled cell; `node stash`'s only item grid was `invGrid`, the bag. `node-bag-control` had not passed when this ran, so this is not a "not readable" | not-observed |
| stash-open-material | The same on the Materials tab, for one named material | Not observed by `node`: the owner saw Unstable Dust 13, the tab's only stack (hand-back B). With the Materials tab on show, `UI_Stash_obj.activeNode` is `stashGrid` itself (`UI_Inventory_Grid_obj` id 262533, 17x18, 1 filled cell, class 14 - the class `tools/stash_tab_counts.py` gives Unstable Dust): this tab uses the ordinary grid object, not a dedicated one like the Socketable tab. `GetItemFromFingerprint` on that cell was not resolved with self = `UI_Inventory_Grid_obj` id 262533 and `a1=0` (fresh budget), so no count member was read. `node-bag-control` had not passed | not-observed |
| move-socket-to-bag | Rows that fire on a whole-stack pick-up/put-back and a 1-Ol move Socketable -> bag, with self/other/arguments/return | Two variants, both seen by name; neither returns a success answer. **Into an empty bag cell** (bag held no Ol): the whole-stack pick-up and put-back and then a 1-Ol move fired `UiASplitStack` and `UI_Split_Stack_obj` `anon@1285` (2 each), `UiAInventorySocketTabClick`, `UI_Stash_Socket_New_obj` `anon@1305`, and `s_InvNode` twice - first with other = a Socketable cell (`UI_Inventory_Grid_obj`) at (0, 0) and the whole stack (`o=217`, `b=1`), then with other = the bag's `InventoryGrid` at (7, 3) and `o=1`, `b=1`; self was not an instance and both returned `undefined`. Stash 217 to 216, bag 0 to 1, by eye. **Onto an existing bag stack** (resumed launch, bag held 1 Ol): `s_InventoryDrag`, `GetInventoryGridNode` 3 times (each returning the bag grid, id 262423), the same split rows, then `InventorySwapItemsNew` at (7, 3) with the Ol's node and, inside it, `InventorySocketItem` with `o=1`, `b=1`, self = other = the bag grid; `InventorySocketItem` returned `false` although the unit arrived (stash 217 to 216, bag 1 to 2 by eye, and `node bag` read `def.o=2`), and no `s_InvNode` fired. Which variant runs follows whether the destination cell already holds that item | pass |
| move-bag-to-socket | Rows that fire on the 1-Ol move bag -> Socketable | 1 Ol bag to Socketable (resumed launch; bag 2 to 1, stash 216 to 217, by eye): `s_InvNode` with the bag stack (`o=2`, `b=1`, other = the bag grid, at (7, 2)); `UiASplitStack` and `anon@1285`; `s_InvNode` with the split unit (`o=1`); then `StashAddToStack` (self = other = the bag grid; six arguments: an array of length 1, `9`, `2`, the item struct with `o=1`, `b=1`, `1`, `8`) returning `true`; then `InvGridClearItemNode` on the emptied bag cell. `StashAddToStack` answering `true` is the game's own bag-to-stash write with a success signal | pass |
| move-material-to-bag | Rows that fire on 1 material Materials -> bag -> Materials | The owner moved 1 Unstable Dust Materials to bag and back (stash 13, bag 0 at the end, by eye). **Bag to Materials leg**: `UiASplitStack` and `anon@1285`, `s_InvNode` (`o=1`, `b=50`, other = the bag grid, at (12, 2)), `StashAddToStack` (an array of length 18, `9`, `2`, the item struct with `o=1`, `b=50`, `1`, `0`) returning `true`, then `InvGridClearItemNode` - the same shape as `move-bag-to-socket`. **Materials to bag leg**: not observed - `s_InvNode`, `StashAddToStack` and `InvGridClearItemNode` counted one call each in the window, all on the other leg; only tab clicks (`UiAStashMaterialTabClick`, `UiAInventoryMaterialTabClick`, `GetInventoryGridNode`, `UI_Stash_obj` closures) and `ProcessInventoryGridInput` calls (on the bag grid and a Materials grid, id 262467, with no item among their arguments) were logged for it | pass (bag to Materials leg); not-observed (Materials to bag leg) |
| stash-closed | The container found open is still readable by id, with the same sum, once the stash window is closed | Not observed. With the stash window closed, `craftprobe stash` printed `UI_Stash_obj has no live instance`, and `node id:262525` and `var id:262525 *` printed `instance_exists is false` for `UI_Stash_Socket_New_obj` id 262525, the Socketable container the same launch had logged as `anon@1305`'s self while the window was open: the window's container was not found by `instance_exists`/`instance_number` once the window closed (destroyed or deactivated; not distinguished). Where the tabs' contents are held while the window is closed is not observed: no special-tab count was reproduced with the window open (rule (a)), and `LoadStash` ran at character load, not when the window opened (`load-capture`) | not-observed |
| load-capture | `LoadStash`, `s_StashTabData` or a `Load_Inventory_obj` row fires during the character load, with a kept return | `LoadStash`: 2 calls during each character load, self `Console_Save_obj`, returning `true` (`cp_backing_LoadStash.json`, 101 bytes - the return is a bool, not the contents); no further call through the resumed launch's stash openings and closings (`backing dump` at the end still kept call 2). `s_StashTabData` and the `Load_Inventory_obj` closures are not among the rows the capture quotes as having fired | pass |
| backing-per-arg | `backing dump` names >= 2 distinct first-argument signatures for `GetInventoryArray` or `CountInventoryItem` | `backing dump`: `GetInventoryArray: 1 first-argument signature(s)` - only `a0=1` (self `Player_obj`, an array of 18 bag fingerprints); `CountInventoryItem` kept nothing - the cube, where Phase 1 saw it called, was never opened in this session | not-observed |
| counts-tool-after | `tools/stash_tab_counts.py` after the game exits: the Socketable tab's sum equals the count the owner stated last | Exit 0: `class=15 b=1 stack=217` and `class=14 b=50 stack=13`, equal to the counts the owner stated last (every unit moved was put back) | pass |

**What Phase 1b settles, and what it leaves open.**

- **`o` is the stack count on a stackable special-tab item.** Every move above
  handed the game's own routines an item struct whose `o` was the stack, or the
  part of it being moved (217 for the picked-up Ol stack, 2 for the bag stack
  before a split, 1 for a one-unit move), with `b` the base id
  `tools/stash_tab_counts.py` prints (1 for Ol, 50 for Unstable Dust); `node
  bag`'s `def.o` equalled the owner's count once the right grid was read. On a
  relic the same key is the upgrade level (repo-root
  `docs/RUNTIME_DATA_MODELS.md` § 2), so it is read as a stack only on an item
  already identified as a stackable.
- **The special-tab move route is outside Phase 1's table, and now seen by
  name.** Into the stash: `s_InvNode`, then `StashAddToStack` answering `true`,
  then `InvGridClearItemNode`, for both tabs. Out of the Socketable tab:
  `s_InvNode` into an empty cell, or `InventorySwapItemsNew` with
  `InventorySocketItem` onto an existing stack - neither answers success
  (`undefined`, and a `false` on a unit that arrived). Out of the Materials tab:
  not observed.
- **Both tabs live in window objects that were not found by
  `instance_exists`/`instance_number` once the window closed (destroyed or
  deactivated; not distinguished).**
  The Socketable tab is `UI_Stash_Socket_New_obj` with one grid instance per
  socketable; the Materials tab is the stash window's ordinary `stashGrid`. The
  item lookup by fingerprint, with the three cells tried (262970, 262971, 262533)
  as self and `a1=0`, answered no struct. What was observed is that the game's
  `LoadStash`, a detoured row, ran during character load and did not fire when
  the stash window opened. A store the game keeps in memory between the two is
  one reading of that; a route outside the rows (a file or buffer read when the
  window opens, say) is not ruled out. No Phase 1b reader looked for either.

### Phase 1c results

Research DLL: `plugin_build\BloodPactPlugin_rel.dll`, built 2026-09-23 with
`plugin_build\build.bat dev` from ForgePact `a7795ca` (SHA-256
`e9d32ec3adb3238fe7c2b3284498591ae8af416da21302946e034a71712e6429`). The player
build (`build.bat release`) from the same commit carries no `phase1c` string.
It is installed as `mods\aurie\BloodPactPlugin.dll` only on the owner's word;
`### Live procedure 1c`'s `dll-hash` check compares the installed plugin
against this hash before anything else counts, so a Phase 1b plugin (or any
other build) fails it.

**Live 1c, 2026-09-23.** One launch, character slot 14 ("Sorak"),
`live-operator` on the command channel (`hs-drive`) and the owner at the
keyboard only to open and close the bag and the stash and to click tabs - no
item was moved and no craft was pressed. The saves were backed up first (an
independent copy and an `hs-drive` backup) and restored in full afterwards; no
ForgePact mod was toggled; the plugin was the build above. The owner's counts by
eye: Ol 216 on the Socketable tab, Unstable Dust 13 on the Materials tab, and
Tor 183 on the bag's Socketable tab (the 173 stated before the launch was an
older count). The capture,
`.claude/workorders/forgepact-issue-14-phase1c-live-1.md`, stays on the owner's
machine with the workorder and is cited below by its step headings; nothing of
it is committed. Two deviations from the procedure, both recorded in the
capture: step 5's `backing dump`/`backing clear`/`backing on` ran after the
stash had opened rather than before (step 4's dump, taken with the stash never
opened, is the before-read), and the last closed reads (`store-closed-after`)
ran after the first close, before the reopen, since the session ended with the
stash open.

Filled from that capture, one row per check (the names and the `## Checks` line
format are fixed by `### Live procedure 1c`). A `fail` or `not-observed` is a
finding, written as "not observed by <reader> (<what it printed>)"; a reader miss
counts only once the control for the path it took passed (`### Live procedure
1c`).

| Check | What it measures | Observed | Verdict |
|---|---|---|---|
| dll-hash | The installed plugin's SHA-256 equals the hash above | `E9D32EC3...71712E6429` from `Get-FileHash` on the installed plugin with the game closed, equal to the hash above (capture, Step 0) | pass |
| marker | A bare `craftprobe` answers `phase1c rows=` (this build, not Phase 1b's or `aa0c72a`'s) | `craftprobe: phase1c rows=223 - research instrument ...` before the character loaded; `hook` answered `223 detoured, 0 failed` (capture, Step 1) | pass |
| control | `CheckPlayerInteraction` non-zero after the character loads | `CheckPlayerInteraction calls=16534` at the first `show` after the load (capture, Step 2) | pass |
| counts-tool-before | `tools/stash_tab_counts.py` before launch: exit 0, a `socket_tab` and a `material_tab` line | Exit 0: `socket_tab: entries=90` with `class=15 b=1 stack=216` (Ol; the owner saw 216) and `material_tab: entries=1` with `class=14 b=50 stack=13` (Unstable Dust; the owner saw 13) (capture, Step 0) | pass |
| load-rows | Which armed rows (`LoadStash`, the stash-tab rows, `Load_Inventory_obj`, `GetItemMap`, the `Console_Save_obj`/`Profile_Manager_obj`/`Town_Stash_obj` closures, `s_SaveStashConstants`, `LoadInventoryOrderNew`, the inventory-map rows) fire during the character load, with self and return | During the load: `LoadStash` twice, both with arguments `5`, `1` - first self `Controller_obj`, returning `false`, then self `Console_Save_obj`, returning `true` (a bool, not the contents); `GetItemMap(0)` (self `Controller_obj`) returning an empty `ds_map`, id 1056, on each of its ten logged calls; `LoadInventoryOrderNew` (self `Console_Save_obj`, first argument `10`) and `s_SaveStashConstants` (argument `5`, other `Console_Save_obj`, self not an instance) once each; `Console_Save_obj` `anon@2587` (55 calls, returning `undefined`) and the `Profile_Manager_obj` closures `anon@2012`, `@2520`, `@5349` and `@5835` (51 to 6184 calls each, already firing at the main menu). No row returned an item container (capture, Step 2) | pass |
| store-candidates | With the stash never opened this launch, every variable or global `store`/`store names` finds whose shape could hold items (an array, a struct, a `ds_map`/`ds_list`, an instance reference), including the kept `GetProfileInventoryData` instance and `Console_Save_obj` | `GetProfileInventoryData`'s return was already kept (`ref instance 257725`, self `UI_Inventory_Grid_obj`) - kept since `backing on` before the load, so it does not show a call made with no window open. That instance is `New_Inventory_Data_obj` (18 variables), and `store` matched 14 of them, all item-shaped: `localItemMap` (a `ds_map`, id 1056 - the map `GetItemMap(0)` returned empty during the load - now 340 item structs), `stashPersonalGrid` (an array of 18), `inventoryMaterialGrid` and `inventorySocketGrid` (arrays of 6), and the charm, incarnation, potion, key, tarot, craft, vault, relic and prospect grids and `mercenaryInventoryStruct`. On the other holder objects only `Profile_Manager_obj.inputSimMap` (an empty `ds_map`); nothing item-shaped on `Console_Save_obj` (0 of 16 matched), `Town_Stash_obj` or `Player_obj`; `Inventory_Loading_obj` and `Load_Inventory_obj` had no live instance; 542 of 3563 globals matched the filters (80 printed) (capture, Step 3) | pass |
| store-closed | A `node var` sum over one of those candidates, with the stash never opened this launch, equal to the Ol or Unstable Dust count - the load-time store read closed | Not observed by `node var`. Over `New_Inventory_Data_obj.localItemMap` with `class=15` and with `class=14` (item-struct path: `entries read=340 fingerprints=0 ... items=340`, no lookup made): no `class=15 b=1` sum of 216 and no `class=14 b=50` entry at all; its class-15 sums included `b=3 def.o=183` and `b=1 def.o=1`, the bag's own Tor and leftover Ol by eye, so the map was summed correctly, but the path had no declared control: not observed (uncontrolled path). Over `stashPersonalGrid` (fingerprint path: `entries read=306 fingerprints=18 (distinct 4)`): classes 3 and 10 only, no class-15 or class-14 fingerprint, so no lookup was made; this path's control, `node-var-control`, failed, so the miss does not count. Repeated after the bag draw (step 4e) with the same result (capture, Steps 3 and 4) | not-observed |
| node-bag-control | `node bag` on the tab the owner names prints a `sum class=<c> b=<b>` line whose member equals the stack the owner states by eye | The owner named Tor on the bag's Socketable tab, 183 at the read. `node bag class=15` on the bag's `InventoryGrid` (`UI_Inventory_Grid_obj` id 263170, 15x6, 21 filled cells, all class 15) printed `sum class=15 b=3: fingerprints=1 cells=1 def.o=183`, equal to the count; the count member is `def.o`. The same read's `b=1 def.o=1` is the owner's leftover Ol (capture, Step 4) | pass |
| node-var-control | `node var` over the kept `GetInventoryArray` array, self the bag grid, equals `node bag` on the same tab | `node var global 0 __cp_backing_GetInvArray_arg0 self=id:263170 class=15`: `array len=18`, `entries read=18 fingerprints=15 (distinct 15) items=0 other=3`, classes 0 to 8 and 16 only - no class-15 fingerprint, so neither of `node bag`'s pairs (`b=1 def.o=1`, `b=3 def.o=183`) was met. The kept `GetInventoryArray(1)` return (self `Player_obj`) holds a different set from the tab on show - by its classes, the player's equipped items, not the bag (Phase 1 and 1b described it as the bag's fingerprints) - so the fail measured the choice of array, not a broken reader; either way the fingerprint path has no passed control from this session (capture, Step 4) | fail |
| fp-a1-bag | `backing dump` names `GetItemFromFingerprint` signatures (a1, a0's class, self objects) while the bag draws | With the bag open and the stash never opened, one second-argument signature: `GetItemFromFingerprint a1=0` (latest a0 `0-0-199477322284-0`; selves `Controller_obj`, `Console_Save_obj`, `UI_Incarnation_Node_obj`, `Load_Specific_Stats_obj`, `Load_Dual_Wielding_obj`, `Mercenary_obj`, the six-object cap). The twenty armed calls all passed other = self, argc 2 and `a1=0`, from `Flask_Controller_obj`, `Mercenary_obj`, `Player_obj` and `UI_Button_Inventory_Item_obj` (one per bag cell); armed call 8 matches the kept signature's self and a0 (capture, Step 4) | pass |
| fp-a1-stash | `backing dump` names a signature or self not seen in `fp-a1-bag`, or an a0 of class 15, once the stash draws the Socketable tab | After the stash opened on the Socketable tab, a second signature: `GetItemFromFingerprint a1=9`, a0 `0-0-200922080099-15` (class suffix 15), selves `UI_Inventory_Grid_obj` and `UI_Stash_obj`, latest self `UI_Stash_obj` - neither the value `9` nor `UI_Stash_obj` was in step 4's dump, and no dump reported a slot overflow. The twenty calls armed with the stash open were all `a1=0` bag and equipment draws, so the game's own `a1=9` call's other and argc are not on record (capture, Step 5) | pass |
| stash-open-socket | `node socket` (with the `a1=`/`self=id:` variants if the plain lookup misses) prints `sum class=15 b=1` with `def.o` equal to the Ol count by eye, naming the (self, a1) that resolved | The owner saw Ol 216. `node socket` on `UI_Stash_Socket_New_obj` id 264484 (reached from `UI_Stash_obj.activeNode` through a `UI_Container_obj`) with the default `a1=0`: all 90 filled cells missed (`returned no struct`, self = each cell's `UI_Inventory_Grid_obj`, `a1=0`). With `a1=9`: all 90 resolved (106 cell instances read, 90 filled, 90 distinct fingerprints), and `sum class=15 b=1: fingerprints=1 cells=1 def.o=216`, equal to the count by eye, from the fingerprint the `a1=9` signature carries. Resolved with self = the cell (a `UI_Inventory_Grid_obj` tagged `StashSocketGrid`) and `a1=9` (capture, Step 5) | pass |
| stash-open-material | The same on the Materials tab's `stashGrid`: `sum class=14 b=50` equal to the Unstable Dust count | The owner saw Unstable Dust 13. With the Materials tab on show, `UI_Stash_obj.activeNode` is the plain `UI_Inventory_Grid_obj` id 264407 (`StashGrid`, 17x18, `itemTypeExclusive` [14, 13]), and the Socketable container 264484 was no longer found by `instance_exists` (destroyed or deactivated; not distinguished). `node id:264407 a1=9` resolved the tab's one filled cell first time: `sum class=14 b=50: fingerprints=1 cells=1 def.o=13`, equal to the count; self = that grid, `a1=9` (capture, Step 6) | pass |
| reopen-id | The Socketable window's id before a close and after a reopen, and whether the old id is alive again (kept) or dead with a new id (a new window was made; the old one destroyed or still deactivated, not distinguished) | Closed: `UI_Stash_obj has no live instance`, and `node id:264484` and `node id:264407` printed `instance_exists is false`. Reopened on the Socketable tab: a new `UI_Stash_obj` (id 266337, was 264335), a new container C2 = `UI_Stash_Socket_New_obj` id 266429 (was 264484) with new cell ids, and `node id:264484` still `instance_exists is false`. A new window was made; the old one destroyed or still deactivated (not distinguished) (capture, Step 7) | pass |
| store-closed-after | The read that passed `stash-open-socket`, and the `store-closed` read, repeated with the window closed: the same sum | Not observed, with the stash closed after its first open (before the reopen). The reads that passed `stash-open-socket` and `stash-open-material` had nothing to read: both containers printed `instance_exists is false`. `localItemMap` again: unchanged - the bag's `b=3 def.o=183` and `b=1 def.o=1`, no `class=15 b=1` sum of 216 and no `class=14 b=50` entry, although the stash had been open (item-struct path: not observed (uncontrolled path)). `stashPersonalGrid` again, now with `a1=9`: unchanged, classes 3 and 10 only, no lookup made (fingerprint path; `node-var-control` failed) (capture, Step 7 part 1 and "Extra: closed-window store-closed retries") | not-observed |
| counts-tool-after | `tools/stash_tab_counts.py` after the game exits: the socket and material lines equal the counts the owner stated (nothing was moved) | After a graceful exit: `class=15 b=1 stack=216` and `class=14 b=50 stack=13`, equal to the owner's counts - nothing moved. The game's own exit save rewrote `herosiege13.hss`, `inventory_order_13.hss`, `shop.ini` and `stash.hss` (`hs_saves_inspect`); the saves were restored from the session's backup afterwards (capture, Step 9) | pass |

**What Phase 1c settles, and what it leaves open.**

- **Both special tabs are readable by name while the stash window is open.**
  The game's own fingerprint lookup resolves a stash cell as
  `GetItemFromFingerprint(<fingerprint>, 9)`: the stash's draw passed `9` as the
  second argument (selves `UI_Stash_obj` and `UI_Inventory_Grid_obj`), and the instrument's
  lookup with self = the cell or grid the fingerprint was read from and `a1=9`
  resolved every filled cell of both tabs, with the counts the owner saw in
  `def.o`. `a1=0`, the bag's value, resolved none of the 90 Socketable cells -
  which is the call shape Phase 1b's misses supplied. `9` is also the second
  argument `StashAddToStack` received in Phase 1b's bag-to-stash moves; that it
  names the stash is a pattern across two routines, not established.
- **No reader reproduced a special-tab count with the stash window closed.**
  Neither read was a controlled miss: `localItemMap`, a map of item structs,
  reproduced the bag's counts and held neither stash stack checked (Ol 216,
  Unstable Dust 13), before or after the stash was open (item-struct path, no
  declared control), and the fingerprint path's control failed. So the
  closed-window store is "not found by these readers", not "not readable".
- **Each open of the stash makes new window objects.** A tab switch
  left the Socketable container no longer found by `instance_exists`, and a
  close and reopen made a new `UI_Stash_obj` and a new container, with the old
  ids still not found (destroyed or still deactivated; not distinguished).
  Anything that reads the tabs through the window's own instances can do so
  only while the window is open on that tab.
- **Where a closed read may still be.** `LoadStash`, a detoured row, fired at
  character load (twice here, the second returning `true` on
  `Console_Save_obj`) and, in Phase 1b, not when the window opened (Phase 1c did
  not count it again after the window opened); a store held in memory is one
  reading of that, a route outside the rows is not ruled out.
  Two leads were kept but not read: `GetItemMap`'s return for first argument
  `9` (kept by `backing`, self a stash grid, `UI_Inventory_Grid_obj` id 266430),
  beside `GetItemMap(0)`, whose map (id 1056) is `localItemMap`, the bag's; and
  `stashPersonalGrid`, whose classes 3 and 10 say it holds an ordinary stash
  tab rather than a special one (not established).

## Decision gate

`decision: pending` - set only by the owner, to one of `H-A`, `H-B`, `H-C`, or
`none` (stop).

The rule: a hypothesis is eligible only if every row it rests on is filled from a
session whose `C-control` is non-zero, it has a by-name call shape recorded live
(no hand-resolved address, no struct-layout read), and the stash tab's container
is readable where the mechanism needs to read it. The paragraph written here after
Phase 1 names which hypotheses the evidence allows, what each would cost the
player (which game operation runs at a moment the game did not choose), and the
recommended one. If no hypothesis has both a by-name shape with a positive control
and a readable stash-tab container, the workorder is blocked and says so here
rather than proposing a struct-layout read. That block may rest on "no readable
stash-tab container" only when the miss passed its control (rules (a) and (b)
under `## Results`); a miss recorded as "not observed by `craftprobe ...`" does
not block - it sends the search to a wider reader (`var`, `backing`, other
filters) - and neither does it, alone, move the owner to the "stash must be
open" fallback.

**After Phase 1c (2026-09-23): still no hypothesis is eligible, and this is
still not a block.** This replaces the reading written after Phase 1b (the Phase
1b rows it cites stand in `### Phase 1b results`). The one thing Phase 1c was
for - a special-tab count read with the stash window closed - is still not
observed. Per hypothesis, the rows it rests on and where they stand:

- **H-A (count and consume through a hook)** rests on `M-craft` (Phase 1: the
  availability and consume rows fire by name, with recorded shapes); on the
  tab's count where the craft runs - with the stash window closed, since the
  cube cannot be open beside it - which is Phase 1c's `store-closed` and
  `store-closed-after`, both not observed; and on a stash-side take with a
  success answer (Phase 1b's `move-socket-to-bag`, which answers none, and
  `move-material-to-bag`'s Materials-to-bag leg, not observed). Phase 1c's
  `stash-open-socket` and `stash-open-material` pass: the count is readable by
  name, `GetItemFromFingerprint(<fingerprint>, 9)`, but only with the window
  open, which is not where the craft runs. Cost to the player: the stash tab
  changes at the craft press, a moment the game chose for the bag only.
- **H-B (pull on demand)** rests on a by-name stash-to-bag move that answers
  success (Phase 1b's `move-socket-to-bag`: seen by name, no success answer;
  the Materials-to-bag leg: not observed) and on the tab being reachable at the
  press with the window closed (Phase 1c's `store-closed` and
  `store-closed-after`, not observed). Phase 1c's `reopen-id` and
  `stash-open-material` sharpen that gap: every observed move ran on the
  window's grid instances, and each open or tab switch makes new ones, the old
  ones no longer found. Cost: a stash-to-bag move at the craft press, a moment
  the game did not choose, which also changes the bag the player sees.
- **H-C (count only)** rests on the same closed-window count (Phase 1c's
  `store-closed` and `store-closed-after`) and `M-craft`, and has H-A's reading
  gap without needing its take. Cost: a recipe the cube shows as craftable
  fails at the press unless the materials are also pulled.

One reading Phase 1c makes possible that none of the three is: count the
special tabs while the stash is open - the one moment Phase 1c read them, by
name - and keep that answer for the cube, as `### Constraints from Phase 1`
allows ("read it at some earlier moment when the stash was open and keep that
answer"). It supplies a count only, never a take, and costs the player a count
that is only as fresh as the last time the stash was opened in the session.

Why this is not a block: no closed-window miss passed its control.
`store-closed` and `store-closed-after` went through `localItemMap` (item
structs: no declared control, so not observed (uncontrolled path), although
that map reproduced the bag's own counts) and `stashPersonalGrid` (fingerprints:
`node-var-control` failed, because the kept `GetInventoryArray(1)` array holds
the equipped items rather than the tab on show). The premise that the game keeps
the tabs' contents in memory while the window is closed is itself untested:
what was observed is that `LoadStash`, a detoured row, ran at character load
and did not fire when the window opened (Phase 1b's `load-capture`); a store
held in memory is one reading, and a route outside the rows at window open (a
file or buffer read, say) is not ruled out.

Recommended (the owner decides): no mechanism yet. If the owner wants one more
reader round first, in the order the evidence points, and with no struct-layout
read: (1) `GetItemMap(9)`'s kept return - kept by `backing` in Live 1c with a
stash grid as self, never read - with `var`/`node var`, first with the stash
open on each special tab as its control (the sums must equal
`stash-open-socket`'s and `stash-open-material`'s), then with the window closed
through the same reference, since `GetItemMap(0)`'s map is `localItemMap`, the
bag's, and `9` is the stash's lookup value; (2) the item-struct path's control
declared before it is used - `node var` over `localItemMap` equal to `node bag`
on the same tab, which Live 1c met without having declared it; (3) a
fingerprint-path control on an array that holds the tab on show, in place of
`GetInventoryArray(1)`; (4) `LoadStash` and the stash-load rows counted across a
stash open, to recheck Phase 1b's observation that the window does not reload.
If a store is found readable closed, H-A is the one this evidence leans
towards, because every stash-side move seen so far ran on window instances that
do not exist while the cube is open, which H-B's pull would need. If none is,
the open-stash count above is what remains, and whether a count-only mechanism
is worth having is the owner's call.
