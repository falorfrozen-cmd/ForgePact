# ForgePact 1.3.19

Two things that looked like they worked, and did not.

## Fixed

- **Remove owned relics from drop pool did nothing at all, in every version since
  1.3.17 announced it.** The switch stayed on, the plugin reported the mod as
  active, and maxed relics kept dropping exactly as often as before. The mod
  checks which relics you already own at 10/10 before each relic roll, and that
  check was reading the wrong kind of value for "your character" — so it came
  back with an empty list every time, for everyone, which the rest of the mod
  could not tell apart from "this character has no maxed relics yet". Nothing
  was ever held back. It now reads your equipped slots, backpack and inventory
  as intended.

  The plugin also says so in its log (`<game>\bin\bp_ipc\out.txt`) now:
  `relicfilter: holding back 3 of 5 maxed relic(s) on this roll`, counting what it
  actually withheld. Previously the only sign of life was "hook installed", which
  it printed just as cheerfully while filtering nothing — if you turned this mod on
  before and could not tell whether it was doing anything, that is why. The log also
  names the cases where it deliberately or unavoidably does nothing, rather than
  staying silent about them; the README lists what each line means.

- **The Satanic Zone mod lists were missing from the World tab.** The section
  header and its explanation were there, with nothing underneath — no positive
  mods, no negative mods, nothing to tick. The 25 positive and 26 negative mods
  are supplied by a shared toolkit component that was not being packed into
  `ForgePact.exe`, so the packaged panel had an empty list to draw. All 51 are
  back, with their names and descriptions.

  **Your saved selection was never lost.** If you had deselected mods in an
  earlier version, `forgepact.json` kept them the whole time and they are exactly
  as you left them now that the lists render again.

## How to update

Download the zip, unzip it, run ForgePact and press **Install** once. Start the game after
that; a game that was already open keeps the old plugin until it is restarted.

The relic fix is in the plugin, so pressing **Install** matters for this one — updating
only the panel leaves the old plugin in place and the mod keeps doing nothing.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
