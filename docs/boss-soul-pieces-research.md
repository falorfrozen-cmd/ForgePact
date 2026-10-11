# #197: boss soul pieces

Issue #197 asks for a drop that lets a player call an act's boss to them: an
ordinary monster in a zone of an act sometimes drops a **Piece of <boss>'s
Soul**, and using a piece summons that act's boss near the player. This
document holds the research behind the mod: why it is shaped the way it is,
what was read of the game before any live session, the routes that were turned
down, the research instrument (`soulprobe`) that Live procedure 1 runs, and what
is still not established. The mod itself (`soulpiece`, off by default) and the
results of the live sessions are recorded here once they exist.

Everything below is labelled. **Static reading** means read from the game's
compiled code in a local decompiler (the `ghidra` MCP, build `498d588550d8`,
2026-10-11) or from local byte scans of `Hero_Siege.exe` and `data.win` with a
positive control, and written here in our own words; no game code is quoted
(AGENTS.md § Legal). **Measured** means observed on the running game, and names
the session and check that observed it. **The owner's decision** is a choice the
owner made, not a fact about the game. A thing looked for and not seen is "not
observed", never "does not happen". The game facts are also recorded in the
hub's `docs/RUNTIME_DATA_MODELS.md` § 21, the shared reference every module
reads.

## Why

The issue's wording, and the owner's decisions of 2026-10-11, fix what a player
sees:

