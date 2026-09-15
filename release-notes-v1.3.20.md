# ForgePact 1.3.20

Everything that got slower the longer you played.

## Fixed

- **Map Reveal showed you the map but never filled it with monsters.** Turning on
  "Reveal full map" cleared the fog exactly as it should, and then the rest of the
  zone stayed empty — the monster half had not been working at all, in any zone, and
  it failed silently, so there was nothing to see but an empty map. It works now:
  in the zone this was confirmed in, revealing went from **42 monsters to 547**.
  If you turned this on and concluded the monster option did nothing, you were
  right, and it was not your settings.

- **The panel got heavier the longer a session ran.** Every five seconds it read
  the plugin's entire log file from the start to count how many times the game had
  launched — and that log only ever grows, so by the end of a long evening each of
  those checks was rereading several megabytes. It now reads only what was added
  since the last check. Measured on a 64 MB log: **35.2 ms per check before,
  3.5 ms after** — roughly 10x faster, and between 8x and 13x across nine runs —
  and unlike before it no longer gets worse as the log grows.

- **Starting the game was slower than it needed to be.** While waiting for the mod
  to load, the panel asked Windows for the list of running programs by launching a
  separate console tool — four times a second, for up to a minute, which is up to
  240 short-lived programs started during the exact moment you are waiting to play.
  It now asks Windows directly, in the panel itself: **357 ms per check before,
  21 ms after** — between 12x and 18x faster across nine runs — with nothing
  spawned.

- **Three mods did more work per frame than they needed to.** Special Content,
  Orb Pickup and Map Reveal's monster pass each repeated work every single frame
  that only needed doing occasionally, or did the same check twice for the same
  monster spawner. Special Content still opens its gate at the same moment it always
  did, globes still glide in at the same steady speed, and a revealed zone fills
  with packs — they simply stop paying for it sixty times a second.
  One thing did change, and it is the price of the rest: a globe that comes into
  range can take up to a quarter-second longer to start gliding toward you. It is
  never missed, and once it starts moving the glide is exactly what it was.

- **Both windows now poll only when something is happening.** The ForgePact panel
  and the HS Offline Launcher used to refresh on a fixed timer forever, whether or
  not anything had changed and whether or not you were even looking at the window.
  They now check quickly for fifteen seconds after anything changes — you move a
  slider, the game starts or stops — then settle down to once every thirty seconds,
  and stop entirely while the window is hidden behind something else. Switching back
  to the window refreshes it immediately.

## Changed

- **The plugin now records its version in its log**, next to the line that says it
  loaded, and the panel shows its own version in the header. The panel and the mod
  plugin are installed separately and can end up being different builds, so if you
  ever report a problem, those two numbers say exactly what you were running.

## How to update

Download the zip, unzip it, run ForgePact and press **Install** once. Start the game after
that; a game that was already open keeps the old plugin until it is restarted.

Some of this is in the plugin, so pressing **Install** matters — updating only the panel
leaves the old plugin in place.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
