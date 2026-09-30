# Moving an item from the bag into the stash, by the game's own routine (ForgePact #68)

**Question.** With the town stash open, which of the game's own routines,
called by name, moves one whole item from the bag page on show into the
stash tab on show, the way a hand move does: a non-stackable into a free
cell of a grid tab, a stackable onto the stash's stack of its kind, the bag
cell emptied, and the item's map entry left where the game's own save
expects it?

**Why it matters.** ForgePact #68 asks for a "Move all" key: with the stash
open and the mod on, one press moves every item on the bag tab on show into
the stash tab on show, each item by the game's own per-item move, once per
item, exactly as if the player had moved it by hand. The owner's conditions
(2026-09-27): call the game's own move, resolved by name; never rewrite an
item container or a map entry directly; never use a hand-resolved address;
change nothing else the game does. Nothing in this repository has yet moved a
whole item from the bag into the stash by name: the two attempts recorded in
`stash-bag-layout-research.md` § Decision (`moveWholeRoute`, `moveOneRoute`)
were refused for reasons that record leaves open, and they moved to this
document (the name `hs-drive-stash-move-research` there points here).

**What this document carries.** Names, argument counts, call order and
measured behaviour, in this document's own words; no decompiled script text,
no code addresses and no listing of any kind. The static reading below was
made in a local Ghidra project that stays on the owner's machine, per the
toolkit's `AGENTS.md` § "Legal: Decompiled Output Never Reaches Any Origin".
Each claim is labelled **static reading** (not measured), **measured** (with
its session), or **not read**.

## Static search

Every name below is an `hs-game-sdk` constant (`HeroSiege::Scripts::`, the
`gml_Script_` prefix left off here); the closures are the grid and stash
windows' own methods.

- **The grid input processor**: `ProcessInventoryGridInput` (about 311 calls
  per window in earlier sessions; the grid node is its self).
- **Placement into a grid**: `GridAddItem`, `StashGridAddItem`,
  `InventoryGridAddItem`, `InventoryGridAddItemPos`,
  `InventoryGridAddItemToTab`, `InventoryGridHasSpace`,
  `InventoryGridHasSpaceMulti`, `s_InvNode`.
- **Stacks**: `StashAddToStack`, `InventoryGridCanAddToStack`,
  `InventoryGridAddToStack`, `GridAddToStack`,
  `InventoryStackUpdateAndRemove`.
- **The source cell**: `InvGridClearItemNode`, `GridRemoveItem`,
  `InventoryGridRemoveItem`, `InventorySwapItemsNew`.
- **The item and its map**: `GetItemFromFingerprint`, `GetItemMap`,
  `ChangeItemOwner`, `AddItemToMap`, `RemoveItemFromMap`, `ValidateItem`,
  `GetPlayerItemOwner`, `GetStashMaxTabs`.
- **Drag state**: `InvStartDragging`, `InvCopyItemDragData`,
  `InvCopyItemToInvDragData`.
- **`UI_Inventory_Grid_obj` closures**: `anon@15345` (the grid's
  `m_MoveItemToGrid`, named by `prospect-window-research.md`) and its six
  `___struct___522..541@anon@15345` helpers, `anon@8881` (`m_DropItem`) and
  its two `___struct___517/519` helpers, `anon@2143`, `anon@2971`,
  `anon@34555`, `anon@36159`, `anon@4064`.
- **`UI_Stash_obj` closures**: `anon@1649`, `anon@2245`, `anon@2483`,
  `anon@3835`, `anon@4195`, `anon@4348`, `anon@5662`, `anon@6631`,
  `anon@7091`, `anon@7525`, `anon@8329`, `anon@8574`.

What earlier work already recorded, cited rather than repeated:

- A hand **drag** of a grid item into a stash grid cell logged only
  `s_InvNode` writes, with twelve armed add, remove and map rows at 0 calls
  while the control climbed (`stash-bag-layout-research.md` § Static search,
  live 1 P0-7). That path is inline and cannot be called by name.
- A hand move of **one unit of a stackable** into a stash special tab logged
  `StashAddToStack` (self = other = the bag grid, six arguments) answering
  `true`, then `InvGridClearItemNode` on the emptied bag cell
  (`crafting-materials-research.md` `### Phase 1b results`, rows
  `move-bag-to-socket` and `move-material-to-bag`). Measured, never replayed
  by name.
- A hand move **out of** the stash into the bag logged `ChangeItemOwner` with
  self the bag's grid, from-owner 9, to-owner 0 and the item's fingerprint
  (`crafting-materials-research.md`, the hand take in `### Phase 1f
  results`).
- The two by-name whole-item attempts (`moveWholeRoute`, `moveOneRoute`) are
  not game negatives: after the owner change nobody read which map held the
  key, and the json route was fed a raw number where the proven shape is the
  key text (`stash-bag-layout-research.md` § Decision).
- `GetProfileInventoryData` needs the window as its self; called otherwise it
  threw and then crashed `Controller_obj` Step (toolkit
  `docs/RUNTIME_DATA_MODELS.md` § 9.4). No shape below calls it or a routine
  that calls it from a self it was not measured with.

## Static reading

Read on 2026-09-28. `ProcessInventoryGridInput` and `anon@15345` both time
out in the decompiler, the first also at a 1800-second limit, so both were
read from their instruction listings: the order of named calls, each call's
argument count, where an argument comes from, and which branch a call's
answer takes. Member names the runtime fills in from `data.win` at run time
cannot be read in the executable (`docs/RUNTIME_DATA_MODELS.md` § 17), so a
condition that tests a member is described by what it tests only when a
number or a named routine makes that plain. Every item here is a **static
reading** unless it says otherwise.

- **The processor has two separate move directions, and the one the plan
  first named is into the bag.** Its last group of calls (the stack check,
  the stack add, the tab add, the source clear, then the item check) places
  an item **into the player's bag**: the tab add, `InventoryGridAddItemToTab`,
  passes its first argument straight to the profile getter, so the tabs it
  can place into are the bag's page tabs (a tab number 0 or more) and the
  bag's special grids (-4 for class 13 or 14, -2 for class 15 with a
  further test that was not read, -3 for class 12), never a stash tab. After a merge onto a bag stack that group
  also looks up the item's map through the player's item owner and deletes
  the merged unit's entry, which is what #14 measured for a merge. So
  `InventoryGridAddItemToTab` cannot place into the stash, and the candidate
  shape that gave it the stash owner 9 and a stash tab is withdrawn.
- **The bag-to-stash direction is a different group.** It first calls
  `StashAddToStack` with six arguments; if that answers true, it calls
  `InvGridClearItemNode` with two arguments - the processor's own first
  argument and an undefined second - under the processor's own self and
  other. If the stack add does not answer true, it calls `GridAddItem` with
  four arguments: a grid array, the item, one of two constants the
  processor picks from a condition just before (which values: **not
  read**), and an undefined fourth. If that placement's
  answer has `success` true, the group ends. If not, and a further condition
  holds, it asks `GetStashMaxTabs` (no argument) and walks the stash's tabs
  in turn, trying the stack add and then `GridAddItem` on each until one
  answers `success`; after the walk two more `GridAddItem` calls follow on
  two other grids (**not read**: what those two grids are - putting the item
  back where it came from is one possible reading). So on this reading a
  quick move that does not fit the tab on show may land in another stash
  tab. Only the game's own input reaches that walk: `GridAddItem` called by
  name searches the one grid array it is handed, so a by-name placement into
  a full tab answers `success=false` whatever the walk would do. Live 1's
  check `grid-move-tab-full` is therefore the measurement only when it is a
  single-input quick move - a right-click or a shift-click on the bag cell,
  with no destination cell chosen, that moved an item into the stash in the
  gesture checks - repeated against a full tab on show, reading which tab
  the item lands on. Click, move, click never qualifies: it picks the item
  up and places it into a chosen cell, which does not reach the walk, and
  a full tab has no empty cell to choose. The
  by-name placement into the same full tab is the instrument check
  `tab-full-byname`, expected to answer `success=false` by construction and
  never evidence about the walk.
- **No map routine in that group.** Between the stack add and the end of the
  tab walk there is no direct call of `ChangeItemOwner`, `AddItemToMap` or
  `RemoveItemFromMap`, and after a successful `GridAddItem` there is no
  source-cell clear in that group either. Where the bag cell empties after a
  grid placement, and where the map owner changes, are **not read** in the
  processor.
- **`m_MoveItemToGrid` (`anon@15345`) is where the owner changes.** It
  declares four arguments and turns a missing second, third or fourth into 0
  (the drag passes undefined then three zeros, `prospect-window-research.md`
  R12). It does not take the item from its arguments: it reads a struct
  through a variable and takes two members of it - on this reading the
  item's fingerprint and its current owner - to look the item up and check
  it (`GetItemFromFingerprint` with two arguments, then `ValidateItem`).
  After asking `GetPlayerItemOwner`, it calls `ChangeItemOwner` with three
  arguments at ten places, one per pair of grid kinds, each followed by the
  matching online stash call (`StashAddItemOnline`, `StashTakeItemOnline`,
  their Unique, Guild and Blood Pact forms). Which of those ten a bag-to-stash
  move takes, and which variable holds the struct, are **not read**. The
  prospect research measured this method's two shapes: a click-in with self
  = the destination grid and other = the source grid and no argument, and a
  drag-in with self = other = the destination grid and four arguments.
  Because the item comes from a variable the plugin cannot set by name
  without writing it, this method is a second-choice route, tried by name
  only after the processor's own group.
- **`StashAddToStack`** takes six arguments; #14 logged a cell array (one row
  of one cell for Socketable, the eighteen-wide array for Materials), 9, 2,
  the item, 1 and a small number (8 for Socketable, 0 for Materials). Read:
  it first tests the item's class and answers false for anything but classes
  12 to 15, before it touches a cell; it walks the cell array's two levels,
  looks each cell's item up by fingerprint, checks the item's hash
  (`ItemCheckHash`), and merges through `InventoryStackUpdateAndRemove`,
  with `ReportClient` on a failed check. No direct call of a map-owner
  routine was read in it (a call dispatched through the script table would
  not show in this reading). The
  sixth argument is tested for a number and otherwise replaced by a large
  default: **not read** what it bounds. The third (2) and fifth (1)
  arguments: **not read** beyond #14's logged values - the fifth equals the
  unit count of the logged one-unit moves.
