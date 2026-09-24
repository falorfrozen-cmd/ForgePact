# Monster Density + "fill the map" - where the frame time goes (2026-09-22)

Status: **analysis; option A implemented the same day as pack markers**
(`plugin/include/ForgePact/PackMarkers.hpp`, the `reveal packs` command and
the panel's monster sub-toggle now mean markers; the real spawn pass moved to
`reveal spawn` / `map_reveal_spawn`, off by default), plus option D's count
gate in `BeWakeObject`. Live look and cost of the markers are still to be
confirmed in-game. Written after the five-second early population candidate
failed live acceptance at 4x density (see
[population-capacity.md](population-capacity.md)). Every game mechanism below
was read from a local decompilation of the game's own scripts (June 2026 image
in `_research/ghidra_current`, cross-checked against the installed 2026-09-17
build's script table) and is **paraphrased**; no decompiler output is
reproduced here, per the hub's `AGENTS.md`. Frame-time figures quoted from
earlier captures are inclusive wall-time samples, not CPU attribution.

## 1. The short version

The lag is not the spawning. It is the number of **living** monsters.

The early-population pass, the admission queue, the deferred density copies and
the extended protected-variable storage all solved real problems (crash at
262144 records, entry stall), but they only change *how fast* monsters are
born. After the queues are empty, every frame the game still walks every
living monster in several places, and each walk costs a few microseconds per
monster. Vanilla never has more than a few hundred monsters alive, because
packs are born only when the player comes within 1050 px. "Fill the map" at
1x makes ~1300 monsters alive in a zone (measured 208 -> 1273, 148 -> 866);
at 4x density that is ~5000. At a few microseconds each across four or five
per-frame passes, 5000 monsters is 30-80 ms per frame - which is exactly the
63-99 ms and 346-410 ms frame intervals the profile captures recorded *after*
both queues had emptied.

No plugin-side optimisation of the population pass can remove that cost. The
only two things that can are (a) not keeping far monsters alive, or (b) not
keeping them *active* (GameMaker deactivation). Section 5 weighs them.

## 2. How the game actually populates and runs monsters

Measured facts from the decompilation, in our own words. Script and object
names are the game's; the numbers are constants it uses.

### 2.1 Birth

- Every `Enemy_Creator_*` spawner exists from zone generation, awake. Each one
  registers a periodic check with the game's timer system
  (`timer_system_update`, handle stored in `enemyCreatorTimer`, observed ~116
  frames). The check measures the creator's distance to `Player_obj`
  (`distance_to_object`); below **1050 px** it takes the spawn branch (`spawnPack`, `alarm[2]`) and then destroys its own
  timer. A creator spawns once; afterwards it never polls again. This is why
  answering 0 to an uninitialised creator leaves it inert forever (Known
  Limitation 13), and why the "creator lie" is the only birth trigger there is.
- `EnemyCreatorPending` only asks whether any creator still has an alarm
  running (spawn in progress). It is not a scheduler.

### 2.2 Who gets a step

- `Controller_obj`'s Step event calls `ActivateDeactivateProps` (or the local
  variant) and then `EnemyStepHandleNew` every frame; `Menu_Controller_obj`'s
  Step event runs `timer_system_update`. Every **30 frames** (`updateEnemyTimer`, forced by `updateEnemies`) it
  walks **every active `Enemy_Child_Basic_obj`**, compares its position with a
  box around the players (`playerBoxL/R/T/B`), and rebuilds
  `monsterHandleArray` / `monsterHandleArrayCount` with the ones inside. A
  monster that leaves the box has its `wasActive` and `isMoving` flags
  cleared, its target dropped through `SocketSetTarget` and its path ended.
- `EnemyStepHandleNew` steps only the handles in that array, one
  `m_EnemyStep` call per handle; a handle to a destroyed or deactivated
  instance simply gets no step. So AI, pathfinding, `EnemyParentBeginStepMain` (which alone
  does ~14 protected-variable reads, health-bar and shadow updates per monster
  per frame) and the effect timers run **only for monsters inside the player
  box**. Far monsters do not step in vanilla either - the Beacon's "zero scans
  beyond 1500 px" was this box, not GameMaker deactivation.
- The game never deactivates monsters. The creator census (271/271 awake,
  enemies counted at 7300 px) already showed this; the decompile confirms the
  monster pass only toggles flags. Deactivation (`DeactivateObject`,
  `instance_deactivate_object`) is used for props and their lights.

### 2.3 What runs for every living monster, every frame

These are the passes whose cost is proportional to the number of **active**
monsters, near or far:

| Pass | Where | Per-monster work |
| --- | --- | --- |
| Minimap dynamic layer | `objMinimap`'s Draw GUI event calls `DrawMinimap` every frame, which calls `DrawMinimapDynamic`; that walks `Enemy_Parent_obj` (one of 17 object families it draws) | reads `enemyType`, `enemyRarity`, `enemyNetId`, position; picks a dot sprite/colour; one draw call |
| Health bars | `Enemy_Health_Bar_Parent_obj` is a separate instance per monster; its Draw GUI event is the only caller of `DrawEnemyHealthBars` | early-out checks, GUI coordinate mapping; measured 4.14 ms/frame in the last capture |
| Monster draw | `Enemy_Child_Basic_obj` has its own Draw event; GameMaker calls it for every active instance whose `visible` is true | on-screen test then sprite work; `visible` is what the game toggles for far monsters |
| Box rebuild | the 30-frame walk in `ActivateDeactivateProps` | position reads and four compares, plus the `wasActive` cleanup - a spike every half second, proportional to the whole population |
| Runtime bookkeeping | GameMaker itself | instance list walks for each event type, alarms, depth ordering |

Buff timers (`Draw_Enemy_Buff_obj`, one per active buff, `m_runEnemyBuffs`
every frame) and corpses (`Corpse_obj`, `m_CorpseStep`) scale with buffs and
corpses, not with the population.

Every row is a few microseconds per monster in this YYC build (a runtime call
per variable read, a method call per `with`). Four rows times a few
microseconds times 5000 monsters is the observed 30-80 ms. The 1x fill test
(816 monsters, 7.80 ms average frame) fits the same curve.

### 2.4 What the plugin adds on top

- **Tyrant's Crown / Beacon wake walk.** `BeWakeObject` runs every 6 frames
  and, before calling `instance_activate_object`, snapshots every active
  monster with `instance_find` plus an identity read. Because the game never
  deactivates monsters, activation never changes the count and the snapshot is
  always wasted: two runtime calls per monster every sixth frame, ~10 k calls
  at 5000 monsters (the 4.31 ms/frame "hunt activation" line). It only runs
  while one of those two mechanics is worn.
- **Protected-variable routing.** Every protected read now passes through the
  bank router. That is a fixed small cost per read and was measured below
  1 ms/frame; it is not the problem, but it multiplies with the same N.
- The distance hook, the admission queue, the density copy queue and the frame
  callback were all measured under 1 ms/frame once the pass is over.

### 2.5 The entry hitch is a different problem

Peak outer creation calls of 89-123 ms belong to `objZoneGenV2`, the game's
zone generator, which uses the instances it creates immediately. Those cannot
be queued or split. A pack birth is the creator's `Alarm_2` event (the largest
event body in the object, ~140 KB of compiled code: satanic-zone buffs, dungeon
checks, then the monster creates, each of which runs a ~67 KB Create event
that builds pathfinding state); it is one indivisible native call per pack,
milliseconds each, and already paced. The five-second target is a policy choice: a lower per-frame
budget (2-3 ms when the previous frame was already slow) trades a longer
population for fewer visible hitches. It does not change the steady-state cost
above.

## 3. What does not help (so it is not retried)

- Faster or smarter admission, copy selection or identity caching: all of
  that is before the steady state. The profile already showed <1 ms/frame in
  those paths with 63-99 ms frames.
- Disabling stepping for far monsters (`DisableEnemyStep`, `enemyStepEvent`):
  they already do not step. The cost is draw/minimap/health-bar/bookkeeping.
- Setting `visible = false` ourselves: the game rewrites it within a second,
  and it does not touch the minimap or health-bar passes.
- A watcher thread or sampling profiler: useful for attribution, not a fix.
  (If one is wanted, an in-plugin `SuspendThread`/`GetThreadContext` sampler
  in the research build, bucketing the frame thread's instruction pointer by
  script-table entry, would give real CPU attribution without WPR. The script
  table can be recovered from the image alone; see `_research` notes.)

## 4. What "fill the map" is for

The feature exists so that the revealed map shows where the packs are, and so
that walking to a pack finds it there. Both goals can be met without keeping
thousands of monsters alive:

- a pack's *position* and *kind* are known from its creator before birth
  (`Enemy_Creator_obj`, `_Champion_`, `_Ancient_`, `_Legion_`, `_Miniboss_`,
  `_Ambush_`, `_Colossal_Chest_`), and creators exist from zone load;
