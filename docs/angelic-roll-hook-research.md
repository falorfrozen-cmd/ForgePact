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
