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
  `docs/RUNTIME_DATA_MODELS.md`, § 19.6 and § 22, labelled as static readings.
- **Live procedure 1:** run on 2026-10-12 (§ Results, "Live 1 results"):
  the five validity checks passed; 21 of 26 checks pass, 3 fail and 2 are
  not-observed. The measured facts about the game are also in
  `docs/RUNTIME_DATA_MODELS.md` § 19.6 and § 22, relabelled as measured.
- **Decision:** recorded from Live 1 (§ Decision). `dress-added` reads
  `item-missing`, the value added for the outcome S17 measured: the dressed
  charm missing after the reload, with no crash and no overlap.
- **Later phases:** phase 2, the mod, is planned from this phase's Decision
  and built (§ Phase 2: the mod); its Mod live procedure 1 has not run yet.

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
§ 22.

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
for the relic grid); `localCharmItemArray` (RDM § 22.1, a static reading,
never read live before this session); `GetItemFromFingerprint` and
`GetOnlinePlayerItemOwner` (RDM § 2, measured); `GetItemSize` and
`GetItemInfo` (RDM § 22.2, a static reading); the json, `InitItemFromJson`,
`LootGroundCreateFromItem` spawn (RDM § 16.2, measured, and `sigdrop`);
`GetUniqueRepoStruct`, field 27 and flag 40 (RDM § 13.4); `DropItemHeroic`
(RDM § 22.4, a static reading); the Custom Forge's final pass (RDM § 16.1 and
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
- **Placing a rebuilt item** (RDM § 22.6). `ParseItemToGrid` rebuilds each
  saved item and places it at its saved cell through
  `InventoryGridAddItemPos`, which answers false and writes nothing when any
  cell of the footprint is taken or off the grid. `ParseItemToGrid` ignores
  that answer, which is the reading behind S17's `dress-added:
  item-missing`: the dressed charm did not fit and was left off the grid.
- **The ground sprite** (RDM § 22.7). `LootGroundInit` gives a new ground
  item the sprite named `Pickup_` plus the name of the item's display sprite
  (key `"11"`), and `Error_spr` when no such sprite exists. The game ships a
  `Pickup_` ground sprite beside each charm icon (13x13, 17x11 and 34x17 for
  the three read). A sprite added at runtime has a generated name with no
  such companion, which is the reading behind `sprite-draw: partial`. The
  player build already detours `LootGroundInit` once, shared by hidden loot
  and the loot announcement, and a second detour on it is refused.
- **The ordinary unique roll** (RDM § 22.8). `DropUniqueItems` reads one
  random entry of its sub-list and drops nothing when that entry fails its
  tests: no second draw, no die per entry. One added entry is therefore
  picked on 1 call in the sub-list's length plus one, between 1 in 51 and 1
  in 222 for the measured lengths.
- `InstancePlaceTallest` gathers the instances at a place and answers the
  tallest, or `noone`, as § 19.6 of RDM already says.

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
- **One flight charm at a time** (RDM § 22.1). The game lists one charm per
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
every point the query names lies inside the room. Anything else, monsters
included, keeps its real answer. `InstancePlaceTallest`'s answer is tested
the same way, by the object of the instance it returned.

Taking the charm off while overlapping the family leaves flight residual: the
answers go on until the saved original `place_meeting` at the player's own
position reads free, and then flight is off. The player is never moved.

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
remembers the layers in front of the game layers; a re-choice recounts their
elements at most once a second, so a zone's `Tree_Leaves` that fills after
the census (an empty one reads as a filter) moves the layer within a second.
Before each write the cached layer must exist and carry our name; a failed
check starts a new visit. A census that yields no layer writes nothing and
counts `no-front=`. Each effect read is counted as a struct, another answer
or a failure (`flightprobe depth`'s `fx=`, the trace line's `fx-struct=`,
`fx-other=` and `fx-failed=`), because a read that never answers a struct
would silently turn the filter rule off. What the effect read answers on this
runner is Mod live procedure 2's `flight-depth` question, with town's
`Effect_1` as the positive control.

### The size guard

`ParseItemToGrid` and `InventoryGridAddItemPos` are hooked by their SDK names,
installed with the Custom Forge's own hooks, because the dressing they protect
runs on every load. Each load opens a scope. Inside it, a placement of a
flight charm larger than 1x1 is held back and answered true (the load ignores
the answer, RDM § 22.6). When the load returns, every vanilla item of that grid
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
static reading only (RDM § 22.6 does not record it). When one of the held
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
`Pickup_` companion by name (RDM § 22.7). So flight rides hidden loot's one
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
  drew from (RDM § 22.8), the coin is exact only when the stand-in occurs
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
`InventoryGridAddItemPos` (§ 22.6, static); `LootGroundInit` (§ 22.7,
static); `DropUniqueItems` (§ 22.4 and § 22.8, static: its by-name call shape
is not proven, which is why `unique-control` is its control); the Angelic
roll and its hooks (RDM § 13.4 and `angelic-roll-hook-research.md` § Session
3 to 5, measured); the charm read and the size and sprite keys (RDM § 22.1,
§ 22.2 and § 22.5, measured).

### Mod live procedure 2

The session that re-runs what Mod live 1 failed or did not observe, each with
the control it needs, and adds the owner's drop-rate check. What Mod live 1
passed is not run again. It runs on the research build (`build.bat dev`) at
the commit where the flight layer, the stat-less tooltip anchor, the `flp2`
probe and these docs are done; the driver records the DLL's SHA-256 and asks
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
rule there, while the effect read itself is measured in both rooms), gates and
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
  `state=off`, `flight-layer=none`, `filter=none` and `fx=0/0/0`: no census
  has run yet.
- T6 `layers-town`: `zonecensus`, then its JSON file. Its layer list has
  `Tree_Leaves` at −9700, and every counted `Fall_Dead_Tree_Leaves_01_obj`
  sits on `Tree_Leaves`. The count, the frontmost visible layer holding
  instances and the filter layer (`Effect_1`, visible, −10300) are recorded,
  and `Wind_Leaves` too if it reads visible (the data file says hidden). It
  passes when every counted canopy sits on `Tree_Leaves`.
- T7 `flight-depth`: Pocket Elf goes into the charm grid, and
  `flightcharm stat` reads `state=on equipped=elf`. `flightprobe depth` then
  names a flight layer whose depth equals the player's depth and lies below
  −9700: the expected reply is `flight-depth=-10001` with
  `front=Invisible_walls@-10000`, in front of the canopies and the walls'
  layer and behind the filter at −10300. A flight depth below the filter's is
  a fail ("Never past a filter"). The same reply reads
  `filter=Effect_1@-10300` with an `fx=` struct count of 1 or more, and the
  trace line's `fx-struct=` is 1 or more: the positive control on the effect
  read, recorded whatever it reads, for RDM § 19.7 as measured. `filter=none`
  fails even with the depth right, and `fx=` names the cause: failures above 0
  mean the read failed, a struct count of 0 with no failures means the effect
  read does not answer a struct on this runner, and a struct count above 0
  means the census read an effect but classed no filter. Screenshots at the
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
  (visible, −10100 in the data file); not observed when no canopy hid him.
  `flight-depth-zone`: the Broomstick back in the grid, and
  `flightprobe depth` reads a flight depth below `Tree_Leaves`' and above the
  filter's; the expected reply is `flight-depth=-10001` with
  `front=Tree_Leaves@-10000`. A reading of −5501 against the game layers
  while the zone's `Tree_Leaves` is still empty (it then counts as a filter)
  is read again after two seconds, by which time the re-choice should have
  moved the layer. The same reply reads `filter=Effect_1@-10100` once the
  canopies hold, or `filter=Tree_Leaves@-10000` while the depth still reads
  −5501, with an `fx=` struct count of 1 or more, all recorded for RDM
  § 19.7; `filter=none` fails, its cause named from `fx=` as in T7. A
  screenshot shows Sorak over the canopy: a fail when he is hidden, not
  observed with `layers-zone`. `pool`, second reading, here before the kills.
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
layers (§ 19.7, static and data file; what the effect read answers is
measured here); the tooltip pass (RDM § 16.6, measured, with the cache an
inference); the one-flight rule (§ 22.1, owner-stated and static) and the
charm read (§ 22.1, measured); the guard's load (§ 22.6, static, measured in
Mod live 1); `DropUniqueItems` and its by-name call shape (§ 22.4 and § 22.8,
static, with `unique-control` its control); the Angelic roll, its list and
its typing (RDM § 13.4, measured); the shares (hub
`docs/models/unique-pick-spec.md`); and the bands (`drop_roll_model`'s
`binomial_band` and `observation_within`). `zonecensus` is the guide's far
scenery sleep command; `tiptrace` is `ModuleMain.cpp`'s research command,
whose stat count is the stats hook's own.
