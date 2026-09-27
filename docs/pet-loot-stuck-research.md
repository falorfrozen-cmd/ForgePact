# Pet moves on from loot it cannot pick up (#94)

Issue #94, as the owner corrected it on 2026-09-27: with **lots** of things on
the ground to pick up (gold stacks, runes, other socketables such as rubies,
crafting materials, not only quest items), the pet gets stuck "jumping around"
one item and neither collects it nor moves on. This is the game's own
companion loot pickup (`Companion_obj`), not ForgePact's Pet Quest Collector,
which handles `Quest_Object_Parent_obj` items only. The owner's framing: a game
bug, to be fixed only if the fix is easy. It is: the mod `petunstick` (panel:
**Pet moves on from loot it cannot pick up**, Mods tab, Quality of Life, off by
default) needs no hook, no address and no call into a game script.

Everything below is labelled. **Static reading** means read from the game's
compiled code in a local decompiler and written here in our own words; no game
code is quoted (`AGENTS.md` § Legal), and objects and scripts are named by their
`hs-game-sdk` names. **Measured** means observed on the running game. A thing
that was looked for and not seen is written "not observed", never "does not
happen". **No session has measured any of this yet**: the whole mechanism below
is a static reading of the current build's compiled events (2026-09-27), and
the only live check planned is Live procedure 1 of the workorder
`forgepact-pet-loot-stuck` (see [The fix and what it does not do](#the-fix-and-what-it-does-not-do)).
The game facts are also recorded in the hub's `docs/RUNTIME_DATA_MODELS.md`
§ 10.6, the shared record every module reads.

**History.** The dev2 bug batch first read #94 as the Pet Quest Collector
circling one quest item, and changed that collector's target selection (a hold
on a failed target, a cursor over the quest-item family;
[dev2-bug-batch-research.md](dev2-bug-batch-research.md#94-the-pet-circles-one-quest-item-when-many-are-on-screen)).
The owner corrected the report on 2026-09-27: #94 is the companion's own loot
pickup, and this document is its reading. The quest-collector change stays as
an improvement of its own, without an issue number.

## Static reading

Read from `Companion_obj`'s Create, Begin Step, Step, Alarm 0 and
Collision-with-`Coin_obj` events, `Loot_Ground_obj`'s Create and Alarm 9,
`Coin_obj`'s Create and alarms, and the scripts `PickupLoot`,
`LootGroundActiveStep` and `LootGroundDeActiveStep`. The events were found
through the game's own event-registration table (pairs of an event name such
as `gml_Object_Companion_obj_Step_0` and its compiled function) and annotated
with the name-slot table described in
[pet-quest-collector-c-research.md](pet-quest-collector-c-research.md).
Object events have no script-table entry, so none of this can be hooked by
name; the mod does not need to.

### The pet's loot variables

`Companion_obj`'s Create gives it `lootList` (a ds_list), `lootTarget` (-4,
meaning none), `lootTimer` (0), `lootDistance` (1500 px), `playerRange`
(128 px), and `seekSpeed`, `baseSpeed` and `deltaSpeed` (0 at Create; Alarm 0
sets the speeds later from character data, and their values were not read),
plus `move` and `deltaTimer`. Its Begin Step sets `deltaTimer` from the game's
global delta factor and the built-in `speed` to `deltaSpeed * deltaTimer`, so
the engine's own motion moves the pet each frame by whatever `deltaSpeed` the
Step left behind.

### The scan

While `lootList` is empty and `lootTimer` has run out, the Step lists every
`Loot_Ground_obj` within `lootDistance` of the **player** (not the pet),
nearest to the player first. An item joins `lootList` only when all of these
hold: its item type is a tarot card, a socketable (except the affix-rolled
socketable bases), a crafting material, a key, or one specific consumable;
`itemActive` is true; its `itemCompanionTimer` is 0 or less; and the loot
filter shows it (`lootFilterVisible`). A second pass over the same circle adds
**every** `Coin_obj`, unfiltered. Then `lootTimer` is set to half a second of
frames.

### The target

When `lootList` is not empty and `lootTarget` does not name a live instance,
the first entry of `lootList` becomes the target. **That is the only place a
new target is chosen**: nothing else replaces a target that still exists.

### Travel and arrival

While the pet is further from its target than twice its `deltaSpeed`, it takes
`seekSpeed` as its `deltaSpeed`, turns toward the target and moves; a target
off screen (with a 256 px margin) makes the pet jump onto it.

Once within twice `deltaSpeed` it has arrived:

- **A ground item** (`Loot_Ground_obj`): the pet runs the game's `PickupLoot`
  script, with the item as `self`, for every ground item within 144 px of the
  **pet** that passes the same type, active and filter tests. An item whose
  pickup succeeds is destroyed. Whether it succeeded or not, the item is taken
  out of `lootList`, and then the list's first entry (the target's slot) is
  removed as well.
