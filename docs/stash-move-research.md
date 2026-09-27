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
  tab: Live 1's check `grid-move-tab-full` is the measurement.
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
  with `ReportClient` on a failed check. It calls no map-owner routine. The
  sixth argument is tested for a number and otherwise replaced by a large
  default: **not read** what it bounds. The third (2) and fifth (1)
  arguments: **not read** beyond #14's logged values - the fifth equals the
  unit count of the logged one-unit moves.
- **`InvGridClearItemNode`** takes two arguments: the cell (node) and a
  second the processor passes as undefined. It looks the cell's item up by
  fingerprint and asks its size, and (#14's Phase 1h reading) reads two
  variables of its self, so its self must be the grid that holds the cell -
  the bag grid for a bag cell. It calls no map routine.
- **`GridAddItem`** (#14's reading, unchanged here) reads no variable of its
  self: it sizes the item, finds a fit in the grid array it is given, writes
  the node, touches no map, and answers a struct with the tab, the position,
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
string.

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
`UI_Inventory_Grid_obj` whose `gridName` is `InventoryGrid`, `<stash>` the
`UI_Stash_obj`, `<sg>` the `UI_Inventory_Grid_obj` the stash lists for the
tab on show, `<x>,<y>` the bag cell of the item being moved (the `cell=` of
its first row), `<K_J>` a non-stackable key (ends `-18`), `<K_M>` a material
key (ends `-14`) and `<o>` that material stack's count (`node bag`). Before
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

**Grid item into the stash grid tab on show, first order** - the processor's
own bag-to-stash group, self = other = the bag grid, as `StashAddToStack` was
logged. The placement, then the source clear, then the owner step only if the
map re-read below still finds `<K_J>` in map 0 and not in map 9:

```
craftprobe call GridAddItem id:<bag> other:<bag> path:id:<sg>.nodeGrid fp:<K_J> 0 undefined confirm   -> expect: a struct with success=true, tabNumber, x, y; success=false is "no room" and nothing changed
craftprobe call InvGridClearItemNode id:<bag> other:<bag> path:id:<bag>.nodeGrid.<x>.<y> undefined confirm   -> expect: true or undefined, and menulayout lists no bag cell holding K_J afterwards
craftprobe call ChangeItemOwner id:<sg> other:<bag> 0 9 <K_J> confirm   -> expect: ret=undefined (its normal answer); only when the map re-read still finds K_J in map 0
```

**Grid item, second order** - only if the first order's placement is refused
or does not appear in the stash: the destination grid's own
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

**The map-owner re-read**, after each block (the game's own lookup, with the
key given as text and the owner as a number):

```
craftprobe call GetItemFromFingerprint id:<stash> other:<stash> <K_J> 9 confirm   -> expect: an item struct when the entry is in map 9
craftprobe call GetItemFromFingerprint id:<stash> other:<stash> <K_J> 0 confirm   -> expect: undefined when the entry has left map 0
```

The same two lines with `<K_M>` follow the stack block; for a merged stack the
game's own merge deletes the merged unit's entry (#14), so `undefined` on both
maps is the expected answer there.

## Live procedure

### Live procedure 1

The procedure is the one in this workorder's context file,
`.claude/workorders/forgepact-68-move-all-context.md` § "Live procedure 1"
(kept on the owner's machine with the plan): the research build above,
positive controls first (the marker, the hook line and a rising
`CheckPlayerInteraction`, then the dispatcher control), the grid block into
the personal tab, the full-tab check, the stack block into the Materials tab,
the bag sub-tab as a source, the three hand gestures, the close and reopen,
and the saved files. No person at the keyboard. Every by-name call follows
the recording rule under § Instrument, and each check is `pass`,
`not-observed` (with what was supplied) or `not-run (instrument: ...)`.

## Results

### Live 1 results

Not run yet. One row per Live 1 check; the logged shape sits beside the
supplied shape for every by-name call.

| Check | What it reads | Logged shape | Supplied shape | Result | Verdict |
|---|---|---|---|---|---|
| dll-hash | the lease's DLL hash against the build above | - | - | | |
| marker | `craftprobe` first line | - | - | | |
| control | `craftprobe hook` line, `CheckPlayerInteraction` rising | - | - | | |
| byname-control | the dispatcher control line and its `show` entry | | | | |
| grid-move-byname | `<K_J>` in a stash grid cell and in no bag cell | | | | |
| grid-move-map | the map 9 and map 0 lookups of `<K_J>` | | | | |
| grid-move-tab-full | where a grid item goes when the tab on show is full | | | | |
| stack-move-byname | the stash sum for `<K_M>`'s kind and the bag cell | | | | |
| bag-subtab-source | whether the bag's Materials sub-tab lists cells | - | - | | |
| gesture-rightclick | rows and movement on a right-click | | - | | |
| gesture-shiftclick | rows and movement on a shift-click | | - | | |
| gesture-clickclick | rows and movement on click, move, click | | - | | |
| close-survives | the game after the stash close | - | - | | |
| reopen-shows | the moved keys in the stash after a reopen | - | - | | |
| saved-stash-has-keys | the moved keys under a stash container in the save | - | - | | |
| saved-bag-lacks-keys | the moved keys under no bag container | - | - | | |

## Decision

Each line is set from Live 1's capture: `byname` with the shape that worked,
`not-observed` with what was supplied, or `shape not reproduced`.

gridMoveRoute: pending
stackMoveRoute: pending
mapOwnerRule: pending
bagSubtabRoute: pending
gestureRoute: pending
sourceCellClear: pending
