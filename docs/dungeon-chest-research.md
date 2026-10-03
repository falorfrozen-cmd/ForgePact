# Dungeon chest opens early — research, 2026-10-03

Issue #31, notes 2.2.0. The **Dungeon chest opens early** control (Mods ›
Gameplay; config keys `mod_dungeon_chest`, a switch, default off, and
`dungeon_chest_pct`, a whole number from 50 to 95, default 75; plugin command
`dungeonchest <pct>|off|status` and `dungeonchest countdown
head|chat|both|none`) lets a player open the end chest of a key dungeon once
that share of the dungeon's monsters is dead, instead of all of them. When 50
or fewer kills remain to the threshold, a countdown says how many
(`Chest: 23 kills to go`): as a short line above the player's head, as chat
lines, or both. Only `Dungeon_Chest_obj` is gated; what the chest drops, how
often and at what rarity is the game's own open and is not touched.

"The dungeon's monsters" is our own definition, not a game fact: the kills
ForgePact has counted since it first saw the chest in this room, plus the
monsters (`Enemy_Parent_obj` instances) alive now. The share is re-evaluated
once a second, and the threshold is reached when the kills reach the share of
that sum, rounded up. The arithmetic is written out in the hub's
[`docs/models/dungeon-chest-spec.md`](../../docs/models/dungeon-chest-spec.md)
and modelled in `hs-game-sdk/python/hs_game_sdk/dungeon_chest_model.py`.

This note follows the rule the other ForgePact research docs follow: measured
behaviour, object and script names and indices, our own code and commands.
The game's code was read locally; what it does is told here in our own words,
and no script text, listing, address or byte pattern is reproduced (hub
`AGENTS.md` › "Legal: Decompiled Output Never Reaches Any Origin").

Status, 2026-10-03: **no live session has run yet.** The game's own rule for
the chest is not established, so the player build has no unlock action, and
`dungeonchest <pct>` refuses every share (`unlockRoute=unavailable`) rather
than count, show a countdown and latch over a chest that stays shut. The chat
form of the countdown is refused until a chat call shape is proven. Live
procedure 1 answers both; its results, and the route each one selects, are
filled in below once it has run.

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
  which watches all three.

The chat candidates' static reading is recorded under [Chat route](#chat-route).

## The instrument

### `dungeonchest` (both builds)

The player command. Every form answers one status line that reports what was
*done*: `dungeonchest: <pct>%|off | kills=<k> alive=<a> threshold=<t>
remaining=<r> latched=<0|1> unlocked=<0|1> unlockRoute=<ok|unavailable>
countdown=<form> chat=<ok|unavailable> chatLines=<n>
hook=ok|table-only|failed|none`. `latched` is the decision; `unlocked` is the
unlock action's write, which is what changes the chest. `kills` is our own
count from the `EnemyDestroyKillProc` hook Headhunter already installs (an
enemy-`self` call, once per instance id, with its own recent-id set); `alive`
is `instance_number` of `Enemy_Parent_obj`, polled once a second while the
room holds a `Dungeon_Chest_obj`. The tally resets on a room change and when
the chest count drops to 0. When the threshold latches, `out.txt` gets one
`dungeonchest: unlocked early at <k>/<t>` line per dungeon, or `threshold
reached ... but the unlock action failed` when the write did not happen.

A share is stored only with the kill hook on both routes (`hook=ok`) and an
unlock action supplied; otherwise it is refused with the reason
(`hook=table-only`, `hook=failed`, `unlockRoute=unavailable`) and the mode
stays as it was. `hook=` is what `HookOneScript` answered when it installed
the hook, not an inference from where the saved original points.

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

**Positive controls.** For the census, the kill hook is the control: it is
the route Headhunter proves (307 calls in the § 13.5 measurement), so `kills`
rising while `alive` falls by the same amount shows the census reads the room
it is in. The builtin counters have two controls, both needed before a zero
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
attributed to the chest. If Live 1 sees no poll, no variable flip and no
store value moving, the widening session (Live procedure 1b) adds those that
can be hooked and a dump of the `global` names that mention dungeons,
enemies, kills or counts.

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

Not yet run.

## Live procedure 2

On the player DLL (`plugin_build\build.bat release`, which refreshes
`modfiles_shipped\BloodPactPlugin.dll`; its SHA-256 recorded with the
session), with the panel served from this branch's build.

