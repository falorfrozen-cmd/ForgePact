# Crafting from the stash's special tabs (ForgePact issue #14)

phase0-status: complete
phase1-status: complete
phase1b-status: complete
phase1c-status: complete
phase1d-status: complete
phase1e-status: complete
phase1f-status: complete

**Status: Phase 0 done (static search, instrument, decision core); Phase 1 (the
first live session, 2026-09-22) done; Phase 1b (a widened instrument and a
second session, 2026-09-22/23) done; Phase 1c (a reader round, 2026-09-23)
done; Phase 1d (a closed-window reader round, 2026-09-23) done; the owner chose
H-A on 2026-09-23 (`## Decision gate`); Phase 1e, the consume-research round
asked for before any player build, done (2026-09-23).** Phase 1
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
(`### Phase 1c results`). Phase 1d, a reader round on the Phase 1c build with
no rebuild, read both counts with the window closed: the map the game's own
`GetItemMap(9)` call returned, kept by the instrument's detour, still gave Ol
216 and Unstable Dust 13 after the stash closed, through the same reference,
with the item-struct path's control passed; `New_Inventory_Data_obj`'s
`inventorySocketGrid` turned out to hold the bag's Socketable tab, and neither
it nor `inventoryMaterialGrid` the stash's (`### Phase 1d results`). Not
observed: that map
before the stash's first open in the launch (the game made no `GetItemMap(9)`
call before it), and whether the map follows a change to a tab (nothing was
moved). That makes H-A eligible, with what a player build would still need
listed under `## Decision gate`. Phase 1e, a research build and one session,
asks the two things H-A still lacks: whether the stash map can be kept through
ForgePact's own both-routes installer `HookOneScript` (the shape every shipped
gameplay hook has) with a currency rule, and whether a by-name call removes one
unit from a stash special tab and answers success (`### Phase 1e rows`,
`### Phase 1e instrument`, `### Live procedure 1e`, `### Phase 1e results`).
Its session (Live 1e) answered the first with a yes: `HookOneScript` installed
on `GetItemMap` through both routes, kept the game's own `GetItemMap(9)` return
(first called at the stash's first open), read all of its 1626 entries, and the
currency rule invalidated it on a room change and made it current again at the
reopen; the kept map followed both hand takes. The second has a partial answer:
a by-name `GridRemoveItem` on the Materials tab's grid answered `true`, emptied
the cell and the saved stash no longer held the unit, but the kept map still
carried it, so no take the kept map confirms is on record. Closing the stash
fired `SaveStash` once each time; a save after a mod take and the map before
the stash's first open were not tried.
Phase 1f (done 2026-09-23), a research round on the installed Phase 1e build
with no rebuild and no code change, asked the three things H-A still lacked:
what the kept map does when the game's own hand move takes a whole stack out of
each special tab (the control Live 1e's stale entry after `GridRemoveItem` had
none of), a by-name take of one unit that the kept map and the next stash save
both show, and the owner's Cube bracket - `SaveStash` by name in the shape the
stash close uses, and `GetItemMap(9)` by name before the stash is opened in a
launch (`### Live procedure 1f`, `### Phase 1f results`). Its session (Live
1f, two launches) found that the game's own hand move of a whole stack out of
either special tab drops the entry from the kept map, on the same map, so the
stale entry is `GridRemoveItem`'s doing (repeated here, the game's own lookup
still finding the unit); recorded the close's `SaveStash` shape (self
`Console_Save_obj`, no argument); saw a by-name `SaveStash` with the stash
closed dispatch with no new write of the file; and obtained the whole stash
map by name with `GetItemMap(9)` before any stash open, its counts equal to
the file. The owner then redirected the round and set the build design under
`## Decision gate`: at the cube, count from the map obtained by name, move the
shortfall into the bag for the game to consume, then save the stash by name.
Nothing player-visible changes yet: the
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

### Phase 1e rows

Phase 1e asks for a stash-side take that answers success (`## Decision gate`).
The one remove shape on record that answers is the cube's own
`GridRemoveItem(<grid array>, <fingerprint>)` -> `true` (Phase 1 `M-craft`);
the hand move out of the Socketable tab gave no success answer and the move out
of the Materials tab was never observed (Phase 1b), and no stack routine has
ever been seen firing. So the static search (2026-09-23, `hs-game-sdk`'s
`scripts.hpp`) took every script and struct method that takes, removes,
subtracts, splits, validates or converts inventory or stash items and that the
table did not carry - 29 rows, 252 in all, each named by its SDK constant and
hooked by the same one `craftprobe hook`. No new object's closures were added,
so the closure-coverage test is unchanged.

