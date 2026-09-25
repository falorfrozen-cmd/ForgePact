# Gems of Incarnation: research

A player on the toolkit's Discord (2026-09-25): most Gems of Incarnation roll one
weak mod ("+7 life", "2% death blow chance"), the loot filter cannot hide them,
and "you pick them all up". The wish: gems with more than 1-2 mods, every mod at
its maximum. This document is what the mod (`IncarnationGemsMod.hpp`, the
`gemmythic` and `gemmaxroll` switches and the `gemfilter` mod filter) is built
on.

Everything below is either **measured** (observed on the running game) or a
**static reading** (read from the compiled game locally and written here in our
own words; no game code is quoted, AGENTS.md § Legal). Build:
`pe-6aaa6779-0cad4fc8` (2026-09-16). Ghidra project: `proj_sep17b` on
`HS_Sep17.exe`, the running build.

## What a Gem of Incarnation is

- Item key `socketable_gem_of_incarnation`: item type 15 (Socketable), base 136,
  `c` 0, `j` 0. It goes only into the Incarnation tree's node sockets: a node
  holds an item fingerprint (`nodeItemFingerprint`), the tree collects them
  (`incarnationSocketItemArray`) and `IncarnationStatGetter` adds their stats.
  **Static reading.**
- A save keeps its seed `a`, `b` 136, `c`, `j` and the flags `n`, `o`, `w`.
  **Measured** (the owner's 21 gems).
- Its affixes sit in stat slots `"10"`-`"14"` as `[stat, min, max, tier]`, the
  rolled value under the stat id. **Measured.**

## How the game rolls one - measured on 14,521 gems

The Item Editor's Item Truth evaluation had the running game build gems from
definitions, through its own save loader (`InitItemFromJson`) and
`CreateItemNew`, at the main menu (2026-09-25).

- **Positive control:** the owner's 21 gems, rebuilt under new time stamps, came
  out identical to what the game had recorded for them - stats and names. The
  roll depends on the definition alone.
- **5,000 random seeds, no `n`:**

  | Rarity (info 27) | Share | Mods |
  |---|---|---|
  | 2 Superior | 91.1% | 1-3 (mostly 1-2) |
  | 3 Rare | 7.0% | 3-4 |
  | 5 Mythic | 2.0% | 4-5 |

  Mods: 1 = 66.8%, 2 = 24.0%, 3 = 7.2%, 4 = 1.9%, 5 = 0.1%. Level requirement
  (info 1) 52/57/62/67 for 1/2/3/4+ mods; tier letter (info 32) always 4 (S).
- **The affix pool:** 37 stats. Each has three tiers (2, 3, 4) with one range per
  tier: across all 14,521 gems and every `n`, no (stat, tier) pair showed a
  second range. Tier 4 is the best range (for example stat 68, Increased Attack
  Speed: tier 2 = 2-8, tier 3 = 2-10, tier 4 = 3-12). Stat 462 is the skill a
  skill grant gives (2-433, an identifier), 463 its levels (1-1), 201 All Skills
  (1-1).
- **Rolls are uniform** within the range (mean 0.5). Every affix at its top: 14%
  of 1-mod gems, 1.3% of 2-mod gems, 0.3% of 3-mod gems. A seed search for
  "every mod at max" is therefore impractical; the mod sets the values instead.
- **`n` moves rarity, not tiers** (the same 2,000 seeds; the tier-4 share stays
  35-37%):

  | `n` | Superior | Rare | Mythic | 3+ mods |
  |---|---|---|---|---|
  | none, 0, 1 | 91-92% | 6-7% | 2.0% | 8-9% |
  | 2 | 88.5% | 8.6% | 3.0% | 11.9% |
  | 3 | 80.0% | 13.7% | 6.3% | 21.3% |
  | 4 | 68.8% | 19.9% | 11.3% | 32.0% |
  | 6 | 50.7% | 29.3% | 20.0% | 50.6% |
  | 5, 8 | same as none | | | |

  `n` is a table index (5 and 8 fall back to the baseline), not a scale, so the
  mod never invents one. Real gear carries 0-4, mostly 3 and 4. No seed rolled
  fewer mods at `n` 4 than at `n` 2.

## The affix pool

Every mod the game rolled on the 14,521 gems, as the panel's filter lists it:
the tooltip's wording, a category, the best tier's range, and how many of the
1,536 Mythic seeds in the live tables (`n` 3, 4 and none, 512 each) carry it.
**Measured.**

| Stat | Mod | Category | Best tier (4) | On Mythic gems |
|---|---|---|---|---|
| 28 | `#% Enhanced Damage` | Attack | 12-35 | 17.3% |
| 68 | `#% Increased Attack Speed` | Attack | 3-12 | 28.6% |
| 74 | `+# to Attack Rating` | Attack | 15-50 | 17.0% |
| 75 | `#% Increased Attack Rating` | Attack | 8-25 | 16.7% |
| 95 | `#% Chance for a Deadly Blow` | Attack | 3-10 | 16.1% |
| 128 | `+# to Physical Damage` | Attack | 2-6 | 2.4% |
| 448 | `+# to Minimum Weapon Damage` | Attack | 6-12 | 16.7% |
| 450 | `+# to Maximum Weapon Damage` | Attack | 6-12 | 25.5% |
| 101 | `#% Magic Skill Damage increased by` | Skills | 5-20 | 19.5% |
| 196 | `#% Faster Cast Rate` | Skills | 2-5 | 24.8% |
| 201 | `+# to All Skills` | Skills | 1-1 | 0.7% |
| 462 | `+# to a single skill` | Skills | 2-433 | 2.9% |
| 133 | `+# to Fire Skill Damage` | Elemental skills | 18-24 | 3.4% |
| 134 | `#% Fire Skill Damage increased by` | Elemental skills | 5-20 | 5.3% |
| 137 | `+# to Cold Skill Damage` | Elemental skills | 18-24 | 2.9% |
| 138 | `#% Cold Skill Damage increased by` | Elemental skills | 5-20 | 5.0% |
| 141 | `+# to Arcane Skill Damage` | Elemental skills | 18-24 | 2.9% |
| 142 | `#% Arcane Skill Damage increased by` | Elemental skills | 5-20 | 5.3% |
| 145 | `+# to Lightning Skill Damage` | Elemental skills | 18-24 | 3.0% |
| 146 | `#% Lightning Skill Damage increased by` | Elemental skills | 5-20 | 5.9% |
| 149 | `+# to Poison Skill Damage` | Elemental skills | 18-24 | 2.7% |
| 150 | `#% Poison Skill Damage increased by` | Elemental skills | 5-20 | 5.9% |
| 29 | `#% Enhanced Defense` | Defense | 25-75 | 16.6% |
| 173 | `#% to All Resistances` | Defense | 2-5 | 11.7% |
| 175 | `#% to Fire Resistance` | Defense | 6-15 | 3.6% |
| 177 | `#% to Cold Resistance` | Defense | 6-15 | 3.1% |
| 179 | `#% to Lightning Resistance` | Defense | 6-15 | 3.5% |
| 181 | `#% to Arcane Resistance` | Defense | 6-15 | 3.6% |
| 183 | `#% to Poison Resistance` | Defense | 6-15 | 3.6% |
| 52 | `+# to Life` | Life & mana | 10-50 | 15.8% |
| 53 | `#% Life Increased by` | Life & mana | 3-10 | 12.4% |
| 57 | `#% Life stolen per Hit` | Life & mana | 2-6 | 28.1% |
| 60 | `+# to Mana` | Life & mana | 10-50 | 16.5% |
| 61 | `#% Mana Increased by` | Life & mana | 3-10 | 10.9% |
| 64 | `#% Mana stolen per Hit` | Life & mana | 2-6 | 27.0% |
| 284 | `#% Increased Magic Find` | Loot | 3-10 | 25.7% |

- A skill grant is two slots: 462 names the skill (its "range" 2-433 is a range
  of skill ids, never maxed) and 463 its levels (1-1). It is one row here.
- The wording follows the game's stat names (the Item Editor's game-verified
  stat table, `hs_stat_semantics_s10.json`).
