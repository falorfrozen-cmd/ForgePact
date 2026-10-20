# High damage on a training dummy - what ended the game on 2026-10-20 (ForgePact #230)

This doc carries no decompiled or disassembled text: the static reading below
is the mechanism in words, every quoted log line is ForgePact's own output or
a tool of this toolkit's, and no address or byte inside `Hero_Siege.exe` or
`PAS_Steam_GMS_x64.dll` is written here (they stay in the workorder's
gitignored files). The game facts are folded into the hub's
`docs/RUNTIME_DATA_MODELS.md` § 8.7, the shortener's arithmetic is the spec
`docs/models/number-compress-spec.md` and its model
`hs_game_sdk.number_compress_model`.

## Status

2026-10-20: **`crash-route: compress-number`, reproduced and fixed.** With
the game's floating damage numbers switched on and compressed, a damage
number of 100 trillion or more reaches the game's Steam number shortener,
which ends the process with a fast-fail (`0xC0000409`, subcode 8) that no
in-process handler sees. Live 2 reproduced it on the research build with
ForgePact's cap forced off: the game ended in the x1e6 window, the last
number handed to the shortener was 102,480,421,310,000 (1.02e14), and
Windows' dump names `PAS_Steam_GMS_x64` with `SteamCompressNumber` on the
stack. With the cap automatic, the same ramp ran through x1e12 (numbers up
to 1.0e20 handed in), the game answered after every window, every damage
number read `99999B`, and the game closed with `0x00000000`.

The fix, `numguard` (`plugin/include/ForgePact/NumberGuard.hpp`): an inline
detour on `num_compressSteam`, installed by name through `HookOneScript`
once a Modifiers lever that can raise a shown number is set off vanilla
(`stat`'s damage and crit entries `damage`, `critdamage`, `critchance`,
`spellcritdamage` and `spellcritchance`; `statadd`; `skillslider`) and never
at vanilla. Experience, gold, Magic Find, speed, replenish and defense
multipliers do not arm it. Each call still runs the game's own shortener; a value
it cannot show (1e14 or more, -1e6 or less, NaN) is handed to it as the
largest value it can show, 99,999,000,000,000 (shown `99999B`), or -999,999,
or 0 for NaN. The damage dealt, the DPS sums and enemy health are untouched:
only the number being turned into text is replaced, in a copy of the
argument. `numguard stat` (both builds) reports whether it is armed, by which
route, how many numbers it capped and the first of them.

## Evidence

Two incident bundles from the reporter, both written on 2026-10-20 by plugin
and panel 2.3.1 on game 7.0.13.0 (Windows 10 build 19045, not the machine the
live sessions ran on): `20261020-121708_crash` and `20261020-121926_crash`.

- **Two sessions, the same ending.** Session 1 ran about 60 s in Act 1's
  town against several training dummies; session 2 about two minutes in Act
  5's town against one. Both exited `0xC0000409`. Neither reached the
  `ExitProcess` marker (`after_clean_shutdown` false), the event-log probe
  found no record for either (`faulting_module` null), and the crash trap
  caught nothing (`trap` null, `trap.txt` holds only its header).
