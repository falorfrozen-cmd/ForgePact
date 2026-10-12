# #197: boss soul pieces

Issue #197 asks for a drop that takes a player to an act's boss: an ordinary
monster in a zone of an act sometimes drops a **Piece of <boss>'s Soul**, and
using a piece moves the player straight into that act's boss room, through the
game's own travel, for a fresh fight. This document holds the research behind
the mod: why it is shaped the way it is, what was read of the game, the routes
that were turned down (three of them by a live session), the research
instrument (`soulprobe`) and the debugger breakpoints Live procedure 3 ran, the
results of the four live sessions, and what is still not established. The mod
itself is `soulpiece`, off by default (the module guide's "Boss soul pieces"
section).

Everything below is labelled. **Static reading** means read from the game's
compiled code in a local decompiler (the `ghidra` MCP, build `498d588550d8`,
2026-10-11) or from local byte scans of `Hero_Siege.exe` and `data.win` with a
positive control, and written here in our own words; no game code is quoted
(AGENTS.md § Legal). **Measured** means observed on the running game, and names
the session that observed it; the session captures (`forgepact-197-boss-soul-pieces-live-<n>.md`
in the hub's workorder folder) hold the lines, and nothing of them is pasted
here. **The owner's decision** is a choice the owner made, not a fact about the
game. A thing looked for and not seen is "not observed", never "does not
happen". The game facts are also recorded in the hub's
`docs/RUNTIME_DATA_MODELS.md` § 21, the shared reference every module reads.

## Why

The issue's wording, and the owner's decisions of 2026-10-11, fix what a player
sees:

- **Which boss.** Act 1 zones drop Pieces of Gurag's Soul, Act 2 zones Pieces of
  the Grim Reaper's Soul, and so on for all nine acts; Act 7's boss is Mevius
  (the owner's decision). The act table, with each act's boss room, lives in
  the decision core (`plugin/include/ForgePact/SoulPieceMod.hpp`, `kActs`).
- **How a piece is used.** Right-clicking it in the backpack and confirming,
  the way the owner uses the Edible PAS Logo (the owner's "PAS token"; the
  owner's decision). After Live procedure 1, the owner chose the boss room over
  a boss beside the player; after Live procedure 3, a direct transfer over a
  portal: confirming a piece moves the player into that act's boss room
  (`Act_01_Boss` for Gurag, and so on) with the game's own `NetworkRoomGoto`,
  and the piece is used up. The boss room's own pentagram brings the boss out
  and handles its death. Before the move the mod drops the boss room's saved
  zone state with the game's `ZoneStateResetSingle`, so the boss comes out
  again even after a kill (the owner's decision after Live procedure 3; Live
  procedure 2 measured it).
- **How often a piece drops.** A fixed chance a little above the Gem of
  Incarnation's, rolled where the game rolls that gem (the owner's decision):
  1 in D per gem roll, D = floor(G x 4 / 5), where G is the gem's
  `droprate.base` read at run time by its repository name. Live procedure 1
  measured G = 9,000, so a piece is 1 in 7,200. Everything that makes the game
  roll gems more often (kill density, the monster's own gem chance, a lever on
  the gem's rate) gives more pieces too; the piece adds no gate, no magic-find
  term and no slider of its own.
- **What a piece is.** A piece must load without ForgePact, so it is one of the
  game's own items marked by a reserved seed, never a new item id. After Live
  procedure 1 it is the game's own Edible PAS Logo (a bag consumable), not a
  key: see "Rejected routes" for why the key and its Key tab went. The owner's
  follow-up (2026-10-12) asks for the Key tab again, pieces stacked per act and
  the Gem of Incarnation's icon; its readings ("The follow-up" under "Static
  reading") propose a key piece whose `b` carries the act in tenths, which Live
  procedure 1 of `forgepact-197-piece-icon-stack` measures.

## Static reading

Read 2026-10-11; each item is a static reading unless it says otherwise. § 21
of the runtime reference holds the same facts in the shared form.

- **The act of a room.** `RoomActIndex(room)` sorts 175 rooms into nine acts;
  any other room answers undefined. **Measured** (Live procedure 1): the answer
  is the act itself (1 in `Town_01_rm` and `Act_01_01`, 2 in `Act_02_01`); the
  first reading's "act minus one" was wrong, and the core's
  `ActOfRoomActIndex` now takes the answer as the act. **Measured** (Live
  procedure 3): two rooms outside the switch, `Act_01_Boss` and
  `Act_01_Pregame`, answered `false`, not undefined; the core takes neither as
  an act. The game itself uses the
  script (its two direct callers, found by a byte scan whose positive control
  was `PickupLoot`'s known callers, load each act's texture pages). The mod asks
  it by name, so a room the game assigns to an act is in that act; the decision
  core adds only the name rule (no town drops, no `_Boss`, `Uber_` or `Dev_`
  room).
- **Stacking, as `ItemsAreStackable` answers it.** It answers true only when
  the two items' class agrees and their definitions' `b` and `c` agree; the
  seed `a` and the count `o` are not compared. A byte scan for direct calls to
  it found none (the same scan found `PickupLoot`'s callers), and the pickup
  path below never calls it.
- **The item repository.** An item's definition names a category and an index
  into the global `itemRepoNormal`, and the game's lookup raises the runtime's
  array-index error for an index past a category's end. Inference, not
  measured: a saved item whose index is past the end cannot load without
  whatever added the entry. That is why a piece borrows an existing entry.
- **Keys.** Category 12 holds 44 entries, matching the 44 `keys_*` names in
  `data.win`. `GetDungeonKeys` builds a list of 26 entries from values not
  visible statically. The bag's Key sub-tab (`InventoryTabKey`) and a variable
  named `inventoryKeyGrid` exist. A variable named `itemSprite` exists; Live
  procedure 1 read it undefined on a built gem, so a piece carries no sprite of
  its own.
- **Act boss rooms.** None of the nine act boss rooms places a boss object; each
  places a `Pentagram_Boss_obj` (with `Boss_Block_obj` and enemy creators).
  Gurag, the Grim Reaper and Mevius all descend from `Enemy_Child_Boss_obj`
  (hs-game-sdk's object table).
- **The backpack's consume path.** `UiAInventoryConsumeItemConfirm` (the answer
  to a "use this item?" prompt) asks `PotionUse` about the item, and only on a
  true answer clears the item's cells with `InvGridClearItemNode`.
  `CanUsePotion` reads the item and the room. No script is named for a
  right-click: the dispatch is inline. **Measured** (Live procedure 1) for the
  Edible PAS Logo: its right-click and confirm reached `CanUsePotion`,
  `PotionUse` (three arguments, the item among them, answering true),
  `InvGridClearItemNode` and the confirm action, and the logo was used up; a
  potion drink reaches `PotionUse` with no item.
- **How the gem's rate reaches its roll.** `DropGems` runs only after the dying
  source's gem gate passed, and reads category 15's entries and their
  `droprate.base` (with per-place overrides in boss dungeons, the Chaos Tower,
  Niflheim, rifts and similar places). Whether it gives each socketable exactly
  1 in its rate, or picks by weight, is not established.
- **The pickup path** (static reading 13). `PickupLoot`'s general path ends in
  `AddToInventory`. Offline, `AddToInventory` reads the item's class, asks for
  its preferred grid, and tries a stack first: `InventoryGridAddToStack` for a
  class with a tab of its own (the Key, Material and Socket tabs, picked by
  class), and `GridAddToStack` for the bag. Only when no stack takes the item is
  it placed as a new one. **Measured** (Live procedure 3, under the debugger):
  five Edible PAS Logos picked up (three Gurag pieces, a Reaper piece and a
  plain logo) hit `AddToInventory` five times and `InventoryGridAddToStack`
  once after each, with the same `self`, and `GridAddToStack` not at all, so a
  consumable's stack attempt went through `InventoryGridAddToStack`, not the
  bag's script this reading names. The key merge replayed hit `AddToInventory`
  once and `InventoryGridAddToStack` not at all; which code merged the key is
  not established. **Corrected** by the follow-up's reading 1 below: this
  reading has the two scripts the wrong way round, and the corrected order
  explains both measurements.
- **What a stack compares** (static reading 14). Both stack attempts walk their
  grid and merge into an item whose class, `b` and `c` match, comparing the
  three fields themselves; neither calls `ItemsAreStackable`, and neither looks
  at the seed. `GridAddToStack` first accepts only classes 12 to 15, so a
  class-11 consumable never stacks in the bag. `c` picks the normal or the
  unique repository, so it cannot mark a piece. **Measured** (Live procedure
  3's `piece-apart`, Live procedure 2's `in-bag`): every piece took its own
  bag cell, apart from the plain logo and from each other, each with count 1.
- **A portal keeps its state in the protected store** (static reading 15).
  `Portal_Parent_obj`'s Create keeps the keys of protected records in
  `portalZone`, `portalActive`, `isEligible`, `portalUsable` and `pSpwd`: the
  variable holds a key, and the value is that record's (the way a trap's
  `enemy_hp` does, § 13.7). `Portal_obj`, a child of it, writes a room into its
  `portalZone` record when it is created. The collision with the player it
  inherits checks the portal's state and the player's interact input and sets
  an alarm; `Portal_obj`'s Alarm_0 reads the room from the `portalZone` record
  and travels there with `NetworkRoomGoto`.
- **The game's travel scripts** (static reading 16). `NetworkRoomGoto(room)`
  checks whether the room is a town, sends network effects when online, and
  offline ends in `RoomGoto(room)`, which sets the room's layers up once and
  then calls the `room_goto` builtin.
- **The boss room makes its boss** (static reading 17). `Pentagram_Boss_obj`
  reads the player's distance and its own spawned and killed state; its Alarm_0
  creates the room's boss on the game layer at the pentagram, choosing the
  object by room. The objects it can create include Gurag, the Grim Reaper,
  Anubis, Damien, the Karp King, Satan, Mevius and Odin's cutscene; Cthulhu was
  not observed among them. Which room maps to which object was not read. The
  death script every act boss runs (`LoadBossDeath`, from
  `Enemy_Child_Boss_obj`'s Destroy) needs a pentagram in the room.
  **Measured** for acts 1 and 2 (Live procedures 3 and 2): `Act_01_Boss`'s
  pentagram brought out Gurag and `Act_02_Boss`'s the Grim Reaper; the
  pentagram's Alarm_0 ran before Gurag appeared, and `LoadBossDeath` ran when
  he died there, with no crash.
- **The portal objects the SDK names** (static reading 18). `Portal_obj` is the
  general one, whose target is only a room. `Portal_Town_obj`,
  `Portal_Dungeon_Exit_obj` and `Portal_Hell_obj` also descend from
  `Portal_Parent_obj`; `Portal_Uber_Boss_obj` takes its target from uber state,
  and `Portal_Waypoint_obj` opens the waypoint menu.
- **Not read** (static reading 19). `PotionUse`'s body did not decompile through
  the MCP (its output exceeds the response buffer), so whether a consumable's
  effect is applied inside it is not established. `CanUsePotion` reads the
  item's stats and the room; whether it refuses the Edible PAS Logo in some
  room is not established.
- **Zone state** (Amendment 2, the runtime reference's § 21.6). The game keeps
  a room's contents after the player leaves in a global map, `zoneStates`,
  keyed by room; `ZoneStateResetSingle(room)` deletes one room's entry and
  `ZoneStateExists(room)` answers whether the room has a visited entry. The
  pentagram sets its spawned state afresh on every creation. **Measured** (Live
  procedures 2 and 4): see "Results".
- **Already in the shared reference.** `LootGroundCreate` writes a fresh seed
  (§ 18.4); `InitItemFromJson` then `LootGroundCreateFromItem` keeps the
  definition, seed included (`sigdrop`, #74; relics, #124); Karp King and four
  uber bosses spawned in Act_01_01 were killed there through HP 0 with their
  drops (§ 13.1); `sCP` is the game's own spawner, and it stamps the spawned
  flag portals carry in `pSpwd` (§ 20.5); the protected store faults on a key
  outside it (§ 11.2).

### The follow-up: a piece in the Key tab, stacked per act, with the gem's icon

The owner's follow-up (2026-10-12, workorder `forgepact-197-piece-icon-stack`)
asks for pieces that show the Gem of Incarnation's icon, live in the bag's Key
tab, stack per act, never join a real key, and load without ForgePact. These
readings were made for it on 2026-10-12, through the `ghidra` MCP (same build)
and headless `FindCallers.java` / `ListCallsIn.java` runs whose positive control
(`InvGridClearItemNode` called from `UiAInventoryConsumeItemConfirm`, the known
site) was found. The local material, outside every repository, is indexed with
the hub's `tools/decomp_index.py`. Each is a static reading unless it says
otherwise, in our words.

1. **Which stack attempt the pickup makes.** Offline, `AddToInventory` first
   asks `GetItemPreferredGrid` for the item. That script answers a grid of its
   own for keys, materials, socketables, tarot cards and the vault, chosen by
   the item's class (`inventoryKeyGrid` for a key). When there is one,
   `AddToInventory` tries `GridAddToStack` on that grid first and only if that
   fails `InventoryGridAddToStack`; when there is none (a consumable), it tries
   `InventoryGridAddToStack` alone. A caller scan found one `GridAddToStack`
   site and two `InventoryGridAddToStack` sites, all in `AddToInventory`. This
   fits both Live procedure 3 measurements: the logo's pickups reached
   `InventoryGridAddToStack`, and the key merge reached only `AddToInventory`,
   because `GridAddToStack` (not watched then) took the key.
2. **What a stack compares, and how.** `GridAddToStack` compares the class,
   then both definitions' `b`, then their `c`, each through the runtime's
   general value compare. That compare treats two reals as equal when they
   differ by no more than the math epsilon (GameMaker's default is 0.00001;
   whether the game changes it is not established). So `b` 2.1 and 2 differ,
   and 2.1 and 2.1 match.
3. **The repository lookup truncates.** `GetNormalRepoStruct` turns its index
   argument into an integer through the runtime's real-to-int conversion, which
   truncates toward zero, so `b` 2.1 looks up entry 2. The item constructor
   takes the unique repository when the definition's `c` is truthy (a real
   above 0.5), and the normal one otherwise.
4. **The save writes reals through `string()`.** `CreateItemSaveStruct` copies
   the definition, and `json_ordered_by_key` writes it, sending every real and
   64-bit integer through the `string` builtin (the slot names came from the
   decompile index's slot table). GameMaker's documented `string()` of a real
   keeps two decimals and prints a whole number whole (runtime documentation,
   not read here; Live procedure 1's `reload-frac-kept` and the hub's
   `save_item_keys.py --defs` measure it). So a `b` in tenths survives a save,
   and a finer fraction would not.
5. **Who else compares `b`.** `CountInventoryItem`, whose only callers seen are
   the crafting scripts `GetCraftItemsAvailable` and `CraftFindRecipeItems`,
   compares the class, an item-info value and the definition's `b`, not `c`.
   `FindInventoryItem` (called from `PlayerMouseAction` and two unnamed sites)
   compares `b` and `c`. Both use the same epsilon compare. Which code a door
   or an NPC uses to find its key (Heimdall for the Bifröst Key, and the rest)
   was not read.
6. **The bag's right-click.** `ProcessInventoryGridInput` is too large to
   decompile through the MCP; the list of the scripts it calls holds, in
   order, `ValidateItem`, `VaultUse`, `CanUsePotion`, `MessagePrompt` (the use
   prompt), `PotionUse`, `InvGridClearItemNode`, then the equip scripts.
   Whether the Key tab's grid goes through the same path, and whether
   `CanUsePotion` answers true for a key, are not established. `PotionUse` is
   also called from `UiAInventoryConsumeItemConfirm`, `ControllerCheckInput`,
   `DrawMobilePotionIcon` and one unnamed site.
7. **The confirm and a stack.** After `PotionUse` answers true,
   `UiAInventoryConsumeItemConfirm` compares one item-info value with 6 (its
   meaning was not read) and then clears the item's cells one by one. Whether
   a stack of N then loses one or all N is not established.
8. **The display sprite.** ForgePact #158's Live 1 (2026-10-12, on its own
   branch) measured item info key `"11"` as the sprite the bag and the ground
   item draw: a charm given a Grand charm's sprite there drew the Grand icon on
   the ground and in the bag, a sprite added at run time drew in the bag and the
   game's error sprite on the ground (the ground item looks up `Pickup_` + the
   sprite's name, #158's static reading), no tooltip drew an icon, and the key
   is not saved. hs-game-sdk's sprite table holds both `Gem_Incarnation_spr`
   and `Pickup_Gem_Incarnation_spr`, so the gem's sprite has a ground
   companion. That a built Gem of Incarnation carries `Gem_Incarnation_spr`
   under `"11"` is not established (Live procedure 1's `gem-sprite`).
9. **`ValidateItem`**, which the input script calls throughout, builds
   description strings (type and rarity names, `sprite_get_name`, loot name
   data); no removal call was seen in it.
10. **Not observed** through the MCP: callers of `GetUniqueRepoStruct` and of
    `CanUsePotion` (its reference table is mostly empty; not a negative).

Together, readings 2 to 4 give a stack route with no hook on the add path: a
piece on a key whose `b` is the key's index plus act/10 (2.1 for Act 1 on an
index-2 key) loads as that key everywhere (reading 3), stacks only with pieces
of its own act and never with the whole-`b` real key (reading 2), with or
without ForgePact, because the save keeps the tenths (reading 4); crafting
counts and any `b`-equality key check never take it for the key (reading 5).
Live procedure 1 of the follow-up measures it beside a control and an
alternative (`soulprobe shape frac`, `cfrac`, `plain`; "The instrument").

### After the follow-up's Live procedure 1: pieces that stack in the bag

Live procedure 1 measured the Key tab's right-click going to the equip script,
never to the use (`keyuse-route: not-reached`), and the owner then kept pieces
as class-11 bag consumables, used by right-click and confirm from the bag, with
pieces of one act stacking there and never joining a real item (2026-10-12).
These readings were made for that on 2026-10-12, through the `ghidra` MCP (same
build) and local annotated decompiles that the hub's `tools/decomp_index.py`
indexes (`InventoryGridAddToStack`, `ItemsAreStackable`, `IsStackable`,
`AddToInventory`, and two new ones, `SaveInventoryOrderNew` and
`InventoryGridCanAddToStack`). Each is a static reading, in our words.

1. **The class rule.** `IsItemTypeStackable` answers true for classes 12, 13,
   14 and 15 only. `IsStackable` first asks the item's own item-info lookup for
   one value (which info key was not resolved) and answers false when it is
   truthy; otherwise it applies the same four-class rule.
2. **`InventoryGridAddToStack` refuses a consumable first.** Its item is its
   second argument. It applies the `IsStackable` rule itself, before anything
   else, and gives up at once when the rule fails, before it reads a grid or
   compares a field. So a class-11 piece is refused whatever its `b`, `c`, seed
   or count: this is why the logo's pickups reached the script in Live
   procedure 3 and still never stacked. For a class that passes, it picks its
   grid by class (15 the socket grid, 12 the Key grid, 13 and 14 the material
   grid); a later walk reads the bag's `inventoryTab`, under a condition not
   read.
3. **The cap.** A merge goes ahead only while the summed count stays at most
   999.
4. **The result.** Every path, the early refusal included, ends by building the
   game's `s_ItemOperation` struct (`gml_Script_s_ItemOperation` in
   hs-game-sdk) and returning it, and `AddToInventory` reads that result's
   `success` at each of its stack attempts. The runtime reference (§ 9.7)
   measured a success result's fields on the prospect path: `tabNumber`, `x`,
   `y`, `tabType`, `success`. Whether `AddToInventory` reads any field but
   `success` was not read.
5. **The bag's save writer.** `SaveInventoryOrderNew` writes the bag's tabs and
   the special tabs (key, material, socket, tarot, relic, vault, charms)
   through `json_stringify`; when the game calls it was not read. Live
   procedure 1's `inventory_order_13.hss` was not rewritten at its graceful
   stop, while every #197 session's bag file was rewritten with the same
   C0000409 exit, so the exit code is not the difference; Live procedure 1's
   Key tab held fractional-`b` pieces. The cause is not established.
6. **So no route without a hook exists.** No consumable class or base stacks
   through the game's stack code, and no value in a piece's own definition can
   make a class-11 piece stack (readings 1 and 2; Live procedure 3 and #197's
   Live procedure 2 measured class 11 never stacking). Every class that does
   stack goes to its own tab (the Key, material or socket grid), not the bag,
   and the Key tab's right-click never reaches the use (Live procedure 1).
7. **The bag's own data.** `soulprobe bag` read the bag as
   `New_Inventory_Data_obj.inventoryTab` in Live procedure 3 (450 cells, every
   piece listed); the runtime reference names `New_Inventory_Data_obj` as the
   inventory data instance. A blind `GetProfileInventoryData` call crashed the
   game in an earlier probe (runtime reference § 9.4), so nothing calls it.
8. **Not observed** through the MCP: the stack scripts' callees (its reference
   table is mostly empty; not a negative).

So the stack is made by one detour on `InventoryGridAddToStack` that changes
only a call carrying a soul piece (the bag stack route, under "Rejected routes,
and why"), and Live procedure 2 of the follow-up measures it.

## Rejected routes, and why

- **New item ids.** Appending a piece to `itemRepoNormal` makes a save that
  cannot load without ForgePact (the repository reading above).
- **A key base and the Key tab** (the first plan, and the owner's first
  preference). **Measured** (Live procedure 1): pieces built on the Bifröst
  Key, dressed and named, merged on pickup into the owner's own Bifröst Key
  stack, which kept its own seed, and the merge survived a reload. Static
  readings 13 and 14 say why: the Key tab's stack attempt compares class, `b`
  and `c` only. No key definition is established to be out of every player's
  reach, so the owner's own rule ("Key tab preferred, bag OK") selects the bag.
  A load-safe key of our own is filed as research (ForgePact #210). The
  follow-up (2026-10-12) revisits the Key tab with a piece whose `b` is not
  whole, which that stack check keeps apart (the follow-up's readings 2 to 4);
  what was rejected here is a key piece with the key's own whole `b`, which
  Live procedure 1 of the follow-up keeps as its merge control.
- **A veto on the add or stack path** (`InventoryGridAddToStack`,
  `GridAddToStack`, `ItemsAreStackable`). The owner ruled out a stack veto; a
  veto there would touch every key, material and socketable pickup in the game,
  and pieces would still merge into keys whenever ForgePact is off. Live
  procedure 1's detour on `ItemsAreStackable`, both routes, counted no call in
  log or veto mode while the keys merged: the stack never asks it (static
  reading 14).
- **A boss created beside the player** (the first plan's use, by
  `instance_create_depth` or `sCP`). **Measured** (Live procedure 1): a Grim
  Reaper created in `Act_02_01` and brought to HP 0 crashed the game, because
  its death script needs the room's pentagram (static reading 17). The boss
  belongs to its room; the player is taken there instead. Neither the probe
  nor the mod creates a boss, nor a pentagram to satisfy the death script.
- **A portal to the boss room** (the design after Live procedure 1).
  **Measured** (Live procedure 3): a `Portal_obj` made beside the player with
  the boss room written into its `portalZone` record showed, stayed and could
  be entered, but its record read back as the zone's ordinary next zone, and
  it took the player there (`Act_01_Pregame` from town, for both the Act 1 and
  the Act 2 piece; `Act_01_02` was what the one opened in `Act_01_01` read).
  Why the record did not keep the boss room is not established. The owner's
  reading, after Live procedure 2 (2026-10-12), not measured: the run-time
  `Portal_obj` is probably the door-to-the-neighbouring-zone kind of portal,
  not one that honours a boss room. `NetworkRoomGoto` by name reached both
  boss rooms, so the owner chose the direct transfer (2026-10-11), and no
  portal ships. The probe keeps its portal commands as research history.
- **The raw `room_goto` builtin.** § 13.1 measured one to an uber room closing
  the game; the game's own `RoomGoto` sets the room up first (static reading
  16). Travel is the game's: `NetworkRoomGoto` by name, which Live procedure 3
  measured reaching both boss rooms.
- **The other portal objects** (static reading 18). `Portal_Uber_Boss_obj`
  takes its target from uber state, not a room we set; `Portal_Waypoint_obj`
  opens the waypoint menu; the town, dungeon-exit and hell portals carry their
  own purpose. `Portal_obj` is the general one whose target is a room, and it
  did not reach the boss room either (above).
- **Duplicating a zone's own portal.** Duplicating reward portals caused endless
  loading (§ 15 of the runtime reference); the research probe made one fresh
  `Portal_obj` through the game's spawner instead, and the shipped mod makes
  none.
- **A second route to a fresh boss**: writing the pentagram's `bossSpawned`
  after arrival (after the game's own step, a frame-timing guess), setting the
  zone controller's `zoneStateReset` while the player leaves the boss room
  (only kills made with the mod on, and a hook at room end),
  `ZoneStateResetAll` (drops every zone's state, the player's own zone
  included), or a detour on any `ZoneState*` script (density already holds
  `ZoneStateResetSingle`'s). The mod calls `ZoneStateResetSingle` on the boss
  room only, the call the game makes itself for the rooms it does not keep.
- **`CanUsePotion` or the confirm action as the use.** `CanUsePotion` runs at
  the right-click, before the confirm, so the transfer would run on a
  right-click the player then cancels; the confirm action's argument is an
  array, not the item. `PotionUse` carries the item, and its true answer is
  what makes the game clear the item's cells.
- **Taking a piece out of the bag ourselves.** The game clears the cell itself
  after `PotionUse` answers true, as it does for the logo; a map entry left
  without its cell crashes the save (runtime reference § 17).
- **The Key of Terror parts** ("Gurag's Soul" and its kin, category 13): real
  crafting items. Every piece name therefore starts "Piece of".
- **`LootGroundCreate` for placing a piece**: it writes a fresh seed, which
  would erase what marks the piece. The probe and the mod place through
  `InitItemFromJson` then `LootGroundCreateFromItem`.
- **A second detour** on a script ForgePact already detours (`DropGems`,
  `CreateItemNew`): a second inline detour on one script falls back to the
  table route, which compiled GML's direct calls never see. The probe's gem
  watch and dress are consumers inside the existing detours.
- **Hooking the bag's right-click itself**: the drag and the right-click are
  dispatched inline, with no named script to hook. The instrument watches the
  scripts that dispatch reaches instead.
- **The bag drop as the use** (the first plan): replaced by the owner's
  right-click, 2026-10-11.
- **A chance from a zone census with a density divisor** (the first plan):
  replaced by the owner's fixed chance inside the gem roll, so density and the
  game's own gem levers scale pieces.
- **A fixed one-in on every kill**, outside the gem roll: it would roll around
  the game's gem gate, so it would be many times the gem's rate rather than "a
  little more common", and no gem lever would reach it.
- **A drop-rate row or slider of our own for pieces**: a dependent switch; the
  owner wants the game's own mechanics to set the rate.
- **#44's route**: `bossrarity` raises a boss's rarity and spawns nothing.
- **The suspend-the-loop class.** This mod is not one of the features
  [menu-pause-plan.md § 0](menu-pause-plan.md#0-recommendation-dont-build-this)
  recommends against: it asks the game's own travel to move the player and
  drops one room's saved state just before, as the game does for rooms it does
  not keep, and the game runs the travel, the boss, its fight and its death as
  it runs any other. Nothing is paused, frozen or restored.

The follow-up's own rejected alternatives (2026-10-12):

- **A veto or guard on `GridAddToStack`, `InventoryGridAddToStack` or
  `AddToInventory`.** Ruled out by the owner in #197; it would touch every key,
  material and socketable pickup, and with ForgePact removed the pieces would
  merge again.
- **New repository entries or item ids.** A save holding them cannot load
  without ForgePact (the repository reading above).
- **ForgePact #210's side file** (a key entry made at run time, the pieces kept
  out of the save and put back at load): save-time hooks and a second store of
  truth, for a result the definition value gives directly. It stays in the
  backlog.
- **Nine different whole key bases**, one per act: each merges with its real
  key.
- **Changing the class at creation over a consumable's `b`**: how a saved
  item's class and repository category are split on load was not read, and
  without ForgePact a consumable could load into the Key grid.
- **`b` fractions finer than tenths** (sixteenths and the like): the save keeps
  two decimals (the follow-up's reading 4).
- **`CanUsePotion` as the use itself**: as above, the transfer would start on a
  right-click the player then cancels. Answering "usable" there for a piece,
  with the transfer still in `PotionUse` after the confirm, is a different
  thing, and is what `soulprobe usewatch allow` measures.
- **Taking a piece out of a stack ourselves after the game cleared it** (and
  rebuilding a smaller stack): more moving parts than lowering the count
  before the game clears anything (`soulprobe usewatch take one`).
- **A sprite of our own** (`sprite_add`): #158 measured the game's error sprite
  on the ground for one. The piece borrows the gem's own sprite and its ground
  companion through key `"11"`.

**The bag stack route** (after the follow-up's Live procedure 1, 2026-10-12).
The owner had preferred no hook at all; the bag-stack readings above show that
"no hook" and "bag consumables that stack" cannot both hold, so pieces stack
through one both-route detour on `InventoryGridAddToStack`, the stack script
every class-11 pickup reaches (Live procedure 3, under the debugger, with the
pickup's own `self`). It always calls the game's own script first with the
call's own arguments and keeps the game's result. Only when the merge is
armed, the result is a struct whose `success` is not true, the item argument
is a legacy soul piece (the core's class-11 rule: class 11, the base's index,
seed 777101 to 777109) and the core's `DecideMerge` names a bag item of the
same act with room under the cap does it change anything: that item's count
`o` rises by the incoming count, its hash is refreshed as the dress does
(`ItemCheckHash` refuses a stale one), and the game's own result gets
`success` true and the target's tab and cell in `tabNumber`, `x` and `y`;
every other field stays as the game wrote it. The first piece of the act in
walk order with room takes it; a real item (no reserved seed) is never a
target and never merged, a key-shape piece is not a bag piece, and the item
being picked up is never its own target. Anything else, a failed read
included, returns the game's result untouched and is counted by reason; a
hash refresh that fails puts the count back, so the game places the piece
apart rather than counting it twice. This answers the three reasons recorded
against a stack hook in #197: it vetoes nothing (the owner's ruling was against
a veto), it changes only calls carrying a soul piece (a key, material or
socketable pickup passes through untouched), and with ForgePact removed nothing
can merge a class-11 piece into anything, so a saved stack stays an Edible PAS
Logo stack with its count. It is one value changed inside a call the game is
already making, and the game's pickup then finishes as for any stack it made
itself, which Live procedure 2 checks (`merge-no-duplicate`). The bag is read
the way `soulprobe bag` read it (static reading 7 above). Until the player
build arms it, only the research verb installs it (`soulprobe merge on`).

Added after the follow-up's Live procedure 1 (bag pieces that stack):

- **Key-tab pieces** (`frac` and the rest): Live procedure 1 measured the Key
  tab's right-click going to `InvGridEquipV2`, never reaching `CanUsePotion`,
  even with the probe's allow armed; the owner chose bag consumables.
- **A consumable class or base that the game already stacks in the bag:** none
  exists. Every stack script takes classes 12 to 15 only, and those classes go
  to their own tabs (the bag-stack readings 1, 2 and 6). Potions are flasks, a
  different container, not items in a bag cell.
- **Merging at the frame boundary** (a tick that finds two pieces of one act in
  the bag and folds one into the other, clearing a cell with
  `InvGridClearItemNode` by name): no hook on the stack path, but it moves bag
  cells outside any call the game is making, needs the grid node as `self`
  (runtime reference § 9.7 measured that on the open prospect grid only),
  leaves the piece standing alone for at least a frame, and validates at the
  frame boundary rather than at the point of use (hub `AGENTS.md`, "Check a
  Permission Where It Is Used"). The pickup's own stack call is that point of
  use.
- **A result struct of our own** for the merge: the game's own result already
  carries whatever `AddToInventory` reads beyond `success` (not read), so the
  merge changes `success` and the cell on the game's own struct and keeps the
  rest.
- **A detour on `AddToInventory`, `GridAddToStack`, `ItemsAreStackable` or
  `IsStackable`:** `AddToInventory` sees every pickup; `GridAddToStack` is
  never reached by a consumable (it has no preferred grid); the two predicates
  are not on a consumable's pickup route (#197's Live procedures 1 and 3
  counted `ItemsAreStackable` at zero). `InventoryGridAddToStack` is the
  narrowest script a consumable's pickup is measured to reach. `StashAddToStack`
  is the stash's, which pieces do not enter.
- **Making the piece's class stackable for the duration of the call:** the
  stack script then routes it to the Key, material or socket grid by class.
- **A `CanUsePotion` detour:** the bag's right-click already reaches the use.

## The instrument

`soulprobe` is research build only (`plugin_build\build.bat dev`), never in the
player build's command list. Every reply line starts `soulprobe <sub>:`, so a
capture can be grepped. Each decision a line prints is the decision core's
(`ForgePact::SoulPieces`): the act of a room (`ActOfRoomActIndex`), the room
rule (`RoomEligible`), the drop verdict and D (`DecideDrop`,
`PieceDenominator`), the use verdict (`DecideUse`) and the boss room a piece
leads to (`TransferTarget`). The probe only supplies what the game answers.
Its portal commands (`portals`, `portal`, `portalroute`, `portaloffset`,
`portalset`, and the portal a `use` opens) are Live procedure 3's research
history: the shipped mod opens no portal. Replan 1
removed `stackhook`, `spawn`, `spawnroute`, `spawnoffset`, `take` and `sprite`,
with their state: the stack veto, the boss beside the player, the probe's own
take and the gem's sprite are all rejected above. The follow-up (2026-10-12)
added `shape`, `icon` and `ground`, the `sprite=` fields and `gem`'s
`sprite11=`, and two use modes, `usewatch allow` and `usewatch take all|one`,
for Live procedure 1 of `forgepact-197-piece-icon-stack`: the stack shapes a
key piece can take, the gem's display sprite under item info key `"11"` (not
the old `itemSprite` route), and the Key tab's right-click. Fractions print
with two decimals (`b=2.10`, `c=0.05`); a whole number prints whole. For
Live procedure 2 (pieces that stack in the bag) it added `soulprobe merge
on|off`, which installs the shipped mod's one `InventoryGridAddToStack` detour
(the bag stack route, above) and no detour of its own, the merge's counters on
`status`, and each `bag` row's cell and key `"11"` sprite.

| Subcommand | What it does and prints |
|---|---|
| `status` | `base=`, `category=`, `dress=`, `shape=`, `icon=`, `allow=`, `takemode=` (`off`, `all` or `one`), the counters `allowed=` and `taken=` (calls `allow` and `take` answered), `usewatch=`, `usescript=`, `gemwatch=`, `portalroute=`, `portaloffset=`, the counters `usecalls`, `usecallsby=` (calls per candidate, so a script called every frame shows during `log`, before `use` is armed on it), `uses` (pieces the chosen script was handed in `use`), `portals` (portals opened, by `portal` or a use) and `gemrolls`, and the gem detour's `gemroute=`; then the bag merge's `merge=<on\|off>`, `mergeroute=` (`none` until the first `merge on`), `mergecalls=` (every call the detour saw), `merged=`, `mergepass=` (every call it left as the game answered it) and `mergepassby=` (those, by reason). |
| `keys` | Every category-12 entry: its repository name, `droprate.base`, and what `GetKeyDungeon` answered when given the entry's index; then `GetDungeonKeys`' list, the keys no dungeon names (`candidates=`), and `entries=`. |
| `act` | The room's index and name, what `RoomActIndex` answered, the act the core takes from it, `town=`, `bossroom=` and the core's `eligible=`. |
| `base <repository name>\|auto` | Chooses the base by its repository name, looked up in category 11 (consumables), then 12 (keys); `auto` is the core's `consumable_edible_pas_logo`. Prints its category and index. Never an index constant. |
| `dress on\|off` | While on, a piece of the chosen base (any shape) takes its act's name on `CreateItemNew`'s return (a consumer inside the shared detour), and with `icon on`, on a key base only, the Gem of Incarnation's sprite under item info key `"11"` (a legacy consumable piece's key `"11"` comes from the shipped dress alone, so the bag check on a legacy piece measures the shipped route and not the probe's write); then the item's hash is refreshed, which covers both writes. The dress runs inside `InitItemFromJson`, so a placed piece's ground item is made from the dressed info. Off at startup. |
| `shape plain\|frac\|cfrac` | How `place <act>` builds a piece on a key base (`soulprobe shape: <s>`; `plain` at startup). `plain`: the key's own whole `b`, `c` 0, the piece's seed: the first design, which merged, so the positive control for every "stays apart" result. `frac` (the candidate): `b` = the key's index + act/10 (the core's `KeyPieceB`: 2.1 for Act 1 on an index-2 key), `c` 0. `cfrac` (the measured alternative): the whole `b`, `c` = act/20 (0.05 to 0.45, under the 0.5 the runtime takes as truthy, so the normal repository). On a consumable base, and for `place plain`, the shape is ignored: the plain base. |
| `icon on\|off` | `soulprobe icon: <on\|off> sprite=Gem_Incarnation_spr index=<i>` (or `index=unresolved(<why>)`): resolves the gem's sprite by the core's name, `Gem_Incarnation_spr`, through `asset_get_index` exactly as the shipped dress does (never an index), and switches the dress's key `"11"` write, which applies to pieces of a key base only. Off at startup. |
| `gem` | Builds one Gem of Incarnation through the loader, places nothing, and prints its `itemSprite`, its item info key `"11"` by name (`sprite11=`, or `-`), the gem's `droprate.base` (found by its repository name) and the D the core derives from it. |
| `place <act> [count]` | Places `count` pieces (each its own item: the base's class, the shape's `b` and `c`, `j` 0, `o` 1, seed 777100 + act), each built through `InitItemFromJson` and placed with `LootGroundCreateFromItem`, the player as self, then reads the ground item's struct back. A fractional `b` or `c` goes into the item's JSON with two decimals. Each item gets its own fingerprint key, `0-0-<stamp>-<class>`, from the millisecond clock stepped past the last stamp the verb used, so two items built in one millisecond never share a key. The line: `act=`, `shape=`, `ground=<id>` (or why not), `itemType=`, `b=`, `c=`, `a=`, `o=`, `name=`, `sprite=` (key `"11"` of the item the ground instance carries, by name, or `-`), then `j=` and `key=`. |
| `place plain [count]` | The same for the base itself with an ordinary seed (a random whole number outside 777000 to 777999) and the base's whole `b`, undressed: the control a piece is compared against. |
| `pickup` | `PickupLoot` on each ground item of the base (every shape and plain: the class and the truncated `b`), self the ground item and other the player (the companion's call shape with the player in the pet's place), and destroys the ground instance on a true return, as the game's callers do. |
| `ground` | Every ground item (`Loot_Ground_obj`, by its SDK name) within 300 px of the player: `soulprobe ground: id=<id> sprite=<name> itemType=<t> b=<b> a=<a> name=<name>`, the sprite being the ground instance's own `sprite_index` by name (`Pickup_` + the display sprite's name when the item carries one, #158's reading), then `soulprobe ground: items=<n>`. Reads only. |
| `keytab` | The Key tab: the object that owns `inventoryKeyGrid` (the first of `New_Inventory_Data_obj`, `Controller_obj`, `UI_Inventory_obj`, `UI_Stash_obj`, `Player_obj` that carries it) and its cells, then one line per item, `soulprobe keytab: <fingerprint> b= c= a= o= name= sprite= piece=<act\|no> shape=<plain\|frac\|cfrac\|->` (`sprite=` is key `"11"` by name), then `soulprobe keytab: items=<n>`. A piece, here and for `allow` and `take`, is an item of a key base's class whose truncated `b` is the base's index and whose seed marks an act, any shape; on a consumable base, a legacy bag piece by the core's rule. The shape is read within the core's tolerance, so a value back from a two-decimal save still reads as its shape. |
| `bag` | Every bag item of the base's class and `b`: a summary line with `route=` (which variable was read), the cells and the items, then one line per item, `soulprobe bag: <fingerprint> t= b= a= o= cells= cell=<tab>,<x>,<y> name= sprite= piece=<act\|no>`: its class, `b`, seed, count `o`, the cells it covers, the cell it starts in (the first the walk meets: the tab, then the column and the row, taking a grid as rows of cells; the merge writes the same cell into the game's result), its name, key `"11"`'s sprite by name (or `-`) and its act. Which variable holds the main bag grid is not established, so it tries `inventoryTab` and `inventoryData` on the same owners as the Key tab (a grid, or a grid per bag tab), then the open bag window's grid nodes, and says which it read and what the others held (Live procedure 3 read `New_Inventory_Data_obj.inventoryTab`, the route the merge reads). |
| `merge on\|off` | `soulprobe merge: <on\|off> route=<both\|table-only\|failed(<why>)\|none>`. The first `on` installs the shipped mod's one both-route detour on `InventoryGridAddToStack` (the bag stack route, above), so every debugger row is armed before it; off at startup, and `off` leaves the detour in place, passing every call. While on, a call whose item carries a soul piece, a reserved seed the core does not take for one, or the base itself (a plain Edible PAS Logo prints its `pass(not-piece)`, so a logo pickup that reaches the detour is told apart from one that never does) prints `soulprobe merge: act=<n> into=<fingerprint> o=<before>-><after> cell=<tab>,<x>,<y> success=<before>-><after> hash=refreshed bag=<n> sameact=<n> fields=<the result's field names>` when it merged, else `soulprobe merge: act=<n\|no> pass(<reason>) bag=<n> sameact=<n> [why=<step>] success=<value> fields=<names>`. `bag=` is how many bag items the merge's own walk read, the incoming item left out (-1 when it never walked the bag), and `sameact=` how many of those are pieces of the incoming piece's act, so a `no-target` from an empty walk reads apart from one where the bag held no piece of that act; `why=` names the step a `read-failed` stopped at (the bag read's own reason, `verdict`, `target-range`, `result-write` or `threw`). The reason is one of `not-piece`, `no-target` (no piece of that act in the bag), `cap` (every one would pass 999), `not-struct` (the game's result is not a struct), `already-success` (the game stacked it itself), `read-failed` (the bag or the result could not be read or written) or `hash-failed` (the target's hash did not refresh, so its count went back; the line names it with `into=`). A call carrying anything else prints nothing, except the first one after each `on`: `soulprobe merge: first-other success=<value> fields=<names>`, the game's own result shape, the control. The lines only read: the result printed is the one the detour returned. |
| `usewatch off\|log\|use [auto\|<script>]` | A both-route detour on each use candidate (`UiAInventoryConsumeItemConfirm`, `CanUsePotion`, `PotionUse`, `PotionUseFlask`, `InvGridClearItemNode`, `InvGridEquipV2`, `ConsumeToken`, `UiASteamInventoryItemUse`, `UiAGuildStashConsumeTabToken`), installed on the first `log` or `use`, with one route line each. Each call prints a `usecall` line: its script, self, the item it was given, whether that item is a piece, its answer, and its `depth=` among the candidates open on the thread. The item is an argument that is an item struct, or one handed over as a grid cell or a fingerprint-shaped string, resolved through `GetItemFromFingerprint` by name. In `log`, every call goes on as it would without the watch, pieces included: to the game for every candidate but `PotionUse`, whose watch rides the shipped mod's one detour, so there "the game" is the mod's own use (`SoulPieceUseCall`), which never hands a piece to the game's `PotionUse`. In `use`, a piece in the chosen script (`auto`: the first candidate whose line carried a piece) is never handed to the game: the core's `DecideUse` decides; on `portal` a portal opens as `portal` makes it, and the call answers as the game answered the control (the plain base) on that script, true when no control was seen, so the game clears the piece's cell itself; a refusal or a failed portal answers false, and the confirm stops with the piece kept. A `use` line names the act, the script, the portal (or the refusal or failure), the target, the answer and whether a control was seen. Every other call passes to the game. |
| `usewatch allow` | `soulprobe usewatch: allow script=CanUsePotion`. Installs the same watches as `log` (no new detour) and, in `CanUsePotion`'s own watch, answers true for a call carrying a piece, without the original: `soulprobe use: script=CanUsePotion piece=<act> shape=<s> answer=true(allowed)`. It measures whether a "usable" answer opens the use prompt for a Key-tab piece the game itself would refuse. |
| `usewatch take all\|one` | `soulprobe usewatch: take <all\|one> script=PotionUse`. A branch inside `PotionUse`'s watch, which rides the shipped detour: a call carrying a piece never reaches the original and makes no transfer. `all` answers true, so the game clears the cells as the confirm does; `one` takes one piece the way the core's `TakeOne` decides it: a stack above one has its count `o` lowered by one on the item's own definition, the item's hash refreshed, and the call answers false so the game keeps the cells; the last piece answers true. Line: `soulprobe use: script=PotionUse piece=<act> shape=<s> o=<before>-><after> answer=<true\|false> mode=<all\|one> hash=<refreshed\|unchanged\|failed>`. `usewatch off` turns `allow` and `take` off with the rest; neither prints `usecall` lines unless `log` or `use` is armed too. |
| `gemwatch off\|log` | A consumer inside the shared `DropGems` detour (installed through the gem mod's own installer once a player exists): per call, the source's object and id, whether it is a monster and a boss, the room and act, and the core's drop verdict with the mod taken as on. Nothing is placed. |
| `portals` | Every instance descending from `Portal_Parent_obj` in the room: its id, object, place, `visible`, the `portalZone` key and the room its record holds, and the `portalActive`, `isEligible`, `portalUsable` and `pSpwd` records. A record is read through `PC_GetVariableGMLWrapper` by name, and only for a whole-number key inside the store (0 to 262143); anything else prints why it was not read. The town's portals are the reference values for `portalset`. |
| `portal <act> [scp\|depth]` | One `Portal_obj`, by its SDK name, at the player's x plus the offset (96 px, mirrored past the room's width) and the player's y: through the game's spawner `sCP` with the player as self (the default; when its answer is not the instance, the new `Portal_obj` nearest to that point), or `instance_create_depth` at the player's depth. Then the act's boss room (the core's `PortalTarget`, resolved by name) is written into the record its `portalZone` key names, through `PC_SetVariableGMLWrapper`, and read back. The line gives the target, the route, the id or why it failed, the place, the key and the record before and after; `portalwatch` lines follow at 1, 5, 10 and 30 s (exists, visible). |
| `portalroute scp\|depth`, `portaloffset <px>` | The defaults `portal` and a use take. |
| `portalset <id> <variable> <value>` | Writes one of a portal's five records (a number, or `true`/`false`) through `PC_SetVariableGMLWrapper` and reads it back, only on an instance descending from `Portal_Parent_obj` and only under a key inside the store: Live procedure 3's way to find which value makes a portal that shows but cannot be entered work. |
| `goto <act>` | The game's `NetworkRoomGoto` with the act's boss room, by its SDK constant, the player as self: the travel without a portal, Live procedure 3's fallback route. Never the raw `room_goto`. |
| `bosses` | Every instance descending from `Enemy_Child_Boss_obj` in the room: its id, object and health (its `enemy_hp` record). |
| `kill <id>` | Sets that instance's `enemy_hp` record to 0 through `PC_SetVariableGMLWrapper`, by name (§ 13.1), and reads whether it is gone 10 s later. It destroys nothing. |

Every script is reached by its hs-game-sdk constant, every object by its SDK
name and every room by the core's name for it, each resolved with
`asset_get_index` at run time. No address is stored. `craftprobe` and
`prospectprobe` list some of the same use candidates, and `prospectprobe`'s
script table holds `InventoryGridAddToStack` too: arm `usewatch` or `merge on`
only in a session where neither is armed, since a script another hook already
holds installs table-only, and its route line says so. Its structure is pinned by
`tests/test_soul_probe_contract.py`.

### The debugger breakpoints

Live procedure 3 also watched game scripts with the `x64dbg` MCP server
(hub `docs/tools/x64dbg-mcp.md`): non-breaking hardware logging breakpoints,
at most four at once, each logging its hit count and `self`. They answer which
game code runs, which a script hook cannot without a detour of its own, and a
detour would change the very bytes the breakpoint is set on. The table of
addresses lives on the researcher's machine, outside every repository (an
address is a research finding, never shipped); each row is re-checked against
the bytes the running game holds before it is set. By function name and
purpose:

- **Control: `CheckTalentUse`.** Runs many times a second in town. Logged
  first, and kept armed through the session, so a zero elsewhere counts only
  while it is rising in the same window.
- **Group A: `AddToInventory`, `InventoryGridAddToStack`, `ItemsAreStackable`.**
  During the key merge replayed under the debugger: whether the pickup reaches
  the add path and the Key tab's stack attempt (static reading 13), and whether
  `ItemsAreStackable` stays silent while a merge happens (static reading 14,
  Live procedure 1's zero from a detour).
- **Group A2: `GridAddToStack`** (in `ItemsAreStackable`'s place). During the
  pickup of five base items, pieces and plain: which stack attempt the bag
  makes for a consumable, and how often.
- **Group B: `Portal_obj`'s Alarm_0, `NetworkRoomGoto`, `RoomGoto`.** Around
  the travel through a piece's portal: which of them fire, and in what order
  (static readings 15 and 16). Armed again for the owner's own way back to
  town, as a control on the game's own travel.
- **Group C: `Pentagram_Boss_obj`'s Alarm_0, `LoadBossDeath`.** In the boss
  room: whether the pentagram's alarm brings the boss out (static reading 17),
  and whether the death script runs when the boss is killed there.

None of these scripts was to carry a research detour in that session (the
`craftprobe`, `prospectprobe`, script trace and pet relic trace tables stayed
unarmed), so each breakpoint would sit on the game's own first bytes.
**Measured**: all but one did. `RoomGoto`'s entry began with a jump where the
game's copy has its first instruction, so the debugger refused the breakpoint
on the byte check and `RoomGoto` was not watched (its hits are "not observed");
which hook placed that jump is not established.

Live procedure 4 (player build) watched two more, with the same control:
`ZoneStateExists` and `ZoneStateResetSingle`, around one transfer from a
piece, to see the shipped transfer reach both scripts independently of the
mod's own counters. Neither carries a ForgePact detour (density's detour on
`ZoneStateResetSingle` sits in `InstallDensityLifecycleHooks`, which nothing
in `ModuleMain.cpp` calls), and both entries matched the game's copy
(**measured**, Live procedure 4).

Live procedure 1 of the follow-up (`forgepact-197-piece-icon-stack`, research
build) plans these rows, with the same control and the same rule that the
table of addresses stays on the researcher's machine:

- **Control: `CheckTalentUse`**, in town, first; a zero elsewhere counts only
  while it logs in the same window.
- **`GridAddToStack` and `InventoryGridAddToStack`**, during each pickup of a
  key piece: which stack attempt a Key-tab item makes (the follow-up's reading
  1 expects `GridAddToStack`), for the plain control's merge and for a `frac`
  piece kept apart.
- **`CanUsePotion`**, during a right-click on the legacy bag piece (the
  control, since Live procedure 3 measured it reached) and then on a Key-tab
  piece: whether the Key tab's right-click reaches the use gate at all
  (reading 6).

All three are armed before any `soulprobe usewatch`, because a detour changes
a function's first bytes, the bytes a breakpoint is set on and checked
against; the `CanUsePotion` breakpoint is cleared before `usewatch allow`
installs the probe's detour there. The debugger detaches before the game is
stopped.

Live procedure 2 of the follow-up (research build, pieces that stack in the
bag) plans these rows, with the same control and the same rule:

- **Control: `CheckTalentUse`**, in town, first.
- **`AddToInventory`**: whether every pickup of a piece reaches the add path
  (bag-stack reading 2 and the follow-up's reading 1 put the consumable's stack
  attempt inside it).
- **`InventoryGridAddToStack`**: the script the merge rides. Its count across
  the merged pickups is compared with `soulprobe status`'s `mergecalls`: a
  logpoint on the script's entry sees the calls the inline route carries, so
  equal counts mean the detour saw every call compiled GML made, not only the
  calls through the script table.
- **`SaveInventoryOrderNew`**: whether the game writes the bag during play, at
  a pickup or at any step (bag-stack reading 5), since Live procedure 1's bag
  file was not rewritten at its stop.

All four are armed before `soulprobe merge on` or any `usewatch`, because the
merge's detour changes `InventoryGridAddToStack`'s first bytes.

## Results

Four sessions, all with slot 14 ("Sorak"), each restoring its own save backup
at the end. The captures live in the hub's workorder folder (local, not
tracked); a check name in brackets is the capture's. A negative is "not
observed" unless the session's control passed in the same window.

**Live procedure 1** (`forgepact-197-boss-soul-pieces-live-1`, 2026-10-11,
research build, 29 checks: 10 pass, 11 fail, 8 not observed). It measured the
first design and rejected two parts of it. `RoomActIndex` answered the act
itself (1 in `Town_01_rm` and `Act_01_01`, 2 in `Act_02_01`), not the act less
one. The Gem of Incarnation's `droprate.base` read 9,000, so a piece is 1 in
7,200, and a built gem's `itemSprite` read undefined, so a piece has no sprite
of its own. Pieces built on the Bifröst Key merged into the owner's own
Bifröst Key stack on pickup (its count went 39, 41, 43) and stayed merged
after a reload, while a both-route detour on `ItemsAreStackable` counted no
call. The owner's "PAS token" is the Edible PAS Logo: its right-click and
confirm reached `CanUsePotion`, `PotionUse` (true), `InvGridClearItemNode` and
the confirm action, and it was used up. A Grim Reaper created in `Act_02_01`
and brought to zero health crashed the game (its death script found no
pentagram), while a Karp King made the same way in `Act_01_01` died cleanly.
29 gem rolls in `Act_02_01` came from ordinary monsters, each a descendant of
`Enemy_Parent_obj`.

**Live procedure 3** (`forgepact-197-boss-soul-pieces-live-3`, 2026-10-11,
research build with the x64dbg debugger, 38 checks: 35 pass, 3 fail; the
debugger's `CheckTalentUse` control logged throughout). The base is category
11's `consumable_edible_pas_logo`, index 12. Five logos picked up (three Gurag
pieces, a Reaper piece, a plain logo) became five bag items with count 1 each,
and the debugger saw each pickup reach `AddToInventory` and then
`InventoryGridAddToStack`, never `GridAddToStack`. The key merge replayed
merged again (count 38 to 39) with `ItemsAreStackable` silent, but it did not
reach `InventoryGridAddToStack` either (the one research fail besides the
portal's). A piece's right-click reached `CanUsePotion` and `PotionUse`
carrying the piece, and answering `PotionUse` true made the game clear its
cell; the piece never showed the logo's own effect (the logo's own use opened
a portal to Sheeponia). A `Portal_obj` made for a piece showed and stayed, but
took the player to the zone's ordinary next zone, not the boss room
(`portal-travel` and `portal-travel-2` failed; "A portal to the boss room"
above): the travel ran the portal's Alarm_0 and then `NetworkRoomGoto` with
the same `self`, and the game's own town portal and waypoints reached
`NetworkRoomGoto` only. `NetworkRoomGoto` called by name with `Act_01_Boss` or
`Act_02_Boss` reached that room (`goto-travel`), where the pentagram's Alarm_0
brought out Gurag (HP about 1.57 billion), later the Grim Reaper; each was
brought to zero health through `enemy_hp`, `LoadBossDeath` ran, and the game
kept running, the Reaper included. No quest or act message showed. A zone in
which a piece's portal was opened loaded again on a revisit. After a relaunch
with the probe's dress off, a save holding a piece loaded and the piece read
as an Edible PAS Logo with its seed 777101 kept. `RoomActIndex` answered
`false` in `Act_01_Boss` and `Act_01_Pregame` and 2 in `Act_02_Boss` and
`Town_02_rm`. The owner then chose the direct transfer and a fresh boss on
every use.

```
item-route: consumable
use-route: potion-use
portal-route: goto
```

`item-route: consumable`: `base-consumable`, `piece-apart` and `reload-loads`
passed. `use-route: potion-use`: `use-control`, `use-reaches-game`,
`use-portal` and `use-takes-one` passed and `no-base-effect` did not fail.
`portal-route: goto`: the portal's travel failed and `goto-travel` passed, and
the owner chose the transfer over a portal (2026-10-11).

**Live procedure 2, attempt 2** (`forgepact-197-boss-soul-pieces-live-2`, its
second `## Checks` block, 2026-10-12, the player build, plugin sha256
`dbbcbb8a…63a0`; 25 checks: 20 pass, 2 fail, 3 not run). Attempt 1 aborted at
the debugger's control before any gameplay, so attempt 2 ran without the
debugger. With the mod on, the status line read `act=1 eligible=yes
gemrate=9000 chance=1/7200` in `Act_01_01` and `eligible=no(town)` in town.
Forced drops gave Pieces of Gurag's Soul, three of them picked up into three
bag cells across two bag tabs. With the switch off, kills dropped nothing and
a piece's right-click was refused (`use-off=1`) with the piece kept. With it
on, a right-click and confirm in `Act_01_01` moved the player into
`Act_01_Boss` within seconds, with no portal (`transfers=1`), and used one
piece. The status line's `left=Act_01_01 leftsaved=yes` was the positive
control for the mod's `ZoneStateExists` call. Gurag came out of the
pentagram, the owner killed him and left by the town portal key (V), and the
status line in town read `left=Act_01_Boss leftsaved=yes`: the game had kept
the boss room's state. A second piece used in town moved the player into
`Act_01_Boss` again, the transfer reading `saved=yes reset=yes after=no`
(`fresh=1 cleared=1`): the entry existed, and the reset removed it. Gurag
came out again (HP about 1.57 billion on both bars) [on-reentry-boss]. An
Act 2 piece used in `Town_02_rm` moved the player into `Act_02_Boss`, where
the Grim Reaper came out (HP about 1.93 billion). After a relaunch with the
switch off the save loaded, the kept pieces showing by name. The room value
the mod hands the zone-state calls is what `asset_get_index` returns (the
transfer line printed its kind, a reference), and with it `ZoneStateExists`
found the entries the game itself had saved (`leftsaved=yes`, `saved=yes`).
In `Act_01_Boss` every gem roll was refused as `no-act` (the room is outside
`RoomActIndex`'s switch), in `Act_02_Boss` as `boss-room`. The two fails are counter mismatches, not mechanism
failures: under `force on` every eligible kill drops a piece, and the owner
kept killing past the pieces asked for (`drops=17` against 3, and +10 against
+1); Live procedure 4 measured both drops again as "at least one". The three
debugger checks did not run (hub #497, since fixed by hub PR #501).

**Live procedure 4** (`forgepact-197-boss-soul-pieces-live-4`, 2026-10-12, the
same installed player build, x64dbg attached, its instrument proven by the
game's frame counter; 8 checks, 8 pass). In `Act_01_01`, forced, `drops=`
went from 0 to 42 and the owner picked up 42 Pieces of Gurag's Soul
[on-drop-act1]; in `Act_02_01` it went from 42 to 49, seven Pieces of the Grim
Reaper's Soul [on-drop-act2]; the tooltips read the piece names. One piece
used in `Act_01_01` moved the player into `Act_01_Boss`; between the use and
the next status line the debugger logged `ZoneStateExists`, then
`ZoneStateResetSingle`, then `ZoneStateExists` again, one argument each and
the same `self` [dbg-zonestate-exists] [dbg-zonestate-reset]. `fresh=0` and
`cleared=0` held, since the room had no saved state after the restored save.
`ZoneStateResetSingle` hit only on that transfer, while the game's own travel
(the town portal, the waypoint) hit `ZoneStateExists` several times itself,
so the game does call it, which the MCP's empty callers answer had left
unobserved.

Every graceful stop in all four sessions (two each in Live procedures 1, 3
and 2 attempt 2, one in Live procedure 4) ended with the process status
C0000409 (hexadecimal) after the plugin's `==== clean shutdown ====` line,
including Live procedure 2's relaunched game, which never had a debugger
attached; the cause is not attributed.

### The piece icon and stack follow-up

Four more sessions on 2026-10-12, slot 14 ("Sorak"), each restoring its own
save backup at the end; Live procedure 4 ran before Live procedure 3. The
captures are local to the hub's workorder folder, as above.

**Live procedure 1** (`forgepact-197-piece-icon-stack-live-1`, research DLL
sha256 `82fcd53f…811e9`, x64dbg with its `CheckTalentUse` control proven; 32
checks: 20 pass, 4 fail, 8 not observed). A built Gem of Incarnation carries
`Gem_Incarnation_spr` under item info key `"11"` [gem-sprite]. A key piece
with a whole `b` (the Bifröst Key's 2) merged into the owner's Bifröst Key
stack (38 to 39) with the `GridAddToStack` logpoint logging the pickup and
the `InventoryGridAddToStack` one not [merge-control] [dbg-merge-route], so
the key merge #197's Live procedure 3 could not attribute is
`GridAddToStack`'s. Key pieces with the act's tenths in `b` (2.10 for Act 1,
2.20 for Act 2) stayed apart from the real key, and a second Act 1 piece
joined the first through `GridAddToStack` [frac-apart-key] [frac-stack]
[frac-apart-act]; pieces with `c` 0.05 did the same [cfrac-stack]
[cfrac-apart]. The Key tab drew the pieces with the gem's icon and the real
key with its own [icon-keytab], and a class-11 bag piece drew the gem's icon
too [icon-legacy-bag]. On the ground a piece kept its base's own pickup
sprite (`Pickup_Bifrost_Key_spr`, `Pickup_Edible_Pas_Logo_spr`), never
`Pickup_Gem_Incarnation_spr` and never the error sprite [icon-ground].
Neither tooltip drew an icon; a key piece's tooltip named the piece and kept
the real key's type and description [icon-tooltip]. Right-clicking a key
piece in the Key tab opened no prompt, and the `CanUsePotion` logpoint stayed
at its count with the control logging, where the bag logo's right-click had
hit it once [keyuse-prompt] [dbg-keyuse-canuse]; with the probe's allow
armed, the right-click reached `InvGridEquipV2` twice and never the use
[keyuse-allow] [keyuse-potionuse]. No use ran, so the four stack-use checks
were not observed. The graceful stop did not rewrite `inventory_order_13.hss`,
so after the relaunch the Key tab held no piece and the four reload checks
were not observed.

```
stack-route: none
keyuse-route: not-reached
stackuse-route: none
icon-route: bag-only
```

The stack route reads none because its reload checks were not observed,
though every in-session fraction check passed; the use route reads
not-reached because `keyuse-allow` failed with `dbg-canuse-control` passing;
no stack use ran; and the icon route reads bag-only because `icon-keytab`
passed and the ground showed the base's own pickup sprite. The owner then
dropped the Key tab (2026-10-12): pieces stay bag consumables, used by
right-click and confirm, and pieces of one act stack in the bag.

**Live procedure 2** (`forgepact-197-piece-icon-stack-live-2`, research DLL
sha256 `21aa4ea0…3099`, x64dbg with its control proven; 27 checks: 22 pass, 1
fail, 4 not observed). With the merge off, two Act 1 pieces took two cells,
each pickup hitting `AddToInventory` and `InventoryGridAddToStack` once
[bag-no-stack-control] [dbg-pickup-route]. With the merge on, a third Act 1
piece joined the first (`o=1->2`, the game's result `success=false->true`,
the hash refreshed), nothing was left on the ground, and the Act 1 counts
summed to the three picked up [merge-stack] [merge-no-duplicate]; an Act 2
piece (`no-target`) and a plain logo (`not-piece`) kept cells of their own
[merge-apart-act] [merge-apart-real]. The game's result carried `tabNumber`,
`x`, `y`, `tabType` and `success` on every call. The `InventoryGridAddToStack`
logpoint and the detour's own call count both rose by three, so the detour
saw every call the logpoint did [dbg-merge-calls]. The bag drew the count
"2" on the stack's cell [stack-draw], and the tooltip named the piece and
showed no count [stack-tooltip]. A right-click on the stack opened the game's
prompt [stack-prompt]. With `PotionUse` answering true, the confirm cleared
the whole stack, both pieces [stackuse-all]. With the probe's take-one, a
stack of 3 went to 2 with `PotionUse` answering false, the prompt closed with
no message, the stack still moved to another cell, and the last piece
answered true and its cell cleared [stackuse-one] [stackuse-after-false]
[stackuse-move] [stackuse-last]. `SaveInventoryOrderNew` logged no call at
any pickup, merge or use [dbg-invsave], and the stop did not rewrite the bag
file [save-written], so the save and reload checks were not observed. That
session's save token (not-written) measured only that the game never saved
the bag, not that a stack is lost; Live procedure 4 replaced it.

```
merge-route: detour
bagstackuse-route: take-one
stackdraw-route: game
```

**Live procedure 4** (`forgepact-197-piece-icon-stack-live-4`, the same
research DLL, x64dbg with its control proven, every step the operator's; 17
checks: 16 pass, 1 fail). Six pickups built an Act 1 stack of 3, an Act 2
stack of 2 and a plain logo in a cell of its own [stacks-built], and none of
them saved the bag: no `SaveLocalFile`, `SaveInventoryOrderNew` or
inventory-window Destroy call, and the file unchanged [dbg-pickup-nosave].
The I key (and B) did not open the bag window, so the bag window's own close
never ran. Opening and closing the stash (`hs_stash_open`, `hs_stash_close`)
did save it: at the close the debugger logged the inventory window parent's
Destroy event once, `SaveLocalFile` seven times and `SaveInventoryOrderNew`
once, and `inventory_order_13.hss`, the character file and `stash.hss` were
rewritten within the same second [dbg-bagclose] [dbg-savelocal]
[dbg-bagsave] [save-written]. A fourth Act 1 piece picked up after that save
took the stack to 4 with no save; the graceful stop then rewrote the
character file but not the bag file, which still held 3 [stop-bag-write]
(hub issue #506). The file held the Act 1 key at `o` 3, the Act 2 key at `o`
2 and the logo's key [save-defs]. After a relaunch with the probe's dress,
icon and merge off and `soulpiece` never on, the bag read the same three
rows with the same counts [reload-stack-kept], and a new Act 1 piece took a
cell of its own beside the kept stack [reload-off-apart]. The reloaded stack
did not show as an Edible PAS Logo: its tooltip named the piece with its
lore line, and its cell drew the gem's icon with the count 3 [reload-base,
the one fail]. The plugin had registered its pieces' Custom Forge entries
(name, lore and sprite, matched by base and seed) at boot, which it does
whether `soulpiece` is on or not, so whether that look came from the save or
from the registration was not separated; no load ran with the plugin removed.

```
save-route: kept
save-trigger: stash-close
```

The save route reads kept because `save-written`, `save-defs` and
`reload-stack-kept` passed; the trigger reads stash-close because the bag
window never opened and the stash's open and close wrote first.

**Live procedure 3** (`forgepact-197-piece-icon-stack-live-3`, the player DLL
sha256 `f433bdd9…893c`, no debugger; 15 checks, all pass). `soulpiece 1`
installed the `PotionUse` and `InventoryGridAddToStack` detours, both routes
each [verb-on]. Forced drops gave two pieces in Act 1 and two in Act 2 (the
owner picked up one more Act 2 piece than asked), and the bag held two
cells, each drawing the gem's icon and the count 2, their tooltips naming a
Piece of Gurag's Soul and a Piece of the Grim Reaper's Soul [on-bag-stack]
[on-icon] [on-name] [on-apart-act]; the status line read `merged=2`, one per
act, read once after both acts [on-merged-count]. Right-clicking the Act 1
stack and confirming took the player to `Act_01_Boss` (`transfers=1`) and
left one piece in the stack [on-transfer] [on-takes-one]. With the mod off,
the Act 2 stack's use was refused (`use-off`) and the stack kept its 2
[off-use-refused]. Opening and closing the stash wrote the bag file
[save-written], which held the two pieces' keys with `o` 1 and 2
[save-defs]; after a relaunch with the mod left off, both stacks were back
with their counts and the pieces' names [reload-off-loads].

Every graceful stop of these four sessions also ended with process status
C0000409; the cause is still not attributed.

## Not established

- Whether a boss killed again after its room's state was dropped credits its
  act's quest or completion again (recorded, never changed); no quest or act
  message showed in any session.
- What else a boss room's saved state holds (loot left on its floor, the exit
  a kill leaves): nothing was left on the floor before a reset in any session,
  so whether it goes with the reset was not observed.
- What `ZoneStateLoad` restores for a boss room, and whether `zoneStates`
  survives a relaunch: Live procedure 2's first transfer of a fresh launch
  read `saved=no`, but no saved boss room was carried across a relaunch.
- Why a run-time `Portal_obj`'s `portalZone` record read back as the zone's
  next zone (the owner's reading is above). (Which code merged the key in
  Live procedure 3 is now measured: `GridAddToStack`, the follow-up's Live
  procedure 1, `dbg-merge-route`.)
- Which hook placed the jump on `RoomGoto`'s entry in the research build.

Beyond the sessions:

- Whether `DropGems` gives each socketable 1 in its rate or picks by weight;
  where it picks by weight, or applies a per-place override, the piece's ratio
  to the gem differs from five quarters there.
- Which room maps to which boss in the pentagram's alarm; the bosses of acts 3
  to 9 are not measured by any live procedure, act 8 opens with Odin's
  cutscene, and Cthulhu was not observed among the alarm's objects.
- Which key index each `keys_*` name holds, and what the values in
  `GetDungeonKeys`' list are.

The follow-up's open questions, as its four sessions left them (what they
answered is in § Results, "The piece icon and stack follow-up"):

- Whether a save keeps a `b` of 2.1 and a `c` of 0.05 as written: Live
  procedure 1's stop did not save the bag, and the Key tab route was dropped
  before another session reloaded one (the reading rests on the runtime's
  documented `string()`, not read).
- Whether `CanUsePotion` would answer true for a key: the Key tab's
  right-click never reached it (it reached `InvGridEquipV2`), so the question
  did not arise.
- What the item-info value the confirm compares with 6 means. That the
  confirm clears a whole stack on `true` is measured (Live procedure 2,
  `stackuse-all`).
- Whether the game changes the runtime's math epsilon from GameMaker's default.
- Which code a door or an NPC uses to find its key (Heimdall for the Bifröst
  Key, and the rest): not read, so whether a key piece could open what its base
  key opens is not established.
- Who else reads `GetUniqueRepoStruct` and `CanUsePotion` (empty through the
  MCP: not observed, not a negative).

Pieces that stack in the bag, as the follow-up's sessions left them (the
merge, the count drawn, the confirm on a stack, the take-one and the save
round trip are measured, § Results):

- Whether `AddToInventory` reads any field of the stack result but `success`:
  the merge writes `tabNumber`, `x` and `y` too, and the pickup then finished
  cleanly (Live procedure 2, `merge-no-duplicate`), but which fields it reads
  was not separated. Likewise which way round the bag's `inventoryTab` holds
  its cells, so whether the `x` and `y` the merge writes are the column and
  the row, was not read.
- Whether the bag window's own close saves the bag: its Destroy event asks
  for the bag file by the static reading, but Live procedure 4's I and B keys
  never opened the window (the owner opens it with C), so only the stash's
  close was measured, which reached the same Destroy logpoint.
- What decides whether a stop writes the bag file: none did in Live
  procedures 1, 2 and 4 (hub issue #506).
- What a piece or a stack shows, and what its use does, with the plugin
  removed: Live procedure 4's reload showed the piece's name and the gem's
  icon with the probe's dress off, while the plugin's own registration of the
  pieces was in place; no load ran without the plugin.
- The 999 cap and a merge into a full stack: not reached live.
- What `IsStackable`'s item-info value is (the bag-stack reading 1), and under
  what condition `InventoryGridAddToStack`'s later walk reads the bag's
  `inventoryTab` (reading 2).

The driver read the sessions' captures into these route tokens, which decided
what the player build ships (Live procedure 2's save token was replaced by
Live procedure 4's, read by the same rule):

- `merge-route: detour` when the merge-off control keeps two pieces apart and
  the merged pickup, no duplicate, another act apart and a real logo apart all
  pass; otherwise `merge-route: none`. If the merge-off control itself fails
  (two pieces stacked with the merge off), no token is set: the session then
  measured nothing about the merge.
- `bagstackuse-route: game-takes-one` when the game's own confirm leaves the
  stack one smaller; `bagstackuse-route: take-one` when it clears the cell and
  the take-one path (count lowered, `false` answered, the stack still moving,
  the last piece `true`) passes; else `bagstackuse-route: none`.
- `stackdraw-route: game` when the bag draws a count on the stack's cell; else
  `stackdraw-route: none`.
- `save-route: kept` when the bag file was written, holds the pieces' keys and
  counts, and a reload shows the stacks with their counts; `save-route:
  not-written` when the file was not written; `save-route: lost` when it was
  written but the keys or the reloaded counts are wrong.

The player build arms the merge only on `merge-route: detour`, `save-route:
kept` and a `bagstackuse-route` of `game-takes-one` or `take-one`, with either
`stackdraw-route`; any other combination goes back to the owner.
