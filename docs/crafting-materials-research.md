# Crafting from the stash's material tab (ForgePact issue #14)

phase0-status: complete
phase1-status: pending

**Status: Phase 0 done (static search, instrument, decision core); the live
session (Phase 1) has not run.** Nothing player-visible changes yet: the
`craftmats` switch exists but nothing is wired to crafting, and the player
build refuses it. Every `## Results` row is empty until the owner-run session
fills it, and no reader should treat a blank row as a negative. A result is only
ever recorded as a negative with its positive control from the same session
(repo-root `AGENTS.md`, "Prove the Instrument Before Trusting a Negative
Result").

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
and where either one lives is a Phase 1 question.

**The goal.** With the switch on, a recipe whose inputs sit partly or wholly in
the stash's material tab counts as craftable at the cube, and the craft consumes
them there, without the player first moving them to the bag. Ordinary stash grid
tabs, the guild stash, the Blood Pact stash, the socketable/unique stash tabs and
the inventory's key/tarot/vault tabs are never touched. Off by default; with the
switch off the game behaves exactly as unmodded.

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
Rejected up front, whatever Phase 1 shows: reading or writing `stash.hss` (the
game holds the file while running, and ForgePact's model is in-memory only); a
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
  container is readable with the stash window closed (`M-stash-closed`).
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
   `M-recipe`. Follow any promising variable with `craftprobe var ...`.
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

## Results

Research DLL: not built yet (recorded here with its commit once built).

| Row | What is measured | Result | Evidence (out.txt lines / by eye) |
|---|---|---|---|
| B0-vanilla | Unmodded: does a recipe whose only input is in the stash tab count as craftable and craft (stash closed; stash open)? | | |
| C-control | `CheckPlayerInteraction` count in the same session as every row below (must be non-zero) | | |
| M-bag | `craftprobe bag`: where the bag's materials tab lives, its kind and entries | | |
| M-stash-open | `craftprobe stash` with the stash window open: where the stash's material tab lives | | |
| M-stash-closed | `craftprobe stash` with the stash window closed: is the material tab readable closed? | | |
| M-recipe | `craftprobe recipe`: the selected recipe's needs, reachable by name? | | |
| M-craft | Hand craft from the bag: which rows fire, with what self/other/arguments/returns; bag count change | | |
| M-move-stash-to-bag | Hand click-move stash tab -> bag: rows, `success` answer, counts by eye | | |
| M-move-bag-to-stash | Hand click-move bag -> stash tab: rows, `success` answer, counts by eye | | |
| M-backing | `backing dump`: what the profile and stash getters returned (json files) | | |
| H-A | Availability and consume rows extendable by name, and the stash tab readable closed? | | |
| H-B | A by-name stash -> bag move with a `success` answer, bag cap not hit? | | |
| H-C | Availability extendable, consume not? | | |

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
rather than proposing a struct-layout read.