- **A coin** (`Coin_obj`): the coin is moved onto the pet, so the pet's
  collision event with the coin credits the gold (through `PickUpGoldCheck` and
  `GoldLogAdd`), takes the coin out of `lootList` and plays the sound. Then the
  list's first entry is removed.
- **Arrival does not stop the pet.** `deltaSpeed` is left at the travel speed,
  so the engine carries the pet on through the target; the next frame it is
  too far again and turns back. With a target that goes away this lasts one
  frame and is invisible. With a target that stays it is the reported
  "jumping around one item".

### `itemCompanionTimer`: the game's own "not for the pet yet"

Every `Loot_Ground_obj` gets a positive `itemCompanionTimer` at Create (from a
global whose value was not read). Its Alarm 9, every 0.3 s of game speed,
takes 0.3 s worth of frames off it while it is above 0, so the timer is in the
game's frame units. The scan skips an item while it is above 0. `Coin_obj` has
no such variable. The player's own pickup path (`Loot_Manager_obj`,
`m_LootGroundPickupCheckStep`, `playerLootTarget`) does not read it.

### The gate

The whole loot block runs only when an unnamed one-argument helper returns
true. It reads the local-player data and checks that an instance exists; it is
most likely "this pet's player is the local one and exists". What it tests is
not established. The same helper gates the coin collision event.

### Why the pet pins itself to one item

Put together: a target is replaced **only** when its instance ceases to exist;
arrival does **not** stop the pet; and a ground item whose `PickupLoot` returns
false stays on the ground while `lootList` drains, and the next scan puts it
back (it still passes every test). So one item that cannot be picked up keeps
`lootTarget` for as long as it lies there. The pet swings through it every
frame, tries `PickupLoot` on everything within 144 px every frame, and nothing
in the game moves it on. That is the reported symptom's shape. The same holds
for a coin whose collision event does not remove it.

Why `PickupLoot` can return false (static reading of the script): the item's
instance is already gone or undefined; `ItemCheckHash` rejects the item; or,
on the offline path, the script returns the result of `AddToInventory`, so a
full grid or a full stack leaves the item on the ground and returns false.

## Not established

- **Which pickup failure the owner meets** (a full grid, a stack limit, a hash
  refusal) and whether "lots of things" acts through a full inventory, a stack
  limit or something else. No session has measured it. The mod does not need
  to know: it acts on the observable "same target, within reach, not going
  away".
- **Whether coins ever stick.** The reading allows it (a coin its collision
  event does not remove); not observed.
- **`seekSpeed`'s value** (set in Alarm 0 from character data) and
  **`itemCompanionTimer`'s starting value** (a global). The mod needs neither.
- **What the gate helper tests.**
- **The kind of value `lootList` reaches us as.** The game stores a ds_list
  handle; whether this runner hands it back as a real or as `VALUE_REF` was not
  measured, which is why the mod gates the clear on `ds_exists` and never on
  the kind.
- **Nothing here is measured.** Every claim above is a static reading of the
  current build. Live procedure 1 records whether the stuck state reproduces
  with the mod off (`off-pet-stuck`) and whether the pet moves on with it on
  (`on-pet-moves-on`, which passes only when the owner's verdict is good **and**
  `held back=` or `coins released=` rose). A `not-observed` there is a finding
  about that session, not a defect, and not evidence that the stuck state does
  not happen.

## The fix and what it does not do

`petunstick 1` / `petunstick 0` (player build and research build; the panel
row `mod_pet_loot_unstick` sends `petunstick 1` only when it is on, and nothing
while off). While on, `FrameCallback` (the end of the frame, after the Step)
runs `PetLootUnstickTick()` in `plugin/ModuleMain.cpp`:

1. Resolve `Companion_obj`, `Loot_Ground_obj` and `Coin_obj` once through
   `asset_get_index`, with the `hs-game-sdk` constants as the fallback. Find
   the first `Companion_obj`; none (a menu, no pet out) forgets the watch.
2. Read the pet's `lootTarget` as a number, whatever kind carries it; below 0,
   or a target `instance_exists` denies, forgets the watch. Read pet and
   target `x`/`y` and take the distance.
