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

**Posture.** Everything here is measured runtime behaviour, a reading written
in our own words, or our own code. Game objects and scripts are named by their
`hs-game-sdk` names and indices; no game script text appears, and the
procedure is written as prose.

**Status.** The instrument (the extended `menulayout`, beside `craftprobe` and
`citrace` in the research build) is built on this branch; the `menulayout`
extension ships in the player build. Phase 0, the research launch below, has
**not run yet**: § Results is empty and every § Decision line reads
`pending`. What § Instrument lists as hypotheses, and what § Static readings
suggests, are not facts until phase 0 records them.

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
- No stash hotkey exists (the owner, 2026-09-23: WASD moves, `E` interacts),
  and no session measured an interact key or a by-name open: the stash was
  opened by hand in every prior session.
- Which input opens the split-stack dialog was not recorded: the owner
  opened it by hand.
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
  them has to leave the same map and hash bookkeeping behind, which is why
  the by-name replay below copies the order the game itself logs rather than
  writing cells.
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
  replay those calls by name. This branch adds four things to it for phase 0
  (below).
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
  install's name in the message is the only thing its flags decide.

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
`craftprobe: phase1k rows=285 - …`; 282 names a build without this branch's
additions. A second control inside the
procedure (P0-5): with the Socketable tab on show, the stash window's
`stashTabSelected` reads -2, the value prior measurement recorded, both in a
`citrace dumpobj` of the window and in the `stashTabSelected=` field of
`menulayout UI_Stash_obj` - the second is the field the hub tools will read,
so it needs its own control rather than borrowing the dump's. If either
fails, that check is instrument-blind.

**Hypotheses phase 0 tests** (none is a fact yet). Setup goes through the
runtime by name; the UI route is measured only as the fallback a § Decision
line may keep.

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

## Results

Phase 0 has not run. One dated row per check, filled from the live capture.
For a by-name call, *Logged shape* is the self, other, argument count and
arguments of the game's own call (its armed line) and *Supplied shape* those
of the replay (its reply line), with the outcome under § Instrument's
recording rule in *Observation*; `-` where a check makes no by-name call.

| Check | Observation | Logged shape | Supplied shape | Control | Date |
| --- | --- | --- | --- | --- | --- |
| dll-hash | pending | - | - | - | - |
| marker | pending | - | - | - | - |
| control | pending | - | - | pending | - |
| P0-1 | pending | pending | pending | pending | - |
| P0-2 | pending | - | - | - | - |
| P0-3 | pending | pending | pending | - | - |
| P0-4 | pending | - | - | pending | - |
| P0-5 | pending | pending | pending | pending | - |
| P0-6 | pending | pending | pending | - | - |
| P0-7 | pending | pending | pending | - | - |
| P0-8 | pending | pending | pending | - | - |
| P0-9 | pending | - | - | - | - |
| P0-10 | pending | - | - | - | - |

## Decision

Fifteen lines, each `pending` until phase 0 has measured it. The hub's stash
and bag tools are written against these lines and against the verbatim
replies phase 0 records, not against the hypotheses above. A route line names
the by-name shape (script, self, other, arguments) or the UI route it fell
back to; `not-observed` only when neither was observed. A line that fell back
after a by-name call recorded `shape not reproduced` or `not-run
(instrument …)` says "by-name not tested with the logged shape" and quotes
that shape.

giveItemRoute: pending
warpRoute: pending
stashOpenRoute: pending
stashCloseRoute: pending
stashTabRoute: pending
stashTabRule: pending
stashTabState: pending
bagTabRoute: pending
bagTabRule: pending
bagTabState: pending
cellRule: pending
itemRule: pending
moveWholeRoute: pending
moveOneRoute: pending
countReader: pending

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
- **cellRule**: how a cell's window point is computed from the grid node's
  own variables - origin, pitch and scale (P0-4).
- **itemRule**: the field the hub matches an item by (P0-4).
- **moveWholeRoute** and **moveOneRoute**: the by-name call order, with self
  and arguments, that moved a whole stack and exactly one unit, or the UI
  route kept as the fallback (P0-7, P0-8).
- **countReader**: where a count is read on each side - expected
  `localItemMap.o (bag only)` for `menulayout` if the P0-4 control passes,
  and the write verb's own item-map read for the stash side (P0-4, P0-8).
