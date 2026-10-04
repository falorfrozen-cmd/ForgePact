# Skill sliders: projectile amount, projectile speed, AoE size (issue #160)

Status (2026-10-04): **research phase done; nothing player-visible ships.**
This document answers, for each of the three values issue #160 asks to make
adjustable, where Hero Siege computes it, which script a ForgePact hook would
detour to change it for the player's skills only, and what is still open. The
static reading is done (`## Static reading`). The research build's `projprobe`
command hooks every candidate in one DLL (`## Candidates`), and three live
sessions on 2026-10-04 measured all three levers (`## Live 1`): each route is
**proven** on a White Mage skill (`amount-route: proven`, `speed-route:
proven`, `aoe-route: proven`). The sliders themselves (panel rows, `statadd`
entries, release notes) are the next workorder, and they start from this
document.

Every claim carries one of four labels, as in
[`docs/models/skill-stat-spec.md`](../../docs/models/skill-stat-spec.md):

- **Static reading**: read from the compiled game locally, through the named
  Ghidra project, and written here in our own words. Unless a claim says
  otherwise, the reading is of the Sep-17 build (`Hero_Siege.exe`, 281,751,552
  bytes), done on 2026-10-04. The decompiler output, listings and addresses
  stay on the researcher's machine (hub `AGENTS.md` § "Legal"); this document
  carries script names, stat ids, array element numbers and behaviour only.
  "Direct callers" are `call rel32` sites found by a scan of the same binary;
  a call routed through the script table is not seen by that scan, so a count
  is "direct callers found", never "all callers".
- **Measured**: observed in a running game.
- **Our code**: what ForgePact does.
- **Not established**: open, and listed again in `## Not established`.

Script names are the `hs-game-sdk` `GameScript` names; the number after a
name is its `gml_Script_*` index in that table.

## Static reading

### Already found, per lever (the question as the plan answered it)

Before any decompiler reading, the static search covered `hs-stat-forge/`
(its 15 `hs_statforge_stats.json` keys, the hook notes, README and release
notes), `hs-game-sdk` (`StatId`, the script and object tables, `curated/`),
`docs/RUNTIME_DATA_MODELS.md`, `docs/models/*`, every `ForgePact/docs/*-research.md`
and `ForgePact/plugin/`. The item-editor, HSCraftSim and HSSaveEditor
submodules were not initialized in that checkout, so their stat catalogs were
not read. The table below is that search plus the planner's first Ghidra pass;
the subsections after it are this phase's reading and supersede the "not read
yet" cells.

| Lever | Status at planning | Where / by what | Label |
|---|---|---|---|
| AoE size | route found, unmeasured | `StatAOESkillSize` (3399), reached only from `ReturnSpecificStat`, the same shape as `StatSpellHaste`, which `statadd` already detours natively (Known Limitations item 41). `StatExplosionAOE` (3353) has the same single caller. `LoadAOEModifiers` reads stats through `ReturnSpecificStat` and was the likely consumer. | static reading |
| Projectile amount | route found, unmeasured | `ReturnExtraSpellProjectiles` (3294), called from the `Talents<Class>` scripts, and `ReturnExtraProjectilesRanged` (3293), called from Marksman, Pirate, Samurai, Amazon, DemonSlayer and `TalentsUniversal`. Each reads a stat through `ReturnSpecificStat`. A shared helper, so one native detour reaches every skill that asks it. | static reading |
| Projectile speed | partially found | The item stat exists: the inventory tooltip prints `+1.50 to Projectile Speed` (`docs/RUNTIME_DATA_MODELS.md` § 16.6, measured 2026-09-24). No `Stat*` script among the 94 enumerated is projectile speed. `LoadProjectileSettings` (2255) makes about 30 `ReturnSpecificStat` reads and has one direct caller, next to the `Projectile_Player_obj` Create closures. `TalentUseSetSpeed` (3830) looked like cast speed. | static reading; item stat measured |
| hs-stat-forge | nothing | Its 15 keys are magic find, movement speed, all skills (+N and exact), exp, total damage, attack speed, FCR, skill haste, defense, crit chance and damage, spell crit chance and damage, monster density. None of the three values, and its "next candidates" list names none of them. | inspected |

### `ReturnSpecificStat`: the dispatcher's case table

- **The table has 186 cases, and 77 of them call no `Stat*` script.** The
  dispatcher's second argument is a stat id; the switch's table is filled in
  on the function's first run, and was read from that setup code. Static
  reading.
- **AoE ids reach three scripts**: id **554** goes to `StatAOESkillSize`, id
  **191** to `StatExplosionAOE`, id **196** to `StatAttackRangeMelee`. Each of
  those three scripts has exactly one direct caller found, the dispatcher.
  Static reading.
- **The projectile-amount ids are handled without a `Stat*` script.** Id
  **394** has a case with no `Stat*` callee; ids 311, 239, 240, 451 and 452
  have no case at all. Static reading.
- **The projectile-speed ids 74 and 75 have no case in the table.** So their
  value comes from the dispatcher's default handling, which was not read in
  full. Static reading; what the default returns is not established.
