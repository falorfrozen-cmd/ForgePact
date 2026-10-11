# #197: boss soul pieces

Issue #197 asks for a drop that takes a player to an act's boss: an ordinary
monster in a zone of an act sometimes drops a **Piece of <boss>'s Soul**, and
using a piece opens a portal to that act's boss room. This document holds the
research behind the mod: why it is shaped the way it is, what was read of the
game, the routes that were turned down (two of them by a live session), the
research instrument (`soulprobe`) and the debugger breakpoints Live procedure 3
runs, and what is still not established. The mod itself (`soulpiece`, off by
default) and the results of the remaining live sessions are recorded here once
they exist.

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
  owner's decision). After Live procedure 1, the owner chose a portal over a
  boss beside the player: using a piece opens a portal beside the player to
  that act's boss room (`Act_01_Boss` for Gurag, and so on), and the piece is
  used up only when the portal opened. The player walks through, and the boss
  room's own pentagram brings the boss out and handles its death.
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
  key: see "Rejected routes" for why the key and its Key tab went.

## Static reading

Read 2026-10-11; each item is a static reading unless it says otherwise. § 21
of the runtime reference holds the same facts in the shared form.

- **The act of a room.** `RoomActIndex(room)` sorts 175 rooms into nine acts;
  any other room answers undefined. **Measured** (Live procedure 1): the answer
  is the act itself (1 in `Town_01_rm` and `Act_01_01`, 2 in `Act_02_01`); the
  first reading's "act minus one" was wrong, and the core's
  `ActOfRoomActIndex` now takes the answer as the act. The game itself uses the
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
  it placed as a new one.