- **`InvGridClearItemNode`** takes two arguments: the cell (node) and a
  second the processor passes as undefined. It looks the cell's item up by
  fingerprint and asks its size, and (#14's Phase 1h reading) reads two
  variables of its self, so its self must be the grid that holds the cell -
  the bag grid for a bag cell. No direct call of a map routine was read in
  it.
- **`GridAddItem`** (#14's reading, unchanged here) reads no variable of its
  self: it sizes the item, finds a fit in the grid array it is given, writes
  the node, makes no direct call of a map routine that was read, and answers
  a struct with the tab, the position,
  the tab type and `success`. #14 measured it by name into the bag's
  persistent Socketable grid with 0 and undefined as its last two
  arguments.
- **`ValidateItem`** is called with two or three arguments at seven places in
  the processor and once in `m_MoveItemToGrid`, right after the lookup. Its
  body copies a struct (`StructCopy`) and reads the weapon-type and loot-name
  tables; whether it refuses an item or repairs one, and what it answers,
  are **not read**. It is a row now so Live 1 logs what it answers around
  each by-name move.
- **The self.** The processor runs with a grid node as its self and passes
  its own self and other on to every call in both groups; #14 logged
  `StashAddToStack` with self = other = the bag grid, so the bag-to-stash
  group runs on the bag grid (the source). Every shape below names its self
  explicitly and never uses `Console_Save_obj` by habit.

### Static reading 2: after Live 1

Read on 2026-09-28, after Live 1 (§ Live 1 results), in the same local
project, to explain each of that session's refusals before the next one runs.
Every item is a **static reading** unless it says **measured** or **read from
the save**; a body that was not read says so.

- **(a) The item owners.** `GetItemOwnerStr` names thirteen owner numbers: 0
  `localPlayer1`, 1 `localPlayer2`, 2 to 5 `onlinePlayer1` to `onlinePlayer4`,
  6 `merchant`, 7 `merchantTraveling`, 8 `merchantBlackMarket`, 9 `stash`, 10
  `stashPact`, 11 `stashGuild`, 12 `tradeOther`. None of them is a personal
  stash. `GetItemMap` picks the map by that number: for owners 0 and 1 it asks
  the profile getter, which depends on its self (`docs/RUNTIME_DATA_MODELS.md`
  § 9.4); owners 2 to 5 read one member of an instance by index; owners 6 to
  12 each read one member of an instance (on the replan's reading
  `Controller_obj` for 6 to 11 and the trade window for 12; owner 9's member
  is the measured `stashInventoryMap`, `docs/RUNTIME_DATA_MODELS.md` § 17). It
  has one further case past 12, **not read**. So the map 9 lookup reads
  neither its self nor its other: Live 1's `Console_Save_obj` self was not the
  problem. **Measured** for the personal tab (Live 1b `lookup-control`, Live 1c
  `hand-personal-map`, Live 1d `byname-personal-clear`): a personal-tab key
  answers an item struct on map 0 and undefined on map 9, before and after a
  move into the personal tab. **Not established** for the shared tabs: every
  shared-tab key looked up (Live 1b, 1c and 1d, including one the game itself
  had just moved there with its owner step from 0 to 9) answered undefined on
  both map 0 and map 9 through the one lookup form of § Phase A shapes, so
  which map holds a shared-tab entry, and why the owner 9 lookup misses it,
  are still open.
- **(b) `GetItemFromFingerprint`** takes the key and the owner. It refuses an
  undefined or non-string key, then asks `GetItemMap` for that owner's map and
  answers the map's value for the key, or undefined. Nothing else. So Live 1's
  undefined for a personal-tab key on map 9 is the game's answer: that key's
  entry is not in the stash map.
- **(c) `StashAddToStack`** answers true only after a merge. It answers false
  for an item class outside 12 to 15; otherwise it walks the cell array's two
  levels and, for a cell whose item has the same identity and passes
  `ItemCheckHash`, merges through `InventoryStackUpdateAndRemove` (four
  arguments: the stash item, the routine's own third argument, the moved item
  and the count) and answers true. A failed hash reports through
  `ReportClient` and answers false; no cell of that identity answers false.
  So Live 1's two false answers - a class 14 material of base id 71, with no
  stack of base id 71 on the Materials tab - are the game's "nothing to merge
  into", not a refused shape. A new identity into a special tab does not go
  through this routine at all; on the processor's reading it goes through the
  tab placement (`GridAddItem` on the tab's array).
- **(d) Names that point at the personal stash and at the quick move.** The
  item struct carries a member named `inPersonalStash` (the old-format item
  converter reads it). The inventory window's hint labels include
  `quick_move`, `equip_use`, `pick_up_item`, `place`, `split_stack`,
  `compare` and `inspect`, beside the key label `CTRL`. The key bindings file
  (`controls2.ini`) stores bindings as unnamed numbers, so which key the quick
  move is bound to is not readable from it; (e) is the evidence.
- **(e) Measured, from Live 1's own screenshot**
  (`C:\Users\stann\AppData\Local\HSDriveMcp\screenshots\20260928T000546134915Z_gestures-setup.png`,
  on the owner's machine, cited not copied; stash open on the personal tab,
  bag on its first page): the inventory window's hint strip reads `LMB: Pick
  up`, `RMB: Equip/Use`, `CTRL + LMB: Quick Move`, `ALT: Inspect`, `SHIFT:
  Compare`, `SHIFT + LMB: Split Stack`. The game's own quick move is Ctrl +
  left click, which Live 1 never tried (its right click equipped the item, as
  the strip says it would). The owner confirmed it on 2026-09-28: a hand move
  from the bag into the stash is Ctrl + click. **Measured by hand** in Live 1c
  (`hand-personal`, `hand-shared`, `hand-material`, `hand-merge`: the owner's
  own Ctrl + left click, each moving the item into the tab on show). A
  scripted click never reached the grid's pick-up in Live 1b
  (`click-control`), so no scripted Ctrl + click has been measured.
- **(f) The bag's item classes.** Live 1's bag grid held classes 6, 7 and 10
  only; `hs-game-sdk`'s `ItemType` names them Shield (6), Ring (7) and Charm
  (10), beside Weapon (3), Material (14) and Potion (18). Potions sit in the
  bag window's separate `PotionGrid`, so a non-stackable test item is any
  single-cell class 6, 7 or 10 item in the bag grid, never a potion.
- **(g) Read from the save** (`tools/save_item_keys.py`, the hub's tool, on
  the copy of slot 14's files taken before Live 1): the personal-tab stash
  item Live 1 used as its map 9 control is saved in the character's own file,
  under its `inventory` field's `personal_stash`; the shared tabs are saved in
  `stash.hss`; and the bag's tabs are saved in a third file,
  `inventory_order_13.hss`, not in the character file. The personal stash
  therefore saves with the character, which fits (a): a personal-tab item has
  no stash owner to be in. **Measured** after moves in Live 1c and Live 1d
  (`saved-stash-has-keys`, `saved-bag-lacks-keys`): a key moved into the
  personal tab saves under `herosiege13.hss`'s `inventory.personal_stash`, one
  moved into shared tab 1 under `stash.hss`'s `stash_tab_1`, a material stack
  moved into the Materials tab under `stash.hss`'s `material_tab`, and none of
  them under a bag container of `inventory_order_13.hss` any more.
- **Not read**: which of `m_MoveItemToGrid`'s ten owner changes a bag-to-stash
  move takes, the processor's third `GridAddItem` argument, and where the
  processor clears the bag cell after a grid placement. Live 1b was to log
  all three from the game's own gesture; its scripted click never reached the
  pick-up, so Live 1c logged them from the owner's hand moves instead:
  its armed row (`InvGrid15345`, the `anon@15345` closure) logged nothing in
  any hand move, whose logged sequence is instead the processor's
  bag-to-stash group of § Static reading; the third `GridAddItem` argument was 0 on every tab
  kind (**measured**, Live 1c); and no armed row logged a clear after a hand
  grid placement, though the bag cell emptied (which routine clears it is
  still **not observed**). By name, the placement leaves the bag cell as it
  was and a separate `InvGridClearItemNode` on the item's anchor cell empties
  it (**measured**, Live 1d `byname-personal-clear`, `byname-shared`).

**The hypothesis Live 1b tests**, written before the session, each claim with
the check that decides it:

1. A personal-tab item's map entry stays in the character's own map (owner
   0), and a shared-tab item's is in the stash map (owner 9) - `lookup-control`
   (one personal-tab and one shared-tab key, each on both maps), then the map
   reads in `gesture-ctrl-personal` and `gesture-ctrl-shared`.
2. So a move into the personal tab needs no owner step, and a move into a
   shared tab needs the owner changed from 0 to 9 - whether `ChangeItemOwner`
   logs in `gesture-ctrl-personal` and in `gesture-ctrl-shared`, then
   `grid-move-map` and `grid-move-shared-map`.
3. The game's own quick move is Ctrl + left click on the bag cell, handled by
   the processor's bag-to-stash group: the stack add, else the tab
   placement, then the source clear - `click-control` (the instrument), then
   `gesture-ctrl-personal` and `gesture-ctrl-shared`, whose logged rows are
   the shapes.
4. The array the placement is handed is the shown tab's own cell array (the
   stash grid node's `nodeGrid`, the same array as `Controller_obj`'s tab
   array or a mirror of it) - `grid-array-identity`.
5. Replaying by name exactly the shapes that group logs, into the shown tab's
   array, with the owner step only where the gesture logged one, reproduces
   the move - `grid-move-byname` (personal) and `grid-move-shared-byname`
   (shared).
6. A merge by name works once a stack of the same identity is on the tab -
   `stack-move-byname`; a new material identity goes through the tab
   placement - `gesture-ctrl-material`, then `mat-new-byname`.
7. Against a full shown tab the game's own quick move either leaves the item
   in the bag or spills it to another tab - `gesture-ctrl-full`, the only
   check `targetTabRule` is set from.

**What Live 1b, 1c and 1d found**, hypothesis by hypothesis. Live 1b measured
only the lookups: its scripted click never started the grid's pick-up
(`click-control` fail), so it sent no gesture and ran no grid block. Live 1c
put the owner's own Ctrl + left click in the gestures' place (the positive
controls for every replay), and Live 1d replayed the hand moves' logged
sequences by name. Each result names its check.

- Hypothesis 1: **half measured.** A personal-tab entry stays on map 0
  (1b `lookup-control`, 1c `hand-personal-map`). A shared-tab entry answers
  on neither map 0 nor map 9 through the one lookup form (1b, 1c
  `hand-shared-map`, 1d `byname-shared-owner`); where it lives is **not
  established**.
- Hypothesis 2: **measured.** The hand move into the personal tab logged no
  `ChangeItemOwner` (1c H1); the one into shared tab 1 logged one, after the
  placement and the stash grid's item check, with self the stash grid, other
  the bag grid, and the owner numbers 0 then 9 and the key as text (1c H2).
  Replayed by name after a confirmed placement it left the item in its
  shared-tab cells and turned its lookups into the shared-tab answer (1d
  `byname-shared-owner`). Replayed alone, with no placement, it took the key
  off map 0 while the item still sat in its bag cell - the half state the
  game's own save does not survive - and was reversed at once (1c step 8).
- Hypothesis 3: **measured by hand, with a correction.** The owner's Ctrl +
  left click moved the item into the tab on show every time (1c
  `hand-personal`, `hand-shared`, `hand-material`, `hand-merge`). The logged
  sequence is the bag-to-stash group of § Static reading, not the
  into-the-bag group: the item check on the bag grid, `StashAddToStack`
  (false when no stack of that identity is on the tab), `GridAddItem` on the
  tab's array (third argument 0, fourth undefined), `s_InvNode` (whose self
  is a struct), the item check again with the stash grid as self, and on a
  shared or Materials tab the owner step. `InventoryGridCanAddToStack`,
  `InventoryGridAddToStack`, `InventoryGridAddItemToTab` and
  `GetStashMaxTabs` never logged. A scripted click through `hs_input` did
  not reach the pick-up (1b `click-control`).
- Hypothesis 4: **measured.** The array the hand moves' `StashAddToStack` and
  `GridAddItem` were handed is the stash grid node's own `nodeGrid` read with
  that tab on show: the same array object, by the id the log prints, for the
  personal tab and for shared tab 1 (1c steps 7 and 8). `Controller_obj` has
  no member `stashPersonalGrid` on this build (1c step 7). The Materials
  tab's array is `Controller_obj.stashMaterialTab` (1c step 9).
- Hypothesis 5: **measured**, once `GridAddItem` is in the replay. Live 1c
  replayed only the group's named rows, which left `GridAddItem` out, and
  moved nothing (1c `byname-personal`, `byname-shared`: not-observed). Live
  1d replayed the logged sequence with `GridAddItem` included: the item
  landed in the personal tab and in shared tab 1 at the cell the answer
  named (1d `byname-personal`, `byname-shared`), the bag cell emptied only
  after a by-name `InvGridClearItemNode` on the item's anchor cell (1d
  `byname-personal-clear`, `byname-shared`), and after a close, a reopen and
  the saved files each moved key was in the stash and in no bag container
  (1d `reopen-shows`, `saved-stash-has-keys`, `saved-bag-lacks-keys`).
- Hypothesis 6: **measured.** With a stack of the same identity on the
  Materials tab, `StashAddToStack` by name answered true and the tab's sum
  rose by exactly one unit; the source clear then emptied the bag cell (1c
  `byname-merge`, replaying the hand merge of `hand-merge`). A new material
  identity went, by hand, through `StashAddToStack` (false), `GridAddItem`
  on the Materials tab's array and the owner step from 0 to 9 (1c
  `hand-material`); that placement was not replayed by name.
- Hypothesis 7: **measured by hand: it stays in the bag.** Against the full
  shared tab 2, the owner's Ctrl + left click logged `StashAddToStack`
  (false) and `GridAddItem` (`success=false`) and nothing after them - no
  `GetStashMaxTabs`, no tab walk, no clear, no owner step; the item stayed in
  its bag cell and the full tab's filled count was unchanged (1c
  `hand-full`). The tab walk § Static reading found behind a failed
  placement did not run for this input. Live 1d's by-name replay against the
  same tab answered the same (`byname-full`), which is expected by
  construction and not what `targetTabRule` is set from.

### Static reading 3: the button

The owner asked on 2026-09-28 for a **Move all** button in the stash window,
left of the backpack's Sort button, with F4 kept ("Build it into #68"). This
subsection is what the local reading of the game's UI node routines says
about how such a button could be made and could reach the plugin, written in
our own words with names only, each item a **static reading** unless it says
measured. Live 1f (§ Live procedure 1f) measures it.

- **Making and removing a node.** `UiCreateNode` runs with the window that
  will own the node as its self and takes, in order, an x, a y, the object to
  create, an activation and a call-stack name: it creates an instance of that
  object, places it, binds the activation as a method of the new node when
  the activation is callable (and leaves the node's `activationFunc`
  undefined otherwise), stores the call-stack name as the node's
  `uiNodeCallstack`, adds the node to a list the window keeps, and answers the
  node. Measured: the prospect research logged it with a grid name as its
  fifth argument and the restart research with `UI_Button_obj` as its third,
  which fixes the order. `UiSetActivationFunc` sets a node's `activationFunc`
  the same way, so an undefined activation is a state the game's own API
  makes; grids and labels carry none. `UiRemoveNode`, with the owning window
  as self, finds the node in that list, drops the entry and destroys the node.
  The bag's tab switch (`InventoryResetTabs`, run by each tab click) removes
  only the window's active grid nodes, so a button node is not expected to go
  with it (unverified: Live 1f `node-survives-tab-switch`).
- **The Sort button.** The bag grid's struct binds `InventorySortTab` as one
  of its own methods, and no direct call of `InventorySortTab` exists anywhere
  in the image, so the Sort button's activation is expected to be that method
  (or a closure calling it). Its body clears the bag grid and re-adds every
  item, reading the grid's own members off its self, so it must never run with
  a button as self: it is rejected as the Move all node's activation. Which
  code creates the Sort button was found in none of the decompiled closures
  and is **not read**; Live 1f reads the button hook-free instead (`stashmoveall
  probe sort`, and `menulayout UI_Button_Small_obj`, whose rows end in
  `text=`).
- **The dispatch script `<S>` (read 2026-09-28, this round).** A node's
  activation must name a game script (a method value cannot wrap plugin
  code), so the plugin can only see a press by hooking the script the node is
  bound to, and that script must be harmless if the hook were ever blind. The
  two candidates were read:
  - `UiSetFloatingToFalse` calls no other named script and reads no grid. It
    does nothing unless one member of its self reads true; when that member
    does, it rewrites a few members of its self and sets one element of an
    array held by a global object, at an index taken from another member of
    its self. So it does not write only its self (the global element is the
    exception), but on a node whose gating member is not true it writes
    nothing at all. The game itself calls it from `ControllerCheckInput`, on
    the instances of `UI_Hud_Talent_obj` whose one member matches a value the
    handler reads (a loop over an object, so children of `UI_Hud_Talent_obj`
    are taken too), together with `UiUnhideRow` and `UiSetFocus`. So a hook on
    it sees the game's own calls too and must forward every call whose self
    is not the mod's node; and if `UI_Button_Small_obj` were
    `UI_Hud_Talent_obj` or a child of it, that loop could call the candidate
    with the mod's node as self with no click at all, which would read exactly
    like a dispatched press. `stashmoveall probe create` checks this by name
    (its `loop check` line: the node object's parents, and whether it is that
    object or a child of it); a `CONFOUND` there makes `UiSetFloatingToFalse`
    unusable as the activation. Other callers (a method value dispatching it,
    which no direct-call scan shows) are **not read**; the unbound node and
    the idle reads of § Live procedure 1f are the controls for them.
  - `UiNodeClearNavigationFunc` calls `UiSetFocus` (it moves the UI focus),
    `DirEnumToAngle` and `CheckSensorInstance` (it reads what is under the
    cursor), and a builtin: it acts on the scene, not only on its self.
  - **Chosen: `UiSetFloatingToFalse`** as `probe create`'s script and Route
    A's candidate, being the closer fit of the two to "writes only its
    argument or self and reads no grid" (neither fits it exactly: the global
    array element above). The variable names behind those members could not
    be recovered from the build, so which flag gates it and which global
    array it touches are **not read**. Live 1f showed this choice unsafe: a
    click on a node bound to it ended the game (the last bullet).
- **Where a click is dispatched.** `ControllerCheckInput` reads the key
  bindings and opens or toggles windows (the inventory, the talents, the
  minimap, the loot filter, a potion, a town portal, the weapon loadout); no
  call of a node's `activationFunc` was found in it. So where the game calls a
  node's activation on a click, and what it does with an undefined one, are
  **not read**. Route B (the frame poll on a node left unbound) is measured on
  exactly that case in Live 1f (`node-press-poll-unbound`). (Live 1f then
  found where: the node's own user event 15, the last bullet below.)
- **The rows Live 1f arms** beside Live 1e's: `UiCreateNode`, `UiRemoveNode`,
  `UiMoveNode`, `InventorySortTab` and `InventoryResetTabs` (rows already),
  and four new `craftprobe` rows by their SDK constants, after `ValidateItem`
  and before the `CheckPlayerInteraction` control: `UiSetActivationFunc` (the
  binder), `UiSetFocus` (a hot row: every hovered frame calls it, so it is
  armed with a small budget and read only for the self it logs),
  `UiSetFloatingToFalse` and `UiNodeClearNavigationFunc` (the two candidates).
  291 rows in all; the marker reads `phase1k rows=291`.
- **What Live 1f and Live 1g measured, and the reading after them.** Each
  item is marked measured (M) or a static reading (R); § Live 1f results and
  § Live 1g results carry the rows.
  - (M) **How a click reaches a node's activation.** In Live 1f a scripted
    click at the centre of the node bound to `UiSetFloatingToFalse` made the
    stash window's Step run the node's user event 15, which called the bound
    activation with the node as self, the stash window as other, and one
    argument, the node's own `activationArgs` array (empty for the mod's
    node; the Sort node's holds 1). The research build's detour of that
    script logged exactly this call as the plugin's last line before the
    crash, so Route A's dispatch does reach the plugin.
  - (M) **The crash.** The script then read, as a bool, a member the button
    object does not carry, and the runner ended the game with no dialog. Its
    own error log (`crash.txt` in the game's local data folder, written in
    the click's minute) names a user-defined event 15 of `UI_Node_Parent_obj`
    and the error "bool argument is unset", with a trace running from
    `UiSetFloatingToFalse` up through that event to the Step events of
    `UI_Parent_obj`, `UI_Inventory_Parent_obj` and `UI_Stash_obj`, and it
    names `InventorySort` as the last UI node. The toolkit's own log showed
    the same chain natively: the script, the plugin's detour, the runner's
    method-call helper, the node's event. So the candidate was wrong, not the
    dispatch: the reading above that the script "writes nothing at all" on a
    node whose gating member is not true was wrong, because on this runner an
    unset member is an error, not false. The script belongs to
    `UI_Hud_Talent_obj`. Route A is dropped for this feature (the owner's
    decision of 2026-09-28, "Test the watch route"); a script safe for a
    `UI_Button_Small_obj` self would reopen it.
  - (R) **What the event does with an undefined activation.** The node's
    user event 15 (unnamed in the local project, found through its own
    call-stack-name text) first applies to the node's `activationFunc` the
    same one-argument check `UiSetActivationFunc` applies before it binds a
    value, read as an undefined check. When the activation is undefined the
    event does nothing: no sound, no call, no write. When it is set and a
    second member check passes, it plays the node's click sound if it has
    one, calls the activation with the node's `activationArgs`, and stores
    the node's call-stack name in a global (the error log's last UI node).
    The Step body of `UI_Parent_obj` that decides which node gets event 15
    is **not read**; that it sends it to the mod's node on a click inside
    the node's box is measured (the crash above).
  - (M) **The unbound node, Live 1g.** A node made by `UiCreateNode` with
    the activation left undefined (`UiSetActivationFunc` never called) took
    a scripted click inside its box with the game running on: the frame
    poll counted exactly one press inside the node, no armed row ran with
    the node as self, and no dialog appeared. Two idle reads six seconds
    apart were flat, and a click on the panel background between the node
    and Sort was counted by the poll as a press outside both buttons only.
    So a click on an unbound node set off no armed routine and no dialog
    (that it runs nothing of the game's at all is the static reading of an
    undefined activation, not measured), and the plugin's end-of-frame
    poll sees it: this is Route B, the route the
    player build uses.
  - (M) **The Sort button.** Its node is a `UI_Button_Small_obj` with
    `uiNodeCallstack` `InventorySort`, text `Sort Tab` (not `Sort`), sprite
    `Inventory_Tab_Button_Solid_spr` and `activationArgs` holding 1; its
    activation is `InventorySortTab` bound with the Sort node itself as self
    (an instance, not the bag grid's struct the reading above expected). The
    stash side's own sort button reads `StashSort`, also `Sort Tab`. The
    player build finds the bag's Sort by its call-stack name, never by its
    text.
  - (M) **The node's lifetime.** `UiCreateNode` answered the node with
    `visible=0` in the same frame and `visible=1` one frame later, drawn
    with the object's own sprite, `Menu_Button_Chat_spr`, text `Move all`,
    placed left of Sort by Sort's width plus 8 GUI units and clear of the
    bag grid. It stayed listed, with the same id and visible, through a
    switch of the bag to its Materials sub-tab and of the stash to shared
    tab 1; `UiRemoveNode` with the stash window as self and other removed
    it; the stash's own close destroyed a listed node; and a reopen listed
    none.

### Static reading 4: the stack cap and the button's origin

ForgePact #131. The owner reported on 2026-09-30 that the shipped button was
"positioned wrong. its right bottom corner is in the middle of the correct
position", and that items stayed in the bag with room on the tab, "especially
materials and socketables"; the Materials tab can hold several stacks of one
item, 999 each, and the Socketable tab only one. Both causes were settled from
the code, the earlier captures and one local reading, without a new session.
Labels as in § Static reading 3: (R) read locally, in our words; (M) measured.

- (R) **The stack cap.** `StashAddToStack`, after checking that the item's
  class is one of the stackable classes (12 to 15), takes a cap from its
  sixth argument: 999 when that argument does not carry flag 8, 999999 when
  it does. It then walks the array it was handed; for each item of the moved
  item's identity it merges only when that stack's count plus the moved count
  stays at or below the cap (through `InventoryStackUpdateAndRemove`) and
  answers true; a stack that would pass the cap is passed over for the next
  one, and after the last it answers false. So the first stack in array order
  that fits the whole count takes it, a stack is never topped up with part of
  an item, and a merge that finds no stack with room answers false. The
  measured merges pass 0 on the pages and the Materials tab and 8 on the
  Socketable tab (Live 1c to 1g), so a Materials or page stack caps at 999 and
  a socketable stack at 999999. Not read: whether the count added is the fifth
  argument or the item's own `o` (the mod passes the whole count as the fifth,
  so both readings agree), and the walk order beyond "array order". Live 3
  later measured the 999 cap on the Materials tab (§ Decision,
  `stackCapRule`); the 999999 cap stays this reading.
- (M, from the code and Live 2) **Why items stayed.** The first release sent a
  stackable to the stack routine whenever the identity's sum on the tab was
  above 0, so a material whose only stack held 999 went to a merge the game
  refused, answered false, and was skipped with a free cell beside it; nothing
  tried a new stack. And the Socketable tab's multi-unit merge was off
  (`socketWholeStackMerge`), so every bag socketable stack of more than one was
  a planned skip - most bag socketables are stacks.
- (M) **The node's origin.** Live 1f and 1g measured the same numbers: the
  Sort node at x 2303.5, y 1198.9 with the bbox 2303.5, 1198.9, 2485.9,
  1261.6 (its origin at its bbox top-left), and the Move all node at x 2113.1,
  y 1198.9 with the bbox 2016.2, 1176.1, 2211.9, 1221.7 (sprite
  `Menu_Button_Chat_spr`; its origin within one GUI unit of its bbox centre).
  `UiCreateNode`'s x, y are the new node's origin, and the first release
  computed them as though that origin were the top-left, as Sort's is: Sort's
  x less its width less 8, and Sort's y. The box was centred on the point
  meant for its top-left corner, and its bottom-right corner (2211.9, 1221.7)
  fell inside the box it should occupy, near its centre - the owner's report.
  The node's extents about its origin are left 96.9, up 22.8, right 98.8,
  down 22.8; the target (right edge 8 left of Sort, vertical centre Sort's) is
  the origin 2196.7, 1230.25 and the bbox 2099.8, 1207.45, 2295.5, 1253.05 at
  the 2560x1440 GUI of save slot 14's sessions. The measured width, 195.7, is
  not a whole sprite size, so a GUI scale is in play that the sprite functions
  do not know: the mod reads the extents from the node itself.
- **Rejected:** moving the node by writing its x and y (whether the UI layer
  draws and hit-tests from them or from members its parent's step recomputes
  is unread and unmeasured; remove-and-create is measured); the origin from
  the sprite's size (the scale above); a fixed offset from these numbers
  (right today, silently wrong after a sprite or scale change); keeping the
  sum rule and letting the game decide (its false leaves the item in the bag
  beside a free cell); passing 1 as the placement route's count (were the game
  to merge there, a stack with room for one unit would take one unit of a
  larger stack); and splitting an item across two stacks (not what the game's
  own Ctrl + click does).

### Static reading 5: the Sort button's look

ForgePact #131, owner scope of 2026-09-30: "Button should be the size and look
of sort tab button. Space is just enough for it", after Live 3 placed the node
right but at 206x48 beside Sort's 192x66 (§ Live 3 results, `button-placed`).
The question was which of a node's variables carry its sprite and size, and
whether a write on the mod's own node can give it Sort's look without calling
a routine beyond `UiCreateNode` and `UiRemoveNode`. Read locally on
2026-09-30: `UiSetNodeScale`, `UI_Layout_Apply_Sprite`, `UI_Node_HasSprite`,
`GetProfileButtonSprite`, and again `InventoryInitGrids`, `UiCreateNode` and
the closures of `UI_Inventory_Parent_obj`'s Create event. Labels as in
§ Static reading 3.

- (M) **The sprite is set after the node is made.** Live 1f and 1g read the
  Sort node's `sprite_index`, by name, as `Inventory_Tab_Button_Solid_spr`,
  while the node the mod made - the same object, `UI_Button_Small_obj` - reads
  and draws the object's own `Menu_Button_Chat_spr`. `UiCreateNode` takes the
  x, y, the object, the activation and the call-stack name, and no sprite, so
  whatever gives Sort its sprite runs after the create.
- (R) **Where Sort is made is still not read.** `UiCreateNode` is called
  directly only from `InventoryInitGrids` (the bag's sub-tab buttons); every
  other caller reaches it through the script table, and the code that makes
  the Sort node is among neither the named scripts nor the named closures of
  this build.
- (R) **How the game gives a node its look.** Right after making each sub-tab
  button, `InventoryInitGrids` writes one member on it - the same member
  `UI_Node_HasSprite` checks for and `UI_Layout_Apply_Sprite` reads as the
  node's sprite. A node's size is two scale members: `UiSetNodeScale(node, sx,
  sy)` sets each to its argument times a global factor (the GUI scale: the
  same pair of globals `UiResizeInventoryNodes` and the craft and split-stack
  windows read) and then calls a method the node carries; a closure of
  `UI_Inventory_Parent_obj`'s Create event writes the same two members, times
  the same factors, directly. `UI_Layout_Apply_Sprite` fits a node to a size
  from its sprite's dimensions, through `UiSetNodeScale` and `UiMoveNode`. So
  the look is plain variable writes on the node: nothing the mod would have
  to call.
- **Not read: the members' names.** This build reads every variable through a
  slot number the runner hands out at start-up, and the slot table the
  toolkit extracts does not cover these ones. That the sprite member is
  `sprite_index` and the two scale members `image_xscale` and `image_yscale`
  is inference, from what they do and from the measured `sprite_index` read
  above. Nor is it read whether the UI layer puts a node's own look back on a
  later step (`UI_Parent_obj`'s Step is large and unread, and so is the
  method `UiSetNodeScale` calls). The mod therefore reads the look again each
  ensure step until the node is judged, and Live procedure 4 measures it
  (`button-look`, `button-state`).
- (M, from the numbers) **One GUI scale for both nodes.** Live 1f and 1g read
  Sort at 182.4x62.7 and the node at 195.7x45.6, Live 3 at 192x66 and 206x48;
  both ratios are 1.0526. Copying Sort's scale is then either nothing (the two
  already carry the same) or what makes the sizes equal; the size rule on the
  settled box decides, not the write.
- (R) **Where it lands.** Wearing Sort's sprite, the node's origin is that
  sprite's, its top-left, so its extents are Sort's own (left 0, up 0, right
  192, down 66 at Live 3's GUI) and `ButtonOrigin` gives x = 2290 - 8 - 192 =
  2090, y = 1262: the box 2090, 1262, 2282, 1328, on target at the first
  creation. Its `menulayout` `gui=` is then its top-left, not its centre.
- **Rejected:** a different object whose own sprite might be the tab look (its
  click is unmeasured, while an unbound `UI_Button_Small_obj`'s click ran
  nothing, Live 1g, and Sort itself is one); Sort's sprite looked up by its
  asset name (a patch that renames or restyles Sort would leave it silently
  wrong, while Sort's own is right by construction); calling `UiSetNodeScale`
  or `UI_Layout_Apply_Sprite` (a routine call beyond the two measured ones,
  ending in a method call on the node whose body is unread); stretching the
  chat sprite to Sort's box by scale alone (Sort's size, not its look).

### Static reading 6: the label and the Mercenary button

ForgePact #131, after Live 4 (§ Live 4 results). Live 4's node took Sort's
sprite and size, but its `Move all` label was drawn at the box's top-left
corner and clipped. The owner then asked, on the same screenshot, for the
button to take the place of the game's own **Mercenary** button, which the
game draws in that spot when the bag is open without the stash: "Use its
coordinates because what you did right now is still a little misaligned".
Two questions follow: which of a node's members places its label, and what
the Mercenary button is and whether it can be read while the stash is open.
Read locally on 2026-09-30 with the named Ghidra project: the Create closure
of `UI_Button_Open_Mercenary_obj`, five Create closures of `UI_Parent_obj`,
two of `UI_Node_Parent_obj`, `UiLabel` and `UiAOpenMercenaryInventory`.
Labels as in § Static reading 3; R is a static reading.

- (R) **The member reads could not be named.** This build reads every member
  through a slot number the runner hands out at start-up. The slot table the
  toolkit extracts resolved none of the member slots these routines use, and
  their builtin calls go through unnamed pointers. The object events
  themselves (each object's Create and Draw) are not named functions in the
  project. So which member places a node's label, and which draw path the
  Sort node takes to centre its own, is **not established**, and it is not
  readable with this tooling in a reasonable time.
- (R, names only) **What the SDK does name.** `UI_Button_Open_Mercenary_obj`
  is object 5004, and `gml_Script_UiAOpenMercenaryInventory` and
  `gml_Script_UiAOpenInventory` exist by name (`hs-game-sdk`'s `objects.hpp`
  and `scripts.hpp`).
- **Not established: the Mercenary button.** That the button the owner saw
  is a `UI_Button_Open_Mercenary_obj` is not established. It may be a
  `UI_Button_Small_obj` with its own call-stack name. Also not established:
  whether it is listed while the stash is open (the owner saw it with the bag
  alone), and whether the bag's `InventorySort` is listed with the bag alone.
- (M, Live 4) **What the screenshot does show.** The Sort node's own label
  is drawn centred in its box. The node's is not, while it carries Sort's
  sprite and scale and reads them back. So whatever centres Sort's label is
  either a member Sort carries and the node lacks or holds differently, or
  something outside the node's members. Only a live comparison of the two
  nodes' members, and a live trial of copying them, can tell these apart.

**Why the fix waits on a session.** Nothing above names the member to copy
or the box to copy from, and guessing either would repeat Live 4: a copy that
reads back and passes every numeric check while the drawn button stays wrong.
So the research build gains an instrument first (`stashmoveall probe dump`,
`diff` and `lookcopy`, described in § Live procedure 5), and a screenshot check,
`tools/button_label_check.py` in the toolkit hub, measures where a label is
drawn. Live procedure 5 measures both nodes and the Mercenary button and
tries the copy live. The fix is written from that measurement.

## Instrument

The instrument is `craftprobe` (research build only; the toolkit guide's
Command Reference and `crafting-materials-research.md` § Instrument). This
feature adds one row and nothing else:

- **Row `ValidateItem`**, by its `hs-game-sdk` constant, after
  `UiACloseButton` and before the `CheckPlayerInteraction` control, which
  stays last. The marker still reads `craftprobe: phase1k rows=` and now
  counts 287 rows. The row logs self, other, arguments and answer like every
  row, so each by-name move below shows whether the game's own item check ran
  and what it answered.

Everything else is `craftprobe`'s existing surface: `hook` detours every row
by name, `arm budget=N <rows>` logs each call, `show` prints them, `node
bag|stash|id:<n>` sums a grid's cells by fingerprint, `var` and `path:` read
a member, `methods id:<n>` names a method value's script, and `call` or
`callm`, each behind the literal `confirm`, is the one write. Argument forms:
numbers, `undefined`, `true`/`false`, `id:<n>`, `fp:<key>` (the map 0 item),
`fp9:<key>` (the map 9 item), `path:<Obj|id:n>.<a.b.c>`, `kept:<Row>`, and a
key given as plain text (`0-0-<n>-<class>`), which reaches the game as a
string. The `fp:` and `fp9:` forms feed an item to a routine; no map check in
this document uses them, or the stash window as a self (§ Phase A shapes,
"The map lookup").

The recording rule is `stash-bag-layout-research.md` § Instrument's: every
by-name call is recorded with the logged shape beside the supplied shape, and
its outcome is exactly one of `reproduced`, `shape not reproduced (<field>:
logged <x>, supplied <y>)` or `not-run (instrument: <why>)`. The last two are
never a route negative.

### Phase A shapes

Research build `plugin_build\BloodPactPlugin_rel.dll`, build sha256=e18d3198e5e24e7357f34a80d91eee0b4b91d679cbdf9d6c80692c3ec5c79a3f (`plugin_build\build.bat dev`, this branch's Phase A commit). The build is not byte-reproducible: every rebuild of the same source gives a new hash, so the session installs the DLL whose hash is written here (a copy is kept beside it as `BloodPactPlugin_rel.phaseA-e18d3198.dll`), or this line is updated to the rebuilt DLL's hash before the session.

The lines Live 1 runs, in order. Each line is the command up to and including
`confirm`; the text after `-> expect:` is the expected reply, not part of the
command.
The placeholders are read from `menulayout` in the same session: `<bag>` the
`UI_Inventory_Grid_obj` whose `gridName` is `InventoryGrid`, `<sg>` the
`UI_Inventory_Grid_obj` the stash lists for the
tab on show, `<x>,<y>` the bag cell of the item being moved (the `cell=` of
its first row), `<K_J>` a non-stackable key (ends `-18`), `<K_J2>` a second
non-stackable bag key for the full-tab checks, `<K_S>` a key `craftprobe node
stash` lists in a stash cell, `<K_M>` a material key (ends `-14`) and `<o>`
that material stack's count (`node bag`). Before
each block, read the cell array the block names with `craftprobe var` (for
`path:id:<sg>.nodeGrid`, `craftprobe var id:<sg> nodeGrid`); if
`nodeGrid.<x>.<y>` reads `undefined` where `menulayout` lists the item, the
axis order is the other way round - use `nodeGrid.<y>.<x>` and write down
which.

**Dispatcher control** (the shape the prospect research proved on this build;
`docs/RUNTIME_DATA_MODELS.md` § 9.7):

```
craftprobe call InventoryGridCanAddToStack id:<bag> other:<bag> 1 undefined fp:<K_M> confirm   -> expect: dispatched #<n> -> ret=undefined, or an item struct (a bag stack of K_M's kind)
```

**The map lookup: one form, measured selfs.** Every map check in this
document, before and after every block, uses exactly these two lines, with
the key given as text and the owner as a number. The map 0 line runs with
self = other = the bag grid node, the self `docs/RUNTIME_DATA_MODELS.md`
§ 9.3 measured for a map 0 lookup. The map 9 line runs with self
`Console_Save_obj`, given by name as in `crafting-materials-research.md`'s
`save-route` line; that self was measured with the stash closed, so running
it with the stash open is exactly what the control below tests. No map check
uses the stash window as its self (no record measures it, and the research
build's own lookup helper says the stash-side self is unmeasured), and none
uses the `fp:` or `fp9:` argument forms, which resolve with the call's own
self.

```
craftprobe call GetItemFromFingerprint id:<bag> other:<bag> <key> 0 confirm   -> expect: an item struct while the entry is in map 0, undefined once it has left
craftprobe call GetItemFromFingerprint Console_Save_obj <key> 9 confirm   -> expect: an item struct while the entry is in map 9, undefined otherwise
```

**Lookup controls** (check `lookup-control`), after the dispatcher control
and before any grid block: the map 0 line with `<K_J>` while `<K_J>` is still
in the bag, and the map 9 line with `<K_S>`, both quoted.

```
craftprobe call GetItemFromFingerprint id:<bag> other:<bag> <K_J> 0 confirm   -> expect: an item struct (K_J is in the bag)
craftprobe call GetItemFromFingerprint Console_Save_obj <K_S> 9 confirm   -> expect: an item struct (K_S is in a stash cell)
```

`lookup-control` is `pass` only when both answer an item struct. If either
does not, **no grid block runs at all** - neither order below, nor the
full-tab block - so no stash cell can end up holding an item whose entry is
still in map 0 (the half state that ends the game at the next stash save);
`grid-move-byname`, `grid-move-map` and `tab-full-byname` are then recorded
`not-run (instrument: lookup control failed)` with both replies quoted. The
stack block still runs (it has no owner step); its map lines are recorded but
decide nothing.

**Grid item into the stash grid tab on show, first order** - runs only when
`lookup-control` is `pass`. The processor's own bag-to-stash group, self =
other = the bag grid, as `StashAddToStack` was logged. The placement, then
the source clear, then the two lookup lines with `<K_J>`, then the owner step
only when those lines say the entry is still in map 0 (the map 0 line
answers an item struct) and not in map 9 (the map 9 line answers
`undefined`); after the owner step, the two lookup lines once more, and those
last replies are what `grid-move-map` quotes:

```
craftprobe call GridAddItem id:<bag> other:<bag> path:id:<sg>.nodeGrid fp:<K_J> 0 undefined confirm   -> expect: a struct with success=true, tabNumber, x, y; success=false is "no room" and nothing changed
craftprobe call InvGridClearItemNode id:<bag> other:<bag> path:id:<bag>.nodeGrid.<x>.<y> undefined confirm   -> expect: true or undefined, and menulayout lists no bag cell holding K_J afterwards
craftprobe call ChangeItemOwner id:<sg> other:<bag> 0 9 <K_J> confirm   -> expect: ret=undefined (its normal answer); only when the two lookup lines put K_J in map 0 and not in map 9
```

`GridAddItem`'s third argument is supplied as `0`, the value #14 measured by
name into the bag's Socketable grid; the processor passes one of two
constants there that were not read (§ Static reading). Its supplied-shape
column therefore reads `a2=0 (processor's constant not read)`, and a refusal
or an odd save after it is recorded as `shape not reproduced (a2)`, never as
a route negative.

**Grid item, second order** - runs only when `lookup-control` is `pass`, and
only if the first order's placement is refused or does not appear in the
stash: the destination grid's own
`m_MoveItemToGrid`, in the click-in shape the prospect research measured
(self = the destination grid, other = the source grid, no argument), after
arming its row (`InvGrid15345` is the row's short name for
`UI_Inventory_Grid_obj anon@15345`). It takes the item from the drag state, not from an argument
(§ Static reading), so a `dispatched` reply with nothing moved is recorded
as `not-observed (the item comes from the drag state)`, never as a route
negative:

```
craftprobe arm budget=20 InvGrid15345 ChangeItemOwner GridAddItem s_InvNode ValidateItem
craftprobe callm id:<sg> inst m_MoveItemToGrid other:<bag> confirm   -> expect: dispatched #<n>; K_J listed in a stash cell and in no bag cell, and ChangeItemOwner logged with self <sg>, 0, 9 and K_J
```

**Full tab by name, an instrument check** (check `tab-full-byname`) - runs
only when `lookup-control` is `pass`, and only when a stash grid tab with no
free cell exists in the slot: show it (`hs_stash_tab`, its free-cell count
quoted from `menulayout`, the tab remembered for the gesture below), read
`<sg>` again for that tab, and run the first order's placement for `<K_J2>`:

```
craftprobe call GridAddItem id:<bag> other:<bag> path:id:<sg>.nodeGrid fp:<K_J2> 0 undefined confirm   -> expect: success=false, and the shown tab and the bag read unchanged
```

`pass` when it answers `success=false` and both sides read unchanged. That
answer is expected by construction - the by-name call searches only the one
array it is handed and cannot reach the processor's tab walk - so this check
shows only that the mod's own by-name placement cannot spill; it is never
evidence for `targetTabRule`. `not-run (no full tab in the slot)` when every
tab has a free cell. Should it answer `success=true` after all, the tab was
not full: finish the placement as the first order does (the source clear,
then the owner step decided by the two lookup lines) and record `not-run
(tab not full)`.

**Stackable into the Materials tab** - the six-argument shape #14 logged for
the hand move, with the whole stack's count as the fifth argument, then the
same source clear:

```
craftprobe call StashAddToStack id:<bag> other:<bag> path:Controller_obj.stashMaterialTab 9 2 fp:<K_M> <o> 0 confirm   -> expect: true, and node stash shows that kind's sum risen by <o>
craftprobe call InvGridClearItemNode id:<bag> other:<bag> path:id:<bag>.nodeGrid.<x>.<y> undefined confirm   -> expect: true or undefined, and no bag cell holds K_M afterwards
```

If the whole-stack count is refused (`false`, both sides unchanged), repeat
the stack add once with `1` as the fifth argument - #14's logged value - and
record which count the game took.

**The map re-read after the stack block** is the two lookup lines above with
`<K_M>`; for a merged stack the game's own merge deletes the merged unit's
entry (#14), so `undefined` on both maps is the expected answer there. When
`lookup-control` failed these replies are recorded and decide nothing.

**Full tab through the game's own quick move** (check `grid-move-tab-full`,
research) - the measurement for `targetTabRule`, and not a by-name line.
Only the game's own input reaches the processor's tab walk, so this repeats,
with the full tab of `tab-full-byname` shown (free cells 0 by `menulayout`),
a single-input quick move on `<K_J2>` (its cell point confirmed on a
screenshot). The qualifying gesture is `gesture-rightclick`, else
`gesture-shiftclick`: the first with verdict `pass` whose item was read in a
stash cell afterwards. `gesture-clickclick` never qualifies, whatever its
verdict: it picks the item up and places it into a chosen cell, and on a
full tab there is no empty cell to choose, so the drop either does nothing
or lands on an occupied cell and swaps - an answer from the instrument, not
the game. Before the gesture, record the shown tab's cell list; after it,
read the bag, the shown tab, and each other stash grid tab in order
(`hs_stash_tab` and `menulayout`) until `<K_J2>` is found, and the two
lookup lines with `<K_J2>`. A stash key that left its cell, or now appears
in the bag, means the input swapped rather than quick-moved. `pass` with the
tab quoted (stayed in the bag, the shown tab, or tab `<n>`, a spill) and the
shown tab's cell list unchanged apart from `<K_J2>`. `not-run (no full tab
in the slot)`, `not-run (instrument: no single-input quick-move moved an
item)`, `not-run (instrument: cell point unconfirmed)` or `not-run
(instrument: a stash item was displaced - <key> from <cell>)` otherwise.

### Phase A' shapes

Research build `plugin_build\BloodPactPlugin_rel.phaseA-e18d3198.dll`, build sha256=e18d3198e5e24e7357f34a80d91eee0b4b91d679cbdf9d6c80692c3ec5c79a3f - the kept copy of Live 1's build, unchanged: no rebuild for Live 1b. Every row Live 1b arms is already a `craftprobe` row of that build (checked by name against the table on 2026-09-28): `InventoryGridCanAddToStack`, `InventoryGridAddToStack`, `InventoryGridAddItemToTab`, `InvGridClearItemNode`, `StashAddToStack`, `InventoryGridAddItemPos`, `InventoryGridAddItem`, `InventoryGridHasSpace`, `ValidateItem`, `ChangeItemOwner`, `RemoveItemFromMap`, `AddItemToMap`, `InventorySwapItemsNew`, `StashGridAddItem`, `GridAddItem`, `GridRemoveItem`, `s_InvNode`, `InvStartDragging`, `InvCopyItemDragData`, `InvCopyItemToInvDragData`, `InventoryStackUpdateAndRemove`, `StashAddItemOnline`, `GetStackOpLocationFromGridType`, `GetItemOwnerFromStackOpLocation`, `InvGrid15345` and the control `CheckPlayerInteraction`, plus `GetItemFromFingerprint` for the lookups.
Live 1c, Live 1d and Live 1e ran on the same kept copy, also with no rebuild:
the rows Live 1c added to the armed list (`GetItemFromFingerprint`,
`GetItemMap`, `GetStashMaxTabs`, `GridAddToStack`, `UiASplitStack`,
`ItemCheckHash`, `GetItemOwnerStr`), the row Live 1e added
(`GetItemPreferredGrid`), and every row Live 1d and Live 1e call are rows of
the same table.

The lines Live 1b runs, in order, in the same form as § Phase A shapes (the
command up to `confirm`, then `-> expect:`). What Live 1 changed about them:
the game's own quick move is **Ctrl + left click** (§ Static reading 2, (e)),
so every by-name block now copies what that gesture logs instead of guessing;
the lookup controls use one personal-tab key and one shared-tab key; the merge
needs a stack of the same identity on the tab first; and `nodeGrid` is indexed
`[y][x]` on the bag and stash grids (measured in Live 1), so a cell `<x>,<y>`
is the path `nodeGrid.<y>.<x>` everywhere below.

Placeholders, read in the same session: `<bag>` the `UI_Inventory_Grid_obj`
whose `uiNodeCallstack` is `InventoryGrid`; `<sg0>`, `<sg1>`, `<sg2>` the
`StashGrid` node while the personal tab, shared tab 1 and shared tab 2 are on
show; `<mg>` the bag's Materials grid node (`UI_Stash_obj.invGrid` while the
bag's Materials sub-tab is on show); `<K_J1>` to `<K_J6>` six single-cell
class 6, 7 or 10 bag keys with their `<x>,<y>`; `<K_S0>` a personal-tab key
and `<K_S1>` a shared-tab-1 key; `<K_MA>` the bag's largest material stack,
`<K_MB>` another material identity, `<K_MU>` the one-unit key
`hs_give_item` makes from `<K_MA>`'s template, `<K_MX>` a bag material whose
base id the Materials tab already holds.

**Dispatcher control** (check `byname-control`), after arming the rows:

```
craftprobe arm budget=400 InventoryGridCanAddToStack InventoryGridAddToStack InventoryGridAddItemToTab InvGridClearItemNode StashAddToStack InventoryGridAddItemPos InventoryGridAddItem InventoryGridHasSpace ValidateItem ChangeItemOwner RemoveItemFromMap AddItemToMap InventorySwapItemsNew StashGridAddItem GridAddItem GridRemoveItem s_InvNode InvStartDragging InvCopyItemDragData InvCopyItemToInvDragData InventoryStackUpdateAndRemove StashAddItemOnline GetStackOpLocationFromGridType GetItemOwnerFromStackOpLocation InvGrid15345 CheckPlayerInteraction
craftprobe call InventoryGridCanAddToStack id:<bag> other:<bag> 1 undefined fp:<K_MA> confirm   -> expect: dispatched #<n> -> ret=undefined, or an item struct
```

**Lookup controls** (check `lookup-control`): the one lookup form of § Phase A
shapes, on a personal-tab key and a shared-tab key, each on both maps.

```
craftprobe call GetItemFromFingerprint id:<bag> other:<bag> <K_S0> 0 confirm   -> expect (hypothesis 1): an item struct
craftprobe call GetItemFromFingerprint Console_Save_obj <K_S0> 9 confirm   -> expect (hypothesis 1): undefined, as in Live 1
craftprobe call GetItemFromFingerprint id:<bag> other:<bag> <K_S1> 0 confirm   -> expect (hypothesis 1): undefined
craftprobe call GetItemFromFingerprint Console_Save_obj <K_S1> 9 confirm   -> expect (hypothesis 1): an item struct
```

`pass` when each key answers an item struct on exactly one map, whichever map
that is; the map each answered on is `personal-map` and `shared-map`, and
whether a struct reply lists `inPersonalStash` among its members is recorded.
Both maps or neither for a key is `fail`: then no by-name grid block runs
(the placements below are `not-run (instrument: lookup control failed)`),
and the gestures still run, since they are the game's own moves.

**Click control** (check `click-control`), before any gesture: a plain left
click with the default hold (never `hold_ms`) on `<K_J1>`'s bag cell, whose
hint is `LMB: Pick up`, then the reads, then a second click on the same point,
which should put it back (`LMB: Place`):

```
hs_input click <K_J1>'s cell point (left, default hold)
craftprobe show   -> expect: InvStartDragging or InvCopyItemDragData calls rising
craftprobe var id:<bag> nodeGrid.<y>.<x>   -> expect: undefined (the item is on the cursor)
hs_input click the same point (left, default hold)   -> expect: menulayout lists <K_J1> in its cell again
```

**The game's own quick move** (checks `gesture-ctrl-personal`,
`gesture-ctrl-shared`, `gesture-ctrl-full`), each after a `craftprobe show`
so the new lines are attributable, with the tab named on show and the bag on
its first page. The gesture is one `hs_input` sequence: `key_down` vk 17
(Ctrl), `wait` 50, `click` the bag cell (left, default hold), `wait` 50,
`key_up` vk 17. After it: `craftprobe show`, `craftprobe node bag`,
`craftprobe node stash`, `menulayout UI_Inventory_Grid_obj`, and the two
lookup lines for the moved key. Every armed row that logged is quoted with
`self=`, `other=`, each `a<n>=` (an array with its `len`/`len0`) and `ret=`:
those are the shapes the by-name blocks replay.

Each gesture has three outcomes the reads tell apart, because the click
control has already shown what a plain click does. The item in a stash cell
and in no bag cell is `pass`. The item still in its bag cell, with no new
`InvStartDragging` or `InvCopyItemDragData` line, is `not-observed`. The bag
cell empty, the key in no stash cell and a drag row logged means the game did
not see Ctrl: the click was a plain `Pick up` and the item is on the cursor.
Nothing in this toolkit has yet shown that a Ctrl held through `hs_input`
(a scancode key-down) reaches the game, so that outcome is
`not-run (instrument: Ctrl not seen - the click picked the item up)`, never
a result about the quick move. The item is put back at once, before any tab
switch, with one plain left click on the same point and a `menulayout` read
of its cell; after it in the first gesture, the later gestures are not sent
(`not-run (instrument: Ctrl not seen in gesture-ctrl-personal)`), the by-name
blocks use their no-gesture shapes, and `gestureRoute` reads not-observed for
the instrument's reason.

- personal tab shown, `<K_J2>` - `gesture-ctrl-personal`; then
  `grid-array-identity` on its landing cell:

  ```
  craftprobe var Controller_obj 0 stashPersonalGrid.<y>.<x>   -> expect: <K_J2>'s node
  craftprobe var id:<sg0> nodeGrid.<y>.<x>   -> expect: the same node (one array, or a mirror)
  ```

- shared tab 1 shown, `<K_J3>` - `gesture-ctrl-shared` (whether
  `ChangeItemOwner` logs, with its self and arguments, is the shared tabs'
  owner rule);
- shared tab 2 shown with 0 empty cells, `<K_J4>` - `gesture-ctrl-full`: the
  tab's cell list before, the bag, the tab, then each other tab in order until
  `<K_J4>` is found (the bag, the shown tab, or tab `<n>`, a spill). This
  gesture is sent only when `gesture-ctrl-personal` or `gesture-ctrl-shared`
  passed earlier in the same session. "Stayed in the bag" is also what a
  gesture reads when it does nothing at all (Ctrl not seen, a point off by a
  cell, the window not focused), so only a Ctrl + left click that has already
  moved an item into the stash lets the full-tab answer mean anything. With
  neither passed it is
  `not-run (instrument: no Ctrl + left click moved an item this session)`.

**Grid item by name into the personal tab** (checks `grid-move-byname`,
`grid-move-map`) - runs only when `lookup-control` passed. The placement for
`<K_J5>` copies the self, other, array (matched by `len`/`len0` against
`id:<sg0>.nodeGrid` and `Controller_obj.stashPersonalGrid`) and third
argument exactly as `gesture-ctrl-personal` logged `GridAddItem`; when the
gesture logged no placement, the first line below, then the second as the
second try. Then the source clear, then the two lookup lines, then the owner
step only if the gesture logged one for this tab (replayed with `<K_J5>`) -
or, with no gesture logged, only if `personal-map` is 9 and `<K_J5>` still
answers on map 0:

```
craftprobe call GridAddItem id:<bag> other:<bag> path:id:<sg0>.nodeGrid fp:<K_J5> 0 undefined confirm   -> expect: a struct with success=true (or the logged shape's own self, array and third argument)
craftprobe call GridAddItem id:<bag> other:<bag> path:Controller_obj.stashPersonalGrid fp:<K_J5> 0 undefined confirm   -> expect: success=true; only when the line above was refused and no gesture logged a placement
craftprobe call InvGridClearItemNode id:<bag> other:<bag> path:id:<bag>.nodeGrid.<y>.<x> undefined confirm   -> expect: true or undefined, and no bag cell holds K_J5
craftprobe call ChangeItemOwner id:<sg0> other:<bag> 0 9 <K_J5> confirm   -> expect: ret=undefined; only under the owner-step rule above
```

`grid-move-map` passes when `<K_J5>`'s two lookup replies match `<K_S0>`'s
(the same map answers, the other is `undefined`); otherwise run the undo
lines below before anything else.

**Second order, only if the first order's placement is refused** (`success=false`
or a throw): start the game's own drag on the source cell by name, then the
destination grid's own `m_MoveItemToGrid` in the click-in shape the prospect
research measured. On a static reading `InvStartDragging` takes two
arguments - the item's key and the source cell's node, whose member picks the
item's owner - looks the item up on that owner's map and copies the key, the
item and the cell into the drag data; it has never been called by name, so a
refusal here is `shape not reproduced`, never a route negative:

```
craftprobe call InvStartDragging id:<bag> other:<bag> <K_J5> path:id:<bag>.nodeGrid.<y>.<x> confirm   -> expect: dispatched; InvCopyItemDragData logged
craftprobe callm id:<sg0> inst m_MoveItemToGrid other:<bag> confirm   -> expect: dispatched; K_J5 in a personal-tab cell and in no bag cell
```

**Grid item by name into shared tab 1** (checks `grid-move-shared-byname`,
`grid-move-shared-map`): the same block for `<K_J6>` (or `<K_J4>` if it stayed
in the bag) with `<sg1>` and the array, third argument and owner step as
`gesture-ctrl-shared` logged them; with no gesture logged, the owner step runs
when `shared-map` is 9 and the key still answers on map 0. The replies are
compared with `<K_S1>`'s.

```
craftprobe call GridAddItem id:<bag> other:<bag> path:id:<sg1>.nodeGrid fp:<K_J6> 0 undefined confirm   -> expect: success=true (or the logged shape)
craftprobe call InvGridClearItemNode id:<bag> other:<bag> path:id:<bag>.nodeGrid.<y>.<x> undefined confirm   -> expect: true or undefined, and no bag cell holds K_J6
craftprobe call ChangeItemOwner id:<sg1> other:<bag> 0 9 <K_J6> confirm   -> expect: ret=undefined; as gesture-ctrl-shared logged it, or under the owner-step rule
```

**Full tab by name, an instrument check** (check `tab-full-byname`): shared
tab 2 on show with 0 empty cells, the first order's placement only, for a
non-stackable still in the bag. `success=false` with both sides unchanged is
expected by construction and is never evidence for `targetTabRule`; a
`success=true` means the tab was not full - finish the block as above and
record `not-run (tab not full)`.

```
craftprobe call GridAddItem id:<bag> other:<bag> path:id:<sg2>.nodeGrid fp:<K_J1> 0 undefined confirm   -> expect: success=false; <sg2> and the bag read unchanged
```

**Materials** (checks `gesture-ctrl-material`, `stack-move-byname`,
`mat-new-byname`), last, so the grid steps ran with the bag on its first
page: the Materials tab on show and the bag's Materials sub-tab on show. The
merge target is prepared first: `hs_give_item` into the bag, with `<K_MA>`
as the template and a count of 1, makes `<K_MU>`, a one-unit stack of `<K_MA>`'s identity; the Ctrl +
left click on `<K_MA>`'s cell in `<mg>` then puts `<K_MA>`'s whole stack on
the tab (`gesture-ctrl-material`, its rows quoted: on the static reading
`StashAddToStack` answers false, then a placement into the tab's array, then
the source clear). Before the merge by name, the Materials tab is read for a
stack of `<K_MU>`'s base id (`<K_MX>`'s when the one-unit give was refused).
With none on the tab no call is made and `stack-move-byname` is
`not-run (instrument: no merge target on the tab)`: the routine answers false
when there is nothing to merge into (§ Static reading 2, (c)), so a false then
says nothing about the route, which is how Live 1's two falses came about. The
merge by name uses the self, other and sixth argument
`StashAddToStack` logged in that gesture if it logged one; otherwise as
written, and once more with `<mg>` as self and other if the first answers
false with the merge target present. Then the source clear on `<K_MU>`'s cell
in `<mg>`. A merge deletes the merged unit's entry, so both lookup lines
answering `undefined` for `<K_MU>` is the expected read. Then the new
identity: the placement the gesture logged, replayed for `<K_MB>` (`not-run
(no reproducible shape logged)` when it logged none).

```
craftprobe call StashAddToStack id:<bag> other:<bag> path:Controller_obj.stashMaterialTab 9 2 fp:<K_MU> 1 0 confirm   -> expect: true, and the tab's sum for that base id up by exactly 1
craftprobe call StashAddToStack id:<mg> other:<mg> path:Controller_obj.stashMaterialTab 9 2 fp:<K_MU> 1 0 confirm   -> expect: true; only when the line above answered false with the merge target on the tab
craftprobe call InvGridClearItemNode id:<mg> other:<mg> path:id:<mg>.nodeGrid.<y>.<x> undefined confirm   -> expect: true or undefined, and no bag cell holds K_MU
craftprobe call GridAddItem <self as logged> <other as logged> path:Controller_obj.stashMaterialTab fp:<K_MB> <a2 as logged> undefined confirm   -> expect: success=true, and the tab gains K_MB's base id with its count
```

**The hand moves' sequences, replayed by name (Live 1d)** (checks
`byname-personal`, `byname-personal-clear`, `byname-shared`,
`byname-shared-owner`, `byname-full`). Live 1c's owner moved one item by hand
into each tab kind; the rows each hand move logged (§ Live 1c results) are
what these lines replay, in logged order, with the placement `GridAddItem`
included - Live 1c's own replays left it out, because the procedure replayed
only the rows of the group § Static reading named, and moved nothing.
`s_InvNode` is never replayed: its self is a struct, and `craftprobe call`
takes an instance self only; a by-name `GridAddItem` sets it off itself
(Live 1d logged it nested under each successful placement). Placeholders, read
in the same session: `<bag>` the bag grid node (`InventoryGrid`), `<sg>` the
`StashGrid` node, which rebinds to whichever stash tab is on show (so
`path:id:<sg>.nodeGrid` is that tab's own array), `<K_P>` and `<K_Q>` two
non-stackable bag keys (Live 1d: two 1 by 3 charms), `<K_R>` a third, and
`<x>,<y>` a key's anchor cell in the bag (the top-left cell of its footprint;
`nodeGrid` is `[y][x]`). The bag is on its first page throughout.

Into the personal tab (personal tab on show), then the source clear, only
when `GridAddItem` answered `success=true` and the key is still in a bag
cell; no owner step:

```
craftprobe call ValidateItem id:<bag> other:<bag> fp:<K_P> confirm   -> expect: ""
craftprobe call StashAddToStack id:<bag> other:<bag> path:id:<sg>.nodeGrid 0 13 fp:<K_P> 1 0 confirm   -> expect: false (no stack of a non-stackable)
craftprobe call GridAddItem id:<bag> other:<bag> path:id:<sg>.nodeGrid fp:<K_P> 0 undefined confirm   -> expect: a struct with success=true and the landing x, y
craftprobe call ValidateItem id:<sg> other:<bag> fp:<K_P> confirm   -> expect: ""; only after success=true
craftprobe call InvGridClearItemNode id:<bag> other:<bag> path:id:<bag>.nodeGrid.<y>.<x> undefined confirm   -> expect: undefined, and no bag cell holds K_P
```

Into shared tab 1 (shared tab 1 on show): the same with the shared tab's
constants, then the owner step last, only after the placement is confirmed
and the bag cell is clear:

```
craftprobe call ValidateItem id:<bag> other:<bag> fp:<K_Q> confirm   -> expect: ""
craftprobe call StashAddToStack id:<bag> other:<bag> path:id:<sg>.nodeGrid 9 2 fp:<K_Q> 1 0 confirm   -> expect: false
craftprobe call GridAddItem id:<bag> other:<bag> path:id:<sg>.nodeGrid fp:<K_Q> 0 undefined confirm   -> expect: success=true and the landing x, y
craftprobe call ValidateItem id:<sg> other:<bag> fp:<K_Q> confirm   -> expect: ""; only after success=true
craftprobe call InvGridClearItemNode id:<bag> other:<bag> path:id:<bag>.nodeGrid.<y>.<x> undefined confirm   -> expect: undefined, and no bag cell holds K_Q
craftprobe call ChangeItemOwner id:<sg> other:<bag> 0 9 <K_Q> confirm   -> expect: undefined; K_Q still in its shared-tab cells, its lookups now as a shared-tab key's
```

Against the full shared tab 2 (on show, 0 empty cells): the placement
answers no room and nothing after it runs - no clear, no owner step:

```
craftprobe call ValidateItem id:<bag> other:<bag> fp:<K_R> confirm   -> expect: ""
craftprobe call StashAddToStack id:<bag> other:<bag> path:id:<sg>.nodeGrid 9 2 fp:<K_R> 1 0 confirm   -> expect: false
craftprobe call GridAddItem id:<bag> other:<bag> path:id:<sg>.nodeGrid fp:<K_R> 0 undefined confirm   -> expect: success=false; the bag, the full tab and every other tab unchanged
```

Every line above answered as expected in Live 1d (§ Live 1d results). The
merge into an existing Materials stack is Live 1c's step 9, the first and
third lines of the **Materials** block above, with the bag's Materials
sub-tab on show (`<mg>` measured as the bag grid node itself, rebound to the
sub-tab); it answered true and the sum rose by one (§ Live 1c results,
`byname-merge`).

**The special tabs' routes, replayed by name (Live 1e)** (checks
`byname-material-new`, `byname-merge-whole`, `byname-socket`,
`byname-socket-merge`). Live 1e's owner moved one new material identity and
three socketables by hand (§ Live 1e results); these lines replay what those
moves logged, in logged order, `s_InvNode` left out as above. Placeholders,
read in the same session: `<bag>` the bag grid node, rebound to the bag's
Materials or Socket sub-tab on show; `<sg>` the `StashGrid` node; `<K_Y>` a
bag material whose identity has no stack on the Materials tab (Live 1e: base
id 73, a stack of 908); `<K_X>` a bag material whose identity the hand move
had just put on the tab, and `<n>` its count (Live 1e: base id 71, 15);
`<x>,<y>` a key's anchor cell in the bag's sub-tab; `<K_SK2>` a bag
socketable of a kind the tab accepts (a rune, a gem or an orb, never a jewel
or an Incarnation Gem) whose identity has no stack on the Socketable tab;
`<K_SKU>` a one-unit socketable whose identity is on the tab; `<skarr>` the
Socketable tab's array exactly as the hand move logged it (one row long, a
different array for each item in Live 1e, and matched to no `path:` form in
that session).

A new material identity (Materials tab and the bag's Materials sub-tab on
show), then the source clear while the key is still in a bag cell, then the
owner step, only after `success=true`. The hand move's first `ValidateItem`
(self and other the bag grid) was not in Live 1e's replay:

```
craftprobe call StashAddToStack id:<bag> other:<bag> path:Controller_obj.stashMaterialTab 9 2 fp:<K_Y> 1 0 confirm   -> expect: false (no stack of that identity on the tab)
craftprobe call GridAddItem id:<bag> other:<bag> path:Controller_obj.stashMaterialTab fp:<K_Y> 0 undefined confirm   -> expect: success=true and the landing x, y
craftprobe call ValidateItem id:<sg> other:<bag> fp:<K_Y> confirm   -> expect: ""; only after success=true
craftprobe call InvGridClearItemNode id:<bag> other:<bag> path:id:<bag>.nodeGrid.<y>.<x> undefined confirm   -> expect: undefined, and no bag cell holds K_Y
craftprobe call ChangeItemOwner id:<sg> other:<bag> 0 9 <K_Y> confirm   -> expect: undefined; K_Y undefined on map 0 and an item struct on map 9
```

A whole stack merged onto its identity's stack (same tabs on show), the
source clear only after the tab's sum rose by exactly `<n>`:

```
craftprobe call StashAddToStack id:<bag> other:<bag> path:Controller_obj.stashMaterialTab 9 2 fp:<K_X> <n> 0 confirm   -> expect: true, and the tab's sum for that base id up by exactly <n>
craftprobe call InvGridClearItemNode id:<bag> other:<bag> path:id:<bag>.nodeGrid.<y>.<x> undefined confirm   -> expect: undefined, and no bag cell holds K_X
```

The Socketable tab (Socketable tab and the bag's Socket sub-tab on show),
**not run in Live 1e** (both checks `not-run`, § Live 1e results): the new
path as the rune and the gem logged it by hand, the sixth argument being the
8 those moves logged, then the merge as the orb logged it. `<skarr>` has to be
matched to a readable form first; without one neither block can run.

```
craftprobe call ValidateItem id:<bag> other:<bag> fp:<K_SK2> confirm   -> expect: ""
craftprobe call StashAddToStack id:<bag> other:<bag> <skarr> 9 2 fp:<K_SK2> 1 8 confirm   -> expect: false
craftprobe call GridAddItem id:<bag> other:<bag> <skarr> fp:<K_SK2> 0 undefined confirm   -> expect: success=true
craftprobe call ValidateItem id:<sg> other:<bag> fp:<K_SK2> confirm   -> expect: ""; only after success=true
craftprobe call InvGridClearItemNode id:<bag> other:<bag> path:id:<bag>.nodeGrid.<y>.<x> undefined confirm   -> expect: undefined, and no bag cell holds K_SK2; only if the placement left it there
craftprobe call ChangeItemOwner id:<sg> other:<bag> 0 9 <K_SK2> confirm   -> expect: undefined
craftprobe call StashAddToStack id:<bag> other:<bag> <skarr> 9 2 fp:<K_SKU> 1 8 confirm   -> expect: true, and the tab's count for that identity up by 1
craftprobe call InvGridClearItemNode id:<bag> other:<bag> path:id:<bag>.nodeGrid.<y>.<x> undefined confirm   -> expect: undefined, and no bag cell holds K_SKU
```

**The Socketable tab's merge and the button, by name (Live 1f and Live 1g)**
(checks `socket-copy`, `byname-socket-merge`, `byname-socket-nonstack`,
`sort-node`, `sort-activation`, `node-created`, `node-press-poll-unbound`,
`node-removed-by-name`). No person: the research build's copy verb makes a
one-unit copy of a stash socketable in the bag's Socket sub-tab, from the
stash map, the stash item left as it was. Placeholders, read in the same
session: `<bag>` the bag grid node with its Socket sub-tab on show; `<N_ORB>`
and `<N_GEM>` the one-cell `StashSocketGrid` nodes holding the orb (base id
118) and the gem (base id 38), found by reading each node on map 9;
`<K_ORB9>`, `<K_GEM9>` their keys; `<K_ORBU>`, `<K_GEMU>` the copies' keys;
`<x>,<y>` a copy's cell in the bag's Socket sub-tab; `<SORT_ID>` the
`UI_Button_Small_obj` whose `uiNodeCallstack` is `InventorySort`. The
Socketable tab's array is the item's own node's `nodeGrid` (one cell), which
settles the one-row array Live 1e's hand moves logged for the merge path;
the new placement stays unreplayed.

```
stashmoveall probe copy <K_ORB9> 1   -> expect: key=<K_ORBU>, a confirmed line, the bag's socket view up by one key
craftprobe call StashAddToStack id:<bag> other:<bag> path:id:<N_ORB>.nodeGrid 9 2 fp:<K_ORBU> 1 8 confirm   -> expect: true, and N_ORB's o up by exactly 1 (1f and 1g: 81 to 82)
craftprobe call InvGridClearItemNode id:<bag> other:<bag> path:id:<bag>.nodeGrid.<y>.<x> undefined confirm   -> expect: undefined, and no bag cell holds K_ORBU
stashmoveall probe copy <K_GEM9> 1   -> expect: key=<K_GEMU>, a confirmed line
craftprobe call StashAddToStack id:<bag> other:<bag> path:id:<N_GEM>.nodeGrid 9 2 fp:<K_GEMU> 1 8 confirm   -> 1f: true, and N_GEM gained o=2 (the gem is stackable)
craftprobe call InvGridClearItemNode id:<bag> other:<bag> path:id:<bag>.nodeGrid.<y>.<x> undefined confirm   -> expect: undefined, and no bag cell holds K_GEMU
```

The button, the stash open with any tabs on show. `probe create none`
leaves the activation undefined; the bound form `probe create <script>` is
not replayed (Live 1f: a click on the node bound to `UiSetFloatingToFalse`
ended the game):

```
stashmoveall probe sort id:<SORT_ID>   -> expect: the Sort row (uiNodeCallstack=InventorySort, text=Sort Tab), activation InventorySortTab self=instance id=<SORT_ID>, and sort poll armed
stashmoveall probe create none watch:UiSetFloatingToFalse   -> expect: created id=<node>, activation left unbound (none), activationFunc=undefined, a loop check line without CONFOUND; visible=1 one frame later
stashmoveall probe show   -> expect: poll_presses, poll_any_presses, poll_sort_presses, row_calls_self_node; a click inside the node raises poll_presses and poll_any_presses by 1 with in_node=1, and nothing else
stashmoveall probe remove   -> expect: UiRemoveNode with self=other=the stash window, listed after: no
```

**Undo**, for a placement whose maps end inconsistent (`grid-move-map` or
`grid-move-shared-map` fails), before any close: take the item back out of
the destination array with the remove shape `crafting-materials-research.md`
measured (Live 1e, `take-trial-grid`: self and other the tab's grid, the
grid's `nodeGrid`, the key as text, answered true and emptied the cell),
reverse the owner step if one ran, and put the item back in a bag cell with
the placement shape #14 measured into a bag grid:

```
craftprobe call GridRemoveItem id:<sg> other:<sg> path:id:<sg>.nodeGrid <key> confirm   -> expect: true, and the cell reads empty
craftprobe call ChangeItemOwner id:<sg> other:<bag> 9 0 <key> confirm   -> expect: ret=undefined; only if an owner step ran
craftprobe call GridAddItem id:<bag> other:<bag> path:id:<bag>.nodeGrid fp:<key> 0 undefined confirm   -> expect: success=true, the item in a bag cell and on map 0 again
```

## Live procedure

### Live procedure 1

The procedure is the one in this workorder's context file,
`.claude/workorders/forgepact-68-move-all-context.md` § "Live procedure 1"
(kept on the owner's machine with the plan): the research build above,
positive controls first (the marker, the hook line and a rising
`CheckPlayerInteraction`, then the dispatcher control, then the two lookup
controls, which gate every grid block), the grid block into the personal
tab, the full-tab instrument check by name, the stack block into the
Materials tab, the three hand gestures from the bag's grid page, the full
tab through a single-input quick move (right-click or shift-click, never
click-move-click), the bag sub-tab as a source, the close and reopen,
and the saved files. No person at the keyboard. Every by-name call follows
the recording rule under § Instrument, and each check is `pass`,
`not-observed` (with what was supplied) or `not-run (instrument: ...)`.

### Live procedure 1b

The procedure is the one in this workorder's context file,
`.claude/workorders/forgepact-68-move-all-context.md` § "Live procedure 1b"
(kept on the owner's machine with the plan): the same research build as Live 1
(§ Phase A' shapes), positive controls first (the marker, the hook line and a
rising `CheckPlayerInteraction`, the dispatcher control, the four lookup
controls, then the click control, which gates every gesture), the game's own
Ctrl + left click into the personal tab, into shared tab 1 and against the
full shared tab 2, the by-name grid blocks into the personal tab and shared
tab 1 copied from what those gestures logged, the full-tab instrument check
by name, then the Materials tab (the gesture, a one-unit merge by name, a new
identity by name), the close and reopen, and the saved files read with
`tools/save_item_keys.py`. Those save reads cover the bag's own file,
`inventory_order_<slot>.hss`, as well as `stash.hss` and the character file:
the bag is saved there, not in the character file (§ Static reading 2, (g)).
All three files are copied before the session and read again after it, in
one read with a `--key` per moved key; the bag check passes only when the
copy's read shows each moved key in a bag container first, which proves the
read can see the bag, and is `not-run` when the bag's file was not read.
No person at the keyboard; every click uses the default hold. Every by-name
call follows the recording rule under § Instrument, and each check is `pass`,
`fail`, `not-observed` (with what was supplied) or `not-run (instrument:
...)`.

### Live procedure 1c

The procedure is the one in this workorder's context file,
`.claude/workorders/forgepact-68-move-all-context.md` § "Live procedure 1c"
(kept on the owner's machine with the plan). **Hand-assisted**, by the
owner's decision of 2026-09-28, for this one session only: Live 1b showed a
scripted click does not start the grid's own pick-up, so the owner made the
quick moves the gestures were meant to make. The same research build as Live
1 and Live 1b (§ Phase A' shapes); the same positive controls first; then
four asks, one at a time, each after a re-arm and a baseline `craftprobe
show` so every logged line belongs to that ask: the owner's Ctrl + left click
into the personal tab, into shared tab 1, against the full shared tab 2, and
on the Materials tab (a stack as a new identity, then one unit merged onto
it). After each ask the operator read every armed row that logged, both
grids and the moved key's lookups. Then, with no person, by-name replays of
the shapes those hand moves logged - but only of the rows in the group §
Static reading named, which left out the placement `GridAddItem` - the
close by name only, the reopen, and the saved files read with
`tools/save_item_keys.py`. Every by-name call follows the recording rule
under § Instrument; each check is `pass`, `fail`, `not-observed` (with what
was supplied) or `not-run (instrument: ...)`. Each hand-move result is
**measured**, and its input is a person, not the instrument.

### Live procedure 1d

The procedure is the one in this workorder's context file,
`.claude/workorders/forgepact-68-move-all-context.md` § "Live procedure 1d"
(kept on the owner's machine with the plan): **fully automatic, no person
step**, on the same research build, the same slot and the saves Live 1c
restored. The same positive controls first; then Live 1c's hand-move
sequences replayed by name in logged order with `GridAddItem` included (§
Phase A' shapes, "The hand moves' sequences, replayed by name"): into the
personal tab, into shared tab 1 with the owner step only after a confirmed
placement, and against the full shared tab 2. The bag's source clear runs
only when the key is still in a bag cell after the placement, and before any
owner step. Then the close by name only, the reopen, and the saved files read
with `tools/save_item_keys.py`, with a positive control on each side. Every
by-name call follows the recording rule under § Instrument; each check is
`pass`, `fail`, `not-observed` (with what was supplied) or `not-run
(instrument: ...)`.

### Live procedure 1e

The procedure is the one in this workorder's context file,
`.claude/workorders/forgepact-68-move-all-context.md` § "Live procedure 1e"
(kept on the owner's machine with the plan). **Hand-assisted**, by the
owner's decision of 2026-09-28 to research the three routes Live 1d left
unmeasured (the Socketable tab with the bag's Socket sub-tab as the source, a
new material identity on the Materials tab, and a merge of more than one
unit) before the player build: the same research build and slot, the saves
Live 1d restored, and the same positive controls first. Then the owner's own
Ctrl + left click, one ask at a time, each after a re-arm and a baseline
`craftprobe show` so every logged line belongs to that ask: a socketable
from the bag's socket view into the Socketable tab, and a one-unit new
material identity into the Materials tab. After each ask the operator read
every armed row that logged, both grids and the moved key's lookups. Then,
with no person, by-name replays of those logged sequences with a second item
of the same kind (§ Phase A' shapes, "The special tabs' routes, replayed by
name"), a socketable unit merge, and a merge by name of a whole material
stack; then the close by name only, the reopen, and the saved files read with
`tools/save_item_keys.py`, with a positive control on each side. Every by-name
call follows the recording rule under § Instrument; each check is `pass`,
`fail`, `not-observed` (with what was supplied) or `not-run (instrument:
...)`. Each hand-move result is **measured**, and its input is a person, not
the instrument.

### Live procedure 1f

The procedure is the one in this workorder's context file,
`.claude/workorders/forgepact-68-move-all-context.md` § "Live procedure 1f"
(kept on the owner's machine with the plan). **No person step**, unless the
by-name activation also answers nothing, when one owner click on the node is
the last resort, asked once. Two questions, by the owner's decisions of
2026-09-28 ("one more session for socketables", "Build it into #68"): whether
a socketable merges into its stack on the Socketable tab by name, and how a
Move all node left of the bag's Sort button can reach the plugin (§ Static
reading 3).

- **Build**: the research build from this branch after the probe verbs,
  `plugin_build\build.bat dev`, kept as
  `plugin_build\BloodPactPlugin_rel.phaseC-fbca7251.dll` (untracked, as the
  Phase A copy is): build
  sha256=fbca7251d72bbfe8e80eacec56c0ee40a69dbbd762187fd28158ae0bc263449e.
  It replaces the first probe build (`phaseC-41558d1c`, on which no session ran),
  which lacked the instrument controls below. Its bare `craftprobe` prints
  `craftprobe: phase1k rows=291 - ...`.
- **The research build's verbs** (`stashmoveall probe`, research build only,
  not a player command): `sort [id:<n>]` prints the Sort node's row (id, x, y,
  bbox, sprite, visible, enabled, `uiNodeCallstack`, text) and its activation
  read hook-free (the method's script, the `craftprobe` row naming it, and
  whether its self is an instance or a struct); `create [<script>|none]` makes
  a `UI_Button_Small_obj` node through `UiCreateNode` with self and other the
  stash window, left of Sort by Sort's own width plus 8 GUI units, call-stack
  name `ForgePactMoveAll`, then binds `<script>` as its activation through
  `UiSetActivationFunc` (none leaves it unbound) and sets its text to `Move
  all` (the one write the probe makes, on its own node); `create ...
  watch:<script>` names the `craftprobe` row counted with the node as self
  (the bound script's by default, so an unbound node can still count the
  candidate), and `create` also prints a `loop check` line (§ Static reading
  3: whether the node's object is `UI_Hud_Talent_obj` or a child of it);
  `remove` runs `UiRemoveNode` with the same self (or destroys the probe's own
  node when the window is gone); `show` prints whether the node is listed and
  its counters: `detour_presses` (Route A: a call of the bound script's row
  whose self is the node), `row_calls_self_node` (the watched row's calls with
  the node as self, bound or not), `poll_presses` (Route B: a left press
  inside the node's bbox), and on a second line the poll's own controls,
  `poll_any_presses` (every left press the frame poll saw), `poll_sort_presses`
  (those inside the Sort node's bbox) and `last_press` (the last press's GUI
  x,y beside both bboxes as read at that frame). The poll is armed by `probe
  sort` or `probe create` and counts from then on, so the Sort click comes
  before any node exists; and `copy <template> <count>` copies a stash item
  into the bag by the give-item verb's loader order with the template read
  from the stash map, the stash item left as it was, so the socketable blocks
  need no person.
- **Instrument controls** (added after the round-0 review of this
  instrument, which found that a zero from the poll and a count from the
  detour could each be the instrument rather than the game):
  - *The poll's positive control is the Sort click* (`sort-click-control`):
    `probe show` before and after it; the poll works when `poll_any_presses`
    and `poll_sort_presses` each rose by exactly 1 and `last_press` lies
    inside `sort_bbox`. `node-press-poll` may be `not-observed` only when this
    control passed. Otherwise it is `not-run (instrument: poll saw no press)`
    when `poll_any_presses` did not rise, or `not-run (instrument: coordinates
    outside Sort's bbox)` when it rose but the press lies outside `sort_bbox`;
    and on the node click, a rise of `poll_any_presses` with `last_press`
    outside `node_bbox` is `not-run (instrument: the click missed the node)`,
    quoting both.
  - *The activation's two negative controls*, both required before
    `node-press-activation` can pass: (1) with the bound node listed and no
    click, two `probe show` reads a few seconds apart, between which
    `row_calls_self_node` and `detour_presses` do not rise; (2) one click on an
    unbound node made by `probe create none watch:<S>`, after which
    `row_calls_self_node` is still 0 (and the idle reads of (1) are repeated on
    that node). A rise in either, or a `CONFOUND` on the `loop check` line,
    makes `node-press-activation` `not-run (instrument: <S> is called with the
    node as self without a press)`, quoting the counts and the row's logged
    self. The unbound click is also `node-press-poll-unbound`'s measurement,
    so it runs whatever (c) found, after the bound node's click and its
    reads.
- **Recording rule**: as § Instrument's. Each check is `pass`, `fail`,
  `not-observed` (with what was supplied) or `not-run (instrument: ...)`; the
  node's fields are quoted from the probe's replies, and the socket merge's
  array and counts from the reads.

### Live procedure 1g

The procedure is the one in this workorder's context file,
`.claude/workorders/forgepact-68-move-all-context.md` § "Live procedure 1g"
(kept on the owner's machine with the plan). **No person step**, the
character select's last resort aside (it was not needed). By the owner's
decision of 2026-09-28 ("Test the watch route"), after Live 1f's bound click
ended the game: the same research build as Live 1f
(`phaseC-fbca7251`, no rebuild; every verb it runs is in that build), the
saves Live 1f restored, and the same positive controls first. It repeats
Live 1f's orb merge and closes the stash by name before the button block,
so the merge's save checks survive a crash. Then only an unbound node (the
activation left undefined) with the plugin's frame poll watching its box:
the Sort click as the poll's positive control, idle reads, one click inside
the node, one click on the panel background as the negative control, a bag
and a stash tab switch, the removal by name, a close with the node listed,
and a reopen. Its **crash rule**: if the game ends, the game's own error log
is read and quoted by its error line and trace names, the check in progress
is `fail (crash - ...)` with the verdict word first, and every later check is
`not-run`. **Recording rule**: as § Instrument's; each check is `pass`,
`fail`, `not-observed` (with what was supplied) or `not-run (instrument:
...)`.

### Live procedure 3

ForgePact #131's confirmation of § Static reading 4 and the fixed player
code. The procedure is Live procedure 1 of the workorder
`.claude/workorders/forgepact-68-move-all-fix-context.md` (kept on the owner's
machine with the plan); this is its summary. It runs the **research build**,
because its setup copies stash items into the bag with `stashmoveall probe
copy`, which the player build compiles out; the Move all code is the same
source in both builds. Save slot 14, the mod off at launch, the session's saves
backed up first and restored at the end. Positive controls first: the lease's
DLL hash (`dll-hash`), the state line (`marker`), a by-name tab switch
(`control`) and the first `probe copy` reading `confirmed` (`copy-control`).
Then, in order: with the mod off, no `ForgePactMoveAll` row beside Sort's
(`off-baseline-button`); on, exactly one, its bbox right edge within 1 GUI unit
of Sort's left less 8 and its vertical centre within 1 of Sort's
(`button-placed`), and a click at its centre starting one run
(`button-press`). On the Materials tab: bag materials whose kind had no stack
placed and those whose stack fit merged (`material-new`); a copy of a kind
with one stack sized so its merge would pass 999 placed as a new stack in a
new cell, the old stack unchanged (`material-overflow`); and a second copy
merged into the new stack while the full one stayed unchanged
(`material-partial`) - the two checks that measure the cap. On the Socketable
tab: an orb stack of 3 merged, its node rising by exactly 3 (`socket-whole`),
a single gem merged (`socket-single`), and a kind the tab lacks stayed in the
bag (`socket-new-stays`). Then the close and reopen (`close-survives`,
`reopen-shows`) and the saved files (`saved-stash-has-keys`,
`saved-bag-lacks-keys`, `no-duplicate`). The cases are the ordinary ones (a
page by the button, a Materials merge, a single socketable) and the outliers
that take another path (a full Materials stack, a partial beside a full one, a
socketable stack, a kind the tab lacks).

### Live procedure 4

The button's look and size (owner scope, 2026-09-30; § Static reading 5), on
the build that copies Sort's look. The procedure is Live procedure 2 of the
same workorder's context file; this is its summary. It runs the **research
build**, because it reads each node's sprite with `stashmoveall probe sort
id:<n>`, which the player build compiles out. Save slot 14, the mod off at
launch, the saves backed up first and restored at the end; no save checks, the
move path being unchanged since Live procedure 3. Positive controls first: the
lease's DLL hash (`dll-hash`), the state line, whose first line now carries
`button_look=none` (`marker`), and a by-name tab switch (`control`). Then:
with the mod off, no `ForgePactMoveAll` row beside Sort's
(`off-baseline-button`); on, after a tab switch and about 2 s, exactly one,
its bbox right edge within 1 GUI unit of Sort's left less 8, its vertical
centre within 1 of Sort's, and its width and height each within 1 of Sort's
(`button-placed`, expected about 2090, 1262, 2282, 1328 at Live 3's GUI); the
bare state line reading `button_place=on`, `button_look=sort`, a
`button_box=` within 0.1 of the `menulayout` box and a `button_size=` within 1
of Sort's (`button-state`); a click at the centre of its box - not at its
`gui=`, now its top-left - starting one run (`button-press`); `probe sort` on
Sort's id (the positive control, `Inventory_Tab_Button_Solid_spr` in Live 1f
and 1g) and on the node's reading the same sprite, not `Menu_Button_Chat_spr`
(`button-look`); then the close (`close-survives`) and a reopen whose node,
made from the extents measured on the first, is placed the same way with
`button_place=on button_look=sort` (`reopen-placed`). The cases are the
session's first node (made from Sort's own extents), a later open's node (made
from measured extents) and one press; no material or socketable case.

### Live procedure 5

The label and the Mercenary button (§ Static reading 6), measured before any
fix. The procedure is Live procedure 1 of the workorder
`.claude/workorders/forgepact-68-move-all-fix2-context.md` (kept on the
owner's machine with the plan); this is its summary. It runs the **research
build**, because its dumps, diffs and trial writes are research commands the
player build compiles out:

- `stashmoveall probe dump <label> id:<n>` reads one instance by name, after
  `instance_exists`: a fixed builtin list (`id`, `object_index`, `visible`,
  `sprite_index` with its sprite's name, `image_index`, `image_speed`,
  `image_blend`, `image_alpha`, `image_xscale`, `image_yscale`,
  `image_angle`, `depth`, `x`, `y` and the four `bbox_*`) and every instance
  variable `variable_instance_get_names` returns. Each value is printed with
  its kind (`real`, `int32`, `int64`, `bool`, `string`, `asset`,
  `reference`, `struct`, `array`, `method`, `undefined`), and the dump is kept
  under the label, eight at most, the oldest evicted. A struct prints its
  name count (`struct{<n>}`), an array its length and a method the script it
  wraps, and a struct or array variable's contents follow it as entries of
  their own, two levels down: `<member>.<name>` for a struct's variable,
  `<member>[<i>]` for an array's element, 64 per level and 2048 per dump. The
  header's `nested=` counts those entries and `nested_cut=` what the caps left
  unread. An instance handle inside is printed, never followed. Printed as one
  token, a struct or array member whose contents differ between two nodes
  would diff as equal, so a label place held inside one would read as "no
  member places it" (the review of this instrument before Live 5).
- `stashmoveall probe diff <a> <b>` prints `~` for a member both dumps hold
  with different values, `+` or `-` for one only one side holds, and a count
  line. An expanded entry is compared like any member, so a struct member
  whose contents differ shows as `~ <member>.<name>`.
- `stashmoveall probe lookcopy id:<src> missing|changed` writes onto the
  mod's own node only, and is refused with nothing written while the mod holds
  none. `missing` writes each member the source has and the node lacks;
  `changed` each member both have whose values differ. Only a number, bool,
  string or asset is written: a reference, struct, array, method or undefined
  never is. Nor are the members that say what the node is, where it is or
  what it does (`id`, `object_index`, `x`, `y`, `xstart`, `ystart`,
  `xprevious`, `yprevious`, the `bbox_*`, `uiNodeCallstack`, `activationFunc`,
  `activationArgs`, `text`, `visible`, `enabled`). Each write prints `wrote
  <name>=<value> read back <value>`, then a count line. An expanded entry the
  tier selects, inside a struct or array member the node also has, is never
  written: it gets a `skip <member>.<name>=... - inside a struct or array
  member, never written` line, so the capture names it, and the count line's
  `nested=` counts them.
- `stashmoveall probe help` prints the usage line naming every subcommand.

Save slot 14, the mod off at launch, the saves backed up first and restored
at the end. Every screenshot is `hs_screenshot` `target="game"`,
`method="grab_window"`, whose pixels are the client area the GUI maps onto.
Positive controls first: the lease's DLL hash (`dll-hash`), `probe help`
naming `dump`, `diff` and `lookcopy` (`marker`), and a by-name tab switch
(`control`). Then, with the bag open on its own: an `InventorySort` row that
is visible (`bag-alone-open`) and the Mercenary button's row, its object,
id, box, sprite, call-stack name and text, left of Sort and overlapping it
vertically (`merc-bag-read`). Dumps of both, where the Sort dump reading
`uiNodeCallstack=InventorySort`, `text=Sort Tab` and some instance variables
is the instrument's control (`dump-control`), and `tools/button_label_check.py`
run on the Mercenary box against Sort's (`merc-label`). With the stash open:
whether the Mercenary node is still listed and where (`merc-stash-listed`),
and whether `InventorySort`'s box is the same as with the bag alone
(`sort-same-both`). Then the mod's node (`node-made`), the label check
against Sort on its screenshot, whose reference line must read `centred`
(`label-tool-control`) and whose box line reproduces Live 4's defect
(`label-baseline`), and dumps and diffs of the node, both Sorts and the
Mercenary button (`dump-diff`). Then the trial: `lookcopy` from Sort with
`missing`, and with `changed` only if the first did not centre the label, each
judged by the label check (`label-trial-missing`, `label-trial-changed`); a
click on the node still counted as one press (`trial-press`); and the close
(`close-survives`), with whether Sort and the Mercenary row are still listed
right after it (`bag-after-close`). The cases are the backpack's Sort, the
stash's Sort, the Mercenary button (with the bag alone, and with the stash if
listed) and the mod's node, in two trial tiers.

### Live procedure 6

The confirmation of the shipped path Live 5 decided (§ Decision
`buttonTarget`, `buttonLabel`; § Ship design, the button's place and look):
the node on the Mercenary box, wearing Sort's 16 look members copied as read
(the two scales scaled), its label centred. It runs the **research build**,
because steps 4 and 5 read and write members with `probe dump`, `probe diff`
and `probe lookcopy`. Save slot 14, the mod off at launch, an independent copy
of the save folder and the tool's backup taken first and the backup restored
at the end without asking, every screenshot `grab_window` (its pixels are the
client area the GUI maps onto). The character pick is the only step that may
need a person, and only if the tool's pick is refused three times; the bag key
`C` needs none.

1. Positive controls: the lease's DLL hash equals the build's (`dll-hash`);
   the bare `stashmoveall` line starts `stashmoveall: state=off key=F4
   button=none` and carries `button_look_same=none`, which only this build
   prints (`marker`).
2. The bag open on its own (`C`), `menulayout UI_Button_Small_obj` and
   `menulayout UI_Button_Open_Mercenary_obj`: the Mercenary row and
   InventorySort's, quoted with the GUI size (`merc-read`, research). `C`
   again closes the bag.
3. The stash open; a by-name tab switch prints its handler line (`control`);
   the mod on; a tab switch there and back, about 2 s, `menulayout
   UI_Button_Small_obj`: exactly one `ForgePactMoveAll` row, `text=Move all`,
   visible, its bbox within 1 GUI unit of the Mercenary box on each side - or,
   when the Mercenary row did not read, of the box Live 5's relation gives from
   this open's InventorySort (left 196/192 of Sort's width left of Sort's left,
   the same top, Sort's width and height) (`button-placed`). The bare state
   line reads `button_place=on`, `button_look=sort`, `button_look_same=16/16`
   and `button_ref=relation` (`button-state`). A screenshot, and
   `tools/button_label_check.py` on it, the node's box against Sort's, exits 0
   (`label-centred`).
4. `probe dump` of Sort and of the node, and `probe diff` of the two: none of
   the 16 look members (`sprite_index`, `image_xscale`, `image_yscale`,
   `textFont`, `dropShadow`, `createX`, `drawXOffset`, `drawYOffset`,
   `navBboxX`, `navBboxY`, `navBboxWidth`, `navBboxHeight`, `naviDown`,
   `naviDownPrev`, `naviRight`, `naviRightPrev`) shows on a `~` or `-` line -
   the scales are equal because this target is Sort-sized - with both dumps'
   `navBboxX`, `navBboxY` and `textFont` lines quoted with the kind each
   printed (`look-members`).
5. Only when `label-centred` did not pass (otherwise `not-run (label-centred
   passed)`): Live 5's trial again on the node, `probe lookcopy` from Sort
   with `changed`, every `wrote` line and the count quoted, then a screenshot
   and the label check on the same boxes - `pass` when it now exits 0, `fail`
   when it exits 1 (`label-trial-control`, research). What the `wrote` lines
   name separates the cause: look members, so the game changed them back
   after the copy or the copy did not take (see `button_look_same=`); other
   members only, so a member Live 5's node already shared now differs; or
   `wrote=0`, so no member differs and the label is off for another reason.
6. Two stash Personal items Ctrl + clicked into the bag, then a click at the
   centre of the node's box: one `moved <n> of <m> from bag tab 0 to stash tab
   0` line with n at least 1, `in_node` and `taken` each up by 1
   (`button-press`).
7. The stash closed, the game still running (`close-survives`); reopened, the
   tab switch, 2 s: the same relations against this open's rows, the state
   line again `button_place=on`, `button_look=sort`, `button_look_same=16/16`,
   and the label check on a new screenshot exiting 0 (`reopen-placed`).

The cases are the session's first node, a reopened node and one press; the
step-5 trial only on a failure. `merc-read` and `label-trial-control` are
research checks, whose `fail` or `not-run` is a finding.

## Results

### Live 1 results

Live 1 ran on 2026-09-28 (research build e18d3198, slot 14; capture
`.claude/workorders/forgepact-68-move-all-live-1.md` on the owner's machine).
Every session-validity check passed; no item moved from the bag into the
stash. The bag grid node was 262384, the personal tab's stash grid node
262428, the stash window 262356; nodeGrid is indexed `[y][x]` on both grids
(measured: `nodeGrid.<x>.<y>` read a different item on three asymmetric
cells, `nodeGrid.<y>.<x>` the right one). The bag grid held classes 6, 7 and
10 only, so `<K_J>` was a class 7 key (the `-18` the procedure asked for sat
in `PotionGrid`, a different grid). One row per check; the logged shape sits
beside the supplied shape for every by-name call, and § What Live 1 supplied
lists every rejected or unproductive shape in full.

| Check | What it reads | Logged shape | Supplied shape | Result | Verdict |
|---|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the build above | - | - | lease `dll_sha256` e18d3198..., equal to the build's | pass |
| marker | `craftprobe` first line | - | - | `craftprobe: phase1k rows=287 - research instrument ...` | pass |
| control | `craftprobe hook` line, `CheckPlayerInteraction` rising | - | - | `286 detoured, 0 failed, 1 held`; `calls=11340`, then `calls=40740` five seconds later | pass |
| byname-control | the dispatcher control line and its `show` entry | `InventoryGridCanAddToStack calls=1 logged=1` | self = other = bag grid 262384; 1, undefined, `fp:` the class 14 bag material | `dispatched #1 -> ret=` an item struct (class 14) | pass |
| lookup-control | the map 0 line for `<K_J>` in the bag and the map 9 line for `<K_S>`, before any grid block | - | map 0: self = other = bag grid, `<K_J>` (class 7, bag cell 4,0); map 9: self `Console_Save_obj`, `<K_S>` (class 3, personal tab cell 14,14) | map 0 answered an item struct; map 9 answered `undefined` (explained by § Static reading 2, (a), (b) and (g): a personal-tab entry is not in the stash map) | fail |
| grid-move-byname | `<K_J>` in a stash grid cell and in no bag cell | - | nothing: no grid block ran | - | not-run (instrument: lookup control failed) |
| grid-move-map | the two lookup lines for `<K_J>` after the block (map 9 a struct, map 0 `undefined`) | - | nothing | - | not-run (instrument: lookup control failed) |
| tab-full-byname | the by-name placement of `<K_J2>` into a full tab on show: `success=false`, both sides unchanged (instrument check) | - | nothing; the search found shared tab 2 full (306 filled, 0 empty), shared tab 1 with 82 empty cells, tab 19 unpurchased | - | not-run (instrument: lookup control failed) |
| grid-move-tab-full | where `<K_J2>` lands when a single-input quick move (the right-click or shift-click that moved an item into the stash; never click, move, click) meets a full tab on show, and the shown tab's cells before and after | - | - | no gesture moved an item into the stash, so none qualified; `<K_J2>` stayed untouched in the bag | not-run (instrument: no single-input quick-move moved an item) |
| stack-move-byname | the stash sum for `<K_M>`'s kind and the bag cell | `StashAddToStack` twice, `ret=bool:false` both | self = other = bag grid; the Materials array, 9, 2, `fp:` `<K_M>` (class 14, base id 71, a stack of 15), count 15 then 1, sixth 0 | both false; the Materials tab re-read identical (45 identities, none of base id 71); the bag cell unchanged, so no source clear ran | fail |
| bag-subtab-source | whether the bag's Materials sub-tab lists cells | - | - | `New_Inventory_Data_obj.inventoryMaterialGrid` read as an array 6 by 15 with per-cell fingerprint structs | pass |
| gesture-rightclick | rows and movement on a right-click, and where the item went | not attributed | right button, no modifier, hold 120 ms, on `<K_J>`'s bag cell, personal tab shown | `<K_J>` left the bag but reached no stash cell; another class 7 item took its cell - it was equipped (`RMB: Equip/Use`) | pass (moved out of the bag by equipping, not into the stash) |
| gesture-shiftclick | rows and movement on a shift-click, and where the item went | - | Shift held, left click with hold 0 ms, on a class 6 bag cell | bag and stash unchanged; a click whose down and up land in one frame activates nothing, so this proves nothing about Shift | not-observed |
| gesture-clickclick | rows and movement on click, move, click | - | left click on a class 10 bag cell (hold not recorded), move, left click on an empty personal-tab cell | both cells unchanged | not-observed |
| close-survives | the game after the stash close | - | - | the game kept running; `menulayout` answered its header with no stash window listed | pass |
| reopen-shows | the moved keys in the stash after a reopen | - | - | nothing had moved; every key read was where it started | not-observed (nothing moved this session to confirm) |
| saved-stash-has-keys | the moved keys under a stash container in the save | - | - | `tools/save_item_keys.py` did not exist yet | not-run (instrument: save_item_keys.py not built) |
| saved-bag-lacks-keys | the moved keys under no bag container | - | - | as above | not-run (instrument: save_item_keys.py not built) |

### What Live 1 supplied

Every call shape and gesture Live 1 tried that was rejected or moved nothing,
with exactly what was supplied, per the toolkit `AGENTS.md` § "Never Call an
Address You Resolved by Hand" (a rejected call shape is a labelling problem to
revisit, not a settled dead end), and after them the same for Live 1b, Live 1c,
Live 1d, Live 1e and Live 1f (the session is named in the first column). The last column is
what § Static reading 2 and the later sessions say the reply means; none of
them is a route negative.

| Call or gesture | Self / other | Arguments or input supplied | Shown | Reply | What it means |
|---|---|---|---|---|---|
| `GetItemFromFingerprint`, map 9 lookup control | `Console_Save_obj` / none | the key of a class 3 item in a personal-tab cell, as text; owner 9 | stash personal tab, bag page 0 | `undefined` | the entry is not in the stash map; the personal stash saves with the character (static reading 2, (a), (b), (g)); a shared-tab key was never tried |
| `GetItemFromFingerprint`, map 0 lookup control | bag grid / bag grid | the class 7 bag key, as text; owner 0 | as above | an item struct | the half that passed |
| `StashAddToStack`, whole stack | bag grid / bag grid | the Materials tab array (`path:Controller_obj.stashMaterialTab`), 9, 2, the class 14 key (base id 71) as `fp:`, count 15, sixth 0 | stash Materials tab | `false`, nothing changed | no stack of base id 71 on the tab to merge into (static reading 2, (c)) |
| `StashAddToStack`, one unit | as above | as above, count 1 | as above | `false`, nothing changed | as above |
| right click | - | right button, no modifier, hold 120 ms, on a class 7 bag cell | stash personal tab, bag page 0 | the item was equipped; the equipped one took its bag cell | the game binds the right button to Equip/Use, not to a move |
| Shift + left click | - | `key_down` vk 16, left click with `hold_ms` 0, `key_up`, on a class 6 bag cell | as above | nothing moved | unproven: a zero hold activates nothing; Shift is Compare / Split Stack in the hint strip, not a move |
| click, move, click | - | left click on a class 10 bag cell (hold not recorded), move, left click on an empty personal-tab cell | as above | nothing moved | unproven (hold not recorded); a pick-up and place, not a quick move |
| grid block (`GridAddItem`, `InvGridClearItemNode`, `ChangeItemOwner`) | - | never called: the lookup control failed first | - | - | not a result |
| Live 1b: `GetItemFromFingerprint`, map 0 lookup control | bag grid / bag grid | the key of a class 1 item in a shared-tab-1 cell, as text; owner 0 | stash personal tab, bag page 0 | `undefined` | the shared-tab entry is not on map 0 |
| Live 1b: `GetItemFromFingerprint`, map 9 lookup control | `Console_Save_obj` / none | the same shared-tab-1 key, as text; owner 9 | as above | `undefined` | nor on map 9 through this form; Live 1c and 1d answered the same for every shared-tab key, including one the game had just moved there, so where a shared-tab entry lives is not established (§ Static reading 2, (a)) |
| Live 1b: plain left click (click control) | - | left button, no modifier, the default hold, on the centre of a class 7 bag cell computed from the grid's box and confirmed on a screenshot | as above | `ProcessInventoryGridInput` ran once and `ValidateItem` once; no drag row logged; the cell still held the item | a scripted click reaches the grid's input but not its pick-up, so no scripted gesture was sent; not a route result |
| Live 1c: `StashAddToStack` alone, personal tab (step 7) | bag grid / bag grid | the personal tab's array (`path:id:<sg>.nodeGrid`), 0, 13, a class 10 key as `fp:`, 1, 0 | stash personal tab, bag page 0 | `false`, nothing moved | the hand move's own answer, reproduced; it only merges, and the placement `GridAddItem` was left out of the replay (Live 1d put it back and moved the item) |
| Live 1c: `StashAddToStack` then `ChangeItemOwner`, shared tab 1 (step 8) | bag grid / bag grid, then stash grid / bag grid | shared tab 1's array, 9, 2, a class 10 key as `fp:`, 1, 0; then 0, 9 and the key as text | stash shared tab 1, bag page 0 | `false`; then `undefined`, and the key answered on no map while it still sat in its bag cell | no placement ran, so the owner step alone left the half state the game's save does not survive; reversed at once with the owner step from 9 to 0, after which the key answered on map 0 again. The owner step runs only after a confirmed placement |
| Live 1c: `var Controller_obj 0 stashPersonalGrid` (step 7) | - | the member name | stash personal tab | "the instance has no variable stashPersonalGrid" | that candidate array does not exist on this build; the personal tab's array is the stash grid node's `nodeGrid` with the tab on show |
| Live 1d: `GridAddItem`, full shared tab 2 (step 6) | bag grid / bag grid | shared tab 2's array (`path:id:<sg>.nodeGrid`, 0 empty cells), a class 10 key as `fp:`, 0, `undefined` | stash shared tab 2, bag page 0 | a struct with `success=false`; nothing changed on any tab or in the bag | the game's no-room answer, the same as the owner's hand move against that tab; the placement searches only the array it is handed |
| Live 1e: Ctrl + left click, by hand (H1, H1b) | - | the owner's own quick move on two jewels (base ids 109 and 110, the first being `<K_SK1>`) and an Incarnation Gem (base id 136) in the bag's socket view | stash Socketable tab, bag Socket sub-tab | `ValidateItem` once per item, self and other the bag grid, answering empty text, as it does for an accepted item; no other armed row logged; every item stayed in its bag cell | the game does not let these kinds into the Socketable tab (the owner: a limit the game's designers set); it stops the move after `ValidateItem` and before any armed placement routine, and nothing in the logged answer shows the refusal, so a by-name call would not meet it |
| Live 1e: `node var Controller_obj 0 stashSocketItemSlot` (step 2) | - | the member name, read the way `node var` reads a container | stash Socketable tab | an array of 106 entries with no fingerprint read, while the tab visibly held items | not the Socketable tab's container as read this way: `menulayout` lists one one-cell `StashSocketGrid` node per item, and the save keeps the tab under `stash.hss` `socket_tab`; not a route result |
| Live 1f: `stashmoveall probe sort` by text (step 2) | - | the text `Sort` searched among `UI_Button_Small_obj` rows | stash Socketable tab, bag Socket sub-tab | `sort not-found`; both sort buttons read `Sort Tab` | the text is `Sort Tab`; the bag's Sort is found by its `uiNodeCallstack`, `InventorySort` (given by id the same session, and in Live 1g) |
| Live 1f: a click on the node bound to `UiSetFloatingToFalse` (step 7 (c)) | the node / the stash window, as the game's own dispatch supplied them | one argument, the node's `activationArgs` (an empty array); the input a scripted left click at the node's box centre, no hold | as above | the game ended with no dialog; its error log names "bool argument is unset" in the node's user event 15, inside that script | the dispatch reached the plugin as measured; the script reads a member a button does not carry (it belongs to `UI_Hud_Talent_obj`), so it is unsafe as any button's activation. Not a route negative for the dispatch; Route A is dropped, Route B (the unbound node, Live 1g) is used |

Live 1 did not try Ctrl + left click, the game's own quick move. Live 1b
could not send it (its click control failed), and in Live 1c the owner made
it by hand (§ Live 1c results).

### Live 1b results

Live 1b ran on 2026-09-28 (research build e18d3198, slot 14, no person at the
keyboard; capture `.claude/workorders/forgepact-68-move-all-live-1b.md` on the
owner's machine). Every session-validity check passed; **nothing moved and no
route was measured**. Two instrument checks failed before any move: the
shared-tab lookup answered on neither map, which stopped every by-name grid
block, and a scripted left click reached the grid's input but never picked
the item up, which stopped every gesture. The bag grid node was 262362, the
stash grid node 262406 (the same node rebinds to each stash tab), the stash
window 262334; shared tab 1 had 82 empty cells and shared tab 2 none. The
stash never closed (three by-name closes refused), so nothing reached the
saves. One row per check; the logged shape sits beside the supplied shape
for every by-name call.

| Check | What it reads | Logged shape | Supplied shape | Result | Verdict |
|---|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the build of § Phase A' shapes | - | - | lease `dll_sha256` e18d3198..., equal to the build's | pass |
| marker | `craftprobe` first line | - | - | `craftprobe: phase1k rows=287 - research instrument ...` | pass |
| control | `craftprobe hook` line, `CheckPlayerInteraction` rising | - | - | `286 detoured, 0 failed, 1 held`; `calls=10920`, then `calls=40320` five seconds later | pass |
| byname-control | the dispatcher control line and its `show` entry | `InventoryGridCanAddToStack calls=1 logged=1` | self = other = bag grid 262362; 1, undefined, `fp:` the bag's largest material stack (class 14) | `dispatched #1 -> ret=` an item struct (class 14) | pass |
| lookup-control | the four lookup lines for `<K_S0>` and `<K_S1>`: each on exactly one map (`personal-map`, `shared-map`) | - | map 0: self = other = bag grid; map 9: self `Console_Save_obj`; `<K_S0>` a class 3 personal-tab key, `<K_S1>` a class 1 shared-tab-1 key | `<K_S0>`: an item struct on map 0, undefined on map 9 (`personal-map: 0`); `<K_S1>`: undefined on both maps, so `shared-map` could not be read | fail |
| click-control | a plain click on `<K_J1>`'s cell picks it up (drag rows rising, the cell empty), a second puts it back | `ProcessInventoryGridInput` +1 and `ValidateItem` +1; no `InvStartDragging` or `InvCopyItemDragData` | - | left click with the default hold on `<K_J1>` (class 7, cell 4,0), its point confirmed on a screenshot; the cell still held `<K_J1>` afterwards | fail |
| gesture-ctrl-personal | Ctrl + left click on `<K_J2>`, personal tab shown: where it lands, its map, every row logged | - | - | not sent | not-run (instrument: click control failed) |
| gesture-ctrl-shared | Ctrl + left click on `<K_J3>`, shared tab 1 shown: where it lands, its map, every row logged (`ChangeItemOwner` or not) | - | - | not sent | not-run (instrument: click control failed) |
| gesture-ctrl-full | Ctrl + left click on `<K_J4>`, full shared tab 2 shown: the bag, that tab or another tab, and the shown tab's cells before and after; sent only after a Ctrl + left click moved an item this session | - | - | not sent | not-run (instrument: no Ctrl + left click moved an item this session) |
| gesture-ctrl-material | Ctrl + left click on `<K_MA>` in the bag's Materials sub-tab, Materials tab shown: the tab's sum and the rows | - | - | not sent | not-run (instrument: click control failed) |
| grid-array-identity | `Controller_obj.stashPersonalGrid` and the grid node's `nodeGrid` at `<K_J2>`'s landing cell | - | - | no gesture, so no landing cell | not-run (instrument: click control failed) |
| grid-move-byname | `<K_J5>` in a personal-tab cell and in no bag cell after the by-name block | - | nothing: no grid block ran | - | not-run (instrument: lookup control failed) |
| grid-move-map | `<K_J5>`'s two lookup replies against `<K_S0>`'s | - | nothing | - | not-run (instrument: lookup control failed) |
| grid-move-shared-byname | `<K_J6>` in a shared-tab-1 cell and in no bag cell after the by-name block | - | nothing | - | not-run (instrument: lookup control failed) |
| grid-move-shared-map | `<K_J6>`'s two lookup replies against `<K_S1>`'s | - | nothing | - | not-run (instrument: lookup control failed) |
| tab-full-byname | the by-name placement into full shared tab 2: `success=false`, both sides unchanged (instrument check) | - | nothing | - | not-run (instrument: lookup control failed) |
| stack-move-byname | the Materials tab's sum for `<K_MU>`'s identity up by 1 and `<K_MU>`'s cell gone, run only with a stack of that identity already on the tab | - | nothing: the one-unit give made `<K_MU>` (base id 72), but no stack of base id 72 was on the tab, and the gesture that would have put one there was not sent | - | not-run (instrument: no merge target on the tab) |
| mat-new-byname | the Materials tab gains `<K_MB>`'s identity and its count; the bag cell gone | - | nothing | the gesture was not sent, so no placement was logged | not-run (no reproducible shape logged) |
| close-survives | the game after the stash close | - | - | the game kept running and `menulayout` answered its header; the stash itself never closed (three by-name closes answered `stash_still_open`) | pass |
| reopen-shows | every moved key where it landed after a reopen, in no bag cell | - | - | nothing had moved, and with the stash still open there was no reopen | not-observed (nothing moved this session to confirm) |
| saved-stash-has-keys | each moved key under a stash container in exactly one file (`save_item_keys.py`) | - | - | nothing moved; `hs_saves_inspect` listed only `shop.ini` as changed, so the stash, bag and character files were byte-identical to the pre-session copy | not-observed (no key moved this session to check) |
| saved-bag-lacks-keys | no moved key under a bag container, `inventory_order_<slot>.hss` included, after the pre-session copy's read showed each one in a bag container | - | - | as above | not-observed (no key moved this session to check) |

### Live 1c results

Live 1c ran on 2026-09-28 (research build e18d3198, slot 14, **hand-assisted**:
the owner made the four quick moves H1 to H4 one at a time, then left;
capture `.claude/workorders/forgepact-68-move-all-live-1c.md` on the owner's
machine). Every session-validity check passed. **Every hand move passed and
the by-name merge reproduced**; the by-name grid replays moved nothing,
because the procedure replayed only the rows of the group § Static reading
named and so left out `GridAddItem`, the row each hand move placed the item
with (Live 1d replayed it). The bag grid node was 262350 (it rebinds to the
bag's Materials sub-tab), the stash grid node 262394 (it rebinds to each
stash tab), the stash window 262322. The bag grid held one single-cell
non-stackable (a class 7 ring); the other test keys were 1 by 3 class 10
charms, each clicked and addressed at its top-left anchor cell. The hot rows
`GetItemFromFingerprint` and `GetItemMap` logged their whole budget with no
input in five seconds and were re-armed out of the list (their counts still
showed; the lookups were run by name). Each hand-move row is **measured**,
with a person as the input; the logged sequence is quoted in the Logged
shape column in logged order.

| Check | What it reads | Logged shape | Supplied shape | Result | Verdict |
|---|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the build of § Phase A' shapes | - | - | lease `dll_sha256` e18d3198..., equal to the build's | pass |
| marker | `craftprobe` first line | - | - | `craftprobe: phase1k rows=287 - research instrument ...` | pass |
| control | `craftprobe hook` line, `CheckPlayerInteraction` rising | - | - | `286 detoured, 0 failed, 1 held`; `calls=12600`, then `calls=41580` five seconds later | pass |
| byname-control | the hook line, and by-name calls dispatching and listed | - | the by-name calls of steps 7 to 9 | every by-name call dispatched; each array argument printed the same array id the matching hand move logged | pass |
| hand-personal | the owner's Ctrl + left click on `<K_A>` (class 7, bag cell 4,0), personal tab shown: where it lands, every row logged | `ValidateItem` self = other = bag grid, the item, answer empty text; `StashAddToStack` self = other = bag grid, the personal tab's array (18 rows), 0, 13, the item, 1, 0, answer false; `GridAddItem` self = other = bag grid, the same array, the item, 0, undefined, answer tab 0, x 0, y 0, `success=true`; `s_InvNode` (self a struct, other the bag grid) with 0, 0, the item; `ValidateItem` self the stash grid, other the bag grid, answer empty text. No clear, no `ChangeItemOwner`, no `GetStashMaxTabs` | a person | `<K_A>` in personal-tab cell 0,0 and in no bag cell; the bag's filled count 25 to 24, the tab's 18 to 19 | pass |
| hand-personal-map | `<K_A>`'s two lookups after the move against `<K_S0>`'s | - | map 0: self = other = bag grid; map 9: self `Console_Save_obj` | both keys: an item struct on map 0, undefined on map 9 | pass |
| hand-shared | the owner's Ctrl + left click on `<K_B>` (class 10, 1 by 3, anchor 2,0), shared tab 1 shown | as hand-personal with `StashAddToStack`'s second and third arguments 9 and 2 and shared tab 1's own array; `GridAddItem` answered x 0, y 1, `success=true`; `s_InvNode` three times; then, after the stash grid's `ValidateItem`, `ChangeItemOwner` self the stash grid, other the bag grid, 0, 9, the key as text, answer undefined | a person | `<K_B>` in shared-tab-1 cells 0,1 to 0,3 and in no bag cell; the bag's filled count 24 to 21, the tab's 224 to 227 | pass |
| hand-shared-map | `<K_B>`'s two lookups after the move against `<K_S1>`'s | - | as hand-personal-map | both keys undefined on map 0 and on map 9: they match, but which map holds a shared-tab entry is still not identified | pass |
| hand-full | the owner's Ctrl + left click on `<K_C>` (class 10, anchor 3,0), full shared tab 2 shown: where it lands, and the tab's cells | `ValidateItem`; `StashAddToStack` with shared tab 2's array, 9, 2, answer false; `GridAddItem` with the same array, the item, 0, undefined, answer `success=false`; nothing after it: no `s_InvNode`, no clear, no `ChangeItemOwner`, no `GetStashMaxTabs` | a person | `<K_C>` stayed in its bag cells; shared tab 2 still 306 filled, 0 empty; shared tab 3, also full, did not hold it | pass |
| hand-material | the owner's Ctrl + left click on `<K_MA>` (base id 72, a stack of 934), bag Materials sub-tab and Materials tab shown | `ValidateItem`; `StashAddToStack` with `Controller_obj.stashMaterialTab`'s array, 9, 2, the item, 1, 0, answer false; `GridAddItem` with the same array, the item, 0, undefined, answer x 3, y 15, `success=true`; `s_InvNode`; `ValidateItem` with the stash grid as self; `ChangeItemOwner` self the stash grid, other the bag grid, 0, 9, the key | a person | the Materials tab gained base id 72 with 934; `<K_MA>` left the bag | pass |
| hand-merge | the owner's Ctrl + left click on `<K_MU1>` (base id 72, one unit), the stack above on the tab | `ValidateItem`; `StashAddToStack` self = other = bag grid, the Materials array, 9, 2, the item, 1, 0, answer true; `InvGridClearItemNode` self = other = bag grid, the unit's bag cell node (anchor 0,0), undefined. No `GridAddItem`, no `ChangeItemOwner` | a person | the tab's sum for base id 72 rose 934 to 935; `<K_MU1>` left the bag | pass |
| byname-personal | `<K_P>` (class 10, anchor 2,3) in a personal-tab cell and in no bag cell after the replay | as hand-personal | only the group's row `StashAddToStack`: self = other = bag grid, `path:id:<sg>.nodeGrid` (the same array id as hand-personal's), 0, 13, `fp:<K_P>`, 1, 0 | answer false; `<K_P>` still in its bag cells; the placement `GridAddItem` was not in the replay | not-observed (the placement row was left out of the replay; Live 1d's `byname-personal`) |
| byname-personal-map | `<K_P>`'s lookups against `<K_A>`'s after H1 | - | as hand-personal-map | both an item struct on map 0 and undefined on map 9; uninformative, since a bag item and a personal-tab item answer the same | pass (by the letter; uninformative) |
| byname-shared | `<K_Q>` (class 10, anchor 3,3) in a shared-tab-1 cell and in no bag cell after the replay | as hand-shared | `StashAddToStack` self = other = bag grid, `path:id:<sg>.nodeGrid` with shared tab 1 on show (the same array id as hand-shared's), 9, 2, `fp:<K_Q>`, 1, 0; then `ChangeItemOwner` self the stash grid, other the bag grid, 0, 9, the key | false, then undefined; `<K_Q>` still in its bag cells | not-observed (the placement row was left out of the replay; Live 1d's `byname-shared`) |
| byname-shared-map | `<K_Q>`'s lookups against `<K_B>`'s after H2 | - | as hand-shared-map | undefined on both maps, matching - but made by the owner step alone, with the item still in its bag cell: the half state the game's save does not survive. Reversed at once (owner step 9 to 0; the key answered on map 0 again) | pass (by the letter; not a move, reversed) |
| byname-merge | the Materials tab's sum for `<K_MU2>`'s base id up by 1 and `<K_MU2>`'s cell gone | as hand-merge | `StashAddToStack` self = other = bag grid, `path:Controller_obj.stashMaterialTab` (the same array id as hand-merge's), 9, 2, `fp:<K_MU2>`, 1, 0; then `InvGridClearItemNode` self = other = bag grid, `path:id:<bag>.nodeGrid.1.0`, undefined | answer true, then undefined; the sum rose 935 to 936; `<K_MU2>` left the bag | pass |
| close-survives | the game after the stash close, by name only | - | - | the first by-name close answered "not confirmed"; after a mouse move `menulayout` no longer listed the stash; the game kept running | pass |
| reopen-shows | every moved key where it landed after a reopen, in no bag cell | - | - | `<K_A>` in the personal tab, `<K_B>` in shared tab 1, the Materials sum for base id 72 still 936; none of them in the bag | pass |
| saved-stash-has-keys | each moved key under a stash container in exactly one file (`save_item_keys.py`) | - | - | `<K_A>` under `herosiege13.hss` `inventory.personal_stash`; `<K_B>` under `stash.hss` `stash_tab_1`; `<K_MA>` under `stash.hss` `material_tab` (the merged units are exempt) | pass |
| saved-bag-lacks-keys | no moved key under a bag container of `inventory_order_13.hss`, after the pre-session copy's read showed each one in a bag container | - | - | none of the three under a bag container after; the pre-session copies listed all three there (`inventory_tab_0`, `inventory_material_tab`) | pass |

### Live 1d results

Live 1d ran on 2026-09-28 (research build e18d3198, slot 14, **fully
automatic**, on the saves Live 1c restored; capture
`.claude/workorders/forgepact-68-move-all-live-1d.md` on the owner's machine).
All thirteen checks passed. **The by-name move reproduced into the personal
tab and into shared tab 1**, with `GridAddItem` in the replay, and it held
through a close, a reopen and the saved files; against the full tab it
answered no room and changed nothing. The bag grid node was 262446, the stash
grid node 262490, the stash window 262418; every key read in step 2 matched
Live 1c's. The by-name calls are § Phase A' shapes, "The hand moves'
sequences, replayed by name"; the logged shape beside each is Live 1c's hand
move of the same tab kind.

| Check | What it reads | Logged shape | Supplied shape | Result | Verdict |
|---|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the build of § Phase A' shapes | - | - | lease `dll_sha256` e18d3198..., equal to the build's | pass |
| marker | `craftprobe` first line | - | - | `craftprobe: phase1k rows=287 - research instrument ...` | pass |
| control | `craftprobe hook` line, `CheckPlayerInteraction` rising | - | - | `286 detoured, 0 failed, 1 held`; `calls=0` right after the arm, then `calls=59640` five seconds later | pass |
| byname-control | the hook line, the armed rows idle before any call, and by-name calls dispatching | - | the by-name calls of steps 4 to 6 | every armed grid row read 0 calls before the first by-name call; every by-name call dispatched | pass |
| byname-personal | `<K_P>` (class 10, 1 by 3, anchor 2,3) in personal-tab cells at the answer's x, y, the tab's filled count up by its footprint | Live 1c hand-personal | `ValidateItem` self = other = bag grid, `fp:<K_P>`; `StashAddToStack` self = other = bag grid, `path:id:<sg>.nodeGrid`, 0, 13, `fp:<K_P>`, 1, 0; `GridAddItem` self = other = bag grid, the same array, `fp:<K_P>`, 0, undefined; `ValidateItem` self the stash grid, other the bag grid | empty text; false; tab 0, x 0, y 0, `success=true`, with three `s_InvNode` calls nested under it; empty text. `<K_P>` in personal-tab cells 0,0 to 0,2; the tab's filled count 18 to 21. The bag cell still held `<K_P>` | pass |
| byname-personal-clear | `<K_P>` in no bag cell after the source clear, its lookups as `<K_S0>`'s | the hand move emptied the bag cell with no armed row logging a clear | `InvGridClearItemNode` self = other = bag grid, `path:id:<bag>.nodeGrid.3.2` (the anchor), undefined | answer undefined, the reply's node naming `<K_P>` at 2,3; the bag's filled count 25 to 22, `<K_P>` in no bag cell; an item struct on map 0 and undefined on map 9, as `<K_S0>` | pass (cleared by `InvGridClearItemNode`) |
| byname-shared | `<K_Q>` (class 10, anchor 3,3) in shared-tab-1 cells at the answer's x, y and in no bag cell | Live 1c hand-shared | as byname-personal with shared tab 1 on show and `StashAddToStack`'s 9, 2; then the source clear `path:id:<bag>.nodeGrid.3.3` | false; x 0, y 1, `success=true`, three nested `s_InvNode`; the tab's filled count 224 to 227; the clear emptied the bag cell (22 to 19) - the placement alone did not | pass |
| byname-shared-owner | after a `byname-shared` pass: the owner step, `<K_Q>` still in its cells, its lookups turning into `<K_S1>`'s | hand-shared's `ChangeItemOwner` | `ChangeItemOwner` self the stash grid, other the bag grid, 0, 9, `<K_Q>` as text | undefined; `<K_Q>`'s lookups from an item struct on map 0 to undefined on both maps, as `<K_S1>`'s; shared tab 1 still 227 filled with `<K_Q>` in it | pass |
| byname-full | `<K_R>` (class 10, anchor 3,0) against full shared tab 2: `success=false`, nothing changed anywhere | Live 1c hand-full | `ValidateItem`; `StashAddToStack` with shared tab 2's array, 9, 2, `fp:<K_R>`, 1, 0; `GridAddItem` with the same array, `fp:<K_R>`, 0, undefined | false; `success=false`, nothing nested; the bag 19 filled with `<K_R>` in it, shared tab 2 still 306 and 0 empty, the personal tab 21 and shared tab 1 227 as after steps 4 and 5 | pass |
| close-survives | the game after the stash close, by name only | - | - | the first by-name close answered "not confirmed"; after a mouse move `menulayout` no longer listed the stash; the game kept running | pass |
| reopen-shows | every moved key where it landed after a reopen, in no bag cell | - | - | `<K_P>` in the personal tab (21 filled), `<K_Q>` in shared tab 1 (227), neither in the bag, `<K_R>` still in the bag | pass |
| saved-stash-has-keys | each moved key under a stash container in exactly one file (`save_item_keys.py`) | - | - | `<K_P>` under `herosiege13.hss` `inventory.personal_stash`; `<K_Q>` under `stash.hss` `stash_tab_1` | pass |
| saved-bag-lacks-keys | no moved key under a bag container of `inventory_order_13.hss`; the pre-session copies list every key there, and the unmoved `<K_R>` is still there | - | - | neither `<K_P>` nor `<K_Q>` under a bag container after; the pre-session copies listed all three under `inventory_tab_0`; `<K_R>` still under it | pass |

### Live 1e results

Live 1e ran on 2026-09-28 (research build e18d3198, slot 14,
**hand-assisted**, on the saves Live 1d restored; capture
`.claude/workorders/forgepact-68-move-all-live-1e.md` on the owner's
machine). Every session-validity check passed; twelve checks passed and two
were `not-run`. **A new material identity and a whole-stack merge moved by
name** and held through a close, a reopen and the saved files. **The
Socketable tab moved by hand only**: the game refused both jewel kinds and
the Incarnation Gem the procedure first picked, the owner then offered a
rune, a gem and an orb (moved from the tab into the bag by hand first, a
direction not under test), and those three went back in, two as new
placements and one as a merge; with those used, no socketable of an accepted
kind was left for a by-name replay. The bag grid node was 262399 (it rebinds
to the bag's Socket and Materials sub-tabs), the stash grid node 262443, the
stash window 262371; the bag's materials matched Live 1c's reads (base ids
71, 73 and 72 with 15, 908 and 934, none on the Materials tab).
`hs_give_item` made a one-unit socketable too, so it is not limited to
class 14. `GetItemPreferredGrid`, the row this session added, logged only
during the reverse move, never on a move from the bag into the stash. Each
hand-move row is **measured**, with a person as the input; the logged
sequence is quoted in the Logged shape column in logged order.

| Check | What it reads | Logged shape | Supplied shape | Result | Verdict |
|---|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the build of § Phase A' shapes | - | - | lease `dll_sha256` e18d3198..., equal to the build's | pass |
| marker | `craftprobe` first line | - | - | `craftprobe: phase1k rows=287 - research instrument ...` | pass |
| control | `craftprobe hook` line, `CheckPlayerInteraction` rising | - | - | `286 detoured, 0 failed, 1 held`; `calls=11340`, then `calls=21420` five seconds later | pass |
| byname-control | the hook line, the armed rows idle after each re-arm, and by-name calls dispatching | - | the by-name calls of steps 5 and 6 | every re-arm's baseline read 0 calls on every armed row; every by-name call dispatched, and each array argument printed the same array id the matching hand move logged | pass |
| hand-socket | the owner's Ctrl + left click from the bag's socket view, Socketable tab shown: where each item lands, every row logged, the path taken | refused items: `ValidateItem` only (self and other the bag grid, answer empty text). Rune (base id 31) and gem (38), the new path: `ValidateItem` self and other the bag grid, empty text; `StashAddToStack` self and other the bag grid, a one-row array (a different one for each item), 9, 2, the item, 1, 8, answer false; `GridAddItem` self and other the bag grid, the same array, the item, 0, undefined, answer tab 0, x 0, y 0, `success=true`; `s_InvNode` (self a struct, other the bag grid) with 0, 0, the item; `ValidateItem` self the stash grid, other the bag grid, empty text; `ChangeItemOwner` self the stash grid, other the bag grid, 0, 9, the key as text, answer undefined. Orb (118), the merge path: `ValidateItem`; `StashAddToStack` the same shape, answer true; `InvGridClearItemNode` self and other the bag grid, the orb's bag cell node, undefined. No `GetStashMaxTabs` | a person | two jewels (base ids 109 and 110, the first `<K_SK1>`) and an Incarnation Gem (136) stayed in their bag cells: the game refuses them for this tab. The rune, the gem and the orb each left the bag's socket view (23, 22, 21, then 20 keys) and reached the tab; the rune and the gem were saved under `stash.hss` `socket_tab`. The one-row array was not matched to a `path:` form: `Controller_obj.stashSocketItemSlot` read no fingerprint while the tab held items | pass (on the rune, gem and orb; `<K_SK1>`, a jewel, was refused by the game) |
| hand-material-new | the owner's Ctrl + left click on `<K_XU>` (base id 71, one unit given from `<K_X>`'s template, bag cell 0,0), bag Materials sub-tab and Materials tab shown | `ValidateItem` self and other the bag grid, empty text; `StashAddToStack` self and other the bag grid, `Controller_obj.stashMaterialTab`'s array (18 rows), 9, 2, the item, 1, 0, answer false; `GridAddItem` self and other the bag grid, the same array, the item, 0, undefined, answer x 3, y 15, `success=true`; `s_InvNode` with 3, 15, the item; `ValidateItem` self the stash grid, other the bag grid; `ChangeItemOwner` self the stash grid, other the bag grid, 0, 9, the key, answer undefined. No clear logged | a person | the Materials tab gained base id 71 with 1, in the cell Live 1c's hand-material landed in; `<K_XU>` left the bag | pass |
| byname-material-new | `<K_Y>` (base id 73, 908, bag cell 14,4): the tab's sum for its base id equal to its count, in no bag cell, its lookups as `<K_XU>`'s after the hand move | hand-material-new | `StashAddToStack` self and other the bag grid, `path:Controller_obj.stashMaterialTab` (the same array id as the hand move's), 9, 2, `fp:<K_Y>`, 1, 0; `GridAddItem` the same self, other and array, `fp:<K_Y>`, 0, undefined; `ValidateItem` self the stash grid, other the bag grid; `InvGridClearItemNode` self and other the bag grid, `path:id:<bag>.nodeGrid.4.14`, undefined; `ChangeItemOwner` self the stash grid, other the bag grid, 0, 9, the key | false; x 3, y 16, `success=true`; empty text; undefined, the reply's node naming `<K_Y>` at 14,4; undefined. The tab's sum for base id 73 was 908; `<K_Y>` in no bag cell; undefined on map 0 and an item struct on map 9, as `<K_XU>` | pass |
| byname-merge-whole | `<K_X>` (base id 71, 15, bag cell 14,3) merged by name onto the unit the hand move placed: the tab's sum up by exactly its count, its cell gone | Live 1c hand-merge (fifth argument 1) | `StashAddToStack` self and other the bag grid, `path:Controller_obj.stashMaterialTab`, 9, 2, `fp:<K_X>`, 15 (the whole count), 0; then `InvGridClearItemNode` self and other the bag grid, `path:id:<bag>.nodeGrid.3.14`, undefined | true; the sum for base id 71 rose 1 to 16, by exactly 15; the clear's node named `<K_X>` at 14,3; `<K_X>` in no bag cell | pass |
| byname-socket | a second socketable identity off the tab placed by name, replaying hand-socket's new path | hand-socket's rune and gem | nothing | the only rune, gem and orb in the bag were used by hand-socket; the bag's other socketables are jewels and Incarnation Gems, which the game refuses for this tab | not-run (instrument: no second socketable identity off the tab) |
| byname-socket-merge | a one-unit socketable merged by name onto its identity's stack on the tab | hand-socket's orb | nothing | the one given unit, `<K_SKU>`, is a jewel; the game refuses that kind before any armed placement routine, so a by-name merge would force it past the refusal; not attempted, and `<K_SKU>` stayed untouched in its bag cell | not-run (instrument: the only given unit is a kind the game refuses for this tab) |
| close-survives | the game after the stash close, by name only | - | - | the first by-name close answered "not confirmed"; after a mouse move `menulayout` no longer listed the stash; the game kept running | pass |
| reopen-shows | every moved key where it landed after a reopen, in no bag cell | - | - | the Materials sums for base ids 71 and 73 still 16 and 908; the bag's Materials sub-tab held only the untouched base id 72 stack; the bag's socket view still 20 keys, the rune, the gem and the orb absent | pass |
| saved-stash-has-keys | each moved key under a stash container in exactly one file (`save_item_keys.py`) | - | - | the rune and the gem under `stash.hss` `socket_tab`; `<K_XU>` and `<K_Y>` under `stash.hss` `material_tab`; the merged `<K_X>` and orb in no live file (exempt) | pass |
| saved-bag-lacks-keys | no moved key under a bag container of `inventory_order_13.hss`, after the pre-session copies' read showed the bag keys in a bag container | - | - | none under a bag container after; the pre-session copies listed `<K_X>` and `<K_Y>` under `inventory_material_tab` (the rune and the gem started in `stash.hss` `socket_tab`, and `<K_XU>` did not exist yet) | pass |

### Live 1f results

Live 1f ran on 2026-09-28 (research build phaseC-fbca7251, slot 14, on the
saves Live 1e restored; capture
`.claude/workorders/forgepact-68-move-all-live-1f.md` on the owner's
machine). Every session-validity check passed. **The Socketable tab merged
by name**, for the orb and for the gem, the gem showing itself stackable.
**The button** was created by name left of Sort and drawn, and the Sort
click proved the frame poll; then the click on the node bound to
`UiSetFloatingToFalse` **ended the game**, and every later check is
`not-run`. Nothing had been saved: the stash and bag files were unchanged
when the saves were inspected after the crash, and the restore left them
clean. One person step: the character select refused five times (the
foreground window changed during the click, an hs-drive gap), and the owner
loaded slot 14 by hand. The bag grid node was 265216 (rebound to its Socket
sub-tab), the stash window 265188; the Socketable tab listed 106 one-cell
`StashSocketGrid` nodes, 91 filled, one key per base id, and the bag's
Socket sub-tab held base ids 109, 110, 111 and 136 only (19 keys), as
expected. The logged shape for the merge is Live 1e's hand-moved orb.

| Check | What it reads | Logged shape | Supplied shape | Result | Verdict |
|---|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the build of § Live procedure 1f | - | - | lease `dll_sha256` fbca7251..., equal to the build's | pass |
| marker | `craftprobe` first line | - | - | `craftprobe: phase1k rows=291 - research instrument ...` | pass |
| control | `craftprobe hook` line, `CheckPlayerInteraction` rising | - | - | `290 detoured, 0 failed, 1 held`; `calls=13860`, then `calls=25620`; after the 45-row arm 0, then 10080 | pass |
| byname-control | the hook line, the armed rows idle after each re-arm, and by-name calls dispatching | - | the by-name calls of steps 5 to 7 | all 45 armed rows read 0 before the first by-name call; every call dispatched, each array or node id the one `menulayout` read | pass |
| sort-node | the bag's Sort node among `UI_Button_Small_obj` rows | - | `probe sort` by the text `Sort`, then `probe sort id:265231` | no row reads `Sort`: five rows, the bag's Sort (`uiNodeCallstack=InventorySort`, text `Sort Tab`, `activationArgs` [1]), the stash's sort (`StashSort`, `Sort Tab`, [2]), `InventoryMobileStackSplit` (`Split Stack`, not visible), `MoveStashLeft` and `MoveStashRight`; by id: x 2303.5, y 1198.9, box 2303.5,1198.9 to 2485.9,1261.6, sprite `Inventory_Tab_Button_Solid_spr`, visible 1, enabled 1 | pass (by hand: the id given from the `InventorySort` row) |
| sort-activation | the Sort node's activation, read hook-free | - | `probe sort id:265231` | a method of `InventorySortTab`, self an instance, the Sort node itself (265231) | pass |
| socket-copy | the copy verb's replies and the bag's Socket sub-tab | - | `probe copy` of the orb's key (node 265556, base id 118, `o` 81), then of the gem's (node 265536, base id 38, no `o`), one each | `<K_ORBU>` with `o=1` and `<K_GEMU>` with no `o`, each `confirmed` in map 0 and its cells (3,1 and 3,2); the sub-tab's keys 19 to 21 | pass |
| byname-socket-merge | the orb's node count up by 1 and `<K_ORBU>` in no bag cell | Live 1e hand-socket's orb: `StashAddToStack` answering true, then `InvGridClearItemNode` on its bag cell | `StashAddToStack` self = other = the bag grid, `path:id:265556.nodeGrid` (one cell), 9, 2, `fp:<K_ORBU>`, 1, 8; then `InvGridClearItemNode` self = other = the bag grid, `path:id:265216.nodeGrid.1.3`, undefined | true; the orb node's `o` 81 to 82; the clear's node named `<K_ORBU>` at 3,1; the sub-tab's keys 21 to 20 | pass |
| byname-socket-nonstack | what an identity then thought non-stackable (the gem) answers | as above | `StashAddToStack` as above on `path:id:265536.nodeGrid`, `fp:<K_GEMU>`, 1, 8; then, to end the half state, the same source clear on `path:id:265216.nodeGrid.2.3` | true, not the false the check expected; the gem node gained `o=2`; the bag cell still held `<K_GEMU>` until the clear, after which the sub-tab was back to its 19 keys | fail (the gem merged: base id 38 is stackable, Live 1e's missing `o` being a count of 1, so no non-stackable case exists on this tab) |
| sort-click-control | the Sort click through both instruments: the `InventorySortTab` row and the frame poll | `InventorySortTab` one call (logged 1, unlogged 0), and the re-sort's 19 `GridAddItem` and 19 `s_InvNode` calls, each with the Sort node as self or other | a scripted left click at Sort's box centre (GUI 2394.7,1230.25, window 1796,971), no hold | `poll_any_presses` and `poll_sort_presses` 0 to 1; `last_press` 2393,1228, inside Sort's box | pass |
| node-created | the probe's reply, the `menulayout` row and a screenshot | `UiCreateNode` one call, answering the node; `UiSetActivationFunc` self = other = the stash window, the node and the script | `probe create UiSetFloatingToFalse` | node 268008 at x 2113.1, y 1198.9, box 2016.2,1176.1 to 2211.9,1221.7, sprite `Menu_Button_Chat_spr`, `uiNodeCallstack=ForgePactMoveAll`, text `Move all`, owner the stash window; `visible=0` in the reply, 1 a frame later; the activation a method of `UiSetFloatingToFalse` with the node as self; the loop check found the object's parents `UI_Button_obj` then `UI_Node_Parent_obj`, no confound; the box left of Sort's and below the bag grid's | pass |
| node-press-activation | a click on the bound node through the detour and the poll | `UiSetFloatingToFalse` one call, self the node, other the stash window, one argument, an empty array: the plugin's last line before the crash | two idle reads first (0 presses and 0 calls, twice); then a scripted left click at the node's box centre (GUI 2114.05,1198.9, window 1586,946), no hold | the game ended with no dialog before any read. The game's `crash.txt`: `ERROR in action number 1 of Other Event: User Defined 15 for object UI_Node_Parent_obj: bool argument is unset`; trace `gml_Script_UiSetFloatingToFalse (line 178)`, `gml_Object_UI_Node_Parent_obj_Other_25 (line 7)`, `gml_Object_UI_Parent_obj_Step_0 (line 280)`, `gml_Object_UI_Inventory_Parent_obj_Step_0 (line 3)`, `gml_Object_UI_Stash_obj_Step_0 (line 93)`; last UI node `InventorySort` | fail (crash - the bound script read a member the button does not carry; the capture writes it `crash (fail)`) |
| node-press-poll | the poll's count for that click | - | - | not reached | not-run (instrument: the game crashed at node-press-activation) |
| node-press-byname | the by-name activation | - | - | not reached | not-run (instrument: the game crashed at node-press-activation) |
| node-press-poll-unbound | a click on an unbound node through the poll | - | - | not reached (Live 1g measured it) | not-run (instrument: the game crashed at node-press-activation) |
| node-survives-tab-switch | the node after a bag and a stash tab switch | - | - | not reached (Live 1g measured it) | not-run (instrument: the game crashed at node-press-activation) |
| node-removed-by-name | the node after `UiRemoveNode` | - | - | not reached (Live 1g measured it) | not-run (instrument: the game crashed at node-press-activation) |
| node-gone-on-close | the node after the stash's own close | - | - | not reached (Live 1g measured it) | not-run (instrument: the game crashed at node-press-activation) |
| close-survives | the game after the stash close | - | - | not reached | not-run (instrument: the game crashed at node-press-activation) |
| reopen-shows | the merge after a reopen | - | - | not reached (Live 1g measured it) | not-run (instrument: the game crashed at node-press-activation) |
| saved-stash-has-keys | the merged unit in no live file and the stash still holding the orb | - | - | nothing was saved: after the crash `stash.hss` and `inventory_order_13.hss` were unchanged (Live 1g measured it) | not-run (instrument: the game crashed before any close) |
| saved-bag-lacks-keys | the merged unit under no bag container | - | - | as above | not-run (instrument: the game crashed before any close) |

### Live 1g results

Live 1g ran on 2026-09-28 (the same research build, phaseC-fbca7251, slot
14, **fully automatic**, on the saves Live 1f restored; capture
`.claude/workorders/forgepact-68-move-all-live-1g.md` on the owner's
machine). **All 22 checks passed** and the game never ended: the orb merge
by name held through a close by name, a reopen and the saved files, and
**a click on the unbound node was seen by the frame poll, and no armed
routine logged a call with it as self and no dialog appeared** (that it ran
nothing of the game's at all is the static reading, not measured). The character select worked on the first attempt. The bag
grid node was 262197, the stash window 262169 (263679 after the reopen);
the orb sat in `StashSocketGrid` node 262422 (key `0-0-198608718206-15`,
`o` 81), and the bag's Socket sub-tab held base ids 109, 110, 111 and 136
only (19 keys), as expected. The node's shape is § Phase A' shapes' button
block; the logged shape for the merge is Live 1f's.

| Check | What it reads | Logged shape | Supplied shape | Result | Verdict |
|---|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the build of § Live procedure 1f | - | - | lease `dll_sha256` fbca7251..., equal to the build's | pass |
| marker | `craftprobe` first line | - | - | `craftprobe: phase1k rows=291 - research instrument ...` | pass |
| control | `craftprobe hook` line, `CheckPlayerInteraction` rising | - | - | `290 detoured, 0 failed, 1 held`; `calls=0`, then `calls=23100` | pass |
| byname-control | the hook line, the armed rows idle after the arm, and by-name calls dispatching | - | the by-name calls of steps 5 to 7 | all 45 armed rows read 0 before the first by-name call; every call dispatched, each array or node id the one `menulayout` read | pass |
| socket-copy | the copy verb's reply and the bag's Socket sub-tab | - | `probe copy` of the orb's key, one | `<K_ORBU>` with `o=1`, `confirmed`, cell 3,1; the sub-tab's keys 19 to 20 | pass |
| byname-socket-merge | the orb's node count up by 1 and `<K_ORBU>` in no bag cell | Live 1f byname-socket-merge | `StashAddToStack` self = other = the bag grid, `path:id:262422.nodeGrid`, 9, 2, `fp:<K_ORBU>`, 1, 8; then `InvGridClearItemNode` self = other = the bag grid, `path:id:262197.nodeGrid.1.3`, undefined | true; `o` 81 to 82; the clear's node named `<K_ORBU>` at 3,1; the sub-tab's keys 20 to 19 | pass |
| merge-close | the game after the stash close by name, before the button block | - | the by-name close (its first reply "not confirmed", then a mouse move and a re-read) | the stash had no live instance; the game running | pass |
| reopen-shows | the merge after the reopen | - | - | the orb's node (found again after the reopen) read `o` 82; `<K_ORBU>` in no bag cell, the sub-tab back to its 19 keys | pass |
| sort-node | the bag's Sort node by id | - | `probe sort id:263722`, the id from the `InventorySort` row | x 2303.5, y 1198.9, box 2303.5,1198.9 to 2485.9,1261.6, sprite `Inventory_Tab_Button_Solid_spr`, visible 1, `uiNodeCallstack=InventorySort`, text `Sort Tab`, `sort poll armed` | pass |
| sort-activation | the Sort node's activation, read hook-free | - | as above | a method of `InventorySortTab`, self an instance, the Sort node (263722) | pass |
| sort-click-control | the Sort click through both instruments | `InventorySortTab` one call (logged 1, unlogged 0) | a scripted left click at Sort's box centre (window 1796,972), no hold | `poll_any_presses` and `poll_sort_presses` 0 to 1; `last_press` 2393,1229 with `in_sort=1` | pass |
| node-created | the probe's reply, the `menulayout` row and a screenshot | `UiCreateNode` one call; no `UiSetActivationFunc` | `probe create none watch:UiSetFloatingToFalse` | node 264522, `activation left unbound (none)`, `activationFunc=undefined`, the same box, sprite and text as Live 1f's node, `visible=1` a frame later, the loop check with no confound | pass |
| node-idle | the counters across two reads with no input | - | two `probe show` reads six seconds apart | `poll_presses` 0, `poll_any_presses` 1, `poll_sort_presses` 1, `row_calls_self_node` 0, both times | pass |
| node-press-poll-unbound | a click on the unbound node: the poll, the game, every armed row | no armed row with the node as self (only the hover row `UiSetFocus`, allowed by the procedure) | a scripted left click at the node's box centre (window 1586,947), no hold | the game running, no dialog on screen; `poll_presses` 0 to 1, `poll_any_presses` 1 to 2, `last_press` 2113,1198 with `in_node=1`; `row_calls_self_node` 0 | pass |
| node-press-negative | a click on the panel background between the node and Sort | - | a scripted left click at GUI 2257.7,1198.9 (window 1693,947), background on the screenshot | `poll_any_presses` 2 to 3; `poll_presses` and `poll_sort_presses` unchanged at 1; `in_node=0 in_sort=0`; the game running | pass |
| node-survives-tab-switch | the node after a bag and a stash tab switch | - | the bag to its Materials sub-tab, then the stash to shared tab 1, by name | listed after both with the same id, 264522, and `visible=1` | pass |
| node-removed-by-name | the node after the probe's removal | `UiRemoveNode` one call, self = other = the stash window, the node | `probe remove` | `listed after: no`; no `ForgePactMoveAll` row | pass |
| node-gone-on-close | a listed node after the stash's own close | - | a new unbound node (265390), then the by-name close | the game running; no `UI_Button_Small_obj` row listed at all | pass |
| reopen-no-stale-node | the reopened stash | - | the stash reopened | five rows (Sort, Split Stack, the two page arrows, the stash's sort), none `ForgePactMoveAll`; `probe show` reads `node=none` | pass |
| close-survives | the game after the stash close | - | the by-name close | the game running; `menulayout` answered its header, no stash window listed | pass |
| saved-stash-has-keys | the merged unit in no live file and the stash still holding the orb | - | `tools/save_item_keys.py` with `--key` for `<K_ORBU>` and for the orb's key | `<K_ORBU>` in no file (merged away); the orb's key under `stash.hss` `socket_tab`; `hs_saves_inspect` had listed `stash.hss` and `inventory_order_13.hss` as changed | pass |
| saved-bag-lacks-keys | the merged unit under no bag container, beside a positive control | - | as above, with `--key` for an unmoved bag socketable (base id 109) | `<K_ORBU>` in no file; the control listed under `inventory_order_13.hss` `inventory_socket_tab` in both the live file and the pre-session copy | pass |

### Live 2 results

Live 2 ran on 2026-09-28 on the **player build** (§ Ship design),
`modfiles_shipped/BloodPactPlugin.dll` SHA-256
`64a55e8d66c1f89c5c766c8f1b1b8e0a0f00869e42751d45121a9bd4ff8b16ed`, slot 14,
**fully automatic** (no person step, and no key the mod moved was moved back by
hand; capture `.claude/workorders/forgepact-68-move-all-live-2.md` on the
owner's machine). All 19 checks passed, each read by `tools/live_checks.py`
from the capture's `## Checks` block, and the game never ended; the saves were
restored afterwards and inspected clean. The session's bag page 0 held 7 grid
items (one of one cell, the others of three or six), the Personal tab 4 items
in 18 cells.

This is the procedure's **third run**; the table is the third run's, since a
capture is never repaired. The first run (the same plugin source, built as
SHA-256 `4171ad7425e5d2335093a57824229aa51b37d58271fc354770142240b5b1e5a2`)
passed every case it scored, but wrote its checks under a heading the check
reader does not read and scored `single-control` in its step text only. The
second run (this build) left `case-full` not observed, because no tab then met
the procedure's free-cell reading of a nearly full tab, and failed
`saved-bag-lacks-keys`, because the operator moved the mod's keys back into the
bag by hand to reuse them. The procedure was then amended: the full tab is the
first tab whose room check refuses an item, found by trying tabs in order, and
no key the mod moved is moved back by hand.

Three cases ran on something other than the plan's first reading, and a fourth
exercised less than its name says:

- **F4 with the switch on** started five of the session's six runs (case-full's
  two tabs, case-refused, case-material-new with case-stack, and case-socket),
  each an `hs_input` press of key 115 that printed its run's lines. That is the
  positive control, in the same session and through the same instrument, for
  `off-baseline-key`'s zero.
- **case-full's bag** was refilled by the game's own Ctrl + left click from the
  stash, because `hs_give_item` refuses every grid-tab class (its
  `giveitem` reports that `GetItemPreferredGrid` answered no grid); the
  refills are setup,
  not under test, and never took a key the mod had moved. Four Personal-tab
  items went first, to Shared tab 1, which took all four; four items taken
  from Shared tab 3 then went to Shared tab 2, which refused all four. The
  first run saw the same answer on Shared tab 2 for 7 multi-cell items, with
  200 of 306 cells taken in scattered one-cell gaps: none fitted, so the room
  check is per item, not a free-cell total.
- **case-socket** ran with 19 socketables in the bag's Socket view, and **none
  of their kinds was on the Socketable tab**, so every one was the planned
  skip `a new kind stays in the bag`. The by-name socket merge was therefore
  **not exercised by the player build**; it was measured on the research build
  only (Live 1f, Live 1g).
- **button-press** carried case-mixed's run. In the first run, turning the
  switch on again **with the stash still open did not bring the button back**
  until a stash tab was clicked (a new node, a new id, then listed). The third
  run clicked a stash tab after switching on, as a known gap, rather than test
  it again. The cause is not established; F4 and `stashmoveall run` are not
  affected by it.

| Check | What it reads | Supplied | Result | Verdict |
|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the dispatched build | - | lease `dll_sha256` 64a55e8d..., `dll_status=hashed`, equal to the dispatched build's | pass |
| marker | the first line of bare `stashmoveall` | - | `stashmoveall: state=off key=F4 button=none presses=0 in_node=0 outside=0 unread=0 errors=0 taken=0 dropped=0 last_drop=none` | pass |
| control | a stash tab switch by name | `hs_stash_tab("shared1")` | `stashtab: before=0 after=1 handler=UiAStashTabClick` | pass |
| off-baseline-key | F4 with the switch off and the stash open | F4 (key 115) held 60 ms | the bag's 7 items and the Personal tab's 4 in the same cells after as before; no `stashmoveall:` line in the log tail. Its positive control is the same session's F4 with the switch on, which started the case-full, case-refused, case-material-new and case-socket runs | pass |
| off-baseline-verb | the run verb with the switch off | `stashmoveall run` | `stashmoveall: refused - off; nothing was called` | pass |
| off-baseline-button | the stash's small buttons with the switch off | `menulayout UI_Button_Small_obj` | five rows (Sort, Split Stack, the two page arrows, the stash's sort), none `ForgePactMoveAll` | pass |
| single-control | the one-item verb | `stashmove` of the bag page's one-cell item | `stashmove: moved <key> -> cell 0,0`; the key on the Personal tab and in no bag cell | pass |
| button-press | the button's row, a scripted click on it, the state line, off and on | a left click at the node's centre (window 1585,947, from the row found by its call-stack name) | one `ForgePactMoveAll` row, `text=Move all`, `visible=1`, left of Sort (window x 1585 against Sort's 1728); after the click `button=held presses=1 in_node=1 outside=0 unread=0 errors=0 taken=1 dropped=0`; one run; `stashmoveall 0` removed the row; `stashmoveall 1` and a stash tab click listed it again with a new id | pass |
| case-mixed | the run the click started, into a tab with room | 6 items on bag page 0 (the seventh moved by single-control), the Personal tab shown | `moved 6 of 6 from bag tab 0 to stash tab 0; skipped 0`; every key on the Personal tab, the bag page empty | pass |
| case-full | the first tab whose room check refuses an item, the tabs around it, and every saved stash container | 4 items on bag page 0 and F4, Shared tab 1 shown, then after a refill Shared tab 2 | Shared tab 1: `moved 4 of 4 from bag tab 0 to stash tab 1; skipped 0`. Shared tab 2: `moved 0 of 4 from bag tab 0 to stash tab 2; skipped 4`, each `skipped: no room on the shown tab`, the 4 in their bag cells, the tab's cells unchanged; across that press, Shared tab 3 (read against its state after the refill had taken 4 items from it) and the Personal tab (read against its state after case-mixed) unchanged cell for cell. The saved `stash.hss` against its copy from before the stash was opened: only `material_tab` (69 to 72 keys), `stash_tab_1` (62 to 66) and `stash_tab_3` (149 to 145, the refill's source) differ; `stash_tab_2` and every other container list the same keys | pass |
| case-refused | non-stackables into the Materials tab | the same 4, bag page 0 confirmed before the Materials tab click, F4 | each `skipped: not taken by the Materials tab`; `moved 0 of 4 ... skipped 4`; the bag and the tab unchanged. The capture quotes the summary with `from bag tab -4`, a bag number it does not explain after page 0 was confirmed; the 4 items planned were the page's 4 | pass |
| case-material-new | new material kinds into the Materials tab | the bag's Materials view: 3 materials plus one copied by `hs_give_item` from one of them (4), none of their kinds on the tab, F4 | the copy and the two other kinds each at a cell of their own (3,15, 3,16, 3,17), out of the bag | pass |
| case-stack | the second item of one kind in the same run | the copy's original, count 1 | `-> stack`; in no bag cell, not listed on its own on the tab, in no saved file (the mod's own line says it merged into the copy's stack, which the run itself had just placed; that stack's count was not read after the run, so these readings do not tell a merge from a loss) | pass |
| case-socket | the Socketable tab from the bag's Socket view | 19 socketables, F4 | every one `skipped: a new kind stays in the bag`; `moved 0 of 19 ... skipped 19`; the merge not reached (no kind on the tab) | pass |
| close-survives | the game after the stash close | `hs_stash_close` | no `UI_Stash_obj` listed on a direct read; the game running (the tool's own re-read said "not confirmed" again, the race) | pass |
| reopen-shows | the reopened stash | `hs_stash_open` | the 14 keys the mod moved on the Personal tab (7), Shared tab 1 (4) and the Materials tab (3), none in the bag; the merged key in no grid | pass |
| saved-stash-has-keys | the moved keys in the saved files | `tools/save_item_keys.py --key` on `stash.hss`, `herosiege13.hss`, `inventory_order_13.hss` | the 7 under `herosiege13.hss` `inventory.personal_stash`, 4 under `stash.hss` `stash_tab_1`, 3 under `stash.hss` `material_tab` | pass |
| saved-bag-lacks-keys | the same keys in the bag's saved file | as above | none of the 14 in `inventory_order_13.hss` | pass |
| no-duplicate | every moved key under exactly one container | as above | each of the 14 under exactly one container, as in the last in-game listing; the merged key in none | pass |

### Live 3 results

Live 3 ran on 2026-09-30 on the **research build** (§ Live procedure 3),
`plugin_build/BloodPactPlugin_rel.dll` SHA-256
`810d28da164b1ec0b9a9306b313559d3984b4de10a8ec6cc917322826df60e62`, slot 14,
fully automatic (the character picked by name at once; capture
`.claude/workorders/forgepact-68-move-all-fix-live-1.md` on the owner's
machine). It is ForgePact #131's confirmation. **17 of the 18 checks passed
and one, `material-new`, was not run**; `tools/live_checks.py` reads the same
18 verdicts from the capture's `## Checks` block. The game never ended, and
the saves were restored afterwards and inspected clean.

These ran otherwise than the procedure first said:

- **material-new was not run.** No bag material was of a kind with no stack
  on the tab: the bag's two (base ids 72 and 73, counts 27 and 3) each met a
  full stack of 999. Both went `-> cell` as new stacks beside the full ones
  (the kind sums 999 to 1026 and 999 to 1002), which is the full-stack case,
  not the new-kind one. A new kind placed as a new stack by the #131 adapter
  is therefore **not observed** here (Live 2's `case-material-new` saw it on
  the #68 adapter).
- **button-placed passed on the relation, not on the absolute box.** The
  procedure's expected box (about 2099.8,1207.5,2295.5,1253.1) was worked out
  from Live 1f and 1g's Sort, and this session's GUI scale was another: Sort
  192 wide against 182.4, the node 206 against 195.7. The relation held
  exactly (below), which is what reading the extents from the node is for.
  The node's x, y (2178.0, 1295.0) sat within one GUI unit of its bbox
  centre, as in Live 1f and 1g.
- **button-press counted 4 presses**, not the 1 expected: `in_node=1` and
  `taken=1` as expected, and the 3 `outside` are, by inference, the three
  setup Ctrl + clicks that refilled the bag from the Personal tab
  (`hs_give_item` refuses grid-tab items, as in Live 2).
- **socket-whole needed a clean re-run.** The first run's copy of 3 of the
  orb merged (`-> stack`), but the orb's node rose by 8 (84 to 92): the bag's
  Socket view held 54 socketables, and other orbs of that kind merged in the
  same run (inferred; the bag was not read before the press). A re-run with
  only the 20 skipped keys and a new copy of 3 in the bag raised the node by
  exactly 3 (92 to 95).
- **socket-single used base id 8**, because the node of base id 38 read no
  numeric `o`; the procedure allows any two filled kinds. **K_M was base id
  60** (`o` 875), because base ids 72 and 73 held two stacks each after
  step 5 and the procedure asks for a kind with one.
- **hs_stash_close answered `not confirmed`** though the window closed (a
  screenshot, the game running, and a new stash window id on the reopen): an
  hs-drive gap, not a result of the mod.

| Check | What it reads | Supplied | Result | Verdict |
|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the build | - | lease `dll_sha256` 810d28da..., equal to the build's, unchanged since taken | pass |
| marker | the first line of bare `stashmoveall` | - | `state=off key=F4 button=none`, every counter 0, `button_place=none` | pass |
| control | a stash tab switch by name | `hs_stash_tab("shared1")` | `stashtab: before=0 after=1 handler=UiAStashTabClick` | pass |
| copy-control | the first `probe copy` | a copy of K_M (base id 60) with `o` 125 | `confirmed`; the bag's Materials view gained one key with `o` 125 | pass |
| off-baseline-button | the stash's small buttons with the switch off | `menulayout UI_Button_Small_obj` | five rows, Sort's (`InventorySort`) bbox 2290.0,1262.0,2482.0,1328.0, none `ForgePactMoveAll` | pass |
| button-placed | the button's row against Sort's | `stashmoveall 1`, then a stash tab clicked | one `ForgePactMoveAll` row, `text=Move all`, `visible=1`, bbox 2076.0,1271.0,2282.0,1319.0: its right edge 2282 is Sort's left less 8, its vertical centre 1295 is Sort's; the mod's own line `placed beside Sort` named the same box | pass |
| button-press | a scripted click at the button's centre | 3 items on bag page 0 (put there by setup Ctrl + clicks), a left click at GUI 2178,1295 | `moved 3 of 3 from bag tab 0 to stash tab 0; skipped 0`, each `-> cell`; `presses=4 in_node=1 outside=3 taken=1`; the keys on the Personal tab, the bag page empty | pass (presses 4, not 1; `in_node` and `taken` as expected, the 3 outside inferred to be the setup clicks) |
| material-new | a bag material whose kind has no stack on the tab | the bag's Materials view: base ids 72 (27) and 73 (3), each beside a full 999 stack | no kind without a stack was available; both `-> cell` as new stacks (sums 999 to 1026 and 999 to 1002), the bag empty - the full-stack case, not the new-kind one | not-run (no bag material with a stackless base id; `-> cell` seen only for the full-stack case) |
| material-overflow | a unit that would carry K_M past 999 | U1, a copy of K_M (base id 60, `o` 875) with `o` 125, F4 | U1 `-> cell 4,0`, a new stack of 125; K_M still 875; the kind's sum 875 to 1000; U1 in no bag cell; no stack above 999; `skipped 0` | pass |
| material-partial | a unit that fits the new stack but not K_M | U2, a copy with `o` 129, F4 | U2 `-> stack` into U1's stack (125 to 254), K_M still 875 (875 + 129 would pass 999); the sum 1000 to 1129; U2 in no bag cell; no stack above 999 | pass |
| socket-whole | the orb's node after a stack of 3 merged | U_ORB, a copy of the orb (base id 118) with `o` 3, F4 | first run `-> stack`, the node 84 to 92 (+8: other orbs of the kind in the bag merged in the same run, inferred); clean re-run, a new copy of 3: `-> stack`, the node 92 to 95, exactly +3; no copy in a bag cell | pass (exactly +3 on the clean re-run) |
| socket-single | a gem's node after one unit merged | U_GEM, a copy of a gem (base id 8) with `o` 1 | `-> stack`, the node 196 to 197; U_GEM in no bag cell | pass |
| socket-new-stays | the bag socketables whose kind has no node | the rest of the bag's Socket view | 20 lines `skipped: a new kind stays in the bag` (first run `moved 36 of 56 ... skipped 20`, re-run `moved 1 of 21 ... skipped 20`); those 20 exactly the bag's filled cells afterwards | pass |
| close-survives | the game after the stash close | `hs_stash_close` | `hs_status` running, the window closed on a screenshot, a new stash window id on the reopen; the tool itself answered `not confirmed` (an hs-drive gap) | pass |
| reopen-shows | the reopened stash | `hs_stash_open` | step 4's 3 keys on the Personal tab; U1 (254) and step 5's two on the Materials tab; the bag holding only the 20 skipped socket keys; U2, U_ORB and U_GEM in no grid | pass |
| saved-stash-has-keys | the placed keys in the saved files | `tools/save_item_keys.py --key` on `herosiege13.hss`, `stash.hss`, `inventory_order_13.hss` | step 4's 3 under `herosiege13.hss` `inventory.personal_stash`; step 5's 2 and U1 under `stash.hss` `material_tab` | pass |
| saved-bag-lacks-keys | the same keys in the bag's saved file | as above | `inventory_order_13.hss` holds only the 20 skipped keys, under `inventory_socket_tab` | pass |
| no-duplicate | every placed key under exactly one container | as above | each placed key under exactly one container; U2, both U_ORB copies and U_GEM (merged away) in no file | pass |

### Live 4 results

Live 4 ran on 2026-09-30 on the **research build** (§ Live procedure 4),
`plugin_build/BloodPactPlugin_rel.dll` SHA-256
`6a50a2f56b1461b2081210ed7480b749c99f16048053e9e8cc5de889abc4e1cc`, slot 14,
fully automatic (the character picked by name at once; capture
`.claude/workorders/forgepact-68-move-all-fix-live-2.md` on the owner's
machine). **All 10 checks the procedure names passed on their numbers**, and
`tools/live_checks.py` reads the same 10 verdicts from the capture's
`## Checks` block. The game never ended, and the saves were restored afterwards
and inspected clean.

**What the numbers cannot see failed.** The ten checks read the node's box,
its size, its `sprite_index` as a name and the mod's own state line; the
screenshot taken at step 3 (`20260930T162809916688Z_live2-button-placed.png`,
the owner's machine) is the only look at what the game draws, and the
procedure left it out of the verdict. It shows:

- **The label is not in the box.** The box at the node's bbox is drawn
  empty. Only a clipped end of the `Move all` text shows, at about the box's
  top-left corner (the screenshot is 2560x1440, the same as the GUI here, so
  its pixels line up with the GUI numbers within a few units), overlapping
  the frame above it and cut off on the left by the bag sub-tab icon cell
  beside it, in a heavier type than Sort Tab's label. The node's x, y are
  2090, 1262, that top-left, since wearing Sort's sprite moved its origin
  there. That the game draws a `UI_Button_Small_obj` node's `text` centred on
  the node's x, y, which was the box's centre under the object's own sprite
  and is its corner under Sort's, fits what was seen and is **not
  established**: neither the draw that places the label nor how the Sort node
  gets its label centred is read.
- **The box does not read as Sort's to the owner.** The operator's note and
  the owner describe the node as a dark, empty box, not the red look they know
  Sort by. In the same screenshot the backpack's own **Sort Tab** button
  (`InventorySort`, the node the look is copied from) is drawn as the same
  dark framed box, while the red **Sort Tab** at the top of the stash window is
  the stash's own button (`StashSort`). Whether the owner's red look is that
  button, a hover or pressed state, or something the copy misses is **not
  established**; the box is not recorded as looking like Sort's.

`button_look=sort` and `probe sort`'s `sprite=` both read back the member the
mod itself wrote, so they show the write held on the node, not what the game
draws with it.

| Check | What it reads | Supplied | Result | Verdict |
|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the build | - | lease `dll_sha256` 6a50a2f5..., equal to the build's, `dll_status hashed` | pass |
| marker | the first line of bare `stashmoveall` | - | `state=off key=F4 button=none`, every counter 0, `button_look=none button_size=none` | pass |
| control | a stash tab switch by name | `hs_stash_tab("shared1")` | `stashtab: before=0 after=1 handler=UiAStashTabClick` | pass |
| off-baseline-button | the stash's small buttons with the switch off | `menulayout UI_Button_Small_obj` | five rows, Sort's (`InventorySort`, id 262105) `gui=2290.0,1262.0` bbox 2290.0,1262.0,2482.0,1328.0 (192x66), sprite `Inventory_Tab_Button_Solid_spr`, none `ForgePactMoveAll` | pass |
| button-placed | the button's row against Sort's | `stashmoveall 1`, then Shared 1 and Personal clicked, about 2 s | one `ForgePactMoveAll` row (id 262287), `text=Move all`, `visible=1`, `gui=2090.0,1262.0`, bbox 2090.0,1262.0,2282.0,1328.0 (192x66): right edge 2282 is Sort's left less 8, both vertical centres 1295, width and height Sort's; the mod's own line `placed beside Sort, box 2090.0,1262.0,2282.0,1328.0`, printed on the enable itself | pass |
| button-state | the bare state line | `stashmoveall` | `button_place=on button_box=2090.0,1262.0,2282.0,1328.0 button_extents=0.0,0.0,192.0,66.0 button_makes=1 button_look=sort button_size=192.0x66.0`: the first node made with Sort's own extents landed on target with no remake | pass |
| button-press | a scripted click at the box's centre | 2 items on bag page 0 (setup Ctrl + clicks from the Personal tab), a left click at GUI 2186,1295 | `moved 2 of 2 from bag tab 0 to stash tab 0; skipped 0`, each `-> cell`; `in_node` 0 to 1, `taken` 0 to 1, `outside=2` (the setup clicks) | pass |
| button-look | each node's sprite by name | `stashmoveall probe sort id:262105`, then `id:262287` | Sort `sprite=Inventory_Tab_Button_Solid_spr` (the positive control, as in Live 1f and 1g); the node `sprite=Inventory_Tab_Button_Solid_spr`, not `Menu_Button_Chat_spr` | pass |
| close-survives | the game after the stash close | `hs_stash_close` | `hs_status` running (pid 60628), the window gone on a screenshot; the tool itself answered `not confirmed` (the hs-drive gap Live 3 saw) | pass |
| reopen-placed | the reopened stash's button | `hs_stash_open`, Shared 1 then Personal, about 2.5 s | Sort id 263002 bbox 2290.0,1262.0,2482.0,1328.0; one `ForgePactMoveAll` row, id 263037, bbox 2090.0,1262.0,2282.0,1328.0; `button_place=on button_look=sort button_makes=1` | pass |
| button-drawn (not a procedure check) | what the game draws for the node | the step 3 screenshot, and the reopen's | the box drawn at the node's bbox with no label inside it; a clipped end of `Move all` at about its top-left corner, under the frame above and behind the bag sub-tab icon beside it; the box read by the owner as dark and empty, not Sort's red look (the backpack's Sort Tab beside it is drawn the same dark way in that frame) | fail (visual, screenshot) |

### Live 5 results

Live 5 ran on 2026-09-30 on the **research build** (§ Live procedure 5),
`plugin_build/BloodPactPlugin_rel.dll` SHA-256
`e80f30dec0548fa8fabdabc2facd0ad6d29d97a614c31dc276accc95474c9980` (the
lease's hash, equal to the build's), slot 14, fully automatic: the character
was picked by name at once, and the owner's bag key, `C`, opened the bag on
its own through `hs_input` and closed it again, so no step was done by hand
(capture `.claude/workorders/forgepact-68-move-all-fix2-live-1.md` on the
owner's machine). **16 of the 18 checks passed and two failed, both of them
research checks whose failure is a finding**: the Mercenary button is not
listed while the stash is open (`merc-stash-listed`), and `lookcopy missing`
had nothing to write (`label-trial-missing`); the `changed` tier then centred
the label. `tools/live_checks.py` reads the same 18 verdicts from the
capture's `## Checks` block. The game never ended, and the saves were
restored afterwards and inspected clean. GUI and window 2560x1440 throughout.

What it settled. **Where Move all goes:** with the bag open on its own the
game's own Mercenary button (a `UI_Button_Open_Mercenary_obj`) sits left of
InventorySort, Sort's size, level with it, its right edge 4 GUI units left of
Sort's; with the stash open it is not listed at all, while InventorySort's box
is the same in both states. So the target is Sort's box moved and sized by
that relation (§ Decision `buttonTarget`). **What centres the label:** after
`lookcopy changed` wrote InventorySort's 13 differing writable members onto
the node, the label check read the node's label centred like Sort's, with the
node's box unchanged (§ Decision `buttonLabel`). No struct or array member
differed between the Sorts and the node beyond `activationArgs` (the node's
dump read `nested=0`, Sort's `nested=1`), so no label place was hidden inside
one.

| Check | What it reads | Supplied | Result | Verdict |
|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the build | - | lease `dll_sha256` e80f30de...9980, `dll_status hashed` | pass |
| marker | `stashmoveall probe help` | - | the usage line naming `dump <label> id:<n>`, `diff <a> <b>` and `lookcopy id:<src> missing\|changed` | pass |
| control | a stash tab switch by name | `hs_stash_tab("shared1")`, run at step 4 (it refuses `stash_not_open` before) | `stashtab: before=0 after=1 handler=UiAStashTabClick` | pass |
| bag-alone-open | InventorySort with the bag open on its own | `hs_input` key `C` (vk 67), 1 s; `menulayout UI_Button_Small_obj` | id 262027 `gui=2290.0,1262.0` bbox 2290.0,1262.0,2482.0,1328.0 (192x66), `visible=1`, `uiNodeCallstack=InventorySort`; header `gui=2560x1440` | pass |
| merc-bag-read | the Mercenary button's row, bag alone | `menulayout UI_Button_Open_Mercenary_obj` | `UI_Button_Open_Mercenary_obj` id 262039 `gui=2094.0,1262.0` bbox 2094.0,1262.0,2286.0,1328.0 (192x66), `visible=1`, sprite `Inventory_Tab_Button_Solid_spr`, `uiNodeCallstack=InventoryMercenary`, `text=Mercenary`; its right edge 2286 left of Sort's 2290, the same top and bottom | pass |
| dump-control | the instrument on a known node | `probe dump sortbag id:262027` | `names=85 nested=1`, `uiNodeCallstack=InventorySort`, `text=Sort Tab` | pass |
| merc-label | the label tool on the Mercenary button (research) | the bag-alone screenshot, box 2094,1262,2286,1328 against Sort's | `box: centred ... offset=0.5,2.0`, `ref: centred ... offset=-0.5,-1.0`, exit 0 | pass |
| merc-stash-listed | the Mercenary node with the stash open (research) | `hs_stash_open`, Personal; `menulayout UI_Button_Open_Mercenary_obj` | `listed=0 absent=none`: not listed | fail (research: not listed) |
| sort-same-both | InventorySort's box with the stash open (research) | `menulayout UI_Button_Small_obj` | id 262329 bbox 2290.0,1262.0,2482.0,1328.0, equal to the bag-alone box on every side; StashSort id 262363 | pass |
| node-made | the mod's node | `stashmoveall 1`, Shared 1 then Personal, 2 s | one `ForgePactMoveAll` row, id 262500, bbox 2090.0,1262.0,2282.0,1328.0, `text=Move all`, `visible=1` | pass |
| label-tool-control | the label tool's reference | the node's screenshot, box N against Sort's | `ref: centred` (548 label px, offset -0.5,-1.0) on every run; exits 0 or 1, never 2 | pass |
| label-baseline | Live 4's defect, reproduced | the same run | `box: not centred (offset,edge,pixels)`, 175 px, label box 2096,1262,2138,1270, offset -69.0,-29.0, exit 1 | pass |
| dump-diff | the dumps' differences | `probe dump` of both Sorts, StashSort and the node; four diffs | `diff sortstash node: changed=21 added=0 removed=1`; `diff merc node: changed=21 added=0 removed=0`; `diff sortstash stashsort: changed=24 added=0 removed=0`; `diff sortbag sortstash: changed=3 added=0 removed=0` (only `id`, `masterUi`, `parent`) | pass |
| label-trial-missing | `lookcopy missing` from Sort, then the label tool | `probe lookcopy id:262329 missing` | `wrote=0 ... excluded=1`: the node lacks no member Sort has; the label still at its corner, exit 1 | fail (nothing to write) |
| label-trial-changed | `lookcopy changed` from Sort, then the label tool | `probe lookcopy id:262329 changed` | 13 members written and read back (`dropShadow`, `textFont`, `createX`, `drawXOffset`, `drawYOffset`, `navBboxHeight`, `navBboxWidth`, `navBboxX`, `navBboxY`, `naviDown`, `naviDownPrev`, `naviRight`, `naviRightPrev`); the node's bbox unchanged; `box: centred`, 532 px, label box 2136,1286,2238,1303, offset 1.0,-0.5, exit 0 | pass |
| trial-press | a click on the node after the trial | a left click at 2186,1295 | `in_node` 0 to 1, `presses=1 taken=1` (the bag page empty, no `moved` line) | pass |
| close-survives | the game after the stash close | `hs_stash_close` | `hs_status` running (pid 80924); the tool answered `not confirmed`, the screenshot shows stash and bag closed | pass |
| bag-after-close | InventorySort and the Mercenary row after the close (research) | `menulayout` of both objects | neither listed (`listed=0` each) | pass |

### Live 6 results

Live 6 ran on 2026-09-30 on the **research build** (§ Live procedure 6),
`plugin_build/BloodPactPlugin_rel.dll` SHA-256
`27ceff5cb71be774a13d54bf6594fbac02447b10d895de3ded02798e4ade15da` (the
lease's hash, equal to the file's), slot 14, fully automatic: the character
was picked by name on the first try and the bag key `C` went through
`hs_input`, so no step was done by hand (capture
`.claude/workorders/forgepact-68-move-all-fix3-live-3.md` on the owner's
machine). **Every required check passed**; the research check
`label-trial-control` did not run, because `label-centred` passed.
`tools/live_checks.py` reads the same 12 verdicts from the capture's
`## Checks` block. The game never ended, and the saves were restored
afterwards and inspected clean. GUI and window 2560x1440 throughout.

What it settled. **The shipped path draws the button Live 5 decided:** the
node sat exactly on the Mercenary button's box, worked out from
InventorySort's by Live 5's relation (`button_ref=relation`), all 16 look
members read back as Sort's (`button_look_same=16/16`), and the label check
read its label centred like Sort's, on the session's first node and again on
the node made after a close and reopen. So the 13 label members copied as read
centre the label with no trial step in between (§ Decision `buttonLabel`), and
`label-trial-control` had no cause to separate. **`textFont` is an asset
reference:** both dumps printed `textFont=ref font __newfont2 (asset)`, not a
string, so the copy compares it by the font's index. **The navigation members
held:** `naviDown`, `naviDownPrev`, `naviRight` and `naviRightPrev` read
`false` on Sort and on the node in step 4, some seconds after the look read,
and the reopened node read 16/16 again; that the game never rewrites them on
one node and not the other in longer play is not established. **The label
tool reads a hovered button as off-centre:** the reopen's first screenshot,
taken with the game's cursor still over the button after the step-6 click,
read `not centred` (1456 label pixels, the box's colour 43,36,35 where every
other capture read 31,23,21); the retake with the cursor moved away read it
centred with the first node's numbers. A screenshot for the label check wants
the cursor off the box.

| Check | What it reads | Supplied | Result | Verdict |
|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the build | - | lease `dll_sha256` 27ceff5c...15da, `dll_status hashed`, equal to the file's | pass |
| marker | the bare `stashmoveall` line, mod off | `stashmoveall` | starts `stashmoveall: state=off key=F4 button=none`, ends `button_look_same=none` | pass |
| control | a stash tab switch by name | `hs_stash_tab("shared1")` after the stash opened | `stashtab: before=0 after=1 handler=UiAStashTabClick` | pass |
| merc-read | the Mercenary row and InventorySort's, bag alone (research) | `hs_input` key `C` (vk 67), 1 s; `menulayout UI_Button_Small_obj`, `menulayout UI_Button_Open_Mercenary_obj` | Mercenary id 260860 bbox 2094.0,1262.0,2286.0,1328.0, `uiNodeCallstack=InventoryMercenary`, `text=Mercenary`; InventorySort id 260848 bbox 2290.0,1262.0,2482.0,1328.0; header `gui=2560x1440` - Live 5's boxes again | pass |
| button-placed | the mod's node against the Mercenary box | `stashmoveall 1`, Shared 1 then Personal, 2 s; `menulayout UI_Button_Small_obj` | one `ForgePactMoveAll` row, id 261161, bbox 2094.0,1262.0,2286.0,1328.0 (equal to the Mercenary box on every side), `visible=1`, `text=Move all`, sprite `Inventory_Tab_Button_Solid_spr`; the reply `button - placed in the Mercenary button's place, box 2094.0,1262.0,2286.0,1328.0` | pass |
| button-state | the bare state line with the node made | `stashmoveall` | `button_place=on button_look=sort button_size=192.0x66.0 button_ref=relation button_look_same=16/16`, `button_makes=1` | pass |
| label-centred | the label tool on the node | screenshot `fix3-live3-button`; `--gui 2560x1440 --box 2094,1262,2286,1328 --ref 2290,1262,2482,1328` | `box: centred`, 532 label px, label box 2140,1286,2242,1303, offset 1.0,-0.5; `ref: centred`, 548 px, offset -0.5,-1.0; exit 0 | pass |
| look-members | the 16 look members, Sort against the node | `probe dump s id:261054`, `probe dump n id:261161`, `probe diff s n` | `changed=8 added=0 removed=1`: `activationArgs`, `activationFunc`, `bbox_left`, `bbox_right`, `id`, `text`, `uiNodeCallstack`, `x`, and `activationArgs[0]` removed - none of the 16; both dumps `navBboxX=2290 (real)`, `navBboxY=1262 (real)`, `textFont=ref font __newfont2 (asset)` | pass |
| label-trial-control | Live 5's trial again on the node (research) | - | `label-centred` passed, so not run | not-run (label-centred passed) |
| button-press | a click on the node's centre | two stash Personal items Ctrl + clicked into the bag through `hs_input`; a left click at 2190,1295 | `moved 3 of 3 from bag tab 0 to stash tab 0; skipped 0` (the two taken and one already in the bag); `in_node` 0 to 1, `taken` 0 to 1 | pass |
| close-survives | the game after the stash close | `hs_stash_close` | `hs_status` running (pid 92460); the tool answered `not confirmed`, the screenshot shows the stash window gone | pass |
| reopen-placed | the node after a close and reopen | `hs_stash_open`, Shared 1 then Personal, 2 s; `menulayout`; `stashmoveall`; the label tool | node id 261704, bbox 2094.0,1262.0,2286.0,1328.0, InventorySort's box unchanged; `button_place=on button_look=sort button_look_same=16/16`; the label tool exit 1 with the cursor over the button, exit 0 on the retake with it moved off (offset 1.0,-0.5) | pass (on the retake; the first capture had the cursor over the label) |

## Decision

Each line is set from a session's capture: `byname` with the shape that
worked, `not-observed` with what was supplied, or `shape not reproduced`.
Live 1 set every line first. Live 1b measured no route (its lookup and click
controls failed), so it changed no line's verdict; Live 1c and Live 1d set
the lines below, each from the session that measured it: a line Live 1d
measured from Live 1d (`gridMoveRoute`, `sourceCellClear`, the shared half of
`mapOwnerRule`), a line only Live 1c measured from Live 1c (`stackMoveRoute`,
`gestureRoute`, `targetTabRule`, the personal half of `mapOwnerRule`), and
`bagSubtabRoute` from Live 1, with what Live 1c added. From Live 1b on, three
lines are written per tab kind, because the static reading puts the personal
tab and the shared tabs in different maps (§ Static reading 2):
`gridMoveRoute` names the array, self and third argument the placement takes
for each kind (`personal: <array> <self> <a2>; shared: <array> <self> <a2>`),
`mapOwnerRule` names the map and the owner step for each (`personal: map <n>
<step>; shared: map <n> <step>`), and `gestureRoute` names the input that
quick-moves (`ctrl-click`, from the hint strip and Live 1b's gestures).
`targetTabRule` (what decides the tab an item lands on, and whether it can
spill to another tab) is set only from a quick move made by the game's own
input against a full shown tab (`grid-move-tab-full` in Live 1,
`gesture-ctrl-full` in Live 1b, `hand-full` in Live 1c), never from a by-name
placement (`tab-full-byname`, `byname-full`), and only when a quick move by
the same input moved an item into the stash earlier in the same session
(the positive control; without it "stayed in the bag" cannot be told from a
gesture that did nothing); when that check is `not-run` it reads
`not-observed (<reason>)`, and the mod's own rule (the shown tab's room is checked before
any call, and no route that can pick another tab is used) stands either way.
In the lines below, "the bag grid" is the bag's `InventoryGrid` node (the
same node when the bag's Materials sub-tab is on show), "the stash grid" the
`StashGrid` node, which rebinds to the stash tab on show, and "the shown
tab's array" that node's `nodeGrid` read with the tab on show. Live 1e set
the last three lines, each from that session alone: `socketRoute`, written
per path (`new: ...; merge: ...`, the source being the bag's Socket
sub-tab), `newMaterialRoute` and `wholeStackMerge`. Live 1f and Live 1g set
four more, recorded together: `buttonRoute` (how a press on the Move all
button reaches the plugin: `poll` only when Live 1g's poll control, idle
reads, click inside the unbound node and click outside it all passed with
the game running), `buttonOwner` (what removes the node when the stash
closes), `sortActivation` (the Sort node the button is placed beside, and
how it is found) and `socketMergeRoute` (the Socketable tab's merge by
name); `socketRoute`'s `merge:` part is rewritten from Live 1f. Live 3
(ForgePact #131) set the last three, from that session alone:
`stackCapRule` (from `material-overflow` and `material-partial`),
`socketWholeStackMerge` (from `socket-whole`) and `buttonPlacement` (from
`button-placed`, with the boxes it read). Live 4 (ForgePact #131, owner scope)
set `buttonLook`, from `button-placed`, `button-state` and `button-look` and
from the screenshot the checks do not read. Live 5 (ForgePact #131, the
owner's request of 2026-09-30) set the last two, from that session alone:
`buttonTarget` (from `merc-bag-read`, `merc-stash-listed`, `sort-same-both`
and `merc-label`) and `buttonLabel` (from `dump-diff`, `label-baseline` and
the two trial tiers).

gridMoveRoute: byname (Live 1d byname-personal, byname-personal-clear, byname-shared, byname-shared-owner, saved-stash-has-keys, saved-bag-lacks-keys, all pass). personal: GridAddItem on the shown tab's array (the stash grid's nodeGrid, personal tab on show), self = other = the bag grid, a2 0, a3 undefined, after ValidateItem (self = other = the bag grid) and StashAddToStack (the same self, other and array, 0, 13, the item, 1, 0, answering false for a non-stackable) and followed by ValidateItem with self the stash grid and other the bag grid; shared: the same on shared tab 1's array with StashAddToStack's 9, 2. GridAddItem answers a struct with the tab, x, y and success; success=true places the item at x, y on the array it was handed and nowhere else (Live 1c's hand moves logged exactly this sequence)
stackMoveRoute: byname (Live 1c byname-merge, replaying hand-merge): StashAddToStack, self = other = the bag grid (its Materials sub-tab on show), Controller_obj.stashMaterialTab, 9, 2, the item, 1, 0, answered true and the tab's sum for that base id rose by exactly one unit; then the source clear. It answers false when no stack of the same identity is on the tab (Live 1, Live 1c hand-material); a new identity then goes, by hand, through GridAddItem on the Materials tab's array and the owner step 0 to 9 (Live 1c hand-material; not replayed by name)
mapOwnerRule: personal: map 0, no owner step (Live 1c hand-personal logged none and hand-personal-map read the key on map 0 as before; Live 1d byname-personal-clear; saved in herosiege13.hss under inventory.personal_stash); shared: owner step 0 to 9 after the placement, ChangeItemOwner with self the stash grid, other the bag grid, 0, 9 and the key as text, answering undefined (Live 1c hand-shared logged it; Live 1d byname-shared-owner replayed it: the item stayed in its cells and its lookups turned into a shared-tab key's, undefined on map 0 and on map 9; saved in stash.hss under stash_tab_1). Never before a confirmed placement: alone it leaves the key on no map while it sits in the bag (Live 1c step 8, reversed). The Materials tab's new identity took the same shared step by hand (Live 1c hand-material); a merge takes none (Live 1c hand-merge). Which map holds a shared-tab entry is not established: the owner 9 lookup misses every one
bagSubtabRoute: byname (cells readable: the bag's Materials sub-tab is New_Inventory_Data_obj.inventoryMaterialGrid, an array 6 by 15 of cells carrying fingerprints, Live 1 bag-subtab-source; Live 1c: hs_bag_tab switches the bag to it by name and the bag grid node rebinds to it, while the way back to the bag's first page is the page-tab click, which logged UiAInventoryTabClick and InventoryResetTabs by hand and is not measured by name)
gestureRoute: ctrl-click (by hand: Live 1c hand-personal, hand-shared, hand-material and hand-merge, each the owner's own Ctrl + left click moving the item into the tab on show; a scripted click through hs_input never reached the grid's pick-up, Live 1b click-control, so no scripted gesture is measured)
sourceCellClear: separate step (Live 1d byname-personal-clear and byname-shared: a by-name GridAddItem leaves the item in its bag cells, and InvGridClearItemNode with self = other = the bag grid, the item's anchor cell node, nodeGrid [y][x], and undefined empties them; it runs after a success=true placement and before any owner step, because it looks the item up by fingerprint while the item is still on map 0. After a merge the same call follows StashAddToStack's true, as the hand merge logged it, Live 1c hand-merge and byname-merge. The hand grid moves emptied the bag cell with no armed row logging a clear: which routine does it there is not observed)
targetTabRule: stays in the bag (Live 1c hand-full: the owner's Ctrl + left click against the full shared tab 2 logged StashAddToStack false and GridAddItem success=false on that tab's own array and nothing after them - no GetStashMaxTabs, no other tab tried; the item stayed in its bag cells and the full tab's filled count was unchanged. The positive control was hand-personal and hand-shared in the same session. Live 1d byname-full answered the same by name, as expected by construction)
socketRoute: new: not-observed (Live 1e byname-socket not-run: no second socketable of an accepted kind was left off the tab; by hand, Live 1e hand-socket, a rune and a gem from the bag's Socket sub-tab took ValidateItem with self = other = the bag grid, StashAddToStack with the same self and other, a one-row array, 9, 2, the item, 1, 8, answering false, GridAddItem on that array with 0 and undefined answering success=true at x 0, y 0, ValidateItem with self the stash grid and other the bag grid, and the owner step 0 to 9 as on a shared tab; the one-row array was different for each item and was matched to no readable path, Controller_obj.stashSocketItemSlot reading no fingerprint while the tab held items; not replayed by name); merge: byname (Live 1f byname-socket-merge and byname-socket-nonstack, Live 1g byname-socket-merge with its close, reopen and save checks; the shape is socketMergeRoute's below: the array is the item's own StashSocketGrid node's one-cell nodeGrid, the one-row array Live 1e's hand moves logged; by hand, Live 1e, an orb merged the same way). The game refuses jewels (base ids 109 and 110) and Incarnation Gems (136) for this tab after ValidateItem and before any armed placement routine, with nothing in the logged answer showing it (Live 1e H1 and H1b), so no by-name shape carries that refusal. The tab saves as stash.hss socket_tab, and menulayout lists one one-cell StashSocketGrid node per item
newMaterialRoute: byname (Live 1e byname-material-new, replaying hand-material-new; saved-stash-has-keys and saved-bag-lacks-keys pass): StashAddToStack, self = other = the bag grid (its Materials sub-tab on show), Controller_obj.stashMaterialTab, 9, 2, the item, 1, 0, answering false (no stack of that identity on the tab); GridAddItem with the same self, other and array, the item, 0, undefined, answering success=true with the landing x, y; ValidateItem with self the stash grid and other the bag grid; the source clear, InvGridClearItemNode with self = other = the bag grid, the item's anchor cell node and undefined, because the placement left the item in its bag cell; then the owner step 0 to 9, ChangeItemOwner with self the stash grid, other the bag grid, 0, 9 and the key as text, answering undefined. The whole stack (908) landed in one cell; the key then answered undefined on map 0 and an item struct on map 9, as the hand-placed unit did, and saved in stash.hss under material_tab. The hand move's first ValidateItem (self = other = the bag grid) was not in the replay
wholeStackMerge: byname (Live 1e byname-merge-whole): StashAddToStack, self = other = the bag grid (its Materials sub-tab on show), Controller_obj.stashMaterialTab, 9, 2, the item, its whole count (15), 0, answered true, and the tab's sum for that base id rose from 1 to 16, by exactly the count; then InvGridClearItemNode with self = other = the bag grid, the item's anchor cell node and undefined emptied the bag cell. Measured on the Materials tab only; every hand merge logged a fifth argument of 1 (Live 1c hand-merge, Live 1e's orb on the Socketable tab)
buttonRoute: poll (Live 1g sort-click-control, node-idle, node-press-poll-unbound and node-press-negative, all pass, the game running throughout): the Move all node is created with its activation left undefined (UiCreateNode's fourth argument undefined, UiSetActivationFunc never called, no script hooked for it), and the plugin's frame tick reads a left press and the mouse's GUI point by name and counts a press inside the node's box, read at that frame, as the button press. On a click on the unbound node no armed row logged a call with the node as self and no dialog appeared (that nothing of the game's runs is Static reading 3, not measured), and the poll counted it once; a click on the panel background beside it counted only as a press outside both buttons. The activation route is dropped: Live 1f's click on a node bound to UiSetFloatingToFalse reached the plugin's detour through the node's user event 15 (self the node, other the stash window, one argument, the node's activationArgs) and then ended the game with "bool argument is unset" inside that script (Live 1f node-press-activation, fail (crash)). The node survived a bag and a stash tab switch (Live 1g node-survives-tab-switch), so it needs no recreate on a tab switch
buttonOwner: UI_Stash_obj (Live 1g node-gone-on-close and reopen-no-stale-node, pass): the node is created with self = other = the UI_Stash_obj window on show, UiRemoveNode with that same self removes it (Live 1g node-removed-by-name), and the stash's own close destroys a node still listed, so a reopen finds none
sortActivation: InventorySortTab self=instance (Live 1f and Live 1g sort-activation, pass): the bag's Sort button is the UI_Button_Small_obj whose uiNodeCallstack is InventorySort, text Sort Tab (not Sort), activationArgs [1], its activation InventorySortTab bound with the Sort node itself as self; the button is found by that call-stack name, never by its text (the stash side's own sort button is StashSort, also Sort Tab)
socketMergeRoute: byname (orb and gem; every identity with a node on the tab merges) (Live 1f byname-socket-merge and byname-socket-nonstack, Live 1g byname-socket-merge, merge-close, reopen-shows, saved-stash-has-keys and saved-bag-lacks-keys): StashAddToStack with self = other = the bag grid (its Socket sub-tab on show), the nodeGrid of the StashSocketGrid node holding the item's identity (one cell; the tab is read as the set of those nodes, each cell's key resolved on map 9), 9, 2, the item, its count, 8, answering true, the node's o rising by exactly the count (orb, base id 118: 81 to 82 in both sessions); then InvGridClearItemNode with self = other = the bag grid, the item's anchor cell node and undefined. The gem (base id 38) merged the same way and gained o=2, so it is stackable (Live 1e's missing o was a count of 1) and there is no non-stackable case on this tab. The merged unit's key reached no saved file and the orb stayed under stash.hss socket_tab. Measured with a count of 1 by the probe, and with a stack of 3 through the shipped adapter in Live 3 (socketWholeStackMerge below); a new identity on this tab stays unmeasured by name (socketRoute new:)
stackCapRule: measured 999 on the Materials tab (Live 3 material-overflow and material-partial, pass): with the sixth argument 0, a stack takes a unit only while its count plus the unit's stays at or below 999. A unit of 125 beside its kind's one stack of 875 (1000 together) was placed as a new stack of 125 in a free cell, the 875 left unchanged; a unit of 129 then passed over the 875 (1004 together) and merged into the stack of 125, which read 254; no stack read above 999 and neither run skipped anything. The same session's two bag materials beside full stacks of 999 each started a new stack too (material-new's setup). The mod's per-stack route (§ Ship design) therefore matches the game's merge on the Materials tab. Not measured: the cap of 999999 with flag 8 (the Socketable tab's stacks read far below 999, so it stays § Static reading 4's reading), a merge that lands exactly on 999, and which of two stacks with room takes the unit (the reading says the first in array order)
socketWholeStackMerge: on (Live 3 socket-whole and socket-single, pass): through the shipped adapter on the research build, a copy of 3 of the orb (base id 118) merged into its kind's one StashSocketGrid node, `-> stack`, and the node's o rose by exactly 3 (92 to 95) on a clean re-run with only kinds the tab lacks left in the bag; the first run read +8 (84 to 92) because other orbs of that kind in the bag merged in the same run (inferred; the bag was not read before the press). A single gem (base id 8) raised its node by exactly 1. No merged unit's key reached a saved file. The flag stays on as #131 shipped it; a kind with no node still stays in the bag (socket-new-stays, 20 skipped)
buttonPlacement: beside Sort, at ButtonOrigin's origin (Live 3 button-placed and button-press, pass): Sort's bbox read 2290.0,1262.0,2482.0,1328.0 and the Move all node's 2076.0,1271.0,2282.0,1319.0 (its x, y 2178.0, 1295.0, within one GUI unit of its bbox centre): its right edge exactly 8 left of Sort's left edge and both vertical centres at 1295, the mod's own `placed beside Sort` line naming the same box. The absolute box the procedure expected from Live 1f and 1g's numbers did not apply at this session's GUI scale (Sort 192 wide against 182.4, the node 206 against 195.7); the relation held, which is why the extents are read from the node rather than fixed. A click at the node's centre started one run (`in_node=1 taken=1`; `presses=4` counts the setup's three Ctrl + clicks, inferred). Not observed: a node first made off target and remade, and the off-target line
buttonLook: size and sprite taken, drawn look not Sort's (Live 4 button-placed, button-state and button-look pass on their numbers; the step 3 screenshot fails): with `sprite_index`, `image_xscale` and `image_yscale` copied from the Sort node, the session's first node, made with Sort's own extents at x, y 2090, 1262, read the bbox 2090.0,1262.0,2282.0,1328.0 (192x66) beside Sort's 2290.0,1262.0,2482.0,1328.0 (192x66), right edge exactly 8 left of Sort's, centres both at 1295, with one make; `menulayout` and `probe sort` read the node's sprite as `Inventory_Tab_Button_Solid_spr`, the same as Sort's, and the state line `button_look=sort button_size=192.0x66.0`; the reopen's node, made from the measured extents 0,0,192,66, read the same box. So the copied sprite and scale persisted on the node through two ensure steps, a tab switch and a reopen, and its bbox followed them. That is not the look: those reads are of the member the mod wrote. In the screenshot the node's box is drawn empty, its `Move all` label is not inside it (a clipped end of the text shows at about the box's top-left corner, under the frame above it), and the owner reads the box as dark rather than Sort's red look (the backpack's Sort Tab in the same frame is drawn the same dark way; the red Sort Tab there is the stash's own `StashSort`). Not a pass for what the player sees. Not established: where the game draws a node's `text` (centred on its x, y is the inference, since that point moved from the box's centre to its corner with the sprite), how the Sort node gets its label centred, and what the owner's red look is. The button still works (the click moved 2 of 2). Superseded for the drawn look by `buttonLabel`: with Live 5's 13 label members copied as well, Live 6 read the node's label centred like Sort's on the shipped path
buttonTarget: relation (Live 5 merc-bag-read, sort-same-both and merc-label pass; merc-stash-listed fail, which is the finding): the button the owner points at is the game's own Mercenary button, a `UI_Button_Open_Mercenary_obj` (SDK object 5004) with `uiNodeCallstack` `InventoryMercenary`, `text` `Mercenary` and sprite `Inventory_Tab_Button_Solid_spr` (Sort's), its x, y its top-left. With the bag open on its own (the `C` key) its box read 2094.0,1262.0,2286.0,1328.0 (192x66) beside InventorySort's 2290.0,1262.0,2482.0,1328.0 (192x66); with the stash open `menulayout UI_Button_Open_Mercenary_obj` listed none, and InventorySort's box read 2290.0,1262.0,2482.0,1328.0 again; after the stash's close neither was listed. As fractions of InventorySort's width and height, the Mercenary box's left edge is -196/192 (about -1.0208) of Sort's width from Sort's left edge, its top edge 0 of Sort's height from Sort's top, its width 1 and its height 1 - its right edge 4 GUI units, 1/48 of Sort's width, short of Sort's left edge, where the old rule put the node's 8 short (2090.0,1262.0,2282.0,1328.0). The label check read the Mercenary button's own label centred (offset 0.5, 2.0; Sort's -0.5, -1.0). So the node's target is InventorySort's box moved and sized by those fractions, read by name at each ensure step, never a GUI-unit constant; a Mercenary node listed with the stash open (merc-route live) would be read instead, and the old rule is the fallback only. Not measured: the relation at another GUI scale (the fractions are expected to follow it, as both nodes' sizes followed the 1.0526 scale between Live 1f/1g and Live 3), and whether the game ever lists the Mercenary node with the stash open. Live 6 confirmed it on the shipped path (merc-read, button-placed, button-state and reopen-placed pass): the bag-alone Mercenary box read 2094.0,1262.0,2286.0,1328.0 again beside InventorySort's 2290.0,1262.0,2482.0,1328.0, and the mod's node, worked out by the relation (`button_ref=relation`), read exactly that box on the session's first node and on the node made after a close and reopen
buttonLabel: members (Live 5 label-trial-changed pass, label-baseline pass, label-trial-missing fail): the node lacks no member InventorySort has (`lookcopy missing` wrote nothing, one excluded). `lookcopy changed` from the stash-open InventorySort (id 262329) wrote its 13 differing writable members onto the node, each read back equal - `dropShadow` false (the node's true), `textFont` `__newfont2` (the node's `__newfont6`), `createX` 2290 (2090), `drawXOffset` 48 (0), `drawYOffset` 9 (-7), `navBboxX` 2290 (1988), `navBboxY` 1262 (1238), `navBboxWidth` 192 (206), `navBboxHeight` 66 (48), and `naviDown`, `naviDownPrev`, `naviRight`, `naviRightPrev` false (true) - and the label check then read the node's label centred like Sort's (532 label pixels against Sort's 548, offset 1.0, -0.5, label box 2136,1286,2238,1303) where it had read it at the box's top-left corner (offset -69, -29), the node's bbox unchanged at 2090.0,1262.0,2282.0,1328.0. The 13 were written together, so the trial does not separate which of them places the label. What the numbers do show: the label's left edge sits at the node's x plus `drawXOffset` (Sort 2290 + 48 = 2338, its label box's left edge; the node after the copy 2090 + 48 = 2138 against 2136), so `drawXOffset` and `drawYOffset` place the label relative to the node's own x, y; `createX` and `navBboxX` were written as Sort's absolute 2290 and the label was still drawn inside the node's box 200 units left of that, so neither places it across (inferred from the one trial); whether `navBboxY`/`navBboxHeight` place it vertically is not separated, since after the copy Sort's and the node's are level. The members holding an absolute GUI position are `createX`, `navBboxX` and `navBboxY` (on Sort, its own box's corner; on the node, the corner of its box as first made under its own centred sprite, never updated when it took Sort's sprite). The mod copies all 13 as read, exactly as the trial wrote them, after `sprite_index` (as read) and `image_xscale`/`image_yscale` (scaled to the target, as Live 4 proved): it is the only set with a positive result, `createX` and `navBboxX` written raw left the label centred in that one trial, `navBboxWidth`/`navBboxHeight` raw equal scaled at the Sort-sized target Live 5's relation gives, and a copy equal to Sort's member for member makes Live 6's `look-members` a plain all-equal check. The build before this one copied a subset of 8 with `navBboxX`/`navBboxY` shifted by the node's offset, a guess that no session measured and that a failed `label-centred` could not have separated. Not established: what the four `navi*` flags and the `navBbox*` members do beyond the label (gamepad navigation, for example, which no session read; Live 5's click and close after the same writes behaved as before), and whether the game recomputes any of them in longer play. Live 6 confirmed the set on the shipped path (button-state, label-centred, look-members and reopen-placed pass): with all 16 members copied (`button_look_same=16/16`) the label check read the node's label centred like Sort's (532 label pixels, offset 1.0, -0.5, against Sort's 548 and -0.5, -1.0), on the first node and on the reopened one, and `probe diff` of Sort and the node named none of the 16. `textFont` printed `ref font __newfont2 (asset)` on both, an asset reference rather than a string, and the `navi*` flags read `false` on both some seconds after the copy. `label-trial-control` did not run, since `label-centred` passed, so no cause needed separating

## Ship design

What the player build does with the lines above (ForgePact 2.1.0, Mods tab →
Quality of Life → **Move all into the stash**, off by default). The decisions
live in `plugin/include/ForgePact/StashMoveAllMod.hpp`, which names no runtime
interface and is run whole by `tests/stash_move_all_harness.cpp`; the adapter
in `plugin/ModuleMain.cpp` (the `stashmoveall, stashmove` block, and the
`stashmoveall button` block for the in-game button) reads the game and calls
it. `tests/test_stash_move_all_contract.py` pins the adapter.

**The control.** The switch is `stashmoveall 1|0` (the panel sends it). While it
is on, the frame callback reads one key, F4; the modifiers, the foreground
window and the stash window are asked only while the key is down, and a press
starts one run only with the game's window in front, a `UI_Stash_obj` listed and
no Alt, Ctrl or Shift held (Alt+F4 closes the game, and a run started as it
closes would move items the stash's own close never saves). While it is off,
the frame path reads nothing. `stashmoveall run` runs the same thing without
the key, `stashmove <fingerprint>` moves one item of the bag tab on show through
the same per-item routine, and bare `stashmoveall` prints
`stashmoveall: state=<on|off> key=F4` with the button's counts (below) and the
usage. After a loss (below) the
state line reads `stashmoveall: state=off-for-this-session <the button's
counts> reason=<reason>` instead (the counts kept, so a click that ended in a
loss reads from the same line; the reason, free text, last), and `stashmoveall 1` answers `stashmoveall: off for this session -
<reason>; ...` and stays off. Every switch, and every loss, prints the state
line; the panel reads the last one in `out.txt` (`/api/state`'s
`stash_move_all_session`) and shows `off (this session)` beside the switch while
the game runs.

**The button** (`buttonRoute: poll`, `buttonOwner: UI_Stash_obj`,
`sortActivation`, all from Live 1f and 1g). While the switch is on, the frame
tick's ensure step, at most every tenth frame, asks the core whether the node
should exist: the switch on, a `UI_Stash_obj` listed, and the bag's Sort button
listed and visible - the `UI_Button_Small_obj` whose `uiNodeCallstack` reads
`InventorySort`, found by that name and never by its text (`Sort Tab`). To make
it, `UiCreateNode` is called by name with self and other the stash window and
five arguments: x and y (the node's origin, below), the object
`UI_Button_Small_obj` by `asset_get_index`, the
activation **undefined**, and the call-stack name `ForgePactMoveAll`; then the
node's own `text` is set to `Move all` and read back, on the instance the mod
made (a node whose label does not read back is taken away again); with Sort's
look (below) the button's only writes, both on that instance. **Its place (ForgePact #131):** x and y are the node's
origin, which for the mod's node (`UI_Button_Small_obj` drawn with
`Menu_Button_Chat_spr`) is its bbox centre while the Sort node's (the same
object, drawn with `Inventory_Tab_Button_Solid_spr`) is its top-left, so the
origin follows the sprite, not the object (§ Static reading 4). The first release passed Sort's x
less Sort's width less 8, and Sort's y, as if the new node's origin were its
top-left, so the button sat centred on the point meant for its top-left corner.
The core's `ButtonOrigin` now gives the origin at which the node's bbox right
edge is 8 GUI units left of Sort's bbox left edge and its vertical centre is
Sort's, from Sort's bbox and the node's own extents about its origin (left,
up, right, down), read by name from the node and kept for the session. The
session's first node is made with Sort's own extents about Sort's x, y (the
node wears Sort's look, below; a box of Sort's own size about its centre when
Sort's x, y do not read). The place is not checked in the frame the node is made: a
box read then is not known to be the settled one (Live 1f: the node read
`visible=0` in that frame and 1 a frame later), and a GUI scale applied after
`UiCreateNode` returns would leave a stale box that could look on target (the
review of #131 round 0). So on each later ensure step the core's `ButtonCheck`
is handed the node's `visible`, x, y and bbox and Sort's bbox, and decides
only once the node is visible and both boxes read the same on two steps in a
row; then it measures the extents and, when the box is not within 1 GUI unit
of the target (`ButtonOnTarget`), the node is removed with `UiRemoveNode` and
made again once at the origin those extents give, and checked the same way -
at most two `UiCreateNode` calls per Create step. Every node made is checked,
so each stash open is. On target is said once a session, `stashmoveall:
button - placed beside Sort, box <l,t,r,b>`; one still off is kept and said
once, `stashmoveall: button - placed <dx>,<dy> off beside Sort; F4 still
works`; a box not settled six ensure steps after the make is said unchecked
once, and a node whose x, y did not read is said unchecked once. Each of those
lines has its own said-once flag, so an early unchecked line never hides a
later node that settles off target; none turns the mod off. The bare
`stashmoveall` state line carries what the check read (`button_place=`,
`button_box=`, `button_extents=`, `button_makes=`, `button_step=`,
`button_look=`, `button_size=`, and since the Mercenary target `button_ref=`
and `button_look_same=`), so `button-placed`'s `menulayout` rows can be
compared with the mod's own reading. With the measured extents of the node
in its own sprite (96.9, 22.8, 98.8, 22.8) and the Live 1g Sort box that is
the origin 2196.7, 1230.25 and the bbox 2099.8, 1207.45, 2295.5, 1253.05 at a
2560x1440 GUI.

**Its target: the Mercenary button's box (the owner, 2026-09-30; § Decision
`buttonTarget`).** The box above was the old rule. The owner asked for the
place where the game draws its own `Mercenary` button when the bag is open
without the stash, "use its coordinates". Live 5 found that button, a
`UI_Button_Open_Mercenary_obj`, Sort's size and level with it, its right edge
4 GUI units left of Sort's, and not listed at all while the stash is open. So
on each Create and ensure step the core's `ButtonTarget` works the target out
from InventorySort's bbox, read by name, by Live 5's relation as fractions of
Sort's width and height (`kMercLeftOfSort` -196/192, `kMercTopOfSort` 0,
`kMercWidthOfSort` 1, `kMercHeightOfSort` 1): at Live 5's GUI the box
2094, 1262, 2286, 1328. Fractions, not GUI units, because the GUI scale moves
both boxes together (1.0526 between Live 1f/1g and Live 3). The origin is the
core's `TargetOrigin` (the node's right edge on the target's, its vertical
centre the target's), the check the same `ButtonCheck` against that target
(`OnTarget` for the place, `TargetSized` for the size), and the lines say
`placed in the Mercenary button's place, box <l,t,r,b>` and `placed <dx>,<dy>
off the Mercenary button's place`. When the target cannot be worked out (a
Sort box with no width or height to scale by; under the live route, a
Mercenary box that did not read) the old rule's box stands in and
`stashmoveall: button - the Mercenary button's place could not be worked out,
so it sits beside Sort by the old rule; F4 still works` is said once. The
state line gains `button_ref=<none|mercenary|relation|sort>` (this build:
`relation`), and `button_size=` is judged against the target's size. A target
of another size than Sort's would scale the copied sprite scale per axis by
target over Sort (`ButtonScale`); at Live 5's relation it is Sort's size, so
the scale is 1.

**Its look (owner scope, 2026-09-30, § Static reading 5):** after the label,
the node is given the Sort Tab button's own look: `sprite_index`,
`image_xscale` and `image_yscale` are read off the Sort node by name at that
moment, written onto the mod's own node as read, and read back off both - no
sprite named, looked up or sized by the mod, no routine called for it. The
core is told whether the look took (`sort`), did not (`differs`) or could not
be read (`unread`), and the node's look is read again each ensure step until
the node is judged, so the look judged is the one on the settled read. Wearing
Sort's sprite the node's origin is its top-left like Sort's, so the first node
of a session is made with Sort's own extents and lands on target at once (at
Live 3's GUI the box 2090, 1262, 2282, 1328). On the settled read that judged
its place, a kept node is judged for its size too: Sort-sized when its width
and height are each within 1 GUI unit of Sort's (`ButtonSortSized`). A node
not Sort-sized is kept and said once a session, `stashmoveall: button - its
size <w>x<h> is not the Sort button's <w>x<h>, so it is kept as it is; F4
still works`; a look that did not take is kept and said once, `stashmoveall:
button - it did not take the Sort button's look (<member> differs; <n>/<m>
members the same), so it is kept with its own; F4 still works`, and one that
could not be read likewise on its own line, naming the member that did not
read.
Neither is a remake, and neither turns the mod off: a node in its own look
still works, and its place still follows its own measured extents.
**What Live 4 showed (§ Live 4 results, § Decision `buttonLook`):** the copy
held and the box is Sort's size and place, but the drawn button is not yet
Sort's look: its `Move all` label is not drawn inside the box (a clipped end
of it shows at the box's top-left corner) and the owner reads the box as dark
rather than Sort's red. `button_look=sort` reads back the member the mod
wrote, so it cannot see either; the look and the label are still open for
#131.
**The label (§ Decision `buttonLabel`, from Live 5):** a trial copy of Sort's
13 differing members centred the node's label like Sort's, and the numbers
show the label placed by `drawXOffset`/`drawYOffset` from the node's own x, y.
The look list is now `sprite_index`, `image_xscale`, `image_yscale` and the
13 members that trial wrote - `textFont`, `dropShadow`, `createX`,
`drawXOffset`, `drawYOffset`, `navBboxX`, `navBboxY`, `navBboxWidth`,
`navBboxHeight`, `naviDown`, `naviDownPrev`, `naviRight`, `naviRightPrev` -
16 entries, each read off InventorySort by name at that moment and written
onto the node as read, as Live 5 wrote them; only `image_xscale` and
`image_yscale` are scaled by the target over Sort, as Live 4 proved (1 at
Live 5's Sort-sized target). Nothing is shifted: in Live 5 `navBboxX` and
`createX` held Sort's absolute 2290 and the label was still drawn inside the
node's own box, so writing them raw left the label centred in that trial. **The
copy never stops on a member's kind** (the review of the build before this
one, which returned `unread` on the first member of a kind it did not accept,
before writing it and every label member after it): each member is written as
read whatever its kind - a number, a bool, a string, an asset reference - a
scale only when it reads as a number, then read back and compared by kind
(numbers and bools by value, strings by text, an asset by its index). A
member that is undefined, of any other kind, or whose read or write throws
costs only its own entry. The verdict is decided after the whole list, in the
core (`LookStep`, `LookCompare`, `StashMoveLookTally`): `sort` when every
member read the same, `differs` when one did not, `unread` when none differs
and one could not be read or compared. The state line gains
`button_look_same=<equal>/<listed>` (`16/16` when the copy took, `none`
before any node), and the look line names the first member that did not read
the same, with the count. Live 6 confirmed this set on the shipped path: the
node on the Mercenary box, `button_look_same=16/16`, and its label centred
like Sort's, on the first node and after a close and reopen (§ Live 6
results); the owner reads Sort's look as the backpack's dark InventorySort
unless they say otherwise. What the `navi*` flags and the `navBbox*` members
do beyond the label (gamepad navigation, for example) is not established;
Live 5's and Live 6's clicks and closes after the same writes behaved as
before.
No `UiSetActivationFunc`, and no script hooked for it: a node
with no activation runs nothing of the game's when clicked (Static reading 3;
Live 1g's click on one showed only that no armed routine logged a call with it
as self and no dialog appeared), while Live 1f's click on a node bound to a game
script ran that script with the node as self and ended the game. The node is
identified as the mod's own by that call-stack name on a listed instance, not
by its id alone. It is removed with `UiRemoveNode`, self and other the window it
was made under, whenever the core says it should not exist, on
`stashmoveall 0`, and on a loss; `instance_destroy` on the mod's own node only
when that window is gone (the stash's own close destroys a node still listed,
Live 1g `node-gone-on-close`, so the mod then just lets go of it). A
`UiRemoveNode` that leaves the node listed is said once and tried again, never
followed by a destroy that would leave the window's list naming a gone node. A
tab switch keeps it (Live 1g `node-survives-tab-switch`).

The press is the frame poll: each frame the node exists,
`mouse_check_button_pressed(mb_left)` by name, and on a press
`device_mouse_x_to_gui(0)` and `device_mouse_y_to_gui(0)` against the node's
`bbox_left`, `bbox_top`, `bbox_right` and `bbox_bottom` read by name at that
frame (the core's `PressInNode`: inclusive sides, a side that did not read is
never a press). A press inside is handed to the core and nothing more happens in
the poll; the frame tick then takes it under F4's own guard (the game in front,
the stash listed, no modifier held), and a key edge and a press in the same
frame start one run between them. A node that cannot be made (the Sort row's
bbox not read, `UiCreateNode` refusing, the label not taking) is
reported once, `stashmoveall: button - <reason>; F4 still works`, is not tried
again until the stash is opened again or the switch turned on again, and never
turns the mod off. A stash open for three ensure steps with no visible Sort
node to sit beside is said once a session (`stashmoveall: button - not shown:
no visible Sort button ...`), so a button that never shows - a game patch
renaming `InventorySort`, say - is not silence.

A click that moved nothing is not silence either (the Phase C review: the
player build carries none of the research probe's counters). The core counts,
for the session, every left press the poll read while the mod held a node and
where it went, and the state line prints the counts after the key:
`stashmoveall: state=on key=F4 button=<held|none> presses=<n> in_node=<n>
outside=<n> unread=<n> errors=<n> taken=<n> dropped=<n>
last_drop=<none|off|fg|stash|modifier>`. `outside` is a press whose point and
box read but did not meet, `unread` one where the mouse point or a bbox side
did not read or the box was inside out, `errors` a poll that threw, `taken` a
press that started a run and `dropped` one the guard refused, with the last
reason. So after a click, bare `stashmoveall` names which of these happened:
poll-blind (`button=held presses=0`), a bbox miss (`presses` rose, `in_node`
did not), a guard drop (`dropped` rose), or a run (`taken` rose, beside its
`stashmoveall: moved ...` line). The state word stays first, so the panel's
read of it is unchanged.

**What one run stands on**, found by name at the point of use: `UI_Stash_obj`
(its `tabSelected` is the bag view on show, its `stashTabSelected` the stash tab
on show), the bag's grid node and the stash's grid node (the two
`UI_Inventory_Grid_obj` instances whose `uiNodeCallstack` reads `InventoryGrid`
and `StashGrid`), and the stash map through the game's own map lookup. The bag's
cells are its grid node's `nodeGrid`, read `[y][x]`; each item is identified on
map 0 by the lookup of § 9.3 (self and other the bag grid).

**Sources and destinations.** A stash page (personal tab 0, shared tabs 1 to 19)
takes items from the bag page on show (`tabSelected` 0 to 4). The Materials tab
takes class 14 from a bag page or from the bag's Materials view (`tabSelected`
-4, the source `stackMoveRoute`, `newMaterialRoute` and `wholeStackMerge` were
measured from). A stash page from a bag sub-tab is refused (`unsupported bag tab
-4 for stash tab <n>`). The Socketable tab takes class 15 from the bag's Socket
view only (`tabSelected` -2, the view `socketMergeRoute` was measured from):
a bag page or the Materials view feeding it is refused (`unsupported bag tab
<t> for stash tab -2`), and the Socket view feeds no other tab. The Unique tab
and the bag's Key, Tarot and Relic views are refused. The route rules are
fixed in the core (`StashMoveRoutes`, `kMeasuredRoutes`) from the lines above;
they are not settings.

**The plan.** Each item of the bag view on show once, row by row from the
top-left, a multi-cell item by its top-left cell, with its footprint taken from
the cells its key covers. The route is decided per stack, not per sum
(ForgePact #131): the core is handed the count of each stack of the item's
identity (class and base id) on the tab, in the array's order, and models the
game's merge (§ Static reading 4) - a stack takes the whole count only while
its count plus the item's stays at or below the cap, 999, or 999999 when the
sixth argument carries flag 8 (the Socketable tab's merge). Per item, on a
stash page: a stackable (class 12 to 15) with a stack of its identity that has
room for its whole count goes onto that stack; one whose every stack there is
too full, or that has none, goes into a free cell as a new stack; anything
else into a cell. On the Materials tab, which holds several stacks of one kind
(the owner, 2026-09-30): class 14 onto a stack of its identity with room for
it, else into a cell of the tab as a new stack (`newMaterialRoute`, whether the
kind is absent or every stack of it is full); any other class is a skip that
calls nothing (`not taken by the Materials tab`). On the Socketable tab, which
holds one stack per kind: class 15 onto the node of its identity when that
node has room for it (`socketMergeRoute`; there is no non-stackable case, the
gem merged too), whatever its count (`socketWholeStackMerge`, on since #131,
confirmed by Live 3's `socket-whole`); a full node is a skip, `its
stack on the shown tab is full`, never a second stack; and a kind with no node
there a planned skip, `a new kind stays in the bag` (`socketRoute` new: not
measured - the tab's 106 one-cell slots are fixed, and which empty one takes
which kind is neither read nor measured); any other class `not taken by the
Socketable tab`. A merge of more than one unit follows `wholeStackMerge` on a
stash page and the Materials tab and `socketWholeStackMerge` on the Socketable
tab; either flag off makes such an item a planned skip. An item is never split
between two stacks: the game's own merge takes the whole count or none.
A stackable whose stack on the tab cannot be read - a shared page's entries
answer on no map by name (`mapOwnerRule`) - is a skip, never read as "no
stack". The plan's route is not the last word: a stackable's route is decided
again at its own call (next paragraph), because an earlier item of the same run
can make the stack a later one joins.

**One item, in order.** At the point of use, before any call: the bag cell
still holds the key and the item still answers on map 0, `stashTabSelected`
still reads the planned tab, and the shown tab's own array (the stash grid
node's `nodeGrid` on a page, `Controller_obj.stashMaterialTab` on the Materials
tab) reads room for it - a free block of the item's footprint, or a stack of its
identity with room for its whole count (`StackRoom`; a full stack is no room,
since the game's merge would answer false). For a stackable the route is
decided here, from its identity's stacks re-read on that array just before the
first call and its count re-read with it (`RouteAtUse` in the core), whatever
the plan said: a stack with room is the stack routine with the whole count,
none with room the placement of a new stack, and stacks or a count that could
not be read a skip (`its stack on the shown tab could not be read`). The round-2 review of
the first player build found why: two bag items of one identity the tab lacked
were both planned into cells, the first made the stack, and the second's
`StashAddToStack` found it and merged one unit while its bag cell stayed - a
duplicate. No room, or a read that could not be made, is `skipped: no room on the
shown tab` (or `the shown tab's room could not be read`) with nothing called, so
the item stays in the bag (the owner's no-overflow rule). Then the calls, each
through its SDK constant, resolved by name and dispatched with self and other
apart:

- into a cell on a stash page: `ValidateItem` (self and other the bag grid, the
  item), `StashAddToStack` (the same self and other, the shown tab's array,
  0 and 13 on the personal tab or 9 and 2 on a shared tab, the item, its whole
  count for a stackable - the value the measured merge passes, so a merge the
  game makes here takes the whole item, never one unit of it (#131) - or 1
  for anything else, 0), expected to answer false; a true answer is decided
  as a merge (the core's `AsMerge`): the bag cell cleared only after the
  identity's sum rose by exactly the count, and moved only on that sum. Then
  `GridAddItem` (the same self and other and
  array, the item, 0, undefined). On `success=true` and the key read at the
  answer's cell: `ValidateItem` with self the stash grid and other the bag grid,
  the source clear (`InvGridClearItemNode`, self and other the bag grid, the
  item's anchor cell, undefined) while the key is still in its bag cell, and on
  a shared page only, once the bag cell reads empty, the owner step
  (`ChangeItemOwner`, self the stash grid, other the bag grid, 0, 9, the key as
  text);
- into a cell of the Materials tab: the same without the first `ValidateItem`
  (as `newMaterialRoute` replayed it), the array
  `Controller_obj.stashMaterialTab`, 9 and 2, and the owner step 0 to 9;
- onto a stack: `ValidateItem` first on a stash page only, then
  `StashAddToStack` with the shown tab's array, the tab kind's two numbers, the
  item and its whole count, 0; the bag cell is cleared only after the shown tab's
  sum for that identity rose by exactly that count;
- onto a socketable's stack on the Socketable tab (`socketMergeRoute`): the tab
  is read as what it is, the set of `UI_Inventory_Grid_obj` instances whose
  `uiNodeCallstack` reads `StashSocketGrid`, one item each, every cell's key
  resolved on map 9 (`Controller_obj.stashSocketItemSlot` is not the container,
  Live 1e); every node is read, and one that does not read makes the sum
  unreadable. `StashAddToStack` with self and other the bag grid, the one-cell
  `nodeGrid` of the node holding the item's identity, 9, 2, the item, its whole
  count, 8 - no `ValidateItem` first, as none ran in the measured merge - and
  the bag cell is cleared only after that node's count rose by exactly the
  count, re-read on the same node.

After the calls the adapter re-reads `stashTabSelected`, the shown tab's own
array (the key at the answer's cell for a placement, read `[y][x]` on a page and
either order on the Materials tab, whose axis order is only a static reading;
the identity's sum for a merge, on the Socketable tab that node's own
`nodeGrid`) and the bag cell, and nothing else: the other
stash tabs have no container readable by name (RUNTIME_DATA_MODELS § 17), so
no-spill rests on the route (each routine is handed only the shown tab's array,
after its room was read) and on Live 2's `case-full` save comparison, as the
owner accepted on 2026-09-28.

**Outcomes and lines.** Moved only when the tab on show is unchanged, the bag
cell no longer holds the key, and the key is at the answer's cell (or the sum
rose by the count), and, where the route ends with the owner step (a shared page,
a new Materials identity), the owner step was dispatched and the key then
answers undefined on map 0, the signature of a step that took (Live 1d
`byname-shared-owner`, Live 1e `byname-material-new`). The second
`ValidateItem`'s answer and the owner step's answer go into the report the core
decides on, so a step that ran and did nothing is not read as success. An owner
step that did not take after the bag cell was cleared is a loss; the item is
left where it was placed, since taking it back out of the tab would leave it in
no grid (there is no by-name route back into the bag cell), and the line says
so. An answer the game gave before changing anything
(`success=false`, `StashAddToStack` false on a merge) with both sides unchanged
is `skipped: <answer>` and the run goes on. Anything else - the tab on show
changed or unreadable, the key in both places, the stack risen by another
amount, `StashAddToStack` answering true for an item planned into a cell - is a
loss: the run stops and the mod turns off for the session. A placed item whose
bag cell did not clear is then taken back out of the shown tab (`GridRemoveItem`
with self and other the stash grid, the shown tab's array and the key, the
research's undo shape) and the owner step reversed if it ran; that undo is not
observed live. The lines: `stashmoveall: item <key> -> cell <x>,<y>|stack|skipped:
<answer>` per item, `stashmoveall: moved <n> of <m> from bag tab <t> to stash tab
<s>; skipped <k>` per run, `stashmoveall: refused - <reason>; nothing was called`
(off, no stash window, no shown tab, an unsupported tab, nothing to move),
`stashmoveall: off for this session - <reason>; turn it on again after
restarting the game`, and for the one-item verb `stashmove: moved <key> -> ...`,
`stashmove: not-taken - <answer>; the item stays in the bag` and
`stashmove: refused - ...`.

**What is not observed** in play before Live 2: every case of this build (Live 1
to Live 1e measured the routines through the research build's `craftprobe`, one
call at a time, not this adapter); a merge on a stash page with the page's own
two numbers (0 and 13 on the personal page, 9 and 2 on a shared page) - every
merge was measured on the Materials tab (by name and by hand), plus one by-hand
orb on the Socketable tab, and none on a page; the Materials tab fed from a bag
page (`newMaterialRoute`, `stackMoveRoute` and `wholeStackMerge` were all
measured with the bag's Materials view on show, `tabSelected` -4); a multi-cell
item placed by name into the Materials tab; the undo, and an owner step that
did not take; F4 itself, and the held-modifier guard; and whether a run of many
items in one frame, a second item merging into a stack the run itself made
among them, behaves as the single calls did. For the button: the shipped
adapter's node and poll (Live 1f and 1g measured the research build's probe,
which made the node and polled the press the same way), a click that starts a
run, the fail-safe line, and whether the backpack's Sort button stays listed
and visible on the bag's Materials and Socket views (the button is shown only
while it is); how the Sort button is created and what the stash window's step
does before a node's click event were not read. For the Socketable tab: a merge
of more than one unit (measured with a count of 1, orb and gem), a merge
through this adapter, and a new kind placed by name (`socketRoute` new).

**What Live 2 then observed** (§ Live 2 results, its third run): the shipped
adapter's run into a tab with room, a Shared tab that took every item handed
to it and the next that refused every one (`no room on the shown tab`, the
tabs around it and every other saved stash container unchanged), the
Materials tab refusing non-stackables, new material kinds and a second item
of one kind reported by the mod as merged into the stack the run itself had
just made (it left the bag and was not listed on its own; that stack's count
was not read, so a merge is not told apart from a loss); the one-item verb;
F4 with the switch on, which started five runs, and, with that as its
control, F4 producing nothing while off; the switch-off baseline for the verb
and the button; the shipped button made beside Sort, a click on it starting
one run, and switching off removing it. **Still not observed after Live 2:**
the held-modifier guard; the Socketable tab's merge through this adapter (no
bag socketable's kind was on the tab); a merge on a stash page or a stackable
on a shared page; the undo, an owner step that did not take, and any loss;
the button's fail-safe lines; whether Sort stays visible on the bag's
Materials and Socket views; and a multi-cell item placed into the Materials
tab. One gap was seen, in the first run: switched on again with the stash
still open, the button came back only after a stash tab click; the cause was
not established, and the third run clicked a tab after switching on rather
than test it again.

**ForgePact #131, observed in Live 3** (2026-09-30, § Live 3 results, 17 of
18 checks passed, one not run): the stack cap of 999 on the Materials tab - a
stackable placed as a new stack beside a full one of its kind, and a later
one merged into that second stack while the full one stayed unchanged
(`material-overflow`, `material-partial`); the Socketable tab's merge of a
stack of 3 (`socket-whole`, `socketWholeStackMerge`); and the button at its
new origin, its right edge 8 left of Sort's and level with it
(`button-placed`). **Still not observed:** a new material kind placed by the
#131 adapter (`material-new` not run: no bag material lacked a stack of its
kind; Live 2 saw one placed by the #68 adapter), the cap of 999999 with flag
8 (a static reading, § Static reading 4), a true answer on the placement
route decided as a merge, and the button remade once when off target.
