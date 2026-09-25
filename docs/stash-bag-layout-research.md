# Stash and bag layout: where the stash, its tabs and the bag's cells are

**Question.** For the town stash and the bag beside it, as the running game
reports them: how does the stash open and close, which listed row is each
stash tab and each bag tab, which variable says which tab is on show, where
is each grid cell and which item it holds, and what input moves one whole
stack, or exactly one unit of a stack, between the bag and the stash?

**Why it matters.** The hub's `hs-drive` MCP server (`docs/tools/hs-drive-mcp.md`
in the toolkit) is gaining five tools - `hs_stash_open`, `hs_stash_close`,
`hs_stash_tab`, `hs_bag_tab` and `hs_move_item` - so a live test session can
run with no one at the keyboard (toolkit issue #147). Each of them clicks
only a point `menulayout` lists, and refuses by name when the game does not
list its target. None of the mechanisms they need is measured in this
repository yet. This document records the static search, the instrument, the
one research launch that measures them, and the decision the hub tools are
written against.

**Posture.** Everything here is measured runtime behaviour and our own code.
Game objects and scripts are named by their `hs-game-sdk` names and indices;
no game script text appears, and the procedure is written as prose.

**Status.** The instrument (the extended `menulayout`) is built on this branch
and ships in the player build. Phase 0, the research launch below, has **not
run yet**: § Results is empty and every § Decision line reads `pending`. What
§ Instrument lists as hypotheses are not facts until phase 0 records them.

## Static search

Source: `hs-game-sdk`'s object and script tables and its parent hierarchy
(the Python binding's parent lookup and the C++ `objects.hpp`), searched on
2026-09-23 over every object whose name matches a stash, bag, inventory, tab,
grid, split or drag concept; plus `menu-layout-research.md`,
`character-select-research.md` and `prospect-window-research.md` in this
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
`UI_Button_Close_obj` (4982, child of `UI_Button_obj`). The item held on the
cursor is a `UI_Inventory_Drag_obj` (5110), which **has no parent**, so only
its own name reaches it. `New_Inventory_Data_obj` (3067, unparented) holds the
bag's item map. Two room-space objects are needed to walk to the stash: the
stash itself, `Town_Stash_obj` (4852, under `Quest_NPC_Parent_obj` and
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

- The stash probe verb (`craftprobe node`) and the stash-tab variable
  (`stashTabSelected`) that toolkit issue #147 cites are **not in this
  repository** at this branch's base or on any branch it builds from. They
  come from the crafting-materials research on another branch (ForgePact
  #14), which this work reads as prior measurement only and does not merge.
  Its findings are relied on here only after a positive control in our own
  phase 0 session.
- The bag's Materials tab is **not a grid node**: the prospect research found
  its stacks drawn by a draw script, not held in a `UI_Inventory_Grid_obj`,
  and no prior research read a count from it. The stash's Socketable tab is
  container-shaped too (per-item grid instances under a container, prior
  measurement below); the stash's Materials tab is not - prior measurement
  found it to be the window's own `StashGrid`, a plain grid node.
- No stash hotkey is recorded anywhere, and no session measured walking to
  the stash or pressing an interact key: the stash was opened by hand in
  every prior session.
- Which input opens the split-stack dialog was not recorded: the owner
  opened it by hand.

What prior measurement established (the prospect research in this
repository, and the #14 crafting-materials research read by path), in our
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
  the shared tabs 1 to 19, Socketable -2, Materials -4 and Unique -5; result
  structs from item placement carried a `tabType` of -4 for the Materials
  tab and 0 for the main grid.
- Closing the stash destroys or deactivates its window, grids and
  containers; a reopen makes new instances with new ids, so no id is cached
  across a close. The stash is saved once per close, and the stash and the
  Cube cannot be open together.
- A bag stack's count is the item struct's `o`, reachable getter-free by
  looking the cell's fingerprint up in `New_Inventory_Data_obj`'s
  `localItemMap` with the runtime's `ds_map_find_value`. The stash's item
  map was not found in any printed variable (not established as absent).

## Instrument

`menulayout` is the player-build command documented in
`menu-layout-research.md`, unchanged in its header, footer, row format,
200-row cap, deduplication by instance id and one-name form
(`menulayout <ObjectName>`). This branch changes three things, and
`tests/test_stash_bag_layout_contract.py` pins each of them beside
`tests/test_menu_layout_contract.py`:

1. **The candidate table** is the twenty-nine names above, each spelled
   through the SDK's `GameObject` enum.
2. **More optional fields.** After `label`, `name`, `slot`, `index`, `page`
   and `selected`, a row prints - only when the instance carries the
   variable, in this order - `uiNodeCallstack`, `activationArgs`, `enabled`,
   `tabNumber`, `tabType`, `stashTabSelected`, `nodeGridWidth`,
   `nodeGridHeight`, `gridScale` and `gridName`; `text` stays last on every
   row. `activationFunc` is left out on purpose: a method value has no
   stable text. The last five after `enabled` are hypotheses (seen on result
   structs or stated by the owner, not on an instance); phase 0 prunes any
   that no dump showed.
3. **Array values** print as `[a,b,...]`: the runtime's `array_length`, then
   each element through `array_get` and the same value formatter as any
   field; a nested array prints `<array>` rather than being walked, past 32
   elements the rest are counted as `,...+N`, and an element that fails to
   read prints `<read-failed>` in its place. Only a string goes through the
   runtime's string conversion: a reference (an instance handle included),
   a struct or method, and a pointer print by kind as `<ref>`, `<object>`
   and `<ptr>`, and any other kind as `<kind N>`, because what that
   conversion makes of them has not been measured. So a tab identity held
   as a handle in `activationArgs` shows only as `<ref>` here; the P0-6
   dumps are where its value is read. This is compiled and run against a
   stand-in runtime that represents arrays, nested arrays, `VALUE_REF`
   handles, structs and pointers, and whose string conversion refuses
   anything but a string (`tests/menu_layout_value_text.cpp`).

It stays a reader. It runs inside the command poll, installs nothing on the
per-frame path, and never clicks, hooks, performs an event, creates,
destroys, writes, or runs a game script - neither through `script_execute`
nor through `CallBuiltinEx`. Game getters such as the item lookup by
fingerprint or the item-map getter are out of scope: a count, when there is
one, is read from instance and struct variables only.

**Positive control, every session.** `ping` answers `pong (YYTK 4.0.1)`, then
in town `menulayout Player_obj` prints a header reading `room=Town_01_rm` and
exactly one `obj=Player_obj` row with a finite `gui=`. A second control
inside the procedure (P0-8): with the Socketable tab on show, the stash
window's `stashTabSelected` reads -2, the value prior measurement recorded,
both in a `citrace dumpobj` of the window and in the `stashTabSelected=`
field of `menulayout UI_Stash_obj` - the second is the field the hub tools
will read, so it needs its own control rather than borrowing the dump's.
If either fails, the instrument is blind and nothing it lists afterwards is
evidence.

**Hypotheses phase 0 tests** (none is a fact yet):

- **Open.** Walk the player next to `Town_Stash_obj` with the movement keys,
  steering by the two rows' room positions, and press the interact key. The
  range is unknown. Whether the game has a stash hotkey is asked of the
  owner, never found by pressing candidates.
- **Warp**, the fallback the owner accepted for a blocked walk: set the
  player's `x` and `y` by name, next to the stash, then interact. Unknown:
  whether the game accepts the write (collision, camera, the interactable's
  range check). Phase 0 measures it with the research build's instance
  variable write; the player build gets its own verb only if it works.
- **Tab identity.** A stash or bag tab row is identified by
  `uiNodeCallstack` and/or `activationArgs` (for instance the tab number its
  handler receives), not by its text or its position.
- **Tab state.** `stashTabSelected` on `UI_Stash_obj`: -2 on Socketable is
  the control; -4 on Materials and 0 on the personal tab are expected from
  the tab table. The bag's own state variable is unknown: the variable a
  bag tab click changes is it.
- **Whole-stack move.** One held click on the item's bag cell either
  quick-moves it into the stash or picks it up; if it picks up, one held
  click on a free stash cell places it. The fallback is a drag.
- **One-unit move.** Shift-click, then right-click, then ctrl-click on a
  stack, one at a time, each followed by a listing of `UI_Split_Stack_obj`;
  the first that lists the dialog is the route, and its controls are listed
  under the roots.
- **Close.** `Esc`, else the stash's close button row; the proof is that no
  `UI_Stash_obj` is listed.

## Live procedure

Owner-run, one launch, the research (dev) build of this branch
(`plugin_build\BloodPactPlugin_rel.dll`, its SHA-256 named in the dispatch).
The owner installs it; the installed DLL is hashed before and after.

**Character.** The save slot the owner names in P0-0. It must have one
non-stackable junk item in the bag's main grid and one material stack of at
least two in the stash's Materials tab. Cases: the junk item bag to stash to
bag (the ordinary case), and one material unit stash to bag to stash through
the split dialog (the outlier: a list-shaped tab and the split route). No
others. Every reply is captured verbatim.

- **P0-0** (needs the owner, before anything is launched). Ask whether a live
  session is convenient now, which save slot to use, which keys move the
  character (WASD by default?), which key interacts with the stash, whether
  there is a stash hotkey, and whether the path from the town spawn to the
  stash is clear. Record the answers.
- **P0-1.** An out-of-band copy of the saves plus `hs_saves_backup`;
  `hs_launch` to `plugin_ready`; `hs_select_character` to `character_loaded`.
  Then the control above.
- **P0-2.** `menulayout Town_Stash_obj`: one row with a finite `gui=`
  expected. Record the player's and the stash's positions and the header's
  `view=`.
- **P0-3.** For each of the four movement keys, hold it 300 ms through
  `hs_input` and list `Player_obj`. Expected: each key changes exactly one of
  x and y, in one direction. Record the pixels per 300 ms.
- **P0-4.** Walk in 300 ms pulses toward the stash until the player is within
  64 px, or record `blocked` when the distance stops shrinking three pulses
  running (then the owner walks the character there by hand). Screenshot.
- **P0-5.** Press the interact key (held 120 ms). Expected: `menulayout
  UI_Stash_obj` lists one visible row. If not, and the owner named a hotkey,
  press it once. Record what opened it. Fixture: the full listing on the open
  stash, plus the listings of `UI_Button_Stash_Tab_obj`,
  `UI_Button_Inventory_Tab_obj`, `UI_Inventory_Grid_obj`,
  `UI_Inventory_Drag_obj` and `UI_Stash_Socket_New_obj`, with `listed=` and
  `capped=`.
- **P0-6.** Dump (`citrace dumpobj`) the stash window (expected: the tab
  state variables at 0 on the personal tab, and the active node, stash grid,
  materials-tab and container references present), stash tab buttons 0 to
  2, bag tab button 0, the stash's grid (expected `StashGrid`, 17 by 18 on
  the Materials tab), the bag's `InventoryGrid` (expected 15 by 6) and
  `New_Inventory_Data_obj` (expected: `localItemMap` a ds_map); run
  `prospectprobe grids`. Count control: look a bag cell's fingerprint up in
  `localItemMap` with `cb ds_map_find_value` and read `o` on the result;
  expected equal to that stack's count by eye (if `cb` cannot address the
  map, record `not-observed`). Note every field of one occupied cell and
  whether a stash cell references its item struct.
- **P0-7.** Click the Materials tab row's `win` (held 120 ms). Expected: the
  screenshot shows the Materials tab.
- **P0-8.** Dump the stash window again and diff against P0-6. Expected: the
  tab state now -4 and little else changed. Then Socketable - the
  **control**, expected -2 in the dump and `stashTabSelected=-2` on the
  `menulayout UI_Stash_obj` row; a different value or a missing variable in
  either is instrument-blind for this check. Then back to the personal tab
  (0 expected). Then click one bag tab row and dump the bag's window before
  and after.
- **P0-9.** With Materials selected, the full listing (fixture). Prior
  measurement says this tab is the window's `StashGrid`, so read its cells
  the way P0-6 reads a grid's, and record which field of an occupied cell
  (or of what it references) names the material and which holds a count.
  Dump one list-entry instance only if P0-5 listed one under the stash; a
  missing list entry on this tab is the expected shape, not a
  `not-observed`.
- **P0-10.** Personal tab selected: one held click on the junk item's bag
  cell (its point from a fresh grid listing). Expected either a quick move
  (the item's fingerprint now in a stash cell, its bag cell empty) or a
  pick-up (the held-item object listed), in which case one held click on a
  free stash cell places it. If neither, drag it. Record the route that
  worked (fixture: the after-listing) and move it back the same way.
- **P0-11.** On the stash material stack, try shift-click, then right-click,
  then ctrl-click, each followed by `menulayout UI_Split_Stack_obj`. Expected:
  one lists the dialog; then the full listing (fixture) for its controls.
  Record `not-observed` for each tried without a dialog.
- **P0-12.** With the dialog open, set one unit the way its listed controls
  allow and click its confirm row. Expected: the bag gains a one-unit entry
  and the stash stack is one lower. Move it back by P0-10's route.
- **P0-13.** Press `Esc`. Expected: no `UI_Stash_obj` listed. If it still is,
  click the close button row nearest the stash window's origin; record which
  closed it.
- **P0-14.** The warp, measured whether or not P0-4 was blocked. With the
  stash closed, list the player; write the player's `x` and `y` to the spawn
  point from P0-2 with the research build's `iset`, and list again -
  expected at that point within 2 px, and the screenshot shows the character
  moved. Then write the point P0-4 stood at when the stash opened (or 64 px
  below the stash if P0-4 was blocked) and press the interact key -
  expected: the stash window listed. Close it by P0-13's route. Record the
  route, or `not-observed` with what happened.
- **P0-15.** `hs_stop_game` (`exited: true`, `forced: false`);
  `hs_saves_inspect` (`stash.hss` expected among the changed files); restore
  the P0-1 backup, since this session moved items, and inspect again:
  nothing changed or missing. The owner restores their DLL; its hash equals
  the pre-session hash.

A `not-observed` on P0-5, P0-8, P0-10 or P0-12 (the open, tab-state, move and
split mechanisms) sends the plan back for revision rather than into an
implementation round. A `not-observed` on P0-14 does not: it records
`warpRoute: not-observed`, the stash-open tool then has no fallback, and no
player-build warp verb ships.

## Results

Phase 0 has not run. One dated row per check, filled from the live capture.

| Step | Observation | Control | Date |
| --- | --- | --- | --- |
| P0-0 | pending | - | - |
| P0-1 | pending | pending | - |
| P0-2 | pending | - | - |
| P0-3 | pending | - | - |
| P0-4 | pending | - | - |
| P0-5 | pending | - | - |
| P0-6 | pending | pending | - |
| P0-7 | pending | - | - |
| P0-8 | pending | pending | - |
| P0-9 | pending | - | - |
| P0-10 | pending | - | - |
| P0-11 | pending | - | - |
| P0-12 | pending | - | - |
| P0-13 | pending | - | - |
| P0-14 | pending | - | - |
| P0-15 | pending | - | - |

## Decision

Fourteen lines, each `pending` until phase 0 has measured it. The hub's stash
and bag tools are written against these lines and against the verbatim
replies phase 0 records, not against the hypotheses above.

stashOpenRoute: pending
interactKey: pending
moveKeys: pending
warpRoute: pending
stashCloseRoute: pending
stashTabRule: pending
stashTabState: pending
bagTabRule: pending
bagTabState: pending
cellRule: pending
itemRule: pending
moveWholeRoute: pending
moveOneRoute: pending
countReader: pending

What each line records:

- **stashOpenRoute**: what opened the stash - `walk then interact`, a hotkey,
  or the owner by hand (P0-4, P0-5).
- **interactKey** and **moveKeys**: the virtual-key codes the owner named and
  P0-3/P0-5 confirmed.
- **warpRoute**: `variable_instance_set x,y then interact`, or `not-observed`
  with what happened (P0-14).
- **stashCloseRoute**: `Esc`, or the close button row and how it is told
  apart from other windows' close buttons (P0-13).
- **stashTabRule** and **bagTabRule**: the listed field, and its value per
  tab, that identifies a stash tab row and a bag tab row (P0-5, P0-6).
- **stashTabState** and **bagTabState**: the object and variable that record
  the tab on show, with the value per tab (P0-8).
- **cellRule**: how a cell's window point is computed from the grid node's
  own variables - origin, pitch and scale (P0-6).
- **itemRule**: the field the hub matches an item by, in a cell or a list
  entry (P0-6, P0-9).
- **moveWholeRoute** and **moveOneRoute**: the inputs that moved a whole
  stack and exactly one unit (P0-10, P0-11, P0-12).
- **countReader**: where a count is read without a game getter - expected
  `localItemMap.o (bag only)` if the P0-6 control passes, `none` otherwise.
