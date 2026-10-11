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
  (below; the shared copy is `docs/RUNTIME_DATA_MODELS.md` § 14.4 in the hub),
  and corrected the same day after Live 1: the pillar's `activated` holds a
  protected-store key, not the state.
- **Harness and contract tests:** pass (`tests/test_chaos_pillar_tips_behavior.py`,
  `tests/test_chaos_pillar_tips_contract.py`,
  `tests/test_chaos_pillar_tips_panel_contract.py`; 50 tests at `82c36ad`).
- **Live procedure 1:** ran 2026-10-11 on the first build (`f7d913be`). The
  hover gate and the depths are measured; the mod showed nothing, because that
  build took the raw `activated` key for the state and read all 17 untouched
  pillars as activated (`on-shown` and `on-leave` failed; "Live 1 results").
  The mod now reads the record behind the key ("The mod").
- **Live procedure 2:** ran 2026-10-11 on the corrected build (`4d49c91a`,
  ForgePact `172b183`), mouse play: 19 of 19 checks pass. Every untouched
  pillar's record read false, the mod showed the box of each on-screen pillar
  without hover and hid it off screen, and a pillar activated through the
  game's own dialog read activated within 2 s and had its box hidden; the game
  then removed that pillar ("Live 2 results"). `activated-route: proven`.
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
the one-to-one count is measured here; Live 1 later measured the hover gate
and the raw `activated` value ("Live 1 results").

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
- **Step.** When the pillar's protected `activated` record reads true, the
  Step takes its own branch (the pillar trembles) and leaves the tooltip
  alone. Otherwise, only while `device` is true, it rewrites the tooltip's
  `visible` every step: true while the local player is within 180 px, false
  otherwise. With mouse input the Step never touches it.
