# The game's own Angelic roll: which step picks the unique (issue #64)

**Status: measured in live session 1 (2026-09-23), research build only.**
Nothing player-visible changes in this round. `## Results` records what that
session printed, both positive controls (C1 and C2) passed, and
`## Decision` labels every candidate. A candidate is only ever named in
`finding:` if its own route's positive control passed in the same session
(`AGENTS.md`, "Prove the Instrument Before Trusting a Negative Result"), and
every zero is written down as `not observed`, with the control that backs it.

This document carries names, indices, measured behaviour and our own commands
only - no decompiled script text. The game's code was read locally to know
where to look; what it does is written here in our own words, with no
addresses, no offsets and no call-by-call transcription.

## The question

Since #63, Headhunter and Tyrant's Crown drop through `angelicdrop`'s pool, so
every ForgePact setting that raises Liquor Holster's chance raises theirs by
the same amount. One path still differs: under an "Angelic item drop chance"
effect (Blood Pact or a dungeon modifier) the game rolls for an Angelic item
itself, and that roll can drop Liquor Holster but never the two signature
items, because they have no entry in the game's own unique list (Known
Limitations item 21, "the vanilla-roll gap").

Closing that gap needs a hook on the step of the game's own roll that picks
the item, so a follow-up can substitute a signature item at Liquor Holster's
share. `docs/angelic-drop-research.md` already describes that roll from a
static reading. In short: the roll only runs for a player carrying buff 332
(`buff_angelic_chance`); `DropItemAngelicChance` is where the roll and the
pick happen; the pick draws on the game's unique list (`lootListUnique`, on
`Loot_Manager_obj`) and on definitions read through `GetUniqueRepoStruct`;
and a hit ends with an item on the ground.

Before this round none of that past the buff check had been *measured* on the
current build: whether a hook on `DropItemAngelicChance` sees the game's own
call, what it returns, whether one call considers one candidate or several,
and which named script places the item. This round measured those, in one
build and one session.

## Static search

Searched: every script name in `hs-game-sdk`'s `scripts.hpp` containing
Angelic, Unique, RareDrop, DropItem, LootGround or CreateDefaultParams, plus
the closures nested in `Loot_Manager_obj`'s Create event. Every candidate
below exists there as a `gml_Script_` constant, and the probe names each one
through that constant rather than a typed string.

### Hooked this round (one row each)

| id | script | why it is a candidate |
| --- | --- | --- |
| `drop-item` | `DropItem` | the caller that owns the buff check; the "inside DropItem" flag hangs off it, and its count against kills shows whether every kill reaches it |
| `angelic-chance` | `DropItemAngelicChance` | the roll and pick; already hooked on demand by `raredrop angelic` and `angelicwatch` |
| `angelic-forced` | `DropItemAngelic` | the guaranteed Angelic maker; counted only, never called (it loops forever on an empty zone list) |
| `drop-boss` | `DropItemBoss` | sibling drop entry point |
| `drop-heroic` | `DropItemHeroic` | sibling drop entry point |
| `drop-debug` | `DropItemDebug` | sibling drop entry point |
| `drop-unique` | `DropUniqueItems` | the name says unique; role unknown |
| `unique-random-id` | `GetUniqueRandomItemID` | a picker by name |
| `unique-repo` | `GetUniqueRepoStruct` | the definition read; calls per roll tell one pick from a list walk |
| `unique-charm` | `GetUniqueCharm` | unique-named; expected to be about charms |
| `angelic-charm` | `DropAngelicCharm` | Angelic-named; expected to be about charms |
| `angelic-key` | `DropAngelicKey` | Angelic-named; expected to be about keys |
| `default-params` | `CreateDefaultParams` | the build step on a hit |
| `loot-create` | `LootGroundCreate` | placement |
| `loot-create-item` | `LootGroundCreateFromItem` | placement; ForgePact's own drops use it too |
| `loot-drop` | `LootGroundDrop` | placement |
| `rare-announce` | `GetRareDropAnnouncement` | fires on a rare drop; a cheap second witness of a hit |

The kill count comes from ForgePact's own hook on `EnemyDestroyKillProc`
(`InstallHeadhunterHook`), which also carries `angelicdrop`.

### Found and set aside

- `DefineItemUnique` followed by an item class (Rings, Belts, Charms, the
  weapon families and so on): the loaders that fill the unique definitions
  when the game starts, not anything a kill runs.
- `LoadAngelicAugment`, `DialogOpenAngelicRealm`, the other
  `DialogAngelicRealm` names, the `UiAAngelicRealm` interface actions,
  `UiUpShowAngelicTalentTooltip` and the `UI_Angelic_Upgrade_obj` /
  `UI_Angelic_Realm_*` closures: the Angelic Realm feature and its interface,
  a different mechanic that shares the word.
- `StashUniqueAddItemOnline`, `StashUniqueTakeItemOnline`,
  `UiAStashTabUniqueBuy`, `UiAStashUniqueItemType` and the
  `UI_Stash_Unique_Items_obj` closures: the online unique stash.
- `LootGroundInit`, `LootGroundDraw`, `LootGroundDeActiveStep`,
  `LootGroundRelicStep`: what a ground item does once it exists (set-up,
  drawing, per-frame behaviour), not how it comes to exist.
- The ten anonymous closures in `Loot_Manager_obj`'s Create event: no name
  ties any of them to the roll. They are the next round's candidates if every
  named row below comes back not observed.
- `cpr_irandom`: the game's die. It answers "what number", not "which script
  picks", it is a hot builtin, and `raredrop ceiling` already table-hooks it.

Existing verbs reused unchanged: `raredrop angelic` (opens the buff check so
every kill rolls), `buffme` (applies a real buff 332 through the game's own
`BuffAdd`), `angelicdrop` (ForgePact's own Angelic drop, the placement
control). `angelicwatch` counts kills against game rolls and is superseded by
this probe for the session, not removed.

## Candidates and controls

| id | script | expected route | positive control |
| --- | --- | --- | --- |
| `drop-item` | `DropItem` | via `Hook_DropItem` (native) | C1 (L-1.7): calls at least 1 with the gate open |
| `angelic-chance` | `DropItemAngelicChance` | detoured | C1 (L-1.7): calls at least 1 |
| `angelic-forced` | `DropItemAngelic` | via `Hook_DropItemAngelic` (native) | C1 (the via route, witnessed by `drop-item`) |
| `drop-boss` | `DropItemBoss` | via `Hook_DropItemBoss` (native) | C1 (the via route, witnessed by `drop-item`) |
| `drop-heroic` | `DropItemHeroic` | detoured | C1 and C2 (own detours) |
| `drop-debug` | `DropItemDebug` | detoured | C1 and C2 (own detours) |
| `drop-unique` | `DropUniqueItems` | detoured | C1 and C2 (own detours) |
| `unique-random-id` | `GetUniqueRandomItemID` | detoured | C1 and C2 (own detours) |
| `unique-repo` | `GetUniqueRepoStruct` | detoured | C1 and C2 (own detours) |
| `unique-charm` | `GetUniqueCharm` | detoured | C1 and C2 (own detours) |
| `angelic-charm` | `DropAngelicCharm` | via `Hook_DropAngelicCharm` (native) | C1 (the via route, witnessed by `drop-item`) |
| `angelic-key` | `DropAngelicKey` | via `Hook_DropAngelicKey` (native) | C1 (the via route, witnessed by `drop-item`) |
| `default-params` | `CreateDefaultParams` | detoured | C1 and C2 (own detours) |
| `loot-create` | `LootGroundCreate` | detoured (under table-only `Hook_LootGroundCreate`) | C2 (L-1.12) |
| `loot-create-item` | `LootGroundCreateFromItem` | detoured (under table-only `Hook_LootGroundCreateFromItem`) | C2 (L-1.12): calls at least the drops `angelicdrop status` reports |
| `loot-drop` | `LootGroundDrop` | detoured | C1 and C2 (own detours) |
| `rare-announce` | `GetRareDropAnnouncement` | detoured | C1 and C2 (own detours) |

One positive control per route, in the same session:

- **C1, own detours and the via route (L-1.7).** With the gate opened by
  `raredrop angelic 2` and at least 30 ordinary kills: `kills=` at least 30
  with the kill control native, the `angelic-chance` row (own detour, through
  `HookAngelicChance`) at least one call, and the `drop-item` row (via
  DropManager's native hook) at least one call. The chance the
  `angelic-chance` row logs is a result, not a pass condition: the hook only
  keeps it when it arrives as a real number, so a missing value would say
  something about the argument's kind, not about whether the hook fired. Zero
  calls on either with kills counted means that route was blind this session,
  and every row on it is `unmeasured`.
- **C2, the route under a table-only hook (L-1.12).** ForgePact's own
  `angelicdrop` places each item by calling `LootGroundCreateFromItem` by
  name, so the `loot-create-item` row has to count at least as many calls as
  `angelicdrop status` reports drops. Those calls sit outside both depths,
  which is how they stay distinguishable from the game's. C2 is also the only
  control that passes through the probe's own detour-counting code (the
  `angelic-chance` row counts from inside `HookAngelicChance` instead), so a
  row on its own detour needs C1 and C2 both: C1 shows a direct call from
  compiled GML reaches an inline detour on this build, C2 shows the probe's
  own detour counts what reaches it.
- The kill control is ForgePact's hook on `EnemyDestroyKillProc`, reported by
  the same route test as every row.

A row reported `TABLE-ONLY`, `blocked` or `not found` has no route and is
`unmeasured`; the probe prints its count as `n/a`, never as 0.

Two ways to make the game roll, both existing: **case A**, the gate opened by
`raredrop angelic 2` (every kill takes the buff branch, one game roll per
kill); **case B**, a real buff 332 applied with `buffme` and the gate closed.
Case A carries C1; case B shows whether the real branch reaches the same rows.

## Instrument

`angelicprobe` (research build only; not a player command) has four
subcommands:

- `angelicprobe on` attaches every row once, prints one line per row naming
  its route, and installs the kill control (`InstallHeadhunterHook`) so
  `kills=` counts, printing that hook's route the same way.
- `angelicprobe show` prints, per row, the route and `calls=`,
  `insideDropItem=`, `insideAngelicChance=` - or `calls=n/a` for a row with no
  route - then `kills=`, the `DropItemAngelicChance` hook's own totals,
  `lastChance=`, `lastReturn=` (the game's roll result: kind, and value when
  numeric) and how many results came back zero, nonzero and non-numeric, with
  the last nonzero one kept so a hit is still visible after later misses.
- `angelicprobe reset` zeroes those counters; the routes stay attached.
- `angelicprobe list` reads `lootListUnique` off `Loot_Manager_obj`, resolved
  by name (`asset_get_index`, `instance_find`, `HhResolveInstance`), read-only:
  its kind, its length and the shape of its first eight entries (kind, an
  array's length and numbers, a struct's key names). A missing object,
  instance or variable prints one refusal line.

The first three calls of each row are logged with the argument count, each
argument's kind and, when numeric and finite, its value formatted with `%g`,
plus the calling instance's object name - never with a bare `%f`, which can
abort the process on a bad value (Known Limitations item 10).

### Three routes, and why seven rows cannot take their own detour

A detour of the probe's own on a script is only possible while the script
table still holds the game's function. By the time a command can be sent, the
research build's startup has already hooked seven of the candidates:
DropManager's drop-multiplier hooks hold `DropItem`, `DropItemBoss`,
`DropItemAngelic`, `DropAngelicKey` and `DropAngelicCharm` (native first
installs), and the item-inspection hooks hold `LootGroundCreate` and
`LootGroundCreateFromItem` through the deliberately table-only installer. A
second install on any of them finds ForgePact's own function in the table and
can only be table-only - blind to the game's direct calls. So each row takes
the first route that applies, the same order `tgprobe` uses:

1. **detoured** - the table holds the game's own function: the probe detours
   it. Ten rows. The `angelic-chance` row reuses `raredrop angelic`'s own
   hook, the same function, saved original and hook id, rather than adding a
   second hook on that name, and reports whether its inline detour went in.
2. **detoured (under table-only hook)** - a ForgePact table-only hook holds
   the table, and the original it saved is the game's function: the probe
   detours that saved original. A direct call and the table hook's own call
   then both pass through the probe exactly once. The two loot-creation rows.
3. **via hook (native)** - a native ForgePact hook holds the table, and its
   saved original is a trampoline, not game code. Patching that would detour
   ForgePact's own code, so the hook body itself notes the call: a
   research-only line at the top of every DropManager drop hook, compiled to
   nothing in the player build. The five DropManager rows.

Every detour target is either a table entry found by name or an original that
a named install saved, and each is checked to be code inside the game's own
module before anything is patched. No address is typed in, no offset is
computed, and the probe never calls a candidate.

### Attribution

Two per-thread depth counters say where a call came from. The drop-item depth
is held for the whole of DropManager's `DropItem` hook body; the
angelic-chance depth is held by research-only lines in `HookAngelicChance`
around every call it makes to the game's roll, ForgePact's extra rolls
included. Every row counts a call as inside DropItem or inside
DropItemAngelicChance when the matching depth is nonzero. ForgePact's own
`angelicdrop` runs in the kill hook before the game's kill handling, so its
calls land outside both depths.

### The Angelic gate is found at startup in the research build

`raredrop angelic` finds the buff-check branch by scanning the game's
`DropItem` for its direct call to `DropItemAngelicChance`, reading both
functions through their script-table entries. In the research build
DropManager replaces `DropItem`'s entry at startup, so a later scan would
search ForgePact's own hook and report the call site as not found - code
reading, not observed live; the September runs that opened the gate predate
the table swap. The research build therefore looks the gate up once during
startup, just before DropManager's hooks go in, and keeps what it found;
opening the gate later reuses it. The lookup changes no byte. Its log line
(`angelic: gate found ...` or the reason it was not) is read at L-1.2; in
session 1 it reported the gate found inside the game's own `DropItem`. The
player build has the same exposure after any `dropmult` and is tracked
separately as ForgePact issue #69.

**Since #69 (2026-09-23), both builds.** `InstallHook` now records `DropItem`'s
and `DropItemAngelicChance`'s own code first thing, before any hook, and
keeps an entry only when `AddrIsExecutableInModule` places it inside the
game; the finder scans that record and never the live table entry. The
research build's startup lookup above stays for its log line, but the order
no longer decides whether the gate is found. The player-build exposure was
observed live on v1.4.5 (`dropmult gold 2`, then `raredrop angelic 2`
answered `angelic: call site not found - game build changed`), and the fix
was checked live the same way: the gate was found inside the game's own
`DropItem`, then `gate OPEN`; `raredrop angelic 1` closed it and restored
the bytes.

### Where it runs

`angelicprobe` is dispatched like every other command, inside
`PollCommands()` on the game thread between frames. Nothing it adds sits on
the per-frame path; after `on`, the only probe code that runs is inside the
hooked calls themselves.

### What the follow-up needs

The follow-up workorder (the substitution itself) reads these from
`## Results`; session 1's answer follows each one.

1. Whether `angelic-chance` saw calls on route `detoured`, and every other
   row's route. Yes: 181 calls in case A and 193 in case B, on its own
   detour; every row took the route it was expected to (L-1.4).
2. The argument count, and which argument is the chance. Four arguments: the
   first two real numbers that read like a map position, the third the
   chance (only 1195 and 1526 were seen), the fourth undefined; the calling
   instance is the dying monster.
3. The roll's return kind and value on a miss and, if one is seen, on a hit.
   Undefined on every call (374 of 374); no hit was seen, so a hit's return
   is not observed.
4. The `unique-repo` inside count per `angelic-chance` call: one means a
   single pick per roll; many means the list is walked. Many: about eight
   per roll on average (1409 over 181 rolls, then 1407 over 193).
5. Which of `loot-create`, `loot-create-item`, `loot-drop` and
   `default-params` has a nonzero inside count. Inside the roll, none (no hit
   happened). Inside `DropItem`, `default-params` and `loot-create` both do,
   on ordinary drops.
6. `lootListUnique`'s length and entry shape. Not read:
   `Loot_Manager_obj` carries no instance variable of that name, so where the
   game keeps the list is not established.
7. Whether case B (the real buff) reached the same rows as case A (the opened
   gate), and the logged chance under `buffme 332`. The same rows, in about
   the same proportions; the logged chance stayed 1195 or 1526 and never read
   the 3000 that `buffme` supplied.

## Live procedure

- **Build:** `plugin_build\BloodPactPlugin_rel.dll`, the research build,
  installed by the owner on request; windowed or borderless display.
