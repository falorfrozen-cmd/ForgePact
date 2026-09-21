# Character select: what can drive main menu, Local, save slot, Play

**Status: instruments built, live session not yet run.** Nothing player-visible
exists, and nothing in this document is a measurement yet. Every `## Results`
row is empty, and `## Decision`'s two lines read `pending` - they are filled by
the owner-run live session described under `## Live procedure`, and no reader
should treat a blank row as a negative. A candidate is only ever listed in
`finding:` if its own positive control passed in the same session
(`AGENTS.md`, "Prove the Instrument Before Trusting a Negative Result").

## The question

The hub's `hs-drive` MCP server can launch the modded game, talk to this
plugin over `bp_ipc`, screenshot the window and close the game again. What it
cannot do is get past the main menu: `hs_launch` reports `plugin_ready` with
the game sitting at its menu, and most of this plugin's gameplay commands act
only once a character is loaded. A human still has to click main menu, Local,
a save slot, Play.

So: can anything outside that human do it? Four candidate mechanisms, each
with its own positive control, measured in one session against one build.

## Static search

Everything below came out of static search before any live step, per
`AGENTS.md`, "Limit Rebuilds & Reruns During Development": exhaust the names
first, then hook every candidate the search turned up in the same build.

### The names the SDK already has

From `hs-game-sdk` (`cpp/include/hs_game_sdk/`), names and indices only - the
interoperability facts, not the game's own code:

* Rooms: `Main_Menu_rm` (194), `Char_Select_rm` (149), `Login_rm` (192),
  `Game_Start_rm` (182), `Town_01_rm` (235). The first is where a launch
  leaves the game; the last is the proof a character loaded.
* Scripts, the menu's own UI actions: `UiAMainMenuLocal` (4182),
  `UiAMainMenuOnline`, `UiAMainMenuOptions`, `UiAMainMenuExit`,
  `UiAChooseSaveSlot` (4029), `UiAChooseSaveSlotPage` (4058),
  `UiACharacterPlay` (4039), `UiACreateCharacter` (4025),
  `UiACharacterDelete`, `UiACharacterDeleteConfirm`, and the two the path
  ends in, `LoadSlot` (2294) and `GameStart` (1586).
* Objects: `Menu_Controller_obj` (2674), `Profile_Manager_obj` (3673),
  `Select_Parent_obj` (4355) with the per-class `Select_*_obj` family
  4341-4365 (`Select_Random_obj` is 4360),
  `Load_Inventory_Char_Select_obj` (2499), `Save_Character_obj` (4276),
  `Save_Slot_Shop_obj` (4279) and `UI_Button_obj` (5003), itself one of a
  `UI_Button_*_obj` family spanning 4977-5016.

**Whether the main menu's buttons are `UI_Button_obj` instances is not
observed.** It is the first thing the live session reads (step C-1.6), and it
is what decides whether candidates (b) and (c) can be measured at all.

### Five verbs that sound right and are not

This plugin already carries around 150 research verbs. Five of them read as
though they might already do this, and none does:

* `forceslot` forces what `GetSlotBloodPact` returns - a Blood Pact slot, not
  a save slot.
* `forcelogin` forces what `IsLoggedIn` returns; it does not log anything in
  or move a room.
* `puppetinput` is the co-op puppet's `IsMyPlayer` hook, not input at all.
* `roomprobe` only *reads* the current room. It is used here as an oracle,
  never as a way to change one.
* `coopstart` is peer-to-peer session setup.

Nothing in the remaining verbs loads a character or changes a room either.

### The research verbs reused unchanged

The live procedure leans on verbs that already exist in the research build:
`cb` (call a builtin by name), `roomprobe` (the room index, with its own
positive control on line `[6]`), `citrace dumpobj` (a full variable dump of
any object's instance), `icall` (a script call inside the toolkit's
`InvokeWithObject`), and `orbpickup stat`, whose line ends with
`player via ...` - the one proof that a character is loaded that works on any
build, because `orbpickup` is in the player allowlist.

## Candidates and controls