- **Which boss.** Act 1 zones drop Pieces of Gurag's Soul, Act 2 zones Pieces of
  the Grim Reaper's Soul, and so on for all nine acts; Act 7's boss is Mevius
  (the owner's decision). The act table lives in the decision core
  (`plugin/include/ForgePact/SoulPieceMod.hpp`, `kActs`).
- **How a piece is used.** Right-clicking it in the backpack outside a town, the
  way the owner uses a PAS token (the owner's decision). The boss appears near
  the player in the current room, and the stack is one piece shorter. A piece
  is never taken before the boss exists.
- **How often a piece drops.** A fixed chance a little above the Gem of
  Incarnation's, rolled where the game rolls that gem (the owner's decision):
  1 in D per gem roll, D = floor(G x 4 / 5), where G is the gem's
  `droprate.base` read at run time by its repository name. With G = 10,000
  (expected, not yet measured) a piece is 1 in 8,000. Everything that makes the
  game roll gems more often (kill density, the monster's own gem chance, a lever
  on the gem's rate) gives more pieces too; the piece adds no gate, no
  magic-find term and no slider of its own.
- **What a save holds.** A piece must load without ForgePact, so it is one of
  the game's own key definitions marked by a reserved seed, never a new item id
  (see the next two sections).

## Static reading

Read 2026-10-11; each item is a static reading unless it says otherwise. § 21
of the runtime reference holds the same facts in the shared form.

- **The act of a room.** `RoomActIndex(room)` sorts 175 rooms into nine acts and
  answers the act minus one; any other room answers undefined. The game itself
  uses it (its two direct callers, found by a byte scan whose positive control
  was `PickupLoot`'s known callers, load each act's texture pages). The mod asks
  it by name, so a room the game assigns to an act is in that act; the decision
  core adds only the name rule (no town, no `_Boss`, `Uber_` or `Dev_` room).
- **Stacking.** `ItemsAreStackable(a, b)` answers true only when the two items'
  class agrees and their definitions' `b` and `c` agree. The seed `a` and the
  count `o` are not compared. So pieces of one act stack on their own, and so
  would pieces of two acts sharing one base key, and a piece and an ordinary key
  of that base; the stack veto exists for that case. How the game reaches
  `ItemsAreStackable` is not observed: a byte scan for direct calls found none
  (the same scan found `PickupLoot`'s callers).
- **The item repository.** An item's definition names a category and an index
  into the global `itemRepoNormal`, and the game's lookup raises the runtime's
  array-index error for an index past a category's end. Inference, not
  measured: a saved item whose index is past the end cannot load without
  whatever added the entry. That is why a piece borrows an existing key's
  entry.
- **Keys.** Category 12 holds 44 entries, matching the 44 `keys_*` names in
  `data.win`. Which index holds which key is not established statically;
  `droprate list 12` and `soulprobe keys` print it. `GetDungeonKeys` builds a
  list of 26 entries from values not visible statically. The bag's Key sub-tab
  (`InventoryTabKey`) and a variable named `inventoryKeyGrid` exist; which
  object owns that grid is not established. A variable named `itemSprite`
  exists; what it controls is not established.
- **Act boss rooms.** None of the nine act boss rooms places a boss object; each
  places the pentagram that brings its boss at run time. So a summoned boss is
  created by us, not borrowed from a room. Gurag, the Grim Reaper and Mevius all
  descend from `Enemy_Child_Boss_obj` (hs-game-sdk's object table).
- **The backpack's consume path.** `UiAInventoryConsumeItemConfirm` (the answer
  to a "use this item?" prompt) asks `PotionUse` about the item, and only on a
  true answer clears the item's cells with `InvGridClearItemNode`.
  `CanUsePotion` reads the item and the room. No script is named for a
  right-click: the dispatch is inline. Whether a right-click on a key reaches
  any of these scripts is not established.
- **The PAS token.** No item of that name is found in `data.win` or the game's
  translation files; the one PAS-named item is a consumable. The owner's own
  token is the live control.
- **How the gem's rate reaches its roll.** `DropGems` runs only after the dying
  source's gem gate passed, and reads category 15's entries and their
  `droprate.base` (with per-place overrides in boss dungeons, the Chaos Tower,
  Niflheim, rifts and similar places). Whether it gives each socketable exactly
  1 in its rate, or picks by weight, is not established.
- **Already in the shared reference.** `LootGroundCreate` writes a fresh seed
  (§ 18.4); `InitItemFromJson` then `LootGroundCreateFromItem` keeps the
  definition, seed included (`sigdrop`, #74; relics, #124); Karp King and four
  uber bosses spawned in Act_01_01 were killed there through HP 0 with their
  drops, Uber Endrixia, Uber Anubis and Uber Luna do not die that way, and a
  boss spawned in a town removes itself (§ 13.1, measured; the spawn route is
  not named there); `sCP` is the game's own spawner (§ 20.5). Whether the nine
  act bosses live, fight and die when created outside their pentagram is not
  established (§ 21.2).

## Rejected routes, and why

- **New item ids.** Appending a piece to `itemRepoNormal` makes a save that
  cannot load without ForgePact (the repository reading above).
- **The Key of Terror parts** ("Gurag's Soul" and its kin, category 13): real
  crafting items; a piece stacking into one would become that part. Every piece
  name therefore starts "Piece of".
- **A consumable base** (category 11): the game's own use would work, but the
  piece would land in the potion grid, not the Key tab the issue asks for.
- **`LootGroundCreate` for placing a piece**: it writes a fresh seed, which
  would erase what marks the piece. The probe and the mod place through
  `InitItemFromJson` then `LootGroundCreateFromItem`.
- **A second detour** on a script ForgePact already detours (`DropGems`,
  `CreateItemNew`): a second inline detour on one script falls back to the
  table route, which compiled GML's direct calls never see. The probe's gem
  watch and dress are consumers inside the existing detours.
- **Hooking the Key tab's input itself**: the drag and the right-click are
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
- **Taking a piece out of the grid by hand with no game call**: measured only
  for materials, and a map entry left without its cell crashes the save
  (runtime reference § 17). `take clear` uses the game's own
  `InvGridClearItemNode`, by name.
- **#44's route for the summon**: `bossrarity` raises a boss's rarity and spawns
  nothing.
- **The suspend-the-loop class.** This mod is not one of the features
  [menu-pause-plan.md § 0](menu-pause-plan.md#0-recommendation-dont-build-this)
  recommends against: it creates one boss instance with the game's own creation
  call and changes nothing else, and the game runs the boss, its fight and its
  death as it runs any other. Nothing is paused, frozen or restored.

## The instrument

`soulprobe` is research build only (`plugin_build\build.bat dev`), never in the
player build's command list. Every reply line starts `soulprobe <sub>:`, so a
capture can be grepped. Each decision a line prints is the decision core's
(`ForgePact::SoulPieces`): the room rule and act (`RoomEligible`), the drop
verdict and D (`DecideDrop`, `PieceDenominator`), the use verdict (`DecideUse`)
and the stack veto (`VetoStack`). The probe only supplies what the game answers.

| Subcommand | What it does and prints |
|---|---|
| `status` | `base=`, `dress=`, `sprite=`, `stackhook=`, `stackroute=`, `usewatch=`, `usescript=`, `take=`, `gemwatch=`, `spawnroute=`, `offset=`, the counters `calls`, `vetoed`, `usecalls`, `usecallsby=` (calls per candidate, so a script called every frame shows during `log`, before `use` is armed on it), `uses`, `armed=`, `suppressed=`, `gemrolls`, `spawned`, and the gem detour's `gemroute=`. |
| `keys` | Every category-12 entry: its repository name, `droprate.base`, and what `GetKeyDungeon` answered when given the entry's index; then `GetDungeonKeys`' list, the `candidates=` a piece may borrow (not Basic or Crystal Key, not in the dungeon list, no `GetKeyDungeon` room), and `entries=`. Whether a candidate is a quest tool is the owner's read of the list. |
| `act` | The room's index and name, what `RoomActIndex` answered, the act, `town=`, `bossroom=` and the core's `eligible=`. |
| `base auto\|<keys_name>` | Chooses the base key by its repository name (`auto`: the first candidate). |
| `dress on\|off`, `sprite on\|off` | While on, a piece of the chosen base takes its act's name on `CreateItemNew`'s return (a consumer inside the shared detour), and with `sprite on` the Gem of Incarnation's sprite in `itemSprite`; the item's hash is refreshed. Off at startup. |
| `gem` | Builds one Gem of Incarnation through the loader, places nothing, and prints its `itemSprite`, the sprite's name, the gem's `droprate.base` (found by its repository name) and the D the core derives from it. |
| `place <act> [count]` | Builds a piece (class 12, the base, `c` 0, `j` 0, seed 777100 + act) through `InitItemFromJson` and places it with `LootGroundCreateFromItem`, the player as self, then reads the ground item's struct back. |
| `pickup` | `PickupLoot` on each ground piece, self the ground item and other the player (the companion's call shape with the player in the pet's place), and destroys the ground instance on a true return, as the game's callers do. |
| `keytab` | The Key tab: the object that owns `inventoryKeyGrid` (the first of `New_Inventory_Data_obj`, `Controller_obj`, `UI_Inventory_obj`, `UI_Stash_obj`, `Player_obj` that carries it), its cells, and one line per stack with its fingerprint, `b`, seed, count, name and act. |
| `stackhook off\|log\|veto` | A both-route detour on `ItemsAreStackable`, installed on the first `log` or `veto`. Each call (at most 200 lines per arming) prints both items and the game's answer; `veto` turns a true into false where `VetoStack` says so. |
| `usewatch off\|log\|use [auto\|<script>]` | A both-route detour on each use candidate (`UiAInventoryConsumeItemConfirm`, `CanUsePotion`, `PotionUse`, `PotionUseFlask`, `InvGridClearItemNode`, `InvGridEquipV2`, `ConsumeToken`, `UiASteamInventoryItemUse`, `UiAGuildStashConsumeTabToken`), installed on the first `log` or `use`, with one route line each. Each call prints its script, self, the item it was given and its answer, and its `depth=` among the candidates open on the thread (0: the outermost). The item is an argument that is an item struct, or one handed over as a grid cell or a fingerprint-shaped string, resolved through `GetItemFromFingerprint` by name (the line then says `via=fingerprint(...)`); when none is, every argument's kind is printed, with a string's text. In `use`, the first piece in the chosen script after each arming (`auto`: the first candidate whose printed line carried a piece, so the innermost of nested candidates) is treated with `DecideUse`: on `summon` the boss is spawned first, and only when it exists is one piece taken. Later piece calls before the next arming pass to the game untouched and are counted as `suppressed=`, which each `use` line reports, so a caller that runs every frame shows as a count, not as a row of bosses. |
| `take game\|clear` | `game`: the detour answers the call as the game answered the control's (the PAS token's) last call, so the game removes the piece itself. `clear`: the call goes to the game, then the probe lowers the stack's count, or clears a single piece's cell with `InvGridClearItemNode` by name. Each `use` line reports the stack's count half a second later. While `usewatch use` is on, `take` also re-arms it for one more piece. |
| `gemwatch off\|log` | A consumer inside the shared `DropGems` detour (installed through the gem mod's own installer once a player exists): per call, the source's object and id, whether it is a monster and a boss, the room and act, and the core's drop verdict with the mod taken as on. Nothing is placed. |
| `spawn <act\|object> [dx] [depth\|scp]` | Creates the act's boss (or an hs-game-sdk object, by name) at the player's x plus dx (default 1,200 px, mirrored when it passes the room's edge; refused in a town), by `instance_create_depth` or the game's `sCP`, then reads it back at 1, 5, 10 and 30 s: exists, health (its `enemy_hp` key read through `PC_GetVariableGMLWrapper`) and distance. |
| `spawnroute depth\|scp`, `spawnoffset <px>` | The spawn defaults the summon uses. |
| `kill <id>` | Sets that instance's `enemy_hp` record to 0 through `PC_SetVariableGMLWrapper`, by name (§ 13.1), and reads whether it is gone 10 s later. It destroys nothing. |

Every script is reached by its hs-game-sdk constant and every object by its SDK
name, resolved with `asset_get_index` at run time; the gem's sprite name is the
Python binding's (`hs_game_sdk.sprites`), since the C++ binding has no sprite
table. No address is stored. `craftprobe` and `prospectprobe` list some of the
same scripts among their research targets: arm `usewatch` and `stackhook` only
in a session where neither is armed, since a script another hook already holds
installs table-only, and its route line says so. Its structure is pinned by
`tests/test_soul_probe_contract.py`.

## Not established

- Whether a right-click on a key in the Key tab reaches any use candidate, and
  which; and whether the PAS token's own right-click does (Live procedure 1's
  `use-control` and `use-seen`). Static search found no script named for a
  right-click.
- What the game answers for a piece once a candidate is chosen, and whether
  answering as it answered the PAS token makes the game remove the piece
  (`use-take-game`), or whether the probe's own take is needed
  (`use-take-clear`), and whether a save still loads after it
  (`reload-mod-off`).
- The Gem of Incarnation's real `droprate.base` (expected 10,000), and whether
  `DropGems` gives each socketable 1 in its rate or picks by weight; where it
  picks by weight, or applies a per-place override, the piece's ratio to the gem
  differs from five quarters there.
- Whether `DropGems`' self is the dying monster (`gems-self`).
- Which object owns `inventoryKeyGrid`, what `itemSprite` controls, and whether
  a piece shows the gem's icon (`sprite-shows`).
- Which key index each `keys_*` name holds, what `GetKeyDungeon` answers for a
  key with no dungeon, and what the values in `GetDungeonKeys`' list are.
- Whether a boss created by `instance_create_depth` or `sCP` in an Act 1 or Act
  2 zone stays, fights and dies like a room-placed one (`spawn-gurag`,
  `spawn-reaper`, `spawn-scp`, `gurag-killable`), and what killing a summoned
  boss does to quests or act completion (recorded, never changed). The bosses
  of acts 3 to 9 are not measured by either live procedure, and acts 4 and 5's
  bosses are an inference from the Key of Terror's parts (§ 21.2).
- Whether the game, with ForgePact gone, merges one-base pieces of different
  acts into the first stack's act, as the stacking reading predicts.