- **Character:** save slot 14, Sorak (White Mage), in or one portal from a
  zone with ordinary monsters (owner's choice, 2026-09-22).
- **Control:** `angelicdrop status` answers one line beginning
  `angelicdrop: off | rolls=0 drops=0 fails=0 | pool` - the IPC channel is
  alive. A player build answers `angelicprobe` with `command unavailable in
  player build`, which ends the session at L-1.3.
- **Hygiene:** a fresh session, and no `citrace nativetrace`, `raredrop
  ceiling`, `scount` or `zonegenlog` in it - each installs table hooks of its
  own. No `angelicwatch` either: its reset zeroes the kill and roll counters
  the probe measures from.

Every reply is recorded verbatim in the session record. Offsets that a log
line prints are recorded there, never copied here.

- **L-1.1** The drive tool's self-check passes; the saves are backed up (an
  independent copy first, then the tool's own backup).
- **L-1.2** Launch to plugin-ready, load slot 14, then read the last 200 log
  lines: the load banner, and the startup gate lookup - expected one line
  beginning `angelic: gate found`. If it reads `call site not found`,
  `gate not uniquely identified` or `not found` instead, case A cannot run:
  skip L-1.6 to L-1.9 (C1 is "not run") and continue at L-1.10.
- **L-1.3** `angelicdrop status` - the control line above.
- **L-1.4** `angelicprobe on` - one line per row plus the kill control.
  Expected: `detoured` for the ten own-detour rows, `detoured (under
  table-only ...)` for the two loot-creation rows, `via Hook_... (native)` for
  the five DropManager rows, the kill control native. Any other route is a
  result in its own right, and that row is `unmeasured`.
- **L-1.5** `angelicprobe list` - the list's kind, length and up to eight
  entry shapes, or one refusal line.
- **L-1.6** `angelicprobe reset`, then `raredrop angelic 2` - expected
  `angelic: gate OPEN` and `x2.00 -> gate open, 1 roll(s) per kill`. The owner
  kills at least 30 ordinary monsters.
- **L-1.7 (C1)** `angelicprobe show`. Pass when `kills=` is at least 30 with
  the kill control native, `angelic-chance` is `detoured` with at least one
  call, and `drop-item` is `via Hook_DropItem (native)` with at least one
  call. `lastChance=` and the `drop-item` count against `kills=` are results,
  not pass conditions. Also read the last 60 log lines for the
  `angelic: roll` lines.
- **L-1.8** From the same reply: the `unique-repo` inside-angelic-chance
  count, and every placement row's inside counts (expected zero on misses).
- **L-1.9** `raredrop angelic 1` - expected `off (vanilla)` and `gate closed,
  original bytes restored`; then `angelicprobe reset`.
- **L-1.10 (case B)** `buffme 332 3000 3000 18000`; the owner kills at least
  30 ordinary monsters within five minutes; then `angelicprobe show`.
  Record `buffme`'s own reply verbatim. `angelic-chance` with at least one
  call and the gate closed means the real branch ran, which by the static
  reading also means the buff was on the player (its presence, not its
  value, decides whether the branch runs). Record `lastChance=`: 3000 would
  mean the buff value arrives as the chance. Any zero here is labelled with
  what was supplied (id, both values, duration) and whether the buff's
  presence was confirmed, not closed as a fact about the hook or the buff.
- **L-1.11 (outlier, optional)** One boss or rare kill if one is at hand, then
  `angelicprobe show`: whether the `drop-boss` or `drop-heroic` rows carry an
  inside count. Skipped means `not observed`.
- **L-1.12 (C2)** `angelicprobe reset`, then `angelicdrop 1` (a line beginning
  `angelicdrop: 1 in 1 kills`); the owner kills three ordinary monsters and
  leaves the items on the ground; then `angelicdrop status` and `angelicprobe
  show`. Pass when `loot-create-item` is `detoured (under table-only
  Hook_LootGroundCreateFromItem)` with at least as many calls as the drops
  `angelicdrop status` reports. Then `angelicdrop off`.
- **L-1.13** Stop the game normally and compare the saves against the backup;
  record what changed.

A game-roll hit is not expected in one session (a chance in the low
thousands against base rates in the millions), so a hit's placement rows stay
`not observed` unless one happens. None happened in session 1.

## Results

Session 1, 2026-09-23: the research build at ForgePact commit 18d2800
(plugin v1.4.4), save slot 14 (Sorak, White Mage), driven through the drive
tool with the owner doing the killing. The replies are quoted without the
offsets some log lines carry; the session record keeps those.

| step | reply |
| --- | --- |
| L-1.1 | The drive tool's self-check passed all six checks, its three positive controls proven. The 104 save files were copied by hand and compared by hash (identical), then backed up by the tool. |
| L-1.2 | Launched to plugin-ready and loaded slot 14. The startup gate lookup printed `angelic: gate found at DropItem+...` - the gate found inside the game's own `DropItem` - so case A could run. The banner then listed the startup hooks, `LootGroundCreate` and `LootGroundCreateFromItem` among them. |
| L-1.3 | `angelicdrop: off \| rolls=0 drops=0 fails=0 \| pool not built yet` - the control line; the command channel was alive and the build was the research build. |
| L-1.4 | `angelicprobe on: 17 row(s) counted, 0 not (calls=n/a)`. Every row took its expected route: `detoured` for the ten own-detour rows (`angelic-chance` through `HookAngelicChance`), `detoured (under table-only Hook_LootGroundCreate)` and the same for `Hook_LootGroundCreateFromItem` on the two loot-creation rows, `via Hook_<Name> (native)` on the five DropManager rows, and the kill control `native (Hook_EnemyDestroyKillProc holds a trampoline)`. |
| L-1.5 | `angelicprobe list: Loot_Manager_obj carries no lootListUnique (variable_instance_exists false) - nothing read`. The object's instance was found; the variable was not on it. |
| L-1.6 | `angelicprobe reset: counters zeroed, routes kept`, then `angelic: gate OPEN (the game now rolls for angelic drops)` and `raredrop angelic: x2.00 -> gate open, 1 roll(s) per kill at the game's own chance`. The owner killed about 50 ordinary monsters. |
| L-1.7 | **C1 passed.** `kills=145` with the kill control native; `drop-item` via its native hook `calls=181`; `angelic-chance` detoured `calls=181 insideDropItem=181`; the hook's totals `hookCalls=181 extraRollBatches=0 lastChance=1526 lastReturn=undefined:-` with returns `zero=0 nonzero=0 nonNumeric=181`. The log's `angelic: roll` lines ran past number 100, with chances 1195 and 1526, each naming the dying instance - one of them a breakable hay prop rather than a monster, which would explain `DropItem` (and the roll) running more often than the kill hook counts kills. |
| L-1.8 | From the same reply: `unique-repo` `calls=1708 insideDropItem=1708 insideAngelicChance=1409`. Placement rows inside the roll were all zero; `default-params` and `loot-create` each read `calls=14 insideDropItem=14 insideAngelicChance=0`; `loot-create-item`, `loot-drop` and `rare-announce` read zero calls. |
| L-1.9 | `angelic: gate closed, original bytes restored`, `raredrop angelic: off (vanilla)`, `angelicprobe reset: counters zeroed, routes kept`. |
| L-1.10 | Case B. `buffme` replied `buffme id=332 [3000.000000,3000.000000] st=0`. After 30 or more kills with the gate closed: `kills=166`; `drop-item` `calls=193`; `angelic-chance` `calls=193 insideDropItem=193`; `lastChance=1526 lastReturn=undefined:-`, `nonNumeric=193`; `unique-repo` `calls=3061 insideDropItem=3061 insideAngelicChance=1407`; `default-params` and `loot-create` `calls=49 insideDropItem=49 insideAngelicChance=0`; every other row zero. The first logged rolls carried four arguments - two real numbers (for instance 13239.6 and 5192.55), then a real chance of 1195 or 1526, then undefined - with a dying monster as the calling instance. The chance never read 3000. |
| L-1.11 | Not run: no boss or rare monster was at hand. `drop-boss` and `drop-heroic` stay not observed for boss and rare kills. |
| L-1.12 | **C2 passed.** Setup printed `angelic pool: 49 candidates, 11 rejected` and `angelicdrop: 1 in 1 kills \| rolls=0 drops=0 fails=0 \| pool 49 candidates`. After the kills, `angelicdrop: 1 in 1 kills \| rolls=9 drops=9 fails=0`, and `loot-create-item` (under its table-only hook) `calls=9 insideDropItem=0 insideAngelicChance=0` - nine calls for nine drops, outside both depths. `kills=9` (the owner reported three; the kill hook counted nine). `drop-item` `calls=12`, `angelic-chance` `calls=0`; `unique-repo` `calls=129 insideDropItem=19`, its first three calls logged with no calling instance just before ForgePact's own pool was built (where the other outside calls came from was not traced). `angelicdrop off` then replied `angelicdrop: off \| rolls=9 drops=9 fails=0 \| pool 49 candidates`. |
| L-1.13 | The game closed normally (not forced). Against the backup, only slot 14's two save files and `shop.ini` changed; nothing was added or missing. |

## Negative results, sourced

Earlier negatives, re-labelled:

- **`LootGroundCreate` "was never called at runtime (measured: 0)"** - a
  comment beside the research build's `LootGroundCreate` hook. That zero came
  from the table-only installer, which cannot see the game's direct calls
  (`AGENTS.md`, "Prove the Instrument"): it was **not observed through a table
  hook**, never a fact about the game. Session 1 contradicts it: the
  `loot-create` row, on a detour under that same table-only hook with C2
  passing on the same route, counted 14 game calls in case A and 49 in case
  B, every one inside `DropItem`. The game does call `LootGroundCreate`
  directly, for ordinary drops.
- **Not observed without buff 332 (984 kills)** - `docs/angelic-drop-research.md`:
  984 kills without the buff produced no call to `DropItemAngelicChance`,
  measured through the hook of that date, which was a direct detour. That
  same hook logged rolls with the gate opened on 2026-09-05/07; whether a
  positive control ran in the same session as the 984 kills is not recorded.
  Session 1 has one window consistent with it - L-1.12, gate closed, no
  `angelic-chance` call over 12 `DropItem` calls - but whether `buffme`'s buff
  had run out by then was not read, so it is not counted as a control.
- **The gate cannot be found after startup in the research build** - code
  reading only (see `## Instrument`), **not observed live**. L-1.2 shows the
  startup lookup found it; no later lookup was tried.

Zeros from session 1. A row on its own detour is backed by C1 and C2, a row
on the via route by C1, a loot-creation row by C2; all three passed, and no
row came up `TABLE-ONLY`, `blocked` or `not found`, so none is `unmeasured`.

- **Via rows, no call in any window** (cases A and B, and L-1.12):
  `angelic-forced`, `drop-boss`, `angelic-charm`, `angelic-key` - not
  observed, backed by C1 (`drop-item` counted 181 on the same route).
- **Own-detour rows, no call in any window:** `drop-heroic`, `drop-debug`,
  `drop-unique`, `unique-random-id`, `unique-charm`, `loot-drop`,
  `rare-announce` - not observed, backed by C1 and C2; the same detour code
  counted `unique-repo` and `default-params` in the same windows.
- **No game call to `LootGroundCreateFromItem`** in cases A and B - not
  observed, backed by C2; all nine calls in L-1.12 were ForgePact's own.
- **No placement inside the roll** - every placement row read
  `insideAngelicChance=0` over 374 game rolls (181 plus 193). No roll hit:
  every return was undefined and `rare-announce` stayed at zero. Which script
  places an item on a hit is therefore not observed; it needs a hit.
- **The supplied buff value as the chance** - not observed with
  `buffme 332 3000 3000 18000` (supplied: buff 332, both values 3000, 18000
  frames; `buffme` replied `st=0`). The buff's presence was confirmed only
  indirectly, by the real branch running 193 times with the gate closed. The
  chance argument read 1195 or 1526, the same values as case A, so what the
  chance is made of was not established.
- **`lootListUnique` on `Loot_Manager_obj`** - not observed: the instance
  exists, the variable does not. The list's length and entry shape are
  unmeasured.
- **Boss and rare kills** (L-1.11) - not run, so `drop-boss` and
  `drop-heroic` are not observed for them. That is a limit of the session's
  schedule, not evidence that a boss's drop skips either script.

## Decision

Each candidate's line opens with its label. `works`: the row counted the
game's own calls on a route whose positive control passed. `not observed`:
no game call on such a route (sourced above). `unmeasured`: no route, or a
failed control - none this session. `finding:` names only candidates
labelled `works` that are the step the follow-up hooks to substitute
Headhunter or Tyrant's Crown at Liquor Holster's share.

* `drop-item` - **works.** 181 calls for 145 counted kills in case A and 193
  for 166 in case B, through DropManager's native hook; every game roll
  happened inside it. It is the caller that owns the buff check, not the
  pick, and it runs for every drop, so it is not where a substitution
  belongs.
* `angelic-chance` - **works.** One call per game roll on its own detour
  through `HookAngelicChance`: 181 with the gate open and 193 under the real
  buff with the gate closed, always inside `DropItem`. Four arguments, the
  third the chance, the dying monster as the calling instance, undefined
  returned on every one of 374 misses. The definition reads that make up the
  pick happen inside this call. This is the step the follow-up hooks.
* `angelic-forced` - **not observed.** No call on the via route in any
  window (C1). It is counted only, never called.
* `drop-boss` - **not observed.** No call on the via route (C1), on
  ordinary kills only; L-1.11 did not run.
* `drop-heroic` - **not observed.** No call on its own detour (C1 and C2),
  on ordinary kills only; L-1.11 did not run.
* `drop-debug` - **not observed.** No call on its own detour (C1 and C2).
* `drop-unique` - **not observed.** No call on its own detour (C1 and C2).
* `unique-random-id` - **not observed.** No call on its own detour (C1 and
  C2), inside the roll or anywhere else, so it is not what picks the unique
  on these kills.
* `unique-repo` - **works.** 1409 of its 1708 calls in case A and 1407 of
  3061 in case B came from inside the roll - about eight definition reads
  per roll on average, so a roll considers several entries rather than one.
  Its many calls outside the roll, inside `DropItem`, make it a noisy place
  to substitute; it tells the follow-up what the roll reads, not where to
  hook.
* `unique-charm` - **not observed.** No call on its own detour (C1 and C2).
* `angelic-charm` - **not observed.** No call on the via route (C1).
* `angelic-key` - **not observed.** No call on the via route (C1).
* `default-params` - **works.** 14 game calls in case A and 49 in case B,
  all inside `DropItem`, none inside the roll: it builds ordinary drops. Its
  part in an Angelic hit is not observed, because no hit happened.
* `loot-create` - **works.** The same counts as `default-params`, on the
  detour under the table-only hook with C2 passing: the game calls
  `LootGroundCreate` directly for ordinary drops. Its part in an Angelic hit
  is not observed.
* `loot-create-item` - **not observed.** No game call in case A or B (C2);
  its nine calls in L-1.12 were ForgePact's own placements.
* `loot-drop` - **not observed.** No call on its own detour (C1 and C2).
* `rare-announce` - **not observed.** No call on its own detour (C1 and C2),
  consistent with no roll hitting.

finding: angelic-chance

`DropItemAngelicChance` is the one measured step that runs exactly once per
game roll, on both ways of making the game roll, on a route that sees the
game's direct call, with the pick inside it. Three things the follow-up still
has to plan around, each sourced above: a hit was never seen, so neither a
hit's return value nor the script that places a hit's item is known; the
game's unique list was not found where the static reading put it; and the
chance the game passes did not follow the value `buffme` supplied.

## Session 2: the substitution (issue #74)

Issue #74 takes Headhunter and Tyrant's Crown out of ForgePact's own Angelic
pool and puts them on the game's own roll instead: while an item's World
switch is on, a real Angelic hit can also drop that item beside the game's own
Angelic or Unholy item, at one pool entry's share, and with both switches off
the game's roll is left alone. The closing paragraph of `## Decision` named
three things this round had to plan around. The static reading below answers
them in words; `### Live procedure 2` describes the session that measures the
result, and `### Results` is where that session's replies go.

### Static reading (2026-10-02)

Read locally on 2026-10-02 in the named Ghidra project: `DropItemAngelicChance`
and the scripts it calls, `GetUniqueRepoStruct`, `GetUniqueRandomItemID`, and a
scan of the roll's callers. Every claim in this subsection is a static
reading unless it says measured; no hit of the game's roll has been seen live
yet, so none of it is confirmed by one.

- **A hit's return value.** The roll sets its result to undefined as it
  starts and never assigns it again, so a hit returns undefined exactly as a
  miss does. That explains why all 374 measured misses in session 1 came back
  undefined, and it rules the return value out as a way to tell a hit from a
  miss: a hit has to be recognised by something the roll does only when it
  hits.
- **What places a hit's item.** Only on a hit does the roll build the item's
  parameters through `CreateDefaultParams`, and then it places the item by a
  direct call to the script ForgePact hooks as `LootGroundCreate`, with the
  roll's position and the picked item's parameters. `CreateDefaultParams` is called nowhere else inside the roll, so a
  call to it while the roll is in progress is a hit. That fits session 1's
  measured `default-params` row: no call inside the roll over 374 misses, 14
  and 49 calls inside `DropItem` on ordinary drops. Because the placement is a
  direct call, the table-only `LootGroundCreate` hook the player build keeps
  for item inspection cannot see it. `CreateDefaultParams` can be watched
  instead: session 1 measured its own inline detour counting the game's
  direct calls, and nothing in the player build holds it at startup, so a
  hook installed by name on it is a first install and gets the detour.
- **Where the list lives.** The roll reads its unique list through a
  reference-typed scope whose variable slot is resolved at runtime. It is not
  an instance variable of `Loot_Manager_obj`, which agrees with session 1's
  measured `variable_instance_exists` false on that instance; which scope it
  is was not resolved. The pick around it: a random index into the list, an
  entry that must be three numbers (type, sub and b), the definition read
  through `GetUniqueRepoStruct`, and a re-pick whenever that definition is
  flagged hidden or its rarity is neither Angelic (7) nor Unholy (10) - the
  same two filters ForgePact's own pool applies. Roughly one entry in eight
  passes, which is the eight-or-so definition reads per roll session 1
  measured. The die is then rolled against the picked definition's rate, a
  value the definition supplies scaled by one global read when the roll
  starts (`droprate.base` is the plausible reading; neither part is
  resolved), and the roll hits when the die lands below the chance.
- **What the chance is made of.** Unresolved. The caller, `DropItem`,
  computes it, and that caller's body did not finish decompiling within the
  planning session. The owner put the chance out of scope for #74, and the
  feature does not need it: it reacts to a hit, whatever produced it.

What #74 builds on this reading: the player build holds a roll-in-progress
state over each of the game's roll calls (ForgePact's extra rolls included),
hooks `CreateDefaultParams` by name, and counts a call to it seen while that
state is raised as a hit. On each hit it rolls the share once - k / (N + k),
k the number of switches on and N the size of ForgePact's validated pool (49
live on 2026-09-23) - and on success drops one of the enabled items through
`SpawnSignatureItem` at the roll's own position, with the dying monster as
the calling instance, while the game's own item lands as well. Neither hook is
installed until a switch is turned on, and with both switches off a hit
passes straight through. The **Angelic / Unholy Drops** slider's pool no
longer carries either item.

Hit detection is only as good as that one hook, and a hook that came up
table-only would see none of the game's direct calls (the blindness `##
Instrument` describes). So the detection is not trusted on its own word:

- The `CreateDefaultParams` hook counts every call it sees, roll in progress
  or not, as `cdpCalls=`. Ordinary drops call it too (session 1's 14 and 49
  inside `DropItem`), so after a few kills a working detour shows
  `cdpCalls` above zero whether or not any roll hit.
- The hook's install route is kept from `HookOneScript`'s own result and
  printed as `detect=detoured`, `detect=TABLE-ONLY`, `detect=not found` (the
  name did not resolve) or `detect=off` (not installed). Both `sigdrop
  status` and `angelicprobe hit status` end with the pair, in this order:
  `cdpCalls=<n> detect=<route>`.
- In the player build, a switch turning on whose `CreateDefaultParams` hook
  did not get its detour does not arm the gate: the install logs one refusal
  line naming `CreateDefaultParams` and the route it got, `detect=` reports
  it, and `gate=` stays off for both items. A gate that reads on therefore
  means a detoured detection, and `gameHits=0` beside `detect=detoured` and a
  growing `cdpCalls` means no hit yet, not a blind hook.

### Live procedure 2

Session 2 runs the research build (`plugin_build\BloodPactPlugin_rel.dll`,
built with the literal `dev`) through the drive tool, with the owner doing the
killing, on session 1's character (save slot 14, Sorak, White Mage). The
step-by-step procedure (the exact commands, the hygiene, the five short kill
batches) is `### Live procedure 1` in the workorder's context file,
`.claude/workorders/forgepact-74-signature-angelic-roll-context.md`, which
stays on the owner's machine; it is this document's second live procedure,
hence the heading here. Its hygiene adds one rule to session 1's: no
`angelicprobe on`, because that command's `default-params` row would take the
detour this feature needs on `CreateDefaultParams`.

A natural hit is not expected in one session (a chance in the low thousands
against rates in the millions), so the research build carries levers under
`angelicprobe hit`: `chance <n>` overwrites the roll's chance argument when it
arrives as a real number, `rate <n>` sets `droprate.base` on every validated
pool entry's definition (remembered and restored), `share <pct>` forces the
share so dispatch shows in a few kills, `off` restores everything, and
`status` reports each lever. Any lever turning on installs the detection, so
hits are counted with both switches off. None of this is in the player build.

Each check below is recorded as pass, fail, not-observed or (for
`force-hit` only) instrument-blind, with the replies quoted in the session
record. `force-hit` and `list-scope` are research checks about the game; the
others check behaviour that ships.

`gameRolls` is a control for the `DropItemAngelicChance` hook and `control`
only shows the command channel is alive; neither says the
`CreateDefaultParams` detection can see the game's direct calls. That route
gets its own positive control: `detect=detoured` and `cdpCalls` above zero
after the first kill batch, read before any `gameHits=0` is recorded.

- **`dll-hash`** - the installed DLL's SHA-256 equals the one recorded when
  the research build was made. Pass: the session measures this build. Fail:
  another build is installed, and nothing after it counts.
- **`marker`** - `angelicprobe hit status` names every lever off and the
  detection not installed, its line ending `cdpCalls=0 detect=off`. Pass:
  the research build, from a clean start. Fail (a player build answers `command
  unavailable in player build`): the session ends there.
- **`control`** - `sigdrop status` answers with the force off, its line
  ending `cdpCalls=0 detect=off`, and every new counter (`gameRolls=`,
  `gameHits=`, `cdpCalls=`, `shareRolls=`, `sigFromGame=`) at zero. Pass:
  the command channel is alive
  and the counters start clean. Fail: no counter read later in the session
  can be trusted.
- **`force-hit`** (research) - with both switches off and the chance lever
  set (the rate lever as the fallback), ten kills give at least one
  `gameHits`. First, after that first batch of ten, `sigdrop status` must
  show `detect=detoured` and `cdpCalls` above zero: the detection's own
  positive control, on the route that counts hits. Pass: a lever makes the
  game's own roll hit, and the detection sees it - the first hit of the
  game's roll seen live in this research. Instrument-blind, either way:
  `detect=` reads `TABLE-ONLY`, `not found` or `off` after a lever was set,
  or `cdpCalls=` is still zero after the batch, so the detection could not
  have counted a hit; or `gameRolls=` is below 10 after that first batch of
  ten kills, so the roll hook was not reached or the gate was not taken and
  the levers never acted on a roll. Either way `gameHits=0` measures the
  hooks, not the levers or the game. The session records the `gameRolls=`,
  `detect=` and `cdpCalls=` replies and the `raredrop angelic 2` reply as
  printed, does not try the rate lever, and the two checks that need a hit
  (`on-headhunter-only`, `on-both`) do not run; the result is a defect to
  fix before the next session, not a finding about the roll. Not-observed
  (only with `detect=detoured`, `cdpCalls=` above zero and `gameRolls=` at
  10 or more): neither lever produced a
  hit; the detection is shown to see the game's calls, so this is a
  statement about the levers, both levers' replies are recorded, the hit
  path rests on the harness and the static reading alone, and the two checks
  that need a hit do not run.
- **`baseline-off-no-signature`** - with both switches off, no signature item
  comes from the game's roll: `sigFromGame` stays at zero and no `sigdrop:`
  line appears, hits or not. Pass: off really is off. Fail: the switch gate
  leaks, a shipping defect.
- **`on-headhunter-only`** - Headhunter on, Tyrant's Crown off, share forced
  to certain: every hit drops one Headhunter beside the game's item and no
  crown, with one `angelic hit:` line each. Pass: the gate and the dispatch
  work per item. Fail: a wrong item or a count that does not match the hits,
  a shipping defect. Not run when `force-hit` is not-observed or
  instrument-blind.