Session order is (a), then (d), then (b), then (c), with no relaunch between
them.

| id | mechanism | change needed | instrument | positive control |
| --- | --- | --- | --- | --- |
| `a-sendinput` | OS-level input from the MCP process, game in the foreground | none in this plugin | `cb keyboard_check <vk>` and `cb keyboard_check_direct <vk>` in one command; `roomprobe` plus a screenshot for the screen | a human holds Shift and both reads must be true (C-1.7); a human clicks Local and the room or screen must change (C-1.9) |
| `a-postmessage` | posted key and mouse messages to the window, no foreground | none | the same | the same |
| `d` | the engine's own `keyboard_key_press` and `keyboard_key_release`, through `cb` | none (research build) | the same | the same; an exception from `cb` means the builtin is not exposed on this runner |
| `bc-event` | perform a mouse or user event on a menu button instance, with that button as self | `menuprobe event` | `roomprobe` plus a screenshot | `menuprobe list` sees button instances at the menu, read beside a `menuprobe list` of an object `citrace dumpobj` shows live there (C-1.6, C-1.13) - both empty measures the instrument, not the button |
| `bc-script` | call a menu UI-action script with a real button as self and other | `menuprobe script` | the same | `menuprobe script GetQuestProgress <Obj> 0 confirm` returns a real |

`keyboard_check_direct` reads the operating system's key state, which
hardware and injected OS-level input both reach and a posted window message
does not; `keyboard_check` reads the engine's own. Taking both in one command
separates "the OS saw it" from "the game saw it", which is the whole reason
(a) is split into two rows.

## Instrument

Two halves, one on each side of the process boundary.

**`hs_input`, in the hub** (`tools/hs_drive_mcp/input.py`, documented in
`docs/tools/hs-drive-mcp.md`). Injects keys, clicks, moves and waits into the
game's window by either route. It refuses unless the target window belongs to
a running game process, and for the OS-level route it compares the foreground
window against the game's immediately before every injection rather than once
at the start - `AGENTS.md`, "Check a Permission Where It Is Used". Click
coordinates default to the client area, which is exactly what a
`grab_window` screenshot pixel is.

Each route reads its own delivery signal, and each has exactly one: the
OS-level route reports how many records the system accepted, the posted-message
route only whether the message was queued at all. The second matters here more
than it looks, because nothing acknowledges a posted message afterwards and the
human controls in this procedure - a hand on Shift, a hand on the mouse - never
exercise that route's delivery path. A message the system refuses (an integrity
mismatch against an elevated game, a window destroyed mid-sequence, a full
queue) comes back as `complete: false` with the message name and the error code
in `detail`, so a key read of false on that route separates "the message was
never queued" from "the game did not react to it".

**`menuprobe`, in this plugin** (research build only; the player build answers
`command unavailable in player build`). Three subcommands:

* `menuprobe list <Obj>` - read-only, no token. One line per live instance
  with its `nth`, `id`, position, visibility and sprite, plus the
  button-shaped variables it actually carries, capped at 64, then a count.
  This is also the **enumeration control**: menu-room instances have never
  been shown to be enumerable on this runner, so a `list` that finds nothing
  means (b) and (c) were not measured, not that they failed.

  The control has to be the same instrument, not the same concept. An empty
  `menuprobe list UI_Button_obj` is equally consistent with "the menu's
  buttons are some other object" and with "nothing in a menu room survives
  the **read** path `list` uses", and `citrace dumpobj` resolving an object
  does not close that gap: `dumpobj` and `list` resolve an instance
  identically - both run `asset_get_index` -> `instance_number` ->
  `instance_find` -> `HhResolveInstance`, the same four calls in the same
  order - but they read it differently once resolved. `dumpobj` walks the raw
  `CInstance*`; `list` and `var` go through
  `variable_instance_exists`/`variable_instance_get` instead, and a menu-room
  instance has never been shown to survive that read path. A negative is only
  worth anything against the instrument that produced it. So the control is
  `menuprobe list Menu_Controller_obj` - a `list` of an object the *same*
  step's `dumpobj` shows live, with `Profile_Manager_obj` as the fallback if
  that one turns out to have no instances at the menu. Read the pair:
  a non-empty control with an empty `UI_Button_obj` is a real negative about
  the button object; **both empty measures the instrument**, so (b) and (c)
  are `control: fail`, unmeasured. Which object is live at the menu is itself
  unknown until step 6 runs, which is why a fallback is named rather than one
  object asserted.
