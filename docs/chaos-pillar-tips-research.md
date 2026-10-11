# Always show Chaos Pillar mods (ForgePact #7)

**Question.** A Chaos Pillar shows its box of mods (pack size, magic find,
experience, the extra loot and the rest) only while the mouse cursor is on it.
Issue #7 asks for the mods to be visible without hovering, the way a Path of
Exile strongbox shows its mods. How does the game decide when the box is
drawn, and what is the smallest change, made by name from ForgePact, that
shows it for every pillar on screen?

**Why it matters.** Deciding which pillar to activate means walking the cursor
over each one in turn. `AGENTS.md` § "Don't Suspend the Game's Own Runtime"
prefers changing one value the game already reads over drawing our own copy of
the box, so this research looks for that value first.

**Posture.** Everything here is measured runtime behaviour, a reading written
in our own words, or our own code. Game objects, scripts and event rows are
named by their `hs-game-sdk` names and indices. No game script text, no
address and no struct offset appears. A reading is labelled as a reading, and
it is not a fact until a live session records it.

**Out of scope.** What a pillar rolls, how many pillars spawn (the World tab's
Chaos Pillars slider), activating a pillar, its fight and its rewards; drawing
our own box; other objects' tooltips (`Chaos_Shrine_obj`, mining nodes, quest
NPCs); hiding a box under an open window; co-op; gamepad play (the owner,
2026-10-11: "Gamepad not supported by default - we are not testing for
gamepad").

## Status

- **Shipped in 2.3.0 (notes `release-notes-v2.3.0.md`), off by default:**
  Mods → Quality of Life → **Always show Chaos Pillar mods**, plugin command
  `pillartips 1|0|stat`, both builds.
- **Static reading:** done 2026-10-11 on game build `pe-6aaa6779-0cad4fc8`
  (below; the shared copy is `docs/RUNTIME_DATA_MODELS.md` § 14.4 in the hub).
- **Harness and contract tests:** pass (`tests/test_chaos_pillar_tips_behavior.py`,
  `tests/test_chaos_pillar_tips_contract.py`,
  `tests/test_chaos_pillar_tips_panel_contract.py`).
- **Live procedure 1:** not run yet.
- **Gamepad:** not supported, and not tested (owner, 2026-10-11).

## Static search

The names that matter, from `hs-game-sdk` (`objects.hpp`, `scripts.hpp`):

| Name | Kind | Index | Why it matters |
|---|---|---|---|
| `Chaos_Pillar_obj` | object | 861 | The pillar. Parents `Quest_NPC_Parent_obj` → `Collision_Parent_obj` → `Avoidable_Parent_obj`; no children. |
| `Chaos_Pillar_Tooltip_obj` | object | 862 | The box of mods. No parent. One per pillar. |
| `Spawn_Chaos_Pillars_obj` | object | 4663 | Places a zone's pillars (the `specialrate chaospillars` lever). |
| `DialogChaosPillar` | script | 1143 | The activation dialog. Not part of the hover path. |
| `CA_enemyCreateChaosPillar` | script | 269 | Co-op: a pillar created on a peer. |
| `NetworkSendEnemyCreateChaosPillar` | script | 2536 | Co-op: the sending half. |
| `anon@1604`, `anon@2325`, `anon@2616`, `anon@2777` `@gml_Object_Chaos_Pillar_obj_Create_0` | closures | 4744-4747 | The four closures the pillar's Create defines; one of them is the interact closure. None writes the tooltip's `visible`. |

The object events are rows of the compiled-code table, not named scripts:
`gml_Object_Chaos_Pillar_obj_Create_0`, `_Alarm_0`, `_Step_0`, `_Draw_0`,
`_Destroy_0`, `_CleanUp_0`, `_Mouse_10` (mouse enter) and `_Mouse_11` (mouse
leave), and `gml_Object_Chaos_Pillar_Tooltip_obj_Create_0` and `_Draw_64`
(Draw GUI). The local Ghidra project names scripts and closures only, so the
event bodies were found through their own name strings and read headless;
Ghidra's callers and xrefs for them came back empty, which is "not observed",
not "has none". The decompiles stay on the researcher's machine (indexed with
`tools/decomp_index.py` under the hub's tooling, never in a repository).

## Static reading

A reading of build `pe-6aaa6779-0cad4fc8`, 2026-10-11, in our own words. Only
the one-to-one count is measured.

- **One tooltip per pillar.** A session counted 18 `Chaos_Pillar_obj` and 18
  `Chaos_Pillar_Tooltip_obj` in one map (**measured** 2026-08-26 on an older
  build, `S10-special-content-notes.md`, "CALISAN TARIF").
- **Create** stores noone (-4) in `chaosPillarTooltip`, sets `device`,
  `isHovered` and `isCoOp` to false and arms alarm 0.
- **Alarm 0 rolls the pillar.** It draws the affixes from `affixPool` into
  `affixes`, works out the box's height, its rarity colour and seventeen
  `tooltip*` display values, then creates the pillar's one
  `Chaos_Pillar_Tooltip_obj` at the pillar's own position on the UI layer,
  keeps it in `chaosPillarTooltip` and copies the values onto it. It sets
  `device` when the player's input device reads as a gamepad (and, with
  `isCoOp`, on a global flag read as local co-op; which flag was not
  established).
