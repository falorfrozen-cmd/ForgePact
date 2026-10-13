# Flight charms (ForgePact #158): research phase

**Question.** Three charms that grant a new **Flight** modifier (Pocket Elf,
Gabriel Wings, Witch's Broomstick) need three things nobody here knows yet.
What stops the player's walk at a prop, and does answering the player's own
collision queries "nothing there" let a walk cross it? Can a forged charm take
a 2x2 or 1x5 footprint, and what does a save do when that footprint appears or
goes away? And which of the game's rolls drops a Heroic unique?

**Why it matters.** Issue #158 asks for flight as a charm modifier: while one
of the three charms sits in the charm grid, the player passes over scenery.
Jump through scenery (#16, `jump-scenery-research.md`) answered the blocking
question for the jump only. A jump is a 104-frame window decided at take-off,
and a walk into the same prop stayed blocked under that research's `all hold`
lever. Flight is continuous and can end anywhere, so the walk is the case that
matters. The two larger charms want footprints no normal charm has, and the
Custom Forge has only ever dressed an item's stats, name and rarity, never its
size or its art. So this phase extends the research build's `jumpprobe` with a
walk lever and an object tally, adds a research-build `charmprobe`, runs one
live session, and records a `## Decision`. The mod itself (three Custom Forge
entries, a flight lever in the player build, drop entries, panel switches that
are off by default, the `NATIVE_BOOLEANS` oracle entries and a release-notes
line) is phase 2, written against that decision.

**Posture.** Everything here is measured runtime behaviour, a reading written
in our own words, or our own code. Game objects and scripts are named by their
`hs-game-sdk` names and indices. This doc carries no decompiled script text, no
address and no struct offset. A reading is labelled as a reading, and it is not
a fact until a live session records it.

**Out of scope for this phase.** The mod itself and every phase 2 design
choice; final art or the charms' own sprites (the sprite probe uses one
placeholder icon we drew); the Item Editor and its signature item list;
co-op; enemies' movement; leap, charge and blink skills; moving the player
from code (`playerwarp` is the operator's recovery tool only); and anything
that suspends the game's own loop.

## Status

- **Phase:** research (phase 1 of 2). Nothing in this phase is
  player-visible, and the player build is free of it.
- **Instruments:** the extended `jumpprobe` and the new `charmprobe`
  (§ Instrument), research build only, on the ForgePact branch
  `158-flight-charms`; during the walk checks, x64dbg logpoints the live
  operator sets by name (§ Instrument, "The debugger logpoints").
- **Static reading:** done on 2026-10-11 through the ghidra MCP and a local
  scan for the names a function's code references (§ Static reading). The
  readings about the game are also in the toolkit's
  `docs/RUNTIME_DATA_MODELS.md`, § 19.6 and § 23, labelled as static readings.
- **Live procedure 1:** run on 2026-10-12 (§ Results, "Live 1 results"):
  the five validity checks passed; 21 of 26 checks pass, 3 fail and 2 are
  not-observed. The measured facts about the game are also in
  `docs/RUNTIME_DATA_MODELS.md` § 19.6 and § 23, relabelled as measured.
- **Decision:** recorded from Live 1 (§ Decision). `dress-added` reads
  `item-missing`, the value added for the outcome S17 measured: the dressed
  charm missing after the reload, with no crash and no overlap.
- **Later phases:** phase 2, the mod, is planned from this phase's Decision
  and built (§ Phase 2: the mod). Its Mod live 1 ran on 2026-10-12 (21 pass,
  5 fail, 1 not-observed) and Mod live 2, on the fixed build, on 2026-10-13
  (24 of 24 pass): § Phase 2: the mod, "Mod live 1 results", "Mod live 2
  results" and "Status and what is not established".

## Static search

Searched on 2026-10-11 over `hs-game-sdk/cpp/include/hs_game_sdk/scripts.hpp`,
`objects.hpp`, the Python name tables (`hs_game_sdk.scripts`,
`hs_game_sdk.objects`) and every `ForgePact/docs/*.md`, for names containing
`fly`, `flight`, `wing`, `float`, `hover`, `phas`, `charm` and `size`, and
for the movement and roll scripts the jump research and the Angelic roll
research already named. Code spells each name through its SDK constant; each
index below is that constant's index.

**Flight-shaped names.** No script or object named for flying is a player
movement mechanism.

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_StatPhasing` | 3390 | the getter behind item stat 26, "Movement Phasing" (walk through monsters); reached from `ReturnSpecificStat` (3344) |
| `gml_Script_GetWingSprite` | 1797 | cosmetic wings (`playerWings`) |
| `gml_Script_ReturnWingsInfo` | 3467 | cosmetic wings |
| `gml_Script_CA_playerWings` | 323 | the co-op relay of the wings choice, by its `CA_*` family (**name only**) |
| `gml_Script_UiSetFloating` | 4472 | the interface's floating state (`floating`), not the player's |
| `gml_Script_CA_addFlyingProjectile` | 220 | a projectile (**name only**) |
| `Pyromancer_Phoenix_Flight_obj` | 3867 | a Pyromancer skill effect (**name only**, not read) |
| `Easter_Egg_Wings_Bat_obj`, `Easter_Egg_Wings_Fairy_obj` | 1385, 1386 | wing cosmetics, by name (**name only**, not read) |

**Movement and collision.**

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_PlayerMovement` | 2774 | the player's movement step (§ Static reading) |
| `gml_Script_CanMove` | 467 | a movement check; calls `InstancePlaceTallest` |
| `gml_Script_InstancePlaceTallest` | 599 | the tallest instance at a place (§ Static reading) |
| `gml_Script_playerJumpGravity` | 2764 | the jump's per-frame script (RDM § 19.1, measured) |
| `gml_Script_ClassStepEvent` | 561 | the class step; the static reading put six `InstancePlaceTallest` sites here, and Live 1 found them in the player object's Step event instead (§ Results) |
| `gml_Script_TilePlaceMeeting` | 3866 | a tile row of `jumpprobe` |
| `gml_Script_CheckTalentUse` | 539 | `jumpprobe`'s per-frame positive control (RDM § 7.2) |

**Charms and item size.**

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_GetPlayerCharmData` | 493 | the equipped charms of a player (§ Static reading) |
| `gml_Script_UpdateCharmData` | 491 | rebuilds that list from the charm grid |
| `gml_Script_NetworkSendClientCharmData` | 492 | the co-op send of it (**name only**) |
| `gml_Script_RunItemEquipped` | 3483 | a caller of `UpdateCharmData` |
| `gml_Script_GetItemSize` | 1976 | an item's footprint in grid cells |
| `GetItemInfo@anon@5277@s_ItemInstanceStruct@InventoryV2Funcs` | 2035 | the item method `GetItemSize` reads through |
| `gml_Script_GetItemStats` | 1682 | stat aggregation; reads the equipped charms |
| `gml_Script_LoadSpecificStat` | 2296 | stat aggregation; reads the equipped charms |
| `gml_Script_DefineItemNormalCharms` | 832 | the normal charm definitions (**name only**) |
| `gml_Script_DefineItemUniqueCharms` | 904 | the unique charm definitions (**name only**) |
| `gml_Script_DropCharms` | 1326 | a charm drop (**name only**) |
| `gml_Script_DropAngelicCharm` | 1306 | an Angelic charm drop (**name only**) |
| `gml_Script_GetUniqueCharm` | 1130 | a unique charm pick (**name only**) |

**Unique rolls.**

| Name | SDK index | Role (reading) |
| --- | --- | --- |
| `gml_Script_DropUniqueItems` | 1424 | the ordinary unique roll, from `LoadDrops` (2171) |
| `gml_Script_DropItemHeroic` | 1374 | a unique roll that skips Angelic and Unholy |
| `gml_Script_DropItem` | 1364 | the Angelic roll (RDM § 13.4) |
| `gml_Script_GetUniqueRepoStruct` | 819 | the unique repository, rarity in field 27 |
| `gml_Script_CreateDefaultParams` | 2349 | the drop's parameters, called by both rolls |
| `gml_Script_VaultUse` | 4597 | a caller of `DropItemHeroic` |

**What the jump research already answers.** During a jump, the player's own
`position_meeting`, `place_meeting`, `instance_position` and `collision_line`
(each naming `Collision_Parent_obj` itself, 957) and `collision_circle`
(naming `Wall_Parent_obj`) decide the crossing, and answering them "nothing
there" lets the jump cross a prop (RDM § 19.2 and § 19.3, measured). The
proven route is `HookBuiltin` on those builtins, a family test through
`object_is_ancestor` by name, and gates and locks kept blocking by running the
original against `Gate_Parent_obj` and `Lock_obj`. What it does not cover:
the walk stayed blocked under `jumpprobe pass 1 ... all hold` (RDM § 19.2,
measured), while two things ran unanswered during that walk. One was
`InstancePlaceTallest`, with 1944 player-self calls (the `scripts` flag that
answers it was not set). The other was 6252 player-self `place_meeting` calls
naming an object outside the family, which was never named. A walk with
`all scripts hold` was never run. What the game does with a player left
inside a prop is not established (the jump research's `valid-landing:` is
`not-observed`), and map edges, buildings and gates were not tried on foot.

## Static reading

Read on 2026-10-11 in the build Ghidra imported (the one
`py -3 tools/decomp_index.py build-hash` names), through the ghidra MCP and a
local scan for the names a function's code references. A name scan lists
candidate direct call sites. It is blind to calls through the script table or
a method value, so a call it did not find is not observed, not absent. The
same readings are in the toolkit's `docs/RUNTIME_DATA_MODELS.md` § 19.6 and
§ 23.

**Walking.**

- `PlayerMovement` does not decompile within the tool's limit; it is roughly
  200 KB of compiled code. Read as the names its code references, it uses
  `place_meeting`, `position_meeting` and `collision_circle`, the path
  builtins, `point_distance`, `instance_nearest` and `object_is_ancestor`,
  and the variables `x`, `y`, `hSpeedFactor`, `vSpeedFactor`, `moving`,
  `mountId`, `inputState` and `pSt`. The scan found no reference to
  `instance_place`, `tilemap_get_at_pixel`, the `mp_grid_*` builtins or
  `move_and_collide`, and it found no direct call from it to `CanMove` or
  `InstancePlaceTallest`. **Static reading.**
- `InstancePlaceTallest` gathers the instances at a place through
  `instance_place_list` into a list it reuses, and answers the one with the
  largest `height`, or `noone` when there is none. Its direct callers are
  `CanMove`, `playerJumpGravity` and six sites in `ClassStepEvent`.
  **Static reading.** Corrected after Live 1: the six sites are in
  `gml_Object_Player_obj_Step_0`, the player object's Step event, which
  follows `ClassStepEvent` in the image and which the symbol dump does not
  name (§ Results, "The walk's callers").
- What this means for the lever: the walk's own test is most plausibly the
  three meeting builtins it references, the ones the jump lever already
  answers. `all` did not free the walk. Candidates include a query naming an
  object outside `Collision_Parent_obj`'s family (the 6252 calls above) and a
  script outside `PlayerMovement` (such as `InstancePlaceTallest`, with its
  1944 calls); a query whose self is not the player, a builtin the scan did
  not list, and the runtime's own solid-object collision handling are not
  excluded. What holds the walk is **not established**. The walk lever below
  tests the first two candidates in one session, and the debugger logpoints
  of Live procedure 1 name the callers of `InstancePlaceTallest` and
  `CanMove` during the same walks; a walk that stays held under every lever
  leaves the other routes open. **Reading.**
- `StatPhasing` is the getter behind item stat 26, "Movement Phasing" ("you
  walk through monsters", in the Item Editor's stat catalog), and the only
  caller found is `ReturnSpecificStat`. So the game has a native
  pass-through-monsters stat; a name search found no pass-through-terrain one
  (not observed, not shown absent). **Static reading**; what stat 26 does on a
  character is not measured.
- `playerWings` (cosmetic wings, through `GetWingSprite` and
  `ReturnWingsInfo`) and `floating` (the interface's floating state, through
  `UiSetFloating`) are not flight mechanisms. **Static reading.**

**Equipped charms.**

- Offline, `GetPlayerCharmData` answers, for player number p, the
  `localCharmItemArray` of `Controller_obj.inventoryData` at index p - 1, the
  entry the relic tab lives in (RDM § 2, where the key rule is measured).
  Online it answers the player's `charmItemArray` on `Client_obj.players`.
  **Static reading.**
- `UpdateCharmData` rebuilds that array. It walks the entry's
  `inventoryCharmGrid` nodes and takes each distinct `nodeFingerprint` once,
  so a charm covering several cells is counted once. It resolves each
  fingerprint to its item through `GetOnlinePlayerItemOwner` and
  `GetItemFromFingerprint` and pushes the item. Its direct callers are
  `RunItemEquipped` and a `Client_obj` closure. **Static reading.**
- `GetPlayerCharmData`'s direct callers are `GetItemStats` and five sites in
  `LoadSpecificStat`, so the stat aggregation reads the equipped charms.
  Whether a stat the Custom Forge writes on a built charm counts is **not
  measured**: the Miner's Helmet's forged stats were never read off the
  character. **Static reading** for the callers.

**Item size.**

- `GetItemSize` asks the item's own `GetItemInfo` method twice, each time
  with a constant key and one shared default, and answers the two values as
  the item's footprint. `GetItemInfo` answers the item's `itemInfoStruct`
  entry for the key, or the default when it is undefined. So an item's
  footprint is read off the built item's `itemInfoStruct`, the struct the
  Custom Forge already writes (`"27"`, `"28"`, `"29"`, `"32"`). **Static
  reading.**
- Which two keys hold the footprint is **not established**: the reading did
  not render the constants. `charmprobe` identifies them from samples.
- `GetItemSize` has 38 direct callers across the grid scripts, among them
  `InventoryGridAddItem`, `GridHasSpace`, `InventoryGridHasSpace` and its
  `Multi` form, `GridAddItem`, `InventorySortTab`, `InvGridEquipV2`,
  `InventorySwapItemsNew`, `ProcessInventoryGridInput` and `CraftEditGrid`.
  Whether every reader of an item's size goes through it is **not
  established**. **Static reading.**
- A save keeps only an item's compact definition (RDM § 16.1, measured), so a
  footprint written into the built struct is not saved and has to be written
  each time the item is built. What the saved grid does when a footprint
  appears or goes away is what `dress-removed:` and `dress-added:` measure.

**Unique rolls by rarity.**

- `DropUniqueItems`, called from `LoadDrops`, picks one entry of
  `Controller_obj.lootListUnique` at a sub-list from 0 to 4, taken from a
  protected zone value with a random chance of stepping up. It keeps the
  entry only when its repository rarity (`GetUniqueRepoStruct`, field 27) is
  below Angelic (7) and it is not flagged hidden, then calls
  `CreateDefaultParams` and the placement. **Static reading.**
- `DropItemHeroic` takes a position and a sub-list index that defaults to 5.
  It draws from that sub-list until an entry is unflagged and is neither
  Angelic (7) nor Unholy (10), so it can give a Heroic or a Satanic unique,
  then calls `CreateDefaultParams` and the placement. Its direct callers are
  `VaultUse` and a Chaos Tower NPC closure. ForgePact has never called it.
  **Static reading.**
- The Angelic roll keeps only rarity 7 or 10 (RDM § 13.4). So of the three
  rolls, only `DropItemHeroic` can drop a Heroic (9) unique. **Static
  reading.**

**Not readings: the Item Editor's catalog** (`hs_full_catalog.json`, in the
owner's checkout of the Item Editor; not a reading of the game and not
measured).

- Normal charms are Small (`b` 0-19, 1x1), Large (20-39, 1x2) and Grand
  (40-59, 1x3). No normal 2x2 or 1x5 charm exists.
- 2x2 uniques include the Angelic melons (`b` 47-50) and Goburin's Head
  (Unholy, 98). The only 1x5 is Soul Collector (Unholy, 91). The Heroic 1x1
  charms are Torstein's Anvil (52), Wizard's Dice (82) and Gladiator's Dice
  (83).
- Aimbot.exe `{10, 0, 76}` is listed as Satanic, not Heroic.
- Item stat ids: 25 Increased Movement Speed (%), 159 Physical Damage Taken
  Reduced by (%), 160 Magic Damage Taken Reduced by (%), 162 a flat Damage
  Taken Reduced by, from all sources. `hs_game_sdk.stats.StatId` numbers stats
  another way (it gives 19 for movement speed), so it is not used for item
  stats.

## Instrument

Both instruments are research build only (`#ifndef FORGEPACT_RELEASE`), are
not player commands, and reach the game by name only: every script and object
through its `hs-game-sdk` constant, every builtin by its name, no address. The
module guide's Command Reference carries each form's exact output; this
section says, per form, what it reads or answers, by which route, and what
its positive control is. Neither instrument moves the player: `playerwarp`
is the operator's recovery tool, not a probe behaviour.

### The walk lever, in `jumpprobe`

The jump research built `jumpprobe` (`jump-scenery-research.md` § Instrument):
twenty script rows detoured at their own address and ten collision builtins
hooked by name, a per-row call count, and the `pass` lever, which answers the
local player's own collision queries "nothing there" without running them.
This phase adds three things to it, in `JumpSceneryProbe.hpp` and the
`jumpprobe` adapter. The shipped `jumpscenery` mod is not touched.

- **`pass 1 [frames] any [scripts] [hold]`, a third rule.** `props` answers
  a query naming a prop, `all` one naming anything in the collision family
  (`Collision_Parent_obj` or a descendant, decided through
  `object_is_ancestor` by name). `any` answers every query the local player
  makes on a hooked builtin row, whatever object it names: an object outside
  the family, an argument that names no object, and the two object-less rows
  (`place_free`, `tilemap_get_at_pixel`). It reads no object argument at all.
  `scripts` and `hold` work as before, and `pass stat` names the rule in
  force. Route: the same builtin detours; a call by any other `self` runs the
  original. Positive control: `passed=` on `pass stat` climbs while the
  player walks into the prop under `hold`; if it never climbs, the lever did
  not engage (`walk-lever: lever-not-engaged`) and a blocked walk says
  nothing about the rule.
- **The object tally, `jumpprobe objects` / `objects reset`.** From `hook`
  on, lever on or off, each builtin row counts the local player's own calls
  by the object index its object argument names (one bucket, `-1`, for the
  object-less rows and for an argument that is no object), up to 64 distinct
  objects a row and the rest under `other=`. `objects` prints each row with
  calls, its objects most-called first, each labelled `family` or `outside`
  by the same ancestry test, then a summary of rows, distinct objects and how
  many lie outside the family. The tally reads the object argument only for
  the player's own calls, and only `objects reset` clears it, so one walk
  counted with the lever off can be set beside the same walk under it. It
  answers which object a blocked walk asked about, which is what the jump's
  rules could not name. Positive control: on a walk the `position_meeting`
  row shows calls (S4 already shows that row counts walking).
- **`jumpprobe free [dx dy] [family|all|<object name>]`.** Asks the saved
  originals of `place_meeting` (the player's mask at its own `x`, `y` moved
  by `dx`, `dy`: `blocked=`) and `instance_position` (the instance at that
  point: `by=`), with the player as `self`. It calls the originals directly,
  never through the lever, and is not counted by the tally. The object asked
  is the collision family's root, `Collision_Parent_obj`, with no word or
  with `family`; GameMaker's `all` with `all`; and otherwise the object of
  that name, resolved by `asset_get_index` and kept only when
  `object_get_name` reads the same name back. The line gives `blocked=yes`
  or `no`, `by=` the object found or `none`, the position, the offset and
  `asked=` (`family:` with the root's name and index, `all(-3)`, or the name
  with its index), and adds a note when `by=` is the player's own object: a
  point query can find the asking instance, `place_meeting` cannot, so
  `blocked=` is then the answer to read. It refuses before `hook` (without a
  saved original the call would go through whatever detour holds the
  builtin), for a family root or a name that does not resolve, for one
  number alone or a third, for a second object word, and with no local
  player. Positive controls: on open ground (S4) it reads `blocked=no`; and
  with the player pressed against the prop, lever off (S5b), the mask moved
  8 px toward the prop, or 16, must read `blocked=yes`. A vanilla player
  stopped at a prop does not overlap it, so the offset is what makes that
  yes possible. S9 counts a `no` only for an object that read `yes` at S5b.

The jump research's two controls still open the walk checks: `CheckTalentUse`
counts once a frame and `position_meeting` climbs while the player walks.

### The charm probe, `charmprobe`

`FlightCharmProbe.hpp` holds the decisions, game-independent (numbers and
strings, never an `RValue`): identifying the size and sprite keys from
samples, parsing every form's arguments with a named reason for each refusal,
the dress rules, and the Heroic summary. The `charmprobe` adapter reads and
calls. A bare `charmprobe` prints the build marker (`fcp1`), the size keys
once known and the number of dress rules, then the usage.

- **`charms`.** Reads the equipped charms offline, as variables: the
  `Controller_obj` instance found by name, its `inventoryData` entry for the
  local player by the relic tab's key rule, and that entry's
  `localCharmItemArray`. It never calls `GetPlayerCharmData`, and it refuses
  online, where the charms live on `Client_obj`. Per item (at most 64) it
  prints the item type, the definition (`a`, `b`, `c`, `j`), the footprint
  from `GetItemSize` called by name with the item, the rarity and name
  (`itemInfoStruct` keys 27 and 28), then `itemstats=`, every
  `itemStatStruct` key (a stat id) whose value is a number, a reference
  (shown with its kind, since this runner hands asset values back as
  references) or a string of at most 40 characters (`none` for an empty
  struct, `missing(no itemStatStruct)` for an item without one; a stat whose
  value is an array is not listed), and last `info=`, every `itemInfoStruct`
  key under the same rule. It writes nothing to a worn charm. Each sized item is a sample
  for the size keys: a key holds the width (or height) when its value equals
  that sample's width (height) on every sample, and the pair is identified
  when exactly one key holds each, across at least two distinct sizes;
  otherwise `sizekeys` says why not. Normal charms are all one cell wide, so
  every key reading 1 on every sample holds the width: expect several keys
  named for `w` unless a worn charm of another width is a sample. An
  identified pair is only a candidate. Once there is one, `charms` ends with
  a read-back: it builds a scratch Small charm by the spawn's route (seed
  779000, just below the spawn range, never placed, so nothing keeps it),
  writes 2x3 and then 4x1 under the pair, asks `GetItemSize` after each
  write, and prints `charmprobe sizekeys: read back` with the pair, the
  scratch item's base size and each size written and read, ending
  `verified` when both read back as written and `keys-unverified`
  otherwise. Only a verified pair is ever written to an item. The size keys,
  the dress rules and the spawn seed counter are process state, lost when
  the game restarts. Positive control: S10, the read of a charm the
  character already wears and of three normal charms of known sizes (Small,
  Large, Grand) dragged into the grid; for the pair, the read-back itself,
  which the game's own `GetItemSize` answers.
- **`dress keys` / `dress keys <w key> <h key>`.** The hand fallback, for
  when the samples cannot settle the keys. Bare, it prints the keys in
  force, whether they were inferred or set by hand, and whether they are
  verified (running the read-back first when this pair has not had one), or
  why they are unknown or unverified. With two key names it runs the
  read-back on that pair: when both sizes read back, the pair becomes the
  keys in force, marked by hand (`by-hand verified` and the read-back's
  text), and `charms` stops feeding samples, so a later sample cannot drop
  it; otherwise it answers `refused` with the pair, `did not read back` and
  the read-back's text, and the keys in force are unchanged. A refusal
  elsewhere for unverified keys (`size keys unverified`) names `charmprobe
  dress keys <w> <h>` as this fallback; one for unknown keys (`size keys
  unknown`) does not. Positive control: the read-back, as for `charms`.
- **`repo <t> <sub> <b>`.** Calls `GetUniqueRepoStruct` by name and reads
  the returned definition's `itemBaseInfoStruct`: rarity (27), name (28), the
  hidden flag (40) and, once the size keys are known, the footprint, then
  dumps that struct's keys. Positive control: S12's read of Goburin's Head
  (10 / 0 / 98), known Unholy (10).
- **`spawn <b> [size <w> <h>] [stat <id>=<v>]... [sprite]`.** Builds a
  normal charm (`b` 0 to 59) by the route the signature items and
  `gambapity`'s forced drop already use: `json_parse` of a compact
  definition with a research seed (from 779001 up, one per spawn, never
  reused), `InitItemFromJson` by name with a research key, then
  `LootGroundCreateFromItem` by name with the local player as `self`, at the
  player. Before placing it, it dumps the item's `itemInfoStruct` as the game
  built it (a sample for the sprite key), then writes the requested
  footprint into the two size keys, the stats into `itemStatStruct` by stat
  id, and with `sprite` the loaded icon, in the kind `sprite_add` answered,
  into the sprite key (refused, before anything is built, until the icon is
  loaded, the key known and the key verified by `sprite verify`). Nothing
  else is written. A `size`
  is refused, before anything is built, with `size keys unknown` until a
  pair is identified and with `size keys unverified` until it read back.
  The spawn line counts the item as placed only when a `Loot_Ground_obj`
  with a higher id than before the call appeared (`ground=new id=<n>
  placed`, else `ground=none`), the same scan `heroic` uses. Its
  `getitemsize=` is `GetItemSize` asked of that new ground item's
  `itemInstance`, or of the built struct when none appeared (marked `(of the
  built struct)`), and a size asked for that did not read back as asked is
  marked `keys-unverified`. Positive control: S10's three spawns, read back
  by `charms` once dragged into the grid; for `stat`, S13's walk measured
  before the charm is worn.
- **`dress <a> <w> <h>` / `dress off`.** Up to eight rules, one per item
  seed `a`. At `CreateItemNew`'s outermost return, the Custom Forge's
  research hook writes a ruled item's two size keys and touches nothing else,
  so a footprint can be made to appear on a charm the save holds as 1x1. The
  research build already detours `CreateItemNew` at setup; `dress` installs
  the same detour only when nothing did, and refuses while the size keys are
  unknown or unverified; the pass itself writes only while the pair in force
  is verified. It prints the rules and how many items it has dressed. The Custom
  Forge's player-build pass is unchanged. Positive control: a bare
  `charmprobe dress` after S17's reload shows `dressed=` above 0, so the
  rule reached the item the game rebuilt; at 0 the reload measured the
  undressed charm, not the footprint.
- **`heroic <n>`** (1 to 20). Calls `DropItemHeroic` by name with the local
  player as `self` and `other` and its position, `n` times. Around each call
  it takes the highest `Loot_Ground_obj` id before and after (instance ids
  only grow, so a call that placed an item leaves a higher one), reads the
  new one's `itemInstance` (item type, definition `b` and `c`, rarity,
  name), and ends with a count of calls, items placed, a histogram by rarity
  code, how many were charms, and `self=player`. Each call's own line tells
  the outcomes apart: `DropItemHeroic threw` or `st=<n>` (the call did not
  dispatch), `nothing placed (returned ...)` (it dispatched and returned, and
  no ground item with a higher id appeared), and `placed ...`. The
  summary's `calls=` and `placed=` do not split the first two, so the
  per-call lines are what S14 records. ForgePact has never called
  `DropItemHeroic` before, and this call shape (the local player as `self`
  and `other`, its position as the two arguments, the third left to its
  default) is not proven: the game's own callers pass a vault or an NPC as
  `self`. Positive control: S10's `spawn` calls, which go through the same
  by-name call with the local player as `self`, to the game's own
  `LootGroundCreateFromItem`, and answer `ground=new id=<n> placed` from the
  same before-and-after id scan. That proves the call machinery and the
  scan, so a `nothing placed` beside placed spawns is the call, not the
  scan; it does not prove this call shape.
