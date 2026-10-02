# Bosses at a forced rarity ("uber" bosses) — research, 2026-10-02

Issue #44, notes 2.2.0. The **Bosses** control (Mods › Gameplay, config key
`boss_rarity`, plugin command `bossrarity off|rare|ancient|status`, off by
default) makes every boss the game spawns while it is on roll as Rare (3,
"uber" boss) or Ancient (4, "uber uber" boss). It does that the way the
Monster Rarity sliders and Tyrant's Crown already raise ordinary monsters: by
writing `enemyRarity` at the entry of the game's own `EnemyRaritySettings`,
before the game builds the monster from it.

Status: **Live procedure 1 has run (2026-10-02 rerun, `boss-rarity: hp-only`).**
A Karp King raised to Ancient was built with ×5.65 its rank-1 health, more
than an ordinary monster's rank-4 ×4.23. Its damage, XP, drop rank, drops
and look were not observed, so nothing in this file, the panel, the README or
the release notes says the raise changes any of those.

## Static reading

Read on 2026-10-02 in the local Ghidra project (`%USERPROFILE%\ghidra_projects\HeroSiege`,
program `Hero_Siege.exe`; Ghidra 12.1.4 headless through
`support\analyzeHeadless.bat`). What follows is what was learned, in our own
words; nothing from the game's script bodies is reproduced here, and the
decompiler output stays on the reader's machine.

- **Where the rarity is consumed.** `EnemyRaritySettings(typeId)` runs from
  `Enemy_Parent_obj`'s Alarm 4 with the monster as `self`, after the spawner
  has decided `enemyRarity` (1 normal, 2 champion, 3 rare, 4 ancient) and
  filled `enemyAffix` / `affixList`, and before the stats, affix effects and
  health bar are built. That is a live trace on ordinary monsters
  (2026-09-05, README § "Tyrant's Crown"), not this reading. Bosses descend
  from `Enemy_Parent_obj` through `Enemy_Child_Boss_obj`, so they pass the same
  alarm; a player report against 1.4.1 (an Anubis whose health rose about
  ninefold with both rarity sliders at 20 %) is why the sliders have skipped
  bosses since 1.4.3. That a boss passes through this hook was not traced by
  us when this was read; Live procedure 1's control traced it on 2026-10-02
  (a Karp King `ENTRY`/`EXIT` pair at rank 1).
- **Which objects are bosses.** `hs-game-sdk` lists 41 descendants of
  `Enemy_Child_Boss_obj` (1407), for example `Karp_King_obj` 2368, `Damien_obj`
  1115, `Uber_Damien_obj` 4945, `Uber_Anubis_obj` 4938 and `Uber_Luna_obj`
  4952. The chain is `<boss> -> Enemy_Child_Boss_obj -> Enemy_Parent_obj ->
  Avoidable_Parent_obj`, and the only direct children of `Enemy_Parent_obj`
  are `Demon_Lightning_obj`, `Enemy_Child_Basic_obj`, `Enemy_Child_Boss_obj` and
  `Enemy_Child_Destructible_obj`, so ancestry alone identifies a boss.
  `Ghost_Pirate_King_Boss_obj` is a `Collision_Prop_obj`, not an enemy, and is
  not covered.