- **On its way out the dispatcher can round the result down** (`floor`) on one
  branch; which argument selects that branch was not fully read. Static
  reading.

### AoE size

- **`StatAOESkillSize` returns a four-element array, element 0 the total.** It
  sums several of the player's buff values (read from `playerBuff` through the
  buff accessors), adds stat **560**, and on one branch adds stat **559**. It
  calls no `min` or `clamp`, so no cap was found in it. Static reading.
  - Stat 560 is also read twice by `StatExplosionAOE`, which adds a
    contribution from stat **192** and returns a four-element array too. Stat
    559 is also read by `StatAOESkillDamage`. What 559 and 560 are called in
    the game's stat list is not established.
- **`LoadAOEModifiers` turns AoE size into element 1086 of a modifier array.**
  It takes six arguments, the first being the array it fills. It reads stat
  554 through the dispatcher, scales it by 0.01 and by its sixth argument, and
  adds the result to element **1086**. When its fourth argument is true it
  does the same with stats **552** and **553**. It also reads stats 595, 596,
  597 and 279 into other elements (918, 922, 923). Static reading.
  - **No direct caller of `LoadAOEModifiers` was found.** The same stat reads
    (554, then 552, 553, 597) appear inside `LoadAllModifiers`, which writes
    element 1086 fourteen times, so the cast path most likely goes through
    `LoadAllModifiers` rather than `LoadAOEModifiers`. Static reading; which
    of the two a given skill runs is not established.
- **`LoadProjectileSettings` adds element 1086 to the projectile's scale.**
  When the calling instance's `projEffect[1086]` is above 0, it adds that value
  to both `image_xscale` and `image_yscale`. So AoE size reaches a projectile
  as an **additive** change of its drawn (and, if the mask follows the sprite,
  collision) scale: 100 points of stat 554, with a sixth argument of 1, add
  1.0 to the scale. Static reading; the factor callers pass, and whether the
  game's `*_AOE_obj` area objects take their size from the same element, are
  not established.

### Projectile amount

- **`ReturnExtraSpellProjectiles(player, x, base)` returns the adjusted count,
  not just the extra.** Stat **394** raises `base` by a percent, with the
  result floored, and stat **311** is added on top; the result is a number. The second argument is passed
  on to the dispatcher as its third argument; what it carries is not
  established. Static reading; the 0.01 is a named global whose value was
  inferred from its use, not read.
- **`ReturnExtraProjectilesRanged(player, x)` returns the extra count only.**
  The extra is a flat stat (**239**) plus up to two chance-based bonuses: one
  worth stat **452**, gated on a chance read from stat **451** (and only for
  some skills, picked by its second argument), and one worth a single
  projectile, gated on a chance read from stat **240**. The caller adds the
  result to its own base count. Static reading; the chances' scale (plausibly
  percent) and which skills qualify for the 451/452 bonus are not established.
- **How a class script uses the count.** At each White Mage site read, the
  call is followed by a collision-list set-up and then by `instance_create_layer`
  and the new instance's scale, owner and damage fields, the shape of a spawn
  loop. Whether the count is a loop count, a spread width or a cap is not
  established; Live 1's amount steps measure it.
- **Direct callers found**: `ReturnExtraSpellProjectiles` has 44 sites in 17
  `Talents<Class>` scripts (White Mage and Shaman 5 each; Storm Weaver,
  Redneck, Plague Doctor, Necromancer, Marauder, Jotunn and Demon Spawn 3
  each; Pyromancer, Paladin, Nomad, Marksman and Exo 2 each; Samurai, Pirate
  and Amazon 1 each) plus one unnamed function. `ReturnExtraProjectilesRanged`
  has 19 sites in six (Marksman 6, Pirate 4, Demon Slayer 4, Samurai 2,
  Amazon 2, `TalentsUniversal` 1) plus one unnamed function. Static reading.

### Projectile speed

- **The speed stats are 74 and 75, read in `LoadAllModifiers`.** Right after
  the dispatcher returns stat **75**, `LoadAllModifiers` (2129) writes element
  **1085** of the modifier array, and right after stat **74**, element
  **1084**. No script on `LoadProjectileSettings`' own call path writes either
  element. Static reading, from a scan of the binary for a constant element
  index passed to the runtime's array-element writer; whether the stat value
  is stored as is or scaled first was not read.
  - `LoadAllModifiers` has 66 direct call sites: one in each `Talents<Class>`
    script, `TalentsAugments`, `TalentsRelics` and `TalentsUniversal`, four
    `LoadAura*` scripts, four in the `Projectile_Player_obj` Create closures,
    and a number of object Create closures elsewhere. Static reading.
  - That 74 and 75 are what the tooltip calls "Projectile Speed" is a reading
    by consumption only; Live 1's `projprobe ids` is the confirmation.