- **`sprite` / `sprite load` / `sprite key <name>`.** `sprite load` takes
  the pack markers' route without changing it: it writes the embedded
  placeholder PNG (a 32x32 teal diamond we drew from code,
  `assets/flight-charms/charmprobe.png`, embedded by
  `make_charmprobe_icon.py` in `FlightCharmProbeIcon.hpp`) under
  `bp_ipc\charmprobe\` only when no file of that name is there, then
  `sprite_add` with the path relative to the game's working directory first
  and the absolute path second. A reference or a real is accepted as the
  index, and it is re-centred with `sprite_set_offset` when it is not the
  embedded size. A bare `sprite` reports the index, the kind `sprite_add`
  returned, which path worked, the sprite key and `check=`, its control's
  state (`verified`, or `no` with the step that is missing). That key is
  identified like the size keys: a candidate is an `itemInfoStruct` key whose
  value `sprite_exists` (called by name) accepts, a whole number or a
  reference handed to it as it is, over the samples `charms` and each spawn
  feed it, and it is identified when exactly one key is a candidate on every
  sample and took at least two values. Only a sample `GetItemSize` sized is
  fed, and a footprint key is never a candidate: the size keys in force are
  left out, and so is every number equal to that sample's width or height,
  because `sprite_exists` accepts a 1, 2 or 3 as readily as a sprite's index,
  and the height key holds 1, 2 and 3 across S10's Small, Large and Grand
  charms. `sprite key <name>` sets it by hand from an `info=` dump when the
  rule cannot decide, and refuses a size key in force. Once known, `charms`
  adds each item's sprite value and name.
- **`sprite verify [<donor b>]` / `sprite verify drew` / `sprite verify
  base`.** The sprite key's positive control, run before anything writes our
  icon under it: an identified or hand-set key is only a candidate, since the
  sample rule says only that its values pass `sprite_exists`. It builds a
  donor charm (`b` 40, a Grand charm, unless another `b` is named; seed
  779000, never placed) and a `b` 3 charm from the spawn range, writes the
  donor's own value under the key, as the game built it, onto the `b` 3
  charm, and places that one. It prints the key, the `b` 3 charm's seed, its
  own value and the donor's, each with its sprite name, the ground answer,
  and what the placed item now holds under the key (`reverted` when the
  placement put the charm's own value back). It refuses while the key is
  unknown or is a size key in force, when either value is not a sprite, and
  when the two charms hold one sprite under the key (another donor is then
  named). If the surfaces
  draw from this key, the `b` 3 charm shows the donor's icon; if not, its
  own. That is the person's to see, so `sprite verify drew` (the donor's
  icon) or `sprite verify base` (its own) records the answer, and `spawn ...
  sprite` is refused with `sprite key ... unverified` until the answer is
  `drew` for the key in force; a key that changes after it needs the control
  again, and a key that later turns out to be a size key in force is refused
  whatever its control said. Positive control for the draw itself: S19's same charm spawned
  without `sprite`, which must draw its base icon on the ground, in the grid
  and in the tooltip.

`tests/test_flight_charm_probe_contract.py` pins the adapter on
comment-stripped source (research build only, by name, the variable read,
`charms` reading `itemStatStruct` and writing nothing, the dress write's
scope, `sprite_add` only inside the research block), and
`tests/test_flight_charm_probe_behavior.py` runs the core through
`flight_charm_probe_harness.cpp`.

### The debugger logpoints, during S5 to S8

Not part of the research build and not a rebuild: `jumpprobe`'s rows count
the calls to `InstancePlaceTallest`, `CanMove` and `TilePlaceMeeting`, but
not who made them. So during the walk checks the live operator attaches the
toolkit's x64dbg tool (the toolkit's `docs/tools/x64dbg-mcp.md`) to the
running game under the hs-drive lease, after S4's `jumpprobe hook`, and sets
non-breaking hardware logging breakpoints. Each function is found by name in
the Ghidra project of this build and mapped onto the running module the way
that guide says; the addresses are kept in the session's local notes and
never enter this doc. Four at once, the hardware's limit, all armed in the
`dbg-control` row before S5's walk, the control first:

- **The control**, `CheckTalentUse` (once per frame, RDM § 7.2), checked by
  `dbg-control`. It must log hits during S5's walk; at zero the debugger
  measured nothing, and the rows below close nothing.
- **`InstancePlaceTallest`** (599, `dbg-instance-place-tallest`) and
  **`CanMove`** (467, `dbg-can-move`), each logging a hit counter and the
  return location at its entry, which names the caller once mapped back
  through the same Ghidra project.
- **`TilePlaceMeeting`** (3866, `dbg-tile-place-meeting`), logging the same.
  It takes the fourth slot, which the plan had kept for the `place_meeting`
  builtin's own function if the Ghidra project named one. It does not: a
  name search for `meeting` there finds only `TilePlaceMeeting`. That leaves
  the slot to the one candidate script `jumpprobe` levers that neither
  logpoint above covers.

**Arming.** `jumpprobe hook` detours all four scripts at their own address,
so their first bytes in the live process are the detour's jump rather than
the bytes of the Ghidra project's copy. Byte matching at arm time would
refuse that expected mismatch, so they are armed without it, and the first
bytes the debugger returns for each are copied into the session's capture
beside the copy's. A candidate whose live bytes equal the copy's, or do not
begin like the control's, is not on its detour: its check is `not-run`
(instrument: bytes) and its zero is no evidence. The control's hits show
that an entry breakpoint on a detoured script fires.

**Per walk** (S5, S7, S8, S8b and the wall repeats) the operator notes each
logpoint's counter before and after the walk and writes that window's
return locations into the session's capture, grouped, each with its count
and its offset in the game module. The session does not name the callers:
the record step maps each offset to its caller through the same Ghidra
project afterwards, and only callers by name, with their counts, reach this
doc. Neither the debugger's return-location field nor its hit counter has
been measured live yet, so hits are counted from the log lines; if the
return location comes out unevaluated, the check's note says so and the
counts stand alone.

**What a logpoint sees.** `jumpprobe hook` does not swap the script table.
It detours each script at the script's own address, the same function the
table entry points to, so a direct call from compiled code and a call routed
through the table (or a method value) both run that entry. The entry
breakpoint therefore sees every call that reaches the detoured function, by
either route, and it fires under `scripts hold` too, where the lever answers
the call without running it. What tells the routes apart is the return
location: a direct call logs the compiled script that made it, while a
table-routed call logs the runner's own script dispatcher. The record step
writes a return location that maps into the runner's dispatcher, rather
than into a compiled script, as `via-dispatcher` (the table or method
route), never as a caller. The row's own `calls=` counts the calls that
reached the same detour, so the two counts are two instruments on the same
calls: a gap between them is not any route's share, and a logpoint with no
hits while its row's `calls=` rose has failed as an instrument
(`logpoint-failed`), which says nothing about how the game routes the call.

**The four checks.** `dbg-control` passes when the control logged hits
during S5's walk. Each of `dbg-instance-place-tallest`, `dbg-can-move` and
`dbg-tile-place-meeting` passes when its logpoint logged. A window with no
hits is `not-observed` only when the control logged in the same window, the
candidate's first bytes showed the detour like the control's, and its
`jumpprobe` row's `calls=` did not rise in that window either, so both
instruments agree the function was not called. No hits while the row's
`calls=` rose is `not-run (instrument: logpoint-failed)`, and bytes that did
not show the detour are `not-run (instrument: bytes)`; in both, the zero is
no evidence. The operator detaches after S8 (and its outlier repeats),
before S9, and in any case before any `hs_stop_game`.

## Live procedure 1

This is the procedure the session runs, complete: the setup, every step with
its expected result in run order, the cases, what it relies on and the
teardown. The debugger rows follow § Instrument, "The debugger logpoints,
during S5 to S8". The one thing this doc leaves out on purpose is the
debugger's addresses, which never enter a tracked file; they are kept in the
session's local notes.

**Setup:**

- **Build:** the research build (`build.bat dev`) at the phase's final
  commit. The owner installs it when asked, and its SHA-256 is recorded in the
  session's log.
- **Character:** save slot 14, "Sorak", in `Town_01_rm`. The saves are backed
  up first and restored at teardown whatever happened.
- **Launch:** `jumpscenery` stays off, because its hooks would hold the
  builtins `jumpprobe` needs: `jumpscenery 0` at character select, on every
  launch, S20's relaunch included.
- **Walking:** holding the move key toward the target for the stated time,
  by hs-drive key input or by the person.
- **Debugger:** attached after S4 under the hs-drive lease, the four
  logpoints armed in the `dbg-control` row, and detached in the
  `dbg-tile-place-meeting` row, after S8 and its repeats, before S9, and in
  any case before any `hs_stop_game`.
- **Order:** the table's: S0 to S4, `dbg-control`, S5, S5b, S6, S7, S8 (with
  S8b and the wall repeats), the three candidate debugger rows, S9 to S14,
  then S18, S19, S15, S16, S20 and S17. S17 runs last because it may crash
  the game.
- **Cases:** for walking, the jump research's town prop (ordinary scenery)
  and a building wall if one is in reach (the outlier). For sizes, one normal
  charm of each size. For the repository, the Heroic 1x1 candidate and the
  two size outliers (a 2x2 Angelic, the 1x5 Unholy). For the sprite, one
  normal 1x1 charm (`b` 3) with and without our icon, and for the key's
control the same charm wearing a Grand charm's (`b` 40) own sprite; the 2x2 and 1x5
  outliers are not drawn in this phase.

**Checks.** `dll-hash`, `marker`, `control`, `walk-control` and
`dbg-control` establish that the session is valid, and must pass. For the
rest, a `fail` or a `not-observed` is itself the finding, as long as the
check's positive control passed in the same session.

| Check | Step | Expected |
| --- | --- | --- |
| S0 `dll-hash` | before launch | the installed DLL's SHA-256 equals the research build's recorded hash |
| S1 `marker` | bare `jumpprobe`, bare `charmprobe` | `jumpprobe: rows=20 hooked=0 held=0 builtins=0`, and `charmprobe` reports `marker=fcp1` |
| S2 `control` | `ping` | `pong (YYTK 4.0.1)` |
| S3 `jumpscenery-off` | `jumpscenery stat` | ends `install=not-armed` |
| S4 `walk-control` | `jumpprobe hook`, walk 3 s on open ground, `jumpprobe show`; then, still on that open ground, `jumpprobe free` and `jumpprobe free all` | `hook` answers `20 detoured, 0 failed, 0 held, 10 builtins`, and the `calls=` of both `CheckTalentUse` and `position_meeting` rose. The two `free` lines are recorded for `free-control` and do not decide S4 |
| `dbg-control` | after S4: the operator attaches x64dbg under the lease and arms the four logpoints, the control first (§ Instrument, "The debugger logpoints, during S5 to S8"), without byte matching, and copies the first bytes each returns into the capture | passes when the control logged hits during S5's walk. A session-validity check: at zero, the candidate rows below close nothing (`walk-callers: control-failed`) |
| S5 `vanilla-walk-block` | the person places Sorak 30-40 px from the jump research's prop (screenshot); `jumpprobe state`, `jumpprobe objects reset`, each logpoint's counter noted, walk into the prop 3 s, the counters noted again, `jumpprobe state` | the position stopped at the prop and did not cross |
| S5b `free-control` | right after S5's walk, the player still pressed against the prop, the lever off: `jumpprobe free <d>` and `jumpprobe free <d> all`, where `<d>` is 8 px toward the prop along the walked axis (`8 0`, `-8 0`, `0 8` or `0 -8`), each once more at 16 px if it read `blocked=no`; `asked=` and `by=` recorded | passes when S4's open-ground `jumpprobe free` read `blocked=no` and at least one of the two offset reads gave `blocked=yes`; the objects that read yes are the ones whose `no` S9 may count |
| S6 `walk-objects` | `jumpprobe objects` | at least one row; every object named is recorded, and the outside object `place_meeting`'s row names most often (a named one, not the `-1` bucket) is recorded for S9 |
| S7 `walk-all-scripts` | `jumpprobe pass 1 90 all scripts hold`, the counters noted, walk into the prop 3 s, the counters noted again, `jumpprobe state`, `jumpprobe pass stat` | `passed=` climbs; crossed or held is recorded, and the check passes if the walk crossed |
| S8 `walk-any` | `jumpprobe pass 0`, `jumpprobe pass 1 90 any scripts hold`, the same walk, counters and reads; if it crossed, S8b: `pass 0`, `pass 1 90 any hold`, the same again | passes if S8 crossed; S8b goes in the note. With a building wall in reach, S7 and S8 are repeated there (the outlier), with the counters noted around each walk |
| `dbg-instance-place-tallest` | after S8 and its repeats: for each walk (S5, S7, S8, S8b and the wall repeats), the `InstancePlaceTallest` logpoint's hits in that walk's window and its return locations, grouped, each with its count and its module offset, into the capture | passes when the logpoint logged; `not-observed` when it logged nothing while `dbg-control`'s control logged in the same window, its first bytes showed the detour like the control's and its `jumpprobe` row's `calls=` did not rise either; `not-run (instrument: logpoint-failed)` when it logged nothing while that row's `calls=` rose; `not-run (instrument: bytes)` when its bytes did not show the detour |
| `dbg-can-move` | the same, for the `CanMove` logpoint | as for `dbg-instance-place-tallest` |
| `dbg-tile-place-meeting` | the same, for the `TilePlaceMeeting` logpoint; then the operator detaches, before S9, quoting the debugger's reply and the outside thread check | as for `dbg-instance-place-tallest` |
| S9 `stuck-inside` | only if S7 or S8 crossed: with that lever, stop while overlapping the prop (screenshot); `jumpprobe free` (the family), `jumpprobe free all` and `jumpprobe free <the S6 object>`, each with `asked=` recorded; `jumpprobe pass 0`; walk 1 s in each of the four directions with `jumpprobe state` after each | passes when some object asked reads `blocked=yes` (the player is shown inside it) and the walk-out outcome is recorded: the game ejected the player, the player walked out, or the player stayed stuck. A `blocked=no` counts only for an object that read `blocked=yes` in S5b. `not-observed` when `free-control` did not pass, when neither S7 nor S8 crossed, or when no object asked read yes (the note names them); a family-only `no` after an S8-only crossing is never a negative. Recovery is the operator's `playerwarp` to the S5 position |
| S10 `charm-read` | `charmprobe charms` (count n), `charmprobe spawn 0`, `spawn 20`, `spawn 40`, the person drags the three into the charm grid (screenshot), `charmprobe charms` | each spawn answers `ground=new id=<n> placed`, and the count is n + 3 with the three seeds at `t=10`. These three spawn lines are the ground scan's control for S14 |
| S11 `size-keys` | S10's output; when it did not end in a verified read-back, the hand fallback in the next column | `size=1x1`, `1x2` and `1x3`. Passes only on a read-back: the `charms` output's `charmprobe sizekeys: read back` line ending `verified`, or a hand-set pair's `by-hand verified`; the note says which. When it reads `unknown` or `keys-unverified`, the operator's step: from S10's `charm #` lines, the width candidates are the `info=` keys whose value equals that line's width on every line, the height candidates those equal to its height on every line; `charmprobe dress keys <w> <h>` for each pair, height candidates in the outer loop, at most 40 pairs, stopping at the first `by-hand verified`; every pair tried goes in the note. No pair reading back is `not-observed`, and S15, S16 and S17 are then not run |
| S12 `repo-rarity` | `charmprobe repo 10 0 76`, then 52, 47, 91 and 98 | each prints a rarity; the control is that 98 (Goburin's Head) reads 10. Aimbot.exe's (76) code is recorded |
| S13 `forged-stat-applies` | on open ground, walk right for exactly 2 s with `jumpprobe state` before and after (distance d0), screenshot of the sheet's movement speed; `charmprobe spawn 1 stat 25=100`, the person drags it into the charm grid; `charmprobe charms`; the walk again (d1) and the screenshot | the read-back: the `charms` line whose `a=` is S13's seed, in `localCharmItemArray`, with `itemstats=` holding `25=100`. Passes if d1/d0 is at least 1.3 or the sheet's value rose. A `no` is recorded only when the read-back passed; the charm missing from the list, or listed without `25=100` in `itemstats=`, is `stat-not-on-item`; a read-back that could not be made is `not-observed` |
| S14 `heroic-route` | control first: S10's three `ground=new id=` spawn lines (the same by-name call with the local player as `self`, and the same before-and-after scan of ground ids), whose charms reached the grid; then `charmprobe heroic 10`, with every per-call line recorded | each call line reads `threw` or `st=` (not dispatched), `nothing placed (returned ...)` (dispatched, nothing placed) or `placed ...`; the summary, ending `self=player`, with its `placed=` and rarity histogram is recorded. Passes if items were placed and no placed rarity is 7 or 10. A control that did not place, or heroic calls that threw, answered `st=` or read `nothing placed`, is `call-not-engaged`, never a negative about the roll |
| S18 `sprite-load` | `charmprobe sprite load`, then `charmprobe sprite`. If the key stays `unknown`, the operator picks it from the `info=` dump with `charmprobe sprite key` (never a size key) and records that in the note. Then the key's control, on open ground: `charmprobe sprite verify`; screenshot on the ground; the person picks the charm into the bag (screenshot) and hovers it (screenshot of the tooltip); `charmprobe sprite verify drew` if it showed the donor's (the Grand charm's) icon, `charmprobe sprite verify base` if it showed its own; `charmprobe sprite`. After a `base`, another candidate from the dump may be set with `sprite key` and verified the same way, at most three keys in all, every key tried going in the note | `loaded=` an index, with `kind=` and `path=`; the `verify` line answers `ground=new id=<n> placed`, with its `key-on-ground=` recorded; the last `charmprobe sprite` reads `check=verified`. Passes if loaded and verified. A key that never drew the donor's icon fails the check, and S19 is then not run |
| S19 `sprite-draw` | only if S18 passed: on open ground, `charmprobe spawn 3` (the control: the same charm with its own sprite) and `charmprobe spawn 3 sprite`; screenshots on the ground; the person picks both into the bag (screenshot) and hovers each (screenshot of each tooltip); `charmprobe charms` | both answer `ground=new id=<n> placed`. The control must draw the base icon everywhere, or the session measured nothing. Per surface (ground, grid, tooltip), whether our icon drew is recorded; passes if all three drew it, with `partial` or `none` in the note otherwise. Both charms stay in the bag |
| S15 `size-override` | not run when S11 found no verified pair: `charmprobe spawn 2 size 2 2`, its line's `getitemsize=` and seed recorded; the person drops it on a free 2x2 area of the bag (screenshot) | passes when the line reads `getitemsize=2x2` from a new ground item (`ground=new id=<n> placed`) with no `keys-unverified`, and the charm covers 2x2 cells in the bag. `not-observed` for `keys-unverified`, `(of the built struct)` or `ground=none`: the write never reached a placed item |
| S16 `dress-removed` | not run when S11 found no verified pair: with that charm in the bag, save and quit to the menu, load slot 14 again (screenshot of the bag) | the game loads, the charm is 1x1, and no item is lost, duplicated or overlapped |
| S20 `sprite-save` | after S16, before S17: note what S16's reload showed for S19's two charms; `charmprobe charms` (the custom charm's `sprite=` index i); save and quit; stop the game and launch the same DLL without ever sending `charmprobe sprite load`; `jumpscenery 0`; load slot 14; screenshot the bag and both tooltips; `charmprobe charms`. No `charmprobe spawn` is sent after the relaunch: the seed counter starts again at 779001 in a new process | `safe` if the game loaded, the charm draws its base icon and its `sprite=` equals the control's (not i); `persisted` if it still holds i; `crash` if the game crashed (it is restarted the same way before S17). S1 to S3 run again afterwards |
| S17 `dress-added` | last, and not run when S11 found no verified pair. First the verified pair is restored in this new process: S20's `charmprobe charms` re-infers and reads back an inferred pair, and a pair S11 set by hand is entered again with `charmprobe dress keys <w> <h>`; a bare `charmprobe dress keys` must read `verified` before going on. Then the person puts S15's now-1x1 charm where its right and lower neighbours are occupied; `charmprobe dress <S15 seed> 2 2`; save and quit; load slot 14 (screenshot, then `charmprobe charms`); a bare `charmprobe dress` | `safe`, `overlap` or `crash` is recorded, when that bare `charmprobe dress` shows `dressed=` above 0. At 0 the reload measured the undressed charm, and the check is `not-observed` |

**What the procedure relies on.** The `Collision_Parent_obj` family and the
five builtins (RDM § 19.2 and § 19.3, measured); `InstancePlaceTallest`'s
player-self calls on a walk (RDM § 19.2, a measured count, and § 19.6, a
static reading); `CheckTalentUse` once per frame (RDM § 7.2); `jumpprobe`
refusing while `jumpscenery` holds the builtins (the module guide's "Jump
through scenery" cost); `inventoryData` and its key rule (RDM § 2, measured
for the relic grid); `localCharmItemArray` (RDM § 23.1, a static reading,
never read live before this session); `GetItemFromFingerprint` and
`GetOnlinePlayerItemOwner` (RDM § 2, measured); `GetItemSize` and
`GetItemInfo` (RDM § 23.2, a static reading); the json, `InitItemFromJson`,
`LootGroundCreateFromItem` spawn (RDM § 16.2, measured, and `sigdrop`);
`GetUniqueRepoStruct`, field 27 and flag 40 (RDM § 13.4); `DropItemHeroic`
(RDM § 23.4, a static reading); the Custom Forge's final pass (RDM § 16.1 and
§ 16.9, measured); stat 25 (the Item Editor's stat catalog; its effect on the
character is not in the shared references); `sprite_add` from `bp_ipc\` with a
relative then an absolute path, a `VALUE_REF` index and `sprite_set_offset`
(the module guide's pack marker icons, verified on the 2026-09-22 build only);
the item's display-sprite key (not in the shared references; S18 identifies
it and checks it with `sprite verify` before S19 writes under it); and a save keeping only the compact definition (RDM § 16.1, measured).

The size checks rely on the read-back: a pair counts only when the game's own
`GetItemSize` answers, on a scratch charm, the sizes written under it
(§ Instrument, `charms`), so S15 to S17 never rest on an inferred pair alone.
The `free` checks rely on the offset read at S5b, since a vanilla player at a
prop does not overlap it.

The debugger logpoints rely on the toolkit's x64dbg tool and its rules
(`docs/tools/x64dbg-mcp.md` § "Breakpoint rules": hardware breakpoints only,
four at most, a positive control first, a detoured entry's bytes not matching
the Ghidra copy's, and an address mapped from Ghidra kept as a session note);
on `CheckTalentUse` firing once per frame (RDM § 7.2), measured there at about
85 hits a second in town on game 7.0.13.0; and on the Ghidra project being an
import of the installed game, checked by its hash before the launch, so that
after a game update the session stops instead of arming stale addresses.

**Teardown:** `jumpprobe pass 0`; the debugger detached if it is still
attached, before any `hs_stop_game`; the saves restored from the session's
backup whatever happened; the hs-drive lease released.

## Results

### Live 1 results

Run on 2026-10-12 by the live operator, on the research DLL built from
ForgePact `ac41a23` (sha256
`ef6af41d4bd3ff478f3516a30dc46c4dd699caed8ab383ef5a5aeb7558ac323f`), save
slot 14 "Sorak", in `Town_01_rm`, on the game build whose executable hashes to
`498d588550d80d0d100a4ce6958bcc044e6b79339920d3d8a931bca9a1b142c0` (the copy
the Ghidra project imported). The record is the session capture
`forgepact-158-flight-charms-live-1.md`, kept with the hub's workorder files on
the researcher's machine and not tracked. The toolkit's
`tools/live_checks.py` reads 26 checks from it: 21 pass, 3 fail, 2
not-observed, none not-run. The five validity checks (`dll-hash`, `marker`,
`control`, `walk-control`, `dbg-control`) all passed, so every research check
below stands on a working instrument. The saves were restored at teardown and
read back clean.

| Check | Verdict | What it showed |
|---|---|---|
| `dll-hash` | pass | The installed DLL was the research build above. |
| `marker` | pass | Both research markers answered (`jumpprobe` and `charmprobe marker=fcp1`), also after S20's relaunch. |
| `control` | pass | `ping` answered `pong (YYTK 4.0.1)` in both processes. |
| `jumpscenery-off` | pass | The shipped Jump through scenery mod stayed off (`install=not-armed`), so no other lever touched the walk. |
| `walk-control` | pass | `jumpprobe hook` detoured all 20 script rows and hooked the 10 builtins; during a 3 s walk on open ground the two controls climbed (`CheckTalentUse` 1890 calls, `position_meeting` 6362). |
| `dbg-control` | pass | The x64dbg logpoint on `CheckTalentUse` logged in every walk window (652 hits during S5's walk), and the debugger's own control hold proved the attach. |
| `vanilla-walk-block` | pass | With every lever off, a 3 s walk down at the edge of the ravine south of the town, next to the wooden bridge (the owner's spot), left the position unchanged. The repeat beside the church's stone wall walked 18 px and stopped at the wall. |
| `free-control` | pass | On open ground the family read `blocked=no`; one step down at the ravine edge the family read `blocked=yes` with no object named, so `free` can tell a blocked spot from a free one. The `all` read names `Camera_obj` everywhere, open ground included, so it says nothing. |
| `walk-objects` | pass | The tally of the blocked walk counted six distinct objects on three builtin rows. The object outside `Collision_Parent_obj`'s family that `place_meeting` named most was `Enemy_Parent_obj` (3015 queries); `Enemy_Child_Boss_obj` and `Enemy_Only_Passage_obj` were also queried from outside the family. `Wall_Parent_obj` was queried too, and the tally labelled it `family`: it is a child of `Collision_Parent_obj` in the game's data file (RDM § 19.8). This row first counted it outside the family, which was wrong (corrected 2026-10-12). |
| `walk-all-scripts` | pass | Under `pass 1 90 all scripts hold` the walk crossed: 699 px down past the ravine edge, and 688 px through the church wall in the repeat. The lever answered 234 `InstancePlaceTallest` calls `noone` on the prop walk. |
| `walk-any` | pass | Under `pass 1 90 any scripts hold` the walk crossed both the ravine edge and the wall. Note: under `pass 1 90 any hold`, the same rule without the scripts (S8b), the walk stayed at the ravine edge, although the lever answered 2340 `position_meeting` and 2410 `place_meeting` queries. |
| `dbg-instance-place-tallest` | pass | The logpoint on `InstancePlaceTallest` logged in every walk, as many hits as the hooked row's `calls=` each time, with the return location of each call (§ "The walk's callers" below). |
| `dbg-can-move` | not-observed | `CanMove` logged no hit in any walk window while the control logged, its entry showed the detour, and its hooked row read `calls=0` all session: through neither route was it seen to be called during a walk. |
| `dbg-tile-place-meeting` | not-observed | The same for `TilePlaceMeeting`: no hit, entry on the detour, `calls=0` all session. The debugger detached cleanly before S9 and every game thread ran again. |
| `stuck-inside` | pass | A 150 ms walk under S7's lever left the player inside the ravine edge, where the family read `blocked=yes` naming `Block_Dynamic_Sprite_obj`. With the lever off, 1 s walks up, down, left and right left the position unchanged: the game did not push the player out and the player could not walk out. `playerwarp` recovered. |
| `charm-read` | fail | The read itself worked: `localCharmItemArray` listed the 12 worn charms, and after the three spawned charms were dragged into the charm grid all three were listed at `t=10`. The count was 13, not 15, because the owner moved two worn charms (a Grand and a Large) to the bag to make room, and those two dropped out of the list. The fail is the count rule, not the array. |
| `size-keys` | pass | The footprint is held by the `itemInfoStruct` keys `"2"` (width) and `"3"` (height), inferred from the 12 worn charms of six sizes and read back on a scratch struct (wrote 2x3, read 2x3; wrote 4x1, read 4x1). The three spawned charms read 1x1, 1x2 and 1x3. No pair was set by hand. |
| `repo-rarity` | pass | The game's own unique repository gives Aimbot.exe `{10, 0, 76}` rarity 6 (Satanic), Torstein's Anvil 9 (Heroic), the Water Melon 7 (Angelic, 2x2), Soul Collector 10 (Unholy, 1x5) and the control Goburin's Head 10 (Unholy, 2x2, the only one of the five flagged). |
| `forged-stat-applies` | pass | A charm spawned with stat 25 at 100 kept it after pickup and equip (its `itemstats=` read back `25=100`), and the character sheet's Movement Speed rose from 151% to 251% (the owner's reading; the owner notes a soft cap at 228%). The 2 s walk covered the same distance before and after (607.8 and 607.7 px, with the inventory open both times), so the walk did not show the rise. Note: after S16's save and reload, `25=100` was gone from that charm's `itemstats=`. |
| `heroic-route` | pass | Ten `DropItemHeroic` calls with the player as `self` placed ten items: seven Heroic (9) and three Satanic (6), none Angelic or Unholy, and no charm among them. S10's spawns, placed through the same by-name call machinery, are the control. |
| `sprite-load` | pass | `sprite_add` loaded our placeholder PNG as a new sprite. The key that holds the display sprite was not inferred (seven keys hold a sprite on every sample); the operator set key `11`, which holds each charm's own sprite, by hand. Its positive control passed: a charm with a Grand charm's sprite written under `11` drew the Grand icon on the ground and in the bag. Keys tried: 11. |
| `sprite-draw` | fail | Partial. A charm carrying our sprite under key `11` drew our icon in the bag, and the game's error placeholder on the ground. The tooltip drew no icon, but it draws none for the control charm either, so the tooltip is not an icon surface at all. |
| `size-override` | pass | A charm whose built struct was written 2x2 before it was placed read `getitemsize=2x2` from a new ground item and covered 2x2 cells in the bag. |
| `dress-removed` | pass | After a save and quit and a reload of slot 14 in the same process, with no size rule in force, the 2x2 charm came back 1x1 with its normal icon. Nothing was lost, duplicated or overlapped. |
| `sprite-save` | pass | After a full stop and a relaunch that never loaded our sprite, the game loaded slot 14 and the charm that had carried it drew a normal small-charm icon in the bag. Its stored sprite value could not be read, because `charmprobe charms` lists the charm grid and the charm was in the bag, so the verdict rests on the icon. S16's in-process reload had already shown the base icon. |
| `dress-added` | fail | A 1x1 charm whose right and lower neighbours were occupied was given a 2x2 rule, saved and reloaded: the rule applied (`dressed=1`) and the game did not crash, and both neighbours were intact and not overlapped, but the dressed charm was no longer at its cell or anywhere on Extra tabs 1 to 3. The Main tab and Extra tab 4 were not checked before the saves were restored, so where it went is not established. The outcome, an item missing after the reload, is none of the check line's three values, so the check reads `fail`; § Decision records it as `item-missing`. |

Also observed, not a check: arming a hardware logpoint stalled the game's
frames for up to about 4.7 s while the debugger set it, and both processes'
graceful closes (the S20 stop and the teardown) ended with the status
`C0000409` rather than 0; whether a close without the research build does the
same was not checked. Eight of the owner's steps had an hs-drive route the
operator did not try at the time; the capture lists them.

#### The walk's callers

The operator logged each logpoint hit's return location as a module offset.
The record step matched every logged offset to a direct call site found by
`FindCallers.java` in the Ghidra project (a site's end, the return a `call`
leaves): all of them match, and no offset was left unmatched. The
`InstancePlaceTallest` sites the walks used are six direct sites inside one
function that the symbol dump does not name. That function's entry is stored
in the runner's object-event registration table right after the name
`gml_Object_Player_obj_Step_0`; the same table read names `ClassStepEvent`'s
own entry correctly, which is the control. So the caller is the player
object's Step event. The static reading had put these six sites in
`ClassStepEvent`, the nearest named script, which they follow in the image;
that attribution was wrong. Neither `CanMove` nor `PlayerMovement` called
`InstancePlaceTallest` during a walk.

Calls to `InstancePlaceTallest` per walk, all from
`gml_Object_Player_obj_Step_0` (sites written A to F, in their order in the
function; their offsets stay in the capture):

| Walk | Lever | Calls | Sites | `CanMove` | `TilePlaceMeeting` | Crossed |
|---|---|---|---|---|---|---|
| S5, ravine edge | off | 315 | B, E, F, 105 each | 0 | 0 | no |
| S7, ravine edge | `all scripts hold` | 234 | B only | 0 | 0 | yes |
| S8, ravine edge | `any scripts hold` | 227 | B only | 0 | 0 | yes |
| S8b, ravine edge | `any hold` (no scripts) | 306 | B, E, F, 102 each | 0 | 0 | no |
| wall, vanilla | off | 303 | A 105, C 99, D 99 | 0 | 0 | no |
| wall, S7 | `all scripts hold` | 230 | A only | 0 | 0 | yes |
| wall, S8 | `any scripts hold` | 234 | A only | 0 | 0 | yes |

Read together: on a blocked walk the Step event queries
`InstancePlaceTallest` from three sites, equally often (a different three for
the ravine edge and for the wall). When the lever answers those calls `noone`,
only the first site of the three runs, and the walk crosses. When only the builtins are answered (S8b),
all three are still made, the scripts' own answer stands, and the walk is held.
So what holds a walk at a prop or a building wall is the answer of
`InstancePlaceTallest` to the player object's Step event. Whether answering
`InstancePlaceTallest` alone, with no builtin rule, frees the walk was not
run.

## Decision

Every key below read `pending` until Live 1 ran. Each takes exactly one of
its listed values, on a line of its own, with the check that decides it on
the line after. Recorded from Live 1 (2026-10-12, § Results). `dress-added`
took `item-missing`, a value added after the session for the outcome S17
measured, which none of its first three values described.

walk-lever: all-scripts

Decided by `walk-all-scripts` (pass): S7 crossed, at the ravine edge and at
the church wall. S8 crossed too, and S8b did not.

`walk-lever:` (S7, S8, S8b) takes exactly one of these values:

- `all-scripts`: S7 crossed. (`all` alone was already blocked, in the jump
  research's Live 1.)
- `any`: S8b crossed.
- `any-scripts`: only S8 crossed.
- `not-observed`: nothing crossed while `passed=` climbed. It is recorded
  only with `walk-callers:` beside it, which says which callers the
  logpoints saw during those walks; a walk held under every lever leaves the
  routes no lever answers open (§ Static reading), it does not show the walk
  cannot be freed.
- `lever-not-engaged`: `passed=` never climbed, so the run says nothing about
  the game.

walk-callers: S5 (ravine edge, lever off) InstancePlaceTallest 315 from gml_Object_Player_obj_Step_0 (three sites, 105 each), CanMove 0, TilePlaceMeeting 0; S7 InstancePlaceTallest 234 from Player_obj Step_0 (one site), CanMove 0, TilePlaceMeeting 0; S8 InstancePlaceTallest 227 from Player_obj Step_0 (one site), CanMove 0, TilePlaceMeeting 0; S8b InstancePlaceTallest 306 from Player_obj Step_0 (three sites, 102 each), CanMove 0, TilePlaceMeeting 0; wall repeats InstancePlaceTallest 303 vanilla (three other Step_0 sites, 105, 99 and 99), 230 under S7 and 234 under S8 (one site), CanMove 0, TilePlaceMeeting 0

Decided by `dbg-instance-place-tallest` (pass), `dbg-can-move` (not-observed) and `dbg-tile-place-meeting` (not-observed), with `dbg-control` passed in the same session: the callers are named from the return locations as § Results, "The walk's callers", says. A 0 is a logpoint that stayed silent while the control logged and its entry showed the detour, with the hooked row's `calls=` also 0, so neither route saw the call.

`walk-callers:` (the checks `dbg-instance-place-tallest`, `dbg-can-move` and
`dbg-tile-place-meeting`, over the walks of S5, S7, S8 and S8b) takes one of
these values:

- one line naming, per walk (S5's vanilla walk, S7, S8 and S8b), the
  callers each candidate logpoint saw, by name with their hit counts, for
  `InstancePlaceTallest`, `CanMove` and `TilePlaceMeeting`. Return
  locations in the runner's script dispatcher rather than in a compiled
  script are written `via-dispatcher` with their count (the table or method
  route; the logpoint sees both routes). A logpoint with no hits while its
  row's `calls=` rose is written as `logpoint-failed` for that walk, an
  instrument fault and not a fact about routing; one whose bytes did not show
  the detour is written as `not-run (bytes)`. The record step names each caller from the return locations
  the capture holds; when they came out unevaluated, the line gives the
  counts and says the callers were not read.
- `control-failed`: the `CheckTalentUse` control (`dbg-control`) logged
  nothing, so the logpoints measured nothing.
- `not-run`: the debugger was not attached.

stuck-inside: stuck

Decided by `stuck-inside` (pass): inside the ravine edge the family read `blocked=yes` naming `Block_Dynamic_Sprite_obj`, and with the lever off the player moved in none of the four directions.

`stuck-inside:` (S9): `game-ejects`, `walks-out` or `stuck`, when some object
`jumpprobe free` asked read `blocked=yes` with the player inside the prop;
otherwise `not-observed`, which it also is when `free-control` did not pass,
when neither S7 nor S8 crossed, or when no object asked read `blocked=yes`. A
`blocked=no` counts only for an object that read `blocked=yes` at S5b, and a
family-only `no` after an S8-only crossing is never a negative.

charm-read: localCharmItemArray

Decided by `charm-read` (fail on its count rule only): the array listed the worn charms and the three spawned charms once they were in the grid; the count was short by the two charms the owner moved to the bag, which left the array with them.

`charm-read:` (S10): `localCharmItemArray` or `not-observed`.

size-keys: found

Decided by `size-keys` (pass): the pair `"2"` (width) and `"3"` (height), inferred and read back `verified`; none set by hand.

`size-keys:` (S11): `found` (a pair read back `verified`, inferred or set by
hand with `dress keys`; the note says which) or `not-observed` (no pair read
back).

size-override: works

Decided by `size-override` (pass): `getitemsize=2x2` from a new ground item, and 2x2 cells in the bag.

`size-override:` (S15): `works` (`getitemsize=2x2` from a new ground item and
2x2 cells in the bag), `ignored` (the placed item read `getitemsize=2x2` and the
bag still gave it one cell), `not-observed` (`keys-unverified`, `(of the built struct)` or
`ground=none`: the write never reached a placed item) or `not-run` (S11
found no verified pair).

dress-removed: safe

Decided by `dress-removed` (pass): the reload brought the charm back 1x1, nothing lost, duplicated or overlapped.

`dress-removed:` (S16): `safe`, `unsafe` or `not-run`.

dress-added: item-missing

Decided by `dress-added` (fail, outcome `item-missing`): after the reload the rule applied (`dressed=1`), the game did not crash and the neighbours were intact and not overlapped, but the dressed charm was neither at its cell nor on Extra tabs 1 to 3; the Main tab and Extra tab 4 were not checked. The check reads `fail` because the check line knows only `safe`, `overlap` and `crash`; the value is this key's, below.

`dress-added:` (S17): `safe`, `overlap`, `crash`, `item-missing` (a bare
`charmprobe dress` read `dressed=` above 0 after the reload, the game did not
crash and nothing overlapped, but the dressed charm was not at its cell or on
any tab checked; the note names the tabs not checked), `not-observed` (a bare
`charmprobe dress` read `dressed=0` after the reload, so the reload measured
the undressed charm) or `not-run`.

forged-stat-applies: yes

Decided by `forged-stat-applies` (pass): the worn charm read back `25=100` and the sheet's Movement Speed rose from 151% to 251%; the walk's distance did not change, and the stat was gone after a save and reload.

`forged-stat-applies:` (S13) takes one of these values:

- `yes`: the read-back passed and d1/d0 is at least 1.3 or the sheet rose.
- `no`: the read-back passed (the worn charm carries 25=100) and neither the
  walk nor the sheet rose.
- `stat-not-on-item`: the read-back shows the charm missing from
  `localCharmItemArray`, or listed without its stat 25 entry at 100, so the
  write did not survive pickup and equip and the walk says nothing about
  forged stats.
- `not-observed`: the read-back could not be made.

heroic-route: heroic-and-satanic

Decided by `heroic-route` (pass): ten calls placed seven Heroic (9) and three Satanic (6) items, no charm among them, with S10's spawns as the control.

`heroic-route:` (S14) takes one of these values:

- `heroic-only`: the control placed, the calls placed items, and the placed
  rarities include Heroic (9) and no Satanic (6); the histogram is the note.
  It means no Satanic among the n items placed, not that the roll cannot
  give one: the static reading says it can.
- `heroic-and-satanic`: as above, with both 9 and 6 among them.
- `no-heroic`: the control placed and the calls placed items, and none of
  them is Heroic (9); the histogram is the note.
- `call-not-engaged`: the control (S10's three `ground=new id=` spawn lines)
  did not place, or the heroic calls threw, answered `st=` or read `nothing
  placed`, so the call shape was not shown to reach the roll and nothing is
  said about it.
- `not-observed`: the control placed and the calls placed items, but no
  placed item's rarity could be read.

aimbot-rarity: 6

Decided by `repo-rarity` (pass): Satanic, as the Item Editor's catalog lists it; the control Goburin's Head read 10.

`aimbot-rarity:` (S12): the rarity code S12 read, or `not-observed`.

sprite-draw: partial

Decided by `sprite-draw` (fail, partial) beside a verified key `11`: the bag drew our icon, the ground drew the game's error placeholder, and the tooltip draws no icon for any charm.

`sprite-draw:` (S19): `all` (the grid, the tooltip and the ground all drew our
icon), `partial` (some did; the note names which), `none`, or `not-run` (the
icon did not load, or no key passed S18's `sprite verify`, so nothing was
written under a key shown to be the drawn one). A `none` is a finding about
the game only beside a verified key.

sprite-save: safe

Decided by `sprite-save` (pass): the relaunched game loaded and the charm drew a base small-charm icon; judged by the icon, because its stored sprite value could not be read in the bag.

`sprite-save:` (S20): `safe`, `persisted`, `crash` or `not-run`.

## Not established

Live 1 (2026-10-12) answered most of what was open before it: what holds a
walk (the answer `InstancePlaceTallest` gives the player object's Step event),
what happens to a player left inside a prop (stuck), where the worn charms
are (`localCharmItemArray`), which keys hold the footprint (`"2"` and `"3"`),
that a 2x2 footprint written on a built charm is honoured, that a reload
without the size rule restores the base footprint safely, that a forged stat
counts on the sheet, what `DropItemHeroic` places, Aimbot.exe's rarity, and
that key `11` holds the display sprite. What is still open:

- **Whether answering `InstancePlaceTallest` alone frees a walk.** Every lever
  that crossed answered the builtins too (`all` or `any` with `scripts`); a
  lever answering only the scripts was not run. Only one prop (the ravine
  edge south of the town) and one building wall were walked, both in
  `Town_01_rm`; map edges, gates, water and other rooms were not.
- **Where the dressed charm went in S17.** After a reload with a 2x2 rule on
  a charm whose neighbours were occupied, it was neither at its cell nor on
  Extra tabs 1 to 3; the Main tab and Extra tab 4 were not checked, and the
  saves were restored. Whether the game dropped it, moved it, or kept it
  undrawn is not established; `dress-added: item-missing` records only that
  it was not found where it was looked for.
- **Whether every reader of a size goes through `GetItemSize`.** The bag
  honoured the 2x2 footprint; other grids (the charm grid, the stash, the
  crafting grid) were not tried.
- **Why the walk did not get faster** although the sheet's Movement Speed
  rose by 100 points (the inventory was open during both timed walks, and the
  owner notes a soft cap at 228%), and whether a forged stat can be kept
  across a save: after S16's reload it was gone from the charm.
- **Whether a ground item can draw our sprite.** The ground drew the game's
  error placeholder for it, while the bag drew it; the tooltip draws no icon
  for any charm. Whether the ground reads the sprite another way was not
  investigated.
- **What `DropItemHeroic` does with the game's own callers' `self`** (a vault
  or an NPC rather than the player), and whether it ever gives a charm: ten
  calls gave none.
- **Whether the bag holds the stored sprite index after a relaunch.**
  `charmprobe charms` lists the charm grid only, so `sprite-save` was judged
  by the icon.
- **What `CanMove` and `TilePlaceMeeting` are for.** Neither was seen to be
  called during a walk (not observed through either route, with the control
  logging); that is not evidence they are never called.
- **Where the walk's collision test lives inside `PlayerMovement`**: it does
  not decompile, and Live 1 found the walk's blocking query in the player
  object's Step event instead, so what `PlayerMovement`'s own meeting
  builtins decide is still not established.

## Phase 2: the mod

Phase 2 builds the three charms against § Decision: built-in Custom Forge
entries with a guarded size, our own icons in the bag and on the ground, the
flight lever, drawing the flying player in front of scenery, and drops behind
panel switches that are off by default. Its plan read five more things about
the game before any code was written. They are recorded here in short and in
full, labelled, in the toolkit's `docs/RUNTIME_DATA_MODELS.md`.

### Phase 2 static reading

Read on 2026-10-12 in the build the Ghidra project imported (the executable
hash in § Live 1 results), through the ghidra MCP and existing local
decompiles, with direct call sites from a local byte scan for calls to each
entry. The collision family was read from the game's data file (`data.win`),
whose object and sprite tables give names, parents and sizes. None of it is
measured unless it says so, and an empty caller list through the MCP is "not
observed".

- **Draw order** (RDM § 19.7). `UpdateDepth` records an instance's y and,
  by default, sets its `layer` to the layer the room's `global.gameLayer`
  holds at that y (clamped to 0..5500). The player object's Step event calls
  it from two of its 2746 direct call sites, so the game re-sets the player's
  draw layer from its y every Step, and an order set before that call does
  not last (inference). Which end of those layers draws in front is not
  established.
- **The collision family** (RDM § 19.8, from the data file). `Wall_Parent_obj`
  is a child of `Collision_Parent_obj`, and the walls, the eight invisible
  walls and `Gate_Parent_obj` with the zone gates sit under it. `Lock_obj` is
  a direct child with no descendants. The ravine edge's blocker in Live 1,
  `Block_Dynamic_Sprite_obj`, sits under `Block_obj`, so it is in the family
  but is not a wall. The family holds 1824 objects. This corrects the
  `walk-objects` row of § Live 1 results, which had counted `Wall_Parent_obj`
  outside the family; the tally itself had labelled it `family`.
- **Placing a rebuilt item** (RDM § 23.6). `ParseItemToGrid` rebuilds each
  saved item and places it at its saved cell through
  `InventoryGridAddItemPos`, which answers false and writes nothing when any
  cell of the footprint is taken or off the grid. `ParseItemToGrid` ignores
  that answer, which is the reading behind S17's `dress-added:
  item-missing`: the dressed charm did not fit and was left off the grid.
- **The ground sprite** (RDM § 23.7). `LootGroundInit` gives a new ground
  item the sprite named `Pickup_` plus the name of the item's display sprite
  (key `"11"`), and `Error_spr` when no such sprite exists. The game ships a
  `Pickup_` ground sprite beside each charm icon (13x13, 17x11 and 34x17 for
  the three read). A sprite added at runtime has a generated name with no
  such companion, which is the reading behind `sprite-draw: partial`. The
  player build already detours `LootGroundInit` once, shared by hidden loot
  and the loot announcement, and a second detour on it is refused.
- **The ordinary unique roll** (RDM § 23.8). `DropUniqueItems` reads one
  random entry of its sub-list and drops nothing when that entry fails its
  tests: no second draw, no die per entry. One added entry is therefore
  picked on 1 call in the sub-list's length plus one, between 1 in 51 and 1
  in 222 for the measured lengths.
- `InstancePlaceTallest` gathers the instances at a place and answers the
  tallest, or `noone`, as § 19.6 of RDM already says.

**Monsters** (RDM § 19.9), read on 2026-10-13 after the owner asked for a
flying player to walk through monsters. From the data file's object table,
`Enemy_Parent_obj` (1429), a child of `Avoidable_Parent_obj`, has 336
descendants under four direct children (`Enemy_Child_Basic_obj`,
`Enemy_Child_Boss_obj`, `Enemy_Child_Destructible_obj` and
`Demon_Lightning_obj`); the destructible child's six children are boss parts,
so they count as monsters too. `Player_obj`, `Mercenary_obj` and
`Summon_Parent_obj` sit under `Enemy_Aggroable_obj`, which has no parent, so
none of them is in the monster family, and `Enemy_Only_Passage_obj`, which the
blocked walk's tally named (§ Live 1 results, `walk-objects`), has no parent
either: it is not a monster and keeps blocking. The training dummies,
`Attack_Dummy_obj` and `Attack_Dummy_Boss_obj`, descend from
`Enemy_Child_Basic_obj`; the data file's room table places seven plain dummies
and one boss dummy on `Town_01_rm`'s `Spawners` layer, two of the plain ones
standing apart at (1816, 704) and (1736, 544). As a static reading, a local
byte scan finds exactly one direct call to `StatPhasing`, the getter of stat
26 "Movement Phasing", inside the stat dispatcher `ReturnSpecificStat`; how the
movement uses stat 26 is not established, because neither its decompile nor
the movement's completed through the ghidra MCP. The walk's `place_meeting`
named `Enemy_Parent_obj` 3015 times in Live 1's tally (measured), but that a
monster in the path is what holds a walk is not established. Mod live 3
walked into a dummy and found that a dummy does not hold a vanilla walk, so
the question stays open for live monsters (§ "Mod live 3 results"); the same
session measured `StatPhasing` entered on every step.

### Why the canopy covered the flying player

Mod live 1 (2026-10-12) failed five checks, for three reasons in the game
that this section records. The replanned work
read more on 2026-10-13, in the same build, through the ghidra MCP, the
game's data file and a byte scan of the imported executable. Each fact is in
RDM in full, labelled static reading, measured or inference; here they are in
short.

- **The canopy is not on a game layer** (RDM § 19.7). The write landed: the
  player's layer read depth −5500, the front end of the room's
  `global.gameLayer` list, and he was visible on open ground. A canopy still
  covered him at both spots probed. The canopy object's Create moves it onto a
  room layer of its own, `Tree_Leaves`, which sits at −9700 in town, in front
  of every game layer, and carries the sway effect. So the front of the game
  layers is not the front of the room (inference). The same section records
  the colour-filter layers some rooms keep in front of that, the 101 rooms
  that have one, and the 8 rooms where a filter lies behind a layer the game
  draws.
- **A tooltip for an item with no stats** (RDM § 16.6). The gold rows wait
  for the first stat call whose stat the item has, and Pocket Elf has none,
  so its `Flight` row never drew. The tooltip function holds all of this
  build's direct stat calls in one run, behind one branch that reads the
  tooltip's surface fields, which suggests a per-item tooltip cache
  (inference, not measured). Whether the run is made for an item with no
  stats at all is not established; Mod live 2 measures it.
- **One flight charm at a time** (RDM § 23.1). The game lists one charm per
  definition `b` above a rarity line, and all three flight charms carry `b`
  0, so with the Wings and the Broomstick both worn only the Wings were
  listed (measured, S14 and S17). The owner confirmed that this is the
  game's own rule, so it is not fixed; Mod live 2 wears each charm alone.

### The items

Three built-in Custom Forge entries, recognised and dressed the way the
signature items are. Each is saved as a plain normal Small Charm (type 10,
`b` 0, `c` 0, `j` 0) whose seed `a` falls in its own range, and its rolls are
decoded from the seed's offset into that range:

| | Pocket Elf | Gabriel Wings | Witch's Broomstick |
| --- | --- | --- | --- |
| word (`flightcharm`, panel key) | `elf`, `mod_flight_elf` | `wings`, `mod_flight_wings` | `broom`, `mod_flight_broom` |
| rarity (`"27"`) | 6, Satanic | 7, Angelic | 10, Unholy |
| tier (`"32"`) | 5 (SS) | 5 (SS) | 5 (SS) |
| footprint (`"2"` x `"3"`) | 1x1 | 2x2 | 1x5 |
| stats | none | 159 and 160, each 1..12 | 25, 10..100 |
| seeds | 777100 | 777200..777343, one per (physical, magic) pair | 777400..777490, one per value |

Every charm shows one gold affix row reading `Flight`, carries `fp_mechanic`
`flight`, and has a line of lore. Its native stats are removed. On
`CreateItemNew`'s outermost return the dressing also writes the footprint
under `"2"` and `"3"` and our bag icon under `"11"`, before the item's hash is
refreshed. A sidecar entry for the same item is matched first and still wins.

The seed layout is a save format: a save keeps only the compact definition, so
changing a range after release re-rolls every copy a player owns. With
ForgePact removed, a charm loads as a plain 1x1 Small Charm (Live 1's
`dress-removed: safe`). With ForgePact installed, a charm is dressed whatever
its switch says. Dressing a charm arms flight; it never installs a hook from
inside `CreateItemNew`.

### Flight

The lever is the one Live 1 measured crossing (`all scripts`), narrowed to the
rows that answered there: the five builtins jumpscenery already hooks
(`position_meeting`, `place_meeting`, `collision_line`, `instance_position`
and `collision_circle`) and `InstancePlaceTallest`. The five builtin rows that
answered nothing in Live 1's S7 (`place_free`, `instance_place`,
`collision_point`, `collision_rectangle` and `tilemap_get_at_pixel`) stay
unhooked, and so do `CanMove` and `TilePlaceMeeting`, which were never called.

Only the local player's own blocked queries are touched, and only while
flight is on (a flight charm is in `localCharmItemArray`, read as variables,
offline only) or residual. The equipped state is refreshed at most once a
frame, by the first such query of that frame, at the point of use. The
original always runs first and a free answer stands. A blocked answer becomes
"nothing there" when the object it names is `Collision_Parent_obj` or a
descendant, is not `Wall_Parent_obj` or `Lock_obj` or a descendant of either,
a re-run of the same query against each of those two blocks nothing, and
every point the query names lies inside the room. `InstancePlaceTallest`'s
answer is tested the same way, by the object of the instance it returned.

Monsters (the owner, 2026-10-13, "flight should ignore enemy collisions",
read as body collision only) are not in the family. A blocked `place_meeting`
of the player that names `Enemy_Parent_obj` or a descendant is answered
"nothing met" under the same conditions: every point inside the room, and the
re-run against walls and locks blocking nothing. It counts `monster=` on the
trace line, inside `answered=`. A monster named on any other row
(`position_meeting`, `instance_position`, `collision_line`,
`collision_circle`), and an `InstancePlaceTallest` answer whose instance is a
monster, keep the game's answer and count `monster-kept=`, inside `other=`: a
point or line query at a monster may be how a target is picked, and
`place_meeting` is the mask's own test. That is a judgement, not a reading;
Mod live procedure 3's `monster-rows` measures it. Anything else keeps its
real answer.

Taking the charm off while overlapping the family or a monster leaves flight
residual: the answers go on, monsters' included, until the saved original
`place_meeting` at the player's own position reads free against
`Collision_Parent_obj` and then against `Enemy_Parent_obj`, and then flight is
off. The player is never moved.

The five builtin detours are one shared install, used by jumpscenery and by
flight. Each runs the original once, then lets jumpscenery decide (with its
switch on), then flight. jumpscenery's behaviour and its stat line are
unchanged. Online, flight reads nothing and stays off.

### Drawing in front

`UpdateDepth` is hooked by its SDK name. After the game's own call, when
`self` is the local player and flight is on, the player's `layer` is set to
the **flight layer**, a room layer of our own named `fp_flight`. `yDepthSet` is
left as the game set it. For any other `self` the detour costs one compare and
one count (the trace line's `depth-other=`), and it is installed only once
flight is armed.

The first design wrote the front end of the room's `global.gameLayer` list.
Mod live 1 measured that write landing at −5500 with a canopy still over the
player, because canopies sit on `Tree_Leaves`, in front of every game layer
(§ "Why the canopy covered the flying player"). So the flight layer's depth is
one below the lowest of the game layers' front end and every drawn layer
(visible, holding an element) behind the room's rearmost filter layer. A
filter layer is visible, holds no element, carries a layer effect (the
effect read answers a struct) and lies in front of the game layers' front
end; the rearmost is the one with the highest depth. Drawn layers in front of
it stay in front of the player, so he keeps every tint he has on the ground
(the owner, 2026-10-13, "Never past a filter"). From the data file that gives
−10001 in town and in the Outskirts of Inoya once its canopies exist, and
changes the answer against "in front of every drawn layer" in 8 rooms only.

A census runs once per room visit, at the first write while flight is on, and
remembers the layers in front of the game layers; a re-choice reads their
visibility and element count again at most once a second, so a zone's
`Tree_Leaves` that fills after the census (an empty one reads as a filter)
moves the layer within a second. A remembered layer that becomes visible and
empty after the census without its effect ever having been read (it held
instances, or was hidden, at the census) has its effect read once at that
re-choice, so the flying player is not drawn in front of a filter that
appears after the census (PR #214's review, round 1; the harness's
`an_effect_layer_that_empties_is_a_filter` and
`a_filter_shown_after_the_census_is_counted`).
Before each write the cached layer must exist and carry our name; a failed
check starts a new visit. A census that yields no layer writes nothing and
counts `no-front=`. Each effect read is counted as a struct, another answer
or a failure (`flightprobe depth`'s `fx=`, the trace line's `fx-struct=`,
`fx-other=` and `fx-failed=`), because a read that never answers a struct
would silently turn the filter rule off. `flightprobe depth` also gives the
kind of the visit's first answer that was not a struct (`fx-kind=`, an
`RValueType` number), and accounts for every layer whose effect the census
did not read (`unread=`, one count per reason: behind the game layers, no
readable depth, an unreadable name, visibility or element count, the flight
layer itself, hidden, or holding elements). Every layer `layer_get_all`
listed lands in exactly one of those nine counts, and `layers=` is their sum.
That separates a census that never read an effect (`fx=0/0/0`, with
`unread=` saying why) from a read that did not answer a struct, so the first
is never mistaken for a fact about `layer_get_fx`.

What the effect read supplies matters as much as what it answers. The census
hands `layer_get_fx` the layer as `layer_get_all` listed it (the id value the
runtime answered, or the id as a number), never the layer's name. The other
layer builtins the census calls take an id or a name, but nothing in this
repository shows that `layer_get_fx` takes an id on this runner, and
GameMaker's manual documents the FX builtins with a layer name. So a struct answer shows the id
shape works, while a read that throws, is refused or answers undefined or a
real (−1, say) cannot tell "this runner does not answer a struct" from "this
builtin wants the name". Such an outcome is recorded here, naming the
argument supplied (a layer id), and is not a runner fact until a read that
passes the layer's name answers the same.

What the effect read answers on this runner is **not established**. It is
**established only by** a reading that names the layer the census classed as
the filter: `flightprobe depth`'s `filter=` naming `Effect_1` with an `fx=`
struct count of 1 or more, in `Town_01_rm` (Mod live procedure 2's
`flight-depth`, town's `Effect_1` being the positive control) and in the
Outskirts of Inoya once its canopies hold (`flight-depth-zone`). A matching
flight depth is not evidence, either way. In both rooms `Effect_1` lies in
front of every drawn layer (−10300 in town, −10100 in the zone, with the
frontmost drawn layer at −10000 in each), so "behind the rearmost filter"
and "in front of every drawn layer" give the same −10001. A census that read
no effect at all, or read one and classed nothing, would still land there.
The depth tells the filter rule apart from replan 2's only in the 8
colour-filter rooms, none of which Mod live 2 visits.

### The size guard

`ParseItemToGrid` and `InventoryGridAddItemPos` are hooked by their SDK names,
installed with the Custom Forge's own hooks, because the dressing they protect
runs on every load. Each load opens a scope. Inside it, a placement of a
flight charm larger than 1x1 is held back and answered true (the load ignores
the answer, RDM § 23.6). When the load returns, every vanilla item of that grid
is in place; each held charm is then placed at its full footprint, or, when
that is refused, written 1x1 under `"2"` and `"3"`, its hash refreshed, tagged
`fp_flight_guarded`, and placed at its own saved cell (`kept=`, or `failed=`
when even that is refused). Vanilla items always go in first, so no vanilla
item can be displaced by a charm. Between two charms, the yield rule decides:
a held charm whose full footprint would cover another held charm's saved cell
goes straight to 1x1 at its own cell (`yielded=`, counted in `kept=` too), so
every full placement leaves every other held charm's cell free, and no save
order of two charms leaves one off the grid. The footprint is tested in both
orientations, `"2"` columns by `"3"` rows and the transpose, because which of
`InventoryGridAddItemPos`'s arguments is the column and which the row is a
static reading only (RDM § 23.6 does not record it). When one of the held
charms' cells is unknown, every held charm in that load yields, since the
guard cannot tell what covers what; a charm alone in its load is never shrunk
for that. The price is a charm loading 1x1 for a session when only its
transposed box touches another charm: a shrink, never a loss. Outside a load
nothing changes: a drag onto a cell where the charm does not fit stays
refused, as in the game. A guarded charm stays 1x1 for the rest of the
session. Whether grids other than the bag place through this route is not
established; `stash-size` and `charm-grid-size` look.

`flightcharm stat` prints the guard's own line after the stat and trace lines:
`flightcharm guard: route= opens= seen= charms= deferred= full= kept= yielded=
failed=`. Read after a load, in order, it says where a quiet guard stopped: a
`route` other than `on` means the guard was refused at startup; `opens` not
risen, that no load reached the hook; `seen` not risen, that no placement
inside a load reached it; `charms` not risen, that `A[1]` is not the item or
the item was not recognised; `deferred` not risen, that the footprint read 1x1
when the item was added. Otherwise `full`, `kept`, `yielded` and `failed` say
what the flush did.

### The ground icon

Six PNGs, the owner's own art, sit in `assets/flight-charms/` with their
manifest: a bag icon and a ground copy per charm (bag 32x32, 64x64 and 32x160;
ground 16x16, 24x24 and 34x17). `make_flight_charm_icons.py` embeds them in
the plugin and checks them. At the first dressing that needs one, each is
written under `bp_ipc\flightcharms\` only when no file of that name is there
and loaded with `sprite_add`, once a session. A failed load writes nothing
and the charm keeps the Small Charm's icon.

The ground draws `Error_spr` for a runtime sprite because the game looks for a
`Pickup_` companion by name (RDM § 23.7). So flight rides hidden loot's one
shared `LootGroundInit` detour as its third consumer, after hidden loot and
loot announcements: after the game's call, when the new ground item holds a
flight charm wearing our bag icon, its `sprite_index` becomes our ground copy
(`ground=` on the stat line). It never takes a detour of its own.

### Drops

Each panel switch gates only its own charm's drop, on #74's machinery (the
typing, hit and rewrite hooks of the Angelic roll, RDM § 13.4):

- **Gabriel Wings and Witch's Broomstick** join the Angelic roll's
  `lootListUnique[5]` for the length of each roll, through a stand-in of the
  same shape (Water Melon for the Wings, Soul Collector for the Broomstick). A
  hit on the stand-in falls to the charm with the added entry's share, and its
  record is rewritten at `CreateItemNew`'s entry to the charm's definition,
  with a uniformly drawn roll, so the game builds it.
- **Pocket Elf** joins the ordinary unique roll. `DropUniqueItems` is hooked
  by its SDK name, only from `flightcharm elf 1`, and for the length of each
  call one stand-in entry goes onto each of `lootListUnique[0..4]` and comes
  off again. Because that roll draws once and cannot say which sub-list it
  drew from (RDM § 23.8), the coin is exact only when the stand-in occurs
  equally often in all five. The stand-in is therefore the first of Aimbot.exe,
  Witches Claw, Traveler's Map Journal, Crow's Feather and Hello its me,
  Steve! with the same count in every sub-list that also passes the roll's own
  filter; with none, the switch refuses and says so.

The stand-in keeps its own rate, and ForgePact adds no die. The research
build's `flightprobe copies <k>` pushes k entries per charm instead of one, so
a live session sees hits; `sigdrop status` is unchanged and the flight
counters print on `flightcharm stat`.

### Hooks

Everything is resolved by name, and every script hook goes through
`HookOneScript` and refuses when only the table swap went in:

- with the Custom Forge's hooks, at startup: `ParseItemToGrid` and
  `InventoryGridAddItemPos` (the size guard);
- once flight is armed and a character resolves (looked for every 60 frames
  through `HhResolveLocalPlayer`): the shared `LootGroundInit` detour,
  `InstancePlaceTallest`, `UpdateDepth` and the five shared builtins. A
  refusal prints one `flightcharm: refused - <why>` line and is final for the
  session;
- from a drop switch turned on: #74's detection hooks, `DropUniqueItems`
  (Pocket Elf only) and the shared `LootGroundInit` detour.

In the research build flight refuses while `citrace` or `jumpprobe` holds one
of its hooks, and those two refuse while flight holds them; the research rows
that hook `DropUniqueItems` and Pocket Elf's switch refuse each other the same
way.

### Mod live procedure 1

The session that proves the mod, on the research build (`build.bat dev`) at
the commit where every code item and the docs are done. The owner installs the
DLL when asked, and its SHA-256 is recorded. Like phase 1's, this procedure
leaves the debugger's addresses out on purpose: they never enter a tracked
file and are kept in the session's local notes.

**Setup.** Save slot 14, "Sorak", in `Town_01_rm`. The saves are backed up
before the launch and restored at teardown whatever happened. Before the
launch the operator confirms that the build the debugger addresses were read
from is the one installed; otherwise the session is aborted. At character
select, `jumpscenery 0`. None of `jumpprobe hook`, `citrace 1`,
`angelicprobe on`, `gambaprobe`, `prospectprobe` or `craftprobe` is used,
because each holds a script or builtin flight needs; `raredrop angelic` and
`angelicprobe hit chance` are #74's levers and are allowed. Teardown: the
debugger detached first, then the game stopped, the saves inspected and
restored, and the lease released.

**Cases.** One family prop, with a wall spot as the outlier; one tall prop for
the draw order; the bag for the guard, with the charm grid and the stash as
the other grids; each drop route once; all three icons. Not tried, and so
"not observed live": monsters, gates, locks, map edges outside town, co-op,
and drops at the natural chance (every drop here uses a research lever).

**Steps, in order:**

- S0 `dll-hash`: the installed DLL's SHA-256 equals the build's.
- S1 `marker`: a bare `flightprobe` begins `flightprobe: marker=flp1`;
  `control`: `ping` answers `pong (YYTK 4.0.1)`.
- S2 `fresh-off`: `flightcharm stat` is exactly the fresh-session line, every
  switch off, `install=not-armed state=off equipped=none`, every counter 0,
  `icons=not-loaded`, `gate=off` and `standin-elf=none`.
- S3 `ground-control`: `charmprobe spawn 0` answers `ground=new id=<n>
  placed`, and `flightprobe ground` names a `Pickup_` sprite of the game's
  Small Charm and reads `game`.
- S4 `ground-icon`: `flightprobe spawn` for each charm (the elf, the wings at
  12 and 1, the broom at 100), each followed by `flightprobe ground`; all
  three read their own name and `ours`, and a screenshot shows our three icons
  on the ground and no ERROR sprite.
- S5 `bag-icon`: the person picks all four into the bag and hovers each flight
  charm. A bag screenshot shows our icons, the Wings on 2x2 cells and the
  Broomstick on 1x5; each tooltip shows its name, its rarity's colour and a
  gold `Flight` row.
- S6 `walk-baseline`: with the charms in the bag, not worn, `flightcharm stat`
  reads `install=installed state=off`, and a 3 s walk down at phase 1's
  ravine edge does not move the player.
- The debugger is attached and four logpoints armed, the control first:
  `dbg-control` on `CheckTalentUse`, then the player Step event's two calls
  to `UpdateDepth` and `ParseItemToGrid`'s call to `InventoryGridAddItemPos`.
- S7 `depth-control`: the person stands Sorak just north of a tall prop that
  covers him and names it. A screenshot shows him hidden, and `flightprobe
  depth` reads `state=off` with `depth=` different from `front=`.
- S8 `equip-detect`: the person drags Pocket Elf into the charm grid; the stat
  reads `state=on equipped=elf`.
- S9 `flight-depth`: `flightprobe depth` reads `depth=` equal to `front=` with
  `state=on`, and a screenshot shows Sorak in front of the prop. Which end
  (`front=` against `back=`) was chosen is recorded, with the
  `flightcharm trace:` line before and after, so that a fail names its cause
  (`depth-calls`, `depth-other`, `depth-idle` or `no-front`, read as the
  `TraceLine` comment in `FlightCharms.hpp` says).
- S10, choosing the spot: at the ravine edge and at the church wall,
  `flightprobe free` toward the edge, once for the family and once for
  `Wall_Parent_obj`. The prop spot is the first whose family read is `yes` and
  whose wall read is `no`; a spot whose wall read is `yes` is the excluded
  spot.
- S11 `flight-prop`: at the prop spot, a 3 s walk across it moves the player
  100 px or more past the start, and `answered=` rises. The
  `flightcharm trace:` line is recorded before and after, so that a fail
  names where flight stopped (`calls`, `not-player`, `free`, or the bucket of
  `seen` that rose).
- S12 `flight-excluded`: at the excluded spot, a 3 s walk passes when the
  player moved under 20 px and `excluded=` rose, fails at 100 px or more, and
  is not-observed when no spot read `yes` for the wall.
- S13 `flight-ends-inside`: a short step into the ravine edge leaves the
  player inside it (`flightprobe free` reads `blocked=yes`); the person moves
  Pocket Elf to the bag and the stat reads `state=residual`. Walks of 1 s, at
  most three, end with `blocked=no` and `state=off`, the position changing on
  the first; a 3 s walk back then stops at the prop's side. No movement while
  residual fails, and `playerwarp` recovers.
- S14 `charm-grid-size` and `rolls`: with the Wings and the Broomstick in the
  charm grid, `charmprobe charms` lists `Gabriel Wings size=2x2 rarity=7`
  with `itemstats=` exactly `159=12;160=1`, and `Witch's Broomstick size=1x5
  rarity=10` with exactly `25=100`; a screenshot is taken.
- S15 `stash-size`: the person drags the Broomstick into an empty stash tab; a
  screenshot of 1x5 cells passes, 1x1 fails. It goes back to the charm grid.
- S16, the guard's setup: `flightprobe dress off`, a Wings spawned at 3 and 4,
  dropped by the person on a bag cell whose right and lower neighbours are
  occupied (it is 1x1 now; screenshot), then `flightprobe dress on`. Two more
  Wings are spawned and dropped by the person at full size on free bag cells,
  away from the 3/4 Wings and its right, lower and lower-right neighbours
  (screenshot); the person checks that the worn Wings and Broomstick in the
  charm grid do not cover each other's top-left cell either way round, and
  moves the Broomstick if they do. The `flightcharm guard:` line is then
  recorded as the before value. The bag alone gives `guard-control` 3
  deferred and 2 full; whether the charm grid also loads through the guard is
  not established, and the session's validity does not rest on it.
- S17, the reload: save and quit to character select, and slot 14 again.
  `guard-kept`: the 3/4 Wings is at its cell at 1x1, both neighbours are
  intact, the stat reads `kept=1 failed=0`, and the guard line reads
  `route=on` with `kept=` risen by 1 since S16, `yielded=0` and `failed=0`;
  a fail records the guard line and names the cause, read in the order
  § "The size guard" gives. `guard-control`: the guard line reads `route=on`
  with `deferred=` and `full=` each risen by at least 2 since S16, and the
  worn Wings is 2x2 and the Broomstick 1x5 (a `deferred=` rise of 5 or more
  suggests the charm grid loads through the guard too, a note for
  `charm-grid-size`). `reload-identity`: the same names, rarities and
  `itemstats=` as S14.
- The debugger is detached, its reply and the outside thread check quoted.
  `dbg-step-depth` passes when either `UpdateDepth` call logged during S7 to
  S9 while the control logged; `dbg-parse-place` passes when the placement
  call logged during S17's reload while the control logged.
- S18 `unique-control`: with every switch off, `flightprobe unique 10` reads
  `placed=` of 1 or more and `elf=0`.
- S19 `drop-elf`: `flightcharm elf 1` reads `gate=on` and `standin-elf=`
  names one of the five candidates; `flightprobe copies 200`, then
  `flightprobe unique 20`. Passes when `elf=` reads 3 or more, the stat's
  `elf=<ours>/<built>` has two equal values of 3 or more, `refused=0` and
  `anomalies=0`, and `flightprobe ground` reads `Pocket Elf ours`.
- S20 `drop-angelic`: in `Act_01_01`, `raredrop angelic 2`, `angelicprobe hit
  chance 1000000000`, `flightcharm wings 1` and `flightcharm broom 1`; the
  person kills in batches of 10, at most four, until both charms have one
  built. Passes when ours equals built for both, `refused=0` and
  `anomalies=0`, and a hovered ground drop reads Gabriel Wings or Witch's
  Broomstick.
- S21 `drop-off`: all three switches off (`switches` all `off`), then
  `flightprobe unique 10` and one batch of 10 kills leave `elf=`, `wings=`,
  `broom=` and `injected=` unchanged.

**Checks**, in the capture's order: `dll-hash`, `marker`, `control`,
`fresh-off`, `ground-control`, `ground-icon`, `bag-icon`, `walk-baseline`,
`dbg-control`, `depth-control`, `equip-detect`, `flight-depth`,
`dbg-step-depth`, `flight-prop`, `flight-excluded`, `flight-ends-inside`,
`charm-grid-size`, `rolls`, `stash-size`, `guard-kept`, `guard-control`,
`reload-identity`, `dbg-parse-place`, `unique-control`, `drop-elf`,
`drop-angelic` and `drop-off`.

- **Validity**, which must pass: `dll-hash`, `marker`, `control`,
  `ground-control`, `walk-baseline`, `dbg-control`, `depth-control`,
  `guard-control` and `unique-control`.
- **Acceptance**: `fresh-off`, `ground-icon`, `bag-icon`, `equip-detect`,
  `flight-depth`, `flight-prop`, `flight-ends-inside`, `charm-grid-size`,
  `rolls`, `guard-kept`, `reload-identity`, `drop-elf`, `drop-angelic` and
  `drop-off`.
- **Research**, where a fail or a not-observed is itself the finding:
  `dbg-step-depth`, `flight-excluded`, `stash-size` and `dbg-parse-place`.

**What it relies on.** The ravine edge and the church wall positions from
phase 1's Live 1 (S5 and the wall repeats); the blockers,
`Block_Dynamic_Sprite_obj` and `Wall_Parent_obj`'s family (RDM § 19.6,
measured, and § 19.8, from the data file); `InstancePlaceTallest` (§ 19.6,
measured); `UpdateDepth` (§ 19.7, static); `ParseItemToGrid` and
`InventoryGridAddItemPos` (§ 23.6, static); `LootGroundInit` (§ 23.7,
static); `DropUniqueItems` (§ 23.4 and § 23.8, static: its by-name call shape
is not proven, which is why `unique-control` is its control); the Angelic
roll and its hooks (RDM § 13.4 and `angelic-roll-hook-research.md` § Session
3 to 5, measured); the charm read and the size and sprite keys (RDM § 23.1,
§ 23.2 and § 23.5, measured).

### Mod live procedure 2

The session that re-runs what Mod live 1 failed or did not observe, each with
the control it needs, and adds the owner's drop-rate check. What Mod live 1
passed is not run again. It runs on the research build (`build.bat dev`) at
the commit where the flight layer, the stat-less tooltip anchor, the `flp2`
probe and these docs are done, and where `flightprobe depth` already carries
the census outcomes (its `unread=` and `fx-kind=` fields); the driver records the DLL's SHA-256 and asks
the owner before installing it. As in procedure 1, the debugger's addresses
stay out of this file: they are kept in the session's local notes.

**Setup.** Save slot 14, "Sorak", in `Town_01_rm`. The saves are backed up
before the launch and restored at teardown whatever happened, and the
operator confirms that the installed game is the build the debugger addresses
were read from, or aborts. At character select, `jumpscenery 0`. The session
starts from the restored saves, so no flight charm is owned and Sorak's own
charms fill the charm grid. The operator drives every person-step itself
through hs-drive (keys, holds, clicks, `playerwarp`, the waypoint) and lists
each in the capture; it spawns ground items away from the waypoint, whose
sprite hid them in Mod live 1, and prefers right-click and keys in the bag,
because a left click also sends Sorak walking. None of `jumpprobe hook`,
`citrace 1`, `angelicprobe on`, `gambaprobe`, `prospectprobe` or
`craftprobe` is used; `raredrop angelic`, `angelicprobe hit chance`,
`flightprobe copies`, `zonecensus`, `tiptrace` and `charmprobe` are allowed.
Headhunter's and Tyrant's Crown's switches stay off. Teardown: the debugger
detached first, then the game stopped (its exit code is recorded as a note:
both earlier sessions ended with a stack-buffer exit on a graceful close),
the saves inspected and restored, and the lease released.

**Cases.** Draw order: the town canopy Mod live 1 measured, on `Tree_Leaves`
at −9700 behind the town filter `Effect_1` at −10300, with a generated zone
(the Outskirts of Inoya, whose `Tree_Leaves` at −10000 fills at runtime,
behind a filter at −10100) as the outlier. Tooltip: Pocket Elf, the stat-less
case that failed, with Gabriel Wings as the control for the shared change.
One flight charm: the Broomstick alone, then beside the Wings. The guard: two
full Wings in the bag and the worn Broomstick across one reload. Drops: each
roll once, at the player build's single copy. Not tried, and so "not observed
live": Pocket Elf in a zone, the 8 colour-filter rooms where "Never past a
filter" changes the answer (both rooms tried have their filter in front of
every drawn layer, so the harness's `front_layer_filter` scenarios carry the
rule there, while the effect read itself is measured in both rooms by
`filter=` and the `fx=` / `unread=` tally, not by the depth), gates and
locks outside town, co-op, and the Angelic roll's natural chance.

**Steps, in order:**

- T0 `dll-hash`: the installed DLL's SHA-256 equals the build's.
- T1 `marker`: a bare `flightprobe` begins `flightprobe: marker=flp2`, which
  proves the new probe is the one installed; `control`: `ping` answers
  `pong (YYTK 4.0.1)`. `flightcharm stat` and `sigdrop status` are recorded.
- T2, the charms (setup): Sorak warps to the stash spot Mod live 1 used, away
  from the waypoint, and the three charms are spawned with a short walk
  between them so their labels do not stack: `flightprobe spawn elf`, the
  Wings at rolls 12 and 1, and the Broomstick at 100, each followed by
  `flightprobe ground`, which reads its own name and `ours`. The operator
  picks all three up and opens the bag for a screenshot.
- The debugger is attached and its logpoints armed (below). Because the
  tooltip is probably drawn from a cached surface after its first frame, the
  tooltip steps count only the first `tiptrace` stat line after each hover
  begins: the pointer leaves every item for a second, `tiptrace 40` is sent,
  the charm is hovered for two seconds with a screenshot, and the log is read.
  The first try whose first line reads a stat count above 0 ends the step; at
  most three tries.
- T3, Gabriel Wings' tooltip (debugger window A). `bag-icon-control`: exactly
  one gold `Flight` row, directly above the physical damage reduction line.
  `tip-calls`, first half: a try reads a stat count above 0 for the Wings.
  `dbg-stat-wings`: read from the logpoints.
- T4, Pocket Elf's tooltip (window B). `bag-icon-elf`: one gold `Flight` row
  between the charm's rarity line and its lore line, and a `tiptrace` line
  saying the forged rows were drawn. `tip-calls`, second half: a try reads a
  stat count above 0 for Pocket Elf. It passes when both halves read above 0;
  it fails when the Wings' does and all three of Pocket Elf's tries read 0
  (the stat block does not run for a stat-less item); it is not observed when
  all three Wings tries read 0 (the instrument is blind). Window C follows:
  the pointer on an empty bag cell for two seconds, with no tooltip.
  `dbg-stat-elf`: read from the logpoints. The debugger is detached here,
  before the depth steps.
- T5 `depth-control`: with no flight charm worn, `flightcharm stat` reads
  `state=off`. Sorak warps under the red canopy Mod live 1 measured, and a
  screenshot shows him hidden under it. `flightprobe depth` reads
  `state=off`, `flight-layer=none`, `filter=none`,
  `fx=0/0/0 unread=behind:0,no-depth:0,no-detail:0,own:0,hidden:0,holds:0`,
  `fx-kind=none` and `layers=0`: no census has run yet. A reply with no
  `unread=` field means the build lacks the census outcomes, which fails
  `depth-control`: the session would then measure nothing about the filter.
- T6 `layers-town`: `zonecensus`, then its JSON file. Its layer list has
  `Tree_Leaves` at −9700, and every counted `Fall_Dead_Tree_Leaves_01_obj`
  sits on `Tree_Leaves`. The count, the frontmost visible layer holding
  instances and the filter layer (`Effect_1`, visible, −10300) are recorded,
  and `Wind_Leaves` too if it reads visible (the data file says hidden). So
  is every layer the list puts in front of −5500, with its visibility: the
  data file has `Effect_1` visible, and `Wind_Leaves` and
  `Xmas_Event_Assets_1` hidden. It passes when every counted canopy sits on
  `Tree_Leaves`.
- T7 `flight-depth`: Pocket Elf goes into the charm grid, and
  `flightcharm stat` reads `state=on equipped=elf`. `flightprobe depth` then
  names a flight layer whose depth equals the player's depth and lies below
  −9700: the expected reply is `flight-depth=-10001` with
  `front=Invisible_walls@-10000`, in front of the canopies and the walls'
  layer and behind the filter at −10300. A flight depth below the filter's is
  a fail ("Never past a filter"). The filter part, the positive control on
  the effect read, passes when the same reply reads `filter=Effect_1@-10300`
  (or `filter=Wind_Leaves@-10200` when T6 recorded `Wind_Leaves` visible)
  with an `fx=` struct count of 1 or more. `fx=`, `unread=`, `fx-kind=`,
  `layers=` and the trace line's three `fx-` totals are recorded; the data
  file predicts `fx=1/0/0` and `hidden:2,holds:4`, with `behind:` holding the
  game layers and the rear room layers, which is a note, not a pass
  condition. `layers=` unequal to the sum of the nine counts is a fail: the
  instrument is wrong. `filter=none` fails even with the depth right, and the
  counts name the cause. A struct count of 1 or more means the census read an
  effect but classed no filter, a classification defect to compare with T6. A
  failed count of 1 or more with no struct means the read, given a layer id,
  throws or is refused. No struct and no failure, with `other` 1 or more
  and `fx-kind=` 5 (undefined) or 0 (a real), means `layer_get_fx`, given a
  layer id, does not answer a struct. Neither of those two is a runner fact:
  the census never passes the layer's name, so each is as consistent with
  the builtin wanting the name as with this runner (§ "Drawing in front"),
  and each is recorded with the argument supplied, a layer id. `other` with any other `fx-kind=` (15, a
  ref; 2, an array; ...) means the census's struct test is too narrow for
  what this runner answers, a defect in our kind test rather than a runner
  fact. `fx=0/0/0` means the census never read an effect, and `unread=`
  names why (`hidden`, `no-detail`, `no-depth`, or `behind` holding
  `Effect_1`): a census finding, never a fact about `layer_get_fx`.
  Screenshots at the
  canopy spot and a second spot under the same canopy show Sorak over it.
  `front=`, `layers=`, `census-ms=` and whether his colours match the scene
  are recorded, the last as a note for the owner. A fail names its cause: no
  layer chosen (`flight-layer=none` with `no-front=` risen), the write not
  landing (the player's depth unlike the flight depth), or the choice itself.
- T8 `flight-excluded`, Pocket Elf still worn: Sorak warps to where Mod live
  1's flying walk stopped, just north of an invisible wall column, and
  `flightprobe free` is asked 24 px south against `Wall_Parent_obj` (40 px if
  that reads free). With `excluded=` noted, S is held for a second and
  `flightprobe free` reads his position again. It passes when he moved under
  20 px with `excluded=` risen, fails at 100 px or more, and is not observed
  when neither read was blocked.
- T9 `broom-alone`: Pocket Elf back into the bag, Sorak's own 1x5 totem out of
  the charm grid to make room, and the Broomstick in. `charmprobe charms`
  lists `Witch's Broomstick size=1x5 rarity=10` with its item stats exactly
  `25=100`, and `flightcharm stat` reads `state=on equipped=broom`.
- T10: Sorak's own 2x2 book out of the charm grid and Gabriel Wings in.
  `one-flight`: `charmprobe charms` lists exactly one of the Broomstick and
  the Wings, and the stat line's `equipped=` names exactly one key, recorded;
  both listed is a fail, and means the owner-stated rule does not hold as
  stated. `unique-b0`: `charmprobe repo 10 0 0`, whose rarity, name and
  hidden flag (or `missing`) are recorded; it passes when it answers either
  way. The Wings go back into the bag, leaving the Broomstick the only flight
  charm worn.
- T11, the guard setup: at the stash spot a second Gabriel Wings is spawned
  (`flightprobe ground` reads `ours`) and picked up, both Wings are moved, at
  2x2, to free bag cells away from every other charm, and the bag is
  screenshotted. The `flightcharm guard:` line is recorded as the before
  value.
- T12, the reload: Save & Exit, Back, and slot 14 again, as Mod live 1's
  reload did. `guard-control`: the guard line reads `route=on` with
  `deferred=` and `full=` each risen by at least 2 since T11.
  `reload-broom`: `charmprobe charms` lists the Broomstick at 1x5 with exactly
  `25=100`, `flightcharm stat` reads `equipped=broom`, and a bag screenshot
  shows both Wings at 2x2.
- T13, drops in town. `unique-control`: with every switch off,
  `flightprobe unique 10` places one item or more and no Pocket Elf. `pool`,
  first reading: `flightprobe pool`, recorded whole. `rate-elf`:
  `flightprobe copies 1`, then `flightcharm elf 1` (the gate on and its
  stand-in named), then `flightprobe unique 20` five times, summing the five
  summaries' Elves and placed items. It passes when `drop_roll_model`'s
  `observation_within` accepts the Elf sum out of 100 calls between the shares
  1 in 222 and 1 in 51, run from the checkout root; at least 50 items were
  placed; the stat's Elf count reads ours equal to built; and `refused=0` and
  `anomalies=0`. The sum is recorded and the switch turned off.
- T14, the zone: from the waypoint, the Outskirts of Inoya, as Mod live 1's
  drops step went. `layers-zone`: with the Broomstick in the bag, so flight is
  off, Sorak walks in short holds until a screenshot shows a canopy hiding
  him (at most 10 walks), then `zonecensus` lists `Tree_Leaves` (−10000 in
  the data file) holding at least one instance and the filter `Effect_1`
  (visible, −10100 in the data file), and every layer in front of −5500 is
  recorded with its visibility (the data file has `Effect_1` and
  `Tree_Leaves`); not observed when no canopy hid him.
  `flight-depth-zone`: the Broomstick back in the grid, and
  `flightprobe depth` reads a flight depth below `Tree_Leaves`' and above the
  filter's; the expected reply is `flight-depth=-10001` with
  `front=Tree_Leaves@-10000`. A reading of −5501 against the game layers
  while the zone's `Tree_Leaves` is still empty (it then counts as a filter)
  is read again after two seconds, by which time the re-choice should have
  moved the layer. The same reply reads `filter=Effect_1@-10100` once the
  canopies hold, or `filter=Tree_Leaves@-10000` while the depth still reads
  −5501, with an `fx=` struct count of 1 or more. `filter=`, `fx=`,
  `unread=`, `fx-kind=`, `layers=` and the trace line's `fx-` totals are
  recorded; the data file predicts `fx=2/0/0` when `Tree_Leaves` was empty
  at the census and `fx=1/0/0` when it already held canopies, a note, not a
  pass condition. `layers=` unequal to the nine counts' sum, and
  `filter=none`, each fail, with the cause named from the counts as in T7;
  the check stays a Research one. A screenshot shows Sorak over the canopy:
  a fail when he is hidden, not observed with `layers-zone`. `pool`, second reading, here before the kills.
  `pool` passes when both readings show an Angelic `droppable=` above 0 and
  five unique sub-list sizes.
- T15 `rate-angelic`: `raredrop angelic 2`, `angelicprobe hit chance` raised
  to its maximum, `flightprobe copies 1`, and `flightcharm wings 1` and
  `flightcharm broom 1`, with Pocket Elf left off. `angelicprobe hit`'s
  `gameHits=` and `flightcharm stat` are recorded, monsters are killed until
  `gameHits=` has risen by 100 or more, and both are read again. With N the
  rise and T the zone's `droppable=`, it passes when the Wings' and the
  Broomstick's rises each lie inside the band `drop_roll_model`'s
  `binomial_band` gives for N trials at a share of 1 in T + 2 (a low end below
  0 read as 0), ours equals built for both, and `refused=0` and
  `anomalies=0`. N, T and both counts are recorded. Both switches are turned
  off before teardown.

**The debugger.** It is attached after T2 and detached after T4, so its
logpoints never slow the depth steps or the kills, and in any case before the
game is stopped. Its positive control is a logpoint on `CheckTalentUse`,
which logged about 85 hits a second in town in Mod live 1 (`dbg-control`).
The candidates are three of the tooltip pass's direct calls to
`DrawInventoryStatsNew` (the first, a middle and the last of its 344, in
address order). Each name's count is recorded at the start and end of each
window. If the tooltip is cached, a site logs once per uncached pass, so a
rise of 1 counts. `dbg-stat-wings` passes when at least one candidate rose in
window A while the control rose, and is not observed otherwise.
`dbg-stat-elf` passes when a candidate that rose in window A also rises in
window B while the control rose, fails when it stays flat in B (window C shows
whether a tooltip is needed at all), and is not observed when
`dbg-stat-wings` did not pass.

**Checks**, the capture's names, in order: `dll-hash`, `marker`, `control`,
`dbg-control`, `bag-icon-control`, `tip-calls`, `dbg-stat-wings`,
`bag-icon-elf`, `dbg-stat-elf`, `depth-control`, `layers-town`,
`flight-depth`, `flight-excluded`, `broom-alone`, `one-flight`, `unique-b0`,
`guard-control`, `reload-broom`, `unique-control`, `pool`, `rate-elf`,
`layers-zone`, `flight-depth-zone` and `rate-angelic`.

- **Validity**, which must pass: `dll-hash`, `marker`, `control`,
  `dbg-control`, `depth-control`, `guard-control` and `unique-control`.
- **Acceptance**: `bag-icon-control`, `bag-icon-elf`, `flight-depth`,
  `broom-alone`, `reload-broom`, `rate-elf` and `rate-angelic`.
- **Research**, where a fail or a not-observed is itself the finding:
  `tip-calls`, `dbg-stat-wings`, `dbg-stat-elf`, `layers-town`,
  `flight-excluded`, `one-flight`, `unique-b0`, `pool`, `layers-zone` and
  `flight-depth-zone`.

**What it relies on.** The canopy spots, from the data file and measured
hidden in Mod live 1, and the wall columns south of the stopped walk, from the
data file (RDM § 19.7 and § 19.8); `UpdateDepth` and the game layers (§ 19.7,
static, and Mod live 1, measured); the canopies' own layer and the filter
layers (§ 19.7, static and data file). What `layer_get_fx` answers is
**established only by** `filter=` naming `Effect_1` with an `fx=` struct
count of 1 or more in `Town_01_rm` (and in the Outskirts of Inoya once its
canopies hold), never by `flight-depth=` (§ "Drawing in front"); on that
condition § 19.7's `layer_get_fx` reading is relabelled measured. A read
that throws or is refused, and one that does not answer a struct
(`fx-kind=` 5 or 0), are findings for this document only, recorded with the
argument supplied (the layer id `layer_get_all` answered, never the name):
the census's call shape cannot tell either from a builtin that wants the
layer's name, so neither reaches § 19.7 as a runner fact until a read that
passes the name answers the same. `fx=0/0/0` and a too-narrow kind test are
findings for this document only too. The rest it relies on: the tooltip pass (RDM § 16.6, measured, with the cache an
inference); the one-flight rule (§ 23.1, owner-stated and static) and the
charm read (§ 23.1, measured); the guard's load (§ 23.6, static, measured in
Mod live 1); `DropUniqueItems` and its by-name call shape (§ 23.4 and § 23.8,
static, with `unique-control` its control); the Angelic roll, its list and
its typing (RDM § 13.4, measured); the shares (hub
`docs/models/unique-pick-spec.md`); and the bands (`drop_roll_model`'s
`binomial_band` and `observation_within`). `zonecensus` is the guide's far
scenery sleep command; `tiptrace` is `ModuleMain.cpp`'s research command,
whose stat count is the stats hook's own.

### Mod live procedure 3

The session that answers the owner's monster scope (2026-10-13, "flight should
ignore enemy collisions", read by the driver as body collision only): a
flying player walks through monsters, and still attacks them. It runs on the
research build (`build.bat dev`) at the commit where the monster rule, the
adapter that resolves `Enemy_Parent_obj` by name, the `flp3` probe and these
docs are done; the driver records the DLL's SHA-256 and asks the owner before
installing it. Nothing Mod live 1 or Mod live 2 passed is run again, except
the wall check, which shows the monster rule did not open walls. As in the
earlier procedures, the debugger's addresses and first bytes stay out of this
file: they are kept in the session's local notes.

**Setup.** Save slot 14, "Sorak", in `Town_01_rm`. The saves are backed up
before the launch and restored at teardown whatever happened, and the
operator confirms that the build the debugger addresses were read from is the
installed game, or aborts. At character select, `jumpscenery 0`. The session
starts from the restored saves, so no flight charm is owned and Sorak's own
charms fill the charm grid. The operator drives every person-step itself
through hs-drive (keys, holds, clicks, `playerwarp`) and lists each in the
capture; the bag opens with `C`, and a right-click equips and unequips. None
of `jumpprobe hook`, `citrace 1`, `angelicprobe on`, `gambaprobe`,
`prospectprobe` or `craftprobe` is used, and every flight drop switch stays
off. Teardown: the debugger detached first, then the game stopped, the saves
inspected and restored, and the lease released.

**Cases.** One monster: a lone training dummy in town, walked into with
flight off and then on, and attacked with flight off and then on. The
training dummies are monsters by the data file's family (`Attack_Dummy_obj`
descends from `Enemy_Child_Basic_obj`, a child of `Enemy_Parent_obj`; hub RDM
§ 19.9), and the two that stand alone, at (1816, 704) and (1736, 544), neither
move nor attack. One outlier on the excluded path, a wall, and one on the
state path, the charm taken off inside the dummy. Not tried, and so "not
observed live": live monsters in a zone, bosses, co-op, and every row but
`place_meeting` at a monster.