| Row label | Group | Runtime name (the SDK constant's value) |
|---|---|---|
| StashTakeItemOnline | script (the stash's own Take; online-suffixed - whether it runs offline is what the session measures) | `gml_Script_StashTakeItemOnline` |
| ___struct___224@StashTakeItemOnline | struct method (StashTakeItemOnline's) | `gml_Script____struct___224@StashTakeItemOnline@InventoryStashFuncs` |
| ___struct___225@StashTakeItemOnline | struct method (StashTakeItemOnline's) | `gml_Script____struct___225@StashTakeItemOnline@InventoryStashFuncs` |
| ___struct___227@StashTakeItemOnline | struct method (StashTakeItemOnline's) | `gml_Script____struct___227@StashTakeItemOnline@InventoryStashFuncs` |
| StashUniqueTakeItemOnline | script (the Unique tab's Take - a special tab's own take, for the shape) | `gml_Script_StashUniqueTakeItemOnline` |
| ___struct___229@StashUniqueTakeItemOnline | struct method (StashUniqueTakeItemOnline's) | `gml_Script____struct___229@StashUniqueTakeItemOnline@InventoryStashFuncs` |
| ___struct___230@StashUniqueTakeItemOnline | struct method (StashUniqueTakeItemOnline's) | `gml_Script____struct___230@StashUniqueTakeItemOnline@InventoryStashFuncs` |
| StashGuildTakeItemOnline | script (the guild stash's Take - whether the Take family shares one path) | `gml_Script_StashGuildTakeItemOnline` |
| StashBloodPactTakeItemOnline | script (another stash's Take, the same question) | `gml_Script_StashBloodPactTakeItemOnline` |
| StashAddItemOnline | script (the Take family's add counterpart, for the return shape) | `gml_Script_StashAddItemOnline` |
| ___struct___237@StashAddItemOnline | struct method (StashAddItemOnline's) | `gml_Script____struct___237@StashAddItemOnline@InventoryStashFuncs` |
| StashUniqueAddItemOnline | script (the Unique tab's add counterpart) | `gml_Script_StashUniqueAddItemOnline` |
| RemoveItemFromMap | script (the item map's own removal - the cube's consume was never watched for it, because it was not a row) | `gml_Script_RemoveItemFromMap` |
| OnlineRemoveItem | script (an item removal by name) | `gml_Script_OnlineRemoveItem` |
| CheckInventoryOperation | script (a check an inventory operation may pass through) | `gml_Script_CheckInventoryOperation` |
| ValidateInventory | script (a validator a take at a moment the game did not choose may trip; `ValidateInventoryNode` is a row already) | `gml_Script_ValidateInventory` |
| DetectInventoryDuplicates | script (validator; `DetectInventoryDuplicateNode` is a row already) | `gml_Script_DetectInventoryDuplicates` |
| DetectInventoryModifications | script (validator) | `gml_Script_DetectInventoryModifications` |
| ConvertOnlineStash | script (the stash to or from its online form) | `gml_Script_ConvertOnlineStash` |
| ConvertOnlineStashMap | script (the stash map to or from its online form) | `gml_Script_ConvertOnlineStashMap` |
| OnlineAddToStack | script (the stack family not yet in this table) | `gml_Script_OnlineAddToStack` |
| ___struct___16@OnlineAddToStack | struct method (OnlineAddToStack's) | `gml_Script____struct___16@OnlineAddToStack@AddToInventoryFunc` |
| InventorySplitOperation | script (a stack split - a take of N units, not a whole entry) | `gml_Script_InventorySplitOperation` |
| ___struct___158@InventorySplitOperation | struct method (InventorySplitOperation's) | `gml_Script____struct___158@InventorySplitOperation@InventoryFuncs` |
| InventorySplitDrop | script (a split's drop) | `gml_Script_InventorySplitDrop` |
| ___struct___155@InventorySplitDrop | struct method (InventorySplitDrop's) | `gml_Script____struct___155@InventorySplitDrop@InventoryFuncs` |
| s_ItemOperation | struct constructor (ran inside `InventoryGridAddToStack` in issue #9's Stage C) | `gml_Script_s_ItemOperation` |
| s_ItemGridInfo | struct constructor (an item's grid position, beside `s_ItemOperation`) | `gml_Script_s_ItemGridInfo` |
| GetOnlinePlayerItemOwner | script (the online owner lookup beside the existing `GetPlayerItemOwner` row) | `gml_Script_GetOnlinePlayerItemOwner` |

Already rows, and armed again rather than added: `InventoryStackUpdateAndRemove`
and `InventoryStackUpdateAndEdit` (with their structs), `InventoryStackHandler`,
`InventorySocketUpdateAndRemove`, `InventorySocketUpdateAndSubtract` (with its
struct), `s_PendingStackOperation`, `GetStackOpLocationFromGridType`,
`GetItemOwnerFromStackOpLocation`, `ChangeItemOwner`, `GridRemoveItem`,
`InventoryGridRemoveItem`, `CraftEditPlayerInventory` (with its struct),
`StashAddToStack`, `s_InvNode`, `InventorySwapItemsNew`, `InventorySocketItem`,
`UiASplitStack`, `SaveInventoryMap` and `SaveStash` (with its structs). Two rows
the table already carries, `GetItemMap` and `LoadStash`, are installed in
Phase 1e by `mapkeep` instead (`### Phase 1e instrument`), and `craftprobe
hook` reports them as held.

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

### Phase 1e instrument

Phase 1d kept the stash map through `craftprobe`'s own detour - an
`MmCreateHook` at `GetItemMap`'s address, research-only - which is not the
shape a player build has. Every shipped gameplay hook installs through
ForgePact's `HookOneScript`: a table swap plus an inline detour at the
function's own address, reporting whether the detour went in (repo-root
`AGENTS.md`, "Prove the Instrument"). So Phase 1e adds a second research-only
verb, `mapkeep`, that keeps the same return through that installer, and
teaches `craftprobe call` the argument shapes the take trial needs. Everything
below is in the research build only (`#ifndef FORGEPACT_RELEASE`; the player
build answers `command unavailable`), nothing calls `GetItemMap`,
`GetProfileInventoryData` or any other getter, and nothing reads a struct
layout. Every subcommand, cap and refusal in the tables above stays as it was.

**The currency rule.** A kept map is *current* only when the game's own
`GetItemMap(9)` call has returned it since the latest character load
(`LoadStash`) or room change - or since the keeper started. GameMaker reuses a
destroyed map's index, so an index that still exists (`ds_exists`) says nothing
about whether it is still the stash's map; `ds_exists` is checked as well, at
the point of use, but never makes a map current on its own. The rule is the
game-independent `CraftMatsKeptMap` in `plugin/include/ForgePact/CraftMatsMod.hpp`,
pinned by `tests/craft_mats_harness.cpp`'s `kept_map` baseline scenarios
(nothing kept is not current; an index still held after a character load or a
room change is not current) and target scenarios (the game's refresh makes it
current, again after an invalidation with the same index or a new one; a clear
is not). The plugin asks that rule at the point of use - `mapkeep stat`,
`mapkeep find` and `craftprobe call`'s `map9` forms - after reading the room
again, then checks `ds_exists` with `ds_type_map`. The frame callback only
notices a room change every 30 frames, as housekeeping (repo-root `AGENTS.md`,
"Check a Permission Where It Is Used"). The keep path reads the room too,
before it decides that a return is already current: if the game returns
`GetItemMap(9)` in a new room before the next frame poll, the change is noticed
at that moment (`room-changes=` grows), the return is kept as the refresh after
it, and its keep line reads `(was room-changed, ...)` - rather than the return
being dropped as current and the map invalidated by the poll a few frames
later, which would read as "nothing refreshed after the room change" when the
game had.

**The `a0=9` control.** Once `mapkeep` holds `GetItemMap`, craftprobe's own
detour does not attach to that row (it reports `held by mapkeep`), so the one
instrument that has seen `a0=9` calls (Live 1d) is absent from this session;
and because `HookOneScript` gives its table swap and its inline detour one dest,
the hook body cannot tell which route delivered a call - so an `a0=9` count of
zero reads "not observed", never "the player shape cannot see the map", unless
the install line reads `both-routes` and `a0=0` is non-zero in the same session.

| Addition | What it prints | Cap | Control |
|---|---|---|---|
| `mapkeep on` | installs hooks on `GetItemMap` and `LoadStash` through `HookOneScript`, named through `HeroSiege::Scripts` constants, and prints one line per hook: `mapkeep on: GetItemMap both-routes (...)` when the installer's native flag came back true, `TABLE-ONLY (...)` when only the table swap went in (the installer's own `hook GetItemMap: TABLE-ONLY (<reason>)` line, printed just before, carries the reason), or `NOT-INSTALLED`. That line is the answer to whether the player-build shape reaches the map, either way. A second `on` answers `already on`; after `off`, `on` resumes keeping | one install per hook per launch: `HookOneScript` cannot be undone, and only its first install attempts the inline detour | refuses, with nothing installed, when `craftprobe hook` already detoured either row this session - the game's address would be patched with the table entry unchanged, so the installer would attempt a second inline detour, fail, and report a false `TABLE-ONLY`. Run `mapkeep on` **before** `craftprobe hook` |
| `GetItemMap` hook body | counts calls by first argument, compared as a number (`int64:9` and `real:9.000000` alike): `a0=0`, `a0=9`, other. Forwards through the trampoline; never calls the script. Keeps an `a0=9` return that is a `ref ds_map` when it is new (the first, a different index, or the same index after an invalidation), rooted in the research global `__cp_mapkeep_9`, and feeds the core's refresh. One line for the first `a0=9` call (`mapkeep: first GetItemMap a0=9 call #<n> self=<object> room=<name> a0=<value>`), one per keep (`mapkeep: kept GetItemMap a0=9 #<n> -> ref ds_map <N> self=... room=... (was <reason>, refreshed=<k>)`) | 8 keep lines per launch, later keeps counted; no per-call line otherwise (Live 1d's `arm` budget was spent on `a0=0` calls) | its `a0=0` count is its own control: Live 1d's detour counted 28715 `a0=0` calls before any window, so a hook that sees calls at all shows a non-zero `a0=0` after the character loads |
| `LoadStash` hook body | counts, and invalidates the kept map (`character-loaded`) before the game's own call runs; one line per call, `mapkeep: LoadStash #<n> self=<object> ret=<value> ...` | 4 lines per launch | Live 1b saw two calls per character load, self `Console_Save_obj`, returning `true`; a `LoadStash calls=0` with `TABLE-ONLY` on this hook is a table-blind finding, not a pass |
| `mapkeep stat` | one line: each hook's install (`both-routes`, `TABLE-ONLY`, `NOT-INSTALLED`, `not-tried`), `keeping=on` or `off`, `a0=0 calls=<n> a0=9 calls=<n> other calls=<n> LoadStash calls=<n>`, `kept=ref ds_map <N>` (with `size=` when current) or `kept=none`, `current=yes` or `no` with `reason=` one of `none`, `not-kept`, `character-loaded`, `room-changed`, `ds-gone`, `refreshed=<k>` (keeps so far), `room-changes=`, `not-a-map=`, `first9: #<call> self=<object> room=<name>` or `none`, and `latest-keep:` the same for the latest keep | - | reads the room again and asks the currency rule, then `ds_exists`, at the moment of the command |
| `mapkeep find <class> <b>` | hook-free: walks the kept map by name (`ds_map_find_first`/`_next`/`_find_value`), only when it is current, and prints every entry whose `itemType` (or key class suffix) is `<class>` and whose `itemDefinitionStruct.b` is `<b>`: `mapkeep find: key=<key> itemType=<t> b=<b> o=<o>`, then `mapkeep find: entries walked=<n> matched=<m> (size=<N> items=<k>)`. It is how the trial names an item by its key, and how a stack shows as one entry. A non-current map prints its reason and reads nothing | 4000 entries; 40 key lines | the map's item structs are read directly, no lookup - the path `item-struct-control` vouched for in Live 1d |
| `mapkeep off`, `mapkeep clear` | `off` stops keeping (the hooks stay installed and keep counting); `clear` releases the kept map (the global set to `undefined`) and resets its currency to `not-kept` | - | - |
| `craftprobe hook` beside `mapkeep` | a row whose function `mapkeep` installed prints `craftprobe hook: <row> held by mapkeep (...)` and is counted neither as detoured nor as failed; the summary reads `<N> detoured, 0 failed, 2 held by mapkeep` on a clean session | - | `CpResolve` would refuse such an entry as not the game's code; that is not a failure of the row. The `GetItemMap` and `LoadStash` rows then report `calls=n/a` in `show all` - their counts are `mapkeep stat`'s |
| 29 new rows (`### Phase 1e rows` under `## Static search`) and the marker | the same `hook`/`arm`/`show` lines; a bare `craftprobe` answers `craftprobe: phase1e rows=252 - ...` | one detour per row | the marker is the build's control: without `phase1e` the installed plugin is not this build, and nothing from the session counts. `CheckPlayerInteraction`, as before |
| `node var` entry cap | 2000 entries per `node var` (was 1000), so the 1626-entry stash map is read whole: `entries read=<n>` with no `(cap ...)` note | 2000 | as `### Phase 1c readers` |
| `craftprobe call` forms | `call <Row> id:<n> [args ...] confirm` takes an instance number as self (after `instance_exists`, resolved by name) in place of `<Obj> <nth>`. New arguments: `fp9:<fingerprint>` - the item the game's own `GetItemFromFingerprint(fp, 9)` returns with the call's self, refused unless a plain struct; `map9` - the kept map itself; `map9:<key>` - its entry for that key (`ds_map_exists`, then `ds_map_find_value`; the key tried as text, then as a number when it is one, since the key's form is not established); `path:<root>.<a.b.c>`, the root an `<Obj>`, `global` or `id:<n>` - the value `var`'s walk reaches (`<Obj>` is its first instance), e.g. `path:id:<stashGrid>.nodeGrid`. `map9` and `map9:` are refused unless `mapkeep` calls the kept map current. Every refusal names what was supplied and ends `nothing was called`; the call line, `before:`, `dispatched -> ret=` or `NOT dispatched`, and `after:` are as before | still exactly one by-name call per command, behind `confirm` | every precondition and argument is resolved after the `confirm` gate and before the one call (`test_craftprobe_writes_are_confirm_gated`) |

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

### Live procedure 1d

Live 1d reuses the Phase 1c build under `live-operator`; the step-by-step
procedure is `### Live procedure 1` in the workorder's context file,
`.claude/workorders/forgepact-issue-14-phase1d-context.md`, which stays on the
owner's machine. It asks one question - can the special tabs' contents be read
while the stash window is closed? - and it is a reader round like Live 1c:
**no hand moves between tabs and the bag, and no craft**. What this document
fixes is its shape:

- **The Phase 1c build, no rebuild.** The installed plugin is the research DLL
  recorded under `### Phase 1c results` (ForgePact `a7795ca`); every command
  the session uses was already run in Live 1c. Nothing is installed for it, and
  nothing calls `GetItemMap`, `GetProfileInventoryData` or any other getter:
  the session reads the returns `backing` kept and instance or global variables
  only, and the one script the instrument calls is the fingerprint lookup, in
  the two (self, second argument) shapes Live 1c proved - a live stash cell with
  `a1=9` while the stash is open, the bag's grid with `a1=0` while the bag is
  open. Every `node var` carries `class=15` or `class=14`; no `backing clear`,
  no `call`.
- **Controls first**: the installed plugin's SHA-256 equals the Phase 1c hash,
  a bare `craftprobe` answers `phase1c rows=`, and `CheckPlayerInteraction` is
  non-zero once the character is loaded; without those three nothing else in
  the file counts. Auto-prospect is off, the saves are backed up first, and
  `tools/stash_tab_counts.py` reads both tabs before the launch and after it.
- **The decisive closed reads come before any window is opened in the launch,
  and again after the stash closes**: the kept `GetItemMap` returns (`backing
  dump`, then `var`/`node var` on the kept global), `New_Inventory_Data_obj`'s
  `inventorySocketGrid` and `inventoryMaterialGrid` with no lookup self, and
  after the close a `store names` pass over the globals.
- **The hand-backs are reads only**: the bag open on its Socketable tab (the
  three path controls below), the stash open on the Socketable tab and then the
  Materials tab (each open read of the kept map and the arrays, judged against
  the same-session instance read of that tab), the stash closed (the same reads
  through the same references, with `GetItemMap` armed and its per-signature
  `calls=` counted across two dumps), and one reopen for whether the kept map's
  index and call count change.
- **Cases**: the Socketable tab (the ordinary case) and the Materials tab (the
  outlier: an ordinary grid object, a different draw family).
- **The capture** is `.claude/workorders/forgepact-issue-14-phase1d-live-1.md`,
  ending with a `## Checks` section of one line per check, exactly
  `- <name> | expected: <text> | observed: <quoted, short> | pass|fail|not-observed`,
  for these seventeen checks in this order: `dll-hash`, `marker`, `control`,
  `counts-tool-before`, `node-bag-control`, `item-struct-control`,
  `fp-array-control`, `map0-identity`, `map-open-socket`, `map-open-material`,
  `invgrid-open`, `map-closed`, `invgrid-closed`, `map-live-closed`,
  `store-names-globals`, `map-reopen` and `counts-tool-after`.
- **Which control vouches for which read.** The `backing` signature lines and
  armed lines rest on `dll-hash`, `marker` and `control`. A reader miss counts
  only once the control for the path it took passed in the same session, and
  this round declares a control for each path: through an instance grid
  (`node bag`, `node socket`, `node id:`), `node-bag-control`; through `node
  var` over item structs read directly (`items=N`, no lookup),
  `item-struct-control` - `node var` over `New_Inventory_Data_obj.localItemMap`
  with `class=15`, bag open on its Socketable tab, gives the same `sum class=15
  b=<b>` pairs and `def.o` as `node bag` on that tab; through `node var` over
  fingerprint entries with lookups, `fp-array-control` - `node var` over
  `New_Inventory_Data_obj.inventorySocketGrid` with the bag grid as self and
  `a1=0` gives the same sums as `node bag` (it replaces Live 1c's failed
  `node-var-control`, whose array held the equipped items). `fp-array-control`
  vouches only for the shape it supplies - the bag grid as self and `a1=0`. A
  lookup with a stash cell as self and `a1=9` (the shape `map-open-socket` and
  `map-open-material` take if the kept map holds fingerprints) rests instead on
  the same session's `node socket a1=9` / `node id:<stashGrid> a1=9` read
  matching the count by eye, which is why those two rows require equality with
  it; with that read missing or unmatched, an `a1=9` miss is uncontrolled. A
  miss whose control did not pass is recorded "not observed (uncontrolled
  path)". A closed read that
  makes no lookup is recorded by what it lists (distinct fingerprints, classes,
  cells) against the open read of the same container, and a kept map that
  `ds_exists` no longer finds is "the kept reference is gone closed", which says
  nothing about whether the game's store is. A `fail` or `not-observed` is a
  finding, recorded under `### Phase 1d results`; no live outcome is an
  acceptance criterion.

### Live procedure 1e

Live 1e runs the Phase 1e research build (`### Phase 1e results` records its
hash) under `live-operator`; the step-by-step procedure is `### Live procedure
1` in the workorder's context file,
`.claude/workorders/forgepact-issue-14-phase1e-context.md`, which stays on the
owner's machine. It asks two questions: (1) can the stash map be kept through
`HookOneScript` - with its control, the moment in a launch the game first calls
`GetItemMap(9)`, the whole map read, and the currency rule across a room
change? (2) is there a by-name route that removes one unit of one stackable
from a stash special tab and answers success, and does the kept map show the
take? What this document fixes is its shape:

- **The Phase 1e build, installed by the owner**; the installed plugin's
  SHA-256 is checked first. Character slot 14 ("Sorak"). Auto-prospect off;
  saves backed up before and restored after; `tools/stash_tab_counts.py` reads
  both tabs before the launch and after it.
- **`mapkeep on` before `craftprobe hook`**, both before the character loads,
  so the keeper sees the load's `LoadStash` calls and every `GetItemMap` call
  of the launch, and craftprobe reports the two rows it holds as held.
- **Hand moves, one unit each, the owner stating every count by eye**: one Ol
  out of the stash's Socketable tab to the bag, one Unstable Dust out of the
  Materials tab to the bag, one unit of a bag material the owner names (X, from
  a stack of two or more) into the stash's Materials tab, and one craft at the
  cube with bag inputs - each with `craftprobe arm budget=8` before and `show`
  after, and the kept map read after each move.
- **The take trial, by name, under `confirm`, on one unit**: T1 is
  `GridRemoveItem` - the one remove shape on record that answered `true`
  (Phase 1 `M-craft`) - on the Materials tab's grid (`path:id:<stashGrid>.nodeGrid`)
  against X's fingerprint; T2 is `RemoveItemFromMap`, only if this session logs
  its argument shape (two arguments, a string second, a whole number or a
  `ref ds_map` first); T3 is a stack routine, only if one fires on a hand take
  from a stash tab this session, replayed with its logged arguments and the
  Ol's item. A refusal is quoted with what was supplied; a shape not run is
  "not observed (<why>)".
- **One room change and back**, then a reopen. Before the zone change, with the
  stash closed, `mapkeep stat`'s `refreshed=`, `room-changes=` and
  `latest-keep:` are noted; after it, `room-invalidate` quotes `current=`,
  `reason=`, `room-changes=` and `latest-keep:`. It passes when `room-changes=`
  grew and the map reads `current=no reason=room-changed` ("invalidated"), and
  also when `room-changes=` grew and the map is current again because
  `latest-keep:` names a later call - "invalidated, then refreshed by <that
  call's self>", a finding about when the game refreshes the map, not a failure
  of the rule. It fails when `room-changes=` did not grow although a zone was
  entered (the keeper missed the change), or when it grew and the map reads
  current with `latest-keep:` unchanged - the rule allows no such state.
  `map-refresh` then reopens the stash and expects `current=yes` with
  `refreshed=` grown against the stat taken before the zone change (a refresh in
  the new room may already have made the map current before the reopen).
- **Cases**: the Socketable tab (the ordinary case: a hand take, T3 if a shape
  appears) and the Materials tab (the outlier: a plain grid object, its hand
  take never observed before; T1 and T2).
- **The capture** is `.claude/workorders/forgepact-issue-14-phase1e-live-1.md`,
  ending with a `## Checks` section of one line per check, exactly
  `- <name> | expected: <text> | observed: <quoted, short> | pass|fail|not-observed`,
  for these twenty-two checks in this order: `dll-hash`, `marker`,
  `counts-tool-before`, `keeper-install`, `hook`, `control`, `keeper-control`,
  `map-first-call`, `map-kept-open`, `map-whole`, `hand-take-socket`,
  `map-follows-socket`, `hand-take-material`, `map-follows-material`,
  `hand-craft`, `take-trial-grid`, `take-trial-map`, `take-trial-stack`,
  `map-follows-trial`, `room-invalidate`, `map-refresh` and
  `counts-tool-after`.
- **Which control vouches for which read.** The armed row lines rest on
  `dll-hash`, `marker` and `control` (`CheckPlayerInteraction` non-zero after
  the load). The keeper's `a0=9` and `kept=` lines rest on `keeper-install` and
  `keeper-control` (`mapkeep stat`'s `a0=0 calls=` non-zero after the load - the
  keeper's own proof that its `GetItemMap` hook sees calls at all). A `node
  var` or `mapkeep find` read over the kept map (item structs, no lookup) rests
  on `map-kept-open`, whose same-session instance read (`node socket a1=9` /
  `node id:<stashGrid> a1=9`) rests on `node-bag-control`'s shape from Live 1d
  - a `sum` equal to a count by eye. A row that did not fire while its control
  climbed is "not observed", never "does not fire". A `fail` or `not-observed`
  is a finding, recorded under `### Phase 1e results`; no live outcome is an
  acceptance criterion.

### Live procedure 1f

Live 1f runs the Phase 1e research build again - already installed, with no
rebuild, no code change and nothing copied (`### Phase 1f results` records the
hash it reuses) - under `live-operator`; the step-by-step procedure is `### Live
procedure 1` in the workorder's context file,
`.claude/workorders/forgepact-issue-14-phase1f-context.md`, which stays on the
owner's machine. It asks three questions: (1) when the game's own hand move
takes a stack's last unit out of a special tab, does the kept `GetItemMap(9)`
map drop the key, or keep a stale one as it did after Live 1e's by-name
`GridRemoveItem`? (2) is there a by-name take of exactly one unit that the kept
map, the game's own lookup, the grid and the next stash save all agree on, and
what argument shapes do the game's own stack decrements use on a hand split and
on a craft? (3) the owner's Cube bracket: does `SaveStash` by name, in the
shape the stash close uses, write `stash.hss` with the stash open and with it
closed, and does `GetItemMap(9)` by name, with a self the game itself uses,
return the stash map before the stash is opened in a launch? What this document
fixes is its shape:

- **The Phase 1e build, two launches.** The installed plugin's SHA-256 is
  checked first; a different hash stops the session, and reinstalling is the
  owner's call. Character slot 14 ("Sorak"). Auto-prospect off. Launch A runs
  the hand moves, the take, the save trials and one craft; launch B runs only
  the `GetItemMap(9)` trial, last, so a crash there costs nothing already
  captured. The saves are backed up once before launch A and restored once
  after launch B, never between: launch B compares the map it obtains against
  the file launch A left. `tools/stash_tab_counts.py` reads both tabs before
  launch A and after it, and the file's `LastWriteTimeUtc` is read at every
  save, the game's own and ours.
- **`mapkeep on` before `craftprobe hook`**, both before the character loads,
  in each launch.
- **The whole-entry control, by hand, the owner stating every count by eye**:
  one unit of a bag material the owner names (X, from a stack of two or more
  not held in the stash's Materials tab) moved into the stash's Materials tab,
  then - its only unit - back to the bag; and the whole class-15 `b=51` stack
  (10) on the Socketable tab dragged into the bag and back (Ol is never moved).
  Each with `craftprobe arm budget=8` before and `show` after, `mapkeep find`
  before and after, and the game's own lookup `craftprobe call
  GetItemFromFingerprint id:<grid> <fingerprint> 9 confirm` before the removal
  (its control: X's struct) and after it. `mapkeep stat`'s kept index
  (`kept=ref ds_map <n>`) and `refreshed=` count are quoted before and after
  each move, and a `dropped:` or `kept:` read is the map's own behaviour only
  when both are unchanged across the move. If either changed, the game called
  `GetItemMap(9)` again during the move and `find` read the new return, so the
  text says `dropped (re-kept <old>-><new>, refreshed <a>-><b>)` or
  `kept (re-kept ...)`, and says nothing about whether the old map drops a key.
- **The save's shape, recorded before it is replayed**: the rows are armed
  before the stash's first close, so `SaveStash`'s logged line - its self, its
  `argc` and every argument - is quoted with the `s_SaveStashConstants` lines
  and the first line of each of its closures. That is the shape the save
  trials use.
- **The stack decrements, recorded and not called**: a hand split of one
  Unstable Dust out of the Materials tab, and one craft with bag inputs (one
  press; amount 2 only if the owner wants `CraftEditPlayerInventory`'s `a0`
  decoded), every non-draw row quoted with its arguments and `ret=`, and
  `craftprobe store map`'s `localItemMap` index set beside
  `CraftEditPlayerInventory`'s `a1` and the kept map's index - index equality
  only, never identity.
- **The trials, by name, under `confirm`, one unit, the Materials tab only**:
  `GridRemoveItem` exactly as Live 1e's T1, on X back in the tab as a 1-stack;
  then the named removal row the hand move of X logged, only if it logged one
  whose arguments `craftprobe call` can supply (a number, a string, an item
  struct as `fp9:`, a `ref ds_map` as `map9`). `SaveStash` in its logged shape
  with the stash open, and again with it closed only if the logged self has a
  live instance then. The open-stash call is judged against a file time read
  directly before it (T1b), with nothing between that read and the call: T1,
  read at the stash's previous close, is followed by the reopen, the split, X's
  move back in and the take, and no other writer of `stash.hss` is ruled out,
  so a file newer than T1 would not be the call's doing. T1 -> T1b is recorded
  as its own line, and a move there is recorded as another writer between the
  close and the call, without changing the verdict. A stack row replayed with the count `1`, only if one
  fired on the hand split. In launch B, with neither the stash nor the bag
  opened, `GetItemMap` with self `Console_Save_obj` (the self of the game's
  own `a0=9` call with the stash closed in Live 1d) and the one argument `9`;
  then the stash is opened, to see whether the game's own call returns the
  index obtained by name. After each trial: the kept map, the lookup, the
  grid, the owner's eye, and for a save the file. A refusal is quoted with
  what was supplied; a shape not run is "not observed (<why>)".
  `CraftEditPlayerInventory` and `CraftEditGrid` with the stash as owner are
  not called: their `a2` is a struct the game creates fresh, which `call`
  cannot build, and `a0` is undecoded.
- **What would make a trial unsafe, and the mitigation.** `SaveStash` by name
  writes the player's real `stash.hss` from whatever structure the game
  serialises (not established), a wrong self or argument is a GML error inside
  `script_execute` that ends the launch, and a save from a structure a mod take
  left stale would write the duplicate the owner reported. `GetItemMap(9)` by
  name is a getter call of the class Phase 0b's `GetProfileInventoryData` crash
  warns against: its self was measured with the stash closed after an earlier
  open, not before any open, and if the script reads window state it may
  error. Mitigation: the shapes the game itself used (the save's replayed
  after the game's own close used it in the same session), one unit,
  `confirm`, saves backed up and restored, the result read from the file
  rather than assumed, and the getter last in its own launch. A crash during a
  trial is that trial's result, recorded with the shape supplied; the rest of
  that launch's checks read `not-observed (launch ended at <check>)`.
- **Unverified going in, stated as such**: that `GetItemMap`'s arity is 1 for
  the `a0=9` call (measured for `a0=0` on the same script in Live 1c; the
  keeper does not print `argc`); that `SaveStash`'s logged arguments are kinds
  `craftprobe call` can supply (if not, both save checks read not-observed
  with the shape quoted); that the whole `b=51` stack moves without a split
  dialog (either path is recorded).
- **Cases**: the Materials tab (the outlier: a plain grid object, the only
  container a `GridRemoveItem` is recorded on; the hand control and every
  trial) and the Socketable tab (the ordinary case: an array of grid
  instances; the hand control only).
- **The capture** is `.claude/workorders/forgepact-issue-14-phase1f-live-1.md`,
  ending with a `## Checks` section of one line per check, exactly
  `- <name> | expected: <text> | observed: <quoted, short> | pass|fail|not-observed`,
  for these sixteen checks in this order: `dll-hash`, `marker`,
  `counts-tool-before`, `hook`, `control`, `last-unit-material`,
  `last-unit-socket`, `save-shape`, `stack-shapes`, `take-1stack`,
  `save-by-name`, `save-by-name-closed`, `take-stack`, `counts-tool-after`,
  `map-by-name` and `map-by-name-vs-game`. The session as it ran left this
  shape after `take-1stack`, on the owner's redirect; `### Phase 1f results`
  records which checks that skipped.
- **Which control vouches for which read.** The armed row lines rest on
  `dll-hash`, `marker` and `control` (`CheckPlayerInteraction` non-zero after
  the load). `mapkeep find` and `mapkeep stat` reads rest on `hook` (`mapkeep
  on` printing `both-routes` for both hooks) and on `control`'s `a0=0 calls=`
  being non-zero, plus the same-session `find` that read X's key at `o=1`
  before the removal; a `dropped:`/`kept:` read counts as the map's own
  behaviour only when the kept index and `refreshed=` are the same before and
  after the move (otherwise it is reported as `re-kept`). A lookup after a
  removal rests on the same call returning X's struct before it, in the same
  step. The file reads rest on `counts-tool-before` (the tool runs) and on the
  file's `LastWriteTimeUtc` moving at the game's own close in the same session;
  a by-name save's write rests on T1b, read directly before that call, never on
  T1. A row that did not fire
  while its control climbed is "not observed", never "does not fire". A `fail`
  or `not-observed` is a finding, recorded under `### Phase 1f results`; no
  live outcome is an acceptance criterion.

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
| M-bag | `craftprobe bag`: where the bag's materials tab lives, its kind and entries. **Control:** the container named holds the counts seen in the bag's materials tab by eye; a miss also needs `M-backing`'s profile-getter returns checked for the tab (rule below) | Not observed by `craftprobe bag` (`UI_Inventory_obj`, filters `mat`/`stack`/`tab`/`inv`, one level): it ran with the bag window closed (`UI_Inventory_obj has no live instance`), and the control was never run. Seen through the hooks instead: `CountInventoryItem(1, <itemType>, 1, <id>)` (self `UI_Craft_obj`, other `Player_obj`) answered plausible bag counts (153, 81, 139, ...), and `GetInventoryArray(1)` (self `Player_obj`) returned 18 fingerprint strings `0-0-<n>-<class>`, 15 non-empty - the bag's main tab, not a materials tab (Phase 1c `node-var-control`: by its classes, the equipped items, not the bag) | out.txt 86, 3731-3789; `cp_backing_GetInvArray.json` |
| M-stash-open | `craftprobe stash` with the stash window open: where the stash's material tab lives, with a count matching the tab by eye (this is `M-stash-closed`'s control) | Not observed by `craftprobe stash` / `var` (instance refs not followed): on the Socketable tab (214 Ol by eye) `UI_Stash_obj` (125 variables) reads `stashTabSelected` = `tabSelected` = -2, and holds `stashGrid`, `uiStashContainer` and `invMaterialTab` as `ref instance` values and `stashSearchGrid` as a `ref ds_grid`; the reader printed each ref's text and stopped, so no container was reached and this row's control has not passed | out.txt 180-216; 214 Ol by eye (session 2) |
| M-stash-closed | `craftprobe stash` with the stash window closed: is the material tab readable closed? **Control:** `M-stash-open` found the tab's container with a count matching by eye, and `M-backing`'s profile and stash getter returns were checked for it (rule below) | Not observed by `craftprobe stash` (`UI_Stash_obj`/`Town_Stash_obj`/globals, filters `stash`/`mat`/`tab`, one level): `UI_Stash_obj has no live instance`, `Town_Stash_obj` matched 0 of 44 variables, and the 80 matching globals include only the tab table (`defaultStashTabStruct`, `stashTabDataStruct`: tab names and indices, no contents). Rules (a) and (b) did not pass, so this is not a "not readable" | out.txt 86-171; `cp_var_stashTabDataStruct.json` |
| M-recipe | `craftprobe recipe`: the selected recipe's needs, reachable by name? | Not observed (`craftprobe recipe` was not run in Phase 1). The recipe reached the craft route as `CraftFindRecipeItems`' arguments instead: a recipe array of length 1, the bench grid array of length 6 and a `ref ds_map` (see `M-craft`) | out.txt 4057 |
| M-craft | Hand craft from the bag: which rows fire, with what self/other/arguments/returns; bag count change | Recipe 3 Ol -> 1 Old, 3 Ol in the crafting bench, crafted twice. Craft 1: `CraftFindRecipeItems` (self `UI_Craft_Recipe_List_Item_obj`, other `UI_Grid_obj`, argc 4) -> `{a=2}`; `DoCraftResult(1, 60, undefined, ds_map, false)` -> `CraftEditGrid` -> `CraftEditPlayerInventory` -> `InventoryGridRemoveItem(1, <Ol fingerprint>)` false -> `GridRemoveItem(<grid array>, <fingerprint>)` true -> `s_CraftItem(15, 2, undefined, false)` -> `GridAddItem` -> `{tabNumber 0, x 0, y 0, tabType 0, success true}`. Craft 2 with 0 Ol left: `CraftFindRecipeItems` -> `{a=1}`, `CraftEditPlayerInventory` ran only the `___struct___88` edit, no remove and no add row fired, `s_CraftItem` ran, and a second Old appeared. By eye: 3 Ol -> 0, **2 Old produced - a vanilla duplication**, reproduced in session 2 with no hook installed (`### Constraints from Phase 1`) | out.txt 4057-4081; by eye, both sessions |
| M-move-stash-to-bag | Hand click-move stash tab -> bag: rows, `success` answer, counts by eye | Not observed by the 97-row table (control non-zero, 135660; `GridAddItem`/`GridRemoveItem` fired in the same build on the craft). The owner picked the whole 214-Ol stack out of the Socketable tab, put it back, then moved 1 Ol to the bag: no add, remove or stack row fired (`StashAddToStack`, `StashGridAddItem`, `InventoryGridAddToStack`, `InventoryGridCanAddToStack`, `GridAddItem`, `GridRemoveItem`, `InventoryGridRemoveItem`, `InvGridClearItemNode`, every `Stack*` row, `ChangeItemOwner`: 0 calls). What fired: `UiAStashTabClick` (self `UI_Button_Stash_Tab_obj`, other `UI_Stash_obj`); `GetInventoryGridNode` x3 (self `UI_Stash_obj`, a0 = `0`, `0`, `-2`, each returning `ref instance 266199`); `GetStashMaxTabs` x6 (8); the `UI_Stash_obj` closures `anon@5662`, `@8574`, `@8329`, `@3835`, `@7091`, `@2245`, `@4348` (x3, a0 = the grid refs 266217, 266278, 266199) and `@7525` (x35); and the draw rows. The move runs through code outside the table | out.txt 4312-4460 |
| M-move-bag-to-stash | Hand click-move bag -> stash tab: rows, `success` answer, counts by eye | Not observed by the 97-row table (control non-zero, 135660; `GridAddItem`/`GridRemoveItem` fired in the same build on the craft). No bag -> stash-tab move was made on its own; the nearest, the whole stack put back into the Socketable tab from the cursor, fired none of the add, remove or stack rows (`M-move-stash-to-bag`) | out.txt 4312-4460 |
| M-backing | `backing dump`: what the profile and stash getters returned (json files) | Stash open, session 2: `GetStashMaxTabs` -> 8 (self `UI_Stash_Tab_Bar_Container_obj`); `GetProfileInventoryData` -> `ref instance 257725` (self `UI_Inventory_Grid_obj`; not followed); `GetPlayerItemOwner` -> 0; `GetInventoryArray` (self `Player_obj`, a0 = 1) -> the 18 bag fingerprints of `M-bag` (Phase 1c `node-var-control`: by its classes, the equipped items, not the bag). `s_StashTabData`, `LoadStash` and `SaveStash`: 0 calls after the hook, across a stash open and close too - they ran at load, before the hook, or on a route outside the table. No kept return held a special tab's contents at the depth read | out.txt 4462-4467; `cp_backing_GetStashMaxTabs.json`, `cp_backing_GetProfileInv.json`, `cp_backing_GetItemOwner.json`, `cp_backing_GetInvArray.json` |
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
| backing-per-arg | `backing dump` names >= 2 distinct first-argument signatures for `GetInventoryArray` or `CountInventoryItem` | `backing dump`: `GetInventoryArray: 1 first-argument signature(s)` - only `a0=1` (self `Player_obj`, an array of 18 bag fingerprints; Phase 1c `node-var-control`: by its classes, the equipped items, not the bag); `CountInventoryItem` kept nothing - the cube, where Phase 1 saw it called, was never opened in this session | not-observed |
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
| node-var-control | `node var` over the kept `GetInventoryArray` array, self the bag grid, equals `node bag` on the same tab | `node var global 0 __cp_backing_GetInvArray_arg0 self=id:263170 class=15`: `array len=18`, `entries read=18 fingerprints=15 (distinct 15) items=0 other=3`, classes 0 to 8 and 16 only - no class-15 fingerprint, so neither of `node bag`'s pairs (`b=1 def.o=1`, `b=3 def.o=183`) was met. The kept `GetInventoryArray(1)` return (self `Player_obj`) holds a different set from the tab on show - by its classes, the player's equipped items, not the bag (Phase 1 and 1b described it as the bag's fingerprints) - so the fail says nothing about the reader: no lookup was made, and the printed default self (`none`, a global path) does not show that `self=id:` was applied; either way the fingerprint path has no passed control from this session (capture, Step 4) | fail |
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
- **In Live 1c, the one reopen and the one tab switch each left new window
  instances.** The tab switch left the Socketable container no longer found by
  `instance_exists`, and the close and reopen made a new `UI_Stash_obj` and a
  new container, with the old ids still not found (destroyed or still
  deactivated; not distinguished). Anything that reads the tabs through the
  window's own instances can do so only while the window is open on that tab,
  as far as observed.
- **Where a closed read may still be.** `LoadStash`, a detoured row, fired at
  character load (twice here, the second returning `true` on
  `Console_Save_obj`) and, in Phase 1b, not when the window opened (Phase 1c did
  not count it again after the window opened); a store held in memory is one
  reading of that, a route outside the rows is not ruled out.
  Leads found but not read: `GetItemMap`'s return for first argument `9`
  (kept by `backing`, self a stash grid, `UI_Inventory_Grid_obj` id 266430),
  beside `GetItemMap(0)`, whose map has the same `ds_map` index (1056) as
  `localItemMap`, the bag's (indices are reused after a map is destroyed, so
  the identity is likely, not established); `New_Inventory_Data_obj`'s
  `inventorySocketGrid` and `inventoryMaterialGrid` (arrays of 6, listed by
  `store`, never summed with `node var`), whose names point at the two tabs in
  question; and the globals, of which 542 of 3563 matched the filters but only
  80 were printed, with no `store names` pass over them. `stashPersonalGrid`,
  which was read, holds classes 3 and 10, which says it holds an ordinary
  stash tab rather than a special one (not established).

### Phase 1d results

Research DLL: the Phase 1c build, unchanged - `plugin_build\BloodPactPlugin_rel.dll`
from ForgePact `a7795ca` (SHA-256
`e9d32ec3adb3238fe7c2b3284498591ae8af416da21302946e034a71712e6429`), installed as
`mods\aurie\BloodPactPlugin.dll` since Live 1c. Phase 1d rebuilt nothing and
installed nothing; `### Live procedure 1d`'s `dll-hash` check compares the
installed plugin against this hash before anything else counts. The session's
capture, `.claude/workorders/forgepact-issue-14-phase1d-live-1.md`, stays on the
owner's machine with the workorder and is cited by its step headings.

**What Live 1c left on disk.** Live 1c's last `craftprobe dump` wrote the kept
returns to `<game bin>\bp_ipc\cp_backing_*.json` (the instrument's own output).
For `GetItemMap` they record two signatures of its first argument: `a0=9`, self
`UI_Inventory_Grid_obj` id 266430, returning `ref ds_map 1050`; and `a0=0`, self
`UI_Inventory_Grid_obj` id 266363, returning `ref ds_map 1056` - the index
`localItemMap` printed in every read of that session. The kept
`GetPlayerItemOwner` return is `0`. So the first-argument-`9` map is a
different map from the bag's, and Phase 1c never summed it. Readings, not
established: `0` is the player's item-owner value and `9` the stash's (the
second argument of the stash's own `GetItemFromFingerprint` calls and of Phase
1b's `StashAddToStack`), so `GetItemMap(<owner>)` would be that owner's map from
fingerprint to item struct, `localItemMap` being owner 0's; the lower index
suggests map 1050 was made before map 1056, plausibly at character load rather
than when a window opened, but GameMaker reuses a destroyed map's index, so the
order of creation is not established; Live 1d's dump before any window (below)
shows no `GetItemMap(9)` call at all before the stash first opened, which does
not support the made-at-load reading either. Id 266430 is a cell of the
Socketable container made at Live 1c's reopen, so at least the reopen made that
call; in Live 1d the first open made one too (`map-open-socket`).
`map0-identity` settles the `0`/`localItemMap` identity at one moment.

**Live 1d, 2026-09-23.** One launch, character slot 14 ("Sorak"),
`live-operator` on the command channel (`hs-drive`) and the owner at the
keyboard only to open and close the bag and the stash and to click tabs - no
item was moved and no craft was pressed. The saves were backed up first (an
independent copy and an `hs-drive` backup) and restored in full afterwards, the
live directory hash-identical to the independent copy; no ForgePact mod was
toggled; the plugin was the build above, not reinstalled. The owner's counts by
eye: Tor 183 and a leftover Ol 1 on the bag's Socketable tab, Ol 216 on the
stash's Socketable tab, Unstable Dust 13 on its Materials tab. `backing on` ran
before the character loaded and `backing clear` never ran. Three things the
capture records beyond the procedure: `node var` over
`New_Inventory_Data_obj`'s two arrays with no `self=` made lookups anyway (on an
instance path the reader's default self is the holder, here
`New_Inventory_Data_obj`, with `a1=0` - a lookup shape outside the two the
procedure allowed; 21 and 48 lookups, no fault); the `arm budget=20` on
`GetItemMap` was spent on `a0=0` calls; and the capture calls
`stashPersonalGrid` a new candidate, which it is not - Live 1c's
`store-candidates` and `store-closed` read it.

Filled from the capture `.claude/workorders/forgepact-issue-14-phase1d-live-1.md`,
cited by its step headings, one row per check (the names and the `## Checks`
line format are fixed by `### Live procedure 1d`). A `fail` or `not-observed` is
a finding, written as "not observed by <reader> (<what it printed>)"; a reader
miss counts only once the control for the path it took passed (`### Live
procedure 1d`). Every `ds_map` index below is this launch's (1049 and 1055
here, 1050 and 1056 in Live 1c): an index is not carried across launches. The
`node var` reads of the kept stash map stopped at the reader's 1000-entry cap
of its 1626 entries, so a sum below covers those 1000; the two stacks checked
were among them.

| Check | What it measures | Observed | Verdict |
|---|---|---|---|
| dll-hash | The installed plugin's SHA-256 equals the Phase 1c hash above | `E9D32EC3...71712E6429` from `Get-FileHash` on the installed plugin with the game closed, equal to the hash above (capture, Step 0) | pass |
| marker | A bare `craftprobe` answers `phase1c rows=` (the Phase 1c build) | `craftprobe: phase1c rows=223 - research instrument ...` before the character loaded; `hook` answered `223 detoured, 0 failed` and `backing on` kept 13 rows, 4 of them per argument (capture, Step 1) | pass |
| control | `CheckPlayerInteraction` non-zero after the character loads | `CheckPlayerInteraction calls=15932` at the first `show` after the load; 82320 at the closed-window `show` (capture, Steps 2 and 6) | pass |
| counts-tool-before | `tools/stash_tab_counts.py` before launch: exit 0, a `socket_tab` and a `material_tab` line | Exit 0: `socket_tab: entries=90` with `class=15 b=1 stack=216` (Ol) and `material_tab: entries=1` with `class=14 b=50 stack=13` (Unstable Dust), the Live 1c counts (capture, Step 0) | pass |
| node-bag-control | `node bag class=15` on the bag's Socketable tab prints a `sum class=15 b=<b>` line whose `def.o` equals the Tor count the owner states | The owner saw Tor 183 and a leftover Ol 1. `node bag class=15` on the bag's `InventoryGrid` (`UI_Inventory_Grid_obj` id 262975, 15x6, 21 filled) printed `sum class=15 b=3: fingerprints=1 cells=1 def.o=183` and `b=1 ... def.o=1`, equal to both counts (capture, Step 3) | pass |
| item-struct-control | `node var` over `localItemMap` with `class=15`, bag open on its Socketable tab, gives the same `b` pairs and `def.o` as `node bag` (the control for item-struct reads) | `localItemMap is ref ds_map 1055 size=340`, `entries read=340 fingerprints=0 ... items=340`, no lookup made; `sum class=15 b=1: ... items=1 def.o=1` and `b=3: ... items=1 def.o=183`, the same pairs as `node bag`. Its other class-15 sums (`b=33`, `b=39`, `b=78`) are socketables not on that grid page (capture, Step 3) | pass |
| fp-array-control | `node var` over `inventorySocketGrid` with the bag grid as self and `a1=0` gives the same sums as `node bag` (the control for fingerprint reads) | `self=id:262975`: the head line named `GetItemFromFingerprint(fp, 0) with self=UI_Inventory_Grid_obj ... id=262975`; `inventorySocketGrid is array len=6`, `entries read=90 fingerprints=21`, and `b=1 def.o=1`, `b=3 def.o=183` plus the same unnamed `b=109`/`110`/`111`/`136` sums as `node bag`, no miss line (capture, Step 3) | pass |
| map0-identity | With no window open, `localItemMap`'s `ds_map` index equals the kept `GetItemMap(0)` return's | Before any window this launch: `localItemMap` `ref ds_map 1055 map size=340` and `__cp_backing_GetItemMap_arg0` `ref ds_map 1055 map size=340`, read with nothing in between. That dump had one signature, `a0=0` (`calls=28715`), and no `a0=9` (capture, Step 2) | pass |
| map-open-socket | The kept `GetItemMap(9)` map, stash open on the Socketable tab, gives `sum class=15 b=1` with `def.o` equal to the Ol count and to `node socket a1=9`'s | The first stash open of the launch; the owner saw Ol 216. The dump now had a second signature, `GetItemMap a0=9 ... calls=260273 ... selves=UI_Inventory_Grid_obj,UI_Stash_obj ... ref ds_map 1049 -> cp_backing_GetItemMap_arg1.json`. `node socket a1=9` (`UI_Stash_Socket_New_obj` id 263747, 90 filled) gave `sum class=15 b=1 ... def.o=216`. `node var global 0 __cp_backing_GetItemMap_arg1 class=15`: `ref ds_map 1049 size=1626`, `entries read=1000 (cap 1000) fingerprints=0 ... items=1000`, no lookup (item structs), `sum class=15 b=1: ... items=1 def.o=216`, equal to both (capture, Step 4) | pass |
| map-open-material | The same map, stash open on the Materials tab, gives `sum class=14 b=50` with `def.o` equal to the Unstable Dust count and to `node id:<stashGrid> a1=9`'s | The owner saw Unstable Dust 13. `node id:263706 a1=9` (the tab's `stashGrid`, 17x18, 1 filled) gave `sum class=14 b=50 ... def.o=13`. The `a0=9` signature still returned `ref ds_map 1049` (`calls=2546743`, latest self that grid), and `node var` over the same kept global with `class=14` (item structs, 1000 of 1626 read) gave `sum class=14 b=50: ... items=1 def.o=13`, equal to both (capture, Step 5) | pass |
| invgrid-open | `inventorySocketGrid` / `inventoryMaterialGrid` with a window open: a sum equal to a count by eye, and which window's tabs the arrays hold | Not observed by `node var` for the stash's tabs: with the stash open on each tab, both arrays read exactly as before any window - `inventorySocketGrid` the bag's Tor (`b=3 def.o=183`) and leftover Ol (`b=1 def.o=1`), never the stash's 216; `inventoryMaterialGrid` 73 fingerprints over 42 class-14 base ids, one of them `b=2 def.o=13` on fingerprint `0-0-209490505056-14`, not the stash's Unstable Dust (`b=50`, fingerprint `0-0-201019147376-14`). The arrays hold the bag's two special tabs, as `fp-array-control` showed for the Socketable one (capture, Steps 4 and 5) | not-observed |
| map-closed | The kept `GetItemMap(9)` map read through the same reference with the stash closed (and before any window, if kept then): the same two sums as the open reads | Before any window: nothing to read, no `a0=9` signature had been kept (see `map0-identity`). After the close (`UI_Stash_obj has no live instance`): `var global 0 __cp_backing_GetItemMap_arg1` still `ref ds_map 1049 map size=1626`; `node var` with `class=15` and then `class=14`, item structs with no lookup (1000 of 1626 read), gave `sum class=15 b=1: ... items=1 def.o=216` and `sum class=14 b=50: ... items=1 def.o=13`, equal to the open reads and the counts by eye; `item-struct-control` passed for this path (capture, Step 6) | pass |
| invgrid-closed | The two arrays read with the stash closed (and before any window): the same entries and sums or fingerprints as the open read that matched a count | Not observed by `node var` for the stash's tabs: before any window and after the close, both arrays read the same as with the stash open (the bag's `b=3 def.o=183` and `b=1 def.o=1`; the same 73 material fingerprints), and no open read of them ever matched a stash count. They resolved the bag's counts with every window closed, through the holder-as-self lookup the capture notes above (capture, Steps 2 and 6) | not-observed |
| map-live-closed | The `GetItemMap a0=9` per-signature `calls=` across two dumps with the stash closed, any armed `a0=int64:9` call and its returned map index | The first closed dump read `calls=2573302`, 26559 more than the last open read, latest self `Console_Save_obj` (those calls came between the Materials-tab read and that dump; which of them came after the close is not on record). The second closed dump, after the `store` runs: `calls=2573302`, a difference of 0, same `ref ds_map 1049` - no `GetItemMap(9)` call was counted in between, by the same counter that grew before the close and at the reopen. Armed line: not observed (budget spent) - all 20 logged `GetItemMap` calls were `a0=0` (`logged=20 unlogged=47253`); the whole-log search for `a0=int64:9` found none, but an armed `UI_Inventory_Grid_obj` call logged its first argument as `real:0.000000`, so that search would miss a `real` 9. So the armed half of this check (a per-call `a0=9` line) is not observed, its budget spent, and the verdict rests on the per-signature counter alone (capture, Steps 6 and 7) | pass |
| store-names-globals | The six `store names` runs and two `store` runs over the globals with the stash closed: the names found, and which variable (if any) holds the kept map | All eight printed their own `global: 3582 globals` line. Matches: `stash` 32, `socket` 41, `material` 6, `map` 99, `inv` 144, `tab` 51. On `New_Inventory_Data_obj`: `stashPersonalGrid` (`array len=18 len0=17`, its first rows all `undefined`), the two special-tab arrays, `localItemMap`, `localIncarnationSocketItemArray`, `inventoryTab` and the other bag grids. `store stash` printed `stashSocketRowMap` (`ref ds_map 36`, 106 entries) and `stashTabDataStruct` (a struct of 8); `store map` printed 80 of its 99. No printed value is `ref ds_map 1049`. Listed by `store names` and never printed by a `store`: 19 of the `map` matches and the `socket`, `material`, `inv` and `tab` globals, which the capture does not name one by one (capture, Step 7) | pass |
| map-reopen | At the stash's reopen: the `a0=9` map index and `calls=` against the closed reads | Reopened on the Socketable tab: `GetItemMap a0=9 ... calls=2707773 ... self=UI_Inventory_Grid_obj ... ref ds_map 1049`, and the kept global `ref ds_map 1049 map size=1626` - the same index as before the close (indices are reused, so that it is the same map is not established); `calls=` grew from 2573302. The final dump, the stash still open, read `calls=2849013`, still 1049 (capture, Steps 8 and 9) | pass |
| counts-tool-after | `tools/stash_tab_counts.py` after the game exits: the socket and material lines equal the counts the owner stated (nothing was moved) | After a graceful exit: `class=15 b=1 stack=216` and `class=14 b=50 stack=13`, equal to the owner's counts - nothing moved. The exit save rewrote `herosiege13.hss`, `inventory_order_13.hss`, `shop.ini` and `stash.hss` (`hs_saves_inspect`); the saves were restored from the session's backup afterwards (capture, Step 9) | pass |

**What Phase 1d settles, and what it leaves open.**

- **A special-tab count is readable with the stash window closed, by name.**
  The one reader that reproduced both counts closed is `node var` over the map
  the game's own `GetItemMap(9)` call returned, kept by the instrument's detour
  on `gml_Script_GetItemMap` (`__cp_backing_GetItemMap_arg1`, `ds_map` 1049 this
  launch): a map of item structs, read directly with no lookup, which gave Ol
  216 and Unstable Dust 13 open and again closed through the same reference,
  with `item-struct-control` passed in the same session. Nothing called
  `GetItemMap` but the game. `New_Inventory_Data_obj`'s `inventorySocketGrid`
  holds the bag's Socketable tab, not the stash's (controlled:
  `fp-array-control`); `inventoryMaterialGrid` is not controlled - shown not
  to be the stash's Materials tab, not shown to be the bag's.
- **The map's index outlives the window.** Its index stayed 1049 through the
  close and the reopen, and the game's calls at the reopen returned the same
  index again, where Live 1c's close and reopen left new window instances.
  GameMaker reuses a destroyed map's index, so this is the same index, not an
  established identity. Readings, not established:
  that the map is the game's store for owner `9`, the stash, which the window
  reads rather than owns; and that it reflects a change to a tab - no item was
  moved this session, so whether a take shows up in it is not observed.
- **The map was not observed before the stash's first open.** With `backing on`
  from before the character loaded, the dump before any window held only
  `GetItemMap(0)`; `GetItemMap(9)` first appeared once the stash had opened for
  the first time that launch (selves `UI_Inventory_Grid_obj` and `UI_Stash_obj`).
  So the first open calls it, which answers the procedure's open question (Live
  1c had it only from the reopen); whether the map exists before the game first
  asks for it is not observed, since nothing asked.
- **While closed and idle, no `GetItemMap(9)` call was counted.** Between the
  two closed dumps its `calls=` stayed at 2573302, a counter that grew before
  the close and at the reopen; the calls just before included one from
  `Console_Save_obj`, the latest self at the first closed dump. The armed per-call log saw no `a0=9` line, but its budget was spent on
  `a0=0` calls, so that half is not observed (budget spent).
- **No global holds the map, as far as printed.** None of the printed `store`
  values is `ref ds_map 1049`; 19 `map` matches and the `socket`, `material`,
  `inv` and `tab` matches were listed by name only. The map was reached only as
  the return of the game's call.

### Phase 1e results

Research DLL: `plugin_build\BloodPactPlugin_rel.dll`, built 2026-09-23 with
`plugin_build\build.bat dev` from ForgePact `ba3046d` (SHA-256
`806d2562689db855de776f79e6df88d19f9ea4783cf56d24546d69398262291c`). The
player build (`build.bat release`) from the same commit carries no `phase1e`,
`mapkeep` or `craftprobe` string. It was installed as
`mods\aurie\BloodPactPlugin.dll` for Live 1e; `### Live procedure 1e`'s
`dll-hash` check compared the installed plugin against this hash before
anything else counted, so the Phase 1c build still installed from Live 1c/1d
(`e9d32ec3...`) fails it on purpose, as does the first Phase 1e build (from
`5aaeaa2`, before the keeper read the room on its keep path), which this one
supersedes.

**Live 1e, 2026-09-23.** One launch, character slot 14 ("Sorak"),
`live-operator` on the command channel (`hs-drive`) and the owner at the
keyboard for the hand moves, the one craft, the zone change and the counts by
eye. The saves were backed up first (an independent copy and an `hs-drive`
backup) and restored in full afterwards, the live directory hash-identical to
the independent copy; no ForgePact mod was toggled. `mapkeep on` ran before
`craftprobe hook`, as the procedure requires. The owner's counts by eye: Ol 216
on the stash's Socketable tab and Unstable Dust 13 on its Materials tab at the
start. The trial material X was Greater Unstable Dust (the owner named no other
material), one unit of which the owner moved from the bag into the stash's
Materials tab, where it sat alone at `b=51` under fingerprint
`0-0-212338259001-14`. Three things the capture records beyond the procedure:
before the real Unstable Dust take the owner picked the stack up by accident
and put it back on the same cell, and the armed log holds both events, which
the capture separates by call order; the dispatch first said no craft, and the
owner asked for it after the stash-save counts below were taken, so `hand-craft`
ran after the first stash close; and the recipe the owner crafted was labelled
"Satanic Crystal Fragment" in the game's recipe window but produced a Destiny
Shard Fragment - the game's own label, recorded as seen.

Filled from the capture, `.claude/workorders/forgepact-issue-14-phase1e-live-1.md`,
cited by its step headings, one row per check, in the procedure's order. A
`fail` or `not-observed` is a finding; a rejected or refused call shape is
recorded with what was supplied, and a shape not run is "not observed
(<why>)". Every `ds_map` index below is this launch's (1050); an index is not
carried across launches.

| Check | What it measures | Observed | Verdict |
|---|---|---|---|
| dll-hash | The installed plugin's SHA-256 equals the Phase 1e hash above | `806D2562...62291C` from `Get-FileHash` on the installed plugin with the game closed, equal to the hash above (capture, Step 0) | pass |
| marker | A bare `craftprobe` answers `phase1e rows=` (this build) | `craftprobe: phase1e rows=252 - research instrument ...` before the character loaded (capture, Step 1) | pass |
| counts-tool-before | `tools/stash_tab_counts.py` before launch: exit 0, a `socket_tab` and a `material_tab` line; X not yet on the Materials tab | Exit 0: `class=15 b=1 stack=216` (Ol) and `class=14 b=50 stack=13` (Unstable Dust), the Live 1d counts; the Materials tab's one line, no `b=51` (capture, Step 0) | pass |
| keeper-install | `mapkeep on` prints one line each for `GetItemMap` and `LoadStash`: `both-routes` or `TABLE-ONLY (...)` - either wording is the finding | `mapkeep on: GetItemMap both-routes (table swap and inline detour at the function's own address)` and the same for `LoadStash` (capture, Step 1) | pass |
| hook | `craftprobe hook` after `mapkeep on`: `0 failed`, with `GetItemMap` and `LoadStash` reported as held by mapkeep | `craftprobe hook: 250 detoured, 0 failed, 2 held by mapkeep.`, the two held lines naming `LoadStash` and `GetItemMap` (capture, Step 1) | pass |
| control | `CheckPlayerInteraction` non-zero after the character loads | `CheckPlayerInteraction calls=16702` at the first `show` after the load, 973980 by the end (capture, Step 2) | pass |
| keeper-control | `mapkeep stat` after the load: `a0=0 calls=` non-zero (the keeper's hook sees calls), and `LoadStash calls=` with its install wording | `GetItemMap=both-routes LoadStash=both-routes ... a0=0 calls=29879 a0=9 calls=0 ... LoadStash calls=2`; `a0=0` reached 7595317 by the end, and `LoadStash` stayed at 2 all session, through two stash closes and two reopens (capture, Step 2 and Step 11) | pass |
| map-first-call | The moment of the first `GetItemMap(9)` call in the launch: `first9:` with its call, self and room - at load, at the first stash open, or elsewhere | After the load, no window open: `a0=9 calls=0`, `kept=none`, `first9: none`. At the stash's first open, on the Socketable tab: `first9: #1 self=UI_Inventory_Grid_obj room=Town_01_rm`, `a0=9 calls=168237` - the first open made the first call, as in Live 1d (capture, Step 2 and Hand-back A) | pass |
| map-kept-open | Stash open on the Socketable tab: `mapkeep stat` reads current, and `node var` over `__cp_mapkeep_9` gives the Ol `def.o` equal to the count by eye and to `node socket a1=9`'s | The owner saw Ol 216. `mapkeep stat`: `kept=ref ds_map 1050 size=1626 current=yes reason=none refreshed=1`; `node socket a1=9` gave `sum class=15 b=1 ... def.o=216`, and `node var global 0 __cp_mapkeep_9 class=15`, item structs with no lookup, gave `sum class=15 b=1: ... items=1 def.o=216`, equal to both. The map's keys print as fingerprints (`0-0-<n>-<class>`) (capture, Hand-back A) | pass |
| map-whole | The same read's `entries read=` equals the map's `size=` with no cap note, and `mapkeep find 15 1` prints exactly one key | `entries read=1626` against `size=1626`, no cap note; `mapkeep find 15 1` printed one key, `0-0-200922080099-15 ... b=1 o=216`, and `entries walked=1626 matched=1` (capture, Hand-back A) | pass |
| hand-take-socket | The rows that fire when the owner moves one Ol from the stash's Socketable tab to the bag, with their `ret=` | The owner moved 1 Ol (stash 215, bag 2). Non-draw rows, one call each: `UiAInventorySocketTabClick`, `UiASplitStack`, `InventorySwapItemsNew` (`argc=3`) and `InventorySocketItem` (`argc=1`, `ret=bool:false`), the others `ret=undefined`; no Phase 1e row, and no `true` or struct with `success` (capture, Hand-back B) | pass |
| map-follows-socket | After that take, the kept map's Ol entry reads one less (or the map is not current - the reason quoted) | `mapkeep stat` still `current=yes` with `refreshed=1`, so no new `GetItemMap(9)` return was kept; `mapkeep find 15 1` read `o=215` on the same key, and `node var` `def.o=215` - the game changed the map it had returned (capture, Hand-back B) | pass |
| hand-take-material | The rows that fire when the owner moves one Unstable Dust from the stash's Materials tab to the bag, with their `ret=` | The owner saw 13, then moved 1 (stash 12, bag 1). The real take: `UI_Split_Stack_obj anon@1285`, `UiASplitStack`, then `s_InvNode` with the bag's grid as `other` and a cell `(7, 1)`; all `ret=undefined`. The accidental put-back before it ran `s_InvNode` on the stash grid's cell `(0, 0)` and a grid drop check `UI_Inventory_Grid_obj anon@15345` with `ret=bool:false`. `StashAddToStack` and `InvGridClearItemNode` had 0 calls; no Phase 1e row fired (capture, Hand-back C continued) | pass |
| map-follows-material | After that take, the kept map's Unstable Dust entry reads one less | `node id:262747 a1=9` `def.o=12`; `mapkeep stat` `current=yes refreshed=1`; `mapkeep find 14 50` `o=12` on `0-0-201019147376-14`, `entries walked=1626 matched=1`; `node var` `def.o=12` (capture, Hand-back C continued). The move of X into the stash next grew the map from 1626 to 1627 entries with X's key at `o=1`, again with no new keep (capture, Hand-back D) | pass |
| hand-craft | The rows that fire on one craft with bag inputs, with their arguments and `ret=` (the self of `GridRemoveItem`, the shape of `RemoveItemFromMap`) | One press: 5 Greater Unstable Dust from the bag (154 to 149), a Destiny Shard Fragment left in the cube. Fired once each: `CraftFindRecipeItems`, `DoCraftResult`, `CraftEditGrid`, `CraftEditPlayerInventory` (`a2` a struct with empty `edit`, `remove` and `log_ids`), `s_CraftItem`, `GetInventoryGridNode`, `GridAddItem` and `GridAddToStack` (each `ret=` a placement struct with `tabNumber` and `x`), `s_InvNode`; `s_ItemOperation` twice. `InventoryGridRemoveItem`, `GridRemoveItem`, `RemoveItemFromMap`, `SaveInventoryMap` and `CheckInventoryOperation` had 0 calls, so no `GridRemoveItem` self was recorded (capture, Hand-back E) | pass |
| take-trial-grid | T1: `craftprobe call GridRemoveItem` on the Materials tab's `nodeGrid` against X's fingerprint, under `confirm`: dispatched with a `ret=`, or refused with what was supplied | Stash reopened on the Materials tab (new grid instance 269846, X still `def.o=1`, `mapkeep stat` `current=yes`). Supplied: self and other the tab's grid (`id:269846`, since `hand-craft` recorded no self), `a0` that grid's `nodeGrid` (`array len=18 len0=17`), `a1` X's fingerprint as a string. `dispatched -> ret=bool:true`; X's cell read empty afterwards and the owner saw X gone, the bag unchanged at 149 (capture, T1) | pass |
| take-trial-map | T2: `craftprobe call RemoveItemFromMap` with the logged shape, only when this session logged one | Not observed (no recorded shape): `RemoveItemFromMap` had 0 calls in every armed window and all session, so T2 was not run (capture, take-trial-map (T2) and take-trial-stack (T3)) | not-observed |
| take-trial-stack | T3: a stack routine replayed with its logged arguments and the Ol's item, only when one fired on a hand take from a stash tab | Not observed (no stack row fired on a hand take): both hand takes went through `UiASplitStack` and `s_InvNode`, and the five stack rows had 0 calls all session, so T3 was not run (capture, the same heading) | not-observed |
| map-follows-trial | After a take that answered `true` and emptied X's cell, X's key is no longer in the kept map | Not followed: right after T1, `mapkeep find 14 51` still printed `key=0-0-212338259001-14 ... b=51 o=1`, the map still 1627 entries, `node var` `def.o=1`. After the zone change and a new keep at the reopen (`refreshed=2`), the same key with `o=1`, while the grid and the owner's eye still showed X gone (capture, map-follows-trial and map-refresh) | fail |
| room-invalidate | After a zone change and back, `mapkeep stat`'s `room-changes=` has grown against the stat before the change, and the map reads `current=no reason=room-changed` ("invalidated") or current again with `latest-keep:` naming a later call ("invalidated, then refreshed by <self>"); current with `latest-keep:` unchanged is a broken rule | Invalidated: before the change `current=yes`, `refreshed=1`, `room-changes=2`, `latest-keep: #1 self=UI_Inventory_Grid_obj room=Town_01_rm`; after the owner entered a zone and came back, `current=no reason=room-changed refreshed=1 room-changes=4`, `latest-keep:` unchanged (capture, Hand-back G) | pass |
| map-refresh | After the stash reopens, `mapkeep stat` reads current with `refreshed=` grown against the stat before the zone change; the index recorded as an index, never as identity | `kept=ref ds_map 1050 size=1627 current=yes reason=none refreshed=2 room-changes=4`, `latest-keep: #3264053 self=UI_Stash_obj room=Town_01_rm` - the same index as before the change, and the same size (capture, map-refresh) | pass |
| counts-tool-after | `tools/stash_tab_counts.py` after the game exits: the Ol and Unstable Dust stacks equal the last counts stated, and X's line absent or present as the trial left it | After a graceful exit: `class=15 b=1 stack=215` and `class=14 b=50 stack=12`, equal to the owner's last counts, and no `class=14 b=51` line - X absent from `stash.hss`, as the grid left it, not as the kept map did. The exit changed `herosiege13.hss`, `inventory_order_13.hss`, `shop.ini` and `stash.hss` (`hs_saves_inspect`) (capture, Step 11) | pass |

**The stash's save, measured around two closes.** The owner reported, during
the session, that the stash seems to be read and written only when its window
opens and closes, and that after a game crash with an item moved out of the
stash into the bag and the stash still open, the item was sometimes duplicated
and sometimes corrupted. Live 1e counted the save rows around both closes
(capture, the three "Owner note: stash save timing" headings): with the stash
open, `SaveStash` and its five closures stood at 0; each close raised `SaveStash`
by 1, its closures by several hundred calls between them, and
`s_SaveStashConstants` by 2. `LoadStash` stayed at 2, its value after the
character load, through both closes and both reopens. So what was measured is
`SaveStash` observed once at each close, and `LoadStash` - installed through
both routes, its two load-time calls the control - not observed at either
reopen. No other writer of `stash.hss` was ruled out: the exit rewrote
`stash.hss` as well (`hs_saves_inspect`, `counts-tool-after`), and nothing
watched the file between the closes. The crash report itself is the owner's,
not measured here: a take from the stash was seen reaching the file only
through a save the game made (a close's `SaveStash`, or the exit), and a crash
before such a save would leave the two save files out of step.

**Question 1: the map in the player-build shape.** `HookOneScript`, the
installer every shipped gameplay hook uses, reported `both-routes` on
`GetItemMap` and on `LoadStash` (`keeper-install`), and its control counted the
game's `a0=0` calls from the load on (`keeper-control`). The first `a0=9` call
came at the stash's first open, with self `UI_Inventory_Grid_obj` in
`Town_01_rm`, and none before (`map-first-call`). The kept map was read whole,
1626 of 1626 entries, its keys printed as fingerprints, and each stack looked
up (Ol, Unstable Dust, X) matched exactly one entry (`map-whole`).
Its entries followed both hand takes and the move of X into the stash with no
new `GetItemMap(9)` return kept in between, so the game changes the map it
returned rather than handing out a new one (`map-follows-socket`,
`map-follows-material`). The currency rule behaved as written: the zone change
invalidated the map (`room-changes=` 2 to 4, `current=no reason=room-changed`),
and the game's own call at the reopen, with self `UI_Stash_obj`, made it current
again at the same index (`room-invalidate`, `map-refresh`). Not observed: the
map before the stash's first open in a launch (nothing asked for it), and the
map after a new launch.

**Question 2: a stash-side take that answers.** On the Socketable tab the hand
take ran `UiASplitStack`, `InventorySwapItemsNew` and `InventorySocketItem`
(`ret=bool:false`, as Phase 1b saw); on the Materials tab it ran
`UiASplitStack` and `s_InvNode` - the first observation of that leg. No row
answered `true` on either hand take, none of the 29 Phase 1e rows fired on them
or on the craft, and the craft itself removed its inputs without
`GridRemoveItem`, `InventoryGridRemoveItem` or `RemoveItemFromMap`. The one
by-name trial that ran, T1, called `GridRemoveItem` with the Materials tab's
grid as self, its `nodeGrid` as the array and X's fingerprint, and it answered
`true`: the cell emptied, the owner saw X gone, and after the exit `stash.hss`
held no X. The kept map did not follow: X's entry still read `o=1` after the
take, and again after the game's own call re-kept the map at the reopen. T2 and
T3 were not run, for want of a recorded shape. So one by-name remove answers
success and reaches the saved file, but the map the game returns kept a unit the
grid no longer held. Which structure `SaveStash` writes from is not
established; in this one trial the grid's state was saved.

**What Phase 1e settles, and what it leaves open.**

- **The map is reachable in the player-build shape, once the stash has been
  opened.** `HookOneScript` installs both routes on `GetItemMap`, keeps the
  game's own `GetItemMap(9)` return, and the kept map is read whole; the
  currency rule `CraftMatsKeptMap` encodes held across a room change and a
  reopen. Before the stash's first open in a launch there was no return to keep. The instrument's control for `a0=9` is the
  install line plus a non-zero `a0=0` in the same session, both of which held.
- **The kept map followed every game move Live 1e made; after the one by-name
  `GridRemoveItem` it kept the removed entry.** The moves it followed lowered a
  stack's `o` (the two hand takes) or added a key (the move of X in), each with
  no new keep. T1 was the only removal of a whole entry, and it was by name:
  after it the map kept X's entry, and the game's next `GetItemMap(9)` return
  still carried it. What the map does when the game's own route removes a whole
  entry is not observed (Live 1e made no such move), so this does not show
  whether the stale entry is `GridRemoveItem`'s doing or the map's behaviour on
  any removal - Phase 1f's first question. A mod that counted from this map
  after such a take would count a unit that is gone - the duplication risk the
  decision core's confirming re-read exists to catch, and why that re-read
  cannot be this map after this call.
- **`SaveStash` was observed at each close.** Each close fired `SaveStash`
  once; `LoadStash` was not observed at either reopen, with its two load-time
  calls as the control. The saved file matched the grid after T1. No other
  writer of `stash.hss` was ruled out (the exit rewrote it too). Not
  established: what `SaveStash` reads, or that it would match the grid for any
  other take.
- **The owner's crash report is a design constraint.** A stash-side take was
  seen reaching `stash.hss` only through a save the game made (a close's
  `SaveStash`, or the exit), and a crash before such a save can leave the item
  in both save files. A take made at the cube with the stash window closed
  would widen that window, so a design has to say when the stash is saved after
  a mod take.
- **The owner's design proposal is the leading persistence candidate.** Bracket
  the cube as the stash window is bracketed: obtain the stash map when the cube
  opens, without drawing the window, and have the game save the stash when the
  cube closes or right after a mod take. Each half needs a by-name call the game
  normally makes itself - `GetItemMap(9)` with a self the game uses, and
  `SaveStash` with the argument shape the stash close uses - and each is a
  getter or write call not yet trialled.

### Phase 1f results

Research DLL: the Phase 1e research build, reused unchanged -
`plugin_build\BloodPactPlugin_rel.dll`, built with `plugin_build\build.bat dev`
from ForgePact `ba3046d` (SHA-256
`806d2562689db855de776f79e6df88d19f9ea4783cf56d24546d69398262291c`), the file
`### Phase 1e results` records. Phase 1f added no row, no command and no
marker, and nothing was rebuilt or installed, so the build control is
`dll-hash` against this hash plus the `phase1e rows=252` marker.
`### Live procedure 1f` gives the session's shape.

**Live 1f, 2026-09-23.** Two launches, character slot 14 ("Sorak"),
`live-operator` on the command channel (`hs-drive`) and the owner at the
keyboard for the hand moves, the stash's closes and opens, and the counts by
eye. The saves were backed up first (an independent copy and the `hs-drive`
backup `20260923T153003Z_issue14-live-1f`) and restored once, after launch B,
from that backup, the live directory then hash-identical to the independent
copy (the workorder's Log). `mapkeep on` ran before the character loaded in
both launches, and before `craftprobe hook` in launch A. The trial material X
was again Greater Unstable Dust (class 14, `b=51`), moved into the stash's
Materials tab twice, each time arriving as a new one-unit item with its own
fingerprint (`0-0-212344781000-14`, then `0-0-212345899002-14`). The Socketable
stack was the class-15 `b=51` stack of 10, Pristine Ruby by the item editor's
catalog. Two things the capture records beyond the procedure. The move of X
into the tab went through the split dialog and `s_InvNode` placing a new node
on the stash grid, with `StashAddToStack` and `InvGridClearItemNode` at 0 calls,
which is not Live 1e's shape. And X's move back out landed as its own bag
stack, which the owner then merged by hand into the bag's stack inside the
same armed window, so that window's rows mix the two moves.

**The owner's redirect.** After `take-1stack` the owner redirected the rest of
the session (the workorder's Log, `### Owner redirect`, verbatim there): a mod
should move materials rather than remove them, and since the stash and the
cube cannot be open together, nothing more is to be tested with the stash
open. So the stash-open save trial (`save-by-name`), the craft (the second half
of `stack-shapes`) and the stack replay (`take-stack`) were not run, and read
`not-observed (skipped by owner redirect)`. The closed-stash save trial ran,
then launch B as written. `counts-tool-after` was read once, after launch B's
exit rather than launch A's; `map-by-name` compared its counts against the
file as read right after launch A's last stash close, and the reading after
launch B gave the same lines.

Filled from the capture, `.claude/workorders/forgepact-issue-14-phase1f-live-1.md`,
cited by its step headings, one row per check, in the procedure's order. A
`fail` or `not-observed` is a finding; a rejected or refused call shape is
recorded with what was supplied, and a shape not run is "not observed
(<why>)". `ds_map` indices are this session's (1049 in launch A, 1050 in
launch B) and are compared as indices only, never as identity.

| Check | What it measures | Observed | Verdict |
|---|---|---|---|
| dll-hash | The installed plugin's SHA-256, read with the game closed, equals the hash above | `806D2562...62291C` from `Get-FileHash` on the installed plugin with the game closed, equal to the hash above (capture, Step 0) | pass |
| marker | A bare `craftprobe` answers `phase1e rows=252` (this build) | `craftprobe: phase1e rows=252 - research instrument ...` before the character loaded, in launch A and again in launch B (capture, Step 1; Launch B) | pass |
| counts-tool-before | `tools/stash_tab_counts.py` before launch A: exit 0, the Socketable tab's `b=1` (Ol) and `b=51` lines and the Materials tab's `b=50` (Unstable Dust) line; no `class=14 b=51` line | Exit 0: `class=14 b=50 stack=13`, `class=15 b=1 stack=216`, `class=15 b=51 stack=10`; no `class=14 b=51` line. File time T0 `2026-09-22T22:18:18.9022398Z` (capture, Step 0) | pass |
| hook | `mapkeep on` prints `GetItemMap` and `LoadStash` `both-routes` (a `TABLE-ONLY` is quoted as the finding), then `craftprobe hook` reads `0 failed` with two rows held by mapkeep | `mapkeep on: GetItemMap both-routes (table swap and inline detour at the function's own address)` and the same for `LoadStash`; `craftprobe hook: 250 detoured, 0 failed, 2 held by mapkeep.`, the two held lines naming `LoadStash` and `GetItemMap`. Launch B's `mapkeep on` printed both `both-routes` lines again (capture, Step 1; Launch B) | pass |
| control | After the load: `CheckPlayerInteraction calls=` and `mapkeep stat`'s `a0=0 calls=` both non-zero; `a0=9 calls=` and `first9:` quoted as launch A's baseline | `CheckPlayerInteraction calls=15358` and `a0=0 calls=27663` after the load, both non-zero at every later read; baseline `a0=9 calls=0`, `first9: none`, `kept=none`. Launch B after its load: `a0=0 calls=24011`, `a0=9 calls=0` (capture, Step 2; Launch B) | pass |
| last-unit-material | X, its only unit, moved by hand from the Materials tab to the bag: `mapkeep stat`'s kept index and `refreshed=` before and after the move; the kept map `dropped:` or `kept:` X's key (the map's own behaviour only when both are unchanged, otherwise `re-kept`), the game's own lookup after it, and the named rows that fired, with `ret=` | `dropped:` on the same map: `kept=ref ds_map 1049` and `refreshed=1` before and after, `size=` 1627 to 1626, `mapkeep find 14 51` `matched=0`, `node var` with no `b=51` sum; X's cell gone from the grid; `GetItemFromFingerprint` with X's fingerprint and `9` returned `undefined` (the same call before the move returned X's struct). Rows: `s_InvNode` twice, `GetItemPreferredGrid`, `s_ItemGridInfo`, `GridAddItem` (a placement struct with `success=true`), `s_ItemOperation` (`argc=5`), `ChangeItemOwner` (self the bag's grid, `a0=int64:9 a1=int64:0`, `a2` X's fingerprint), `InventorySwapItemsNew`, `InventorySocketItem` (`ret=bool:false`) and `RemoveItemFromMap` (self the bag's grid, `a0=ref ds_map 1055`, `a1` X's fingerprint), the window including the owner's merge in the bag; 1055 is not the kept 1049. `InvGridClearItemNode`, `GridRemoveItem`, `InventoryGridRemoveItem`, `OnlineRemoveItem`, `CheckInventoryOperation` and `InventorySplitOperation` had 0 calls (capture, Step 5 continued) | pass |
| last-unit-socket | The whole class-15 `b=51` stack dragged by hand from the Socketable tab to the bag: the same reads, under the same same-map rule | `dropped:` on the same map: `kept=ref ds_map 1049` and `refreshed=1` before and after, `size=` 1626 to 1625, `mapkeep find 15 51` `matched=0`; `node socket a1=9` with no `b=51` sum, filled cells 90 to 89. No split dialog: only `ChangeItemOwner` (self and other the bag's grid, `a0=int64:9 a1=int64:0`, `a2` the stack's fingerprint) and `s_InvNode` (the `o=10` stack placed at bag cell `(3, 1)`) fired, and no removal-family row. The game's lookup was not called on this tab (the step names none). Moved back afterwards: `o=10 matched=1`, `size=1626` (capture, Step 6 continued) | pass |
| save-shape | The stash's close logs `SaveStash` with its self, `argc` and every argument's kind - the shape the save trials replay | `SaveStash #1 self=Console_Save_obj#980@199951 other=Console_Save_obj#980@199951 argc=0`, `ret=undefined`; `s_SaveStashConstants` twice with `a0=int64:4` (other `UI_Stash_obj`, then `Console_Save_obj`); the five closures' first lines with no instance as self and `Console_Save_obj` as other, `___struct___359@SaveStash` called once, on the Materials tab's Unstable Dust (`o=13`, `b=50`). The file time moved from T0 to T1 `2026-09-23T15:52:29.1750485Z`, and the file then held no X and the Socketable stack of 10 (capture, Step 7) | pass |
| stack-shapes | The rows and arguments of a hand split of one Unstable Dust and of one craft with bag inputs; `CraftEditPlayerInventory`'s `a0`, and its `a1` index against `localItemMap`'s and the kept map's | Not observed (skipped by owner redirect): the craft was not run, so `CraftEditPlayerInventory`'s `a0` and the three indices are not observed. The split (stash 13 to 12, bag 1) was logged: `UI_Split_Stack_obj anon@1285`, `UiASplitStack` (self `UI_Button_Small_obj`, other `UI_Split_Stack_obj`, `a0` an empty array), the Materials tab click with `InventoryResetTabs` (`a0=int64:-4`), and `s_InvNode` twice placing a new one-unit Dust item (`o=1`, `b=50`) on the bag's grid; no stack-family or operation row fired. The kept map followed it: `mapkeep find 14 50` read `o=12` on the same key (capture, Step 8: Hand-back F part 2 result) | not-observed |
| take-1stack | `GridRemoveItem` by name on X back in the Materials tab as a 1-stack, then the hand move's removal row if one was logged in a shape `call` can supply: `ret=`, the cell, the kept map, the lookup | Supplied: self and other the reopened tab's grid (`id:270001`), `a0` its `nodeGrid` (`array len=18 len0=17`), `a1` X's fingerprint as a string. `dispatched -> ret=bool:true`; X's cell gone, and the owner saw X gone (bag 154). The kept map did not follow: `mapkeep find 14 51` still `o=1 matched=1`, `size=1627`, index 1049 with `refreshed=1`; the game's own lookup still returned X's struct. `RemoveItemFromMap` (step 5's row) not run: its logged self, the bag grid of the first opening, no longer existed, `UI_Inventory_Grid_obj 0` resolved to the floating potion grid instead, and its map was the bag's (capture, Step 9 and Hand-back G result) | pass |
| save-by-name | `SaveStash` by name in the logged shape with the stash open: dispatched, the file's write time moved against T1b (read directly before the call, nothing between; T1 -> T1b recorded as its own line), and the file against the grid and against the kept map | Not observed (skipped by owner redirect): no stash-open save trial ran, so no T1b was read (capture, Owner redirect) | not-observed |
| save-by-name-closed | The same with the stash closed, judged against T3 (read right after the close), only if the logged self has a live instance then | The owner's close fired the game's own `SaveStash` once; T3 `2026-09-23T16:06:38.7004546Z`; one read-only `craftprobe var Console_Save_obj 0 *` (live, id 199951) came between T3 and the call. Supplied: self `Console_Save_obj` 0, no argument. `dispatched -> ret=undefined`, the row logged `argc=0`, and its `___struct___359@SaveStash` closure logged the Materials tab as Unstable Dust `o=12`, `b=50`, with no X. The file time after it equalled T3: no write seen. The file held Dust 12 and no X, the grid's state; the kept map had last read X at `o=1` (step 9, before the close) (capture, Trial 1: save-by-name-closed) | not-observed |
| take-stack | A stack row replayed by name with the count `1` on the Unstable Dust stack, only if one fired on the hand split | Not observed (skipped by owner redirect); no stack-family or operation row fired on the hand split either, so there was no shape to replay (capture, Owner redirect; Step 8) | not-observed |
| counts-tool-after | `tools/stash_tab_counts.py` after launch A: each line against the owner's last count and the trials' results | Read after launch B's graceful exit, in the redirect's order: exit 0, `class=14 b=50 stack=12`, `class=15 b=1 stack=216`, `class=15 b=51 stack=10`, no `class=14 b=51` line - the owner's last counts, and X absent as the grid left it; the same lines as the read right after launch A's last stash close. `hs_saves_inspect` against the backup: `herosiege13.hss`, `inventory_order_13.hss`, `shop.ini` and `stash.hss` changed (capture, Session end) | pass |
| map-by-name | Launch B, no window opened: `GetItemMap` by name with self `Console_Save_obj` and the argument `9` - dispatched, kept current by `mapkeep`, and its Ol and Unstable Dust counts against the file launch A left | Before: `a0=9 calls=0`, `first9: none`, `kept=none`. Supplied: self `Console_Save_obj` 0 (live, id 199951), `argc=1`, `a0` the real `9`. The keeper printed its first `a0=9` call with self `Console_Save_obj` in `Town_01_rm` and kept `ref ds_map 1050`; `dispatched -> ret=ref ds_map 1050`, a map keyed by item fingerprints. After: `a0=9 calls=1`, `size=1626 current=yes refreshed=1`; `mapkeep find 15 1` `o=216` and `find 14 50` `o=12`, one match each, equal to the file; `node var` read 1626 of 1626 entries, no `b=51` sum. The game did not crash (capture, Launch B) | pass |
| map-by-name-vs-game | Launch B, then the stash opened: the game's own `GetItemMap(9)` call leaves the index obtained by name as the latest keep (`same-index:`), or makes a new keep (`new-keep:`, its self and index) | `same-index:` - after the owner opened the stash, `latest-keep: #1 self=Console_Save_obj` and `ref ds_map 1050` unchanged, read twice. But `a0=9 calls=` stayed at 1 while `a0=0` climbed (38459 to 178984): the game's own open made no `GetItemMap(9)` call the keeper saw, unlike launch A's first open (`a0=9 calls=34260` at the first read) and Live 1d/1e's, so whether the game's own call returns the index obtained by name is not observed. Whether the window drew its content as usual was not recorded (capture, Hand-back I result) | pass |

**Question 1: the whole-entry control.** On both special tabs the game's own
hand move of a whole stack into the bag took the entry out of the kept map:
X's only unit from the Materials tab (1627 to 1626 entries, `matched=0`) and
the stack of 10 from the Socketable tab (1626 to 1625, `matched=0`). Each read
was on the same map - the kept index 1049 and `refreshed=1` unchanged across
the move - so both are the map's own behaviour, not a re-keep
(`last-unit-material`, `last-unit-socket`). After the Materials move the
game's own lookup no longer found X either (`undefined`, where the same call
before the move returned X's struct). So the stale entry after Live 1e's T1
was `GridRemoveItem`'s own doing, not the map's behaviour on any removal: this
session's `take-1stack` reproduced it on the same map that had just dropped X
on the hand move. No named row was seen removing the entry from the kept map.
The one `RemoveItemFromMap` line named a different map (1055), with the bag's
grid as self, in the window where the owner also merged X into the bag's
stack; on the Socketable tab no removal-family row fired at all, only
`ChangeItemOwner` and `s_InvNode`. `ChangeItemOwner`, with `a0=9`, `a1=0` and
the item's fingerprint, is the one named row both moves share; that it is what
removes the entry is not shown, and the removal itself is not observed by
name.

**Question 2: a by-name take the map confirms.** None is on record.
`GridRemoveItem` in T1's shape answered `true` again and emptied the cell, the
owner saw X gone, and the next save wrote the grid's state. But the kept map
kept X at `o=1` on the same index and refresh count, and the game's own lookup
`GetItemFromFingerprint(<X>, 9)` still returned X's struct (`take-1stack`). So
a by-name `GridRemoveItem` leaves the grid and the map out of step - the grid
(and the file after the next save) without the unit, the map and the game's
lookup with it - which is no take a re-read of the map can confirm. The only
removal row the hand moves logged, `RemoveItemFromMap`, ran on the bag's map
with a bag grid of the first opening as self; no live instance matched that
self after the reopen, so it was not replayed (not observed). The hand split
of one Unstable Dust went through the split dialog (`UiASplitStack`) and
`s_InvNode` creating a new one-unit item in the bag, with no stack-family or
operation row, and the kept map followed it (`o=12`). So no stack decrement by
name was recorded, and `take-stack` would have had no shape to replay. The
craft that would have decoded `CraftEditPlayerInventory`'s `a0` was skipped on
the owner's redirect, so `a0` stays undecoded (`stack-shapes`, `take-stack`:
not observed).

**Question 3: the cube bracket's two calls.** The save's shape is on record:
the stash close logs `SaveStash` with self `Console_Save_obj` and no argument,
and the close wrote `stash.hss` (T0 to T1) with the grid's state
(`save-shape`). `SaveStash` by name in that shape, with the stash closed and
T3 read right after the owner's close, dispatched and returned `undefined`,
and its serialising closure ran on the Materials tab without X - but the
file's write time did not move after T3 (`save-by-name-closed`: not
observed). Its control is the close's own write moving the file time in the
same session (T0 to T1). The owner's close had written the same state just
before T3, so the file cannot say whether the call writes when there is
something new to write; what `SaveStash` writes with the window closed, and
from which structure, is not established. The open-stash form was skipped
(`save-by-name`), so no T1 -> T1b line exists. `GetItemMap(9)` by name, before
the stash or the bag was opened in launch B, with self `Console_Save_obj` and
the one argument `9`, dispatched and returned the full stash map (index 1050,
1626 entries); the keeper kept it current, its Ol (216) and Unstable
Dust (12) equalled the file's, and the game did not crash (`map-by-name`).
The owner's stash open afterwards made no `GetItemMap(9)` call the keeper
counted while its `a0=0` control climbed, so the map obtained by name stayed
the latest keep. Whether the game's own call would have returned the same
index is not observed, and no second map was seen (`map-by-name-vs-game`).

**What Phase 1f settles, and what it leaves open.**

- **The kept map follows the game's own moves, whole-entry removals
  included.** On both special tabs, on the same map; Phase 1e saw it follow a
  lowered `o` and an added key. A map re-read after a move the game makes is a
  confirmation; after a by-name `GridRemoveItem` it is not.
- **The map is reachable by name before any stash open.** `GetItemMap(9)`
  with self `Console_Save_obj` and the argument `9` returned the whole map,
  with counts equal to the file, and did not crash - once, in one launch.
- **The save's shape is recorded** (self `Console_Save_obj`, no argument).
  A by-name call in that shape with the stash closed dispatched, but no new
  write was seen, so whether it saves the stash with the window closed is
  open.
- **Open**: a by-name move of a stash item into the bag or into the craft
  with the stash window closed (every stash-side move on record ran on grid
  instances that exist only while the window is open); what `SaveStash` writes
  with the window closed; `CraftEditPlayerInventory`'s `a0`; and why the
  game's own stash open made no `GetItemMap(9)` call after ours.

## Decision gate

decision: H-A

The owner set this on 2026-09-23 ("H-A, research consume first"), choosing
H-A and asking that Phase 1e, the consume-research round, come before any
player build. During Live 1f the owner also set the design an H-A build is to
follow, below.

**After Phase 1f (2026-09-23): H-A stays the decision, and the owner has set
its build design. A player build now has the stash map before the first stash
open, its counts, the save's shape and a map that follows the game's own
moves; it lacks a measured way to move a stash item with the stash window
closed** (the next workorder is the owner's decision). This replaces the
"After Phase 1e" reading below, kept as it stood then, including its
recommended round, which the owner's redirect overtook. The owner's design,
given during Live 1f (the workorder's Log, `### Owner redirect`, verbatim
there), in this document's words:

1. When the Crafting Cube opens, obtain the stash map by calling
   `GetItemMap(9)` by name (self `Console_Save_obj`, the one argument `9`).
2. Add the stash's special-tab counts from that map to the cube's
   availability check - never trusting the cube's own `a`
   (`### Constraints from Phase 1`).
3. At the craft, hook the consume (`CraftEditPlayerInventory`). When the bag
   is short, move the shortfall out of the stash into the bag - through the
   move route ForgePact's auto-prospect already uses, or the game's own
   stash-to-bag route (`s_InvNode`, `ChangeItemOwner`) - so the game consumes
   it from the bag as usual.
4. Then have the game save the stash: `SaveStash` by name, self
   `Console_Save_obj`, no argument.

A mod moves; it does not remove. Nothing is done with the stash window open,
since the stash and the cube cannot be open together. The stash stays in
memory while its window is closed (Phase 1d's `map-closed`) and is loaded at
the character load (`LoadStash`'s two load-time calls, in every session since
Phase 1e). What a build on that design has, by measurement with `control` and
the keeper's own control non-zero (`### Phase 1e results`,
`### Phase 1f results`):

- *The map by name before any stash open* (`map-by-name`). `GetItemMap(9)`
  called by name with the self and arity the game itself uses returned the
  whole map (1626 entries) in a launch where neither the stash nor the bag had
  been opened, kept current through `HookOneScript`'s both routes, and the game
  did not crash. One trial, one launch.
- *The counts.* Read from that map, Ol and Unstable Dust equalled the file
  (`map-by-name`), as the kept map's counts equalled the owner's eye and the
  file in Phases 1d and 1e (`map-closed`, `map-whole`).
- *The save's shape* (`save-shape`). The stash close calls `SaveStash` with
  self `Console_Save_obj` and no argument, and that close wrote `stash.hss`
  with the grid's state.
- *A map that follows the game's own moves, whole entries included.* A hand
  move of a whole stack out of either special tab dropped the entry on the
  same map (`last-unit-material`, `last-unit-socket`), as Phase 1e's hand takes
  lowered `o` and its move in added a key. So a re-read of the map after a move
  the game makes can confirm it. After a by-name `GridRemoveItem` it cannot
  (`take-1stack`: the grid lost the unit, the map and the game's lookup kept
  it), and the design no longer removes.

What it still lacks, as far as observed:

- *A by-name move of stash items into the bag, or into the craft, with the
  stash window closed.* Every stash-side move on record - the owner's hand
  moves, the game's own `s_InvNode` and `ChangeItemOwner` legs they ran, and
  `GridRemoveItem` - ran on grid instances that exist only while the stash
  window is open, and a reopen replaces them (Phase 1c's `reopen-id`; new grid
  ids in Live 1e and in Live 1f). Auto-prospect's move pass moves material
  cells out of the ProspectGrid, not out of the stash, and has not been run
  against a stash tab. Whether either route runs with the stash closed, and
  with what self and arguments, is not observed. Step 3 rests on it.
- *What `SaveStash` writes with the window closed.* The by-name call with the
  stash closed dispatched and its closure serialised a Materials tab matching
  the grid, but no new write of `stash.hss` was seen after T3, the owner's
  close having just written the same state (`save-by-name-closed`: not
  observed). Whether it saves the in-memory stash with the window closed, and
  from which structure, is not established. Step 4 rests on it.
- *`CraftEditPlayerInventory`'s `a0`*: an owner value (1 for the bag, 9 for
  the stash, as elsewhere) or the craft amount, undecoded - the craft was
  skipped (`stack-shapes`). Step 3's hook reads it.
- *The game's own `GetItemMap(9)` after ours.* After the by-name call, the
  owner's stash open made no `a0=9` call the keeper saw
  (`map-by-name-vs-game`). Why is not established, and whether the window then
  behaved as usual was not recorded.
- *The duplication constraint* (`### Constraints from Phase 1`) still holds:
  the count comes from the map only, never from the cube's `a`, and a stash
  unit supplies an input only once the mod has counted it and seen it leave the
  stash - for a move the game makes, its entry dropped or lowered in the map.

Next (the owner decides): steps 1 and 2 rest on measured answers and step 4 on
a recorded shape; step 3's move with the stash closed is the one thing no
session has shown. So the next workorder is either a build design whose first
gate is that move, trialled by name with the stash closed on a research build
(one unit, under `confirm`, the map re-read after it) before anything reaches
a player, or one more research round for exactly that move. Neither is a
struct-layout read or a write to the game's own structures.

**After Phase 1e (2026-09-23): H-A stays the decision; a player build now has a
map route and a remove call that answers, but no take it can confirm, so the
next round is research, not a build design** (the owner decides which). This
replaces the list of what H-A still needed after Phase 1d (kept below as it
stood then). What a player H-A build now has, by measurement in Live 1e with
`control` and the keeper's own control non-zero (`### Phase 1e results`):

- *The map, through the player-build installer.* `HookOneScript` installs both
  routes on `GetItemMap` (`keeper-install`), keeps the return of the game's own
  `GetItemMap(9)` call without ever calling it, and the kept map is read whole
  (`map-whole`: 1626 of 1626 entries). Nothing in this route is a hand-resolved
  address or a struct-layout read.
- *The currency rule.* `CraftMatsKeptMap`'s rule held live: a room change
  invalidated the kept map and the game's own call at the stash's reopen made
  it current again (`room-invalidate`, `map-refresh`).
- *The map following the game's own moves.* Both hand takes and the move of X
  into the stash showed in the kept map with no new keep
  (`map-follows-socket`, `map-follows-material`).
- *One by-name remove that answers success.* `GridRemoveItem` with the
  Materials tab's grid as self, its `nodeGrid` and X's fingerprint answered
  `true`, emptied the cell, and the saved `stash.hss` held no X after the exit
  (`take-trial-grid`, `counts-tool-after`). It ran with the stash window open,
  on the window's own grid instance, which a close and reopen replace (Phase
  1c's `reopen-id`; a new grid id again in Live 1e); with the window closed
  there is no grid to pass.

What it still lacks, none of it measured:

- *A consume the kept map confirms.* After that `GridRemoveItem`, the kept map
  still carried X at `o=1`, and so did the game's next `GetItemMap(9)` return
  (`map-follows-trial`: fail). The decision core turns the mod off on a success
  its re-read cannot confirm, so this take cannot drive it as it stands. No
  `RemoveItemFromMap` or stack-row shape was recorded to try instead (T2 and
  T3 not observed). The moves the map did follow lowered a stack's `o` or
  added a key; T1 was the only removal of a whole entry, and it was by name, so
  the map's behaviour when the game's own route removes a whole entry is not
  observed (Live 1e) - Phase 1f's first question.
- *The save's source.* The kept map is a view the game keeps up to date on its
  own moves, not shown to be what `SaveStash` serializes: which structure the
  stash save writes from is not established; the grid's state was saved in this
  one trial.
- *A save after a mod take.* Live 1e observed `SaveStash` once at each stash
  close, and the exit rewrote `stash.hss` too; no other writer was ruled out.
  A take made at the cube with the stash closed has no save observed after it
  until the next stash close or the exit, and the owner's report of a crash
  duplicating or corrupting an item moved out of an open stash makes that
  window a design constraint. The owner's proposal - bracket
  the cube like the stash window, obtaining the map when the cube opens and
  having the game save the stash when it closes or right after a mod take - is
  the leading persistence candidate. Both halves are by-name calls the game
  normally makes itself (`GetItemMap(9)` with a self the game uses, `SaveStash`
  with the shape the stash close uses), and neither has been tried.
- *The map before the stash's first open.* The first `GetItemMap(9)` call in
  the launch came at the stash's first open (`map-first-call`), so until then a
  player build has no map and refuses the press.
- *The duplication constraint* (`### Constraints from Phase 1`) still holds:
  the count comes from the kept map only, never from the cube's `a`, and a
  stash unit is used only once the mod has both counted it and seen it removed.

Recommended next round (the owner decides): one more research round on a
research build, still with no struct-layout read, whose one thing to try is a
stash-side take the kept map confirms - first recording how the game's own move
removes a whole entry from the stash map (a hand move of the last unit of a
stash stack into the bag, with the map read before and after and the take and
remove rows armed; Live 1e's hand takes split one unit off larger stacks, so
the map's `o` fell but no entry left it), then replaying that shape by name
after a `GridRemoveItem` and re-reading the map. The cube bracket's two
by-name calls follow only once a confirmed take exists.

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

**After Phase 1d (2026-09-23): H-A is eligible; H-B is not, H-C is not
recommended on its own, and nothing was decided then** (the owner has since
chosen H-A, above). This replaces the reading
written after Phase 1c (the Phase 1c rows it cited stand in `### Phase 1c
results`). The one thing Phase 1c could not show - a special-tab count read
with the stash window closed - Phase 1d's `map-closed` shows: the map the
game's own `GetItemMap(9)` call returned, kept through a by-name detour, gave
both counts with the window closed, the path's control (`item-struct-control`)
passed in the same session, and `control` was non-zero. That meets the rule's
third condition - the stash tab's container readable where the mechanism needs
to read it - for a count, on both tabs. Per hypothesis:

- **H-A (count and consume through a hook)** is eligible under its own
  "Decided by": `M-craft` (Phase 1: the availability and consume rows fire by
  name, with recorded shapes) and a container readable with the window closed,
  which Phase 1d's `map-open-socket`, `map-open-material` and `map-closed` now
  give. It is the one the evidence leaned towards after Phase 1c, because every
  stash-side move seen so far ran on window instances that do not exist while
  the cube is open, and the kept map does not depend on them (Phase 1d's
  `map-reopen`: the same index before the close and after the reopen; indices
  are reused, so identity is not established). Cost to
  the player: the stash tab changes at the craft press, a moment the game chose
  for the bag only. What a player build would still need, none of it measured
  yet after Phase 1d (the list as it stood then; `After Phase 1e` above
  rewrites it):
  - *The map, by name, without a blind call.* The route Phase 1d measured is a
    detour on `GetItemMap` keeping the return of a call the game itself makes
    with first argument `9` - resolved by name and installed as the
    instrument's research-only `MmCreateHook` detour at the function's own
    address, never calling the script. That is not the player-build shape: a
    player build installs through ForgePact's both-routes installer
    `HookOneScript` (a table swap plus an inline detour), and a `HookOneScript`
    install on `GetItemMap` is not measured until Phase 1e. Its gap: that call was not
    observed before the stash's first open in a launch (`map0-identity`, and
    `map-closed`'s before-any-window read), so until the player opens the stash
    once, the mod has no map and refuses the press (the decision core's
    "an unreadable count refuses the whole press"). Calling `GetItemMap(9)`
    ourselves would close that gap, but it is a getter call of the kind Phase
    0b's `GetProfileInventoryData` crash warns against; it would need its own
    research-build trial, supplying the self the game used
    (`UI_Inventory_Grid_obj`, `UI_Stash_obj` or `Console_Save_obj`), before a
    player build could consider it. Either way the map's index changes from
    launch to launch (1050 in Live 1c, 1049 in Live 1d), so a kept reference is
    never carried as a number. Nor is `ds_exists` at each press enough on its
    own, for the same reason: GameMaker reuses a destroyed map's index, so a
    live index may name a different map. A kept map is current only when the
    game's own `GetItemMap(9)` call has returned it since the last character
    load or room change - the rule `CraftMatsKeptMap` in `CraftMatsMod.hpp`
    encodes - and `ds_exists` is checked at the point of use besides.
  - *A consume route that answers success.* Phase 1b saw the move out of the
    Socketable tab by name with no success answer (`move-socket-to-bag`:
    `undefined`, and a `false` on a unit that arrived) and did not observe the
    move out of the Materials tab; only the move into the stash,
    `StashAddToStack`, answered `true`. So no stash-side take with a success
    answer is on record, and the decision core turns the mod off for the
    session on a tab change without one. Also not observed: whether the kept
    map shows a take at all - nothing was moved in Phase 1d - which the core's
    re-read would need in order to confirm one.
  - *The duplication constraint* (`### Constraints from Phase 1`): the count
    comes from the kept map only, never from the cube's `a`, and a stash stack
    must never supply an input the mod has not both counted there and seen
    removed; with no confirmed take, the press refuses rather than letting the
    cube's own count carry it.
  - *The whole map.* Phase 1d's reads stopped at 1000 of the map's 1626
    entries; a player build walks all of it, and reads `o` as a stack only on
    an item already identified as a stackable.
- **H-B (pull on demand)** is not eligible. It rests on a by-name stash-to-bag
  move that answers success (Phase 1b: out of the Socketable tab, no success
  answer; out of the Materials tab, not observed), made with the window closed,
  while every observed move ran on the window's own grid instances, which each
  open or tab switch replaces (Phase 1c's `reopen-id`). Phase 1d's map gives
  H-B a count to size the shortfall, not a move. Cost: a stash-to-bag move at
  the craft press, a moment the game did not choose, which also changes the
  bag the player sees.
- **H-C (count only)** meets the rule on its count (Phase 1d's `map-closed`,
  with `M-craft`), but it runs into the duplication constraint: with consume
  left vanilla, a recipe counted from the stash either fails at the press or,
  as the vanilla duplication did, produces its result without the stash's
  inputs being removed. Phase 1d changes nothing about that, so H-C is not
  recommended on its own - only as the counting half of H-A.

Phase 1c's open-stash reading - count while the stash is open and keep the
answer - is no longer the only way to count: the kept map is that count,
still readable after the close. It still supplies no take.

Recommended after Phase 1d (the owner then chose H-A, and Phase 1e was this
round): H-A, but not yet as player code. Before any,
one more research round on a research build, still with no struct-layout read:
(1) a hand move into and out of each special tab (bag to stash and back, the
Materials leg included), with the kept map read before and after each move,
with the stash open and closed - does the map follow a take, and does any move
out of a tab answer success; (2) whether the map can be reached before the
stash's first open (the load and the first open counted by `backing`), and only
if not, a controlled by-name trial of the game's own `GetItemMap(9)` shape in
the research build; (3) the whole map read past the reader's 1000-entry cap, to
show each stack is one entry. If (1) finds no take with a success answer, what
remains is a count with no take, which the duplication constraint rules out on
its own, and whether anything short of H-A is worth having is the owner's call.
