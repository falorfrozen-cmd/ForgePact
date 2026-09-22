# Crafting from the stash's special tabs (ForgePact issue #14)

phase0-status: complete
phase1-status: complete
phase1b-status: pending

**Status: Phase 0 done (static search, instrument, decision core); Phase 1 (the
first live session, 2026-09-22) done; Phase 1b (a widened instrument and a
second session) pending.** Phase 1 measured the vanilla baseline, the cube's
craft route and a vanilla duplication, but its instrument reached neither the
stash's special-tab container nor the route a hand move between a special tab
and the bag takes (`## Results`). Nothing player-visible changes yet: the
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
| `node id:<n>`, `node <Obj> <nth>`, `node stash`, `node bag` | for each instance: `nodeGridWidth`/`nodeGridHeight`, filled and empty cells, the distinct `nodeFingerprint` values; per fingerprint the item the game's own `GetItemFromFingerprint(fp, 0)` returns - its `itemType`, its `itemDefinitionStruct.b`, the fingerprint's class suffix, and every numeric member of the item, its definition struct or the cell whose name contains `stack`, `amount`, `count` or `qty` - then a `sum class=<c> b=<b>` line per pair with each candidate member summed. `stash` reads every live instance reference of the open stash window (`stashGrid`, `uiStashContainer`, `invMaterialTab`, ...); `bag` reads every `UI_Inventory_Grid_obj` with its `uiNodeCallstack`. An instance without `nodeGrid` prints its variable names | 64 lookups per command (a fingerprint past the cap is summed with `b=?`); 12 instances; 40 fingerprint lines per instance | `node bag` is the known-good read (the bag grid holds items the player can see); a special-tab miss counts only once it has found filled cells and a (class, b) in the same session. Which candidate member is the stack count is decided by eye against the tab and against `tools/stash_tab_counts.py` |
| `backing` per first argument | for `GetInventoryArray` and `CountInventoryItem`, besides the latest return, the latest return per distinct first argument (`1`, `-2`, `-4`, ...): one line when a new signature is first seen, and `backing dump` writes `cp_backing_<row>_arg<k>.json` per signature plus a count line | 8 signatures per row; later new signatures are counted, not kept | the getters are still never invoked; only the game's own calls are kept |

The hub tool `tools/stash_tab_counts.py` (repo-root `docs/tools/stash-tab-counts.md`)
is the save-side cross-check: after the game has exited it decodes
`hs2saves\stash.hss` read-only with `hero-siege-item-editor`'s own decoder and
prints, per `socket_tab*` and `material_tab*` key, the entry count and the summed
stack per (class, b). It runs outside the game and the plugin, and never writes.

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
  and put-back, and a one-unit move each way.
- **The capture** is `.claude/workorders/forgepact-issue-14-phase1b-live-2.md`,
  ending with a `## Checks` section of one line per check, exactly
  `- <name> | expected: <text> | observed: <quoted, short> | pass|fail|not-observed`,
  for these checks: `dll-hash`, `marker`, `control`, `node-bag-control`,
  `counts-tool-before` (the instrument's controls - all five must pass, or
  nothing else in the file counts), then `stash-open-socket`,
  `stash-open-material`, `move-socket-to-bag`, `move-bag-to-socket`,
  `move-material-to-bag`, `stash-closed`, `load-capture`, `backing-per-arg` and
  `counts-tool-after`. A `fail` or `not-observed` there is a finding, recorded
  under `### Phase 1b results` with what the reader printed; it is never
  rewritten as "not readable" unless its control passed (rules (a)-(c) under
  `## Results`).

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
`plugin_build\build.bat dev` from ForgePact `a64cdbf` (SHA-256
`774b2df50b2d0ade1fcfa688baa49b015d209847cd74385ec33dfd557cdfc563`). The player
build (`build.bat release`) from the same commit carries no `phase1b` string.
Not installed into the game's `mods` folder: that, and the session, are the
owner's call. `### Live procedure 1b`'s `dll-hash` check compares the installed
plugin against this hash before anything else counts.

Filled from the Live 2 capture, one row per check it carries (the check names
and their `## Checks` line format are fixed by `### Live procedure 1b`). A
`fail` or `not-observed` verdict is a finding, not a defect; a mechanism row
counts only when `dll-hash`, `marker`, `control` and, for a special-tab miss,
`node-bag-control` passed in the same session.

| Check | What it measures | Observed | Verdict |
|---|---|---|---|
| dll-hash | The installed plugin's SHA-256 equals the hash above | | |
| marker | A bare `craftprobe` answers `phase1b rows=202` (this build, not `aa0c72a`) | | |
| control | `CheckPlayerInteraction` non-zero after the character loads | | |
| node-bag-control | `craftprobe node bag` finds filled cells and at least one fingerprint with its (class, b) - the known-good grid read | | |
| counts-tool-before | `tools/stash_tab_counts.py` before launch: exit 0, a `socket_tab` and a `material_tab` line | | |
| stash-open-socket | A reader prints a per-(class, b) sum equal to the Socketable tab's Ol count by eye; the container's object and id | | |
| stash-open-material | The same on the Materials tab, for one named material | | |
| move-socket-to-bag | Rows that fire on a whole-stack pick-up/put-back and a 1-Ol move Socketable -> bag, with self/other/arguments/return | | |
| move-bag-to-socket | Rows that fire on the 1-Ol move bag -> Socketable | | |
| move-material-to-bag | Rows that fire on 1 material Materials -> bag -> Materials | | |
| stash-closed | The container found open is still readable by id, with the same sum, once the stash window is closed | | |
| load-capture | `LoadStash`, `s_StashTabData` or a `Load_Inventory_obj` row fires during the character load, with a kept return | | |
| backing-per-arg | `backing dump` names >= 2 distinct first-argument signatures for `GetInventoryArray` or `CountInventoryItem` | | |
| counts-tool-after | `tools/stash_tab_counts.py` after the game exits: the Socketable tab's sum equals the count the owner stated last | | |

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