3. Feed the game-independent `PetLootStuckWatch`
   (`plugin/include/ForgePact/PetLootUnstickMod.hpp`). It answers "give it up"
   once, on the frame the same target has been within `kPetLootStuckRadiusPx`
   (160 px; the game's pickup circle is 144 px) for `kPetLootStuckFrames`
   (90 frames, 1.5 s at 60 fps) in a row. No target, another target, a
   distance beyond the radius or a skipped frame restarts the count; the same
   target is not given up twice until another target, or none, has been seen.
4. Giving up writes only what the game itself reads next: on a ground item
   (by `object_index`, the ground-item family), `itemCompanionTimer` =
   `kPetLootHoldFrames` (600 of the game's frames, about 10 s), so the next
   scan skips it; on every target, the pet's `lootTarget` = -4; and the pet's
   `lootList` is emptied with `ds_list_clear`, only when it reads as a finite
   number that `ds_exists(…, ds_type_list)` confirms is a live list, so the
   game's next scan (at most half a second later) rebuilds it without the held
   item. The clear is the write that matters: the timer only keeps the item
   out of a new scan, and a list left holding it would hand it straight back.
5. It counts what it did. `petunstick 0` prints one line:
   `petunstick stat: held back=<n> coins released=<n> longest same-target=<n> frames`,
   followed by where the ticks went (`ticks=`, `no pet=`, `no target=`,
   `target gone=`) and every refusal with the read it failed on
   (`unreadable=`, `list not cleared=`), so a bug report can tell "did
   nothing" from "could not look" from "did the wrong thing".

Why these numbers: a target within 160 px for 1.5 s that has not gone away has
had dozens of arrived frames of `PickupLoot` attempts, and a pet travelling to
a far item never counts because it is beyond the radius. 600 frames is the same
hold the Pet Quest Collector uses (`kPetQuestHoldFrames`): long enough that the
pet visibly moves on and other items get their turn, short enough that an item
whose pickup failed for a passing reason (the player made room) is tried again.
All three are constants in the header, so a live finding moves one number.

**What it does not do.** It collects, destroys and credits nothing, and calls
no game script: no `PickupLoot`, no `instance_destroy`, no coin moved. It does
not change what the pet picks up or how fast it walks, and it does not make a
failed pickup succeed: an item the game cannot put in the inventory stays on
the ground, and the pet comes back to it about ten seconds later. A coin gets no
timer (it has none), only a dropped target, so a coin that sticks again is
given up again and counted again (`coins released=`).

**Rejected alternatives**, so nobody re-proposes them:

- **Detouring the companion's Step event or its arrival branch** (stopping the
  pet on arrival, retargeting when `PickupLoot` fails): the event is unnamed
  compiled code with no script-table entry, reachable only through a measured
  address, which `AGENTS.md` § "Never Call an Address You Resolved by Hand"
  forbids in a player build, and it would change the game's own loop.
- **Calling `PickupLoot` or `instance_destroy` from the mod** to finish the
  pickup: it would invent a collect the game refused (with a full inventory the
  item would be lost), and calling game scripts cold has crashed before
  ([pet-quest-collector-c-research.md](pet-quest-collector-c-research.md#why-the-games-scripts-cannot-be-called-cold-measured-2026-09-11)).
- **Only setting `lootTarget` to -4**: the current list, or the next scan half
  a second later, hands the same item straight back, because it is the item
  nearest the player. The hold needs the item's own timer and an emptied list.
- **Writing `lootDistance` or the type filter**: that changes what the pet does
  with every item, not what it does when it is stuck.

**Tests.** `tests/test_pet_loot_unstick_behavior.py` +
`tests/pet_loot_unstick_harness.cpp` compile the real header: baseline
scenarios (the game's own rule keeps a surviving target; the watch never asks
while the mod is off) and target scenarios (given up at exactly
`kPetLootStuckFrames` and not a frame earlier; a travelling target never
counts; another target or none restarts; given up once per target; a target
that vanishes asks for nothing). `tests/test_pet_loot_unstick_contract.py` pins
the tick's shape (gated on the switch, the three names it writes, objects by
name, the timer only on a ground item, nothing collected, destroyed or hooked,
no kind check as the gate), the off default and the panel text.

**Live.** Not yet confirmed in a live game. Live procedure 1 (player DLL, one
crowded spot with mixed loot, mod off then on) is what will record it; its
results belong in this section when it has run.
