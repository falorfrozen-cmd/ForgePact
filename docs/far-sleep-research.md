# Far scenery sleep - research record

`farsleep` (Mods → Quality of Life → **Far scenery sleep**, off by default)
puts a zone's far scenery props to sleep with the runner's own
`instance_deactivate_object` and wakes them with `instance_activate_object`
before a player comes near. This file records why, what was measured, the
rules the class keeps, and what is not known. Everything below is measured
behaviour or our own code; no game script is quoted.

## Why: the runner walks every active instance, many times a frame

`frameprof` (docs/frame-profiler.md) in Act_01_01 showed the GameMaker
runtime itself, not the game's scripts, as the largest share of the frame
thread's work: 33% of the frame at density 1x, 57% at 5x. Reading the hot
runtime functions (Sep-17 build, in Ghidra, locally; nothing committed)
explained it:

- **The draw passes** walk every layer of the room and every element of every
  layer once per draw sub-event, and look up each instance's event table as
  they go. Instances the game hides (`visible = false`) are still walked.
- **The alarm pass** walks every instance of every object that owns an alarm
  event, every frame; the prop parent objects own one.
- **The step upkeep** (previous positions, motion, layer upkeep) walks every
  active instance.

Every room also carries 5,501 layers named `Game_Layer_0` .. `Game_Layer_5500`
(depth 0 to -5500): `SetupRoomLayers` makes them and `UpdateDepth` moves an
instance onto `room.gameLayer[i]` by its position, so they cannot be removed.

A deactivated instance drops out of all three walks: the runner takes it off
the active and object lists at its next flush and moves its layer element
behind the active ones.

## The census: what fills a zone

`zonecensus [radius]` (research build) writes every active instance's object,
parent chain, visibility, layer and distance from the player to
`bp_ipc\zonecensus-<room>-<time>.json`. Act_01_01, 2026-09-28:

| | instances |
| --- | --- |
| all active | 6,140 - 7,254 (layouts vary) |
| within 1,500 px of the player | 389 - 446 |
| `Visual_Parent_obj` family (bushes, gunpowder, rocks, leaves, ravens, flames...) | 2,427 |
| `Destructible_NoCollision_Parent_obj` (hay) | 900 |
| `Collision_Prop_obj` family (trees, rocks, fences, tombstones, boxes, urns...) | 1,798 |
| `Invisible_Wall_obj` | 668 |
| monsters, spawners, their shadows and health bars | about 900 |

Act_02_01 has the same families with its own props (`Winter_*`, 2,328 of
3,440). The game already hides far props (`visible = false`), but hidden is
not asleep: all of them stay in the walks above.

## What sleeping the far props saves

Research build, Act_01_01, player standing still at the zone start, 60 fps
cap, `frameprof` captures of 15-20 s. "Work" is the share of the frame
thread's time not spent in the frame limiter.

| state | work (each capture) |
| --- | --- |
| off | 52.8%, 59.4%, 54.3%, 56.3% (alternating A/B, 13:01-13:05) |
| on | 45.0%, 45.6% (the same session, after each scan) |
| lab, one-shot sleep of 4,282 props at 2,500 px | 61.4 / 62.3% off → 48.9 / 46.2% on |

About 10 points of a 60 fps frame, 1.6-2.4 ms. The saving is in the runtime
passes: the layer-walk overhead fell about 2-5 points and the step upkeep about
6 points (the alarm pass alone 5 → 2). At 60 fps with frame time to spare this
shows as idle time; where a frame is over budget it is frame time.

**Density 5x** (`density 5` before entering, 8,100-8,300 instances, 4,204
props asleep; the same session, alternating, 13:48-13:51). The first two
captures were taken while packs fought the player, the last three after it
calmed down, so compare neighbours:

| capture | fps | frame-thread work | GameMaker runtime | step upkeep |
| --- | --- | --- | --- | --- |
| off, fighting | 52.5 | 99.9% | 52.0% | 21.7% |
| on, fighting | 57.3 | 90.7% | 44.0% | 17.5% |
| off, calm | 59.8 | 77.5% | 42.2% | 21.3% |
| on, calm | 59.9 | 67.0% | 33.7% | 13.6% |
| off, calm | 59.9 | 69.4% | 39.3% | 20.9% |

The step upkeep falls by 4-8 points whenever the props sleep; with the frame
over budget that showed as 52.5 → 57.3 fps (the fight's own load changes
between captures, so read that pair as a direction, not a precise figure).
Monsters, their shadows and health bars are not scenery and stay in every
walk.

Tried and dropped: hiding the empty `Game_Layer_#` layers (4,321 of 5,501)
saved only about 1.5 points, and an instance moving onto a hidden layer would
not be drawn.

## What sleeps, and what never does