**Steps, in order:**

- W0 `dll-hash`: the installed DLL's SHA-256 equals the build's.
- W1 `marker`: a bare `flightprobe` begins `flightprobe: marker=flp3`, which
  proves the installed build carries the monster rule; `control`: `ping`
  answers `pong (YYTK 4.0.1)`. All of `flightcharm stat`'s lines are recorded.
- W2, the charm (setup): Sorak warps to (884, 628), `flightprobe spawn elf`
  places Pocket Elf, and `flightprobe ground`
  reads its name and `ours`. The operator picks it up by its label and leaves
  it in the bag; two seconds later `flightcharm stat` reads `installed` and
  `state=off`.
- W3 `dummy-control`: Sorak warps 96 px south of the dummy at (1816, 704) for
  a screenshot. `flightprobe free` with no offset, against the family and
  against `Enemy_Parent_obj`, reads `blocked=no` both times, and `flightprobe
  free` 96 px north against `Enemy_Parent_obj` reads `blocked=yes` with `by=`
  naming the dummy's object (any descendant of `Enemy_Parent_obj` passes, and
  is named). If any read fails, the same reads are made 96 px south of the
  other lone dummy, at (1736, 544). The dummy that passes is "the dummy" below,
  its y is D, and its spot is "the start". It fails when neither spot passes;
  the session then measures nothing about monsters, skips the debugger and
  W4, W6, W7 and W8, and still runs W5 and W9.