- **`LoadProjectileSettings` applies them to the projectile's `deltaSpeed`, not
  to the `speed` built-in.** Stat 75 (element 1085) acts as a percent
  multiplier on the calling instance's `deltaSpeed`, and stat 74 (element
  1084) as a flat addition scaled by `roomSpd`, a global speed factor. So 75
  is the multiplicative form and 74 the flat form. Static reading; the order
  in which the two combine is left to Live 1's before/after measurement and
  is recorded there as measured, not as a reading.
  - The `speed` built-in is read once in `LoadProjectileSettings`, inside a
    branch on `host`/`playerEffect`, and not written in the part read. Across
    the binary, `deltaSpeed` is referenced 1,503 times and `speed` 246 times.
    That `deltaSpeed` is what moves a player projectile each step is the
    natural reading; Live 2 measured it on Shadow Bolt (`## Live 1`).
- **The stats `LoadProjectileSettings` itself reads** (the constant loaded
  before each dispatcher call): 2, 3 to 10, 79, 194, 355, 361 and 561 to 565.
  `LoadProjectileRangedSettings` reads 195, 225 to 228, 231 and 237;
  `LoadProjectileMeleeSettings` reads 216 and 339. None is 74 or 75. Static
  reading; the scan pairs each call with the nearest constants, so a listed
  id may be an argument other than the stat id.
- **`LoadProjectileSettings` most likely runs from the projectile's Create
  event.** Its one direct caller is an unnamed function placed among the
  `Projectile_Player_obj` Create closures. Static reading, by position only;
  the Create event bodies are unnamed in the symbol file.
- **`TalentUseSetSpeed` is cast speed, not projectile speed.** It reads stats
  68 and 69 (`StatAttackSpeedMainHand`, `StatAttackSpeedOffHand`) and 106
  (`StatFasterCastRate`) alongside `GetTalentCooldown`, `GetTalentInfo` and
  `ReturnSubTalentLevel`, and is called from `TalentUse` and
  `CA_playerTalentActive`. Static reading.
- **`SetAllModifiersNew` (3157) has 436 direct call sites**, almost all in the
  `Talents<Class>` scripts; at one White Mage site it runs just before a
  `ReturnExtraSpellProjectiles` call. Whether it is what reaches `LoadAllModifiers` at cast time was not
  read, and `projprobe` does not hook it.

### Why no White Mage bar skill reached `LoadProjectileSettings`

Read after Live 1's census, where casts of seven White Mage skills moved
`StatAOESkillSize`, `StatExplosionAOE`, `ReturnExtraSpellProjectiles`,
`TalentUseSetSpeed` and `LoadAllModifiers` and none of the projectile rows; no
cast was observed calling `LoadProjectileSettings` (`## Live 1`). Static reading unless
marked; the method was the same Ghidra project plus a scan of the binary for
direct calls and for the instruction shape that passes an object index.

- **It is not a town gate.** Shadow Bolt and Restless Spirits drew their
  projectiles in town, and the helper rows ran on those casts (measured). The
  cast path ran; it does not go through the projectile rows.
- **White Mage talents create their own objects.** The White Mage talent
  script (`TalentsWhiteMage`, reached through a thin per-class wrapper) passes
  the White Mage objects' indices (`White_Mage_Shadow_Bolt_obj`,
  `White_Mage_Restless_Spirit_obj`, `White_Mage_Heavenly_Fire_obj`,
  `White_Mage_Mana_Orb_obj`, `White_Mage_Healing_Zone_obj`,
  `White_Mage_Soul_Spurn_AOE_obj`, `White_Mage_Chain_of_Holy_Light_obj` and
  others), and not `Projectile_Player_obj`'s. Among its direct callees are
  `ReturnExtraSpellProjectiles` (5 sites), `LoadAllModifiers` (1),
  `SetAllModifiersNew` (18) and `CreateAoeIndicator` (1); `LoadProjectileSettings`,
  `LoadProjectile`, `LoadAOEModifiers`, `CreatePhysicalProjectile` and
  `CA_playerProjectile` are not among them.
- **`LoadProjectileSettings` belongs to `Projectile_Player_obj`.** Its one direct
  caller is an object-event body placed in `Projectile_Player_obj`'s code,
  after its two named Create closures, and it calls `LoadProjectileSettings`
  only while a flag on the instance is set. The only scripts found passing
  `Projectile_Player_obj`'s index are `attackClasses` (the class basic attack)
  and `ProjectileCollision10Amazon`. The SDK parents every White Mage skill
  object under `Player_Damage_Parent_obj` or `Player_Ability_Parent_obj`, not
  under `Projectile_Player_obj`, so a White Mage talent does not reach
  `LoadProjectileSettings` anywhere; a White Mage reaches it, if at all,
  through the basic attack. Whether the White Mage's basic attack takes that
  branch is not established (Live 2 saw no basic attack; `## Live 1`).
- **The speed and size arithmetic is repeated in the two parents.** An event
  of `Player_Damage_Parent_obj`, and one placed in `Player_Ability_Parent_obj`'s
  code block (both found by code layout), apply to their own instance the same
  modifier-array arithmetic as `LoadProjectileSettings`: element 1085 scales
  `deltaSpeed` and element 1084 adds to it, both only while `deltaSpeed` is
  above 0, and element 1086 is added to the instance's size (next
  subsection). Which event type each body is, and whether every White Mage
  child inherits it, is not established.