- **What a stack compares** (static reading 14). Both stack attempts walk their
  grid and merge into an item whose class, `b` and `c` match, comparing the
  three fields themselves; neither calls `ItemsAreStackable`, and neither looks
  at the seed. `GridAddToStack` first accepts only classes 12 to 15, so a
  class-11 consumable never stacks in the bag. `c` picks the normal or the
  unique repository, so it cannot mark a piece.
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
- **Already in the shared reference.** `LootGroundCreate` writes a fresh seed
  (§ 18.4); `InitItemFromJson` then `LootGroundCreateFromItem` keeps the
  definition, seed included (`sigdrop`, #74; relics, #124); Karp King and four
  uber bosses spawned in Act_01_01 were killed there through HP 0 with their
  drops (§ 13.1); `sCP` is the game's own spawner, and it stamps the spawned
  flag portals carry in `pSpwd` (§ 20.5); the protected store faults on a key
  outside it (§ 11.2).

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
  A load-safe key of our own is filed as research (ForgePact #210).
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
  belongs to its room; a portal takes the player there instead. Neither the
  probe nor the mod creates a boss, nor a pentagram to satisfy the death
  script.
- **The raw `room_goto` builtin.** § 13.1 measured one to an uber room closing
  the game; the game's own `RoomGoto` sets the room up first (static reading
  16). Travel is the game's: through the portal, or, as the fallback Live
  procedure 3 measures, `NetworkRoomGoto` by name.
- **The other portal objects** (static reading 18). `Portal_Uber_Boss_obj`
  takes its target from uber state, not a room we set; `Portal_Waypoint_obj`
  opens the waypoint menu; the town, dungeon-exit and hell portals carry their
  own purpose. `Portal_obj` is the general one whose target is a room.
- **Duplicating a zone's own portal.** Duplicating reward portals caused endless
  loading (§ 15 of the runtime reference); a piece makes one fresh `Portal_obj`
  through the game's spawner instead.
- **`CanUsePotion` or the confirm action as the use.** `CanUsePotion` runs at
  the right-click, before the confirm, so a portal would open on a right-click
  the player then cancels; the confirm action's argument is an array, not the
  item. `PotionUse` carries the item, and its true answer is what makes the game
  clear the item's cells.
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
  recommends against: it makes one portal with the game's own spawner and
  changes nothing else, and the game runs the travel, the boss, its fight and
  its death as it runs any other. Nothing is paused, frozen or restored.

## The instrument

`soulprobe` is research build only (`plugin_build\build.bat dev`), never in the
player build's command list. Every reply line starts `soulprobe <sub>:`, so a
capture can be grepped. Each decision a line prints is the decision core's
(`ForgePact::SoulPieces`): the act of a room (`ActOfRoomActIndex`), the room
rule (`RoomEligible`), the drop verdict and D (`DecideDrop`,
`PieceDenominator`), the use verdict (`DecideUse`) and the portal's target
(`PortalTarget`). The probe only supplies what the game answers. Replan 1
removed `stackhook`, `spawn`, `spawnroute`, `spawnoffset`, `take` and `sprite`,
with their state: the stack veto, the boss beside the player, the probe's own
take and the gem's sprite are all rejected above.

| Subcommand | What it does and prints |
|---|---|
| `status` | `base=`, `category=`, `dress=`, `usewatch=`, `usescript=`, `gemwatch=`, `portalroute=`, `portaloffset=`, the counters `usecalls`, `usecallsby=` (calls per candidate, so a script called every frame shows during `log`, before `use` is armed on it), `uses` (pieces the chosen script was handed in `use`), `portals` (portals opened, by `portal` or a use) and `gemrolls`, and the gem detour's `gemroute=`. |
| `keys` | Every category-12 entry: its repository name, `droprate.base`, and what `GetKeyDungeon` answered when given the entry's index; then `GetDungeonKeys`' list, the keys no dungeon names (`candidates=`), and `entries=`. |
| `act` | The room's index and name, what `RoomActIndex` answered, the act the core takes from it, `town=`, `bossroom=` and the core's `eligible=`. |
| `base <repository name>\|auto` | Chooses the base by its repository name, looked up in category 11 (consumables), then 12 (keys); `auto` is the core's `consumable_edible_pas_logo`. Prints its category and index. Never an index constant. |
| `dress on\|off` | While on, a piece of the chosen base takes its act's name on `CreateItemNew`'s return (a consumer inside the shared detour), and the item's hash is refreshed. Off at startup. |
| `gem` | Builds one Gem of Incarnation through the loader, places nothing, and prints its `itemSprite`, the gem's `droprate.base` (found by its repository name) and the D the core derives from it. |
| `place <act> [count]` | Places `count` pieces (each its own item: the base's class, the base, `c` 0, `j` 0, `o` 1, seed 777100 + act), each built through `InitItemFromJson` and placed with `LootGroundCreateFromItem`, the player as self, then reads the ground item's struct back. |
| `place plain [count]` | The same for the base itself with an ordinary seed (a random whole number outside 777000 to 777999), undressed: the control a piece is compared against. |
| `pickup` | `PickupLoot` on each ground item of the base (pieces and plain), self the ground item and other the player (the companion's call shape with the player in the pet's place), and destroys the ground instance on a true return, as the game's callers do. |
| `keytab` | The Key tab: the object that owns `inventoryKeyGrid` (the first of `New_Inventory_Data_obj`, `Controller_obj`, `UI_Inventory_obj`, `UI_Stash_obj`, `Player_obj` that carries it), its cells, and one line per item with its fingerprint, `b`, seed, count, name and act. |
| `bag` | Every bag item of the base's class and `b`: a summary line with `route=` (which variable was read), the cells and the items, then one line per item with its fingerprint, class, `b`, seed, count, cells, name and act. Which variable holds the main bag grid is not established, so it tries `inventoryTab` and `inventoryData` on the same owners as the Key tab (a grid, or a grid per bag tab), then the open bag window's grid nodes, and says which it read and what the others held. |
| `usewatch off\|log\|use [auto\|<script>]` | A both-route detour on each use candidate (`UiAInventoryConsumeItemConfirm`, `CanUsePotion`, `PotionUse`, `PotionUseFlask`, `InvGridClearItemNode`, `InvGridEquipV2`, `ConsumeToken`, `UiASteamInventoryItemUse`, `UiAGuildStashConsumeTabToken`), installed on the first `log` or `use`, with one route line each. Each call prints a `usecall` line: its script, self, the item it was given, whether that item is a piece, its answer, and its `depth=` among the candidates open on the thread. The item is an argument that is an item struct, or one handed over as a grid cell or a fingerprint-shaped string, resolved through `GetItemFromFingerprint` by name. In `log`, every call goes to the game, pieces included. In `use`, a piece in the chosen script (`auto`: the first candidate whose line carried a piece) is never handed to the game: the core's `DecideUse` decides; on `portal` a portal opens as `portal` makes it, and the call answers as the game answered the control (the plain base) on that script, true when no control was seen, so the game clears the piece's cell itself; a refusal or a failed portal answers false, and the confirm stops with the piece kept. A `use` line names the act, the script, the portal (or the refusal or failure), the target, the answer and whether a control was seen. Every other call passes to the game. |
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
`prospectprobe` list some of the same use candidates: arm `usewatch` only in a
session where neither is armed, since a script another hook already holds
installs table-only, and its route line says so. Its structure is pinned by
`tests/test_soul_probe_contract.py`.

### The debugger breakpoints

Live procedure 3 also watches game scripts with the `x64dbg` MCP server
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

None of these scripts carries a research detour in that session (the
`craftprobe`, `prospectprobe`, script trace and pet relic trace tables stay
unarmed), so each breakpoint sits on the game's own first bytes.

## Not established

Live procedure 3 asks:

- Whether `consumable_edible_pas_logo` is the logo's repository name (category
  11, index 12 in Live procedure 1's session) (`base-consumable`).
- Whether pieces of the logo stay apart in the bag, one per cell, from the
  plain logo and from each other (`piece-apart`), which stack attempt the
  pickup makes for them (`dbg-consumable-route`), and whether a save holding a
  piece loads with the probe's dress off, showing the logo's own name
  (`reload-loads`, `reload-base-name`).
- Whether the key merge goes through `AddToInventory` and
  `InventoryGridAddToStack` with `ItemsAreStackable` silent
  (`dbg-merge-route`, `dbg-stackable-unused`).
- Whether a piece's right-click reaches `CanUsePotion` and `PotionUse` as the
  logo's does (`use-control`, `use-reaches-game`), whether answering
  `PotionUse` for a piece as the game answered the logo makes the game take the
  piece (`use-takes-one`), and whether the logo's own effect shows for a piece
  (`no-base-effect`; whether `PotionUse` applies the effect is not read).
- Whether a `Portal_obj` made beside the player shows, stays and can be entered
  (`portal-visible`, `portal-stays`, `portal-travel`), which of its records it
  needs set beyond `portalZone`, which game scripts carry the travel
  (`dbg-travel-route`, `dbg-travel-control`), and what `NetworkRoomGoto` by
  name does on its own (`goto-travel`).
- Whether the boss room brings its boss out through the pentagram's alarm when
  the player arrives by a piece's portal, and whether Gurag and the Grim Reaper
  die there without a crash (`boss-arrives`, `dbg-boss-from-pentagram`,
  `boss-dies`, `dbg-boss-death-path`, the Act 2 checks); what such a kill does
  to quests or act completion (recorded, never changed).
- Whether a portal opened in a zone, left unused, leaves the zone loadable when
  the player comes back (`portal-revisit`).

Beyond Live procedure 3:

- Whether `DropGems` gives each socketable 1 in its rate or picks by weight;
  where it picks by weight, or applies a per-place override, the piece's ratio
  to the gem differs from five quarters there.
- Which room maps to which boss in the pentagram's alarm; the bosses of acts 3
  to 9 are not measured by any live procedure, act 8 opens with Odin's
  cutscene, and Cthulhu was not observed among the alarm's objects.
- Which key index each `keys_*` name holds, and what the values in
  `GetDungeonKeys`' list are.