- The debugger is attached and its three logpoints armed, the control first
  (below).
- W4 `monster-baseline` (debugger window A), with flight off: `flightcharm
  stat` reads `state=off` and the trace line is recorded. From the start, W is
  held for 1500 ms; then `flightprobe free` with no offset gives `y=`, and the
  trace line is recorded again. It passes when `y=` is greater than D (the
  dummy held the walk), and fails when `y=` is below D − 40 (the walk crossed
  with flight off, so a dummy does not block and W6 cannot show flight's
  effect); a value between the two fails too, with the value recorded.
- W5, equipping (setup): the bag is opened and Pocket Elf right-clicked into
  the charm grid; if the grid is full, one of Sorak's own one-cell charms is
  first right-clicked to the bag, and which one is recorded. The bag is closed
  and `flightcharm stat` reads `state=on equipped=elf`.
- W6 `flight-monster` (debugger window B): Sorak warps to the start and the
  trace line is recorded; W is held for 1500 ms, `flightprobe free` gives
  `y=`, and the trace line is recorded again. It passes when `y=` is below
  D − 40 and the trace's `monster=` rose. It fails when `y=` is greater than
  D, and the trace names the cause: `monster-kept=` rose (another row held the
  walk), `monster=` rose while the walk was still held (something the hooked
  rows do not see holds it), or `idle=` rose (flight was not on).
  `monster-rows`, from the same two trace lines, passes when `monster=` rose
  and `monster-kept=` did not, fails when `monster-kept=` rose, and is not
  observed when neither rose.
