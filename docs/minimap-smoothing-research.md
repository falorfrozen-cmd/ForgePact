# Minimap smoothing in performance mode — research log

Status (2026-09-16): **Phase 0 ran once; H = not observed.** Performance mode
(`minimapTurboPerformance`) stretches the interval between bursts of
`DrawMinimapDynamic` from 16 to 109 frames at 144 fps; `DrawMinimap` still runs
every frame. Each burst draws markers per object type straight from live
instances onto a surface. No stored per-marker position list was found, so the
positive control the decision rules require (a hold inside the refill) could not
run, and nothing is concluded. Nothing player-visible has shipped: there is no
`mmsmooth` command and no panel toggle. See § Results. Issue #19 was closed as
not planned on 2026-09-16 with this measurement: smoothing would need the
per-type marker layer redrawn every frame, which gives up what performance mode
saves (D4).

phase0-status: complete

## The issue

ForgePact issue #19, "[QoL] Map in performance mode interpolation": *"Make map
in performance mode lag 1 sec but interpolate between polls for
smooth/continuous looking update."*

In the game's performance mode the minimap's moving markers (player, monsters,
whatever else the game puts in that list) appear to update about once a second
and jump between positions. The request is to draw each marker gliding from its
previous reported position to its current one over the game's own update
interval, accepting that the minimap then lags the game by one update.

What the eventual mod may do is bounded by the repository's rules: change
marker positions inside the draw the game is already making (the "change one
value inside a call the game is already making" class), never make the game
refresh more often, never extrapolate, never write the player's minimap
options, and never touch the fog grid Map Reveal owns.

## Static search

Run 2026-09-16, before planning any live round, over `hs-game-sdk`'s
`scripts.hpp`, `objects.hpp`, `rooms.hpp`, `player.hpp`, `sprites.py`,
`sounds.py`, the toolkit's `docs/`, and the ForgePact, HS-Offline-Tracker and
hub sources.

**No name containing `perform`, `lowspec`, `potato`, `quality`, `gfxmode` or
`lite` exists anywhere in the SDK.** The game's named video/options scripts are:
Fullscreen, DisplayMode, WindowSize, AspectRatio, Vsync, ShowFPS,
CursorOutline, FPSOption / FPSOptionSelected / `UiUpOptionsVideoFps`, Outline,
ShowExperience, ColorBlindMode, CoopCameraMode, Font, ColorPickerMinimap /
`UiUpOptionsVideoColorMinimap`, LootFilter, ChatSettings, CenterDPSMeter,
CenterBossCooldowns, ControllerVibration, MoveStick. So "performance mode" is
one of: a global whose name only a live sweep can find, a data-driven options
row with no dedicated script, or what players call the FPS option. The
instrument answers this with a snapshot/diff of globals around toggling the
option, rather than by guessing a name.

**The minimap script family, complete** (short names as `HookOneScript` takes
them; SDK script indices in brackets):

- `DrawMinimap` (1259/1260), `DrawMinimapDynamic` (1261/1262)
- `MinimapRefresh` (2398), `MinimapChangeSize` (2399), `MMStamp` (2397)
- `PlayerUpdateMinimap` (2787/2788), `playerUpdateTimer` (2789/2790),
  `playerUpdateTimerLife` (2791)
- `s_MinimapPoint` (1004), `s_MinimapLine` (1003)
- `outline_start_minimap` (2649/2650)
- `ZoneStateParseMinimapDataSend` (4627), `ZoneStateParseMinimapDataReceive` (4628)
- five closures split out of `objMinimap`'s Create event:
  `anon@2958@gml_Object_objMinimap_Create_0`, `anon@6403@…`, `anon@7771@…`,
  `anon@8167@…`, `anon@11068@…` (5066–5070)
- `MinimapFuncs` (2395) and `PlayerUpdateMinimapFunc` (2787) are script-file
  containers, not routines.

The `s_` prefix is this game's struct-constructor naming (`s_LootExplosionData`
and `s_PromptMessage` are neighbours), so `s_MinimapPoint` is very likely the
per-marker record. **None of this is measured; it is a candidate list.**

**Objects:** `objMinimap` = 3244 (the instance Map Reveal already finds with
`asset_get_index("objMinimap")` + `instance_find`), `Minimap_Hide_obj` = 2772,
`Autotile_Minimap_Wall_obj` = 418, `Invisible_Wall_Not_Minimap_obj` = 2268.
Object event code (Step/Draw/Alarm) is not in the static table, and **session 7
measured it unreachable by name**: 22 raw `gml_Object_<Obj>_<Event>_<n>` names
tried through `GetNamedRoutinePointer` all returned "not found" (status 14)
(`pet-quest-collector-research.md`, "MEASURED 2026-09-10, session 7"). A later
paraphrase in `pet-quest-collector-c-research.md` said those names "resolved";
that sentence was wrong and is corrected in the same change as this one.
`mmprobe` still tries the 27 raw `gml_Object_objMinimap_<Event>_<n>` names,
because the result costs nothing and settles it for this object on this build,
**expecting every one to print not found**. So only the named-script rows can
count: if the per-frame draw or the throttled refresh is inline in
`objMinimap`'s event code, no `mmprobe` row sees it.