- There is no movement or other mobility stat in the pool.
- +All Skills is by far the rarest mod: 0.7% of Mythic gems (0.17% in the
  smaller 589-gem sample above), yet 3-4 of the 512 seeds at every `n` built.

## How it drops - static reading

- `DropGems` (drop type 6, "gem", RUNTIME_DATA_MODELS §13.1) picks a socketable
  from repository category 15 by drop rate and hands it to `LootGroundCreate`.
- `LootGroundCreate(x, y, type, params, ...)` writes a fresh random seed into the
  params' `a`, makes the item instance (type, time stamp) and calls
  `CreateItemNew(instance, undefined)`. So the seed is fixed between that write
  and `CreateItemNew`'s first line; that is where the mod swaps it.
- Loading an item goes through `InitItemFromJson`, not `DropGems`: a gem the
  player owns never takes a new seed.
- `LootGroundCreate` is already detoured natively by the Mining Ore mod; a second
  detour of it would fall back to table-only, which compiled GML's direct calls
  bypass. The mod hooks `DropGems` instead, which nothing else hooks.

## Why the loot filter never hides them - static reading

The ground item's loot-filter closure (a `Loot_Ground_obj` Create closure) runs
its checks for equipment (types 0-8), charms (10), consumables (11), potions (18)
and **socketables with base 97-111 only** - the Uncut Jewels, the only random-mod
socketables before Season 10. Every other socketable, base 136 included, skips
the checks and stays visible. The Season 10 gem was never added to that range.
Not measured live.

