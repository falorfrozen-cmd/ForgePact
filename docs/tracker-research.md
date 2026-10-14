# HS Offline Tracker in ForgePact: the sensor's routes (ForgePact #185)

ForgePact #185 moves HS Offline Tracker into ForgePact. The Tracker's data came
from its own producer DLL (`HS-Offline-Tracker/aurie-producer/`), loaded beside
ForgePact's plugin, which counted gold, experience and kills and recorded rare
ground drops into a named pipe the Tracker app read. ForgePact's `tracker` mod
does the same job from inside ForgePact's own plugin and writes protocol-v1
records (`HS-Offline-Tracker/PROTOCOL.md`) to one session file per game
process, `<game>\bin\bp_ipc\tracker\session-<pid>-<unix ms>.ndjson`, which the
panel reads.

No route here is new research. Each one was already measured somewhere: by the
Tracker's producer (`HS-Offline-Tracker/aurie-producer/README.md`, § "Current
counter contract", at tag `v0.1.4`), or by ForgePact itself for Loot
announcements. This note records which route the sensor uses, where its
evidence comes from, and what was turned down. The module guide's section "HS
Offline Tracker in ForgePact (`tracker`, #185)" describes the result; hub
`docs/RUNTIME_DATA_MODELS.md` § 13.11 carries the game facts.

Labels: **measured** (a live run recorded it; who ran it is named),
**static reading** (read from the game locally, not run), **not established**
(neither yet).

## Routes

- **Gold: `GoldLogAdd`, by SDK name, `HookOneScript` with both routes (hook id
  `fp_tracker_gold`).** The game calls it to write a gold pickup's UI log; the
  balance itself changes in `PickUpGoldCheck` (RDM § 13.10, static reading).
  Whether every gold credit (vendor sales, quest rewards, a mod's
  `PickUpGoldCheck` credit) passes through it is **not established**, so the
  gold total is what this script saw, not known to be all income. Two
  arguments, the earned amount at index 1 on the 2026-08-29 build: **the
  Tracker's reviewed code profile (static reading)** (its README, "Current
  counter contract", which cites no live run for gold).
  The decompiler copy of the current build reads as the same two-argument
  UI-log writer (**static reading**, 2026-10-13). The sensor takes the
  producer's adaptive rule rather than its exact one: 1 to 4 arguments, the
  last one that decodes as a positive whole number, so an amount that moves
  between index 0 and 1 is still counted.
- **Experience: `ExperienceUpdate`, through its one shared detour
  (`fp_mining_rolls_xp`, `MiningOre::InstallExperience`).** Two arguments,
  the amount at index 0, and a call that answers an explicit boolean `false`
  is a refusal, not a gain: **the Tracker's reviewed code profile (static
  reading)**. Whether every experience credit passes through it is **not
  established**. Extra rolls
  (`miningrolls`) already holds a native pass-through detour on this script,
  and a second `HookOneScript` on the same script installs table-only, which
  a direct `call` never passes through (AGENTS.md, "Prove the Instrument").
  So the detour has two consumers, and the tracker's capture and count are
  hooks on it. Mining's completion run calls it once per run (RDM § 12,
  **measured** 2026-09-28 by ForgePact).
- **Kills: `EnemyAddStatistics`, by SDK name, both routes (`fp_tracker_kills`).**
  One argument, the kill's `killStatistic` id (non-negative, 0 included), which
  equals the monster's rank (RDM § 13.7, measured). One call per kill: in a
  packaged live run the producer's calls rose by 56 while the character's saved
  `statisticTotalMonsterKills` rose by 56 over the same save interval
  (**measured by the Tracker**).
- **Each counter detour reads its arguments before the game's call and counts
  after it returns**, as the producer does. Every call while the switch is on
  counts in `*-calls=`, the positive control that the detour runs at all; only
  accepted calls add to `*-sum=`.