- **`on-both`** - both switches on: `sigFromGame` grows by the hit count and
  both items appear. Pass: the even split between two enabled items works.
  Fail: a shipping defect. Not run when `force-hit` is not-observed or
  instrument-blind.
- **`pool-without-signature`** - switches and levers off, the slider at one
  in one: every kill drops an Angelic item, `angeliclist` prints no signature
  line, and `sigFromGame` does not move. Pass: the slider's pool is the game's
  real uniques again. Fail: one of the two items is still in ForgePact's own
  pool.
- **`sigdrop-still-forces`** - `sigdrop crown` and one kill drop a Tyrant's
  Crown. Pass: the test command is unchanged. Fail: a regression in it.
- **`list-scope`** (research) - `angelicprobe list` says which scope answered
  for the unique list and what it held. This is a result, not a pass
  condition: a global answer locates the list, an instance answer would
  contradict session 1, and neither leaves its location unresolved.

### Results

Session 2 ran as Live 1 on 2026-10-02: the research build whose SHA-256
begins `4534c0ff`, save slot 14 (Sorak), the owner doing the killing and the
drive tool sending every command. All ten checks were recorded; the session
record stays with the workorder on the owner's machine. Every value below is
**Measured (Live 1, 2026-10-02, research dll 4534c0ff…)** unless it says
otherwise. The owner killed more monsters than each batch asked for and the
exact counts were not taken, so the checks that depend on a kill count were
judged from the growth of the counters between two status reads instead.

- **`dll-hash`** - pass. The installed DLL's hash matched the research build's.
- **`marker`** - pass. `angelicprobe hit status` named every lever off and the
  detection not installed, ending `cdpCalls=0 detect=off`.
- **`control`** - pass. `sigdrop status` answered with the force off and every
  counter at zero, ending `cdpCalls=0 detect=off`.
- **`force-hit`** (research) - pass, under the chance lever alone; the rate
  lever was not needed. After the first batch (more than ten kills) the
  status read `detect=detoured`, `cdpCalls=47`, `gameRolls=47` and
  `gameHits=44`. This is the first hit of the game's own Angelic roll seen in
  this research, and it was seen through the `CreateDefaultParams` detection,
  on the detoured route, with that route's own positive control (`cdpCalls`
  above zero) read first. The ground filled with real Angelic and Unholy
  uniques, none of them Headhunter or Tyrant's Crown.
- **`baseline-off-no-signature`** - pass. With both switches off,
  `sigFromGame` stayed at zero across those 44 hits, and each hit logged that
  no switch was on and nothing was rolled; no `sigdrop:` line appeared.
- **`on-headhunter-only`** - pass. With Headhunter on, Tyrant's Crown off and
  the share forced to certain, the next batch added 14 hits and 14 to
  `sigFromGame`, every one a belt (`belt=14`, `crown=0`), with 14 matching
  `angelic hit:` lines and 14 Headhunter drop lines.
- **`on-both`** - pass. With both switches on, the next batch added 40 hits
  and 40 to `sigFromGame`: 20 crowns and 20 belts. Both items lay on the
  ground beside the game's own items.
- **`pool-without-signature`** - pass on the parts that do not depend on the
  kill count. With switches and levers off and the slider at one in one,
  `sigFromGame` stayed at 54, the forced-roll counter stayed at zero, no new
  `sigdrop:` line appeared, the pool's nine drops were all real uniques, and
  `angeliclist` printed no signature line. The procedure's `drops=3` was not
  established: more than three kills were made, and the slider reported
  `drops=9`.
- **`sigdrop-still-forces`** - pass. `sigdrop crown` dropped ten Tyrant's
  Crowns (the forced-roll counter went from 0 to 10) while `sigFromGame` stayed
  at 54: the test command is unchanged and is counted apart from the game's
  roll.
- **`list-scope`** (research) - not observed in either scope (no positive
  control on the same instance or the global scope). `angelicprobe list`
  found no global `lootListUnique` and no `lootListUnique` instance variable on
  `Loot_Manager_obj`, and read nothing. **Re-labelled by Session 3: this
  negative asked the wrong scopes.** The static reading of 2026-10-02 puts
  the list on `Controller_obj`, which the probe did not ask, so the result
  says nothing about the list itself.

Two further readings came out of the session:

- **The pool's size.** `angeliclist` and the first hit's log line both read
  47 candidates and 11 rejected, where the procedure expected 49 (the count
  measured on 2026-09-23, when #63 still appended both signature items to the
  pool; 47 plus those two is consistent with it, but not checked). The
  share a hit rolls is therefore k / (47 + k) on this build: 1 in 48 with one
  switch on, as the `angelic hit:` lines printed.
- **Calls outside a roll pass through.** With the vanilla gate closed again
  (step 7), `cdpCalls` rose from 113 to 118 while `gameRolls` stayed at 108:
  ordinary drops still reach `CreateDefaultParams`, the detection counts them,
  and since no roll was in progress none was taken for a hit.

What this settles of the static reading above:

- **A hit is detected, measured.** A call to `CreateDefaultParams` while the
  roll is in progress is how a hit shows, and Live 1 counted 98 of them over
  108 rolls under the chance lever, with the ground filling with the game's
  own Angelic and Unholy items (seen in screenshots, not counted against the
  hits). A hit at the natural chance (a chance in the low thousands)
  has still not been observed; the lever replaced only the chance argument,
  not the path a hit takes.
- **Some rolls at a chance of 1e9 still missed, and why is not established.**
  The first batch counted 47 `gameRolls` against 44 `gameHits`; two candidate
  causes are unverified: the lever's write reaching only rolls whose chance
  argument arrived as a real number, and the re-pick running out of entries
  before one passed its filters.
- **The list's scope is still not identified.** The list was not observed in
  either scope (no positive control on the same instance or the global scope):
  `angelicprobe list` found neither a `lootListUnique` global nor a
  `Loot_Manager_obj` instance variable; which scope the roll reads remained
  unresolved at the end of this session. Session 3 re-labels this negative as
  one that asked the wrong scopes: the roll reads a variable of
  `Controller_obj` (static reading, `## Session 3: list injection (issue #74)`
  below).

## Session 3: list injection (issue #74)

Session 2's design dropped a signature item beside the game's own Angelic or
Unholy item. On 2026-10-02 the owner replaced it: Headhunter and Tyrant's
Crown are to join the game's own Angelic unique list, so that the game's
picker chooses them, its die decides, and on a hit the game itself builds and
places the item - one item per hit, in place of what the roll would otherwise
have dropped, never beside it. The owner's words in #74: if the game draws
from a lookup of items, pass that lookup plus our items, so its own choice can
land on ours naturally. Gate: each item's panel switch only; with both off the
roll is vanilla. The fallback, only if that proves infeasible, is replacing
the game's item at the hit. The static reading below is what the design rests
on; `### Live procedure 3` is the session that measures it, and `### Results`
is where that session's replies go.

### Static reading (2026-10-02, list injection)

