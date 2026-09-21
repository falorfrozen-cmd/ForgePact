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
build. Phase 0, the live enumeration of the character-select screen, ran on
2026-09-21 with the research build; its positive control passed and § Decision
is filled from what it listed. The slot-card and `PLAY` click points are
measured by the listing; `PLAY` itself was deliberately never clicked, so the
end-to-end load through it is L-2's to show, not this document's.

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

What the 2026-09-21 research session already established
(`character-select-research.md`, results C-1.6, C-1.13, C-1.15 and C-1.17, and
the replies it left in the game's own `out.txt`). That research was PR #60; it
is folded into this feature's branch and pull request, so the document cited
here ships beside this one; its `menuprobe` verb stays research-build only.
What it found:

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
slot cards and `PLAY` live in GUI space too was not known before phase 0, so
the header also prints view 0's camera rectangle: a room-space object is
recognisable because its scaled point will not land near the point a
screenshot measured. Phase 0 showed both are in GUI space: `PLAY` maps to
(584, 345) against C-1.15's screenshot point (583, 346), and a click at slot
1's mapped point opened its panel (§ Results P0-5, P0-6).

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

Phase 0 ran on 2026-09-21, one launch, research build
`plugin_build\BloodPactPlugin_rel.dll` (SHA-256 `ad17f0a5...`, built from this
branch at `f9889a6`), windowed 1920x1080. The listing lines below are runtime
data recorded as the game printed them; the three full framed replies are
kept verbatim by the hub as its test fixtures.

| Step | Observation | Control | Date |
| --- | --- | --- | --- |
| P0-1 | robocopy of `hs2saves` to `C:\Users\stann\HeroSiege-manual-save-backup\hs2saves-20260921-pre-menulayout`: 137 files, 1,281,612 bytes, exit code 1 (files copied). `hs_saves_backup` id `20260921T124419Z_pre-menulayout-phase0`, 137 files, 1,281,612 bytes. | - | 2026-09-21 |
| P0-2 | Installed `BloodPactPlugin.dll` SHA-256 `22371422eb0c0f5d104e36dc5b05775ecace31e042fd04983594448454fabc33` (1,454,080 bytes; not the `ae52529a...` noted at planning time), copied beside itself as `BloodPactPlugin.dll.pre-menulayout-22371422`; research build `ad17f0a564a181b2e2239297f5b1610f0ae63e38e084effbf6bc040e9b2838de` installed. | - | 2026-09-21 |
| P0-3 | `hs_launch`: phase `plugin_ready` in 21.6 s; `ping` answered `pong (YYTK 4.0.1)`. | - | 2026-09-21 |
| P0-4 | Main menu: header `menulayout: room=Main_Menu_rm gui=2560x1368 window=1920x1080 fullscreen=0 view=0.0,0.0,1280.0,720.0`; row `obj=UI_Button_obj id=257029 gui=448.0,676.4 win=336,534 bbox=240.0,604.2,658.0,750.5 visible=1 sprite=Menu_Button_Kaelith_spr text=Play local`; footer `listed=19 absent=none capped=0`. The 19 rows include `UI_Button_Close_obj`, `UI_Button_Menu_DLC_obj`, `UI_Button_Language_obj` and `UI_Container_obj`, none of which is in the thirteen-name table, so listing a root does reach child instances. | pass | 2026-09-21 |
| P0-5 | One held click (120 ms, `send_input`) at `win=336,534` changed the room; the next `menulayout` read `room=Chose_rm`, `listed=56`. The save-slot cards are 48 `Choose_Parent_obj` instances (not in the table; reached through a root), each carrying a `slot` variable: `slot=1` to `24` are `visible=1`, sprite `Choosing_SSF_spr`, in three rows of eight; `slot=25` to `48` are `visible=0`, sprite `Choosing_spr`, at the same eight-by-three positions (the other page). Visible top row: slot 1 `win=177,174`, slot 2 `win=381,174`, then x `585`, `789`, `993`, `1197`, `1401`, `1605`, all on y `174`; slots 9 to 16 on y `429` from x `177`; slots 17 to 24 on y `684`. Slot 1's row is `obj=Choose_Parent_obj id=257048 gui=236.0,220.4 win=177,174 bbox=190.0,138.7,462.0,433.2 visible=1 sprite=Choosing_SSF_spr slot=1 text=`; its bbox encloses the expected GUI point (324, 285). `citrace dumpobj Choose_Parent_obj 0` on that instance: `slot = real:1.000000`, `slotClassName = string:"Paladin"`, `uiNodeCallstack = string:"ChooseHeroSlot"`, `clickActivate = bool:true`. Sorting the 24 visible rows by `win` y then x reproduces `slot=1` to `24` exactly. Screenshot `C:\Users\stann\AppData\Local\HSDriveMcp\screenshots\20260921T124546347276Z_p0-chose_rm.png` shows Pal (Paladin) top-left and Miss Fortune to its right. | - | 2026-09-21 |
| P0-6 | One held click at slot 1's listed origin `win=177,174` opened the character panel for Pal; the room stayed `Chose_rm`, `listed=111`. New row `obj=UI_Button_obj id=257591 gui=778.0,437.0 win=584,345 bbox=604.0,397.1,954.0,478.8 visible=1 sprite=Menu_Button_spr text=Play` (the screen draws it as `PLAY`); it was not in the P0-5 listing at all, hidden or otherwise. The 24 card rows stay `visible=1` under the panel. No other row carries `text=Play`. `citrace dumpobj UI_Button_obj 5` on it: `text = string:"Play"`, `uiNodeCallstack = string:"CharacterPlay"`, `enabled = bool:true`, parent the panel's `UI_Character_obj` (id 257590). Screenshot `C:\Users\stann\AppData\Local\HSDriveMcp\screenshots\20260921T124616092325Z_p0-panel.png`. `PLAY` was not clicked. | - | 2026-09-21 |
| P0-7 | `hs_stop_game`: `exited: true`, `forced: false`. `hs_saves_inspect` before restore: `changed`, `added` and `missing` all empty (not even `shop.ini` changed). Restored (pre-restore backup `20260921T124658Z_pre-restore`); inspect again: `changed: []`, `missing: []`. | - | 2026-09-21 |
| P0-8 | Installed DLL restored from `BloodPactPlugin.dll.pre-menulayout-22371422`; SHA-256 again `22371422eb0c0f5d104e36dc5b05775ecace31e042fd04983594448454fabc33`, equal to P0-2. | pass | 2026-09-21 |
| P0-9 | § Decision filled below. No optional variable had to be added: `slot` was already printed. | - | 2026-09-21 |

## Decision

Four lines, each `pending` until phase 0 has measured it. The hub's matchers
for the slot card and `PLAY` are written against these lines and against the
verbatim replies recorded above, not against expectations.

slotObject: Choose_Parent_obj
slotRule: order:y,x over the visible=1 Choose_Parent_obj rows (slot 2 is the card right of slot 1 on the same row); the game's slot variable agrees on all 24 page-1 cards; point:origin
playObject: UI_Button_obj
playRule: text=Play exactly (case-sensitive, not Play local) and visible=1; not listed until the slot click, so poll for it; point:origin

What phase 0 measured behind each line (2026-09-21):

- The cards are not in the thirteen-name table. They were listed because a
  root reaches them, which is why the roots are in the table.
- Page 1's 24 cards are `visible=1`; the other page's 24 sit at the same
  positions with `visible=0`. A slot matcher that does not filter on
  `visible=1` would match two cards per position.
- The game's own `slot` variable numbers the visible cards 1 to 24 in exactly
  the row-major order the owner stated. The hub may match by `slot=N` or by
  sorting; its test must still show slot 2 is the card to the right of
  slot 1.
- `PLAY` is created by the slot click, not revealed. Its `text` is `Play`, so
  matching the drawn caption `PLAY` would miss it. The card rows stay
  `visible=1` while the panel is open, so the cards disappearing is not a
  signal that the panel is up.
- `point:origin` was measured for `Play local` and the slot card (both
  clicks acted). It was not measured for `PLAY`, which was never clicked;
  its origin sits at the centre of its bbox, the same shape as `Play local`.

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
