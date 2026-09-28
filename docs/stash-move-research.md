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
  problem.
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
  from the bag into the stash is Ctrl + click.
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
  no stash owner to be in.
- **Not read**: which of `m_MoveItemToGrid`'s ten owner changes a bag-to-stash
  move takes, the processor's third `GridAddItem` argument, and where the
  processor clears the bag cell after a grid placement. Live 1b logs all
  three from the game's own gesture instead of reading them.

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
revisit, not a settled dead end). The last column is what § Static reading 2
says the reply means; none of them is a route negative.

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

Ctrl + left click, the game's own quick move, was not tried.

### Live 1b results

Not run yet. One row per Live 1b check; the logged shape sits beside the
supplied shape for every by-name call, and the rows each gesture logged are
quoted in full.

| Check | What it reads | Logged shape | Supplied shape | Result | Verdict |
|---|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the build of § Phase A' shapes | - | - | | |
| marker | `craftprobe` first line | - | - | | |
| control | `craftprobe hook` line, `CheckPlayerInteraction` rising | - | - | | |
| byname-control | the dispatcher control line and its `show` entry | | | | |
| lookup-control | the four lookup lines for `<K_S0>` and `<K_S1>`: each on exactly one map (`personal-map`, `shared-map`) | - | | | |
| click-control | a plain click on `<K_J1>`'s cell picks it up (drag rows rising, the cell empty), a second puts it back | | - | | |
| gesture-ctrl-personal | Ctrl + left click on `<K_J2>`, personal tab shown: where it lands, its map, every row logged | | - | | |
| gesture-ctrl-shared | Ctrl + left click on `<K_J3>`, shared tab 1 shown: where it lands, its map, every row logged (`ChangeItemOwner` or not) | | - | | |
| gesture-ctrl-full | Ctrl + left click on `<K_J4>`, full shared tab 2 shown: the bag, that tab or another tab, and the shown tab's cells before and after; sent only after a Ctrl + left click moved an item this session | | - | | |
| gesture-ctrl-material | Ctrl + left click on `<K_MA>` in the bag's Materials sub-tab, Materials tab shown: the tab's sum and the rows | | - | | |
| grid-array-identity | `Controller_obj.stashPersonalGrid` and the grid node's `nodeGrid` at `<K_J2>`'s landing cell | - | - | | |
| grid-move-byname | `<K_J5>` in a personal-tab cell and in no bag cell after the by-name block | | | | |
| grid-move-map | `<K_J5>`'s two lookup replies against `<K_S0>`'s | - | | | |
| grid-move-shared-byname | `<K_J6>` in a shared-tab-1 cell and in no bag cell after the by-name block | | | | |
| grid-move-shared-map | `<K_J6>`'s two lookup replies against `<K_S1>`'s | - | | | |
| tab-full-byname | the by-name placement into full shared tab 2: `success=false`, both sides unchanged (instrument check) | | | | |
| stack-move-byname | the Materials tab's sum for `<K_MU>`'s identity up by 1 and `<K_MU>`'s cell gone, run only with a stack of that identity already on the tab | | | | |
| mat-new-byname | the Materials tab gains `<K_MB>`'s identity and its count; the bag cell gone | | | | |
| close-survives | the game after the stash close | - | - | | |
| reopen-shows | every moved key where it landed after a reopen, in no bag cell | - | - | | |
| saved-stash-has-keys | each moved key under a stash container in exactly one file (`save_item_keys.py`) | - | - | | |
| saved-bag-lacks-keys | no moved key under a bag container, `inventory_order_<slot>.hss` included, after the pre-session copy's read showed each one in a bag container | - | - | | |

## Decision

Each line is set from a session's capture: `byname` with the shape that
worked, `not-observed` with what was supplied, or `shape not reproduced`.
Live 1 set every line below; Live 1b rewrites them. From Live 1b on, three
lines are written per tab kind, because the static reading puts the personal
tab and the shared tabs in different maps (§ Static reading 2):
`gridMoveRoute` names the array, self and third argument the placement takes
for each kind (`personal: <array> <self> <a2>; shared: <array> <self> <a2>`),
`mapOwnerRule` names the map and the owner step for each (`personal: map <n>
<step>; shared: map <n> <step>`), and `gestureRoute` names the input that
quick-moves (`ctrl-click`, from the hint strip and Live 1b's gestures).
`targetTabRule` (what decides the tab an item lands on, and whether it can
spill to another tab) is set only from a gesture against a full shown tab
(`grid-move-tab-full` in Live 1, `gesture-ctrl-full` in Live 1b), never from
`tab-full-byname`, and in Live 1b only when `gesture-ctrl-personal` or
`gesture-ctrl-shared` passed in the same session (the Ctrl + left click's
positive control; without it "stayed in the bag" cannot be told from a
gesture that did nothing); when that check is `not-run` it reads
`not-observed (<reason>)`, and the mod's own rule (the shown tab's room is checked before
any call, and no route that can pick another tab is used) stands either way.

gridMoveRoute: not-observed (Live 1 ran no grid block: its lookup control failed on the map 9 half, with a personal-tab key; Ctrl + left click was not tried)
stackMoveRoute: not-observed (Live 1: StashAddToStack answered false twice - self and other the bag grid, the Materials array, 9, 2, a class 14 key of base id 71, count 15 then 1, sixth 0 - with no stack of base id 71 on the tab to merge into)
mapOwnerRule: not-observed (Live 1: a personal-tab key answered undefined on map 9 with self Console_Save_obj; no shared-tab key was looked up and nothing moved)
bagSubtabRoute: byname (cells readable: the bag's Materials sub-tab is New_Inventory_Data_obj.inventoryMaterialGrid, an array 6 by 15 of cells carrying fingerprints, Live 1 bag-subtab-source)
gestureRoute: not-observed (Live 1: right click with a 120 ms hold equipped the item; Shift + left click with a 0 ms hold and click, move, click with an unrecorded hold moved nothing; Ctrl + left click was not tried)
sourceCellClear: not-observed (Live 1: never called - no placement or merge succeeded)
targetTabRule: not-observed (no single-input quick move ran)