Read locally on 2026-10-02 in the named Ghidra project: the roll's reference
to its list, the sites that load the same variable slot, `GetUniqueRepoStruct`,
`CreateDefaultParams`, `LootGroundCreate` and its callees `CreateLootInFreePos`
and `LootGroundInit`. Each claim below is labelled: **static reading** (what
the game's code was read to do, in our own words), **measured** (with the
session that measured it), **source reading** (ForgePact's own code) or **not
established**. Nothing of this design has been seen live yet.

- **The list's scope (static reading).** The roll reads its unique list as a
  variable of an object-scoped reference whose constant names object index
  984, which the SDK calls `Controller_obj`
  (`HeroSiege::Objects::GameObject::Controller_obj`). In GameMaker terms the
  roll reads that variable off the first active `Controller_obj` instance.
  This is why both earlier negatives came back empty: session 1's
  instance-variable check on `Loot_Manager_obj` and Session 2's `list-scope`
  both asked the global scope or `Loot_Manager_obj`, and neither is the scope
  the roll reads. They asked the wrong scopes; they are not evidence about the
  list. Not measured yet: Live procedure 3's `list-scope` check is the first
  look at that instance.
- **The variable's name (not established statically).** The slot the roll
  loads is filled at startup in a shape the slot-name recovery script
  `FindSlotNames` does not match (it recovered 23,971 name pairs, none of them
  this slot). The three follow-up searches run in planning came back empty
  too: `SlotRefs` found no site that stores to the slot or takes its address,
  `FindPointers` no initialised pointer to it, and `FindRvaTable` no table
  entry for it, 0 hits each for the four slot globals involved. With the
  scripts at hand the static search for the name is exhausted. So the name is
  read live: the first `Controller_obj` instance's variable names, filtered by
  shape (an array of at least 100 entries, each an array of three numbers).
  Live 1 found that shape wrong (the variable it refused has six elements,
  not a flat run of triples), and `## Session 4: the list layout (issue #74)`
  has the layout the roll actually reads.
  The player build resolves the list by the name Live procedure 3 measures,
  and refuses (gate off, one log line naming the variable) when that variable
  is missing or not of that shape. Until then the research build finds it by
  shape through the probe and takes the name through a lever.
- **Who else reads the list (static reading).** The same slot is loaded by
  `DropUniqueItems`, `DropItemHeroic`, `DropItemDebug`, the roll's caller
  `DropItem`, other drop routines around `DropExclusive`,
  `PopulateTravelingMerchantGrid`, `PopulateBlackMarketGrid`,
  `ReturnRandomSatanic`, `CreateShrineEffect`, `DoCraftResult` and several
  unnamed object events. None of them is the roll. That is why the injection
  is scoped to the roll call: an entry left in the list between rolls would be
  seen by merchants, shrines, crafting and the other drop routines too. **Not
  established:** who builds the list, and whether it is rebuilt per zone or
  per load; its writers are among the unnamed object events, whose bodies
  were not read. The scoped design does not depend on it, because a rebuilt
  list is simply what the next roll injects into. Live procedure 3 measures
  the list's length at load, after a zone change and at the end.
- **The repository's shape and bounds (static reading).** A list entry is
  three numbers, type, sub and b. `GetUniqueRepoStruct` looks the definition
  up in a global three-level array indexed by type, then sub, then b (type 3
  takes a separate branch; the global's own slot name is also unresolved),
  using GameMaker's own bounds checks. An entry whose indices fall outside the
  array therefore raises the runtime's array error rather than missing
  quietly, so **an injected entry must be a triple that resolves**. ForgePact's
  own pool table `kAngelicBases` is exactly such triples (source reading):
  Liquor Holster is type 8, sub 0, b 51 and Lucifer's Crown type 0, sub 0,
  b 85, where sub 0 is the unique repository and b the unique's own index.
  The picker's filters are Session 2's: a definition flagged hidden, or whose
  rarity is neither Angelic (7) nor Unholy (10), is skipped and the pick runs
  again; the die is then rolled against the picked definition's rate.
- **What the placement chain does not read (static reading).**
  `CreateDefaultParams` builds the parameter struct from its three arguments
  and the result of one builtin that takes none; it reads no repository. The
  roll then hands that struct straight to `LootGroundCreate`, and neither it
  nor its callees `CreateLootInFreePos` and `LootGroundInit` refer to
  `GetUniqueRepoStruct` or the repository global at all. After the pick, then,
  the parameter struct is the only thing that says which item is built.
- **`CreateItemNew` (not established).** The game's own item constructor is
  missing from the Ghidra import, so whether it looks a unique up by its c and
  b fields was not read; it must, for c = 1. The built item carries the
  parameters as its `itemDefinitionStruct` with the fields w, j, b, a and c
  (hub `docs/RUNTIME_DATA_MODELS.md` § 13.4): c = 1 selects the unique
  repository and b the unique; c = 0 selects the normal repository, b the base
  item and a the seed or affix id. **Headhunter and Tyrant's Crown are c = 0
  items** (source reading of `SpawnSignatureItem`: the belt is a 777002, b 2,
  the crown a 777001, b 7), and an item built from those values through the
  game's constructor comes out dressed - **measured** with `sigdrop`, 30 of 30
  and 17 of 17 on 2026-09-18, and again in Session 2's `sigdrop-still-forces`.
- **How the forge hook recognises them (source reading).**
  `CustomForgeMatches` compares every field of a selector: t against the
  created item's `itemType`, and a, b, c and j against its
  `itemDefinitionStruct`. The two built-in entries are Headhunter (t 8,
  a 777002, b 2, c 0, j 0, keeping its native behaviour) and Tyrant's Crown
  (t 0, a 777001, b 7, c 0, j 0). The hook sits on `CreateItemNew`, installed
  by `InstallCustomForgeItemHooks` through `HookOneScript`, so it has the
  inline detour. An item the game builds from a 777002, b 2, c 0, j 0 under
  item type 8 is therefore recognised exactly as a `sigdrop` belt is. A
  game-built one has not been observed yet.
- **Why a stand-in (static reading).** The roll passes the picked entry's
  type to the placement itself, not through the parameter struct, so the
  entry we add must already carry the mod item's type. And the picker needs a
  definition its filters accept and a rate to roll against. So each mod item
  enters the list as a **stand-in**: a real Angelic unique of the same type,
  taken from `kAngelicBases` by name and validated by ForgePact's own pool
  build. Headhunter's stand-in is Liquor Holster (the owner's choice in #74).
  Tyrant's Crown needs a helmet (type 0); which one is the owner's call, and
  until they make it the plugin takes the validated type 0 entry with the
  lowest `droprate.base` and names it in the switch-on log line. The
  stand-in's rate becomes the item's.
- **Where a hit's type can be seen (measured and static reading).** A list
  entry is the whole triple, and b is an index within a type, so a sub and b
  pair alone names no entry: sub 0, b 51 is Liquor Holster under type 8 and
  also Supreme Elemelon under type 10. The parameter struct carries only sub
  and b, and the type reaches `LootGroundCreate` as a separate argument. The
  roll's own definition reads carry all three: `GetUniqueRepoStruct` takes
  type, sub and b, and **measured** in Session 1 it is called from inside the
  roll (1409 of 1708 calls in case A, about eight per roll, on its own
  detour). That the last of those reads before a hit is the picked entry's
  follows from the order the roll was read to run in (pick, read the
  definition, skip and pick again on the filters, then roll the die against
  the definition just read, then build the parameters): **static reading,
  not measured**. The plugin types each hit from that read (below), and only
  when the read's sub and b are the parameters' own; a read that does not
  agree leaves the hit untyped rather than mistyped. Live procedure 3's
  `typing` check is the first measurement of that order.

What #74 builds on this reading (the design as the plugin implements it; none
of it measured yet):

- **Scoped injection.** When a switch is on and the list resolved, the
  Angelic roll hook pushes each enabled item's stand-in entry onto the game's
  list before its first call to the original roll and removes the entries
  after the last one, ForgePact's extra rolls included, under a scope guard so
  a throw removes them too. How many copies of its stand-in each enabled item
  pushes per roll is one number: the constant 1 in the player build, and in
  the research build whatever the lever `angelicprobe inject copies <k>`
  (1 to 400, default 1) set. The hook remembers the length before the push
  and removes only when the tail still holds exactly the pushed entries,
  every copy, in push order; otherwise it leaves the list as found, logs one
  line saying the list changed during the roll, and counts an anomaly
  (`anomalies=`). Between rolls the list is byte for byte vanilla: nothing is
  saved with it, the other readers above never see the entries, and
  switching off needs no cleanup. `injected=` counts the entries pushed whose
  push read back (next bullet). It says nothing about whether the roll's
  picker draws them: a push onto an array the roll does not read, or onto a
  `Controller_obj` instance the roll does not read, counts exactly the same.
  Only a change in what the picker draws shows that (`reach`, below).
- **The held read-back.** Right after the push the plugin reads the variable
  again by name off the first `Controller_obj` instance - a fresh read, never
  the handle it just pushed onto - and checks that its length is the length
  before plus the number pushed and that its tail holds the pushed triples.
  It reads only the length and the pushed tail, never the full walk the
  list's resolution does, so it stays light on every roll. When the fresh
  read does not show the push (the runtime handed back a copy, or the name
  now holds another array), the plugin takes the entries off the handle it
  pushed onto (only while that handle's tail is still ours), logs
  `inject: push not visible through Controller_obj.<name>` once until a push
  reads back again, counts it in `anomalies=` (and, research build,
  `heldMiss=`), and the roll carries nothing: no hit in it is attributed.
  The read-back catches a copy. It cannot show that the roll reads that
  variable at all; that is `reach`, measured live.
- **Typing the hit.** A third hook, on `GetUniqueRepoStruct`, is installed by
  its SDK name beside the other two, from the same installer. It always calls
  through and, only while the roll is in progress, records the type, sub and
  b it was asked for (three numbers; anything else clears the record); the
  record is cleared before each call to the original roll. At a hit the
  `CreateDefaultParams` detour takes the call's own sub and b and the latest
  record. When the record's sub and b equal the call's, the hit is typed: its
  entry is the record's type with that sub and b, the whole triple.
  Otherwise the hit is **untyped**: never attributed, never rewritten, left
  to the game, counted in `untyped=` (both builds), and its `angelic hit:`
  line says `untyped`. That the roll reads definitions inside the roll is
  measured (Session 1); that the latest read before the parameters is the
  picked entry's is the static reading above, **not established** until
  Live procedure 3's `typing`. In the research build the probe's
  `unique-repo` row (`angelicprobe on`) and this hook never both detour the
  script: whichever comes second refuses and says so. The typing hook then
  reports its route as `held-by-angelicprobe` and the gate stays off; the row
  reports itself blocked and names the typing hook's counters as the
  measure.
- **Attribution on the whole triple, one entry's share.** A typed hit whose
  type, sub and b equal the stand-in triple of an item this roll pushed is a
  candidate; a same-pair entry of another type (Supreme Elemelon's, type 10)
  is no candidate at all. The picker cannot tell our entries from the
  vanilla stand-in's, because they are the same triple: m enabled items
  sharing one stand-in with k copies each hold m·k of its n + m·k entries,
  where n is how many times the vanilla list holds that same whole triple
  (counted when the list resolves, whenever its length changes and whenever
  the stand-ins change; 1 when the stand-in is listed once). So a candidate
  is ours with probability m·k / (n + m·k), shared among the items by their
  copies: 1 in n + 1, one entry's share, for one item in the player build.
  The stand-in keeps its own share and our item gets one entry's, the same
  as every other entry. `ourHits=` counts ours. The coin is the plugin's
  own, so it fires whether or not a pushed entry was ever drawn: on a list
  the picker never saw, a vanilla Liquor Holster hit still becomes ours one
  time in n + 1 (and almost every time under `copies 200`), and `ourHits=`,
  `built=` and the Headhunter on the ground look the same as on a working
  injection. `ourHits=` is therefore evidence of the rewrite, never of the
  injection.
- **What "Liquor Holster's share" means now.** In Session 2 the share was
  one pool entry of ForgePact's own validated pool, k / (47 + k), rolled on
  top of the game's hit. Now it is literal: in the player build Headhunter is
  one more entry in the game's own list, carrying Liquor Holster's
  definition and rate, so it is picked as often as one Liquor Holster entry
  is, and its hit replaces the item that entry would have dropped.
- **The game builds our item.** On our hit the `CreateDefaultParams` detour
  calls the original, then rewrites the returned struct's a, b, c and j to
  the mod item's own values and reads them back. A missing field is a
  refusal: the struct stays vanilla and its JSON is logged. The game's own
  placement then builds a Heavy Belt or Great Helm with that seed, the forge
  hook dresses it, and the game places it where the roll said. No
  `SpawnSignatureItem` runs on this path. `built=`, `crown=` and `belt=`
  count what the game built. Each hit logs one `angelic hit:` line naming the
  typed entry (`picked <type>/<sub>/<b>`, or `picked untyped <sub>/<b>`) and
  where it went: untyped and left to the game; vanilla (with the stand-in
  and the odds, `ours <m·k> in <n + m·k>, not this time`, when the entry was
  a stand-in's); or our item with its stand-in and the odds, ending
  `built by the game`, `handed to the game, not seen built during the roll`
  or `refused`. Whether the game builds our item from a rewritten struct is
  **not established** until Live procedure 3's `inject-build`.
- **The research controls, and what each can show.** Three counters exist
  in the research build only, read off `angelicprobe inject status`:
  - `standinPicks=` counts hits inside the roll whose `CreateDefaultParams`
    sub and b equal a validated stand-in's, whether or not anything was
    pushed and whatever the typing said. It is keyed on the pair on purpose,
    so the reach reading does not rest on the typing. Its growth over the
    growth of `gameHits=` with both switches off is the pair's vanilla share
    p0; the same ratio with Headhunter on under `copies 200` is p1. A push
    the picker draws moves the stand-in's share from about 0.02 to about 0.8
    at about 47 entries passing the filter (about 0.4 at 300); a push it
    cannot see leaves p1 at p0. Read alone it cannot tell the stand-in from
    another unique sharing its pair, which is why it is read only against
    its own baseline in the same session.
  - `heldMiss=` counts pushes the held read-back did not see. Zero means no
    copy was caught; it does not show reach.
  - `typeAgree=` and `typeDisagree=`: on a typed hit that was not
    rewritten, the first item the Custom Forge hook's final pass sees built
    while the roll is in progress is the game's own pick, so its `itemType`
    is compared with the type the record gave the hit. The comparison is
    independent of the record, because it is read off the built item.
    Rewritten hits are not compared. Each hit line carries `builtType=`
    either way (source reading: the built-in signature entries make the
    forge hook install its `CreateItemNew` detour in every session, which
    `built=` rests on too). Both see only an item built **while the roll is
    still in progress**: a hit whose item is built later, in some other
    event, leaves `builtType=?` on its line and moves neither counter nor
    `built=`. That the build happens inside the roll is not established
    (Session 1 saw no placement there, and `CreateItemNew` is missing from
    the Ghidra import), so a vanilla hit line's `builtType=` is the
    comparator's own positive control, read before `typing` is judged.

  Each `angelic hit:` line in this build also carries `lootDelta=`
  (ground-loot instances after the original call minus before) and, for the
  first three hits of ours, the parameter struct's JSON before and after the
  rewrite. `lootDelta=` has the same blind spot as `builtType=`: it counts
  only placement that happens while the roll is still in progress, and
  Session 1 saw none there. An item placed later, in its own event, leaves
  `lootDelta=0` on every hit, vanilla or ours, so a vanilla hit line's
  `lootDelta=` is that counter's own positive control, read in the same
  check as `builtType=`'s.
- **Gate.** The item's panel switch, a validated stand-in, the list resolved
  by name (`list=` names it and its length), and all three hooks,
  `CreateDefaultParams`, `DropItemAngelicChance` and `GetUniqueRepoStruct`,
  inline detours: a table-only route cannot be shown to see the roll's
  direct calls, so a hit would be invisible or untypable. `detect=` reads
  `detoured` only when all three are; otherwise it names the first that is
  not (`CreateDefaultParams`'s route alone, the other two as
  `<script>:<route>`). Else the gate stays off and the switch-on logs what is
  missing. Both switches off: no entry pushed, no rewrite, the roll
  untouched, and with neither ever on no hook installed. Forging an item
  turns its mechanic on, never the drop.

Alternatives set aside:

- **Registering our own definition in the game's unique repository**, so the
  picker has a real definition for our item and no stand-in is needed. It
  would mutate the global three-level array every other item routine indexes
  (merchants, the codex, `GetUniqueRandomItemID`, the `DefineItemUnique*`
  scripts); the built item would come out as a c = 1 unique and still need
  relabelling to c = 0 for the forge hook; and the constructor's argument list
  is unresolved. It is the only way to remove the one-in-n+1 coin, and it is
  the research route if injection is not observed.
- **Attributing on the `CreateDefaultParams` pair alone** (the plugin before
  this design was settled). The pair is a strict part of the entry's
  identity, n is counted over the whole triple, and Liquor Holster's 0/51 is
  also Supreme Elemelon's under type 10: a hit on that entry read as the
  stand-in's would rewrite a struct for the wrong type and build a malformed
  item.
- **Refusing a stand-in whose pair another validated unique shares** (the
  same earlier plugin's `ambiguous:` refusal). It covered ForgePact's own
  validated pool only, not the game's list, and still left the hit itself
  untyped. Typing the hit from the roll's own definition read replaces it.
- **Judging reach from one pushed entry's share.** One entry moves the
  stand-in's pair from about 1 hit in 47 to about 2 in 48, a difference no
  session's kill count resolves. `copies 200` moves the stand-in's share from
  about 0.02 to about 0.8 at 47 passing entries (0.4 at 300), so 20 hits
  separate a push the picker draws from one it cannot see.
- **A stand-in no vanilla entry shares**, so every hit on it is ours. Not
  available: the list appears to hold every unique (about one entry in eight
  passes the Angelic/Unholy filter), and a triple outside the repository's
  bounds raises the array error. The design already handles n = 0 without a
  coin.
- **Persistent membership**, pushing on switch-on and removing on switch-off.
  The same list feeds merchants, shrines, crafting and the other drop
  routines; a rebuilt list would drop or duplicate the entries; and a save
  mid-session would have to be shown never to write the list. The scoped form
  meets "off removes it, never left behind" and "all off is vanilla" by
  construction.
- **Rewriting the picked entry's type.** Not possible: the type comes from the
  list entry, not from the parameter struct.
- **Typing the hit at `LootGroundCreate`**, which receives the type and the
  parameter struct together. The player build holds `LootGroundCreate`
  through the deliberately table-only inspection installer, so this needs a
  second, inline route on a function those hooks already own, and which
  struct field carries sub is not established. It is the research step if
  `typing` fails.
- **Suppressing the game's placement and spawning ours beside it** (the
  fallback as the owner first worded it). No name-resolved way to make
  `LootGroundCreate` place nothing was found: the player build's hook on it is
  table-only and blind to the roll's direct call, and an undefined parameter
  struct would reach `CreateItemNew`. The fallback that can be measured is
  **replace by removal**: let the game place the stand-in, destroy the one
  `Loot_Ground_obj` instance the roll created (present after the original
  call and not before), and spawn ours at the roll's position through
  `SpawnSignatureItem`. Its risk is a loot registry still listing the destroyed
  instance; it is measured only if injection fails, with pickups of other
  items as the control.
- **Detecting the hit any other way** (counting ground loot around the call,
  hooking `LootGroundCreate` natively, a rate-weighted share): set aside in
  Session 2's planning for reasons that still hold.
- **`name auto` as the spelling of the scan.** An earlier draft of the
  procedure spelled it that way, which would have set the list's name to the
  literal `auto`. The scan is `angelicprobe inject auto`, and the `name`
  lever refuses the value `auto` with a line naming the right spelling, so a
  slip can never set that name.

The decision between injection and replacement is mechanical, from Live
procedure 3's verdicts, taken in this order:

1. A session-validity check (`dll-hash`, `marker`, `control`, `force-hit`)
   not pass: no route. The session is run again on the same build.
2. `typing` fail or not-observed: **route not-observed**. The hit cannot be
   typed, so nothing ships attributed. The research step is typing at
   `LootGroundCreate` (above), and the owner decides it. This step is never
   taken on `typing` not-run (instrument-blind: no build seen inside the
   roll), which reads as "typing's comparator could not see the build", not
   "the hit cannot be typed": with `untyped=0` every hit was typed, and only
   the check of that typing against the built item is missing. That evidence
   does not prescribe typing at `LootGroundCreate`. The steps below are still
   taken, with `inject-build` judged in its blind form; steps 4 and 5 need
   `typing` pass, so such a session ends at step 6 at most, with
   `inject-build`'s verdict and `untyped=` recorded for the owner, and the
   research step it names is a comparator placed where the item is built.
3. `reach` fail or not-observed, after every candidate list `list-scope`
   printed was tried: **route not-observed**. The roll does not read what the
   plugin pushes onto; `inject-build` is recorded as not-run (instrument-blind:
   reach did not pass), never as fail.
4. `typing` pass, `reach` pass, `inject-build` pass: **route inject**. The
   design above ships; the replace mode stays research-only. The player
   build's one-entry share is arithmetic on a reach measured at
   `copies 200`, and the record says so.
5. `typing` pass, `reach` pass, `inject-build` fail, `replace-remove` pass:
   **route replace**. On a hit of ours the game places the stand-in, the
   plugin removes it and spawns ours.
6. Anything else (`inject-build` not-observed; `inject-build` fail and
   `replace-remove` not pass): **route not-observed**. The owner decides;
   registering a definition in the repository is the research route left.

Not-observed never selects the replace route: replace needs `reach` pass and
`inject-build` fail. Not-observed means the batch reached its cap without the
hits a verdict needs, or the instrument a check rests on did not pass; it is
never evidence that injection does not build the item.

`typing`, `reach`, `inject-build`, `on-both` and `replace-remove` are never
pass conditions of the session: their verdict is the finding, and `on-both`
and `replace-remove` are not run on some routes. `off-removes`,
`list-restored`, `baseline-off-vanilla` and `sigdrop-still-forces` check
behaviour that ships, and must pass whichever route is taken.

### Live procedure 3

Session 3 runs the research build through the drive tool, with the owner
doing the killing. This section is the whole procedure; Session 3's record
goes under `### Results`.

- **Build.** `plugin_build\BloodPactPlugin_rel.dll`, built with
  `plugin_build\build.bat dev` (the literal `dev`; anything else is the
  player build, which has none of the levers below). Its SHA-256 is recorded
  when it is built. The owner installs it when asked; until then `dll-hash`
  fails and nothing else runs.
- **Character.** Save slot 14 (Sorak), selected on the back end, in town at
  load.
- **Standing steps.** Take the drive tool's lease, run its self-check, take
  an independent copy of the saves and the tool's own backup, launch, select
  the character. At the end: stop the game normally, inspect the saves,
  restore the backup, release the lease.
- **Hygiene.** A fresh session; no `citrace nativetrace`, `raredrop ceiling`,
  `scount`, `zonegenlog` or `angelicwatch`; and no `angelicprobe on`, whose
  `default-params` and `unique-repo` rows touch the very hooks under test.
- **People steps.** One zone change (`list-stable`), then kill batches of ten
  ordinary monsters with the drops left on the ground. The operator reads the
  counters between batches and stops a step as soon as its hit count is
  reached. Counts below are **hits** (growth of `gameHits=`), never kills;
  with the chance lever about 44 of 47 rolls hit (Session 2).

The research build's levers stay under `angelicprobe`, because the command
dispatcher's branch count and the `angelicprobe` literal are both pinned by
`test_angelic_probe_contract.py`:

- `angelicprobe list` first gives one line each for the two scopes Session 2
  asked (the global scope and `Loot_Manager_obj`, both expected empty). Then
  it finds `Controller_obj` by name and prints its instance count and
  `names=<count>` for the first instance: the positive control on the
  instance read, where 0 means the read failed, not that no list exists. It
  prints one `candidate` line per variable shaped as an array of
  three-number arrays (its name, length, first three entries and each
  stand-in's n), one `rejected` line per other array with its length and why
  the shape failed, and the remaining variables counted by kind. The last
  line is `angelicprobe list: candidates=<k> best=<name>:<len>`. The instance
  may come back as a reference or as an object, and the numbers as reals or
  as 32- or 64-bit integers; none of those makes the read empty.
- `angelicprobe inject` takes `auto` (the scan's best candidate),
  `name <var>` (a variable by name; the value `auto` is refused, the scan is
  `angelicprobe inject auto`), `copies <k>` (1 to 400, default 1: how many
  copies of its stand-in each enabled item pushes per roll), `mode` (`inject`,
  the default, or `replace`, the fallback) and `status`. Any other word
  prints the usage line:

  `angelicprobe inject: name <var> | auto | copies <k> | mode inject|replace | status`

  `status` prints `angelicprobe inject: mode=<m> copies=<k>`, then
  `list=<name>:<len>` (or `list=none`), each item's stand-in with its n, then
  `injected=`, `ourHits=`, `untyped=`, `standinPicks=`, `heldMiss=`,
  `typeAgree=`, `typeDisagree=`, `built=`, `removed=` and `anomalies=`.
- `angelicprobe hit` keeps its chance, rate, off and status levers; its
  status line ends `detect=<route>`. Each `angelic hit:` line
  (`picked <type>/<sub>/<b>` or `picked untyped <sub>/<b>`, then the
  position, the gate and the outcome) carries `lootDelta=` and `builtType=`
  in this build, and the struct's JSON for the first three hits of ours.
- `sigdrop status`, in both builds, on a fresh session reads exactly:

  `sigdrop: force off | rolls=0 drops=0 fails=0 | game roll: gameRolls=0 gameHits=0 injected=0 ourHits=0 untyped=0 built=0 crown=0 belt=0 anomalies=0 list=none gate=tyrant:off,headhunter:off cdpCalls=0 detect=off`

  `list=` is `none` until a switch resolves the list, then its name and
  length, or `missing` after a failed resolution.

Each check is recorded as pass, fail, not-observed or not-run, with the
replies quoted in the session record. The research checks are never pass
conditions of the session (the decision rule above):

1. **`dll-hash`** - the lease's DLL SHA-256 equals the research build's,
   recorded when it was built. Fail: nothing after it runs.
2. **`marker`** - `angelicprobe hit status` answers with a line beginning
   `angelicprobe hit:` and ending `detect=off`. A player build answers that
   the command is unavailable, and the session ends there.
3. **`control`** - `sigdrop status` reads exactly the fresh-session line
   above.
4. **`list-scope`** (research) - `angelicprobe list`: one `Controller_obj`
   instance, `names=` above 0, `candidates=` at least 1, and `best=<name>:<len>`
   with a length of 100 or more, each stand-in's n expected 1. Every
   `candidate` line and every `rejected` line is recorded as printed.
   `names=0` is fail (the instance read is broken); `candidates=0` with
   `names=` above 0 is not-observed, and every variable line is recorded.
   Either way checks 8 to 10 are not-run.
5. **`repo-standin`** (research) - `angelicprobe inject auto`, then
   `angelicprobe inject status`: `copies=1`, `list=` naming the best
   candidate and its length, and the stand-ins as Headhunter with Liquor
   Holster (n = 1) and Tyrant's Crown with the chosen helmet (n = 1). A
   stand-in printed as not validated cannot arm; which one is recorded. This
   shows only that `auto` chose a list of the right shape; whether the roll
   reads it is `reach`.
6. **`list-stable`** (research) - **the owner takes the town portal or a
   waypoint to any ordinary zone.** `angelicprobe list` again names the same
   best variable with the same length. A different length is the finding, and
   both are recorded.
7. **`force-hit`** (validity), **`baseline-off-vanilla`** and **`typing`**
   (research), both switches off. `raredrop angelic 2` answers
   `angelic: gate OPEN`; `angelicprobe hit chance 1000000000` turns the
   override on, with `detect=detoured`. **The owner kills in batches of
   ten** until `gameHits=` is at least 20, three batches at most.
   - `force-hit`: `cdpCalls=` above 0, `detect=detoured` and `gameHits=` at
     least 20 pass; anything else fails, and checks 8 to 10 are not-run.
   - `baseline-off-vanilla`: `injected=0`, `ourHits=0`, `built=0`, `list=`
     with `list-scope`'s length (or `none` or `missing` when `list-scope` did
     not pass) and no `inject:` line pass. The growth of `standinPicks=` over
     the growth of `gameHits=` is recorded as the baseline share p0. A
     screenshot of the ground is taken.
   - `typing`: first its instrument's positive control. The `builtType=`
     field of every vanilla hit line is recorded. If none carries a number
     (every one reads `builtType=?`), the forge hook's final pass saw no item
     built inside the roll, so `typeAgree=` and `typeDisagree=` could not
     grow: `typing` is `not-run (instrument-blind: no build seen inside the
     roll)`, never not-observed, and `untyped=` is recorded beside it. Only
     when at least one vanilla hit line carries a number: `untyped=0`,
     `typeAgree=` at least 10 and `typeDisagree=0` pass. `untyped=` or
     `typeDisagree=` above 0 fails, and those hit lines are recorded. Fewer
     than 10 agreements is not-observed.
   - `lootDelta=` control (recorded; it decides no verdict of this check):
     the `lootDelta=` field of every vanilla hit line is recorded. If none
     reads 1, no placement was seen inside the roll either, and check 8
     judges no hit on `lootDelta=` (below).
8. **`reach`** (research) and **`inject-build`** (research; the route's
   input). `angelicprobe inject copies 200`, then `headhunter force`, which
   answers `headhunter: ON (forced)` and one `signature drops:` line naming
   the list and Liquor Holster; `sigdrop status` reads
   `gate=tyrant:off,headhunter:on`. **The owner kills in batches of ten**
   until `gameHits=` has grown by at least 20 since this check began, two
   batches at most; only when `reach` is still inconclusive, on to 40 hits,
   four batches at most.
   - `reach`, from p1, the growth of `standinPicks=` over the growth of
     `gameHits=` in this check: `heldMiss=0` and p1 of 0.20 or more pass;
     `heldMiss=` above 0, or p1 below 0.10, fail; p1 from 0.10 to below 0.20
     at 40 hits, or fewer than 20 hits at the cap, is not-observed. On a
     `reach` fail with `heldMiss=0` while `list-scope` printed another
     candidate, `angelicprobe inject name <next candidate>` and this check
     runs again, once per remaining candidate, two at most; each is
     recorded.
   - `inject-build` runs only when `reach` passed and `typing` passed or is
     not-run (instrument-blind: no build seen inside the roll); otherwise it
     is not-run (instrument-blind: whichever of the two did not pass). Pass:
     `ourHits=` grew by 3 or more, `built=` and `belt=` grew by the same
     amount, `crown=0`, `anomalies=0`, every hit line of ours ends
     `built by the game` with `lootDelta=1`, no `refused` line and no
     `sigdrop:` line, and **the owner hovers one item on the ground and names
     it**: Headhunter (with a screenshot). Fail: a crash, `built=` growing
     less than `ourHits=`, a `lootDelta=` other than 1 on a hit of ours, a
     refusal line (its JSON recorded), or an item that is not Headhunter.
     Not-observed: fewer than 3 hits of ours at 40 hits (about 32 are
     expected when `reach` passed). When `typing` was instrument-blind,
     `built=` and `belt=` rest on the same in-roll build and are blind too,
     as is the hit line's `built by the game` ending (a hit of ours whose
     build the hook did not see ends `handed to the game, not seen built
     during the roll`): their growth and endings are recorded but are no
     criterion, so `built=` growing less than `ourHits=` is not a fail, and
     the check judges on `lootDelta=1` on every hit of ours plus the owner's
     named Headhunter, with the other pass and fail criteria unchanged. When
     no vanilla hit line in check 7 read `lootDelta=1`, the `lootDelta=`
     criteria are blind the same way, in either form: each hit of ours'
     `lootDelta=` is recorded, and a `lootDelta=` of 0 on a hit of ours is
     not a fail. With both blind, the check judges only on `ourHits=`
     growing by 3 or more, `crown=0`, `anomalies=0`, no refusal and no
     `sigdrop:` line, and the owner's named Headhunter; a crash, a refusal
     line or an item that is not Headhunter still fails.
9. **`on-both`** (acceptance of the shipped pairing; not-run unless
   `inject-build` passed). `tyrant force`, copies still 200. **The owner
   kills in batches of ten** until `crown=` is at least 1, two batches at
   most. Pass: `crown=` at least 1, `belt=` grew, `built=` grew by the growth
   of `ourHits=`, and a Tyrant's Crown is seen on the ground (screenshot).
10. **`replace-remove`** (research fallback; run only when `typing` and
    `reach` passed and `inject-build` failed, else not-run).
    `angelicprobe inject mode replace`, `headhunter force`, copies 200.
    **The owner kills in batches of ten** until `ourHits=` has grown by 3 or
    more, two batches at most. Pass: every hit line of ours reads
    `removed stand-in, spawned Headhunter`, `removed=` grew by the growth of
    `ourHits=`, `lootDelta=1`, the owner picks up one other ground item
    without an error (screenshots before and after), and `lootcensus`
    answers. Then `angelicprobe inject mode inject`.
11. **`off-removes`** (acceptance) - `angelicprobe inject copies 1`,
    `headhunter off`, `tyrant off`; `sigdrop status` reads
    `gate=tyrant:off,headhunter:off` and `angelicprobe inject status` shows
    `list=` with `list-scope`'s length (or `none` when `list-scope` did not
    pass). **The owner kills five.** `injected=` and `ourHits=` unchanged, no
    mod item and no `inject:` line pass.
12. **`sigdrop-still-forces`** (acceptance) - `sigdrop crown`; **the owner
    kills one**; the log shows `sigdrop: Tyrant's Crown dropped at`; then
    `sigdrop off`. The last people step.
13. **`list-restored`** (acceptance) - `angelicprobe hit off`,
    `raredrop angelic 1`, then `angelicprobe list`: `best=` equal to
    `list-scope`'s (`none` both times counts) and each stand-in's n equal to
    `list-scope`'s.
14. Stop the game normally, then inspect and restore the saves (the standing
    steps).

### Results

Session 3 ran once, as Live 1, on 2026-10-02 (the drive tool's lease taken
at 16:36 UTC), on the research dll `f7560e80…d5be71` (SHA-256
f7560e804472fee6b9c7b61457e1c3b1713ba09a2bd5ec15226ffa5874d5be71, built with
`build.bat dev`), save slot 14 (Sorak), the kills in Outskirts of Inoya
(Hell, zone level 243). The drive tool's self-check passed 6 of 6; the saves
were copied before launch and restored at teardown with nothing left
changed. Every verdict below was measured in that session; the replies are
quoted in its capture, which stays with the hub's workorder and is not
tracked.

- **`dll-hash`: pass.** The lease's DLL hash was the research dll's.
- **`marker`: pass.** `angelicprobe hit status` answered a line beginning
  `angelicprobe hit:`, detection not yet installed, ending `detect=off`.
- **`control`: pass.** `sigdrop status` read exactly the fresh-session line
  of `### Live procedure 3`.
- **`list-scope`: not-observed.** The global scope and `Loot_Manager_obj`
  both answered that they carry no `lootListUnique`, as the static reading
  expected. One `Controller_obj` instance answered, with `names=221` (222
  after the zone change), so the instance read worked. Every array variable
  on it was refused, each with `why=entry 0 is not three numbers`; among them
  `lootListUnique` with `array_length=6` and `lootListNormal` with
  `array_length=5`, both refused by the flat shape this session's scan was
  built for. The scan ended `candidates=0 best=none`. The validated pool read
  in the same step: 47 candidates, 11 rejected.
- **`repo-standin`: not-observed.** `angelicprobe inject auto` answered that
  no `Controller_obj` variable was shaped like the list and changed nothing;
  the status line read `list=none`, both stand-ins with n = 0.
- **`list-stable`: not-observed.** After the zone change the scan read
  `names=222`, the same refused set (`lootListUnique` still 6,
  `lootListNormal` still 5; only `enemy_pool` changed, from 1 to 4 elements)
  and `candidates=0`: no best variable to compare.
- **`force-hit`: pass.** With `raredrop angelic 2` and the chance lever on,
  one batch of kills gave 57 hits over 59 rolls (`gameRolls=59
  gameHits=57`), `cdpCalls=70`, `detect=detoured`. The later batches brought
  it to 85 hits over 92 rolls (`cdpCalls=106`) and 87 over 94.
- **`baseline-off-vanilla`: pass.** Both switches off: `injected=0`,
  `ourHits=0`, `built=0`, `list=none` on the inject status line (and
  `list=missing` on `sigdrop status`, since the resolution had failed), no
  `inject:` line. `standinPicks=1`, so the baseline share p0 is 1/57: one
  vanilla hit fell on Liquor Holster's own entry.
- **`typing`: pass.** Its instrument's positive control first: all 57
  vanilla hit lines carried a `builtType=` number (none read `builtType=?`)
  and every one `lootDelta=1`, for example
  `angelic hit: picked 7/0/57 at 4176,3856 gate=tyrant:off,headhunter:off lootDelta=1 builtType=7 -> vanilla`;
  the first eight hits built types 3, 7 and 10, each equal to the picked
  entry's type. Then `untyped=0`, `typeAgree=57`, `typeDisagree=0`, and still
  no disagreement at 85 and 87 agreements in the later reads.
- **`reach` and `inject-build`: not-run**, because `list-scope` found no
  candidate and nothing was ever pushed. **`on-both`: not-run**
  (`inject-build` did not run). **`replace-remove`: not-run** (`reach` did not
  run).
- **`off-removes`: pass, as weak evidence.** Copies back to 1, both switches
  off, `gate=tyrant:off,headhunter:off`; another batch gave 28 more hits,
  every hit line ending `-> vanilla`, `injected=` and `ourHits=` still 0, no
  `inject:` line, and `list=none` as at `list-scope`. Nothing was injected in
  this session, so the removal path itself was not exercised.
- **`sigdrop-still-forces`: pass.** `sigdrop crown`, then one kill; the log
  read `sigdrop: Tyrant's Crown dropped at 3663,2896 kind=15` and
  `sigdrop: Tyrant's Crown dropped at 3842,2869 kind=15` (`rolls=2
  drops=2`), and the ground screenshot shows two Tyrant's Crowns.
- **`list-restored`: pass.** After `angelicprobe hit off` and
  `raredrop angelic 1` the scan read `best=none` again and both stand-ins
  n = 0 again, equal to `list-scope`.

**Route: not-observed.** The injection never had a list it would accept:
the shape check, written for one flat array of 100 or more triples, refused
the real `lootListUnique`, whose outer length is 6. So the session says
nothing about whether the roll reads a pushed entry, nor about inject against
replace. What it did establish: the hit is typed on every vanilla hit (57 of
57), the chance lever and the three detours work on this build, the forced
`sigdrop` path is unaffected, and a variable named `lootListUnique` lives on
`Controller_obj` with six elements, which the static search had not named.
`## Session 4: the list layout (issue #74)` reads that layout and gives the
procedure that measures it.

## Session 4: the list layout (issue #74)

Live 1 (Session 3's `### Results`) found on `Controller_obj` a variable
`lootListUnique` of outer length 6 and refused it, because Session 3 modelled
the list as one flat array of `[type, sub, b]` triples, 100 or more. On
2026-10-02 the owner chose to read the list's shape and teach the injection
it (#74), over keeping the beside design or researching a definition of our
own. The roll was read again locally in the named Ghidra project; this
session's static reading is below. `### What the plugin does with it` lists
the only changes to the design: how the list is resolved, shape-checked,
pushed onto, read back and cut. Everything else in Session 3 stands - the
typing, the attribution, the rewrite, the gate and the roll scope.
`### Live procedure 4` measures the layout before anything is pushed (Live
2), and `### Results` is where that session's record goes.

### Static reading (2026-10-02, the layout)

Each claim is labelled **static reading** (what the game's code was read to
do, in our own words), **measured** (with the session) or **not
established**. The decompiler's output stays on the researcher's machine and
nothing of it is quoted here.

- **The variable's name (static reading, confirmed by Live 1).** The runtime
  keeps, beside every variable-slot global and every builtin-pointer global,
  a pointer to that global's name. A new script, `DerefNear`, reads the
  memory beside a given global; on the slot the roll loads, the name record
  reads `lootListUnique`. Live 1 measured a variable of exactly that name on
  the first `Controller_obj` instance, with outer length 6. Session 3's
  "not established statically" was a limit of the scripts used then
  (`FindSlotNames`, `SlotRefs`, `FindPointers`, `FindRvaTable`): they look
  for stores into a slot and tables pointing at it, not for the name record
  beside it.
- **One element, by a constant index (static reading).** The roll's read of
  `Controller_obj.lootListUnique` passes an array index, the constant 5: it
  reads the sixth element, `[5]`, and never treats the outer array as a list.
  The same read helper is handed a "no index" marker for the caller's plain
  reads of `x` and `y`, which is how the index is told apart from them.
  `lootListNormal` is not read by this roll.
- **The element is a `ds_list`, and its entries are the triples (static
  reading).** On the value it read, the roll calls three builtins, named
  through the same name records: `ds_list_size` with the element as its one
  argument, then `ds_list_find_value` with the element and a random index
  drawn up to that size, then `is_array` on the value it got back. Only when
  `is_array` holds does it take that value's elements 0, 1 and 2 as the type,
  sub and b it hands to `GetUniqueRepoStruct`; otherwise it draws again.
  Everything after the read - the filters, the die, `CreateDefaultParams`,
  the placement - is as Session 3's static reading says.
- **The layout (static reading).** `Controller_obj.lootListUnique` is an
  array of length 6 (measured, Live 1) whose element 5 the Angelic roll uses
  as a `ds_list` (static reading; a ds container reads as `ref ds_list` on
  this runner, `docs/RUNTIME_DATA_MODELS.md` §5.4); what elements 0-4 hold is
  not established. Each entry of `lootListUnique[5]` is an array of three
  numbers, type, sub and b. Session 3's minimum length of 100, and its expectation that
  the list holds every unique, are withdrawn.
- **Not established.** What the six elements are keyed by, and what `[0]` to
  `[4]` hold. The size of `[5]`: the validated pool read 47 candidates and 11
  rejected (58 definitions), which is the order of magnitude to expect, not
  a prediction. Whether the random index can equal the size, an off-by-one
  the `is_array` guard would absorb as a re-draw, and which does not matter to
  the injection. Who builds the six lists, and when. Live 2 measures the
  first two.
- **Negative results.** `FindWrites` and `FindPointers`, run on the three
  builtin-pointer globals (`ds_list_size`, `ds_list_find_value`,
  `is_array`): 0 hits each. Those names could not have come from a store or a
  table search, only from the name records. Recorded so nobody runs them
  again.
- **What it changes, and why the read-back changes shape.** A `ds_list` id is
  a handle into the runtime's own store, so Session 3's worry that a fresh
  read hands back a copy cannot apply to the sub-list itself. The held
  read-back now checks that a fresh read of the outer array still holds the
  same id at the index, and that the sub-list's size and tail still hold the
  push.
- **The proven routes the plugin reuses (source reading).** Reading a
  variable off the first `Controller_obj` instance by name, as Session 3's
  scan already does (measured in Live 1: `names=221`). Gating a value on
  `ds_exists` with the list type, 2, not on an allow-list of kinds, then handing the
  `ds_list_*` builtins the value as it was read: the route the pet loot
  collector already takes on a pet's `lootList` (it takes `ToDouble` only to
  refuse a handle that is unreadable, non-finite or negative, asks
  `ds_exists` with the `RValue` it read, because a live ds handle can arrive
  as a reference, and passes that same `RValue` to `ds_list_clear`). A value
  `ToDouble` cannot convert is refused as `id unreadable` before `ds_exists`
  is asked; that is a failed conversion, not a rule about kinds. (Session 6
  adds one step in front of the conversion: a kind that can never be a
  handle is refused as `never a handle` without being converted, because
  converting one raises a runner error that the catch cannot take back:
  measured for an array, `REAL argument incorrect type array` in Live 3's
  capture, and for undefined, ForgePact #144; a source reading for a
  string, a struct and null, whose error text has not been captured.)
  Building an entry with
  `array_create` and `array_set`, as Session 3's push does. New on a list the
  game owns are `ds_list_add` and `ds_list_delete`; their first live use is
  Live 2 (`reach`, `off-removes`, `list-restored`).

### What the plugin does with it

The deltas, numbered as the plan numbers them. Both builds unless marked
research.

1. **Resolution.** The plugin reads `Controller_obj.<name>` as before, then
   requires, in order: an array whose `array_length` exceeds the index; the
   element at the index (read with `array_get`) a value that `ds_exists`
   confirms is a live `ds_list` (type 2, as the plugin's other list checks
   spell it). No handle kind is required first, and none is predicted: the kind of
   `[5]` is not established until Live 2's dump prints it, and a ds container
   can read as `ref ds_list` on this runner (`VALUE_REF`,
   `docs/RUNTIME_DATA_MODELS.md` §5.4), so a number-only gate such as
   `SigNumber` could refuse the real list. First, a value of a kind that can
   never be a handle (an array, a string, a struct, an undefined or a null) is
   refused before any conversion (Session 6, report#2); real, ref and every
   other kind go on. `ToDouble` is then taken only to refuse
   a value that is unreadable (the conversion throws), non-finite or
   negative. The element is passed to `ds_exists` and to every `ds_list_*` call
   as it was read, ref or real, and two ids are compared by handle value
   (`ToDouble`), never by kind. Then a `ds_list_size` of at least `kSigListMinSize`, 10 (the old
   minimum length of 100, `kSigListMinLength`, is gone); and every entry, read
   with `ds_list_find_value`, an array of exactly three numbers. One helper
   serves the shape check, the tail check and the scan. The player build's
   index is `kAngelicListIndex`, 5, from the static reading; the research
   build keeps it in a variable the `at` lever sets. A stand-in's n counts its
   whole triple over the sub-list only. Each refusal is one reason naming the
   step: `Controller_obj.<name> is not an array`,
   `has <L> elements, none at [<i>]`, `[<i>] array_get threw`,
   `[<i>] is not a ds_list (kind=<kind>, <step>)`,
   `[<i>] has <s> entries, fewer than <min>` and
   `[<i>] entry <k> is not three numbers`. In the `ds_list` refusal, `<kind>`
   names the kind the element was read as (`real`, `int32`, `int64`, `bool`,
   `string`, `struct`, `array`, `ptr`, `undefined`, `null` or `ref`, else
   `kind<N>`) and `<step>` the step that refused it: `never a handle`
   (array, string, struct, undefined or null, before any conversion),
   `id unreadable`, `id non-finite`, `id <value>` (a negative handle),
   `ds_exists threw` or `ds_exists false`. The kind is reported in every
   refusal, and only the five never-a-handle kinds are refused by it, so a
   refusal of a live reference reads as one. The held read-back compares two reads by
   handle value, whatever kind each arrived as, and the dump's `ds_list=`
   uses the same gate.
2. **Push and remove.** Per copy the entry is built as before and appended
   with `ds_list_add`. Removal runs only while the tail check, now over
   `ds_list_size` and `ds_list_find_value`, says the last pushed entries are
   exactly ours: `ds_list_delete` at the last index once per pushed entry,
   after which `ds_list_size` must equal the size before the push. Otherwise
   the list is left as found, with one line and `anomalies=` (the rule is
   unchanged). The roll-scoped record keeps the outer variable, the list id,
   the index and the size before.
3. **Held read-back.** A fresh `variable_instance_get` of the outer variable,
   then `array_get` at the index; the push counts as visible only when that
   element is the same id (equal handle values by `ToDouble`, whatever kind
   either read arrived as) and the tail holds. A different id, a non-list or a
   short tail is a miss (`anomalies=`, and `heldMiss=` in the research build):
   the entries come off the id they were pushed onto, under delta 2's tail
   rule, and the roll carries nothing.
4. **The scan, `angelicprobe list` (research).** It keeps the report on the
   earlier scopes and the per-kind count. A candidate is now a
   `Controller_obj` array variable whose element at the current index is a
   live `ds_list` of at least 10 entries, all three-number arrays. Its lines:

   `angelicprobe list: candidate <name> array_length=<L> at=<i> ds_list_size=<s> first=[t,s,b][t,s,b][t,s,b] standins=<as now>`

   `angelicprobe list: rejected <name> array_length=<L> why=<reason of delta 1>`

   `angelicprobe list: candidates=<k> best=<name>[<i>]:<s>`

   Best is `lootListUnique` when it is a candidate, else the candidate with
   the largest sub-list; `angelicprobe inject auto` takes best.
5. **The layout dump, `angelicprobe list dump [<var>]` (research, new).**
   Live 2's positive control on the layout. It reads `Controller_obj.<var>`
   (`lootListUnique` when no name is given) and prints its shape two levels
   down, with no injection and no shape rule. The first line is

   `angelicprobe list dump: Controller_obj.<var> kind=<kind> array_length=<L>`

   (or `kind=<kind>, not an array`, and the dump stops). Then one line per
   element, up to 32:

   `[<i>] kind=<kind> ds_list=yes:<size>|no triples=<count>/<size> first=[t,s,b][..][..] standins=Headhunter:Liquor Holster(n=<n>),Tyrant's Crown:<name>(n=<n>)`

   where `ds_list=` uses delta 1's kind-free gate (`ds_exists` on the element
   as read) and `kind=` prints the element's kind beside it, `triples` counts
   the entries that are three-number arrays, `first`
   shows up to three entries (an entry that is not a triple prints its kind,
   as `entry<k>=<kind>`), and n counts the stand-in's whole triple in that
   element. A `ds_list` is read up to 2000 entries. The last line is

   `angelicprobe list dump: <var> elements=<L> ds_lists=<k> triple-lists=<m>`

   and any throw ends the dump with
   `angelicprobe list dump: EXCEPTION at [<i>]`.
6. **The `at` lever (research).** `angelicprobe inject at <k>`, 0 to 31, sets
   the sub-list index, resolves the list again and prints the list line. The
   usage line is now exactly:

   `angelicprobe inject: name <var> | auto | at <k> | copies <k> | mode inject|replace | status`

   The `name` lever still refuses the value `auto` (the scan is
   `angelicprobe inject auto`), and `angelicprobe inject status` keeps its
   tokens: the index rides inside `list=`.
7. **The `list=` token, and the player build.** On both status lines
   (`sigdrop status` and `angelicprobe inject status`) `list=` now prints
   `<name>[<index>]:<size>`, for example `lootListUnique[5]:380`, and `none` or
   `missing` as before. The player build's `kAngelicListVar` stays empty
   until Live 2's `reach` proves a name, and `kAngelicListIndex` stays 5
   unless that session had to move it; with the name empty the gate refuses
   with `list=missing`. The hit coin, the rewrite, the typing hook, the
   three-detour gate and every counter are unchanged, and nothing a player
   sees changes.

### Live procedure 4

Live 2 runs the rebuilt research build through the drive tool, with the
owner doing the killing. It is `### Live procedure 3` with three changes: the
layout dump runs first, as the positive control on the layout, and replaces
`list-scope`; `list=` reads `<name>[<index>]:<size>`; and the `at` lever
replaces Live 1's "next candidate" fallback. It is a new session on a
restored save, so every starting value is the state before Live 1. The
session's capture is `forgepact-74-angelic-list-injection-live-2.md`, kept
with the hub's workorder and not tracked; its record goes under
`### Results` below.

- **Build.** `plugin_build\BloodPactPlugin_rel.dll` from
  `plugin_build\build.bat dev`, with the layout changes above; its SHA-256 is
  recorded when it is built. The owner installs it when asked; until then
  `dll-hash` fails and nothing else runs.
- **Character.** Save slot 14 (Sorak), selected on the back end, in town at
  load.
- **Standing steps** and **hygiene**: as `### Live procedure 3`.
- **People steps.** One zone change (`list-stable`), then kills with the
  drops left on the ground. Live 1 showed that one batch of ordinary kills in
  Outskirts of Inoya gives 50 or more hits under the chance lever, so each
  kill step is one batch, read after it; the operator asks for a second batch
  only when the step's hit count is not reached. Counts are hits, never
  kills.

Each check is recorded as pass, fail, not-observed or not-run, with the
replies quoted in the session record. The research checks are never pass
conditions of the session.

1. **`dll-hash`** - the lease's DLL SHA-256 equals the research build's.
   Fail: nothing after it runs.
2. **`marker`** - `angelicprobe hit status` answers a line beginning
   `angelicprobe hit:` and ending `detect=off`. A player build answers that
   the command is unavailable, and the session ends there.
3. **`control`** - `sigdrop status` reads exactly Session 3's fresh-session
   line (`list=none`, every counter 0, `detect=off`).
4. **`layout`** (research; the positive control on the layout, before
   anything is injected) - `angelicprobe list dump lootListUnique`: the first
   line reads `kind=array array_length=6`; six element lines follow; `[5]`
   reads `ds_list=yes:<size>` with `triples=<size>/<size>`, a size of 10 or
   more, and the stand-ins as Headhunter with Liquor Holster (n = 1) and
   Tyrant's Crown with Mask of the Celestial (n = 1). Then `angelicprobe list`:
   `candidates=` at least 1 and `best=lootListUnique[5]:<size>`. All six
   element lines are recorded verbatim (what `[0]` to `[4]` hold is a finding
   on its own), and `angelicprobe list dump lootListNormal` is recorded too,
   with no verdict. Pass: `[5]` a `ds_list` whose entries are all triples,
   size 10 or more, each stand-in at n = 1, best as expected. Fail: `[5]` not
   a `ds_list`, or holding an entry that is not a triple, or a stand-in at an
   n other than 1; the record names any other element that holds triples with
   both stand-ins at n = 1, which is check 8's `at` fallback. Not-observed:
   the first line is not `kind=array array_length=6` (recorded as printed).
5. **`repo-standin`** (research) - `angelicprobe inject auto`, then
   `angelicprobe inject status`: `copies=1`, `list=lootListUnique[5]:<size>`
   with check 4's size, and both stand-ins at n = 1. A stand-in printed as not
   validated cannot arm; which one is recorded.
6. **`list-stable`** (research) - **the owner takes the town portal or a
   waypoint to any ordinary zone.** `angelicprobe list dump lootListUnique`
   again shows the same six sizes. A different size is the finding, and both
   are recorded.
7. **`force-hit`** (validity), **`baseline-off-vanilla`** (acceptance) and
   **`typing`** (research), both switches off: exactly as Live procedure 3's
   check 7 (`raredrop angelic 2`, `angelicprobe hit chance 1000000000`, one
   batch of kills to `gameHits=` of 20 or more, the `builtType=` and
   `lootDelta=` controls), with `list=` compared against check 4's
   `lootListUnique[5]:<size>`; `sigdrop status` prints the same token. The
   baseline share p0, `standinPicks=` over `gameHits=`, is recorded.
8. **`reach`** (research) and **`inject-build`** (research; the route's
   input): exactly as Live procedure 3's check 8.
   `angelicprobe inject copies 200`, then `headhunter force`; one batch of
   kills, until `gameHits=` has grown by 20 or more. p1 is the growth of
   `standinPicks=` over the growth of `gameHits=`: `heldMiss=0` and p1 of 0.20
   or more pass; `heldMiss=` above 0, or p1 below 0.10, fail; p1 between, or
   too few hits, is not-observed. `inject-build` is judged only when `reach`
   passed, with the instrument-blind forms written there. **The fallback,
   replacing Live 1's "next candidate" rule:** on a `reach` fail with
   `heldMiss=0`, or on a `layout` fail with another element holding triples,
   `angelicprobe inject at <i>` for each other element check 4's dump showed
   holding triples with both stand-ins at n = 1 (two at most, lowest index
   first), `headhunter force` still on, one batch each. Each index's p1 is
   recorded; `reach` passes on the first index that passes, and the record
   names that index, which the player build and the curated record then take.
9. **`on-both`** (acceptance of the shipped pairing; not-run unless
   `inject-build` passed) - as Live procedure 3's check 9.
10. **`replace-remove`** (research fallback; run only when `typing` and
    `reach` passed and `inject-build` failed, else not-run) - as Live
    procedure 3's check 10.
11. **`off-removes`** (acceptance) - `angelicprobe inject copies 1`,
    `angelicprobe inject at 5` (when check 8 moved it), `headhunter off`,
    `tyrant off`; `sigdrop status` reads `gate=tyrant:off,headhunter:off`, and
    `angelicprobe inject status` shows `list=lootListUnique[5]:<size>` equal
    to check 4's. **The owner kills five.** `injected=` and `ourHits=`
    unchanged, no mod item and no `inject:` line pass.
12. **`sigdrop-still-forces`** (acceptance) - as Live procedure 3's check 12.
    The last people step.
13. **`list-restored`** (acceptance) - `angelicprobe hit off`,
    `raredrop angelic 1`, then `angelicprobe list dump lootListUnique`: all
    six sizes equal to check 4's and each stand-in's n equal to check 4's;
    `angelicprobe inject status` shows `list=` equal to check 4's.
14. Stop the game normally, then inspect and restore the saves (the standing
    steps).

The capture lists the checks in this order: `dll-hash`, `marker`, `control`,
`layout`, `repo-standin`, `list-stable`, `force-hit`, `baseline-off-vanilla`,
`typing`, `reach`, `inject-build`, `on-both`, `replace-remove`,
`off-removes`, `sigdrop-still-forces`, `list-restored`. The session must pass
`dll-hash`, `marker`, `control`, `force-hit`, `baseline-off-vanilla`,
`off-removes`, `sigdrop-still-forces` and `list-restored`. The route comes
from Session 3's decision rule, applied to these verdicts in the same order,
its validity step first.

### Results

Session 4 ran once, as Live 2, on 2026-10-02 (the drive tool's lease taken
at 18:47 UTC), on the research dll `e30981d5…e3bcbade` (SHA-256
e30981d596bbddbad1cf4569ae0b802b0ba50453e67624e8fc796352e3bcbade, built with
`build.bat dev` from ForgePact `eb46209`, tag `forgepact-74-live2-base`, and
kept as `plugin_build/live2/BloodPactPlugin_rel.dll`), save slot 14, plugin
banner v2.1.0, the kills in Outskirts of Inoya (Hell). The drive tool's
self-check passed 6 of 6; the saves were copied before launch and restored at
teardown with nothing left changed. Measured in that session, every verdict
below; the replies are quoted in its capture
(`forgepact-74-list-injection-live2-live-1.md`), which stays with the hub's
workorder and is not tracked. One deviation from the procedure's order: the
arming of step 7 was done before step 6's zone change, to batch the owner's
asks, so `list-stable`'s second dump was taken after the first kill batch
with both switches off.

- **`dll-hash`: pass.** The lease's DLL hash was the research dll's.
- **`marker`: pass.** `angelicprobe hit status` answered a line beginning
  `angelicprobe hit:`, detection not installed, ending `detect=off`.
- **`control`: pass.** `sigdrop status` read exactly the fresh-session line,
  ending `list=none gate=tyrant:off,headhunter:off cdpCalls=0 detect=off`.
- **`layout`: pass.** `angelicprobe list dump lootListUnique` printed
  `Controller_obj.lootListUnique kind=array array_length=6` and six element
  lines. All six elements are ref ds_lists whose every entry is a triple:
  `[0]` 50 entries, `[1]` 61, `[2]` 79, `[3]` 152, `[4]` 221 and `[5]` 380,
  each line reading `kind=ref ds_list=yes:<size> triples=<size>/<size>`. The
  sixth is the one the static reading named:
  `[5] kind=ref ds_list=yes:380 triples=380/380 first=[0,0,1][0,0,15][0,0,29]`,
  with the stand-ins `Headhunter:Liquor Holster(n=1)` and
  `Tyrant's Crown:Mask of the Celestial(n=1)`; elements 0-4 hold neither
  stand-in (n = 0 each), and their first entries are all `[0,0,<b>]` too. The
  summary read `elements=6 ds_lists=6 triple-lists=6` and the scan
  `candidates=1 best=lootListUnique[5]:380`. So element 5 arrives as a ref
  value, the kind-free gate accepted it, and the sub-list holds 380 entries.
  What elements 0-4 mean, and who builds the six, is still not established.
  `lootListNormal` on the same instance (not read by this roll) dumped as five
  ref ds_lists of 70, 70, 73, 76 and 74 triples.
- **`repo-standin`: pass.** `angelicprobe inject auto` answered
  `list lootListUnique[5]:380 stand-ins Headhunter:Liquor Holster(n=1),Tyrant's Crown:Mask of the Celestial(n=1)`,
  and the inject status line read `mode=inject copies=1
  list=lootListUnique[5]:380` with no `(not validated)`.
- **`list-stable`: pass.** In Outskirts of Inoya the dump read the same six
  sizes (50, 61, 79, 152, 221, 380) and the same stand-in counts.
- **`force-hit`: pass.** With `raredrop angelic 2` and the chance lever at
  1e9, one batch gave 46 hits over 49 rolls (`gameRolls=49 gameHits=46`),
  `cdpCalls=47`, `detect=detoured`.
- **`baseline-off-vanilla`: pass.** Both switches off over those 46 hits:
  `injected=0`, `ourHits=0`, `built=0`, `list=lootListUnique[5]:380`, no
  `inject:` line, every hit line `-> vanilla` with `lootDelta=1`.
  `standinPicks=2`, so the baseline share p0 is 2/46 (0.043).
- **`typing`: pass.** The positive control first: all 46 vanilla hit lines
  carried a `builtType=` number (none `?`), for example
  `angelic hit: picked 7/0/57 at 4176,3856 gate=tyrant:off,headhunter:off lootDelta=1 builtType=7 -> vanilla`.
  Then `untyped=0`, `typeAgree=46`, `typeDisagree=0`; at the end of the
  session `typeAgree=211` (215 hits by the last read), still
  `typeDisagree=0` and `untyped=0`.
- **`reach`: pass.** `angelicprobe inject copies 200`, `headhunter force`,
  `gate=tyrant:off,headhunter:on`. Over the batch the hits grew from 46 to 126
  (+80) and `standinPicks=` from 2 to 67 (+65), so p1 = 65/80 = 0.81 against
  p0 = 2/46, with `heldMiss=0` and `injected=22800` (200 entries on each of
  114 rolls). The roll's picker draws the entries the plugin pushes onto
  `lootListUnique[5]`. The `at` fallback was not needed.
- **`inject-build`: fail.** `ourHits=` grew from 0 to 65 while `built=` and
  `belt=` stayed 0, and the ground held Liquor Holsters. Every one of the 65
  our-hit lines read `angelic hit: picked 8/0/51 at <x,y> gate=tyrant:off,headhunter:on lootDelta=1 builtType=8 -> Headhunter (stand-in Liquor Holster, 200 in 201) refused (no field a), the game placed its own stand-in`,
  each beside the refusal
  `inject: Headhunter refused (no field a), the parameters left vanilla: {"b":51.0,"j":0.0,"c":1.0}`.
  What this measures is the plugin, not the game: the rewrite's first step
  asks whether `CreateDefaultParams`' returned struct has a field `a`, and the
  struct has exactly three fields, `b` (the unique's index, 51), `j` (the
  sub, 0) and `c` (1, the unique repository). There is no `a` to rewrite, so
  nothing was written and the game was never handed our parameters. Whether
  the game would build Headhunter from parameters that carry its `a` is not
  observed; where the built item's `a` comes from is Session 5's question.
- **`on-both`: not-run** (`inject-build` did not pass).
- **`replace-remove`: pass.** `angelicprobe inject mode replace`, copies
  200, Headhunter forced: `ourHits=` grew from 65 to 107 (+42) and `removed=`
  from 0 to 42 (+42), 42 lines
  `... -> Headhunter (stand-in Liquor Holster, 200 in 201) removed stand-in, spawned Headhunter`,
  every one `lootDelta=1`, each followed by
  `sigdrop: Headhunter dropped at <x,y> kind=15`; no refusal and no error in
  the log slice, `anomalies=0`, and `lootcensus` answered
  (`ground=205 hidden=25 invisible=157 coins=0 walked=205`). The ground
  screenshot after the batch shows many Headhunter labels. The pick-up part
  rests on the owner's report, not on an observation: "previously picked up
  and identified liqor holter, no problems" (gold and the log showed no
  pick-up).
- **`off-removes`: pass.** Copies back to 1, both switches off; another
  batch gave 30 more hits (181 to 211), every hit line `-> vanilla`,
  `injected=36400` and `ourHits=107` unchanged, `built=`, `belt=` and
  `removed=` unchanged, no `inject:` line, `list=lootListUnique[5]:380`
  equal to `layout`'s. This time the removal path was exercised: 36400
  entries had been pushed and cut before it.
- **`sigdrop-still-forces`: pass.** `sigdrop crown`, then more than one
  kill: three lines `sigdrop: Tyrant's Crown dropped at <x,y> kind=15`
  (`rolls=3`, `drops=` 42 to 45).
- **`list-restored`: pass.** After `angelicprobe hit off` and
  `raredrop angelic 1` the dump read the same six sizes, n = 0 for
  `[0]`-`[4]` and n = 1 for both stand-ins at `[5]`, and the inject status
  line `list=lootListUnique[5]:380`, all equal to `layout`'s.

Without a verdict: the runner's YYError summary read `total=1` before step 4
and `total=37` (`distinct=5`, `top=report#2 x30`) after the first kill batch,
and did not move again through the end of the session. That is 30 repeats of
one report over a batch of 49 rolls, with the chance lever on and both
switches off. Its cause is not established and its message text was not
recorded; Session 5's procedure records it.

**Route:** `route: pending (owner: one more round for inject)`. Session 3's
decision rule, step 5 (`typing` pass, `reach` pass, `inject-build` fail,
`replace-remove` pass), would have given `route: replace`. The owner set that
aside on 2026-10-02 ("One more round for inject"), because the inject-build
fail is the plugin's rewrite assuming a field the struct does not carry, not
the game refusing the item: replace stays the measured fallback (42 of 42),
research build only, and the next session re-runs inject-build once the
rewrite writes where the built item's `a` is set. What Live 2 established:
`lootListUnique[5]` is a ref ds_list of 380 triples that the roll draws from,
stable across a zone change and restored exactly after the switches go off;
the hit is typed on every hit; the pushed entries are drawn (p1 0.81 at
copies 200, against p0 0.043 with nothing pushed; the player build's share
with one copy is arithmetic on that, not measured); and
`CreateDefaultParams`' struct is `{b, j, c}`, with no field a.

## Session 5: the id on the built item (issue #74)

Live 2 (Session 4's `### Results`) refused all 65 of its own hits with
`no field a`: the plugin rewrote the struct `CreateDefaultParams` returns, and
that struct carries `b`, `j` and `c` only. On 2026-10-02 the owner chose one
more round for inject over shipping the replace fallback ("One more round for
inject"): find where the unique's id reaches the built item, move the rewrite
there, rebuild, and run Live 3. The chain from the roll to the built item was
read locally; this session's static reading is below.
`### What the plugin does with it` lists the changes. Everything else in
Sessions 3 and 4 stands - the list, the push and its removal, the typing, the
attribution and the gate. `### Live procedure 5` is Live 3, and
`### Results` is that session's record: every check passed, the game built
the item from the record rewritten at `CreateItemNew`'s entry, and the route
is `inject`. The predictions in the static reading below are labelled as
such; the results say which held.

### Static reading (2026-10-02, the built item's id)

Each claim is labelled **static reading** (what the game's code was read to
do, in our own words), **measured** (with the session), **source reading**
(ForgePact's own code) or **not established**. The chain was read in a
private read-only copy of the named Ghidra project; the decompiler's output
stays on the researcher's machine and none of it is quoted here. The bodies
read were `LootGroundCreate`, `LootGroundCreateFromItem`,
`CreateLootInFreePos`, `LootGroundInit`, the item-instance constructor
`s_ItemInstanceStruct`, and the item constructor, which the import left
unnamed: the registration body that binds names to scripts binds it to
`CreateItemNew`, and it is the one function `LootGroundCreate` calls directly
with the new item instance.

The variable slots these bodies use are not named by `FindSlotNames`, and no
store into them has the shape `FindWrites` matches: they are filled at
startup from a name table. In this build each such slot sits right after a
pointer to its own name, so every slot the bodies use was named by reading
that pointer (a small local script, kept beside the output). Every load and
store of the `a`, `b`, `c`, `j`, `w`, `itemDefinitionStruct` and `itemType`
slots in the chain was then listed.

1. **The roll calls `CreateDefaultParams`** with the sub, the unique's b and
   1, and it returns a new struct holding exactly `j`, `b` and `c` (static
   reading, Session 3; measured, Live 2: `{"b":51.0,"j":0.0,"c":1.0}`).
   There is no field a.
2. **The roll passes that struct, the record, to `LootGroundCreate`**, with
   the position and the picked entry's type (static reading). On both of its
   branches that build an item
   locally, `LootGroundCreate` stores a value of its own into the record's
   `a` before anything else uses the record, computed from a protected game
   variable (what that value is made of is out of scope). The store does not
   look at what was there: the only references to the `a` slot in
   `LootGroundCreate` are those two stores, so nothing reads or tests an `a`
   already on the record (static reading). Its third branch, taken when the
   game's online flag is set, hands the record to an online creation pool and
   builds nothing locally; that is not the offline path (static reading).
   `LootGroundCreate` never references `b`, `c`, `j` or `w` (static reading).
3. **`LootGroundCreate` creates the item instance with the record as its
   argument.** The instance constructor resets the instance's own fields
   (`itemType` among them, to 0) and keeps its argument as
   `itemDefinitionStruct`. For a struct that is a second reference to the
   same record, not a copy of its fields (static reading). So the built item's
   `itemDefinitionStruct` is the very struct `CreateDefaultParams` returned,
   with `LootGroundCreate`'s `a` on it.
4. **`LootGroundCreate` then sets the instance's `itemType` to its own type
   argument** (static reading). In the roll that is the picked entry's type,
   the stand-in's: 8 for Liquor Holster, 0 for Mask of the Celestial (source
   reading of the stand-in table). Nothing reads a type from the record.
5. **`LootGroundCreate` calls `CreateItemNew` on the instance, directly**
   (static reading; only an inline detour sees that call, which
   `HookOneScript` installs). `CreateItemNew` reads the instance's
   `itemType` and the definition's `b`, `c` and `j`: `c` chooses the unique
   or the normal repository, which is looked up by type, `j` and `b`. It reads
   the definition's `a` once and hands it to a one-argument helper ahead of
   the item's random rolls. That this `a` seeds those rolls agrees with
   ForgePact's gem mod, which swaps this `a` for a Mythic seed at this very
   point (source reading, `GemsBeforeCreate`); it is not resolved further.
   `CreateItemNew` stores into none of `a`, `b`, `c`, `j` or `w`; its only
   member stores are the item's info and stat structs (static reading).
6. **`CreateLootInFreePos`** only finds a free spot and creates the ground
   object. **`LootGroundInit`**, after `CreateItemNew`, sets the ground
   object's display, sound and filter state and reads the definition's `b`.
   Neither references `a`, `c`, `j` or `w` (static reading).
   **`LootGroundCreateFromItem`** (the `sigdrop` path) only hands an
   already-built item to the ground object. `sigdrop` builds its item through
   `InitItemFromJson`, never through `CreateItemNew` here, so its `a` 777002
   is not evidence about this chain (source reading of `SpawnSignatureItem`,
   static reading of the script).

`w` is referenced nowhere in this chain (static reading), so the constructor
does not need it. Whether a built definition carries `w` at all is not
established; Live 3's `built:` line records it.

**The rewrite point: `CreateItemNew`'s entry**, before the original runs, on
its argument's `itemDefinitionStruct` (the record itself), writing `a`
777002, `b` 2, `c` 0, `j` 0 for the belt and `a` 777001, `b` 7, `c` 0, `j` 0
for the crown, and no `w` (static reading). By then the record already
carries `a`, stored by `LootGroundCreate`, so the write overwrites rather than
creates.

- **An `a` written where `CreateDefaultParams` returns does not survive**
  (static reading): `LootGroundCreate` overwrites it before the item instance
  exists. `b`, `c` and `j` written there would survive, since nothing in the
  chain stores into them. So the old point was right for three of the four
  fields and wrong for `a`. Live 2 never tested even that, because it refused
  before writing anything.
- **An `a` written at `CreateItemNew`'s entry survives** to its own reads and
  to its return (static reading: nothing between the entry and the return
  stores into `a`, `b`, `c` or `j`). ForgePact already writes the
  definition's `a`, `b`, `c` and `j` at exactly this point for its gem mod
  (source reading: `GemsBeforeCreate`, in the pre-call slot of the existing
  `CreateItemNew` hook, the same hook whose return is the Custom Forge's
  final pass).
- **The forge's selector can match** (static reading): at `CreateItemNew`'s
  return the item's `itemType` is the stand-in's (8 or 0) and its definition
  reads the four written values, the `{t, a, b, c, j}` selector `built=`
  waits for. Not established until Live 3's `inject-build`: that the game
  builds a playable Headhunter from a `c` 0 record through this constructor,
  rather than through `InitItemFromJson`.
- **Second candidate**, if the first fails live: the instance constructor's
  entry, on its argument (the record itself), with the same fields. It runs
  after `LootGroundCreate`'s store and before `CreateItemNew`, and nothing
  between them stores into `a`, `b`, `c` or `j` (static reading). It ranks
  second because it runs through the runtime's new-object builtin, which no
  ForgePact hook has attached to yet; whether a hook on it fires is not
  established.
- **What Live 3's `record` check should see** (a prediction from the
  reading, not established): a vanilla record dumped at `CreateItemNew`'s
  entry already carries `a` (the value `LootGroundCreate` stored), `b` the
  unique, `c` 1 and `j` the sub, and the `built:` definition shows the same
  four values.
- **`report#2`'s 30 repeats** in Live 2 are not established; Live 3 records
  the message text. The reading found no error this chain raises on every
  hit, and 30 repeats over a batch of about 49 rolls that gave 46 hits is not
  one per hit. One unranked candidate: both `LootGroundCreate` and
  `LootGroundInit`, which run on every placed drop rather than only on Angelic
  hits, read protected game values through a route that can call the
  runtime's extension stub builtin; whether that raises offline is not
  established.

### What the plugin does with it

Only the rewrite moves, and it gains a latch; the rest of Sessions 3 and 4's
design is unchanged.

- **The rewrite point.** `Hook_CreateDefaultParams` still types the hit and
  attributes it (`SignatureAttributeHit`), but no longer writes anything: it
  hands the hit to the rewrite point by `g_SigHitItem`.
  `SignatureBeforeCreate` runs in the `CreateItemNew` hook's pre-call slot,
  beside `GemsBeforeCreate`, on the outermost call only, once per hit, and
  only while the roll is in progress and a hit was seen. It reads the
  argument's `itemDefinitionStruct` and, on a hit of ours, writes the item's
  `a`, `b`, `c` 0 and `j` 0 onto it. A hit that is not ours is never touched.
- **A missing field is created, not refused.** The rewrite asks which of
  `a`, `b`, `c`, `j` the record has, keeps their values, writes all four
  (`variable_struct_set` creates a field a struct lacks) and reads every one
  back. A value that does not read back is still a refusal, and the refusal
  puts every field back, removing one the rewrite created
  (`variable_struct_remove`). No `w` is written: the reading found the
  constructor never reads it.
- **The hook is the fourth route.** `LootGroundCreate` calls `CreateItemNew`
  directly, so the rewrite needs an inline detour there.
  `InstallSignatureAngelicHooks` installs `Hook_CreateItemNew` by its SDK
  name (`gml_Script_CreateItemNew`) beside the other three, unless the Custom
  Forge or Item Truth already hold it (the same hook either way), and then
  reads its route from the saved original like the others. The gate needs
  all four detoured, and `detect=` names `CreateItemNew:<route>` when it is
  not. In the research build, the item-inspect hooks install a table-only
  `CreateItemNew` at startup when no forge entry did first; that reads
  `detect=CreateItemNew:TABLE-ONLY`, and the gate stays off.
- **`built=` keeps its meaning.** The forge hook's final pass, on the same
  `CreateItemNew` call's return, saw an item whose `{t, a, b, c, j}` is a mod
  item's selector while that item's rewrite was pending in this roll.
- **The refusal latch, both builds.** The first refused rewrite of an item
  turns it off for the rest of the session. `SignatureSwitchOn` says no, so
  no more of its copies are pushed: a copy whose hits are all refused would
  only double a vanilla unique's share while the switch reads on. The
  refusal prints `inject: <item> refused (<why>), the record left vanilla:
  <json>` and says the item stays off for this session. `SignatureOffReason`
  answers `rewrite refused: <why>`, so the switch-on and auto-arm lines say
  why; `headhunter status` and `tyrant status` read `angelicDrops=refused`;
  and `sigdrop status` counts every refusal as `refused=`, beside
  `ourHits=`. A hit of ours that no `CreateItemNew` call reached during the
  roll is a refusal too (`CreateItemNew did not run for it during the
  roll`).
- **The record dump (research build only).** `angelicprobe hit show <k>`
  (0..50, default 3) marks the next k hits inside the roll. Each prints,
  at the rewrite point, `angelic hit record: vanilla <json>` on a hit that is
  not ours (the record as the game made it), or `angelic hit record: before
  <json>` and `angelic hit record: after <json>` around our write. On the
  forge hook's final pass it prints `angelic hit built: itemType=<t>
  definition=<json>`, the built item's `itemDefinitionStruct`. The JSON is
  the game's own `json_stringify`, bounded at 420 characters.
  `angelicprobe hit status` prints `show=<k>` remaining. These replace
  Session 4's parameter lines, which printed the struct at the old point.
- **No `point` lever.** The reading established the point
  (`CreateItemNew`'s entry), so the second candidate is recorded above and
  not built. If Live 3's `record` check shows the record is not the one the
  constructor fills, the instance constructor is the next round's point.
- **The model.** The behaviour harness's `CreateDefaultParams` now returns
  the measured `{j, b, c}`; its `LootGroundCreate` stores its own `a` and
  hands `CreateItemNew` an instance whose definition is the record; and its
  `CreateItemNew` hook is the Custom Forge's, detoured, as in Live 2.
  `test_build_id` runs `measured_params_build_our_item` (switch on: no
  refusal, the game's build recorded once with `t` 8, `a` 777002, `b` 2, `c`
  0, `j` 0, `built=1`, `belt=1`) and `measured_params_vanilla_untouched`
  (switches off: the record keeps the model's own `a`). Against the plugin
  Live 2 ran (tag `forgepact-74-live2-base`) the first fails with `no field
  a`, which is the baseline of this session. `test_refusal_latch` runs
  `refusal_latches_item_off`.

### Live procedure 5

Live 3 runs the rebuilt research build through the drive tool, with the
owner doing the killing. It is `### Live procedure 4` with three changes: the
record dump replaces `repo-standin` and `list-stable` as the second positive
control, `inject-build` is judged on the rewrite at `CreateItemNew`'s entry,
and `replace-remove` is not re-run (Live 2 measured it, 42 of 42). It is a
new session on a restored save, so every starting value is the state before
Live 2. The session's capture is
`forgepact-74-list-injection-live2-live-2.md`, kept with the hub's workorder
and not tracked; its record goes under `### Results` below.

- **Build.** `plugin_build\live3\BloodPactPlugin_rel.dll`, a frozen copy of
  `plugin_build\build.bat dev` with this session's change; its SHA-256 is
  recorded when it is built. The owner installs it when asked; until then
  `dll-hash` fails and nothing else runs.
- **Character.** Save slot 14 (Sorak), selected on the back end, in town at
  load.
- **Standing steps** and **hygiene**: as `### Live procedure 3`. In addition,
  the newest `[hs] YYError summary: total=` line of `bin\YYToolkit.log` is
  recorded, with no verdict, before check 4, after check 6's first batch and
  before teardown, and once `top=report#N` names a report, the message line
  of that report's first-occurrence block (the line with the error text,
  never the frames below it).
- **People steps.** One zone change (check 5), then batches of 10 kills with
  the drops left on the ground, read after each batch; a step ends as soon as
  its hit count is reached. Counts are hits (`gameHits=` growth), never
  kills.

Each check is recorded as pass, fail, not-observed or not-run, with the
replies quoted in the session record. The research checks are never pass
conditions of the session.

1. **`dll-hash`** - the lease's DLL SHA-256 equals the research build's.
   Fail: nothing after it runs.
2. **`marker`** - `angelicprobe hit status` answers a line beginning
   `angelicprobe hit:` and ending `detect=off`. A player build answers that
   the command is unavailable, and the session ends there.
3. **`control`** - `sigdrop status` reads exactly the fresh-session line,
   `sigdrop: force off | rolls=0 drops=0 fails=0 | game roll: gameRolls=0 gameHits=0 injected=0 ourHits=0 refused=0 untyped=0 built=0 crown=0 belt=0 anomalies=0 list=none gate=tyrant:off,headhunter:off cdpCalls=0 detect=off`.
4. **`layout`** (research; the positive control on the layout, before
   anything is injected) - `angelicprobe list dump lootListUnique`: the first
   line reads `kind=array array_length=6`, six element lines follow, and
   `[5]` reads `kind=ref ds_list=yes:<size> triples=<size>/<size>` with both
   stand-ins at n = 1 (Live 2: 380). Then `angelicprobe inject auto` answers
   `list lootListUnique[5]:<size>`, and `angelicprobe inject status` reads
   `copies=1 list=lootListUnique[5]:<size>` with no `(not validated)`. Pass:
   all of that. Fail: `[5]` not a `ds_list` of triples, a stand-in at an n
   other than 1, or `(not validated)` (the record names which). Not-observed:
   the first line is not `kind=array array_length=6`.
5. **`record`** (research; the positive control on the dump, before anything
   of ours is written) - **the owner takes the town portal or a waypoint to
   any ordinary zone.** `angelicprobe hit show 6` confirms `show=6`;
   `raredrop angelic 2` opens the gate; `angelicprobe hit chance 1000000000`
   turns the override on with `detect=detoured`. **The owner kills in batches
   of 10** until `gameHits=` is 20 or more (three batches at most), both
   switches off. The first `angelic hit record: vanilla <json>` line and the
   first `angelic hit built: itemType=<t> definition=<json>` line are read
   from the log.
   - Pass: at least one vanilla hit printed both lines; the `record: vanilla`
     JSON names the record's fields as the game made them (the field set is
     recorded verbatim, and whether it carries `a` at all); and the `built:`
     definition carries `a` as a number, with `b`, `c` 1 and `j` matching the
     record.
   - Fail: a `built:` definition without `a`, or with a `b`, `c` or `j` the
     record did not carry (the record is not the one the constructor fills,
     and `inject-build` is then judged in its blind form).
   - Not-observed: no hit printed a `record: vanilla` line (the dump did not
     see the point) or no `built:` line (the forge hook did not see the build
     inside the roll). The record says which, and the session continues.
6. **`force-hit`** (validity), **`baseline-off-vanilla`** (acceptance) and
   **`typing`** (research), from check 5's batches (more kills only when
   `gameHits=` is below 20):
   - `force-hit`: `cdpCalls=` above 0, `detect=detoured` and `gameHits=` of 20
     or more pass; anything else fails, and checks 7 and 8 are not-run.
   - `baseline-off-vanilla`: `injected=0 ourHits=0 refused=0 built=0`,
     `list=` equal to check 4's `lootListUnique[5]:<size>`, and no `inject:`
     line pass. The baseline share p0, `standinPicks=` over `gameHits=`, is
     recorded.
   - `typing`: every vanilla hit line carries a numeric `builtType=` (if none
     does: not-run, instrument-blind, with `untyped=` recorded); then
     `untyped=0`, `typeAgree=` of 10 or more and `typeDisagree=0` pass;
     `untyped=` or `typeDisagree=` above 0 fail; fewer than 10 agreements are
     not-observed. Every vanilla hit's `lootDelta=` is recorded, and a
     screenshot of the ground taken.
7. **`reach`** (research) and **`inject-build`** (research; the route's
   input) - `angelicprobe hit show 6` again (so our first hits print),
   `angelicprobe inject copies 200`, then `headhunter force`: `headhunter:
   ON (forced)` and a `signature drops:` line naming `lootListUnique[5]` and
   Liquor Holster, and `sigdrop status` reading
   `gate=tyrant:off,headhunter:on`. The step's starting `gameHits=`,
   `standinPicks=`, `ourHits=`, `refused=`, `built=` and `belt=` are
   recorded. **The owner kills in batches of 10** until `gameHits=` has grown
   by 20 or more (two batches at most; up to 40 hits and four batches only
   when `reach` is inconclusive).
   - `reach`, from p1, the growth of `standinPicks=` over the growth of
     `gameHits=`: `heldMiss=0` and p1 of 0.20 or more pass (Live 2: 0.81);
     `heldMiss=` above 0, or p1 below 0.10, fail; anything between is
     not-observed. There is no `at` fallback in this session: the layout and
     the index are measured.
   - `inject-build` is judged only when `reach` passed and `typing` passed or
     was instrument-blind; otherwise it is not-run (instrument-blind, naming
     which did not pass).
   - Pass: `ourHits=` grew by 3 or more; `built=` and `belt=` grew by the same
     amount; `crown=0`, `anomalies=0` and `refused=0`; every our-hit line ends
     `built by the game` with `lootDelta=1`; no `refused` line and no
     `sigdrop:` line; the first our-hit printed `angelic hit record: before
     <json>` and `angelic hit record: after <json>`, the `after` JSON with `a`
     777002, `b` 2, `c` 0 and `j` 0, and an `angelic hit built: itemType=8
     definition=<json>` with the same four values. Then **the owner hovers one
     item on the ground and names it**: Headhunter (with a screenshot).
   - Fail: a crash; `built=` growth below `ourHits=` growth; a `lootDelta=`
     other than 1 on an our-hit; a refusal line (its text and JSON recorded:
     the latch then turns Headhunter off, `gate=...headhunter:off` and
     `angelicDrops=refused`, which the record notes); a `built:` definition
     whose `a` is not 777002 after an `after` JSON that carried it (the
     constructor overwrote it: both JSONs recorded); or an item that is not
     Headhunter.
   - Not-observed: fewer than 3 hits of ours by 40 hits.
   - Blind form (`record` or `typing` did not pass): `built=`, `belt=` and
     the line endings are recorded but decide nothing; the verdict rests on
     `ourHits=` growth of 3 or more, `crown=0`, `anomalies=0`, no refusal, no
     `sigdrop:` line, and the owner naming a Headhunter.
8. **`on-both`** (acceptance of the shipped pairing; not-run unless
   `inject-build` passed) - `tyrant force`, copies still 200. **The owner
   kills in batches of 10** until `crown=` is 1 or more (two batches at
   most). Pass: `crown=` 1 or more, `belt=` grew, `built=` growth equal to
   `ourHits=` growth, and a Tyrant's Crown seen on the ground (screenshot).
9. **`off-removes`** (acceptance) - `angelicprobe inject copies 1`,
   `headhunter off`, `tyrant off`; `sigdrop status` reads
   `gate=tyrant:off,headhunter:off`, and `angelicprobe inject status` shows
   `list=lootListUnique[5]:<size>` equal to check 4's. **The owner kills
   five.** `injected=` and `ourHits=` unchanged, no mod item and no `inject:`
   line pass.
10. **`sigdrop-still-forces`** (acceptance) - `sigdrop crown`; **the owner
    kills one**; the log shows `sigdrop: Tyrant's Crown dropped at`; then
    `sigdrop off`. The last people step.
11. **`list-restored`** (acceptance) - `angelicprobe hit off`,
    `raredrop angelic 1`, then `angelicprobe list dump lootListUnique`: all
    six sizes equal to check 4's and each stand-in's n equal to check 4's;
    `angelicprobe inject status` shows `list=` equal to check 4's.
12. Stop the game normally, then inspect and restore the saves (the standing
    steps).

The capture lists the checks in this order: `dll-hash`, `marker`, `control`,
`layout`, `record`, `force-hit`, `baseline-off-vanilla`, `typing`, `reach`,
`inject-build`, `on-both`, `off-removes`, `sigdrop-still-forces`,
`list-restored`. The session must pass `dll-hash`, `marker`, `control`,
`force-hit`, `baseline-off-vanilla`, `off-removes`, `sigdrop-still-forces`
and `list-restored`. The route comes from Session 3's decision rule as the
owner's decision after Live 2 amends it: `route: replace` is never set
without the owner's word.

### Results

Session 5 ran once, as Live 3, on 2026-10-02 (the drive tool's lease taken
at 20:46 UTC), on the research dll `e0749368…5f4f9c` (SHA-256
e0749368d198c63ce6ce2bed397051f958e3c362d3f73a417a1705311c3f4f9c, built with
`build.bat dev` from ForgePact `df1f54b` and kept as
`plugin_build/live3/BloodPactPlugin_rel.dll`), save slot 14, plugin banner
v2.1.0, the kills in Outskirts of Inoya (Hell, zone level 243, read from the
HUD). The drive tool's self-check passed 6 of 6; the saves were copied before
launch and restored at teardown with nothing left changed. Measured in that
session, every verdict below; the replies are quoted in its capture
(`forgepact-74-list-injection-live2-live-2.md`), which stays with the hub's
workorder and is not tracked. As in Live 2, the arming of check 5 was done
before the zone change. The owner reported each batch as about 10 kills
without an exact count, so every count below is hits, never kills.

- **`dll-hash`: pass.** The lease's DLL hash was the research dll's.
- **`marker`: pass.** `angelicprobe hit status` answered
  `angelicprobe hit: chance off | rate off | show=0 | detection not installed (CreateDefaultParams not hooked, DropItemAngelicChance not hooked, GetUniqueRepoStruct not hooked, CreateItemNew hooked) | gameRolls=0 gameHits=0 cdpCalls=0 detect=off`.
  `CreateItemNew` was already hooked at load (it carries the Custom Forge
  final pass); the three roll hooks were not.
- **`control`: pass.** `sigdrop status` read exactly the fresh-session line,
  ending `list=none gate=tyrant:off,headhunter:off cdpCalls=0 detect=off`.
- **`layout`: pass.** The dump read as in Live 2:
  `Controller_obj.lootListUnique kind=array array_length=6`, all six elements
  ref ds_lists of triples (50, 61, 79, 152, 221 and 380 entries), and
  `[5] kind=ref ds_list=yes:380 triples=380/380 first=[0,0,1][0,0,15][0,0,29]`
  with `Headhunter:Liquor Holster(n=1)` and
  `Tyrant's Crown:Mask of the Celestial(n=1)`, n = 0 in `[0]`-`[4]`.
  `angelicprobe inject auto` answered `list lootListUnique[5]:380`, and the
  inject status line read `mode=inject copies=1 list=lootListUnique[5]:380`
  with no `(not validated)`.
- **`record`: pass.** `angelicprobe hit show 6` printed six vanilla hits,
  each with both lines. The first:
  `angelic hit record: vanilla {"b":9.0,"a":270500966.0,"j":6.0,"c":1.0}`
  and
  `angelic hit built: itemType=3 definition={"b":9.0,"a":270500966.0,"j":6.0,"c":1.0}`.
  The record's field set, verbatim, is `b`, `a`, `j`, `c` on all six: at
  `CreateItemNew`'s entry the record already carries `a`, a number (270500966,
  41023733, 585711642, 244995739, 727960336 and 54011597 on the six), as the
  static reading above predicted from `LootGroundCreate`'s store. Each
  `built:` definition held the same four values as its record, `c` 1. No
  record and no built definition on this path carried `w` (a `sigdrop` item,
  built through `InitItemFromJson`, carries `w` and `o`; check 10 below).
