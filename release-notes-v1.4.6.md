# ForgePact 1.4.6

Two fixes: entering the Underground Garden no longer freezes the game, and the
`raredrop angelic` command works again when a drop multiplier is set.

## Fixed

- **Entering the Underground Garden no longer freezes the game.** With the mod
  plugin installed, going from Misty Swamp into the Underground Garden froze
  the game on "Generating Zone..." for half a minute or more, even with every
  mod switched off. Building that dungeon makes the game report a harmless
  warning thousands of times, and the hidden log window the mod framework
  opens made the game write out every one of them before it could go on.
  ForgePact now closes that hidden window instead of only hiding it, so the
  warnings cost nothing, as in the unmodded game: the Underground Garden loads
  in a second or two. Players never saw that window, so nothing else changes.
- **`raredrop angelic` works again when a drop multiplier is set.** The
  command opens the game's own Angelic item roll. Once any drop multiplier
  above x1 was active (the panel sets those when the game starts), it answered
  "call site not found - game build changed" and changed nothing, because it
  looked for the roll in ForgePact's own drop code instead of the game's. It
  now finds it however the drop multipliers are set.

## How to update

Download and extract the complete release, then reopen ForgePact. Your existing
settings are retained. Source users can run `Prepare-Plugin.bat` if plugin files
are missing before using **Install Mod Plugin**. This change is in the plugin,
so pressing **Install Mod Plugin** matters - updating only the panel leaves the
old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
