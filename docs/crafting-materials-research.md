# Crafting from the stash's special tabs (ForgePact issue #14)

phase0-status: complete
phase1-status: complete
phase1b-status: complete
phase1c-status: complete
phase1d-status: complete
phase1e-status: complete
phase1f-status: complete
phase1g-status: complete
phase1h-status: complete
phase1i-status: complete
phase1j-status: complete
phase1k-status: complete
phaseC-status: complete

**Status: Phase 0 done (static search, instrument, decision core); Phase 1 (the
first live session, 2026-09-22) done; Phase 1b (a widened instrument and a
second session, 2026-09-22/23) done; Phase 1c (a reader round, 2026-09-23)
done; Phase 1d (a closed-window reader round, 2026-09-23) done; the owner chose
H-A on 2026-09-23 (`## Decision gate`); Phase 1e, the consume-research round
asked for before any player build, done (2026-09-23); Phase 1f, the owner's
build-design round on the installed Phase 1e build, done (2026-09-23);
Phase 1g, the closed-stash move, save and craft-route measurement, done
(2026-09-23); Phase 1h, the Ghidra-read take, its save and the recipe's
shape, done (2026-09-23); and Phase 1i, reaching `Controller_obj`, the
complete take and the recipe tie, done (2026-09-24), leave the complete
by-name take, a stash close after it and the selected recipe's amount
measured once each, with the stash file's write left to the game's own save
at the next stash close.** Phase 1j - one research build and one session
measuring a by-name save route through `SaveLocalFile`, a take of part of a
stack, the mod's count inside the game's own availability check and the
Crafting Cube's input grid as a destination - is done (2026-09-24,
`### Phase 1j results`). Its session (Live 1j) proved the close's own
`SaveLocalFile` save route by name, rejected the one `callm` shape tried for
a partial-stack take (self `Console_Save_obj`; the control's struct self and
`UI_Split_Stack_obj` other were not supplied, so it is not replayable
through `callm`, which supplies an instance self - a struct self was not
tried) and also proved a whole-item take-and-return, which the owner has
since ruled out as the build's route, leaving the partial take for a further
research round (Phase 1k); it proved a count injection reaching the game's
own availability check, and a whole-item placement into the Crafting Cube's
own input grid by name; whether the game's own count walks that grid stayed
not observed. Phase 1k - one research build and one session (Live 1k,
2026-09-24) for a by-name partial take, the split's own per-unit edit with
the item's methods re-bound to the item through the runtime's `method`
builtin (or an inline write plus `ItemCheckHash`) for a stack the
destination already holds, and a new unit made by the game's own loader
route for one it does not - is done: Live 1k rejected the one bound-call
shape tried, proved the inline stacked take and the json no-stack creation
with its placement into the bag or the Cube's `craftGrid`, and left the
game's own hash acceptance of an edited or created item on a drag, a merge,
a save and a reload not observed (`### Phase 1k rows`, `### Phase 1k
instrument`, `### Live procedure 1k`, `### Phase 1k results`). The owner
confirmed Phase 1k's three route tokens and chose to build on the inline
edit, and the player build is written (`## Ship design`: ForgePact 1.4.5,
`craftmats`, off by default); Phase C, its one live verification session, is
done (2026-09-24, `## Phase C results`): 13 of its fourteen checks pass, and
`bag-control` fails on the produced item's name only, an exception the owner
accepted as a game bug. Phase 1
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
Phase 1g (done 2026-09-23), one research build with three instrument additions
and one session, measured what that design still rested on: a move of a whole
stash entry into the bag by name with the stash closed, whether a by-name
`SaveStash` then writes, the recipe's readable shape, and whether the consume
and the result's production run inside one craft-route row
(`### Phase 1g instrument`, `### Live procedure 1g`, `### Phase 1g results`).
Its session (Live 1g) found the consume and the result's placement inside one
`DoCraftResult` call in a one-unit craft, and moved a Materials entry into a
bag stack by name with the stash closed (`InventoryGridCanAddToStack`,
`InventoryGridAddToStack`, `ChangeItemOwner`, self `Console_Save_obj`), the
kept map dropping it on the same index; the by-name `SaveStash` printed
`NOT dispatched` with the stash closed and open and no write was seen, the
game then crashed inside its own save at the next stash close, and the
recipe's input members and a move of an entry the bag has no stack for were
not observed - so the `## Decision gate` now reads a player build as blocked
on the save and on the recipe's shape.
Phase 1h (done 2026-09-23) read the game's code in a local Ghidra project
first - where the closed stash's cells live (`Controller_obj`), why the save
after Live 1g's take faulted (on that reading, the take left the stash cell
the save looks up), and the complete by-name take per case - then ran one
research build (two rows, a split `call` reply, array path segments) and one
session (`### Phase 1h rows`, `### Phase 1h instrument`,
`### Live procedure 1h`, `### Phase 1h results`). Its session (Live 1h) took a Materials entry's
bag side and map side by name again (now with `RemoveItemFromMap`), and
measured the save after that half-take stopping by name with an exception
after the Dust cell and before X's, the game surviving, while the game's own
save at the next stash close stopped at the same point and ended the game
again. The stash-cell clear, the Socketable case and the selected recipe's
decoded amount were not observed - the instrument could not list the
`Controller_obj` variables past 80, and the selected recipe's calls were not
told apart from the rest of the Cube's list - so the "After Phase 1h" paragraph keeps a player build blocked and names
what the next research build needs.
Phase 1i (done 2026-09-24) ran one research build - a paged `var`, a
content search `find` and an `inroute` gate (`### Phase 1i instrument`) - and
one session (`### Live procedure 1i`, `### Phase 1i results`). Its session
(Live 1i) named the stash's containers on `Controller_obj` by content
(`stashMaterialTab` and `stashSocketItemSlot`, beside the map
`stashInventoryMap`), ran the complete by-name take - bag side, map side and
stash-cell clear - on one Materials entry and one Socketable stack, and read
the selected recipe's required amount, 5, at the craft press. Each by-name
`SaveStash` (self `Console_Save_obj`, no argument, stash closed, Cube open)
after a take returned with one item fewer and no fault, but wrote no file in
this one launch, the one before any take included, making 1627
`CreateItemSaveStruct` calls; a separate save-control window - the owner's
stash open, hand move and close, run before any by-name call - counted 1993
`CreateItemSaveStruct` calls, a different window in scope, so the gap
between them is not yet explained and may be the open and the move rather
than the by-name save itself. The owner's own stash close after both takes
kept the game running and wrote a file holding neither. The "After Phase 1i"
paragraph records what a player build now has and still lacks.
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

### Phase 1h rows

Phase 1g left two things unmeasured that a row can make readable
(`## Decision gate`, "After Phase 1g"): the recipe's amount, which no plain
member of the recipe entry carried, and what a `SaveStash` does per item, which
Live 1g could only read from the file's timestamp and the game ending. The
local Ghidra reading (`### Phase 1h instrument`) named the one function behind
each. Two rows, 254 in all, each named by its SDK constant like every other row
and hooked by the same one `craftprobe hook`; no object's closures were added,
so the closure-coverage test is unchanged.