- **`force-hit`: pass.** With `raredrop angelic 2` and the chance lever at
  1e9, one batch gave `gameRolls=52 gameHits=47`, `cdpCalls=49`,
  `detect=detoured`.
- **`baseline-off-vanilla`: pass.** Both switches off over those 47 hits:
  `injected=0 ourHits=0 refused=0 built=0`, `list=lootListUnique[5]:380`, no
  `inject:` line, every hit line `-> vanilla` with `lootDelta=1`.
  `standinPicks=2`, so the baseline share p0 is 2/47 (0.043).
- **`typing`: pass.** All 47 vanilla hit lines carried a `builtType=` number
  (none `?`), for example
  `angelic hit: picked 3/6/9 at 16000,4336 gate=tyrant:off,headhunter:off lootDelta=1 builtType=3 -> vanilla`;
  `untyped=0`, `typeAgree=47`, `typeDisagree=0`. By the end of the session
  `typeAgree=87`, still `typeDisagree=0` and `untyped=0`.
- **`reach`: pass.** `angelicprobe inject copies 200`, `headhunter force`,
  `gate=tyrant:off,headhunter:on`. Over the batch the hits grew from 47 to
  109 (+62) and `standinPicks=` from 2 to 48 (+46), so p1 = 46/62 = 0.742
  against p0 = 2/47, with `heldMiss=0` and `injected=18000` (200 entries on
  each of 90 rolls). Live 2 measured 0.81 the same way.