* `menuprobe event <Obj> <nth> <type> <number> [Obj2] confirm` - exactly one
  event performed on that instance.
* `menuprobe script <Script> <Obj> <nth> [args...] confirm` - exactly one
  script call with that instance as both self and other.

Both mutating subcommands print the object, the `nth`, the instance's `id`
and position, and the room index before and after, so a refusal, a no-op and
a fault are three distinguishable outcomes. `confirm` is a literal word and
always last, so a half-written command file fails closed instead of firing -
the same gate the `citrace` verbs use. Every instance is reached by name
(`asset_get_index`, `instance_number`, `instance_find`, then
`HhResolveInstance`); no address is resolved, nothing is hooked and no loop
encloses a call.

Where these commands run is worth saying precisely, because an earlier draft
of this paragraph claimed the opposite. Every ForgePact command executes
inside `PollCommands()`, which `FrameCallback` calls every 30 frames once the
plugin's setup flag is set - so `menuprobe` already runs **on** the frame
thread, and that is exactly what makes calling `CallBuiltinEx` and
`CallGameScriptEx` from it safe: the runtime is between frames and owned by
this thread. What is true and load-bearing is the narrower claim: nothing
`menuprobe` adds is installed on the per-frame path, and nothing of it runs
unasked - a contract test pins that `FrameCallback`'s own body mentions
neither the verb nor any of its helpers.

## Live procedure

Run from a Claude Code session with `hs-drive` connected, the game and the
panel closed, and the game's display mode **windowed or borderless**:
exclusive fullscreen is minimized by Windows when it loses focus, and a
minimized window refuses every capture and every injection. Record each
reply in the `## Results` table under its step id. Keys used below:
`16` Shift, `13` Enter, `27` Escape, `37` to `40` the arrows.

1. `hs_selfcheck` - six checks pass, healthy true.
2. `hs_saves_backup("pre-charselect")` - record the backup id.
3. Record the SHA-256 of the installed `BloodPactPlugin.dll`; copy that file
   to a `.ship-<first 8 of the hash>` sibling (a copy, not a move); copy
   `plugin_build/BloodPactPlugin_rel.dll` over the installed one.
4. `hs_launch()` - expect phase `plugin_ready`. A `plugin_consumed_without_pong`
   or a timeout means the research build did not come up: stop, close the
   game, restore the shipping plugin per step 21 and record it. Then
   `hs_ipc_tail(60)` and confirm the load banner and the line reporting no
   `coop.ini`; any other co-op line stops the session and is recorded.
5. **Build check.** `roomprobe` - the reply must contain the `fps` positive
   control on line `[6]` and must **not** say the command is unavailable in a
   player build. Record line `[3]`, the room (expect `Main_Menu_rm`, record
   whatever it is) and a `grab_window` screenshot.
6. **Read-only reads at the menu**, all in one command so they are one frame:
   `citrace dumpobj Menu_Controller_obj 0`, `citrace dumpobj Profile_Manager_obj 0`,
   `menuprobe list Menu_Controller_obj`, `menuprobe list Profile_Manager_obj`,
   `menuprobe list UI_Button_obj`, `menuprobe list Select_Parent_obj`, and
   `cb` reads of the window position and size, the interface size and the
   fullscreen flag. Record whether each object has instances here, how many
   buttons and where, the interface-to-window size ratio, and beside them the
   client rectangle, client size and DPI that a one-millisecond `hs_input`
   wait reports.

   The two `menuprobe list` lines that mirror a `citrace dumpobj` are **step
   13's enumeration control**, which is why they are read here, in the same
   frame, before anything is performed. Record each one's count even when it
   is zero, and note which object `dumpobj` showed live: that object's `list`
   is what decides whether an empty `UI_Button_obj` is a fact about the button
   object or a fact about the instrument.