- **The rows that stayed at zero.** No direct caller of `LoadAOEModifiers` was
  found; `CA_playerProjectile` is a client action with one unnamed caller;
  `LoadProjectile` is item-side; `CreatePhysicalProjectile` is called from
  collision scripts.
- **Consequence.** Each lever acts upstream, on a call the White Mage path
  makes (`ReturnExtraSpellProjectiles`, `StatAOESkillSize`, stats 74/75 under
  `LoadAllModifiers`), so the probe's levers were the right ones; Live 1
  lacked a reading of the result, which Live 2 and 3 took from the skill
  objects themselves (`skillstate`'s instance count, `tgprobe vars`, `oget`,
  `craftprobe var`).

### Where element 1086 reaches a skill object, and why the Healing Zone does not grow

Read after Live 2, where `projprobe aoe 50` applied but the Healing Zone kept
its size (measured, `## Live 1`). Static reading unless marked.

- **Naming the variables.** The runtime keeps its variable-slot globals in a
  table whose entries begin with a pointer to the variable's name, so the
  slots an unnamed body touches can be named from that table. That named
  `projEffect`, `maxScale`, `image_xscale`, `image_yscale`, `destroyTimer` and
  `loadSettings` in the bodies below without a live round (hub issue #413
  tracks the tool).
- **Where element 1086 goes.** Both parent events add the instance's
  `projEffect[1086]`, when it is above 0, to `maxScale` if `maxScale` is
  non-zero, and otherwise to both `image_xscale` and `image_yscale`. The
  Damage parent's event does this only while `loadSettings` is true; the
  Ability parent's has no such condition. Which event type each body is was
  not established.
- **The modifier list reaches a skill object through `SetAllModifiersNew`.**
  It writes a modifier list into its first argument's `projEffect`, element by
  element. `TalentsWhiteMage` calls `LoadAllModifiers` once near its start and
  then, in every object-creating branch found (Shadow Bolt, Soul Spurn,
  Mana Orb, Heavenly Fire, Restless Spirits, Satan's Mark, the Malediction
  crow, Black Mass, Burst of Light, Benediction, the Chain of Holy Light altar),
  calls `SetAllModifiersNew` on the new object.
- **The Healing Zone branch has no such call.** It creates the zone and sets
  its owner, `destroyTimer` and `heal`, and nothing else, and no read of
  `projEffect` was found in the zone's own events. Its Create sets `maxScale`
  to 3 and both image scales to 0; a per-step body grows the scales towards
  `maxScale` and clamps them to a fixed 1.5. So the zone is drawn at 1.5
  whatever `maxScale` is (measured: 1.5 and `maxScale` 3 on every cast in
  Live 2 and Live 3). Found by direct calls only; a call through the script
  table would not be seen by this scan.
- **How stat 554 becomes element 1086 on this path** was not read:
  `LoadAllModifiers`' decompile exceeded the tool's limits. Live 3 measured it
  instead: +50 on stat 554 put 0.5 in element 1086, a factor of 1 on 0.01 per
  point (`## Live 1`, Live 3).