- **`inject-build`: pass.** Over the same batch `ourHits=`, `built=` and
  `belt=` each grew from 0 to 46, with `crown=0`, `anomalies=0` and
  `refused=0`. Each of the 46 our-hit lines read
  `angelic hit: picked 8/0/51 at <x,y> gate=tyrant:off,headhunter:on lootDelta=1 builtType=8 -> Headhunter (stand-in Liquor Holster, 200 in 201) built by the game`,
  and the other 16 hits `-> vanilla`; 62 of 62 hit lines `lootDelta=1`; no
  refusal line and no `sigdrop:` line. The first our-hit printed
  `angelic hit record: before {"b":51.0,"a":648002927.0,"j":0.0,"c":1.0}`,
  `angelic hit record: after {"b":2.0,"a":777002.0,"j":0.0,"c":0.0}` and
  `angelic hit built: itemType=8 definition={"b":2.0,"a":777002.0,"j":0.0,"c":0.0}`;
  the other five shown printed the same `after` and `built` JSON, each
  `before` with `b` 51, `c` 1, `j` 0 and its own `a`. So the `a` written at
  `CreateItemNew`'s entry is the `a` the built definition carries: the
  constructor does not overwrite it, and the game builds the item from a
  `c` 0 record through `CreateItemNew`. The ground screenshot showed many
  `Headhunter | SS` labels and no Liquor Holster label, and the owner,
  hovering one: "tooltip reads correctly a headhunter" (no item type stated;
  the item's identity rests on that report and the labels).
- **`on-both`: pass.** `tyrant force`, copies still 200. Over one batch the
  hits grew from 109 to 124 (+15), `ourHits=` and `built=` each +13,
  `crown=` 0 to 11, `belt=` 46 to 48, `refused=0`, `anomalies=0`. The 15 hit
  lines, all `lootDelta=1`: 11
  `picked 0/0/86 ... builtType=0 -> Tyrant's Crown (stand-in Mask of the Celestial, 200 in 201) built by the game`,
  2 Headhunter and 2 `-> vanilla`. The ground screenshot showed several
  `Tyrant's Crown | SS` labels beside Headhunter ones.
- **`off-removes`: pass.** Copies back to 1, both switches off. Over five
  kills the hits grew from 124 to 146 (+22), every hit line `-> vanilla`,
  `injected=27200` and `ourHits=59` unchanged, `built=`, `crown=` and `belt=`
  unchanged, no `inject:` line, `list=lootListUnique[5]:380` equal to
  `layout`'s. Two of those hits were the game's own picks of Liquor Holster
  (`picked 8/0/51 ... builtType=8 -> vanilla`, `standinPicks=` 61 to 63) with
  nothing pushed, and stayed Liquor Holsters.
- **`sigdrop-still-forces`: pass.** `sigdrop crown`, then the one kill the
  owner reported: eight lines `sigdrop: Tyrant's Crown dropped at <x,y> kind=15`
  (the hits grew from 150 to 157 in that window), each item's definition
  `{"w":1.0,"o":1.0,"a":777001.0,"b":7.0,"j":0.0,"c":0.0}`; `sigdrop off`
  answered `rolls=8 drops=8 fails=0`.
- **`list-restored`: pass.** After `angelicprobe hit off` and
  `raredrop angelic 1` the dump read the same six sizes, n = 0 for
  `[0]`-`[4]` and n = 1 for both stand-ins at `[5]`, and the inject status
  line `list=lootListUnique[5]:380`, all equal to `layout`'s.

Without a verdict: the runner's YYError summary read `total=1` before check 4
and `total=19` (`distinct=5`, `top=report#2 x15`) after check 5's first
kill batch, and did not move again through the end of the session, when 90
more rolls had pushed entries and their our-hits had been built. The message
line of `report#2`'s first-occurrence block is
`REAL argument incorrect type array`. So it is a runner conversion of an
array value to a number, raised between the read before check 4 and the end
of the first batch: the window that holds the layout dump, the
`angelicprobe inject auto` scan, the arming of the research levers and 52
rolls with both switches off. Live 2 showed the same shape (1 to 37,
`report#2` x30, in the same window). Which of those raises it is not
established. What was measured is that the count did not move through
checks 7 to 11, while rolls pushed entries and their our-hits were built.
Session 6 takes the cause up.

**Route:** `route: inject`. Session 3's decision rule, step 4 (`typing`
pass, `reach` pass, `inject-build` pass), as the owner's decision after
Live 2 amends it. The design ships: the player build pushes one entry per
enabled item and rewrites the record at `CreateItemNew`'s entry; the replace
mode stays research build only, unused. What Live 3 established on top of
Live 2: at `CreateItemNew`'s entry the record already carries `LootGroundCreate`'s
`a`; a record rewritten there to the item's `a`, `b`, `c` 0 and `j` 0 keeps
those values through the build; and the game builds and places one
Headhunter (48 of 48: 46 in `inject-build`, 2 in `on-both`) or Tyrant's
Crown (11 of 11) per hit that falls to it,
in place of the stand-in. Still not observed: a hit at the game's natural
chance (the chance lever was at 1e9 throughout, and a natural hit is about
one in several thousand rolls), and the player build's one-entry share, which
is arithmetic on a reach measured at copies 200, not a measurement.