GameMaker's event numbering: `Step_0/1/2` (normal/begin/end), `Draw_0`, `Draw_64` (GUI), `Draw_72/73` (begin/end), `Draw_74/75` (GUI
begin/end), `Draw_76/77` (pre/post), `Alarm_0`…`Alarm_11`, `Create_0`,
`Destroy_0`, `CleanUp_0`, and the `Other` group's room start/end events. On
this game the Room Start event was measured as `Other` number 5
(`Controller_obj`'s, `S10-special-content-notes.md`), so the generic numbering
is not assumed for that group.

**Earlier findings that bear on this** (`map-reveal-research.md`):
`minimapShowMonsters` / `minimapShowEnvironment` are globals, both 1 by default
(§5); enemies carry no per-instance minimap flag (§5); enemy dots appear only
near the player because far packs do not exist yet (§10), so in a populated
zone the marker list can be large; `objMinimap` carries
`minimapDiscoveredGrid`, `minimapCellsX/Y` and `minimapRevealed` (§1–2). A local
read of `DrawMinimapDynamic`, `s_MinimapPoint`, `PlayerUpdateMinimap` and
`MinimapRefresh` was started on 2026-09-11 and abandoned as unnecessary for map
reveal (§10); **no conclusion was recorded**, and none is assumed here.

## Hypotheses Phase 0 decides between

| | Mechanism | Shipped design it implies | Required evidence (all of it) |
|---|---|---|---|
| **H1** | The game keeps a container of marker records refilled every N frames in performance mode, and something draws from it **every frame**. | **Design A**. In class. | (a) container found (R3); (b) `watch` on it, while walking, dominant change gap ≥ 4 frames, with the `Player_obj x` control changing on most frames in the same session; (c) frame-tick `mmprobe hold` on the player's own record with performance mode on: read-back matches, `overwritten` gaps ≈ the `watch` gap (the container is live), **and the player's marker is visibly displaced for most of the hold, snapping back once per refresh** (a displacement is itself the positive result; no further control); (d) a named-script row with dominant gap 1 whose calls track frames (R2) and a row at ≈ the `watch` gap (R1), or a single row doing both. |
| **H3** | The throttle is per marker source (e.g. `PlayerUpdateMinimap` gated by `playerUpdateTimer`); the draw reads the throttled writer's container every frame. | Design A variant (different R1). | As H1; (d) names the per-source writer as R1. |
| **H2** | The dynamic draw itself runs only every N frames, onto a surface blitted every frame; nothing reads positions between refreshes. | Design B → **D4: close the issue with the measurement.** | (a)–(b) as H1; (c) frame-tick `mmprobe hold` with performance mode on: read-back matches, `overwritten` gaps ≈ the `watch` gap, **yet the player's marker is never displaced**; **(c') positive control on the same record, field and route, in the same session, with performance mode on: `mmprobe hold … at <R1 row> post` (step 12) displaces the marker** — a displacement by the frame-tick hold with performance mode off (step 8) ties the record only to the mode-off draw, is recorded in R10, and does not satisfy (c'); **(c'') the draw-side store ledger (R11, built at steps 3 and 7) is complete and every entry other than R3 reads `excluded (held)` or `excluded (scalar)`** — the ledger fails closed: an entry that was not enumerated, not watched or not held, that no instrument in this build can read or write, or whose control failed is `not excluded`, never excluded by default; (e) supporting only, never sufficient and never required: a variable on `objMinimap` for which `cb surface_exists <id>` returns true. **A script row's gap is never sufficient on its own.** Without (c'), H = `not observed (hold: no displacement at <R1 row> post with performance mode on)`; without (c''), H = `not observed (draw-side store not excluded: <variable> - <reason>)`. A concluded H2 quotes the ledger's limit in H: `exclusion covers only objMinimap's variables and the Player_obj and global variables whose names contain minimap, marker, radar, icon or blip`, followed by `each enumeration is controlled by one known name, so a partial print that still contains that name is not excluded`. |
| **H0** | Performance mode does not throttle the minimap; the jump is the FPS cap or something else. | No mod. `BLOCKED` with the numbers. | (f) R8 found by `diff`, reverses on toggling back, with the `minimapShowMonsters` control passing; **(g) R10 = displaced with performance mode on — by the frame-tick hold (step 7) or by `hold … at <R1 row> post` (step 12) — on the same variable whose `watch` gaps form (h), with `gjson <R8 var>` printed just before that hold showing R8's on value** — this is what ties the watched container to the draw with performance mode on. A displacement with performance mode off (step 8) ties R3 only to the mode-off draw, is recorded in R10, and does not satisfy (g); (h) `watch` dominant gap on == off, with the `Player_obj x` control passing in **both** runs, each run printing at least two changes and `read failures=0`, and each run with ` list` exactly when R3 is a ds_list id (with ` list`, `watch` compares the size and the first 64 elements only, so R3's player record index must be below 64, else H = `not observed (watch: player record beyond the compared prefix)`) — a run with fewer than two changes has no dominant gap and equals nothing; each run preceded by `gjson <R8 var>` showing that run's mode (step 5: R8's on value, step 6: R8's off value), else H = `not observed (watch: mode not read before <on|off> run)`. **(i) V was recorded with both answers, each look preceded by `gjson <R8 var>` showing that look's mode (step 2), else H = `not observed (V: mode not read before <on|off> look)`, and does not read that the dots jump with performance mode on and move every frame with it off** — if it does, H = `not observed (V contradicts H0)`, because a container refreshed every frame in both modes does not exclude a surface redraw that performance mode throttles; if either answer is missing or `unknown`, H = `not observed (V not recorded)`. **`mmprobe show` alone is never sufficient.** A concluded H0 quotes its limit in H: `mode read from R8's option value; a setting the game applies later than the toggle is not excluded`. Without (g), H = `not observed (hold: the watched container was never shown to be the drawn one with performance mode on)`; without (h), H = `not observed (watch: no measurable cadence on <variable>)`. |
| **not observed** | Anything that meets none of the rows above: no container found; container found but not live; no hold form displaced the marker; no named-script row carries gap 1 (R2) or the refresh gap (R1); any control failed. | **No hypothesis is concluded.** Stage B step 13 returns `PLAN-DEFECT`. | Record every such R-field as `not observed (<which instrument, which control>)`, e.g. R2 = "not observed among named-script rows; `objMinimap` event names printed not found at step 9 (consistent with session 7)". Recorded follow-up (N5): a research-only detour on `objMinimap`'s event entry points taken from the runtime's object record, each validated with `AddrIsExecutableInModule` before `MmCreateHook` as `citrace nativetrace` does, with a positive control on the same layout read (an event of an object already proven to fire, e.g. an interactable's step event, which calls `CheckPlayerInteraction`) before any zero on `objMinimap` is believed. Also recorded: an H2 in which one routine both refills and draws admits no write point that displaces; the local Ghidra read (step 13) may *inform* the follow-up but never closes the issue by itself. Not built in this workorder. |

Human decisions recorded 2026-09-16 for the mod, should Phase 0 support one:
the gate is the **measured** refresh cadence (D1), default off (D2), every
marker kind in the container (D3), a one-period lag (D5), and the player's own
marker smoothed too (D6).

## The interpolation core (built, game-independent)

`MinimapSmoothManager` takes `{key, x, y}` samples on each refresh
(`OnRefresh`) and hands back positions per frame (`Interpolate`). Its behaviour
does not depend on any Phase 0 answer, so it is pinned now by
`tests/test_minimap_smooth_behavior.py` + `tests/minimap_smooth_harness.cpp`,
which compile the real header against a modelled refresh/draw loop:

- **Gate.** Engaged only once two refreshes are at least `kMinSmoothGapFrames`
  (4) apart. One refresh is not a cadence, so nothing is written before the
  second. A game refreshing every frame gets no writes at all.
- **Lag, not prediction.** `t = (frame - refreshFrame) / measuredPeriod`,
  clamped to [0, 1]: a marker reaches its reported position one period after
  the refresh and waits there if the next refresh is late. The period is
  clamped to [1, `kMaxPeriodFrames` = 600]; `kDefaultPeriodFrames` = 60 until
  measured.
- **No pop.** Each new glide starts from where markers were *displayed* when
  the refresh arrived, not from the previous snapshot. The harness showed why:
  rotating from the snapshot makes an early refresh jump every marker forward
  in one frame.
- **Appear / disappear.** A marker new in a refresh is shown where reported; a
  marker gone from it gets no position. Pairing uses the marker's key; when the
  only identity is the index (`keysAreIndices`), a count change means every
  marker is new.
- **Budget.** At most `kMaxSmoothedMarkers` (128) positions per frame; the
  excess is counted in `truncated`, never dropped silently.

Each target scenario was seen failing against a deliberately broken core
before being relied on; the observed failing line is recorded beside each
scenario in the harness.

## Instrument

`mmprobe`, research build only (`plugin_build\build.bat dev`), inside
`#ifndef FORGEPACT_RELEASE`, absent from `kPlayerCommands`. Driven with
`.\ForgePact\tools\ipc.ps1 "<line>"` (or `-Lines` to batch).

- `mmprobe hook [substr ...]` — resolves every candidate above by name
  (`GetNamedRoutinePointer`), takes the script's function pointer, refuses it
  unless it is executable code inside `Hero_Siege.exe`, and patches it with
  `MmCreateHook` (a native detour, as `citrace nativetrace` does — a
  script-table swap is blind to this build's direct calls). Targets: every
  named script listed above, the option-toggle witnesses
  `UiAOptionsVideoFPSOption`, `UiAOptionsVideoFPSOptionSelected`,
  `UiUpOptionsVideoFps`, `UiAOptionsVideoVsync`, `UiAOptionsVideo`,
  `UiAOptionsGameplay`, **`CheckPlayerInteraction` as the positive control**,
  and 27 raw `gml_Object_objMinimap_<Event>` names (Create, Destroy, CleanUp,
  Step 0–2, the eight Draw events, Alarm 0–11, and `Other_4`). **The 27 event
  rows are expected to print `not found … st=14`** (session 7, § Static
  search); a row that prints `detoured` contradicts that measurement and is a
  headline result. The `Other` event numbered 5 (Room Start on this game) is
  **not** a target: first, event names are not expected to resolve at all;
  second, hooking Room Start crashed the game during the special-content work,
  and `tests/test_est_force_behavior.py` keeps that name out of the plugin
  source. Prints one line per target (`detoured exe+0x…`, `not found`,
  `resolved to a code address, not a CScript; not detoured (record it)`,
  `resolved, but no script function`, `not code inside Hero_Siege.exe`,
  `MmCreateHook st=…`) and a total. The code-address refusal exists because a
  builtin's name resolves to a function address rather than a script record,
  and reading a script record's fields off code would dereference instruction
  bytes; no target is a builtin, so it is not expected to fire. Substrings
  restrict the set, so a crash can be bisected without a rebuild. Idempotent.
- `mmprobe show` — per installed target: `calls`, `since` (calls since the last
  show), `lastGap`, and the two commonest gaps between consecutive calls, in
  frames (`gap=60 x 9` reads "called once every 60 frames, 9 times"; `gap=0`
  means several calls in one frame, e.g. once per marker). Header line: frames
  since reset, `fps`, `fps_real`, `game_get_speed(gamespeed_fps)`. Last line:
  the control, `CheckPlayerInteraction: calls=… <- positive control; 0 here
  voids every row above`.
- `mmprobe reset`, `mmprobe verbose on|off` (first 3 calls per target: `argc`,
  the first four arguments and `self`, each with its kind). **Event rows log
  `self` only.** An object event is called with `self` and `other` and nothing
  else, so the argument count and argument array a script detour reads are, for
  an event, whatever the caller left in those registers; reading them would be
  an access violation that ends the session. The detour still forwards every
  slot unchanged, which is safe for both callee shapes.
- `mmprobe vars [Obj=objMinimap]` — every custom instance variable of the
  object's first instance with its kind, so an array, struct or ds handle
  stands out. It lists what `variable_instance_get_names` returns: a built-in
  such as `x` is never listed, and the count line is printed whether the
  builtin returned the whole list, part of it or nothing — so the print is
  controlled by a name already known on that object (§ Deciding the
  hypothesis, the ledger), never by its count.
- `mmprobe snap global|<Obj>` then `mmprobe diff` — every scalar member
  (real/int/bool/string) before and after; `diff` prints changed/added/removed
  (capped at 200 lines) and re-baselines, so toggling back and diffing again
  shows the change reversing.
  `snap global` enumerates the global instance's members directly, while
  `snap <Obj>` goes through the instance-variable names builtin — two different
  routes, so each needs its own control. For `snap global` the control is a
  known global changed from the game's menu (`minimapShowMonsters`). `diff` only
  sees scalars, so an empty R8 can only ever read "not observed among scalar
  globals".
- `mmprobe watch <Obj|global> <var> [frames=180] [list]` — samples one variable
  every frame from `FrameCallback` (arrays/structs compared by their full
  `json_stringify` text) and prints the frames it changed on and the gaps. This
  measures the refresh cadence **without any hook**: the cross-check for
  `show`. A frame whose read fails (no instance, a throw, an undefined value) is
  counted in `read failures` and skipped, never compared, so a zone change or a
  briefly absent minimap instance does not show up as change frames; the end
  line says so when it happened. A ds_list container is a number (its id), and
  comparing the id would report 0 changes on a list the game refills: with
  `list`, the variable must hold a live ds_list and its size plus its first 64
  elements are compared. Without `list`, a numeric variable prints a reminder
  to re-run with `list` if it is a ds_list id.
- `mmprobe hold <Obj|global> <var> <index> <field> <delta> [frames=180] [list] [at <label> [pre|post]]`
  and `mmprobe hold stop` — **the one subcommand that writes.** Each write
  takes element `index` of the container (an array, or a ds_list with `list`),
  decides what kind of thing it is, reads `field` (a member name for a struct,
  an index for an array; it must be a number), and writes that value plus
  `delta`. The kind is decided in this order, and no instance check is ever
  handed something it cannot take — `instance_exists` wants a number or a
  reference, and a GML type error inside a builtin is the runner's fatal
  dialog, which no C++ handler catches:
  1. an array takes the array route (an array cannot be an instance);
  2. a real, int32, int64 or reference (the kinds `HhResolveInstance` passes to
     the same builtin) goes through `instance_exists` and is refused either way
     — as a game instance, or as "not a struct or array";
  3. an object is asked the runtime's own `typeof`: `"struct"` takes the struct
     route, `"method"` is refused without the instance check, and anything else
     goes through `instance_exists` and is refused either way;
  4. every other kind (string, `undefined` — commonly a freed slot — pointer,
     bool) is refused with its description, **refused without any runtime
     call**.

  `typeof` decides the struct route. Whether this runner reports an
  instance-backed object as `"struct"` is **unmeasured for instance-backed
  objects** (earlier GameMaker runners did), so the first write prints what
  `typeof` said and the session records it in C; the write is bounded and
  restored either way. This is the toolkit's first `typeof` call; if it fails
  to resolve, the hold stops with an EXCEPTION line before writing anything.

  If the value read is not what the hold last wrote, the game rewrote the
  record since the last write: that write is counted as `overwritten` and the
  new value becomes the base. The first write is read straight back and
  printed at once as
  `route=<array|list>><struct|array> via <variable_struct_set|array_set>, typeof=<t>, at=<label pre|post|frame>, base=…, readback ok|MISMATCH (…)`
  (`typeof=n/a` on the array route). At the end (frames exhausted or
  `hold stop`) it writes the base back if the field still holds its own last
  write, and prints `frames= wrote= stuck= overwritten= readFailures= restored= at=`,
  the overwrite frames and gaps (a third measure of the refresh cadence), and
  the reminder below. **An abort after a write restores too** (a record
  replaced mid-hold, a throw): the stop line reads
  `stopped - <why> (wrote=W restored=<yes|no (…)>, at=…)`.

  **Where the write lands.** Without `at`, the hold writes from the same
  research frame tick as `watch` (nothing is added to the frame loop; one of
  `watch` / `hold` runs at a time). That tick runs at the **end** of the frame,
  so the write is what the next frame's draw sees only if the game does not
  refill the container first. With performance mode off the container is
  presumably refilled every frame, and if that refill runs before the draw it
  clobbers the write whether or not the draw reads the field — so a
  frame-end hold that displaces the marker is a positive result, and one that
  does not proves nothing. With `at <label> [pre|post]` (default `post`), the
  same bounded, restored write is made inside that script row's detour, before
  or after the game's own call, on every call of the row, and the frame tick
  only counts the hold down. The label must be a script row this session
  already detoured (`hook it first` otherwise); event rows are never write
  points. `at <R1 row> post` lands after the game's own refill and before
  whatever draws next, placed by the game's call order rather than by frame
  order, so it is the positive control on record, field and route: the
  player's marker displaced for the whole hold means the draw, in the
  performance mode set during the hold, depends on this record, field and
  route (not that it reads this container directly; see § Deciding the
  hypothesis). Post-original on the refill row reads a fresh
  value on every call, so `overwritten` ≈ `wrote` there is expected. A
  reentrancy guard keeps a builtin called from inside the write from writing
  twice. A hold that is never displaced proves nothing about the draw unless
  the same record, field and route was displaced through some write point.
- `mmprobe dslist <id> [n=10]` — size and first entries of a ds_list.

Every subcommand except `hold` is read-only.

Existing research commands the session also uses: `gnames <substr>`,
`inames <Obj> <substr>`, `oget`, `oset`, `ojson <Obj> <var>` (writes
`bp_ipc\ojson_<Obj>_<var>.json`), `cb <builtin> [args]`, `perf` / `perf reset`.

## Live procedure

Research build, one round, about 45 minutes.

The order is deliberate: everything that does not depend on a hook runs
first, so a crash in step 9 cannot cost the hook-free evidence, and nothing
about H0/H1/H2 is concluded until step 14. Every step names its control. A
zero, a "no change" or a "not displaced" is written down as **not observed**
unless that step's control passed in this session. A hold that never
displaced the marker proves nothing about the draw unless the same record,
field and route displaced it through some write point (step 12, with
performance mode on, for H2; step 7 or 12, with performance mode on, for H0).
An exclusion is a measurement too: a store that was not enumerated, measured
or written is `not excluded`, never excluded by default.

Walk continuously (in a small circle near monsters) during every `watch`,
`hold` and `show` measurement: a standing player and idle monsters produce no
marker changes, which looks exactly like a slow cadence. Do not change zones
during a measurement.

1. **Boot, and look.** `plugin_build\build.bat dev`; copy
   `plugin_build\BloodPactPlugin_rel.dll` over
   `<game>\bin\mods\aurie\BloodPactPlugin.dll` (do **not** touch
   `modfiles_shipped\`). Load a character into an outdoor zone with monsters
   and at least one interactable nearby, with performance mode **on**.
   `.\ForgePact\tools\ipc.ps1 -Tail 20` must show the boot line with the
   current version. `ipc.ps1 "perf reset"`. Then, by eye, walking near
   monsters for ten seconds: do the minimap dots (yours and the monsters')
   jump about once a second? Toggle performance mode off: do they move every
   frame? Toggle it back on. Record both answers as a first impression of **V** (`yes` or `no` each; step 2 re-takes V with the mode read;
   H0 cannot be concluded while either is missing). If the jump is not
   seen with performance mode on, record `symptom: not reproduced` in V and
   continue; step 14 then routes to a human regardless of H.
2. **Snapshot control, then performance mode (hook-free).**
   *Control first:* `mmprobe snap global`; in the game's options toggle the
   minimap "show monsters" setting; `mmprobe diff` must list
   `minimapShowMonsters` changing; toggle it back; `diff` must show it
   reverse. If the menu has no such option, or `diff` does not show it,
   record "snapshot route uncontrolled" in C and treat any R8 result as not
   observed.
   *Then:* `mmprobe snap global`; toggle only the performance-mode setting;
   `mmprobe diff` → **R8**; toggle it back on; `diff` must reverse. If the
   control passed and nothing changed, R8 = `not observed among scalar
   globals`. Also run
   `ipc.ps1 -Lines "gnames perf","gnames Perf","gnames mode","gnames quality","gnames minimap","gnames Minimap","gnames fps"`
   and paste the output into the doc.
   *V, with the mode read:* if R8 was named, repeat step 1's look (on, off, back on) with `gjson <R8 var>` pasted before each look, which must show that look's mode (on, off, on; a look whose reading shows the other mode, or has none, is not an answer), and record those answers as **V**; step 1's answers stay in the doc as a first impression only. If R8 was not named, V stays as step 1 recorded it and H0 cannot be concluded anyway (it needs (f)).
3. **Find the container (hook-free).** `mmprobe vars objMinimap` and
   `mmprobe vars Player_obj` (read by eye for minimap/mm/marker/point/dot
   names). Then
   `ipc.ps1 -Lines "gnames minimap","gnames marker","gnames radar","gnames icon","gnames blip"`,
   and `gjson <name>` for every name printed. *Enumeration control (§
   Deciding the hypothesis, the ledger):*
   `oget objMinimap minimapDiscoveredGrid`, `iget equippedItems`,
   `iget inventory`, `gjson minimapShowMonsters`,
   `cb asset_get_index objMinimap`, `cb asset_get_index Player_obj`, then
   `cb instance_number <index>` for each.
   The `vars objMinimap` print must contain `minimapDiscoveredGrid` and
   `minimapCellsX`; the `vars Player_obj` print must contain whichever of
   `equippedItems` / `inventory` `iget` found; `gnames minimap` must contain
   `minimapShowMonsters`; every count line must match the lines pasted;
   `objMinimap`'s instance count must be 1. Otherwise R11 =
   `not excluded (enumeration uncontrolled: <command>)` and the ledger is
   not filled at step 7. Paste all of it into the doc:
   every `objMinimap` variable, and every `Player_obj` variable and global
   whose name contains minimap, marker, radar, icon or blip, is an entry of
   the draw-side store ledger (**R11**, § Deciding the hypothesis), in the
   order printed and with the kind printed. For each array/struct/ds-handle candidate:
   `ojson objMinimap <var>` (read `bp_ipc\ojson_objMinimap_<var>.json`) or
   `mmprobe dslist <id>`. Record provisional **R3**, **R4**, **R9**, and, in
   **each** candidate, which element is the **player's own record** (its x/y
   track `oget Player_obj x` / `y`, in whatever space R4 says). If a record carries more than one
   x-like field (world and minimap space), note both; step 7 holds one and,
   if it is not displaced, step 7 is repeated on the other. For every
   variable whose name contains `surf`, run `cb surface_exists <value>` and
   record the result (H2 supporting evidence (e); never sufficient).
4. **`watch` control (hook-free).** `mmprobe watch Player_obj x 120` while
   walking: it must report changes on most frames. If it does not, `watch`
   is blind this session: record it, and no hypothesis may use a `watch`
   number.
5. **Container cadence, performance mode ON (hook-free).**
   If step 2 named R8, `gjson <R8 var>` first and paste it; it must print R8's on value, else this run cannot serve H0's (h). Then
   `mmprobe watch objMinimap <R3 var> 300` (append ` list` if R3 is a ds_list
   id; use `global` instead of `objMinimap` if it is a global) → the `watch`
   part of **R6**. If more than one candidate from step 3, watch each.
6. **Container cadence, performance mode OFF (hook-free).** Toggle
   performance mode off; `gjson <R8 var>` before `mmprobe watch` in step 6: paste it, and it must print R8's off value, else this run cannot serve H0's (h) (the toggle did not take). Then repeat step 4's control and step 5's `watch` → the `watch`
   part of **R7**. Toggle performance mode back **on**.
7. **Is the container live, and does the draw read it? (hook-free, one
   write, performance mode ON.)** If step 2 named R8, `gjson <R8 var>` first
   and paste it: the mode this hold ran in is a reading, not a memory.
   Walking:
   `mmprobe hold objMinimap <R3 var> <player record index> <x field> <delta> 300`
   (` list` if a ds_list), with `<delta>` about a quarter of the minimap's
   width in the record's own units (R4). Read the first line: `readback ok`
   and the `route=` → **R5** (a `MISMATCH` means that route does not write;
   record it); the `typeof=` value → **C**. Watch the player's dot for the
   5 seconds: displaced for most of it and snapping back about once a
   second, never displaced, or `unsure` → the *on* part of **R10**
   (`unsure` satisfies nothing). The end line's
   `overwritten` gaps → the frame-tick `hold` part of **R6** (they must be
   ≈ the step 5 gap, else the container is not the one the game refreshes).
   If the element is refused (a game instance, a method, a non-numeric kind
   — the line says which, and says whether `instance_exists` was even
   called), record the line and try the neighbouring index once; if still
   refused, R5/R10 are not observed. If not displaced and step 3 noted a
   second x-like field, repeat this step on it. Then fill the draw-side store
   ledger (**R11**) in the order step 3 printed it,
   stopping at the first `not excluded`:
   for an array or ds_list entry other than R3, repeat this step on it (its
   own player record index and every x field, same `<delta>` scale); for a
   scalar entry, `mmprobe watch <Obj|global> <var> 300`; (an entry whose
   printed value is a non-negative integer is
   `not excluded (possible handle)` without a watch — nothing a watch
   prints can exclude it);
   record each entry's
   status exactly as § Deciding the hypothesis defines it. An entry this build
   cannot watch or hold is `not excluded`, never skipped.
   (`hold` decides the
   route by kind: an array takes the array route; a `VALUE_OBJECT` is asked `typeof`
   and only `"struct"` takes the struct route, `"method"` is refused
   outright; a number or reference, or an object `typeof` calls something
   else, goes through `instance_exists` and is refused either way; every
   other kind is refused without any runtime call. Whether this runner
   reports an instance-backed object as `"struct"` is unmeasured — that is
   why the line prints `typeof=`.)
8. **The same hold with performance mode OFF (hook-free).** Toggle
   performance mode off (if R8 is known, `gjson <R8 var>` must now print the
   off value; paste it); run the step 7 command again with the **same
   arguments**; toggle back on. End line's `overwritten` gaps → the
   frame-tick `hold` part of **R7**; displaced or not → the *off* part of
   **R10**. Displaced here ties this record, field and route only to the draw
   with performance mode off: it is recorded in R10, never satisfies H2's (c'),
   and does not satisfy H0's (g). Not displaced here proves
   nothing: the write lands at the end of the frame, and a refill that runs
   before the next draw clobbers it whether or not the draw reads the
   field. *Then:* `ipc.ps1 "perf"` → the no-hooks frame-time baseline.
9. **Hook everything (first hook of the session).** `ipc.ps1 "mmprobe hook"`;
   paste every line into the doc. Expected: the 25 named-script rows
   `detoured`, the 27 `objMinimap_<Event>` rows `not found … st=14`
   (consistent with session 7). A `detoured` event row contradicts session
   7: record it prominently. If the game crashes, relaunch, redo step 1 only,
   and bisect with `mmprobe hook Draw`, `mmprobe hook Minimap`, `mmprobe hook
   objMinimap_`, one group per launch; record which group crashes. Steps 2–8
   need not be repeated.
10. **Hook cadence, performance mode ON.** `mmprobe reset`; walk 10 s;
    `mmprobe show`. *Control:* the last line's `CheckPlayerInteraction:
    calls=` must be > 0, else every row is void. Record every row's dominant
    gap. A row whose gap ≈ the `watch` gap is an **R1** candidate; a row with
    gap 1 whose `calls` ≈ frames since reset is an **R2** candidate; add R1's
    gap as the `show` part of **R6**. No row with gap 1 → R2 = `not observed
    among named-script rows; objMinimap event names printed not found at
    step 9 (consistent with session 7)`. `DrawMinimap` tracking frames is a
    hypothesis to record, not a control.
11. **Hook cadence, performance mode OFF.** Toggle off; `mmprobe reset`; walk
    10 s; `mmprobe show` → `show` part of **R7** (control as in 10). Toggle
    back on. Then `mmprobe verbose on`, `mmprobe reset`, and read the
    first-3-call lines for `s_MinimapPoint` and the R1 candidate
    (supplementary R4 evidence; paraphrase). `mmprobe verbose off`.
12. **The positive control for the draw: hold inside the refill row
    (performance mode ON).** If R8 is known, `gjson <R8 var>` first (paste
    it). For the R1 candidate from step 10 (each, if
    several), walking:
    `mmprobe hold objMinimap <R3 var> <player record index> <x field> <delta> 300 at <R1 row> post`
    (` list` before `at` if a ds_list). Read the first line: `at=<row> post`,
    `readback ok`. End line: `wrote` ≈ that row's calls over 300 frames,
    `overwritten` ≈ `wrote` (a post-refill write sees a fresh value on every
    call — expected), gaps → the `at` part of **R6**. The player's dot
    displaced for the whole hold → the *at* part of **R10** = `yes`: the draw
    with performance mode on depends on this record, field and route (H2's
    (c')), and step 7's answer now
    means something. Not displaced → `no`. No R1 candidate → `not run (no R1
    row)`. If step 10 also produced a gap-1 R2 candidate, run once more with
    `at <R2 row> pre`: displaced there is direct H1 evidence and confirms
    R2. `ipc.ps1 "perf"` → frame time with hooks.
13. **Optional local read.** If R3/R4 are still ambiguous, a **local**
    Ghidra read of the R1/R2 candidates (via `citrace symdump` +
    `tools/ghidra/ImportSymbols.java`) is permitted. Only a paraphrase of the
    structure goes into the doc — no script text, no listing, no screenshot,
    no decompiler symbol names. A local read informs R-fields; it never
    closes the issue by itself.
14. **Decide, using only `## Deciding the hypothesis`.** Fill every R-field,
    V, C, and H with the evidence letters met. Anything not met is `not
    observed (<instrument>, <control>)`. Set `phase0-status: complete`,
    append `phase0: complete` to this workorder's `## Log` with the date and
    the H value, restore the shipped DLL in `mods\aurie\`, and run
    `/workorder resume minimap-performance-mode-interpolation`. The driver
    re-enters the planner to activate Stage B against the table (a fill-in
    for H1/H3; `BLOCKED` for H2 per D4 and for H0, and for any session whose
    V reads `symptom: not reproduced`; `PLAN-DEFECT` with the N5 follow-up
    for not observed).

## Deciding the hypothesis

Conclude a hypothesis only when **every** piece of its required evidence was
observed in the session, with its control passing. The letters are what
§ Results' `H` row records.

| | Mechanism | Shipped design it implies | Required evidence (all of it) |
|---|---|---|---|
| **H1** | The game keeps a container of marker records refilled every N frames in performance mode, and something draws from it **every frame**. | **Design A**. In class. | (a) container found (R3); (b) `watch` on it, while walking, dominant change gap ≥ 4 frames, with the `Player_obj x` control changing on most frames in the same session; (c) frame-tick `mmprobe hold` on the player's own record with performance mode on: read-back matches, `overwritten` gaps ≈ the `watch` gap (the container is live), **and the player's marker is visibly displaced for most of the hold, snapping back once per refresh** (a displacement is itself the positive result; no further control); (d) a named-script row with dominant gap 1 whose calls track frames (R2) and a row at ≈ the `watch` gap (R1), or a single row doing both. |
| **H3** | The throttle is per marker source (e.g. `PlayerUpdateMinimap` gated by `playerUpdateTimer`); the draw reads the throttled writer's container every frame. | Design A variant (different R1). | As H1; (d) names the per-source writer as R1. |
| **H2** | The dynamic draw itself runs only every N frames, onto a surface blitted every frame; nothing reads positions between refreshes. | Design B → **D4: close the issue with the measurement.** | (a)–(b) as H1; (c) frame-tick `mmprobe hold` with performance mode on: read-back matches, `overwritten` gaps ≈ the `watch` gap, **yet the player's marker is never displaced**; **(c') positive control on the same record, field and route, in the same session, with performance mode on: `mmprobe hold … at <R1 row> post` (step 12) displaces the marker** — a displacement by the frame-tick hold with performance mode off (step 8) ties the record only to the mode-off draw, is recorded in R10, and does not satisfy (c'); **(c'') the draw-side store ledger (R11, built at steps 3 and 7) is complete and every entry other than R3 reads `excluded (held)` or `excluded (scalar)`** — the ledger fails closed: an entry that was not enumerated, not watched or not held, that no instrument in this build can read or write, or whose control failed is `not excluded`, never excluded by default; (e) supporting only, never sufficient and never required: a variable on `objMinimap` for which `cb surface_exists <id>` returns true. **A script row's gap is never sufficient on its own.** Without (c'), H = `not observed (hold: no displacement at <R1 row> post with performance mode on)`; without (c''), H = `not observed (draw-side store not excluded: <variable> - <reason>)`. A concluded H2 quotes the ledger's limit in H: `exclusion covers only objMinimap's variables and the Player_obj and global variables whose names contain minimap, marker, radar, icon or blip`, followed by `each enumeration is controlled by one known name, so a partial print that still contains that name is not excluded`. |
| **H0** | Performance mode does not throttle the minimap; the jump is the FPS cap or something else. | No mod. `BLOCKED` with the numbers. | (f) R8 found by `diff`, reverses on toggling back, with the `minimapShowMonsters` control passing; **(g) R10 = displaced with performance mode on — by the frame-tick hold (step 7) or by `hold … at <R1 row> post` (step 12) — on the same variable whose `watch` gaps form (h), with `gjson <R8 var>` printed just before that hold showing R8's on value** — this is what ties the watched container to the draw with performance mode on. A displacement with performance mode off (step 8) ties R3 only to the mode-off draw, is recorded in R10, and does not satisfy (g); (h) `watch` dominant gap on == off, with the `Player_obj x` control passing in **both** runs, each run printing at least two changes and `read failures=0`, and each run with ` list` exactly when R3 is a ds_list id (with ` list`, `watch` compares the size and the first 64 elements only, so R3's player record index must be below 64, else H = `not observed (watch: player record beyond the compared prefix)`) — a run with fewer than two changes has no dominant gap and equals nothing; each run preceded by `gjson <R8 var>` showing that run's mode (step 5: R8's on value, step 6: R8's off value), else H = `not observed (watch: mode not read before <on|off> run)`. **(i) V was recorded with both answers, each look preceded by `gjson <R8 var>` showing that look's mode (step 2), else H = `not observed (V: mode not read before <on|off> look)`, and does not read that the dots jump with performance mode on and move every frame with it off** — if it does, H = `not observed (V contradicts H0)`, because a container refreshed every frame in both modes does not exclude a surface redraw that performance mode throttles; if either answer is missing or `unknown`, H = `not observed (V not recorded)`. **`mmprobe show` alone is never sufficient.** A concluded H0 quotes its limit in H: `mode read from R8's option value; a setting the game applies later than the toggle is not excluded`. Without (g), H = `not observed (hold: the watched container was never shown to be the drawn one with performance mode on)`; without (h), H = `not observed (watch: no measurable cadence on <variable>)`. |
| **not observed** | Anything that meets none of the rows above: no container found; container found but not live; no hold form displaced the marker; no named-script row carries gap 1 (R2) or the refresh gap (R1); any control failed. | **No hypothesis is concluded.** Stage B step 13 returns `PLAN-DEFECT`. | Record every such R-field as `not observed (<which instrument, which control>)`, e.g. R2 = "not observed among named-script rows; `objMinimap` event names printed not found at step 9 (consistent with session 7)". Recorded follow-up (N5): a research-only detour on `objMinimap`'s event entry points taken from the runtime's object record, each validated with `AddrIsExecutableInModule` before `MmCreateHook` as `citrace nativetrace` does, with a positive control on the same layout read (an event of an object already proven to fire, e.g. an interactable's step event, which calls `CheckPlayerInteraction`) before any zero on `objMinimap` is believed. Also recorded: an H2 in which one routine both refills and draws admits no write point that displaces; the local Ghidra read (step 13) may *inform* the follow-up but never closes the issue by itself. Not built in this workorder. |

A script row's gap is never sufficient on its own for H2, and `mmprobe show`
alone is never sufficient for H0.

H2 and H0 both require the same record, field and route to have displaced the
marker through some write point in the same session; a hold that was never
displaced through any write point proves nothing about the draw.

H2's (c') is met only by the `at <R1 row> post` hold with performance mode on
(step 12); a displacement by the frame-tick hold with performance mode off
(step 8) ties the record only to the mode-off draw, is recorded in R10, and
never satisfies (c'). H2's (c'') is met only by a complete draw-side store
ledger in which every entry other than R3 is excluded by a positive
measurement; an entry that no instrument in this build can measure or write is
`not excluded`, and so is an entry that was never enumerated or never run.
Otherwise H2 = `not observed (draw-side store not excluded: <variable> -
<reason>)`.

### The draw-side store ledger (H2's (c''))

A copy of R3 made after the refill row returns, which the draw reads every
frame, would satisfy (c) and (c') exactly as H2 does. So H2 needs every store
such a copy could live in to be excluded, and an exclusion is a measurement
like any other. The ledger is enumerated at step 3, measured at step 7, and
recorded as R11.

**Entries: the enumeration, and its whole limit.** Every variable `mmprobe
vars objMinimap` prints; every variable `mmprobe vars Player_obj` prints whose
name contains, ignoring case, `minimap`, `marker`, `radar`, `icon` or `blip`;
and every global that `gnames minimap`, `gnames marker`, `gnames radar`,
`gnames icon` and `gnames blip` print, with its kind from `gjson <name>`.
Nothing else is enumerated: variables of any other object (a monster's own
cached position included), anything reachable only through a handle, a
reference or a method, and globals under any other name. A concluded H2
quotes that limit in H.

**The enumeration is controlled, or it is nothing.** `mmprobe vars` prints
what `variable_instance_get_names` returns for the object's first instance —
custom instance variables; a built-in such as `x` is never listed, and no
built-in is a container — and `gnames` what it returns for the global scope;
either prints a count and no error when that builtin returns nothing or part
of the list. So each enumerating command is checked in the same session
against a name it is already known to print, through a second builtin that
does not share its route: `mmprobe vars objMinimap` must print
`objMinimap.minimapDiscoveredGrid =` and `objMinimap.minimapCellsX =` (the fog
grid `MapRevealManager` clears and its width, `map-reveal-research.md`), and
`oget objMinimap minimapDiscoveredGrid` must print a kind; `mmprobe vars
Player_obj` must print `Player_obj.equippedItems =` or `Player_obj.inventory
=`, whichever of `iget equippedItems` / `iget inventory` prints a kind for
(both are `hs-game-sdk` `kRelicContainerFields`; `iget` goes through
`variable_instance_exists`); `gnames minimap` must print
`minimapShowMonsters` (`map-reveal-research.md` §5), and `gjson
minimapShowMonsters` must print a kind. The print must be whole: the
`objMinimap.` and `Player_obj.` lines pasted are as many as the `N variables`
count line says, each `gnames` call's names pasted are as many as its `shown`
count, and all five `gnames` calls print the same `/ N global` total. The
enumeration reads the first instance only, so `cb asset_get_index objMinimap`
then `cb instance_number <index>` must print exactly 1 (`cb instance_number
<Player_obj index>` ≥ 1 is that builtin's own control). Any control name
absent, `unknown object`, `no instance of`, `EXCEPTION`, `0 variables`, `0 / 0
global`, `(does not exist)`, `(player has no such var)` for both names, a
count that does not match the lines pasted, or an `objMinimap` instance count
other than 1 → R11 = `not excluded (enumeration uncontrolled: <command>)`,
and no entry is excluded. A single entry printed `<read failed>`,
`<describe-failed>` or `kind=<n>` is `not excluded (unreadable)`.

**Statuses: exactly one per entry, and only two of them exclude.**

- `excluded (held)`: the entry is an array, or a ds_list id held with ` list`;
  step 3 identified the player's own record in it and every field of that
  record that tracks the player's x; each such field was held at step 7 with
  performance mode on; every one of those holds printed `readback ok`, and its
  end line shows `wrote` > 0, `stuck` > 0, `readFailures=0` and no
  `overwritten` gap below 4; and the player's marker was never displaced — a
  negative by eye, whose control is (c')'s displacement seen in the same
  session at the same `<delta>` scale; `unsure` is not `never displaced`.
- `excluded (scalar)`: `mmprobe vars` or `gjson` printed `real:`, `int32:`,
  `int64:`, `bool:` or `string:` for it (`undefined` and `null` are not
  excludable: `watch` counts an undefined read as a read failure, so `read
  failures=0` is unreachable for it, and a null read is unmeasured on this
  runner); **neither the value `vars`/`gjson` printed nor the watch's
  `first:` nor its `last:` is a non-negative integer** — an `int32:` or `int64:` with no minus sign, or a `real:` with no
  minus sign and only zeros after the decimal point,
  which is what every ds, buffer, surface, sprite and instance handle looks
  like, and `watch` compares the number, not what it names; `mmprobe watch`
  on it for 300 frames with performance mode on, while walking, printed `read
  failures=0` in a session whose `Player_obj x` control passed; and either it
  changed on most frames with dominant gap 1 (a value rewritten every frame is
  not held between refreshes), or it printed `0 changes`.
- `not excluded (<reason>)`: everything else. Among them: a struct container
  (`object/struct`; `hold` writes into array and ds_list containers only);
  any entry whose printed value, or whose watch `first:` or `last:`, is a
  non-negative integer, changed or not — `not excluded (possible handle)`: a
  ds_grid, ds_map, ds_list or buffer id is one (`objMinimap.minimapDiscoveredGrid`
  is a ds_grid), a front/back pair of ds_list ids swapping every frame changes
  with gap 1 and is still two handles, and a 0/1 option flag such as
  `minimapShowMonsters` is indistinguishable from a handle by its value;
  `undefined` or `null`; an entry printed `<read failed>` (`not excluded
  (unreadable)`); a by-eye reading recorded `unsure`; every entry at once
  when an enumeration control failed (`not excluded (enumeration
  uncontrolled: <command>)`); a scalar whose dominant gap
  is anything but 1; `ptr`, a reference, a method or any `kind=<n>`; a player
  record or x field that step 3 could not identify; a hold that was refused,
  printed `MISMATCH`, had a read failure, an `overwritten` gap below 4 or
  `stuck=0`, or displaced the marker (record that displacement in R10 too: it
  is H1 evidence on that container); a watch or hold that was not run; a
  failed control.

The ledger may stop at the first `not excluded` entry. **Expected before the
session, and in every session of this build:** `objMinimap` carries
`minimapDiscoveredGrid` (a ds_grid), and the enumeration carries 0/1 option
flags such as `minimapShowMonsters`, so the ledger is expected to stop at
`not excluded` and H2 to be `not observed` in this session. It will in every
session on this build: the ledger cannot exclude a handle-shaped value, and
nothing in this build reads a ds_grid, a ds_map or a buffer, or observes the
draw itself. That is the rule working, not a defect: H2 closes the issue
(D4), so it waits for an instrument that observes the draw directly, rather
than treating what this build cannot read as absent. The ledger's job on this
build is to record, fail-closed, which store blocked it.
Recorded follow-up (not built): count per-frame calls of the draw builtins the
minimap uses, through an instrument with its own positive control, while
`minimapShowMonsters` is toggled; markers drawn every frame change that count
every frame, markers drawn to a surface only once per refresh.

H2 closes the issue (D4), so it needs positive evidence that nothing draws
positions between refreshes: a named script whose gap is about one refresh is
just as consistent with "that script is the refresh, and the per-frame draw is
inline event code", which no row can see. H0 closes it too, so it needs the
hook-free `watch` cadence to match with performance mode on and off, with the
control passing both times; two blind rows would also read the same on and off.

An undisplaced hold is guaranteed under **every** hypothesis whenever the held
field is not the one drawn — a world-space `x` beside a minimap-space one, a
sibling container the draw actually reads, a value copied into a draw-side list
at refresh time. That is why H2's (c) needs (c'), and why H0's (g) needs R10
displaced on the same variable whose `watch` gaps form (h): without it, the
watched container is tied to nothing the game draws. `surface_exists` on some
`objMinimap` variable ties nothing either (fog and background surfaces are
near-universal), so it is supporting only.

Why the frame-end hold with performance mode off is not H2's control: that
hold writes at `HkPresent`, the end of the frame. With performance mode off
the game presumably refills the container every frame; if the refill runs in a
step or alarm event, or inside the draw before it draws, the next frame's
refill clobbers the write before the draw reads it, and the marker is never
displaced even though the draw reads exactly that field. Only a refill that
runs after the draw leaves the frame-end write visible. So that hold is a
valid control when it displaces and inconclusive when it does not. Even when
it displaces, it ties the record only to the mode-off draw, so it never
satisfies H2's (c'). `mmprobe hold … at <R1 row> post` performs the same write
inside the refill row, after the game's own refill: under H1 (draw every
frame) and H2 (draw at refresh time, to a surface) alike, a marker drawn
displaced for the whole period shows that the draw depends on this record,
field and route. It does not show that the draw reads this container
directly: a copy the game makes after the refill row returns carries the
displaced value too, and such a copy is excluded only by H2's (c'') rule. It
is placed by the game's call order, not by frame order.

The limit of that control, recorded before the session: an H2 in which **one**
routine both refills the container and draws it admits no write point that
displaces — pre-original is clobbered by the refill, post-original lands after
the draw. Then (c') cannot be met, H is recorded as `not observed (hold: no
displacement through any write point)`, and the workorder routes to
`PLAN-DEFECT` with the follow-up below. The issue is not closed: a verdict that
closes it must rest on a positive observation.

### If nothing is observed

No hypothesis is concluded, Stage B does not start, and the workorder routes to
`PLAN-DEFECT` naming this follow-up. The recorded next instrument (not built):
a research-build-only native detour on `objMinimap`'s event entry points, taken
from the runtime's object record rather than by name (session 7 showed names do
not reach event code on three other objects, and step 9 measures `objMinimap`'s
own). Each entry point is validated with
`AddrIsExecutableInModule` before `MmCreateHook`, as `citrace nativetrace`
does. Reading the entry points is a struct-layout read, so it needs a positive
control on the same layout read first: an event of an object already proven to
fire (for example an interactable's step event, which calls
`CheckPlayerInteraction`) must count calls through the same route before any
zero on `objMinimap` is believed.

This includes the refill-and-draw-in-one-routine case above: no hold form can
displace there, and the event-entry detour is still the next instrument. The
local Ghidra read (step 13) may inform that follow-up, but it never closes the
issue by itself.

## Results

Game build / date of the session: Steam install as of 2026-09-16 (build number not read); ForgePact research dev build v1.3.20 on branch feat/minimap-smoothing-research; game fps setting 144.

| Field | Meaning | Value |
|---|---|---|
| R1 | Routine that (re)fills the marker container: a named-script row whose dominant gap ≈ the `watch` gap, or `not observed among named-script rows` | `not observed` as a container refill (no container, R3). Throttled routine observed: `DrawMinimapDynamic` runs in bursts, dominant gap 109 frames with performance mode on (13 bursts in 1530 frames) and 16 frames with it off (97 bursts in 1560 frames). It is a candidate for the throttled draw, not for a refill. `MinimapRefresh` made 0 calls in both runs. |
| R2 | Routine that draws markers from the container each frame: a named-script row with dominant gap 1, or `not observed among named-script rows; objMinimap event names printed not found at step 9 (consistent with session 7)` | `DrawMinimap`: gap 1 every frame (1530 calls in 1530 frames on, 1560 in 1560 off), and `outline_start_minimap` likewise. Whether it draws markers from a container or blits a surface is not observed; no container exists to draw from (R3). `objMinimap` event names and its 5 anonymous `Create_0` closures printed not found `st=14` at step 9 (consistent with session 7). |
| R3 | Where the container lives (`objMinimap.<var>` / `global.<var>`) and its kind (array / ds_list id / struct) | `not observed`. `objMinimap`'s 50 custom variables contain no array, ds_list or struct holding marker records. Non-scalars: `minimapDiscoveredGrid` (ds_grid), `mapPathfindGrid` (ds_grid), surfaces `minimapDynamicSurface`, `minimapMaskSurface`, `minimapInitSurface`, `minimapFinalSurface`, `minimapSurface` (all `surface_exists` = 1), and five method structs. No `Player_obj` variable or global whose name contains minimap/marker/radar/icon/blip is a container. |
| R4 | Per-marker record shape: field names holding x and y (world or minimap space — say which) and any identity/sprite/colour fields | `not observed` (no record). Supplementary (step 11, paraphrased): each `DrawMinimapDynamic` call takes 8 arguments; the first is an object asset (for example a quest object parent or a multiplayer-dead object), then an icon size (4 or 0) and the minimap cache scale twice (0.23925, equal to `objMinimap.minimapCacheScaleX/Y`). About 36-39 calls happen in one frame per burst, one per object type, so positions appear to be read from live instances during the burst. |
| R5 | Write route that `mmprobe hold` printed with `readback ok` (e.g. `array>struct via variable_struct_set`) | `not run (no container to hold, R3)` |
| R6 | Refresh cadence **with performance mode on**, in frames: `watch` gap (hook-free, primary), frame-tick `hold` overwrite gap (step 7), `hold … at` overwrite gap (step 12), and `show` gap of R1 — all four, or which were not observed | `watch`: not observed (no container; the counters `objMinimap.minimapUpdate` and `Player_obj.updateMinimap` change every frame, and the flags `minimapFinalDirty` and `minimapDrawReady` showed 0 changes in 300 frames, so the frame-end `watch` cannot see this cadence). Frame-tick `hold`: not run. `hold … at`: not run. `show`: `DrawMinimapDynamic` bursts every 109 frames (gjson `minimapTurboPerformance` = 1 before the run). |
| R7 | Same with performance mode **off**: `watch` gap, frame-tick `hold` overwrite gap (step 8), `show` gap of R1 | `watch`: not run (no container). Frame-tick `hold`: not run. `show`: `DrawMinimapDynamic` bursts every 16 frames (gjson = 0 before the run). `PlayerUpdateMinimap` and `playerUpdateTimerLife` run every 16 frames and `playerUpdateTimer` every 144 frames in **both** modes. |
| R8 | The performance-mode variable (name, on/off values) from `snap global`/`diff`, or `not observed among scalar globals` (with the `minimapShowMonsters` control result) | global `minimapTurboPerformance`: on = 1, off = 0. `diff` showed 1->0 on switching off (no other setting changed) and 0->1 on switching back on. The `minimapShowMonsters` control passed in both directions. The saved settings string carries it as `minimap_turboperformance`. |
| R9 | Identity a marker keeps across refreshes (instance id field / none) | `not observed` (no record) |
| R10 | Does the draw read this record's field: the player's own marker displaced by the frame-tick `hold` with performance mode on (step 7) / off (step 8) / by `hold … at <R1 row> post` (step 12) — `yes` / `no` / `unsure` / `not run (<why>)` for each (`unsure` and `not run` satisfy nothing), with the `gjson <R8 var>` value read before each when R8 is known; plus R2's `show` gap | step 7: `not run (no container)`; step 8: `not run (no container)`; step 12: `not run (no R3 variable to hold, although the R1 candidate row exists)`. R2's `show` gap: 1. |
| R11 | Draw-side store ledger (steps 3 and 7, H2's (c'')): every entry in the order printed, each `excluded (held)` / `excluded (scalar)` / `not excluded (<reason>)` (or the whole ledger `not excluded (enumeration uncontrolled: <command>)`), the enumeration controls' output (the control names found, the count lines, the `instance_number` values), the entry it stopped at, and the enumeration limit | `not excluded (enumeration uncontrolled: mmprobe vars Player_obj)`. Neither `equippedItems` nor `inventory` exists on this player (`iget`: player has no such var). The other controls passed: `vars objMinimap` printed 50 names including `minimapDiscoveredGrid` and `minimapCellsX`; `gnames minimap` printed 22 including `minimapShowMonsters`; `instance_number` = 1 for `objMinimap` (index 3244) and for `Player_obj` (3553). `cb asset_get_index` returns a reference, not a number, so the SDK indices were used. `vars Player_obj` printed 116 names. The ledger was not filled. |
| V | By eye (step 2, each look preceded by `gjson <R8 var>` showing that look's mode, else that answer is missing; step 1's look is a first impression only): with performance mode on the dots jump about once a second (yes/no); with it off they move every frame (yes/no). Never sufficient on its own. | Taken at step 2 with `gjson minimapTurboPerformance` read before each look: on (1) -> dots jump about once a second: **yes**; off (0) -> dots move every frame: **yes**; on again (1) -> jump: **yes**. At step 1 the tester also switched performance mode while doing the control, so step 1's first impression is not used. |
| C | Controls: `CheckPlayerInteraction` calls (> 0), `Player_obj x` watch changes (on and off), `minimapShowMonsters` diff seen (yes/no), the `typeof=` the hold printed for the player's record | `CheckPlayerInteraction` calls = 9481 (mode on) and 9360 (mode off); `Player_obj x` watch = 119 changes in 120 frames, gap 1, read failures=0 (a mode-off `x` watch was not run, because the mode-off container watch did not apply); `minimapShowMonsters` diff seen = yes, both directions; hold `typeof=`: not run. |
| H | Hypothesis concluded (H1 / H3 / H2 / H0 / not observed) and the evidence letters (a)–(i), including (c') and (c''); a concluded H2 quotes R11's enumeration limit | **not observed.** (a) fails: no container. H1/H3: (a)-(c) not observed. H2: (a)-(c) not observed, (c') not run and (c'') uncontrolled; supporting (e) seen (`objMinimap.minimapDynamicSurface` exists). H0: (f) met, but (g) and (h) are not observed and (i) is contradicted by V (`not observed (V contradicts H0)`). Stage B step 13 -> `PLAN-DEFECT`. |

Free text to record here: the `gnames` output from step 2, every
`mmprobe hook` line from step 9 (which `objMinimap` event rows, if any,
printed `detoured` rather than `not found`), the `hold` first and end lines,
the first-3-call argument lines (paraphrased), and the `perf` frame averages
with and without the hooks.

**Session 2026-09-16, free text.**

- Step 2 `gnames`: `perf` and `Perf` -> `minimapTurboPerformance` only;
  `quality` -> none; `fps` -> `showFPS` and four options UI scripts; `mode` -> 22
  names (display, colour-blind and co-op camera options, chat moderation UI);
  `minimap` -> 22 names (the setting globals `minimapAlpha`,
  `minimapEnemyIconScale`, `minimapLootIconScale`, `minimapObjectIconScale`,
  `minimapScale`, `minimapShowEnvironment`, `minimapShowMonsters`,
  `minimapTurboPerformance`, `minimapBlockedTint` and `colorMinimap`, plus the
  scripts from § Static search).
- Step 9 `mmprobe hook`: 20 detoured (every named-script row in the table plus
  `CheckPlayerInteraction`), 32 not found `st=14`: all 27 `objMinimap_<Event>`
  rows and all 5 `anon@…@gml_Object_objMinimap_Create_0` closures. No event row
  printed `detoured`. No crash.
- Step 10 (on) / step 11 (off), calls in about 1530 frames: `DrawMinimap`
  1530 / 1560; `DrawMinimapDynamic` 504 (bursts, gap 109) / 3528 (bursts,
  gap 16); `MMStamp` 6182 / 6654; `PlayerUpdateMinimap` 98 / 99 (gap 16);
  `playerUpdateTimer` 11 / 11 (gap 144); every other row 0 in both.
- Step 11 verbose (paraphrased): `MMStamp` takes one surface argument;
  `PlayerUpdateMinimap` takes one argument that is undefined;
  `DrawMinimapDynamic` as in R4.
- `perf` with hooks installed: 7.05 ms average frame interval over the
  session (the maximum reflects menu and zone pauses). A run without hooks was
  not taken.
- **What the numbers imply for a mod, not a conclusion under the rules:**
  performance mode saves work by drawing the per-type marker layer less
  often. Showing markers moving continuously would mean running that layer
  every frame, with interpolated positions: about 16 times the mode-off rate
  of the layer, which is the Design B trade-off that D4 describes.
  Interpolating a stored list (Design A) has nothing to act on, because no
  such list was observed.
