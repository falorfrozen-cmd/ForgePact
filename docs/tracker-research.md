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
- **The room: polled, not hooked.** Each tick compares `room_get_name` of the
  `room` builtin with the last name. On a change the sensor writes `room`, a
  `progress` with the local player's `name` variable (the read Loot
  announcements' chat line uses), and, when ForgePact's satzone readers answer
  (`Controller_obj.satanicZone`, `GPV` with the player as `self`, the
  `satanicZoneBuff`/`satanicZoneDebuff` arrays; module guide Known
  Limitations, the `satmods` item's 2026-10-03 correction, and RDM § 14.1), a
  `satanic_zone` named `Satanic_<act>_<zone>` as the producer names it, and a
  `vitals` saying whether the character stands in it.
- **Arming:** armed at launch, hooked once a character exists, as Loot
  announcements (module guide Known Limitations item 8).
- **Writing:** on the game thread only, every 30 frames, one append of whole
  lines to a file opened with shared read and delete. No thread and no
  exit-time code: `tests/test_tracker_mod_exit_behavior.py` holds such a file
  open with lines pending across `ExitProcess` and reads exit `0x0` and the
  lines intact, against `0x7E2` for a joinable `std::thread` left at exit.

**Not established** until Live procedure 1 of the toolkit workorder
`forgepact-185-tracker-move` runs: that the sensor's call counts equal debugger
hit counts on the three counter scripts, that its sums equal the save's
experience and kill differences, that a real bag drop is held, that a zone
change writes its `room` and `satanic_zone`, and that the game closes cleanly
with the sensor and the producer both installed.

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