- **No function boundary for the scripts that matter.** `EnemyRaritySettings`,
  `DropItem`, `DropItemBoss`, `CreateEnemyElite`, `CA_enemyCreate`,
  `DropBossGems` and `DropBossRunes` are among the 83 rows of `symbols.csv`
  that landed in module `other`: the dump ran with ForgePact's table hooks
  installed, so the project has no symbol for them
  ([static-model workflow, tooling findings](../../docs/agents/static-model-workflow.md#tooling-findings)).
  The research build installs the rarity hook at startup, so a plain re-dump
  from it does not fix this script.
- **Direct call sites** (a caller search over `call rel32`): `LoadMonsterDropTables`
  has one caller, at 0x141962abe; `ReturnEnemyStats` a cluster of five at
  0x141b257e5..0x141b25e59 and about twenty more elsewhere; `LoadDrops` one, at
  0x14180734a (inside `DropItem`, by the Prime Evil reading);
  `LoadMonsterAffixes` five; `EnemyAffixes` none (it is reached through the
  table or as a method); `CA_createBoss` one, at 0x14770ba99.
- **Where the reading stopped.** The first two sites sit in one unnamed run of
  code, about 596 KB from 0x141936290 with no padding inside it, which Ghidra's
  decompiler refuses as too long. So the body of `EnemyRaritySettings` was
  **not read**: whether it picks a stat set and a drop table by the rarity it is
  handed, and whether a boss takes a branch of its own, is not established by
  this reading. The S10 notes recorded drop-table constants inside this script
  on an earlier build, which fits a script that writes the rank's drop table,
  but that is an old note, not a reading of this build.
- **Bodies decompiled and skimmed, with no conclusion drawn:**
  `ReturnEnemyStats` (176 KB, no referenced strings), `EnemyAffixes`,
  `LoadMonsterAffixes`, `LoadMonsterDropTables` (453 KB),
  `EnemyDestroyExperience`, `CA_createBoss`, `DropBossParts`.
- **Two ways to read further**, neither needed for the control to work:
  (a) a disassembly window around 0x141b257e5 and 0x141962abe, to see which
  comparisons gate the five `ReturnEnemyStats` calls; (b) `citrace symdump`
  from a research build with the startup rarity hook and creation hooks left
  out, then `ForgePact/tools/ghidra/ImportSymbols.java` again, which would give
  `EnemyRaritySettings` its own function boundary. Both stay local.

Rejected routes, from the same search: a hook on a boss-only script
(`CA_createBoss`, the boss portal and shrine objects) would miss bosses a room
places and duplicate the sliders' route; writing `forceRarity` instead of
`enemyRarity` has nothing recorded showing the setup honours it (the research
build's `raritytrace` prints it at entry and exit, so Live procedure 1 sees its
value anyway).

## Live procedure 1

### The instrument (research build only)

Both commands are compiled out of the player build (`#ifndef FORGEPACT_RELEASE`,
pinned by `tests/test_boss_rarity_contract.py`).

- `bossprobe` prints one `bossprobe control:` line, then one line per live boss
  (an `Enemy_Parent_obj` instance whose object descends from
  `Enemy_Child_Boss_obj`), then `bossprobe: <n> boss(es) among <m> enemies`.
  - The control line reads `gDataProtected[177]` through the proven `GPV` and
    through the game's `PC_GetVariableGMLWrapper`, and names the getter every
    later read uses: `getter=PC_GetVariableGMLWrapper (agrees with GPV)`,
    `getter=GPV (wrapper did not agree)`, or `getter=unproven`. Slot 177 is the
    one the plugin's Angelic code already reads as the base Heroic chance (28
    unmodded); that it holds its own key is the probe's assumption, which the
    control line itself tests.
  - A boss line carries its name, `enemyRarity`, `forceRarity`, affixes and
    health bar, then every instance variable whose name contains `hp`,
    `health`, `damage`, `dmg`, `exp`, `slots`, `chance`, `dropmult` or
    `droptable`.
  - A number is handed to the getter only when it is a whole number in
    `[0, 262144)`, the protected store's size: a -1 handle passed into that
    store faulted the game (`docs/RUNTIME_DATA_MODELS.md` § 5.8), and the
    plugin's `catch (...)` does not catch an access violation. Anything else
    prints `->not-key`; with `getter=unproven` every in-range number prints
    `->unread`, which records the call shape that failed rather than a fact
    about the boss.
  - Only `enemy_hp` is a known key (the kill route in
    `tools/boss_drop_trial.py` writes 0 through it), and prints as
    `enemy_hp=<key>-><value>`. Every other in-range number prints `->?<value>`:
    what that record holds if the number is a key, and an unrelated record if
    it is not. A `->?` value is never counted as evidence.
- `droptrace <n>` (default 20, at most 500, `off` or `0` stops it) prints the
  next n `DropItem` / `DropItemBoss` calls that pass ForgePact's drop hooks, one
  `droptrace: <script> self=<instance> argc=<n> <arguments>` line each.
  `DropItem`'s first argument is the drop rank on ordinary monsters
  (`docs/RUNTIME_DATA_MODELS.md` § 13.7). It installs the drop hooks if they
  are not already in (the research build has them from startup).

### Session

**2026-10-02, aborted after the control.** Capture
`.claude/workorders/forgepact-issue-44-uber-bosses-live-1.md` (hub, local),
research DLL `a3efc8e8...bef29`.

- Passed: `dll-hash`, `marker` (`bossrarity: off ... hook=ok`) and `control`
  (a Karp King `rarity` `ENTRY`/`EXIT` pair at `enemyRarity=1`).
- Then the game froze. The owner watched YYToolkit's console fill with the
  runner's own `YYError` text, "Unable to find any instance for object index
  '<id>'", raised in `timer_system_update`, a different id on each line
  (owner-observed, not instrumented). `out.txt` holds none of it, so no
  ForgePact print was involved. The game's `bin\YYToolkit.log` kept only
  report #1, id 257102, which is the vanilla error every launch raises at
  character load (`docs/RUNTIME_DATA_MODELS.md` § 5.7), not a finding; the
  repeats take the toolkit's count-only path and are not in the file.
  `no-crash` was recorded as a fail (frozen after the raw
  `instance_create_depth` spawn); every other check is `not-run`.
- Conditions: the hero stood in a Hell zone (Outskirts of Inoya, zone level
  243), and that zone's load had already stalled the game 50.5 s.
- What left those timers without an owner is **not measured**. A timer in the
  game's timer list addressed an instance that no longer existed, and several
  did; whether those were short-lived objects of the raw spawn or the Hell
  zone's own population is open. This record does not say the spawn caused
  the storm.
- Why the console froze the game is the #58 mechanism: the runner writes each
  runtime error to YYToolkit's console synchronously. The player build has
  detached from that console since #58; the research build kept it until this
  session. It now detaches too (the module guide's #58 entry).

The rerun used the detached-console research build, a zone confirmed by
screenshot before any spawn, and a liveness check after every spawn and kill,
with the runner's own error counter read before and after each spawn.

**2026-10-02, the rerun.** Capture
`.claude/workorders/forgepact-issue-44-uber-bosses-live-1.md` (hub, local; its
`# RERUN` block), research DLL
`f85bc1d557ef9b9880f7a8d2931fbe85771dfad16400b7db8a4d48b1be0289cf`, the
detached-console build. Character slot 14 (Sorak); lease held 18:09:25Z to
18:35:05Z; saves restored clean afterwards. `tools/live_checks.py` over the
plan's 26 checks: 19 pass, 0 fail, 5 not-observed, 2 not-run.

Route tokens the driver set from it: **`boss-rarity: hp-only`** and
**`boss-drops: not-observed`**.

The lines below are quoted from the capture with two fields left out, because
the repository's decompiled-output check reads the runner's word for an
unset value followed by a name as a decompiler declaration: every `rarity`
and `bossprobe` line printed `forceRarity` unset (the runner's `undefined`)
at entry and at exit, raised or not, and every `rarity ... EXIT` line printed
the script's return value, also `undefined`, after its `->`. Both are
dropped from each quote; nothing else is changed.

Where, and under what error rate:

- `zone-check` **pass**: the banner read "Outskirts of Inoya" / "Nightmare" /
  "Zone Level 170" (modifier line "Flooded Plains"), with no "Safe Zone" and
  no "Town of". Nightmare was the lowest difficulty the character offered.
- `yyerror-baseline` **pass**: `YYToolkit.log`'s summary read `total=1` at
  18:21:01Z and still `total=1` at 18:21:48Z, growth 0 over 47 s, and the zone
  load left no `STALL ended` line.
- `yyerror-delta` **pass**: +0 to +3 per spawn, `total` 1 -> 12 over about
  13 minutes and 8 spawns. `yyerror-storm` **pass**: no spawn above 300, the
  largest +3.
- The log's two full reports: #1, the vanilla `Unable to find any instance for
  object index '257102'`; #2, new, `REAL argument incorrect type undefined`,
  raised in the ancient Karp King's spawn window with `BloodPactPlugin` frames
  on its stack, so a ForgePact call handed the runner `undefined` where it
  wanted a number. One report, not a flood; this session did not trace it.
- `no-crash` **pass**: `pong` after every spawn, every kill and the final ping.

The instrument checks, and what each proves about the instrument:

- `dll-hash` **pass**: the lease's `dll_sha256` is the dispatched
  `f85bc1d5...289cf`. `marker` **pass**:
  `bossrarity: off raised=0 (rare 0, ancient 0) seen=0 enemyBorn=0 notRank1=0 writeFailed=0 hook=ok`,
  so the build carries the command and the shared hook is in.
- `control` **pass**:
  `rarity #127 ENTRY self=Karp_King_obj#299102 argc=1 a0=int64:302 enemyRarity=int64:1 affixList=0[] enemyAffix[58] set=- myHealthBar=real:-4.000000`,
  its matching `EXIT` at `enemyRarity=int64:1`, then `pong (YYTK 4.0.1)`. A
  boss does pass through `EnemyRaritySettings`, and with the control off the
  game left it at rank 1.
- `baseline-karp` **pass**: the probe's control line proved its getter
  (`getter=PC_GetVariableGMLWrapper (agrees with GPV)`), and the boss read
  `enemyRarity=int64:1` and `enemy_hp=real:178801.000000->real:44625000.000000`.
- `baseline-drop-trace` **not-observed** (lines: none). It was to show that
  `droptrace` sees a boss's own `DropItem` at rank 1, the anchor every
  later drop rank is compared with. With `droptrace 20` armed, the rank-1
  Karp King's death printed no `droptrace:` line and added no line to
  `itemdrops.jsonl`. The trace was blind on a boss in this session, so
  `ancient-drop-rank` could not run.
- `drop-rank-control` **pass**: a `Skeleton_Mage_Fire_obj` the sliders raised
  (`rarity #323 EXIT -> enemyRarity=real:4.000000 ...`) died with
  `droptrace: DropItem self=Skeleton_Mage_Fire_obj#312258 argc=12 a0=real:4.000000 a1=int64:0 a2=real:2800.000000 a3=real:4264.000000 a4=real:1.000000 a5=real:500.000000 a6=kind=15 str=ref ds_list 1243 a7=kind=15 str=ref ds_list 1244`.
  The trace does see a raised rank on an ordinary monster; the blindness
  above is the boss's, not the trace's in general.
- `visual-source-control` **not-observed**: `oset Karp_King_obj enemyRarity 4`
  read back `now real:4.000000`, but the boss's body was off screen in both
  shots (spawned 1200 px from the hero) and the HUD boss bar drew the name
  the same way before and after. Whether the ancient name style is built by
  the setup or drawn each frame from `enemyRarity` is unknown, so no
  screenshot of this session counts as something the game built.
- `game-built-raise` **pass**, decided by `ancient-hp` alone (below).

What the hook wrote (readbacks of our own write; none of them decides the
route):

- `ancient-karp-raised` **pass**:
  `rarity #197 ENTRY self=Karp_King_obj#304071 argc=1 a0=int64:302 enemyRarity=int64:1 affixList=0[] enemyAffix[58] set=- myHealthBar=real:-4.000000`,
  then
  `rarity #197 EXIT  -> enemyRarity=real:4.000000 affixList=3[real:17.000000, real:25.000000, real:16.000000] enemyAffix[58] set=16,17,25 myHealthBar=real:-4.000000`,
  and `bossrarity: ancient raised=1 (rare 0, ancient 1) seen=1 enemyBorn=0 notRank1=0 writeFailed=0 hook=ok`.
- `rare-karp-raised` **pass**:
  `rarity #230 EXIT  -> enemyRarity=real:3.000000 affixList=2[real:13.000000, real:6.000000] enemyAffix[58] set=6,13 myHealthBar=real:-4.000000`.
- `ancient-damien-raised` **pass** (`rarity #259`, `Damien_obj#308339`),
  `ancient-uber-damien-raised` **pass** (`rarity #284`,
  `Uber_Damien_obj#309455`) and `ancient-uber-anubis-raised` **pass**
  (`rarity #289`, `Uber_Anubis_obj#309794`): each entered at
  `enemyRarity=int64:1` and its `EXIT` read `enemyRarity=real:4.000000` with
  three affixes. The hook wrote the rank, and it was still there after the
  game's own setup ran.
- `ancient-affixes` **pass**: the ancient Karp King's `EXIT` carried three
  affixes (17, 25, 16). That is our own top-up read back; the HUD drawing
  their names ("Multishot, Treasure Gobbler, Pyromaniac") counts for nothing.
- `sliders-leave-bosses` **pass**: with the control off and `rarity 0 100`,
  `rarity #290 EXIT -> enemyRarity=int64:1 ...` on
  `Karp_King_obj#310083`, and the sliders' `bosses left alone` went 0 -> 1.

What the game built:

- `ancient-hp` **pass**. The game built a higher `enemy_hp` from the rank the
  hook wrote: rank 1 read 44,625,000 on both baseline spawns (gap 0), the
  ancient Karp King 252,242,812, ×5.65 (5.6525), past both of the check's
  thresholds (at least 1.5 times the base, and a rise larger than the gap).
  The ordinary monsters' rank-4 health median is ×4.23 (`docs/RUNTIME_DATA_MODELS.md`
  § 13.7), so this boss rose 1.34 times as far: on health a raised boss does
  not follow the ordinary rank table (`monster_rank_model`'s
  `boss_hp_follows_rank_table` is `False`). One boss, one kill, one zone.
  The three other ancient bosses read `Damien_obj` 159,906,250,
  `Uber_Damien_obj` 1,306,210,937 and `Uber_Anubis_obj` 4,451,343,750, with
  no rank-1 spawn of the same boss to compare.
- `ancient-damage` **not-observed**: the only damage-named variable,
  `damage`, printed as `->?` (an unproven key) in every read, 217 at rank 1
  and 716 at rank 4. A `->?` value is not evidence.
- `ancient-xp` **not-observed**: `killExperience` (4950 -> 37129) and
  `experience` (2152 -> 16143) printed as `->?` too; `experienceColor` read
  as a plain number but is a colour, and did not change.
- `ancient-drop-rank` **not-run** (instrument blind: no boss `DropItem` line
  at the rank-1 death, and none at the ancient death either). The rare Karp
  King's traced death did print one line,
  `droptrace: DropItem self=Karp_King_obj#306374 argc=12 a0=real:3.000000 a1=int64:2 a2=real:2800.000000 a3=real:4344.000000 a4=real:1.000000 a5=real:0.000000 a6=kind=15 str=ref ds_list 1243 a7=kind=15 str=ref ds_list 1244`:
  a first argument of 3, which without the rank-1 anchor is a number, not a
  verdict.
- `ancient-drops` **not-observed**: one kill at each rank, both with
  `droptrace` armed, added 0 lines to `itemdrops.jsonl` (0 -> 0). The
  untraced rank-1 kill added 20 and the traced rare kill 20, so the zeros
  are not a count of a boss's drops. Why those two deaths dropped nothing is
  not established in this session.
- `ancient-visual` **not-run** (no control: `visual-source-control` was not
  observed). The HUD bar's name style was the same in every shot.
- `DropBossGems`, `DropBossRunes` and `DropBossParts`: not measured (not
  instrumented).

The `bossprobe` boss line before the raise (b1, whole):

```
bossprobe #0 Karp_King_obj#299102 enemyRarity=int64:1 affixList=0[] enemyAffix[58] set=- myHealthBar=real:-4.000000 | dropTable=kind=15 str=ref ds_list 1243 dSatanicDropMult=real:178781.000000->?real:0.215000 dSlots=real:178778.000000->?real:10.000000 m_EnemyDamageParent=object/struct currentHpPercentage=real:178807.000000->?real:1.000000 enemy_hp=real:178801.000000->real:44625000.000000 damage=real:178800.000000->?real:217.000000 damage_type=int64:4->?real:0.000000 damageActive=bool:true damageDealer=real:178806.000000->?real:-4.000000 damageTaken=real:178804.000000->?real:0.000000 damageTakenTimer=real:178805.000000->?real:-1.000000 dCommonChance=real:178779.000000->?real:58.000000 dCommonDropMult=real:178780.000000->?real:70.000000 myHealthBar=real:-4.000000->not-key killExperience=real:178783.000000->?real:4950.000000 etherEbKeyChance=real:178794.000000->?real:0.000000 etherSrKeyChance=real:178793.000000->?real:0.000000 experience=real:178799.000000->?real:2152.000000 experienceColor=real:16711890.000000->not-key experienceTxt=string:"" hitRegListDamage=kind=15 str=ref ds_list 1246 trapDamageTimer=real:0.000000->?real:0.000000 max_hp=real:178802.000000->?real:44625000.000000 drawHealthbar=bool:true maxHpUnscaled=real:178803.000000->?real:44625000.000000
bossprobe: 1 boss(es) among 127 enemies
```

and after it (the ancient Karp King, as the capture records it; the `...`
are the capture's own elisions, the whole line is in that session's
`out.txt`):

```
bossprobe #0 Karp_King_obj#304071 enemyRarity=real:4.000000 affixList=3[real:17.000000, real:25.000000, real:16.000000] enemyAffix[58] set=16,17,25 myHealthBar=real:-4.000000 | dropTable=kind=15 str=ref ds_list 1243 dSatanicDropMult=...->?real:0.215000 dSlots=...->?real:14.000000 ... currentHpPercentage=...->?real:1.000000 enemy_hp=real:178801.000000->real:252242812.000000 damage=real:178800.000000->?real:716.000000 ... killExperience=real:178783.000000->?real:37129.000000 ... experience=real:178799.000000->?real:16143.000000 experienceColor=real:16711890.000000->not-key ... max_hp=...->?real:252242812.000000 ... maxHpUnscaled=...->?real:252242812.000000
```

**What `boss-rarity: hp-only` means for this feature.** The one game-built
check that passed is `ancient-hp`, at ×5.65; `ancient-visual` did not run.
None of the damage, XP or drop rank checks passed, so the issue's own
condition (that a rarity changes a boss's drops and damage, not only its
health) is not met by this session. The owner's decision for this token
("Ship, measured wording only", 2026-10-02) keeps the feature going; the
panel, README and release-notes wording is `live2-record`'s, and may name
the health change only. Live procedure 1b, on the research build with the
plugin's runner error fixed, is the next attempt at damage, XP, the look
and the drops.

## The plugin's runner error and the traced kills

Two things in the Live 1 rerun pointed at ForgePact rather than the game, and
the owner asked for both to be found and fixed before Live 2 ("Find and fix
first", 2026-10-02). One is found and fixed; the other is narrowed to the
game's side and left for Live procedure 1b to measure.

**The report.** YYToolkit full report #2 carried the runner's text
`REAL argument incorrect type undefined` with seven `BloodPactPlugin+` frames. It
first appeared at the ancient Karp King's spawn; there was none for the
control and visual-control spawns, when every mode was off, and the
session's summary line `top=report#2 x<n>` then climbed by one per later
spawn, to x7 (per-spawn attribution inferred from timing). Report #1
(`Unable to find any instance for object index`, from `timer_system_update`)
has only game frames and is the game's own background noise.

**The frame mapping, and how it was made (measured).** `build.bat` writes no
map or pdb, so the session's own objects (`plugin_build/obj_dev/*.obj`, the
build of DLL `f85bc1d5...289cf`) were relinked with `link /DLL /MAP` into a
scratch folder. The relinked `.text` section is byte-identical to the session
DLL's (only `.rdata`'s export and debug bytes differ), so the map applies.
The frames are return addresses, so each is looked up at `offset-1`.
Outermost first: `FrameCallback` → `IpcServer::PollCommands` → `RunCommand`
→ `CallBuiltinCmd` (the `cb` command) → two YYToolkit frames (its
`CallBuiltin` dispatch into our builtin hook) → `HookICD` → the
`EnemyBornScope` constructor → `CallerObjectIndex` → the runner's conversion
routine, which raised. To repeat it on another commit, run the same `link`
over a rebuilt `obj_dev`. `ModuleMain.cpp` had not changed in any of these
functions between the session's build (`b230792`) and this fix.

**The cause (static reading of our code).** `HookICD` (and `HookICL`) build
an `EnemyBornScope` first, which asks `CallerObjectIndex` what object the
creating `self` is. That read `object_index` with `variable_instance_get`
and converted the answer with `RValue::ToDouble()`, which in YYToolkit is
the runner's own `REAL_RValue`. On a value with no number in it that routine
raises the runner's error and returns instead of throwing a C++ exception,
so the `catch (...)` around it never saw anything. The `cb` command calls
`instance_create_depth` with a `self` that is not a game instance, so its
`object_index` came back undefined (inferred from the error's text). The
guard reads the caller only while a rarity slider, Tyrant's Crown,
`bossrarity` or Monster Density is on and the created object is a monster or
a spawner, which is why the all-off spawns raised nothing. What the
conversion returned after raising is **not established**; nothing visible
changed (the ancient Karp King was still raised, `enemyBorn=0`).

**The fix.** One named check, `IsNumericInstanceRead`, now stands before the
conversion in `CallerObjectIndex` and `InstanceIdOf` (the `id` read the pack
markers and map reveal's birth observation use), and in the research-only
`TyInstName` (both its `object_index` read, before `object_get_name`, and its
`id` read). It accepts every kind the runtime produces for these reads
(REAL, INT32, INT64 and REF: an asset or instance reference converted before
and still does) and reads anything else as unknown: -1, or `?` in a trace
line. Nothing else in the guard or the create hooks changed.
`tests/test_caller_kind_behavior.py` compiles the production
`CallerObjectIndex` and `InstanceIdOf` against a stub whose conversion
records a runner error for every kind it cannot convert, as the real one
does: the numeric reads come back unchanged (baseline), an undefined
`object_index` or `id` reads as -1 with no error recorded (targets, which
failed against the unfixed source), and the old unchecked shape, compiled in
the same harness, records the error (the negative control).

**Reach into the player build: not established.** The guard is compiled in
both builds, but the player build has no `cb`. Whether any game code path
creates a monster or spawner with a `self` that has no `object_index` was
not read and is not established, so the release notes do not list this as a
player-facing fix.

Other conversions on the `HookICD` / `HookICL` path that are still unchecked,
listed and deliberately left alone: the created object's index
(`Args[3].ToDouble()` in both hooks, in the `EnemyBornScope` constructor, in
`PackMarkerBirth`, in `PopulationBirthScope` and in the Headhunter death
effect's trigger) and the position arguments (`Args[0]`, `Args[1]`) inside
`DoMultiCreate`. Those are the caller's own arguments to the create builtin,
which the builtin itself converts; none was observed to raise. The
research-only `density: late spawner multiplied` line now names an unknown
caller through `object_get_name(-1)` instead of a raised conversion.

**The traced kills (measured, capture `# RERUN`).** With `droptrace 20`
armed, the rank-1 Karp King's and the ancient Karp King's deaths each printed
no `droptrace:` line, added no line to `itemdrops.jsonl` and printed no
`dropmult` gold line. Traced deaths that did drop: the rare Karp King
(`droptrace: DropItem self=Karp_King_obj#306374 argc=12 a0=real:3.000000
...`, a gold line and 20 item lines) and a slider-raised
`Skeleton_Mage_Fire_obj` (`DropItem ... a0=real:4.000000`). Untraced deaths
that dropped: the visual-control Karp King (20 lines and a gold line) and
Damien (a gold line). Every kill took the same route: the boss's
`enemy_hp` key from `oget`, `callnum PC_SetVariableGMLWrapper <key> 0`, then
6 s.

**Static reading of our code.** Every DropManager hook (`FP_DROP_HOOK` in
`plugin/include/ForgePact/DropManager.hpp`) enters the probe scope first
(which notes the trace) and then calls the original with its own `S, O, R,
argc, A`; the trace only reads and returns nothing, and the scope's result
only raises a depth counter. The gold hooks also call their original once
each, and change only the coin amount, only under a `dropmult gold`
multiplier. So an armed trace cannot skip a drop or change one, and a death
that printed no trace line never entered the `DropItem` / `DropItemBoss`
hook bodies at all. `tests/test_droptrace_contract.py` pins both halves:
`test_drop_hook_calls_the_original_whatever_the_trace`, and
`test_trace_name_read_checks_the_kind` (the trace's own name read, the one
residual risk, never observed to fire, which failed before the fix). With no
gold line either, the reading that fits is that those two deaths ran **no
drop routine at all**: not established, and not caused by the trace (the
traced rare and Skeleton kills dropped and printed).

**What decides whether a boss's death runs its drop routine: not
established.** Read locally in the named Ghidra project (output kept local,
per the hub's Legal rule): the drop scripts (`DropItem`, `DropItemBoss`,
`DropBossGems`, `DropBossRunes`) carry no name in this import, because the
symbol dump recorded them pointing outside the game's module (they were
hooked when it ran), so their callers could not be found by name. The named
death-side functions that were read (`CA_setEnemyDeathState`,
`EnemyDestroyKillProc`, `LoadEnemyDestroyFuncs`, `LoadBossDeath` (which the
decompiler timed out on) and the six closures of `Enemy_Parent_obj`'s create
event) reference no variable-name string the decompiler could resolve, and
the callees it could name are skill, cooldown, animation, network and
collision helpers, none of them a drop script. So none of them could be
tied to a killer, a death state or a flag the health-write kill route might
skip. The candidates the session itself cannot
separate stay open: the kill route (health written to 0 through the
protected store, outside combat), the boss's state when it was written (the
visual-control boss that dropped was awake and casting), and timing. A
re-import with a symbol dump taken with no ForgePact hooks installed would
name the drop scripts and let their callers be read.

**What Live procedure 1b does about it.** It pairs every kill with
`dropstats` before and after. In the research build `BP_DIAG_INCREMENT`
counts every DropManager hook entry (`Item c=`, `ItemBoss c=`, `Gold c=`,
`MonsterGold c=`, `BossGems c=`, `BossRunes c=`), trace or not, so a death
that runs no drop routine shows as unchanged counters whether or not
`droptrace` is armed.

## Not verified

Each of these is "not observed", not "does not happen":

- **What the raise does to a boss beyond its health.** Damage, XP, drop rank,
  drops and look of a boss built at rarity 3 or 4: not observed in Live
  procedure 1 (its record above says why for each). The health is the one
  game-built change on record, on one Karp King.
- **That bosses arrive at rarity 1.** The control raises a boss only when the
  game rolled it at exactly 1. A boss the game already made champion, rare or
  ancient keeps its rarity and is counted as `notRank1`; how often a boss
  arrives above 1 on its own is not known.
- **That a write took effect, in the player build.** `raised` counts a write
  of `enemyRarity` that did not throw; the plugin reads nothing back, and the
  affix top-up's result is not checked. In Live procedure 1 the research
  build's `raritytrace` did read both back at the hook's exit on five raised
  bosses; the player build has no such readback.
- **Whether the hook fired at all.** `seen` counts only bosses judged while the
  mode is on, so `seen=0 hook=ok` does not tell "no boss came through" from
  "the hook never ran".
- **A hook that did not install.** `bossrarity rare|ancient` stores the mode
  even when the shared hook came up `failed` or `table-only`; the status line
  says which (`hook=`), and the boss is then not raised on the paths a
  table-only hook cannot see.
- **The affix counts.** A raised boss gets the sliders' own top-up, up to 2
  affixes at Rare and 3 at Ancient, from the same pool. Those are the floors the
  sliders already use (their code notes the game's own rares carry 1-2 and
  ancients 2-4). Live procedure 1 read back 2 at Rare and 3 at Ancient on the
  raised bosses, which is the top-up itself, not a count the game chose.
- **Tyrant's Crown with the Bosses control.** The crown's block runs after the
  boss raise in the same hook and reads the rarity the raise wrote, so while the
  crown is on a boss raised to Rare can also get the crown's one extra affix (a
  reading of our own code; not observed live).
- **Bosses a boss creates** (Odin's second phase, Architect clones, the player
  clone) are "enemy-born" and left at the game's own rarity, like the sliders
  leave them, so a split child is never raised again and again.
- **Uber Luna** is never spawned or killed from outside: doing that closed the
  game on 2026-09-26 (`docs/RUNTIME_DATA_MODELS.md` § 13.1).

## What shipped

- **The control.** Mods › Gameplay, a new third sub-tab after Quality of Life
  and Items holding only the **Bosses** select: `Normal (the game's own)`,
  `Rare — "uber" boss`, `Ancient — "uber uber" boss`. Config key `boss_rarity`
  (`off` / `rare` / `ancient`, default `off`). At startup the panel sends
  `bossrarity <mode>` only when the mode is not `off`; a change while the game
  runs always sends it, `off` included. `/api/set` refuses any other value.
- **The command.** `bossrarity off|rare|ancient|status` is a player command
  (both builds). `rare` and `ancient` install the shared `EnemyRaritySettings`
  hook (`InstallTyrantHook()`, once) and set the mode; `off` (or `0`) only
  clears the mode and leaves the hook in place for the sliders and the crown.
  Every form answers one line:
  `bossrarity: <mode> raised=<n> (rare <r>, ancient <a>) seen=<s> enemyBorn=<e> notRank1=<k> writeFailed=<w> hook=ok|table-only|failed|none`.
  An unknown word answers the usage and leaves the mode unchanged.
- **The raise.** In `Hook_EnemyRaritySettings`, before the sliders' own boss
  check: an instance that `RarInstanceIsBoss` accepts (ancestry, asked at the
  point of use), not created by a monster, at `enemyRarity` exactly 1, gets
  `enemyRarity` 3 or 4 and the affix top-up; then the game's original runs.
  The decision and the counters live in `plugin/include/ForgePact/BossRarityMod.hpp`.
  Ordinary monsters are never touched by this control, and the Monster Rarity
  sliders still leave every boss alone.
- **Tests.** `tests/test_boss_rarity_behavior.py` compiles
  `tests/boss_rarity_harness.cpp` against the header (baseline: mode off leaves
  a boss and a monster alone; target: rank 1 to 3 and to 4, and what the mode
  leaves alone), `tests/test_boss_rarity_contract.py` pins the wiring, and
  `tests/test_boss_rarity_panel.py` the panel half, including that the Gameplay
  card names no damage, drops or XP until the session above records them.