- **Drops: `LootGroundInit`'s shared detour `fp_hiddenloot_init` and the shared
  `Hook_CreateItemNew` on `CreateItemNew`.** The tracker is the fourth consumer
  of the one `LootGroundInit` detour hidden loot sleep installs
  (`HiddenLootInstall()`, by whichever mod turns on first), and it notes
  `CreateItemNew` returns through the shared hook, installing it by name with
  both routes (`fp_tracker_new`) when nobody holds it yet. A ground item is
  recorded only when it was built through `CreateItemNew` in its frame or the
  one before: the creation guard Loot announcements **measured** on this build
  (a bag drop held, natural drops passing; module guide § "Loot announcements",
  RDM § 16.10 and § 16.11). The tracker keeps its own copy of the guard, so it
  works with `lootann` off. The rarity is `itemInfoStruct["27"]`, and only the
  producer's set {4 Set, 6 Satanic, 7 Angelic, 9 Heroic, 10 Unholy} is
  recorded, with the producer's rarity names; the item's `itemType`,
  `itemDefinitionStruct.b` and `.j` and info `"28"` (the name) are read only
  for those, as the producer's ground-item capture read them.
- **The room: polled, not hooked; the zone and the name read until they
  answer** (replan 2, after Live procedure 1 below). Each tick compares
  `room_get_name` of the `room` builtin with the last name. On a change (and
  at install) the sensor writes `room` and marks a `satanic_zone` and the
  character's name as owed. It reads the zone on that tick and then every
  15th tick, owed or not, and the name on the same ticks while it is owed:
  - the name is the local player's `name` variable (the read Loot
    announcements' chat line uses), written once as a `progress` when it
    reads as text. `tracker stat` prints `name-owed=` (1 while it is owed)
    and `name-unresolved=` (the reads tried while owed that came back empty;
    the read keeps no reason token);
  - the zone goes through the tracker's own copy of ForgePact's satzone
    readers: `Controller_obj.satanicZone`, `GPV` of that key by SDK name with
    the local player as `self` (the proven pair `HhResolveLocalPlayer` +
    `HhResolveInstance`, the shape every measured read used), `room_get_name`
    of the room it answers, and the `satanicZoneBuff`/`satanicZoneDebuff`
    arrays (module guide Known Limitations, the `satmods` item's 2026-10-03
    correction, and RDM § 14.1). Its code is `Satanic_<act>_<zone>`, as the
    producer names it.
  An answer writes a `satanic_zone` and a `vitals` (whether the character
  stands in the zone's room) when a record is owed, or when the zone, the
  buffs or the debuffs differ from the last record written: a rotation, which
  the game rolls on its own between room changes (RDM § 14.1, **measured**
  2026-10-03: the store moved 20 to 4 within one session). A read that does
  not answer writes nothing, keeps the record owed, and keeps the step that
  failed for `zone-why=` (`no-controller`, `no-zone-key`, `no-player`,
  `gpv-failed`, `no-zone-rolled`, `room-unnamed`, `not-act-zone`, and
  `arrays-unread`: a buff or debuff array absent, not an array, or whose
  read threw). Before `arrays-unread`, the shared array reader answered an
  empty array on every failure, so a failed read would have passed for an
  empty one, and the rotation rule would have written a zone with `[]`, then
  another when the read recovered (**static reading** of the old adapter; not
  observed live); the tracker now reads both arrays through its own
  `TrReadIntArray`, which says whether the read happened. `zone-polls=`
  counts reads tried (answered = zone-polls - zone-unresolved), failures
  included, so it tells "nothing ran" from "ran" and no more: the zone
  readers' positive control is the install-time `satanic_zone` record (Live
  procedure 2's `zone-control`), not this count. Why reads can fail on a
  room change's first ticks: `GetMyPlayer` answers noone with no
  `Player_obj` (**static reading**), `Player_obj` gets a new instance id on
  a room change, and there is no `Player_obj` while the map is open (both
  **measured**; RDM § 1, "How the local player arrives"). How many ticks a new
  act zone runs without a player is **not established**. Live procedure 2
  bounds it rather than measuring it: it counts k failed polls (the
  `zone-unresolved=` delta), so the readers answered between (k-1)*15 and
  k*15 ticks after the room change. The milliseconds between the `room`
  record and the `satanic_zone` after it are an upper bound on when the
  readers answered, not a measured duration; it depends on how long the
  ticks ran during the load, not on `kZonePollTicks` (15 ticks) alone, and `zone-why=` keeps only the last failure's token. The
  tracker writes nothing of `satzone`'s state
  and does not call `SatZoneReadValue`, which records its failures as
  `satzone stat`'s last reason.
- **Arming:** armed at launch, hooked once a character exists, as Loot
  announcements (module guide Known Limitations item 8).
- **Writing:** on the game thread only, every 30 frames, one append of whole
  lines to a file opened with shared read and delete. No thread and no
  exit-time code: `tests/test_tracker_mod_exit_behavior.py` holds such a file
  open with lines pending across `ExitProcess` and reads exit `0x0` and the
  lines intact, against `0x7E2` for a joinable `std::thread` left at exit.

Live procedure 1 (below) measured the counters, a held bag drop and a clean
close with the producer installed. Live procedure 2 of the toolkit workorder
`forgepact-185-tracker-zone-fix` (below) measured that the rule above writes
the `satanic_zone` and the name after a room change, into an act zone and
back to town, and the `satanic_zone` on a rotation with no room change, in a
real session. That rotation was ForgePact's own `satzone pin 1` writing the
key with the arrays unchanged, not the game's own re-roll, and the name is
not read on a rotation at all (only while owed, after a room change).

## Live procedure 1 (2026-10-13)

Capture: `.claude/workorders/forgepact-185-tracker-move-live-1.md` in the
toolkit hub (`forgepact-185-tracker-move-live-1`). The research build
(sha256 `5d56eda0…`), Sorak in character-select slot 14, HS Offline
Tracker's v0.1.4 producer (`5f83e542…`) installed beside it, the ForgePact
panel closed, and x64dbg logpoints on the three counter scripts beside a
`CheckTalentUse` control. The owner took one step by hand: the bag drop, and
the travel to `Act_01_01` with the fighting there. The checks, with the
results the capture gives them:

- **Session validity**, all `pass`: `dll-hash`, `exit-reader`, `control`,
  `marker`, `dump-control` (cell A's forced crash read `0xC0000005` and its
  dump parsed), `dbg-control` (the control rose from hit 3,473 to 26,856).
- **Shipped behaviour:**
  - `arms-at-select` `pass`: `tracker 1` with no character answered
    `install=waiting-for-character` (sent at the main menu, not on the
    save-slot screen);
  - `installs-with-character` `pass`: `install=installed`, every route
    `both`, 5 s after the load;
  - `session-file` and `identity-records` `pass`: one file for the pid, 139
    lines, sequence 1 to 139 with none bad; `bridge_status`
    `transport_ready`, `room` `Town_01_rm` and `progress` `Sorak` first;
  - `placed-heroic` `pass`: a placed Heroic belt became one `ground_drop`;
    `mythic-untracked` `pass`: a placed Mythic did not, and
    `drops-untracked=` rose by 1;
  - `bag-drop-held` `pass`: the Heroic, picked up and dropped from the bag by
    the owner, wrote no `ground_drop`, and `held-bag-drop=` went from 0 to 1;
  - `deltas-flow` and `replay-matches` `pass`: the file's `session_delta`
    sums, 1,267 gold, 501,483 XP and 588 kills, equal `gold-sum=`,
    `xp-sum=`, `kills-sum=` and `tools/tracker_replay.py`'s totals;
  - `room-change` `pass`: a `room` record `Act_01_01`;
  - `save-read` `fail`: the procedure read save file 14, another character's.
    The save files count from 0, so slot 14 is file 13 (RDM § 8.2); the
    replay tool's `--save` now says so;
  - `clean-close` `pass`: `hs_stop_game` in the world with the tracker on
    read `0x00000000`, `out.txt` ended on `==== clean shutdown ====`, and the
    dump reader found no dump.
- **Research:**
  - `dbg-gold`, `dbg-xp`, `dbg-kills` `pass`: 17 `GoldLogAdd`, 588
    `ExperienceUpdate` and 588 `EnemyAddStatistics` hits, each equal to the
    matching `*-calls=` rise, with 2, 2 and 1 arguments on every hit. The
    candidates' first bytes were the installed detours' jumps, as the
    procedure expected, so the logpoints were armed on the live bytes.
    **Measured**: each script's argument count, and that the sensor sees
    every call the debugger sees. Gold's amount index stays the reviewed
    profile, and whether every gold credit passes through `GoldLogAdd` stays
    **not established**;
  - `kills-vs-save` and `xp-vs-save` `fail` as written: they read file 14.
    The operator's own reads of Sorak's file 13 moved by +588 kills and
    +501,483 experience, exactly the sensor's sums (**measured**);
  - `zone-record` `fail`: after the room change to `Act_01_01` the file
    gained no `satanic_zone` and no `progress`, and the stat read
    `zone-unresolved=1`. The only `satanic_zone` was the one written at
    install, in town. The sensor read both once, on the room change's own
    tick; replan 2's rule (Routes, the room) reads them until they answer;
  - `producer-coexists` `pass`, though whether the producer loaded into the
    process was not separately observed.

One 4.6 s freeze in town lined up in time with a debugger break in
`discordhook64.dll` that the debugger's keeper resumed. Its cause is **not
established** beyond that timing.

## Live procedure 2 (2026-10-14)

Capture: `.claude/workorders/forgepact-185-tracker-zone-fix-live-2.md` in the
toolkit hub (`forgepact-185-tracker-zone-fix-live-2`), from the toolkit
workorder `forgepact-185-tracker-zone-fix`. The research build with the five
fixes (sha256 `77125bac…58ed`), Sorak in character-select slot 14 and his
save file 13, HS Offline Tracker's v0.1.4 producer (`5f83e542…`) installed
beside it and unchanged at teardown, the ForgePact panel closed, and x64dbg
logpoints on `GetSatanicZoneOffline`, `GetSatanicZoneEligible` and
`NetworkRoomGoto` beside a `CheckTalentUse` control, every candidate's first
bytes matching the procedure's. The operator did every step, the fighting
included (`hs_input`); the owner took none. All 24 checks `pass`, as the
capture gives them:

- **Session validity**, all `pass`: `dll-hash`, `exit-reader`, `control`,
  `marker`, `dbg-control` (the control rose from hit 4,793 to 16,807) and
  `zone-control` (the install-time `satanic_zone` named `Satanic_3_5`).
- **Shipped behaviour**, all `pass`:
  - `installs-with-character`: `install=installed`, every route `both`,
    `zone=Satanic_3_5 zones-written=1`, `name-owed=0 name-unresolved=0`, 5 s
    after the load; the file opened with `bridge_status`, `room`
    `Town_01_rm`, `progress` `Sorak`, `satanic_zone` and `vitals` false;
  - `session-file`: one file for the pid, 59 lines, sequence 1 to 59;
  - `zone-rotation`: `satzone pin 1` with no room change wrote a second
    `satanic_zone`, `Satanic_1_1`, with the same arrays, and the stat read
    `zone=Satanic_1_1 zones-written=2`. No natural re-roll came before it;
  - `zone-after-act-room` and `name-after-room`: after `pcall
    NetworkRoomGoto 1` the `room` `Act_01_01` was followed by a `progress`
    `Sorak`, a `satanic_zone` and a `vitals` before the next `room`;
  - `zone-after-town` and `name-after-room`: after `pcall NetworkRoomGoto
    235` the `room` `Town_01_rm` was followed the same way, the `vitals`
    false;
  - `replay-matches`: `tools/tracker_replay.py` on the file read 201 gold,
    69,175 XP and 58 kills with `bad_lines` 0, the file's own sums;
  - `save-read`: both `--save 13` reads named Sorak;
  - `clean-close`: `hs_stop_game` in the world with the tracker on read
    `0x00000000`, and `out.txt` ended on `==== clean shutdown ====`.
- **Research**, all `pass`:
  - `here-true`: in `Act_01_01` with the store pinned to it, the `vitals`
    read `satanic_here: true`;
  - `zone-load-reason`: the act zone's first ticks had no player. Between
    the stats before and after the travel, `zone-unresolved=` rose 0 to 2
    with `zone-why=no-player` (which keeps only the last failure's reason,
    so only the last of the two is known to be `no-player`; the bound needs
    no more), so k = 2 and the readers answered between 15 and 30 ticks
    after the room change. The `room` and the `satanic_zone` after it were
    2,482 ms apart, an upper bound on when the readers answered, not a
    measured duration: it depends on how long the ticks ran during the
    load, not on `kZonePollTicks` alone, and at most 30 ticks in 2,482 ms
    means they ran far slower than 60 a second. Between the stat after the
    fighting (step 8, `zone-unresolved=2`) and the one after the way back
    to town (step 9, `zone-unresolved=5`), 3 more reads failed, the last
    `no-player`; all 3 fell after the travel's room change:
    `name-unresolved=` rose with them, and the name is read only while it
    is owed after a room change (no `room` record lies between step 8's
    stat and the travel; 633 ms between the travel's records, the same
    kind of bound). `name-unresolved=` rose in
    step with them (2, then
    5) while `name-owed=` read 0 at each stat, after the name was written;
  - `deltas-flow`, `kills-vs-save` and `xp-vs-save`: 58 kills and 69,175 XP
    from the operator's fighting; the file's sums equal `gold-sum=201`,
    `xp-sum=69175` and `kills-sum=58`, and save 13 moved by exactly the XP
    and kill sums (**measured**);
  - `dbg-travel`: `NetworkRoomGoto` hit twice, once per travel, each with
    one argument;
  - `dbg-roll` and `dbg-eligible`: `GetSatanicZoneOffline` and
    `GetSatanicZoneEligible` had no hit over the whole window, beside the
    rising control and both travels. So no roll was observed beside a room
    change by `NetworkRoomGoto`, in town or into an act zone. What does
    trigger a roll stays **not established**: no natural re-roll came in the
    session's 337 s (the file's four `satanic_zone` records are the install,
    the pin and the two travels), so the logpoints, armed for about 4.5
    minutes of it, had no roll to catch;
  - `producer-coexists`: present at the start and at teardown, with a clean
    close.

