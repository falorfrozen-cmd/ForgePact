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
Live 1c and Live 1d ran on the same kept copy, also with no rebuild: the rows
Live 1c added to the armed list (`GetItemFromFingerprint`, `GetItemMap`,
`GetStashMaxTabs`, `GridAddToStack`, `UiASplitStack`, `ItemCheckHash`,
`GetItemOwnerStr`) and every row Live 1d calls are rows of the same table.

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
revisit, not a settled dead end), and after them the same for Live 1b, Live 1c
and Live 1d (the session is named in the first column). The last column is
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
tab's array" that node's `nodeGrid` read with the tab on show.

gridMoveRoute: byname (Live 1d byname-personal, byname-personal-clear, byname-shared, byname-shared-owner, saved-stash-has-keys, saved-bag-lacks-keys, all pass). personal: GridAddItem on the shown tab's array (the stash grid's nodeGrid, personal tab on show), self = other = the bag grid, a2 0, a3 undefined, after ValidateItem (self = other = the bag grid) and StashAddToStack (the same self, other and array, 0, 13, the item, 1, 0, answering false for a non-stackable) and followed by ValidateItem with self the stash grid and other the bag grid; shared: the same on shared tab 1's array with StashAddToStack's 9, 2. GridAddItem answers a struct with the tab, x, y and success; success=true places the item at x, y on the array it was handed and nowhere else (Live 1c's hand moves logged exactly this sequence)
stackMoveRoute: byname (Live 1c byname-merge, replaying hand-merge): StashAddToStack, self = other = the bag grid (its Materials sub-tab on show), Controller_obj.stashMaterialTab, 9, 2, the item, 1, 0, answered true and the tab's sum for that base id rose by exactly one unit; then the source clear. It answers false when no stack of the same identity is on the tab (Live 1, Live 1c hand-material); a new identity then goes, by hand, through GridAddItem on the Materials tab's array and the owner step 0 to 9 (Live 1c hand-material; not replayed by name)
mapOwnerRule: personal: map 0, no owner step (Live 1c hand-personal logged none and hand-personal-map read the key on map 0 as before; Live 1d byname-personal-clear; saved in herosiege13.hss under inventory.personal_stash); shared: owner step 0 to 9 after the placement, ChangeItemOwner with self the stash grid, other the bag grid, 0, 9 and the key as text, answering undefined (Live 1c hand-shared logged it; Live 1d byname-shared-owner replayed it: the item stayed in its cells and its lookups turned into a shared-tab key's, undefined on map 0 and on map 9; saved in stash.hss under stash_tab_1). Never before a confirmed placement: alone it leaves the key on no map while it sits in the bag (Live 1c step 8, reversed). The Materials tab's new identity took the same shared step by hand (Live 1c hand-material); a merge takes none (Live 1c hand-merge). Which map holds a shared-tab entry is not established: the owner 9 lookup misses every one
bagSubtabRoute: byname (cells readable: the bag's Materials sub-tab is New_Inventory_Data_obj.inventoryMaterialGrid, an array 6 by 15 of cells carrying fingerprints, Live 1 bag-subtab-source; Live 1c: hs_bag_tab switches the bag to it by name and the bag grid node rebinds to it, while the way back to the bag's first page is the page-tab click, which logged UiAInventoryTabClick and InventoryResetTabs by hand and is not measured by name)
gestureRoute: ctrl-click (by hand: Live 1c hand-personal, hand-shared, hand-material and hand-merge, each the owner's own Ctrl + left click moving the item into the tab on show; a scripted click through hs_input never reached the grid's pick-up, Live 1b click-control, so no scripted gesture is measured)
sourceCellClear: separate step (Live 1d byname-personal-clear and byname-shared: a by-name GridAddItem leaves the item in its bag cells, and InvGridClearItemNode with self = other = the bag grid, the item's anchor cell node, nodeGrid [y][x], and undefined empties them; it runs after a success=true placement and before any owner step, because it looks the item up by fingerprint while the item is still on map 0. After a merge the same call follows StashAddToStack's true, as the hand merge logged it, Live 1c hand-merge and byname-merge. The hand grid moves emptied the bag cell with no armed row logging a clear: which routine does it there is not observed)
targetTabRule: stays in the bag (Live 1c hand-full: the owner's Ctrl + left click against the full shared tab 2 logged StashAddToStack false and GridAddItem success=false on that tab's own array and nothing after them - no GetStashMaxTabs, no other tab tried; the item stayed in its bag cells and the full tab's filled count was unchanged. The positive control was hand-personal and hand-shared in the same session. Live 1d byname-full answered the same by name, as expected by construction)
