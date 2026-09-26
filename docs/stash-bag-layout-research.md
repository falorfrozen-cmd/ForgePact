# Stash and bag layout: where the stash, its tabs and the bag's cells are

**Question.** For the town stash and the bag beside it, as the running game
reports them: how does the stash open and close, which listed row is each
stash tab and each bag tab, which variable says which tab is on show, where
is each grid cell and which item it holds, and which of the game's own
routines, called by name, moves one whole stack or exactly one unit of a
stack between the bag and the stash - and creates the items a test needs?

**Why it matters.** The hub's `hs-drive` MCP server (`docs/tools/hs-drive-mcp.md`
in the toolkit) is gaining six tools - `hs_give_item`, `hs_stash_open`,
`hs_stash_close`, `hs_stash_tab`, `hs_bag_tab` and `hs_move_item` - so a live
test session can run with no one at the keyboard (toolkit issue #147). They
exist to *set up* game state for other tests, so each goes through the
runtime by name wherever the game's own routine can be called (the player's
position, the tab handlers, the move routines, the item loader), and proves
what it did by re-reading `menulayout`. The UI route - a click on a point
`menulayout` lists - stays only as the fallback a § Decision line keeps.
None of these mechanisms is measured for this purpose yet. This document
records the static search, what a local static reading suggests, the
instrument, the one research launch that measures them, and the decision the
hub tools are written against.

**Note (2026-09-25, D11).** The owner split the whole-stack and one-unit
moves out of this workorder: `hs_move_item` and the `moveWholeRoute`/
`moveOneRoute` orders that P2-5 and P2-6 below measured now belong to
`hs-drive-stash-move-research`, which starts from what those two checks
found. This document's tools are five: `hs_give_item`, `hs_stash_open`,
`hs_stash_close`, `hs_stash_tab` and `hs_bag_tab`. The procedure below is
left as it ran; only the § Decision lines for the two moves record where
they went.

**Posture.** Everything here is measured runtime behaviour, a reading written
in our own words, or our own code. Game objects and scripts are named by their
`hs-game-sdk` names and indices; no game script text appears, and the
procedure is written as prose.

**Status.** The instrument (the extended `menulayout`, beside `craftprobe` and
`citrace` in the research build) is built on this branch; the `menulayout`
extension ships in the player build. The first research launch (§ Live
procedure › Live procedure 1) ran on 2026-09-25 and settled six § Decision
lines: the warp, the close, which field names a stash tab, which variable
says which stash tab is on show, how a count is read and what an item is
matched by. It could not replay any by-name call - the instrument had no
argument kind for a live instance or an object reference, and the drag turned
out to write its cell through a constructor - so the open, both tab switches,
both moves, the bag's tab state and the item provisioning were measured again
by name in Live procedure 2 (2026-09-25), on a build carrying the additions §
Instrument lists. That second launch closed the other nine lines; two of
them, `moveWholeRoute` and `moveOneRoute`, moved out of this document's scope
under D11 to `hs-drive-stash-move-research` once their refusals turned out to
answer a mis-read prerequisite, not a game negative. § Decision now carries
all fifteen. What § Instrument lists as hypotheses, and what § Static
readings suggests, stay readings except where a § Decision line or the live
sessions' own § Results rows confirm them.

## Static search

Source: `hs-game-sdk`'s object and script tables and its parent hierarchy
(the Python binding's parent lookup and the C++ `objects.hpp`), searched on
2026-09-23 over every object whose name matches a stash, bag, inventory, tab,
grid, split or drag concept; plus `menu-layout-research.md`,
`character-select-research.md`, `prospect-window-research.md` and, since this
branch merged `main` on 2026-09-25, `crafting-materials-research.md` in this
repository.

`UI_Inventory_Parent_obj` (5115) has ten children: every window that shows
the bag beside its own content, among them `UI_Stash_obj` (5257) and
`UI_Inventory_obj` (5114). The stash's tab strip is a
`UI_Stash_Tab_Bar_Container_obj` (5260); a stash tab is a
`UI_Button_Stash_Tab_obj` (5010, child of `UI_Button_obj`); the bag's tabs are
`UI_Button_Inventory_Tab_obj` (4988) and `UI_Button_Inventory_Tab_Small_obj`
(4989). A grid of cells is a `UI_Inventory_Grid_obj` (5112). The split-stack
dialog is `UI_Split_Stack_obj` (5254, parent `UI_Parent_obj`), the stash's
drop-down `UI_Stash_Dropdown_obj` (5255), and the Socketable tab's container
`UI_Stash_Socket_New_obj` (5259). A window's close button is a
`UI_Button_Close_obj` (4982, child of `UI_Button_obj`).
`UI_Inventory_Drag_obj` (5110) **has no parent**, so only its own name
reaches it; whether it is what holds an item on the cursor is not measured
(the prospect research attributes the held item to `s_InventoryDrag`, a
script). `New_Inventory_Data_obj` (3067, unparented) holds the bag's item
map. Two room-space objects place the player beside the stash: the stash
itself, `Town_Stash_obj` (4852, under `Quest_NPC_Parent_obj` and
`Collision_Parent_obj`), and the player, `Player_obj` (3553); for these two
the row's `gui=` is a room position and its `win=` means nothing.

The command's candidate table is therefore twenty-nine names: the thirteen of
the character-select search plus sixteen.

| Group | Objects |
| --- | --- |
| Roots | `UI_Node_Parent_obj`, `UI_Parent_obj`, `UI_List_Item_Parent_obj` |
| Character-select leaves | `UI_Button_obj`, `UI_Button_Small_obj`, `UI_Character_obj`, `UI_Create_Character_obj`, `UI_Main_Menu_obj` |
| Unparented (menu) | `Save_Character_obj`, `Save_Slot_Shop_obj`, `Load_Inventory_Char_Select_obj`, `Menu_Controller_obj`, `Profile_Manager_obj` |
| Windows with the bag | `UI_Inventory_Parent_obj`, `UI_Stash_obj`, `UI_Inventory_obj` |
| Tabs | `UI_Stash_Tab_Bar_Container_obj`, `UI_Button_Stash_Tab_obj`, `UI_Button_Inventory_Tab_obj`, `UI_Button_Inventory_Tab_Small_obj` |
| Grids, dialogs, buttons | `UI_Inventory_Grid_obj`, `UI_Split_Stack_obj`, `UI_Stash_Dropdown_obj`, `UI_Stash_Socket_New_obj`, `UI_Button_Close_obj` |
| Unparented (inventory) | `New_Inventory_Data_obj`, `UI_Inventory_Drag_obj` |
| Room space | `Town_Stash_obj`, `Player_obj` |

Negative results of the search, recorded so nobody repeats it:

- The bag's Materials tab is **not a grid node**: the prospect research found
  its stacks drawn by a draw script, not held in a `UI_Inventory_Grid_obj`,
  and no prior research read a count from it. The stash's Socketable tab is
  container-shaped too (per-item grid instances under a container, prior
  measurement below); the stash's Materials tab is not - prior measurement
  found it to be the window's own `StashGrid`, a plain grid node.
- No stash hotkey exists (the owner, 2026-09-23: WASD moves). The interact
  key is `F`, not `E`: live 1 measured `E` doing nothing beside the stash and
  `F` opening it, and the owner confirmed "All interactions are with f". No
  session has yet opened the stash by name (live 1's two by-name attempts
  are `shape not reproduced`, § Results).
- Which input opens the split-stack dialog was not recorded: the owner
  opened it by hand.
- The drag and the split call **none** of the named grid and map routines.
  With the twelve rows `InventoryGridRemoveItem`, `GridRemoveItem`,
  `InventoryGridAddItem`, `InventoryGridAddItemToTab`, `StashGridAddItem`,
  `GridAddItem`, `InventorySwapItemsNew`, `InvGridClearItemNode`,
  `NetworkSendInventoryUpdate`, `RemoveItemFromMap`, `AddItemToMap` and
  `ChangeItemOwner` armed, a hand drag from the bag into the stash and a hand
  split of one unit into the bag left all twelve at 0 calls, while the
  interaction-check control climbed in the same session (live 1, P0-7). The
  only item write either logged was `s_InvNode`, a struct constructor whose
  self is the struct being built (§ Static readings). That is a measured
  negative for the drag path, not for the routines: the `craftmats` family
  calls several of them by name and they work (§ Static readings).
- `DebugItemSpawning` resolves to a stub with no body a static reading
  reaches, so there is nothing to call. `CreateItemNew` (the from-definition
  item creator) has no body in the local static project, so its argument
  shape is unknown; the game's own loader reaches it (the in-tree comments at
  `TruthBuildItem` and at the signature drop say so), and phase 0 logs that
  shape from the loader's own call (§ Instrument) instead of guessing it.
  Neither is *absent*; both are *not observed*.
- The give-item route copies a template the character already holds. That
  is a design choice, not a measured limit: `SpawnSignatureItem` and
  `TruthBuildItem` in this repository already build items the character never
  held from a json through `InitItemFromJson`.

