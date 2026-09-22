# The game's own Angelic roll: which step picks the unique (issue #64)

**Status: instrument built, live session not yet run.** Nothing player-visible
changes in this round. Every `## Results` row and every `## Decision` line
reads `pending` until the owner-run session under `## Live procedure` fills
them in, and no reader should treat a `pending` row as a negative. A candidate
is only ever named in `finding:` if its own route's positive control passed in
the same session (`AGENTS.md`, "Prove the Instrument Before Trusting a
Negative Result").

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
static reading: `DropItem` asks whether the player carries buff 332
(`buff_angelic_chance`) and, only then, hands the summed buff value and the
kill position to `DropItemAngelicChance`, which picks from `lootListUnique` on
`Loot_Manager_obj`, reads the definition through `GetUniqueRepoStruct`, skips
the development-only entries, rolls against that definition's base drop rate
and, on a hit, builds the item and places it on the ground.

None of the steps after the buff check has been *measured* on the current
build: whether a hook on `DropItemAngelicChance` sees the game's own call,
what it returns, whether one call considers one candidate or walks the whole
list, and which named script places the item. This round measures those, in
one build and one session.

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
| `angelic-chance` | `DropItemAngelicChance` | detoured | C1 (L-1.7): calls at least 1 and a finite `lastChance=` |
| `angelic-forced` | `DropItemAngelic` | via `Hook_DropItemAngelic` (native) | C1 (the via route) |
| `drop-boss` | `DropItemBoss` | via `Hook_DropItemBoss` (native) | C1 (the via route) |
| `drop-heroic` | `DropItemHeroic` | detoured | C1 (own detours) |
| `drop-debug` | `DropItemDebug` | detoured | C1 (own detours) |
| `drop-unique` | `DropUniqueItems` | detoured | C1 (own detours) |
| `unique-random-id` | `GetUniqueRandomItemID` | detoured | C1 (own detours) |
| `unique-repo` | `GetUniqueRepoStruct` | detoured | C1 (own detours) |
| `unique-charm` | `GetUniqueCharm` | detoured | C1 (own detours) |
| `angelic-charm` | `DropAngelicCharm` | via `Hook_DropAngelicCharm` (native) | C1 (the via route) |
| `angelic-key` | `DropAngelicKey` | via `Hook_DropAngelicKey` (native) | C1 (the via route) |
| `default-params` | `CreateDefaultParams` | detoured | C1 (own detours) |
| `loot-create` | `LootGroundCreate` | detoured (under table-only `Hook_LootGroundCreate`) | C2 (L-1.12) |
| `loot-create-item` | `LootGroundCreateFromItem` | detoured (under table-only `Hook_LootGroundCreateFromItem`) | C2 (L-1.12): calls at least the drops `angelicdrop status` reports |
| `loot-drop` | `LootGroundDrop` | detoured | C1 (own detours) |
| `rare-announce` | `GetRareDropAnnouncement` | detoured | C1 (own detours) |

One positive control per route, in the same session:

- **C1, own detours and the via route (L-1.7).** With the gate opened by
  `raredrop angelic 2` and at least 30 ordinary kills: `kills=` at least 30
  with the kill control native, the `angelic-chance` row (own detour, through
  `HookAngelicChance`) at least one call with a finite chance, and the
  `drop-item` row (via DropManager's native hook) at least one call. Zero on
  either with kills counted means that route was blind this session, and
  every row on it is `unmeasured`.
- **C2, the route under a table-only hook (L-1.12).** ForgePact's own
  `angelicdrop` places each item by calling `LootGroundCreateFromItem` by
  name, so the `loot-create-item` row has to count at least as many calls as
  `angelicdrop status` reports drops. Those calls sit outside both depths,
  which is how they stay distinguishable from the game's.
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
(`angelic: gate found ...` or the reason it was not) is read at L-1.2. The
player build has the same exposure after any `dropmult` and is tracked
separately as ForgePact issue #69.

### Where it runs

`angelicprobe` is dispatched like every other command, inside
`PollCommands()` on the game thread between frames. Nothing it adds sits on
the per-frame path; after `on`, the only probe code that runs is inside the
hooked calls themselves.

### What the follow-up needs

The follow-up workorder (the substitution itself) reads these from
`## Results`:

1. Whether `angelic-chance` saw calls on route `detoured`, and every other
   row's route.
2. The argument count, and which argument is the chance.
3. The roll's return kind and value on a miss and, if one is seen, on a hit.
4. The `unique-repo` inside count per `angelic-chance` call: one means a
   single pick per roll; many means the list is walked.
5. Which of `loot-create`, `loot-create-item`, `loot-drop` and
   `default-params` has a nonzero inside count.
6. `lootListUnique`'s length and entry shape.
7. Whether case B (the real buff) reached the same rows as case A (the opened
   gate), and the logged chance under `buffme 332`.

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
  own.

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
  call and a finite `lastChance=`, and `drop-item` is `via Hook_DropItem
  (native)` with at least one call. The `drop-item` count against `kills=` is
  a result, not a pass condition. Also read the last 60 log lines for the
  `angelic: roll` lines.
- **L-1.8** From the same reply: the `unique-repo` inside-angelic-chance
  count, and every placement row's inside counts (expected zero on misses).
- **L-1.9** `raredrop angelic 1` - expected `off (vanilla)` and `gate closed,
  original bytes restored`; then `angelicprobe reset`.
- **L-1.10 (case B)** `buffme 332 3000 3000 18000`; the owner kills at least
  30 ordinary monsters within five minutes; then `angelicprobe show`.
  `angelic-chance` with at least one call and the gate closed means the real
  branch ran; record `lastChance=` (3000 means the buff value arrives as
  passed). Zero here after C1 passed is a real negative for `buffme`'s value
  shape, not for the hook.
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

A game-roll hit is not expected in one session (a chance around 3000 against
base rates in the millions), so a hit's placement rows stay `not observed`
unless one happens.

## Results

| step | reply |
| --- | --- |
| L-1.1 | pending |
| L-1.2 | pending |
| L-1.3 | pending |
| L-1.4 | pending |
| L-1.5 | pending |
| L-1.6 | pending |
| L-1.7 | pending |
| L-1.8 | pending |
| L-1.9 | pending |
| L-1.10 | pending |
| L-1.11 | pending |
| L-1.12 | pending |
| L-1.13 | pending |

## Negative results, sourced

- **`LootGroundCreate` "was never called at runtime (measured: 0)"** - a
  comment beside the research build's `LootGroundCreate` hook. That zero came
  from the table-only installer, which cannot see the game's direct calls
  (`AGENTS.md`, "Prove the Instrument"). It is **not observed through a table
  hook**, not a fact about the game; the `loot-create` row re-measures it on a
  route that can see direct calls, with C2 as its control.
- **No buff, no roll** - `docs/angelic-drop-research.md`: 984 kills without
  buff 332 produced 0 calls to `DropItemAngelicChance`. Measured through the
  hook of that date, which was a direct detour; kept as the reason case A
  opens the gate.
- **The gate cannot be found after startup in the research build** - code
  reading only (see `## Instrument`), **not observed live**. L-1.2 records what
  the startup lookup actually printed.

Each zero this session produces is added here after the session, labelled
`not observed` with the control that backs it (C1 for the own-detour and via
rows, C2 for the two loot-creation rows); a row whose route was `TABLE-ONLY`,
`blocked` or `not found` is `unmeasured`.

## Decision

Pending the live session. Each candidate's line opens with `works`, `not
observed` or `unmeasured` once it has run, and `finding:` names only
candidates labelled `works` - the step (or steps) the follow-up hooks to
substitute Headhunter or Tyrant's Crown at Liquor Holster's share - or reads
`none`, with the reason sourced above.

* `drop-item` - **pending.**
* `angelic-chance` - **pending.**
* `angelic-forced` - **pending.**
* `drop-boss` - **pending.**
* `drop-heroic` - **pending.**
* `drop-debug` - **pending.**
* `drop-unique` - **pending.**
* `unique-random-id` - **pending.**
* `unique-repo` - **pending.**
* `unique-charm` - **pending.**
* `angelic-charm` - **pending.**
* `angelic-key` - **pending.**
* `default-params` - **pending.**
* `loot-create` - **pending.**
* `loot-create-item` - **pending.**
* `loot-drop` - **pending.**
* `rare-announce` - **pending.**

finding: pending
