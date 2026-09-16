# ForgePact 1.3.21

Less running in the background while you play.

## Changed

- **The plugin no longer runs a freeze detector during your game.** Earlier
  versions started a background check that woke up twice a second, every second
  you played, to notice if the game stopped drawing frames — and, if it had,
  briefly paused the game's own thread to see where it was stuck. It was built to
  track down a freeze on the character screen, and that freeze turned out not to
  be caused by ForgePact at all. It is now only in the developer build, so the
  plugin you install does less work and no longer writes `STALL` lines to its log.
  Nothing you can turn on or off has changed.

## How to update

Download the zip, unzip it, run ForgePact and press **Install** once. Start the game after
that; a game that was already open keeps the old plugin until it is restarted.

This change is in the plugin, so pressing **Install** matters — updating only the panel
leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
