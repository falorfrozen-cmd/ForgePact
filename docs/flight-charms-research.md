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
- **Later phases:** phase 2, the mod, is planned from this phase's Decision.

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
| `walk-objects` | pass | The tally of the blocked walk counted six distinct objects on three builtin rows. The object outside `Collision_Parent_obj`'s family that `place_meeting` named most was `Enemy_Parent_obj` (3015 queries); `Wall_Parent_obj`, `Enemy_Child_Boss_obj` and `Enemy_Only_Passage_obj` were also queried from outside the family. |
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
