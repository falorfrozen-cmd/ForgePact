# ForgePact 1.4.6

Gems of Incarnation drop Mythic, with the mods you pick, and every mod on them
shows its best roll.

## New

- **Mythic Gems of Incarnation.** Most Gems of Incarnation used to drop with a
  single weak mod. The game itself makes 1 in 50 of them Mythic, with 4 or 5
  mods. With this switch on, every Gem of Incarnation that drops is Mythic. The
  game still rolls the gem, and ForgePact hands it a seed the game has already
  rolled Mythic for the same kind of drop, so what drops is an ordinary Mythic
  gem. Gems you already own keep their mods. The first time the switch is on,
  the game rolls sample gems quietly in the background, under a minute at the
  main menu. Until that is done, a gem that drops keeps the game's own roll.
  Mods tab → **Mythic Gems of Incarnation**, off by default.
- **Gem mod filter.** Under the two switches, **Filter...** lists every mod a
  Gem of Incarnation can roll - 36, in six groups (Attack, Skills, Elemental
  skills, Defense, Life & mana, Loot). Tick the ones you want and save: every
  Mythic gem that drops then carries as many of them as the game's own Mythic
  rolls allow. Tick Increased Attack Speed and Increased Magic Find, and the
  gems have both. It starts with every mod ticked, which is no filter.
- **Max-roll Gems of Incarnation.** Every mod on every Gem of Incarnation, new or
  old, shows the highest value its best tier can roll: Increased Attack Speed
  reads 12, where a lower tier could have stopped at 8. A skill grant keeps its
  skill. Nothing is written to your save: turn the switch off, and a gem shows
  its own rolls again the next time the game loads it. Mods tab →
  **Max-roll Gems of Incarnation**, off by default.

## Changed

- **The panel's address is now http://127.0.0.1:8780** (it was 8766). 8766 is
  one of the ten ports the Item Editor keeps for itself. With the editor open,
  the panel already ended up on 8780, and the Toolkit Hub, checking 8766 for
  ForgePact, reached the editor instead. The panel now starts on 8780 whether or
  not the editor is open, and the hub checks it there. If 8780 is taken, it
  tries 8801, 8899, 9133 and 9777, as before. The panel still opens in its own
  window; this only matters if you open it in a browser.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This change is in the plugin,
so pressing **Install Mod Plugin** matters - updating only the panel leaves the
old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