- The debugger is detached, after W6 and before W7.
- W7, attacks, the control with flight off first. `attack-control`: Pocket
  Elf is right-clicked to the bag (`state=off`), Sorak warps 40 px south of
  the dummy, and the pointer is held on the dummy with the left button for
  1500 ms, or `Q` is cast at it, as Mod live 2 attacked; a screenshot is taken
  during the attack. It passes when the screenshot shows damage numbers on the
  dummy and `flightprobe free`'s `x=` and `y=` moved less than 40 px, so Sorak
  attacked rather than walked. `flight-attack`: Pocket Elf back in the grid
  (`state=on`), the same spot and the same attack, with the same pass; it fails
  when Sorak walks to the pointer or no damage number shows.
- W8 `monster-ends-inside`: flying, Sorak warps onto the dummy (its x and D),
  where `flightprobe free` against `Enemy_Parent_obj` reads `blocked=yes`.
  Pocket Elf is right-clicked to the bag and `flightcharm stat` reads
  `state=residual`. S is held for 1000 ms, up to three times, until
  `flightprobe free` with no offset reads `blocked=no` against both
  `Enemy_Parent_obj` and the family and the state reads `off`. It passes when
  the position changes on the first walk and the state ends `off`, and fails
  when there is no movement while `residual`; `playerwarp` recovers.