- **`activated` holds a key, not the state** (read after Live 1 showed every
  untouched pillar's raw value as true). The pillar's Create asks the game's
  anti-cheat extension for a new record in the native protected-value store
  (hub `docs/RUNTIME_DATA_MODELS.md` § 5.8), keeps the key it gets in
  `activated` and stores the starting value under it, inferred false from the
  constant passed; CleanUp frees the key. `rarity`, `affix`,
  `activateTimer`, `magicFind01`..`06`, `lootAmount01`..`06`, `xZe` and
  `zrmb` hold keys the same way. The game reads the
  state back through the store: the pillar's interact closure does nothing
  once the record is true, so a used pillar cannot be talked to again, and the
  Step's true branch is the tremble. `DialogChaosPillar` reads the `rarity`
  record; at the lower rarities it calls the pillar's activation closure at
  once, at the higher ones it first takes an item from the inventory (which
  item was not established). The activation closure sends a client effect
  carrying the pillar's position; the handler that presumably sets the record
  true and starts the fight was not read.
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
- *Activation is the pillar's protected record.* The adapter's `PtReadRecord`
  reads `activated` by name (the key), hands it to the core's key test
  (`IsStoreKey`: a real, int32 or int64 holding a finite whole number in
  [0, 262144); any other kind, bool included, is no key), and only for an
  accepted key calls the game's `PC_GetVariableGMLWrapper` script by its SDK
  name, the route `PackMarkers::ReadStore` ships for a creator's `spawnPack`
  (hub `docs/RUNTIME_DATA_MODELS.md` §§ 5.8, 11.2). A refused key never
  reaches the call: a -1 key faulted the game, and `catch (...)` does not
  catch that under /EHsc. The core's `ActivationFromAnswer` maps the answer:
  true, or a number above 0.5, is activated; false, or a number at or below
  0.5, is not; a refused key, a read that threw, undefined or any other kind
  is unreadable, which the core never shows. The raw key is never itself taken
  for the state. The mod reads records and writes none.
- *On screen* is view camera 0 grown by a quarter of its width and height on
  every side (`kViewMargin` 0.25), edges included. A view of zero or unread
  size decides nothing that frame.
- *The census* (each pillar's id, x, y, activation, tooltip) is taken at most
  once every 30 frames (`kCensusFrames`) and on the first frame in a new room;
  a room change empties the shown set without writing, since those instances
  went with the room. That assumes the room is not persistent, as the zones
  measured were not; in a persistent room a tooltip the mod showed would keep
  `visible` true until the game's own mouse leave (not observed). It reads at
  most 512 pillars. A census whose object names do not resolve is not taken
  and is counted (`censusFailed`); the previous one keeps deciding. So a pillar
  activated between two censuses keeps its box for up to 30 frames, about half
  a second, then gets one hide, even under the cursor; the next hover shows it
  again, the game's own way.
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
  retried (`writesRefused`).
- *No arming.* There is no hook to install, so `pillartips 1` sent at launch
  only sets the switch; the tick finds no pillar until a zone has one.

**Stat line.** Every form prints
`pillartips: <on|off> pillars= onscreen= shown= activated= unreadable= notooltip= showWrites= hideWrites= writesRefused= censusFailed= ticks= hudCalls=`;
`activated` counts pillars whose record answered true, `unreadable` those whose
key was refused or whose read failed, `writesRefused` and `censusFailed` the
adapter's refusals, and `hudCalls` is the HUD hook's own call count, the proof
the write point runs.
The modstate JSON carries `"pillarTips":{"enabled":<bool>,"shown":<n>}`.

**Why that write point.** Controller_obj's Draw GUI runs after every Step-phase
writer of `false` (mouse leave, and the gamepad branch of the pillar's Step;
`docs/RUNTIME_DATA_MODELS.md` § 5.6), so a show written there is not undone
before the draw for mouse play. Whether it also runs before the tooltips' own
Draw GUI in the same frame is a layer-depth question. Live 1 measured
Controller_obj at depth -12998 and every tooltip at -13001 on
`Layer_UI_Parent_1`; that the tooltips therefore draw after the hook, so a
show written there is drawn the same frame, is an inference from those depths.

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
6. *The raw `activated` value as the state* (the first build). Live 1 read it
   as true on every untouched pillar: it is the record's key.
7. *`trembleTimer`, `flameImage`, `isDiscovered` or `speakCooldown` as the
   signal.* They are ordinary variables, but `trembleTimer` counts down, so a
   pillar used earlier would read as unused once it stops, and the reading
   does not tie the other three (animation, discovery, talk cooldown) to
   activation. Live procedure 2 records them only as candidates, in case the
   record does not flip.
8. *Calling the extension's `GetVariable` by name.* It does not resolve from
   the plugin (measured on the gamba machine, hub
   `docs/RUNTIME_DATA_MODELS.md` § 20.5).
9. *Making an activation from the plugin* (writing the record, or calling the
   pillar's activation closure). The mod writes no game state but `visible`,
   and nobody has read how the activation's client effect behaves offline; a
   player's own activation is the case under test, so Live procedure 2 drives
   it through the game's own dialog.

## Instrument

- **`pillarprobe list`** (research build only, never in `kPlayerCommands`):
  one header line,
  `pillarprobe: room=<name> pillars=<n> view=<x>,<y>,<w>,<h> window=<w>x<h> controllerDepth=<d>`
  (the refusal counts Live 1 read here, `writesRefused=` and `censusFailed=`,
  are now on `pillartips stat`), then one row per pillar in `instance_find`
  order,
  `pillarprobe #<k> id= x= y= hx= hy= onscreen=<0|1> activatedKey=<raw> activatedValue=<ans> activation=<inactive|activated|unreadable> rarityKey=<raw> rarityValue=<ans> activateTimerKey=<raw> activateTimerValue=<ans> trembleTimer= flameImage= isDiscovered= speakCooldown= questState= image_index= sprite_index= device= tooltip=<id|none> tipExists=<0|1> visible= tipDepth= tipLayer=`.
  `hx`/`hy` is the client pixel of the pillar's bounding-box centre, what
  `hs_input`'s `move` takes; `onscreen` is the shipped on-screen test. Each
  of the three records is read through the shipped `PtReadRecord`: `<raw>` is
  the variable as read (the key: a number in full, or its kind), `<ans>` the
  store's answer (a number in full, `true`/`false`, a kind, `refused` when the
  key test refused the key and no call was made, or `threw`), and
  `activation` is what the core decides from them. The plain fields and
  `visible` print a number in full, `true`/`false` for a bool, or a kind (Live
  1's build printed flags as `0`/`1`). The probe writes no record and
  activates nothing.
- **`pillarprobe getter`**: the store getter's positive control, the same as
  `packmarks census getter` (verified live 2026-10-06): it reads the key in
  `gDataProtected[177]`, runs the key test, then calls `GPV` and
  `PC_GetVariableGMLWrapper` by name and prints
  `pillarprobe getter: gDataProtected[177]=<value> GPV=<value> wrapper=<value> -> proven`
  when both answer the same non-zero number, else `-> unproven`. A record
  read on a pillar means something only in a session where this says
  `proven`.
- **`pillarprobe device <k> <0|1>`** writes pillar k's `device` and prints
  `pillarprobe: device #<k> before= after=`. It was built for a gamepad
  measurement the owner then dropped (gamepad is not supported); neither live
  procedure uses it.
- **`evcount gml_Object_Chaos_Pillar_Tooltip_obj_Draw_64`** counts the
  tooltip's Draw GUI calls through its compiled-code row (the research build's
  `evcount`, verified live on Clean Up rows in the far-sleep research). Since
  the runner skips the draw events of an invisible instance, a rising count
  means some tooltip is visible and a flat one means none is.
- **Positive controls.** Hovering a pillar with the mod off must turn its row
  to `visible=1` and the `evcount` count rising, and leaving it must turn them
  back (Live procedure 1, step 2). A `visible=0` or a flat count elsewhere is
  read only after that control has moved both. `pillartips stat`'s
  `hudCalls=` growing between two reads is the control for the write point,
  and `pillarprobe getter` reading `proven` is the control for the store
  reads.

## Live procedure 1

Ran 2026-10-11 on the first build; the results are under "Live 1 results".
The research build (`plugin_build\build.bat dev`, the owner asked before it is
installed), save slot 14 "Sorak". Mouse play only.

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

## Live procedure 2

Ran 2026-10-11 ("Live 2 results"). The corrected research build (`plugin_build\build.bat dev` after
the record read; a new hash, and the owner is asked before it is installed;
the installed DLL's hash is noted first so it can be put back), save slot 14
"Sorak". Mouse play only: no gamepad step of any kind.

- **Control:** `ping` answers `pong (YYTK 4.0.1)`; `pillartips stat` prints
  one line starting `pillartips: off` that carries `writesRefused=` and
  `censusFailed=`, and whose `hudCalls=` grows between two reads 1 s apart;
  the installed DLL's SHA-256 equals the built one (`dll-hash`); this
  launch's plugin-loaded line (`marker`); and `pillarprobe getter` reads
  `-> proven` (`getter-control`, the store reads' positive control).
- **Setup:** as Live procedure 1, by back end with no owner step: note every
  mod's state, `pillartips 0`, the four `estforce` lines and `specialrate
  chaospillars 10` in town, the town waypoint to the Outskirts of Inoya;
  `pillarprobe list` shows a pillar with `tipExists=1` (`pillars-present`);
  `playerwarp` 200 px below pillar #0, park the cursor at least 300 px from
  every `hx`/`hy`, start `evcount gml_Object_Chaos_Pillar_Tooltip_obj_Draw_64`.
- **Steps** ("rising" and "flat" as in Live procedure 1; `visible` true is
  printed `1` or `true`, false `0` or `false`, as the runtime answers):
  0. `activated-key` (research), from the first `pillarprobe list` in the new
     zone, before any interaction: on every row `activatedKey=` is a whole
     number in [0, 262144), `activatedValue=false` or `0`,
     `activation=inactive`, and the keys differ from row to row. The rows are
     recorded as printed.
     `record-control` (must pass): the store read's positive control on the
     pillar itself. An untouched pillar's `activatedValue=false` or `0` is
     also what a read that returns zero for every key would print, so in
     those same rows at least one `rarityValue=` (or `activateTimerValue=`)
     must be a number, not `refused`, `threw` or a kind name, and the rows'
     `rarityValue=` values must not all be the same (each is compared with
     the rarity the game's own box shows).
  1. `off-hidden`: switch off, cursor parked: #0 `visible` false, flat, no box
     in the screenshot.
  2. `hover-control` (and `draw-count-control`, its rise and fall): the cursor
     on #0's `hx`,`hy` for 0.5 s: `visible` true, rising, the box in a
     screenshot; back to the parked point: `visible` false, flat.
  3. `on-shown`: `pillartips 1` answers `pillartips: on`; cursor parked, 1 s:
     #0 `visible` true, rising, the box without hover; on the stat line
     `onscreen=` and `shown=` at least 1, `activated=0`, `unreadable=0`,
     `writesRefused=0`, `censusFailed=0` and `showWrites=` growing.
  4. `on-leave`: hover #0, then park: still `visible` true, rising.
  5. `on-offscreen`: `playerwarp` at least 3000 px from every pillar: every
     row with `onscreen=0` reads `visible` false, the stat line `shown=0` and
     `hideWrites=` at least 1 (the mod hiding what it had shown), flat.
  6. `cost` (research), back 200 px below #0: `pillartips 0`, `perf reset`,
     10 s, `perf`; then `pillartips 1`, the same. The `DrawHudBuffs hook`
     `avg=` both ways, this time with show writes running; expected: on adds
     under 100 us.
  7. `off-restores`: `pillartips 0`: #0 `visible` false with the cursor
     parked; hover: true; park: false. The game's own behaviour is back.
  8. `activation-made` (research), last, because the fight it starts would
     disturb every step above. Pillar #k, not #0, preferably the nearest with
     the lowest `rarityValue=` (the dialog charges an item at the higher
     rarities). `pillartips 1`, `playerwarp` 60 px below #k, park the cursor,
     `pillarprobe list` (#k's "before" row, expected `visible` true). #k's
     `id=` is noted from that row: steps 9-11 pick #k's row by that `id=`,
     never by its place in `instance_find` order, and if no row carries that
     `id=` afterwards, that is recorded as its own observation (the pillar
     removed on activation), not as `activate-flips` failing. The
     operator clicks #k through `hs_input`, screenshots the dialog, clicks
     its activation option read from the screenshot, and screenshots again.
     If the dialog asks for an item the character lacks, the next nearest
     pillar is tried once. If no click route works, the owner is asked for
     one manual step (activate any pillar near the character by clicking it
     and choosing its activation option, no fight needed, and say which),
     recorded in the capture's manual steps; the plugin never writes pillar
     state, and no back-end tool drives a game dialog except by click.
     `pass` when the game's own dialog activated a pillar, `not-observed`
     when neither route did.
  9. `activate-flips` (research): within 2 s of the activation, cursor
     parked, `pillarprobe list`: #k's `activatedKey=` unchanged,
     `activatedValue=true` or `1`, `activation=activated`; every other row
     still `inactive`.
  10. `on-activated` (research): cursor parked, #k `visible` false, the mod
      having hidden it once (stat `activated=1`, `hideWrites=` up by at least
      1), the other on-screen pillars still shown; hover #k: true, the game's
      own hover; park: false. #k's `tipExists` says whether the tooltip
      survived activation.
  11. `activation-candidates` (research, `pass` when recorded): #k's before
      and after values of `rarityValue`, `activateTimerValue`, `trembleTimer`,
      `flameImage`, `isDiscovered`, `speakCooldown`, `questState`,
      `image_index`, `sprite_index` and `tipExists`, side by side.
- **Route:** `activated-route: proven` when `activation-made`,
  `activate-flips` and `on-activated` all pass; `not-observed` when no
  activation was made; `contradicted` when one was made and `activate-flips`
  or `on-activated` fails, which sends the rule back to research with the
  candidates. `contradicted` is read only when `record-control` passed:
  without it, a failing `activate-flips` measured the store read, not the
  rule, and the route is `not-observed`.
- **Cases.** The ordinary case is an untouched, hovered pillar on screen
  (#0); the outliers are the off-screen pillar (step 5) and the activated
  pillar (steps 8-10), the only one that takes the activated branch of the
  rule. Gamepad: not measured and not supported.
- **Teardown:** `pillartips 0`, `estfree all`, `specialrate chaospillars 1`,
  every mod back to its noted state, `evcount reset`; stop the game, even
  mid-fight, and restore the session's save backup. The owner decides
  whether the previously installed DLL goes back.
- **Must pass:** `dll-hash`, `marker`, `control`, `getter-control`,
  `pillars-present`, `draw-count-control`, `off-hidden`, `hover-control`,
  `on-shown`, `on-leave`, `on-offscreen` and `off-restores`. The research
  checks' verdicts are findings, carried by the route.

## Results

### Live 1 results

2026-10-11, mouse play, save slot 14 "Sorak", in `Act_01_01` (the Outskirts
of Inoya) with `specialrate chaospillars 10`: 17 pillars, each with its
tooltip. The installed and the built DLL were the same, SHA-256
`f7d913beea8edd155b368371f6d9ec9ecae41f7ae9dfc6e4947b7918c8a4713e`, the first
build (plugin v2.2.1 line), which still took the raw `activated` value for
the state.

| Check | Verdict | What was seen |
|---|---|---|
| `dll-hash` | pass | installed and built DLL equal (`f7d913be…`) |
| `marker` | pass | this launch's `==== BloodPact plugin loaded ==== v2.2.1` |
| `control` | pass | `pong (YYTK 4.0.1)`; `pillartips: off …` with `hudCalls=` 1048, then 1498 |
| `pillars-present` | pass | `pillars=17`, every row `tipExists=1` |
| `draw-count-control` | pass | the tooltip Draw GUI count 106 → 706 hovered, 1311 → 1311 parked |
| `off-hidden` | pass | switch off, parked: #0 `visible=0`, count flat at 0, no box |
| `hover-control` | pass | hovered: `visible=1`, count rising, the box in the screenshot ("Experience Gain increased by 5%", "Monster pack drops 1x Relic"); parked: `visible=0`, flat |
| `on-shown` | **fail** | switch on, parked: #0 `visible=0`, flat, no box; the stat line `onscreen=1 shown=0 activated=17 showWrites=0` |
| `on-leave` | **fail** | after hover and park: `visible=0`, flat; still `shown=0 activated=17` |
| `on-offscreen` | pass | warped over 4100 px from the nearest pillar: all 17 rows `onscreen=0 visible=0`, `shown=0`, flat; but the mod had shown nothing to hide |
| `depth-order` (research) | recorded | `controllerDepth=-12998`; every tooltip `tipDepth=-13001` on `Layer_UI_Parent_1` |
| `cost` (research) | pass | `DrawHudBuffs hook` `avg=` 35.4 us off, 52.9 us on (+17.5 us), with no show writes running |
| `off-restores` | pass | switch off: parked `visible=0`; hover `visible=1`; parked again `visible=0` |

- **Why `on-shown` and `on-leave` failed.** The probe read `activated` as
  true (`activated=1`) on all 17 pillars before anyone touched them, and the
  mod counted `activated=17`, so it skipped every pillar and wrote nothing
  (`showWrites=0`). The value is a protected-store key, not the state ("Static
  reading"): a non-zero key is truthy. The probe printed only the truthiness,
  so the key itself was not recorded. The mod now reads the record behind the
  key ("The mod"), and the harness replays this census with keys and shows
  every untouched pillar. Live procedure 2 checks it in the game.
- **Measured.** The hover gate: hovering sets the tooltip's `visible` and its
  Draw GUI runs (the count rises, the box is drawn); leaving clears it and the
  count stops; with the cursor parked the box is not drawn. The depths above.
  That the tooltips draw after the HUD hook, so a show written there is drawn
  the same frame, is an inference from the depths, not a measurement.
- **Cost.** +17.5 us on the hook's average with the mod on, but measured while
  it found every pillar activated and made no show write; Live procedure 2
  measures it with show writes running.
- **Not exercised:** an activated pillar (the probe's `activated=1` was the
  key, not a used pillar); gamepad (not supported).

### Live 2 results

2026-10-11, mouse play, save slot 14 "Sorak", in `Act_01_01` (the Outskirts
of Inoya) with `specialrate chaospillars 10`: 35 pillars, each with its
tooltip. The installed and the built DLL were the same, SHA-256
`4d49c91ac47ff9264e6c33ab6cbbe4dade46ef33e6d4deeda92ea353d74010c7`, the
research build at ForgePact `172b183` (after `live2-controls`; plugin v2.2.1
line), which reads the record behind `activated`. The capture is the toolkit
workorder's `forgepact-7-chaos-pillar-mods-live-2`.

| Check | Verdict | What was seen |
|---|---|---|
| `dll-hash` | pass | installed and built DLL equal (`4d49c91a…`) |
| `marker` | pass | this launch's `==== BloodPact plugin loaded ==== v2.2.1` |
| `control` | pass | `pong (YYTK 4.0.1)`; `pillartips: off … writesRefused=0 censusFailed=0` with `hudCalls=` 2141, then 2291 |
| `getter-control` | pass | `pillarprobe getter: gDataProtected[177]=177 GPV=28 wrapper=28 -> proven` |
| `pillars-present` | pass | `pillars=35`, every row `tipExists=1` |
| `activated-key` (research) | pass | every row: `activatedKey=` a whole number (174773 … 175959, all distinct), `activatedValue=0`, `activation=inactive` |
| `record-control` | pass | `rarityValue=` a number on every row, values 1, 2, 3, 5 and 6; #0 read 3 and the game's own box named it a "Rare Chaos Pillar" with three mods; `activateTimerValue=216` on every row |
| `off-hidden` | pass | switch off, parked: #0 `visible=0`, the Draw GUI count flat at 0, no box |
| `hover-control` | pass | hovered: `visible=1`, count 97 → 787, the box in the screenshot; parked: `visible=0`, 1648 → 1648 |
| `draw-count-control` | pass | step 2's rise and fall, above |
| `on-shown` | pass | switch on, parked: #0 `visible=1`, count 2158 → 3418, the box without hover; `onscreen=1 shown=1 activated=0 unreadable=0 writesRefused=0 censusFailed=0`, `showWrites=` 510 → 1770 |
| `on-leave` | pass | hover then park: still `visible=1`, count 4558 → 5578 |
| `on-offscreen` | pass | warped 1475 px from the nearest pillar (see the deviation below): all 35 rows `onscreen=0 visible=0`; `shown=0`, `hideWrites=32`; count flat at 15171 |
| `cost` (research) | pass | `DrawHudBuffs hook` `avg=` 29.9 us off, 50.9 us on (+21.0 us), with show writes running (`showWrites=` 14179 → 16219 in the window) |
| `off-restores` | pass | switch off: parked `visible=0`; hover `visible=1`; parked again `visible=0`; the mod's `ticks=` stopped |
| `activation-made` (research) | pass | the operator clicked pillar id 297083 (rarity 1) and chose "Yes please!" in the game's own dialog |
| `activate-flips` (research) | pass | within 2 s: the same `activatedKey=175925`, `activatedValue=1`, `activation=activated`; the other 34 rows `inactive` |
| `on-activated` (research) | pass | parked: the activated pillar `visible=0`, `tipExists=1`, stat `activated=1`, `hideWrites=` 36 → 37; the other on-screen pillar stayed `visible=1`. The hover and park sub-step was not observed: the game removed the pillar first |
| `activation-candidates` (research) | pass | the table below |

- **Getter control.** `pillarprobe getter` read `gDataProtected[177]=177
  GPV=28 wrapper=28 -> proven`: the game's `PC_GetVariableGMLWrapper`, called
  by name, answered the same non-zero number as `GPV` for the same key, so the
  record reads below come from an instrument that can return non-zero.
- **`activatedKey` and `activatedValue`.** In the first `pillarprobe list` in
  the zone, before any interaction, all 35 rows carried a distinct key between
  174773 and 175959, with `activatedValue=0` and `activation=inactive`. The
  same rows' `rarityKey=` and `activateTimerKey=` sat two below and one above
  each `activatedKey`. So the raw variable Live 1 took for the state is a key,
  and the record behind it reads false on an untouched pillar. That a read of
  zero is not just the getter answering zero for every key is what
  `record-control` shows: the `rarity` records of the same rows read 1 to 6,
  and #0's 3 matched the "Rare" the game's own box printed.
- **Screenshots, described.** Switch off with the cursor parked: the pillar's
  name, "Rare Chaos Pillar", and no box. Hovered: the box under the name,
  "Experience Gain increased by 15%", "+1 Monster Affixes", "Monster pack
  drops 1x Relic". Switch on with the cursor parked in the bottom-right
  corner: the same box, without hover. After the dialog's click: the pillar's
  column gone and a pentagram with monsters spawning in its place, and a
  "[Basic Key] Picked up" line. The dialog itself read "Chaos Pillar /
  Activate the pillar? / Yes please! / Goodbye!", and asked for no item at
  rarity 1.
- **Cost.** +21.0 us on the hook's average with the mod on and writing a show
  every frame, against Live 1's +17.5 us with no writes. Under the
  procedure's 100 us bound.
- **The activation.** Made by `live-operator`, with no owner step: `pillartips
  1`, `playerwarp` to 60 px below the pillar (id 297083, the nearest to #0 and
  of the lowest rarity), a left click on the pillar with `hs_input`, which
  opened the game's dialog, and a second click on "Yes please!", read from a
  screenshot. The pillar was then replaced by its fight. In the list taken
  within 2 s it read activated and the mod had hidden its box; in the next
  list, a few seconds later, the room had 34 pillars and no row carried its
  id. The candidate fields, before and after:

  | Field | Before | After |
  |---|---|---|
  | `rarityValue` | 1 | 1 |
  | `activateTimerValue` | 216 | 122.002… |
  | `trembleTimer` | 14.4 | 7.2 |
  | `flameImage` | 4427.10… | 4820.85… |
  | `isDiscovered` | true | true |
  | `speakCooldown` | 0 | -0.290… |
  | `questState` | 10 | 10 |
  | `image_index` | 0 | 0 |
  | `sprite_index` | ref | ref |
  | `tipExists` | 1 | 1, then the pillar was removed (35 → 34 pillars) |
  | `activatedValue` / `activation` | 0 / `inactive` | 1 / `activated` |

  `activateTimer`'s record counting down from 216 and `trembleTimer` halving
  fit the Step's activated branch. `flameImage` only advances with time.
- **Manual steps.** None. Every step, the activation included, ran through
  `hs-drive` (`hs_command`, `hs_input` clicks, `hs_screenshot`).
- **Deviations.** `on-offscreen` asked for a warp at least 3000 px from every
  pillar; with 35 pillars no walkable point in the zone is that far (at most
  1913 px from the nearest inside their bounding box), and the game put the
  character back from two farther tries. It ran at 1475 px from the nearest
  pillar, where every row read `onscreen=0`, which is what the check needs.
  `on-activated`'s hover and park on the activated pillar could not run,
  because the game removed it.
- **Also seen.** After `estfree all`, `eSt[10]` read 85, which appeared after
  the activation (the game's own value; nothing was forced). The game exited
  with code `0xC0000409` on `hs_stop_game` while ForgePact's log ended with its
  clean-shutdown line. Neither is attributed to the mod.
- **Measured now.** That the raw `activated` is a store key whose record reads
  false on an untouched pillar; that the game's own activation turns it true
  within 2 s, keeping the same key; that the mod shows the box of every
  on-screen, untouched pillar without hover, keeps it through a hover and
  park, hides it off screen and when the pillar is activated, and gives the
  game its own hover back when switched off; the cost with writes running.
  The tooltip still existed 2 s after activation, and the game then removed
  the pillar.

## Route

`activated-route: proven` (Live 2, 2026-10-11: `activation-made`,
`activate-flips` and `on-activated` pass). The mod's write point is fixed
(above), and gamepad: not supported (owner, 2026-10-11). No other write-point
route is measured.

## Not established

- Which code sets the pillar's `activated` record true. Live 2 measured that
  the game's own activation does it within 2 s; the writer sits behind the
  activation closure's client effect, whose handler was not read.
- What the record holds after the fight, and what happens to a pillar's box
  in the seconds between activation and the game removing the pillar beyond
  the one read Live 2 made (`tipExists=1`, `visible=0`). Whether the game
  always removes an activated pillar, or did so here because its fight began,
  is not established: one activation, at rarity 1, was observed.
- The game's hover on an activated pillar with the switch on: not observed,
  since the pillar was gone before it could be hovered.
- The draw order of Controller_obj's Draw GUI and the tooltips' within a
  frame: Live 1 measured the depths (-12998 and -13001), and the order is
  inferred from them. For mouse play it does not matter: mouse leave is the
  only writer of `false`, once.
- Persistent rooms: the rule empties the shown set on a room change without
  writing, which assumes the zone's instances went with it. The zones
  measured were not persistent.
- Gamepad play: with a controller the pillar's Step rewrites `visible` every
  step from the player's distance, which may hide the box again before it is
  drawn. Not supported, and not tested (owner, 2026-10-11).
- The tooltip's starting `visible` (an object property, inferred false; Live
  1 and Live 2 read it false on every untouched pillar with the cursor away).
- Which item the activation dialog charges at the higher rarities.
- Which global flag Alarm 0 reads as local co-op; co-op as a whole.
- How the boxes look when several overlap, or over an open window: shipped as
  the game draws them.
- Whether anything outside the pillar's own events reaches its tooltip by a
  route the two static scans cannot see.
