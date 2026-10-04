# Skill sliders: projectile amount, projectile speed, AoE size (issue #160)

Status (2026-10-04): **research phase; nothing player-visible ships.** This
document answers, for each of the three values issue #160 asks to make
adjustable, where Hero Siege computes it, which script a ForgePact hook would
detour to change it for the player's skills only, and what is still open. The
static reading is done (`## Static reading`). The research build's `projprobe`
command hooks every candidate in one DLL (`## Candidates`); its live session
has not run yet (`## Live 1`). The sliders themselves (panel rows, `statadd`
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
    natural reading, not established; Live 1 step 10 measures it.
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

### What this means for a slider

- **AoE size**: the `statadd` route fits. A native detour on
  `StatAOESkillSize` that adds to element 0 is the `StatSpellHaste` shape,
  and every reader of stat 554 that goes through the dispatcher sees it. The
  effect on a projectile is additive on its scale (+0.01 per point times the
  caller's factor). Static reading; Live 1 steps 6-7 measure it.
- **Projectile amount**: two shared helpers, so two native detours cover every
  class script that asks them. Adding `k` after the game's own calculation
  adds `k` projectiles if the class scripts use the result as a spawn count.
  The two helpers return different things (adjusted total versus extra only),
  so adding the same `k` to both is still one more projectile per `k` in each
  case. Static reading; Live 1 steps 4-5 measure it.
- **Projectile speed**: no `Stat*` script to detour. The stat route is stats
  74/75 read by `LoadAllModifiers`; the object route is `deltaSpeed` after
  `LoadProjectileSettings` returns. `projprobe` carries both forms, and which
  may ship is the owner's call in the next workorder.

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

Not yet run.

## Not established

- What `ReturnSpecificStat`'s default handling returns for an id with no case
  (74 and 75 among them), and which branch rounds a result down.
- Whether stats 74 and 75 are what the tooltip calls "Projectile Speed", and
  which of them the `+1.50` line is. Live 1 step 8.
- Whether `LoadAllModifiers` stores stats 74 and 75 as read or scaled, and so
  what one point does to `deltaSpeed`.
- That `deltaSpeed`, not the `speed` built-in, is what moves a player
  projectile each step. Live 1 steps 9-10.
- Whether `SetAllModifiersNew` is the cast-time route into `LoadAllModifiers`.
- What stats 559 and 560 are, and what factor callers pass as
  `LoadAOEModifiers`' sixth argument.
- Whether the game's AoE area objects (`*_AOE_obj`, under
  `Player_Damage_Parent_obj`) take their size from `projEffect[1086]` the way
  projectiles do, or from a variable of their own. Live 1 steps 6-7 read their
  variables.
- How the class scripts use the extra-projectile count (loop count, spread or
  cap), and so whether `+k` on the helpers' return is `+k` projectiles. Live 1
  steps 4-5.
- The scale of the chances in `ReturnExtraProjectilesRanged`, which skills
  its second argument qualifies for the 451/452 bonus, and what
  `ReturnExtraSpellProjectiles`' second argument carries.
- In which order stats 74 and 75 combine on `deltaSpeed`, and what either does
  when it is 0 or negative. Live 1 steps 9-10.
- Whether a lever on these scripts stays with the player's own skills. The
  two extra-projectile helpers are reached from the `Talents<Class>` scripts
  plus one unnamed function each, which was not identified. `LoadAllModifiers`
  is also called from Create closures of non-player objects (for example
  `Necro_Summon_Parent_obj`, `Mariel_NPC_obj`, `Pyromancer_Volcano_obj`), so
  a stat-form speed lever scoped to it may reach summons or NPCs; that was
  not observed either way. The enemy count rows only show the enemy side's
  own scripts.