- W9 `wall-still`: Pocket Elf back in the grid (`state=on`). Sorak warps to the
  excluded spot at (929.9, 2031.1), where `flightprobe free` 8 px south against
  `Wall_Parent_obj` reads `blocked=yes`; `excluded=` is recorded and S held for
  3000 ms. It passes when the walk moved less than 20 px and `excluded=` rose,
  fails when it moved 100 px or more, and is not observed when the wall read
  is `no`.

**The debugger.** It is attached after W3 and detached after W6, and in any
case before the game is stopped. Its positive control is a logpoint on
`CheckTalentUse` (`dbg-control`), which logged about 85 hits a second in town
in Mod live 1 and Mod live 2. The two candidates are `StatPhasing`, the
getter of item stat 26 "Movement Phasing", whose one direct call site is the
stat dispatcher (a static reading, hub RDM § 19.9), and
`InstancePlaceTallest`, which the player's Step calls on a blocked walk (RDM
§ 19.6). Window A is W4's walk and window B is W6's; each name's count is
recorded at the start and end of each window. `dbg-phasing` passes when
`StatPhasing` rose in window A or B while the control rose, which means the
game reads stat 26 during a walk at a monster, and is not observed when it
stayed flat while the control rose; either way it is a finding. `dbg-ipt`
records the rise in both windows, passes when it rose in window A while the
control rose (a walk held at a monster reaches `InstancePlaceTallest`), and is
not observed when it stayed flat while the control rose.

