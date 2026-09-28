# ForgePact 2.1.0

## New

- **Frame profiler.** A new plugin command, `frameprof start`, measures what
  the game itself spends its frames on while you play, so a scene that
  stutters or drops frames can be pinned on the code that makes it slow
  instead of guessed at. Start it where the game is slow and keep playing;
  after 30 seconds (or `frameprof start <seconds>`, or `frameprof stop`) a
  short summary appears in `bp_ipc\out.txt`: frames per second, the median
  and worst frames, how the time split between the game's own code, the
  graphics driver, the GameMaker runtime, mods and waiting, and the heaviest
  events, scripts and built-in functions. The full report, with what ran
  during each slow frame, a per-second record and the CPU used by every game
  thread, is saved in `bp_ipc\perf`, and `tools/frameprof_report.py` turns it
  into a page you can open in a browser.

  It changes nothing in the game and costs nothing until you start it. While
  it runs it briefly pauses the game 250 times a second, which normally costs
  under 2% of the game's frame time, and it slows itself down if that ever
  adds up to more than 3%. There is no panel switch yet: send the command with
  `tools/ipc.ps1` or any tool that writes `bp_ipc\cmd.txt`.

- **Far scenery sleep.** A new switch in Mods → Quality of Life, off by
  default. A zone's far trees, bushes, hay, rocks and fences are put to sleep
  so the game stops updating them every frame, and they wake again before they
  come into view. In Act 1's first zone about 4,200 of its 6,200 objects sleep,
  and the game's own work per frame drops by about a sixth: at 60 fps that is
  spare time, and in crowded zones where frames run long it is frame time.
  Shrines, chests, traps, walls and monsters are never touched, towns and menus
  are left alone, and switching it off wakes everything at once.

## Fixed

- **A plugin message with a percent sign could close the game.** When the
  plugin printed a message containing `%` (or a very long one) to its log,
  the game could stop at once with an error from Windows' C runtime. It was
  seen once, while the new frame profiler printed its summary; any message
  could have triggered it. Messages are now printed safely, and every one
  still reaches `bp_ipc\out.txt` in full.