- **The hover gate is the tooltip's built-in `visible`.** The pillar's mouse
  enter sets it true and mouse leave sets it false, each only while
  `chaosPillarTooltip` holds a tooltip. The tooltip's own code never writes it;
  its starting value is an object property, inferred to be false because the
  box shows only on hover.
- **Step.** An `activated` pillar takes its own branch and leaves the tooltip
  alone. Otherwise, only while `device` is true, it rewrites the tooltip's
  `visible` every step: true while the local player is within 180 px, false
  otherwise. With mouse input the Step never touches it.
- **The tooltip's Draw GUI** does nothing unless its `visible` is true; then
  it maps its room position through view camera 0 into GUI space and draws the
  box there, so the box follows its pillar wherever the camera is.
- **Destroy** destroys the tooltip and stores -1 (not -4) in
  `chaosPillarTooltip`, so both values mean "no tooltip".
- **Two negatives, not observed.** A byte scan for code using the
  `chaosPillarTooltip` variable found its uses only inside the pillar's own
  Create, Alarm 0, Step, mouse enter, mouse leave and Destroy. A scan for code
  building a reference to object 862 found only Alarm 0's creation; the same
  scan on the gamba machine (4644) found its four sites, the positive control.
  A reference built another way, or a `variable_instance_*` access by string,
  would not show in either. `isHovered` is written in Create and read nowhere
  the scan sees.

So the game already has the switch issue #7 needs: set the tooltip's
`visible` to true and the game draws its own box, with its own text,
translations and colours.

## The mod

`pillartips 1|0|stat` (both builds; Mods → Quality of Life → **Always show
Chaos Pillar mods**, panel key `mod_pillar_tips`, off by default).

**Shape.** The decision is `plugin/include/ForgePact/ChaosPillarTips.hpp`
(`ForgePact::ChaosPillarTips::Core`, game-independent: instance ids as
integers, positions and the view as numbers), compiled whole by
`tests/chaos_pillar_tips_harness.cpp`. The adapter in `ModuleMain.cpp`, from
"Always show Chaos Pillar mods (ChaosPillarTips.hpp): the adapter" to "end of
the Chaos Pillar mods adapter", feeds it a census and the view and carries out
the writes it returns. `PillarTipsTick()` is one call line in
`Hook_DrawHudBuffs`, right after `DungeonChestDraw();`: Controller_obj's Draw
GUI, a hook ForgePact already installs at init in both builds. The mod
installs no hook of its own and runs under the existing `hudlabels` incident
scope.

**Rules.**
- *Off* reads nothing and writes nothing. Switching off writes one hide
  (`visible` false) per tooltip the mod showed and still holds, then nothing.
- *On*, every frame: each pillar that is on screen, not activated, whose
  `activated` could be read and that has a tooltip gets a show write
  (`visible` true), every frame, since the game may have hidden it since, and
  joins the shown set. A shown tooltip whose pillar left the screen, was
  activated, became unreadable, lost its tooltip or left the census gets one
  hide and leaves the set.
- *A tooltip the mod did not show is never written.* An activated pillar stays
  the game's: its box shows on hover, as before (the owner's default,
  2026-10-11).