7. **Control for the key-read instrument** (human). Hold left Shift for three
   seconds while the agent takes both key reads; both must be true. Release
   and repeat; both must be false. If the first read is not true the
   instrument is blind: record `control: fail` and do not measure (a) or (d).
8. **(a) OS-level input, keyboard.** Press and hold Shift through `hs_input`,
   take both key reads, then release. Record both reads and the foreground
   window before and after. Repeat over the posted-message route, recording
   that route's `complete` flag and any `PostMessageW ... failed` detail with
   it: a refused post is `control: fail` for the route - the message never
   reached the window - and says nothing about whether the game reads posted
   keys. Record a line per route.
9. **Control for the screen oracle** (human). Click Local with the real
   mouse; take `roomprobe` line `[3]` and a `grab_window` screenshot; record
   the room and what the screen shows. Press Escape until the main menu is
   back and confirm the room returns to step 5's value. If the room does not
   change on a real click, the oracle is the screenshot alone - say so.
10. **(a) OS-level input, mouse.** Take Local's pixel from step 9's
    screenshot (a `grab_window` pixel is a client coordinate - measured
    2026-09-20, a 1920x1080 client captured as 1920x1080) and click it
    through `hs_input`; then `roomprobe` and a screenshot - the same change
    as step 9? Escape back. Repeat over the posted-message route, again
    recording its `complete` flag and any delivery error beside the result.
    Record both.
11. **(a) keyboard navigation.** At the main menu, send arrows and Enter
    through `hs_input`: does a highlight move, does Enter activate? Record
    `keyboard-nav: observed` or `not observed`. Escape back.
12. **(d) the engine's own builtin.** Press Shift through `cb
    keyboard_key_press`, take both key reads, then release through `cb
    keyboard_key_release`. Record all three replies - an exception here means
    the builtin is not exposed on this runner, and that is the result. If the
    engine read went true and step 11 showed keyboard navigation, repeat step
    11 through the same builtin and record whether the menu reacts.
13. **(b) an event on a button - enumeration control first.** Re-run the
    control alongside this step's own reading, and record the pair:
    `menuprobe list Menu_Controller_obj` - or `menuprobe list
    Profile_Manager_obj` if step 6 found the first one empty and the second
    one live - **and** `menuprobe list UI_Button_obj`. Three readings, three
    different conclusions: control non-empty and `UI_Button_obj` listing
    instances with distinct positions means the measurement is on; control
    non-empty and `UI_Button_obj` empty is a real negative about
    `UI_Button_obj` only (`control: pass`, (b) and (c) measured as
    not-`UI_Button_obj`) - it is **not** a negative about the mechanism, and
    not yet about any other object, so the next action is named rather than
    left to improvise: re-run this step and step 14 against
    `Select_Parent_obj` and whichever other objects step 6 found live at the
    menu, before drawing any conclusion about (b) or (c) as a whole; **both
    empty measures the instrument**, so (b) and (c) are `control: fail`,
    unmeasured - skip to 15. Otherwise, one command at a time, on the instance
    whose position matches Local: the mouse-pressed event, then mouse-enter,
    then mouse over, then user events 10 to 15 in turn. After each,
    `roomprobe` and a screenshot. Record what was performed, the result, the
    room before and after and whether the screen changed. Escape back between
    reactions.
14. **(c) a warm script call.** Positive control first:
    `menuprobe script GetQuestProgress` on **the object that listed
    instances in step 13** (not a hardcoded `UI_Button_obj 0` - if step 13
    fell to `Select_Parent_obj` or another object, the control runs against
    that object) must return a real. On 2026-09-21, run as
    `GetQuestProgress UI_Button_obj 0 confirm`, that control raised
    (`EXCEPTION calling gml_Script_GetQuestProgress`) instead of returning
    (C-1.14): before this step's control can be trusted, a re-run needs a
    control script already proven to return a known value at the main menu -
    `GetQuestProgress` is not that script, on this object, at this screen.
    Then the same form against `UiAMainMenuLocal` on the matching button,
    then the existing `icall` route for comparison, and, if the screen
    changed, `UiAChooseSaveSlot` and `UiACharacterPlay` on the later screens.
    Record the status value, the result or the exception, and the room and
    screen either side. Faults are expected - every cold call shape measured
    in 2026-09-11's work faulted and the process survived each one. If the
    game does die, launch again and continue from step 5.
15. **Full path, best mechanism.** Using whichever of steps 8 to 14 moved the
    screen, drive main menu, Local, save slot, Play, with one `grab_window`
    screenshot per screen. Record the layout: for each click, the client size
    and the click point as fractions of client width and height; for keyboard
    navigation, the exact key list. The slot is **slot 1**.
16. **Proof.** Arm first: `orbpickup 1`, then `orbpickup stat` at the main
    menu, before walking step 15's path - the reply's `player via ...` must
    read `none` (the negative control: `orbpickup 1` arms the resolver, but
    no player exists yet to resolve). Only then walk step 15's path to town
    and `orbpickup stat` again - the reply's `player via ...` must not be
    `none`. Record the room and a screenshot showing town, and any settle
    time needed. Restore with `orbpickup 0` once recorded.
17. **Layout in a second display mode.** Switch windowed and borderless in
    the game's options (human), return to the main menu (human) and repeat
    step 15 in that mode; record whether step 15's fractions still hit.
18. `hs_stop_game()` - exited true, forced false.
19. `hs_saves_inspect(<id from step 2>)` - record changed, added and missing.
20. `hs_saves_restore(<id>, confirm_backup_id=<id>)`, then `hs_saves_inspect`
    again - changed and missing must both be empty.
21. Restore the shipping plugin: copy the `.ship-<hash>` sibling back over
    the installed `BloodPactPlugin.dll` and re-run step 3's hash - it must
    match. Leave the sibling copy in place.
22. Fill `## Decision`: one sentence per candidate, then the two literal
    lines.