- **The last lines.** Each session's plugin log ends with the three `first
  boosted call` lines written once the character is in the world (`statadd
  StatSpellHaste: first boosted call 40 -> 240` among them). Nothing follows:
  the game ended while the player hit dummies, with no ForgePact line in
  between.
- **The settings.** `forgepact.json`, `modstate.json` and `mods.txt` are the
  same in both bundles. The 63 plugin lines they make include Total Damage
  1000% (`stat CalculateEndDamage -> x11.00`), Critical Strike Damage 1000%
  (x11) and Critical Strike Chance 500% (x6), the three slider maxima, plus
  attack speed x2, cast rate +100, skill haste +200, all skills +100,
  projectile amount +5, area +50, experience x100 and mining rolls x10. The
  relic filter line shows all 141 relics at 10/10, an endgame character.
- **What the bundles could not say.** They carry no damage value, no dump and
  no copy of the game's own display options (`shop.ini`), so which formatter
  ran, and with which number, had to come from the static reading and the
  sessions below.

## Static reading

Read locally on 2026-10-20: the Steam extension
`bin\PAS_Steam_GMS_x64.dll` (127,488 bytes, sha256
`c789ee5e5850e0d68c42a6d0b84876509eaea4f2ffab1ea9c35e71709c47b88f`) with
MSVC's `dumpbin /disasm`, and the named Ghidra project of `Hero_Siege.exe`
(sha256 `dab03f134327d9b682c67f1dc461822fb822a7e535009636624bb1454f498e2b`)
through the hub's Ghidra MCP, with byte scans of the exe for direct calls
and references where the project's caller tables came back empty. The
listings stayed on that machine. Each point is a static reading, in my own
words; Live 2 has since measured the parts marked so.

- **The shortener writes into 8 bytes.** `SteamCompressNumber` takes one
  number and returns a pointer to a static 8-byte text buffer, which it
  clears first. Zero gives "0". A negative is written whole after a minus
  sign. Otherwise: below 10,000 the whole number; up to a million it is shown
  in thousands with one decimal and `K` ("12,3K"); below a billion in
  millions with `M`; from a billion up in billions with `B`. The decimal
  follows a comma and is dropped when it is 0.
- **It ends the game for a number it cannot fit.** Every write into the
  buffer is range-checked by the compiler, and a failed check ends the
  process with fast-fail code 8 (`FAST_FAIL_RANGE_CHECK_FAILURE`, exit
  `0xC0000409`). That happens for any number of 100 trillion (1e14) or more,
  for a negative of a million or more in size, and for NaN; past about
  2.1e17 the whole-number conversion inside it overflows and fails the same
  way. The largest positive number it can show is just under 1e14, shown
  `99999B`. Measured in Live 2: a number of 1.02e14 ended the game with that
  code and subcode, raised in this DLL.
- **`num_compressSteam` is the game's only route to it.** The script
  (`gml_Script_num_compressSteam`, SDK 2608) takes one argument and returns
  the export's text; the extension is referenced once in the exe's code,
  there. `num_compressFunc` stores it and `num_compressGML` (2607, a
  shortener written in GML with no fixed buffer) as method values, and
  nothing in the exe calls either script directly.
- **Every caller goes through the global `num_compress`.** It is read by the
  damage text (`Combat_Text_obj`), the DPS meter (`DPS_Meter_obj`), enemy
  health bars and HUD code, and a few menus (hub RDM § 8.7 lists them). Which
  shortener the global holds on the Steam PC build could not be read
  statically. Measured in Live 2: `num_compressSteam`, for the damage text.
- **Three display options decide whether a hit's number gets there.**
  `showDamage` creates the floating damage number for a hit on an enemy;
  `combatTextCompress` makes the damage text format it through
  `num_compress`; `dpsMeterCompress` does the same for the DPS meter, which
  otherwise uses `RealFormat` (thousands separators, no fixed buffer). They
  live in the save directory's `shop.ini`, `[settings]`, as `show_damage`,
  `combat_text_compress` and `dps_meter_compress`. That the game applies the
  file at start was inferred; Live 2 measured it (damage numbers drawn after
  setting `show_damage` in the file alone).
- **Why the crash trap saw nothing.** A fast-fail ends the process in the
  kernel without dispatching an exception in it, so neither the plugin's
  exception handlers nor its exit marker can run; Windows still writes a
  dump when the error mode allows, which is what `tools/crash_dump_stack.py`
  (hub) reads.
- **Vanilla.** The shortener fails for such a number with or without
  ForgePact. Whether a vanilla character can put 1e14 on screen is not
  tested; ForgePact's Modifiers make it reachable.

## Instrument

- **The research trace** (research build only), `numguard trace on|off`.
  It hooks `num_compressSteam` and `StatsManager`'s `CalculateEndDamage` as
  pass-throughs if nothing armed them, and writes `numguard trace: <site>
  <value>` whenever a site's value reaches a new decade (at most 400 lines a
  session). The `num_compressSteam` line is written before the game's
  shortener runs, and `Out()` closes `out.txt` before returning, so the last
  such line before a fast-fail names the number that ended the game.
- **The cap forced off** (research build only), `numguard off` /
  `numguard auto`. With it off every number reaches the shortener as given,
  still counted and traced, so the research build reproduces the 2.3.1
  behaviour with the instrument in place.
- **The status line**, both builds: `numguard: <armed|not armed>
  route=<native|table-only|failed|none> clamped=<k> first=<value|-> calls=<n>`,
  plus `forced=`, `trace=` and `largest=` in the research build. The first
  cap of a session writes one `numguard: capped <value> -> <shown value>
  (shown "<text>")` line.
- **The debugger**: the hub's `x64dbg` MCP (`docs/tools/x64dbg-mcp.md`),
  non-breaking hardware logpoints on a control (`CheckTalentUse`, which logged
  steadily in both sessions) and on the candidates `num_compressSteam`, `num_compressGML`,
  `SteamCompressNumber` (and in Live 1 `RealFormat`), each armed only after
  its first bytes matched the installed files, and detached before any crash
  so Windows, not the debugger, takes the fast-fail. The addresses were
  mapped from the Ghidra project and are research findings only, kept out of
  this repository.
- **The dump**: hs-drive launches the game with the default error mode and
  records every exit code; the hub's `tools/crash_dump_stack.py --pid <pid>
  --expect-module PAS_Steam_GMS_x64` reads the code, subcode, raising module
  and stack from the dump Windows wrote.

## Live procedure 1

Run 2026-10-20 (capture in the hub workorder `forgepact-230-damage-crash`,
Live 1): research DLL `3a8c2884...b0e7d173` (ForgePact `c654f87`), slot 14
"Sorak" in Act 1's town, launched by hs-drive with no panel.

- The reporter's 63 lines, made by ForgePact v2.3.1's own `build_cmds` from
  the reporter's `forgepact.json`, sent to the running game.
- A spot 96 px south of a training dummy (`playerwarp 1816 800`), windows of
  two 10 s held clicks; Sorak's own attacks hit the dummies.
- Launch A: a vanilla window under the debugger, then `numguard off` and
  the trace, then a ramp of `stat damage` x11, x100, x1e4, x1e6, x1e8, x1e10
  and x1e12, one window each. Launch B: the same ramp with the cap automatic.
- **Why it could not reach the shortener.** The owner's `shop.ini` had
  `show_damage` 0 and `dps_meter_compress` 0, so no floating damage number
  was ever drawn and the DPS meter used `RealFormat`. Nothing in the session
  called `num_compress`. The procedure did not set the display options,
  because the route through them had not been read yet.

## Live procedure 2

Run 2026-10-20 (capture: the hub workorder's Live 2): research DLL
`aa77e122...44d75b98` (ForgePact `36890da`), the same character, spot,
reporter's lines and ramp. The one change: the display options, set in
`shop.ini` with the game closed after a save backup, and restored at the end.

- Launch A, `show_damage` 1 and `combat_text_compress` 1 (the latter
  already on in the owner's file): a vanilla window with a screen grab
  mid-hold, a vanilla window under the debugger, then `numguard off`, the
  trace, the reporter's lines and the ramp, stopping where the game ended;
  then the exit code and the dump.
- Launch B, `dps_meter_compress` also 1: `numguard stat` before any lever,
  the reporter's lines, `numguard trace on`, then the ramp through x1e12 with
  the cap automatic, `ping` and `numguard stat` after each window and a grab
  in each.
- **As run, it differed in two ways.** From the debugger window on, the
  owner had the windows use right-click casts on the dummy and movement only
  by `playerwarp`, instead of held left clicks that walk the character
  away. Launch B's x100 window had no screen grab.

## Results

### Live 1 results

- **Measured.** In a 20 s vanilla window the debugger logged 0 hits on
  `num_compressSteam` and 0 on `SteamCompressNumber` (0 all session) while
  the control logged 792 and `RealFormat` 820. `numguard` counted `calls=0`
  through the whole ramp in both launches, while
  `numguard trace: CalculateEndDamage.returned` rose to 1.63e18. The DPS
  meter drew full digits up to `7 896 498 402 070 754 304` (about 7.9e18) and
  both launches exited `0x00000000`.
- **What it means.** For the shortener this is "not observed", not a
  refutation: the display options kept every `num_compress` consumer off. It
  is a measured positive for the DPS meter's `RealFormat` route, alive at
  7.9e18.

### Live 2 results

- **The route, measured.** Damage numbers were drawn over the dummies from
  the `shop.ini` edit alone (`970,4K`, `1,2M`, `5M` in the mid-hold grab).
  In the debugger's vanilla window `num_compressSteam` and
  `SteamCompressNumber` each logged 748 hits, one for one, while the control
  logged 322. `num_compressGML` logged 0 hits in the same window, with its
  bytes checked and the control logging: on this build the damage text's
  `num_compress` is `num_compressSteam`, and the GML shortener was not
  observed.
- **The crash, reproduced** (launch A, cap forced off). Traced numbers handed
  to the shortener, in order: 2,915,427 and 11,012,142 (vanilla); 170,962,231
  (as the reporter's lines went in); 1,025,141,275 (x11 window); 10,465,191,980 (x100);
  114,222,292,100 and 1,016,700,999,360 (x1e4); 15,038,794,170,000 and then
  **102,480,421,310,000** (x1e6). The game ended during the first hold of
  the x1e6 window, exit `0xC0000409`. The dump:
  `code=0xc0000409 subcode=8 raised-in=PAS_Steam_GMS_x64`, with
  `SteamCompressNumber` the third frame, under it the game's code and
  ForgePact's forced-off detour passing the number through. Every earlier
  traced number was positive and below 1e14, and no negative was traced.
- **The fix, measured** (launch B, cap automatic). Before any lever:
  `numguard: not armed route=none clamped=0 ...`. After the reporter's
  lines: `numguard: armed route=native clamped=0 first=- calls=0 forced=auto
  ...`. The first number past the limit came in the x1e6 window:
  `numguard: capped 100300112243000 -> 99999000000000 (shown "99999B")`,
  `clamped=448` after that window. Traced numbers rose through 1.16e15,
  1.02e16, 1.16e17, 1.01e18 and 1.14e19 to 1.00752867645e20 in the x1e12
  window (`largest=2.1921742131e+20`, past the shortener's whole-number
  overflow too); `ping` answered after every one of the seven windows; the
  status after the last read `clamped=29990 ... calls=78421`. Every floating
  number over the dummies in the x1e12 grab read `99999B`. The game closed
  through `hs_stop_game` with `0x00000000`.
- **At the reporter's x11 on Sorak** the largest number handed to the
  shortener was about 2.1e9 (`largest=2075940756`), 50,000 times short of the
  limit: this character needed x1e6 to reach it.

### What the two sessions settle

The crash is the game's own shortener, reached through the compressed damage
text: reproduced with the reporter's settings plus a higher damage lever,
with the dump naming the module and code the static reading predicted and
the trace naming the number. The cap at `num_compressSteam` holds past every
number the ramp produced. The trap, the exit marker and the event log are
blind to this class of crash by construction; the exit code and the dump are
the evidence.

## Not established

- **How the reporter's character reached 1e14 at x11.** Sorak reached about
  2.1e9 there; the bundles carry no damage value. A much stronger endgame
  character is the likely difference, but nothing measured it.
- **Why the shortened numbers run larger than one hit.** In both launches
  the traced shortener values were several times the largest traced
  `CalculateEndDamage` return of the same window (for example 1.5e13 against
  3.6e12 at x1e6). Whether the damage text adds hits together, applies a
  multiplier after `CalculateEndDamage`, or something else, was not read.
- **The compressed DPS meter.** In launch B, with `dps_meter_compress` 1, the
  DPS meter read `0` in the x1e12 grab (and `Sorak 0` right after loading,
  before any hit). Two readings narrow it: the meter already read `Sorak 0`
  while `numguard` counted `calls=0`, before the shortener had seen a single
  number, and the first cap substituted a real number
  (`first=100300112243000`), not NaN, so the cap's NaN-to-0 substitute was
  not the source of that `0`. What the meter shows with compression on, and
  whether the cap plays any part in the later `0`, is still not established:
  no window ran with the meter compressed and the cap forced off, which is
  what would separate the game's own behaviour from the cap's. Launch A's
  meter (not compressed) drew full digits.
- **The other `num_compress` consumers** (enemy health bars, HUD, menus)
  get the same cap, since it sits on the shortener they all meet, but none of
  them was observed past the limit.
- **The negative band and NaN.** No negative number and no NaN was traced
  in either session; the -1e6 limit and the NaN failure stay a static
  reading.
- **Eight-character texts.** A number between about 1e13 and 1e14 with a
  nonzero decimal fills all 8 bytes, leaving the text's terminator past the
  buffer. The game kept running after 15,038,794,170,000 reached the
  shortener in launch A; what it drew for it was not read.
- **Vanilla.** Whether an unmodded character can reach 1e14 was not tested.
- **The player build.** Both sessions ran the research build. The player
  build shares the detour, the arming and the cap (the harness tests in
  `tests/test_number_guard_behavior.py` run both builds), but it was not run
  in a game session.