## Session 6: the scan and the runner errors (issue #74)

Live 2 and Live 3 each saw the runner's YYError total rise once, early in the
session, and then hold (Session 4 and Session 5, Results). Live 3 recorded
the message of the report that rose most, `report#2`:
`REAL argument incorrect type array`. The owner asked for it to be
investigated, then for the fix and a short live confirmation (2026-10-02,
"Fix + confirm live"). This section is what a read-only pass over the
plugin, the runner's headers and the two sessions' own output established,
the fix, and the session that confirms it.

### The finding

- **What raised it (an arithmetic fit on measured counts; the stack is not
  established).** `bin\bp_ipc\out.txt` still holds Live 2's printed
  `angelicprobe list` scan (measured). It rejected 15 `Controller_obj` array
  variables with `[5] is not a ds_list (kind=array, id unreadable)`
  (`buffAfterText`, `buffCalculate`, `buffDrawValue`, `questlogMaxProgress`,
  `buffRoom`, `questlogObjectiveText`, `questlogProgress`, `buffValueText`,
  `stashGuildTab`, `stashMaterialTab`, `stashPactTab`,
  `stashSocketItemSlot`, `stashTab`, `stashUniqueItemSlot`, `merchantGrid`)
  and 3 string variables with `kind=string, id unreadable`
  (`questlogDescription`, `buffNameText`, `questlogName`). `id unreadable` is
  the plugin's catch after the numeric conversion in `SigListHandle` (source
  reading). Live 2 ran two scans (`angelicprobe list` and
  `angelicprobe inject auto`) and its total went from 1 to 37, which is
  1 + 2 x 18, with `report#2` x30, which is 2 x 15. Live 3 ran one scan
  (`angelicprobe inject auto`) and went from 1 to 19, which is 1 + 18, with
  `report#2` x15. That Live 3's scan, which prints no rejected lines, met the
  same 18 elements is inferred, not measured. The text of reports #3 to #5 is
  not established; by the same fit, the three string conversions are the
  likeliest source. Live 4 cannot settle the strings' share: the fix refuses
  strings before converting them, so it can show only that the total stops
  rising, not that a string conversion raises.