- the pack's *composition* (rarity rolls, affixes) is decided at birth, so a
  marker cannot show "this pack has a rare" - but the dot the game draws today
  only shows rarity for monsters that already exist anyway.

## 5. Options

| | A. Virtual fill (markers) | B. Rolling-radius fill | C. Full fill + sleep far packs | D. Status quo + micro-fixes |
| --- | --- | --- | --- | --- |
| What the player sees | every pack on the map as a marker by pack kind; packs spawn at 1050 px as in vanilla | real monsters within ~2500-3000 px, spawned ahead as you move; markers beyond | every pack really alive at entry; far ones asleep | as today |
| Steady-state cost | none | bounded by radius, but leftover unkilled packs behind you stay alive | bounded to the near set; sleepers cost nothing | 30-80 ms at 4x |
| Entry cost | none | a few packs | full pass (paced) | full pass |
| Mechanism | hook `DrawMinimapDynamic`, draw one dot per unspawned creator using the same coordinate transform (or paint once into the static minimap surface) | keep the creator lie open per zone but only for creators within R of the player (the Beacon path already does this) | `instance_deactivate_object` on monsters (and their health-bar instance) beyond R, `instance_activate_object` on approach, markers for sleepers | - |
| Risk | low: read-only plus draw calls | low: same hook and guards as today | medium: helper instances and references to sleeping ids (timer system pauses timers of missing instances - measured; `with` on a sleeping id is a no-op - measured; other references unknown until tested). The hub rules class wholesale deactivation as not recommended; this would be targeted, fail-open (any doubt -> wake everything) and opt-in | - |
| Effort | medium | small | large | small |

### Recommendation

1. **A + B as the default** "fill the map": markers for every pack, real
   monsters around the player. This removes the steady-state cost entirely
   for the part of the map the player is not near, keeps density where it is
   fought, and needs no new game mechanism.
2. **C as an opt-in experiment** for players who want every pack alive
   ("Sleep far monsters"), built fail-open: wake everything on room change,
   toggle-off, any accounting mismatch or any runtime error; keep the sleeper
   list as plain numeric ids; never sleep a monster inside the player box or
   any monster not born from the pass. It needs a live session to find out
   which references to a sleeping monster the game still makes.
3. **D regardless**: make the Tyrant/Beacon wake walk count-gated (activate,
   compare counts, walk only if something was actually re-activated), and
   prefer a smoothness budget over the five-second target when the previous
   frame was slow.

## 6. Live checks that would settle the remaining unknowns

- `census` (research build) plus fps at 1x/2x/4x with fill on, standing
  still in a cleared area: frame time should track `instance_number
  (Enemy_Parent_obj)` roughly linearly if section 2.3 is right.
- Toggle the minimap off (game option) with everything else equal: the
  difference is the minimap pass alone.
- For option C: sleep one far pack by hand (`instance_deactivate_object` on
  its ids and their `myHealthBar`), play for ten minutes, wake it, walk to it.
  Any "Unable to find any instance" error names the reference that has to be
  handled.

## 7. Evidence trail

- Script table recovered from the June image and the installed 2026-09-17
  build (20844 and 20925 entries respectively; stride 24, name pointer +
  function pointer). The installed build has the same scripts and objects.
- The installed build's `ActivateDeactivateProps`, `EnemyStepHandleNew` and
  `DrawMinimapDynamic` were decompiled separately and show the same shape:
  the 30-frame `updateEnemyTimer`, the `playerBox` test over
  `Enemy_Child_Basic_obj`, the `monsterHandleArray` rebuild and `wasActive`
  cleanup, one `m_EnemyStep` call per handle in that array, and the minimap walk over
  `Enemy_Parent_obj`. The mechanism did not change between the two builds.
- Functions read: `EnemyStepHandleNew`, `ActivateDeactivateProps`,
  `LocalActivateDeactivateProps`, `DrawMinimapDynamic`, `DrawEnemyHealthBars`,
  `EnemyParentBeginStepMain`, `EnemyChildStepGlobalTimers`,
  `EnemyChildStepEffectTimers`, `timer_system_update`, `DisableEnemyStep`,
  `EnemyCreatorPending`, `DeactivateObject`, `ViewVisible`, the creator's
  periodic check closure, and the object event tables for `Enemy_Parent_obj`,
  `Enemy_Child_Basic_obj`, `Enemy_Health_Bar_Parent_obj`,
  `Draw_Enemy_Buff_obj`, `Corpse_obj`, `Controller_obj`, `objMinimap`.
- Earlier live measurements reused: creator census (map-reveal-research.md
  section 10), profile captures and frame intervals (population-capacity.md).
