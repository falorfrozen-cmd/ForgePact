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
| `bc-event` | perform a mouse or user event on a menu button instance, with that button as self | `menuprobe event` | `roomprobe` plus a screenshot | `menuprobe list` sees button instances at the menu |
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

**`menuprobe`, in this plugin** (research build only; the player build answers
`command unavailable in player build`). Three subcommands:

* `menuprobe list <Obj>` - read-only, no token. One line per live instance
  with its `nth`, `id`, position, visibility and sprite, plus the
  button-shaped variables it actually carries, capped at 64, then a count.
  This is also the **enumeration control**: menu-room instances have never
  been shown to be enumerable on this runner, so a `list` that finds nothing
  means (b) and (c) were not measured, not that they failed.
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
`HhResolveInstance`); no address is resolved, nothing is hooked, no loop
encloses a call, and nothing runs from the frame callback.

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
   `menuprobe list UI_Button_obj`, `menuprobe list Select_Parent_obj`, and
   `cb` reads of the window position and size, the interface size and the
   fullscreen flag. Record whether each object has instances here, how many
   buttons and where, the interface-to-window size ratio, and beside them the
   client rectangle, client size and DPI that a one-millisecond `hs_input`
   wait reports.
7. **Control for the key-read instrument** (human). Hold left Shift for three
   seconds while the agent takes both key reads; both must be true. Release
   and repeat; both must be false. If the first read is not true the
   instrument is blind: record `control: fail` and do not measure (a) or (d).
8. **(a) OS-level input, keyboard.** Press and hold Shift through `hs_input`,
   take both key reads, then release. Record both reads and the foreground
   window before and after. Repeat over the posted-message route. Record a
   line per route.
9. **Control for the screen oracle** (human). Click Local with the real
   mouse; take `roomprobe` line `[3]` and a `grab_window` screenshot; record
   the room and what the screen shows. Press Escape until the main menu is
   back and confirm the room returns to step 5's value. If the room does not
   change on a real click, the oracle is the screenshot alone - say so.
10. **(a) OS-level input, mouse.** Take Local's pixel from step 9's
    screenshot (a `grab_window` pixel is a client coordinate - measured
    2026-09-20, a 1920x1080 client captured as 1920x1080) and click it
    through `hs_input`; then `roomprobe` and a screenshot - the same change
    as step 9? Escape back. Repeat over the posted-message route. Record
    both.
11. **(a) keyboard navigation.** At the main menu, send arrows and Enter
    through `hs_input`: does a highlight move, does Enter activate? Record
    `keyboard-nav: observed` or `not observed`. Escape back.
12. **(d) the engine's own builtin.** Press Shift through `cb
    keyboard_key_press`, take both key reads, then release through `cb
    keyboard_key_release`. Record all three replies - an exception here means
    the builtin is not exposed on this runner, and that is the result. If the
    engine read went true and step 11 showed keyboard navigation, repeat step
    11 through the same builtin and record whether the menu reacts.
13. **(b) an event on a button - enumeration control first.**
    `menuprobe list UI_Button_obj` must list instances with distinct
    positions at the main menu. None means `control: fail` for (b) and (c);
    skip to 15. Otherwise, one command at a time, on the instance whose
    position matches Local: the mouse-pressed event, then mouse-enter, then
    mouse over, then user events 10 to 15 in turn. After each, `roomprobe`
    and a screenshot. Record what was performed, the result, the room before
    and after and whether the screen changed. Escape back between reactions.
14. **(c) a warm script call.** Positive control first:
    `menuprobe script GetQuestProgress UI_Button_obj 0 confirm` must return a
    real. Then the same form against `UiAMainMenuLocal` on the matching
    button, then the existing `icall` route for comparison, and, if the
    screen changed, `UiAChooseSaveSlot` and `UiACharacterPlay` on the later
    screens. Record the status value, the result or the exception, and the
    room and screen either side. Faults are expected - every cold call shape
    measured in 2026-09-11's work faulted and the process survived each one.
    If the game does die, launch again and continue from step 5.
15. **Full path, best mechanism.** Using whichever of steps 8 to 14 moved the
    screen, drive main menu, Local, save slot, Play, with one `grab_window`
    screenshot per screen. Record the layout: for each click, the client size
    and the click point as fractions of client width and height; for keyboard
    navigation, the exact key list. The slot is **slot 1**.
16. **Proof.** `orbpickup stat` - the reply's `player via ...` must not be
    `none`. Record the room and a screenshot showing town. Note any settle
    time needed.
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

Empty by construction: the session above has not been run. One row per step
id; `control` is `pass` or `fail` for every step that is a control, and `-`
for the rest.

| step | what to record | observed | control | date |
| --- | --- | --- | --- | --- |
| C-1.1 | six checks pass, healthy | | - | |
| C-1.2 | backup id | | - | |
| C-1.3 | installed plugin hash before the swap | | - | |
| C-1.4 | launch phase, load banner, co-op line | | - | |
| C-1.5 | research build confirmed, room, screenshot | | pass/fail | |
| C-1.6 | instances per object, button count and positions, size ratio, client rectangle and DPI | | - | |
| C-1.7 | both key reads while a human holds Shift, then released | | pass/fail | |
| C-1.8 | `a-sendinput` and `a-postmessage`: both key reads, foreground before and after | | - | |
| C-1.9 | room and screen after a real mouse click on Local, and after Escape | | pass/fail | |
| C-1.10 | `a-sendinput` and `a-postmessage`: room and screen after an injected click | | - | |
| C-1.11 | keyboard navigation observed or not observed | | - | |
| C-1.12 | the three replies from the engine's own key builtins | | - | |
| C-1.13 | `menuprobe list UI_Button_obj` at the menu, then each event performed and its result | | pass/fail | |
| C-1.14 | script control result, then each UI-action call, status and result | | pass/fail | |
| C-1.15 | the full path, per-screen client size and click fractions or key list | | - | |
| C-1.16 | `player via ...`, room, screenshot, settle time | | - | |
| C-1.17 | whether step 15's fractions hit in the second display mode | | - | |
| C-1.18 | exited, forced | | - | |
| C-1.19 | changed, added, missing | | - | |
| C-1.20 | changed and missing after the restore | | - | |
| C-1.21 | the hash after restoring the shipping plugin | | - | |
| C-1.22 | the decision written below | | - | |

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

One sentence per candidate goes here after the session, then the two lines
below. Until then every candidate is `not observed`, and the two lines are
placeholders rather than an answer:

* `a-sendinput` - not observed (session not run).
* `a-postmessage` - not observed (session not run).
* `d` - not observed (session not run).
* `bc-event` - not observed (session not run).
* `bc-script` - not observed (session not run).

finding: pending
shipRoute: pending

`pending` is not a value the shipping work can act on. The follow-on
workorder reads these two lines from this file and must refuse a `pending`
on either of them; only the owner-run session above may replace them, and a
candidate may be named in `finding:` only if its own control passed in the
same session.