- **Not per hit or per roll (measured).** Live 3's total held at 19 from its
  step 6 to teardown, while `gameHits` went from 47 to 157, `injected` reached
  27200 and 59 of our hits were built. Live 2's held at 37 through 215 hits.
  The later `angelicprobe list dump` runs, which read only `lootListUnique`'s
  own elements (all `kind=ref`), left it unchanged.
- **Mechanism (source reading, with two kinds measured).** By source
  reading, `RValue::ToDouble` is the runner's own REAL conversion
  (`plugin_build/include/YYToolkit/YYTK_Shared_Types.cpp`); that
  identification is itself part of the reading. Given a kind it cannot turn
  into a number, the runner raises its error and the call then fails. The
  plugin's C++ `try` turns the failure into `id unreadable`, but it does
  not take back the runner's report, which the YYError hook has already
  counted. For an array the raise is measured: Live 3's capture holds
  `REAL argument incorrect type array`. For undefined it is measured on the
  same kind of route: `REAL argument incorrect type undefined` (ForgePact
  #144, `docs/RUNTIME_DATA_MODELS.md` in the hub, § 5.4). For a string, a
  struct and null it rests on the reading alone; their error text has not
  been captured.
- **Who reaches it (source reading).** The scan over every array variable,
  `SigListScan`, runs only in the research build (`angelicprobe inject auto`
  and `angelicprobe list`), and it is the only caller that hands
  `SigListHandle` arbitrary values. In both builds `SigListHandle` is also
  called from the list resolution and the held read-back, which hand it only
  `lootListUnique[5]`, a ref. So the player build carries a latent risk, not
  a live error.
- **The log's report#2 block is gone (measured).** A later session overwrote
  `bin\YYToolkit.log`, so the message survives only in Live 3's session
  record. Live 4 therefore keeps a copy of the log before and after it runs.

### The fix

Before any conversion, `SigListHandle` refuses a value whose kind can never
be a data-structure handle: an array, a string, a struct (`VALUE_OBJECT`), an
undefined, or a null, which YYToolkit's enum keeps as a kind of its own. The
check is a named predicate, `SigNeverAHandle`, and the refusal goes through
the gate's existing reason, so it reads
`[<i>] is not a ds_list (kind=<kind>, never a handle)`. A real and a ref go on
to the conversion and `ds_exists` as before, and so do the kinds the owner's
list does not name (int32, int64, bool, ptr).

This keeps Session 4's decision. That decision was against an allow-list:
the runner hands a live handle back as a real or a ref, and a gate that lets
only those two through would refuse whatever else a handle might arrive as.
A deny-list of kinds that are never handles cannot refuse a handle. The
contract tests still pin that the gate reads no kind outside its diagnostic,
and that an allow-list put in front of the conversion fails them.

The harness pins the behaviour (`test_kind_gate`, scenario
`kind_gate_refuses_before_converting`). Its stand-in `ToDouble` counts each
conversion it refuses, the way the runner counts a raised error. With an
array, a string, a struct, an undefined or a null at `[5]`, the gate must
refuse `never a handle` and the count must not move. A live list whose
handle is a real, and one whose handle is a ref, must still be accepted.
Against the plugin before the fix (tag `forgepact-74-kindgate-base`) the
scenario fails by its own assertion: the array is refused as
`id unreadable`, after a conversion.

### Live procedure 6

Live 4 asks one question: with the kind gate in place, does the scan still
raise runner errors? It is a new session on a restored save, so every
starting value is the state before Live 2: the counters at 0, and
`lootListUnique[5]` at Live 3's 380 in town. There are no kills and no zone
change. The session's capture is
`forgepact-74-list-injection-live2-live-3.md`, kept with the hub's workorder
and not tracked; its record goes under `### Results` below.

- **Build.** `plugin_build\live4\BloodPactPlugin_rel.dll`, a frozen copy of
  `plugin_build\build.bat dev` with this session's change; its SHA-256 is
  recorded when it is built. The owner installs it when asked; until then
  `dll-hash` fails and nothing else runs.
- **Character.** Save slot 14 (Sorak), selected on the back end, in town at
  load.
- **People steps.** None in the session. The owner's only time is installing
  the DLL when asked, about 5 minutes in all.
- **Hygiene.** Before check 6, run nothing but the commands below: no
  `angelicprobe list dump`, no `angelicprobe on`, and no `citrace`,
  `raredrop`, `scount`, `zonegenlog` or `angelicwatch`.
  `angelicprobe inject auto` is the only command between the two reads of
  check 6.
- **Reading the runner-error instrument.** `bin\YYToolkit.log` in the game
  folder is read as a file (the drive tool's IPC tail returns the plugin's
  `out.txt`, not this log). The summary line
  `[hs] YYError summary: total=... distinct=...` is written from the frame
  hook at most once per 30 s, and only when a counter moved. So each read
  waits at least 35 s after the last command, and "no newer summary line"
  means no counter moved. Each read records the newest summary line whole,
  and the count of full-report headers
  (`The runner raised an error through YYError (full report #N).`). For a new
  header it records the line after `Runner-given error information:` (the
  error text), never the frames.

Each check is recorded as pass, fail or not-observed, with the replies quoted
in the session record.

1. Before launch, right after the lease: copy `bin\YYToolkit.log` as it
   stands to the session scratch and record the path (no check). It is the
   previous session's log, which the launch overwrites. Then the standing
   steps: self-check, an independent save copy, the save backup, launch, and
   save slot 14 selected on the back end.
2. **`dll-hash`** (validity) - the lease's DLL SHA-256 equals the research
   build's, the `live4` copy, and is not Live 3's.
3. **`marker`** (validity) - `angelicprobe hit status` answers a line
   beginning `angelicprobe hit:` and ending `detect=off`. A player build
   answers that the command is unavailable, and the session ends there.
4. **`control`** (validity) - `sigdrop status` reads exactly the
   fresh-session line,
   `sigdrop: force off | rolls=0 drops=0 fails=0 | game roll: gameRolls=0 gameHits=0 injected=0 ourHits=0 refused=0 untyped=0 built=0 crown=0 belt=0 anomalies=0 list=none gate=tyrant:off,headhunter:off cdpCalls=0 detect=off`.
   Live 3 produced it on this build line.
5. **`yyerror-control`** (research; the positive control on the instrument) -
   at least 35 s after the character loaded, read the log (read T0, headers
   H0).
   - Pass: the log carries `[hs] YYError hook install: MmCreateHook => AURIE_SUCCESS`
     and a summary line written this launch with `total=` of 1 or more, so the
     instrument counted a raise in this launch. Live 2 and Live 3 both read
     `total=1 ... top=report#1 x1` at this point. Not established: the hook
     marker's exact text, which comes from the YYToolkit patch series' table
     (patch 0005), not from a log this session has read.
   - Not-observed: the hook line is there but there is no summary line yet
     (installed, not shown to count).
   - Fail: the log cannot be read, or it has no hook line.
   - There is no stronger control: no research command is known to raise a
     runner error on demand on this build. The static search found none, and
     the one measured, this scan, is what the fix removes. So the control is
     this launch's own count, plus the rise Live 2 and Live 3 measured on the
     same command with the earlier DLLs: 18 per scan.
6. **`scan-clean`** (acceptance of the fix) - `angelicprobe inject auto`
   answers
   `angelicprobe inject auto: list lootListUnique[5]:<n> stand-ins Headhunter:Liquor Holster(n=1),Tyrant's Crown:Mask of the Celestial(n=1)`
   (Live 3: 380). Wait at least 35 s, then read T1 and H1.
   - Pass: `yyerror-control` passed, the line names `lootListUnique[5]`, T1's
     `total=` equals T0's (no newer summary line, or a newer one with the same
     total), and H1 equals H0.
   - Fail: the total rose or a new header appeared. Record the rise and each
     new report's error text.
   - Not-observed: `yyerror-control` did not pass, or `inject auto` named no
     list. Record which.
7. **`list-refusals`** (acceptance of the fix) - run `angelicprobe list` and
   read every printed `angelicprobe list:` line. Then wait at least 35 s and
   read T2 and H2.
   - Pass, when all of these hold: a line
     `angelicprobe list: candidate lootListUnique array_length=6 at=5 ds_list_size=<n> ...`
     is printed (the gate still accepts the real list, a ref); every
     `rejected` line whose `why=` names `kind=array` or `kind=string` ends
     `never a handle)`; no `rejected` line reads `kind=array, id unreadable`
     or `kind=string, id unreadable`; and T2's total equals T1's, with H2
     equal to H1.
   - Record the counts by kind and step, and any `rejected` line of another
     kind as printed. Live 2 had 15 array and 3 string; a different count is
     recorded, not a fail.
   - Fail: an `id unreadable` line for an array or a string, no
     `candidate lootListUnique` line, or a rise in the total. Record each new
     report's error text.
   - Not-observed: no `rejected` line of kind array or string at all.
8. Stop the game normally. Copy this session's `bin\YYToolkit.log` to the
   session scratch and record the path; it is the copy the record reads if a
   report appeared. Then inspect the saves, restore the session's backup
   (automatically, never asking), and release the lease.

Expected checks, in this order: `dll-hash`, `marker`, `control`,
`yyerror-control`, `scan-clean`, `list-refusals`. The session must pass
`dll-hash`, `marker`, `control`, `scan-clean` and `list-refusals`.
`yyerror-control` is research and never a pass condition; if `scan-clean` is
not-observed because of it, the session showed nothing about the fix, and the
record says so.

### Results

**Measured (Live 4, 2026-10-02, research dll `4b5994c3…`, built with
`build.bat dev` from ForgePact `ed59983`).** Live 4 ran Live procedure 6
once, on the frozen `plugin_build\live4\` copy, with save slot 14 (Sorak)
selected on the back end, in town, with no kills and no zone change. All six
checks passed. The session record is
`forgepact-74-list-injection-live2-live-3.md`, kept with the hub's workorder.

- **`dll-hash`: pass.** The lease's DLL SHA-256 was
  `4b5994c3ae7f313c47f82ac40bf8a8d4a0662d71aba1fcbf4f290dd7e6868ee7`, the
  `live4` copy, not Live 3's `e0749368…`.
- **`marker`: pass.** `angelicprobe hit status` answered
  `angelicprobe hit: chance off | rate off | show=0 | detection not installed (CreateDefaultParams not hooked, DropItemAngelicChance not hooked, GetUniqueRepoStruct not hooked, CreateItemNew hooked) | gameRolls=0 gameHits=0 cdpCalls=0 detect=off`.
- **`control`: pass.** `sigdrop status` read the fresh-session line exactly,
  `sigdrop: force off | rolls=0 drops=0 fails=0 | game roll: gameRolls=0 gameHits=0 injected=0 ourHits=0 refused=0 untyped=0 built=0 crown=0 belt=0 anomalies=0 list=none gate=tyrant:off,headhunter:off cdpCalls=0 detect=off`.
- **`yyerror-control`: pass (T0, H0).** More than 35 s after the character
  loaded, `bin\YYToolkit.log`, written this launch, carried
  `[hs] YYError hook install: MmCreateHook => AURIE_SUCCESS` and the summary
  `[hs] YYError summary: total=1 distinct=1/32 reports=1 report_failures=0 ... top=report#1 x1`,
  with one full-report header: T0 `total=1`, H0 = 1. That report's error
  text is `Unable to find any instance for object index '257102' name '<undefined>'`,
  raised while the main menu was being navigated, before the character was
  played and before any command, so it is not the scan's. The previous
  session's log, copied before launch, holds the same single report and
  `total=1`.
- **`scan-clean`: pass (T1, H1).** `angelicprobe inject auto`, the only
  command between the two reads, answered
  `angelic pool: 47 candidates, 11 rejected`,
  `angelicprobe inject auto: list lootListUnique[5]:380 stand-ins Headhunter:Liquor Holster(n=1),Tyrant's Crown:Mask of the Celestial(n=1)`
  and
  `angelicprobe inject: mode=inject copies=1 list=lootListUnique[5]:380 standins=Headhunter:Liquor Holster(n=1),Tyrant's Crown:Mask of the Celestial(n=1) injected=0 ourHits=0 untyped=0 standinPicks=0 heldMiss=0 typeAgree=0 typeDisagree=0 built=0 removed=0 anomalies=0`.
  Read 40 s later, the log was unchanged and its newest summary line was
  still the `total=1 ... top=report#1 x1` one: T1 `total=1` = T0, H1 = 1 =
  H0. On Live 3's DLL the same command had taken the total from 1 to 19.
- **`list-refusals`: pass (T2, H2).** `angelicprobe list` printed
  `angelicprobe list: candidate lootListUnique array_length=6 at=5 ds_list_size=380 first=[0,0,1][0,0,15][0,0,29] standins=Headhunter:Liquor Holster(n=1),Tyrant's Crown:Mask of the Celestial(n=1)`
  and `angelicprobe list: candidates=1 best=lootListUnique[5]:380`, so the
  gate still accepts the real list, a ref. Its refusals, by kind and step:
  15 `kind=array, never a handle`, 3 `kind=string, never a handle`, and no
  `id unreadable` line of either kind (Live 2 had the same 15 and 3, every
  one `id unreadable`). Of the other kinds it printed one
  `[5] is not a ds_list (kind=real, ds_exists false)` (`questlogId`) and two
  `kind=ref, ds_exists false` (`buffSprite`, `monsterHandleArray`); the rest
  were refused for their length (`has <L> elements, none at [5]`) or their
  sub-list's size (`[5] has <s> entries, fewer than 10`). It also printed the
  two scopes Session 2 asked (no `lootListUnique` global, none on
  `Loot_Manager_obj`), `Controller_obj instances=1 names=221` (the first
  instance a ref), and the other variables by kind, `bool=24 int64=3 real=93 ref=18 string=7 struct=8 undefined=3`.
  Read 40 s later, the log was unchanged: T2 `total=1` = T1, H2 = 1 = H1.
- **The log copies.** The previous session's `bin\YYToolkit.log`, copied
  before launch:
  `C:\Users\stann\AppData\Local\Temp\claude\C--Users-stann-Projects-hero-siege-offline-toolkit--claude-worktrees-workorder-test-optimization-2395e1\5706ebd7-4ede-4811-8542-154d2cef9c96\scratchpad\YYToolkit.prev.log`.
  This session's, copied after the game stopped:
  `C:\Users\stann\AppData\Local\Temp\claude\C--Users-stann-Projects-hero-siege-offline-toolkit--claude-worktrees-workorder-test-optimization-2395e1\5706ebd7-4ede-4811-8542-154d2cef9c96\scratchpad\YYToolkit.live3.log`
  (one full report, the `Unable to find any instance` one above, and its
  last line the `total=1` summary). Neither copy is tracked. No new report
  appeared during the session, so there is no new error text to record.
- **Teardown.** The game stopped normally, the save inspection after exit
  showed only `shop.ini` changed, the session's own backup was restored
  (nothing changed after the restore), and the lease was released with no
  restore pending.

**What Live 4 settles, and what it does not.** With the kind gate refusing
the 15 array and 3 string elements before converting them, two scans (`inject
auto`, then `angelicprobe list`) added nothing to the runner's total, where
each scan had added 18 on the earlier DLLs (Live 3, 1 to 19 over one scan;
Live 2, 1 to 37 over two). So the research scan's numeric conversions of
those elements are measured as the cause of Live 2's and Live 3's rise, and
the fix as what removed it. Live 4 refused arrays and strings alike, so it
cannot split the 18 between them: the arrays' part has its measured message
(`REAL argument incorrect type array`, Live 3), the strings' part is still
the arithmetic fit, and that a string conversion raises is still a source
reading, its error text not captured. Not observed: a struct or a null on
the scan (the scan reads only array variables, and no `kind=struct` or
`kind=null` element was met), and whether a bool or ptr element would
raise (none was printed). The player build's own calls of the gate hand it
only `lootListUnique[5]`, a ref, which Live 4 shows is still accepted.