## "Auto loot" - static reading

No automatic pickup was found. `Loot_Manager_obj`'s Step picks up only the
targeted item (`playerLootTarget`) after an input (key, click, gamepad);
`Loot_Ground_obj` has no Step; `Alarm 9` only sets visibility from the filter and
the screen. The translation `auto_pickup` ("Auto Pickup") is referenced by no
code in the exe or `data.win`. The Steam community and the Season 10 patch notes
say gems and runes are picked up by hand. The player's "auto loot" reads as
"cannot hide them".

## The mod

- **`gemmythic`**: while `DropGems` runs, a `CreateItemNew` whose instance is a
  Gem of Incarnation takes a seed from a table of seeds the game itself rolled
  Mythic at the same `n` (4-5 mods). No seeds yet for that `n`: the drop keeps
  the game's roll, and that `n` is queued.
- **`gemfilter all|<stat,...>`** (the panel's **Filter...** list): with a
  filter, a drop takes one of the table's seeds carrying the most ticked mods,
  so each Mythic gem has as many of them as any seed at that `n` has; ties are
  picked at random. Nothing ticked matches any seed (a stat outside the pool):
  the drop takes any Mythic seed and the miss is logged once. `all` (the
  default) is no filter. A skill grant is ticked by its skill slot, 462.
- **`gemmaxroll`**: every finished Gem of Incarnation - dropped, loaded, checked
  by the Item Editor - has each affix set to its best tier's range and top value;
  identifier stats (462, 21) are left alone; an affix whose best range is not
  known yet takes its own range's top. The item's hash is refreshed. Off, the
  next build shows the gem's own rolls: nothing is saved.
- **The tables**: learned once per game build by having the game build candidate
  gems through its save loader, 4 ms a frame at the menu and 1 ms in play, and
  kept in `%LOCALAPPDATA%\Hero_Siege\forgepact_gem_tables.json` with each
  seed's mods (schema 2). The candidates skip Custom Forge, the dress and Item
  Truth. 512 Mythic seeds per `n`; the `n` 4, 3 and none rows are built first,
  others when a drop needs them.
- **Measured cost** (research build, 2026-09-25): a candidate costs 0.3 ms
  (47,147 built in 14.4 s of build time). The three first rows took 4,471
  (`n` 4), 8,807 (`n` 3) and 33,869 (none) candidates, about 45 s at the main
  menu, once per game build.
- Tests: `tests/incarnation_gems_harness.cpp` (the core, baseline and target),
  `tests/test_incarnation_gems_contract.py` (the wiring and the panel).

## Live checks

Research build, main menu, simulated drops (`gems drop <n>`: a gem built
through the game's loader and `CreateItemNew` inside the drop scope):

- Tables: 512 Mythic seeds for `n` 3, 4 and none, 37 best ranges, no conflicts.
- `gemfilter 201`: 4 of 4 drops (`n` 4, 4, 3, none) carried +All Skills.
- `gemfilter 68,284`: 3 of 3 drops (`n` 4, 3, none) carried both.
- `gemfilter 133,134,137,138`: the drops carried two of the four, the most any
  `n` 4 seed has.
- `gemfilter 21` (not in the pool): the miss was logged once and the drops were
  Mythic with any mods.
- `gemfilter all`: Mythic drops with any mods. Malformed filters (`0,x`, `463`)
  were refused and left the filter as it was.
- Every drop: rarity 5, 4-5 affixes, each at its best tier's top.

## Open

- The seed swap on a real drop, and the loot-filter reading, are not yet
  observed live.
- Hiding weak gems (the filter gap) is not built: with both switches on, there
  are no weak gems.
