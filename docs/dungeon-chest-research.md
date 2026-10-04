# Dungeon chest opens early — research, 2026-10-03

Issue #31, notes 2.2.0. The **Dungeon chest opens early** control (Mods ›
Gameplay; config keys `mod_dungeon_chest`, a switch, default off, and
`dungeon_chest_pct`, a whole number from 50 to 95, default 75; plugin command
`dungeonchest <pct>|off|status` and `dungeonchest countdown
head|chat|both|none`) lets a player open the end chest of a key dungeon once
that share of the dungeon's monsters is dead, instead of all of them. When 50
or fewer kills remain to the threshold, a countdown says how many
(`Chest: 23 kills to go`): as a short line above the player's head, as chat
lines, or both, whichever the player picks in the child select under the
switch (`dungeon_chest_countdown`, head by default). Only `Dungeon_Chest_obj` is gated; what the chest drops, how
often and at what rarity is the game's own open and is not touched.

"The dungeon's monsters" is our own definition, not a game fact: the
dungeon's **planned total** T, every monster its spawners will produce,
spawned yet or not, fixed once when ForgePact first sees the chest in the
room. Progress is the kills ForgePact has counted in this dungeon divided by
T; the threshold is the share of T, rounded up, and once reached it stays
latched. Live procedure 1 showed why it cannot be the kills plus the monsters
alive now (the first definition): the monsters stream in from spawners as the
player moves, so that sum starts near 44 in a dungeon of 600 (see
[Negative results](#negative-results)). Live procedure 1b found no spawner
variable that holds the count, so T is an estimate: the monsters alive at the
chest's first sight plus the spawners still to fire then, times a mean that
session measured (`total-route: estimate`, [Live procedure 1b](#live-procedure-1b)).
A build with no source for T, or whose spawner census failed (the spawner family unresolved, no spawners, or any spawner unreadable), refuses the share (`total=unavailable`). The arithmetic is written out in the hub's
[`docs/models/dungeon-chest-spec.md`](../../docs/models/dungeon-chest-spec.md)
and modelled in `hs-game-sdk/python/hs_game_sdk/dungeon_chest_model.py`.

This note follows the rule the other ForgePact research docs follow: measured
behaviour, object and script names and indices, our own code and commands.
The game's code was read locally; what it does is told here in our own words,
and no script text, listing, address or byte pattern is reproduced (hub
`AGENTS.md` › "Legal: Decompiled Output Never Reaches Any Origin").

Status, 2026-10-04: **Live procedures 1 and 1b have run** ([Results](#results),
[1b's results](#live-procedure-1b)). The chest polls
`instance_exists(Enemy_Parent_obj)` itself about once a frame, and answering
that one call as "none" for the chest while the threshold is latched opens it
with monsters alive (`unlock-route: builtin`, confirmed in Live procedure 1b,
[Route](#route)). The chat call is proven (`chat-route: proven`:
`ChatAddServerMessage`, [Chat route](#chat-route)). The measured dungeon held
122 spawners from the first tick and took 600 kills to clear (619 in Live
procedure 1b), which replaced the denominator above; no spawner variable
holds its count, so the player build estimates the total (`total-route:
estimate`). The owner chose to ship every countdown form as a choice on the
panel (`countdown-form: choice`, 2026-10-04) and reported a flickering head
label, which the build now holds steady. Live procedure 2 (the player build)
follows.

## Static reading

Read on 2026-10-03 in the local Ghidra project
(`%USERPROFILE%\ghidra_projects\HeroSiege`, program `Hero_Siege.exe`). The
reading's raw output stayed on the reader's machine. Each line says how it is
known.

- **The chest's events** (static reading). `Dungeon_Chest_obj` (SDK index
  1366) has five events: Create, Step, Draw, Alarm 0 and Other 7, the
  animation-end event. They were found through the event rows the runtime
  keeps for every object (name, function, variable table — the same table the
  frame profiler walks), which makes object events readable statically even
  though `symbols.csv` names only scripts.
- **What the Step uses** (static reading). The chest's Step calls, by name,
  `GetKeyDungeonRoom`, `GetKeyDungeon`, `NetworkSendClientEffect`, `GPV` (the
  getter for a player variable, read by key from the protected store; see
  `boss-rarity-research.md`), `IsDefined` and `PlaySound3D`. It calls no named
  script that counts or lists enemies.
- **The open** (static reading). The animation-end event is where the chest
  opens: it calls `CreateInFreePos`, an `instance_create` by name, `SPV`,
  `ReturnSpecificStat`, `quest_exists`, `QuestComplete` and
  `CommunityQuestAddProgress`. Alarm 0 calls `ReturnSpecificStat` and `GPV`;
  Create calls `CheckTown`, `UpdateDepth` and `ReportClient`. The mod leaves
  all of this to the game.
- **What the static reading cannot show** (static reading). In this build no
  script body was seen calling a builtin directly (across 150 bodies read,
  `is_handle` was the only one): builtins are reached through the runtime's
  function table. So whether the chest's Step polls `instance_number`,
  `instance_exists` or a relative of them about monsters is **not
  established** either way. Variable names are not visible either: bodies read
  and write instance variables through slot numbers, and neither the slot-name
  dump nor a search for stores into the slot globals named them.
- **The neighbours** (static reading). `Dungeon_Boss_Blocker_obj` (1365) has
  Create, Step and Draw; its Step calls `quest_exists` and `GPV`.
  `Spawn_Dungeon_obj` (4667) has Create, Step, Alarm 0 and Draw; its Alarm 0
  calls `sc_rift`, `StringStartsWith`, `GPV` and `instance_create`. It is the
  world-side entrance spawner of the special-content family, not a monster
  spawner. `Dungeon_Spawner_1_obj` to `Dungeon_Spawner_4_obj` (1375 onwards)
  have only a small Create.
- **The kill path** (measured, hub `docs/RUNTIME_DATA_MODELS.md` § 13.5).
  `EnemyDestroyKillProc` runs with the dying enemy as `self`, and again with
  the player as `self`; `Enemy_Parent_obj` (1429) is the monster family;
  `Enemy_Death_Effect_obj` is not made on every kill, so it cannot count kills.
- **What follows** (reading, not a measurement). What decides the unlock is
  not established. Candidates include a builtin poll the static reading cannot
  see, a chest or blocker variable (built-in or not) that another event
  writes, and the player variable the Step reads through `GPV`. Only a live
  census can tell which, so the unlock code waits for `dungeonprobe` (below),
  which watches all three. Live procedure 1 answered it: the first candidate,
  a builtin poll (`instance_exists` with the chest as `self`).
- **The spawners** (static reading, second pass on 2026-10-03).
  `Enemy_Creator_obj` has Create, Alarm 0, Alarm 1, Alarm 2 and CleanUp
  events, plus the timer callback ForgePact's `Hook_TraceCreatorCheckSpawn`
  already hooks. The callback tests the player's distance and, when it
  spawns, removes its own timer and arms an alarm; it reads nothing that
  names a count. The pack is built in the alarm events (Alarm 1 calls
  `LoadMonsterAffixes`, `NetworkSendEnemyCreate`, `GetWormholeLevel`,
  `LoadSatanicZone`, `ReturnSpecificStat` and `IsObtainablePlace`; Alarm 2
  could not be read to the end). Instance variables are reached through slot
  numbers and builtins (`irandom`, `instance_create_*`) through the function
  table, so **which variable or roll sets the pack size cannot be named from
  the reading**, only where it happens (at the birth, after the distance
  test). The runtime census in Live procedure 1b reads the spawners'
  variables by name instead.

The chat candidates' static reading is recorded under [Chat route](#chat-route).

## The instrument

### `dungeonchest` (both builds)

The player command. Every form answers one status line that reports what was
*done*: `dungeonchest: <pct>%|off | kills=<k> notEnemy=<n> total=<T|unavailable[(<why>)]>
creators=<c> pending=<p> unreadable=<u> alive=<a>
threshold=<t> remaining=<r> latched=<0|1> unlocked=<0|1>
unlock=<ok|failed|none> answered=<n> countdown=<form> chat=<ok|unavailable>
chatLines=<n> hook=ok|table-only|failed|none`. `total=` is the planned total,
`creators=`/`pending=`/`unreadable=` what the total source counted at first
sight (every spawner, those still to fire, those whose state it could not
read), `unlock=` the `instance_exists` detour's install state and `answered=` the
chest's polls the detour answered `false` in this room. The Live procedure 1
build printed `unlockRoute=<ok|unavailable>` in place of the last three; that
field is gone, and the Live procedure 1b build had no `creators=`, `pending=`
or `unreadable=`. `latched` is the decision; `unlocked` is
the unlock action, which is what changes the chest. `kills` is our own count
from the `EnemyDestroyKillProc` hook Headhunter already installs (an
enemy-`self` call, once per instance id, with its own recent-id set), and
`notEnemy` the room's kill-hook calls the adapter refused because their `self`
failed the enemy check (the player-`self` call of a kill, or an enemy check that
cannot answer): `kills=0` beside a rising `notEnemy=` means the hook fired and
the check refused it, both at 0 that the hook did not fire. Live 2's player
build had no `notEnemy=` (attempt 1 under Live procedure 2's results); `alive`
is `instance_number` of `Enemy_Parent_obj`, polled once a second while the
room holds a `Dungeon_Chest_obj`, and is used only for the status line and the
clamp (the threshold is never set so that the mod would open the chest later
than the game would). The total is asked once per room, at the chest's first
sight (and again once a second only while it answered 0). The tally resets on
a room change and when the chest count drops to 0. When the threshold latches,
`out.txt` gets one `dungeonchest: unlocked early at <k>/<T> alive=<n>` line
per dungeon.

**The total source** (`total-route: estimate`, both builds). At the chest's
first sight the adapter counts the spawner family
(`kKnownDensityCreatorObjects`, a child object never counted twice) and reads
each spawner's `enemyArray` by name: one that is not an array has not fired
yet (Live procedure 1b, `creator-state`). T is the monsters alive then plus
those pending spawners times 614 / 117, rounded up (`EstimatedTotal` and its
two named constants in `DungeonChestMod.hpp`; the inputs are run A of Live
procedure 1b, curated as the hub's DC19). A spawner whose state cannot be read
is counted (`unreadable=`) and refuses the estimate, as do an unresolved
spawner family, a spawner object `instance_number` could not count, and a room
with no spawners: counted as fired, an unread or uncounted spawner would
shrink T below the share the player set, and the clamp (kills plus the
monsters alive) does not stop a total of a few percent of the dungeon. The
owner chose "refuse if any unreadable" on 2026-10-04, after round 3 had
refused only when every spawner was unreadable. Each refusal names itself
(`TotalFromCensus`): `total=unavailable(family-unresolved)` (no spawner object
resolved by name, a build or SDK problem), `(count-failed)`, `(no-creators)`
(a fact about the room) and `(unreadable=<u>/<c>)`; `total=unavailable` with
no word means no census was taken (no source, or the mode off). The room's
first refusal also prints one `dungeonchest: no planned total in this room
(<why>): <cause>; the share is not applied here and the game's own rule stays
...` line, once per room however many polls re-ask the source. Before this
change an `instance_number` failure skipped that object's spawners
silently, and a refused census printed nothing outside `status`.

**The head label** (D12). Live procedure 1b's owner report: the label "felt
jerky and was blinking very fast as it was updating every frame". The header
now holds the label's state: its text is rewritten only when the remaining
count changes, its height above the player is taken once when it appears
(from the player's origin, not the bounding box top, which follows the
sprite's animation frame) and held, its position is whole GUI pixels, it sits
in a fixed line under the Headhunter labels' base rather than above however
many of them drew that frame, and a frame whose reads fail draws it at the
last position instead of not at all. Which of those inputs the owner saw is
not established; Live procedure 2's `on-head-steady` is the check.

**The unlock action** (`unlock-route: builtin`, both builds). One `HookBuiltin`
detour on `instance_exists`, installed once on the first non-off mode together
with the kill hook (all-off installs neither). Its first test is the room's
latched flag; while that is false every call goes to the original untouched.
While it is true, a call whose `self` is the room's chest (a pointer compare
against the instance captured at first sight) and whose first argument is
`Enemy_Parent_obj` or a descendant is answered `false`, and `answered` counts
it. The research probe's `instance_exists` counters run inside the same
detour as a second consumer, because a second `HookBuiltin` on one builtin
comes up table-only.

A share is stored only with the kill hook on both routes (`hook=ok`), the
detour installed (`unlock=ok`) and a total source present (a source whose
census then fails leaves the mode stored but answers `total=unavailable`, so
the share never latches and the game's rule stays); otherwise it is
refused with the reason (`hook=table-only`, `hook=failed`, the same for
`unlock=`, or `total=unavailable`) and the mode stays as it was. `hook=` and
`unlock=` are what the installers answered, not an inference from where a
saved original points.

### `dungeonprobe` (research build only)

Compiled only without `FORGEPACT_RELEASE` and not in `kPlayerCommands`, so the
player build has none of it. `dungeonprobe on|off|status|dump`, plus the chat
sub-commands under [Chat route](#chat-route).

- **The census.** While on, and only while a `Dungeon_Chest_obj` exists in the
  room, it prints once a second `dungeonprobe: room=<index> alive=<n>
  creators=<n> blockers=<n> kills=<k> chestVars=<n>`. `alive` counts
  `Enemy_Parent_obj` instances, `creators` live instances of the
  `Enemy_Creator_obj` family (the set `ResolveKnownCreatorObjects` resolves),
  `blockers` live `Dungeon_Boss_Blocker_obj` instances, and `kills` is the
  `dungeonchest` tally.
- **The variables.** On first sight of the chest it lists every instance
  variable of the chest, and of any `Dungeon_Boss_Blocker_obj` and
  `Spawn_Dungeon_obj` instance, through `variable_instance_get_names` (the
  census route `petrelic census` uses), one `dungeonprobe var <object>
  <name>=<value>` line each. `variable_instance_get_names` returns user
  variables only, so the census also reads each instance's built-ins by name
  (`sprite_index`, `image_index`, `image_speed`, `image_alpha`, `visible`,
  `mask_index`, `solid`) and its alarm 0 (`alarm_get` with the instance as
  `self`), printed as `builtin:<name>`: a lock carried in the sprite, its
  frame or speed, visibility, the mask or the alarm still produces a line.
  After that it prints only what changed since the previous second,
  `dungeonprobe diff <object> <name> <old>-><new>`. `dump` prints the full
  list again.
- **The store reads.** The chest's Step and Alarm 0, the blocker's Step and
  the spawner's Alarm 0 call `GPV` (static reading), which reads a value by
  key from the game's protected store, not an instance variable, so the
  variable census cannot see it. `dungeonprobe on` detours `GPV` and `SPV`
  (its write) count-only through `HookOneScript`. For every `GPV` call whose
  `self` is a watched instance, it keeps the key and the value returned, and
  the census prints `dungeonprobe gpv <object> key=<k> =<value> (first read)`
  and then `dungeonprobe gpvdiff <object> key=<k> <old>-><new>` whenever that
  value moves. An `SPV` call that writes one of those keys is recorded with
  its writer's object (`dungeonprobe spv key=<k> writer=<object> ...`,
  `spvdiff`), so a key that flips at the last kill also names what wrote it.
  `status` prints `dungeonprobe store GPV calls=<n> gameCalls=<g> hook=...`
  and every row.
- **The builtin counters.** `dungeonprobe on` installs, once, `HookBuiltin`
  detours (resolved by name, patched at the builtin itself, so they see every
  caller) on `instance_number`, `instance_exists`, `instance_find` and
  `instance_place`. Each counts every call (`calls=`), splits off the
  plugin's own calls (`ownCalls=`: the census, the `dungeonchest` poll and
  the chat-self lookups run inside a scope that marks them) and the game's
  (`gameCalls=`: the rest whose `self` is an instance other than the global
  one the plugin's calls carry), and separately counts the game's calls
  whose `self` is a `Dungeon_Chest_obj`, `Dungeon_Boss_Blocker_obj` or
  `Spawn_Dungeon_obj`, keyed by the object index in the first argument:
  `dungeonprobe builtin instance_number self=Dungeon_Chest_obj
  arg=Enemy_Parent_obj calls=<n>` on `status`. Every counter prints at zero
  too, so a blind detour shows as zero everywhere rather than as silence.
  Since this workorder the probe no longer installs `instance_exists` itself:
  its counters are a research-only consumer inside the shared unlock detour
  (above), `status` reports that detour's state as `hook=installed|failed`,
  and prints the unlock's own row, `instance_exists self=Dungeon_Chest_obj
  arg=Enemy_Parent_obj answered=<n>`.
- **`dungeonprobe creators`, the spawner census.** At the chest's first sight
  the probe walks every live instance of the creator family (the seven
  `kKnownDensityCreatorObjects`, children not counted twice), reads each
  one's variables by name (`variable_instance_get_names` /
  `variable_instance_get`, the chest census's route) and keeps every numeric
  value in memory, keyed by instance and name. It prints `dungeonprobe
  creators: first-sight alive0=<n> births0=<n> spawned0=<n> creators=<n>
  sampled=8 names=<n>` and the
  full dump of the first 8 creators as `dungeonprobe cvar <object>#<k>
  <name>=<value>` lines, with `builtin:alarm[0..2]` and `enemyCreatorTimer`.
  The census runs at the probe's first once-a-second tick that sees the
  chest, up to a second after the room's first birth started the births
  count: `births0` is the births already counted then (those monsters are in
  `alive0` too) and `spawned0` the census creators that had already fired, so
  their first-sight values are after-spawn ones.
  The `dungeonprobe creators` command then prints `alive0`, `creators`,
  `births`, `births0`, `birthsSince` (`births − births0`), `spawned`,
  `spawned0` and `kills`; one `dungeonprobe cand <name> sum0=<sum at first
  sight> now=<sum now> match=<m>/<s> sum0Pending=<sum over the creators with
  no birth before the census> matchPending=<m'>/<s'>` row per numeric name
  present on at least 90 % of the creators (`m` = creators whose first-sight
  value equals the births they made, `s` = creators that made at least one;
  `m'`/`s'` the same over the creators pending at the census only); and the
  `dungeonprobe cdiff <object>#<k> <name> <old>-><new>` rows of the sampled
  creators since first sight, which separate a spawner that has fired from one
  still pending. Rows print at zero.
- **Births.** The create hooks density already installs (`HookICD` /
  `HookICL`, through `InstallCreateHooks`, which `dungeonprobe on` calls if
  they are not in yet) gain a research-only consumer: an enemy created with a
  creator as the caller counts one birth under that creator's instance. No
  second `HookBuiltin` on `instance_create_*`. The per-second line gains
  `births=<n> spawned=<creators with at least one birth>`.
- **`dungeonprobe total <n>|off`.** A research override: the adapter's total
  source answers `<n>` for the next room (`dungeonprobe: total override <n>
  for the next room`), so Live procedure 1b can prove the unlock and the
  countdown against the measured 600 before the build has a source of its
  own; `off` returns to the build's own source (now the estimate, which also
  runs beside the override to fill `creators=`/`pending=`/`unreadable=`).
  Never in the player build.

**Positive controls.** For the census, the kill hook is the control: it is
the route Headhunter proves (307 calls in the § 13.5 measurement), and in Live
procedure 1 it counted one per kill to 600 at `alive=0`. (`kills` rising
while `alive` falls by the same amount, the first form of this control,
cannot hold in a dungeon whose monsters stream in; see
[Negative results](#negative-results).) For the spawner census, two: the
chest census beside it, proven in Live procedure 1 (`chestVars=27`), and
`births=` against the rise of `alive=` at the entrance (5 → 44 with no kill
in Live procedure 1, so `births=` must reach 30 by the second tick). For
`dungeonprobe total`, the kill tally and the `dungeonchest status` line of the
same room. The builtin counters have two controls, both needed before a zero
on the chest's own row is a finding about the chest:

- **The detour sees the game.** `gameCalls=` above zero on a builtin. The
  plain `calls=` cannot serve: the probe itself calls `instance_number` and
  `instance_find` every second, through the very routine the detour patches,
  so `calls=` is above zero whether or not any game call arrives.
  `gameCalls=` is meaningful only with `global=resolved` on the
  `dungeonprobe: on` line and the `status` header: with
  `global=unresolved`, other features' calls carrying the global instance
  as `self` count as game calls too.
- **The per-`self` match sees a chest.** At first sight of the chest the probe
  calls `instance_number` once through `CallBuiltinEx` with the chest instance
  the census resolved as `self`, and an argument the chest has no reason to
  poll (its own object). The detour must file it as `dungeonprobe builtin
  control instance_number self=Dungeon_Chest_obj arg=Dungeon_Chest_obj
  calls=1`, and the census prints `dungeonprobe control: PASS ...`. The
  control row is kept apart from the self rows, so it never reads as a poll.

For the store reads, `GPV`'s own `gameCalls=` is the control: the game reads
its protected store constantly. A zero row is written "not observed", never
"the chest does not poll".

**What it cannot see.** Reads the runtime does without calling one of those
four builtins (`with`-style iteration over an object, the `collision_*` family,
`place_meeting`) are not counted. A `GPV` call the chest makes from inside a
`with` on another instance carries that instance as `self` and is not
attributed to the chest. Live procedure 1 saw the poll, so the widening it
would otherwise have needed (those reads, and a dump of the `global` names
about dungeons, enemies, kills or counts) was not built; Live procedure 1b is
the spawner census instead.

## Live procedure 1

On the research DLL (`plugin_build\build.bat dev`, its SHA-256 recorded with
the session). The owner is asked before it is installed.

- character: save slot 14 (Sorak), holding at least one Cellar Key. The saves
  backup taken before the session and restored after it returns the key the
  session uses.
- dungeon: Pumpkin Cellar, entered with a Cellar Key on the Pumpkin Patch map
  (the 1.3 map). SDK names: entrance `Pumpkin_Cellar_obj` (3727), room
  `Pumpkin_Cellar_01_rm` (216). The runtime room index is expected to equal the
  SDK's; another value is recorded, not a failure. Whether this dungeon holds a
  `Dungeon_Boss_Blocker_obj` is not known beforehand (`blockers=` answers it).
- control: `ping` → a line starting `pong`. Marker: `dungeonprobe status` → a
  line starting `dungeonprobe: off` (research build identified).
- steps:
  1. `dungeonchest status` → `dungeonchest: off | kills=0 alive=… unlockRoute=unavailable
     countdown=head chat=unavailable … hook=none` (the player command answers).
  2. `dungeonprobe on` → a `dungeonprobe: on …` line naming `global=resolved`,
     plus a `HOOK INSTALLED` line for each of the four builtins, `GPV`, `SPV`
     and each chat candidate. `global=unresolved` means `gameCalls=` cannot be
     told apart from the plugin's own global-`self` calls, so
     `builtin-hook-fires` fails.
  3. `dungeonprobe chat control` → the `IsDefined` pair: the defined argument
     answers true, `undefined` answers false (`chat-call-control`).
  4. Person: load slot 14 if not loaded, go to the Pumpkin Patch map, and use a
     Cellar Key at the Pumpkin Cellar entrance (one action). Expected within
     2 s: a `dungeonprobe: room=216 alive=N creators=C blockers=B kills=0
     chestVars=V` line, a block of `dungeonprobe var Dungeon_Chest_obj …`
     lines (`builtin:` ones included), the self-attribution control line
     `dungeonprobe control: PASS …`, and any `dungeonprobe gpv …` first-read
     lines.
  5. `dungeonprobe status` → the header names `global=resolved`; the builtin
     counters: `gameCalls=` > 0 for at least one builtin and the `builtin
     control … self=Dungeon_Chest_obj
     arg=Dungeon_Chest_obj calls=1` row (`builtin-hook-fires`), the
     `self=Dungeon_Chest_obj` rows (zero or not — the finding), and the
     `store GPV` line with `gameCalls=` > 0 and its `gpv` rows.
  6. Person: kill monsters until about half are dead. Expected: `kills=`
     rising, `alive=` falling by the same amount, `diff` or `gpvdiff` lines if
     any chest value moves per kill.
  7. Person: kill the rest. Expected at the last kill: `alive=0` and one or
     more `diff Dungeon_Chest_obj …` (`builtin:` included) or `gpvdiff
     Dungeon_Chest_obj …` lines (the `unlock-signal`), with any `spv`/`spvdiff`
     line naming the writer, or none.
  8. Person: open the chest. Expected: loot; `dungeonprobe` shows the open's
     variable changes (helps tell the "openable" flag from the "opened" one).
  9. `dungeonprobe status` once more (the builtin rows after the unlock).
  10. Person: open the in-game chat and send one short line (one action). Then
      `dungeonprobe status` → the `chathook` rows: calls > 0 on the script(s)
      the game used, with self and argument kinds (`chat-hook-fires`), plus the
      `Ingame_Chat_obj` / `UI_Ingame_Chat_obj` counts. If no chat opens
      offline, the person says so and the check is not-observed.
  11. `dungeonprobe chat list`, then for each shape n in order:
      `dungeonprobe chat try <n>` and a screenshot of the chat area. Expected
      per try: the `supplied …` line; the finding is whether `ForgePact chat
      test <n>` is visible (`chat-shape`). Stop at the first visible one. The
      chat tries run last on purpose: a shape that crashes the game then costs
      nothing already measured; record the last `try` line printed and do not
      relaunch for the rest.
  12. `dungeonprobe off`.
- cases: one ordinary Pumpkin Cellar run (steps 4–9). Outlier `boss-dungeon`:
  only Pumpkin Cellar keys are at hand, so no second dungeon type is invented.
  If B > 0, this run is itself the blocker case: record pass/fail by whether
  the chest unlocked at the last kill as in the ordinary reading; if B = 0,
  record `not-observed` with "not covered live: only Pumpkin Cellar keys".
- checks: `dll-hash`; `marker`; `control`; `chat-call-control` (the
  `IsDefined` pair answered true then false); `kill-hook-fires` (`kills=`
  equals the drop in `alive=` within ±1 over step 6); `builtin-hook-fires`
  (`global=resolved` on the `dungeonprobe: on` line, `gameCalls=` > 0 on at
  least one builtin, never the plain `calls=`, and the `dungeonprobe control:
  PASS` line with its control row `calls=1`);
  `chest-vars-dumped` (V ≥ 1 and at least one `var` line); `alive-count` (N >
  0 at entry); `creators-in-dungeon` (C, recorded; pass if 0, fail if > 0 — a
  finding either way); `unlock-signal` (a chest or blocker variable, built-ins
  included, or a value the chest or blocker read through `GPV` (`gpvdiff`),
  changed at the last kill: pass with its name or key and values, and the
  writer if an `spv` row names one, else not-observed); `builtin-poll` (a `self=Dungeon_Chest_obj` row
  with an enemy-family `arg=` and calls > 0: pass with the builtin's name, else
  not-observed); `boss-dungeon`; `chat-hook-fires` (a candidate's calls > 0
  after step 10: pass with script, self and argument kinds, else
  not-observed); `chat-shape` (a try's text visible: pass with n and what it
  supplied, else not-observed with every try's `supplied` line).
- Restore the saves backup after the session, as always.

### Results

Run on 2026-10-03 by the live operator on the research DLL (SHA-256
`256d3ba41036fc784ac8a64e44698d5622b0b3f79760513a2b9505ffec6f0817`, the same
for the installed file, the `plugin_build` file and the session lease), save
slot 14 (Sorak), one Cellar Key, Pumpkin Cellar. The saves were backed up
before and restored after; the post-restore inspection was clean. This
section is the tracked copy of the session capture, which stays with the
workorder.

The room printed as `ref room Pumpkin_Cellar_01_rm` rather than the SDK index
216 (recorded, not a failure).

| check | result | what was seen |
|---|---|---|
| `dll-hash` | pass | the three hashes above are identical |
| `marker` | pass | `dungeonprobe: off global=unresolved \| chests=0 kills=0 killHook=none …` |
| `control` | pass | `pong (YYTK 4.0.1)` |
| `chat-call-control` | pass | `dungeonprobe chat control: PASS defined->true undefined->false` |
| `kill-hook-fires` | fail as written | `kills=` rose one per kill and reached 600 at `alive=0`, `killHook=ok killNotEnemySelf=0`; the written criterion (kills equal the drop in `alive=`) cannot hold while monsters stream in: at the owner's pause `kills=35` with `alive=` up from 44 to 60. The criterion was wrong, not the hook ([Negative results](#negative-results)) |
| `builtin-hook-fires` | pass | `global=resolved`; `instance_exists gameCalls=7656707`, `instance_find gameCalls=631489`, `instance_place gameCalls=6422` at step 5; `dungeonprobe control: PASS … row instance_number self=Dungeon_Chest_obj arg=Dungeon_Chest_obj calls=1` |
| `store-hook-fires` | pass | `store GPV calls=2 gameCalls=2 hook=native \| SPV calls=0 hook=native`; no `gpv` row for any watched instance |
| `chest-vars-dumped` | pass | `chestVars=27`, 36 `var Dungeon_Chest_obj` lines with the `builtin:` ones |
| `alive-count` | pass | `alive=5` on the first line, `alive=44` a second later, `kills=0` |
| `creators-in-dungeon` | fail (finding) | `creators=122` on every line from the first to the last |
| `unlock-signal` | pass (weak) | the only chest change at the last kill: `nearest` from `-4` to an instance reference, the tick after `alive=0 kills=600`; no writer named; no `gpvdiff`, `spv` or `spvdiff` line in the session |
| `builtin-poll` | pass | `instance_exists self=Dungeon_Chest_obj arg=Enemy_Parent_obj calls=3402` at step 5, 41519 after the unlock |
| `boss-dungeon` | not-observed | `blockers=0` on every line; not covered live: only Cellar Keys |
| `chat-hook-fires` | not-observed | offline chat cannot be typed (the owner: "cant write in the chat. opened and closed chat window few times though"); every chat row stayed at 0 until our own try |
| `chat-shape` | pass (n=1) | shape 1 visible on the first try |

Key lines, in order:

```
dungeonprobe: room=ref room Pumpkin_Cellar_01_rm (Pumpkin_Cellar_01_rm) alive=5 creators=122 blockers=0 kills=0 chestVars=27
dungeonprobe: … alive=44 creators=122 blockers=0 kills=0 chestVars=27
dungeonprobe builtin instance_exists self=Dungeon_Chest_obj arg=Enemy_Parent_obj calls=3402
dungeonprobe: alive=0 creators=122 blockers=0 kills=600 chestVars=27
dungeonprobe diff Dungeon_Chest_obj nearest real:-4.000000->kind=15 str=ref instance 293392
dungeonprobe diff Dungeon_Chest_obj builtin:sprite_index ref sprite Dungeon_Chest_Closed_spr->ref sprite Dungeon_Chest_Open_spr
dungeonprobe chat try 1 supplied script=ChatAddServerMessage self=Player_obj args=string:"ForgePact chat test 1" -> dispatched=1 ret=undefined
```

After the last kill, `status` listed every argument the chest polled through
`instance_exists`: `Enemy_Parent_obj` 41519, `Player_obj` 1897,
`Loot_Ground_obj` 12, `objZoneGenV2` 12, `Controller_obj` 3,
`Menu_Controller_obj` 2, `Client_obj`, `Codex_Controller_obj` and
`Infernal_Codex_Controller_obj` 1 each. The chest's rows for
`instance_find` and `instance_place` stayed at 0, and those zeros are
evidence: both detours saw the game's own calls (`gameCalls=631489` and
`6422`). The chest's `instance_number` row also read 0, but that row was
blind: `instance_number calls=18754 gameCalls=0 ownCalls=574`, so the detour
attributed no call to any game `self` in the whole session, and the control's
`calls=1` was ForgePact's own `CallBuiltinEx` call, not the route compiled
GML takes. Whether the chest polls `instance_number` is not observed. The
open, about 13 s after the last kill, changed only `builtin:sprite_index`
(Closed → Open), `builtin:image_index` and `builtin:image_speed`; no user
variable moved.

`alive=` over the run (one-second lines, run-length collapsed): 5, 44 (the
owner: "game counts unspawned monsters too. 44 spawned monsters are the one
that spawned because they were close to the entrance"), up to 72 at
`kills=23`, 60 at the pause at `kills=35`, a peak of 210 at `kills=260`, then
down to 0 at `kills=600`. `creators=122` did not move.

Screenshots (kept on the owner's machine, in the hs-drive screenshots folder,
not committed): `20261003T194728473420Z_chat-baseline.png` (the empty chat
area) and `20261003T194733632798Z_chat-try1.png` (a red `SERVER: ForgePact
chat test 1` line, bottom left).

What it means:

- **The chest decides by polling.** About once a frame it asks
  `instance_exists(Enemy_Parent_obj)` with itself as `self`. Once no monster
  exists it looks for the nearest player (`nearest`) and opens on approach. No
  unlock flag was written anywhere the probe watched.
- **The monsters are not all there at entry.** Every spawner (122) exists when
  the room loads and stays after it has spawned; the monsters arrive pack by
  pack as the player nears each spawner. The dungeon took 600 kills, about 4.9
  per spawner.
- **The chat line is ours to add** through `ChatAddServerMessage`, with the
  `SERVER:` sender the game puts in front.

**Tokens:** `unlock-route: builtin` (its sufficiency measured in Live
procedure 1b, `unlock-works`) and `chat-route: proven` (shape 1).

## Live procedure 1b

On the research DLL from this workorder's join (`plugin_build\build.bat dev`,
`plugin_build\BloodPactPlugin_rel.dll`); its SHA-256 is recorded when the
session is asked for, and `dll-hash` compares against it. The owner is asked
before it is installed.

- character: save slot 14 (Sorak), with at least two Cellar Keys (the owner
  has 30). Saves backed up before and restored after, as always.
- dungeon: Pumpkin Cellar via a Cellar Key on the Pumpkin Patch map (the 1.3
  map), room `Pumpkin_Cellar_01_rm`, twice in one launch: run A (the census,
  mod off) and run B (the unlock, `dungeonchest 50` with `dungeonprobe total
  600`), a second key.
- control: `ping` → a line starting `pong`. Marker: `dungeonprobe status` → a
  line starting `dungeonprobe: off` (research build identified).
- steps, run A:
  1. `dungeonchest status` → `dungeonchest: off | kills=0 alive=… total=… …
     unlock=none … hook=none` (the player command answers; `total=` and
     `unlock=` are new fields).
  2. `dungeonprobe on` → `dungeonprobe: on global=resolved …` plus `HOOK
     INSTALLED` lines for the builtins, `GPV`, `SPV`, the chat candidates and
     (if not yet installed) `instance_create_depth` / `instance_create_layer`.
  3. Person: load slot 14 if needed, go to the Pumpkin Patch map, use a Cellar
     Key at the Pumpkin Cellar entrance (one action). Expected within 2 s: the
     first `dungeonprobe: room=… alive=N creators=C blockers=B kills=0
     births=… spawned=…` line, the chest `var` block, `dungeonprobe control:
     PASS …`, `dungeonprobe creators: first-sight alive0=N births0=B0
     spawned0=S0 creators=C sampled=8 names=<n>` and the `cvar` block for 8
     creators.
  4. After about 10 s standing still: `dungeonprobe creators` → the `cand`
     rows (zero `match` is fine now) and the `cdiff` rows of the sampled
     creators that fired near the entrance (`creator-state`); the per-second
     line's `births=` ≥ 30 while `kills=0` (`births-hook-fires`).
  5. Person: clear the dungeon (every monster; the game says it is cleared),
     then open the chest. Expected: `kills=` rising one per kill, `births=`
     rising as packs arrive, `creators=` constant; at the end `alive=0`, the
     `nearest` diff, then the open's sprite diffs.
  6. `dungeonprobe creators` → with kills to clear K and `alive0`, the `cand`
     rows: a name with `sum0` = K (or `alive0 + sum0Pending` = K) within
     2 % (`creator-sum`), its `match=m/s`, or `matchPending=m'/s'` when
     `spawned0` > 0 (`creator-match`), `birthsSince=` against K − alive0
     (`kills-equal-births`).
  7. `dungeonprobe status` → the builtin rows (`builtin-poll` again:
     `instance_exists self=Dungeon_Chest_obj arg=Enemy_Parent_obj calls>0`),
     `killHook=ok killNotEnemySelf=0` (`kill-hook-fires`).
  8. `dungeonprobe off` (counters stay). Person: leave the dungeon.
- steps, run B (same launch):
  9. `dungeonprobe on`, `dungeonprobe total 600` → `dungeonprobe: total
     override 600 for the next room`; `dungeonchest 50` → `dungeonchest: 50% |
     … total=… unlock=ok hook=ok` (`on-status-research`; a refusal line here
     ends run B: record it verbatim).
  10. Person: second Cellar Key, enter. Expected within 2 s: `dungeonchest
      status` → `total=600 threshold=300 remaining=300 latched=0`.
  11. Person: kill until `status` shows `remaining=50` (250 kills), then one
      screenshot of the character: `Chest: 50 kills to go` above the head
      (`countdown-head-seen`). `dungeonchest countdown both`; kill on; at the
      next milestone (40) one screenshot showing the chat line `Chest: 40
      kills to go` and the head label (`chat-countdown-seen`).
  12. Person: kill to 300. Expected in `out.txt`: `dungeonchest: unlocked
      early at 300/600 alive=N` (kills over the total) with N > 0, a chat line `Chest: ready to
      open`, the label gone; `dungeonchest status` → `latched=1 unlocked=1
      answered=<n>` with n > 0 (`poll-answered`).
  13. Person: walk to the chest and open it while `status` shows `alive=` > 0
      (`unlock-works`: it opens and gives its loot; the probe shows the sprite
      diffs). If it does not open, record `status` and the probe's
      `self=Dungeon_Chest_obj` rows, then kill the rest and record whether it
      opens at `alive=0`.
  14. `dungeonchest countdown head`, `dungeonchest off`, then, last,
      `dungeonprobe chat try 8` and a screenshot (`chat-sender`; a crash here
      is recorded as `fail (crash - …)`, nothing is relaunched).
  15. `dungeonprobe off`.
- cases: run A ordinary (census), run B ordinary (unlock at 50 % of a known
  total). Outlier `boss-dungeon`: as Live 1 — `blockers=` > 0 would make run A
  the blocker case; otherwise not-observed, "not covered live: only Cellar
  Keys".
- checks: `dll-hash`; `marker`; `control`; `kill-hook-fires` (`killHook=ok`,
  `killNotEnemySelf=0`, `kills=` rose one per kill to K > 0 at `alive=0` in
  run A); `builtin-hook-fires` (`global=resolved` on the `on` line, the
  `dungeonprobe control: PASS` line with its row `calls=1`); `builtin-poll`
  (the chest-`self` `instance_exists arg=Enemy_Parent_obj` row > 0: pass with
  the count, else not-observed); `creators-at-load` (C on the first tick and
  `sampled=8 names=n` with n ≥ 1: pass with C, n and the sampled names, else
  fail); `births-hook-fires` (`births=` ≥ 30 by step 4 with `kills=0`: pass
  with the number, else fail — then `kills-equal-births` and `creator-match`
  are not-observed, never fail); `kills-equal-births` (K vs alive0 +
  `birthsSince` within ±2 at the clear, `birthsSince` = `births` − `births0`
  from step 6's header, so a monster born before the census is not counted
  twice; K vs `births` alone also passes when every monster came from a
  creator); `creator-sum` (pass with the name, the reading that matched — `sum0`
  over every creator, or alive0 + `sum0Pending` over the creators pending at
  the census — and both sums, else not-observed with the three nearest
  candidates); `creator-match` (pass with `m/s` ≥ 90 %, or with
  `matchPending=m'/s'` ≥ 90 % when `spawned0` > 0, since a creator that fired
  before the census has after-spawn first-sight values; else not-observed
  with the best row); `creator-state` (pass with the variable(s) that differed
  between a fired sampled creator and a pending one, else not-observed);
  `boss-dungeon`; `on-status-research` (step 9 line, pass/fail with the line);
  `countdown-head-seen` (step 11 screenshot); `chat-countdown-seen` (step 11
  second screenshot); `poll-answered` (`answered=` > 0 after the latch);
  `unlock-works` (step 13: pass when the chest opened with `alive=` > 0, fail
  with the recorded lines, not-observed if run B never latched); `chat-sender`
  (step 14: pass if a line without the `SERVER:` prefix appeared, fail (crash)
  or not-observed otherwise).
- Restore the saves backup after the session, as always.

Reading it into tokens:

- `total-route: variable` when `creator-sum` passes **and** either
  `creator-match` passes or `births-hook-fires` failed (then the sum alone
  decides). The player build then reads that name, by the reading that
  matched: the sum over every creator, or `alive0` plus the sum over the
  pending ones.
- `total-route: estimate` when `creator-sum` is not-observed but
  `creator-state` passes: the creators pending at first sight are countable,
  and the mean per pending creator measured here becomes a curated constant
  (T = alive0 + pending × mean).
- `total-route: not-knowable` otherwise; the owner then chooses how the total
  is known.
- `unlock-route: builtin` is confirmed when `unlock-works` passes;
  `unlock-route: builtin-insufficient` when it fails with `latched=1
  unlocked=1 answered>0` (the detour answered and the chest still did not
  open: the next candidates are the `GPV` key the chest's Step reads, its
  Alarm 0 path, and an `instance_number` poll the probe cannot attribute,
  since Live 1's `instance_number` row was blind with `gameCalls=0`);
  `unlock-route: instrument` when `answered=0` (the detour did
  not reach the chest's call: check the `unlock=` state and the `self`
  pointer match first).
- `countdown-head-seen` and `chat-countdown-seen` are observations for the
  owner's choice of countdown form, not tokens.

### Results

Run 2026-10-03 on the research DLL (SHA-256 `316a68ed…1bd6d`, the hash taken
at dispatch, matched by the lease), save slot 14 (Sorak), two Pumpkin Cellar
runs (`Pumpkin_Cellar_01_rm`) in one launch; the saves backup was restored
after it. The capture stays local
(`.claude/workorders/forgepact-issue-31-dungeon-chest-b-live-1b.md`); this is its
tracked copy. Screenshots are named, not pasted.

| check | result | observed |
|---|---|---|
| `dll-hash` | pass | the installed DLL's hash matched the dispatch's |
| `marker` | pass | `dungeonprobe: off global=unresolved …` |
| `control` | pass | `pong (YYTK 4.0.1)` |
| `kill-hook-fires` | pass | `killHook=ok killNotEnemySelf=0`; run A `kills=` rose 0 → 619 to `alive=0` |
| `builtin-hook-fires` | pass | `dungeonprobe: on global=resolved`; `dungeonprobe control: PASS … calls=1` |
| `builtin-poll` | pass | `instance_exists self=Dungeon_Chest_obj arg=Enemy_Parent_obj calls=17697` (run A), 56440 by the end of run B |
| `creators-at-load` | pass | first tick `creators=122`; `first-sight alive0=5 births0=5 spawned0=5 creators=122 sampled=8 names=20` |
| `births-hook-fires` | pass | `births=42` with `kills=0` about 10 s after entry (`alive=42 spawned=11`) |
| `kills-equal-births` | fail | K = 619, `alive0` = 5, `birthsSince` = 639 (`alive0` + `birthsSince` = 644), `births` = 644: both 25 above K |
| `creator-sum` | not-observed | no candidate near 619; nearest `img` (`sum0` 112.67, `sum0Pending` 91), `isWormhole` (19806532), `spawnPack` (19970468) |
| `creator-match` | not-observed | best row `img match=0/122 matchPending=0/117`; every candidate 0 |
| `creator-state` | pass | sampled spawners at first sight `alarm[0]=5`, `enemyCreatorTimer` undefined, `enemyArray` undefined; armed (step 4) `alarm[0]=-1`, `enemyCreatorTimer` 214..221; fired (step 6) `enemyArray` an array |
| `boss-dungeon` | not-observed | `blockers=0` throughout both runs; not covered live, only Cellar Keys |
| `on-status-research` | pass | `dungeonchest: 50% \| … unlock=ok answered=0 countdown=head chat=ok chatLines=0 hook=ok` |
| `countdown-head-seen` | pass | `head-label-9.png`: `Chest: 9 kills to go` above the character (the 50 mark was passed before the screenshot); the owner reported the label flickering (below) |
| `chat-countdown-seen` | pass | the owner saw `Chest: 5 kills to go` in chat (`chatLines=2`); `chat-5.png` shows the head label, the chat line had faded |
| `poll-answered` | pass | `latched=1 unlocked=1 answered=390` |
| `unlock-works` | pass | `dungeonchest: unlocked early at 304/600 alive=214`; the sprite went from closed to open at `kills=325 alive=193`; the drop was not checked by the operator |
| `chat-sender` | pass | `chat-try-8.png`: `[00:49] ForgePact : ForgePact chat test 8`, no `SERVER:` prefix, no crash |

Key lines. Run A at first sight: `dungeonprobe creators: first-sight alive0=5
births0=5 spawned0=5 creators=122 sampled=8 names=20`. At the clear:
`dungeonprobe creators: alive0=5 creators=122 births=644 births0=5
birthsSince=639 spawned=122 spawned0=5 kills=619`. The variables on the
spawners whose names suggest a pack size read as one large real per spawner,
rising from one spawner to the next (about 163047 upward), not as counts. Run
B: `dungeonchest status` in the room read `total=600 alive=41 threshold=300
remaining=300 latched=0`, and after the open `kills=325 total=600 alive=193
threshold=300 remaining=0 latched=1 unlocked=1 unlock=ok answered=390
countdown=both chat=ok chatLines=3`; the chat hook saw
`ChatAddServerMessage … "Chest: ready to open"`. One deviation from the
procedure: step 9's commands were first issued while the owner was still in
run A's room, so the override bound to that room; they were issued again
after the owner left, before the second key.

The owner, on the head label in run B: "the text above character felt jerky
and was blinking very fast as it was updating every frame". That is our draw,
not the game; see the head label under [The instrument](#the-instrument).

**Tokens.** `unlock-route: builtin`, confirmed: `unlock-works` passed with
`answered=390`, so answering the chest's own poll is enough to open it.
`total-route: estimate`: `creator-sum` and `creator-match` were not observed
and `creator-state` passed, so the planned total is the monsters alive at
first sight plus the spawners still to fire (`enemyArray` not an array) times
a mean from run A: (619 − 5) ÷ (122 − 5) = 614 ÷ 117 ≈ 5.25 per pending
spawner, over kills, never births (`kills-equal-births`: 25 births were never
killed). If the 5 monsters alive at first sight were idle ones rather than
the 5 fired spawners' packs, all 122 were pending and the same mean gives 646
for that run instead of 619; an over-count only raises the threshold. Live 1's
(600 − 44) ÷ 122 ≈ 4.6 is a second point, from a census taken later, and not
the constant.

**Countdown form** (`countdown-form: choice`). The owner, 2026-10-04, after
this session, on which countdown form ships: "we can ship both with an option
to choose, like we can chose skill counter style". So the panel has a select
under the switch, "Where the countdown shows" (`dungeon_chest_countdown`:
head, chat or both, head by default), sending `dungeonchest countdown <form>`
only while the switch is on. `none` stays a plugin word only.

## Live procedure 2

On the player DLL (`plugin_build\build.bat release`, which refreshes
`modfiles_shipped\BloodPactPlugin.dll`; its SHA-256 recorded with the
session), with the panel served from this branch's build.

- character: save slot 14 (Sorak). Two Pumpkin Cellar runs: run A with the
  mod on at 50 %, run B off (a second Cellar Key; or, with one, stopping the
  game, restoring the saves backup and relaunching — a backend step, with the
  panel's switch turned off before it, since its state survives in
  `forgepact.json`).
- dungeon: Pumpkin Cellar on the Pumpkin Patch map (the 1.3 map), as Live 1.
- control: `ping` → `pong…`. Marker: `dungeonchest status` → a line starting
  `dungeonchest: off`.
- steps:
  1. Panel: Mods → Gameplay → switch "Dungeon chest opens early" on, click the
     value beside the slider and type 50, Enter. Expected in `out.txt`:
     `dungeonchest: 50% …` and the `HOOK INSTALLED` lines (`on-status`).
  2. Person: run A — Cellar Key at the Pumpkin Cellar entrance. `dungeonchest
     status` within 2 s → `total=T threshold=ceil(T/2) remaining=…
     latched=0` with T > 0 from the build's planned source, the estimate
     (`on-total`; T, `creators=`, `pending=` and `unreadable=` are recorded
     against Live procedure 1b's kills to clear).
  3. Person: kill until `status` shows `remaining=50`; screenshot: `Chest: 50
     kills to go` over the character, no chat line (`on-countdown-head`).
     Person: stand still about 10 s without killing, then kill one: the label
     neither blinks nor jumps and its number changes only at the kill
     (`on-head-steady`; the owner's words recorded).
  4. Panel: Mods → Gameplay, the countdown form select ("Where the countdown
     shows") → In chat. Expected in `cmd.txt`/`out.txt`: `dungeonchest
     countdown chat` and `countdown=chat` (`on-form-control`); kill to the
     next milestone; screenshot: the chat line, no label (`on-countdown-chat`).
  5. Panel: the form → Both; at the next milestone one screenshot with both
     (the both-form note under `on-countdown-chat`).
  6. Person: kill to the threshold. Expected: `dungeonchest: unlocked early at
     T'/T' alive=N` (N > 0), the label gone, `Chest: ready to open` in chat;
     `status` → `kills=` equals the threshold, `answered=` > 0
     (`no-double-count`: `kills=` equals the threshold exactly at the latch
     line, and never exceeds the births plus `alive0` the research run saw).
  7. Person: open the chest while `status` shows `alive=` > 0
     (`on-opens-early`).
  8. Panel: the form → Above your character (`dungeonchest countdown head`;
     the select is greyed while the switch is off).
  9. Panel: switch off → `dungeonchest: off …`.
  10. Person: run B (off): the chest does not open with monsters alive; kill
      all; it opens (`off-baseline`).
- cases: run A on at 50 % (head, chat, both), run B off; outlier
  `boss-dungeon` as Live procedure 1b.
- checks: `dll-hash`; `marker`; `control`; `on-status`; `on-total`;
  `on-countdown-head`; `on-head-steady`; `on-form-control`;
  `on-countdown-chat` (not-observed only if the chat form is refused);
  `on-opens-early`; `off-baseline`; `no-double-count`; `boss-dungeon`.
- Restore the saves backup after the session, as always.

### Results

Not yet run. The countdown form is already the player's choice
(`countdown-form: choice`, the owner, 2026-10-04, recorded under [Live
procedure 1b](#live-procedure-1b)); this session checks the control and each
form.

## Route

How the mod makes the chest openable is chosen by what Live procedure 1
shows, and is written only after it. Each route changes one value inside a
call or a state the game already uses; none suspends the game or calls an
address.

- `builtin`: `builtin-poll` shows the chest's `self` calling
  `instance_number`, `instance_exists` or a relative with an enemy object
  index. The mod answers that one call as "none alive" while the threshold is
  latched, for a `Dungeon_Chest_obj` `self` only. Preferred: one value in a
  call the game makes.
- `variable`: `unlock-signal` shows a chest or blocker variable reaching a
  terminal value (0, true, a state number) at the last kill, with no poll seen.
  The mod writes that value once at the latch through `variable_instance_set`
  and lets the game's own Step do the rest.
- `writer`: the variable flips, but a diff shows another object
  (`Spawn_Dungeon_obj`, a room variable) changing first. The mod detours that
  writer by name and performs the same write at the latch.
- `store`: the flip is a `gpvdiff` key only (a value read from the game's
  protected store through `GPV`). No route is written until the plan is
  amended with the write (the matching `SPV` writer, if an `spv` row named
  one); the unlock step stops on this token.

Reading Live 1 into the token: `builtin-poll` pass → `unlock-route: builtin`.
`builtin-poll` not-observed and `unlock-signal` pass with the flipped instance
variable (a `diff` row, `builtin:` included) on the chest or blocker →
`unlock-route: variable`. `unlock-signal` pass with the first flip on another
object → `unlock-route: writer`. An `unlock-signal` pass carried only by a
`gpvdiff` row (a `GPV` store key, not an instance variable) →
`unlock-route: store`; it never maps to `variable`, because
`variable_instance_set` would write an instance variable that does not exist,
and it goes back to the plan before any unlock code is written. Both
not-observed → `unlock-route: not-observed`, and a widening session (Live
procedure 1b, same research DLL) follows before any unlock code is written.

**Token: `unlock-route: builtin`** (Live procedure 1, 2026-10-03).
`builtin-poll` passed: `instance_exists self=Dungeon_Chest_obj
arg=Enemy_Parent_obj` counted 3402 calls with 44 monsters alive and 41519 by
the end of the run, about one a frame. `unlock-signal` passed only weakly:
the one chest variable that moved at the last kill was `nearest` (from `-4`
to an instance reference, the tick after `alive=0`), which reads as the chest
finding the nearest player once the poll answered "none", not as an unlock
flag; no store key moved and no `SPV` ran. So the unlock action is the
`instance_exists` detour described under [The instrument](#the-instrument):
while the threshold is latched, the chest's own call about `Enemy_Parent_obj`
is answered `false`, every other call untouched, and the game's Step, its
`nearest` lookup and its open run as they do at `alive=0`.

**Confirmed in Live procedure 1b** (2026-10-03, `unlock-works` pass): at 50 %
of an override total of 600 the threshold latched at `unlocked early at
304/600 alive=214`, the detour answered the chest's poll `false` 390 times
(`answered=390`), and the chest's sprite went from closed to open at
`kills=325 alive=193`. Answering that one call is enough; the `GPV` key and
the Alarm 0 path below are not needed.

Before that session, sufficiency was indicated, not proven: the chest's Step also reads `GPV` and
has an Alarm 0. It was measured in Live procedure 1b (`unlock-works`: the chest
must open with monsters alive). Had it failed with `answered>0`, the token
becomes `unlock-route: builtin-insufficient` and the next candidates are, in
that order, the `GPV` key, the Alarm 0 path, and an `instance_number` poll by
the chest. That last one is not ruled out: Live 1's `instance_number` detour
attributed no call to any game `self` (`gameCalls=0`), so its zero chest row
measured nothing, and an `instance_number(Enemy_Parent_obj)` poll the probe
cannot attribute stays a candidate until an instrument that sees the game's
`instance_number` calls says otherwise (`status` now marks such a row
`(blind: gameCalls=0 …)`). With `answered=0` it is `unlock-route:
instrument`, a defect in our detour rather than a finding about the game.

## Chat route

No ForgePact code had put a line in the game's chat before this issue. The
countdown's chat form needs one call that does, made by name.

- **Candidates**, all in `hs-game-sdk` (`scripts.hpp`): `CA_chatIngame`,
  `ChatAddMessage` (with its method `ChatAddMessageFunc`),
  `ChatAddServerMessage`, `ChatIngameAdd` and
  `IngameChatFeedAddLatest`. The chat objects are `Ingame_Chat_obj` (2258) and
  `UI_Ingame_Chat_obj` (5107). Nothing in ForgePact's docs, the hub's
  `RUNTIME_DATA_MODELS.md` or `hs-game-sdk/curated` described their arguments
  (searched 2026-10-03). The research build also hooks
  `ChatAddMessageFiltered` and `ChatAddIngameMessageFiltered`, found by the
  same SDK substring search, and counts `Chat_obj` beside the two chat
  objects; `ChatAddMessageFunc` is a method, not a script `HookOneScript` can
  reach by name, so it is not hooked.
- **The shapes.** The local reading of the candidates was turned into the
  numbered table `dungeonprobe chat list` prints, as Live procedure 1 recorded
  it: 1 `ChatAddServerMessage`, `self` the player, the text; 2 the same with a
  trailing 0; 3 `ChatAddServerMessage` with `Ingame_Chat_obj` as `self`; 4
  `ChatIngameAdd`, player, text, two empty strings, white, 0; 5 the same with
  the text second; 6 `ChatAddMessage`, player, text; 7
  `ChatAddMessageFiltered`, player, text. Shape 8, added after Live 1 and run
  only in Live procedure 1b, calls `ChatAddMessage` directly with the sender
  `"ForgePact"` and the argument kinds the game itself passed in Live 1 (two
  strings, two reals, two int64s, two reals, a `[hh:mm]` string, six
  `undefined`), to learn whether the line can carry our name instead of
  `SERVER:` (`chat-sender`).
- **The game's own calls.** `dungeonprobe on` also installs count-only
  `HookOneScript` detours (both routes) on every candidate. `dungeonprobe
  status` prints, at zero too, `dungeonprobe chathook <script> calls=<n>
  self=<object> args=<kinds>` (a string argument shows its first 40
  characters), and the live counts of `Ingame_Chat_obj` and
  `UI_Ingame_Chat_obj` — whether an offline game holds a chat feed at all. The
  shape the game itself uses when the owner types a line is the best
  candidate.
- **The positive control for the call route.** `dungeonprobe chat control`
  calls `IsDefined` by name through the route the tries use (the
  `ApCallScript` route: `asset_get_index` of the short name, then
  `script_execute` through `CallBuiltinEx` with a chosen `self`), once with a
  defined value and once with `undefined`. True then false proves the route
  runs a script and returns its value. A try that merely dispatched proves
  nothing on its own: a script can run and add nothing.
- **The tries.** `dungeonprobe chat try <n>` runs shape n with the text
  `ForgePact chat test <n>` and prints `dungeonprobe chat try <n> supplied
  script=<name> self=<what> args=<kinds> -> dispatched=<0|1> ret=<kind>`; a
  refused resolution or a throw prints which field failed. One shape per
  command, so a shape that crashes the game is the last one printed. Success is
  the test text visible in the chat feed on a screenshot, nothing less.

A rejected shape is recorded with what was supplied, as "rejected with these
arguments", never as "this script cannot be called". `chat-shape` pass → the
token `chat-route: proven`, naming the shape; otherwise `chat-route:
not-observed`, the countdown ships above the head only, and the shapes tried
are listed here.

**Token: `chat-route: proven`** (Live procedure 1, 2026-10-03), shape 1.
`dungeonprobe chat try 1` supplied `script=ChatAddServerMessage
self=Player_obj args=string:"ForgePact chat test 1"`, answered `dispatched=1
ret=undefined`, and the screenshot showed a red `SERVER: ForgePact chat test
1` line at the bottom left. The call control (`IsDefined`, true then false)
passed in the same session. The hooks then showed what the game did inside
it: one `ChatAddMessage` call with 15 arguments (the sender string `"SERVER"`,
our text, six numbers, a `[hh:mm]` time string, six `undefined`) and one
`IngameChatFeedAddLatest` call (`self` the player, a reference and a bool).
Shapes 2–7 were not tried: the procedure stops at the first visible one.
`chat-hook-fires` is not-observed, because offline chat cannot be typed, so
no game-sent line was seen.

The player build's chat form therefore calls `ChatAddServerMessage` by its SDK
name through the `ApCallScript` route, with the local player as `self` and
one string; a refused resolution or a failed call turns the chat form off
with one log line naming the field (`chat=unavailable`), never retried per
kill. A countdown line is therefore expected to read `SERVER: Chest: <n>
kills to go`, in red (seen in Live procedure 1b, `chat-countdown-seen`): the
prefix is the game's. Live procedure 1b's shape 8 (`ChatAddMessage` called
directly with the sender `"ForgePact"`) showed `[00:49] ForgePact : ForgePact
chat test 8` with no prefix and no crash (`chat-sender` pass); it stays a
research shape. The owner then chose to offer every form on the panel
(`countdown-form: choice`, 2026-10-04) without asking for a different sender,
so only shape 1 ships.

## Negative results

A result that comes back empty is written here as "not observed", with the
control that ran beside it, never as "does not happen".

- **The first denominator, kills ÷ (kills + monsters alive now), is
  contradicted** (Live procedure 1). The dungeon's monsters are not all alive
  at entry: 5 on the first tick, 44 a second later, a peak of 210 at 260
  kills, 600 kills to clear, while 122 spawners stood in the room the whole
  time. At entry that sum would have called 50 % reached after about 22 kills
  of 600. Replaced by the planned total (see the top of this note).
- **`kill-hook-fires` as first written cannot be tested** in a dungeon whose
  monsters stream in: kills cannot equal the drop in `alive=` while packs
  arrive (at 35 kills `alive=` had risen from 44 to 60). The hook itself
  counted one per kill to 600 at `alive=0` with `killNotEnemySelf=0`. Live
  procedure 1b's form compares the kills at the clear instead, and checks
  them against the births it counts.
- **`chat-hook-fires` not observed.** Offline chat cannot be typed (the owner
  opened and closed the chat window; every chat row stayed at 0), so the
  game's own call for a typed line was not seen. The call control and shape 1
  ran beside it and passed.
- **`boss-dungeon` not observed** (not covered live: only Cellar Keys were at
  hand; `blockers=0` throughout Live procedures 1 and 1b).
- **No spawner variable read by name was observed to hold its pack count**
  (Live procedure 1b, `creator-sum` and `creator-match` not observed). Every
  numeric variable on the 122 spawners at first sight was summed against the
  619 kills to clear and compared with each spawner's births; none came near,
  and every match row read 0. The control beside it held: the births counter
  saw 42 births by the tenth second with no kill, and the census read 20
  names on the sampled spawners. That is a negative about the variables the
  census saw (numeric ones present on at least 90 % of the spawners at first
  sight), not about every state the game keeps; the total is estimated
  instead.
- **Kills and births differ** (Live procedure 1b, `kills-equal-births` fail):
  619 kills to clear, 644 births by spawners. 25 monsters were born and never
  killed by the player, so the estimate's mean is taken over kills.
- **No unlock flag observed** at the last kill: no store key read through
  `GPV` moved, no `SPV` ran, and of the chest's 27 user variables and its
  built-ins only `nearest` changed. That is a negative about what the probe
  watched (user variables, seven built-ins, alarm 0, the store keys the
  chest read), not about every state the game keeps.
