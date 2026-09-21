# Menu layout: where the character-select buttons are

**Question.** Where, in window (client) coordinates, are the buttons a tool
has to press to get from the main menu to a loaded character - `Play local`,
save slot N on page 1 of the save-slot screen, and `PLAY` on the character
panel - as the running game reports them, rather than as a screenshot once
measured them?

**Why it matters.** The hub's `hs-drive` MCP server (`docs/tools/hs-drive-mcp.md`
in the toolkit) drives the game to a loaded character with
`hs_select_character`. Its first version clicked three client fractions
measured by hand on 2026-09-21. A fraction is a guess the moment the game
patches its menu or the window changes shape, and a guess that lands on the
wrong spot does something other than refuse. This document records the
instrument that replaces those fractions, the procedure that measures which
game objects the slot cards and `PLAY` are, and the decision the hub is
written against.

**Posture.** Everything here is measured runtime behaviour and our own code.
Game objects are named by their `hs-game-sdk` names and indices; no game
script text appears, and the procedure is written as prose.

**Status.** The instrument (`menulayout`) is built and ships in the player
build. Phase 0, the live enumeration of the character-select screen, is
**pending**; the four lines of § Decision stay `pending` until it has run.

## Static search

Source: `hs-game-sdk`'s object table and its parent hierarchy (the Python
binding's parent lookup and the C++ `objects.hpp`), searched on 2026-09-21
over every object whose name matches a UI, select, save or menu concept.

Every clickable UI node descends from `UI_Node_Parent_obj` (5186): that covers
`UI_Button_obj` (5003) and its 30 children, among them `UI_Button_Small_obj`
(5009), plus `UI_Radio_Button_obj`, `UI_Button_Character_Customize_obj` (4979),
`UI_Button_Unique_obj` and the `Select_*_obj` class tiles under
`Select_Parent_obj` (4355). Panels descend from `UI_Parent_obj` (5205):
`UI_Character_obj` (5024), `UI_Character_Delete_obj`, `UI_Create_Character_obj`
(5056), `UI_Main_Menu_obj` (5143) and `UI_Main_Menu_Featured_obj`. A third
root, `UI_List_Item_Parent_obj` (5130), has no parent and holds list items.
Five objects on the path have no parent at all: `Save_Character_obj` (4276),
`Save_Slot_Shop_obj` (4279), `Load_Inventory_Char_Select_obj` (2499),
`Menu_Controller_obj` (2674) and `Profile_Manager_obj` (3673).

The command's candidate table is therefore thirteen names:

| Group | Objects |
| --- | --- |
| Roots | `UI_Node_Parent_obj`, `UI_Parent_obj`, `UI_List_Item_Parent_obj` |
| Leaves | `UI_Button_obj`, `UI_Button_Small_obj`, `UI_Character_obj`, `UI_Create_Character_obj`, `UI_Main_Menu_obj` |
| Unparented | `Save_Character_obj`, `Save_Slot_Shop_obj`, `Load_Inventory_Char_Select_obj`, `Menu_Controller_obj`, `Profile_Manager_obj` |

Listing a parent is expected (GameMaker semantics) to include its children's
instances. The leaves are listed as well and the output is deduplicated by
instance id, so the answer does not depend on that expectation. Phase 0
records whether the root listing alone would have caught the thirteen
main-menu buttons.