What prior measurement established (the prospect research and the ForgePact
#14 crafting-materials research, both in this repository; the latter's
authoritative record is its own § Results, § Decision gate and § Ship
design, folded into the toolkit's `docs/RUNTIME_DATA_MODELS.md` § 17), in our
own words:

- A button carries `uiNodeCallstack` (a string naming its handler),
  `activationFunc` (a method value) and `activationArgs` (an array); a grid
  node carries `nodeGrid` (rows of cell structs, or an empty marker),
  `nodeGridWidth`, `nodeGridHeight`, `gridScale` and `gridName`; an occupied
  cell struct carries its start cell, lock flags and `nodeFingerprint`.
- The bag's main grid is the `UI_Inventory_Grid_obj` tagged `InventoryGrid`,
  15 by 6. A click on a prospect-grid item quick-moves it to its preferred
  grid; a click on a stash Socketable stack picked it up onto the cursor. A
  drag is pick-up and place.
- On the Socketable tab, `UI_Stash_obj` read `stashTabSelected` and
  `tabSelected` both -2. The game's own tab table numbers the personal tab 0,
  the shared tabs 1 to 19, Socketable -2, Materials -4 and Unique -5; the
  max-tabs getter answered 8. A hand click on a stash tab ran
  `UiAStashTabClick` with self the tab button and other the stash window; a
  bag tab click ran `UiAInventorySocketTabClick` or
  `UiAInventoryMaterialTabClick`. Result structs from item placement carried
  a `tabType` of -4 for the Materials tab and 0 for the main grid. Not
  measured there: -4 on the window's own variable, the bag's tab-state
  variable, and which button field carries the tab number.
- Closing the stash destroys or deactivates its window, grids and
  containers; a reopen makes new instances with new ids, so no id is cached
  across a close. The stash is saved once per close, and the stash and the
  Cube cannot be open together.
- A bag stack's count is the item struct's `o`, reachable getter-free by
  looking the cell's fingerprint up in `New_Inventory_Data_obj`'s
  `localItemMap` with the runtime's `ds_map_find_value`. The stash's item
  map was not found in any printed variable (not established as absent).
- One unit moved by hand into a stash special tab went through the split
  dialog and then the stash's add-to-stack routine, which answered success;
  one unit out of a special tab went through the grid's own node placement
  with no success answer. Several routines are proven by name with self the
  save-console instance: the stash save, an edit of a stash stack's `o`
  followed by the game's own hash refresh, and creating one unit by the json
  route (save struct, count, timestamp, loader, map add, grid add) into a bag
  grid or the Cube grid. The shipped `craftmats` mod calls these by name.

## Static readings

A local static reading (2026-09-25) of the named routines and of the Create
closures of the stash, inventory-grid and tab-bar objects, in our own words.
Nothing from it is copied here, and every item below is a reading to be
confirmed by phase 0, never a fact.

- **Opening.** The town stash's interaction handler checks that the player
  is in town, that no other interface is open, the rule that loot on the
  ground blocks the use key, and that the player is within talking range;
  only then does it create the stash window through `UiCreate`. So a by-name
  open either calls that handler with the stash as self after placing the
  player in range, or calls `UiCreate` with the arguments the handler passes.
- **Tab clicks.** `UiAStashTabClick` reads the profile inventory data and the
  item owner, stops any running identify, and writes three members, two of
  them to the same slot - consistent with `stashTabSelected` and
  `tabSelected` reading the same value. The bag tab handlers reset the tabs,
  read the inventory data object, set the focus and write one member, so the
  bag's tab state most likely lives on `New_Inventory_Data_obj`. The tab
  bar's closures set each tab button's activation function and read the
  max-tabs getter.
- **Grid handlers.** The inventory grid's drop and click handlers look the
  item up by fingerprint, validate it, check and refresh its hash, stamp it,
  copy it, add it to a stack or a tab, split a drop, edit the item data,
  remove it from its map and send an inventory update. A move that bypasses
  them has to leave the same map and hash bookkeeping behind. Live 1 did not
  observe the hand drag going through these handlers (none of the twelve
  armed rows fired, with the control climbing; below), so
  the by-name move is the `craftmats` family, never a replay of the drag.
- **Split.** `UiASplitStack` parses a typed number, reads the item and its
  size, and can report the client: the dialog takes a typed count and
  validates it.
- **Drag.** Starting a drag resolves the owner and the item and copies it
  into the drag data.
- **Close.** The close button's routine only removes a window; the stash save
  runs from the close route (measured in #14), not from the button. So the
  close goes through the game's own route, never through destroying the
  window.
- **Method values.** The activation functions the closures set are resolved
  by script name at run time, from the same name table
  `GetNamedRoutinePointer` reads.
- **Not observed.** The debug item spawner and the two talent-use entry
  points read as short stubs whose real bodies are reached through the name
  table; a static reading of those names shows nothing. That is a limit of
  the reading, not an absence.
- **Slot names.** The variable names the bodies read are not recoverable
  from the static project, so the readings give call graphs, constants and
  control flow, not field names; the phase 0 dumps supply the names live.

A second reading (2026-09-25, after live 1), of the routines live 1 logged
and of the ones it showed the plan had wrong. Each is a reading until
Live procedure 2 confirms it:

- **The drag path.** `s_InvNode` is a constructor: its self is the struct
  under construction, and it is what writes the destination cell (live 1
  logged it with the destination grid as other and the cell's x, y and the
  item struct as arguments). The drag's own handling is inline code with no
  named routine of its own - `ProcessInventoryGridInput`, 311 calls in the
  same window, is the likely holder - so no named routine that reproduces
  it was found by this reading (`s_InvNode` itself was not replayable: this
  instrument cannot supply a struct under construction as self). The two grid closures that call the named grid
  routines (the one that checks the hash, stamps, copies and adds to a stack
  or a tab, and the one that edits the item data, splits a drop, sends the
  inventory update and removes from the map) did not run on live 1's drag:
  they serve another input path.
- **The tab handlers' wiring.** The tab bar's Create closures build each tab
  button, set its activation function through `UiSetActivationFunc`, and
  scroll the strip with `UiMoveNode`. The scroll is why a click at a
  child's own `win=` missed (live 1, P0-5): `menulayout` reports the
  button's own position, not the clipped, scrolled one. Each tab's handler
  is its activation method, so `craftprobe methods` on the button names it
  without a hook. `UiAStashTabClick` reads the profile inventory data and the
  item owner, stops a running identify and writes the tab members;
  `UiAStashMaterialTabClick` only stops the identify and writes.
  `UiAInventoryMaterialTabClick` and `UiAInventorySocketTabClick` reset the
  tabs (`InventoryResetTabs`: stop the identify, set the focus, remove
  nodes), read the inventory data object, set the focus and write one member
  - the bag's tab state.
- **`UiACloseButton`** calls only runtime builtins (a node removal). The
  stash save runs from the close route, which live 1 measured on the close
  row's click (P0-9); whether calling the routine by name reaches that route
  is Live procedure 2's P2-7.
- **The move's library is `craftmats`.** The shipped mod already moves items
  by name, every script through its `hs-game-sdk` constant and
  `GetNamedRoutinePointer`, with self the save-console instance. Creating
  an item is `CreateItemSaveStruct` from a source item, `LootTimestamp`,
  `InitItemFromJson` with the saved struct and the new key, `AddItemToMap`
  into the destination map, then `GridAddItem` with the destination's cell
  array, the item, 0 and `undefined` (it answers a placement struct carrying
  `success`); removing is `RemoveItemFromMap` then `GridRemoveItem` with the
  cell array and the key; editing a count is a write of the item
  definition's `o` followed by `ItemCheckHash`; the bag's cell array comes
  from `GetItemPreferredGrid`. `StashGridAddItem` wraps `GridAddItem` after
  the max-tabs getter. `ChangeItemOwner`, with self the bag's grid instance
  and the source map, destination map and key as arguments, moves one map
  entry (#14 logged it on the game's own stash-to-bag drag). #14 also found
  the persistent cell arrays on `New_Inventory_Data_obj` and each grid
  instance's own mirror, `nodeGrid`; a stash tab's array is found the same
  way (`craftprobe find`).

Live 2 (2026-09-25) confirmed the second reading's items that were still
readings, not facts:

- **The tab handlers' shapes.** `UiAStashTabClick`'s logged shape (Shared1,
  self the tab button, other `UI_Stash_Tab_Bar_Container_obj`, `argc=1
  a0=array len=2`) and `UiAStashMaterialTabClick`'s by-name replay both ran
  as the second reading described; `UiAInventoryMaterialTabClick`'s logged
  shape (self the sub-tab button, other `UI_Stash_obj`, `argc=1 a0=array
  len=0`) left `tabSelected` at -4. No full before/after member diff of
  either was taken, so "one member and nothing else" is not measured, and
  the by-name replay's value before the call was not read (see the
  `bagTabRoute` line), so its -4 does not separate a switch from no change.
  `tabSelected` is not `stashTabSelected`: the P2-3 dump read -2 on the
  second beside 0 on the first, and a bag page-tab click moved the first
  alone, so the second reading's "consistent with both reading the same
  value" does not hold.
- **`UiACloseButton` by name reaches the close route.** The reading above
  left it open whether calling the routine by name reaches the route live 1
  measured on the close-row click. Live 2's P2-7 called it by name
  (`craftprobe call UiACloseButton id:<button> other:<window> confirm`) on a
  reopened window and got the same effect as the click: the window unlisted
  and `SaveStash` incremented once. The routine is a route, not only a
  runtime-builtin node removal.
- **`activeNode` was not observed to follow a by-name bag tab call.** A
  real click on the bag's Materials sub-tab moved `UI_Stash_obj.activeNode`
  to that sub-tab's id (the `citrace dumpobj UI_Stash_obj` after it). After
  the by-name replay of `UiAInventoryMaterialTabClick` the same member still
  held the previous sub-tab's id. Why is not established: the static reading
  above says the sub-tab handlers themselves set the focus, and the replay
  supplied no argument where the logged call passed one empty array - the
  untried logged shape is the first alternative to test. `hs_bag_tab` proves
  `tabSelected`, never `activeNode` or the focus.

## Instrument

Three commands, used together in the research (dev) build:

- **`menulayout`** is the player-build command documented in
  `menu-layout-research.md`, unchanged in its header, footer, row format,
  200-row cap, deduplication by instance id and one-name form
  (`menulayout <ObjectName>`). It is the reader every proof in the hub tools
  rests on. This branch changes three things, and
  `tests/test_stash_bag_layout_contract.py` pins each of them beside
  `tests/test_menu_layout_contract.py`: the candidate table, the optional
  fields and the array format (below).
- **`craftprobe`** (research build, in-tree since this branch merged `main`;
  every subcommand is documented under `crafting-materials-research.md`
  § Instrument) native-detours a table of named routines by address after
  resolving them by name, logs the self, other and arguments of their next
  calls (`arm`, `show`), reads the stash, bag and item maps hook-free
  (`stash`, `node bag`, `node stash`, `find`, `mapkeep`), and makes exactly
  one by-name call of a plain script (`call <Row> <Obj> <nth> [args] confirm`)
  or of a method value (`callm`), or one variable write (`set … confirm`).
  Phase 0 uses it to log what the game's own hand-driven moves call, and to
  replay those calls by name. This branch adds to it for the two research
  launches (below).
- **`citrace`** (research build): `dumpobj <Obj> [nth]` prints every instance
  variable, which is where the tab-state and tab-identity variables are
  found; `cb <builtin> [args]` calls one runtime builtin; `iset <var> <n>`
  writes one player variable by name.

The three `menulayout` changes:

1. **The candidate table** is the twenty-nine names above, each spelled
   through the SDK's `GameObject` enum.
2. **More optional fields.** After `label`, `name`, `slot`, `index`, `page`
   and `selected`, a row prints - only when the instance carries the
   variable, in this order - `uiNodeCallstack`, `activationArgs`, `enabled`,
   `tabNumber`, `tabType`, `stashTabSelected`, `nodeGridWidth`,
   `nodeGridHeight`, `gridScale` and `gridName`; `text` stays last on every
   row. `activationFunc` is left out on purpose: a method value has no
   stable text. Of the seven names after `enabled`, only `tabNumber` and
   `tabType` are unmeasured on an instance (they were seen on result
   structs); `stashTabSelected` (#14) and the four grid names (the prospect
   research) were read on instances. Phase 0 prunes any name no dump showed.
   After live 2 (Step 4 of the ship round, 2026-09-26): every one of these
   showed on a live row, so none was pruned, and `tabSelected` joined after
   `stashTabSelected` because `bagTabState` depends on it. The same round
   added the cell rows: after each `UI_Inventory_Grid_obj` row, one
   `  cell=<x>,<y> grid=<id> fp=<nodeFingerprint|none> o=none` row per
   occupied node (`cellRule`), the first 200 row-major, the grid's row
   carrying `cellcap=1` past that.
3. **Array values** print as `[a,b,...]`: the runtime's `array_length`, then
   each element through `array_get` and the same value formatter as any
   field; a nested array prints `<array>` rather than being walked, past 32
   elements the rest are counted as `,...+N`, and an element that fails to
   read prints `<read-failed>` in its place. Only a string goes through the
   runtime's string conversion: a reference (an instance handle included),
   a struct or method, and a pointer print by kind as `<ref>`, `<object>`
   and `<ptr>`, and any other kind as `<kind N>`, because what that
   conversion makes of them has not been measured. So a tab identity held
   as a handle in `activationArgs` shows only as `<ref>` here; the P0-4
   dumps are where its value is read. This is compiled and run against a
   stand-in runtime that represents arrays, nested arrays, `VALUE_REF`
   handles, structs and pointers, and whose string conversion refuses
   anything but a string (`tests/menu_layout_value_text.cpp`).

The `craftprobe` additions (research build only; `strip_research_blocks`
leaves none of them in the player build, and
`tests/test_stash_bag_layout_contract.py` pins each). Without them three
steps of the procedure below could only have recorded a negative the
instrument caused, never one the game did: `call` refuses a closure, so a
by-name open through the stash's own handler could never run; no row logged
`CreateItemNew`, which the give-item route reaches; and `call` passed its
self as the other too, while the stash tab click #14 logged had the tab
button as self and the stash window as other.

- **`other:<id>`** on `call` (directly after the self) and on `callm`
  (directly after the member): the instance passed as other, resolved as an
  `id:<n>` self is (`instance_exists`, then resolved by name), and refused
  with nothing called when it does not resolve. One parser serves both (and
  the skill research's `skillprobe call`), and both dispatchers pass the
  other separately to `script_execute`; without the token the other is the
  self, as before. The reply prints `self=` and `other=` in the form the
  armed lines use, so the logged and the supplied shape compare field by
  field. A logged other that is not an instance (a struct, `undefined`,
  `(null)`) cannot be supplied: that is `shape not reproduced`, below.
- **`methods <Obj> <nth>|id:<n>`** calls nothing and hooks nothing: it lists
  each variable of the instance, and each member of a plain-struct variable
  one level down, whose value the runtime calls a method, with the script the
  method wraps (the read-only `method_get_index` and `script_get_name`) and
  the `craftprobe` row that names it, if any.
- **`callm <Obj> <nth>|id:<n> inst <member> …`**: the holder `inst` reads the
  member off the instance itself. The other holders reach only plain
  structs, and a closure the Create event stores sits on the instance. It is
  refused before the dispatch unless the value is a method and a
  `craftprobe` row names its script, so a closure is called only when the
  reply can say which one it was. A method in a plain-struct member keeps
  using `path:`. `call`'s own refusal of a closure now names this route:
  `methods`, then `callm … inst` or `callm … path:`.
- **Three rows**, each by its `hs-game-sdk` constant, after the Phase 1k rows
  and before the interaction-check control, which stays last:
  `CreateItemNew` (the give-item route's own loader call is its positive
  control), `UiCreate` (the open logs it and replays it) and
  `NetworkSendInventoryUpdate` (on the grid handlers' path). The marker
  reads `rows=285` (#14's 282 plus these three). A row whose function
  another install in this plugin already holds is reported `held by
  <install>`, neither detoured nor failed.
- **`CreateItemNew` under this build's own item hook.** The research build
  swaps `CreateItemNew`'s script-table entry at setup for its item-inspect
  hook (`bp_citemn`, table-only), so the entry the row resolves is not the
  game's code. The row then detours the game's function that hook saved as
  its original, after the same executable-code check - the shape the
  toggle-skill probe already uses - and the detoured line says so:
  `craftprobe hook: detoured CreateItemNew at exe+0x… (under table-only
  bp_citemn)`. When Custom Forge entries are loaded or the Item Editor has
  asked for Item Truth, that install has already detoured `CreateItemNew`
  inline, its saved original is a trampoline rather than the game's code,
  and the row is reported `held by custom forge` or `held by item truth`
  (inline detour). Which of the two applies is decided by that address; the
  install's name in the message is the only thing its flags decide. Live 1
  found that Custom Forge detours `CreateItemNew` at plugin start on every
  launch of this build (the launch banner prints `HOOK INSTALLED on
  CreateItemNew` before any command), with no forged-item entries and no
  Item Truth request, so the row is `held by custom forge` in every session
  and the `under table-only` path does not occur on this build.

Two more additions for Live procedure 2 (research build only, pinned by the
same test). Every by-name route live 1 could not reach failed on the
instrument, not the game: no argument form could supply a live instance or
an object reference, and the close had no row.

- **`id:<n>` and `obj:<Name>` arguments**, in the one argument parser that
  `call`, `callm` and the skill research's `skillprobe call` share. `id:<n>`
  passes the runtime's own `id` value of a live instance (checked with
  `instance_exists`, then read with `variable_instance_get(<n>, "id")` - the
  kind a logged `ref instance N` argument has); `obj:<Name>` passes what
  `asset_get_index` answers for that object name, exactly as answered (this
  runner answers `ref object <Name>`, the kind `UiCreate`'s logged first
  argument has), refused unless the runtime names that index an object of
  that name (`object_exists`, `object_get_name`). Each refuses before the
  dispatch, naming what was supplied. The reply prints every supplied
  argument through the same value printer the armed lines use, so a
  supplied `ref instance N` or `ref object Name` compares with the logged
  one field by field.
- **One row, `UiACloseButton`**, by its `hs-game-sdk` constant, after the
  three rows above and before the control. The marker reads `rows=286`.
  The restart research probe's table names the same script; whichever
  installs first holds it, reported as any other `held` row is.

**Recording rule**, for every by-name call in the procedure: the capture and
§ Results carry the *logged* shape (self, other, argument count and each
argument, from the same routine's armed line in this session, or from #14's
record where the procedure names it) beside the *supplied* shape (the call's
own reply line). Fields a step changes on purpose (a swapped grid, another
tab button, a new key) are listed as `intended:` before the call and are not
a mismatch. The outcome is exactly one of:

- `reproduced`: the shapes are equal, so the game's answer - success or
  refusal - is a measured result;
- `shape not reproduced (<field>: logged <x>, supplied <y>)`;
- `not-run (instrument: <why>)`: the instrument refused or could not reach
  the routine (a closure with no holder, a row `held` or `failed`, a row
  whose control stayed at 0 calls).

The last two are never a route negative. A § Decision line that falls back
to a UI route after either says "by-name not tested with the logged shape".

`menulayout` stays a reader. It runs inside the command poll, installs
nothing on the per-frame path, and never clicks, hooks, performs an event,
creates, destroys, writes, or runs a game script - neither through
`script_execute` nor through `CallBuiltinEx`. Game getters such as the item
lookup by fingerprint or the item-map getter are out of scope for it: a
count, when there is one, is read from instance and struct variables only.
The write verbs the hub tools will use are separate commands, not
`menulayout`, and they may call the game's own routines by name.

**Positive control, every session.** `ping` answers `pong (YYTK 4.0.1)`; in
town `menulayout Player_obj` prints a header reading `room=Town_01_rm` and
exactly one `obj=Player_obj` row with a finite `gui=`; `craftprobe hook`
answers `<n> detoured, 0 failed` (a `held by` note for a row another probe
or install holds is fine); after the character load `craftprobe show`
reports a non-zero count for the interaction-check control row. Any of these
failing makes the session instrument-blind. A bare `craftprobe` must answer
`craftprobe: phase1k rows=286 - …` since the `UiACloseButton` row (live 1's
build answered `rows=285`; 282 names a build without this branch's
additions). A second control inside the
procedure (P0-5): with the Socketable tab on show, the stash window's
`stashTabSelected` reads -2, the value prior measurement recorded, both in a
`citrace dumpobj` of the window and in the `stashTabSelected=` field of
`menulayout UI_Stash_obj` - the second is the field the hub tools will read,
so it needs its own control rather than borrowing the dump's. If either
fails, that check is instrument-blind.

**Hypotheses phase 0 tests** (none is a fact yet). Setup goes through the
runtime by name; the UI route is measured only as the fallback a § Decision
line may keep. Written before live 1, and kept as written; live 1 changed
three of them. The interact key is `F`, not `E`. The whole-stack and
one-unit moves are not replayed from a hand move, because the drag calls no
named routine: Live procedure 2 measures the `craftmats` family by name
instead (§ Static readings). And a tab's shape is logged on a tab that is on
screen, then replayed by name for the others, because a click at a scrolled
child's `win=` misses.

- **Warp** (the route, not a fallback). Write the player's `x` and `y` by
  name - the player resolved by object name, the write through the runtime's
  own instance-variable setter, no address and no struct offset - to a point
  beside `Town_Stash_obj`, starting 48 px below it. Unknown: whether the game
  keeps the written position (collision, camera), and which offset satisfies
  the talking-range check the reading names. The research build measures it
  with `iset`; the player build gets a `playerwarp` verb.
- **Open.** (b) first, because it logs the shapes the by-name routes
  replay: the interact key `E` after the warp, with the stash's handler and
  `UiCreate` armed so their self, other and arguments are logged. Then (a)
  by name: the stash's handler, a closure on the stash, found with `methods`
  and called through `callm … inst` with the logged shape; and (c)
  `UiCreate` by name with the arguments (b) logged. The player build gets a
  `stashopen` verb only for (a) or (c); otherwise the hub presses `E`.
- **Tab identity.** A stash or bag tab row is identified by
  `uiNodeCallstack` and/or `activationArgs` (for instance the tab number its
  handler receives), not by its text or its position; the number of listed
  stash tab rows equals the max-tabs answer.
- **Tab switch by name.** The tab handler a click runs, with self the
  matching tab button and other the stash window (the shape a hand click
  logged in #14), supplied through `other:<id>`. Which handler a Materials
  click runs is not recorded - `UiAStashTabClick` or
  `UiAStashMaterialTabClick` - so every tab handler row is armed, the click
  logs the shape first, and the replay copies it. For the bag, the handler
  its click logs, the same way. The UI fallback is a click on the row's
  `win`.
- **Tab state.** `stashTabSelected` = `tabSelected` on `UI_Stash_obj`: -2 on
  Socketable is the control; -4 on Materials and 0 on the personal tab are
  expected from the tab table. The bag's variable is whichever member of
  `New_Inventory_Data_obj` (or `UI_Inventory_obj`) changes across one bag
  tab switch.
- **Whole-stack move by name.** The game's own grid routines, in the order
  and with the arguments one hand move logs: the owner drags the junk item
  from the bag to the stash's personal tab once with the grid, stash, map and
  swap routines armed, and the operator replays that order by name in the
  other direction on the same item. The replay that lands it back is the
  route. The UI fallback, only if no by-name order lands the item, is one
  held click on the item's cell (a quick move) or a drag.
- **One-unit move by name.** Stash to bag is the partial take `craftmats`
  already ships (edit the stash stack's count with the game's hash refresh,
  then create one unit in the bag by the json route). Bag to stash is its
  mirror: the owner splits one unit by hand once with the split routine and
  the rows armed, then the operator creates one unit into the stash's
  Materials grid by the json route and removes the bag's unit in the logged
  order. The UI fallback is the split dialog, reached by whichever input
  opens it, with `1` typed and its confirm row clicked.
- **Give item.** The json copy route: build a save struct from a template
  item the maps already hold, set its count, stamp a fresh key, load it with
  the game's own loader, add it to the destination's map (0 bag, 9 stash) and
  place it into the destination grid, all with self the save-console
  instance. Proven for the bag and Cube grids in #14; the stash grid is the
  open question.
- **Close.** `Esc`, else the stash's close button row; the proof is that no
  `UI_Stash_obj` is listed and the stash save ran once. Never by destroying
  the window.

## Live procedure

Two research launches measure the mechanisms, and a third, on the player
build, verifies the hub tools. Live procedure 1 ran on 2026-09-25 and is kept
as it was written, so § Results can be read against it; Live procedure 2 is
the second research session. The third, Live procedure 3, is the
verification session on the player build; it ran on 2026-09-26 against the
Step 9 player build and is written in the toolkit's own workorder context
(`.claude/workorders/hs-drive-stash-bag-actions-context.md` §
"Live procedure 3 (verification, player build)"), not in this document -
its results are below in § Results and § Decision.

### Live procedure 1

Operator-run through the hub's `hs-drive` tools, one launch, the research
(dev) build of this branch (`plugin_build\BloodPactPlugin_rel.dll`, its
SHA-256 named in the dispatch). The owner installs it; the game lease records
the installed DLL's hash. The person is needed only for the two hand moves
that arm the by-name replay, batched into one hand-back. The launch may be
shared with the skill-actions research (toolkit workorder
`hs-drive-skill-actions`), stash checks first; each keeps its own capture.
Every `craftprobe` form is used as `crafting-materials-research.md`
§ Instrument documents it (plus this branch's additions in § Instrument
above), and every reply is captured verbatim. Every by-name call below
follows § Instrument's recording rule: its capture line quotes the logged
armed line and the call's own reply, and its outcome is one of
`reproduced`, `shape not reproduced (…)` or `not-run (instrument: …)`.

**Character.** Save slot 14. It needs one non-stackable junk item in the
bag's main grid and one material stack of at least two in the stash's
Materials tab. P0-1 reads both and creates whichever is missing by name;
neither is assumed, and neither is asked of the owner. Cases: the junk item
bag to stash to bag (the ordinary case: grid to grid), and one material unit
stash to bag to stash (the outlier: a stack, the split). No others.

- **dll-hash.** The lease's DLL hash equals the dispatch's research-build
  hash.
- **marker.** A bare `craftprobe` answers `craftprobe: phase1k rows=285 - …`
  (#14's 282 rows plus this branch's three); `rows=282` is a build without
  them, and fails this check.
- **control.** The positive control of § Instrument. With `mapkeep on`
  (it holds two rows) and neither Custom Forge entries nor an Item Truth
  request, `craftprobe hook` answers `283 detoured, 0 failed, 2 held by
  mapkeep, craftmats or an item hook` and prints `craftprobe hook: detoured
  CreateItemNew at exe+0x… (under table-only bp_citemn)`. With Custom Forge
  entries loaded or Item Truth requested it answers `282 detoured, 0 failed,
  3 held …`, and P0-1's `CreateItemNew` check is `not-run (instrument: held
  by <install>)`.
- **Before launch.** No forged-item sidecar entries and no Item Truth
  request for this launch; the capture records which state the session was
  in. A manual copy of the saves plus `hs_saves_backup`;
  `py -3 tools/stash_tab_counts.py` in the toolkit, recording the Materials
  rows (expected: at least one material-tab stack of two or more).
  `hs_launch` to `plugin_ready`; `hs_select_character(14)` to
  `character_loaded`.
- **P0-1, prerequisites by name.** `craftprobe mapkeep on`, then
  `craftprobe hook`. `craftprobe node bag`: expected at least one occupied
  bag cell whose item is not a material (record its fingerprint `K_J` and
  cell). `craftprobe stash` with the stash closed: expected the Materials
  container with a stack of two or more (record `K_M` and its `o`). Whatever
  is missing is created by the json copy route from a template of the wanted
  class the maps hold (`mapkeep find` for a material; any non-material key
  from `node bag` or `node stash` for the junk item): the save struct from
  the template, its `o` set to 2 for the material or 1 for the junk item, the
  timestamp, the loader, the map add on map 0 or 9, and the grid add into
  the destination grid as `node bag` or `node stash` names it (or the stash
  grid add for the stash), each by `craftprobe call … confirm` with self the
  save-console instance. Expected: the new key placed with the asked `o`, and
  the hash-check and client-report rows still at zero calls. Record which
  order placed into each destination as `giveItemRoute`; if nothing places
  into the stash grid, record `stash: not-observed`, and a missing material
  stack is then a `fail` for this check, not a hand-back. Before the first
  loader call, arm the `CreateItemNew` row (`craftprobe arm budget=3
  CreateItemNew`): the loader reaches it, so its count must be at least 1
  after that call, and its armed line (self, other, arguments) is recorded
  as its shape. A count of 0, or the row `held` or `failed`, is `not-run
  (instrument: …)` for that shape, never `not-observed`.
- **P0-2, warp.** `menulayout Town_Stash_obj` (one row, a finite `gui=`;
  record it and the player's). `iset x` to the stash's x and `iset y` to its
  y plus 48 (each answers the new value), then `menulayout Player_obj`:
  expected within 2 px of the target, and a screenshot shows the character
  beside the stash. If the game moved the character back, retry once at
  plus 96. Record the offset that held as `warpRoute`.
- **P0-3, open.** Arm the stash handler and `UiCreate` (`craftprobe arm
  budget=3 anon@663 UiCreate`; the substrings also arm the stash window's
  own closure and `UiCreateNode`, which is harmless). (b) first, because it
  logs the shapes the by-name routes replay: `hs_input` key 69 held 120 ms -
  expected `menulayout UI_Stash_obj` lists one visible row (`interact`), with
  both rows' armed lines quoted (self, other, arguments). The handler row at
  0 calls while the window opened is recorded as that (the key reached the
  window another way), not as a failure. Close with `Esc` (key 27; a first
  close observation for P0-9). (a) `craftprobe methods Town_Stash_obj 0` -
  expected a variable (or a plain-struct member one level down) whose method
  wraps the stash handler; then `craftprobe callm Town_Stash_obj 0 inst
  <that variable>`, with `other:<id>` and the arguments as logged, and
  `confirm` (or the `path:` holder for a struct member) - expected listed
  (`closure`). No holder found is `not-run (instrument: no holder of
  anon@663)`, quoting the `methods` output. `Esc`. (c) `craftprobe call
  UiCreate` with the self, `other:<id>` and arguments as logged, and
  `confirm` - expected listed (`uicreate`); a logged argument no `call` form
  can supply (a struct or method that `path:` or `kept:` cannot reach) is
  `shape not reproduced`. `stashOpenRoute` records every route that listed
  the window, with its shape. Leave the stash open (by the preferred route)
  for P0-4.
  Fixture: the full `menulayout` on the open stash, plus the listings of
  `UI_Button_Stash_Tab_obj`, `UI_Button_Inventory_Tab_obj`,
  `UI_Inventory_Grid_obj`, `UI_Inventory_Drag_obj` and
  `UI_Stash_Socket_New_obj`, with `listed=` and `capped=`.
- **P0-4, reads.** `citrace dumpobj UI_Stash_obj 0` (expected:
  `stashTabSelected` and `tabSelected` present, 0 on the personal tab; the
  active node, stash grid, materials-tab and container references present);
  dumps of stash tab buttons 0 to 2 and bag tab button 0 (which field carries
  the tab number: `stashTabRule`, `bagTabRule`); the stash's grid and the
  bag's `InventoryGrid` (`cellRule`: the node's `x`, `y` and `bbox` against
  its width, height and scale); `New_Inventory_Data_obj 0` (`localItemMap` a
  ds_map); `craftprobe node bag` and `node stash` (per cell: fingerprint and
  `o`). Count control: `cb ds_map_find_value` on `localItemMap` with `K_J`,
  then `o` - expected equal to `node bag`'s `o` for `K_J`; `not-observed` if
  `cb` cannot address the map. Record every field of one occupied cell, and
  whether a stash cell references its item struct.
- **P0-5, stash tab by name.** `craftprobe arm budget=3 TabClick`: the
  substring arms every tab handler row (the stash's two, the bag's three),
  because which one a Materials click runs is not recorded. Log the shape by
  the UI route first: click the Materials row's `win` (`hs_input` click,
  held 120 ms) - expected `menulayout UI_Stash_obj` shows
  `stashTabSelected=-4`, the screenshot shows Materials, and the armed line
  names the handler that ran with its self (a stash tab button), other and
  arguments (`stashTabRoute` gains `click`). Click the personal row back
  (expected 0). Then by name with that shape: `craftprobe call` of the
  handler row the click logged, with `id:<the logged self>`, `other:<the
  logged other>`, the arguments as logged and `confirm` - expected
  `stashTabSelected=-4` again (`stashTabRoute: byname`, outcome
  `reproduced`). **Control**: Socketable, by name with the Socketable row's
  button as self (listed `intended:` in the capture) and the logged other
  and arguments, or by click if the by-name call did not change the state -
  expected -2 in the `menulayout UI_Stash_obj` row **and** in `citrace
  dumpobj UI_Stash_obj 0`; a different value or an absent field is
  instrument-blind for this check. Back to the personal tab (0). Fixture:
  the full listing with Materials selected.
- **P0-6, bag tab by name** (the tab handler rows still armed from P0-5;
  `craftprobe arm budget=3 TabClick` again if their budget is spent). Dump
  `New_Inventory_Data_obj 0` and `UI_Inventory_obj 0`; click the bag's
  materials tab row `win`; dump both again. Expected: exactly one member
  changed on one of them (`bagTabState`), the screenshot shows the bag's
  materials tab, and the armed line gives the handler's self, other and
  arguments (`bagTabRoute` gains `click`). Click the bag's main tab row back
  (the member returns). Then by name with the logged shape (`craftprobe
  call` of that handler row, `id:<logged self>`, `other:<logged other>`, the
  arguments as logged, `confirm`) - expected the same member change
  (`bagTabRoute: byname`). Back to the main tab by click. Record the bag tab
  names listed.
- **P0-7, whole-stack move** (personal tab selected; the grid, stash, map and
  swap routines armed with `craftprobe arm`). One hand-back, two actions:
  "1. drag the junk item from the bag into an empty cell of the stash's
  personal tab; 2. switch to the Materials tab and move exactly one unit of
  the material stack into the bag (shift-click or right-click the stack,
  type 1 in the split dialog, confirm); reply when both are done". Then
  `craftprobe show`: the logged lines for both actions, in call order, are
  the two shapes. Expected for action 1: an order of grid calls that removes
  `K_J` from the bag grid and adds it to the stash grid; for action 2: the
  split routine and the calls after it. Replay action 1 in the other
  direction by `craftprobe call … confirm` in the logged order - self,
  `other:<id>` and arguments as logged, the `fp:`/`fp9:` forms for the item,
  a closure through `methods` and `callm … inst`; the swapped source and
  destination grids are listed `intended:` in the capture before the first
  call. Expected: `node bag` shows `K_J` back and `node stash` no longer
  does - that order is `moveWholeRoute`. If a call is refused, record which,
  with both shapes and its outcome under the recording rule, and try the
  alternative add routines the reading names (`InventoryGridAddItemToTab`,
  `StashGridAddItem`); if none lands it, `moveWholeRoute: click`, with the
  UI route taken from P0-4's fixtures and the by-name part recorded with its
  outcome (a `reproduced` refusal is a measured negative; `shape not
  reproduced` and `not-run` are not). Fixture: the after-listing.
- **P0-8, one unit by name** (bag to stash, the mirror of `craftmats`' take;
  after P0-7 the bag holds exactly one unit of the material). Create one
  unit of `K_M` into the stash's Materials grid by the json route (map 9,
  the grid as `node stash` names it), then remove the bag's unit in action
  2's logged order, reversed. Expected: `node stash` shows `K_M`'s `o` back
  to its P0-1 value and `node bag` no longer holds the unit
  (`moveOneRoute: byname`, with the order); otherwise record the refusal
  with both shapes and its outcome under the recording rule, and
  `moveOneRoute: dialog`, quoting the input that opened the dialog. Stash to
  bag for one unit is `craftmats`' proven route and is not re-measured.
  Record how the count was read on each side (`countReader`).
- **P0-9, close.** `hs_input` key 27. Expected: `menulayout UI_Stash_obj`
  lists nothing and the save rows show one call each
  (`stashCloseRoute: esc`). If still listed, click the close button row
  nearest the stash window's origin (`stashCloseRoute: close-row`).
- **P0-10, stop and restore.** `hs_stop_game` (`exited: true`,
  `forced: false`); `hs_saves_inspect` (`stash.hss` expected among the
  changed files); restore the pre-launch backup, since this session moved
  items, and inspect again: nothing changed or missing. Release the lease;
  the DLL hash after equals the hash before.

A `not-observed` on P0-3 (open), P0-5 (tab state), or on P0-7 or P0-8 where
both the by-name route and its hand-move fallback went unobserved, sends the
plan back for revision rather than into an implementation round. A single
by-name route going unobserved while its UI route was observed does not, nor
does a by-name route recorded `shape not reproduced` or `not-run
(instrument …)`: the § Decision line records the UI route, names the
by-name route as not tested with the logged shape (quoting that shape), and
the hub tool ships the UI route. A logged shape that was reproduced and that
the game refused is a measured negative, and is recorded as one, with both
shapes quoted.

### Live procedure 2

Measures by name what live 1 could not: the open (P2-2), the stash and bag
tab switches (P2-3, P2-4), the whole-stack and one-unit moves (P2-5, P2-6)
and the close (P2-7). It runs on the research build that carries the
`id:<n>`/`obj:<Name>` arguments and the `UiACloseButton` row (§ Instrument),
`plugin_build\BloodPactPlugin_rel.dll` as last built, its SHA-256 named in
the dispatch; the owner installs it and the lease records its hash. No hand
move: every move is by name. Nothing here depends on Live procedure 1 having
been re-run. Every by-name call follows § Instrument's recording rule (the
logged shape beside the supplied one; the outcome `reproduced`, `shape not
reproduced (…)` or `not-run (instrument: …)`), and a field a step changes on
purpose is listed `intended:` before the call.

**Note (2026-09-25, D11).** P2-5 and P2-6 below measured the whole-stack and
one-unit moves; the owner split that ground out to `hs-drive-stash-move-research`
after this session, so `moveWholeRoute` and `moveOneRoute` are recorded there
under § Decision as `moved to hs-drive-stash-move-research`, not as routes
this workorder ships. The steps below are left exactly as run.

**The capture ends with a `## Checks` block**: one line per check named
below, in this order, written `- <name> | expected: … | observed: … |
pass|fail|not-observed [note]`. Live 1's capture had none, so no tool could
read its verdicts.

**Character.** Save slot 14, the same prerequisites as live 1 (live 1 found
a class 18 non-stackable, `b=19`, in the bag's `PotionGrid`, and a class 14
material, `b=72`, `o=934`, in the stash's item map), read in P2-1 and created
by name only when missing. Cases: the junk item's whole stack bag to stash
to bag (the ordinary case), and one material unit stash to bag to stash (the
stack outlier). No others.

- **dll-hash.** The lease's DLL hash equals the dispatch's hash.
- **marker.** A bare `craftprobe` answers `craftprobe: phase1k rows=286 - …`;
  285 is a build without the `UiACloseButton` row and fails this check.
- **control.** `ping` answers `pong (YYTK 4.0.1)`; `menulayout Player_obj`
  prints `room=Town_01_rm` and one `obj=Player_obj` row with a finite `gui=`;
  `mapkeep on` **before** `craftprobe hook` (the reverse order is refused);
  `craftprobe hook` answers `<n> detoured, 0 failed, <h> held …` with n + h =
  286 and every held line naming its holder (283 and 3 with mapkeep and
  Custom Forge; 277 and 9 with `craftmats` on) - a `failed` count above 0
  fails the check; after the character load `craftprobe show` reports a
  non-zero `CheckPlayerInteraction calls=`.
- **Before launch.** A manual copy of the saves; `hs_saves_backup` labelled
  `hs-drive-stash-bag-actions-live-2` (keep its id); `py -3
  tools/stash_tab_counts.py` in the toolkit (record the Materials rows);
  `hs_launch` to `plugin_ready`; `hs_select_character(14)` to
  `character_loaded`.
- **P2-1, prerequisites.** `craftprobe node bag` (K_J: an occupied cell whose
  class is not 14; record its fingerprint, grid id and cell) and `craftprobe
  node var id:<New_Inventory_Data_obj id> localItemMap class=14` (K_M: `o`
  of two or more; record its fingerprint and `b`). Whichever is missing is
  created by the json route (the save struct, count, timestamp, loader, map
  add and grid add, each a `craftprobe call … confirm`), and that
  destination's `giveItemRoute` is recorded; otherwise `giveItemRoute` is
  left to P2-5's variant B.
- **P2-2, open by name.** Warp first: `menulayout Town_Stash_obj` (record
  `gui=`), `iset x` to its x, `iset y` to its y plus 48, `menulayout
  Player_obj` within 2 px, a screenshot (this re-checks `warpRoute`). Arm
  `craftprobe arm budget=3 UiCreate`, the `UiCreate` row alone: the closure
  `anon@663` runs every frame, so a budget armed on it is spent before any
  call that matters (live 1's was). (a) `craftprobe call UiCreate
  Town_Stash_obj 0 obj:UI_Stash_obj 1 1 confirm`, against live 1's logged
  `UiCreate` line: expected `menulayout UI_Stash_obj` lists one row
  (`stashOpenRoute: uicreate`, `reproduced`), and the reply's first argument
  reads `ref object UI_Stash_obj`. Close by the `UI_Button_Close_obj` row's
  click (live 1's route) and confirm the window is unlisted. (b) With no key
  down, `craftprobe show` twice, a second apart: record the `anon@663` row's
  two `calls=` counts (rising counts are its per-frame caller). `craftprobe
  methods Town_Stash_obj 0` (expected `m_TalkToNPC` wrapping `anon@663`),
  then `craftprobe callm Town_Stash_obj 0 inst m_TalkToNPC id:<player id>
  confirm`, against live 1's logged closure line - a per-frame call, not the
  call at a key press, which live 1 could not log: expected listed
  (`closure`), with the armed `UiCreate` line showing (a)'s shape. If nothing
  is listed, the outcome is `shape not reproduced (key state: the replay
  supplies the per-frame shape; the closure's call at the press, with the key
  down, has never been logged)`, never a measured refusal of the closure
  route (record the reason if the reply names one). `stashOpenRoute` then
  records whichever route listed, preferring `uicreate`, then `closure`, then
  `interact` (key `F`: `hs_input` vk 70 held 120 ms). Leave the stash open by
  the preferred route. Fixture: `menulayout` of `UI_Stash_obj`,
  `UI_Button_Stash_Tab_obj`, `UI_Button_Inventory_Tab_obj`,
  `UI_Button_Inventory_Tab_Small_obj`, `UI_Inventory_Grid_obj` and
  `UI_Button_Close_obj`.
- **P2-3, stash tab by name.** `craftprobe methods id:<Materials button id>`
  and `craftprobe methods id:<Shared1 button id>`: the variable holding each
  button's activation method and the row it names (expected
  `UiAStashTabClick`, or `UiAStashMaterialTabClick` for Materials). Arm
  `craftprobe arm budget=3 TabClick`. Log the shape on a tab that is on
  screen: click Shared1's `win` (visible at the strip's default scroll) -
  `menulayout UI_Stash_obj` reads `stashTabSelected=1`, and the armed line
  gives self (that button), other, argument count and every argument; click
  the Personal tab's `win` back (0). Replay for Materials: `craftprobe call`
  of the row Materials' method names, with `id:<Materials button id>`,
  `other:<the logged other's id>` and the logged arguments - -4 where the
  logged shape carried 1, a logged `ref instance` as `id:<n>`, a logged `ref
  object` as `obj:<Name>` - and `confirm` (intended: the button and the tab
  number). Expected `stashTabSelected=-4` and the screenshot shows Materials
  (`stashTabRoute: byname`, with the shape). Control: Socketable the same
  way - -2 in the `menulayout UI_Stash_obj` row **and** in `citrace dumpobj
  UI_Stash_obj 0`; a different value or an absent field is instrument-blind
  for this check. Back to 0 by name. `craftprobe var id:<UI_Stash_obj id>
  cycleTabsRightButton` (record the object's name; do not click it).
  Fixture: `menulayout` of `UI_Stash_obj` and `UI_Button_Stash_Tab_obj` with
  Materials selected.
- **P2-4, bag tab by name.** `craftprobe var id:<UI_Stash_obj id>
  invMaterialTab` and `invSocketTab` give their ids; `menulayout
  UI_Button_Inventory_Tab_Small_obj` shows whether those ids are its rows and
  which field tells them apart (`bagTabRule`: `uiNodeCallstack`, `tabNumber`
  or `text`); `craftprobe methods id:<invMaterialTab id>` names its handler
  row (expected `UiAInventoryMaterialTabClick`). Dump `citrace dumpobj
  New_Inventory_Data_obj 0` and `citrace dumpobj UI_Stash_obj 0`, click the
  bag's materials sub-tab's `win` (the bag's strip is not scrolled; the
  handler rows are armed from P2-3, re-armed if spent), and dump both again:
  exactly one member changed on one of them is `bagTabState`, and the armed
  line is the shape; click the bag's main tab back. Replay: `craftprobe call
  <handler row> id:<invMaterialTab id> other:<logged other id> <arguments as
  logged> confirm` - expected the same member change (`bagTabRoute:
  byname`); back by name with the logged main-tab shape, else by a click.
  Record the bag's tab set: the page tabs by `tabNumber`
  (`UI_Button_Inventory_Tab_obj`), the sub-tabs by `bagTabRule`.
- **P2-5, whole-stack move by name** (Personal tab selected; no hand move).
  The cell arrays first: `craftprobe find New_Inventory_Data_obj 0 <K_J>`
  and `craftprobe find id:<bag InventoryGrid id> <K_J>` (the bag's),
  `craftprobe find New_Inventory_Data_obj 0 <K_M>`, `craftprobe find
  UI_Stash_obj 0 <K_M>`, `craftprobe find id:<stash grid id> <K_M>` and
  `craftprobe store <K_M>` (the stash tab's; `find` takes `<Obj> <nth>` or
  `id:<n>`, never `global` - `store` searches the globals). Record every path
  whose leaf is `nodeFingerprint`, and dump one node (`craftprobe var <that
  path without the leaf> *`): its fields are `cellRule` (the fingerprint,
  and the count field if the node carries one - `countReader`'s node
  answer). Arm `craftprobe arm budget=4 ChangeItemOwner GridAddItem
  StashGridAddItem GridRemoveItem InvGridClearItemNode AddItemToMap
  RemoveItemFromMap s_InvNode ReportClient ItemCheckHash`.
  **Variant A**, the owner change, keeping the key: `craftprobe call
  ChangeItemOwner id:<bag InventoryGrid id> 0 9 fp:<K_J> confirm` (logged:
  #14's record, self the bag grid, `a0=int64:9 a1=int64:0 a2=<fp>`;
  intended: 0 to 9), then `craftprobe call GridAddItem Console_Save_obj 0
  path:<the Personal tab's cell array> fp9:<K_J> 0 undefined confirm`
  (logged: the `craftmats` shape - cell array, item, 0, `undefined`;
  intended: the stash's array and map 9), expected a returned struct with
  `success=true`; then `craftprobe call GridRemoveItem Console_Save_obj 0
  path:<the bag's cell array> <K_J as text> confirm` (the `craftmats` take's
  shape). Proof: `craftprobe node id:<stash grid id>` shows K_J and
  `craftprobe node bag` no longer does; record the `ReportClient` and
  `ItemCheckHash` calls. A `success=false` or a refused `ChangeItemOwner` is
  recorded with both shapes, and `StashGridAddItem` with the same arguments
  (intended) is tried once.
  **Variant B**, the json copy with a new key (also the stash side of
  `giveItemRoute`): `CreateItemSaveStruct` from `fp:<K_J>`, `LootTimestamp`,
  `InitItemFromJson` with the two kept returns, `AddItemToMap` into `map9`
  with the kept key and item, `GridAddItem` into the stash's cell array with
  the kept item, 0 and `undefined` - each a `craftprobe call …
  Console_Save_obj 0 … confirm` - then `RemoveItemFromMap` and
  `GridRemoveItem` of the original from map 0 and the bag's cells. B runs
  only if A did not land; when A landed, B's five creating calls run once
  anyway with `fp9:<K_M>` as the template and `set kept:CreateItemSaveStruct
  o 1 confirm` before the loader, into the Materials tab's array, as the
  `giveItemRoute: stash` measurement, and that unit is removed again
  (`RemoveItemFromMap` on map 9, then `GridRemoveItem`). Reverse the move
  that landed (stash to bag, the mirrored order) so the save is left as
  found. `moveWholeRoute` is the order that landed, every shape quoted.
  Fixture: `craftprobe node bag` after the move.
- **P2-6, one unit by name.** (i) Stash to bag, the `craftmats` inline route
  through `craftprobe`: `set fp9:<K_M>.itemDefinitionStruct o <o-1>
  confirm`, `call ItemCheckHash Console_Save_obj 0 fp9:<K_M> confirm`, then
  B's creating calls with `set kept:CreateItemSaveStruct o 1 confirm` and
  the bag's preferred grid (`call GetItemPreferredGrid Console_Save_obj 0 0
  kept:InitItemFromJson confirm`, whose kept return is `GridAddItem`'s first
  argument). Expected: `node bag` shows one unit of K_M's kind, and `node
  var … localItemMap class=14` shows `o-1`. (ii) Bag to stash, the mirror and
  this check's measurement: `RemoveItemFromMap` on map 0 and `GridRemoveItem`
  of that unit from the bag's cells, then `set fp9:<K_M>.itemDefinitionStruct
  o <o> confirm` and `ItemCheckHash`, restoring the stack. Expected: P2-1's
  counts on both sides. If the unit should merge through the game's own
  stack routine instead, `StashAddToStack` with #14's logged six-argument
  shape (an array, 9, 2, the item struct, 1, the tab) is the second variant,
  tried once. `moveOneRoute` is both legs, with their orders.
- **P2-7, close by name.** `craftprobe arm budget=2 UiACloseButton`;
  `menulayout UI_Button_Close_obj` gives the stash's row; click it once -
  the logged shape (self the button, other, arguments), the window
  unlisted and `SaveStash` one call (live 1's route, re-checked). Reopen by
  P2-2's route. `craftprobe call UiACloseButton id:<close button id>
  other:<logged other id> <arguments as logged> confirm` - expected unlisted
  and the save rows one call each (`stashCloseRoute: byname`, with the
  shape); otherwise it stays `close-row` and the by-name result is recorded
  under the recording rule.
- **P2-8, stop.** `hs_stop_game` (`exited: true`, `forced: false`);
  `hs_saves_inspect` (record `changed`; `stash.hss` expected among them);
  `hs_lease_release` (the DLL hash after equals the hash before). The
  operator has no restore tool; the driver restores the
  `hs-drive-stash-bag-actions-live-2` backup on the owner's word and records
  it.

Routing: P2-2 with neither by-name route `reproduced` and the key route not
observed, P2-3 `not-observed`, or both P2-5 and P2-6 with no by-name order
landing the item, sends the plan back for revision. A by-name route recorded
`reproduced` and refused by the game is a measured negative: the § Decision
line records the route that did work, and the hub tool ships it. A single
move variant refused while the other landed is a § Decision line, not a
revision. `shape not reproduced` and `not-run (instrument …)` are never a
route negative.

## Results

One dated row per check, filled from the live capture: live 1's rows from
the toolkit's `.claude/workorders/hs-drive-stash-bag-actions-live-1.md`
(2026-09-25, research DLL SHA-256 `71e9fc54…dee6c`, ForgePact `c2f3805`, save
slot 14, a launch shared with the skill research), the rows marked live 2
from its own capture once it has run. For a by-name call, *Logged shape* is
the self, other, argument count and arguments of the game's own call (its
armed line) and *Supplied shape* those of the replay (its reply line), with
the outcome under § Instrument's recording rule in *Observation*; `-` where a
check makes no by-name call.

| Check | Observation | Logged shape | Supplied shape | Control | Date |
| --- | --- | --- | --- | --- | --- |
| dll-hash | pass: the lease's `dll_sha256` `71e9fc54…dee6c` equals the dispatch's research-build hash, and is the same at the lease's release | - | - | - | 2026-09-25 |
| marker | pass: a bare `craftprobe` answered `craftprobe: phase1k rows=285 - research instrument …` | - | - | - | 2026-09-25 |
| control | pass: `ping` answered `pong (YYTK 4.0.1)`; `menulayout Player_obj` printed `room=Town_01_rm` and one `obj=Player_obj` row, `gui=912.0,822.0`; `mapkeep on` reported both rows both-routes, then `craftprobe hook` answered `282 detoured, 0 failed, 3 held by mapkeep, craftmats or an item hook` (mapkeep's two rows, and `CreateItemNew` held by custom forge: its inline detour is installed at plugin start on every launch of this build, with no forged-item entries and no Item Truth request). A first launch ran `craftprobe hook` before `mapkeep on`, which then refused; that launch was stopped with nothing touched and relaunched in the right order | - | - | `CheckPlayerInteraction calls=31080` after the load | 2026-09-25 |
| P0-1 | pass, nothing created: K_J `0-0-209492724983-18` (class 18, `b=19`, no count member) in the bag's `PotionGrid` grid, instance 257745; K_M `0-0-209564349884-14` (class 14, `b=72`, `o=934`), read with `craftprobe node var id:<New_Inventory_Data_obj> localItemMap class=14` because the kept stash map was not current with the stash closed. Both existed, so the give-item route was not exercised. `CreateItemNew`'s shape: `not-run (instrument: held by custom forge, inline detour at plugin start)` | - (no call made) | - (no call made) | - | 2026-09-25 |
| P0-2 | pass: `Town_Stash_obj` `gui=884.0,580.0`; `iset x 884` and `iset y 628` each answered the new value; `Player_obj` then read `gui=884.0,628.0`, 0 px from the target, no collision and no retry; the screenshot shows the character beside the stash's `[F] USE` prompt | - | - | - | 2026-09-25 |
| P0-3 | (b) interact: key `E` (vk 69) left `UI_Stash_obj` unlisted, key `F` (vk 70, 120 ms) listed it (`stashTabSelected=0`), and `Esc` then closed it. `UiCreate` logged one call on the key. Whether that call came through the closure `anon@663` (held in `m_TalkToNPC`, `craftprobe methods Town_Stash_obj 0`) is not observed: the closure runs every frame (the later by-name call entered it as call #9721), and its armed budget of three log lines was spent on those per-frame calls (self = other = the stash, `argc=1 a0=ref instance <player>`) before the key was pressed, so a call at the press could not have been logged. The static reading has the closure as the interaction handler that ends in `UiCreate`. (a) closure: `shape not reproduced (argc: logged 1 with a0=ref instance <player>, supplied 0 - callm had no argument form for a live instance)`, `script_execute threw`. (c) `UiCreate`: `shape not reproduced (a0: logged ref object UI_Stash_obj, supplied string "UI_Stash_obj" - call had no argument form for an object reference)`, `script_execute threw`. Only the key listed the window. Fixture listed 23 stash tab rows, 5 bag page tab rows, 6 grids (the stash's `StashGrid` 17 by 18, the bag's `InventoryGrid` 15 by 6), 2 drag objects and no Socketable container | `UiCreate #1 self=Town_Stash_obj#4852@228465 other=Town_Stash_obj#4852@228465 argc=3 a0=kind=15 str=ref object UI_Stash_obj a1=real:1.000000 a2=real:1.000000`, `ret=kind=15 str=ref instance 263555`; `anon@663` (its three per-frame calls before the press, the only lines its budget allowed): self = other = the same stash, `argc=1 a0=ref instance 261723` (the player) | (a) `callm Town_Stash_obj 0 inst m_TalkToNPC confirm`, `argc=0`; (c) `call UiCreate Town_Stash_obj 0 UI_Stash_obj 1 1 confirm`, `a0=string:"UI_Stash_obj"` | - | 2026-09-25 |
| P0-4 | partly observed: `citrace dumpobj UI_Stash_obj 0` - 125 variables; `stashTabSelected` and `tabSelected` both 0 on Personal; `activeNode` = `stashGrid` = the stash grid instance; `invMaterialTab`, `invSocketTab`, `uiStashContainer`, an `invTab` array of the five bag page tabs, and the closures `m_UpdateStashContainer`, `m_StashSetGrid`, `m_UpdateInventoryGrid`. not-observed (not run, to keep the session's time for its hand moves): the tab button dumps, the grid dumps, `New_Inventory_Data_obj`'s dump and the count control | - | - | not-observed: the `cb ds_map_find_value` count control was not run | 2026-09-25 |
| P0-5 | not-observed: two clicks - at the Materials row's `win=908,71`, and near the stash header's list icon - reached none of the five armed tab-handler rows (0 calls each). The strip is scrolled and clipped by `UI_Stash_Tab_Bar_Container_obj`, and `menulayout` prints the child button's own `win=`, which lies past the panel's visible edge; every `UI_Button_Stash_Tab_obj` row reads `visible=0`, the drawn Personal tab included, so that field does not follow what is drawn. An instrument and coordinate gap, not a game negative. By name: `not-run (instrument: no logged shape - the click reached no handler)`. The Materials value was read later (P0-9: -4, after the owner's hand switch) | - (no handler ran) | - | not-observed: the Socketable control was not run | 2026-09-25 |
| P0-6 | not-observed: not attempted. The bag's page tabs are `UI_Button_Inventory_Tab_obj` rows with `tabNumber` 0 to 4 (Main and four Extra); its materials and socket sub-tabs are the instances `UI_Stash_obj.invMaterialTab` and `invSocketTab` name | - | - | - | 2026-09-25 |
| P0-7 | UI drag only. The owner dragged a non-stackable (not K_J) from the bag into the stash's Personal tab, then split one unit of a material (`b=65`, not K_M) from the Materials tab into the bag. The twelve armed grid and map rows stayed at 0 calls, with the control climbing; the only item writes logged were two `s_InvNode` calls and one `UiASplitStack`. By name: `not-run (instrument: s_InvNode's self is a struct under construction, which call and callm cannot name)` | `s_InvNode #1 self=(not an instance: object/struct object_index=undefined) other=UI_Inventory_Grid_obj#5112@264143 argc=3 a0=real:5.000000 a1=real:6.000000 a2=struct{… item, b=29 …}`; `UiASplitStack #1 self=UI_Button_Small_obj#5009@267236 other=UI_Split_Stack_obj#5254@267235 argc=1 a0=array len=0`; `s_InvNode #2` self as #1, `other=UI_Inventory_Grid_obj#5112@264099 argc=3 a0=real:0.000000 a1=real:0.000000 a2=struct{… item, b=65, o=1 …}` | - (no call made) | - | 2026-09-25 |
| P0-8 | not-observed: not attempted. No order to reverse was observed (none of the armed named grid routines was seen on the split), and the json route was left for the session's time | - | - | - | 2026-09-25 |
| P0-9 | pass, by the close row: `Esc` did nothing with the Materials tab on show (`stashTabSelected=-4`, still listed; earlier, straight after opening on Personal, `Esc` had closed it). The `UI_Button_Close_obj` row (`uiNodeCallstack=InventoryClose`, `win=1195,27` under 1920x1080, `visible=1`) clicked once: `UI_Stash_obj` unlisted, `SaveStash calls=1`, and one call each of `SaveCommit` and `SaveFileGMAsync` for the close | - | - | - | 2026-09-25 |
| P0-10 | partly observed: `hs_stop_game` answered `exited: true`, `forced: false`; `hs_saves_inspect` changed `herosiege13.hss`, `inventory_order_13.hss`, `shop.ini` and `stash.hss`, nothing added or missing. The operator could not restore (its toolset has no restore tool), and the lease's release says so; the driver restored the backup afterwards on the owner's word. The DLL hash after equals the hash before | - | - | - | 2026-09-25 |
| dll-hash (live 2) | pass: the lease's `dll_sha256` `ea3f38f5…92fa` (ForgePact `01510ed`) equalled the dispatch's research-build hash, and was the same at the lease's release | - | - | - | 2026-09-25 |
| marker (live 2) | pass: a bare `craftprobe` answered `craftprobe: phase1k rows=286 - research instrument …` | - | - | - | 2026-09-25 |
| control (live 2) | pass: `ping` answered `pong (YYTK 4.0.1)`; `menulayout Player_obj` printed one row, `gui=912.0,822.0`; `mapkeep on` ran before `craftprobe hook`, which answered `283 detoured, 0 failed, 3 held by mapkeep, craftmats or an item hook` (283+3=286, matching P0-1's held set - `GetItemMap`/`LoadStash` by mapkeep, `CreateItemNew` by Custom Forge's inline detour) | - | - | `CheckPlayerInteraction calls=8820` after the load, climbing to 103320+ | 2026-09-25 |
| P2-1 | pass: K_J `0-0-209492724983-18` (class 18, `b=19`, cells=2) in the bag's `PotionGrid` (`id=258308`), matching live 1 exactly; K_M `0-0-209564349884-14` (class 14, `b=72`, `def.o=934`) in `New_Inventory_Data_obj.localItemMap` (`id=258288`), matching live 1 exactly. Both already present; `giveItemRoute` left to P2-5 variant B | - (no call made) | - (no call made) | - | 2026-09-25 |
| P2-2 | pass: warp landed 0 px off (`gui=884.0,628.0`); the F key (vk 70, 120 ms) listed `UI_Stash_obj` (`id=262983`, `stashTabSelected=0`), no crash this attempt. The logged `UiCreate` shape on the key matches live 1's P0-3 line and attempt 1's crashed by-name call exactly. The by-name `UiCreate` open crashed the game once in attempt 1 (`ret=ref instance 262616`), causality not established from one session; not re-called this attempt (`stashOpenRoute: interact`) | `UiCreate #1 self=other=Town_Stash_obj#4852@228465 argc=3 a0=ref object UI_Stash_obj a1=1 a2=1`, `ret=ref instance 262983` (this attempt); attempt 1's crashed call had the same shape, `ret=ref instance 262616` | - (by-name not re-called this attempt) | - | 2026-09-25 |
| P2-3 | pass: Materials button `id=263032` `activationArgs=[-4,<ref>]` `tabNumber=-4`; Shared1 `id=263035` `activationArgs=[1,<ref>]` `tabNumber=1`. `craftprobe methods` named Materials' handler `UiAStashMaterialTabClick` and Shared1's `UiAStashTabClick`, hook-free. A click on Shared1 (recalibrated to ~1.5x the reported `win=`; the strip's scroll/clip transform is not reflected in `win=`, matching P0-5) logged the shape; the by-name replay of Materials reproduced `stashTabSelected=-4` and the grid contents (`stashTabRoute: byname`) | `UiAStashTabClick #1 self=UI_Button_Stash_Tab_obj#5010@263035 other=UI_Stash_Tab_Bar_Container_obj#5260@263030 argc=1 a0=array len=2 ([1,<ref>])`, `ret=undefined` | `call UiAStashMaterialTabClick id:263032 other:263030 -4 id:263032 confirm`, `argc=2 a0=-4 a1=ref instance 263032` | `-2` in both `menulayout UI_Stash_obj` and `citrace dumpobj UI_Stash_obj 0` (Socketable, replayed through `callm id:263031 inst activationFunc` - its `activationFunc` is a closure, not a named script) | 2026-09-25 |
| P2-4 | pass: `UI_Stash_obj.invMaterialTab`=263017, `.invSocketTab`=263016; `menulayout UI_Button_Inventory_Tab_Small_obj` gave 7 rows keyed by `uiNodeCallstack` (`bagTabRule`; `text` empty, no `tabNumber`); `craftprobe methods id:263017` named `UiAInventoryMaterialTabClick`. The raw `win=` missed the row (landed on the page tab `UI_Button_Inventory_Tab_obj` above it, a miss reversed by clicking back); `win=` + (54,50) hit it. The click changed `tabSelected` 1 -> -4 (`bagTabState`; the 1 was left by the page-tab miss - the two `citrace dumpobj UI_Stash_obj` replies before the click read it in the session's `bp_ipc\out.txt`; the capture's "0 -> -4" is its summary) and `activeNode` to 263017. The by-name replay read `tabSelected=-4` after it (`bagTabRoute: byname`), with no read between the Socket click and the call, so the switch by name is not separated from no change here; `UI_Stash_obj.activeNode` stayed at the previous sub-tab's id (263016) - not observed to follow; recorded, not a fail | `UiAInventoryMaterialTabClick #1 self=UI_Button_Inventory_Tab_Small_obj#4989@263017 other=UI_Stash_obj#5257@262983 argc=1 a0=array len=0`, `ret=undefined` | `call UiAInventoryMaterialTabClick id:263017 other:262983 confirm`, `argc=0` | - | 2026-09-25 |
| P2-5 | fail (reproduced refusals, recorded under the recording rule - not a game negative once re-read against #14; moved to `hs-drive-stash-move-research`): variant A - `ChangeItemOwner id:258308 0 9 fp:<K_J>` dispatched (`ret=undefined`, its normal answer), then the `fp9:<K_J>` lookup for `GridAddItem` with self `Console_Save_obj` refused ("returned no item struct"); no read of map 0 or of the kept map 9 followed, so which map held K_J afterwards is not known; `StashGridAddItem` refused identically; the attempted owner change was reversed and `node bag` showed K_J unchanged. Variant B - `CreateItemSaveStruct` (5 members, no `o`: a non-stackable), `LootTimestamp` 212527295000, then `InitItemFromJson Console_Save_obj 0 kept:CreateItemSaveStruct kept:LootTimestamp` -> `undefined` - the second argument was the raw real where #14's proven shape is the key text `0-0-<S>-<class>` (`shape not reproduced (a1)`); `AddItemToMap` and `GridAddItem` inherited the `undefined` (the latter threw) | - (no prior by-name call logged to reproduce; #14's proven json-route key is the text `0-0-<S>-<class>`) | A: `ChangeItemOwner id:258308 0 9 fp:<K_J>` then `GridAddItem … fp9:<K_J> 0 undefined confirm` (refused); B: `CreateItemSaveStruct`, `LootTimestamp`, `InitItemFromJson … kept:CreateItemSaveStruct kept:LootTimestamp confirm` (a1 supplied as the raw real, not the key text) | - | 2026-09-25 |
| P2-6 | not-observed: `set fp9:<K_M>.itemDefinitionStruct o 933 confirm` refused ("returned no item struct") - the same map-9 lookup P2-5 hit. K_M was never in map 9: live 1 read it from `New_Inventory_Data_obj.localItemMap`, which #14 measured as owner 0's map (`map0-identity`); live 2 found it only in the bag's `inventoryMaterialGrid` (the bag's Materials sub-tab, #14) and resolved it through `fp:` (map 0) in the same session. So the "map-9 lookup limitation" is the lookup answering correctly about map-0 items; the prerequisite was mis-read, not the game. The full move sequence was not re-run given the identical, already-measured blocker; moved to `hs-drive-stash-move-research` | - | `set fp9:<K_M>.itemDefinitionStruct o 933 confirm` (refused) | `fp:<K_M>` (map 0) lookup with the same self resolved and wrote correctly (`set fp:<K_M>.itemDefinitionStruct o 933 confirm` -> `before=934 after=934`, reverted) | 2026-09-25 |
| P2-7 | pass: `UI_Button_Close_obj` row `id=263010`, `win=1195,27` (matches live 1); `craftprobe methods` named its handler `UiACloseButton`. The click closed the window (`listed=0`, `SaveStash calls=1`); reopened by the F key (new ids 268034/268061); the by-name replay closed the reopened window (`listed=0`, `SaveStash calls=2`, +1) (`stashCloseRoute` by-name reproduced) | `UiACloseButton #1 self=UI_Button_Close_obj#4982@263010 other=UI_Stash_obj#5257@262983 argc=1 a0=array len=0`, `ret=undefined` | `call UiACloseButton id:268061 other:268034 confirm`, `argc=0` | - | 2026-09-25 |
| P2-8 | pass: `hs_stop_game` answered `exited: true`, `forced: false`; `hs_saves_inspect` changed `herosiege13.hss`, `inventory_order_13.hss`, `shop.ini` and `stash.hss` (the shared skill workorder's K5 replays left a small residual Shadow Bolt allocation on the character save, beyond this session's own writes); the DLL hash after release equals the hash before. The operator could not restore (its toolset has no restore tool); the driver restored the backup afterwards on the owner's word | - | - | - | 2026-09-25 |
| dll-hash (live 3) | pass: the lease's `dll_sha256` `57aba60c…bffc` (the Step 9 player build) equalled the dispatch's hash at acquire and was unchanged (`dll_changed_since_taken: false`) at release | - | - | - | 2026-09-26 |
| control (live 3) | pass: main-menu `menulayout` listed `UI_Button_obj` "Play local" at `win=336,534` under `window=1920x1080`; `hs_select_character(slot=14)` reached `character_loaded`, proof "player via GetMyPlayer" | - | - | - | 2026-09-26 |
| V0 (live 3) | pass: `hs_give_item(to="bag", template=<K_M>, count=1)` -> `confirmed:true`, key `0-0-212584560001-14`, `before=3 after=4 o=1`; the post-V1 `menulayout` read listed no `cell=` row carrying the key; why is not established - one untested explanation is that the Materials sub-tab (where a material's preferred grid lands it, RDM §9.7) was not the active bag sub-tab at read time (Main was), which was not itself checked; V0 did not drag, save or reload the item, and the session's save backup was restored afterward; `hs_give_item(to="stash", ...)` refused `route_not_measured`, nothing sent (this workorder's own control on the bag route: `giveItemRoute`'s bag part) | - | - | - | 2026-09-26 |
| V0b (live 3) | research, never required: `hs_give_item(to="bag", template=<K_J> (class 18, non-stackable), count=1)` refused `give_refused` - "`GetItemPreferredGrid(1, item)` answered no grid; 0-0-212584570002-18 was taken out of map 0 again (craftmats' undo)" | - | - | - | 2026-09-26 |
| V1 (live 3) | pass: `hs_stash_open` -> `ok phase:stash_open route:interact`; `playerwarp` `before=912.0,822.0 after=884.0,628.0`; `menulayout UI_Stash_obj` listed=1, `stashTabSelected=0 tabSelected=0` | - | - | - | 2026-09-26 |
| V2 (live 3) | pass: `hs_stash_tab` materials `0 -> -4` (`UiAStashMaterialTabClick`), socketable `-4 -> -2` (the closure handler), personal `-2 -> 0` (`UiAStashTabClick`) - all three match the procedure's predicted `selected_after` values, including the closure-handler outlier | - | - | - | 2026-09-26 |
| V3 (live 3) | pass: `hs_bag_tab` materials `0 -> -4`, socket `-4 -> -2`, `activeNode` unchanged at 262324 through both (read, not proven); `vault` refused `route_not_measured`, nothing sent | - | - | - | 2026-09-26 |
| V4 (live 3) | pass (reason token differs from the procedure's prediction): `hs_stash_tab("materials", backup_id="no-such-backup")` refused `invalid_backup_id` (predicted `backup_incomplete` - there is no directory at all for that id, as opposed to an incomplete one); nothing sent, `verb_trail` and `layout_trail` both empty | - | - | - | 2026-09-26 |
| V5 (live 3) | pass: `hs_stash_close` -> `ok phase:stash_closed` (one transient "not confirmed" poll frame, resolved); a second `hs_stash_tab` and a second `hs_stash_close` both refused `stash_not_open` | - | - | - | 2026-09-26 |
| V6 (live 3) | pass: `hs_stop_game` exited cleanly (`forced:false`); `hs_saves_inspect` changed `herosiege13.hss`, `inventory_order_13.hss`, `shop.ini`, `stash.hss`, nothing added or missing; the DLL hash after equalled the hash before | - | - | - | 2026-09-26 |

## Decision

Fifteen lines. Live 1 (2026-09-25) settled six: `warpRoute`,
`stashCloseRoute`, `stashTabRule`, `stashTabState`, `itemRule`,
`countReader`. Live procedure 2 (2026-09-25) measured the other nine and
closed every line; two of those, `moveWholeRoute` and `moveOneRoute`, record
that the whole-stack and one-unit by-name moves are out of this document's
scope from here on - they moved to `hs-drive-stash-move-research`. Neither
is a game negative: P2-6's refusal answered a mis-read prerequisite (K_M was
never in the stash's map 9), and P2-5's are one refusal left unexplained
(which map held K_J after the owner change was never read) and one shape not
reproduced, quoted below. The hub's stash and
bag tools are written against these lines and against the verbatim replies
the sessions record, not against the hypotheses above. A route line names
the by-name shape (script, self, other, arguments) or the UI route it fell
back to; `not-observed` only when neither was observed. A line that fell back
after a by-name call recorded `shape not reproduced` or `not-run
(instrument …)` says "by-name not tested with the logged shape" and quotes
that shape.

giveItemRoute: bag: json (#14 `partial-nostack`, prior measurement on this build, not exercised by live 1 or live 2; confirmed by this workorder's own control, live 3's V0, for a stackable material at count 1: `confirmed:true`, key `0-0-212584560001-14`, `before=3 after=4` - V0 did not drag, save or reload the item, and the session's save backup was restored afterward). A non-stackable template (class 18) was refused in the same session's V0b, token `give_refused` - "`GetItemPreferredGrid(1, item)` answered no grid; 0-0-212584570002-18 was taken out of map 0 again (craftmats' undo)" - ForgePact's reader `ApPreferredGrid` found no array `grid` in that result, or the call failed; its own `RemoveItemFromMap` undo took the unit back out, not the game's loader; one case, not established as working beyond it. A `give_refused` message naming which check failed and the result's kind, or a second non-stackable template, would close the question. stash: not-observed (live 2 P2-5 variant B: `InitItemFromJson` answered `undefined` - shape not reproduced (a1: proven text `0-0-<S>-<class>`, supplied the raw real `S`) - moved to hs-drive-stash-move-research). The player verb `giveitem` and the hub tool `hs_give_item` ship the bag destination on that measurement and refuse `route_not_measured` for the stash.
warpRoute: the player's `x` set to `Town_Stash_obj`'s `gui=` x and `y` to its `gui=` y plus 48 (live 1: `iset x`, `iset y`), both written by name on the player resolved by name; it landed 0 px from the target on the first try, with no collision (P0-2; re-confirmed 0 px off at 884,628 in live 2's P2-2). The player verb `playerwarp` writes the same two variables.
stashOpenRoute: interact - key F (vk 70, hold 120 ms) after the warp; `UI_Stash_obj` listed=1 (`id=262983`, `stashTabSelected=0`; live 2 P2-2). The logged `UiCreate` on the key: self = other = `Town_Stash_obj#4852@228465`, argc=3, `a0=ref object UI_Stash_obj`, `a1=1`, `a2=1`, `ret=ref instance 262983` (equal to live 1's P0-3 line). The by-name `UiCreate` open with that exact shape was dispatched once in live 2 attempt 1 (`ret=ref instance 262616`) and the game process died on the next command; causality not established from one session; not re-called; not shipped. The closure replay (`callm … inst m_TalkToNPC`) is `not-run (P2-2 amended to the key open)`. The player verb `stashopen` is not shipped; `hs_stash_open` sends the warp then the key.
stashCloseRoute: close-row - the `UI_Button_Close_obj` row whose `uiNodeCallstack` is `InventoryClose` (`win=1195,27` under 1920x1080), clicked once: the window unlisted and the stash saved (`SaveStash` one call; P0-9). `Esc` closed the window once, straight after opening, and did nothing once with the Materials tab on show, so it is not a route. `UiACloseButton` by name reproduced it in live 2's P2-7 (self the close-row button, other the window, `argc=1 a0=array len=0`) on the reopened window: `UI_Stash_obj` unlisted, `SaveStash` +1. The player verb `stashclose` ships this by-name shape.
stashTabRoute: byname - logged (the Shared1 click, live 2 P2-3): self = the `UI_Button_Stash_Tab_obj`, other = `UI_Stash_Tab_Bar_Container_obj#5260@263030`, argc=1, `a0=array len=2` (`activationArgs` `[1,<ref>]`), `ret=undefined`. Supplied and working: `craftprobe call UiAStashMaterialTabClick id:263032 other:263030 -4 id:263032 confirm` (argc=2, `a0=-4`, `a1=ref instance 263032`) -> `stashTabSelected=-4`, screenshot of the Materials grid; Socketable (its `activationFunc` is the closure `UI_Stash_Tab_Bar_Container_obj anon@1018`) through `craftprobe callm id:263031 inst activationFunc -2 id:263031 confirm` -> -2 in both readers; Personal back with `UiAStashTabClick … 0 id:263034`. Each button's handler is read hook-free from its `activationFunc` (`craftprobe methods id:<button>`): Materials -> `UiAStashMaterialTabClick`, Shared1 -> `UiAStashTabClick`. `cycleTabsRightButton` is a `UI_Button_Small_obj` (`uiNodeCallstack="MoveStashRight"`), not clicked. The player verb `stashtab` ships this shape: it reads the button's handler from its `activationFunc`, calls `UiAStashTabClick`/`UiAStashMaterialTabClick` by their SDK constants with other the tab-bar container, calls the tab bar's closure as the method value with self = other = the button, and refuses any other handler as not a measured shape (Unique's handler was never read).
stashTabRule: `tabNumber` on `UI_Button_Stash_Tab_obj`, equal to `activationArgs[0]`: 23 rows - Socketable -2, Materials -4, Unique -5, Personal 0, Shared 1 to 19 - each with `tabType` 1 or 2 (P0-3's fixture, re-confirmed live 2 P2-3). Every row reads `visible=0` although drawn; that field does not follow what is drawn, and is never matched on.
stashTabState: `stashTabSelected` on `UI_Stash_obj`, and never `tabSelected` (live 2's P2-3 dump read `stashTabSelected=-2` beside `tabSelected=0` on the same instance, and a bag page-tab click moved `tabSelected` alone to 1; #14's reading of both at -2 was one observation, not a rule): 0 on Personal (P0-3, P0-4), -4 on Materials (P0-9, after the owner's hand switch; live 2 P2-3), -2 on Socketable (#14; re-confirmed live 2 P2-3). The bag's page tabs are `tabNumber` 0 to 4 on `UI_Button_Inventory_Tab_obj` (Main, then four Extra); the bag's own tab state is `bagTabState`.
bagTabRoute: byname - logged (the Materials click, live 2 P2-4): self = `UI_Button_Inventory_Tab_Small_obj#4989@263017`, other = `UI_Stash_obj#5257@262983`, argc=1, `a0=array len=0`, `ret=undefined`; `activeNode` moved to 263017 on the click. Supplied: `craftprobe call UiAInventoryMaterialTabClick id:263017 other:262983 confirm` (argc=0) -> `tabSelected=-4` after it. Nothing read `tabSelected` between the Socket click that preceded it and this call, so the value after equals the last one read before the Socket click and this record cannot tell "the call switched the sub-tab" from "the call did nothing" from live 2 alone. Live 3's V3, through the shipped verb, separated it with its own before/after: `bagtab materials` moved `tabSelected` 0 -> -4, and `bagtab socket` moved it -4 -> -2, both confirmed switches (`activeNode` unchanged at 262324 through both - read, not proven). **`UI_Stash_obj.activeNode` was not observed to follow the call** (it stayed at the Socket sub-tab's id, 263016; the supplied argc=0 differs from the logged argc=1 with an empty array, an untried alternative; recorded, not a fail). Socket's handler `UiAInventorySocketTabClick` fired on its click (`logged=1`); its by-name replay was not run (the Materials one is the measured case; the verb takes the same path). The main sub-tab (`vault`) was restored by a click at a recalibrated coordinate, never by name, so `vault` is `route_not_measured`. The player verb `bagtab` ships the Materials and Socket routes only.
bagTabRule: `uiNodeCallstack` on `UI_Button_Inventory_Tab_Small_obj` (7 rows: VaultActive, Vault, Socket = `InventoryTabSocket`, Material = `InventoryTabMaterial`, Key, Tarot, Relic; `text` empty on every row, no `tabNumber` field on this object).
bagTabState: `tabSelected` on `UI_Stash_obj` (1 -> -4 on the Materials click, live 2 P2-4 - the 1 was left by the page-tab miss, which the click back did not return to 0 - so the page tabs write it too; it is not `stashTabSelected`, which read 0 throughout). `UI_Stash_obj.invMaterialTab`/`invSocketTab` are the Material/Socket rows' ids.
cellRule: `nodeFingerprint` only - a node struct (`New_Inventory_Data_obj.potionGrid.0.0`, live 2 P2-5) has `nodeStartX`, `nodeStartY`, `nodeLocked`, `nodeIsPermanent`, `nodeFingerprint`; no count member. `stashPersonalGrid` is `array[18]` and `.0.0` read `undefined` (an empty cell or an indexing convention not resolved - recorded as read, never a route: a click on a cell is not a route here).
itemRule: fingerprint - the item map's key, which a cell carries as `nodeFingerprint`, shaped `0-0-<n>-<class>` (class 18 a non-stackable, class 14 a material; P0-1). The hub's give-item tool answers the new one.
moveWholeRoute: moved to hs-drive-stash-move-research (docs/stash-move-research.md) - live 2 P2-5: variant A's `ChangeItemOwner id:258308 0 9 fp:<K_J>` dispatched (`ret=undefined`, its normal answer) and the next call's `fp9:<K_J>` lookup with self `Console_Save_obj` refused; no read of map 0 or of the kept map 9 followed, so which map held K_J afterwards is not known; `StashGridAddItem` refused identically; the attempted owner change was reversed and `node bag` showed K_J unchanged. Variant B: `CreateItemSaveStruct` (5 members, no `o`: a non-stackable), `LootTimestamp` 212527295000, then `InitItemFromJson Console_Save_obj 0 kept:CreateItemSaveStruct kept:LootTimestamp` -> `undefined` - the second argument was the raw real where #14's proven shape is the key text `0-0-<S>-<class>` (`shape not reproduced (a1)`); `AddItemToMap` and `GridAddItem` inherited the `undefined` (the latter threw).
moveOneRoute: moved to hs-drive-stash-move-research (docs/stash-move-research.md) - live 2 P2-6: not-observed - `set fp9:<K_M>.itemDefinitionStruct o 933` refused, and K_M was never in map 9: live 1 read it from `New_Inventory_Data_obj.localItemMap`, which #14 measured as **owner 0's** map (`map0-identity`), live 2 found it only in the bag's `inventoryMaterialGrid` (the bag's Materials sub-tab, #14) and resolved it through `fp:` (map 0) in the same session. So the "map-9 lookup limitation" is the lookup answering correctly about map-0 items; the prerequisite was mis-read, not the game.
countReader: the item definition's `o` on the item struct the map holds under the fingerprint - in the research build, `craftprobe node bag`, `node stash` or `node id:<grid>`'s `def.o` (P0-1, P0-7); in the player build, `itemDefinitionStruct.o` read the way `craftmats` reads an item's count. Never a game getter from `menulayout`. After D11 no shipped tool reads a stash-side count; `giveitem` prints `o=` from the struct it built.

What each line records:

- **giveItemRoute**: the order that placed a created item into each
  destination - `bag: json`, `stash: json`, or `stash: not-observed` (P0-1).
- **warpRoute**: the position write and the offset below the stash that held
  and let the open succeed, or `not-observed` with what happened (P0-2).
- **stashOpenRoute**: `closure`, `interact` or `uicreate`, with the self and
  arguments of the by-name call (P0-3).
- **stashCloseRoute**: `esc`, or the close button row and how it is told
  apart from other windows' close buttons (P0-9).
- **stashTabRoute** and **bagTabRoute**: the handler called by name with its
  self and other, or `click` (P0-5, P0-6).
- **stashTabRule** and **bagTabRule**: the listed field, and its value per
  tab, that identifies a stash tab row and a bag tab row (P0-3, P0-4).
- **stashTabState** and **bagTabState**: the object and variable that record
  the tab on show, with the value per tab (P0-5, P0-6).
- **cellRule**: the fields a grid's node carries - the fingerprint, and a
  count if it has one. No geometry: a click on a cell is never a route here
  (P2-5).
- **itemRule**: the field the hub matches an item by (P0-1).
- **moveWholeRoute** and **moveOneRoute**: measured in P2-5 and P2-6, then
  moved out of this document's scope under D11 to `hs-drive-stash-move-research`,
  which starts from those two checks' refusals. The drag path is not a
  candidate: it calls no named routine (§ Static search).
- **countReader**: where a count is read on each side, and by which build
  (P0-1, P0-7).
