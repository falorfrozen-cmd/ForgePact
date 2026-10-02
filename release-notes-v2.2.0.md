# ForgePact 2.2.0

Release date: 2026-10-09

**Headhunter** and **Tyrant's Crown** now drop the way the game's own Angelic
items do: from the game's Angelic roll, and only while that item's switch is
on. The **Angelic / Unholy Drops** slider no longer drops them. With both
switches off, the default, neither one ever drops, even if you have forged it.

## Changed

- **Headhunter and Tyrant's Crown drop only from the game's own Angelic roll.**
  - Before, they were two more items in the **Angelic / Unholy Drops** slider's
    pool: ForgePact's own die dropped them whatever their switches said, and
    the game's own Angelic roll never did.
  - Now the game decides. When a Blood Pact or dungeon "Angelic item drop
    chance" effect is active and the game's Angelic roll hits, the game drops
    its own Angelic or Unholy item as usual, and ForgePact rolls the signature
    items' share of that hit: the share of one item in the pool, about 1 hit in
    48 with one switch on and 2 in 49 with both. On a success, the item lands
    beside the game's own item, where the monster died. With both switches on,
    each success is one or the other at even odds.
  - This was verified in a live game session, using a test build that made the
    game's Angelic roll hit far more often than it normally does: with a switch
    on, its item dropped beside the game's own Angelic items; with both off,
    neither ever did.
  - Only while that item's switch (**Mods → Items → Headhunter** or
    **Tyrant's Crown**) is on. Forging the item in the Custom Forge still turns
    its mechanic on, as in earlier versions, but not this drop: a forged
    Headhunter or Tyrant's Crown with its switch off never drops from the
    game's Angelic roll.
  - Both switches are off by default, so a default install drops neither item,
    forged or not, and the game's roll is left as it is.
  - ForgePact adds no chance of its own for these two and does not change the
    game's Angelic chance.
- **The Angelic / Unholy Drops slider's pool is the game's real Angelic and
  Unholy uniques again**, with no signature items in it.
- `sigdrop crown|belt|off|status` is still a test command that makes every kill
  drop the named item. `sigdrop status` now also counts the game's Angelic
  rolls and hits, and the signature items they dropped.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. The signature drop change is in
the plugin. **Launch Modded Game** brings the plugin up to date for you. If you
start the game from Steam instead, or ForgePact's warning asks for it, press
**Install Mod Plugin** once after updating.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