| Row label | Group | Runtime name (the SDK constant's value) |
|---|---|---|
| PilipaliDecrypt | script (crafting group: the decoder a recipe input's stored amount goes through at count time; its armed line gives the argument shapes and `ret=` the decoded amount) | `gml_Script_PilipaliDecrypt` |
| CreateItemSaveStruct | script (stash group: the per-item step of `SaveStash`; a healthy save logs one entry per saved item with a struct `a0`, and the fault Live 1g ended on reads as an entry with `a0=undefined` and no `SaveStash ret=` after it) | `gml_Script_CreateItemSaveStruct` |

**An SDK finding, recorded rather than used.** The runtime's script table also
holds `SaveStashFunc` and `LoadStashFunc` - the wrappers the game itself calls
around `SaveStash` and `LoadStash` (the `@SaveStashFunc` suffix of the
`___struct___357..364@SaveStash` methods names the first). `hs-game-sdk`
already carries both, bare rather than `gml_Script_`-prefixed: C++
`HeroSiege::Scripts::SaveStashFunc_Index` = 3536 and
`HeroSiege::Scripts::LoadStashFunc_Index` = 2300
(`hs-game-sdk/cpp/include/hs_game_sdk/scripts.hpp`); Python
`GameScript.SaveStashFunc` = 3536 and `GameScript.LoadStashFunc` = 2300
(`hs-game-sdk/python`); TypeScript `GameScripts.SaveStashFunc` and
`GameScripts.LoadStashFunc` (names only, no indices, `hs-game-sdk/ts`). This
search's earlier pass looked only for `gml_Script_`-prefixed constants, which
is why it missed both names. Neither is a row or a call target here: a name
the SDK already carries needs no regeneration, only a citation
(`AGENTS.md` § "HS Game SDK Usage").

### Phase 1j rows

Phase 1i left the owner's design resting on three things no session had
measured a mechanism for, and the owner added a fourth (`## Decision gate`,
"After Phase 1i", and the owner's answers of 2026-09-24): a by-name route that
makes the game itself write `stash.hss` with the stash closed, a take that
moves only part of a stack, the mod's count inside the game's own availability
check, and the Crafting Cube's own input grid as a destination when the bag is
full. The local Ghidra reading (`### Phase 1j instrument`) named the functions
behind each. Twenty-four rows, 278 in all, each named by its SDK constant like
every other row and hooked by the same one `craftprobe hook`; they sit
together, just before the `CheckPlayerInteraction` control. None is a
craft-route row, so `within=` and `kCpCraftRouteRows` are unchanged, and no
object's closures were added, so the closure-coverage test is unchanged.
`UiSetGrid` and `UiSetGridArray` are also in `prospectprobe`'s table, a
different instrument; one instrument per session, as `hook` already warns.

| Row label | Group | Runtime name (the SDK constant's value) | Why |
|---|---|---|---|
| SaveLocalFile | save route | `gml_Script_SaveLocalFile` | the one direct caller of `SaveStash` in the image (the reading); per kind it brackets a saver with `SaveStart` and `SaveCommit`. The close's call gives the shape (`self`, `argc`, `a0`, `a1`) the by-name save replays |
| SaveStart | save route | `gml_Script_SaveStart` | runs before the kind's saver; `SaveLocalFile` runs the saver only if it answered true |
| SaveCommit | save route | `gml_Script_SaveCommit` | runs after the saver; the step a by-name `SaveStash` never reached, which is why it wrote no file (the reading) |
| SaveFileGMAsync | save route | `gml_Script_SaveFileGMAsync` | a file-save script by its name, not followed by the reading; whether the stash's save reaches it is read from the close |
| EncryptStringSave | save route | `gml_Script_EncryptStringSave` | shared by every saver; its count at the close says how many savers ran |
| GenerateItemHash@s_ItemInstanceStruct | item method | `gml_Script_GenerateItemHash@anon@4791@s_ItemInstanceStruct@InventoryV2Funcs` | by shape the split's no-argument call; the hash step after an edit is one of the questions |
| GetItemInfo@s_ItemInstanceStruct | item method | `gml_Script_GetItemInfo@anon@5277@s_ItemInstanceStruct@InventoryV2Funcs` | a string-keyed getter, a candidate for the split's first call |
| GetItemStat@s_ItemInstanceStruct | item method | `gml_Script_GetItemStat@anon@5523@s_ItemInstanceStruct@InventoryV2Funcs` | the stat getter, named so a stat call is not mistaken for the split's |
| GetItemStatArray@s_ItemInstanceStruct | item method | `gml_Script_GetItemStatArray@anon@5844@s_ItemInstanceStruct@InventoryV2Funcs` | the item's remaining method family, for the same reason |
| ___struct___240@GetItemStatArray@s_ItemInstanceStruct | item method | `gml_Script____struct___240@GetItemStatArray@anon@5844@s_ItemInstanceStruct@InventoryV2Funcs` | the struct `GetItemStatArray` builds |
| GetItemDef@s_ItemInstanceStruct | item method | `gml_Script_GetItemDef@anon@7191@s_ItemInstanceStruct@InventoryV2Funcs` | a string-keyed getter on the definition struct, where the stack count `o` lives (measured, Live 1i); the likeliest first split call |
| SetItemDef@s_ItemInstanceStruct | item method | `gml_Script_SetItemDef@anon@7339@s_ItemInstanceStruct@InventoryV2Funcs` | its two-argument setter; the likeliest second split call (the key, then the count minus the amount) |
| SetItemStat@s_ItemInstanceStruct | item method | `gml_Script_SetItemStat@anon@7507@s_ItemInstanceStruct@InventoryV2Funcs` | a two-argument setter, a candidate by shape |
| AddStat@s_ItemInstanceStruct | item method | `gml_Script_AddStat@anon@7675@s_ItemInstanceStruct@InventoryV2Funcs` | named so a stat edit is told apart from a count edit |
| SetItemInfo@s_ItemInstanceStruct | item method | `gml_Script_SetItemInfo@anon@7965@s_ItemInstanceStruct@InventoryV2Funcs` | the info setter, a candidate for the split's second call |
| s_ItemInstanceStruct | creation | `gml_Script_s_ItemInstanceStruct` | the item constructor; a candidate for how the split unit's item is made at the drop |
| StructCopy | creation | `gml_Script_StructCopy` | a copy of the source item is the other way the unit's item could be made |
| AddItemToMap | creation | `gml_Script_AddItemToMap` | how a new item enters a map |
| ItemCheckHash | identity | `gml_Script_ItemCheckHash` | the hash step the game's own merge (`GridAddToStack`, `InventoryGridAddToStack`) runs after its inline count edit |
| LootTimestamp | identity | `gml_Script_LootTimestamp` | a new item's stamp; a creation marker |
| GetCounterHash | identity | `gml_Script_GetCounterHash` | a counter-based hash a new fingerprint may come from |
| EditItemData | identity | `gml_Script_EditItemData` | an item edit by name, a candidate for the unit's or the source's edit |
| UiSetGrid | Cube grid | `gml_Script_UiSetGrid` | the Cube window's Create closures bind its grids through it; the drop into the Cube's input grid names which array it binds |
| UiSetGridArray | Cube grid | `gml_Script_UiSetGridArray` | the array form of the same binding |

### Phase 1k rows

Live 1j left the partial take - the owner's route (`## Decision gate`, After
Phase 1j, and the owner's decision of 2026-09-24) - with one rejected shape
and no creation route: the split's creating rows ran with a self `callm`
cannot supply, and the item constructor has no direct call site to replay
(`### Phase 1k instrument`). The local reading found the game's own loaders
making an item without the constructor, from a save-shaped struct and a
fingerprint key. Four rows, 282 in all, each named by its SDK constant and
hooked by the same one `craftprobe hook`; they sit together after the Phase 1j
rows and before the `CheckPlayerInteraction` control. Three are that loader
route; the fourth, `ReportClient`, was added in the build's review round so
that `hash-accept`'s "`ReportClient` did not fire" rests on a row that would
have logged a call, rather than on a function nothing was watching. None is
a craft-route row and none is a closure, so `within=`, `kCpCraftRouteRows`
and the closure-coverage test are unchanged. `ParseItemToGrid` is also in
`prospectprobe`'s table, a different instrument; one instrument per session,
as `hook` already warns. The trials call rows already in the table
(`CreateItemSaveStruct` since Phase 1h, `StructCopy`, `AddItemToMap`,
`ItemCheckHash` and `LootTimestamp` since Phase 1j) plus `InitItemFromJson`.

| Row label | Group | Runtime name (the SDK constant's value) | Why |
|---|---|---|---|
| InitItemFromJson | creation | `gml_Script_InitItemFromJson` | makes an item from a save-shaped struct and a fingerprint key, without the constructor: the step the game's loaders run (the reading) and the plugin's own `SpawnSignatureItem` already calls by name in the player build (proven live); the no-stack take's creation, and its row shows the argument order the game itself passes |
| ReCreateItem | creation | `gml_Script_ReCreateItem` | a one-call helper that remakes an item (the reading: it passes its argument and `undefined` on to one helper); a row so a creation the game makes through it is seen, not a route |
| ParseItemToGrid | creation | `gml_Script_ParseItemToGrid` | a loader that follows `InitItemFromJson` with `AddItemToMap` (the reading); the pattern the no-stack take replays, so its row, beside those two, shows the order of the calls when the game loads an item |
| ReportClient | hash check | `gml_Script_ReportClient` | the flag the game raises when a stored item hash disagrees (the Custom Forge's comment in `ModuleMain.cpp` names it); armed at `hash-accept` and `partial-cube`, so "did not fire" is read from a detoured row with a call count, not from silence |

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
  or stash container - true when searched (2026-09-22), before this issue's
  own findings were folded into the hub's `## 5. Stash Special Tabs & the
  Crafting Route` - the HSSaveEditor and hero-siege-item-editor guides mention
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

### Phase 1g instrument

The owner's build design (`## Decision gate`, "After Phase 1f") rests on four
things no session has measured: a move of a stash entry into the bag, by name,
with the stash window closed; whether a by-name `SaveStash` writes the file
after such a move; which members of the recipe name an input's type, base and
amount; and whether the consume and the result's production both run inside
one craft-route row, which is the only place a later hook could refuse a craft
by not calling the game's own function. Phase 1g adds three things to the
research build for that, and nothing else: no row (the table stays at 252), no
hook target and no new by-name call path. Every `craftprobe`/`mapkeep`
subcommand, cap and refusal above stays as it was. All of it is inside
`#ifndef FORGEPACT_RELEASE`; the player build answers `command unavailable`.

The move trial reuses what `craftprobe call` already dispatches (`fp9:`,
`map9`, `path:`, `id:<n>`) plus the one argument kind it could not supply.
Auto-prospect's proven move - `GetItemFromFingerprint(fp, 0)`, then
`InventoryGridCanAddToStack(1, undefined, item)`, `InventoryGridAddToStack(1,
item)` and, only on its `success`, `InvGridClearItemNode(cell, undefined)` -
passes a value of kind undefined twice, and `call` typed every other token as
a number, a bool or a string, so the word `undefined` reached the game as text.

| Addition | What it prints | Cap | Control |
|---|---|---|---|
| The marker | a bare `craftprobe` answers `craftprobe: phase1g rows=252 - ...` | - | the build's control: without `phase1g` the installed plugin is not this build (the Phase 1e build, which Phase 1f also ran, prints `phase1e` with the same 252 rows), and nothing from the session counts |
| `craftprobe call` argument `undefined` | the literal token `undefined` (any case) is a value of kind undefined, where it was the string `undefined`; the call line shows it as `a<i>=undefined(undefined)`. Resolved with the other forms, after the `confirm` gate and before the one call | still exactly one by-name call per command, behind `confirm` | `test_craftprobe_call_takes_the_literal_undefined`; live, the argument's kind is the one the call line prints |
| `within=<row>#<n>\|none` on the craft-route rows | the armed line of `CraftFindRecipeItems`, `DoCraftResult`, `CraftEditGrid`, `CraftEditPlayerInventory`, `s_CraftItem`, `GridAddItem`, `GridAddToStack`, `s_ItemOperation` and `GetInventoryGridNode` names, after the call number, the outermost of those nine rows that was already on the game thread's stack when this call was entered, and which call of it (`#n`, the number on that row's own entry and `ret=` lines), or `none`. The detours track it themselves: each of the nine rows keeps a depth, raised just before its trampoline and lowered just after it returns (also when it unwinds), and the entry order and call number of its outermost frame. The field is read before the call's own frame is entered, so a row never names itself unless it is recursing. The other 243 rows' lines are unchanged | the rows' own `arm` budgets; tracking runs on every call of the nine rows whether or not they are armed, so arming during a craft cannot misreport the nesting | only rows `craftprobe hook` detoured take part: a row that did not detour cannot appear as `within=`, so a `within=none` is read beside `hook`'s `0 failed` and the enclosing row's own `calls=` in `show all`. Live 1e's call order on a bag-stack craft (`CraftFindRecipeItems`, `DoCraftResult`, `CraftEditGrid`, `CraftEditPlayerInventory`, `s_CraftItem`, `GridAddItem`/`GridAddToStack`) is the comparison: a line whose row logged before `DoCraftResult` in that order reading `within=DoCraftResult#<n>` would be the instrument's error. "One row encloses both the consume and the production" is read only when the consume line and the production line name the same `<row>#<n>` **and** that row's own `#n` entry line and its `#n` `ret=` line are both in the log, one before and one after them. At amount 2 the two may sit in two calls of the same row (one per unit), which differs only in `#n`; an outer row such as `CraftFindRecipeItems` enclosing the whole craft names itself on every line and hides whether `DoCraftResult` alone encloses both, which the `DoCraftResult` entry and `ret=` lines around the two lines then settle. A bracket line lost to the budget leaves the reading not observed |

### Phase 1h instrument

Phase 1g left the owner's H-A design blocked on two things (`## Decision
gate`, "After Phase 1g"): the stash save after a by-name move - every
`SaveStash` after Live 1g's take stopped, and the owner's next stash close
ended the game inside its own save - and the recipe's readable shape. Phase 1h
settles both with a reading of the game's code first and one session second.
The research build gains four things and nothing else: every other
`craftprobe`/`mapkeep` subcommand, cap and refusal stays as it was, no hook
target or call path is added beyond the two rows, and all of it is inside
`#ifndef FORGEPACT_RELEASE` - the player build answers `command unavailable`.

| Addition | What it prints | Cap | Control |
|---|---|---|---|
| The marker | a bare `craftprobe` answers `craftprobe: phase1h rows=254 - ...`, the count still derived from the table (`kCpTargetCount`) | - | the build's control: without `phase1h` the installed plugin is not this build (the Phase 1g build prints `phase1g rows=252`), and nothing from the session counts |
| Rows `PilipaliDecrypt` and `CreateItemSaveStruct` (`### Phase 1h rows`) | their armed lines and `ret=` lines, like every row's: the first gives the decoded amount and its argument shapes, the second one entry per item a save writes | the rows' own `arm` budgets | `hook`'s `252 detoured, 0 failed, 2 held by mapkeep`; `save-control` is the healthy `CreateItemSaveStruct` signature (a struct `a0` on every entry, then `SaveStash #<n> ret=`) every later save is compared with |
| `craftprobe call`'s reply, split | one of four lines, each naming the row's call number - the `#<n>` the row's own detour prints on its entry line for this call (its next count, read on the game thread just before the dispatch): `NOT dispatched #<n>: asset_get_index found no script` (decided before any `script_execute`), `entered #<n>, script_execute threw` (the dispatcher's `catch (...)` took an exception - how a GML runtime error inside the call reaches it is the static reading's inference, not measured; Live 1h's `error-baseline` printed this line, beside the row's own entry line, with the game still running), `entered #<n>, script_execute returned st=<s>` (a failure status), or `dispatched #<n> -> ret=<value>`. Before, the first three printed one merged `NOT dispatched` line, which is how Live 1g's faulting by-name `SaveStash` read the same as a name that never resolved | still exactly one by-name call per command, behind `confirm`; the dispatch is the one auto-prospect's `ApCallScript` makes (`asset_get_index`, then `script_execute` with self = other = the instance), with its failures kept apart | `test_craftprobe_call_reply_splits_three_outcomes_with_call_number`; live, the reply's `#<n>` against the row's own `<Row> #<n>` entry line - a row `mapkeep` holds, or one not detoured, prints no entry line, so its number has nothing to match |
| Numeric `var` / `path:` segments | a whole-number segment (digits only) on a value `is_array` calls an array reads that element, inside `array_length`, so `var Controller_obj 0 <S>.<k>.0.0` and `path:Controller_obj.<S>.<k>` reach a row of the stash's Socketable structure. On anything that is not an array, or past its end, the walk stops naming the segment, as it does for a missing member. A global's name is never taken as an index | the walk's existing caps (`json`'s depth cap of 10 included) | `test_craftprobe_var_walk_takes_an_index_only_on_an_array`; live, `holders` reads the cell's fingerprint back as `K_S` |

**What the local Ghidra reading showed, in this document's words.** Read on
2026-09-23 in a named local Ghidra project, which stays on the owner's
machine: Ghidra 12.1.4 (`C:\Users\stann\tools\ghidra_12.1.4_PUBLIC`), project
`C:\Users\stann\ghidra_projects\HeroSiege`, program `Hero_Siege.exe`, with the
runtime's script names applied by `ImportSymbols.java`. The function bodies
were written locally to `C:\Users\stann\tools\hs-decomp\` (one file per
function, a raw one and one with the runtime helpers renamed by that
directory's `simplify.py`) by `C:\Users\stann\ghidra_scripts\DecompileTo.java`,
run through `C:\Users\stann\tools\hs-decomp\run_decomp.cmd <Name> ...`. The
next phase re-reads them there; nothing below quotes them.

- **Where the closed stash lives.** `SaveStash` (self `Console_Save_obj`, no
  argument) reads nothing of its own self. It walks the persistent stash
  containers, which are variables of the `Controller_obj` instance
  (`HeroSiege::Objects::GameObject::Controller_obj`, 984) - not of
  `Console_Save_obj` and not of the stash window. The ordinary tabs are one
  three-level array (tab, x, y; saved under keys beginning `stash_tab_`, the
  `___struct___357` method); the Materials tab is one two-level array (x, y;
  `___struct___359`); the Socketable tab is an array of rows, each a 1x1 cell
  array (`___struct___361` - one grid per row, which is why Live 1f counted 90
  "cells"); two more sections follow (`___struct___363`, `___struct___364`).
  For every cell that is its own anchor it reads the cell's fingerprint, looks
  the item up with `GetItemFromFingerprint(fp, 9)` and passes the result
  straight to `CreateItemSaveStruct` with no check for `undefined`; that
  function's first act reads a member of the item, which on `undefined` is a
  GML runtime error.
- **The map and owner functions.** `GetItemFromFingerprint(fp, owner)` is a
  find in `GetItemMap(owner)`'s map, `undefined` on a miss. `GetItemMap`
  answers owner 0 with the profile's own map
  (`New_Inventory_Data_obj.localItemMap`, Phase 1d) and owner 9 with a
  `ds_map` variable of `Controller_obj`. `ChangeItemOwner(from, to, fp)` finds
  the entry in one map, adds it to the other and deletes it from the first -
  maps only, no grid. `RemoveItemFromMap(map, fp)`, offline, deletes the entry.
  `InventoryGridAddToStack(1, item)` works on the bag's profile grids only.
  None of them reads its self.
- **The crash, as that reading explains it** - an inference from the static
  reading, not a measurement; `### Phase 1h results` says which part Live 1h
  measured. Live 1g's take moved X's entry out of map 9 and raised the bag's
  stack, and nothing cleared X's cell in `Controller_obj`'s Materials array.
  On this reading, every later `SaveStash` saved the Dust cell (the one
  `___struct___359` line all three calls logged), reached X's cell, got
  `undefined` from map 9, and faulted in `CreateItemSaveStruct`. Called by
  name, the fault would reach the dispatcher's `catch (...)` - which under
  `/EHsc` takes C++ exceptions and not access violations; that a GML runtime
  error arrives as one is inferred, not measured - and printed the merged
  `NOT dispatched`; the game's own call at the owner's close had no catch.
  Live 1f's same by-name call returned because the grid and the map still
  agreed.
- **The game's hand move** clears the source cell in the stash window's own UI
  code, not in any hooked row (Live 1f: `GridRemoveItem` and
  `InvGridClearItemNode` 0 calls). `GridRemoveItem(grid, fp)` reads no
  variable of its self: it walks a two-level array and empties every cell
  holding that fingerprint, answering `true` if it emptied any. Live 1f's
  by-name `GridRemoveItem` on the window's grid changed what a closed-window
  save wrote, so the window's grid is `Controller_obj`'s Materials array (or is
  copied back to it on close); with the window closed, `Controller_obj`'s array
  is the one to clear. `InvGridClearItemNode` reads two variables of its self
  and is not usable closed.
- **The bag side.** `GetItemPreferredGrid(p, item)` looks up the player's
  profile data and, by the item's type, answers the grid bits and the
  profile's own two-level special-grid array - so Live 1g's "numeric array no
  `path:` reaches" was the bag's persistent special grid itself (6 rows x 15),
  held as `New_Inventory_Data_obj.inventoryMaterialGrid` / `inventorySocketGrid`
  (Phase 1d) and reachable as `path:New_Inventory_Data_obj.inventorySocketGrid`.
  `GridAddItem(grid, item, a2, a3)` reads no variable of its self: it sizes the
  item, finds a fit, writes the node and answers the tab, position, tab type
  and success; it touches no map. The game's own Socketable move (Live 1f) is
  `ChangeItemOwner(9, 0, fp)` plus a node placement.
- **The complete by-name take, per case**, as the game's own moves amount to
  it, self `Console_Save_obj` throughout (nothing reads it), bag side first (a
  refusal there changes nothing), map second, stash cell last, and the save
  only after all three. The stacked case (a Materials entry the bag stacks):
  `InventoryGridCanAddToStack(1, undefined, item)`, `InventoryGridAddToStack(1,
  item)`, then `RemoveItemFromMap` on map 9 - the game's own choice for a merge
  (Live 1f `last-unit-material`: the merged unit's entry is deleted, not moved;
  `ChangeItemOwner` would leave an orphan entry in the bag's map with no cell,
  which a count may find) - then `GridRemoveItem` on `Controller_obj`'s
  Materials array. The no-stack case (a Socketable entry): `GridAddItem` into
  the bag's persistent Socketable grid, moving the whole struct (its `o` with
  it), `ChangeItemOwner(9, 0, fp)`, then `GridRemoveItem` on the entry's row of
  `Controller_obj`'s Socketable structure. Each ends with a by-name `SaveStash`
  and then the owner's own stash open and close, the route that ended Live 1g.
- **The recipe's inputs.** The recipe row (`UI_Craft_Recipe_List_Item_obj`,
  its Create closure `anon@840`) and `CraftFindRecipeItems(entries, ?, map)`
  read one entry shape: an item-definition-like struct (it carries the type
  member the inventory code reads) whose amount is stored encrypted and decoded
  with `PilipaliDecrypt` (three arguments) when it is counted;
  `CountInventoryItem` (four arguments) counts the input in the bag, and the
  count is compared with the decoded amount to set `requirementFound`. No
  plain member holds the type, base or amount, which is why Live 1g's member
  reads found none. By name, the shape is read by arming `PilipaliDecrypt` and
  `CountInventoryItem` at list population and at the press, then finding which
  member of the row holds the entry whose value `PilipaliDecrypt` received.
- **What the static reading could not give: variable names.** The executable
  holds none of the game's variable names (`nodeFingerprint`, `localItemMap`,
  `stashPersonalGrid`, `craftIndex` and the rest are in no initialised block);
  the slots the code reads them through are filled from `data.win` at run
  time. So `Controller_obj`'s member names are found live, by shape and
  content (`craftprobe var Controller_obj 0 *`). The string-constant table was
  recoverable (`hs-decomp\slots.csv`, 23,971 entries - the `stash_tab_` key
  prefix among them).
- **SDK cross-check** (`AGENTS.md` § "HS Game SDK Usage"): every script name
  above and every callee the reading followed (53, the `SaveStash` struct
  methods, `___struct___68@CraftFindRecipeItems`, the recipe row's `anon@840`
  and the Cube window's `anon@1834`/`anon@4988` closures included) is an
  `HeroSiege::Scripts::gml_Script_<name>` constant (`PilipaliDecrypt` at index
  85, `CreateItemSaveStruct` at 693), and every object named is in
  `objects.hpp` (`Controller_obj` 984, `Console_Save_obj` 980,
  `New_Inventory_Data_obj` 3067). `SaveStashFunc` and `LoadStashFunc` are also
  in every binding, bare rather than `gml_Script_`-prefixed (`### Phase 1h
  rows`), which is why this search's `gml_Script_`-only pattern missed them
  the first time; neither is a row or a call target here.

**Unverified going in, stated as such:** that `Controller_obj` has a live
instance `craftprobe var` resolves (the game reads its variables through an
object reference, so one is expected); that a `var ... json` dump reaches the
cell structs (the depth cap is 10; they sit at depth 3-4); that
`inventorySocketGrid` is the array `GetItemPreferredGrid` answers for a
class-15 item (a shape match - the ruby landing in the bag's Socketable tab is
the behavioural check); and that a by-name `SaveStash` writes when there is
something new to write (Live 1f's had nothing new). A not-observed result on
any of them is a finding.

### Phase 1i instrument

Phase 1h left a player build blocked on two things (`## Decision gate`,
"After Phase 1h"): the complete by-name take with its stash-cell clear, and
which decoded amount belongs to the selected recipe. Both stopped at the
instrument, not at the game. `craftprobe var Controller_obj 0 *` listed 80 of
the instance's 221 variables, so the Materials and Socketable containers were
never named; and `PilipaliDecrypt` ran 532,552 times while the Cube was open,
so no `arm` budget could reach the selected recipe's decode. The Phase 1i
research build adds four things and nothing else: no row (the table stays at
254), `kCpCraftRouteRows` keeps its nine labels, and every other subcommand,
cap and refusal stays as it was - the `bag|stash|recipe` readers and `store`
keep their "narrow the filter" text, because they take filters. All of it is
inside `#ifndef FORGEPACT_RELEASE`; the player build answers `command
unavailable`.

| Addition | What it prints | Cap | Control |
|---|---|---|---|
| The marker | a bare `craftprobe` answers `craftprobe: phase1i rows=254 - ...`, the count still derived from the table (`kCpTargetCount`) | - | the build's control: without `phase1i` the installed plugin is not this build (the Phase 1h build prints `phase1h rows=254`), and nothing from the session counts |
| Paged `var *` | `var <Obj> <nth> * from=<i>` (or `var id:<n> * from=<i>`) lists variables starting at the i-th name `variable_instance_get_names` returns. The header carries `vars=<V>` (and `from=<i>`), a closing line reads `(indices <a>..<b> of vars=<V> read, <k> printed)`, and a capped page ends with a line naming how many variables remain and the exact command for the next page, `craftprobe var <root> * from=<i>` - the root as it was typed, or a followed instance by `id:<n>`. `from=` past the last variable lists nothing and says so | 80 per page (`kCpReaderMaxLines`) | `test_craftprobe_var_pages_every_variable_and_names_the_next_page`; live, `var-pages`: the pages' counts add up to `vars=` |
| `craftprobe find <Obj> <nth>` or `find id:<n>`, then `[from=<i>] <text>` | hook-free and read-only. It walks every variable of the root into arrays (inside `array_length`) and plain structs, and prints `match <k>: <path> (string)` or `(reference)` for every string equal to `<text>` (the rest of the line, single-spaced) and every reference whose runtime text is `<text>` (`ref ds_map <N>`). The path is the dotted whole-number-segment form `var` and `path:` take. It never enters an instance or a data structure (a reference is compared by its text only) and never calls a method (one is counted). One summary line: matches, values visited, variables walked of the total | depth 8 segments below a variable (`kCpFindMaxDepth`; the first container at the cap is named, and a struct cycle ends there); 100,000 values visited (`kCpFindMaxVisits`; the cap line names the `from=<i>` that resumes); 40 match lines (`kCpFindMaxMatches`; every match is still counted) | `test_craftprobe_find_is_a_hook_free_read_only_content_search`; live, `find-control`: the kept stash map's own text found on the same root, and a text nothing holds giving 0 matches with values visited > 0 |
| `within=` on `PilipaliDecrypt` and `CountInventoryItem`, and `arm ... inroute` | the two rows' armed lines carry `within=<row>#<n>` or `within=none`, like the craft-route rows'. They read the route, push no frame and are not added to `kCpCraftRouteRows`, so Phase 1g's `within=` answers are unchanged; the two are named through their SDK constants (`kCpInRouteRows`). With the keyword `inroute`, `arm` turns on a gate under which those two rows log a call only while a craft-route row is on the stack: a call outside still counts in `calls=` and spends no budget, so `show`'s `unlogged=` is what the gate held back. `arm` and `show` say whether the gate is on; an `arm` without the keyword turns it off | the rows' `arm` budget | `test_craftprobe_inroute_gate_holds_back_calls_outside_the_craft_route`; live, `recipe-shape`: `show all` before the press reads `PilipaliDecrypt calls>0 logged=0` |

**What the local Ghidra reading showed, in this document's words.** Read on
2026-09-24 in the same named local project as Phase 1h: Ghidra 12.1.4
(`C:\Users\stann\tools\ghidra_12.1.4_PUBLIC`), project
`C:\Users\stann\ghidra_projects\HeroSiege`, program `Hero_Siege.exe`. The
bodies were already written to `C:\Users\stann\tools\hs-decomp\`
(`GridRemoveItem`, `GridClear`, `GridAddItem`, `SaveStash`,
`CraftFindRecipeItems` and the recipe row's Create closure `anon@840`, each
raw and simplified); `run_decomp.cmd <Name>` in that directory writes another.
They stay on the owner's machine; nothing below quotes them.

- **What a cell holds, and so how to find the containers.** In every grid
  these functions walk, a cell is either a struct or the runtime's undefined
  value. `SaveStash` and `GridRemoveItem` read the same member of a cell
  struct: the item's fingerprint text, which is also its key in map 9.
  `SaveStash` first checks that the cell's stored x and y match where the cell
  sits. So `Controller_obj` should hold X's fingerprint once, at
  `<M>.<x>.<y>.<member>`, and the ruby's at `<S>.<k>.0.0.<member>`; the
  ordinary tabs sit one array level deeper. A content search for the
  fingerprint text therefore names the container and the cell in one step, and
  the path's shape tells them apart: two index segments are the Materials
  array, three a Socketable row or an ordinary tab. The stash map is a direct
  `ds_map` variable of `Controller_obj` (Phase 1h's reading of
  `GetItemMap(9)`), so a search for its runtime text (`ref ds_map <N>`) has a
  known answer on the same root. That is the search's positive control.
- **What the clear leaves.** `GridRemoveItem` writes the runtime's undefined
  value into each cell holding the fingerprint. That is the value `GridClear`
  fills a new grid with, and the same global the game passes as
  `PilipaliDecrypt`'s third argument (logged `a2=undefined` in Live 1h). A
  cleared cell therefore reads like one never used.
- **`GridAddItem`'s last two arguments are optional.** An undefined third
  argument is taken as 0, and the fourth may be undefined, so `... 0
  undefined` is the default call.
- **Where the recipe's amount ties to the selected recipe.** The recipe row's
  Create closure takes no argument. It reads one variable of its self as an
  index into a global recipe table and resizes a per-input flags array on its
  self (Phase 1g saw `craftIndex` and `requirementFound` on the row; that these
  are the two it uses is unverified). For each input entry it decodes one
  encrypted member with `PilipaliDecrypt` (the member, a game constant,
  undefined), counts the input with `CountInventoryItem`, and stores whether
  the count reaches the amount. `CraftFindRecipeItems` decodes the same member
  of each entry inside its per-entry loop, and once more after the loop on a
  path this reading did not follow; `DoCraftResult` reads that member too.
  Phase 1g measured that the craft rows run with the selected row as self, and
  that `CraftFindRecipeItems #1` runs once at the press, before
  `DoCraftResult #1` (`craft-order`). So a `PilipaliDecrypt` call made inside
  `CraftFindRecipeItems` at the press is the selected recipe's amount - which
  is what the `inroute` gate keeps and the list's calls cannot drown. Live 1h's
  532,552 calls with the Cube open fit the list evaluating every row every
  frame; that is an inference, not a measurement, and it is why a larger
  budget alone cannot reach the press.
- **SDK cross-check** (`AGENTS.md` § "HS Game SDK Usage"): every script the
  procedure calls or arms is a `HeroSiege::Scripts::gml_Script_<name>`
  constant (`hs-game-sdk/cpp/include/hs_game_sdk/scripts.hpp`), and every
  object named is in `objects.hpp` (`Controller_obj` 984, `Console_Save_obj`
  980, `New_Inventory_Data_obj` 3067, `UI_Craft_Recipe_List_Item_obj` 5055).

**Unverified going in, stated as such:** that `find` meets the stash map
directly on `Controller_obj` (`find-control` tests it); that
`inventorySocketGrid` is the array `GetItemPreferredGrid` answers for a
class-15 item (a match on shape only); that a by-name `SaveStash` writes the
file; and that the decodes inside `CraftFindRecipeItems` reach
`PilipaliDecrypt`'s inline detour (the list closure's did in Live 1h). A
not-observed result on any of them is a finding.

### Phase 1j instrument

Phase 1i left the owner's design resting on three things with no measured
mechanism (`## Decision gate`, "After Phase 1i"), and the owner's answers of
2026-09-24 added a fourth. *The save*: a by-name `SaveStash` returned but wrote
no file in four calls across Live 1f and 1i; the owner wants the game itself to
write `stash.hss` with the stash closed, and if no by-name route does, the
build ships without a save at the press. *The partial take*: every proven take
moved a whole entry; the owner wants only the shortfall moved. *Availability*:
the mod's count goes into the game's own check, so a short recipe stays
disabled by the game - a count injection not observed blocks the build.
*Destination*: the take may land in the Crafting Cube's own input grid, since
the bag may be full. The Phase 1j research build adds five things and nothing
else: every other `craftprobe`/`mapkeep` subcommand, cap and refusal stays as
it was, `kCpCraftRouteRows` keeps its nine labels and `within=` its meaning,
and no hook target is added beyond the rows. All of it is inside `#ifndef
FORGEPACT_RELEASE`; the player build answers `command unavailable`.

| Addition | What it prints | Cap | Control |
|---|---|---|---|
| The marker | a bare `craftprobe` answers `craftprobe: phase1j rows=278 - ...`, the count still derived from the table (`kCpTargetCount`) | - | the build's control: without `phase1j` the installed plugin is not this build (the Phase 1i build prints `phase1i rows=254`), and nothing from the session counts |
| 24 rows (`### Phase 1j rows`) | their armed and `ret=` lines, like every row's: the close's `SaveLocalFile #<n> self=... argc=... a0=... a1=...` is the save's shape; the hand split's rows, in call order, are the split's mechanism | the rows' own `arm` budgets | `hook`'s `276 detoured, 0 failed, 2 held by mapkeep`; `CheckPlayerInteraction` climbing after the load |
| `craftprobe callm <Obj> <nth>\|id:<n> <struct> <member> [args ...] confirm` | ONE invocation of a method-valued member of a struct, by name. `<struct>` is `fp:<K>` (the game's own lookup, map 0), `fp9:<K>` (a1=9, the stash map), either optionally followed by a dotted tail through plain structs only (`fp9:<K>.itemDefinitionStruct`), or `path:<root>.<a.b>` walked as `var` walks; the value reached must be a plain struct. The member is read with `variable_struct_exists` then `variable_struct_get` and must be a method - `is_method`, or `typeof` if the runtime does not answer `is_method`. The dispatch is `script_execute(<method>, args...)` with self = other = the named instance, which also makes the `fp:`/`fp9:` lookups: the route `InvokeMethodValue` proved live on 2026-09-11 for a bound method (its route A), reused, never a `CScriptRef` read. Arguments take `call`'s forms (the two now share one resolver). The reply is `call`'s: `entered <no>, script_execute threw`, `entered <no>, script_execute returned st=<s>` or `dispatched <no> -> ret=`, where `<no>` is the call number of the row that names the method's script (read with `method_get_index` and `script_get_name`), so the reply matches that row's own entry line; a refusal names the member's kind and what was supplied, and says nothing was called | one invocation per command, behind `confirm`; every precondition after the gate and before the call | `test_craftprobe_callm_invokes_a_method_value_by_name_behind_confirm`; live, the method's own row printing its entry line with the reply's number |
| `craftprobe set <struct> <member> <number> confirm` | ONE write of one existing member that already holds a number (`variable_struct_set`), read back: `before=<v> after=<v>`. `<struct>` as `callm`'s; `fp:`/`fp9:` look the item up with self the first `Console_Save_obj` instance, the self every by-name trial since Phase 1h used. A missing member, or one that holds anything but a number, is refused naming its kind; nothing is called. It exists for the case the split control shows the game's own edit is inline, when a member write is the game's own step | one member per command, behind `confirm` | `test_craftprobe_set_writes_one_existing_number_member_behind_confirm`; live, the same member re-read by `mapkeep find` or the lookup |
| `craftprobe inject <class> <b> <extra> [owner=<a0>]` / `inject off` | while on, `CountInventoryItem`'s detour replaces the game's own return with return + `<extra>` - after the trampoline, after the row's own `ret=` line and `backing` have kept the game's value - only for a call with `a0` the owner (1 by default, the value Live 1i logged in the craft route; `owner=` sets another), `a1` the class and `a3` the base, made while one of the game's three counting frames is on the game thread's stack: the window's availability call `GetCraftItemsAvailable` (run by `UI_Craft_obj anon@1834`, not inside `anon@840`), the recipe row's Create closure (`UI_Craft_Recipe_List_Item_obj anon@840`), or a craft-route row. The first two keep their own depth counters and are not craft-route rows. A logged call prints `injected: game ret=<v> -> <v>` and the frame. `show` prints `inject: class=<c> b=<b> extra=<e> owner=<o> injected=<n> (availability=<a> recipe-row=<r> craft-route=<c>)`, then the calls of that class and base it left alone - `outside-route=` (outside all three frames), `other-owner=` (another `a0`, with the latest value) and `not-a-number=` - or `inject: off`. Refused unless all three frames' rows and the count row are detoured, so `injected=0` cannot be the instrument's blindness. Nothing is written: it changes one return value inside a call the game is already making | one identity at a time; `inject off` ends it; counters reset on every `inject` | `test_craftprobe_inject_scopes_the_count_to_the_craft_route`; live, the Ol recipe unavailable without, available with, unavailable again after `inject off` |

**What the local Ghidra reading showed, in this document's words.** Read on
2026-09-24 in the same named local project as Phases 1h and 1i: Ghidra 12.1.4
(`C:\Users\stann\tools\ghidra_12.1.4_PUBLIC`), project
`C:\Users\stann\ghidra_projects\HeroSiege`, program `Hero_Siege.exe`, the
runtime's script names applied by `ImportSymbols.java`. The bodies were written
to `C:\Users\stann\tools\hs-decomp\` by
`C:\Users\stann\ghidra_scripts\DecompileTo.java` through `run_decomp.cmd
<Name> ...`, one raw and one simplified file per function. The project was
imported without analysis, so it carries no cross-references; callers were
found with a new local scanner, `C:\Users\stann\ghidra_scripts\FindCallers.java`
(direct `call rel32`/`jmp rel32` sites to a named function), run through
`hs-decomp\run_callers.cmd <Name> ...`. All of it stays on the owner's
machine; nothing below quotes it.

- **Why a by-name `SaveStash` wrote no file.** Measured: four by-name calls
  across Live 1f and 1i returned and moved no file. The reading of why:
  `SaveStash` builds the save struct, walks the stash containers and ends by
  handing the encrypted text to a buffer the game keeps in a global - on this
  reading it serialises, and the file is committed elsewhere, not by
  `SaveStash`. Its one direct call site is inside `SaveLocalFile`, which matches its
  first argument against a table of seventeen save kinds and, per kind, runs
  `SaveStart` with the kind's name and its own second argument, then - only if
  that answered true - the kind's saver (`SaveStash` among `SaveSlot`,
  `SaveLogin`, `SaveCharacter` and the rest), then `SaveCommit`. On this
  reading the stash is the table's second entry, selected by the value 4 - a
  reading of the table's initialiser, unverified until the close logs its
  `a0`. The stash branch reads nothing of `SaveLocalFile`'s self, and
  `EncryptStringSave` is shared by every saver. So the route to measure is
  `SaveLocalFile` by name with the kind (and second argument) the close
  itself passes, stash closed, read from the file's write time and
  `tools/stash_tab_counts.py`. `SaveLocalFile` has about a hundred callers
  with no symbol (object events), one a `Console_Save_obj` Create closure, so
  the close's caller is left to the row. The 1993-versus-1627
  `CreateItemSaveStruct` gap of Live 1i fits a close that saves more than the
  stash - the character's savers call `CreateItemSaveStruct` too - a reading
  the kinds the close logs will settle.
- **How the game splits a stack.** The split dialog's button script
  (`UiASplitStack`) checks the typed amount, looks the item up by
  fingerprint and owner, then calls two method-valued members of the item
  struct - one with a string, the other with the same string and the first
  call's answer minus the amount - and a third with no argument, then builds
  the drag for the split unit. None of its callees creates an item, and the
  dialog's own closure only moves a node, so the new unit's item is made at
  the drop, in code no session has rowed (Live 1f logged `s_InvNode` twice
  placing a one-unit item with a new fingerprint). Which member pair it calls
  is unverified: by shape a string-keyed get/set pair, on the definition
  struct (`GetItemDef`/`SetItemDef`, where `o` lives - measured, Live 1i) or
  on the info struct, and the no-argument call reads as `GenerateItemHash`.
  The hand-split control decides; the creation candidates are rows.
- **The game's merge and consume edit the count inline.** `GridAddToStack`
  and `InventoryGridAddToStack` call no item method: they change the count in
  place and call `ItemCheckHash`; `CraftEditPlayerInventory` calls no item
  method either. So a write of the definition struct's `o` is what the game's
  own merge and consume do; which hash step follows an edit is what the
  control's row order shows.
- **The split-operation family is online.** `InventorySplitOperation`,
  `InventorySplitDrop`, `InventoryStackUpdateAndRemove` and
  `InventoryStackUpdateAndEdit` each go through `InventoryStackHandler`, whose
  callees are the online request and its encoding. Live 1f logged none of them
  on the hand split. Not a route.
- **Where the availability check runs.** The recipe row's Create closure
  (`anon@840`) decodes each input's amount and counts it with
  `CountInventoryItem`, storing whether the count reaches it; the window's own
  availability call, `GetCraftItemsAvailable` (run by `UI_Craft_obj`'s
  `anon@1834`, not from inside `anon@840`), reads the profile data and the
  owner, looks items up by fingerprint and counts with `CountInventoryItem`
  too; and `CraftFindRecipeItems` counts again at the press. Which of the
  three the window's display reads is not on record, so the injection covers
  all three. At the press the owner is 1, with the class and the base (Live
  1i: `a0=1 a1=14 a2=1 a3=51`, logged inside the craft route); the owner the
  other two pass is not on record, so `inject` counts calls with another
  owner (`other-owner=`) instead of dropping them silently.
  `CountInventoryItem` itself walks members of the profile data - the bag's
  grids, not the item map. A count raised inside those three frames only is
  the owner's "inject count into the crafting check".
- **The Cube's input grid.** `CraftFindRecipeItems` and `CraftEditGrid` take
  the Cube's grid as an array argument (Live 1e/1g: `a1=array len=6`, `a2`
  the bag's map, other `UI_Grid_obj`), and `CraftEditGrid` looks each cell's
  fingerprint up in that map - so an item in the Cube's grid keeps its entry
  in map 0, the bag's. The window's Create closures bind its grids through
  `UiSetGrid` (`anon@4988` after reading one profile member, `anon@7914` six
  times). Whether the Cube's grid is a profile array (reachable by `path:`)
  or a window-owned one, and whether `CountInventoryItem` walks it (the
  profile member the window reads is not among those the count reads, so the
  reading says no), are found live: the holder by content search after the
  owner places one unit, the count by the Ol recipe's availability with one Ol
  in the grid. `GridAddItem` reads no self and takes any grid array (Phase 1h
  reading, measured on two containers), so a stash item is placed there by
  name with the whole-take shape and the array swapped.
- **SDK cross-check** (`AGENTS.md` § "HS Game SDK Usage"): every script above
  is a `HeroSiege::Scripts` constant (`SaveLocalFile` 3518, `SaveStart` 3533,
  `SaveCommit` 3534, `SaveFileGMAsync` 3535, `EncryptStringSave` 81,
  `s_ItemInstanceStruct` 2044, `StructCopy` 4562, `AddItemToMap` 2052,
  `ItemCheckHash` 2092, `UiSetGrid` 4470, the item's methods from 2034), and
  every object is in `objects.hpp` (`Console_Save_obj` 980, `Controller_obj`
  984, `New_Inventory_Data_obj` 3067, `UI_Craft_obj` 5054,
  `UI_Craft_Recipe_List_Item_obj` 5055, `UI_Grid_obj` 5081, `Craft_Cube_obj`
  1002). `SaveLocalFileFunc` and `SaveStashFunc` are bare wrappers in the SDK
  and not routes.

**Unverified going in, stated as such:** that the stash's kind is 4 (the
close's `a0` settles it); which get/set pair and which hash method the split
calls; that a bag-to-bag drop runs the same creation code as a
stash-to-bag drop (Live 1f/1g logged the same rows for both; the bag-to-bag
drop itself is not on record); that this runtime answers `is_method` (`callm`
falls back to `typeof`); that `GridAddItem` places into a
`stashSocketItemSlot` row and into the Cube's grid, and `GridRemoveItem`
clears `inventorySocketGrid` - each measured on another container only; the
axis order of `inventoryMaterialGrid.<x>.<y>`; whether the Cube's grid is a
profile array and whether `CountInventoryItem` walks it; and whether the
Cube's grid persists across a save (Phase 1 recorded that the Prospect grid's
contents do not); which of the three counting frames the window's display
reads, and the owner `a0` the recipe row's closure and `GetCraftItemsAvailable`
pass to `CountInventoryItem` (only the craft route's `a0=1` is on record). A
not-observed result on any of them is a finding.

### Phase 1k instrument

Live 1j left the owner's route - a by-name partial take, only the shortfall
leaving a stash stack (`## Decision gate`, After Phase 1j, and the owner's
decision of 2026-09-24: the whole take and its return are ruled out, and the
Crafting Cube's own input grid is an accepted destination) - with the one
`callm` shape it tried rejected and no creation shape to try. The Phase 1k
research build adds five things and nothing else: every Phase 1e-1j
subcommand, cap and refusal stays as it was, `kCpCraftRouteRows` keeps its
nine labels, `within=` its meaning, `inject` its scope, and no hook target is
added beyond the rows. There is no composite "split" subcommand: each step is
one confirm-gated command, so a route that fails at its third call leaves the
operator free to try the other branch in the same launch. All of it is inside
`#ifndef FORGEPACT_RELEASE`; the player build answers `command unavailable`.

| Addition | What it prints | Cap | Control |
|---|---|---|---|
| The marker | a bare `craftprobe` answers `craftprobe: phase1k rows=282 - ...`, the count still derived from the table (`kCpTargetCount`) | - | the build's control: without `phase1k` the installed plugin is not this build (the Phase 1j build prints `phase1j rows=278`; the first Phase 1k build, `d6a5582`, printed `phase1k rows=281` and was never installed), and nothing from the session counts |
| 4 rows (`### Phase 1k rows`) | their armed and `ret=` lines, like every row's: `InitItemFromJson`'s arguments and return, and the order the game's own loaders call it in; `ReportClient`'s arguments if the game raises a hash flag | the rows' own `arm` budgets | `hook`'s `280 detoured, 0 failed, 2 held by mapkeep`; `CheckPlayerInteraction` climbing after the load |
| `call`'s own kept return | after a `call` that ran, `  kept as kept:<row> #<n> (by-name call <k>; <shape>; ...)`, `<k>` counting that row's by-name dispatches (a row mapkeep holds is never detoured, so its `#<n>` never moves): the return of that very call, held in a slot of the row's own that no detour writes and rooted in the research global `__cp_call_<row>`. `kept:<row>` - as a `call`/`callm` argument or as `set`'s struct - reads that slot first and the game's latest return `backing` kept only when the row has none, and each use prints which one answered (`kept:<row> is the by-name `call <row>` #<n> return` or `... the game's call #<n> ..., kept by `backing``). The slot is emptied before every dispatch, and a dispatch that did not return (no script, threw, a failed status) leaves it empty with the reason (`  kept:<row> is empty (by-name call <k> (#<n>): script_execute threw); ...`), so `kept:<row>` then refuses, naming that call, and never falls back to an earlier call's return or the game's. With neither, the refusal names the cause: the row is held by mapkeep, the row is not detoured, `backing on <row>` was not run, or the game has not called it since. `backing clear` releases these slots and a lost call's reason too | one kept value per row, replaced only by the next `call` of that row | `test_craftprobe_call_keeps_its_own_return_for_kept`; live, `kept:GetItemMap` answering after `call GetItemMap Console_Save_obj 0 0 confirm` with mapkeep holding the row |
| `craftprobe callm ... [args ...] bind confirm` | the word `bind` directly before `confirm`: after every existing precondition, in the same order (the gate, the self, the struct, the member read and the method check, the arguments), the member is re-bound to the struct it was read from through the runtime's own `method` builtin, by name - `method(<struct>, <member>)`, the route `HashRouteMethod` ships for `GenerateItemHash` - and that bound value is what the one `script_execute` gets, self = other = the named instance as before. Refused, naming what was supplied, if `method` throws or gives anything but a method. The header line ends in ` bind`, then `bind=yes method(<struct>, <struct>.<member>) method_get_self: before=<v> after=<v>`, the runtime's `method_get_self` reading for the value as read and as bound (`<not answered>` if the call throws; UNVERIFIED on this runtime - `undefined` reads the same for an unbound method and for a runtime that answers nothing - and the dispatch does not depend on it). Without `bind` the command is Live 1j's; no `CScriptRef` read, no `InvokeMethodValue` route B, no address | one invocation per command, behind `confirm` | `test_craftprobe_callm_bind_rebinds_through_the_runtime_method_builtin`; live, `bind-control`: `GetItemDef o` on a stash item throwing unbound and answering the item's count bound, in that order |
| `craftprobe set kept:<row>[.a.b] <member> <number> confirm` | `<struct>` may be a row's kept return - the row's own by-name `call` return (above), else the game's latest return under `backing on <row>` - walked through plain structs by the same tail as `fp:`/`fp9:` (`kept:StructCopy.itemDefinitionStruct`). Refused, naming what was supplied and why, when that row keeps nothing. Only `set` takes the form; `callm`'s struct forms are Live 1j's | one member per command, behind `confirm` | `test_craftprobe_set_accepts_a_kept_return`; live, the member re-read through the kept struct or the item made from it |

**The build's review round: why `call` keeps its own return.** The first
Phase 1k build (`d6a5582`, never installed) kept a row's return only from
inside that row's craftprobe detour, and the no-stack take names three kept
returns. Two things made that instrument blind to the trial it was built
for. `GetItemMap`'s row is held by mapkeep - the session runs `mapkeep on`
before `craftprobe hook`, because mapkeep refuses to install over a row
craftprobe already detours, and the kept stash map is what every `mapkeep
find` read rests on - so craftprobe never detours `GetItemMap`, and nothing
could fill `kept:GetItemMap`: the `AddItemToMap` call would have been refused
with "`backing on` first" although `backing on` had been run. And a detour's
kept value is the row's latest return from any caller, so the game's own
`GetItemMap(0)` (Live 1j: 26903 calls in one session) and a save's
`CreateItemSaveStruct` calls would replace the value between the by-name call
and the command naming it. So `call` now keeps what its own dispatch
returned, in a slot no detour writes, and `kept:` reads that slot first; the
session order is unchanged (`mapkeep on`, then `craftprobe hook`), and
`backing on` is no longer needed for a value the session made by name.

The second review round closed one more gap in the same slot. As first
rebuilt (`0d58d5c`, never installed), only a `call` that ran touched it, so a
`call` that threw, failed or found no script left the previous call's return
in place, and `kept:` would then hand that on - or, with no earlier by-name
call, the game's latest return under `backing`. After a thrown `call
InitItemFromJson` in `partial-cube`, the next `call AddItemToMap ...
kept:InitItemFromJson` would have re-added the unit `partial-nostack` made,
and the result read as the game's. The slot is now emptied before every
dispatch (after the arguments are read), a dispatch that did not return
records why, and `kept:` refuses on that. One limit is recorded here rather
than fixed, because fixing it needs a rebuild that would break the match
between this source and the DLL Live 1k ran (`### Phase 1k results`): a
`call` naming its own row's `kept:` argument passes a struct that is
**not rooted** during that same dispatch (a copy held only by an
`RValue`), and nothing refuses the case - no Live 1k step named a row's own
kept return, so the limit never touched a measurement. A second, narrower
gap: if `variable_global_set` throws inside
`CpKeepCallReturn`, the slot keeps the pre-dispatch reason "did not return",
even though the console's `ret=` line shows the call did in fact return. The
reply also counts the row's by-name calls itself (`by-name call <k>`), since
`GetItemMap`'s `#<n>` comes from a detour mapkeep's hold keeps at zero and
would read `#1` on every call.

**Why Live 1j's partial take threw, and what changes.** Live 1j ran
`SetItemDef` and `GenerateItemHash` through `callm` with self = other =
`Console_Save_obj`, and all four calls threw inside the game's method with no
state changed (`### Phase 1j results`, Question 3). The reading below explains
it: the item's methods are stored on the struct unbound, so `script_execute`
runs one with the caller's self, and the method's first act - reading the
item's definition struct off its self - fails on an instance that has none;
the game's own member call on the item supplies the struct. The by-name
equivalent is the runtime's `method(<struct>, <function>)`, which returns the
function bound to that struct, so `script_execute` on the bound value runs
with the struct as self whatever instance the plugin passes. The plugin's
`HashRouteMethod` already does this for `GenerateItemHash` in the player
build, but the hub guide's `hashprobe` entry records that `+method` was never
measured to change a hash, so this is a reading with a shipped precedent, not
a measurement: the session measures it first with a read-only positive
control (`bind-control`). Neither method reads `other` (the reading), so the
control's `UI_Split_Stack_obj` other is not supplied, and that is recorded.

**What the local Ghidra reading showed, in this document's words.** Read on
2026-09-24 in the same named local project as Phases 1h-1j: Ghidra 12.1.4
(`C:\Users\stann\tools\ghidra_12.1.4_PUBLIC`), project
`C:\Users\stann\ghidra_projects\HeroSiege`, program `Hero_Siege.exe`, the
runtime's script names applied by `ImportSymbols.java`. The bodies were
written to `C:\Users\stann\tools\hs-decomp\` through `run_decomp.cmd <Name>
...` (and `py -3 simplify.py <Name>.c`), callers found through
`run_callers.cmd <Name> ...` (`FindCallers.java`). All of it stays on the
owner's machine; nothing below quotes it.

- **The item's methods.** The item constructor, `s_ItemInstanceStruct`,
  builds its nine methods with no bound self and stores them on the struct.
  `GetItemDef(key)` reads one member of its self - the definition struct -
  and returns that struct's element `key`; `SetItemDef(key, value)` sets
  that element and nothing else; `GenerateItemHash()` joins several members
  of its self (one through `DecryptStringApi`) and stores the digest on its
  self. So `GetItemDef("o")` answers a stack's count only with the item as
  self - the positive control - and `SetItemDef("o", n)` then
  `GenerateItemHash()` is the split's own per-unit edit (Live 1j,
  `split-control`).
- **`ItemCheckHash(item)`** takes the item as its one argument, re-runs the
  item's hash method through the item, compares it with the stored hash and
  reads the repository structs. The Custom Forge calls it by name (the global
  instance as self) after dressing an item, proven live since v1.3.13
  (`HashRouteItemCheck`): the by-name hash step for an inline edit, and what
  the game's own merge runs after its own count edit.
- **Creation without `new`.** `s_ItemInstanceStruct` has no direct call site
  in the image (`new` goes through the runtime), so the split's own creation
  is not replayable by name. Two of the game's own routes make an item
  without it. (a) The merchant's multi-buy (`UiAMerchantBuyMultiple`):
  `StructCopy(<item>)` (a one-argument clone), `LootTimestamp()`, member
  writes on the copy, `GetItemMap`, `AddItemToMap` with three arguments (a
  map set; their order was not read), `InventoryGridAddItem` and
  `GetCounterHash`. (b) The loaders: `InitItemFromJson(<save-shaped struct>,
  <fingerprint key>)`, then `AddItemToMap` - the pattern of
  `ParseItemToGrid`, `ControllerLoadOnlineData` and the trade and market
  handlers, and of the plugin's own `SpawnSignatureItem`, which builds
  Headhunter and Tyrant's Crown through `InitItemFromJson` with a parsed
  definition and a `0-0-<ms>-<type>` key in the player build, proven live.
  `CreateItemSaveStruct(<item>)` (a row since Phase 1h) returns the
  save-shaped struct, so (b) is the round trip the game makes at every load.
  `ReCreateItem(x)` is one helper call with `(x, undefined)`: a row, not a
  route. Whether `StructCopy`'s clone shares nested structs with its source
  was not read.
- **SDK cross-check** (`AGENTS.md` § "HS Game SDK Usage"): every script is a
  `HeroSiege::Scripts` constant (`InitItemFromJson` 1883, `ReCreateItem`
  2094, `ParseItemToGrid`, `ReportClient`), and every script the trials call
  or arm is a `craftprobe` row already except those four, which this build
  adds. No container the SDK lacks is named.

**The trials, per lead** (self `Console_Save_obj 0` throughout; every write
behind `confirm`; the step-by-step is the workorder's `### Live procedure 1`):

- **`bind-control`** (read-only): `GetItemDef o` on the stash entry
  `fp9:<K_X>`, unbound first - it must throw as Live 1j's calls did (the
  negative control) - then with `bind`, which must answer the entry's count.
  Only that pair, in that order, licenses the bound writes; a bound call that
  also throws sends `partial-stacked` to the inline branch.
- **`partial-stacked`** (one unit of a Materials stack into the bag stack of
  the same material): bound - `SetItemDef o <o-1>` and `GenerateItemHash` on
  the stash entry, `SetItemDef o <k+1>` and `GenerateItemHash` on the bag
  stack, each with `bind` (the split's edit on both sides; the game's drop
  merge is the same count edit plus a hash). Inline - `set
  <struct>.itemDefinitionStruct o <n>` then `call ItemCheckHash` on each
  side. Then **`hash-accept`**: the owner's drag of the edited bag stack to
  an empty cell and back runs the game's own `ItemCheckHash` on it (the
  reading: the grid input handler calls it); a `ReportClient` call names the
  flag the game raised, and the edit is then not a route.
- **`partial-nostack`** (one unit of a Materials stack into a bag holding
  none). Json branch first (each `call` keeps its own return, which the
  next command names as `kept:<row>`; `backing on` is not needed for it):
  `call CreateItemSaveStruct` on the stash entry (kept); `set
  kept:CreateItemSaveStruct o 1`; `call LootTimestamp` (the stamp `<S>`, the
  split's fingerprint shape in Live 1j); `call InitItemFromJson` with the kept
  struct and `0-0-<S>-14` (the plugin's order; on a throw, swapped once);
  `call GetItemMap` for map 0 (kept by the call itself - mapkeep holds the
  row, so no detour could keep it); `call AddItemToMap` with the kept map,
  the key and the kept item (order UNVERIFIED; on a throw, one other order
  once); the lookup of the new key in map 0; `GetItemPreferredGrid` and
  `GridAddItem` into the bag's Materials grid (Phase 1i's proven shape); then
  the stash entry's own edit by the route `partial-stacked` proved. Clone
  branch, only if `InitItemFromJson` gives no item: `call StructCopy` on the
  stash entry (kept), `set kept:StructCopy.itemDefinitionStruct o 1`, the
  source's `o` re-read (unchanged keeps the branch; lowered to 1 means the
  clone shares its nested struct - stop and record), the clone's stamp
  member set to `<S>`, `call GetItemFingerprint` on the clone (its key),
  `call ItemCheckHash` on it, then `AddItemToMap` and the grid add as above.
- **`partial-cube`** (a second unit into the Crafting Cube's grid): the
  branch that succeeded with the grid `path:New_Inventory_Data_obj.craftGrid`
  (Live 1j `cube-place`: `GridAddItem` takes any grid array; the unit is made
  in map 0, so no `ChangeItemOwner`), and the source lowered again; then the
  owner's drag of that unit onto the bag's stack of the same material is the
  game's own merge accepting a created unit, or the finding.
- **What would make a trial unsafe, and the mitigation.** A method run with
  the wrong self is a caught GML error (Live 1j: four, no crash); an edit the
  hash check rejects surfaces at `hash-accept`, before any save; a malformed
  created item could fault the bag's draw, so one unit per trial, with the
  lookup re-read before the grid add. Saves are backed up before the launch
  and restored after it.

**Unverified going in, stated as such:** that `method` re-binds the item's
methods on this runtime (`HashRouteMethod`'s `+method` is shipped but was
never measured to change a hash) and that `method_get_self` answers; the
argument order of `AddItemToMap` (three arguments, not read); that
`InitItemFromJson` accepts `CreateItemSaveStruct`'s struct with `o` edited
and a `0-0-<S>-14` key and returns an item (the plugin's order is proven for
a parsed definition, not for this struct); whether `StructCopy`'s clone shares
its nested structs with its source; the name of the clone's stamp member
(read from the kept struct's listing in the session); that an edited stack
passes the game's own `ItemCheckHash` when the owner moves it, and whether
`ReportClient` fires; and that the game's own merge accepts a unit this
toolkit created, into the bag or from the Cube's grid. A not-observed result
on any of them is a finding.

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

### Live procedure 1g

Live 1g runs the Phase 1g research build (`### Phase 1g instrument`; its hash
is in `### Phase 1g results`), one launch, under `live-operator`; the
step-by-step procedure is `### Live procedure 1` in the workorder's context
file, `.claude/workorders/forgepact-issue-14-phaseA-context.md`, which stays on
the owner's machine. It asks four questions, all with the stash window closed
and the Crafting Cube open, since the two are never open together: (1) which
recipe members name an input's type, base and amount, read by name; (2) at one
craft, which craft-route row encloses the consume and the result's production
(`within=`), and what `CraftEditPlayerInventory`'s `a0` is at amount 2; (3)
whether a whole stash entry moves into the bag by name through the routes the
game's own moves were seen to use, confirmed by the kept map dropping the
entry on the same index; (4) whether a by-name `SaveStash` then writes
`stash.hss`, and whether the stash window draws as usual afterwards. What this
document fixes is its shape:

- **The Phase 1g build, one launch.** The installed plugin's SHA-256 is
  checked first; a different hash stops the session, and installing is the
  owner's call. Character slot 14 ("Sorak"). Auto-prospect off (one instrument
  per session). The saves are backed up before the launch (an independent copy
  and the `hs-drive` backup) and restored after it. `tools/stash_tab_counts.py`
  reads both tabs before the launch and after it, and the file's
  `LastWriteTimeUtc` is read before the launch (T0), at the stash's close (T1),
  directly before (T2) and after (T3) the by-name save with the stash closed,
  again a few seconds after T3 (T3b), and directly before (T4) and after (T5)
  the same by-name save with the stash open.
- **`mapkeep on` before `craftprobe hook`**, both before the character loads.
- **The map at the Cube**: with the Cube open and the stash not yet opened in
  the launch, `GetItemMap` by name with self `Console_Save_obj` and the one
  argument `9` (Live 1f's `map-by-name` shape), then `mapkeep stat` and
  `mapkeep find` on the Ol and Unstable Dust entries against the file.
- **The recipe's shape, hook-free**: `craftprobe recipe`, `craftprobe var` on
  `UI_Craft_Recipe_List_Item_obj` and `UI_Craft_obj`, and `craftprobe bag` for
  the bag's grid instances (the candidate selves for the move).
- **One craft, at amount 2**, of the Greater Unstable Dust recipe from bag
  inputs, with the rows armed: each craft-route row's `within=` and
  `CraftEditPlayerInventory`'s `a0` are quoted. `a0` still `1.0` at amount 2
  reads as an owner value, `2.0` as the amount. A row encloses both the
  consume and the production only when both lines name the same `<row>#<n>`
  and that row's `#n` entry and `ret=` lines bracket them (`### Phase 1g
  instrument`). `craftprobe show all` is read directly before the craft and
  after it, and each craft-route row's `logged=` against `calls=` is quoted: a
  row that had already logged its whole budget before the craft (the Cube's
  own frame or UI work can call a row, as `GetInventoryGridNode` fired three
  times on one stash tab click in Phase 1) reads `not observed (budget spent
  before the craft)`, never absent.
- **The move, by name, under `confirm`, one Materials entry and then one
  Socketable entry.** The lookup with the stash closed comes first
  (`GetItemFromFingerprint(<key>, 9)` with self `Console_Save_obj`, else a bag
  grid instance); then, with the self that resolved it,
  `InventoryGridCanAddToStack(1, undefined, <item>)`,
  `InventoryGridAddToStack(1, <item>)` and `ChangeItemOwner(9, 0, <key>)` -
  the game's own order, placement then owner change - and `RemoveItemFromMap`
  on the kept map only if the entry is still there. Each is followed by
  `mapkeep stat` and `mapkeep find` on the same index. The Materials entry is
  one Greater Unstable Dust (class 14, `b=51`) the owner moves into the
  Materials tab by hand during the session; the Socketable entry is the
  class-15 `b=51` stack of 10, or a one-unit socketable the owner names, and
  runs only if the Materials move was confirmed. Ol (`b=1`, 216) is never
  moved. No partial-stack decrement is replayed: `take-partial` records the
  shapes the craft logged.
- **The save, by name, only after a confirmed move**: `SaveStash` with self
  `Console_Save_obj` and no argument (Live 1f's `save-shape`), judged on T2 ->
  T3, on T3b (a write that lands a few seconds late is not "no write"), and
  on `tools/stash_tab_counts.py` no longer listing the moved entry. Its
  positive control is the same call on the same route with the stash window
  open, run only when the closed-window save ran: when the owner opens the
  stash afterwards, and before they close it, T4, the same `SaveStash` call,
  T5. The only by-name `SaveStash` run so far
  (Live 1f, stash closed) saw no write, and its stash-open trial was skipped,
  so this route has never been seen to write at all; without T4 -> T5 a
  closed-window T3 = T2 could not tell "does not write while closed" from
  "never writes when called by name". The results go in `save-closed`'s
  observed text; the fifteen check names stay as they are.
- **What would make a trial unsafe, and the mitigation.** A wrong self or
  argument is a GML error inside `script_execute` that ends the launch; a
  by-name move the map does not follow, once saved, could write a duplicate or
  lose an item; `SaveStash` writes the player's real `stash.hss` from whatever
  the game serialises. Mitigation: shapes the game itself used, one entry at a
  time, `confirm`, the save only after a confirmed move, saves backed up and
  restored, and results read from the file rather than assumed. A crash during
  a trial is that trial's result, recorded with the shape supplied; the later
  checks read `not-observed (launch ended at <check>)`.
- **Unverified going in, stated as such**: that `craftprobe var` reaches the
  recipe array's entry deeply enough to read its members; that
  `GetItemFromFingerprint(<key>, 9)` resolves with self `Console_Save_obj`
  (Live 1f used a grid self); that `InventoryGridAddToStack(1, <item>)` accepts
  a non-grid self and carries a stack's `o`; that a bag grid instance exists
  while the Cube is open. A not-observed result on any of them is a finding.
- **The capture** is `.claude/workorders/forgepact-issue-14-phaseA-live-1.md`,
  ending with a `## Checks` section of one line per check, exactly
  `- <check> | expected: <text> | observed: <text> | pass|fail|not-observed`,
  for these fifteen checks in this order: `dll-hash`, `marker`,
  `counts-tool-before`, `hook`, `control`, `map-at-cube`, `recipe-shape`,
  `craft-order`, `lookup-closed`, `take-material`, `take-socket`,
  `take-partial`, `save-closed`, `stash-window-after` and `counts-tool-after`.
- **Which control vouches for which read.** The armed row lines, `within=`
  included, rest on `dll-hash`, `marker` (`phase1g rows=252`) and `control`
  (`CheckPlayerInteraction` non-zero after the load), and a `within=none`
  also on `hook`'s `0 failed`. `mapkeep find` and `mapkeep stat` reads rest on
  `hook` (`both-routes` for both keeper hooks) and on `control`'s `a0=0
  calls=` being non-zero; `map-at-cube` rests on `counts-tool-before`'s file
  counts. A move's `dropped` read counts as the map's own behaviour only when
  the kept index and `refreshed=` are unchanged across it (Live 1f's same-map
  rule; otherwise it is `re-kept`, which proves nothing). `lookup-closed`
  rests on Live 1f's grid-self lookup having returned the struct; a lookup
  that returns `undefined` is not-observed, with the self supplied. The
  by-name save's write rests on T2, read directly before the call, and on the
  game's own close moving T0 -> T1 in the same session; a closed-window
  no-write (T3 and T3b equal to T2) counts as a finding about the window only
  beside T4 -> T5 moving on the same by-name route with the window open, and
  with T5 = T4 as well it is a finding about the route, not the window.
  `craft-order`'s `within=` readings rest on each craft-route row's
  `logged=`/`calls=` before and after the craft. A row that did not
  fire while its control climbed is "not observed", never "does not fire". A
  `fail` or `not-observed` is a finding, recorded under `### Phase 1g
  results`; no live outcome is an acceptance criterion.

### Live procedure 1h

Live 1h runs the Phase 1h research build (`### Phase 1h instrument`; its hash
is in `### Phase 1h results`), one launch, under `live-operator`; the
step-by-step procedure is `### Live procedure 1` in the workorder's context
file, `.claude/workorders/forgepact-issue-14-phase1h-context.md`, which stays
on the owner's machine. It measures the complete by-name take the Ghidra
reading describes on one Materials entry (the stacked case) and one Socketable
entry (the no-stack case), each followed by a by-name `SaveStash` that writes
and by the owner's own stash close that does not end the game, and it reads
the recipe's shape at one craft press. What this document fixes is its shape:

- **The Phase 1h build, one launch.** The installed plugin's SHA-256 is
  checked first; a different hash stops the session. Character slot 14
  ("Sorak"). Auto-prospect off (one instrument per session). The saves are
  backed up before the launch (an independent copy and the `hs-drive` backup)
  and restored by the driver after it, verified by hash. `tools/stash_tab_counts.py`
  reads the stash before the launch (Dust `class=14 b=50 stack=13`, Ol
  `class=15 b=1 stack=216`, the ruby stack `class=15 b=51 stack=10`, no
  `class=14 b=51`), after each save and after the launch; `stash.hss`'s
  `LastWriteTimeUtc` is read at every save and close (T0 before the launch,
  then T1 onwards).
- **`mapkeep on` before `craftprobe hook`**, both before the character loads;
  `hook` expects `252 detoured, 0 failed, 2 held by mapkeep`.
- **The healthy save first**: the owner moves one Greater Unstable Dust (X,
  class 14, `b=51`) from the bag into the Materials tab by hand and closes the
  stash, with `SaveStash`, `CreateItemSaveStruct` and `___struct___359` armed,
  giving the signature every later save is compared with.
- **At the Cube** (stash closed): the map by name (`GetItemMap` with the one
  argument `9`), `K_X` and `K_S` from `mapkeep find`, the recipe rows armed
  (`PilipaliDecrypt`, `CountInventoryItem`, `CraftFindRecipeItems`), and the
  holders read from `Controller_obj` by shape: the stash map (the `ref ds_map`
  whose index is the kept one), `<M>` (a two-level array holding `K_X`) and
  `<S>` (an array of rows, `K_S` at row `k`).
- **The stacked take** (X): the closed-window lookup, then
  `InventoryGridCanAddToStack(1, undefined, fp9:<K_X>)`,
  `InventoryGridAddToStack(1, fp9:<K_X>)` and `RemoveItemFromMap(map9, <K_X>)`,
  the map dropping X on the same index. Then `error-baseline` - kept by the
  owner's decision of 2026-09-23 - calls `SaveStash` by name before the stash
  cell is cleared, to reproduce Live 1g's fault on purpose inside the
  instrument's catch; then `GridRemoveItem(path:Controller_obj.<M>, <K_X>)`,
  the by-name `SaveStash` that should now return and write, and the owner's
  own stash open and close on the Materials tab.
- **The no-stack take** (S, the ten-unit Pristine Ruby stack; only if `<S>`
  and `k` were named): `GetItemPreferredGrid` and the bag's
  `inventorySocketGrid` read, `GridAddItem(path:New_Inventory_Data_obj.inventorySocketGrid,
  fp9:<K_S>, 0, undefined)`, `ChangeItemOwner(9, 0, <K_S>)`, the map dropping
  S on the same index, `GridRemoveItem(path:Controller_obj.<S>.<k>, <K_S>)`,
  the by-name save, one craft press of the Greater Unstable Dust recipe at its
  fixed amount (`recipe-shape`'s second part) and the owner's own stash open
  and close on the Socketable tab.
- **What would make a trial unsafe, and the mitigation.** A wrong self or
  argument is a GML error inside `script_execute` that may end the launch; a
  take the map or the grid does not follow, once saved, could write a
  duplicate or lose an item; `error-baseline` raises a GML runtime error on
  purpose; `SaveStash` writes the player's real `stash.hss`. Mitigation: only
  shapes the game itself uses, bag side first so a refusal there changes
  nothing, one entry at a time behind `confirm`, `error-baseline` only by name
  (the instrument's catch survived the same fault twice in Live 1g) and never
  through the game's own close, the owner's closes only after a by-name save
  returned, saves backed up and restored, and results read from the file
  rather than assumed. A case that cannot complete its sequence stops before
  its first write and reads not-observed; a crash is that check's result,
  recorded with what was supplied, and every later check reads
  `not-observed (launch ended at <check>)`.
- **The capture** is `.claude/workorders/forgepact-issue-14-phase1h-live-1.md`,
  ending with a `## Checks` section of one line per check, exactly
  `- <check> | expected: <text> | observed: <text> | pass|fail|not-observed`,
  the verdict token last on the line (a reason goes in the observed text), for
  these eighteen checks in this order: `dll-hash`, `marker`,
  `counts-tool-before`, `hook`, `control`, `save-control`, `map-at-cube`,
  `holders`, `lookup-closed`, `take-material`, `error-baseline`,
  `save-after-take`, `close-after-take`, `take-socket`, `save-after-socket`,
  `close-after-socket`, `recipe-shape` and `counts-tool-after`.
- **Which control vouches for which read.** Every armed line rests on
  `dll-hash`, `marker` (`phase1h rows=254`) and `control`
  (`CheckPlayerInteraction` non-zero after the load); a row that stayed silent
  also on `hook`'s `0 failed`. `mapkeep find`/`stat` reads rest on `hook`
  (`both-routes` for both keeper hooks) and on `control`'s `a0=0 calls=` being
  non-zero; a `dropped` read counts only on the same kept index (a re-keep
  proves nothing). `error-baseline`'s fault and every later save's health are
  read against `save-control`'s healthy `CreateItemSaveStruct` signature, and
  a `call` reply against its row's own `#<n>` entry line. A by-name save's
  write rests on the T read directly before it, and on the game's own close
  moving T0 -> T1 in the same session; a no-write is a finding about the
  by-name route only beside that close. `holders` rests on the `json` dumps
  reaching the cells, `take-socket`'s placement on the ruby being seen in the
  bag's Socketable tab. A `fail` or `not-observed` is a finding, recorded under
  `### Phase 1h results`; no live outcome is an acceptance criterion.

### Live procedure 1i

Live 1i runs the Phase 1i research build (`### Phase 1i instrument`; its hash
is in `### Phase 1i results`), one launch, under `live-operator`; the
step-by-step procedure is `### Live procedure 1` in the workorder's context
file, `.claude/workorders/forgepact-issue-14-phase1i-context.md`, which stays
on the owner's machine. Its centre is the complete by-name take for each case
and the by-name save after it, and the owner's one stash close comes last:
only the close has ever ended a launch (Live 1g and 1h), and a by-name save
walks the same cells as the close's save, so each case's evidence is in hand
before the close is tried. What this document fixes is its shape:

- **The Phase 1i build, one launch.** The installed plugin's SHA-256 is
  checked first; a different hash stops the session. Character slot 14
  ("Sorak"). Auto-prospect off (one instrument per session). The saves are
  backed up before the launch (an independent copy and the `hs-drive` backup
  labelled `forgepact-issue-14-phase1i-live-1`) and restored by the driver
  after it, verified by hash. `tools/stash_tab_counts.py` reads the stash
  before the launch (Dust `class=14 b=50 stack=13`, Ol `class=15 b=1
  stack=216`, the ruby stack `class=15 b=51 stack=10`, no `class=14 b=51`),
  after each save and after the launch; `stash.hss`'s `LastWriteTimeUtc` is
  read at every save and close (T0 before the launch, then T1 onwards).
- **`mapkeep on` before `craftprobe hook`**, both before the character loads;
  `hook` expects `252 detoured, 0 failed, 2 held by mapkeep`, and the marker
  `phase1i rows=254`.
- **The healthy save first** (`save-control`): the owner moves one Greater
  Unstable Dust (X, class 14, `b=51`) from the bag into the Materials tab by
  hand and closes the stash, with `SaveStash`, `CreateItemSaveStruct` and
  `___struct___359` armed at `budget=5000` (`kCpMaxLogBudget`), so the
  per-item entries are no longer spent on the ordinary tabs before the
  Materials tab as in Live 1h.
- **At the Cube** (stash closed, the Greater Unstable Dust recipe selected
  and left open): the map by name (`GetItemMap` with the one argument `9`),
  `K_X` and `K_S` from `mapkeep find`; then `save-by-name-control`, a by-name
  `SaveStash` before any take, whose `CreateItemSaveStruct calls=` and zero
  `a0=undefined` entries are the healthy by-name signature every later save
  is compared with.
- **Naming the holders.** `find-control` searches `Controller_obj` for the
  kept map's runtime text (`ref ds_map <N>`, at least one match) and for a
  text nothing holds (0 matches, values visited > 0); `var-pages` follows
  `var Controller_obj 0 *`'s cap lines until the listed counts add up to
  `vars=`; `holders` searches for `K_X` (one match, `<M>.<x>.<y>.<member>`)
  and `K_S` (one match, `<S>.<k>.0.0.<member>`). No take starts until its
  container and cell are named; a case whose holder stays unknown stops
  before its first write.
- **The stacked take** (`lookup-closed`, `take-material`, `save-after-take`):
  the closed-window lookup, then `InventoryGridCanAddToStack(1, undefined,
  fp9:<K_X>)`, `InventoryGridAddToStack(1, fp9:<K_X>)`,
  `RemoveItemFromMap(map9, <K_X>)` and `GridRemoveItem(path:Controller_obj.<M>,
  <K_X>)`, a `find` for `K_X` then giving 0 matches; then a by-name
  `SaveStash`, expected to make one `CreateItemSaveStruct` call fewer than
  `save-by-name-control` and to leave no X in the file.
- **The no-stack take** (`take-socket`, `save-after-socket`; only if `<S>` and
  `k` were named): `InventoryGridCanAddToStack` expected to answer
  `undefined`, `GetItemPreferredGrid` and the bag's `inventorySocketGrid`
  read, `GridAddItem(path:New_Inventory_Data_obj.inventorySocketGrid,
  fp9:<K_S>, 0, undefined)`, `ChangeItemOwner(9, 0, <K_S>)`,
  `GridRemoveItem(path:Controller_obj.<S>.<k>, <K_S>)` and a `find` giving 0
  matches; then the by-name save, one call fewer again and no
  `class=15 b=51` in the file.
- **The recipe tie** (`recipe-shape`): `arm budget=200 inroute` on
  `PilipaliDecrypt`, `CountInventoryItem`, `CraftFindRecipeItems` and
  `DoCraftResult`; `show all` before the press is the gate's control
  (`PilipaliDecrypt calls>0 logged=0`); then one craft press of the Greater
  Unstable Dust recipe at its fixed amount, and the decode logged
  `within=CraftFindRecipeItems#1` beside the `CountInventoryItem` line for the
  Dust (`a1=14 a3=51`) is compared with the owner's figure, 5.
- **The one close** (`close-after-takes`, only if every clear that ran
  answered `true`): the owner closes the Cube, opens the stash on the
  Materials and the Socketable tab, and closes it - the route that ended Live
  1g and Live 1h; then `counts-tool-after` after a graceful stop.
- **What would make a trial unsafe, and the mitigation.** A wrong self or
  argument is a GML error inside `script_execute` that may end the launch; a
  take the map or the grid does not follow, once saved, could write a
  duplicate or lose an item; `SaveStash` writes the player's real
  `stash.hss`; and the stash close has ended the game twice after a take that
  left the stash cell. Mitigation: only shapes the game itself uses, bag side
  first so a refusal there changes nothing, one entry at a time behind
  `confirm`, no take before its holder is named and a `find` after each clear
  confirming it, the close once and last and only if every clear answered
  `true`, no deliberate faulting save (so a close crash cannot be confused
  with an earlier throw), saves backed up and restored, and results read from
  the file rather than assumed. A case that cannot complete its sequence
  stops before its first write and reads not-observed; a crash is that
  check's result, recorded with what was supplied, and every later check
  reads `not-observed (launch ended at <check>)`.
- **The capture** is `.claude/workorders/forgepact-issue-14-phase1i-live-1.md`,
  ending with a `## Checks` section of one line per check, exactly
  `- <check> | expected: <text> | observed: <text> | pass|fail|not-observed`,
  the verdict token last on the line (a reason goes in the observed text), for
  these nineteen checks in this order: `dll-hash`, `marker`,
  `counts-tool-before`, `hook`, `control`, `save-control`, `map-at-cube`,
  `save-by-name-control`, `find-control`, `var-pages`, `holders`,
  `lookup-closed`, `take-material`, `save-after-take`, `take-socket`,
  `save-after-socket`, `recipe-shape`, `close-after-takes` and
  `counts-tool-after`.
- **Which control vouches for which read.** Every armed line rests on
  `dll-hash`, `marker` (`phase1i rows=254`) and `control`
  (`CheckPlayerInteraction` non-zero after the load); a row that stayed silent
  also on `hook`'s `0 failed`. `mapkeep find`/`stat` reads rest on `hook`
  (`both-routes` for both keeper hooks) and on `control`'s `a0=0 calls=` being
  non-zero; a `dropped` read counts only on the same kept index. A `find`
  answer - a holder named, or 0 matches after a clear - counts only beside
  `find-control`'s map found on the same root in the same session and its
  no-match search that visited values, and `var-pages` vouches that every
  variable was reachable. Every save after a take is read against
  `save-by-name-control`'s `CreateItemSaveStruct calls=` (and `save-control`'s
  close), an `a0=undefined` entry being the fault site, measured; a by-name
  save's write rests on the T read directly before it and on `save-control`'s
  close moving T0 -> T1, so a no-write beside `save-by-name-control`'s own
  no-write is not-observed rather than fail. `recipe-shape`'s in-route decode
  rests on the gate's control (`calls>0 logged=0` before the press) and on
  `CraftFindRecipeItems #1` logging at the press; with no in-route line it is
  not-observed. `take-socket`'s placement rests on the ruby being seen in the
  bag's Socketable tab at `close-after-takes`. A `fail` or `not-observed` is a
  finding, recorded under `### Phase 1i results`; no live outcome is an
  acceptance criterion.

### Live procedure 1j

Live 1j runs the Phase 1j research build (`### Phase 1j instrument`; its hash
is in `### Phase 1j results`), one launch - a second only for the reload
check, the owner's call at session time - under `live-operator`; the
step-by-step procedure is `### Live procedure 1` in the workorder's context
file, `.claude/workorders/forgepact-issue-14-phase1j-context.md`, which stays
on the owner's machine. It answers four questions in one session: the save
route, the partial take (a stacked and a no-stack case, with a return route
as the no-stack fallback), the Cube's input grid as a destination, and the
count injection. It presses nothing: the craft press is the player build's
test. What this document fixes is its shape:

- **The Phase 1j build, one launch.** The installed plugin's SHA-256 is
  checked first; a different hash stops the session. Character slot 14
  ("Sorak"). Auto-prospect off (one instrument per session). The saves are
  backed up before the launch (an independent copy and the `hs-drive` backup
  labelled `forgepact-issue-14-phase1j-live-1`) and restored by the driver
  after it, verified by hash. `tools/stash_tab_counts.py` reads the stash
  before the launch (Dust `class=14 b=50 stack=13`, Ol `class=15 b=1
  stack=216`, the ruby stack `class=15 b=51 stack=10`, no `class=14 b=51`),
  after the by-name save, after the close and after the launch;
  `stash.hss`'s `LastWriteTimeUtc` is read before and after every save.
- **`mapkeep on` before `craftprobe hook`**, both before the character loads;
  the marker reads `phase1j rows=278` and `hook` `276 detoured, 0 failed, 2
  held by mapkeep`. The control: `CheckPlayerInteraction` and `mapkeep
  stat`'s `a0=0 calls=` non-zero after the load.
- **The healthy close first.** With the save route's rows, `SaveStash` and
  `CreateItemSaveStruct` armed, the owner moves two Greater Unstable Dust (X,
  class 14, `b=51`) from the bag into the Materials tab and closes the stash.
  Every `SaveLocalFile` line the close logs, with its self, `argc`, `a0` and
  `a1`, is the save's shape; the one whose branch ran `SaveStash` is what the
  by-name save replays, near the end of the session.
- **At the Cube** (stash closed, the Greater Unstable Dust recipe open): the
  kept map by name, and `K_X`, the Dust's `K_D` and the ruby stack's `K_S`
  from `mapkeep find`; their cells by `find` on `Controller_obj`; the bag's
  Greater Unstable Dust stack `K_B` and its count `k` from the bag's
  `inventoryMaterialGrid` and the owner's eye.
- **The split control** is a hand split in the bag - one unit off the bag's
  Greater Unstable Dust stack into an empty bag cell - with the item's
  methods, the creation candidates and the drop's rows armed. The same dialog
  and drop serve a stash split (Live 1f/1g logged the same rows for both), and
  the stash stays closed (owner, Live 1f). Its rows, in call order with their
  arguments, name the source edit (a method pair and its key, inline, or
  none), the hash step, and how the new unit's item was made.
- **The stacked partial take** (X, one unit into the bag's stack): the source
  side by the control's route - `callm` the pair with the control's key and
  the hash method if the control ran it, or `set` on
  `fp9:<K_X>.itemDefinitionStruct` `o` when the edit is inline - and the bag
  side by the control's creation replay merged through
  `InventoryGridAddToStack`, or else the same edit on the bag stack's own
  struct. Confirmed when the stash entry reads `o` lowered by one on the same
  key and kept index, the bag stack raised by one, and the stash cell still
  holding the fingerprint.
- **The no-stack partial take** (one Unstable Dust into a bag that holds
  none) only through a replayable creation shape from the control, placed
  with `GetItemPreferredGrid` and `GridAddItem`, plus the source edit;
  otherwise not observed. Its fallback, **the return route**, on the ruby
  stack: the proven whole take, then `ChangeItemOwner(0, 9, <K_S>)`,
  `GridAddItem` back into the stash row the take emptied and `GridRemoveItem`
  on the bag's `inventorySocketGrid` - each read back on both maps, a `find`
  on `Controller_obj` and the bag cell.
- **The Cube as a destination.** The owner drags the split unit into the
  Cube's input grid with the drop's rows armed; a content search on
  `New_Inventory_Data_obj`, `UI_Craft_obj` and the recipe rows' `UI_Grid_obj`
  names the array that holds it, and the lookup with owner 0 and 9 names its
  map. Only if that array is reachable by `path:`: the ruby stack placed into
  it by name (`GridAddItem` with the array swapped, `ChangeItemOwner(9, 0,
  <K_S>)`, `GridRemoveItem` on the stash row), then the owner's eye on the
  grid and a hand move back to the bag. Then one Ol placed in the grid by hand
  (bag 1, grid 1) and the Ol recipe's availability read by eye and from
  `CountInventoryItem`'s logged return: available means the game counts the
  Cube's grid on its own.
- **The count injection** on the Ol -> Old recipe (3 Ol; bag 2, stash 216):
  unavailable without, `inject 15 1 216`, available with it and
  `injected=` above 0 in `show` (quoted whole: the per-frame split,
  `outside-route=` and `other-owner=`), unavailable again after `inject off`,
  a screenshot at each. The recipe is reselected (or the Cube closed and
  reopened on it) after every `inject` and after `inject off`, before the
  display and `show` are read: the counting frames run when the window builds
  or selects a row, so arming over an already-open window runs none of them
  and `injected=0` would measure the procedure. If `show` reports
  `other-owner=` above 0 with `injected=0`, the injection is re-armed once with `owner=<the a0 it
  names>` and the recipe reselected, in the same launch. No press.
- **The by-name save** runs once, after every take's re-read and with the
  stash closed: exactly the close's `SaveLocalFile` shape, by name, with the
  save rows armed; the file's write time before and after, and the counts
  tool against the map's last reads. **The close** runs once and last: the
  owner opens the stash on the Materials and the Socketable tab, says the
  counts and closes it, the game still running. The reload, if the owner
  chooses it, is a graceful stop, a launch and the same counts read again.
- **What would make a trial unsafe, and the mitigation.** A wrong argument to
  a method or to `SaveLocalFile` is a GML error inside `script_execute` that
  may end the launch; a count edit the game's hash check rejects could flag or
  drop an item; `SaveLocalFile` writes the real `stash.hss` and, by its kind,
  possibly more; an item placed in the Cube's grid by name may be lost at the
  next save if the grid does not persist. Mitigation: only shapes the control
  logged, one unit per trial, `callm` and `set` refusing anything but a method
  or a number member, the by-name save once and after every re-read, the
  close once and last, the reload the owner's call, saves backed up and
  restored, and results read from the file rather than assumed. A crash
  during a by-name trial ends the launch: that check's result is the shape
  supplied, and every later check reads `not-observed (launch ended at
  <check>)`.
- **The capture** is `.claude/workorders/forgepact-issue-14-phase1j-live-1.md`,
  ending with a `## Checks` section of one line per check, exactly
  `- <check> | expected: <text> | observed: <text> | pass|fail|not-observed`,
  the verdict token last on the line (a reason goes in the observed text), for
  these twenty-one checks in this order: `dll-hash`, `marker`,
  `counts-tool-before`, `hook`, `control`, `save-control`, `map-at-cube`,
  `holders`, `bag-stack`, `split-control`, `partial-stacked`,
  `partial-nostack`, `return-socket`, `cube-holder`, `cube-place`,
  `cube-count`, `count-inject`, `save-route`, `close-after`, `reload-after`
  and `counts-tool-after`.
- **Which control vouches for which read.** Every armed line rests on
  `dll-hash`, `marker` (`phase1j rows=278`) and `control`; a row that stayed
  silent also on `hook`'s `0 failed`. `mapkeep find` reads rest on `hook`'s
  `both-routes` for the keeper and a `dropped` or lowered read counts only on
  the same kept index. A `find` answer counts only with every variable walked
  and no visit-cap line, as in Live 1i. `split-control` is the control for
  `partial-stacked` and `partial-nostack`: a route it did not show is not
  replayed, and with no row fired the two use `set` or read not-observed.
  `save-route` rests on `save-control`'s close moving the write time and
  logging `SaveLocalFile`; with no such line it is not-observed, never a
  fail. `count-inject` rests on `injected=` above 0 - with 0 the display is
  not evidence and the check reads not-observed, naming `other-owner=` and
  `outside-route=` - and on the same recipe read unavailable before and
  after. It is a `fail` only when the display did not change with
  `injected=` above 0 and both `outside-route=0` and `other-owner=0`: a
  matching count the injection did not reach (`outside-route=` or
  `other-owner=` above 0) may be the one the display reads, so an unchanged
  display beside one reads not-observed, with the counters quoted.
  `cube-count`'s reading counts either way.
  `cube-place` rests on `cube-holder` naming a holder reachable by `path:`.
  A `fail` or `not-observed` is a finding, recorded under `### Phase 1j
  results`; no live outcome is an acceptance criterion.

### Live procedure 1k

Live 1k runs the Phase 1k research build (`### Phase 1k instrument`; its hash
is in `### Phase 1k results`), one launch - a second only for the reload
check, the owner's call at session time - under `live-operator`; the
step-by-step procedure is `### Live procedure 1` in the workorder's context
file, `.claude/workorders/forgepact-issue-14-phase1k-context.md`, which stays
on the owner's machine. It answers one question in parts: whether a by-name
partial take holds - only the shortfall N leaves a stash stack of o, the stash
entry keeps o-N on the same map key and index, the destination (the bag, or
the Crafting Cube's `New_Inventory_Data_obj.craftGrid`) gains exactly N,
cells and maps stay consistent, the game's own hash check accepts the edited
items, and the next save and close hold. It presses nothing: the craft press
is the player build's test. What this document fixes is its shape:

- **The Phase 1k build, one launch.** The installed plugin's SHA-256 is
  checked first; a different hash stops the session. Character slot 14
  ("Sorak"). Auto-prospect off (one instrument per session). The saves are
  backed up before the launch (an independent copy and the `hs-drive` backup
  labelled `forgepact-issue-14-phase1k-live-1`) and restored by the driver
  after it, verified by hash. `tools/stash_tab_counts.py` reads the stash
  before the launch (Unstable Dust `class=14 b=50 stack=13`, Ol `class=15 b=1
  stack=216`, the ruby stack `class=15 b=51 stack=10`, no `class=14 b=51`),
  after the owner's setup move, after the by-name save and after the launch;
  `stash.hss`'s `LastWriteTimeUtc` is read before and after every save.
- **`mapkeep on` before `craftprobe hook`**, both before the character
  loads; the marker reads `phase1k rows=282` and `hook` `280 detoured, 0
  failed, 2 held by mapkeep`. That order is fixed: mapkeep refuses to
  install over a row craftprobe already detours, and every `mapkeep find`
  read rests on it. So `GetItemMap` is never a craftprobe detour in this
  session, and `kept:GetItemMap` (like every `kept:` the takes name) is the
  return of the session's own by-name `call`, which no game call replaces
  (`### Phase 1k instrument`). The control: `CheckPlayerInteraction` and
  `mapkeep stat`'s `a0=0 calls=` non-zero after the load (Live 1j: 21168 and
  26903).
- **The healthy close first**, also the setup: with the save route's rows
  armed, the owner moves two Greater Unstable Dust (X, class 14, `b=51`) from
  the bag into the Materials tab and closes the stash; the `SaveLocalFile`
  line with self `Console_Save_obj`, `a0=4`, `a1=1` and its `SaveStash` is
  the shape the by-name save replays near the end.
- **At the Cube** (stash closed, the Greater Unstable Dust recipe open, left
  open): Live 1j's reads - the kept map by name, `K_X` (`o=2`) and the
  Unstable Dust's `K_D` (`o=13`) from `mapkeep find`, their
  `stashMaterialTab` cells by `find` on `Controller_obj`, and the bag's
  Greater Unstable Dust stack `K_B` with its count `k` and `itemDataHash`,
  with no Unstable Dust (`b=50`) in the bag.
- **The bound call's control** (`bind-control`), read-only: `GetItemDef o` on
  `fp9:<K_X>` unbound (expected to throw, as Live 1j's calls did), then with
  `bind` (expected `ret=real:2`), both replies and the `bind=` line quoted.
  Only that pair, in that order, licenses the bound writes.
- **The stacked partial take** (`partial-stacked`): the bound branch when
  `bind-control` passed, the inline branch (`set` on
  `itemDefinitionStruct.o`, then `ItemCheckHash` by name) otherwise, on both
  the stash entry (2 -> 1) and the bag stack (`k` -> `k+1`). Confirmed when
  `mapkeep find` reads `K_X` at `o=1` with the kept map's `size=` unchanged,
  the lookups read `K_B` at `k+1` and `K_X` at 1 with both hashes changed,
  `find` on `Controller_obj` still names `K_X`'s cell, and the owner reads
  `k+1` in the bag.
- **The hash check** (`hash-accept`): with `ItemCheckHash`, `ReportClient`
  (a Phase 1k row, so its `calls=` is a detour's count) and the two merge
  rows armed, the owner drags the edited bag stack to an empty cell and
  back; `ItemCheckHash` answering true with `ReportClient calls=0` is the
  pass.
- **The no-stack partial take** (`partial-nostack`): one Unstable Dust into
  a bag holding none, by the json branch, then the clone branch only if
  `InitItemFromJson` gives no item, each call's `ret=` and every order tried
  quoted; then the stash entry's own edit (13 -> 12) by the route the stacked
  take proved. Confirmed when the new key's lookup in map 0 reads `o=1`, the
  bag's `node var` listing shows it under `b=50`, `mapkeep find` reads the
  stash entry at 12 with `find` naming the same cell, and the owner reads one
  Unstable Dust in the bag.
- **The Cube as a destination** (`partial-cube`): the branch that succeeded,
  with a new stamp and the grid `path:New_Inventory_Data_obj.craftGrid`, the
  source 12 -> 11; `find` on `New_Inventory_Data_obj` names the new unit's
  `craftGrid` cell, a screenshot and the owner's eye confirm it is drawn,
  and the owner's drag of it onto the bag's Unstable Dust is the game's own
  merge (bag 2), with `ItemCheckHash`'s return and `ReportClient` read.
- **The by-name save** (`save-route`) runs once, after every take's re-read
  and with the stash closed: `SaveLocalFile` by name in the healthy close's
  shape (Live 1j's proven route), the file's write time before and after,
  and the counts tool reading `class=14 b=51 stack=1` and the Unstable Dust
  at 11 or 12. **The close** (`close-after`) runs once and last: the owner
  closes the Cube, opens the stash's Materials tab, says the counts and closes
  it, then the bag's, the game still running and the file written again. The
  reload (`reload-after`), if the owner chooses it, is a graceful stop, a
  launch and the same counts read again.
- **What would make a trial unsafe, and the mitigation** are in
  `### Phase 1k instrument` (the trials, per lead). A crash during a by-name
  trial ends the launch: that check's result is the shape supplied, and every
  later check reads `not-observed (launch ended at <check>)`.
- **The capture** is `.claude/workorders/forgepact-issue-14-phase1k-live-1.md`,
  ending with a `## Checks` section of one line per check, exactly
  `- <check> | expected: <text> | observed: <text> | pass|fail|not-observed`,
  the verdict token last on the line (a reason goes in the observed text), for
  these eighteen checks in this order: `dll-hash`, `marker`,
  `counts-tool-before`, `hook`, `control`, `save-control`, `map-at-cube`,
  `holders`, `bag-stack`, `bind-control`, `partial-stacked`, `hash-accept`,
  `partial-nostack`, `partial-cube`, `save-route`, `close-after`,
  `reload-after` and `counts-tool-after`.
- **Which control vouches for which read.** Every armed line rests on
  `dll-hash`, `marker` (`phase1k rows=282`) and `control`; a row that stayed
  silent also on `hook`'s `0 failed`. `mapkeep find` reads rest on `hook`'s
  `both-routes` for the keeper, and a lowered read counts only on the same
  kept index. A `find` answer counts only with every variable walked and no
  visit-cap line, as in Live 1i. `bind-control` is the control for the bound
  branch: a bound write is tried only after the unbound call threw and the
  bound one answered the count; if the unbound call did not throw, the
  negative control did not reproduce Live 1j, the pair measured nothing, and
  no bound call is evidence either way. `hash-accept` rests on
  `ItemCheckHash`'s own lines appearing for the owner's move - with none, it
  reads not-observed, never a pass. `partial-cube` rests on
  `partial-nostack`'s branch; with no route it reads not-observed.
  `save-route` rests on `save-control`'s close logging the same
  `SaveLocalFile` shape. A `fail` or `not-observed` is a finding, recorded
  under `### Phase 1k results`; no live outcome is an acceptance criterion.

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

### Phase 1g results

Research DLL: `plugin_build\BloodPactPlugin_rel.dll`, built with
`plugin_build\build.bat dev` from ForgePact `24a4441` (SHA-256
`81a033498d63c736a07f758bd23c7d248986372266bee6ae40577421978ebf4a`), the
Phase 1g research build (`### Phase 1g instrument`): the Phase 1e build's 252
rows with the `phase1g` marker, `craftprobe call`'s `undefined` argument and
`within=<row>#<n>` on the craft-route rows. It replaces the first Phase 1g
build (`38ab6ca`), which printed the enclosing row without its call number
and was never installed. `plugin_build\build.bat release` from the
same commit produced a ship DLL with no `craftprobe`, `mapkeep` or `phase1g`
string. The build control is `dll-hash` against this hash plus the
`phase1g rows=252` marker. `### Live procedure 1g` gives the session's shape.

**Live 1g, 2026-09-23.** One launch, character slot 14 ("Sorak"),
`live-operator` on the command channel (`hs-drive`) and the owner at the
keyboard for the Cube, the craft press, the stash's opens and closes, the hand
move of X into the Materials tab and the counts by eye. The saves were backed
up first (an independent copy and the `hs-drive` backup
`20260923T175020Z_forgepact-issue-14-phaseA-live-1`) and restored by the
driver afterwards from that backup (the workorder's Log, `### Live 1`).
`mapkeep on` and `craftprobe hook` ran before the character loaded, and
auto-prospect stayed off. The trial material X was again Greater Unstable Dust
(class 14, `b=51`), moved by hand into the Materials tab as a one-unit item
with its own fingerprint (`0-0-212353257001-14`); the Socketable entry was the
class-15 `b=51` stack of 10, Pristine Ruby by the item editor's catalog (the
owner's choice). Three things the capture records beyond the procedure. The
craft ran once at the recipe's fixed amount: the owner found no amount to
select on the Greater Unstable Dust recipe, so no amount-2 press ran and every
`craft-order` reading is for a one-unit craft. Both by-name `SaveStash` calls,
with the stash closed and with it open, printed `NOT dispatched`. And the game
ended at the owner's last stash close, which came after the by-name take and
both of those calls: the plugin's log stops inside the game's own `SaveStash`
call for that close, after one of its closures returned, with no `ret=` line
for the call, no crash-handler text and no closing line, and the process was
gone when the operator next looked (`hs_status`: not running). `stash.hss`
kept its T1 write time. So `counts-tool-after` was read after a crash, not a
graceful exit. What caused the crash is not established: the capture gives the
order of events (the take, the two by-name `SaveStash` calls, the close's own
save), and the leading unconfirmed lead is the take's after-state, after which
no `SaveStash` was observed to finish; the aborted by-name `SaveStash #1`/`#2`
before it are not excluded either.

Filled from the capture, `.claude/workorders/forgepact-issue-14-phaseA-live-1.md`,
cited by its step headings, one row per check, in the procedure's order. A
`fail` or `not-observed` is a finding; a rejected or refused call shape is
recorded with what was supplied, and a shape not run is "not observed
(<why>)". `ds_map` indices are this session's (1049 the stash map, 951 the map
the craft rows received) and are compared as indices only, never as identity.

| Check | What it measures | Observed | Verdict |
|---|---|---|---|
| dll-hash | The installed plugin's SHA-256, read with the game closed, equals the hash above | `81A03349...EBF4A` from `Get-FileHash` on the installed plugin before the launch, equal to the hash above (capture, Pre-launch) | pass |
| marker | A bare `craftprobe` answers `phase1g rows=252` (this build) | `craftprobe: phase1g rows=252 - research instrument ...` before the character loaded (capture, Step 1) | pass |
| counts-tool-before | `tools/stash_tab_counts.py` before the launch: exit 0, `class=14 b=50 stack=13`, `class=15 b=1 stack=216`, `class=15 b=51 stack=10`, no `class=14 b=51` line; file time T0 | Exit 0: `class=14 b=50 stack=13`, `class=15 b=1 stack=216`, `class=15 b=51 stack=10`; no `class=14 b=51` line. File time T0 `2026-09-22T22:18:18.9022398Z` (capture, Pre-launch) | pass |
| hook | `mapkeep on` prints `GetItemMap` and `LoadStash` `both-routes` (a `TABLE-ONLY` is quoted as the finding), then `craftprobe hook` reads `0 failed` with two rows held by mapkeep | `mapkeep on: GetItemMap both-routes (table swap and inline detour at the function's own address)` and the same for `LoadStash`, no `TABLE-ONLY`; `craftprobe hook: 250 detoured, 0 failed, 2 held by mapkeep.` (capture, Step 1) | pass |
| control | After the load: `CheckPlayerInteraction calls=` and `mapkeep stat`'s `a0=0 calls=` both non-zero; `a0=9 calls=0`, `kept=none`, `first9: none` as the baseline | `CheckPlayerInteraction calls=18648` and `a0=0 calls=25455` after the load, and non-zero at every later read (after the craft's `arm` reset, 0 to 266280); baseline `a0=9 calls=0`, `kept=none`, `first9: none` (capture, Step 1; Step 4, the owner's reply) | pass |
| map-at-cube | With the Cube open and the stash not yet opened: `GetItemMap` by name (self `Console_Save_obj`, argument `9`) dispatched, kept current, and `mapkeep find` reading Ol 216 and Unstable Dust 13, equal to the file | Supplied: self `Console_Save_obj` 0 (live, id 199951), `argc=1`, `a0` the real `9`, with the Cube open and the stash not opened in this launch. `dispatched -> ret=` `ref ds_map 1049`, 1626 entries; `mapkeep stat`: `a0=9 calls=1`, `kept=ref ds_map 1049 size=1626 current=yes`, `refreshed=1`, `first9: #1 self=Console_Save_obj room=Town_01_rm`; `mapkeep find 15 1` `o=216` and `find 14 50` `o=13`, equal to the file (capture, Step 2) | pass |
| recipe-shape | The members of the recipe array's entry and of `UI_Craft_Recipe_List_Item_obj` that name the selected recipe's input type/base and amount, read by name, or the member names listed when none does | `craftprobe recipe` matched 8 of `UI_Craft_obj`'s 104 variables (`invMaterialTab`, `craftButton`, `craftGrid`, `craftSearch`, `craftSearchString`, `m_PopulateCraftRecipeList`, `recipeList`, `tabSelected`). `craftprobe var UI_Craft_Recipe_List_Item_obj 0 *` read the recipe list's first cell (id 262515, 29 variables), which was not the selected row: `selected=false`, `recipeResultItem=undefined`, `craftResultName` empty, `craftRecipeNames` an empty array; of its members, `craftIndex`, `requirementFound` (three bools), `itemIdArrayPos` and `craftItemsAvailable` name no input's type, base or amount, and `UI_Craft_obj`'s dump (80 of 104 printed) named none either. No recipe-array entry was reached: none of the commands run reads the recipe definitions. The craft's rows (step 4) ran with self `UI_Craft_Recipe_List_Item_obj` id 262543, a different cell, which was not read. `craftprobe bag`: no `UI_Inventory_obj` instance with the Cube open (capture, Step 3) | not-observed |
| craft-order | One craft at amount 2: every craft-route row's `within=<row>#<n>` (one row encloses both only when the same `#n` entry and `ret=` lines bracket the consume and the production line), each row's `logged=`/`calls=` from `show all` before and after the craft (a budget spent before the craft is `not observed (budget spent before the craft)`), and `CraftEditPlayerInventory`'s `a0` (owner value or amount) | Supplied: one craft at the recipe's fixed amount - amount 2 was not selectable on this recipe (the owner), so no amount-2 press ran. Read right after `arm budget=8`, all nine rows `calls=0`. After the craft: `CraftFindRecipeItems`, `DoCraftResult`, `CraftEditGrid`, `CraftEditPlayerInventory`, `s_CraftItem`, `GridAddToStack` and `GridAddItem` `calls=1 logged=1`, `s_ItemOperation` `calls=2 logged=2`, `GetInventoryGridNode` `calls=0` (not observed on this craft, the control climbing); no budget spent. `CraftFindRecipeItems #1` and `DoCraftResult #1` read `within=none`, and `CraftFindRecipeItems #1` returned before `DoCraftResult #1` was entered. Between `DoCraftResult #1`'s entry line and its `ret=undefined` line, in this order and each `within=DoCraftResult#1`: `CraftEditGrid #1`, `CraftEditPlayerInventory #1`, `s_CraftItem #1`, `GridAddToStack #1` (`success=false`), `s_ItemOperation #1`, `GridAddItem #1` (`success=true`, the result placed as a new item, `d=14 b=66`) and `s_ItemOperation #2`. `CraftEditPlayerInventory` `a0=real:1.0` (capture, Step 4, the pre-craft read and the owner's reply) | pass |
| lookup-closed | `GetItemFromFingerprint(<X>, 9)` with the stash closed returns X's struct, with self `Console_Save_obj` or a bag grid instance; the self that resolved it | Stash closed, Cube open. Supplied: self `Console_Save_obj` 0 (id 199951), `a0` X's fingerprint, `a1` `9`. `dispatched -> ret=struct{...itemType=real:14.0...}` on the first try, so no bag grid was tried (none existed: `craftprobe bag`, Step 3). `Console_Save_obj` 0 is the self for the take (capture, Step 5; Step 6) | pass |
| take-material | On X (one Greater Unstable Dust in the Materials tab): the add's answer, `ChangeItemOwner`'s `ret=`, and the kept map on the same index (`dropped`, `kept` or `re-kept`); `RemoveItemFromMap` only if kept; the bag by eye | Before: X in the Materials tab after the hand move and the owner's close (T1 `2026-09-23T18:01:01.1362495Z`, later than T0), `mapkeep find 14 51` `o=1 matched=1`, `size=1627`, index 1049 with `refreshed=1`. Supplied, each with self `Console_Save_obj` 0, the stash closed and the Cube open: `InventoryGridCanAddToStack 1 undefined fp9:<X>` -> an item struct with another fingerprint, `itemType=14` (a bag stack to join); `InventoryGridAddToStack 1 fp9:<X>` -> `struct{tabNumber=0, x=7, y=0, tabType=-4, success=true}`; `ChangeItemOwner 9 0 <X>` -> `ret=undefined`. After: `size=1626`, `mapkeep find 14 51` `matched=0`, index 1049 and `refreshed=1` unchanged - `dropped` on the same map, so `RemoveItemFromMap` was not run. Rows that fired: `InventoryGridCanAddToStack`, `InventoryGridAddToStack`, `ChangeItemOwner`, `GetItemPreferredGrid`, `GetInventoryMaxTabs`, `s_ItemOperation` and `s_ItemGridInfo`, one call each. The owner counted 150 Greater Unstable Dust in the bag afterwards; the capture has no count by eye from before the add (capture, Step 5; Step 7, both headings) | pass |
| take-socket | The sequence `take-material` confirmed, on the Socketable entry (the `b=51` stack of 10 or the owner's one-unit socketable): the same reads, and whether the add carried `o` | The class-15 `b=51` stack, `mapkeep find 15 51` `o=10`. Supplied, self `Console_Save_obj` 0: `InventoryGridCanAddToStack 1 undefined fp9:<S>` -> `undefined` (no bag stack to join); then `GetItemPreferredGrid 1 fp9:<S>` -> `struct{gridBits=0, grid=array len=6}`, a numeric array with no instance or container a `path:` argument can reach, so the procedure stopped this item there: `GridAddItem` and `ChangeItemOwner` were not run (not in `craftprobe show`) and nothing moved. The owner saw no Pristine Ruby in the bag, and the stash still showed the 10 at step 11. A move of an entry the bag has no stack for, and whether an add carries a stack's `o`, are not observed (capture, Step 8, both headings) | not-observed |
| take-partial | The argument shapes `s_ItemOperation` and any stack-family row logged at the craft; nothing replayed | Recording only, from the craft's armed log: `s_ItemOperation #1` (`argc=1`, `a0=false`) and `#2` (`argc=5`, `a0=true`, `a1` to `a4` `0`), each with no instance as self and the recipe row as other, both `ret=undefined` and both `within=DoCraftResult#1`; the stack-family row `GridAddToStack #1` (`a0=1`, `a1` an array, `a2` the result item's struct) answered `success=false` before `GridAddItem` placed the result. No decrement shape was replayed (capture, Step 9) | not-observed |
| save-closed | `SaveStash` by name with the stash closed after a confirmed move: T2 -> T3, T3b a few seconds later, and `tools/stash_tab_counts.py` without the moved entry; the window control, the same call with the stash open: T4 -> T5 | After `take-material`'s drop, the stash closed: T2 `2026-09-23T18:01:01.1362495Z`, equal to T1. Supplied: `SaveStash`, self `Console_Save_obj` 0, no argument, under `confirm`. The call printed `NOT dispatched (asset_get_index found no script, or script_execute failed)`, while the same command's output logged a `SaveStash #1` entry (self `Console_Save_obj`) and its closures (`___struct___357@SaveStash` eight times, `___struct___359@SaveStash` once). T3, and T3b about five seconds later, equal T2; `tools/stash_tab_counts.py` still listed `class=14 b=51 stack=1`. The window control, the same call with the stash open: T4 equal T2, `NOT dispatched` again beside a `SaveStash #2` entry and one `___struct___359@SaveStash` closure, T5 equal T4. No write was seen in either state: a window control only, since both sides ran on the same post-take state - it says nothing about the route. Which of the two causes the message names applied - no script found, or `script_execute` failing after the function was entered - is not established from the capture; in Live 1f the same supplied shape printed `dispatched -> ret=undefined` (`save-by-name-closed`) (capture, Step 10; Step 11, save-closed window control) | not-observed |
| stash-window-after | The stash window after the by-name `GetItemMap(9)` and the move: both special tabs drawn, their counts by eye against the map's last reads; `mapkeep stat`'s `a0=9 calls=` and `latest-keep:` | The owner opened the stash: the Materials tab showed Unstable Dust 13 and no X, the Socketable tab the Pristine Ruby stack of 10, equal to the map's last reads (`find 14 51` `matched=0`, `find 15 51` `o=10`); the screenshot of the Materials tab shows the one stack of 13. `mapkeep stat`: `a0=9 calls=58646` (1 after the by-name call, 17434 after the step-5 open: the game's own opens called `GetItemMap(9)` this time), `kept=ref ds_map 1049 size=1626 current=yes refreshed=1`, `latest-keep: #1 self=Console_Save_obj` - the index obtained by name stayed the latest keep. The step-5 open, before the take, also drew and took the hand move (capture, Step 5; Step 11, the owner's confirmation) | pass |
| counts-tool-after | `tools/stash_tab_counts.py` after the graceful exit, against the owner's last counts | No graceful exit: the game ended at the owner's stash close after step 11. The plugin's log stops inside that close's own `SaveStash #3` (self `Console_Save_obj`, `argc=0`) after one `___struct___359@SaveStash` closure returned, with no `ret=` for the call, no crash-handler text and no closing line; `hs_status` then read not running, and `hs_stop_game` was never called. `stash.hss` kept T1's write time. `tools/stash_tab_counts.py`: exit 0, `class=14 b=50 stack=13` and `class=14 b=51 stack=1` on the Materials tab, `class=15 b=51 stack=10` - X still in the file, against the owner's last count (Dust 13, no X). `hs_saves_inspect` against the backup: `herosiege13.hss`, `inventory_order_13.hss`, `shop.ini` and `stash.hss` changed; whether the character's save holds the unit the add put in the bag was not read (capture, Step 11, the owner's reply; Step 12) | fail |

**Question 1: the recipe's readable shape.** Not observed. No member read by
name gave the selected recipe's input type, base or amount: the recipe list's
first cell, which was read, was not the selected row, and among its members
and `UI_Craft_obj`'s the ones that concern a recipe (`craftIndex`,
`requirementFound`, `itemIdArrayPos`, `craftItemsAvailable`, `recipeList`)
carry an index, flags or references, not an input list. No command run reads
the recipe definitions, so the recipe array's entry was not reached
(`recipe-shape`). Two things the craft's log adds, neither decoded: the craft
rows ran with the selected row as self (id 262543, a different cell from the
one read), and `CraftFindRecipeItems` received two arrays and the map the
consume later received (951) and returned `struct{a=31, e=undefined}`. Which
members a count would read stays open.

**Question 2: the craft order.** In the one craft, at the recipe's fixed
amount, `DoCraftResult #1` enclosed both the consume and the result's
production: `CraftEditGrid #1` and `CraftEditPlayerInventory #1`, then
`s_CraftItem #1`, the failed `GridAddToStack #1` and the `GridAddItem #1` that
placed the result (`success=true`), all name `within=DoCraftResult#1`, and
`DoCraftResult #1`'s own entry line and `ret=` line are in the log before and
after all of them - the same-frame rule in `### Phase 1g instrument` is met.
`CraftFindRecipeItems #1` ran and returned before `DoCraftResult #1` began,
so it encloses nothing. No row's budget was spent before the craft (every
row read `calls=0` right after the arm). `CraftEditPlayerInventory`'s `a0` was
`1.0`; with one unit crafted that cannot tell an owner value (1 for the bag)
from the amount, so it stays undecoded, and whether a craft of more than one
unit runs in one `DoCraftResult` call or one per unit is not observed
(`craft-order`).

**Question 3: the closed-window take.** On the Materials tab, one route
dropped the stash entry on the same map with the stash closed and the Cube
open: with self `Console_Save_obj` 0 throughout,
`InventoryGridCanAddToStack(1, undefined, <X's item>)` returned a bag stack's
item struct, `InventoryGridAddToStack(1, <X's item>)` answered
`success=true`, `ChangeItemOwner(9, 0, <X>)` returned `undefined`, and the
kept map then no longer held X, with the index (1049) and `refreshed=1`
unchanged (`take-material`). That is R1 in the order the context gives,
confirmed by the map; R2 (`RemoveItemFromMap`) was not needed and not run.
The stash window afterwards showed no X (`stash-window-after`). On the
Socketable tab the same route stopped at its first step: the bag had no stack
to join (`InventoryGridCanAddToStack` returned `undefined`), and the fallback
`GetItemPreferredGrid` returned a numeric grid code that no `path:` argument
reaches, so neither the add nor the owner change was called and nothing moved
(`take-socket`: not observed). No by-name move of an entry the bag has no
stack for is on record, and whether an add carries a whole stack's `o` is not
observed. No call of either take trial was refused by `craftprobe`, and none
that ran answered with a failure.

**Question 4: the save.** Not proven. The by-name `SaveStash` (self
`Console_Save_obj` 0, no argument), called after the confirmed take with the
stash closed (`SaveStash #1`) and again with it open (`SaveStash #2`),
printed `NOT dispatched` both times, and `stash.hss`'s write time stayed at
T1 through T2, T3, T3b, T4 and T5; the file still listed X (`save-closed`:
not observed). The open-stash call is a window control: it ran on the same
post-take state as the closed-stash call, so both failing controls for the
window, not for the route. The route's own control is Live 1f's by-name
`SaveStash` in the same supplied shape (self `Console_Save_obj` 0, no
argument), run after a by-name `GridRemoveItem` take (`take-1stack`) but
before any by-name add or owner change: it dispatched and returned,
`dispatched -> ret=undefined` (`save-by-name-closed`), and its
`___struct___359@SaveStash` closure logged the Materials tab. So the route
itself does dispatch and return on this runtime; the two `NOT dispatched`
results here do not fault the route's ability to dispatch, but the two
aborted by-name `SaveStash #1`/`#2` calls before the crash are a cause not
yet excluded. The owner's next stash close then ran the
game's own `SaveStash #3`, and the game ended inside it after one
`___struct___359@SaveStash` closure returned, with no `ret=` for the call, no
crash-handler text and no closing line (`counts-tool-after`: fail). All three
`SaveStash` calls after the take were entered, and none of the three logged a
`ret=` for the call itself; `#2` and `#3` each stopped right after one
`___struct___359@SaveStash` closure. The leading lead, unconfirmed, is that
the take's after-state (`InventoryGridAddToStack` plus `ChangeItemOwner`)
leaves the stash in a state `SaveStash` does not finish; it is one occurrence
with no control that isolates it, and its cause is not established. In Live
1e and 1f the stash's saves after a by-name `GridRemoveItem` wrote the file
(`take-trial-grid`, `take-1stack`); this was the first stash close on record
after a by-name add and owner change, and it happened once.

**Question 5: the stash window after a by-name `GetItemMap(9)`.** It drew and
behaved as usual: the stash opened twice after the by-name call (step 5, when
the owner moved X in by hand, and step 11), drew both special tabs with the
counts the map gave, and the game's own opens called `GetItemMap(9)` (17434
`a0=9` calls by the step-5 read), unlike launch B of Live 1f, while the index
obtained by name stayed the latest keep (`stash-window-after`).

### Phase 1h results

Research DLL: `plugin_build\BloodPactPlugin_rel.dll`, built with
`plugin_build\build.bat dev` from ForgePact `2ad168d` (SHA-256
`ee896d9f19a2e98c9bab51b1df155c4079168a45a8ce39e3ad134506c8b99c71`), the
Phase 1h research build (`### Phase 1h instrument`): the Phase 1g build's 252
rows plus `PilipaliDecrypt` and `CreateItemSaveStruct` (254), the `phase1h`
marker, `craftprobe call`'s four-way reply with the row's call number, and
whole-number `var`/`path:` segments on arrays. `plugin_build\build.bat
release` from the same commit produced a ship DLL with no `craftprobe`,
`mapkeep`, `phase1h` or `PilipaliDecrypt` string. The build control is
`dll-hash` against this hash plus the `phase1h rows=254` marker. `### Live
procedure 1h` gives the session's shape.

**Live 1h, 2026-09-23.** One launch, character slot 14 ("Sorak"),
`live-operator` on the command channel (`hs-drive`) and the owner at the
keyboard for the hand move of X into the Materials tab, the Cube, the stash's
open and close and the counts by eye. The saves were backed up first (an
independent copy and the `hs-drive` backup
`20260923T205636Z_forgepact-issue-14-phase1h-live-1`) and restored by the
driver afterwards from that backup, verified by hash (the workorder's Log,
`### Live 1`). `mapkeep on` and `craftprobe hook` ran before the character
loaded. The trial material X was again one Greater Unstable Dust (class 14,
`b=51`), moved by hand into the Materials tab (fingerprint
`0-0-212364006000-14`); the Socketable entry was the class-15 `b=51` stack of
10, Pristine Ruby (`0-0-200921844065-15`). Four things the capture records
beyond the procedure. `craftprobe var Controller_obj 0 *` resolved the
instance (221 variables) but printed only the first 80 - the `var` reader's
cap, with no filter to narrow it - and none of those 80 was the stash map or
an array shaped like a tab, so neither stash container was named (`holders`)
and neither stash-cell clear could run. That is a limit of the instrument's
listing, not a finding that the containers are absent: 141 variables were
never listed. `CreateItemSaveStruct`'s `arm` budget of 12 was spent on the
ordinary tabs' items, which a save writes before the Materials tab, so the
per-item line the static reading predicts for the fault (an entry with
`a0=undefined`) could not print. The game ended at the owner's stash close
after the take, inside its own `SaveStash`, at the same point as Live 1g. And
after that crash the owner relaunched the game and closed it again, outside
the procedure, before the post-session reads - so `counts-tool-after` reads a
file that launch wrote, not this session's end state.

Filled from the capture, `.claude/workorders/forgepact-issue-14-phase1h-live-1.md`,
cited by its step headings, one row per check, in the procedure's order. A
`fail` or `not-observed` is a finding; a refused or failed call shape is
recorded with what was supplied, and a shape not run is "not observed
(<why>)". The capture's `## Checks` block was normalized to the eighteen
check names after the session, with no observation changed (a note under that
block says so): the operator's separate `TABLE-ONLY` line is folded into
`hook`, `take-material (dropped)` reads `take-material`, and
`save-after-socket`, which the operator left out, is `not-observed` because
the launch ended at `close-after-take` (its `Steps 11-12` heading). `ds_map` 1049 is this session's stash map index
and is compared as an index only. A `call` reply's `#<n>` is read against the
row's own entry line: a row `mapkeep` holds or one `hook` did not detour
prints none, so its number has nothing to match.

| Check | What it measures | Observed | Verdict |
|---|---|---|---|
| dll-hash | The installed plugin's SHA-256, read with the game closed, equals the hash above | `EE896D9F...8B99C71` on the installed plugin before the launch, equal to the hash above (capture, the header) | pass |
| marker | A bare `craftprobe` answers `phase1h rows=254` (this build) | `craftprobe: phase1h rows=254 - research instrument for docs/crafting-materials-research.md (research build only)` (capture, Step 1) | pass |
| counts-tool-before | `tools/stash_tab_counts.py` before the launch: exit 0, `class=14 b=50 stack=13`, `class=15 b=1 stack=216`, `class=15 b=51 stack=10`, no `class=14 b=51`; file time T0 | Exit 0, all four as expected; T0 `2026-09-22T22:18:18.9022398Z` (capture, Pre-launch: counts-tool-before, T0) | pass |
| hook | `mapkeep on` prints `GetItemMap` and `LoadStash` `both-routes` (a `TABLE-ONLY` quoted as the finding), then `craftprobe hook` reads `252 detoured, 0 failed, 2 held by mapkeep` | `mapkeep on: GetItemMap both-routes (table swap and inline detour at the function's own address)` and the same for `LoadStash`, no `TABLE-ONLY`; `craftprobe hook: 252 detoured, 0 failed, 2 held by mapkeep.` (capture, Step 1) | pass |
| control | After the load: `CheckPlayerInteraction calls=` and `mapkeep stat`'s `a0=0 calls=` non-zero; `a0=9 calls=0`, `kept=none` as the baseline | `CheckPlayerInteraction calls=18620`, `a0=0 calls=25439`, `a0=9 calls=0`, `kept=none`; the control still non-zero later (`calls=292320`, Step 4) (capture, Step 1) | pass |
| save-control | The owner's hand move of X into the Materials tab and stash close, with `SaveStash`, `CreateItemSaveStruct` and `___struct___359` armed: the healthy save's signature | Supplied: `arm budget=12 SaveStash CreateItemSaveStruct ___struct___359`, then the owner's move and close. `SaveStash #1 self=Console_Save_obj ... argc=0`; `CreateItemSaveStruct #1` to `#12`, each `a0=struct{...}` and `ret=struct{...}`; `___struct___359@SaveStash #1` Dust (`o=13`, `b=50`) and `#2` X (`o=1`, `b=51`); `SaveStash #1 ret=undefined`; T1 `2026-09-23T21:00:10.8049402Z`, later than T0 (capture, Step 2) | pass |
| map-at-cube | With the Cube open and the stash closed: `GetItemMap` by name (self `Console_Save_obj`, argument `9`) dispatched, kept current, and `mapkeep find` naming `K_X` and `K_S` | Supplied: `call GetItemMap Console_Save_obj 0 9 confirm`. `dispatched #1 -> ret=kind=15 str=ref ds_map 1049`; `mapkeep stat`: `kept=ref ds_map 1049 size=1627 current=yes refreshed=1`, `a0=9 calls=3619` (the game's own `GetItemMap(9)` calls since the step-2 stash open, `first9: #1 self=UI_Stash_obj`, so this was not the launch's first); `find 14 51` `key=0-0-212364006000-14 ... o=1 matched=1`; `find 15 51` `key=0-0-200921844065-15 ... o=10 matched=1` (capture, Step 3) | pass |
| holders | `Controller_obj`'s stash map, `<M>` (the Materials two-level array holding `K_X`) and `<S>` (the Socketable array of rows, `K_S` at row `k`), named by shape from `craftprobe var Controller_obj 0 *` | Supplied: `var Controller_obj 0 *`. `Controller_obj id=257721 vars=221`, the reply `...(capped at 80; narrow the filter)`; `var` takes `*` or one exact name (`var Controller_obj 0 stash`: no such variable), and `craftprobe store` does not list `Controller_obj`. None of the 80 listed was a `ref ds_map` 1049 or a tab-shaped array (HUD, quest and view state). Not observed (the listing's 80-variable cap; 141 variables never listed) (capture, Step 5) | not-observed |
| lookup-closed | `GetItemFromFingerprint(<K_X>, 9)` with the stash closed returns X's struct | Supplied: self `Console_Save_obj` 0, `a0` the string `0-0-212364006000-14`, `a1` `9`. `dispatched #3451335 -> ret=struct{itemDataHash=..., itemType=real:14.000000, itemInfoStruct=struct members=20}` (capture, Step 6) | pass |
| take-material | On X, the stacked case, stash closed, Cube open, self `Console_Save_obj` 0: the bag's add, then `RemoveItemFromMap` on map 9, the kept map dropping X on the same index | Supplied, after `arm budget=8`: `InventoryGridCanAddToStack 1 undefined fp9:<K_X>` -> an item struct, `itemType=14`; `InventoryGridAddToStack 1 fp9:<K_X>` -> `struct{tabNumber=0, x=7, y=0, tabType=-4, success=true}`; `RemoveItemFromMap map9 <K_X>` -> `ret=undefined`. After: `find 14 51` `matched=0`, `size=1626` (was 1627), index 1049 unchanged - `dropped` (capture, Step 7) | pass |
| error-baseline | Before X's stash cell is cleared, `SaveStash` by name reproduces Live 1g's fault inside the instrument's catch: `entered #<n>, script_execute threw` (or `returned <status>`), the `SaveStash #<n>` entry, Dust's `___struct___359`, a `CreateItemSaveStruct` line with `a0=undefined`, no `SaveStash #<n> ret=`, no write | Supplied: `arm budget=12 SaveStash CreateItemSaveStruct ___struct___359`, then `call SaveStash Console_Save_obj 0 confirm` (no argument). `SaveStash #1 self=Console_Save_obj ... argc=0` (the row's own entry line, matching the reply's `#1`); `CreateItemSaveStruct #1` to `#12`, every one `a0=struct{...}`; `___struct___357@SaveStash #1` to `#12`; one `___struct___359@SaveStash #1`, Dust (`o=13`, `b=50`), `ret=undefined`; then `entered #1, script_execute threw` and no `SaveStash #1 ret=`. No `___struct___359` entry for X. T3 equal T2 (`21:00:10.8049402Z`): no write; `hs_status` running, same pid. The `a0=undefined` line was not observable: the row's budget of 12 was spent on the ordinary tabs' items first (capture, Step 8). The verdict is the capture's; the procedure's own criterion (the `a0=undefined` line quoted) was not met, so this passes on the throw, no write and the game running only | pass |
| save-after-take | `GridRemoveItem(path:Controller_obj.<M>, <K_X>)`, then the by-name `SaveStash` returns and writes, the file without X | Not run as written: `<M>` was not named (`holders`), so X's stash cell was not cleared. Supplied instead, the same `arm` and `call SaveStash Console_Save_obj 0 confirm` as `error-baseline`, on the same state: the identical lines, ending `entered #1, script_execute threw`, no `SaveStash #1 ret=`; T5 equal T4, no write. Not observed (the cell clear never ran) (capture, Step 9) | not-observed |
| close-after-take | The owner's stash open on the Materials tab and close: the game running after it, the close's `SaveStash #<n> ... ret=` line, the file without X | The owner: "materials shows 13 dust", then the game ended at the stash close. The log's last lines are the close's own `s_SaveStashConstants` pair, `SaveStash #2 self=Console_Save_obj ... argc=0` (the game's call - no `dispatched`/`before`/`after` lines around it), one `___struct___359@SaveStash #2` for Dust (`o=13`, `b=50`) and its `ret=undefined`; then no `SaveStash #2 ret=`, no `___struct___359` for X, no crash text, and the next line is a fresh plugin banner. The launch's pid 89248 was gone; the pid then running, 81172, was the owner's own relaunch (capture, Step 10, Establishing the crash) | fail |
| take-socket | On S, the no-stack case: `GridAddItem` into the bag's `inventorySocketGrid`, `ChangeItemOwner(9, 0, <K_S>)`, the map dropping S, then the stash cell clear on `<S>.<k>` | Not run: the launch ended at `close-after-take`, and `<S>` and `k` were not named (`holders`). Not observed (capture, Steps 11-12) | not-observed |
| save-after-socket | The by-name `SaveStash` after `take-socket` returns and writes | Not run: the launch ended at `close-after-take`. Not observed (capture, Steps 11-12) | not-observed |
| close-after-socket | The owner's stash open on the Socketable tab and close: running, the close's `ret=` line, a later T, the file without `class=15 b=51` | Not run: the launch ended at `close-after-take`. Not observed (capture, Steps 11-12) | not-observed |
| recipe-shape | Part 1, the Cube open on the Greater Unstable Dust recipe: one input's `PilipaliDecrypt` and `CountInventoryItem` arguments and returns, and the row member holding the entry `PilipaliDecrypt` received; part 2, `CraftFindRecipeItems` at the craft press | Supplied: `arm budget=60 PilipaliDecrypt CountInventoryItem CraftFindRecipeItems`, then the owner opened the Cube on that recipe. `show all`: `PilipaliDecrypt calls=532552 logged=60`, `CountInventoryItem calls=490 logged=60`, `CraftFindRecipeItems calls=0`. Each logged pair: `PilipaliDecrypt #<n> self=UI_Craft_obj other=Player_obj argc=3 a0=int64:<m> a1=int64:46 a2=undefined` -> a real, next to `CountInventoryItem #<n> self=UI_Craft_obj other=Player_obj argc=4 a0=1 a1=<class> a2=1 a3=<base>` -> a real. Three pairs name the Dust identity (`a1=14`, `a3=51`) with `CountInventoryItem ret=154`, the owner's bag count by eye; their `PilipaliDecrypt` returns were 100 (`a0=940`), 200 (`968`) and 30 (`98`), and the owner stated the recipe uses 5. `var id:264274 *` (`UI_Craft_obj`): `recipeList` a `UI_Grid_obj` whose `itemGrid` is a `ds_grid` 4x37 of recipe-row instances; the member holding an input entry was not named. Part 2 not run (the launch ended at `close-after-take`). Not observed: the selected recipe's row was not isolated among the list's calls (capture, Step 4; `recipe-shape` (part 2, at step 11's craft)) | not-observed |
| counts-tool-after | `tools/stash_tab_counts.py` after the launch, against the owner's last controlled counts | Exit 0: `class=14 b=50 stack=13`, `class=14 b=51 stack=1`, the Socketable tab as before; T6 `2026-09-23T21:21:18.9618695Z`. That write came after the crash, when the owner relaunched and closed the game outside the procedure, so the file is not this session's end state. `hs_saves_inspect`: `herosiege13.hss`, `inventory_order_13.hss`, `shop.ini` and `stash.hss` changed. Not observed (the owner's relaunch intervened) (capture, Step 13) | not-observed |

**Question 1: the complete take, stacked case (a Materials entry the bag
stacks).** Half observed. The bag side and the map took X by name with the
stash closed, self `Console_Save_obj` 0: `InventoryGridCanAddToStack` found a
bag stack, `InventoryGridAddToStack` answered `success=true`, and
`RemoveItemFromMap` on map 9 dropped X on the same kept index
(`take-material`). The last step, clearing X's cell in `Controller_obj`'s
Materials array with `GridRemoveItem`, did not run: that array's member name
was not among the 80 variables the `var` reader lists (`holders`: not
observed, an instrument limit). So the complete take is not on record, and
neither is a take the stash cell and the map both follow.

**Question 2: the complete take, no-stack case (a Socketable entry).** Not
observed. Nothing of it ran: the launch ended at `close-after-take`, and the
Socketable structure's name and the ruby's row were not named either
(`take-socket`, `save-after-socket`, `close-after-socket`: not observed).
A by-name move of an entry the bag has no stack for, and whether an add
carries a whole stack's `o`, stay not observed since Phase 1g.

**Question 3: the save after the take.** Not observed after a complete take,
because no complete take ran (`save-after-take`: not observed). What was
measured is the save after the take's first half, with X's stash cell left in
place. Called by name, `SaveStash` was entered (its own `SaveStash #1` entry
line), logged the ordinary tabs' entries and the Dust cell, and then
`script_execute` threw before any `___struct___359` entry for X and before the
call's `ret=`; the game kept running and `stash.hss` was not written - twice,
the same lines each time (`error-baseline`, and the `save-after-take`
attempt). The split reply makes that distinguishable from a name that never
resolved, which is what Live 1g's merged `NOT dispatched` could not. The
healthy signature it is read against is `save-control`: the same rows, one
`___struct___359` entry for Dust and one for X, then `SaveStash #1
ret=undefined` and a write (T0 -> T1). The route's own positive control stays
Live 1f's by-name `SaveStash`, which returned (`save-by-name-closed`).

**Question 4: the recipe's readable shape.** Not observed for the selected
recipe; part of the shape was read. With the Cube open on a recipe,
`PilipaliDecrypt` ran with self `UI_Craft_obj`, other `Player_obj` and three
arguments (a whole number that varies, `46`, `undefined`), answering a real
number, and next to each call `CountInventoryItem(1, <class>, 1, <base>)` with
the same self and other answered the bag's count - 154 for the Dust identity,
the owner's count by eye. The three decodes logged next to a Dust count were
100, 200 and 30, not the 5 the owner stated this recipe uses. The calls read
as the Cube's list counting inputs across its recipe rows, not only the
selected one (`PilipaliDecrypt` 532552 calls in the window,
`CountInventoryItem` 490, the list a 4x37 grid of recipe-row instances) - the
capture's reading, not established - and within a budget of 60 the selected recipe's row was not isolated.
That is not a contradiction of the reading's decoder, only a row not found;
which member of a recipe row holds the entry `PilipaliDecrypt` receives was not
named, and `CraftFindRecipeItems` never ran (0 calls; no craft was pressed).

**The crash, as measured.** The static reading (`### Phase 1h instrument`,
"The crash, as that reading explains it") says a `SaveStash` after a take that
leaves the stash cell faults when it looks the vanished entry up in map 9 and
hands the miss to `CreateItemSaveStruct`. Live 1h measured part of that. By
name, after a take that left X's cell, `SaveStash` stopped with an exception
inside `script_execute` after Dust's cell and before X's, and the game
survived it (`error-baseline`). The game's own `SaveStash` at the owner's next
stash close stopped at the same point - after Dust's `___struct___359` entry,
before any for X - and the game ended (`close-after-take`: fail), the same end
as Live 1g's close. Live 1g's take used `ChangeItemOwner` for the map step and
this one `RemoveItemFromMap`; both left the stash cell, and both closes ended
at that point, which fits the reading that the cell rather than the map call
decides it. Not measured: the fault site inside `SaveStash` - the
`CreateItemSaveStruct` entry with `a0=undefined` that would show it could not
print (the row's budget was spent on the ordinary tabs) - so the site is the
static reading's, not a measurement; that the thrown exception is a GML
runtime error rather than another exception; and whether the two by-name
calls that threw before the close contributed, which no control here
separates. A save after a take that also clears the cell - the reading's
prediction that the save then returns and writes - is not observed. Each
close crash is one occurrence per launch, two launches in all.

### Phase 1i results

Research DLL: `plugin_build\BloodPactPlugin_rel.dll`, built with
`plugin_build\build.bat dev` from ForgePact `900e53f` (SHA-256
`eaf6e7b8a423191a27d8307be65a50d7b2eab4270efc300e51d5bb30828a3fed`), the
Phase 1i research build (`### Phase 1i instrument`): Phase 1h's 254 rows
unchanged, the `phase1i` marker, `var <root> * from=<i>` paging with a cap
line naming the next page, the `find` content search, and `within=` on
`PilipaliDecrypt` and `CountInventoryItem` with `arm`'s `inroute` gate.
`plugin_build\build.bat release` from the same commit produced a ship DLL with
no `craftprobe`, `mapkeep`, `phase1i` or `inroute` string. The build control
is `dll-hash` against this hash plus the `phase1i rows=254` marker. `### Live
procedure 1i` gives the session's shape.

**Live 1i, 2026-09-24.** One launch, character slot 14 ("Sorak"),
`live-operator` on the command channel (`hs-drive`) and the owner at the
keyboard for the hand move of X into the Materials tab, the Cube, the one
craft press, the one stash open and close, and the counts by eye. The saves
were backed up first (an independent copy and the `hs-drive` backup
`20260923T223814Z_forgepact-issue-14-phase1i-live-1`) and restored by the
driver afterwards from that backup, verified by hash (the workorder's Log,
`### Live 1`). `mapkeep on` and `craftprobe hook` ran before the character
loaded, and auto-prospect was off. The trial material X was again one Greater
Unstable Dust (class 14, `b=51`), moved by hand into the Materials tab
(fingerprint `0-0-212370161000-14`, a new unit's, not Live 1h's); the
Socketable entry was the class-15 `b=51` stack of 10, Pristine Ruby
(`0-0-200921844065-15`). Every step of the procedure ran, in order, and the
launch ended with a graceful stop. Three things the capture records beyond
the procedure. First, every by-name `SaveStash` returned but none wrote
`stash.hss`: the one before any take (`save-by-name-control`) left the file's
write time unchanged too, so both saves after a take read not-observed rather
than fail, and the file side of each take was read only after the game's own
save at the owner's stash close. Second, the by-name saves'
`CreateItemSaveStruct` counts - 1627 before any take, 1626 after the first,
1625 after the second - each equal the kept map's size at that point, while
the `save-control` window (the owner's stash open, move and close) counted
1993 calls; what the difference is made of is not measured. Third, the
close's own `SaveStash` in `close-after-takes` was counted
(`SaveStash calls=1 (+1 since last show) logged=0 unlogged=1 (not selected)`)
but not traced, because the arm active then (`recipe-shape`'s) did not select
it; that save rests on the counter, the advanced write time and the file's
counts, not on a `SaveStash #<n> ... ret=` line.

Filled from the capture, `.claude/workorders/forgepact-issue-14-phase1i-live-1.md`,
cited by its step headings, one row per check, in the procedure's order, the
verdict the capture's `## Checks` line gives. A `fail` or `not-observed` is a
finding; a call shape is recorded with what was supplied. `ds_map` 1049 is
this session's stash map index and is compared as an index only. `<K_X>` and
`<K_S>` stand for the two fingerprints above. Every `find` quoted walked all
221 of `Controller_obj`'s variables with no visit-cap line, so each 0 is a
complete search beside `find-control`'s.

| Check | What it measures | Observed | Verdict |
|---|---|---|---|
| dll-hash | The installed plugin's SHA-256, read with the game closed, equals the hash above | `EAF6E7B8...28A3FED` on the installed plugin before the launch, equal to the hash above (capture, Before launch: dll-hash) | pass |
| marker | A bare `craftprobe` answers `phase1i rows=254` (this build) | `craftprobe: phase1i rows=254 - research instrument for docs/crafting-materials-research.md (research build only)` (capture, Launch and character load: craftprobe / mapkeep on / craftprobe hook) | pass |
| counts-tool-before | `tools/stash_tab_counts.py` before the launch: exit 0, `class=14 b=50 stack=13`, `class=15 b=1 stack=216`, `class=15 b=51 stack=10`, no `class=14 b=51`; file time T0 | Exit 0, all four as expected; T0 `2026-09-22T22:18:18.9022398Z` (capture, Before launch: counts-tool-before; T0 at Step 2) | pass |
| hook | `mapkeep on` prints `GetItemMap` and `LoadStash` `both-routes`, then `craftprobe hook` reads `252 detoured, 0 failed, 2 held by mapkeep` | `mapkeep on: GetItemMap both-routes (table swap and inline detour at the function's own address)` and the same for `LoadStash`, no `TABLE-ONLY`; `craftprobe hook: 252 detoured, 0 failed, 2 held by mapkeep.`, read from the IPC tail because the immediate reply was cut by size (capture, Launch and character load: craftprobe / mapkeep on / craftprobe hook) | pass |
| control | After the load: `CheckPlayerInteraction calls=` and `mapkeep stat`'s `a0=0 calls=` non-zero; `a0=9 calls=0`, `kept=none` as the baseline | `CheckPlayerInteraction calls=19978`, `a0=0 calls=26219`, `a0=9 calls=0`, `kept=none`; `show` also reads `inroute gate off` (capture, Launch and character load: control (after load)) | pass |
| save-control | The owner's hand move of X into the Materials tab and stash close, with `SaveStash`, `CreateItemSaveStruct` and `___struct___359` armed at `budget=5000`: the healthy save's signature | Supplied: `arm budget=5000 SaveStash CreateItemSaveStruct ___struct___359`, then the owner's move and close ("done, dust moved and stash closed"). `SaveStash calls=1`, `CreateItemSaveStruct calls=1993` (N0); `SaveStash #1 self=Console_Save_obj ... argc=0`; no `a0=undefined` entry; `___struct___359@SaveStash #1` Dust (`o=13`, `b=50`) and `#2` X (`o=1`, `b=51`), each `ret=undefined`; `SaveStash #1 ret=undefined`; T1 `2026-09-23T22:42:48.5539572Z`, later than T0; the counts tool `class=14 b=51 stack=1` (capture, Step 2) | pass |
| map-at-cube | With the Cube open on the Greater Unstable Dust recipe and the stash closed: `GetItemMap` by name (self `Console_Save_obj`, argument `9`) dispatched, kept current, and `mapkeep find` naming `K_X` and `K_S` | Supplied: `call GetItemMap Console_Save_obj 0 9 confirm`. `dispatched #1 -> ret=kind=15 str=ref ds_map 1049`; `mapkeep stat`: `kept=ref ds_map 1049 size=1627 current=yes`, `a0=9 calls=3136`, `first9: #1 self=UI_Stash_obj`; `find 14 51` `key=0-0-212370161000-14 ... o=1`, `matched=1`; `find 15 51` `key=0-0-200921844065-15 ... o=10`, `matched=1` (capture, Step 3) | pass |
| save-by-name-control | A by-name `SaveStash` before any take: it returns, and its `CreateItemSaveStruct calls=` (N1) with no `a0=undefined` entry is the by-name signature later saves are read against; T read before and after | Supplied: the same `arm`, then `call SaveStash Console_Save_obj 0 confirm` (no argument). `dispatched #1 -> ret=undefined`; `SaveStash #1 self=Console_Save_obj ... argc=0` and `SaveStash #1 ret=undefined`; `CreateItemSaveStruct calls=1627` (N1); 0 `a0=undefined` entries across the call's 6518 reply lines. T3, and T3b about 5 s later, equal T2 (`2026-09-23T22:42:48.5539572Z`): no write. The verdict is on the return and the signature; the no-write is what reads the later saves' no-writes as not-observed (capture, Step 4) | pass |
| find-control | `find` on `Controller_obj` for the kept map's runtime text gives at least one match; for a text nothing holds, 0 matches with values visited above 0 | Supplied: `find Controller_obj 0 ref ds_map 1049` -> `match 1: stashInventoryMap (reference)`, `1 match(es), 44966 values visited, 221 of 221 variables walked (from=0)`; `find Controller_obj 0 no-such-text-phase1i` -> `0 match(es), 44966 values visited, 221 of 221 variables walked (from=0)`; no visit-cap line on either (capture, Step 5) | pass |
| var-pages | `var Controller_obj 0 *`, following each cap line, lists as many variables as `vars=`, the map among them | `vars=221`; page 1 indices 0..79 (80), its cap line naming `craftprobe var Controller_obj 0 * from=80` and 141 more; page 2 indices 80..159 (80), naming `from=160` and 61 more, `stashInventoryMap` on it; page 3 indices 160..220 (61), no cap line; 80 + 80 + 61 = 221 (capture, Step 6) | pass |
| holders | `find` for `K_X` gives one match `<M>.<x>.<y>.<member>`, for `K_S` one match `<S>.<k>.0.0.<member>`, and `var` on `<S>.<k>` reads an array | Supplied: `find Controller_obj 0 <K_X>` -> `match 1: stashMaterialTab.0.1.nodeFingerprint (string)`; `find Controller_obj 0 <K_S>` -> `match 1: stashSocketItemSlot.73.0.0.nodeFingerprint (string)`; each `1 match(es)`, 221 of 221 walked; `var Controller_obj 0 stashSocketItemSlot.73` -> `array len=1 len0=1 [0]=array[1]={object/struct}`. `<M>` is `stashMaterialTab`, `<S>` `stashSocketItemSlot`, `k` 73 (capture, Step 7) | pass |
| lookup-closed | `GetItemFromFingerprint(<K_X>, 9)` with the stash closed returns X's struct | Supplied: self `Console_Save_obj` 0, `a0` the string `0-0-212370161000-14`, `a1` `9`. `dispatched #1108800 -> ret=struct{itemDataHash=..., itemType=real:14.000000, itemInfoStruct=struct members=20}` (capture, Step 8) | pass |
| take-material | On X, the stacked case, stash closed, Cube open, self `Console_Save_obj` 0: the bag's add, `RemoveItemFromMap` on map 9 with the kept map dropping X, then `GridRemoveItem` on `<M>` and a `find` for `K_X` giving 0 matches | Supplied, after `arm budget=8`: `InventoryGridCanAddToStack 1 undefined fp9:<K_X>` -> an item struct, `itemType=14`; `InventoryGridAddToStack 1 fp9:<K_X>` -> `struct{tabNumber=0, x=7, y=0, tabType=-4, success=true}`; `RemoveItemFromMap map9 <K_X>` -> `ret=undefined`, then `mapkeep find 14 51` `matched=0`, `size=1626`; `GridRemoveItem path:Controller_obj.stashMaterialTab <K_X>` -> `ret=bool:true`, then `find Controller_obj 0 <K_X>` -> `0 match(es), 44961 values visited, 221 of 221 variables walked` (capture, Step 9) | pass |
| save-after-take | After `take-material`, the by-name `SaveStash` returns with one `CreateItemSaveStruct` call fewer than `save-by-name-control`, no `a0=undefined` entry, and writes a file without X | Supplied: the same `arm budget=5000 ...` and `call SaveStash Console_Save_obj 0 confirm`. `dispatched #1 -> ret=undefined`, `SaveStash #1 ret=undefined`; one `___struct___359@SaveStash` entry, Dust's (`o=13`, `b=50`), none for X; `CreateItemSaveStruct calls=1626` (N1 - 1); 0 `a0=undefined` entries across 6514 reply lines. T5, and T5b about 5 s later, equal T4: no write, so the counts tool still read X from the unchanged file. Not observed (the file side: no by-name `SaveStash` wrote the file this session, `save-by-name-control` included) (capture, Step 10) | not-observed |
| take-socket | On S, the no-stack case: `InventoryGridCanAddToStack` answers `undefined`; `GetItemPreferredGrid` and the bag's `inventorySocketGrid` read; `GridAddItem` into it, `ChangeItemOwner(9, 0, <K_S>)` with the map dropping S, then `GridRemoveItem` on `<S>.<k>` and a `find` giving 0 matches | Supplied, after `arm budget=8`: `InventoryGridCanAddToStack 1 undefined fp9:<K_S>` -> `ret=undefined`; `GetItemPreferredGrid 1 fp9:<K_S>` -> `struct{gridBits=0, grid=array len=6 len0=15}`; `var New_Inventory_Data_obj 0 inventorySocketGrid` -> a 6x15 array of item structs and `undefined`; `GridAddItem path:New_Inventory_Data_obj.inventorySocketGrid fp9:<K_S> 0 undefined` -> `struct{tabNumber=0, x=3, y=1, tabType=0, success=true}`; `ChangeItemOwner 9 0 <K_S>` -> `ret=undefined`, then `mapkeep find 15 51` `matched=0`, `size=1625`; `GridRemoveItem path:Controller_obj.stashSocketItemSlot.73 <K_S>` -> `ret=bool:true`, then `find Controller_obj 0 <K_S>` -> `0 match(es), 44956 values visited, 221 of 221 variables walked`. The placement: the owner counted 10 rubies in the bag at `close-after-takes` (capture, Step 11) | pass |
| save-after-socket | After `take-socket`, the by-name `SaveStash` returns with one call fewer again and writes a file without `class=15 b=51` | Supplied as in `save-after-take`. `dispatched #1 -> ret=undefined`, `SaveStash #1 ret=undefined`; one `___struct___359@SaveStash` entry, Dust's; `CreateItemSaveStruct calls=1625` (N2 - 1); 0 `a0=undefined` entries. T6b, about 5 s after T6, unchanged: no write, the counts tool still reading X and the ruby stack. Not observed (the file side, for the same reason) (capture, Step 12) | not-observed |
| recipe-shape | `arm budget=200 inroute` on `PilipaliDecrypt`, `CountInventoryItem`, `CraftFindRecipeItems` and `DoCraftResult`; `show all` before the press is the gate's control; one craft press; the in-route decode beside the Dust count against the owner's 5 | Supplied: `arm budget=200 inroute PilipaliDecrypt CountInventoryItem CraftFindRecipeItems DoCraftResult` (reply: `inroute gate ON`). Before the press: `PilipaliDecrypt calls=82500 logged=0`. The owner pressed craft once ("crafted once, got a destiny shard fragment, recipe selected again"). After: `CraftFindRecipeItems calls=1 logged=1`, `DoCraftResult calls=1 logged=1`, `PilipaliDecrypt calls=672588 logged=200`, `CountInventoryItem calls=492 logged=200`. `CraftFindRecipeItems #1 within=none self=UI_Craft_Recipe_List_Item_obj#5055@264097 other=UI_Grid_obj#5081@264066 argc=4`; inside it `PilipaliDecrypt #502253 within=CraftFindRecipeItems#1`, the same self and other, `a0=int64:29 a1=int64:46 a2=undefined` -> `ret=real:5.000000`, beside `CountInventoryItem #1 within=CraftFindRecipeItems#1 ... a0=real:1.000000 a1=real:14.000000 a2=int64:1 a3=int64:51` -> `ret=real:155.000000`; `CraftFindRecipeItems #1 ret=struct{a=real:31.000000, e=undefined}`; then `DoCraftResult #1 within=none` with the same self, and 199 decode and count pairs `within=DoCraftResult#1` before the budget ran out. The decode equals the owner's 5 (capture, Step 13) | pass |
| close-after-takes | Only if every clear answered `true`: the owner closes the Cube, opens the stash on the Materials and the Socketable tab and closes it; the game running after it, a later T, the file without X or `class=15 b=51` | Both `GridRemoveItem` calls had answered `bool:true`. The owner: "materials 13 dust, no rubies in stash, bag has 10 rubies. no crash on exit". `hs_status` `game_state=running`, pid 95716 as at the launch; T `2026-09-23T22:59:54.3034104Z`, later than every earlier T; `craftprobe show` `SaveStash calls=1 (+1 since last show) logged=0 unlogged=1 (not selected)`, counted but not traced; the counts tool `class=14 b=50 stack=13` only, no `class=15 b=51` (capture, Step 14) | pass |
| counts-tool-after | `tools/stash_tab_counts.py` after a graceful stop, against the owner's last controlled counts | `hs_stop_game`: `exited=true`, `forced=false`. Exit 0: `material_tab: entries=1`, `class=14 b=50 stack=13`, no `class=15 b=51`, equal to `close-after-takes`' read. `hs_saves_inspect`: `herosiege13.hss`, `inventory_order_13.hss`, `shop.ini` and `stash.hss` changed, none added or missing (capture, Step 15) | pass |

**Question 1: the complete take, stacked case (a Materials entry the bag
stacks).** Observed, once. With the stash closed and the Cube open, self
`Console_Save_obj` 0 throughout, the four by-name calls ran on X in order and
each answered success: `InventoryGridCanAddToStack` found a bag stack,
`InventoryGridAddToStack` answered `success=true`, `RemoveItemFromMap` on map
9 dropped X on the same kept index (1627 entries to 1626), and
`GridRemoveItem` on `Controller_obj.stashMaterialTab` with X's fingerprint
answered `true`, after which a complete content search of `Controller_obj`
no longer found the fingerprint (`take-material`). The cell had been named by
that same search before the take: X's fingerprint sat once in
`Controller_obj`, as the `nodeFingerprint` member of
`stashMaterialTab.0.1` (`holders`). At the owner's stash close afterwards the
Materials tab showed 13 Dust and no X, the game kept running, and the file
the close wrote held no X (`close-after-takes`).

**Question 2: the complete take, no-stack case (a Socketable entry).**
Observed, once. On the ruby stack of 10, `InventoryGridCanAddToStack`
answered `undefined` (no bag stack to add to); `GetItemPreferredGrid`
answered a struct whose `grid` is a 6x15 array, the shape of the bag's
`inventorySocketGrid` (a match on shape only - that the two are one array is
not established); `GridAddItem` into `inventorySocketGrid`, with the default
last two arguments, placed it (`success=true`, cell 3,1);
`ChangeItemOwner(9, 0, <K_S>)` dropped it from the kept map (1626 entries to
1625); and `GridRemoveItem` on `Controller_obj.stashSocketItemSlot.73`
answered `true`, a complete search then no longer finding its fingerprint
(`take-socket`). The row had been named by content as
`stashSocketItemSlot.73.0.0` (`holders`). After the owner's stash close the
bag held 10 rubies and the stash none, by the owner's eye, and the written
file held no `class=15 b=51` (`close-after-takes`) - so the add carried the
whole stack's `o`, which had stayed not observed since Phase 1g.

**Question 3: the save after the take.** The save after each complete take
returned, with no fault; its file side is not observed by name. Each by-name
`SaveStash` after a take returned (`ret=undefined`), made exactly one
`CreateItemSaveStruct` call fewer than the save before it (1627, 1626, 1625)
with no `a0=undefined` entry, and logged a `___struct___359` entry for Dust
and none for the taken item (`save-after-take`, `save-after-socket`, both
read against `save-by-name-control`). Live 1h's by-name save after a take
that left the cell threw after Dust's cell instead; this fits the static
reading that the stash cell, not the map call, decides the fault (`### Phase
1h results`, "The crash, as measured"). The fault site itself stays the
reading's: no save faulted, so no `a0=undefined` line printed. No by-name
`SaveStash` wrote `stash.hss`, though, the one before any take included,
although each used the close's own shape (self `Console_Save_obj`, no
argument, as `save-control` logged it) - as Live 1f's by-name save with the
stash closed did not either. So the file after the takes was read only after
the game's own save at the owner's stash close, which wrote it, without X or
the ruby stack, and did not end the game (`close-after-takes`). What makes
the close's save write while the by-name one does not - `SaveStash` itself or
something the close runs around it - is not measured.

**Question 4: the selected recipe's decoded amount.** Observed, once. With
`arm`'s `inroute` gate on, 82,500 `PilipaliDecrypt` calls ran between the arm
and the read before the press, none logged (the gate's control). At the one
press, `CraftFindRecipeItems` ran once with the recipe-list row as self
(`UI_Craft_Recipe_List_Item_obj`, other `UI_Grid_obj`), and inside it one
`PilipaliDecrypt(29, 46, undefined)` answered 5, beside
`CountInventoryItem(1, 14, 1, 51)` - the Greater Unstable Dust identity -
answering 155; the owner states this recipe uses 5 (`recipe-shape`). So the
selected recipe's required amount is readable by name, in the craft route, as
the decode inside `CraftFindRecipeItems` at the press. The bag count, 155,
is one more than Live 1h's 154, read in that session with X already moved
into the stash and before its take, the saves restored to the same start
between the two: that fits the unit `take-material` put in the bag being
counted by the game, an inference not measured separately. `DoCraftResult`
then ran with the same self, and 199 decode and count pairs inside it were
logged before the 200-line budget ran out; which recipe each belongs to was
not read. The row's own variables were not read either: `var id:264097 *`
after the craft found the row instance gone. A recipe with more than one
input, and a craft of more than one unit, are not observed.

### Phase 1j results

Research DLL: `plugin_build\BloodPactPlugin_rel.dll`, built with
`plugin_build\build.bat dev` from ForgePact `fd61bc6` (SHA-256
`63f41bd74246ec26eb435de3780244c0f8e00822f78d870f7a1f730be6c87850`), the
Phase 1j research build (`### Phase 1j instrument`): Phase 1i's 254 rows plus
the 24 of `### Phase 1j rows`, 278 in all, the `phase1j` marker, `callm`,
`set` and `inject`. It supersedes the first Phase 1j build (`01e33ab`, no
session run on it), whose `inject` did not reach `GetCraftItemsAvailable`'s
count; a session on that build's hash fails `dll-hash`. `plugin_build\build.bat release` from the same commit
produced a ship DLL with no `craftprobe`, `mapkeep`, `phase1j`, `callm` or
`inject` string. The build control is `dll-hash` against this hash plus the
`phase1j rows=278` marker. `### Live procedure 1j` gives the session's shape.

**Live 1j, 2026-09-24.** One launch, character slot 14 ("Sorak"),
live-operator `a123c75575e3e4c35` on the command channel (`hs-drive`), one
launch plus the reload the owner asked for, and the owner at the keyboard for
every hand move, split, drag and stash open/close. Each row is filled from
the capture, `.claude/workorders/forgepact-issue-14-phase1j-live-1.md`, cited
by its step headings, with the verdict its `## Checks` line gives; a call
shape is recorded with what was supplied, and a negative only beside its
control.

| Check | What it measures | Observed | Verdict |
|---|---|---|---|
| dll-hash | The installed plugin's SHA-256, read with the game closed, equals the hash above | Computed SHA-256 of the installed DLL, `63F41BD7...`, equals the hash above case-insensitively (capture, Before launch: dll-hash) | pass |
| marker | A bare `craftprobe` answers `phase1j rows=278` (this build) | `craftprobe: phase1j rows=278 - research instrument for docs/crafting-materials-research.md (research build only)` (capture, Launch and character load: craftprobe / mapkeep on / craftprobe hook) | pass |
| counts-tool-before | `tools/stash_tab_counts.py` before the launch: exit 0, `class=14 b=50 stack=13`, `class=15 b=1 stack=216`, `class=15 b=51 stack=10`, no `class=14 b=51`; file time T0 | Exit 0, all four as expected; T0 `2026-09-22T22:18:18.9022398Z` (capture, Before launch: counts-tool-before) | pass |
| hook | `mapkeep on` prints `GetItemMap` and `LoadStash` `both-routes`, then `craftprobe hook` reads `276 detoured, 0 failed, 2 held by mapkeep` | `mapkeep on: GetItemMap both-routes` and the same for `LoadStash`; `craftprobe hook: 276 detoured, 0 failed, 2 held by mapkeep.`, `GetItemMap` and `LoadStash` each `held by mapkeep` (capture, Launch and character load: craftprobe / mapkeep on / craftprobe hook) | pass |
| control | After the load: `CheckPlayerInteraction calls=` and `mapkeep stat`'s `a0=0 calls=` non-zero; `a0=9 calls=0`, `kept=none` | `CheckPlayerInteraction calls=21168`; `mapkeep stat a0=0 calls=26903`; `a0=9 calls=0`, `kept=none` (capture, Launch and character load: control (after load)) | pass |
| save-control | The owner's move of two X into the Materials tab and stash close, with the save route's rows armed: every `SaveLocalFile` line's self, `argc`, `a0`, `a1`, the one whose branch ran `SaveStash`, T1 later than T0, and the counts tool reading `class=14 b=51 stack=2` | All 7 `SaveLocalFile` calls quoted in order (self, argc, a0, a1); only `SaveLocalFile #7` (self `Console_Save_obj#980@199951`, a0=4, a1=1) ran `SaveStart #7` (stash.hss, saveId=4, ret=true), then `SaveStash #1`, `SaveCommit #4`, `SaveFileGMAsync #4`; T1 `2026-09-24T09:44:51.9771241Z`, later than T0; counts tool `class=14 b=51 stack=2` (capture, Step 2: save-control) | pass |
| map-at-cube | With the Cube open and the stash closed: `GetItemMap` by name dispatched and kept current; `mapkeep find` naming `K_X` (`o=2`), `K_D` (`o=13`) and `K_S` (`o=10`) | `GetItemMap Console_Save_obj 0 9` dispatched `ret=kind=15 str=ref ds_map 1049`; `mapkeep stat` `kept=ref ds_map 1049 size=1627 current=yes`; `mapkeep find` named K_X (`o=2`), K_D (`o=13`), K_S (`o=10`), each `matched=1` (capture, Step 3: map-at-cube) | pass |
| holders | `find` on `Controller_obj` names the cell of `K_X` and `K_D` in `stashMaterialTab` and of `K_S` in `stashSocketItemSlot`, one match each | `find Controller_obj 0 <K_X>` matched `stashMaterialTab.0.1.nodeFingerprint`; `<K_D>` matched `stashMaterialTab.0.0.nodeFingerprint`; `<K_S>` matched `stashSocketItemSlot.73.0.0.nodeFingerprint` (k=73); each one match, all 221 variables walked, no visit-cap line (capture, Step 4: holders) | pass |
| bag-stack | The bag's Greater Unstable Dust stack read by `node var` and `var` on `inventoryMaterialGrid`: its fingerprint `K_B` and count `k`, equal to the owner's count | `node var` summed class=14 b=51 to fingerprint K_B (`0-0-200937046200-14`), `def.o=153`; the owner's eye-count (153, column 7, row 0) matched; `var ... inventoryMaterialGrid.0.7` (row.col order) read the struct with `nodeStartX=7, nodeStartY=0`; column.row order (`.7.0`) went out of range (capture, Step 5: bag-stack) | pass |
| split-control | The owner's hand split of one unit in the bag: every armed row that fired, in call order, with its arguments - the source edit (method and key, inline, or none), the hash step, the creation of the new unit - and the source stack reading `k-1` | Every armed row fired quoted in call order with arguments: `StructCopy`, `s_ItemInstanceStruct`, `SetItemInfo` x22, `GenerateItemHash`, `UiCreateNode` x4, `UiMoveNode` x4, `UiASplitStack` (other=`UI_Split_Stack_obj#5254@266505`), `SetItemDef` (key `o`, value 152), `GenerateItemHash`, then the new unit's creation (`StructCopy`, `s_ItemInstanceStruct` with `o=1`, `LootTimestamp`, `SetItemInfo` x22, `GenerateItemHash`, `s_InvNode` at x=7,y=1); source-edit is the method `SetItemDef` on key `o`, hash step `GenerateItemHash`; the creating rows' self reads `(not an instance: object/struct object_index=undefined)`, so the creation is not replayable by name; K_B re-read `o=152` = k-1 (capture, Step 6: split-control) | pass |
| partial-stacked | One unit of X into the bag's stack by the control's route: the stash entry's `o` 2 -> 1 on the same key and index, the bag stack `k-1` -> `k`, the stash cell kept | Four `callm` calls on self `Console_Save_obj 0`: `fp9:<K_X> SetItemDef o 1`, `fp9:<K_X> GenerateItemHash`, `fp:<K_B> SetItemDef o 153`, `fp:<K_B> GenerateItemHash`, each `entered #n (...), script_execute threw`; K_X unchanged `o=2`, K_B unchanged (itemDataHash unchanged), map size unchanged at 1627, the stash cell kept at the same path, game kept running (capture, Step 7: partial-stacked) | fail |
| partial-nostack | One Unstable Dust into a bag that holds none, by a replayed creation plus the source edit: the stash entry 13 -> 12, a one-unit item in the bag's map and grid; not observed without a replayable creation shape | Step 6 found the creating rows' self unresolvable (`not an instance: object/struct object_index=undefined`) for `s_ItemInstanceStruct`, `SetItemInfo` and `GenerateItemHash`, so no replayable creation shape exists to try; stays not observed (capture, Step 8: partial-nostack) | not-observed |
| return-socket | The ruby stack's proven whole take and its return: `ChangeItemOwner(0, 9)`, `GridAddItem` into the emptied stash row and `GridRemoveItem` on the bag's grid, both maps, the stash cell and the bag cell read back | Take: `InventoryGridCanAddToStack` undefined, `GetItemPreferredGrid` a struct, `GridAddItem` into `inventorySocketGrid` success at x=3,y=1, `ChangeItemOwner(9,0,<K_S>)`, `mapkeep find` matched=0, `GridRemoveItem` on `stashSocketItemSlot.73` true, `find` 0 matches. Return: `ChangeItemOwner(0,9,<K_S>)`, `mapkeep find` `o=10` matched=1, `GetItemFromFingerprint` map0 undefined, `GridAddItem` into `stashSocketItemSlot.73` success, `GridRemoveItem` on `inventorySocketGrid` true, `find` 1 match at `stashSocketItemSlot.73.0.0.nodeFingerprint`, bag cell `inventorySocketGrid.1.3` undefined (capture, Step 9: return-socket) | pass |
| cube-holder | The owner's drag of the split unit into the Cube's input grid: the rows that fired, the holder array by content search, and the map its entry is in | The drag placed the split unit (K_U) via `s_InvNode` at (0,0) on the Cube's own `UI_Inventory_Grid_obj` (`@264102`); content search found `New_Inventory_Data_obj.craftGrid.0.0.nodeFingerprint` (persistent), `id:264102.nodeGrid.0.0.nodeFingerprint` (instance mirror), `UI_Craft_obj.invDrag.itemDragFingerprint` (transient drag reference); `GetItemFromFingerprint` map0 gave a struct, map9 undefined; `var New_Inventory_Data_obj 0 craftGrid` an array len=6, len0=9, cell [0][0] holding the item (capture, Step 10: cube-holder) | pass |
| cube-place | The ruby stack placed into the Cube's grid by name (only if the holder is reachable by `path:`): placed, dropped from map 9, the stash cell cleared, drawn in the grid by the owner's eye | `GridAddItem` into `path:New_Inventory_Data_obj.craftGrid` for the ruby stack (K_S) answered success at x=0,y=1; `ChangeItemOwner(9,0,<K_S>)`, `mapkeep find` matched=0, `GridRemoveItem` on `stashSocketItemSlot.73` true, `find` 0 matches; a screenshot showed the Dust icon plus a stack of 10 rubies at the expected cell, and the owner confirmed and moved the rubies to the bag by hand (capture, Step 11: cube-place) | pass |
| cube-count | One Ol placed in the Cube's grid by hand (bag 1, grid 1): whether the Ol recipe reads available, and `CountInventoryItem`'s logged return | The bag actually held 1 Ol, not the procedure's assumed 2: before the move bag=1 grid=0, after bag=0 grid=1, both unavailable (needs 3), so the reading cannot discriminate bag-only from bag+grid counting; only one `CountInventoryItem(a0=1,a1=15,a2=1,a3=1)` line was logged (`ret=1.0`), ascribed to the pre-move scan, with 390 of 490 calls unlogged once the row's budget was spent by the game's own recipe-list rescans; stays not observed (capture, Step 12: cube-count) | not-observed |
| count-inject | The Ol recipe unavailable without, available with `inject 15 1 216` and `injected=` above 0 (per frame: availability, recipe row, craft route; with `outside-route=` and `other-owner=` quoted), unavailable again after `inject off`; no press | With bag=1 Ol, the recipe read unavailable; `craftprobe inject 15 1 216` on, reselecting showed available; `show` read `injected=2 (availability=1 recipe-row=1 craft-route=0) outside-route=0 other-owner=0`; `inject off`, reselecting showed unavailable again; no craft press (capture, Step 13: count-inject) | pass |
| save-route | `SaveLocalFile` by name with `save-control`'s shape, stash closed: the file's write time moves and the counts tool reads the map's last reads | `craftprobe call SaveLocalFile Console_Save_obj 0 4 1 confirm`, the exact shape `save-control` logged, ran `SaveStart #1` (stash.hss, saveId=4, ret=true), `SaveStash #1`, `SaveCommit #1`; T3 `2026-09-24T10:24:20.6018779Z` later than T2 (unchanged since T1); the counts tool read this session's actual last state (`class=14 b=50 stack=13`, `class=14 b=51 stack=2`, no stash `class=15 b=51` since K_S was moved to the bag by hand at step 11) (capture, Step 15: save-route) | pass |
| close-after | The owner's stash open on both tabs and close after every trial: the counts by eye, the game running, a later write time, the counts tool unchanged from `save-route` | The owner reported stash Materials Dust 2 and 13, stash Socketable no ruby stack, bag Socketable 10 rubies; a first screenshot still showed the stash open (file unchanged from T3), and after the owner's follow-up close a second screenshot showed the town view; T4 `2026-09-24T10:35:32.4450633Z` later than T3; `hs_status` running; counts tool identical to `save-route`'s read (capture, Step 16: close-after) | pass |
| reload-after | After a graceful stop and a new launch (the owner's call): the same counts by eye | After a graceful `hs_stop_game` and a fresh `hs_launch`/`hs_wait_ready`/`hs_select_character(14)`, the owner reported: stash Dust 2, bag Dust 153 (K_B's 152 plus K_U's 1, merged back into one stack after the split unit's return from the Cube at step 14); stash Unstable Dust 13; no ruby stack in the stash, 10 in the bag; stash closed - matching the pre-reload state exactly (capture, Step 17: reload-after) | pass |
| counts-tool-after | `tools/stash_tab_counts.py` after a graceful stop, equal to `close-after`'s read | After a final graceful stop, `tools/stash_tab_counts.py` read `class=14 b=50 stack=13`, `class=14 b=51 stack=2`, no `class=15 b=51`, identical to `close-after`'s read; `hs_saves_inspect` showed `herosiege13.hss`, `inventory_order_13.hss`, `shop.ini` and `stash.hss` changed, none added or missing (capture, Step 18: stop, counts-tool-after, hs_saves_inspect) | pass |

**Question 1: the save route.** Observed, twice. The owner's hand move of two
Greater Unstable Dust into the Materials tab and stash close ran seven
`SaveLocalFile` calls; only the seventh (self `Console_Save_obj#980@199951`,
`a0=4`, `a1=1`) ran `SaveStart` (`stash.hss`, saveId 4, `ret=bool:true`), then
`SaveStash`, `SaveCommit`, `SaveFileGMAsync`, moving `stash.hss`'s write time
and matching the counts tool's read (`save-control`). The identical shape,
replayed by name (`craftprobe call SaveLocalFile Console_Save_obj 0 4 1
confirm`) with the stash closed, ran the same chain and moved the file's
write time again, with the counts tool reading the map's last state for this
session (`save-route`). This is the reading that `SaveStash` alone only
serialises and `SaveLocalFile` commits; the 1627-versus-1993
`CreateItemSaveStruct` gap from Phase 1i stays unsolved - Live 1j did not
measure it.

**Question 2: the split mechanism.** Observed, once. The hand split ran
`UiASplitStack` (self `UI_Button_Small_obj`, other `UI_Split_Stack_obj`),
then `SetItemDef` (self not an instance, other `UI_Split_Stack_obj`, key
`"o"`, value 152 = k-1) and `GenerateItemHash` on the source struct, then a
new one-unit struct (`s_ItemInstanceStruct`, `LootTimestamp` for the new
fingerprint, 22 `SetItemInfo` calls, `GenerateItemHash`, `s_InvNode` at grid
x=7,y=1). Every creating row's self reads `(not an instance: object/struct
object_index=undefined)`, so the creation is not replayable through `callm`,
which supplies an instance self; a struct self was not tried
(`split-control`).

**Question 3: the stacked partial take.** Observed, once, and it failed
safely. With self `Console_Save_obj 0`, both `SetItemDef`/`GenerateItemHash`
on the stash entry (`fp9:<K_X>`) and on the bag stack's own struct
(`fp:<K_B>`) each returned `entered #n (...), script_execute threw`; the
stash entry stayed `o=2`, the bag stack's hash was unchanged, the map size
stayed 1627, and the stash cell was kept at the same path - no state
changed, and the game did not crash (`partial-stacked`). The rejected shape
is recorded next to what the control showed: the game's own split runs
these same methods with a self that reads `not an instance` (inferred, not
probed, to be the item's own struct) and other `UI_Split_Stack_obj`, a self
`callm` cannot supply; the inline `set` route on
`itemDefinitionStruct.o` was never tried, because the control showed a
method, not an inline write.

**Question 4: the no-stack partial take.** Not observed: Question 2 found
the split's creating rows unresolvable through `callm`, which supplies an
instance self, and a struct self was not tried, so no creation shape was
available to try a one-unit item into a bag that holds none of that base
(`partial-nostack`).

**Question 5: the return.** Observed, once, both directions. The take moved
the ruby stack from the stash's Socketable row into the bag's
`inventorySocketGrid` (`GridAddItem`, success x=3,y=1), `ChangeItemOwner(9,
0)`, then cleared the stash cell (`GridRemoveItem`, `find` 0 matches). The
return moved it back: `ChangeItemOwner(0, 9)` restored the map entry on the
same key and index, `GridAddItem` placed it back into
`Controller_obj.stashSocketItemSlot.73`, and `GridRemoveItem` cleared the bag
cell; a `find` afterwards matched the same path as before the take
(`return-socket`).

**Question 6: the Cube.** Observed, twice. A hand drop into the Cube placed
the split Dust unit at a Cube-specific `UI_Inventory_Grid_obj`; content
search found the persistent holder `New_Inventory_Data_obj.craftGrid` (an
array of 6 length-9 arrays), a mirror at that grid instance's own
`nodeGrid`, and a transient `UI_Craft_obj.invDrag` reference - the item
stayed in map 0 throughout (`cube-holder`). By name, `GridAddItem` into
`path:New_Inventory_Data_obj.craftGrid` placed the ruby stack
(`success=true`), dropped it from map 9, cleared the stash cell, and a
screenshot (confirmed by the owner) showed it drawn in the grid
(`cube-place`). Whether the game's own counting reads that grid stays not
observed: the bag/grid setup (1 Ol bag / 0 grid, then 0 / 1) never reached
the recipe's requirement of 3 either way, so "unavailable" in both states
cannot tell bag-only counting from bag-plus-grid counting, and the one
logged `CountInventoryItem(a0=1,a1=15,a3=1)` reading is ascribed to the
pre-move scan (`cube-count`).

**Question 7: the injection.** Observed, once. With the bag's actual 1 Ol,
the recipe read unavailable; `craftprobe inject 15 1 216` on, it read
available; `inject off`, it read unavailable again. `show` read `injected=2
(availability=1 recipe-row=1 craft-route=0)`, `outside-route=0`,
`other-owner=0` - the injection reached exactly its intended routes, with no
craft press (`count-inject`).

**Question 8: close and reload.** Observed, once each, with one deviation
the capture records: the first post-close screenshot still showed the stash
open (the file unchanged from the save-route write); once the owner closed
it in response to a follow-up, a second screenshot showed the town view and
the file's write time moved past the save-route write (`close-after`).
After a graceful stop and a fresh launch, the owner's counts matched the
pre-reload state exactly, including the bag's Greater Unstable Dust reading
153 - the bag stack's 152 plus the split unit's 1, merged back into one
stack after the unit's return from the Cube (`reload-after`).

**Unverified going in, after Live 1j.** Of `### Phase 1j instrument`'s list:
the stash's kind is 4, measured (the close's own `SaveLocalFile` call read
`a0=4`, `save-control`). Which get/set pair and hash method the split calls
is measured for the source edit: `SetItemDef` on key `"o"` (the definition
struct), then `GenerateItemHash` (`split-control`); the info struct's own
method is not implicated, since only `SetItemDef`/`GenerateItemHash` ran on
the source. That a bag-to-bag drop runs the same creation code as a
stash-to-bag drop stays not observed - Live 1j split inside the bag only,
same as Live 1f/1g. That this runtime answers `is_method` stays not
observed directly: every `callm` call this session dispatched and reached
`script_execute` (throwing only inside the game's own method), which needs
the member to resolve as callable one way or another, but whether that
resolution went through `is_method` or the `typeof` fallback is not
distinguished by the capture. That `GridAddItem` places into
`stashSocketItemSlot` and into the Cube's grid, and `GridRemoveItem` clears
`inventorySocketGrid`, is now measured: the return leg of `return-socket`
placed by name into `Controller_obj.stashSocketItemSlot.73`, and
`cube-place` placed by name into `New_Inventory_Data_obj.craftGrid`;
`return-socket`'s take leg cleared `inventorySocketGrid.1.3` by name. The
axis order of `inventoryMaterialGrid` is measured: row-major,
`[row][column]` (`.0.7` held the bag's stack at column 7 row 0; `.7.0` was
out of range) (`bag-stack`). Whether the Cube's grid is a profile array is
measured as reachable by `path:` the same way every other profile array
here is (`cube-holder`); whether `CountInventoryItem` walks it stays not
observed (`cube-count`). Whether the Cube's grid persists across a save
stays not observed - the grid was emptied by hand before the save route ran.
Which of the three counting frames the window's display reads stays not
observed, since `inject` covers all three and cannot tell them apart; the
owner `a0` `GetCraftItemsAvailable` passes to `CountInventoryItem` is now
measured as 1, matching the craft route's own value (the one logged
reading's self was `UI_Craft_obj`, `GetCraftItemsAvailable`'s own instance,
not the recipe row's) (`cube-count`); the recipe row's closure's own `a0`
stays not observed.

### Phase 1k results

Research DLL: `plugin_build\BloodPactPlugin_rel.dll`, built with
`plugin_build\build.bat dev` from ForgePact `9a1de0c` (SHA-256
`38717043cbaf78484e2c05e5eda3d74c2f4667618dfc8dfe5b4ad3b309cacf9b`), the
Phase 1k research build (`### Phase 1k instrument`): Phase 1j's 278 rows plus
the four of `### Phase 1k rows`, 282 in all, the `phase1k` marker, `callm`'s
`bind`, `set`'s `kept:` form and `call` keeping its own return, emptied when
a dispatch does not return. It replaces two Phase 1k builds, neither ever
installed: the first (`d6a5582`, 281 rows), whose `kept:` could not be filled
for `GetItemMap`, and the second (`0d58d5c`), whose `kept:` survived a `call`
that threw (`### Phase 1k instrument`, the build's review rounds).
`plugin_build\build.bat release` from the same commit produced a ship DLL with
no `craftprobe`, `mapkeep`, `phase1k`, `callm` or `kept:` string. The build
control is `dll-hash` against this hash plus the `phase1k rows=282` marker;
the Phase 1j build installed until now, and both earlier Phase 1k builds,
fail `dll-hash`. `### Live procedure 1k` gives
the session's shape.

**Live 1k, 2026-09-24.** One character (slot 14, "Sorak"), live-operator
`ae806ceef4c39ecb4` on the command channel (`hs-drive`). The owner said
"start again" before step 2 was confirmed done; the driver restored the
saves and the procedure re-ran in full from step 1. The reload the owner
asked for at step 13 ran after that. Each row is filled from the capture,
`.claude/workorders/forgepact-issue-14-phase1k-live-1.md`, cited by its step
headings, with the verdict its `## Checks` line gives; a call shape is
recorded with what was supplied, and a negative only beside its control.

| Check | What it measures | Observed | Verdict |
|---|---|---|---|
| dll-hash | The installed plugin's SHA-256, read with the game closed, equals the hash above | Recomputed SHA-256 of the installed DLL, `38717043CBAF78484E2C05E5EDA3D74C2F4667618DFC8DFE5B4AD3B309CACF9B`, equals the hash above case-insensitively, both before the first launch and again after the owner-requested restart (capture, Before launch: dll-hash; Re-verification before relaunch) | pass |
| marker | A bare `craftprobe` answers `phase1k rows=282` (this build) | `craftprobe: phase1k rows=282 - research instrument for docs/crafting-materials-research.md (research build only)`, confirmed on both the first attempt and the final attempt after the restart (capture, Launch and character load: craftprobe / mapkeep on / craftprobe hook, attempt 2) | pass |
| counts-tool-before | `tools/stash_tab_counts.py` before the launch: exit 0, `class=14 b=50 stack=13`, `class=15 b=1 stack=216`, `class=15 b=51 stack=10`, no `class=14 b=51`; file time T0 | Exit 0, all four as expected; T0 `2026-09-22T22:18:18.9022398Z`, re-confirmed identical after the restore (capture, Before launch: counts-tool-before; Re-verification before relaunch) | pass |
| hook | `mapkeep on` prints `GetItemMap` and `LoadStash` `both-routes`, then `craftprobe hook` reads `280 detoured, 0 failed, 2 held by mapkeep` | `mapkeep on` read `GetItemMap both-routes` and `LoadStash both-routes`; `craftprobe hook: 280 detoured, 0 failed, 2 held by mapkeep.` (capture, attempt 2: craftprobe / mapkeep on / craftprobe hook) | pass |
| control | After the load: `CheckPlayerInteraction calls=` and `mapkeep stat`'s `a0=0 calls=` non-zero | `CheckPlayerInteraction calls=19432`; `mapkeep stat a0=0 calls=25909`; `a0=9 calls=0`, `kept=none` (capture, attempt 2: control) | pass |
| save-control | The owner's move of two X into the Materials tab and stash close, with the save route's rows armed: the `SaveLocalFile` line with self `Console_Save_obj`, `a0=4`, `a1=1` and its `SaveStash`, T1 later than T0, and the counts tool reading `class=14 b=51 stack=2` | All 7 `SaveLocalFile` calls traced in order; only `SaveLocalFile #7` (self `Console_Save_obj#980@199951`, `a0=4`, `a1=1`) ran `SaveStash #1`; T1 `2026-09-24T14:35:44.4021723Z` later than T0; counts tool read `class=14 b=51 stack=2` (capture, Step 2: save-control, attempt 2) | pass |
| map-at-cube | With the Cube open and the stash closed: `GetItemMap` by name dispatched and kept current; `mapkeep find` naming `K_X` (`o=2`) and `K_D` (`o=13`) | `GetItemMap Console_Save_obj 0 9` dispatched by name, kept as `kept:GetItemMap #1`, `ret=ref ds_map 1050` with 1627 entries; `mapkeep stat` read `current=yes`; `mapkeep find` named K_X (`o=2`) and K_D (`o=13`), each `matched=1` (capture, Step 3: map-at-cube) | pass |
| holders | `find` on `Controller_obj` names the `stashMaterialTab` cell of `K_X` and of `K_D`, one match each | `find Controller_obj 0 <K_X>` matched `stashMaterialTab.0.1.nodeFingerprint`; `<K_D>` matched `stashMaterialTab.0.0.nodeFingerprint`; each 1 match, all 221 variables walked, no visit-cap line (capture, Step 4: holders) | pass |
| bag-stack | The bag's Greater Unstable Dust stack `K_B`: its count `k` equal to the owner's, its `itemDataHash`, and no Unstable Dust (`b=50`) in the bag | `node var` summed K_B (`0-0-200937046200-14`) to `def.o=153`, no `class=14 b=50` line anywhere in the bag; the owner's eye-count (153, column 7, row 0) matched; `var ... inventoryMaterialGrid.0.7` read `nodeStartX=7, nodeStartY=0` (capture, Step 5: bag-stack) | pass |
| bind-control | `callm ... fp9:<K_X> GetItemDef o confirm` throws (the negative control), then `... GetItemDef o bind confirm` answers `ret=real:2`, with the `bind=` line's `method_get_self` reading | Unbound `callm Console_Save_obj 0 fp9:<K_X> GetItemDef o confirm` entered `script_execute` and threw (the negative control reproduced Live 1j). Bound (`bind`), the reply showed `bind=yes` and `method_get_self` reading `before=undefined` `after=struct members=11` (the rebind was applied), but the one `script_execute` also threw and did not answer the entry's count. Recorded as a rejected shape - `script_execute` of the bound value with an instance self, self=other=`Console_Save_obj#980@199951`, one argument `"o"` - not a finding that the method route is impossible; no other way to dispatch a bound method was tried (capture, Step 6: bind-control) | fail |
| partial-stacked | One unit of X into the bag stack by the bound branch (or the inline one): `K_X` `o` 2 -> 1 on the same key, index and cell with the map's size unchanged, `K_B` `k` -> `k+1`, both hashes changed, the owner's count `k+1` | Bind-control failed, so the inline branch ran: `set fp9:<K_X>.itemDefinitionStruct o 1` (`before=2 after=1`), `call ItemCheckHash Console_Save_obj 0 fp9:<K_X>` answered `bool:false`; `set fp:<K_B>.itemDefinitionStruct o 154` (`before=153 after=154`), `call ItemCheckHash Console_Save_obj 0 fp:<K_B>` also answered `bool:false`. K_X stayed on the same key and cell (`stashMaterialTab.0.1.nodeFingerprint`), the map size stayed 1627, and the owner confirmed the bag count 154. K_B's `itemDataHash` changed from step 5's earlier read; K_X's hash was read only after the edit, so a K_X hash change is supplied by the reading, not itself observed against an earlier value (capture, Step 7: partial-stacked) | pass |
| hash-accept | The owner's drag of the edited bag stack away and back: `ItemCheckHash` answers true and `ReportClient` (a detoured row) reads `calls=0` | During the owner's drag of the edited bag stack away and back, `ItemCheckHash`, `ReportClient`, `GridAddToStack` and `InventoryGridAddToStack` all logged 0 calls, while `CheckPlayerInteraction` and background inventory rows climbed, confirming the instrument was live and the drag produced game activity. Per the procedure, the check rests on `ItemCheckHash`'s own line appearing; with none, this reads not observed, never a pass. `ReportClient` never fired at all this session, so it has no positive control - its 0 is not evidence that no flag was raised (capture, Step 8: hash-accept) | not-observed |
| partial-nostack | One Unstable Dust into a bag that holds none, by the json branch (or the clone branch): the new key's lookup in map 0 reads `o=1`, the bag's grid holds it, `K_D` 13 -> 12 on the same cell; every order tried | The json branch succeeded on the first order tried at every stage, self `Console_Save_obj 0` throughout: `CreateItemSaveStruct` kept (struct members=6); `set kept:CreateItemSaveStruct o 1`; `LootTimestamp` returned `<S>`=212428116002; `InitItemFromJson` on the kept struct and the new key `0-0-<S>-14` returned an item with `b=50 o=1`; `GetItemMap(0)` returned map 0 (`ds_map 1056`); `AddItemToMap` on that map, key and item answered `undefined` with no throw; the new key's lookup in map 0 read `o=1`; `GetItemPreferredGrid` then `GridAddItem` into `New_Inventory_Data_obj.inventoryMaterialGrid` answered `success=true` at x=7, y=1. `K_D` was then lowered 13 -> 12 by the same inline route as `partial-stacked`. The owner confirmed 1 unit at column 7, row 1. The clone branch (`StructCopy`) was not run (capture, Step 9: partial-nostack) | pass |
| partial-cube | A second created unit placed into `New_Inventory_Data_obj.craftGrid`, `K_D` 12 -> 11: drawn in the Cube, and merged by the owner's drag onto the bag's Unstable Dust (bag 2) with `ItemCheckHash` true and no `ReportClient` | The same json route made a second unit (K_N2), then `GridAddItem` placed it into `New_Inventory_Data_obj.craftGrid` at [0][0] (`success=true`), and `find` named `craftGrid.0.0.nodeFingerprint`. `K_D` went 12 -> 11; no `ChangeItemOwner` was needed, since the unit was made directly in map 0. A screenshot showed the unit drawn, and the owner confirmed the drag merged it into the bag's Unstable Dust, now 2. `InventorySwapItemsNew`, `InventorySocketItem` and `RemoveItemFromMap` each logged one call in that window, recorded as logged without interpreting them; `ItemCheckHash` and `ReportClient` logged zero, the same gap as `hash-accept` (capture, Step 10: partial-cube) | pass |
| save-route | `SaveLocalFile` by name with `save-control`'s shape, stash closed: the file's write time moves and the counts tool reads `class=14 b=51 stack=1` and the Unstable Dust at 11 or 12 | `craftprobe call SaveLocalFile Console_Save_obj 0 4 1 confirm` ran `SaveStart` (`stash.hss`), `SaveStash #1`, `SaveCommit #1`; T3 `2026-09-24T14:54:45.2649220Z` later than T2; counts tool read `class=14 b=51 stack=1`, `class=14 b=50 stack=11` (capture, Step 11: save-route) | pass |
| close-after | The owner's Cube close and stash open and close: the counts by eye (Greater Unstable Dust 1; Unstable Dust 11 or 12; the bag's `k+1` and 2 or 1), the game running, a later write time, the counts tool unchanged from `save-route` | The owner reported stash Greater Unstable Dust 1, Unstable Dust 11; bag Greater Unstable Dust 154, Unstable Dust 2, matching expected exactly; a screenshot confirmed the town view; T4 `2026-09-24T14:56:33.5930943Z` later than T3; counts tool identical to `save-route`'s read (capture, Step 12: close-after) | pass |
| reload-after | After a graceful stop and a new launch (the owner's call): the same counts by eye | After a graceful `hs_stop_game` and a fresh `hs_launch`/`hs_wait_ready`/`hs_select_character(14)` (the owner's call: "run the reload now"), the owner reported the same counts again, stash 1 and 11, bag 154 and 2, exactly matching the pre-reload state (capture, Step 13: reload-after) | pass |
| counts-tool-after | `tools/stash_tab_counts.py` after a graceful stop, equal to `close-after`'s read | After a final graceful stop, the counts tool read `class=14 b=50 stack=11`, `class=14 b=51 stack=1`, identical to `close-after`'s read (capture, Step 14: stop, counts-tool-after) | pass |

**Question 1: the bound call.** Observed, once, negative. `callm ... GetItemDef
o confirm` unbound entered `script_execute` and threw, reproducing Live 1j's
negative control. Bound (`bind`), `bind=yes` and `method_get_self`
(`before=undefined`, `after=struct members=11`) confirmed the rebind was
applied, but the one `script_execute` also threw without answering the
expected `ret=real:2`. This is a rejected shape - `script_execute` of
the bound value with an instance self, self=other=`Console_Save_obj#980@199951`,
one argument `"o"` - not a finding that the method route is impossible; no
other way to dispatch a bound method was tried (`bind-control`).

**Question 2: the stacked partial take.** Observed, once, on the inline
branch (bind-control's failure sent it there, per the procedure's own
fallback). `set fp9:<K_X>.itemDefinitionStruct o 1` and
`set fp:<K_B>.itemDefinitionStruct o 154` each read back cleanly; `call
ItemCheckHash` on each side answered `bool:false`. K_X stayed on the same
key, index and cell (`stashMaterialTab.0.1.nodeFingerprint`) with the map's
size unchanged at 1627; K_B's count read 154, the owner's own count. K_B's
`itemDataHash` changed from step 5's earlier read; K_X's hash was read only
after the edit, so a K_X hash change is supplied, not itself observed against
an earlier reading. Why `ItemCheckHash` answered `false` on an item that then
carries a new hash is an inference (R), not itself probed: the reading is
that `ItemCheckHash` re-runs the item's own hash method for comparison and
`GenerateItemHash` is what stores the new digest, consistent with a stale-
versus-new mismatch, but that mechanism was not measured directly.
`ItemCheckHash`'s own counter numbered the two by-name calls `#1` and `#2` in
one arm window, so its detour saw a call routed through `script_execute` - a
both-routes control for this row's detour only, not evidence a direct
compiled call would be seen the same way (`partial-stacked`).

**Question 3: the hash acceptance.** Not observed, twice. During the owner's
drag of the edited bag stack away and back, none of the four armed rows
(`ItemCheckHash`, `ReportClient`, `GridAddToStack`, `InventoryGridAddToStack`)
logged a call, while `CheckPlayerInteraction` and background inventory rows
climbed, confirming the instrument was live. The same four rows stayed at
zero again during the Cube merge of Question 5. Per the procedure, the check
rests on `ItemCheckHash`'s own line appearing; with none, this reads not
observed, never a pass. What was seen alongside: the edited K_B stack and
both created units survived the drag, the merge, the save and a reload.
`ReportClient` never fired at all this session, so it has no positive
control - its zero is not evidence that no flag was raised; `GridAddToStack`
and `InventoryGridAddToStack` fired on no merge this session either, so
their zero here has no positive control and is not evidence the merge
avoids them - and whether the game hash-checks an edited item on any of
these paths stays not observed (`hash-accept`, `partial-cube`).

**Question 4: the no-stack partial take.** Observed, once, on the json
branch, first order at every stage. Self `Console_Save_obj 0` throughout:
`CreateItemSaveStruct` kept a struct (members=6); `set
kept:CreateItemSaveStruct o 1`; `LootTimestamp` returned `<S>`=212428116002;
`InitItemFromJson` on the kept struct and the new key `0-0-<S>-14` returned an
item with `b=50 o=1`; `GetItemMap(0)` returned map 0 (`ds_map 1056`);
`AddItemToMap` on that map, key and item answered `undefined` with no throw;
the new key's lookup in map 0 read `o=1`; `GetItemPreferredGrid` then
`GridAddItem` into `New_Inventory_Data_obj.inventoryMaterialGrid` answered
`success=true` at x=7, y=1 - the cell directly under K_B. `K_D` was then
lowered 13 -> 12 by the same inline route as Question 2. The owner confirmed
1 unit at column 7, row 1. The clone branch (`StructCopy`) was not run, so
its nested-struct sharing and its clone's stamp member stay not observed
(`partial-nostack`).

**Question 5: the Cube placement and the merge.** Observed, once. The same
json route made a second unit (K_N2) by the same calls, then `GridAddItem`
placed it into `New_Inventory_Data_obj.craftGrid` at [0][0] (`success=true`),
and `find` named `craftGrid.0.0.nodeFingerprint`. `K_D` went 12 -> 11. No
`ChangeItemOwner` was needed, since the unit was made directly in map 0. A
screenshot showed the unit drawn, and the owner confirmed the drag merged it
into the bag's Unstable Dust, now 2. In that drag window
`InventorySwapItemsNew`, `InventorySocketItem` and `RemoveItemFromMap` each
logged one call, recorded as logged without interpreting them; the four
merge/hash rows (`ItemCheckHash` included) logged zero, the same gap as
Question 3. `GridAddToStack` and `InventoryGridAddToStack` fired on no merge
this session, so their zero here has no positive control either and is not
evidence the merge avoids them (`partial-cube`).

**Question 6: the save, the close and the reload.** Observed, twice. The
by-name `SaveLocalFile Console_Save_obj 0 4 1` ran `SaveStart` (`stash.hss`),
`SaveStash`, `SaveCommit`, moved `stash.hss`'s write time past T2, and the
counts tool read `class=14 b=51 stack=1`, `class=14 b=50 stack=11`
(`save-route`). The owner's Cube close and stash open-and-close read stash 1
and 11, bag 154 and 2, exactly as expected, with a later write time and the
counts tool unchanged (`close-after`). After a graceful stop and the owner's
requested reload, `hs_select_character` succeeded and the owner reported the
same counts again, exactly matching the pre-reload state, and the final
`counts-tool-after` read was identical to `close-after`'s (`reload-after`,
`counts-tool-after`).

**Unverified going in, after Live 1k.** Of `### Phase 1k instrument`'s list:
that `method` re-binds the item's methods on this runtime is now measured as
applied (`method_get_self` reading `before=undefined` `after=struct
members=11`), but the one bound `script_execute` still threw, so whether a
bound call can complete stays not observed. `AddItemToMap`'s argument order
(map, key, item) is now measured as accepted with no throw. That
`InitItemFromJson` accepts `CreateItemSaveStruct`'s struct with `o` edited and
a `0-0-<S>-14` key and returns an item is now measured, on the first order
tried both times it was run. Whether `StructCopy`'s clone shares its nested
structs with its source, and the name of the clone's stamp member, stay not
observed - the clone branch was never run. That an edited stack passes the
game's own `ItemCheckHash` when the owner moves it, and whether `ReportClient`
fires, stay not observed - `ItemCheckHash`'s own line never appeared for the
drag, and `ReportClient` never fired all session. That the game's own merge
accepts a unit this toolkit created is now measured as true for the
Cube-placed unit, merged into the bag's existing stack by the owner's drag
(Question 5); the bag-placed unit (Question 4) was added directly to the
grid by `GridAddItem`, not merged, so a bag-side merge of a created unit
stays not observed.

## Decision gate

decision: H-A

The owner set this on 2026-09-23 ("H-A, research consume first"), choosing
H-A and asking that Phase 1e, the consume-research round, come before any
player build. During Live 1f the owner also set the design an H-A build is to
follow, below.

**After Phase 1k (2026-09-24): three route tokens, confirmed by the owner; H-A
stays the decision.** Live 1k measured the by-name partial take left open by
the round before it (RD `### Phase 1k results`):

- `partial-stacked: inline` - bind-control's one shape (`script_execute` of a
  `method`-bound value, self=other=`Console_Save_obj#980@199951`, one
  argument `"o"`) threw and did not answer the count, so `partial-stacked`
  passed on the inline branch instead: `set fp9:<K_X>.itemDefinitionStruct o
  1` then `call ItemCheckHash Console_Save_obj 0 fp9:<K_X>`, and the same
  shape on the bag stack (`K_B`, `o` 153 -> 154). `bound` is not proposed,
  because the bind-control shape failed.
- `partial-nostack: json` - the loader route (`CreateItemSaveStruct` ->
  `InitItemFromJson` -> `GetItemMap(0)` -> `AddItemToMap` ->
  `GetItemPreferredGrid` -> `GridAddItem`) passed on the first order tried at
  every stage; the `StructCopy` clone branch was never run.
- `partial-cube: proven` - the same json route placed a second unit into
  `New_Inventory_Data_obj.craftGrid`, and the owner's drag merged it into the
  bag's existing Unstable Dust stack, now 2.

Each of these rests on an edit or a creation whose acceptance by the game's
own hash check was not observed (`hash-accept`): during the owner's drag of
the edited K_B stack and again during the Cube merge, `ItemCheckHash` and
`ReportClient` both logged zero calls, while the edited and created items
survived the drag, the merge, the save and a reload (`### Phase 1k results`,
Question 3).
The owner confirmed the three tokens on 2026-09-24 ("Confirm all three"),
so `partial-stacked: inline`, `partial-nostack: json` and `partial-cube:
proven` are set, and chose to build on the inline route while that hash
acceptance is unobserved ("Build on it"): Phase C's `no-flag` and
`reload-after` checks watch for a flagged item on the player build
(`## Phase C live procedure`). The player build is `## Ship design`.

**After Phase 1j (2026-09-24): the record proposes five route tokens; H-A
stays the decision.** Live 1j measured a by-name save route (`SaveLocalFile`,
self `Console_Save_obj`, `a0=4`, `a1=1`, stash closed, writes `stash.hss`), a
rejected partial-stack take, a proven whole-item take-and-return the owner
has since ruled out as the build's route, a count injection that reaches the
game's own availability display, and a whole-item placement into the
Crafting Cube's own grid (RD `### Phase 1j results`):

- `save-route: proven` - the close's own `SaveLocalFile` shape (self
  `Console_Save_obj`, `a0=4`, `a1=1`) replayed by name with the stash closed
  ran `SaveStart`, `SaveStash`, `SaveCommit`, moved `stash.hss`'s write time,
  and the counts tool read the map's last state (`save-control`,
  `save-route`).
- `partial-stacked: none` - the only route the split control showed,
  `SetItemDef`/`GenerateItemHash` on the item's own struct, was tried by
  name with self `Console_Save_obj` on both the stash entry and the bag
  stack; all four calls entered `script_execute` and threw, safely, with no
  state change. The game's own split instead ran these methods with a self
  that reads `not an instance` (inferred, not probed, to be the item's own
  struct) and other `UI_Split_Stack_obj` (`split-control`), a self `callm`
  cannot supply; the inline `set` route was never run because
  the control showed a method. This is a rejected shape, not a finding that
  the take is impossible (`partial-stacked`).
- `partial-nostack: none` - no creation shape was available to try
  (the split's creating rows' self reads `not an instance`), and the owner
  has rejected this record's first draft (`partial-nostack: return`, the
  proven whole-item take-and-return, `return-socket`) as the build's route
  for this case ("partial take is what we want"). The build is blocked on a
  partial take, and the next research round (Phase 1k) tries what Live 1j
  did not: the item's own methods (`SetItemDef`/`GenerateItemHash`) with a
  struct self and other `UI_Split_Stack_obj`, the shape the split control
  actually showed, and the untried inline `set` route on
  `itemDefinitionStruct.o`.
- `inject: proven` - scoped to the availability display: `craftprobe inject
  15 1 216` moved the Ol recipe from unavailable to available and back
  after `inject off`, with `injected=2 (availability=1 recipe-row=1
  craft-route=0)`, `outside-route=0`, `other-owner=0` (`count-inject`); no
  craft was pressed, so the injection's effect inside the craft route was
  not observed. *The duplication constraint* (`### Constraints from Phase
  1`) still holds: the count comes from the map only, never from the
  cube's `a`.
- `cube-destination: proven` - a stash stack was placed into the Cube's own
  grid (`path:New_Inventory_Data_obj.craftGrid`) by name, `success=true`,
  dropped from map 9, the stash cell cleared, and drawn in the grid
  (`cube-place`). The game's own counting of that grid remains not observed
  (`cube-count`): the bag/grid setup available (1/0 to 0/1) never approached
  the recipe's requirement of 3 either way, so no reading discriminated
  bag-only from bag-plus-grid counting. The owner's reason for standing on
  `proven` regardless: only the craft's own items are placed into the grid,
  at the press, and consumed at once, so the grid's persistence across a
  save is not needed by the build.

These five tokens are proposed, not set: the owner confirms them (this
workorder's `## Needs human judgement` 1), and the Cube's grid (3) is still
the owner's decision before a build plan is written. The partial-take
choice (2) is resolved by the owner's 2026-09-24 decision above; what
remains is Phase 1k proving a route for it.

**After Phase 1i (2026-09-24): H-A stays the decision. The complete by-name
take, with its stash-cell clear, is now measured for one Materials entry the
bag stacks and one Socketable stack it does not, and the owner's stash close
after both kept the game running and wrote a stash file holding neither; the
selected recipe's required amount is readable by name at the craft press.
What the design's save step rests on is now the game's own save: a by-name
`SaveStash` (self `Console_Save_obj` 0, no argument, stash closed, Cube open)
returned cleanly but wrote no file in this one launch, making 1627
`CreateItemSaveStruct` calls; a separate save-control window, the owner's
stash open, hand move and close before any by-name call, counted 1993
`CreateItemSaveStruct` calls - the two windows differ in scope, so that gap
is the lead to follow before this route is treated as closed - so the stash
file is written at the game's next own save - measured here at the next
stash close.** Whether and how a player build follows is the owner's
decision (the next workorder). This replaces the "After Phase 1h" list of
what the design lacked, kept below as it stood then. By measurement in Live
1i - one launch, the representative cases only - with `control`, the keeper's
control and `find-control` non-zero (`### Phase 1i results`), for a later
player build:

- *Where the stash's cells live.* `Controller_obj` holds the stash map as
  `stashInventoryMap` and the two special tabs' cells as `stashMaterialTab`
  (a two-level `[x][y]` array; X was at `0.1`) and `stashSocketItemSlot` (an
  array of rows; the ruby stack at row 73, `.73.0.0`); a cell holds its
  item's fingerprint in its `nodeFingerprint` member (`holders`,
  `find-control`, `var-pages`). These names were found live, by content: the
  Ghidra reading had no member names from the executable to name them by.
  They are this build's runtime names, read in one launch.
- *The complete take, stacked case.* Self `Console_Save_obj` 0, stash closed,
  Cube open: `InventoryGridCanAddToStack(1, undefined, <item>)` (a struct),
  `InventoryGridAddToStack(1, <item>)` (`success=true`), `RemoveItemFromMap`
  on map 9, then `GridRemoveItem(Controller_obj.stashMaterialTab,
  <fingerprint>)` (`true`); the kept map and a complete content search both
  then lacked the entry (`take-material`). The item is the game's own
  `GetItemFromFingerprint(<fingerprint>, 9)` return with that self
  (`lookup-closed`).
- *The complete take, no-stack case.* The same self:
  `InventoryGridCanAddToStack` `undefined`, then `GridAddItem(<the bag's
  inventorySocketGrid>, <item>, 0, undefined)` (`success=true`, the whole
  stack of 10 arriving in the bag), `ChangeItemOwner(9, 0, <fingerprint>)`,
  and `GridRemoveItem(Controller_obj.stashSocketItemSlot.<row>,
  <fingerprint>)` (`true`) (`take-socket`, and the owner's count at
  `close-after-takes`). That `inventorySocketGrid` is the grid
  `GetItemPreferredGrid` answers is a match on shape only.
- *A save after a complete take did not fault (Live 1i).* Each by-name `SaveStash`
  after a take returned, one `CreateItemSaveStruct` call fewer each time and
  no `a0=undefined` entry (`save-after-take`, `save-after-socket`, read
  against `save-by-name-control`), and the owner's stash close after both
  takes kept the game running and wrote a file with neither item
  (`close-after-takes`) - where Live 1g's and Live 1h's closes, after a take
  that left the cell, ended the game. One close, one launch.
- *The selected recipe's amount, by name.* At the press, the
  `PilipaliDecrypt` call inside `CraftFindRecipeItems` (self the selected
  recipe row) answered 5 beside `CountInventoryItem` for the Dust, and the
  owner's figure is 5 (`recipe-shape`). Outside the craft route the Cube's
  list decodes all the time (82,500 calls before the press, none in route), so
  the amount is read in route, not from the list.
- *The hook point, unchanged: `DoCraftResult`* ran once at the press, after
  `CraftFindRecipeItems` and with the same self (`recipe-shape`), as in Phase
  1g's `craft-order`.

What it still lacks, as far as observed:

- *A by-name stash save that writes.* The lead to follow before this route is
  treated as closed: a by-name save made 1627 `CreateItemSaveStruct` calls; a
  separate save-control window, the owner's stash open, hand move and close
  before any by-name call, counted 1993 - a different window in scope, so
  what that gap consists of is not measured, and may be the open and the
  move rather than the save. No by-name `SaveStash` wrote
  `stash.hss` in this session, the one before any take included, though each
  used the close's own shape (self `Console_Save_obj`, no argument); Live 1f
  saw the same with the stash closed. Only the game's own save at the stash
  close wrote the file. For a player build this means the stash side of a
  take reaches the file at the game's next own save - measured here as the
  next stash close - and what the character's save and the stash file hold if
  the game ends between a take and that save is not observed, nor what at the
  close does the writing. How a build handles that window is not decided
  here.
- *The fault site inside `SaveStash`* stays the static reading's: no save
  faulted, so no `a0=undefined` line printed.
- *A take of part of a stack* - the design's "move the shortfall" - is not
  observed: X was a one-unit entry and the ruby stack was taken whole.
- *More than one input, more than one unit.* One press of a one-input
  recipe was read; the 199 decode and count pairs logged inside
  `DoCraftResult` were not tied to a recipe, and a craft of more than one
  unit is not observed.
- *`CraftEditPlayerInventory`'s `a0`* stays undecoded (Phase 1g).
- *The duplication constraint* (`### Constraints from Phase 1`) still holds:
  the count comes from the map only, never from the cube's `a`.

**After Phase 1h (2026-09-23): H-A stays the decision, and a player build is
still blocked on the stash save after a by-name move and on the recipe's
readable shape. Phase 1h measured where the save stops after a take that
leaves the stash cell, but the complete take - the stash-cell clear that the
Ghidra reading says the save needs - did not run, because the research
instrument could not list the `Controller_obj` variable that holds the cell;
the next step is another research build, not a player build** (the next
workorder is the owner's decision). This replaces the "After Phase 1g" list
of what the design still lacked, kept below as it stood then. By measurement
in Live 1h, with `control` and the keeper's own control non-zero
(`### Phase 1h results`), for a later player build:

- *The take's bag side and map side, for a Materials entry the bag already
  stacks.* With the stash closed and the Cube open, self `Console_Save_obj` 0
  throughout: `InventoryGridCanAddToStack(1, undefined, <item>)` (a struct),
  `InventoryGridAddToStack(1, <item>)` (`success=true`), then
  `RemoveItemFromMap` on map 9, the kept map dropping the entry on the same
  index (`take-material`); the item is the game's own
  `GetItemFromFingerprint(<fingerprint>, 9)` return with that self
  (`lookup-closed`). Phase 1g confirmed the same bag side with
  `ChangeItemOwner` as the map step.
- *Where a save after that half-take stops.* By name, `SaveStash` was entered,
  logged Dust's Materials cell, then threw inside `script_execute` before X's
  cell, with no write and the game running (`error-baseline`, twice); the
  game's own `SaveStash` at the next stash close stopped at the same point and
  ended the game (`close-after-take`: fail), as in Live 1g. **The bag-side
  and map-side moves without the stash-cell clear are not safe to ship: both
  times they were measured, the next stash close ended the game** - once
  after each map step (`ChangeItemOwner` in Live 1g, `RemoveItemFromMap`
  here), one launch each. The fault site inside `SaveStash` is the static
  reading's, not measured (`### Phase 1h results`, "The crash, as measured").
- *A healthy save's per-item signature* to read later saves against: every
  `CreateItemSaveStruct` entry a struct, one `___struct___359` entry per
  Materials item, then the call's `ret=` and a write (`save-control`).
- *The recipe's counting calls, by name.* At the Cube, `PilipaliDecrypt`
  (self `UI_Craft_obj`, three arguments) answers a decoded amount, and
  `CountInventoryItem(1, <class>, 1, <base>)` beside it answers the bag's
  count, 154 for the Dust matching the owner's count by eye (`recipe-shape`,
  part 1). Which decoded amount belongs to the selected recipe was not
  isolated.
- *The hook point, unchanged from Phase 1g: `DoCraftResult`* enclosed both the
  consume and the result's production in the one one-unit craft measured
  (Phase 1g `craft-order`). Phase 1h pressed no craft, so it neither adds to
  nor weakens that; a craft of more than one unit is not observed.

What it still lacks, as far as observed:

- *The stash-cell clear, and so the complete take and a save after it.*
  `Controller_obj` resolved with 221 variables, but `craftprobe var ... *`
  lists 80 and takes no filter, so the Materials and Socketable containers
  were not named (`holders`: not observed - the listing's limit, not a
  finding that they are absent), `GridRemoveItem` on the stash cell never ran,
  and no by-name save after a complete take is on record (`save-after-take`:
  not observed). The design's save step rests on it.
- *The no-stack case* (a Socketable entry): nothing of it ran (`take-socket`,
  `save-after-socket`, `close-after-socket`: not observed), and whether an add
  carries a whole stack's `o` stays not observed.
- *The recipe's inputs for the selected recipe.* The three decodes logged next
  to a Dust count were 100, 200 and 30, not the 5 the owner stated: the calls
  read as the Cube's list counting inputs across its recipe rows, and within
  a budget of 60 the selected row was not isolated - not a contradiction of
  the decoder. The member of a recipe
  row that holds the entry was not named, and `CraftFindRecipeItems` did not
  run (`recipe-shape`: not observed). The design's count step rests on it.
- *The end state after the crash*: `counts-tool-after` read a file written by
  the owner's own relaunch after the crash (not observed), and whether the
  character's save held the unit the add put in the bag was not read.
- *`CraftEditPlayerInventory`'s `a0`* stays undecoded (Phase 1g).
- *The duplication constraint* (`### Constraints from Phase 1`) still holds:
  the count comes from the map only, never from the cube's `a`.

What the next research build needs, from this session's gaps: a
`craftprobe var` listing that reaches past 80 variables (paged, or filtered by
name or by the kind of value), so `holders` can name the containers - and a
cap message that names that route, since today's "narrow the filter" points at
a filter `var` does not take; and a way to tie a `PilipaliDecrypt` call to the
selected recipe row (the row as self or argument, or a read of the selected
row's entries), so the decoded amount can be compared with the recipe's stated
one. The missing `a0=undefined` line needs no build change: `arm` takes a
budget of up to 5000 at run time, so the next procedure arms
`CreateItemSaveStruct` with a budget above the ordinary tabs' item count plus
the Materials tab's, and reads `craftprobe show`'s `CreateItemSaveStruct
calls=` before and after each save. Phase 1h's procedure can then run again on
the same two cases.

**After Phase 1g (2026-09-23): H-A stays the decision, and the owner's
design now has a hook point, a lookup self and a by-name move with the stash
closed that the kept map confirms, for a Materials entry the bag already
stacks. It has no stash save after such a move on record - the one stash close
that followed it ended the game inside the game's own save - and no recipe
members to count from, so a player build is blocked on the save and on the
recipe's shape** (the next workorder is the owner's decision). This replaces
the "After Phase 1f" list of what the design still lacked, kept below as it
stood then. By measurement in Live 1g, with `control` and the keeper's own
control non-zero (`### Phase 1g results`), for a later player build:

- *The hook point: `DoCraftResult`.* In the one craft, at the recipe's fixed
  amount (amount 2 was not selectable), the consume (`CraftEditGrid`,
  `CraftEditPlayerInventory`) and the result's production (`s_CraftItem`,
  `GridAddToStack`, and the `GridAddItem` that placed the result) all ran
  inside the same `DoCraftResult` call, whose own entry and `ret=` lines
  bracket them (`craft-order`). So a row does enclose both, and a hook body on
  `DoCraftResult` that does not call the game's function would skip the
  consume and the placement together - measured on one one-unit craft;
  whether a craft of more than one unit runs one `DoCraftResult` call or one
  per unit is not observed.
- *The take route, R1, for a Materials entry the bag already has a stack of.*
  With the stash closed and the Cube open, self `Console_Save_obj` 0
  throughout: `InventoryGridCanAddToStack(1, undefined, <item>)` (a struct:
  a bag stack exists), `InventoryGridAddToStack(1, <item>)` (`success=true`),
  then `ChangeItemOwner(9, 0, <fingerprint>)`; the kept map then dropped the
  entry on the same index and refresh count (`take-material`). The item is
  the game's own `GetItemFromFingerprint(<fingerprint>, 9)` return.
  `RemoveItemFromMap` was not needed. For an entry the bag has no stack for
  (the Socketable stack of 10), the route stopped at its first step and
  nothing moved (`take-socket`: not observed). Every `SaveStash` call after
  this take stopped without a `ret=` for the call, and the next stash close
  crashed the game inside its own `SaveStash`; R1 is not ruled out as the
  crash's cause and is not yet safe to ship.
- *The lookup self: `Console_Save_obj` 0.* It resolved
  `GetItemFromFingerprint(<X>, 9)` with the stash closed on the first try; no
  bag grid instance existed with the Cube open (`lookup-closed`,
  `recipe-shape`'s `craftprobe bag`).
- *The stash window after the by-name `GetItemMap(9)`* drew both special tabs
  with the map's counts, and the game's own opens called `GetItemMap(9)` while
  the index obtained by name stayed the latest keep (`stash-window-after`).

What it still lacks, as far as observed:

- *A stash save after a by-name move.* The save is not proven. The by-name
  `SaveStash` (`SaveStash #1`, stash closed) printed `NOT dispatched`, and so
  did the same call with the stash open (`SaveStash #2`); the file's write
  time did not move in either state (`save-closed`: not observed). The open
  call is a window control only - it ran on the same post-take state as the
  closed call, not a different route - the route's own control is Live 1f's
  by-name `SaveStash` in the same supplied shape, run after a by-name
  `GridRemoveItem` take (`take-1stack`) but before any by-name add or owner
  change, which printed `dispatched -> ret=undefined`. Neither `SaveStash #1`
  nor `SaveStash #2` logged a `ret=` for the call itself. The owner's next
  stash close then ran the game's own `SaveStash #3`, and the game ended
  inside it after one closure, with no `ret=` for that call either, the file
  unwritten and still listing X (`counts-tool-after`: fail). The leading lead,
  unconfirmed, is that the take's after-state leaves the stash in a state
  `SaveStash` does not finish; it happened once, no control isolates it, and
  its cause is not established. Whether the character's save then held the
  unit the add had put in the bag was not read - so whether a crash there
  leaves one item in both the stash file and the bag is not observed. The
  design's step 4 rests on it, and so does any design that leaves the write
  to the next close or exit.
- *The recipe's inputs by name.* No member read gave the selected recipe's
  input type, base or amount, and the recipe definitions were not reached
  (`recipe-shape`: not observed). Step 2's count rests on it.
- *A move of an entry the bag has no stack for*, and whether an add carries a
  stack's `o` (`take-socket`: not observed).
- *`CraftEditPlayerInventory`'s `a0`*: `1.0` at a one-unit craft, which
  cannot tell an owner value from the amount; undecoded.
- *The duplication constraint* (`### Constraints from Phase 1`) still holds:
  the count comes from the map only, never from the cube's `a`, and a stash
  unit supplies an input only once the mod has counted it and seen its entry
  leave the kept map.

For the owner (the workorder's `## Needs human judgement` item 2), as a
result, not a decision: one craft-route row, `DoCraftResult`, did enclose both
the consume and the result's production in the craft measured, so refusing at
that row is not ruled out by this round, and the gate-only fallback is not
forced by it; what a multi-unit craft does there is not observed.

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

## Ship design

The player build of H-A: ForgePact 1.4.5, `craftmats` (panel: Quality of
Life, **Craft from the stash**, `mod_craft_mats`, off by default). It rests on
the route tokens in force - `save-route: proven`, `inject: proven` and
`cube-destination: proven` (Phase 1j), and `partial-stacked: inline`,
`partial-nostack: json` and `partial-cube: proven` (Phase 1k, confirmed by the
owner, `## Decision gate`) - and on the owner's design: count the two special
tabs in the game's own availability check, move only the shortfall at the
press, save the stash by the game's own route right after, and never widen
the vanilla duplication (`### Constraints from Phase 1`).

- **Core:** `plugin/include/ForgePact/CraftMatsMod.hpp`, game-independent (it
  names no runtime interface), pinned by `tests/test_craft_mats_behavior.py`
  + `tests/craft_mats_harness.cpp` (baseline and target scenarios; the
  targets' first failing lines are recorded in the harness comment) and
  `tests/test_craft_mats_contract.py`. Its source enum is the two special
  tabs and nothing else: `StashMaterialTab = 1`, `StashSocketTab = 2`.
- **Hooks:** six, each through `HookOneScript` by its `HeroSiege::Scripts`
  constant (`SdkShortScriptName`), installed once from `FrameCallback` on the
  first frame after setup with the switch on (the auto-prospect pattern):
  `CountInventoryItem`, `GetCraftItemsAvailable`, the recipe row's Create
  closure (`anon@840`, `UI_Craft_Recipe_List_Item_obj`),
  `CraftFindRecipeItems`, `PilipaliDecrypt` and `DoCraftResult`. One line
  names each hook's route: `craftmats: hooks CountInventoryItem=both-routes
  ... -> ON`. A `TABLE-ONLY` or `NOT-INSTALLED` hook turns the mod off for the
  session on the same line (`-> off for this session: ...`): a table swap
  never sees the compiled calls the crafting route makes. Every body calls the
  game's function through its trampoline, and with the switch off every body
  only forwards.
- **The route frames:** `GetCraftItemsAvailable`, the recipe row's closure and
  `CraftFindRecipeItems` each raise their own depth around their trampoline,
  only while the switch is on, and lower it on unwinding too.
- **The count:** after the game's own `CountInventoryItem`, only with the
  switch on, only inside one of the three frames, and only for `a0 = 1` (the
  bag owner, as Live 1i logged), the return becomes the game's count `k` plus
  the stash's count `s` of the same identity (`a1` the class, `a3` the base).
  `s` is the sum of `o` over the entries of the two special tabs only:
  `Controller_obj.stashMaterialTab` (`[x][y]` cells) and every row of
  `Controller_obj.stashSocketItemSlot` (each its own `[x][y]` cells). Each
  filled cell's `nodeFingerprint` is resolved in the map `GetItemMap(9)`
  returns, called by name at the point of use; an item's identity is its
  `itemType` and its `itemDefinitionStruct.b`. The ordinary tabs share that
  map and are never a source, so the map itself is never walked. A display
  count reuses one walk per game frame; inside `CraftFindRecipeItems` every
  count walks fresh. An unreadable walk leaves `k` and is named once:
  `craftmats: stash-unreadable - ...`.
- **The needs:** inside `CraftFindRecipeItems`, and only there, each
  `PilipaliDecrypt` answer and each count (identity, the game's own `k`) go
  into the core's record, and the core pairs them. R, a static reading in
  this document's own words, observed only for a one-input recipe: each
  input's amount is decoded before that input is counted, and an input that
  accepts several bases is counted one base at a time after its one decode,
  stopping at the first base whose count reaches the amount - so a count's
  amount is the latest decode before it, and of a run of counts after one
  decode the last is the one the game used. A count with no decode before it
  makes the needs unreadable. The next `CraftFindRecipeItems` entry discards
  the record; the press after it uses it once, for the same recipe row (by
  instance id; a row with no id matches nothing). Outside that frame the
  decode hook only forwards: `PilipaliDecrypt` ran 672,588 times in one
  session (`### Phase 1i results`, `recipe-shape`). The mod's own by-name
  calls are never recorded as the game's.
- **The press (`DoCraftResult`), in order:**
  1. The gate. Switch off, no record, or no stash count added during that
     `CraftFindRecipeItems` call: the game's own press, untouched. A record
     that cannot be paired, belongs to another recipe row, or already served a
     press: refused.
  2. The plan: `need - k` per input, capped by a fresh walk's count, and never
     more (the core's `Plan`: rows of one material summed; two reads of one
     material that disagree refuse).
  3. The takes, per input: split across the material's entries, whole entries
     first and then a partial from the last one needed; each take moves and
     is confirmed (below). The first take that is not confirmed stops the rest.
  4. Any refusal returns before the game's `DoCraftResult`; otherwise the
     game's press runs.
  5. After it, per material moved: the character's total over `GetItemMap(0)`
     (the bag and the Cube's grid alike) must equal the total before the move,
     plus what moved, minus what the recipe needs. A mismatch - the game's own
     produce-without-consuming duplication included - is named once and turns
     the mod off for the session: `craftmats: consume mismatch - ...`.
  6. After at least one confirmed move - following the game's press, or a
     refusal that came after a move - `SaveLocalFile(4, 1)`, self
     `Console_Save_obj`, the stash close's own save (`save-route: proven`).
     Never when nothing moved.
  7. One line per press that moved something: `craftmats: moved <n>
     class=<c> b=<b> from <materials|socketable> to
     <bag-stack|bag-new|cube>; saved=<yes|no|failed>`.
- **The take, per case.** Every call is by name through `script_execute`,
  self = other = `Console_Save_obj` (resolved by `asset_get_index` and
  `instance_find`), in the shapes Live 1i, 1j and 1k measured:
  - *Destination first.* `GetItemPreferredGrid(1, <stash item>)`'s `grid`,
    its cells resolved in map 0 for a stack of the same identity. Stacked
    (`partial-stacked: inline`): the stack's `itemDefinitionStruct.o += n`,
    then `ItemCheckHash(<stack>)`. No stack (`partial-nostack: json`):
    `CreateItemSaveStruct(<stash item>)`, its `o` set to `n`, `LootTimestamp()`
    for `<S>`, `InitItemFromJson(<struct>, "0-0-<S>-<class>")`,
    `AddItemToMap(<map 0>, <key>, <item>)`, then `GridAddItem(<bag grid>,
    <item>, 0, undefined)` - or, when the bag grid has no empty cell or
    answers `success=false`, `GridAddItem(New_Inventory_Data_obj.craftGrid,
    ...)` (`partial-cube: proven`). A unit no grid took is taken out of map 0
    again, `RemoveItemFromMap(<map 0>, <key>)`.
  - *Then the source,* only once the destination re-reads as risen by `n`: a
    partial take `o -= n` then `ItemCheckHash(<stash item>)`; a whole entry
    `RemoveItemFromMap(<map 9>, <key>)` then `GridRemoveItem(<its cell
    array>, <key>)` - the Materials tab whole, or the entry's Socketable row
    (Phase 1i's pair, which keeps the save invariant).
  - *Confirmation:* both sides re-read on their keys (`GetItemFromFingerprint(
    <key>, 9)` or `(<key>, 0)`, and the map's own `ds_map_exists`) and their
    cells. Confirmed: the source dropped by exactly `n` (or its entry and its
    cell are both gone) and the destination rose by exactly `n`. Not-taken:
    both sides as they were; the craft is refused. Anything else is a loss.
  - *Undo:* a source step that did not land undoes the destination - the
    stack's `o` put back and `ItemCheckHash`, or the new unit's
    `GridRemoveItem` from its grid and `RemoveItemFromMap` from map 0 - and a
    whole entry whose map entry went while its cell stayed is put back into
    map 9 first, `AddItemToMap(<map 9>, <key>, <item>)`, so no cell is left
    without its entry. These undo shapes reuse the measured scripts on
    another map or grid and were not run live; they run only on a failed
    take. An undo that cannot be confirmed is a loss.
- **Refusal and loss lines,** each reason once per session: `craftmats:
  unreadable - ... the craft was refused ...` (needs unpaired, another recipe
  row, or a count that did not read); `craftmats: not-taken - ... the craft
  was refused` (the game declined a move and both sides read as before);
  `craftmats: off for this session - ...` (a loss, an error inside the press,
  or an install without both routes); `craftmats: consume mismatch - ...;
  off for this session ...`. A player build carries no `craftmats stat` (the
  owner's rule); the per-press line names the work done instead.
- **Research build only:** `craftmats stat`; `craftmats 1` refuses, and the
  install stays off, while `craftprobe` detours any of the six (a second
  detour on one function reads as a false `TABLE-ONLY`); `craftprobe hook`
  reports the rows craftmats holds as `held by craftmats`.
- **Not covered, recorded as not observed live:** the Cube fallback (a full
  bag), a whole-entry take (a shortfall that empties a stash stack),
  multi-input and multi-unit recipes (the pairing reading), and the game's
  hash acceptance of an edited or created item beyond Phase C's watch
  (`no-flag`, `reload-after`; R, a static reading: `ItemCheckHash` itself
  calls no reporting script, and `ReportClient` has no positive control,
  Live 1k). The character's own save after a press-time
  stash save is left to the game; a crash in between loses that craft's
  stash-supplied materials (accepted by the owner, the hub guide's Known
  Limitations item 23).
- **Player DLL:** `plugin_build/BloodPactPlugin_ship.dll`, sha256
  `eedc27c30c57236ecbf0e1dd8c04a423e257aaea927911e6e9d932edb01c46f3`, built
  on 2026-09-24 by `plugin_build\build.bat release` from ForgePact `6f45abe`
  (the code commit; this line is a doc-only commit after it). The build is not
  byte-reproducible: a second `build.bat release` of the same commit, run
  minutes apart, gave a different sha256 (`a8822c4b...`; the two files first
  differ in the PE header), so this hash names this one file, and Phase C's
  `dll-hash` compares the installed DLL against it, not against a rebuild.

## Phase C live procedure

One session on the player DLL named in `## Ship design`, run by
`live-operator`, two launches, slot 14 ("Sorak"): a Socketable recipe (Ol ->
Old, the bag's Ol stacked onto) and a Materials recipe (Greater Unstable Dust
-> Destiny Shard Fragment, a new bag stack), the switch off and on, one press
each, and a watch for a flagged item. It is verification of shipped
behaviour, not research: every check is an acceptance check, and `dll-hash`
and `control` show the session measured anything at all. The step-by-step is
the workorder's `forgepact-issue-14-player-build-context.md` › `### Live
procedure 1`; saves are backed up before the launch and restored after it.

The capture is `forgepact-issue-14-player-build-live-1.md`, whose `## Checks`
section carries one line per check, `- <check> | expected: <text> |
observed: <text> | pass|fail|not-observed`, the fourteen checks in this
order: `dll-hash`, `control`, `off-stacked`, `hooks`, `bag-control`,
`on-stacked`, `stash-saved`, `off-nostack`, `on-nostack`, `no-duplicate`,
`stash-window-after`, `no-flag`, `reload-after`, `counts-tool-after`. A crash
ends the session: the check in progress is `fail`, and every later check
reads `not-observed (launch ended at <check>)`.

## Phase C results

**Live Phase C, 2026-09-24, 17:54-18:18 UTC.** One character (slot 14,
"Sorak"), two launches, `live-operator` on the command channel (`hs-drive`),
on the player DLL `## Ship design` names (sha256 `eedc27c3...`, built from
ForgePact `6f45abe`), installed in the game's mods folder. The saves were
backed up before the first launch and restored after the session. Each row
is filled from the capture,
`.claude/workorders/forgepact-issue-14-player-build-live-1.md`, cited by its
step headings, with the verdict its `## Checks` line gives. 13 checks pass;
`bag-control` fails on the produced item's name, an exception the owner
accepted (below).

| Check | What it measures | Observed | Verdict |
|---|---|---|---|
| dll-hash | The installed DLL is the player DLL `## Ship design` names | `hs_lease_acquire` hashed the installed DLL as `eedc27c3...` (`dll_status hashed`), equal to `## Ship design`'s hash (capture, dll-hash check) | pass |
| control | `ping` answers and `craftprobe` is unavailable: the IPC works and this is the player build | `ping` answered `pong (YYTK 4.0.1)`; `craftprobe` answered `command unavailable in player build: craftprobe` (capture, Control) | pass |
| off-stacked | The switch never on: the Ol recipe the bag cannot cover is unavailable and a press produces nothing | With the bag's Ol at 1 of the recipe's 3 and the switch never on, the owner read the Ol recipe as unavailable; nothing was pressed and nothing produced (capture, Step 1 result; Step 2 - off-stacked) | pass |
| hooks | `craftmats 1`: all six hooks `both-routes`, the mod on | `craftmats 1` answered that its hooks install once the game has settled; then `HOOK INSTALLED` for each of the six and `craftmats: hooks CountInventoryItem=both-routes GetCraftItemsAvailable=both-routes anon@840@...=both-routes CraftFindRecipeItems=both-routes PilipaliDecrypt=both-routes DoCraftResult=both-routes -> ON` (capture, Step 3 - hooks) | pass |
| bag-control | A recipe the bag covers crafts once from the bag, with no move line (the positive control) | With the mod on and the bag's 155 Greater Unstable Dust covering the recipe: available, one press, one result item, the bag's Dust at 150 (155 - 5), and no new output at all in the IPC log after the press, so no `craftmats:` move line. The item produced was a Satanic Crystal Fragment, not the expected Destiny Shard Fragment; the owner attributes it to an unrelated game bug (below) (capture, Step 4 - bag-control) | fail |
| on-stacked | The Ol recipe available, one Old produced, the bag's Ol used up, one move line (`socketable`, `bag-stack`, `saved=yes`), no refusal | Owner: "available, produced 1 Old, 0 ol left."; `craftmats: moved 2 class=15 b=1 from socketable to bag-stack; saved=yes`; no refusal, `off for this session` or `consume mismatch` line (capture, Step 5 - on-stacked) | pass |
| stash-saved | `stash.hss` written after each on-press, and the counts tool reads the lowered stacks, before any stash open | `stash.hss`'s write time moved after each on-press (18:01:42 after `on-stacked`, later than T1 of 2026-09-22; 18:10:57 after `on-nostack`, later than T2 18:07:40), and the counts tool, before any stash open, read `class=15 b=1 stack=214` (216 - 2) and then `class=14 b=51 stack=145` (150 - 5) (capture, Step 6 - stash-saved, first half; Step 6/9 - stash-saved, second half and overall) | pass |
| off-nostack | `craftmats 0`: the Dust recipe unavailable and a press produces nothing, with the hooks installed | `craftmats 0` answered `craftmats: off - crafting is unchanged`. The Cube, opened while the mod was on, still read the Dust recipe as available; after a Cube reopen the recipe read greyed out. Nothing was pressed in either part (capture, Step 8 - off-nostack; Step 8 result - off-nostack) | pass |
| on-nostack | The Dust recipe available, one Destiny Shard Fragment produced, one move line (`materials`, `bag-new`, `saved=yes`), no refusal | After `craftmats 1` and a Cube reopen, owner: "available, produced 1 destiny shard fragment, 0 dust left"; `craftmats: moved 5 class=14 b=51 from materials to bag-new; saved=yes`; no refusal, loss or mismatch line (capture, Step 9 result - on-nostack) | pass |
| no-duplicate | Each on-press and `bag-control` produced exactly one result, and each off-press none | Steps 2 and 8 (off) produced nothing; steps 4, 5 and 9 (`bag-control`, `on-stacked`, `on-nostack`) produced exactly one result each (capture, Step 10 - no-duplicate) | pass |
| stash-window-after | The stash window shows the lowered counts | Owner: "214 ol, 145 dust, drag looked fine"; a screenshot of the Materials tab shows the 145 stack (capture, Step 11 result - stash-window-after) | pass |
| no-flag | Dragging the edited stacks shows nothing wrong (no positive control exists: "no flag observed") | The owner dragged the Ol and Dust stacks away and back: "drag looked fine", no missing or changed stack, no mark, no message. No flag observed; no positive control for a flag exists (`ReportClient` never fired, Live 1k) (capture, Step 12 result - no-flag) | pass |
| reload-after | After the game's own quit and reload the stash and the bag match | After a quit through the game's menu, a relaunch and the character's load, owner: "214 ol, 145 dust, all items in bag, nothing marked". The bag was judged against what this session produced - 1 Old, 1 Destiny Shard Fragment, 1 Satanic Crystal Fragment (`bag-control`'s result) and 4 Nuts (the two extras below) - not the procedure's "two Destiny Shard Fragments" (capture, Step 13 - reload-after, relaunch; Step 13 result - reload-after, part 2) | pass |
| counts-tool-after | With the game stopped, the counts tool reads the lowered stacks | After a graceful `hs_stop_game`, `class=15 b=1 stack=214` and `class=14 b=51 stack=145`; `hs_saves_inspect` against the session's backup: changed `herosiege13.hss`, `inventory_order_13.hss`, `shop.ini` and `stash.hss`, nothing added or missing (capture, Step 14 - counts-tool-after, hs_saves_inspect) | pass |

**`bag-control`, the one fail, an owner-accepted exception.** The Greater
Unstable Dust recipe, pressed with the mod on and the bag's own Dust covering
it, produced a Satanic Crystal Fragment where the procedure expected a
Destiny Shard Fragment. The owner said at the press "different result caused
by the game bug, disregard", and after the session answered "Accept as game
bug (Recommended)". So the check stays `fail` here and in the capture, and is
recorded as that exception rather than re-labelled. What it controls for
held: the bag's count went 155 to 150, exactly the recipe's 5, the IPC log
gained no output after the press, so no `craftmats:` move line, and one
result was produced - the mod did not touch that craft. Whether the same
recipe produces the other item with the mod off was not measured; no game
bug is filed (the owner's rule).

**The count at the press.** With the switch on, each recipe the bag alone
could not cover read available and crafted on one press: Ol (the bag's 1 of
3, the stash's 216), Greater Unstable Dust (none of 5 in the bag, the
stash's 150) and the owner's Nut (neither input in the bag). Each move line
names exactly the bag's shortfall - 2 Ol, 5 Dust, then 3 Sal and 1 Chipped
Sapphire - so the amount the mod paired with each input from the decode
inside `CraftFindRecipeItems`, less the bag's own count, matched the recipe
each time. With the switch never on (`off-stacked`), and after `craftmats 0` and a
Cube reopen (`off-nostack`), the same recipes read unavailable: the game's
own count.

**The moves, per case.** The stacked case moved onto the bag's existing
stack (`bag-stack`: 2 Ol onto the bag's 1). The no-stack cases created new
bag stacks (`bag-new`: 5 Dust; the Nut's two inputs, both in one line).
Every take was a partial one: each source stack stayed non-empty (Ol 214,
Dust 145, Sal 187 then 178, Chipped Sapphire 208 then 205), so no
whole-entry take ran, and the bag always had room, so the Cube fallback
(`cube`) did not run.

**The consume.** After each on-press the bag held none of the input: the
game consumed the moved and the created units together with the bag's own,
and produced one result per unit crafted. No `consume mismatch` line
appeared, and the mod stayed on for the whole session - every later press
still logged its moves.

**The save.** Each on-press logged `saved=yes`, `stash.hss`'s write time
moved after it, and the counts tool read the lowered stacks with the stash
never opened (`stash-saved`). The same counts read in the stash window
(`stash-window-after`), after the game's own quit and reload
(`reload-after`) and with the game stopped (`counts-tool-after`), and
`hs_saves_inspect` names `stash.hss` among the changed files.

**The flag watch.** The owner's drag of the edited stacks and the reload
showed nothing marked, missing or changed (`no-flag`, `reload-after`). With
no positive control for a flag, this is "no flag observed": the game's own
hash acceptance of an edited or created item beyond this watch stays not
observed.

**The two extras, observed live once each.** The owner added two cases
during the session. They are in the capture's `## Extra checks`, which the
check tool does not read, and both passed there.
- `extra-nut-multi-input`, a multi-input recipe. The owner asked: "i want to
  try to produce nut too - 3 sal, 1 chip sapphire (none in the bag). this
  recipe is shown available too". One press gave one line, `craftmats: moved
  3 class=15 b=12 from socketable to bag-new, 1 class=15 b=52 from socketable
  to bag-new; saved=yes`; the stash's Sal went 190 to 187 and its Chipped
  Sapphire 209 to 208, `stash.hss`'s write time moved, and one Nut was
  produced (capture, extra-nut-multi-input - after-reads).
- `extra-nut-multi-craft-x3`, a multi-unit press of the same recipe. The
  owner asked: "let me also try the same nut craft but multiple at the time -
  for example 3". At the Cube's quantity 3, one press gave one combined line
  moving 9 Sal and 3 Chipped Sapphire with `saved=yes`; the stash went 187 to
  178 and 208 to 205, 3 Nuts were produced, and the Cube's craftable maximum
  read 62 before and 59 after. The capture shows no refusal, `off for this
  session` or `consume mismatch` line (capture, Extra checks -
  extra-nut-multi-craft-x3, after-reads; extra-nut-multi-craft-x3 - final).

These show that the pairing and the moved amount were right for this recipe
at quantities 1 and 3. They do not show how the Cube's quantity reaches the
need the mod recorded, or how many `DoCraftResult` calls one quantity-3 press
makes. The `unreadable` refusal line is printed once per session, so "no
refusal line" is what the capture records, and no more.

**The Cube reopen.** The Cube's recipe list shows availability as computed
when the Cube opened: after `craftmats 0` the Dust recipe still read
available while the window stayed open, and read greyed out at the next Cube
open, after the owner reopened the Cube (`off-nostack`). Observed once, on to
off. Nothing was pressed on the stale row, so a press on a row whose shown
availability is stale was not observed; the count at a press is taken inside
`CraftFindRecipeItems`, and what the game does on such a press is not
measured.

**`## Ship design`'s "Not covered", after Phase C.** Phase C has now observed
a multi-input recipe and a multi-unit press (the two extras above, once each,
for one recipe); the Cube fallback, a whole-entry take and the game's hash
acceptance beyond the `no-flag`/`reload-after` watch remain not observed
live, as do the undo shapes a failed take runs. `## Ship design` itself is
left as built, since it records the DLL this session ran.