- character: save slot 14 (Sorak). Two Pumpkin Cellar runs: run A with the mod
  on, run B off (a second run of the same dungeon with another Cellar Key).
  With two Cellar Keys, both runs happen in one launch. With one, run B follows
  stopping the game, restoring the saves backup and relaunching (the restore
  returns the key) — a backend step, not one for the person; the panel's
  switch state survives in `forgepact.json`, so turn it off before the
  relaunch (step 9).
- dungeon: Pumpkin Cellar on the Pumpkin Patch map (the 1.3 map), as Live 1.
- control: `ping` → `pong…`. Marker: `dungeonchest status` → a line starting
  `dungeonchest: off`.
- steps:
  1. Panel: Mods → Gameplay → turn "Dungeon chest opens early" on, then click
     the value beside the slider and type 50, Enter (the number-input path;
     one person action, or the panel driven by `playwright`). Expected in
     `out.txt`: `dungeonchest: 50% …` and the hooks' `HOOK INSTALLED` lines
     (`on-status`).
  2. Person: run A — use a Cellar Key at the Pumpkin Cellar entrance.
     `dungeonchest status` → `alive=N threshold=T countdown=head` with
     T = ceil(N/2).
  3. Person: kill a few monsters. Screenshot: `Chest: <n> kills to go` over the
     character, n equal to `status`'s `remaining=` (`on-countdown-head`); the
     chat shows no countdown line.
  4. `dungeonchest countdown chat`. With `chat-route: proven`: `status` shows
     `countdown=chat`; the person kills until a milestone (50, 40, 30, 20, 10,
     5…1) is passed; screenshot: a `Chest: <m> kills to go` line in chat at
     that milestone, and no label over the character (`on-countdown-chat`).
     With `chat-route: not-observed`: the expected answer is `countdown chat
     refused: chat route not available`, the form stays `head`, and
     `on-countdown-chat` is recorded not-observed.
  5. `dungeonchest countdown both` (only with `chat-route: proven`): at the
     next milestone, one screenshot with both the label and the chat line —
     recorded under `on-countdown-chat` as the both-form note.
  6. Person: kill on to T. Expected: `dungeonchest: unlocked early at T/T` in
     `out.txt`; the label disappears; in a chat form one `Chest: ready to open`
     line. `dungeonchest status` → `kills=` equals N − `alive=` exactly
     (`no-double-count`).
  7. Person: open the chest while `dungeonchest status` still shows `alive=` >
     0. Expected: it opens and drops loot (`on-opens-early`).
  8. `dungeonchest countdown head` (back to the default).
  9. Panel: turn the switch off. Expected `dungeonchest: off …`.
  10. Person: run B (second key, or after the restore above). Try the chest
      with monsters alive — it does not open; kill all; it opens
      (`off-baseline`, the game's own rule).
- cases: run A on at 50 % (head, chat, both forms), run B off. Outlier
  `boss-dungeon`: as Live 1 — from `blockers=` if Live 1 found B > 0, else
  `not-observed`, "not covered live: only Pumpkin Cellar keys".
- checks: `dll-hash`; `marker`; `control`; `on-status` (step 1 line);
  `on-countdown-head` (step 3 screenshot, right number); `on-countdown-chat`
  (step 4 screenshot, right milestone; not-observed when chat is refused);
  `on-opens-early` (step 7); `off-baseline` (step 10); `no-double-count` (step
  6); `boss-dungeon`.
- Restore the saves backup after the session, as always.

### Results

Not yet run. The shipping countdown form (`countdown-form:` head, chat, both,
or a player choice) is the owner's pick after this session, and is recorded
here with its reason.

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

**Token:** not yet set (Live procedure 1 has not run). Until it is, the
player build has no unlock action and refuses every share
(`unlockRoute=unavailable`).

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
- **The static reading of the candidates** — what each reads, of what kind,
  what `self` it expects, what it writes — is done locally and turned into the
  numbered shape table `dungeonprobe chat list` prints. Its paraphrase is
  added here once the research build is written. Not yet recorded.
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

**Token:** not yet set (Live procedure 1 has not run). Until it is, the
player build has no chat call and `dungeonchest countdown chat` / `both` answer
`countdown chat refused: chat route not available`.

## Negative results

None yet. A result that comes back empty is written here as "not observed",
with the control that ran beside it, never as "does not happen".