## Results

Run 2026-09-21 against the real modded install: research build `1fadd1ec...`, game
build 7.0.13.0, plugin v1.4.4, YYTK 4.0.1. One row per step id; `control` is
`pass` or `fail` for every step that is a control, and `-` for the rest.

**Transport caveat.** The `hs-drive` MCP server process connected to the
session predates `hs_input`'s registration and exposed only twelve tools, so
candidate (a) was driven by calling `tools.hs_drive_mcp.input.inject`
directly -- the exact function the MCP tool body is a one-line pass-through
to, and importable without the MCP SDK because of the release boundary the
core workorder pins. The gate, the window resolution and the refusals are
therefore the shipped ones. What differs is the **process**: these calls ran
from a Bash-spawned interpreter rather than the server process, and process
integrity level is one of the things that makes `PostMessageW` fail, so
C-1.8's posted-route result is scoped to a same-integrity caller.

| step | observed | control | date |
| --- | --- | --- | --- |
| C-1.1 | six checks pass, `healthy: true`; positive controls `process_snapshot`, `backup_roundtrip` and `screenshot_screen` all listed in `positive_controls_proven` | - | 2026-09-21 |
| C-1.2 | `backup_id: 20260921T063610Z_pre-charselect`, 137 files, 1,281,604 bytes | - | 2026-09-21 |
| C-1.3 | **the expectation was wrong.** The installed plugin was `282cbd1c...` (3,463,168 B, carrying `citrace`/`roomprobe`/`prospectprobe`, no player-build marker), not the expected `24020eac...`. The player build was beside it as `BloodPactPlugin.dll.preprobe-backup` (`24020eac...`, 970,240 B), left installed by an earlier session. Both preserved as `.found-282cbd1c` and `.ship-24020eac`; research `_rel` `1fadd1ec...` installed over the top | - | 2026-09-21 |
| C-1.4 | `phase: plugin_ready`, `ready: true`, banner `==== BloodPact plugin loaded ==== v1.4.4`, reply `pong (YYTK 4.0.1)`; the line `coop: no coop.ini at ...bin/bp_ipc` present and no other `coop:` line | - | 2026-09-21 |
| C-1.5 | `[6] GetBuiltin("fps", nullptr) -> real:144.000000` (positive control), `[5] variable_global_exists("room") -> bool:false` (negative control), and no `command unavailable in player build`. The room reads as `kind=15 str=ref room Main_Menu_rm` -- a room **reference**, not the `real:194` the plan predicted | pass | 2026-09-21 |
| C-1.6 | `UI_Button_obj` **13** live instances; control `Menu_Controller_obj` **1**; `Select_Parent_obj` **0**; `Profile_Manager_obj` 1 via `citrace dumpobj`. GUI 2560x1368 against window 1920x1080. `hs_input` geometry: hwnd 1247526, `client_rect [320,191,2240,1271]`, `client_size [1920,1080]`, `dpi 96` | - | 2026-09-21 |
| C-1.7 | held: **13/13** polls across 27 s read `keyboard_check 16 = real:1` and `keyboard_check_direct 16 = real:1`, game in foreground throughout. Released: **4/4** reads `real:0` on both | pass | 2026-09-21 |
| C-1.8 | `a-sendinput`: `keyboard_check = real:1`, `keyboard_check_direct = real:1`. `a-postmessage`: `keyboard_check = real:1`, `keyboard_check_direct = real:0`. Both routes `complete: true`, `records_rejected: 0`, `foreground_before == foreground_after ==` the game hwnd. Both returned to `0`/`0` after key-up | - | 2026-09-21 |
| C-1.9 | **not run.** The injected click at C-1.10 produced the state change, so the human-click oracle was not needed to interpret a negative | - | 2026-09-21 |
| C-1.10 | zero-hold click (`hs_input`'s own `click` action): `complete: true`, 3 records sent, **room unchanged** -- but the screenshot shows the cursor moved onto the button and the button lit, so the *move* arrived and the *press* did not. Repeating with **120 ms between button-down and button-up**: `Main_Menu_rm` -> `Chose_rm`. The zero-hold failure measured the instrument, not the game | - | 2026-09-21 |
| C-1.11 | arrows (vk 38/40) and Enter: no `image_index` change on any of the 13 buttons, no room change, and no visible highlight in the screenshot. Keys are proven to arrive (C-1.7, C-1.8), so this is a measured negative: **not observed**. Consistent with `menuNav = real:-1` and `gamepadCursorManager = real:-4` in the `Menu_Controller_obj` dump | - | 2026-09-21 |
| C-1.12 | `cb keyboard_key_press 16 -> undefined`, then `keyboard_check = real:1` **and** `keyboard_check_direct = real:1`; `cb keyboard_key_release 16 -> undefined`, then `real:0` on both. Baseline before the press was `0`/`0` | - | 2026-09-21 |
| C-1.13 | control `menuprobe list Menu_Controller_obj` -> 1 live instance (non-empty) beside `menuprobe list UI_Button_obj` -> 13, so the enumeration control **passed** and the ambiguous both-empty branch did not arise. Events on `nth=0` (`Play local`, instance 257029): `ev_mouse` 4, 0, 10 and 5, then user events 10, 11 and 12 -- each `performed -> bool:true`, the instance alive in the `after:` line every time, room and screenshot unchanged after all seven | pass | 2026-09-21 |
| C-1.14 | control `menuprobe script GetQuestProgress UI_Button_obj 0 confirm` -> **`EXCEPTION calling gml_Script_GetQuestProgress`**, where `real:-1` was expected. The control failed, so no UI-action call was attempted and the route is **unmeasured** | fail | 2026-09-21 |
| C-1.15 | windowed, GUI 2560x1368 / client 1920x1080: `Play local` at gui(448, 676.4) -> client(336, 534), fractions (0.175, 0.4944); character slot 1 (`Pal`) at client(243, 225); `PLAY` at client(583, 346). The full path main menu -> `Chose_rm` -> character panel -> `Town_01_rm` driven entirely by held `send_input` clicks | - | 2026-09-21 |
| C-1.16 | room `Town_01_rm`; `menuprobe list Player_obj` -> **1 live instance** at (912, 822), with `Player_Parent_obj` -> `not found (asset_get_index)` as the negative control. **`orbpickup stat` did not prove it**: `player via (not tried)` -- but this session never sent `orbpickup 1`, and `g_OrbPlayerHow` is written only inside `FrameCallback`'s `orbpickup`-on branch, so this row measured the **off** state, not a general failure of the field. The on-state reading is `not observed` until the ship workorder's live gate arms `orbpickup` and reads it. Settle time under 3 s | - | 2026-09-21 |
| C-1.17 | fullscreen (`cb window_get_fullscreen -> real:1`), GUI 2560x1440 / client 2560x1440: the **same** button reports gui(448, **712**) -- its absolute GUI position moved -- yet the fractions are (0.175, 0.4944), identical to windowed. The same read-live-and-scale formula produced client(448, 712) and the click drove `Main_Menu_rm` -> `Chose_rm` again | pass | 2026-09-21 |
| C-1.18 | `exited: true, forced: false` on both stops (pids 80640 and 66588), `WM_CLOSE` to 2 windows each time | - | 2026-09-21 |
| C-1.19 | `changed: ["shop.ini"]`, `added: []`, `missing: []` -- no character save altered by loading a character and exiting from town | - | 2026-09-21 |
| C-1.20 | after the restore: `changed: []`, `added: []`, `missing: []`, 137 files; pre-restore backup `20260921T064937Z_pre-restore` | - | 2026-09-21 |
| C-1.21 | `24020eac387d90a0c1b385a4f855e504f3cee42352bae8cdf1bcdb0e15e7372a`, match `True`, player-build marker present, `menuprobe` and `citrace` absent. **A deliberate change of state, recorded as one:** the install arrived carrying the research build `282cbd1c...`, and the owner chose on 2026-09-21 to end with the shipping plugin instead; `282cbd1c...` is kept as `BloodPactPlugin.dll.found-282cbd1c` | - | 2026-09-21 |
| C-1.22 | see the Decision below | - | 2026-09-21 |

## Negative results, sourced

Each of these is a *measured* negative with its own control, and each one is
why a candidate is shaped the way it is. None of them says a mechanism cannot
work - only that, so far, it has not been seen to.

* **Game scripts called cold fault.**
  `pet-quest-collector-c-research.md`, "Why the game's scripts cannot be
  called cold (MEASURED 2026-09-11)": every direct call shape faulted on two
  scripts, with a control call through the same plumbing that worked, and the
  process survived every fault. Arguments alone changed nothing. So (c) is
  expected to fault **unless** a UI-action script tolerates a button as self
  with no arguments - which is `not observed`, not disproven, because no UI
  script was ever tried with a real button as self.
* **The toolkit's instance-scoped invoke has an enumeration gap.** Same
  document, round 4: with a provably correct object index it found zero
  instances of an object `instance_find` had just resolved. That is why
  `menuprobe` resolves through `instance_find` and `HhResolveInstance`, and
  offers the older route only as a second opinion.
* **The script-visible input state is a downstream mirror.**
  `pet-quest-collector-b4-research.md`, Conclusion: a real key press flips
  the mirror (the control), but writing the mirror changed nothing, because
  the pressed-this-frame read is native. So the instrument here reads engine
  and OS key state, never the mirror.
* **Injected OS-level input has never been run against this game.**
  `pet-quest-collector-plan.md`, "Plan B3 - not pursued", rejected it for a
  *player-facing mod*, on the grounds that it fights the player for the
  cursor. Those grounds do not apply to a tool that owns a test session.
  Status: `not observed`.
* **An event that reports success proves only that the builtin ran.** Same
  document, C0.3. A `menuprobe event` that changes nothing is `not observed`,
  never "there is no such event".

## Decision

Measured 2026-09-21 in one session against the real modded install. Every
sentence below is backed by a row of the Results table, and no candidate is
named in `finding:` whose control did not pass in that same session.

* `a-sendinput` - **works.** Keys reach both `keyboard_check` and
  `keyboard_check_direct` (C-1.8), and a held mouse click drove the whole path
  main menu -> `Chose_rm` -> character panel -> `Town_01_rm` (C-1.10, C-1.15,
  C-1.16). **The hold is the mechanism.** A button-down and button-up emitted
  back to back land inside one frame and are invisible to a 144 fps sample
  loop; `hs_input`'s own `click` action emits them with no delay, so it moved
  the cursor, lit the button under it, and activated nothing. The same click
  with 120 ms between the two records changed the room every time.
* `a-postmessage` - **works for `keyboard_check` only.** The posted route sets
  the window-level key state but not the device state that
  `keyboard_check_direct` reads (C-1.8), so anything the game gates on the
  direct read is beyond it. Scoped to a same-integrity caller; see the
  transport caveat above the Results table.
* `d` - **works.** `keyboard_key_press` and `keyboard_key_release` set and
  clear *both* key reads (C-1.12), and need no foreground. Keys only, though,
  and keyboard navigation is not observed at this menu (C-1.11), so it cannot
  drive the path by itself.
* `bc-event` - **not observed, for `UI_Button_obj` and for the seven events
  tried** (`ev_mouse` 0, 4, 5 and 10; user events 10, 11 and 12). Each
  returned `performed -> bool:true` with the instance still alive in the
  `after:` line and nothing changing on screen (C-1.13). The enumeration
  control passed in the same session, so this is a real negative about *that
  object and those events*. It is not a statement about the mechanism, and not
  about any other object.
* `bc-script` - **unmeasured.** This route's positive control,
  `menuprobe script GetQuestProgress UI_Button_obj 0 confirm`, returned an
  exception where `real:-1` was expected (C-1.14). Nothing the route reported
  afterwards could be distinguished from a blind instrument, so no UI-action
  call was attempted. Whether the control is simply unsuitable at the main
  menu or the route is broken is itself undetermined, and saying which would
  need a script already proven to return a known value there.

finding: a-sendinput, a-postmessage, d
shipRoute: mcp-only

### What the shipping workorder still has to solve

`shipRoute: mcp-only` was the owner's choice on 2026-09-21, made knowing that
two things this session leaned on came from **research-only** verbs and are
therefore unavailable to `hs_select_character` on a player build.

1. **Where the buttons are.** The coordinates here came from `menuprobe list`.
   Without it, the fallback is the client fractions, which C-1.17 showed are
   stable across display modes even though the absolute GUI coordinates are
   not: `Play local` sits at **(0.175, 0.4944)** of the client rectangle in
   both windowed and fullscreen. Two further fractions were measured **in
   windowed mode only** and have not been checked in a second mode: character
   slot 1 at **(0.1266, 0.2083)** and `PLAY` at **(0.3036, 0.3204)**. They are
   layout constants, so a game patch can move them; whatever ships should fail
   loudly with `layout_not_measured` rather than clicking a guessed point.
2. **Proving a character actually loaded.** `orbpickup stat` alone does
   **not** do it: with no orbs nearby and `orbpickup` off, it reports `globe
   objs=0` and `player via (not tried)` (C-1.16) -- but this session never
   armed the mod, and `g_OrbPlayerHow` is written only inside
   `FrameCallback`'s `orbpickup`-on branch, so that row measured the **off**
   state, not a general failure of the field. The ship workorder's decision
   D17 assumed the field answers regardless of `orbpickup`'s state, and this
   session's off-state measurement falsified that assumption; the on-state
   reading is `not observed` until a session arms `orbpickup` and reads it.
   What did work was `menuprobe list Player_obj` -> 1 live instance, which is
   research-only, so a player-build-answerable proof still has to be found.
3. **`hs_input`'s `click` needs a hold.** Its `key` action already takes
   `hold_ms` and defaults it to 60; the pointer path has no equivalent and
   emits down and up with nothing between them. Until that is fixed, the
   shipped tool cannot press a button even though the mechanism works. This is
   a defect in an instrument this repository already shipped, found by
   measurement rather than by review.
