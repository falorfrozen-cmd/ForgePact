# Prime Evil parts: boss drops and the slider (2026-09-26)

A player asked for a Prime Evil parts slider for crafting Keys of Terror. The
Blood Pact screen has a "Prime Evil part drop rate" row for the same thing.

## The parts

- **Twelve category-13 items.**
  - 13:2-7 are Gurag's Soul, Death's Sigil, Damien's Eye, Anubis' Ankh, Karp
    King's Bellybutton and Satan's Horn.
  - 13:43-48 are their infernal versions.
  - Vanilla `droprate.base` is 27 for the normal parts and 35 for the infernal
    ones ([blood pact §3](blood-pact-values-research.md)).
- **The keys:** Key of Terror is 12:5 and Key of Infernal Terror is 12:34 (the
  game's item catalog).
- **Drop type:** the parts drop through LoadDrops type 41, which they share with
  Relics. That is why the panel did not offer them until now: opening type 41
  drops Relics as well, and the plugin skips the part scripts while the Relic
  gate rolls.

## How they drop

Static reading, Sep-17 build `pe-6aaa6779`, in our own words:

- **Callers.** `DropBossParts`, `DropBossPartsNext` and `DropUberParts` each have
  exactly one direct call site in the executable, all inside `LoadDrops`.
  - `LoadDrops` in turn is called only from `DropItem`.
  - Monsters reach `DropItem` from `Enemy_Parent_obj`'s Destroy event. Chests,
    goblins and destructibles reach it their own way.
- **`DropBossParts`:**
  - it looks up a category-13 part's repository entry;
  - it reads that entry's drop rate (`GetDropRate`);
  - it reads player stat 736, probably the part-drop bonus (not measured);
  - it places the part with `LootGroundCreate`.

  So the part's own `droprate.base`, which `droprate group primeevil` divides,
  takes part in the roll.
- **`DropUberParts`** reads no drop rate, and it creates no Prime Evil part.
  - It creates one of category 13's items 14-18: Soul of Anguish, Soul of Despair,
    Soul of Corruption, Scroll of Ra or Colosseum Fragment. It can also create
    their infernal versions, 49-53.
  - It places the item with `LootGroundCreate`.
  - The `primeevil` group covers none of these ten items. `DropUberParts` calls
    no `GetDropRate`, so no `droprate group` can scale them.
- **Which drop type runs which script** (`LoadDrops`' switch table):
  - Type 41 (Relics and the parts) first checks `LoadDrops`' fourth argument. It
    does nothing when that argument is false.
  - Type 43 goes to `DropUberParts`, and type 26 to `DropDimensionalShard`.
    The 2026-08-27 type map ([blood pact §6](blood-pact-values-research.md))
    recorded a Dimensional Shard for type 43. The two disagree; not resolved.
- **`Enemy_Parent_obj`'s Destroy** calls `DropItem` only when the monster's
  protected HP is 0 or less.

## Measured

Setup: 2026-09-26, research build, hero Suh (slot 2, softcore), Act_01_01.

For each kill:
1. Spawn Karp King 1200 px from the hero.
2. Wait about 2 s, until its `dropTable` exists.
3. Set its protected HP to 0: `PC_SetVariableGMLWrapper(key, 0)`, where `key` is the boss's `enemy_hp`.
4. The boss dies through its own death path.
5. Read the drops from `bp_ipc\itemdrops.jsonl`.

| `droprate group primeevil` | kills | Karp King's Bellybutton | per kill |
| --- | --- | --- | --- |
| x1 (vanilla) | 15 | 11 | about 0.7 |
| x5 | 12 | 36 | 3.0 |
| x35 | 15 | 138 | about 9.2 |

Notes on these numbers:
- **Several rolls per kill.** Each kill rolls the part more than once: at x35,
  where the part's base is 1, one kill dropped up to 14. At x1 some kills dropped
  none.
- **Totals, not single kills.** A boss's death animation can outlast the 6 s
  window, so a kill's drops sometimes land in the next window. The totals are
  what count.
- **The slider changes nothing above x35.** It divides the base and never goes
  below 1: normal parts reach 1 at x27, infernal ones at x35.

What else was seen:
- **In town,** a boss removes itself within seconds and drops nothing (Karp
  King in Town_02_rm).
- **`instance_destroy`** on a live boss runs its Destroy event, but nothing
  drops. The drop path needs a real death, HP to 0.
- **Next to the hero,** Karp King killed the level-100 hero in about 15 s (once;
  softcore, no loss). Spawning it 1200 px away avoided that.
- **Uber Anubis** did not die from HP 0. All 10 test instances were removed with
  `instance_destroy`. "Uber bosses" below has the way that kills it.

## Uber bosses (measured 2026-09-26, afternoon)

This test checked whether the slider does anything for uber bosses. Same research
build, same hero, Act_01_01. The slider was at x35, where Karp King drops about
10 parts per kill.

| Boss | how it died | kills | Prime Evil parts | uber items (13:14-18, 49-53) |
| --- | --- | --- | --- | --- |
| Karp King (control, before) | HP 0 | 1 | 12 | 0 |
| Uber Damien | HP 0 | 3 | 0 | 0 |
| Reaper (`Reaper_Uber_obj`) | HP 0 | 3 | 0 | 0 |
| Uber Endrixia | HP 0, then `instance_destroy` | 2 | 0 | 0 |
| Uber Anubis | HP 0, then `instance_destroy` | 2 | 0 | 0 |
| Karp King (control, after) | HP 0 | 1 | 8 | 0 |

- **No uber boss dropped a Prime Evil part at x35.** The slider does nothing for
  them, at least outside their own realm. Two of the ten uber kills dropped
  ordinary gear.
- **The same at x1,** earlier that afternoon: three Uber Damien kills, one Reaper,
  one Uber Endrixia and one Uber Anubis. They dropped no part and no uber item,
  each at a spot where a Karp King kill did drop loot.
- **How they die.**
  - Uber Damien and Reaper die from HP 0.
  - Uber Endrixia, Uber Anubis and Uber Luna do not.
  - For Endrixia and Anubis, HP 0 followed by `instance_destroy` runs the Destroy
    event with HP at 0, and the drop path follows.
- **Not measured:**
  - **Uber Luna.** `instance_destroy` after HP 0 closed the game, with no crash
    dump. Its Destroy event decrypts an item from an API string.
  - **Uber bosses inside their own realm.** `room_goto` to `Uber_Inoya_rm` also
    closed the game. It is open whether they drop the uber items there, and
    whether any Prime Evil part comes with them.
- **A spawned boss must die where loot can land.**
  - A Karp King spawned past the room's right edge dropped nothing, and no
    `Loot_Ground_obj` appeared. The same boss 1200 px to the hero's left dropped
    normally.
  - In one more entry, with the hero at (7880, 10336), even Karp King dropped
    nothing. The hero was found dead afterwards. That entry's results were
    discarded.
  - So every result above comes from a spot where a Karp King control dropped.

## Not verified

- **Where the infernal parts drop.** No test dropped one. Every Karp King part
  was the normal bellybutton, and the uber bosses dropped no parts at all.
- **Uber bosses in their own realm** (see above).
- **Stat 736's meaning.**
- **Which bosses, in which zones, roll type 41 natively.** Only Karp King was
  measured dropping parts. Four uber bosses dropped none in Act_01_01.

## What shipped

- **Panel:** a `KEYS` entry `("primeevil", "Prime Evil Parts (Key of Terror)",
  None)`. With no drop type, the slider sends only `droprate group primeevil
  <m>`, never `dungeonkey add 41`.
- **Plugin:** the `primeevil` group's name fragments were "satans_horn", which
  missed `collectible_satans_infernal_horn`. They now use "satans_". In category
  13 that matches only the two horns: 12 of 12 parts, where it used to be 11.
- **Tests:** `tests/test_prime_evil_parts_contract.py`.
- **Tool:** `tools/boss_drop_trial.py` repeats the kill loop above. `--dx` moves
  the spawn, `--destroy` adds the HP 0 + `instance_destroy` step for bosses that
  outlive HP 0, and each kill also counts uber items. It needs the
  research build, a hero in a zone, and the hub's `hs-game-sdk` next to this
  checkout. The hero can be entered without input with HS-AFK-Expedition's
  `tools/game_session.py prepare`, then `travel --room Act_01_01`.
