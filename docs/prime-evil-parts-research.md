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
- **`DropUberParts`** reads no drop rate: it creates the part directly.

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
- **Uber Anubis** did not die from HP 0 (it has phases and dummy objects). All 10
  test instances were removed with `instance_destroy`.

## Not verified

- **Infernal parts from uber bosses.** `DropUberParts` reads no drop rate, so the
  slider probably does not change them.
- **Stat 736's meaning.**
- **Which bosses, in which zones, roll type 41 natively.** Only Karp King was
  measured, spawned by the research command in Act_01_01.

## What shipped

- **Panel:** a `KEYS` entry `("primeevil", "Prime Evil Parts (Key of Terror)",
  None)`. With no drop type, the slider sends only `droprate group primeevil
  <m>`, never `dungeonkey add 41`.
- **Plugin:** the `primeevil` group's name fragments were "satans_horn", which
  missed `collectible_satans_infernal_horn`. They now use "satans_". In category
  13 that matches only the two horns: 12 of 12 parts, where it used to be 11.
- **Tests:** `tests/test_prime_evil_parts_contract.py`.
- **Tool:** `tools/boss_drop_trial.py` repeats the kill loop above. It needs the
  research build, a hero in a zone, and the hub's `hs-game-sdk` next to this
  checkout. The hero can be entered without input with HS-AFK-Expedition's
  `tools/game_session.py prepare`, then `travel --room Act_01_01`.
