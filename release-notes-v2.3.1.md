# ForgePact 2.3.1

A quick fix on 2.3.0: Mining Ore Extra Rolls no longer crashes the game, and a
crash report now says what crashed.

## Fixed

- **Mining Ore Extra Rolls crashed the game (#222).** In 2.3.0, with Loot →
  Mining Ore Extra Rolls above 1, finishing a mining node on a dig that gives
  mining XP could crash the game. ForgePact held back the floating XP text
  during the extra rolls, but the game goes on to use that text, and the error
  that followed was passed back to the game. Now the extra rolls make their
  floating XP text as the game expects, and ForgePact removes those extra texts
  as each roll ends, so a node is meant to show one XP text, as without the mod.
  If an extra roll ever fails anyway, ForgePact no longer passes the error back
  to the game: it stops rolling, leaves the node dug out, turns extra rolls off
  until you restart the game (the ore multiplier keeps working) and writes one
  `miningrolls: an extra roll failed` line to `out.txt`.

  This is tested outside the game, with a stand-in for the dig that uses the
  text the way the game does; it has not yet been checked in a game session.

## Changed

- **A crash report now says what crashed (#223).** Until now a crash report
  could only say that the game had not closed cleanly: the hook and mod read
  `unknown`, and when Windows kept no record of the crash, nothing named what
  crashed either. ForgePact now writes down, as the game dies, the error code,
  the file it happened in (the game, ForgePact's plugin or another DLL) and
  where in that file, whether it was the game's main thread, which ForgePact
  hook and mod were running, and, for an error thrown as a C++ exception, its
  type. The next start folds that into the report, together with the end of
  the game's `YYToolkit.log` and `aurie.log` as they were at the crash and a
  small crash dump, `crash.dmp`, that holds the stack of every thread and the list of
  loaded files but none of the game's other memory. Every report, crash, freeze
  or FPS drop, now carries the end of those two logs too. As before, a report
  is saved without any notification and nothing is uploaded. A game that was
  killed, or that ended itself without raising an error, still leaves a report
  without that record.

## How to update

Download and extract the complete release, then reopen ForgePact and press
**Install Mod Plugin** once. Only the plugin changed (the extra rolls and the
crash reports); the panel works as in 2.3.0. Your existing settings are
retained. Start the game after installing; a game that was already open keeps
the old plugin until it is restarted.

Use ForgePact only with an offline / EAC-disabled copy of Hero Siege.