- **Consequence.** The AoE lever shows on a White Mage skill whose branch
  calls `SetAllModifiersNew` (Soul Spurn's `White_Mage_Soul_Spurn_AOE_obj`,
  Mana Orb's `White_Mage_Mana_Orb_obj`), and not on the Healing Zone, which is
  the outlier case. Which skills `LoadAllModifiers` reads stat 554 for is not
  established: the census saw `StatAOESkillSize` run on Healing Zone, Soul
  Spurn, Heavenly Fire, Mana Orb and Shadow Bolt casts, and not on Restless
  Spirits or Chain of Holy Light.

### What this means for a slider

- **AoE size**: the `statadd` route fits. A native detour on
  `StatAOESkillSize` that adds to element 0 is the `StatSpellHaste` shape,
  and every reader of stat 554 that goes through the dispatcher sees it. The
  effect on a projectile is additive on its scale (+0.01 per point times the
  caller's factor). Static reading. **Measured** (Live 3): +50 on
  `StatAOESkillSize`'s element 0 made stat 554 read 50 inside
  `LoadAllModifiers`, put 0.5 in the Soul Spurn object's `projEffect[1086]` and
  grew its image scales from 7.5 to 8.0; the Healing Zone did not grow (the
  subsection above says why).
- **Projectile amount**: two shared helpers, so two native detours cover every
  class script that asks them. Adding `k` after the game's own calculation
  adds `k` projectiles if the class scripts use the result as a spawn count.
  The two helpers return different things (adjusted total versus extra only),
  so adding the same `k` to both is still one more projectile per `k` in each
  case. Static reading. **Measured** (Live 2) for `ReturnExtraSpellProjectiles`
  on Shadow Bolt: +2 on a return of 1 made 3 bolts instead of 1.
  `ReturnExtraProjectilesRanged` was not reached by this character (a White
  Mage) and is not observed.
- **Projectile speed**: no `Stat*` script to detour. The stat route is stats
  74/75 read by `LoadAllModifiers`; the object route is `deltaSpeed` after
  `LoadProjectileSettings` returns. `projprobe` carries both forms, and which
  may ship is the owner's call in the next workorder. **Measured** (Live 2),
  the stat route on Shadow Bolt: stat 75 acts as a percent of `deltaSpeed`
  and stat 74 as a flat addition, and the object's `speed` built-in followed
  `deltaSpeed`. The object route was not measured: no White Mage skill cast was
  observed calling `LoadProjectileSettings` (seven skills; the same instrument
  counted it from the mercenary in the same session), and no basic attack was
  observed.
- **Who else the levers reach** (measured): `Mercenary_obj` calls
  `LoadAllModifiers` about every 97 frames while it fights, and
  `LoadProjectileSettings` ran at the same rate with `self=Projectile_Player_obj`
  and no input from the player (taken to be the mercenary's shots by timing);
  many casts made a second `ReturnExtraSpellProjectiles` call with a base of 6
  beside a `LoadAllModifiers` call whose fourth argument was 243 (taken by the
  operator to be an item proc, since `Explosion_Item_obj` rows followed), and
  the amount lever raised it too (6 -> 8); the double-cast proc
  (`Universal_Double_Cast_obj`) calls `ReturnExtraSpellProjectiles`,
  `StatAOESkillSize` and `LoadAllModifiers` on its own. A slider built on these
  routes reaches all of them unless it scopes itself to the player's own casts.

## Candidates

`projprobe` (research build only, inside `#ifndef FORGEPACT_RELEASE`) installs
a native detour on every row below with `projprobe hook`, counts calls, and
logs up to 40 lines a row until `projprobe reset`: the `self` object name,
`argc`, each numeric or string argument and the return (its kind; element 0
of an array; the value of a real). `projprobe ids on` additionally records the
stat id of every `ReturnSpecificStat` call made while one of five outer rows
is on the stack: one line per new (outer row, stat id) pair, and `projprobe
show` lists every pair with its hit count and last return, so the dispatcher
calls around one cast cannot use up the budget before stats 74 and 75 appear.
A speed lever call that moved nothing (a multiplier on a native 0) is counted
as a no-op, not as applied, and the speed lever's stat form can also add
(`speed stat <id> add <bonus>`), which raises a stat the character does not
carry. Our code; the full command is in the hub's ForgePact guide.

| Script | SDK constant | Direct callers found by name | What `projprobe` logs or does | Control role |
|---|---|---|---|---|
| `StatAOESkillSize` | `gml_Script_StatAOESkillSize` (3399) | `ReturnSpecificStat` only | call row; outer row for `ids`; `projprobe aoe <bonus>` adds to element 0 of its result | lever row (AoE) |
| `StatExplosionAOE` | `gml_Script_StatExplosionAOE` (3353) | `ReturnSpecificStat` only | call row | count only; explosion area, a separate stat (191) |
| `LoadAOEModifiers` | `gml_Script_LoadAOEModifiers` (2232) | none found | call row | count only; a zero here is not evidence against the AoE route (see `LoadAllModifiers`) |
| `ReturnExtraSpellProjectiles` | `gml_Script_ReturnExtraSpellProjectiles` (3294) | 44 sites in 17 `Talents<Class>` scripts, one unnamed | call row with arguments and return; outer row for `ids`; `projprobe amount <k>` adds to its return | lever row (amount) |
| `ReturnExtraProjectilesRanged` | `gml_Script_ReturnExtraProjectilesRanged` (3293) | 19 sites in six talent scripts, one unnamed | call row; outer row for `ids`; `projprobe amount <k>` adds to its return | lever row (amount) |
| `LoadProjectileSettings` | `gml_Script_LoadProjectileSettings` (2255) | one unnamed, among the `Projectile_Player_obj` Create closures | call row (its count per cast is the projectile count, if `self` is the projectile); outer row for `ids`; `projprobe speed <mult>` scales `self`'s `deltaSpeed` and `speed` after it returns | lever row (speed); the counter for amount |
| `LoadAllModifiers` | `gml_Script_LoadAllModifiers` (2129) | 66 sites: every `Talents<Class>` script, four `LoadAura*`, four in the projectile Create closures, other Create closures | call row; outer row for `ids` (where 74 and 75 are expected); `projprobe speed stat <id> <mult>` scales, and `speed stat <id> add <bonus>` raises, stat `<id>` while it or `LoadProjectileSettings` is on the stack | lever row (speed, stat form) |
| `LoadProjectile` | `gml_Script_LoadProjectile` (2253) | two unnamed functions, two sites each | call row | count only; reads `GetItemFromFingerprint`, an item-side loader |
| `CreatePhysicalProjectile` | `gml_Script_CreatePhysicalProjectile` (696) | `hitboxCollisionChainSlice`, `hitboxCollisionTrickShot` (2), `ProjectileCollision05Nomad`, one unnamed | call row | count only; sub-projectiles spawned on collision |
| `CA_playerProjectile` | `gml_Script_CA_playerProjectile` (354) | one unnamed | call row | count only; a client action (multiplayer side) |
| `TalentUseSetSpeed` | `gml_Script_TalentUseSetSpeed` (3830) | `TalentUse`, `CA_playerTalentActive` | call row | negative control for speed: cast speed, expected to move on every cast and change no projectile |
| `GetProjectileGravity` | `gml_Script_GetProjectileGravity` (1744) | none found | call row | count only |
| `AddAoeIndicatorSize` | `gml_Script_AddAoeIndicatorSize` (105) | none found | call row | count only; the ground indicator, not the hit area |
| `CreateAoeIndicator` | `gml_Script_CreateAoeIndicator` (665) | 16 sites in 11 talent scripts (Shaman 3; Storm Weaver, Necromancer, Demon Spawn 2 each; seven others 1 each) | call row | count only; tells which bar skill is an AoE skill (census) |
| `CA_enemyProjectile` | `gml_Script_CA_enemyProjectile` (275) | not scanned | count row, never a lever | enemy side; in town a zero proves nothing |
| `ClientCreateEnemyProjectile` | `gml_Script_ClientCreateEnemyProjectile` (574) | not scanned | count row, never a lever | enemy side; in town a zero proves nothing |
| `ReturnSpecificStat` | `gml_Script_ReturnSpecificStat` (3344) | about 140,000 calls a session (`RUNTIME_DATA_MODELS`) | hooked only on the first `projprobe ids on`; one line per new (outer row, stat id) pair, 200 lines, and every pair in `projprobe show` | instrument, not a candidate |

The session's positive control is not in this table: `statadd skillhaste`,
whose `StatSpellHaste` detour is already proven native, run in the same
session to show the instrument sees a call it is known to see (Live
procedure 1, step 2).

## Live 1

Three sessions ran on 2026-10-04, all on Sorak (slot 14, a White Mage), all
with the research DLL built from ForgePact `32f16f9` (sha256
`e9064f6ab2c0b733b22b9ba824cf69471f35a1e622f3f27bdc0a17b0af5603c3`, checked by
the session's lease). Each session's operator capture is a local workorder file
in the hub (`.claude/workorders/`, not tracked); this section is a summary of
each, check by check, and cites the capture by name rather than copying it.
Everything here is **measured**; a negative is "not observed".

In all three sessions the instrument checks passed: the all-off marker
(`projprobe: hooks=0/16 amount=+0 aoe=+0 speed=x1.00 ids=off`), `ping`,
`projprobe hook: 16 native, 0 via hook, 0 blocked, 0 not found`, and the
positive control, `statadd skillhaste 5`, whose `StatSpellHaste` detour printed
its first boosted line and counted calls across a cast. That counter also rises
with no cast (background reads), so the control shows the detour fires, not
that the cast alone moved it. Each session's own idle window, a `projprobe
show` before and after a few seconds of no input, tells which rows the game
calls with no cast.

Sorak's bar (Q Mana Orb, E Healing Zone, R Soul Spurn, right click Chain of
Holy Light) was extended by the operator through the game's skill popup in
Live 1 and Live 2 (Shadow Bolt on Q, Restless Spirits on Y, Heavenly Fire on
U); no talent was spent, reset or unlearned, and the saves were backed up
before each session. No check recorded `fail` or `not-observed`; the
`not-run` ones are listed with the reason.

### Live 1 (town)

Capture: `forgepact-issue-160-skill-sliders-live-1.md`. 14 checks: 6 pass,
8 not-run.

- **Census.** The idle window (1,170 frames) left every row at 0. Across casts
  of Mana Orb, Healing Zone, Soul Spurn, Chain of Holy Light, Shadow Bolt,
  Restless Spirits and Heavenly Fire, the rows that moved were
  `StatAOESkillSize` (with `self=Player_obj`, the caster), `StatExplosionAOE`,
  `ReturnExtraSpellProjectiles`, `TalentUseSetSpeed` and `LoadAllModifiers`, and
  none of the projectile rows: `LoadProjectileSettings`,
  `LoadProjectile`, `LoadAOEModifiers`, `CreatePhysicalProjectile`,
  `CA_playerProjectile`, `ReturnExtraProjectilesRanged`, `GetProjectileGravity`,
  `AddAoeIndicatorSize` and `CreateAoeIndicator` stayed at 0: not observed on
  this character. `## Static reading` § "Why no White Mage bar skill reached
  `LoadProjectileSettings`" is the reading that explains it.
- **`speed-ids`: pass.** With `projprobe ids on` around one Shadow Bolt cast,
  `projprobe show` listed `ids LoadAllModifiers stat=75 hits=2 lastRet=real:0`
  and `ids LoadAllModifiers stat=74 hits=2 lastRet=real:0`, and 74/75 under no
  other row; nothing was saturated and no `stat=?` line appeared. The same run
  showed stats 560 and 559 read under `StatAOESkillSize` and 394 and 311 under
  `ReturnExtraSpellProjectiles`, the ids the static reading names. Sorak reads
  0 for both speed stats.
- **Amount, AoE and speed, levers applied but no result read** (all
  `not-run (instrument: inconclusive)`). `projprobe amount 2` printed
  `first boosted call 1 -> 3` on Shadow Bolt, but the bolts could not be
  counted from screenshots and `LoadProjectileSettings` was not called, so it
  gave no count. `projprobe aoe 50` printed `first boosted call 0 -> 50` on
  Healing Zone, with no object to read. `projprobe speed stat 75 add 0.5` and
  `speed stat 74 add 0.5` each applied (`statApplied=1`, `first boosted call
  0 -> 0.5`), with no projectile object read.
- **Outlier, Chain of Holy Light with all three levers armed.** The skill
  moved no lever row of its own, but the speed lever applied and the amount
  lever printed `first boosted call 6 -> 8`, on the base-6
  `ReturnExtraSpellProjectiles` call that rode along with the cast (see
  "Who else the levers reach" above).
- `enemy-rows`: not-run (no enemy fired in town). `save-clean`: not-run (the
  restore was left to the driver).

### Live 2 (combat zone)

Capture: `forgepact-issue-160-skill-sliders-live-2.md`. Outskirts of Inoya
(`Act_01_01`), reached by the town waypoint. 18 checks: 12 pass, 6 not-run.

- **Idle window.** With an enemy in view and no input, `LoadProjectileSettings`
  (`self=Projectile_Player_obj`), `TalentUseSetSpeed` and `LoadAllModifiers`
  (both `self=Mercenary_obj`) each ran 28 times in 2,670 frames: the player's
  mercenary, fighting. Every other row stayed at 0.
- **`census-lps`: not-run.** No bar skill moved `LoadProjectileSettings` above
  that idle rate. The basic-attack route is not observed, and this instrument
  could not have seen it: a basic attack's call would carry
  `self=Projectile_Player_obj`, like the mercenary's, and the point-in-time
  `tgprobe vars Projectile_Player_obj` read never found even the mercenary's
  `Projectile_Player_obj` while `LoadProjectileSettings` ran on it 28 times.
  The mercenary killed the enemies within seconds.
- **`amount-count-baseline`: pass.** `skillstate`'s instance count of
  `White_Mage_Shadow_Bolt_obj` read 1 after each of three single Shadow Bolt
  casts, each with `ReturnExtraSpellProjectiles` called with a base of 1 and
  returning 1.
- **`amount-count-boost`: pass.** With `projprobe amount 2` the first boosted
  line read `1 -> 3`, and two single casts left 3 bolts each. A double cast
  left 7: the player's call (base 2 that time) returned 4 and the double-cast
  proc's (base 1) returned 3. `amount-route: proven`.
- **`aoe-obj-baseline` and `aoe-obj-boost`: pass by the procedure's rule, but
  the zone did not grow.** Healing Zone at +50: the first boosted line read
  `0 -> 50`, but `image_xscale`/`image_yscale` stayed 1.5 and `maxScale` 3,
  as at baseline; only a step timer and a camera value moved, and the
  screenshot showed no larger area. The static reading above explains why this
  measured the zone, not the lever; Live 3 took the AoE route on other skills.
- **`speed-skill-baseline`: pass.** Two Shadow Bolts read `deltaSpeed`
  2.916667 each.
- **`speed-skill-boost`: pass**, the stat form, each on a new bolt, with
  `statApplied` rising and the first boosted line printed every time:

  | Lever | `deltaSpeed` | Against 2.916667 | `speed` built-in at the read |
  |---|---|---|---|
  | `speed stat 75 add 0.5` | 2.931250 | × 1.005 | 2.943725 |
  | `speed stat 75 add 50` | 4.375000 | × 1.5 | 4.358970 |
  | `speed stat 74 add 0.5` | 3.125000 | + 0.208333 | 3.123900 |
  | `speed stat 74 add 50` | 23.750000 | + 20.833333 | 21.375000 |

  So stat 75 is a percent of `deltaSpeed` (`1 + value / 100`) and stat 74 a
  flat addition of 5/12 (0.416667), added to `deltaSpeed`, per point in this
  zone.
  On all six Shadow Bolt reads (two baselines, four boosted) the `speed`
  built-in equalled `deltaSpeed` times the object's `deltaTimer` at the read,
  so `deltaSpeed` is what sets the bolt's movement. `speed-route: proven`
  (stat form). The two stats were not raised together, so the order they
  combine in is not observed.
- **`attack-ids`, `speed-attack-baseline`, `speed-attack-boost`: not-run.** No
  basic attack was seen, so the instance form (`projprobe speed <mult>` on
  `Projectile_Player_obj`) was not measured.
- `enemy-rows`: not-run (`CA_enemyProjectile` and `ClientCreateEnemyProjectile`
  stayed at 0 with enemies on screen, but no screenshot showed an enemy shot).
  `save-clean`: not-run (left to the driver).

### Live 3 (town, AoE only)

Capture: `forgepact-issue-160-skill-sliders-live-3.md`. 14 checks: 12 pass,
2 not-run. `oget` and `craftprobe var` both read `Player_obj.image_xscale` as a
real first (`read-control`), and `projprobe ids on` stayed armed throughout.

- **`spurn-baseline`: pass.** Two single Soul Spurn casts, each a new
  `White_Mage_Soul_Spurn_AOE_obj`: `image_xscale` = `image_yscale` = 7.5,
  `maxScale` 0, `projEffect[1086]` 0, and stat 554 read 0 under
  `LoadAllModifiers`.
- **`spurn-boost`: pass.** With `projprobe aoe 50`: first boosted line `0 -> 50`
  after the cast, stat 554 read 50 under `LoadAllModifiers` in the same frame,
  `projEffect[1086]` 0.5, and both image scales 8.0 (`viewSize` 512 -> 544).
  `aoe-route: proven`.
- **`aoe-ids`: pass.** The `StatAOESkillSize` call, its first boosted line, the
  stat 554 `ids` line (`ret=real:50`, 0 at baseline) and the `LoadAllModifiers`
  call came in that order, in one frame.
- **`orb-baseline`: pass; `orb-boost`: not-run.** Both baseline Mana Orbs were
  double casts, reading 1.15 on both scales, `maxScale` 0 and element 1086 0.
  The boosted cast was a single cast: 1.65 on both scales and element 1086 0.5,
  and a visibly wider aura, but with no single-cast baseline among the written
  two the check stays inconclusive. A third, supplementary single-cast baseline
  outside the procedure read 1.15.
- **`hz-modifiers`: pass.** Healing Zone at +50: stat 554 read 50, but the
  zone's `projEffect[1086]` read 0 (element 1086 read above 0 on two other
  objects in the same session), `maxScale` 3 and `image_xscale` 1.5: the
  modifier list does not reach the zone.
- `save-clean`: not-run (left to the driver).

### What the three sessions settle

- **Amount**: on Shadow Bolt the number `ReturnExtraSpellProjectiles` returns
  is the number of bolts, and `+k` on its return is `+k` bolts.
  `ReturnExtraProjectilesRanged` is not observed (no White Mage call).
- **AoE**: stat 554 reaches a White Mage skill object as `projEffect[1086]` at
  0.01 per point, added to its image scales when `maxScale` is 0 (Soul Spurn,
  measured; a Mana Orb object read the same, a supporting read only, since its
  check was left not-run). Healing Zone is the outlier and does not grow.
- **Speed**: stats 74 and 75 are the projectile-speed stats, stat 75 a percent
  of `deltaSpeed` and stat 74 a flat addition, and `deltaSpeed` drives the
  `speed` built-in. Which of them the tooltip's "Projectile Speed" line is,
  and the instance form, are not observed.
- **Route tokens**: `amount-route: proven`, `speed-route: proven`,
  `aoe-route: proven`.

## Not established

- What `ReturnSpecificStat`'s default handling does with a non-zero value for
  an id with no case (74 and 75 among them; Sorak, with no projectile-speed
  gear, read 0 for both), and which branch rounds a result down.
- Which of stats 74 and 75 the tooltip's `+1.50 to Projectile Speed` line is.
  Both act on projectile speed (measured); no character with that gear was
  read.
- How the 5/12 per point that stat 74 adds to `deltaSpeed` splits between the scale
  `LoadAllModifiers` stores it with and `roomSpd`, and whether it is the same
  in other rooms. Stat 75's 0.01 per point is measured.
- In which order stats 74 and 75 combine on `deltaSpeed` (they were not raised
  together), and what either does when it is negative.
- The instance form of the speed lever (`projprobe speed <mult>` on a
  `Projectile_Player_obj`) and whether a White Mage's basic attack makes a
  `Projectile_Player_obj` at all: no basic attack was seen.
- `ReturnExtraProjectilesRanged` in play: no White Mage skill calls it. The
  scale of its chances, which skills its second argument qualifies for the
  451/452 bonus, and what `ReturnExtraSpellProjectiles`' second argument
  carries.
- How a class other than the White Mage uses the extra-projectile count (loop
  count, spread or cap). On Shadow Bolt the count is the number of bolts
  (measured).
- What stats 559 and 560 are, what factor callers pass as `LoadAOEModifiers`'
  sixth argument, and how `LoadAllModifiers` turns stat 554 into element 1086
  in general (on Soul Spurn the net is 0.01 per point, measured; a Mana Orb
  read agreed, its check not-run).
- Which skills `LoadAllModifiers` reads stat 554 for, which event types the two
  parent bodies are, and whether every White Mage child inherits them.
- What element 1086 does to a skill object whose `maxScale` is non-zero (by the
  reading, it is added to `maxScale`); no such object took the lever.
- How a slider keeps to the player's own casts. The mercenary, the base-6
  helper call that rides along with casts, and the double-cast proc all reach
  the routes (measured); summons and NPCs whose Create closures call
  `LoadAllModifiers` (for example `Necro_Summon_Parent_obj`, `Mariel_NPC_obj`,
  `Pyromancer_Volcano_obj`) were not observed either way. The enemy count rows
  saw no enemy shot.
