# Graceful stops that read 0xC0000409 (ForgePact #201)

When the hub's hs-drive stops Hero Siege gracefully (`hs_stop_game`, which
posts `WM_CLOSE` to the game window), the game's exit code reads `0xC0000409`
even though `bp_ipc\out.txt` ends `==== clean shutdown ====`. This doc records
what raised it, as far as the evidence on this machine reaches, and the two
live sessions that test the rest.

This doc carries no decompiler output. The one static reading below (the pause
menu's Exit) was made locally and is written here in our own words: script and
room names, and what the script does in what order, never its text. The only
offsets quoted are those of our own and third-party DLLs (the tracker
producer, `ucrtbase.dll`); no game address or game offset appears.

## Question

ForgePact #201: when `hs_stop_game` closes the game and reports `0xC0000409`,
what raised the fast fail, and do ForgePact or the way the game was closed have
anything to do with it?

The owner's hypothesis (2026-10-13): the error comes from closing the window
without first taking the game's own way out, Escape -> Exit ("Save & Exit"),
which saves and leaves the world before the window closes. The owner asked to
test it next to a mods-on / mods-off split, with a positive control showing the
instrument can read a code other than `0xC0000409` on a path expected to exit
cleanly, keeping to representative cases.

The fix is not part of this question. If the producer is the only cause, the
follow-on is to re-pin `modfiles_shipped/HSOfflineTrackerProducer.dll` to
HS-Offline-Tracker v0.1.4's fixed build; that is a separate change.

## What is already on record

- **The tracker producer aborts at exit** (hub guide, Known Limitations
  item 25). The HS-Offline-Tracker producer shipped in every ForgePact release
  from 1.3.10 (`modfiles_shipped/HSOfflineTrackerProducer.dll`, sha256
  `36608aa0…b14e`, a 2026-09-05 build; 1.3.8 and 1.3.9 shipped Tracker 0.1.1's
  `8594d096…`, see the #185 section) keeps its publisher in a global
  `std::thread`. When the game leaves through `ExitProcess`, that thread is
  still joinable when the DLL's globals are destroyed, so its destructor calls
  `std::terminate`: fast fail 7, which reads as `0xC0000409`. Nine of the ten
  dumps Windows kept on 2026-09-25/26 showed it. HS-Offline-Tracker PR #8 saw
  it on a plain main-menu close; its fixed producer exited `0` three times out
  of three. HS-Offline-Tracker v0.1.4 (published 2026-10-06) ships that fix;
  ForgePact still pins the old build.
- **ForgePact's own coop thread did the same until PR #90** (guide item 26).
  On 2026-09-26, with the producer moved out of `mods\aurie`, research build
  `9915ca5` exited `0` on a main-menu `CloseMainWindow`. Before the sessions
  below, nothing had measured an in-world close without the producer, and
  hs-drive had never reported `0x00000000` for the game.
- **hs-drive's exit codes** (`docs/tools/hs-drive-mcp.md` in the hub, § "Exit
  codes", Verification X5): #173's two graceful closes read `0xC0000409` after
  `==== clean shutdown ====`. `dropmult-gold-crash-research.md` § Live 2
  results: that dump's stack was not read.
- **The bag file at a stop** (the hub's `docs/RUNTIME_DATA_MODELS.md` § 9.5,
  hub #506): stops since 2026-10-04 have not rewritten
  `inventory_order_<N>.hss`, while the game's Game End event asks for the bag
  save (a static reading). § 15 there has the exit-abort row.

## Static evidence

All gathered read-only on this machine on 2026-10-13.

**The ten kept dumps.** `%LOCALAPPDATA%\CrashDumps\Hero_Siege.exe.<pid>.dmp`,
2026-10-12 17:06 to 2026-10-13 09:59 (pids 31308, 27324, 26788, 27952, 20972,
15772, 20580, 51596, 51748, 20124), read with `cdb.exe` from the Windows Kits
debuggers and an empty local symbol path. Every one shows the same thing:
exception `c0000409`, subcode 7 (`FAST_FAIL_FATAL_APP_EXIT`), raised at
`ucrtbase!abort` from `terminate`, called by `HSOfflineTrackerProducer.dll`
(return address at its offset `0x1AA03`, inside the function at `0x1A9F0` that
guide item 26 names as the producer's static `std::thread` destructor), run
from the producer's `_execute_onexit_table`, under `LdrShutdownProcess`, under
`RtlExitUserProcess`, under the kernel32 exit call, under the same four game
frames in all ten. The producer image in them is timestamped 2026-09-05. Nine
of these pids are graceful `hs_stop_game` stops of the #197 sessions (their
captures say so), most with a character in the world: pid 51748 launched,
loaded slot 14 and was stopped 57 s later. So the in-world graceful stop is
already attributed to the producer, from the dump, on every stop that left one.

**The 2026-10-08 batch closes** (#198-#200 Live 1, pid 14440, and Live 2, pid
9204). Their dumps have rotated out (Windows keeps ten), but the Application
log keeps an Application Error event (`ucrtbase.dll`, `0xc0000409`) for each
pid, and `C:\ProgramData\Microsoft\Windows\WER\ReportArchive\` keeps both
reports: fault offset `0xa527e` in `ucrtbase.dll` (`abort+0x4e`), exception
data 7, WER bucket `b91e9d5db4dfa653d4bd2ed51598b5ab`, with
`HSOfflineTrackerProducer.dll` among the loaded modules. The report of the
dump-attributed 2026-10-12 23:38 stop (pid 20580) carries the same bucket. The
bucket keys on ucrtbase's abort, so it agrees with the producer without
proving it.

**The installed chain.** `mods\aurie\HSOfflineTrackerProducer.dll` is sha256
`36608aa0…b14e` (the pinned old build), beside `BloodPactPlugin.dll`
(`dd87086c…`, as installed at 2026-10-13 09:43, when this reading was made),
`YYToolkit.dll` (`51a393d7…`) and
`AurieCore.dll` (`3cf98af9…`, the hub's `aurie-v2.0.2-hs.1`). In the WER
report the producer loads after `BloodPactPlugin.dll`, and teardown runs in
reverse load order, so while the producer is present ForgePact's own
exit-time code never runs, and any fault of its own would be masked. That is
inferred from the order, not measured.

**The logs.** `bp_ipc\out.txt` ends `==== clean shutdown ====`, the incident
monitor's marker written at `ExitProcess`, before any DLL teardown.
`YYToolkit.log` and `aurie.log` stop before the abort. No log names the
faulting module; the dump does. The hub's `tools/crash_dump_stack.py` (#201)
reads one dump through `cdb.exe` and prints `raised-in=<module>`, the topmost
frame's module outside the C runtime and the Windows loader, so a session can
attribute a stop without opening a debugger: `py -3 tools/crash_dump_stack.py
--pid <pid>` from the hub root.

**The pause menu's Exit (a static reading, 2026-10-13, in our own words).** The
game names `UiAIngameExit` beside `UiAIngameRestart`, the Restart activation
the hub's `RUNTIME_DATA_MODELS.md` § 8.4 measured, so it is read as the Exit
button's activation. It saves through `SaveLocalFile` (six call sites; which
file kinds they ask for was not read), calls `DisableEnemyStep`, and then
changes room to 150, which `hs-game-sdk` names `Chose_rm` (the character
select). It does not end the game. So Escape -> Exit -> close is: the game's
own save, a room change to the character select, and then the same window
close from a menu room. The producer's abort happens in process teardown,
after the game's own exit call, so this reading predicts that the Exit path
changes nothing about `0xC0000409`. Live procedure 1 measures it. Not
established statically: the button's text, its `uiNodeCallstack`, whether a
confirmation follows, and whether an injected Escape opens the pause menu at
all (on 2026-09-22 it was opened by hand). The sessions measured all four
(§ "Results"): an injected Escape opens it, the button reads "Save & Exit"
with `uiNodeCallstack` `PauseSaveExit`, no confirmation follows, and the game
reaches `Chose_rm` with both save files rewritten. That the button runs
`UiAIngameExit` stays a static reading.

## Live procedures

Two sessions, both on the ForgePact DLL already installed (no build, no
install), slot 14 "Sorak", with `ping` -> `pong (YYTK 4.0.1)` as the control.
hs-drive gained three instruments for them: `hs_exit_to_menu`, which presses
Escape and clicks the pause menu's Exit where `menulayout` lists it; a
self-check, `exit_code_readback`, which proves the exit-code reader reads a
clean code and a fast-fail code exactly; and the dump reader above. No
debugger is attached: the question is which module raises the fast fail in
process teardown, x64dbg has to be detached before `hs_stop_game` and so
cannot see teardown, and the dump answers it.

**Live procedure 1, the producer present.** After the self-check reads both
codes back and the producer's hash is confirmed, two cells. Cell B is the
in-world close: launch, load slot 14, `hs_stop_game`. It is expected to read
`0xC0000409`, end `==== clean shutdown ====`, and leave a dump whose
`raised-in` is `HSOfflineTrackerProducer`. Cell C is Save & Exit then close:
launch, load slot 14, `hs_exit_to_menu` to `Chose_rm`, then `hs_stop_game`.
The owner's hypothesis predicts `0x00000000` here; the static reading predicts
`0xC0000409` with the same dump. Each cell also records whether
`inventory_order_13.hss` was rewritten, for #506 (the Exit is expected to
rewrite it, the in-world stop is expected not to).

**Live procedure 2, the producer held aside.** With the owner's go-ahead, the
driver moves the producer out of `mods\aurie` (never deleting it, checking its
hash on both sides) and puts it back afterwards. Four cells: E, a main-menu
close, the clean-exit control, which repeats PR #90's 2026-09-26 result on
this build and would be the first `0x00000000` hs-drive has read; D, an
in-world close with every mod off; G, Save & Exit then close; F, an in-world
close with three mods on: `lootann 1` and `stashmoveall 1`, which hook at
character load and add a UI node, and `frameprof start 600`, the outlier, a
ForgePact worker thread running at teardown, held in `ExitSafeThread`
(`plugin/include/ForgePact/FrameProfiler.hpp`, `m_Thread`), the guide item 26
fix, not the plain `std::thread` shape that aborted. Each is expected to read
`0x00000000` and leave no dump. A
fifth cell, H, closes the session with the driver putting the producer back
and a main-menu close: it aborts (HS-Offline-Tracker PR #8 saw it on a plain
main-menu close) and so is the dump reader's positive control in the same
session, which a "no dump" in the four cells before it needs (the owner's
amendment, 2026-10-13). A non-zero code in any cell is recorded with its
dump's `raised-in`, and the session goes on.

**Why the matrix is smaller than the owner's.** The owner asked for
{vanilla, ForgePact with mods off, mods on} x {Save & Exit then close, close
in-world, close from a menu}. The evidence points at which DLLs are loaded at
teardown, so the cells follow that factor instead:

- With the producer present, the in-world close (the issue's case) and Save &
  Exit then close (the hypothesis). The main-menu close is already on record
  (HS-Offline-Tracker PR #8). Mods on is left out: the producer aborts first
  and would mask anything later.
- With the producer held aside, the main-menu close (the control), the
  in-world close with mods off, Save & Exit then close, and the in-world close
  with mods on.
- No vanilla cell. hs-drive refuses to launch an unpatched executable
  (`mod_chain_incomplete`), so it could not read that exit code, and restoring
  the unpatched executable changes the install. If every producer-absent close
  reads `0x00000000`, the game and ForgePact are both clean on those paths and
  a vanilla cell adds nothing. If one aborts, its dump names the module, and a
  no-ForgePact cell is planned then.

"Mods off" means every mod off at the stop. `hs_select_character` arms
`orbpickup` and switches it back off, so that hook may be installed.

The two sessions decide two things. Exit attribution: producer only, when
both producer-present codes read `0xC0000409` with the producer's dump and all
four producer-absent codes read `0x00000000` with no dump; another module, when
any producer-absent code is not `0x00000000`, named by its dump; not settled
otherwise. The Save & Exit hypothesis: refuted when the producer-present Save
& Exit close still reads `0xC0000409`; supported when it reads `0x00000000`
while the in-world close reads `0xC0000409`; not observed when the Exit route
did not complete.

## Results

Both sessions ran on 2026-10-13, back to back, on the ForgePact DLL already
installed (`BloodPactPlugin.dll` v2.2.1, sha256 `8a16a38e…4bc92`, the same at
both leases; another workorder, forgepact-158, had installed it after the
static reading's `dd87086c…` chain was read, and this workorder installed
nothing), slot 14 "Sorak", with `ping` -> `pong (YYTK 4.0.1)` and
`hs_selfcheck`'s `exit_code_readback` reading `0x00000000` and `0xC0000409`
back exactly before each. No debugger, no build, no install. The captures are
`forgepact-201-exit-c0000409-live-1` and `forgepact-201-exit-c0000409-live-2`,
kept with the hub's workorder; both sessions restored their save backups and
inspected clean afterwards. All readings below are **measured**.

**Live procedure 1, the producer present** (capture
`forgepact-201-exit-c0000409-live-1`, 17 checks, 16 pass). Cell B, the
in-world close: slot 14 loaded, `hs_stop_game` (`WM_CLOSE`, not forced) read
`0xC0000409`, `out.txt` ended `==== clean shutdown ====`, and the dump reader
named the producer: `code=0xc0000409 subcode=7
raised-in=HSOfflineTrackerProducer`, the faulting thread being `ucrtbase!abort`
<- `terminate` <- `HSOfflineTrackerProducer+0x1aa03` <- `execute_onexit_table`
<- the producer's exit-time code <- `LdrShutdownProcess` <-
`RtlExitUserProcess`, the stack the ten kept dumps showed. Cell C, Save & Exit
then close: `hs_exit_to_menu` opened the pause menu with an injected Escape,
found one new button, a `UI_Button_obj` reading "Save & Exit" with
`uiNodeCallstack` `PauseSaveExit`, clicked it on the first call, and the game
reached `Chose_rm` with no confirmation in between. The close from
`Chose_rm` still read `0xC0000409`, after the clean-shutdown line, with the
same twelve-frame dump naming the producer. The one check that failed is a
research one, and it failed on the outcome hub #506 predicted rather than
against it: the check passed on a rewrite, and the in-world stop left the bag
file `inventory_order_13.hss` unchanged (it rewrote `herosiege13.hss`), which
is what #506 predicted. The Exit rewrote both files within the same second,
and the close from `Chose_rm` after it rewrote neither.

**Live procedure 2, the producer held aside** (capture
`forgepact-201-exit-c0000409-live-2`, 25 checks, 24 pass). The driver moved
`HSOfflineTrackerProducer.dll` out of `mods\aurie`, hash-checked on both
sides, and `test ! -e` confirmed it gone. Cell E, the main-menu close with no
character, read `0x00000000`, the first clean code hs-drive has read, and left
no dump (the reader waited 30 s and exited 3). Cell D, the in-world close with
every mod off, read `0x00000000`, ended `==== clean shutdown ====` and left no
dump. Cell G, Save & Exit then close, reached `Chose_rm` through the same
"Save & Exit" row on the first call and read `0x00000000` with no dump. Cell
F, the in-world close with `lootann 1`, `stashmoveall 1` and `frameprof start
600` switched on after the character loaded (each replied `on`, `frameprof`
naming its 250-per-second sampler for 600 s), read `0x00000000` with the
clean-shutdown line and no dump, so ForgePact's own exit-time code, the
`frameprof` worker thread included, exits cleanly on these paths. That the
worker was still running at teardown is inferred, not measured: the 600 s had
not elapsed and no capture summary line appeared before shutdown. Cell H,
after the driver put the producer back (its sha256 `36608aa0…b14e` confirmed
in full), was a main-menu close that read `0xC0000409` with a dump naming
`HSOfflineTrackerProducer`, so dumps were being written in that session and
the four "no dump" readings before it are evidence. The failed check is again
the bag, and again on #506's predicted outcome (the check passed on a
rewrite): the in-world stop left `inventory_order_13.hss` unchanged with the
producer absent too, while the Exit rewrote it, so #506 is not the producer's
doing.

So the only module that turned a graceful close into `0xC0000409` on this
machine is the tracker producer, whether the close came from the world, from
the main menu or after the game's own Save & Exit, with every mod off, and
(producer held aside) with `lootann`, `stashmoveall` and `frameprof` on. The
owner's hypothesis is refuted: Save & Exit first does not change the code
while the producer is loaded, and all four closes made without it read
clean. Not covered: a vanilla game and a game without ForgePact (see "Why
the matrix is smaller than the owner's"), other mods, and close paths other
than these four.

exit-attribution: producer-only
saveexit-hypothesis: refuted

The follow-on, which continues in ForgePact #185, is a fix workorder: re-pin
`modfiles_shipped/HSOfflineTrackerProducer.dll` in `tools/toolchain-pins.json`
to HS-Offline-Tracker v0.1.4's fixed build, with provenance, release notes
(the follow-on asks the owner for a milestone before writing them), and a live
gate in which an in-world `hs_stop_game` reads `0x00000000` with the re-pinned producer. Whether
`hs_stop_game` should take Save & Exit first is not decided by the exit code
(it changes nothing there); it rests on #506's bag observations, which these
sessions add to: an in-world stop rewrites the character file and not the bag
file, with the producer present or absent, and the game's Save & Exit
rewrites both.

## Follow-on: the fixed producer pinned (ForgePact #185)

ForgePact #185 carried out that follow-on. `tools/toolchain-pins.json` now
takes `modfiles_shipped/HSOfflineTrackerProducer.dll` from HS-Offline-Tracker's
own `v0.1.4` release: member `producer/HSOfflineTrackerProducer.dll` of
`hs-offline-tracker-0.1.4-portable.zip` (archive sha256 `37b03813…9709`, the
value the release's `SHA256SUMS-0.1.4.txt` states), member sha256
`5f83e542f41490ec57106198cf8546ec178421961e388699ff1da5de45956f94`. It replaces
the 1.3.16 package's `36608aa0…b14e`, a 2026-09-05 build (its PE link time).
The 0.1.4 build carries Tracker PR #8's exit teardown; how that avoids the
abort is written once, in the hub's `docs/RUNTIME_DATA_MODELS.md` (§15, the
row on a plugin DLL whose global `std::thread` is still joinable at exit).

Every Tracker producer released before 0.1.4, hashed from the release assets
on 2026-10-13 (each built from source that still keeps the publisher in a
global `std::thread`; only `36608aa0…` was measured aborting):

| Tracker | Producer sha256 | Read from |
|---|---|---|
| 0.1.1 | `8594d096…676d` | the NSIS installer (the release has no zip); the same extraction of 0.1.2's installer gives 0.1.2's zip member, the control for the method. ForgePact 1.3.8 and 1.3.9 shipped these bytes (their release zips' `ForgePact/modfiles/` member); 1.3.7 shipped no sensor |
| 0.1.2 | `36608aa0…b14e` | the portable zip; byte for byte the sensor ForgePact shipped from 1.3.10 through 2.2.x (1.3.10's release zip checked) |
| 0.1.3 | `9e0c63c8…a58d` | the portable zip |
| 0.1.4 | `5f83e542…6f94` | the portable zip; the new pin |

The 0.1.4 zip's `aurie-loader/AurieCore.dll`, `YYToolkit.dll` and
`AuriePatcher.exe` hash to ForgePact's own pins for those files (`3cf98af9…`,
`51a393d7…`, `4d3aec43…`), so ForgePact and Tracker 0.1.4 install identical
sensor and loader files.

What changed in ForgePact around the pin:

- `src/forgepact.py` names the builds: `TRACKER_SENSOR_ABORTING` (0.1.1 to
  0.1.3) and `TRACKER_SENSOR_CLEAN` (0.1.4, measured below).
  `tests/test_tracker_producer_pin.py` checks the pins file against them, with
  the old entry and the 0.1.1 and 0.1.3 builds as negative controls, and
  checks the loader hashes above.
- The pin lists `36608aa0…` under `superseded_sha256`, so
  `tools/fetch_toolchain.py` (run by `Prepare-Plugin.bat`) replaces that file
  on a checkout that already holds it, where it used to refuse it as
  hand-placed.
- A launch from a release build of the panel replaces a game sensor whose hash
  is in the aborting set (`refresh_stale_tracker_sensor`), under the plugin
  refresh's gates (the game's `YYToolkit.dll` and `AurieCore.dll` must be the
  bundled ones); otherwise the launch message asks for Install, unless the
  package lacks the fixed sensor, when it says to extract the complete release
  or update the Tracker instead.
- Install Mod Plugin installs the bundled sensor when the game has none, or
  has one the panel knows (aborting or clean); it leaves an unrecognised one,
  such as a newer Tracker's, as it is, and never installs a bundled build that
  is itself in the aborting set.
- Tracker 0.1.1 to 0.1.3's own "Update live sensor" puts their build back,
  and the two tools then swap the file; the hub guide's "Launch-Time Plugin
  Refresh" section describes it once, and the release notes tell players.

The fix ships in ForgePact 2.3.0.

> **Later decision (#185, 2026-10-14):** the bundled-sensor design above did
> not ship. ForgePact 2.3.0 ships no producer: Install Mod Plugin moves an
> aborting one aside and installs nothing in its place. See
> `tracker-research.md` § "The Tracker tab (part 2b)", Decisions, "ForgePact
> ships no producer (Q11)".

The live gate ran on 2026-10-13, in capture
`forgepact-185-tracker-mod-live-1`, kept with the hub's workorder. It used the
installed v2.2.1 plugin (`8a16a38e…4bc92`, unchanged at teardown), slot 14,
`ping` -> `pong (YYTK 4.0.1)` in every cell, and `hs_selfcheck`'s
`exit_code_readback` passing first. All 11 checks pass, and every reading below
is **measured**:

| Cell | Producer | Close | Exit code | Dump |
|---|---|---|---|---|
| A, the control | old, `36608aa0…` | main menu | `0xC0000409` | yes, `raised-in=HSOfflineTrackerProducer` (reader exit 0) |
| B | fixed, `5f83e542…` | in the world, every mod off | `0x00000000` | none (reader waited 30 s, exit 3) |
| C | fixed | after the pause menu's Save & Exit (`hs_exit_to_menu` reached `Chose_rm`) | `0x00000000` | none (exit 3) |
| D | fixed | in the world, `lootann 1` on (`install=installed`) | `0x00000000` | none (exit 3) |

The driver swapped the file between cells A and B, holding the old build
aside, and both copies were hash-checked. In each of cells B, C and D the
producer's own log shows its bridge pipe opened under that game's pid, so the
fixed sensor armed on this game build, and `out.txt` ended on
`==== clean shutdown ====`. Cell A's dump has the same stack as the kept dumps
above, so dumps were written and read in this session and the three "none"
readings are evidence. Cell D is the outlier: ForgePact's `LootGroundInit`
detour and the producer's own detour on that script were both live at
teardown, a pair #201 never ran together.

Not covered: a main-menu close with the fixed build (Tracker PR #8 measured
three clean ones on its test build `061fe033…`, built from the same fix
commit, with ForgePact 1.4.7 loaded; not the released `5f83e542…`), a vanilla or ForgePact-free game, other mods, and other close paths.