Two freezes lined up with the debugger's own holds: the control's arming
hold, and a `discordhook64.dll` TLS-callback break (4.7 s) the keeper
resumed. They are the debugger's, as in Live procedure 1.

## Rejected routes

- **Holding the mod off while `HSOfflineTrackerProducer.dll` is loaded.**
  ForgePact's Install has placed the producer since 1.3.8, so most players
  have it. Holding off would make the mod report ON and do nothing for them,
  the "reports itself ON while doing nothing" bug class. The two sensors feed
  different readers (the producer's pipe, this session file), so nothing is
  counted twice.
- **Reading the producer's pipe from the panel.** The pipe takes one client,
  and the panel would record only while it is open.
- **Hooking `RoomGoto`.** Polling the `room` builtin needs no hook. The
  producer's quiet window after a room change existed only for its drop guard,
  which the creation guard replaces.
- **The producer's thread-local suppression around `CA_playerItemDrop` and
  `Api_Exchange`.** Not measured on this build, and it needs a hook on an
  object event; the creation guard is measured.
- **The producer's exact profiles and its caller-address allowlist.** They are
  build-specific addresses (AGENTS.md, "Never Call an Address You Resolved by
  Hand"); everything here resolves by name.
- **A writer thread, as the producer had.** The producer's thread is what
  aborted every game exit (ForgePact #201). Batching on the game
  thread costs one small append every 30 frames, and what is still buffered at
  exit, at most 30 frames of it, is lost.
- **A second `HookOneScript` on `ExperienceUpdate`.** Table-only, since Extra
  rolls already holds the inline detour; one shared detour with two consumers
  instead.
- **Aggregating in the plugin, or in the page's JavaScript.** Run history and
  the import need disk and must work with the page closed; the plugin stays a
  sensor and the panel's ledger derives everything from event time.
- **Porting all of `stats.rs`.** About half of it is online-packet logic
  (purses, mail, pickups, ownership, server announcements, guild XP) that never
  ran offline, plus reconciliation of two sources; with the sensor in the game
  process there is no second source.
- **Settings inside `forgepact.json`.** Keeping the Tracker's own
  `settings.json` shape makes the import a copy and lets the Tracker tab port
  the Tracker's settings components.

Turned down in replan 2, for the zone after Live procedure 1:

- **Reading the zone inside `RoomGoto`, as the producer did.** The producer
  hooks `RoomGoto` and resolves the zone after the original returns, with
  that call's own `self` and `other`, before the new room loads. That is a
  new hook target, and it would still see a rotation only at the next room
  change. A polled read that retries covers both.
- **`GPV` with the global instance as `self`**, so the read needs no player.
  `GPV` hands its `self` on to its implementation (static reading), and every
  measured read used the player. A new call shape is a research question;
  the player's absence now costs only a retry.
- **Writing a zone only when it changes**, with no record per room change. A
  room change into or out of the zone's room changes `satanic_here`, which
  the ledger takes from `vitals`, and the producer also wrote one per room
  change.
- **Retrying only after a room change**, with no read between room changes.
  It misses a rotation while the player stays in one room (RDM § 14.1: the
  store moved on its own within a session).
- **A fixed delay after a room change.** The readers are asked until they
  answer, as map reveal asks its creators whether they are ready, rather
  than guessing how long a room takes to load.

## The Tracker tab (part 2b)

The workorder `forgepact-185-tracker-tab` (v2.3.0) builds the Tracker tab on
the data path above: the tab, rarity and zone alerts, a compact overlay
window and Discord presence, and it retires the producer ForgePact used to
ship. It researches no game mechanism: everything it shows comes from the
sensor's records and the save reader described above, and its live session
reuses the verbs Live procedures 1 and 2 proved. What it had to find out was
about the panel's own window, so the two probes below ran in throwaway
pywebview windows outside the repository before anything relied on them. The
module guide's section "HS Offline Tracker in ForgePact" describes the result.

### The probes

- **Overlay transparency: it does not draw through (measured 2026-10-14,
  pywebview 6.2.1, WebView2, Windows 11).** pywebview's `create_window` takes
  `transparent`, and on EdgeChromium it sets the WebView2 background to
  transparent (static reading of the installed package). The probe opened a
  transparent, frameless, on-top, `focus=False` window over a solid magenta
  one and read a screen pixel in the overlay's empty area through `GetPixel`:
  it was the form's grey (`#f0f0f0`), not the magenta behind (`#e83ffb`, read
  beside it), while a solid control window read its own colour. So the
  overlay ships solid (`OVERLAY_TRANSPARENT = False`), Q8's fallback. The
  same probe saw clicks reach a `focus=False` window, transparent or not,
  and neither window take the foreground.
- **Alert autoplay: the WebView2 variable works (measured 2026-10-14,
  WebView2, Edg/155).** pywebview 6.2.1 sets its own
  `AdditionalBrowserArguments` (static reading), so whether
  `WEBVIEW2_ADDITIONAL_BROWSER_ARGUMENTS` would still take effect was open.
  A probe page created an AudioContext and reported its state: with
  `--autoplay-policy=no-user-gesture-required` in the variable before
  `webview.start()` it was `running` at creation; without it, `suspended`,
  and still `suspended` 1.5 s after a `resume()` with no gesture. The panel
  sets the variable (`allow_alert_autoplay`), and the page's "click to let it
  play alert sounds" note ships anyway, for a browser tab, where no flag
  applies.

Neither probe ran inside the panel itself; Live procedure 1 of
`forgepact-185-tracker-tab` is where the panel's own window is observed (not
established until it runs).

### Decisions

- **The overlay is a panel window (Q8 (b), owner 2026-10-14).** A second
  pywebview window on the panel's own `/overlay.html`, solid because of the
  probe above. In-game counters, Q8 (c), are ForgePact #218. The flourish
  is a card inside that window, not a window of its own.
- **Offline features only (Q10).** The Game Link installer, the online-only
  views and fields (mail, purses, `resources`, MF) and the keys/resources
  counter are not ported. The counter never counted offline, since the drop
  filter keeps only the five tracked rarities; it is ForgePact #219.
- **ForgePact ships no producer (Q11, owner 2026-10-14).** Its own sensor and
  tab replace HS Offline Tracker, so `tools/toolchain-pins.json` loses the
  producer pin, `build_release.py` packages none and Install Mod Plugin copies
  none. Install moves a sensor whose hash is a known aborting build to
  `%LOCALAPPDATA%\Hero_Siege\backup\` and says why; Remove leaves every
  sensor; the launch only names an aborting one. This settles ForgePact #217:
  Remove keeps unknown sensors, the packaging hash guard, the clean-build
  ordering and the check-then-copy race are moot, and the persistent notice
  and the corrupt sensor reduce to Install's step and the launch note. HS
  Offline Tracker is frozen, pointing players here.
- **One window plays, each alert once.** Only the main window's page plays,
  and it claims an entry from the panel (`POST /api/tracker/announce`) before
  playing, so two windows of the panel never both play an id. A page claims
  only while its AudioContext runs, so nothing is marked announced while it
  is silent. The zone alert is numbered (`seq`, rising by 1 per rotation in
  the panel process) and claimed as `zone:<seq>`, and reading it never
  consumes it.
- **The page polls only when it has a reason.** `/api/tracker` is polled at
  most once a second, only while the switch is on or the tab is open, so a
  player who never uses the tracker sends no tracker request and the panel's
  performance budgets and behaviour oracle do not move.
- **The codex comes from the API.** The Tracker's 176 KB `items.js` is not
  bundled; `/api/tracker/codex` serves one row per named item from the
  tables part 2a generated.
- **Presence is off by default and needs an application ID.** One daemon
  thread, started only with an ID, which leaves the tracker lock before any
  pipe I/O. The Tracker's own releases never embedded an ID, and ForgePact
  ships none yet, so the switch stays hidden for players until the project
  registers a Discord application.
- **Tests never touch the developer's data.** The panel sandbox points the
  tracker store, the import source and the session folder into its own temp
  directory and answers whether a session's process is live itself; before
  this, the first browser suite that read `/api/tracker` built the store
  under the real `%LOCALAPPDATA%\Hero_Siege\tracker` and imported the real
  Tracker data.

### Rejected alternatives

- **A separate full-screen, click-through flourish window.** It needs Win32
  `WS_EX_TRANSPARENT` on a second transparent window, which is unmeasured on
  pywebview and, after the probe above, unlikely to draw through; it did not
  fit before the v2.3.0 date. The flourish is drawn inside the overlay.
- **Consuming the zone change on read** (the ledger's `take_zone_change` as
  it was). The first window to poll would take it from the others; a rising
  `seq` and a claim replace it.
- **Bundling `items.js` into the panel** for the codex: 176 KB on every load.
- **Keeping the sensor install for clean builds.** Q11 accepted retiring it
  entirely; keeping it would have left #217's ordering and race questions
  open for the next Tracker release.
- **Running the live session on the player build.** It has no drop on demand
  and no dump control, so it could measure neither the alert path nor the
  close. The workorder changes no plugin code; the player build is compiled
  at its final gate instead.
- **Building each panel item into `panel/dist`.** Parallel items would
  clobber one `dist`; each builds into its own `panel/artifacts/dist-<item>`.
- **`build_release.py` in an acceptance criterion.** It runs `taskkill /F
  /IM ForgePact.exe`, which would close the owner's own panel; release CI
  packages it.