The class (`plugin/include/ForgePact/FarSleep.hpp`) reaches the game only
through `CallBuiltin`: no struct layouts, no game addresses.

- **Families:** leaf descendants of `Visual_Parent_obj`,
  `Destructible_NoCollision_Parent_obj` and `Collision_Prop_obj`, found once a
  session by walking the object table (`object_exists`, `object_get_parent`).
  Leaves only: a parent's `instance_number` includes its children.
- **Never:** descendants of `Shrine_Parent_obj`, `Special_Dungeon_Parent_obj`
  (dungeon entrances), `Pile_Parent_obj`, `Chest_Parent_obj`,
  `Quest_Object_Parent_obj`, `Enemy_Parent_obj`, `Wall_Parent_obj`,
  `Block_obj`; any `Trap_*` object; any object that, or whose parent up to the
  family, owns a Step, Begin/End Step or Draw GUI event (read once from the
  compiled-code table's row names: code that runs every frame is doing
  something).
- **Rooms left alone:** towns (`Town*`), menus (`*Menu*`), developer rooms
  (`Dev_*`: HS-AFK-Expedition's Stronghold arena lives in `Dev_10_rm`) and
  persistent rooms (all measured zones are non-persistent).

## Waking and sleeping

- **Radii.** Wake radius = half the camera view's diagonal + 1,000 px (1,705 px
  at the 1229x691 view measured in the game); sleep radius = wake + 600 px, so a
  player stepping back and forth does not flip props. Solid props
  (`Collision_Prop_obj`) also stay awake as far out as ForgePact keeps monsters
  hunting (the Beacon's wake radius, 4,000 px, plus 400) while a hunt is on, so
  a moving monster never meets a sleeping fence; a hunt over the whole map
  keeps every solid prop awake.
- **When.** Nothing before a room has been unchanged for 3 s with a player in
  it and its instance count has held still between two looks (a fight that
  keeps it moving is waited out for 10 s at most). The zone is then scanned a
  slice a frame. A pass over the known props runs every 10 frames and costs
  runner calls only for props that change state; a player who jumps more than
  400 px (a teleport, a warp) starts a pass at once with a bigger budget. Every
  runner call counts against 400 a frame (2,400 after a jump or while waking
  everything).
- **Positions.** A prop that has been awake for more than a second has its
  position read again before it sleeps (the ravens fly).
- **What the game does meanwhile.** A slice of 32 known props is checked once
  a second: one the game woke itself is managed as awake again, one that was
  destroyed is dropped. One top-up scan 30 s after the first adds props the
  zone made later. A restart under the same room name (the objMinimap instance
  and its grid are new) forgets the zone and scans again.
- **Off.** Switching off wakes everything it put to sleep, 2,400 calls a frame,
  then stops asking the game anything.

## Verified

- `tests/test_far_sleep_behavior.py` runs the real class against a controlled
  runner: 37 scenarios (bounded calls, only scenery, settling, walking, jumps,
  two players, the hunt radius, a raven that moved, the game waking props, the
  top-up, off, room changes, skipped rooms, restarts, a refusing runner).
- Live, 2026-09-28 (research build, Suh, Act_01_01): after every census the
  props within 1,500 px were all active and identical to the all-awake census
  at the same spot; a warp started an urgent pass; `farsleep 0` woke 4,441 of
  4,441; town was skipped; a second visit was scanned on its own.
- **A room that ends destroys its sleeping props like the awake ones.**
  `evcount` (research build) counted the Clean Up events of three prop
  families on leaving Act_01_01 for town: 981 / 1,702 / 159 with every prop
  awake, and the same 981 / 1,702 / 159 with 4,236 of them asleep. Nothing the
  props hold (lists, sounds, extension variables) is left behind.

## Not known

- **Co-op.** Every `Player_obj` counts as a player, so another player's
  neighbourhood stays awake, but no co-op session was measured.
- **Other acts.** The live A/B ran in Act_01_01 (density 1x and 5x);
  Act_02_01 was censused, not measured.
- **Pop-in.** A woken prop becomes visible when the game's own visibility pass
  next reaches it (every 30 frames), exactly as a far prop does without far
  sleep; after a long teleport both can show props a moment late.
- **Props made after the top-up scan** (debris from broken boxes, for
  example) stay awake: they cost what they cost without far sleep.
- **Restarts in a room without `objMinimap`** are not noticed; the zone's new
  props then stay awake until the next room change.

## Research commands (research build only)

- `zonecensus [radius]` - the census above.
- `evcount <compiled-code row name> | stat | reset` - counts calls of one object
  event through its table row (4 slots); how the Clean Up check was made.
- `farsleep ids plain|handle` - address props by plain numeric id or by the
  handle `instance_find` gave (the default). Both measured the same.