What the 2026-09-21 research session already established (PR #60's
`character-select-research.md`, results C-1.6, C-1.13, C-1.15 and C-1.17, and
the replies it left in the game's own `out.txt`):

- At `Main_Menu_rm`, `UI_Button_obj` has 13 live instances, each carrying a
  `text` string; `Play local` is one of them. **`text` is the identifier** for
  a `UI_Button_obj`: two `Cosmetic Shop` buttons exist, so sprite and position
  are not.
- At `Chose_rm`, the screen after `Play local`, `UI_Button_obj` has only 5
  instances (`Back`, `Page 1`, `STEAM Cloud`, a hero-list icon with empty
  text and a hidden question-mark button). **The save-slot cards and `PLAY`
  are not `UI_Button_obj` instances**, and were never enumerated. The
  character panel with `PLAY` is still room `Chose_rm`.
- Performing a mouse or user event on a `UI_Button_obj` was not observed to
  activate anything (C-1.13); a held left click at the button's position did.

## Instrument

`menulayout` is a player-build ForgePact command. It is read-only: it runs
inside the command poll like every command, installs nothing on the
per-frame path, and never clicks, performs an event, calls a game script,
creates, destroys or writes anything. `tests/test_menu_layout_contract.py`
pins that, and pins the output format below byte for byte, because the hub
parses it.

For each candidate name it resolves the object by name (confirmed by the
name round-tripping through `object_get_name`; a name that does not resolve
is listed in the footer's `absent=` field rather than skipped), counts its
instances with `instance_number` and reaches each with `instance_find`,
skipping an instance id it has already printed. It reads through the handle
`instance_find` returned, whatever kind that is. Per instance: the object
name, `id`, `x`, `y`, `visible`, the sprite name (or `none`), the four
`bbox_*` edges, then - only when the instance carries them - `label`, `name`,
`slot`, `index`, `page` and `selected`, and always last, `text`. A read that
fails prints `<read-failed>` in that field instead of stopping the listing.
Rows are capped at 200.

```
menulayout: room=<RoomName> gui=<W>x<H> window=<W>x<H> fullscreen=<0|1> view=<x>,<y>,<w>,<h>
  obj=<ObjectName> id=<id> gui=<x>,<y> win=<cx>,<cy> bbox=<l>,<t>,<r>,<b> visible=<0|1> sprite=<SpriteName|none>[ label=<v>][ name=<v>][ slot=<v>][ index=<v>][ page=<v>][ selected=<v>] text=<text to end of line>
menulayout: listed=<n> absent=<comma-separated names or none> capped=<0|1>
```

`menulayout <ObjectName>` lists that one object the same way. A player build
older than this command answers `command unavailable in player build:
menulayout`, which the hub reads as `layout_command_missing`.

**The mapping.** The window point is the instance's GUI position scaled by
the window size over the GUI size, all four sizes read from the game by name:
the x coordinate times `window_get_width` over `display_get_gui_width`, and
the y coordinate times `window_get_height` over `display_get_gui_height`,
each rounded to a whole pixel. `window_get_width` and `window_get_height` are
the client size: C-1.6 measured the window read equal to `hs_input`'s
`client_size` of 1920x1080. Measured on `UI_Button_obj` only: windowed, GUI
2560x1368 over a 1920x1080 client, `Play local` at GUI (448, 676.4) maps to
(336, 534); fullscreen, GUI 2560x1440 over a 2560x1440 client, at GUI
(448, 712) it maps to (448, 712). Both clicks changed the room. Whether the
slot cards and `PLAY` live in GUI space too is not known until phase 0, so
the header also prints view 0's camera rectangle: a room-space object is
recognisable because its scaled point will not land near the point a
screenshot measured.

**Positive control, every session.** At the main menu on a 1920x1080
windowed client, the row for `Play local` must read `win=336,534` under a
header reading `window=1920x1080`. If it does not, the instrument is blind
and nothing it lists afterwards is evidence.

Expected GUI positions for the two unknown objects, derived from C-1.15's
screenshot points and used **for matching only, never for clicking**: slot 1
near GUI (324, 285) and `PLAY` near GUI (777, 438), that is the client point
scaled by 2560 over 1920 and 1368 over 1080. The candidate whose position or
bounding box encloses the expected point is the one.

## Live procedure

Owner-run, one launch, the research (dev) build of this branch. Before
starting: the game and the panel are closed (`hs_status` reports not
running) and the display mode is windowed 1920x1080. No step clicks `PLAY`:
no character load is needed, and that keeps the save untouched.

1. Take an out-of-band copy of the saves with robocopy, from
   `%LOCALAPPDATA%\Hero_Siege\hs2saves` to
   `C:\Users\stann\HeroSiege-manual-save-backup\hs2saves-<yyyymmdd>-pre-menulayout`
   with the whole tree (robocopy exit codes 0 to 3 are success). This copy is
   made by the agent, never by the feature under test. Then take a
   `hs_saves_backup` labelled `pre-menulayout-phase0`. Record both.
2. Record the SHA-256 of the installed `bin\mods\aurie\BloodPactPlugin.dll`,
   copy it beside itself as `BloodPactPlugin.dll.pre-menulayout-<hash8>`, and
   copy this branch's `plugin_build\BloodPactPlugin_rel.dll` over the
   installed one.
3. Launch with `hs_launch` and wait for phase `plugin_ready`; send `ping`
   through `hs_command` and expect the pong.
4. At the main menu, send `menulayout`. Control: a row with
   `obj=UI_Button_obj`, `win=336,534` and `text=Play local` under a header
   reading `window=1920x1080`. Record the whole framed reply verbatim; it is
   the hub's first fixture. If the control fails, stop here, record
   `control: fail`, and treat the instrument as blind.
5. Click that row's `win` point with `hs_input`: one `click`, `hold_ms` 120,
   the `send_input` route. Send `menulayout` again until the header reads
   `room=Chose_rm` and record that reply verbatim (fixture 2). Identify the
   card rows (§ Decision). Dump the slot-1 card with `citrace dumpobj` and
   record only the variables that could number it. Take a screenshot of the
   game window with the `grab_window` capture method.
6. Click the slot-1 card's chosen point the same way, send `menulayout`
   again and record the reply verbatim (fixture 3). Identify `PLAY`, dump it
   with `citrace dumpobj` and take a screenshot. Do not click `PLAY`.
7. Stop the game with `hs_stop_game` and expect `exited: true` and
   `forced: false`. Inspect the backup with `hs_saves_inspect` (at most
   `shop.ini` should differ), restore it with `hs_saves_restore` quoting the
   same id as the confirmation, and inspect again: `changed` and `missing`
   must both be empty.
8. Restore the installed DLL from the `.pre-menulayout-<hash8>` copy and
   hash it again; the hash must equal step 2's.
9. Fill § Results, a dated row per step, and § Decision.

If no candidate row encloses either expected point on either screen, that
shows only that the cards are not listed by the thirteen-name table, or are
not in GUI space - the only space the `win` mapping is valid for. It is not
evidence that a panel object draws the cards itself. Record the three
listings and the dumps of `UI_Character_obj` and `UI_Main_Menu_obj`, then
compare each row's `bbox` with the printed `view` rectangle (a room-space
card would sit inside the view, not the GUI), and widen the search with a
`menulayout <ObjectName>` sweep over every UI, select and save object the
static search names before concluding anything about how the screen is
built. Do not fall back to fractions.

## Results

Not yet run. One row per live-procedure step; the listing lines are runtime
data and are recorded as the game printed them.

| Step | Observation | Control | Date |
| --- | --- | --- | --- |
| P0-1 | pending | - | - |
| P0-2 | pending | - | - |
| P0-3 | pending | - | - |
| P0-4 | pending | pending | - |
| P0-5 | pending | - | - |
| P0-6 | pending | - | - |
| P0-7 | pending | - | - |
| P0-8 | pending | - | - |
| P0-9 | pending | - | - |

## Decision

Four lines, each `pending` until phase 0 has measured it. The hub's matchers
for the slot card and `PLAY` are written against these lines and against the
verbatim replies recorded above, not against expectations.

slotObject: pending
slotRule: pending
playObject: pending
playRule: pending

What each line records:

- **slotObject** is the object name of the save-slot card: the `obj=` field
  of the row at `Chose_rm` that encloses GUI (324, 285).
- **slotRule** is how the hub numbers the cards on page 1. The order is the
  owner's (2026-09-21): row-major - slot 1 is the top-left card, numbers run
  left to right along a row and continue on the next row, so slot 2 is the
  card at grid (x:1, y:0), never (x:0, y:1). The hub's rule is therefore
  `order:y,x`: cards sorted by `win` y, then `win` x, with cards whose y
  differ by less than half a card height counted as one row. If a printed or
  dumped variable numbers the cards, phase 0 records it beside the row-major
  numbering and checks the two agree on every listed card. A disagreement is
  written here as `slotRule: CONFLICT` with both numberings, and neither is
  preferred until the owner decides.
- **playObject** and **playRule** are the `PLAY` button's object name and the
  field the hub matches it by: `text=PLAY` if it carries text, otherwise its
  sprite name and visibility. Also recorded: whether the row already existed,
  hidden, before the slot click - the hub polls for `visible=1`.
- Either rule may add `point:bbox-centre` when clicking the instance origin
  does not activate the control (a card whose origin is its top-left corner,
  for instance). The default is `point:origin`, which is what `Play local`
  measured.