**Checks**, the capture's names, in order: `dll-hash`, `marker`, `control`,
`dummy-control`, `monster-baseline`, `dbg-control`, `dbg-phasing`, `dbg-ipt`,
`flight-monster`, `monster-rows`, `attack-control`, `flight-attack`,
`monster-ends-inside` and `wall-still`.

- **Validity**, which must pass: `dll-hash`, `marker`, `control`,
  `dummy-control`, `monster-baseline`, `dbg-control` and `attack-control`.
- **Acceptance**: `flight-monster`, `flight-attack`, `monster-ends-inside` and
  `wall-still`.
- **Research**, where a fail or a not-observed is itself the finding:
  `dbg-phasing`, `dbg-ipt` and `monster-rows`.

**What it relies on.** `Enemy_Parent_obj`'s family and the training dummies'
town positions, from the data file (hub RDM § 19.9); the walk's
`place_meeting` naming `Enemy_Parent_obj` (RDM § 19.6 and this document's
§ "Live 1 results", `walk-objects`, measured); the excluded spot, whose step
south meets a `Wall_Parent_obj` (RDM § 19.8, measured in Mod live 1 and 2);
`CheckTalentUse` as the debugger's control (Mod live procedure 2 and its
results); `InstancePlaceTallest`, called from the player's Step on a blocked
walk (RDM § 19.6 and this document's § "The walk's callers"); and `StatPhasing`'s one call
site (RDM § 19.9, static). Nothing in the shared references says
`StatPhasing` runs per step. That a monster in the path holds a walk at all is
not established before this session: `monster-baseline` is its control.

### Mod live 1 results

Run on 2026-10-12 by the live operator, on the research DLL built from
ForgePact `26eecd2` with `build.bat dev` (sha256
`c15f85e1a79ccd7acfd3ad8d271605a4061182f6364a1d23c5c6096c1b2cd442`), save
slot 14 "Sorak", in `Town_01_rm` and, for the Angelic drops, the Outskirts of
Inoya (`Act_01_01`, room 1, Hell, zone level 243), on the game build whose
executable hashes to
`498d588550d80d0d100a4ce6958bcc044e6b79339920d3d8a931bca9a1b142c0`. The
record is the session capture `forgepact-158-flight-charms-mod-live-1.md`,
kept with the hub's workorder files on the researcher's machine and not
tracked. The toolkit's `tools/live_checks.py` reads 27 checks from it: 21
pass, 5 fail, 1 not-observed, none not-run. Of the nine validity checks,
eight passed; the ninth, `guard-control`, failed on its charm-list half only
(below). The owner drove one step by hand (spreading the items on the ground
in S4); the operator drove every other person-step through hs-drive. The
saves were restored at teardown and read back clean.

| Check | Verdict | What it showed |
|---|---|---|
| `dll-hash` | pass | The installed DLL was the research build above. |
| `marker` | pass | Bare `flightprobe` answered `marker=flp1`. |
| `control` | pass | `ping` answered `pong (YYTK 4.0.1)`. |
| `fresh-off` | pass | The first `flightcharm stat` of the session was the fresh-session line, character for character. |
| `ground-control` | pass | A normal Small Charm spawned through `charmprobe spawn 0` landed on the ground with the game's own ground sprite, `Pickup_Charms_Normal_Small_Charm_01_spr`. |
| `ground-icon` | pass | Pocket Elf, Gabriel Wings and Witch's Broomstick each read their own name and `ours` on the ground, and a screenshot showed our three ground icons with no error placeholder. The waypoint's sprite hid them in the first screenshots. |
| `bag-icon` | fail | The bag drew our icons, the Wings on 2x2 cells and the Broomstick on 1x5, with each name in its rarity's colour (Satanic red, Angelic pale gold, Unholy pink), and the Wings' and the Broomstick's tooltips showed the gold `Flight` row. Pocket Elf's tooltip had no `Flight` row. |
| `walk-baseline` | pass | With the charms in the bag, not worn, flight read `install=installed state=off`, and a 3 s walk down at the ravine edge left the position unchanged. |
| `dbg-control` | pass | The `CheckTalentUse` logpoint logged throughout, and the debugger's control hold proved the attach by the game's frame counter (73033 and 73142 before the arm, no answer at the hold, 73374 after the resume). |
| `depth-control` | pass | At (1180, 1650), under a large red-leaved tree, Sorak was hidden, and with flight off `flightprobe depth` read his layer at depth −830 against the game layers' front at −5500. |
| `equip-detect` | pass | Pocket Elf right-clicked into the charm grid gave `state=on equipped=elf`. |
| `flight-depth` | fail | The write landed: his layer read −5500, the front end of the game layers (`front-end=last`), with `depth-calls` and `depth-seen` rising, `depth-idle` flat once flight was on and `no-front=` 0. Two screenshots still showed him under the canopy. |
| `dbg-step-depth` | pass | Over S7 to S9 the control rose by 3707 while the logpoints on the player Step's two `UpdateDepth` call sites rose by 3704 and 3703. |
| `flight-prop` | pass | With Pocket Elf worn, a 3 s walk down from the ravine edge crossed it, 440 px south of the start, and `answered=` rose from 45 to 235. |
| `flight-excluded` | not-observed | Neither probed spot had a wall answer: at the ravine edge and at the church wall the family read `blocked=yes` and `Wall_Parent_obj` read `blocked=no`, so there was no excluded spot. The S11 walk did stop at (929.9, 2031.1) with `excluded=` rising by 1951, which is where Mod live 2 probed. |
| `flight-ends-inside` | pass | Left inside the ravine's blocker (`Block_Dynamic_Sprite_obj`), Pocket Elf taken off gave `state=residual`; the first 1 s walk moved him out (y 1638.5 to 1837.8) and flight read `off`; a walk back north stopped at the ravine's south side. |
| `charm-grid-size` | fail | With Gabriel Wings and Witch's Broomstick both worn, `charmprobe charms` listed the Wings at 2x2, rarity 7, but not the Broomstick, though the grid drew it at 1x5 and showed its tooltip; the stat line read `equipped=wings`. |
| `rolls` | fail | The Wings read exactly `159=12;160=1`; the Broomstick, unlisted, could not be read (its spawn had read `25=100`). |
| `stash-size` | pass | Dragged into an empty area of the stash's Personal tab, the Broomstick took 1x5 cells. |
| `guard-kept` | pass | A Gabriel Wings saved 1x1 at a bag cell whose right and lower neighbours were taken loaded at that cell at 1x1, with both neighbours intact and its own rolls (3% and 4%), and the guard read `route=on`, `kept=1`, `yielded=0`, `failed=0`. |
| `guard-control` | fail | The guard's half passed: `route=on`, with `deferred=` up by 5 and `full=` up by 4 over the reload. The listing half failed as `charm-grid-size` did: the worn Broomstick was not listed. |
| `reload-identity` | pass | After the reload the charm list was the same, entry for entry, as before it (the Wings 2x2, rarity 7, `159=12;160=1`; the Broomstick absent from both). |
| `dbg-parse-place` | pass | The logpoint on `ParseItemToGrid`'s call to `InventoryGridAddItemPos` logged nothing before the reload and 1114 times during it, while the control rose from 20583 to 21553. |
| `unique-control` | pass | With every switch off, ten by-name calls of the ordinary unique roll (`flightprobe unique 10`, the player as `self`) placed ten Satanic uniques and no Pocket Elf. |
| `drop-elf` | pass | Pocket Elf's switch named Aimbot.exe as its stand-in. At the research lever's 200 copies, 20 unique-roll calls placed 19 items, 11 of them Pocket Elf; the stat line read `elf=11/11`, `refused=0`, `anomalies=0`, and a ground drop read `Pocket Elf ... ours`. |
| `drop-angelic` | pass | In the Outskirts of Inoya, at 200 copies with the Angelic roll's chance raised, both charms dropped: 13 each of the first 26 Angelic drops, and 21 Wings and 22 Broomsticks of 44, ours equal to built each time, `refused=0`, `anomalies=0`; a hovered ground drop read Witch's Broomstick, Unholy Charm (1x5). The kills ran to 44, four over the procedure's 40. |
| `drop-off` | pass | With all three switches off, ten unique-roll calls and 47 kills (43 Angelic drops) added no charm: `elf=11/11`, `wings=21/21`, `broom=22/22` and `injected=37600` stayed as they were. The kill batch ran to 47, not 10. |

**The debugger's counts**, by logpoint name (no address is recorded here):
the control `ctu`, on `CheckTalentUse`; `sda` and `sdb`, the player Step's two
calls to `UpdateDepth`; and `ppl`, `ParseItemToGrid`'s call to
`InventoryGridAddItemPos`.

- While the logpoints were armed, in town with flight off: `ctu` 2433 lines,
  `sda` 1197, `sdb` 691, `ppl` none.
- From then to the end of S9 (S7 to S9: the tree, the equip, the flight
  write): `ctu` +3707, `sda` +3704, `sdb` +3703, `ppl` none.
- Before the reload: `ctu` at 20583, no `ppl` line. During S17's reload: `ppl`
  from 1 to 1114, `ctu` from 20583 to 21553.
- While armed, the game's frame time rose from about 13 ms to about 33 ms.

**What its five fails and its not-observed mean.**

- **`flight-depth`: the model was wrong, not the write.** The player's layer
  read −5500, the front end of the room's game layers, and S16 showed him
  visible on open ground at that depth. A canopy still covered him at both
  spots, because canopies are not on a game layer: they move themselves onto
  a room layer of their own, `Tree_Leaves`, at −9700 in town, in front of
  every game layer (§ "Why the canopy covered the flying player", RDM
  § 19.7). The fix is the flight layer (§ "Drawing in front"); Mod live 2's
  `flight-depth` measures it.
- **`bag-icon`: the gold rows had no stat to anchor at.** They draw at the
  tooltip's first stat row the item has, and Pocket Elf has none. The fix is
  the stat-less anchor; Mod live 2's `bag-icon-elf` measures it, with the
  Wings as the control.
- **`charm-grid-size`, `rolls` and `guard-control`: the game counts one
  flight charm.** The Broomstick was worn beside the Wings, and the game
  lists one charm per definition `b` above a rarity line; all three charms
  carry `b` 0 (RDM § 23.1). The owner said this is how the game works, so
  nothing is fixed, and Mod live 2 wears each charm alone (`broom-alone`,
  `reload-broom`). The guard's own counters passed.
- **`flight-excluded`: not observed, with its control.** The family read
  `blocked=yes` at both spots in the same session, so the instrument could
  answer; neither spot was a wall. The data file puts
  `Invisible_Wall_Not_Minimap_obj` columns, children of `Wall_Parent_obj`,
  just south of where S11's walk stopped (RDM § 19.8), and Mod live 2 probed
  there.

Notes, not checks: the game exited with code C0000409 on a graceful close,
as phase 1's Live 1 did; a left click in the bag also sends Sorak walking,
and a `W` tap clears the walk it leaves.

### Mod live 2 results

Run on 2026-10-13 by the live operator, on the research DLL built from
ForgePact `7dfea36` with `build.bat dev` (sha256
`dd87086cab7ba1b27d4742a68b063a85c593a7d97597412fd7f1714d08f2dc11`): Mod
live 1's fixes and the split workorder's `flightprobe depth` fields
`filter=`, `fx=`, `unread=` and `fx-kind=`. Save slot 14 "Sorak", in
`Town_01_rm` and the Outskirts of Inoya (`Act_01_01`, room 1, Hell, zone
level 243), on the same game build as Mod live 1. The record is the session
capture `forgepact-158-flight-charms-mod-live-2.md`, kept with the hub's
workorder files and not tracked. `tools/live_checks.py` reads 24 checks from
it: 24 pass, none fail, none not-observed, none not-run. All seven validity
checks passed. The operator drove every person-step through hs-drive; none
went to the owner. The saves were restored at teardown and read back clean.

| Check | Verdict | What it showed |
|---|---|---|
| `dll-hash` | pass | The installed DLL was the research build above. |
| `marker` | pass | Bare `flightprobe` answered `marker=flp2`; `sigdrop status` read `gate=tyrant:off,headhunter:off`. |
| `control` | pass | `ping` answered `pong (YYTK 4.0.1)`. |
| `dbg-control` | pass | The `CheckTalentUse` logpoint logged throughout, and the control hold proved the attach by the frame counter (19848 and 19955 before the arm, no answer at the hold, 20114 after the resume). |
| `bag-icon-control` | pass | Gabriel Wings' tooltip showed one gold `Flight` row, directly above `Physical Damage Taken Reduced by 12%`. |
| `tip-calls` | pass | The first tooltip pass after each hover made stat calls, for the Wings and for Pocket Elf alike: `statLines=333 forgedRows=1` both times, on the first try. Every later pass of the same hover read `statLines=0`. |
| `dbg-stat-wings` | pass | During the Wings' hover one of the three stat-call sites logged (the middle one, 0 to 1) while the control rose. |
| `bag-icon-elf` | pass | Pocket Elf's tooltip showed one gold `Flight` row between `Satanic Charm (1x1) (Unique Equipped)` and its lore line, and the trace printed `tiptrace forged rows at y=98.000000 (1 rows)`. |
| `dbg-stat-elf` | pass | The same site rose again during Pocket Elf's hover (1 to 2) while the control rose, and stayed flat with the pointer on an empty cell. |
| `depth-control` | pass | With no flight charm worn, `state=off`; at (1180, 1650) Sorak was hidden under the red canopy, as in Mod live 1; `flightprobe depth` read his layer at −830, `flight-layer=none`, `filter=none`, every census count 0 and `layers=0`. |
| `layers-town` | pass | `zonecensus` counted 42 `Fall_Dead_Tree_Leaves_01_obj`, all 42 on `Tree_Leaves` at −9700, although the data file places 8 of them on `Objects`. |
| `flight-depth` | pass | With Pocket Elf worn: `state=on equipped=elf`; the flight layer at −10001, in front of `Invisible_walls` (−10000) and behind the filter `Effect_1` (−10300); `filter=Effect_1@-10300` with a struct count of 2; `layers=5598`, the sum of the nine counts. Screenshots at (1180, 1650) and (1158, 1720) showed him over the canopy that hid him without flight. |
| `flight-excluded` | pass | At (929.9, 2031.1) the `Wall_Parent_obj` read one step south was `blocked=yes`; a 1 s walk south moved him 0 px while `excluded=` rose from 0 to 1317. |
| `broom-alone` | pass | Worn alone, Witch's Broomstick was listed at 1x5, rarity 10, exactly `25=100`, and the stat line read `state=on equipped=broom`. |
| `one-flight` | pass | With Gabriel Wings added beside the worn Broomstick, the list held the Wings only and the stat line read `equipped=wings`; the grid still drew the Broomstick. |
| `unique-b0` | pass | The game's unique repository answered `{10, 0, 0}`: `charms_raiders_torch`, rarity 6, 1x2, no hidden flag. |
| `guard-control` | pass | Over the reload, with two full-size Wings in the bag and the Broomstick worn: `route=on`, `deferred=` and `full=` each up by 3, `kept=0`, `yielded=0`, `failed=0`. |
| `reload-broom` | pass | After the reload the Broomstick was listed at 1x5, rarity 10, `25=100`, flight read `equipped=broom`, and the bag showed both Wings at 2x2 where they were. |
| `unique-control` | pass | With every switch off, ten by-name unique-roll calls placed ten Satanic uniques and no Pocket Elf. |
| `pool` | pass | Both readings, in town and in the zone before the kills, were the same: the Angelic list `lootListUnique[5]` of 380 entries with 48 droppable, Water Melon and Soul Collector once each; the ordinary lists at 50, 61, 79, 152 and 221 entries, with Aimbot.exe on none of them. |
| `rate-elf` | pass | At one copy, 100 unique-roll calls placed 97 items, one of them Pocket Elf; the stat line read `elf=1/1`, `injected=500`, `refused=0`, `anomalies=0`. |
| `layers-zone` | pass | With flight off, a canopy hid Sorak on the third short walk; `zonecensus` counted 187 canopies, all on `Tree_Leaves` at −10000, and the filter `Effect_1` visible at −10100. |
| `flight-depth-zone` | pass | With the Broomstick worn: the flight layer at −10001 on the first read, in front of `Tree_Leaves` (−10000) and behind `Effect_1` (−10100); `filter=Effect_1@-10100` with a struct count of 2; `layers=5620`, the nine counts' sum. A screenshot showed him over the canopy. |
| `rate-angelic` | pass | At one copy each, with the Angelic roll's chance raised: 102 Angelic drops (114 rolls), 2 Witch's Broomsticks and no Gabriel Wings, ours equal to built, `refused=0`, `anomalies=0`. |

**The debugger's counts**, by name: the control `ctu`, on `CheckTalentUse`,
and `ds1`, `ds2` and `ds3`, the first, a middle and the last of the tooltip
pass's 344 direct calls to `DrawInventoryStatsNew`. Window A is the Wings'
hover, B Pocket Elf's, C the pointer on an empty bag cell.

| Name | A start | A end | B start | B end | C start | C end |
|---|---|---|---|---|---|---|
| `ctu` | 4275 | 5627 | 8953 | 9944 | 10351 | 10970 |
| `ds1` | 0 | 0 | 0 | 0 | 0 | 0 |
| `ds2` | 0 | 1 | 1 | 2 | 2 | 2 |
| `ds3` | 0 | 0 | 0 | 0 | 0 | 0 |

One logged pass per hover, for an item with stats and for one without, and
none with no tooltip: with `tip-calls`' zero on every later pass, this
supports the reading that the tooltip is drawn from a cached surface after its
first frame (RDM § 16.6).

**`pool`'s numbers.** The Angelic roll's list held 380 entries, 48 of them
droppable (T = 48), with Water Melon and Soul Collector, the Wings' and the
Broomstick's stand-ins, once each (n = 1). The ordinary roll's five lists held
50, 61, 79, 152 and 221 entries, the lengths #74 measured on 2026-10-02, and
Pocket Elf's stand-in, Aimbot.exe, on none of them (n = 0, so a pick of its
pushed entry is always Pocket Elf). The town and zone readings were the same.

