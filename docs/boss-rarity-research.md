# Bosses at a forced rarity ("uber" bosses) — research, 2026-10-02

Issue #44, notes 2.2.0. The **Bosses** control (Mods › Gameplay, config key
`boss_rarity`, plugin command `bossrarity off|rare|ancient|status`, off by
default) makes every boss the game spawns while it is on roll as Rare (3,
"uber" boss) or Ancient (4, "uber uber" boss). It does that the way the
Monster Rarity sliders and Tyrant's Crown already raise ordinary monsters: by
writing `enemyRarity` at the entry of the game's own `EnemyRaritySettings`,
before the game builds the monster from it.

Status: **written, not yet run in a live game.** What a forced rarity does to a
boss beyond its `enemyRarity` (its health, damage, XP, drop rank and drops,
affixes and look) is what Live procedure 1 below is for. Until it has run,
nothing in this file, the panel, the README or the release notes says the
raise changes any of those.

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
  bosses since 1.4.3. That a boss passes through this hook has not yet been
  traced by us; Live procedure 1's first raised boss is that trace.
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

The rerun uses the detached-console research build, a low-level zone
confirmed by screenshot before any spawn, and a liveness check after every
spawn and kill, with the runner's own error counter read before and after
each spawn. The procedure is in the workorder; the rerun's capture, each
check's verdict and the route it sets are recorded here when it has run.

## Not verified

Each of these is "not observed", not "does not happen":

- **What the raise does to a boss.** Health, damage, XP, drop rank, drops,
  affixes and look of a boss built at rarity 3 or 4: none observed yet.
- **That bosses arrive at rarity 1.** The control raises a boss only when the
  game rolled it at exactly 1. A boss the game already made champion, rare or
  ancient keeps its rarity and is counted as `notRank1`; how often a boss
  arrives above 1 on its own is not known.
- **That a write took effect.** `raised` counts a write of `enemyRarity` that
  did not throw; nothing reads the value back, and the affix top-up's result is
  not checked.
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
  ancients 2-4), not counts measured on bosses.
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