- *On screen* is view camera 0 grown by a quarter of its width and height on
  every side (`kViewMargin` 0.25), edges included. A view of zero or unread
  size decides nothing that frame.
- *The census* (each pillar's id, x, y, `activated`, tooltip) is taken at most
  once every 30 frames (`kCensusFrames`) and on the first frame in a new room;
  a room change empties the shown set without writing, since those instances
  went with the room. It reads at most 512 pillars.
- *By name only.* The two objects through `asset_get_index` on the SDK's
  `GetObjectName`, variables through `variable_instance_get` /
  `variable_instance_set`, the view through `view_get_camera(0)` and
  `camera_get_view_*`. Instances are kept between frames only as their `id`s.
  A tooltip is accepted only if it exists, its own `id` is the value read and
  its `object_index` is `Chaos_Pillar_Tooltip_obj`; noone and -1 are none.
  Pillars are accepted through `IsInstanceHandle` (the runner returns
  `VALUE_REF`).
- *Point of use.* Right before each write the adapter checks
  `instance_exists` on that tooltip; a write it cannot make is counted, not
  retried (`writesRefused`, shown on `pillarprobe list`).
- *No arming.* There is no hook to install, so `pillartips 1` sent at launch
  only sets the switch; the tick finds no pillar until a zone has one.

**Stat line.** Every form prints
`pillartips: <on|off> pillars= onscreen= shown= activated= unreadable= notooltip= showWrites= hideWrites= ticks= hudCalls=`;
`hudCalls` is the HUD hook's own call count, the proof the write point runs.
The modstate JSON carries `"pillarTips":{"enabled":<bool>,"shown":<n>}`.

**Why that write point.** Controller_obj's Draw GUI runs after every Step-phase
writer of `false` (mouse leave, and the gamepad branch of the pillar's Step;
`docs/RUNTIME_DATA_MODELS.md` § 5.6), so a show written there is not undone
before the draw for mouse play. Whether it also runs before the tooltips' own
Draw GUI in the same frame is a layer-depth question; Live procedure 1 records
both depths.

**Rejected alternatives** (do not re-propose without new evidence):
1. *Drawing the mods ourselves.* It re-implements the game's seventeen-line
   box, its translations, colours and layout, drifts with every patch, and
   draws from this hook inherit an unstable font.
2. *Writing at `EVENT_FRAME`.* That is the end of the frame, after the draw;
   a gamepad pillar's Step hides the box again before the next draw.
3. *Detouring an event row* (the pillar's Step or mouse leave, or the
   tooltip's Draw GUI) through the compiled-code table, as `evcount` and the
   gamba probe do. It would be the first player-build event-row hook, and a
   Draw GUI detour cannot help: the runner skips draw events of an invisible
   instance, and the body checks `visible` first anyway.
4. *Every tooltip visible, on screen or not.* Each visible tooltip runs the
   game's box-building body every frame wherever it is, and `specialrate
   chaospillars` can put over a hundred pillars in a zone.
5. *Writing `device` or `isHovered` on the pillar.* `device` changes how the
   pillar is interacted with; `isHovered` is read nowhere the scan sees.

## Instrument

- **`pillarprobe list`** (research build only, never in `kPlayerCommands`):
  one header line,
  `pillarprobe: room=<name> pillars=<n> view=<x>,<y>,<w>,<h> window=<w>x<h> controllerDepth=<d>`
  followed by `writesRefused=` and `censusFailed=` (the adapter's refusals,
  which the shipped stat line does not carry), then one row per pillar in
  `instance_find` order,
  `pillarprobe #<k> id= x= y= hx= hy= onscreen=<0|1> activated= device= tooltip=<id|none> tipExists=<0|1> visible= tipDepth= tipLayer=`.
  `hx`/`hy` is the client pixel of the pillar's bounding-box centre, what
  `hs_input`'s `move` takes; `onscreen` is the shipped on-screen test.
- **`pillarprobe device <k> <0|1>`** writes pillar k's `device` and prints
  `pillarprobe: device #<k> before= after=`. It was built for a gamepad
  measurement the owner then dropped (gamepad is not supported); Live
  procedure 1 does not use it.
- **`evcount gml_Object_Chaos_Pillar_Tooltip_obj_Draw_64`** counts the
  tooltip's Draw GUI calls through its compiled-code row (the research build's
  `evcount`, verified live on Clean Up rows in the far-sleep research). Since
  the runner skips the draw events of an invisible instance, a rising count
  means some tooltip is visible and a flat one means none is.
- **Positive controls.** Hovering a pillar with the mod off must turn its row
  to `visible=1` and the `evcount` count rising, and leaving it must turn them
  back (Live procedure 1, step 2). A `visible=0` or a flat count elsewhere is
  read only after that control has moved both. `pillartips stat`'s
  `hudCalls=` growing between two reads is the control for the write point.

## Live procedure 1

Not run yet. The research build (`plugin_build\build.bat dev`, the owner
asked before it is installed), save slot 14 "Sorak". Mouse play only.

- **Control:** `ping` answers `pong (YYTK 4.0.1)`; `pillartips stat` prints
  one line starting `pillartips: off` whose `hudCalls=` grows between two
  reads 1 s apart; the installed DLL's SHA-256 equals the built one; this
  launch's plugin-loaded line is in `bp_ipc\out.txt`.
- **Setup:** note every mod's state. `pillartips 0`. In town `estforce 0 0`,
  `estforce 1 0`, `estforce 2 0`, `estforce 3 0`, `specialrate chaospillars
  10`; take the town waypoint to the Outskirts of Inoya. `pillarprobe list`
  must show a pillar with `tipExists=1` (else `specialrate chaospillars 100`
  and one more zone). `playerwarp` to 200 px below pillar #0, park the cursor
  at least 300 px from every `hx`/`hy`, and start
  `evcount gml_Object_Chaos_Pillar_Tooltip_obj_Draw_64`.
- **Steps** ("rising" is `evcount stat` up by at least 30 over two reads 1 s
  apart, "flat" unchanged):
  1. `off-hidden`: switch off, cursor parked: row #0 `visible=0`, flat, no box
     in the screenshot.
  2. `hover-control`: move the cursor onto #0's `hx`,`hy`: `visible=1`,
     rising, the box in a screenshot; back to the parked point: `visible=0`,
     flat.
  3. `on-shown`: `pillartips 1`, cursor parked: `visible=1`, rising, the box
     without hover; `onscreen=` and `shown=` at least 1.
  4. `on-leave`: hover #0, then park: still `visible=1`, rising.
  5. `on-offscreen`: `playerwarp` at least 3000 px from every pillar: each
     row with `onscreen=0` reads `visible=0`; `shown=0`; flat.
  6. `depth-order` (research): record `controllerDepth=` and #0's `tipDepth=`
     and `tipLayer=`.
  7. `cost` (research), back 200 px below pillar #0: `pillartips 0`, `perf
     reset`, 10 s, `perf`; then `pillartips 1`, the same. Record the
     `DrawHudBuffs hook` `avg=` both ways (expected: on adds under 100 us).
  8. `off-restores`: `pillartips 0`: #0 `visible=0` with the cursor parked;
     hover: `visible=1` again.
- **Cases.** The ordinary case is one hovered pillar on screen; the outlier is
  the off-screen pillar. An activated pillar is covered by the harness only
  (activating one starts its fight): not observed live.
- **Teardown:** `pillartips 0`, every mod back to its noted state, stop the
  game, restore the session's save backup.

## Results

Not run yet.

## Route

Not run yet. No route is measured: the mod's write point is fixed (above), and
gamepad: not supported (owner, 2026-10-11).

## Not established

- Whether Controller_obj's Draw GUI runs before or after the tooltips' Draw
  GUI in a frame (Live procedure 1 records both depths). For mouse play it
  does not matter: mouse leave is the only writer of `false`, once.
- Gamepad play: with a controller the pillar's Step rewrites `visible` every
  step from the player's distance, which may hide the box again before it is
  drawn. Not supported, and not tested (owner, 2026-10-11).
- The tooltip's starting `visible` (an object property, inferred false).
- Which global flag Alarm 0 reads as local co-op; co-op as a whole.
- An activated pillar live (harness only).
- How the boxes look when several overlap, or over an open window: shipped as
  the game draws them.
- Whether anything outside the pillar's own events reaches its tooltip by a
  route the two static scans cannot see.