**The rates against their bands.** Each charm's intended share is one entry's
share of its roll (hub `docs/models/unique-pick-spec.md`):

- Pocket Elf, at one copy: 1 call in L + 1 of the ordinary roll, between 1 in
  222 and 1 in 51 by the list the roll chooses. Over 100 calls that is 0.45 to
  1.96 Elves expected; 1 was seen, and `drop_roll_model`'s
  `observation_within`, for 1 in 100 against that range, answered `True`.
- Gabriel Wings and Witch's Broomstick, at one copy each: 1 in T + 2 = 1 in
  50 of the Angelic roll's drops. Over 102 drops that is 2.04 of each
  expected, and `binomial_band` for 102 drops at 1 in 50 gives −2.2 to 6.3,
  read as 0 to 6. The Broomstick's 2 and the Wings' 0 both lie inside.
- These counts say the share is not far off one entry's; they are too small
  to tell one entry's share from, say, two. Every drop used a research lever
  (the Angelic roll's chance raised, the ordinary roll called by name), so the
  natural chance per kill is not measured.

**Which layer the flight layer went in front of.** In town it went to −10001,
in front of `Invisible_walls` (−10000, 77 invisible walls, the room's
frontmost layer that holds instances), the windmill sails (−9900) and
`Tree_Leaves` (−9700), and behind `Effect_1` (−10300). In the Outskirts of
Inoya it went to −10001 on the first read, in front of `Tree_Leaves`
(−10000), the frontmost layer there that holds instances, and behind
`Effect_1` (−10100). No re-choice was needed: the zone's canopies already
existed. The operator's note for the owner: his colours looked like the scene
around him, the same warm tint, at the transport's 1280x720.

**The census.** It took 9.34 ms in town and 8.60 ms in the zone, once per
room visit. It saw 5598 layers in town and 5620 in the zone, each landing in
exactly one of the nine counts:

- Town: `fx=2/50/0` (two effect reads answered a struct, 50 answered
  something else, none failed), and of the layers it did not read, 5522
  behind the game layers, 2 hidden and 22 holding elements.
- Zone: `fx=2/55/0`, with 5549 behind, none hidden and 14 holding elements.
- The 50 and 55 answers that were not a struct were all of kind 0, a real
  (`fx-kind=0`). The census hands `layer_get_fx` the layer id `layer_get_all`
  answered, never the name, so this is recorded here only, with that
  argument, and is not a runner fact (§ "Drawing in front"). Which second
  layer answered a struct in each room was not named by the probe.
- The data file had predicted `fx=1/0/0` and two hidden and four holding
  layers in town, and `fx=1/0/0` in the zone once its canopies held. The
  runtime adds layers the data file does not list: about 5500 behind the game
  layers, the runner's own `Layer_*` layers in front (the UI, the camera,
  particles, from −11001 to −14003), and two room layers in front of −5500
  that the data file's list for town lacks, `FilterLayer1` (visible, −10501)
  and `Raven` (visible, −10300). The same two are in the zone.
- The flight layer `fp_flight` was still in the zone's layer list at −10001
  after the Broomstick was taken off: ForgePact leaves its layer in the room.

**What `layer_get_fx` answers.** Given a layer id as `layer_get_all` lists
it, it answered a struct for `Effect_1` in both rooms: `filter=` named
`Effect_1` with a struct count of 2, in town (the positive control) and in
the zone. That is the condition RDM § 19.7 set, and the bullet there now reads
measured.

Notes, not checks: the game exited with code C0000409 on a graceful close,
as in both earlier sessions. The first `F` press in town picked up a unique
that T13 had dropped on the waypoint, so the operator stepped off it first.
The worn Broomstick that the game did not list was still drawn in the grid,
as in Mod live 1. The owner's 2026-10-13 decision that flight should also
ignore monsters came while this session ran; nothing here tested monsters.

### Mod live 3 results

Run on 2026-10-13 by the live operator, on the research DLL built from
ForgePact `c879f69` with `build.bat dev` (sha256
`8a16a38edd8325dd288e7d77538b2a34ff4390d5ba783d6276ba17851874bc92`): the
monster rule and the marker `flp3`. Save slot 14 "Sorak", in `Town_01_rm`,
on the same game build as both earlier sessions, starting from the restored
saves with no flight charm owned. The record is the session capture
`forgepact-158-flight-charms-mod-live-3.md`, kept with the hub's workorder
files and not tracked. `tools/live_checks.py` reads 14 checks from it: 11
pass, 1 fail (`monster-baseline`), 2 not observed (`attack-control`,
`flight-attack`), none not run. The operator drove every person-step through
hs-drive; none went to the owner. The saves were restored at teardown and
read back clean.

**The monster path is not verified by this session.** `monster-baseline`,
the control for the monster walk, failed: with flight off, Sorak's walk
crossed the training dummy. So a training dummy does not stop a vanilla
player walk, and `flight-monster`'s pass shows nothing about what flight
changes. That a flying player walks through monsters is **not observed
live** by this session. The owner's play check after it (2026-10-13, on the
same research build, with a Pocket Elf spawned for him) relabels it:
**owner-confirmed in play, 2026-10-13**, an owner report rather than an
instrumented measurement (§ "Status and what is not established").

| Check | Verdict | What it showed |
|---|---|---|
| `dll-hash` | pass | The DLL the lease hashed was the research build above. |
| `marker` | pass | Bare `flightprobe` answered `marker=flp3`, the build with the monster rule. |
| `control` | pass | `ping` answered `pong (YYTK 4.0.1)`. |
| `dummy-control` | pass | From (1816, 800), 96 px south of the lone dummy at (1816, 704), the spot read free against the family and against `Enemy_Parent_obj`, and the spot 96 px north read blocked by `Attack_Dummy_obj`. So the dummy is there, and is a monster to the saved original. |
| `monster-baseline` | fail | With flight off, a 1.5 s walk north from (1816, 800) ended at y 503.24, past the dummy's row (below 704 − 40). The dummy did not hold the walk. The trace counted 52 blocked queries of the player during the walk, all left alone because flight was off (`idle=`), not split by object. |
| `dbg-control` | pass | The `CheckTalentUse` logpoint logged throughout, and the control hold proved the attach by the game's frame counter (13041 and 13149 before the arm, no answer at the hold, 13305 after the resume). |
| `dbg-phasing` | pass | `StatPhasing` was entered during both walks (419 times in window A while the control rose 418; 385 in window B against 386) and also while Sorak stood still, one-for-one with the control (317 against 317 before window A, 12693 against 12692 between the windows). Measured: the getter of item stat 26 runs on every step, once per `CheckTalentUse` hit, standing included. |
| `dbg-ipt` | pass | `InstancePlaceTallest` was entered 111 times during window A's walk and 103 during window B's, and not at all while Sorak stood still. The first arm was refused because ForgePact's own detour sits at the function's entry in the live game; the operator armed it on the detoured entry. |
| `flight-monster` | pass | With Pocket Elf worn, the same walk ended at y 474.554, and the trace's `monster=` rose from 0 to 19 while `monster-kept=` and `idle=` stayed put. The monster rule answered the dummy's `place_meeting` queries; because the walk crossed without flight too, this does not show that flight changes the walk. Sorak ended inside a haystack's collision, which flight allows. |
| `monster-rows` | pass | During the flying walk `monster=` rose (0 to 19) and `monster-kept=` did not (0 to 0): no other row named the dummy and was kept. |
| `attack-control` | not observed | With flight off, a 1.5 s left-button hold with the pointer on the dummy walked Sorak 334 px north, past the dummy to the fence; a `Q` cast at it moved him 0 px. No damage number appeared in any screenshot of the session, although the DPS meter read 26 to 32 million throughout, so the check's instrument read nothing. |
| `flight-attack` | not observed | Flying, the same `Q` cast moved him 0 px and the state stayed `on`; no damage number, for the same reason. |
| `monster-ends-inside` | pass | Flying, warped onto the dummy: the monster read was blocked by `Attack_Dummy_obj` and the family read free. With the charm taken off, flight read `residual`; the first 1 s walk south moved him from y 704 to 910.538, both reads came back free, and flight read `off`. While he stood on the dummy, `residual`, `monster=` rose from 19 to 1819 and `monster-kept=` from 0 to 703 (inside `other=`, also 0 to 703): the other rows named the dummy while he stood inside it, and kept their answers. Since the dummy does not hold a vanilla walk, this shows the residual state ending on the monster read, not that he needed it to walk out. |
| `wall-still` | pass | Flying, at the excluded spot (929.9, 2031.1) the `Wall_Parent_obj` read one step south was blocked; a 3 s walk south moved him 0 px while `excluded=` rose from 0 to 3920. The monster rule did not open walls. |

**The debugger's counts**, by name: the control `ctu`, on `CheckTalentUse`;
`phasing`, on `StatPhasing`; `ipt`, on `InstancePlaceTallest`. Window A is
the walk at the dummy with flight off, window B the same walk flying.

| Name | At arming | A start | A end | A rise | B start | B end | B rise |
|---|---|---|---|---|---|---|---|
| `ctu` | 2912 | 3229 | 3647 | +418 | 16339 | 16725 | +386 |
| `phasing` | 1256 | 1573 | 1992 | +419 | 14685 | 15070 | +385 |
| `ipt` | none | 0 | 111 | +111 | 111 | 214 | +103 |

**What `dbg-phasing` changes.** The monster rule rejected hooking
`StatPhasing` because nothing showed the game reading stat 26 on every step
rather than from a value cached when the stats are rebuilt. This session
measured that the getter is entered once per step, walking or standing. What
calls it each step, and whether the movement uses its answer to let a player
through monsters, is still not established. Whether that makes the game's own
stat a better route than the `place_meeting` answer is the owner's question,
not decided here.

Notes, not checks: the game exited with code C0000409 on a graceful close, as
in every earlier session. The bag opened with `C` held 150 ms, not 80 ms.
Nothing moved one of Sorak's own charms: the charm grid had room for Pocket
Elf.

### Status and what is not established

All three sessions are recorded. Every acceptance check of Mod live 2 passed,
and it re-ran every check Mod live 1 failed or did not observe. Mod live 3
tried the monster rule against a town training dummy; its control failed, so
it verified the walls and the residual state but not the monster path. The
owner then played with a Pocket Elf on the same research build and reported
the mod working as intended, monsters included.

**Owner-confirmed in play, 2026-10-13** (the owner's report, not an
instrumented measurement: no trace, counter or debugger read backs it):

- a flying player walks through monsters ("flight should ignore enemy
  collisions"; "record mod as working as intended");
- monsters still hit a flying player and he still hits them ("hits damaged
  me while in flight. everything worked correctly").

Measured:

- the flight lever crosses scenery and stops at walls (`flight-prop` in Mod
  live 1; `flight-excluded` in Mod live 2) and ends cleanly when the charm
  comes off inside a prop (`flight-ends-inside`);
- the flight layer draws the player over tree canopies, in town and in a
  generated zone, without passing the room's colour filter (`flight-depth`,
  `flight-depth-zone`);
- the gold `Flight` row on all three charms, Pocket Elf included
  (`bag-icon-control`, `bag-icon-elf`), and our icons in the bag and on the
  ground (`ground-icon`, Mod live 1's `bag-icon` for the icons);
- the size guard over a reload (`guard-kept`, `guard-control`,
  `reload-broom`), the true sizes in the bag, the charm grid and the stash;
- the drops at one entry's share, at the player build's single copy
  (`rate-elf`, `rate-angelic`), and none with the switches off (`drop-off`);
- the game counts one flight charm at a time (`one-flight`);
- the monster rule leaves walls blocking (`wall-still`: 0 px moved,
  `excluded=` +3920) and ends `residual` on the monster read when the charm
  comes off inside a monster (`monster-ends-inside`);
- a training dummy (`Attack_Dummy_obj`) does not stop a vanilla player walk
  (`monster-baseline`, the failed control);
- `StatPhasing`, the getter of item stat 26, is entered on every step, once
  per `CheckTalentUse` hit, standing still included (`dbg-phasing`), and
  `InstancePlaceTallest` during walks only (`dbg-ipt`).

**Not established**, each written as not observed live rather than as a
negative:

- **The natural drop rate.** Every drop used a research lever. The Angelic
  roll's own chance per kill is not measured, and how often a player meets a
  charm depends on it.
- **The rates' precision.** One Elf in 100 calls and two Broomsticks in 102
  drops fit one entry's share; counts this small cannot tell it from a share
  a few times larger or smaller. T = 48 was read in two rooms of one session;
  whether it differs by difficulty, season or a game patch is not measured.
- **Which flight charm the game lists** when two are worn. The game's walk
  order decides; with the Broomstick worn first and the Wings added, it
  listed the Wings, as Mod live 1 did.
- **Raider's Torch.** The game's unique repository holds a Satanic unique
  charm at `{10, 0, 0}`, `charms_raiders_torch`, so by RDM § 23.1's reading
  the game would count only one of it and a worn flight charm. That is an
  inference; wearing the two together was not tried. Whether that is
  acceptable is the owner's question, not decided here.
- **Monsters, by instrument.** The owner decided on 2026-10-13 that flight
  should ignore monsters too. Mod live 3 walked into a training dummy, but
  with flight off the walk crossed it as well, so `flight-monster`'s pass
  shows nothing; the pass-through and the hits both ways rest on the owner's
  play check above, not on a measurement. Which per-step consumer's
  `place_meeting` query at a monster holds a vanilla walk is unidentified:
  whether a live monster holds a vanilla walk at all, through `place_meeting`
  or another row, is not observed by any instrument. Monsters' damage and the
  player's attacks with flight on are not observed by an instrument either
  (`attack-control` and `flight-attack`: no damage number in any
  screenshot).
- **What `StatPhasing`'s answer is used for.** It runs on every step
  (`dbg-phasing`); whether the movement reads it to let a player through
  monsters is not established. The owner's report that the mod works as
  intended settled the design question on the default: the shipped
  `place_meeting` answer stays.
- **Not tried:** Pocket Elf in a zone, the 8 colour-filter rooms where "Never
  past a filter" changes the answer (the harness's `front_layer_filter`
  scenarios prove the rule), gates and locks and map edges outside town, the
  church's wall walked with flight on (at the church wall spot the blocker is
  in the family but is not a `Wall_Parent_obj`, so flight would cross it),
  grids other than the bag, the charm grid and the stash, co-op.
- **The census's other answers.** What the 50 and 55 non-struct effect reads
  were, and whether passing a layer's name instead of its id changes them.
- **How the flying player looks.** The operator saw the scene's own tint on
  him; the owner judges the screenshots.
- **The exit code** C0000409 on a graceful close, seen in every session of
  this feature, phase 1's included; not investigated.
