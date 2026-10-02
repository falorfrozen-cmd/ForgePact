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
  can equally be another type's unique. The parameter struct carries only sub
  and b, and the type reaches `LootGroundCreate` as a separate argument. The
  roll's own definition reads carry all three: `GetUniqueRepoStruct` takes
  type, sub and b, and **measured** in Session 1 it is called from inside the
  roll (1409 of 1708 calls in case A, about eight per roll, on its own
  detour). That the last of those reads before a hit is the picked entry's
  follows from the order the roll was read to run in (pick, read the
  definition, skip and pick again on the filters, then roll the die against
  the definition just read, then build the parameters): **static reading,
  not measured**. Live procedure 3 checks it on every hit (`inject-build`
  below).

What #74 builds on this reading (the design as the plugin implements it; none
of it measured yet):

- **Scoped injection.** When a switch is on and the list resolved, the
  Angelic roll hook pushes one stand-in entry per enabled item onto the
  game's list before its first call to the original roll and removes them
  after the last one, ForgePact's extra rolls included, under a scope guard so
  a throw removes them too. It remembers the length before the push and
  removes only when the tail still holds exactly the pushed entries;
  otherwise it leaves the list as found, logs one line saying the list changed
  during the roll, and counts an anomaly. Between rolls the list is byte for
  byte vanilla: nothing is saved with it, the other readers above never see
  the entries, and switching off needs no cleanup. `injected=` counts the
  entries pushed. It says nothing about whether the roll's picker can see
  them: a push into a copy, into another array of the same shape, or onto a
  `Controller_obj` instance the roll does not read counts exactly the same.
  Only a change in what the picker draws shows that, and at one entry that
  change is too small to measure (below, `inject-build`'s reach control).
- **The hit's triple.** While the roll is in progress a detour on
  `GetUniqueRepoStruct`, installed by name through `HookOneScript`, records
  the type, sub and b of the roll's latest definition read; nothing is
  recorded outside the roll, where most of its calls come from. When the
  roll's `CreateDefaultParams` call comes, the hit's triple is that record,
  and only if the record's sub and b equal the call's own sub and b. With no
  record, or a record that disagrees, the hit is not typed: it stays vanilla,
  is never rewritten, and `untyped=` counts it. The type is never inferred
  from sub and b.
- **Attribution, one in n + 1.** The picker cannot tell our entry from the
  vanilla stand-in, because they are the same triple. A hit is a candidate of
  ours only when its whole triple, type included, equals a stand-in's triple;
  a hit that shares only the stand-in's sub and b under another type is
  vanilla. A candidate is ours with probability 1 / (n + 1), where n is how
  many times the vanilla list already holds that same triple, matched on all
  three numbers (counted when the list resolves and whenever its length
  changes; 1 when the stand-in is listed once). The match and the count
  therefore read the same thing. The stand-in keeps exactly one entry's share
  and our item gets one entry's share, the same as every other entry. Two
  enabled items sharing one stand-in split the extra share evenly. `ourHits=`
  counts them. The coin is the plugin's own, so it fires whether or not the
  pushed entry was ever drawn: on a list the picker never saw, a vanilla
  Liquor Holster hit still becomes ours one time in two, and `ourHits=`,
  `built=` and the Headhunter on the ground look the same as on a working
  injection. `ourHits=` is therefore evidence of the rewrite, never of the
  injection. In the research build the push can carry k copies of each
  stand-in (`angelicprobe inject copies <k>`, 1 by default and in the player
  build); the coin then reads k / (n + k), so the attribution stays one entry's
  share per copy.
- **What "Liquor Holster's share" means now.** In Session 2 the share was
  one pool entry of ForgePact's own validated pool, k / (47 + k), rolled on
  top of the game's hit. Now it is literal: Headhunter is one more entry in
  the game's own list, carrying Liquor Holster's definition and rate, so it
  is picked as often as one Liquor Holster entry is, and its hit replaces the
  item that entry would have dropped.
- **The game builds our item.** On our hit the `CreateDefaultParams` detour
  calls the original, then rewrites the returned struct's a, b, c and j to
  the mod item's own values and reads them back. A missing field is a
  refusal: the struct stays vanilla and its JSON is logged. The game's own
  placement then builds a Heavy Belt or Great Helm with that seed, the forge
  hook dresses it, and the game places it where the roll said. No
  `SpawnSignatureItem` runs on this path. `built=`, `crown=` and `belt=`
  count what the game built, and each hit logs one `angelic hit:` line naming
  its triple and saying vanilla, untyped, or our item with its stand-in and
  the one-in-n+1 odds.
- **Gate.** As in Session 2: the item's panel switch and a detoured
  `CreateDefaultParams` detection, plus a detoured `GetUniqueRepoStruct`
  definition read (a table-only route cannot be shown to see the roll's
  calls, and without it no hit can be typed) and the list resolved (`list=`
  names it and its length), else the gate stays off and the switch-on logs
  one refusal naming what is missing. Both switches off: no entry pushed, no rewrite, the
  roll untouched, and with neither ever on no hook installed. Forging an item
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
- **Recognising our hit by the parameter struct's sub and b.** That is the
  part of the triple the struct happens to carry, not the entry's identity: it
  would take another type's unique with the same sub and b for a stand-in,
  rewrite it into a mod item built under the wrong type, and count n by a
  triple the match never checked.
- **Typing the hit at `LootGroundCreate`**, which receives the type and the
  parameter struct together. The player build holds `LootGroundCreate`
  through the deliberately table-only inspection installer, so this needs a
  second, inline route on a function those hooks already own, and which
  struct field carries sub is not established. It is the route if the
  definition-read typing is not observed.
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

The decision between injection and replacement is mechanical, from Live
procedure 3's verdicts:

`inject-build` has two parts, and each ends pass, fail or not-observed with no
fourth outcome: the **reach** control (does the picker draw the entries the
plugin pushes) and the **build** check (does the game build and place our item
from a rewritten struct).

- Reach pass and build pass (every hit of ours built exactly one item, the
  game's own ground-loot count rose by one per hit, the forge hook dressed it,
  no vanilla item for that hit, no crash): **route inject**. The design above
  ships; the replace mode stays research-only.
- Reach pass and build fail, and `replace-remove` (run only then) pass:
  **route replace**. On a hit of ours the game places the stand-in, the
  plugin removes it and spawns ours. Replace still rests on the injection
  reaching the picker, which is why it is never taken on a reach that failed
  or was not measured. If `replace-remove` fails as well, **route
  not-observed** and the owner decides; if it is not-observed, **no route**,
  as below.
- Reach fail: **route not-observed**, recorded as a measured negative (the
  picker does not draw what the plugin pushes, on every candidate list the
  probe printed). Replace is not run, since it would only rewrite a share of
  the stand-in's own hits. The owner decides; registering a definition in the
  repository is the research route left.
- Reach or build not-observed (too few hits to decide): **no route**. Nothing
  is concluded about injection, the session record says how many typed hits
  were seen, and the owner decides whether to run another session. A short
  sample never selects a route.

Build is read only after reach passed. With reach failed or not-observed, the
build counters are recorded as printed but carry no verdict, because the
plugin's coin and rewrite produce every one of them on a push the picker never
saw. Not-observed always means *not powered*: the batch reached its cap
without the hits a verdict needs. It goes to the owner and is never evidence
that injection does not build the item, so it never selects `route replace`.
Replace ships only on a build that failed under a reach that passed.

So only a reach that passed can ship the inject route, and only a reach that
failed can close the inject line.

`inject-build` and `replace-remove` are never pass conditions of the session:
their verdict is the finding. `off-removes` and `list-restored` check
behaviour that ships, and must pass whichever route is taken.

### Live procedure 3

Session 3 runs the research build (`plugin_build\BloodPactPlugin_rel.dll`,
built with the literal `dev`) through the drive tool, with the owner doing the
killing, on save slot 14 (Sorak), selected on the back end and in town at
load. The step-by-step procedure (the exact commands, the standing steps and
the four short kill batches) is `### Live procedure 1` in the workorder's
context file, `.claude/workorders/forgepact-74-angelic-list-injection-context.md`,
which stays on the owner's machine; it is this document's third live
procedure, hence the heading here. Hygiene is Session 2's: a fresh session; no
`citrace nativetrace`, `raredrop ceiling`, `scount`, `zonegenlog` or
`angelicwatch`; and no `angelicprobe on`, whose `default-params` and
`unique-repo` rows would take the `CreateDefaultParams` and
`GetUniqueRepoStruct` detours this feature needs. The one people step
before the kill block is a portal or waypoint to an ordinary zone, so the
owner can leave once the kills are done.

The research build's levers stay under `angelicprobe`, because the command
dispatcher's branch count and the `angelicprobe` literal are both pinned by
`test_angelic_probe_contract.py`:

- `angelicprobe list` (rewritten) finds `Controller_obj` by name, counts its
  instances, reads the first instance's variable names, and for every variable
  shaped as an array of three-number arrays prints its name, length, first
  three entries and how many entries equal each stand-in's triple. The global
  scope and `Loot_Manager_obj` keep one line each, both expected empty. It
  ends with one summary line naming the number of candidates and the best one
  with its length.
- `angelicprobe inject` takes `name` (a variable, or `auto` for the probe's
  best candidate), `mode` (`inject`, the default, or `replace`, the fallback),
  `copies <k>` (how many copies of each enabled stand-in one roll pushes,
  1 by default; the attribution coin becomes k / (n + k) and the tail check
  before removal covers all k) and `status`, which prints the copies, the mode, the list's name and length, each
  item's stand-in with its count n, and the `injected=`, `ourHits=`,
  `built=`, `removed=` and anomaly counters.
- `angelicprobe hit` keeps its chance, rate, off and status levers. Each hit's
  `angelic hit:` line in this build also carries `lootDelta=` (ground-loot
  instances after the original call minus before) and, for the first three
  hits of ours, the parameter struct's JSON before and after the rewrite.
- `sigdrop status`, in both builds, reports the force, then `gameRolls=`,
  `gameHits=`, `injected=`, `ourHits=`, `untyped=`, `built=`, `crown=`,
  `belt=`, `list=`
  (`none` until a switch resolves it, then the name and length, or `missing`
  after a failed resolution) and `gate=` per item, and ends with
  `cdpCalls=<n> detect=<route>`.

Each check is recorded as pass, fail, not-observed or not-run (and
`force-hit` also as instrument-blind), with the replies quoted in the session
record:

- **`dll-hash`** - the installed DLL's SHA-256 equals the research build's,
  recorded when it was built. Fail: nothing after it counts.
- **`marker`** - `angelicprobe hit status` names every lever off and ends
  `detect=off`. A player build answers that the command is unavailable, and
  the session ends there.
- **`control`** - `sigdrop status` shows the force off, every counter at
  zero, `list=none`, both gates off, and ends `cdpCalls=0 detect=off`.
- **`list-scope`** (research) - `angelicprobe list` finds one
  `Controller_obj` instance, at least one candidate, and a best candidate of
  length 100 or more whose shown entries are three numbers each, with n for
  each stand-in (expected 1). The name and the global and `Loot_Manager_obj`
  lines are recorded as printed. No candidate is not-observed: every variable
  name printed is recorded, and the session stops after the baseline, since no
  injection is possible.
- **`repo-standin`** (research) - `angelicprobe inject name auto`, then its
  status, names the list with its length and each item's stand-in with n = 1:
  Headhunter with Liquor Holster, Tyrant's Crown with the chosen helmet. A
  stand-in the pool rejected reads not validated, and that item's switch
  cannot arm. This check shows only that `auto` chose a list of the right
  shape. Whether it is the list the roll reads is `inject-build`'s reach
  control, so every candidate `list-scope` printed is kept in the record for
  that control to fall back on.
- **`list-stable`** (research) - after the owner takes a portal or waypoint to
  an ordinary zone, `angelicprobe list` names the same best variable with the
  same length. A different length is the finding, and both are recorded.
- **`baseline-off-vanilla`** - with the Angelic gate opened through
  `raredrop angelic 2` and the chance lever at 1000000000, ten kills. First
  the detection's own positive control: `detect=detoured` and `cdpCalls`
  above zero (otherwise `force-hit` is instrument-blind and the checks that
  need a hit do not run). Then at least ten `gameRolls` and one `gameHits`
  pass `force-hit`; with both switches off, `injected=0`, `ourHits=0`,
  `built=0`, the list's length equal to `list-scope`'s and no injection line
  in the log pass `baseline-off-vanilla`. Only the game's own Angelic and
  Unholy items lie on the ground.
- **`inject-build`** (the route's first branch, recorded as its two parts'
  verdicts, reach and build, per the decision rule above) - `headhunter force`
  turns Headhunter on and logs one line naming the list and Liquor Holster;
  the gate reads Headhunter on, Tyrant's Crown off. Then
  `angelicprobe inject copies 47`: each roll pushes 47 copies of Liquor
  Holster's triple, about as many entries as pass the picker's filters
  (Session 2's validated pool held 47). One push of one entry cannot be
  measured: it moves the triple's share of hits from about 1/47 to 2/48,
  which a session's rolls cannot tell apart. 47 copies move it to about
  48/94, one hit in two, which ten kills can. Session 2 measured about 47
  rolls per ten-odd kills, and with the chance lever every roll hits, so ten
  kills give about 45 typed hits. The thresholds below are counted in hits,
  which here are rolls, not in kills; the kill count only bounds the owner's
  time. After ten kills each part is read. A part still short of its
  threshold (H below 20, or one or two hits of ours) is not a verdict: a
  second batch of ten kills runs, and that is the cap. A part still short at
  the cap is not-observed, which the decision rule routes to the owner.
  - **Reach**, the injection's positive control, read off what the picker
    drew and not off the plugin's own counters. Count the typed `angelic
    hit:` lines (H) and, among them, those naming the stand-in's triple
    (type 8, sub 0, b 51), ours or vanilla alike (S); untyped hits count in
    neither and are recorded. Pass: H at least 20 and S at least H / 4.
    Fail: H at least 20 and S below H / 4. Not-observed: H below 20 at the
    cap. A push the picker cannot see (a copy of the array, another array
    of the same shape, a `Controller_obj` instance the roll does not read)
    leaves the triple at its vanilla one hit in 47, whatever `injected=`
    says. At H = 20, a quarter or more without reach has a probability of
    about 5 in 100,000, and under a quarter with reach about 5 in 1,000; at
    H = 40, about 1 in 100,000,000 and 2 in 10,000. S / H is recorded beside
    the 48/94 expected; a share near it also shows that each pushed copy
    counts as one entry. Before fail is recorded, when `list-scope` printed
    more than one candidate, the control is repeated with `angelicprobe
    inject name <candidate>` for each other candidate, ten kills each; a
    candidate that passes is the list the roll reads, and its name is the
    one recorded for the player build. `injected=`, `ourHits=`, `built=` and
    a Headhunter on the ground are never reach evidence: the plugin's own
    coin and rewrite produce all four on a push the picker never saw.
  - **Build**, from the same batches, read only once reach passed. With 47
    copies the coin reads 47/48, so once reach holds a typed hit is ours with
    probability 48/94 times 47/48, which is one in two. At H = 20 a working
    injection then gives fewer than 3 hits of ours about 2 times in 10,000.
    Pass: `injected=` grew by
    47 times the growth of `gameRolls=`, `ourHits=` at least 3, `built=`
    equal to `ourHits=`, `belt=` equal to `built=`, `crown=0`, every hit of
    ours with `lootDelta=1`, no `sigdrop:` line, and the owner names a
    Headhunter on the ground. Typing is checked on every hit: each `angelic
    hit:` line names a triple whose sub and b equal its parameter struct's,
    and `untyped=` stays 0. Fail: a crash, fewer built than hits, a
    `lootDelta=` other than 1 on a hit of ours, a struct missing a field, a
    ground item that is not Headhunter, or a hit of ours whose triple is not
    Liquor Holster's. An `untyped=` above zero is recorded with its hit lines
    and means the definition-read typing was not observed as read. One or
    two hits of ours continue to the cap. Not-observed: fewer than 3 hits of
    ours at the cap, recorded as not powered, never as replace evidence.
  - **The shipped count**, last: `angelicprobe inject copies 1` and five
    kills. `injected=` grew by the growth of `gameRolls=` (one push per
    roll), no anomaly line, and afterwards the list's length equals
    `list-scope`'s. This batch measures no rate and records none. That the
    shipped single entry carries one entry's share is inferred from reach
    and from the picker drawing every entry alike (static reading), not
    measured at one copy.
- **`on-both`** (not run unless `inject-build`'s reach and build both
  passed): `angelicprobe inject copies 47` again and Tyrant's Crown on as
  well; ten to twenty kills, then `copies 1`. At one copy a crown of ours is
  about one roll in 48, about two in twenty kills, too few for a verdict; at
  47 copies of each stand-in both items are common. `built=` grew by the growth of `ourHits=`, `crown=` reached at
  least 3 and `belt=` grew, and both items are seen on the ground.
  Not-observed: `crown=` below 3 after 20 kills.
- **`off-removes`** - both switches off; the gate reads both off and the list's
  length equals `list-scope`'s. Five kills: `injected=` and `ourHits=`
  unchanged, no mod item, no injection line.
- **`replace-remove`** (research fallback, run only if `inject-build`'s reach
  passed and its build failed) - replace mode, `copies 47` and Headhunter on;
  ten to twenty kills. Pass: at least 3 hits of ours, every one saying it
  removed the stand-in and spawned Headhunter, `removed=` equals `ourHits=`,
  `lootDelta=1` after the swap, another ground item picks up without an
  error, and `lootcensus` runs. Not-observed: fewer than 3 hits of ours after
  20 kills. Headhunter off, inject mode and `copies 1` again afterwards.
- **`sigdrop-still-forces`** - `sigdrop crown` and one kill drop a Tyrant's
  Crown, then `sigdrop off`. The last people step.
- **`list-restored`** - every lever off and the Angelic gate vanilla again;
  `angelicprobe list` names the same variable with `list-scope`'s length, and
  no stand-in triple appears more often than its vanilla n.

### Results

Session 3: not yet run.
