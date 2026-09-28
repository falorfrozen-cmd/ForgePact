# Development 2 bug batch: #93, #94, #77, #83, #80 and #95 part 1

Five findings from the Development 2 round ship together in ForgePact 2.0.1
(2.0.0 was published on 2026-09-28, before this batch merged): the relic filter now
counts relics worn in the equipped slots (#93), the pet moves on from ground
loot it cannot pick up when the new, off-by-default `petunstick` mod is on (#94,
the game's own companion loot pickup; its reading is in
[pet-loot-stuck-research.md](pet-loot-stuck-research.md)), `dropmult gold`
scales one coin's amount instead of creating a hundred or ten thousand coins
(#77), Mana Orb with the Chosen One upgrade shows the timed-skill countdown
(#83), and every refused craft press names its own reason (#80). #95 part 1 is a
measurement only: what a ground item the loot filter hides still costs. The Pet
Quest Collector also gained a target selection that holds back a quest item it
could not collect and walks the whole quest-item family, an improvement of its
own with no issue number: this batch first read #94 as that collector's bug,
and the owner corrected the report on 2026-09-27 (§ "#94" below). This document
is what those fixes were built on.

Everything below is labelled. **Measured** means observed on the running game,
and each measurement names the check of the session that took it. **Static
reading** means read from the game's compiled code in a local decompiler and
written here in our own words; no game code is quoted (AGENTS.md § Legal). A
**reading of our own code** is ForgePact's source, read to find where a symptom
comes from. A thing that was looked for and not seen is written "not observed",
never "does not happen". The game facts established here are also recorded in
the hub's `docs/RUNTIME_DATA_MODELS.md` (§1, §2, §7.3, §10.3, §13.10, §18.5),
the shared record every module reads.

## Status

| Issue | What was established | Fix | Checked by |
|---|---|---|---|
| #93 | **Measured** (Live 1): the equipped relic slots 10-14 are fingerprint strings in a global, and the scan through them found the three maxed relics the save shows | `hs-game-sdk`'s relic scan reads the equipped slots, both bindings | Live 1's `relic-scan-count` (research build); Live 2's `on-relic-scan` (player build) |
| #94 | **Static reading** of the game's companion loot pickup (`Companion_obj`): a target is replaced only once it ceases to exist, so an item the pickup keeps failing on pins the pet; no session measured it ([pet-loot-stuck-research.md](pet-loot-stuck-research.md)) | `petunstick` (off by default): an item the pet has stayed on for 1.5 s is held back through the game's own `itemCompanionTimer` when `variable_instance_exists` says the item carries it (otherwise nothing is written to the item and it is counted `timer absent=`), and either way the pet's target and loot list are dropped; `re-picked while held=` counts a given-up target the pet takes back within the hold | Live 1 of the workorder `forgepact-pet-loot-stuck` (player DLL): the stuck state did not reproduce and the mod gave nothing up, so not observed; shipped off by default ([pet-loot-stuck-research.md](pet-loot-stuck-research.md#live-1-results-2026-09-28)) |
| Pet Quest Collector (first filed here as #94) | **Reading of our own code** only; no session measured it | target selection that remembers a failed target, and a cursor over the quest-item family (no issue number) | none: Live procedure 3 (the owner's own test) was not run, so no session confirms it |
| #77 | **Measured** (Live 1): `dropmult gold 100` makes 10,000 coins per monster gold drop and stalls the game for seconds, at the drop and again at the pickup | gold multiplies one coin's amount; each gold script runs once | Live 2's `on-gold-amount` |
| #83 | **Measured** (Live 1) with Chosen One: the orb's own timer counts down across the cast, and the countdown's rule never selected Mana Orb. Without Chosen One: not observed | an explicit countdown row for Mana Orb (`manaorb-route: object-timer`) | Live 2's `on-manaorb-countdown` |
| #80 | **Reading of our own code**: one refusal kind covered four different causes, and the hash call's result was dropped | one kind, counter and line per cause; `hash-failed` when `ItemCheckHash` did not run | Live 2's `on-craftmats-press` and `no-new-refusal` |
| #95 part 1 | **Measured** (Live 1): a filtered item stays a live, invisible instance; 281 of 291 ground items were hidden at a strict filter | none: measurement only | Live 1's `loot-hidden-count`, `loot-showall-control`, `loot-frame` |

Live 1 ran on 2026-09-27 (research DLL sha256 `87ad8265...32a6`, slot 14
"Sorak", a White Mage) and Live 2 on 2026-09-27 (player DLL sha256
`c36004c6...8e6b`, the same slot): all eight of Live 2's checks passed
([Live 2 results](#live-2-results-2026-09-27)). The owner's Live procedure 3
was not run. The batch ships in ForgePact 2.0.1, since 2.0.0 was published
before it merged. The 2.0.1 player DLL, built afterwards from the tree merged
with ForgePact's `main`, has not been run by any session; the results here
belong to the DLLs they name.

## #93: the relic filter did not see equipped relics

**What the save shows** (read with the Item Editor's save decoder, read-only, on
2026-09-27). A character save's `[inventory]` section holds one base64 JSON
value whose top-level keys are `potions`, `equipped_items`, `personal_stash` and
`minion_inventory_melee`. `equipped_items` is a dict keyed `0-0-<stamp>-<class>`;
the key's trailing number is the item class (16 = relic), and each value is
`{"data": {...}}`. On the five equipped relics `g` is the slot (10-14), `o` the
level, `b` the relic id, and `c` is **0** (1 on unique gear). No relic
definition carries `relicLevel`, `cls` or `itemType`. Sorak's five were
`g 10: b 15 o 8`, `g 11: b 135 o 10`, `g 12: b 124 o 10`, `g 13: b 140 o 9` and
`g 14: b 109 o 10`, so the maxed set is `{109, 124, 135}`.

**Why the scan found nothing** (reading of our own code). The SDK identified a
relic by `c`, `cls` or `itemType` equal to 16, or by a `relicLevel` field, on
whatever struct it walked, and it descended only into `data`. A game relic's
definition matches none of those, and the item class lives on the item
**instance** beside `itemDefinitionStruct`, which the walk never reached from the
player's containers. The equipped slots were not among those containers at all.

**Measured, Live 1** (2026-09-27, research build, checks by name):

- `relic-global-slots` pass. `gjson equippedItems` read an array of six
  per-player entries; only index 1 was populated, and `gjson mplr` read
  `real:1.000000`, so the offline local character is player 1, not 0. Entry 1
  holds two arrays of 18 strings each. The first, `equippedItems[1][0]`, is the
  worn gear: indices 10-14 hold five `0-0-<digits>-16` strings, the five relics.
  The other slots hold the same key shape with each item's own class as the
  suffix (index 9, the off hand, ends in `-7`), and indices 15-17 were empty.
  The second array, `equippedItems[1][1]`, holds seven fingerprints in slots
  0-8; what it is was not established.
- `relic-instance-var` pass. `ijson equippedItems` answered that the player
  instance has no such variable. The equipped items live only in the global.
- `relic-scan-count` pass. After `relicfilter 0` and `relicfilter 1`:

  ```
  relicfilter: scan found 3 maxed relics (ids 109,124,135)
  relicfilter: equipped slots mplr=1 slots=18 inrange=5 strings=5 owner=ok resolved=5 refused=0 nonstruct=0 noclass=0 relic=5 otherclass=0 relics=10:15@8,11:135@10,12:124@10,13:140@9,14:109@10 control=resolved itemType=0 stopped=none
  ```

  The same two lines had already appeared at character load. Every fingerprint
  resolved, every result was a relic instance, and the five `slot:id@level`
  pairs equal the save's `g`/`b`/`o` exactly, so the in-memory route and the
  save agree. The positive control, the helmet slot through the same resolver,
  resolved to an item of class 0.

**Not exercised:** a relic drop. Relics roll only in Satanic zones, and the
arm-time line is the check instead of a relic farm.

**The fix.** `hs-game-sdk`'s scan (both bindings) now recognises an item
instance by `itemType == 16`, reading id and level from its definition's `b` and
`o`, and a save entry by the key's class; the C++ scan also reads slots 10-14 of
`global.equippedItems[mplr][0]`, resolving each fingerprint through the game's
own scripts by name (`GetOnlinePlayerItemOwner`, then `GetItemFromFingerprint`,
with the global instance as self and other). ForgePact logs the two lines above
once per arm, in both builds, so a player's log names what the filter found and,
for a zero, the stage that stopped.

<a id="94-the-pet-circles-one-quest-item-when-many-are-on-screen"></a>
## Pet Quest Collector target selection (first read as #94; #94 is the companion's loot pickup)

**Corrected 2026-09-27.** This section is the batch's first reading of #94, and
that reading was wrong about which pet code the report meant. The owner
corrected it: #94 happens "when there are lots of things for pet to pick up.
Not necessarily quest item. Can be gold stacks, runes, other socketables like
rubys, crafting materials etc." That is the game's own companion loot pickup
(`Companion_obj`), not the Pet Quest Collector, and it is answered by
`petunstick`; its static reading and the fix are in
[pet-loot-stuck-research.md](pet-loot-stuck-research.md). What follows is kept
as the design record of the Pet Quest Collector's target-selection change,
which ships as an improvement of its own with no issue number; nothing below is
the explanation of #94.

Live 1 did not measure #94. What follows is a **reading of our own code** plus
one earlier **static reading**, and the fix design built from them. The check is
the owner's own test on the final player DLL (Live procedure 3, below).

**What the tick rules out.** `PetQuestCollectorTick` chooses a target only while
idle and keeps it through the travel until arrival, a 240-frame timeout, or the
item ceasing to exist. So the pet cannot switch targets mid-travel; circling
between two items would need two idle picks.

**What it cannot rule out.** After each collect the idle step picks the nearest
collectable item again, with no memory of what just failed. Three outcomes each
make the same item the next target, forever:

- the collect was dispatched but the item stayed (`dispatched-but-item-remained`);
- the game's own gate refused the item on arrival (`skipped(gate)`);
- the travel timed out (`travel timeouts`).

That is the reported symptom's shape. The first is plausible exactly when many
items are on screen: the quest pickup reads the objective's progress and its
maximum before it updates the quest (static reading, `pet-quest-collector-c-research.md`
§ 7), so an item whose objective has just filled can be collected without being
consumed.

**A second cause, for items ignored rather than circled.** The idle step reads
at most 64 instances of the quest-item family per tick, and that family also
holds the static props the mod excludes. With many props, collectable items past
index 63 are never candidates.

**The fix design.** No session named which cause the owner met, and the design
does not need one, because the selection has no memory of failure in any of
them. The target selection moves into `PetQuestCollectorMod.hpp` as a
game-independent decision: candidates as id, squared distance and whether they
are collectable, a bounded set of recently failed ids each with a retry frame,
and the current frame. It picks the nearest candidate not held back. A target
whose collect left the item in place, whose gate refused it at arrival, or whose
travel timed out is held back for a fixed number of frames and counted
(`held back=` on `petquest 0`'s lines). With every candidate held back the pick is
none, so the pet waits rather than spins. A cursor over the family resumes where
the per-tick budget stopped, so every instance is reached within a few ticks.
Baseline test: the old rule (nearest, no memory) picks the failed id again at
once. Target tests: the next pick after a no-effect collect is another item; the
held item is eligible again after the hold; with all held back the pick is none;
the cursor reaches an item at index 70 within two ticks of a 64 budget.

## #77: `dropmult gold 100` froze the game

**Static reading** (2026-09-27). `DropMonsterGold` reads its six arguments,
applies the profile's gold getters, rounds an amount down and calls `DropGold`
**once**, directly (a compiled call, which `HookOneScript`'s inline detour
intercepts), with nine arguments; it has no loop. `DropGold` reads the gold-find
values, creates **one** instance, and sets that coin's value with one call to
the coin's own `m_SetGoldValue` method, then sets its spread and log fields. One
call, one coin, one value.

**Why x100 compounded** (reading of our own code). The drop hooks called the
game's original `m_Mult` times, and `dropmult gold` set both `DropGold` and
`DropMonsterGold`. Each of the hundred `DropMonsterGold` originals reaches the
detoured `DropGold`, which runs its original a hundred times: 10,000 coins per
monster gold drop, each processed again at pickup.

**Measured, Live 1** (research build; `goldtrace` logs each gold call's
arguments, `lootcensus` counts `Coin_obj` instances by SDK name):

- `gold-baseline` fail, on the frame budget only. At x1, about 60 s of killing:
  `Gold c=4` and `MonsterGold c=4`, one inner call per outer call. `perf` read
  `avg 7.27 ms, max 284.1 ms`, and the expectation was a max under 100 ms. No
  multiplier was active, so that stall is not #77's; its cause was not
  established.
- `gold-args` pass. Four drops, each a `DropGold` line then its
  `DropMonsterGold` line:

  ```
  DropGold argc=9 a0="<40-hex string>" a1=11024 a2=4528 a3=1 a4=51 a5=kind5 a6=kind5 a7=kind5 a8=kind5
  DropMonsterGold argc=6 a0=11024 a1=4528 a2=0 a3=1 a4=0 a5=0
  ```

  (`a0` was the same string on all four calls and is shortened here.) `a1`/`a2`
  are the drop's position and equal `DropMonsterGold`'s `a0`/`a1`; `a3` read 1
  and `a5`-`a8` kind 5, undefined, on every call. Only `a4` varied per drop: 51,
  59, 31, 29. That is the amount's shape and the static reading's candidate, but
  it was not cross-checked against a gold figure the game showed.
- `gold-x100-nesting` pass. At x100 two gold drops moved `MonsterGold c=` from 4
  to 6 and `Gold c=` from 4 to 204: a ratio of exactly 100.
- `gold-x100-instances` pass. `lootcensus` read `coins=0` before and
  `coins=20000` after those two drops, 10,000 per drop, while `ground=` read 23.
  So `DropGold`'s coin is a `Coin_obj`, and coins are not `Loot_Ground_obj`
  instances.
- `gold-x100-frame` pass. `perf` read `max 8400.2 ms` across the drop. The owner:
  "spawning it froze the game", "game completely turned into 1 fps slideshow".
  Walking over the pile then read `max 6535.4 ms` in a fresh window and
  `coins=0` afterwards, and the owner: "collecting coins process moved fps back
  to sub 1fps. after coins were collected it went back to 144 fps". The freeze
  happens twice, at the drop and at the pickup, and ends once the coins are gone.

**The fix.** Gold becomes an amount multiplier, the "one value inside the game's
own call" shape the owner asked for during the session ("i think we should make
the multiplier to the value of the coin, not the amount of coin objects
created"). `DropGold` and `DropMonsterGold` get their own hook bodies: the
monster-gold original runs once whatever the multiplier, and the `DropGold`
original runs once with argument 4 multiplied, when it is a finite number. The
player build logs `dropmult gold: x<n> applied to the coin's amount (one coin
per drop)` the first time it scales in a session. Every other `dropmult` target
keeps its count semantics.

## #83: Mana Orb showed no countdown with Chosen One

**Static reading** (2026-09-27). `White_Mage_Mana_Orb_obj`'s Create runs its
parent's Create, then sets `destroyTimer` to three seconds of game speed (432
frames at 144), the orbit fields to 0, `chosenOne` and the other upgrade flags to
0, and the pulse timer from the game speed. Its Step orbits `host` when
`orbitRadius` is above 0, and with `chosenOne` truthy it sets the orb's position
to `host`'s every frame: the orb follows the player. The pulse it spawns copies
`chosenOne` and the other upgrade fields. Step never touches `destroyTimer`; the
inherited parent step counts it down. On this reading Chosen One changes where
the orb is, not what carries its duration.

**Measured, Live 1**, with Chosen One (Sorak's loadout: node s12 at 3), Mana Orb
cast from bar slot 0,3:

- `manaorb-rule-entry` fail: absent. `skilltimer stat` listed 17 rule rows and
  none was Mana Orb (talent 253), before and after the cast: `ruleRows=17
  ruleDenied=44 ruleUnreadableFields=0 ruleNoName=260`. The rule tier selects a
  talent only with a positive `abilityDuration` and a cooldown above 0.25 s,
  and many timed effects read `abilityDuration` 0 (hub guide §7.1). Whether
  that is the reason for talent 253 was not read.
- `manaorb-chosenone-flag` pass: `chosenOne (bool:true)`, `orbitRadius
  (real:0.000000)`.
- `manaorb-timer-chosen` pass. `destroyTimer` read `4151.708064` within 2 s of
  the cast and `1599.867504` about 18 s later (by the two screenshots' clocks),
  2551.84 frames, about 142 per second: a countdown spanning the cast. The
  duration sweep's row for the object: `first=5040.000000 last=-0.876816
  draws=5033 own=unreadable`. So the orb starts at 5040 frames (35 s), not the
  Create's 432: something lengthens it after the Create, and which script does
  was not read.
- `manaorb-draw-chosen` fail: none. No draw was counted, as expected with no
  rule entry.

**Not observed:** the orb without Chosen One. After the owner reset the talents
and learned Mana Orb again without s12, the cast was refused
`cast_not_confirmed` (slot 0,3's effect count did not move in 10 s), so
`manaorb-timer-plain` and `manaorb-draw-plain` have no reading.

**The fix.** `manaorb-route: object-timer`: the orb's own `destroyTimer` spans
the cast, so Mana Orb gets an explicit countdown row on `White_Mage_Mana_Orb_obj`,
measured in `toggle-skills-research.md` § "Duration sweep (session 8)" →
"Results" from this session. The release notes claim only the Chosen One case,
the one measured.

## #80: every refused craft press said `unreadable`

**Reading of our own code**, no game reading and no Live 1 check. The craft-from-
stash press gate (`CraftMatsMod.hpp`, `PressStep`) refused a press for four
different reasons under one kind, `unreadable`: the record had already served a
press, the counts could not be paired with the recipe's amounts, the recipe row
had no number, or the record belonged to another row. The press's line and its
counter could not say which. Separately, the adapter's count edit called
`ItemCheckHash` by name and dropped the call's result, so a hash call that did
not dispatch still let the take read as confirmed.

**The fix.** Each cause has its own kind, name, line and counter:
`already-served`, `unpaired`, `unnumbered-row` and `other-row`, and each line
says the craft was refused and nothing moved. A count edit whose `ItemCheckHash`
call did not dispatch now returns failure, feeds a seam in the core that marks
the take unconfirmed, counts `hash-failed` and logs it once per session, and
the press is refused. `crafting-materials-research.md` § "Ship design" links
here. Live 2 checks one ordinary press and that none of the five lines appears
(`no-new-refusal`).

## #95 part 1: what a hidden ground item still costs

**Static reading** (2026-09-27). `Loot_Ground_obj`'s Create sets
`lootFilterVisible`, `lootFilterHighlight`, `skipLootFilter`, `inviewCheck`,
`itemCompanionTimer`, `visible` and alarm 4, and binds `m_LootFilter` and
`m_LootGroundDeActiveStep`. Its Alarm 9 reads `lootFilterVisible`, sets
`visible` from it, and re-arms itself for 0.3 s of game speed. So an item the
filter hides stays a live instance that re-checks its visibility every 0.3 s. The
hub guide §18.6 adds, also static, that Alarm 9 sets visibility from the screen
as well as the filter.

**Measured, Live 1** (`lootcensus`, research build, read-only: counts
`Loot_Ground_obj` and `Coin_obj` by SDK name and reads `lootFilterVisible` and
`visible` on up to 2048 ground items):

- `loot-hidden-count` pass. At the owner's strict filter, after about 60 s of
  killing at `dropmult item 10`:
  `lootcensus: ground=291 hidden=281 invisible=281 coins=22`. An earlier read at
  the same filter, after the x1 gold kills, gave `ground=68 hidden=68
  invisible=68`.
- `loot-showall-control` fail, partly. The game has no "Show all loot" key (the
  owner, during the session), so turning the loot filter off was the control:
  `ground=291 hidden=0 invisible=95`. `hidden` fell to 0 with `ground` unchanged,
  so the instrument is not blind. `invisible` stayed at 95 where the check
  expected 0: `visible` also follows something besides the filter, which fits
  the static reading that Alarm 9 consults the screen, but which cause held
  those 95 was not established.
- `loot-frame` not observed. The filter-off window read `avg 7.08 ms, max 40.7
  ms`, clean. The strict window read `avg 12.99 ms, max 6535.4 ms`, but it
  included #77's pickup stall, so no isolated strict-filter frame time exists
  and no cost of hidden items was measured. The difference is not evidence of a
  cost, and not evidence of none.

**No mod.** #95 part 2 (dropping or removing filtered items) is out of this
batch. A part 2 would first need an isolated frame-time window at a strict
filter, with nothing else in it.

## Live procedures

Each procedure's step-by-step lives in the workorder that ran it
(`forgepact-dev2-bug-batch`); this is the summary. Every session runs on slot 14
("Sorak"), backs the saves up before the launch and restores them after, and
must pass `dll-hash`, `marker` (the boot line `==== BloodPact plugin loaded ====
v2.0.0`) and `control` before anything else it records counts. Person actions
come first, in one hand-back, so the owner can leave for the back-end steps.

**Live 1, the measurement round** (research DLL, 2026-09-27). Its checks, in
order, all recorded above: `dll-hash`, `marker`, `control` (pass);
`relic-global-slots`, `relic-instance-var`, `relic-scan-count` (#93, pass);
`manaorb-rule-entry` (fail, absent), `manaorb-chosenone-flag`,
`manaorb-timer-chosen` (pass), `manaorb-draw-chosen` (fail, none),
`manaorb-timer-plain`, `manaorb-draw-plain` (not observed) (#83);
`gold-baseline` (fail on the max frame), `gold-args`, `gold-x100-nesting`,
`gold-x100-instances`, `gold-x100-frame` (pass) (#77); `loot-hidden-count`
(pass), `loot-showall-control` (fail on `invisible`), `loot-frame` (not
observed) (#95). Research checks record findings; only the first three had to
pass.

**Live 2, the fix gate** (player DLL `c36004c6...8e6b`, which also carries the
quest-collector change but does not test it; ran 2026-09-27, results in
[Live 2 results](#live-2-results-2026-09-27)). Checks: `dll-hash`, `marker`,
`control`; `on-craftmats-press` (one Ol to Old press moves from the stash and
logs one `craftmats: moved` line); `on-gold-amount` (three gold drops at x100,
exactly one `dropmult gold: x100 applied to the coin's amount` line, no freeze);
`on-relic-scan` (`scan found 3 maxed relics (ids 109,124,135)`);
`on-manaorb-countdown` (an arc on slot 0,3 while the orb lives, none after);
`no-new-refusal` (none of the five new `craftmats:` refusal lines in the
session's log). All are acceptance checks.

**Live procedure 3, the owner's own test of the quest-collector change** (first
filed as the #94 test; the final player DLL, all five fixes). The owner drives the character to a spot with eight or more quest
items on screen and one with two or three, and judges the pet; the operator
reads `petquest 0`'s counter lines before and after each. Checks:
`on-petquest-many` and `on-petquest-few` (the owner's verdict, with `collected=`
rising), and `petquest-counters` (a record of which counter rose besides
`collected`, never a gate). **It was not run**: the batch ships in 2.0.1
without it, so no session confirms the quest-collector change, and the notes
say so ("Not yet confirmed in a live game.").

## Live 2 results (2026-09-27)

**Measured**, one session: player DLL SHA-256 `c36004c6...8e6b` (boot line
`==== BloodPact plugin loaded ==== v2.0.0`), slot 14 ("Sorak"). All eight
checks passed: `dll-hash`, `marker`, `control`, and the five below.

- **#93, `on-relic-scan`: pass.** The relic filter logged `relicfilter: scan
  found 3 maxed relics (ids 109,124,135)` when it armed and again on a formal
  re-run: the equipped slots are read in the player build.
- **#83, `on-manaorb-countdown`: pass.** With Chosen One, the countdown arc
  showed on the Mana Orb slot (0,3) right after the cast and was gone once the
  orb cleared. Mana Orb without Chosen One was not checked.
- **#77, `on-gold-amount`: pass.** At x100 the hook logged one `dropmult gold`
  line reading `first coin 44 -> 4400`. The HUD's gold rose from 188948 to
  189019 at x1 (+71) and on to 201889 at x100 (+12870, two stacks the screen
  showed as 5720 and 7150). The owner saw no freeze at the kill or at the
  pickup. This is the first time argument 4 of `DropGold` has a gold figure
  the game showed beside it; monster gold only, one session.
- **#80, `on-craftmats-press` and `no-new-refusal`: pass.** The re-press
  logged `craftmats: moved 1 class=15 ...` and the owner crafted one Old; none
  of the five new `craftmats:` refusal lines appeared anywhere in the session.

Two findings, neither a defect of the batch:

- **The mods were not armed after `hs_launch`.** The session's first craft
  press ran with no ForgePact mod armed, so it tested nothing; the operator
  armed the mods by command and the re-press is the one that counts. Why the
  mods did not arm on their own is not established.
- **The operator's bag count was wrong.** A `menulayout` read taken as the
  bag's Ol count said 19 while the bag held none: the cells it counted share
  the recipe's item class with other items, and whether the grid read was the
  bag was not proven. The count came from the instrument, not the bag.

Live procedure 3 was not run, so the quest-collector change is confirmed by no
session. The 2.0.1 player DLL, built later from the tree merged with
ForgePact's `main`, is a different binary, and no session has run it.
