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
  since 1.3.10 (`modfiles_shipped/HSOfflineTrackerProducer.dll`, sha256
  `36608aa0…b14e`, a 2026-09-05 build) keeps its publisher in a global
  `std::thread`. When the game leaves through `ExitProcess`, that thread is
  still joinable when the DLL's globals are destroyed, so its destructor calls
  `std::terminate`: fast fail 7, which reads as `0xC0000409`. Nine of the ten
  dumps Windows kept on 2026-09-25/26 showed it. HS-Offline-Tracker PR #8 saw
  it on a plain main-menu close; its fixed producer exited `0` three times out
  of three. HS-Offline-Tracker v0.1.4 (published 2026-10-06) ships that fix;
  ForgePact still pins the old build.
- **ForgePact's own coop thread did the same until PR #90** (guide item 26).
  On 2026-09-26, with the producer moved out of `mods\aurie`, research build
  `9915ca5` exited `0` on a main-menu `CloseMainWindow`. Nothing has measured
  an in-world close without the producer, and hs-drive has never reported
  `0x00000000` for the game.
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
(`dd87086c…`, 2026-10-13 09:43), `YYToolkit.dll` (`51a393d7…`) and
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
all (on 2026-09-22 it was opened by hand).

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
ForgePact worker thread still running at teardown (the shape guide item 26
aborted on). Each is expected to read `0x00000000` and leave no dump; the
producer-present dumps of Live procedure 1 are the dump reader's positive
control for that "no dump". A non-zero code in any cell is recorded with its
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
