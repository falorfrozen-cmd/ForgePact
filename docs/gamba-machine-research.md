# The gamba machine and Goburin's Head (ForgePact #134): research phase

**Question.** How does Hero Siege's gamba machine (`Slot_Machine_01_obj`) take a
spin, when does it explode, and where does it roll and build its prize? The
prize this issue is about is Goburin's Head, the unique charm at repository
type 10 / sub 0 / base 98. And which of those calls could a pity mod answer?

**Why it matters.** Issue #134 asks for a pity mod: after enough spins or
explosions without the charm, the machine should give it. Nothing in this
repository knew how the machine works. `docs/RUNTIME_DATA_MODELS.md` § 13.10
recorded "no gamble routine was found in Season 10", which is true of the
game's named scripts and wrong about the game: the gamble is an object, and
its logic lives in that object's events. What decides the mod's shape is still
unmeasured: which call rolls the prize (a GameMaker RNG builtin, a named
script, or the machine's own closure), whether the roll happens per spin or at
the explosion, and how the built item is placed on the ground. Each answer
gives a different mod. So this phase builds one research-build instrument
(`gambaprobe`) that hooks every candidate in one build, runs one live session,
and records a `## Decision`. The mod itself (a Mods → Quality of Life switch,
off by default, with a configurable pity count, `gambapity`) is phase 2,
planned from that decision, as `jump-scenery-research.md` phase 2 was.

**Posture.** Everything here is measured runtime behaviour, a reading written in
our own words, or our own code. Game objects and scripts are named by their
`hs-game-sdk` names and indices. No game script text and no address appears. A
reading is labelled as a reading, and it is not a fact until a live session
records it.

**Out of scope for this phase.** The pity mod itself, its panel row, its oracle
entry, its release note and its persistent counter (phase 2). Changing how often
the game's own Angelic/Unholy roll or the **Angelic / Unholy Drops** slider
drops Goburin's Head (it is a `kAngelicBases` row). Spawning machines for
players, changing the spin price, or anything that suspends the game's loop.
`Glyph_of_Gamba` and `Relic_Casino_Dice_obj` (named in the static search, not
instrumented). Reading or changing the player's gold balance from the plugin.

## Status

- **Phase:** research for Goburin's Head pity (ForgePact #134). Phases 1 and
  1c measured the machine, phase 3 wrote the pity mod (`gambapity`, on this
  branch and unreleased), and phase 4 measures the explosion it must fire on.
  Nothing of the research instrument is player-visible, and the player build
  is free of it.
- **Instrument:** `gambaprobe` (§ Instrument), research build only, on the
  ForgePact branch `134-goburins-head-pity-research`.
- **Live procedure 1:** ran on 2026-10-04 and came back INSTRUMENT-BLIND: every
  spawned machine was removed in the step after its creation, before its first
  `Step_0`, so no spin was measured (§ Results, `### Live 1 results`).
- **Live procedure 2:** ran on 2026-10-04 (§ Results, `### Live 2 results`).
  The instrument was proven (every row installed, the builtin rows' self-test
  moved by one, the caller walk printed), but no spawn route survived: each
  machine was removed by its own `Alarm_9` in its first step, whatever route
  created it. No spin was measured, so every spin key in § Decision reads
  `not-observed`, and `pity-design` reads `blocked` until a machine can be
  measured.
- **Next:** the successor workorder moves the force to the explosion after
  Live procedure 4.
- **Phase 1c (2026-10-04):** the local reading, the `scp` and `stamp` spawn
  routes, the extension-function rows and `fnwalk` are in (§ Static reading,
  § Instrument); § Live procedure 3 ran on 2026-10-04 (§ Results, `### Live 3
  results`): no spawn route survived, but a natural machine spun; the state
  route and the by-name route are both blind.
- **Owner's decisions (2026-10-04):** the pity counts explosions, with the gold
  equivalent shown; the session character (slot 14 "Sorak") has enough gold
  for the full procedure; measure first, then plan the mod. Phase 3 counted
  spins instead (the explosion was never observed). The 2026-10-05 ruling
  names the explosion as the firing event; whether the count unit stays spins
  is the successor's decision.
- **Phase 4 (2026-10-05):** the owner ruled that the pity fires on the first
  **explosion** after the count is reached, never on a payout, and that an
  explosion is the machine's last act after roughly 10-14 spins and the only
  time Goburin's Head drops (the owner's report, not measured). Phase 3's
  force acts on a payout build, the wrong event, so it is superseded and
  stays unreleased. Nothing has observed the explosion, so `gambaprobe` gains
  the explosion watch (§ Instrument, `gambaprobe window`) and § Live procedure
  4 runs one natural machine to its explosion. The mod itself is the next
  phase, planned from that session.

## Static search

Every line is labelled. **Static search** means the name or index is in
`hs-game-sdk` or the game's executable; it says nothing about behaviour.

- **Static search:** object `Slot_Machine_01_obj` (`HeroSiege::Objects`, index
  4644). Sprites `Slot_Machine_01_spr`, `Slot_Machine_01_Destroyed_spr` and
  `Slot_Machine_Icons_spr`; sound `Las_Gambas_snd`.
- **Static search (phase 4):** `Slot_Machine_01_Destroyed_spr` (Python SDK
  sprite 26574) is the likely look of a machine the owner reports as used up
  after its explosion. **Not established** that the explosion sets it, which
  is why the explosion watch prints any sprite change, and a machine that is
  gone, rather than waiting for this one name.
- **Static search:** sprites `Glyph_of_Gamba_spr` and
  `Pickup_Glyph_of_Gamba_spr` belong to a glyph item, and object
  `Relic_Casino_Dice_obj` to a relic. Neither is the machine, and neither is
  instrumented.
- **Static search:** the object has one Create closure,
  `gml_Script_anon_1474_gml_Object_Slot_Machine_01_obj_Create_0`
  (`HeroSiege::Scripts`, index 5343). Its name moves with every game patch, so
  the code spells it through the SDK constant only; this doc calls it
  `anon@1474`.
- **Static search:** Goburin's Head is a unique charm, repository type 10 /
  sub 0 / base 98, key `charms_goburins_head`. It is the `kAngelicBases` row in
  `ModuleMain.cpp`, flagged not angelic. Its kill effect is
  `gml_Script_EnemyKillGoburinsGreed` (1507), which is not part of the drop.
- **Static search:** the executable stores the string "Gamba Machine" beside
  the charm keys, as the item database's drop-source label.
- **Not established:** whether the game rates the charm 7 (Angelic) or 10
  (Unholy). If it does, `BuildAngelicPool` already admits it to the
  **Angelic / Unholy Drops** slider's pool. That is a fact for the owner, not
  for this phase.
- **Static search:** no script named for gamba, gamble, slot, jackpot or casino
  exists in `scripts.hpp`. `RUNTIME_DATA_MODELS.md` § 13.10's "no gamble
  routine was found in Season 10" was a statement at call-skeleton level about
  scripts; the machine's logic is in its object events (§ Static reading).

## Static reading

Every claim carries one of four labels, plus a source:

- **Static reading**: a local reading of the game's compiled-code table and
  event bodies, kept on the reader's machine, written here in our own words. No
  script text or address is quoted or transcribed (`AGENTS.md` § "Legal" in the
  toolkit). Unless a claim says otherwise, the reading is of the build current
  on 2026-10-04, read that day.
- **Measured**: observed in a running game.
- **Our code**: what ForgePact does.
- **Not established**: not known yet. § Not established lists what is still
  open after Live 2.

### The object's events

- **Static reading:** the object has seven events, `Create_0`, `Alarm_0`,
  `Alarm_9`, `Step_0`, `Draw_0`, `Draw_64` and `CleanUp_0`, plus the closure
  `anon@1474`. They were found through the compiled-code table (rows of 24
  bytes: name, function, variables), the same table `FrameProfiler.hpp`'s
  `FindGmlTable` walks at runtime.
- **Static reading, not verified:** an object event takes two pointer
  parameters (`self`, `other`) and returns nothing. A detour on an event row
  must use that signature, not `PFUNC_YYGMLScript`, and call the trampoline
  with the same two.

### Step_0 picks and builds the prize

- **Static reading:** `Step_0` is the only event that calls
  `GetUniqueRepoStruct` (three sites) and `CreateDefaultParams` (seven sites)
  directly. That is the pair the Angelic roll uses to pick a unique definition
  and build the item's parameters (`RUNTIME_DATA_MODELS.md` § 13.4).
- **Static reading:** `Step_0` also calls `GetGoldAmount` (2),
  `GoldOperationPending` (2), `GetGoldCounterHash` (1), `LootBlocksUseKey` (2)
  and `NetworkSendClientEffect` (2) directly, and refers by name to `GPV` (57
  references), `SPV` (15), `PlaySound3D` (3), `CheckUseKey` (2),
  `GetLocalPlayer` (2), `InputPressed` (2), `os_mobile` and `draw_speak`. Its
  strings include the prompt "Gamba for $10000". It is about 117 KB of code,
  far too large to read whole.
- **Not established:** what the three unique picks and seven parameter builds
  in `Step_0` correspond to (prize tiers?).

### What `Step_0`'s call layout shows, and what it does not (phase 4)

- **Static reading (2026-10-05, the build current that day):** `Step_0` does
  not decompile: the decompiler process died on it, so it is deliberately not
  in the decompile index, where a failure stub could be mistaken for a body.
- **Static reading:** a call-by-call listing of `Step_0`, callees named from
  the symbol dump, shows only the named script calls listed above: 7
  `CreateDefaultParams`, 3 `GetUniqueRepoStruct`, 2 `LootBlocksUseKey`, 2
  `GetGoldAmount`, 2 `GoldOperationPending`, 2 `NetworkSendClientEffect` and 1
  `GetGoldCounterHash`. The builds sit in one stretch: three builds with no
  pick before them, three pick-then-build pairs, and one trailing build.
- **Static reading, not verified:** each of the three pick sites loads the
  same small constants at the same distances before the call, consistent with
  all three picking the measured `(1, 0, 72)`; no site was seen loading 10 or
  98. A small operand can be a stack offset, so this is not proof.
- **Not established:** which of those builds, if any, is the explosion's, its
  `self`, and whether the explosion builds an item at all. The layout cannot
  say; the explosion watch (§ Instrument) measures it.

### The die and the ground placement are reached another way

- **Static reading:** no event calls `LootGroundCreate`,
  `LootGroundCreateFromItem`, `CreateLootInFreePos`, `DropItem`,
  `DropUniqueItems`, `cpr_irandom` or `cpr_rand32` directly, and none names
  them for lookup.
- **Not established:** how the die and the ground placement are reached: a
  builtin through the runtime's function table (the dungeon-chest reading found
  that builtins are never called directly in this build), or a method on a
  struct.

### The machine's state is in the protected store

- **Static reading:** `Create_0` calls `InitPV` (28 references) and `SPV` (26);
  `CleanUp_0` frees them (`FPV`, 28). This is the `GPV`/`SPV` protected store
  that the dungeon chest and the satanic zone use (`RUNTIME_DATA_MODELS.md`
  § 14.1, `boss-rarity-research.md`), keyed by number.
- **Static reading:** so the machine's spin count, gold spent and explosion
  threshold are not ordinary instance variables. `variable_instance_get_names`
  on the machine will not list them; a `GPV`/`SPV` trace with the machine as
  `self` will.
- **Static reading:** `Draw_64` draws localized text with the keys `gold_spent`
  and `gamble_for` and the literal " $10000" (`GetLocalized`, four sites), so
  each machine shows the gold spent on it: a per-machine gold-spent value
  exists.
- **Not established:** which numeric keys hold that value, the spin count and
  the threshold.

### Alarm_9, Alarm_0 and the closure

- **Static reading:** `Alarm_9` reports the spawn through `DebugLogAddExt` and
  `ReportClient` ("Slot Machine Spawned", "Slot Machine Spawned out of thin
  air"). The plugin's `debuglog` command already hooks `DebugLogAddExt` into
  `bp_ipc\gamelog.txt`, so this line is a cheap positive control for a spawned
  machine.
- **Static reading:** `Alarm_0` calls builtins and `GPV`/`SPV` only; the
  closure `anon@1474` calls `GPV` (23), `SPV` (9) and `PlaySound3D` (2).
- **Not established:** which of `Alarm_0`, `Step_0` and the closure is the reel
  stop, the spin and the explosion.
- **Static reading:** neither 10,000 nor 750 appears as a literal in the
  `Create_0`, `Alarm_0` or closure bodies read (the closure's only named callee
  is itself; the rest are runtime helpers and builtins). So the price and the
  odds live in the constant tables or the protected store, and only the trace
  will show them.

### Why `GetNamedRoutinePointer` cannot reach an event

- **Measured** (`pet-quest-collector-research.md`, 2026-09-10):
  `GetNamedRoutinePointer` resolves no `gml_Object_*` name; 22 of 22 raw event
  names returned not found. So `HookRawNamedRoutine` cannot be the instrument,
  and neither can `citrace`'s raw rows.
- **Our code:** the route left is the compiled-code table. `FindGmlTable`
  (`FrameProfiler.hpp`) walks it from the row a known `CScript::m_Functions`
  points at, each row carrying a `gml_` name and a function pointer; 14,694 of
  its 20,951 rows are object events (`frame-profiler.md`). § Instrument
  resolves the machine's event rows there, by name.

### What `Create_0` runs, and the parent chain

Read after Live 1 (2026-10-04), to explain its zero counts.

- **Static reading:** `Create_0` (about 67 KB of code) has one exit and no
  early return. In order, it updates the machine's depth (`UpdateDepth`), arms
  its alarm 9 for the next step, runs the inherited Create of its parent
  `Collision_Prop_obj` (which updates the depth again and arms alarm 11 two
  steps out), and then makes about 80 calls by name to `gml_Script_InitPV`,
  `gml_Script_SPV` and `gml_Script_GPV`. `CleanUp_0`'s by-name callee is
  `gml_Script_FPV`.
- **Static reading:** `Alarm_9` logs "Slot Machine Spawned" through
  `DebugLogAddExt` first, on every path, and reports to clients; "out of thin
  air" is on a later branch.
- **Static reading:** the object's parents are `Collision_Prop_obj` (959) ->
  `Collision_Parent_obj` (957) -> `Avoidable_Parent_obj` (433)
  (`hs-game-sdk` `kObjectParents`). `Collision_Prop_obj` owns a `Create_0`
  and an `Alarm_11`; the two parents above it own a `Create_0` only.
- **Our code:** the far-sleep mod sleeps `Collision_Prop_obj` leaves outside
  towns (`far-sleep-research.md` § What sleeps). It is off by default, and
  § Live procedure 2 keeps it off.

### The game's own creation call

- **Static reading:** the game creates a machine from
  `gml_Script_ClientCreateEffect`, in the one case of its switch that carries
  the object index 4644. That case is a single call of the game's own
  `gml_Script_instance_create` script with three arguments: x, y and the
  object.
- **Static reading:** `instance_create` reads a layer from a global array and
  calls the `instance_create_layer` builtin with that layer. So the game's
  props live on a named layer of the game's choosing, while Live 1's
  `gambaprobe spawn` called `instance_create_depth(x, y, 0, object)`, which
  puts the instance on a depth layer the runner manages.
- **Static reading, not followed:** `Zone_State_Buffer_obj`'s Create closures
  also name the object, which is presumably how machines persist between
  visits to a zone. Those bodies were not read.

### The by-name route

- **Static reading:** compiled GML in this build reaches `InitPV`, `SPV`,
  `GPV` and `FPV` through a cached lookup by name that ends in a call through
  the runtime's functions array (24-byte entries: name, routine, argument
  count, usage; the layout YYToolkit's patch 0004 mirrors), with the builtin
  calling convention (result, self, other, argument count, arguments).
- **Static reading:** a direct call to a script's own function (`UpdateDepth`,
  `GetGoldAmount`, `GetUniqueRepoStruct`) is a different route, and that one
  the inline detour of `HookOneScript` sees. The script table's `InitPV`
  (`citrace symdump`'s `symbols.csv`) and the compiled-code table's
  `gml_Script_InitPV` are the same function. So whatever routine the
  functions array holds for `gml_Script_InitPV`, the calls `Create_0` made
  through it did not pass through that function's entry.
- **Measured** (two sessions): `InitPV`, `SPV` and `FPV` counted 0 calls in
  Live 1 while `Create_0` and `CleanUp_0` ran to their end, and
  `dungeon-chest-research.md` (its Live procedure 1) counted `store GPV
  calls=2 gameCalls=2` for a whole dungeon, where chests read the store every
  step. Both were first read as the game not calling. Both are the detour not
  seeing the call: **not observed by the detour**, not "not called".
- **Not established:** whether `gml_Script_InitPV` (or the short name) names
  a functions-array entry whose routine is a different executable address from
  the script's own function, or whether both names name the script and the
  call reaches its function in a way the detour misses. `byname=same` does
  not answer it: that column only shows which entry `GetNamedRoutineIndex`
  (one index per name) prefers, and the functions array was never walked.
  `gambaprobe fnwalk` (§ Instrument) walks the array and answers it.
- **Measured** (Live 2, 2026-10-04): every script row read `byname=same`: both
  the short name and the `gml_Script_` name resolve to the script itself. That
  column only shows which entry `GetNamedRoutineIndex` (one index per name)
  prefers; it does not show that the functions array lacks a same-named entry
  with a different routine, because the array was never walked. Yet four
  `Create_0` runs left `InitPV`, `SPV`, `GPV` and `FPV` at `machine-self=0`.
  So the store rows are blind, and whether a same-named functions-array
  routine exists is open until `gambaprobe fnwalk` walks the array
  (§ Instrument; § Results, `### Live 2 results`).

### What `Alarm_9` checks before it removes the machine

Read after Live 2 (2026-10-04), from a local reading of the event body, kept
on the reader's machine, to explain why every spawn route died in its first
step.

- **Static reading:** `Alarm_9` first logs "Slot Machine Spawned" through
  `DebugLogAddExt` on every path, then reads the machine's protected value
  `activated` through the extension function `GetVariable`, called by name
  with the key `activated`. When that reads false it sets `activated` and
  `isActive` to true and `rollTimes01`..`rollTimes04` each to 8 plus the
  result of a runtime routine called directly with the argument 8, all
  through `SetVariable` by name. **Static reading, not verified:** that
  runtime routine has the shape of the runner's `irandom` core (a
  sign-adjusted argument, an integer result); if so, that one call bypasses
  the `irandom` builtin's table entry. This says nothing about the prize
  roll: a zero on a builtin RNG row is "not observed", never "not called",
  and `roll-route: builtin` stays open.
- **Static reading:** then it reads the protected value `pSpwd` (the
  instance variable `pSpwd` is the key) through `GetVariable`; when that is
  false it reports "Slot Machine Spawned out of thin air" (`ReportClient`)
  and destroys the machine through the runner's instance-destroy routine,
  called directly — not through the `instance_destroy` builtin's table
  entry. Live 2's `instance_destroy` row counted no machine call, and the
  `CleanUp_0-caller` walk's runner frames lie inside that routine and its
  callee (a measured reading of the same code). Nothing else in `Alarm_9`
  destroys or deactivates.
- **Static reading:** `Create_0` initialises `pSpwd` to false (reads the
  instance's `pSpwd` key, then the by-name `InitPV` call with `false`). So
  every machine ForgePact created so far died for one reason: nobody set
  `pSpwd` to true between its `Create_0` and its first step.
- **Static reading:** the same `pSpwd` guard is shared game-wide: 60
  compiled functions read that variable slot (chests, portals, shrines,
  globes, pickups, the zone state buffer's Create closure,
  `ZoneGenPopulatePresetObjects`, `CreateItemDrop`, `DropGold`,
  `LoadBossDeath`, and `ClientCreateEffect` once inside its machine case).
  So § "The game's own creation call" was incomplete: the effect case also
  stamps `pSpwd`, which is why Live 2's `game` route (the call without the
  stamp) died.

### Who sets the spawned flag

Read after Live 2 (2026-10-04), from a local reading of `sCP`, kept on the
reader's machine.

- **Static reading:** `sCP(x, y, object)` calls the `instance_create_layer`
  builtin with `(x, y, global.gameLayer[room][0], object)` — the layer the
  game's own `instance_create` script also picks, `room` being the builtin —
  then reads the new instance's `pSpwd` key and calls `SetVariable(key,
  true)` by name with the caller as `self`, and returns the instance. No
  early return, no other check. `hs-game-sdk` names it `gml_Script_sCP`
  (index 474).
- **Static reading; measured (Live 3):** the argument order contradicts
  `S10-special-content-notes.md` ("object ref, x, y") and
  `RUNTIME_DATA_MODELS.md` § 14.3's bullet copied from it. This reading
  follows the argument list the local reading builds (the first and second
  arguments copied into the builtin's x and y slots, the third into its
  object slot). Live 3's `spawn scp` answered `object=4644` with the default
  `(x, y, object)` order, confirming it (the `oxy` order was never needed; a
  wrong order would create whatever object index `y` names, at `(object,
  x)`: harmless in a research session that restores its saves).
- **Static reading, not followed:** the effect route the game uses
  (`ClientCreateEffect`'s machine case) was rejected in 1b and stays
  rejected: its effect number is unverified and a wrong one dispatches
  another network effect. `sCP` is the game's own stamping spawner,
  reachable by name through the route `DungeonChestChat` proved
  (`dungeon-chest-research.md` § Chat route) and the `game` route reproduced
  in Live 2 (a machine was created by that route; only the stamp was
  missing).

## Instrument

`gambaprobe` is research build only (`#ifndef FORGEPACT_RELEASE`). It is never
in `kPlayerCommands`, and nothing of it reaches the player build. It lives in
`plugin/ModuleMain.cpp`, dispatched from `HandleGambaProbeCommand` as a
standalone early return straight after `HandleJumpProbeCommand`, with its
decision core in `plugin/include/ForgePact/GambaProbe.hpp` (namespace
`ForgePact::GambaProbe`, game-independent, tested whole by its harness).
`jumpprobe` (`jump-scenery-research.md` § Instrument) is its model.

**Rows.** One `gambaprobe hook` installs all of them.

- **Event rows** (compiled-code table walk): `Create_0`, `Alarm_0`, `Alarm_9`,
  `Step_0` and `CleanUp_0` of `Slot_Machine_01_obj`. Each counts its calls and
  logs the instance id and the frame number (trace budget below).
  - Each `gml_Object_Slot_Machine_01_obj_*` name is resolved in the table by
    name at `hook` time, never by a stored address.
  - A row whose function is not executable inside `Hero_Siege.exe`
    (`AddrIsExecutableInModule`), that is, one a mod has swapped, is reported
    `row=swapped` and skipped. A name the table does not hold is reported
    `row=missing`.
  - Otherwise the row is detoured with `MmCreateHook` and a tagged thunk, as
    `HookOneScript`'s native half does, with the `(self, other)` event
    signature.
  - This is the third tier of "Never Call an Address You Resolved by Hand" in
    the toolkit's `AGENTS.md`: a runtime struct, validated, with a positive
    control (`step-fires` in § Live procedure 1).
- **Script rows**, machine-self filter (`HookOneScript`, dual install): the
  closure `anon@1474`, `GPV`, `SPV`, `InitPV`, `FPV`, `GetGoldAmount`,
  `GoldOperationPending`, `GetGoldCounterHash`, `PickUpGoldCheck`,
  `LootBlocksUseKey`, `CheckUseKey`, `InputPressed`, `NetworkSendClientEffect`,
  `PlaySound3D`, `LootGroundCreate`, `LootGroundCreateFromItem`,
  `CreateLootInFreePos`, `CreateItemNew`, `DropItem`, `DropUniqueItems`,
  `GetUniqueRepoStruct`, `CreateDefaultParams`, `cpr_irandom` and
  `cpr_rand32`, each through its `HeroSiege::Scripts` constant. Each logs
  `argc`, up to six described arguments and the result (trace budget below);
  `GPV`/`SPV` log key and value, so the machine's state keys surface.
- **Builtin rows**, machine-self filter (`HookBuiltin`): `irandom`,
  `irandom_range`, `random`, `random_range`, `choose`, `instance_destroy`,
  `instance_create_depth` and `instance_create_layer`, and, since Live 1, the
  removal candidates `instance_change`, `layer_destroy_instances`,
  `instance_deactivate_object` and `room_goto`, each through its `HeroSiege`
  builtin name. A builtin row's `self` is the call's `self`, read by the
  instance-handle rule. `instance_destroy`, `instance_change` and
  `instance_deactivate_object` also count and log a call whose first argument
  names a machine, whoever its `self` is (`machine-arg=`): Live 1's
  `instance_destroy` row counted no machine-self call while the machine was
  removed, and a removal from another object's code would carry the machine as
  its argument, not its `self`. `hook` refuses while
  `citrace`, `jumpprobe` or `jumpscenery` holds one of them, as `jumpprobe
  hook` does.
- **The extension-function rows**, machine-self filter (`HookBuiltin`):
  `GetVariable`, `SetVariable` and `SetVariableToUndefined`, the extension
  functions `Alarm_9` and `sCP` call by name with the builtin convention
  (the compiled form of a `SetVariable` call branches on
  `is_undefined(value)` to `SetVariableToUndefined(key)`), each resolved by
  name. A `HookBuiltin` detour on a functions-array routine is the kind of
  row that counted 3,402 `instance_exists` calls in the dungeon-chest
  research, so these three rows (`GetVariable` logs key and result,
  `SetVariable` key and value) give the spin trace the machine's state keys
  and values without the store scripts. Their positive control is free: a
  surviving machine's `Alarm_9` makes at least one `GetVariable` and six
  `SetVariable` calls with the machine as `self` (`state-rows-live`). If
  the three rows read `missing` at `hook`, the extension functions are not
  in the table YYTK's lookup reads, and `stamp` cannot work either: both are
  one finding (`state-route: blind`). Rows the player build already holds
  are unchanged; nothing here re-detours one.
- **Held rows.** A script or builtin detours once. The rows another ForgePact
  hook already holds are reached through that hook, never hooked a second
  time: `GetUniqueRepoStruct` and `CreateDefaultParams`
  (`InstallSignatureAngelicHooks`, #74), `CreateItemNew` (custom forge, item
  truth, or the research build's item-inspect hook), `LootGroundCreate`
  (mining ore), `LootGroundCreateFromItem` (the research build's item-inspect
  hook), `DropItem` (`DropManager`'s `Hook_DropItem`, installed at startup in
  the research build), `GPV` (`abysstrace`), `instance_create_depth` /
  `instance_create_layer` (density), and `instance_destroy` while co-op's or
  `destroywatch`'s hook holds it.
  - `hook` runs the existing installer when one of them is not yet installed
    (never the item-inspect installer: a second run re-swaps a dozen table
    entries; if that hook is not in, the row is the probe's own), then
    reaches the row by what the holder's saved original is.
  - **A trampoline** (the holder detoured natively): the probe's detour is
    spliced into the saved original, so the holder's body calls the probe,
    which calls the trampoline. The probe sees exactly the calls the holder
    sees, on both routes, and no player hook body changes. The row reads
    `shared`.
  - **The game's own function** (the holder is table-only; in the research
    build that is `LootGroundCreateFromItem` always and `CreateItemNew`
    unless custom forge or item truth installed it first): the probe detours
    that function itself with `MmCreateHook`, once `AddrIsExecutableInModule`
    says it is code inside `Hero_Siege.exe`. The holder's body and compiled
    GML's direct calls both reach the probe; the holder is left as it is.
    The row reads `detoured-under`. This is `angelicprobe`'s second route,
    "detoured (under table-only hook)" (`angelic-roll-hook-research.md`),
    which its own live procedure used on this same
    `LootGroundCreateFromItem` hook.
  - **This plugin's own code** (the holder sits behind another table-only
    hook): nothing the probe can splice or detour sees every call, so the row
    stays `missing` and its line names the holder and the reason.
- **Table-only rows.** A script row the probe hooks itself comes up
  `table-only` when another install already holds its table entry; it is
  then blind to compiled GML's direct calls. `hook` counts such rows in its
  summary and names them in a `WARNING` line, so a `not-observed` from one is
  never read as a measured zero.
  - `DebugLogAddExt` (`debuglog`) is not a probe row: `Alarm_9`'s spawn line
    is read from `bp_ipc\gamelog.txt` through `debuglog`, step 1 of § Live
    procedure 1.

**Trace budget.** Every row counts every call. While the probe is armed (from
`hook` until `off`) or its lever is on, a call is classified by its `self`: a
machine, another `self` inside a machine's own event (counted as `in-event=`
and logged with `scope=machine-event`, in case the prize is placed from a
`with` block or a struct method), or anything else. The first two are logged,
within a budget that is per **window**:

- Each line has a key, what it is about: a script's first argument (a `GPV`
  or `SPV` state key), a builtin's argument text, an event's instance id. One
  key writes at most 8 lines a window (`kTraceLinesPerKey`); the lines a key
  was refused are counted as `key-capped=`. So one call shape the machine
  repeats every frame with a moving result (an idle `irandom`, a timer key)
  spends its own 8 lines and nothing of any other key's.
- A row writes at most 512 lines a window (`kTraceLinesPerRow`), which is 64
  full keys (`kTraceKeysPerRow`). The row cap exists only to bound a row whose
  keys never repeat. It is set above the 28 keys `InitPV` sets up, so that
  however many of the machine's state keys move during a spin's animation,
  the keys written at the spin's end (the gold-spent and spin-count update,
  the threshold compare) still get their lines. Once a row has written 512,
  its calls are still counted but not described, and its `status` line ends
  `BUDGET SPENT`; `status` also prints a `BUDGET SPENT on <n> row(s)` line
  above the rows.
- A line identical to its key's last line is not logged and costs nothing.
  That is all the deduplication guarantees: a key whose value or result
  changes spends a line each time it changes, until its 8 are gone.
- A call folded that way is never silent. The row counts it as `repeats=`
  (per window, and summed on the status line), the row's `status` line names
  each key still holding such calls as `unlogged-repeats: +<n> "<key>"`, and
  the key's next logged line carries them, prefixed `(+<n> repeat(s) of this
  key's previous line, not logged)`. Without this, a prize roll with the
  same arguments and result as a reel roll earlier in the spin would leave no
  line, and the reel roll would read as the call that decided the prize.
- `gambaprobe trace`, `gambaprobe hook` again and every `gambaprobe spawn`
  start a new window, and § Live procedure 1 sends `gambaprobe trace` right
  before each spin a check reads. A per-frame call with the same arguments
  as the prize roll can still use up that shape's 8 lines between the
  `trace` and the spin; the row's `machine-self=` count still moves, and a
  `key-capped=` above zero at the spin says the shape was cut.

The `rng` lever still answers machine-self calls only, and only the one
builtin and argument text it is aimed at (`gambaprobe rng` below).

**The machine-self filter.** A call counts for the machine only when its `self`
resolves (instance-handle rule, `VALUE_REF` accepted) to an instance whose
object index equals the resolved `Slot_Machine_01_obj` index. It is never a
kind check, and a call from any other object passes through untouched.

**Verbs.** Every lever reports what it did or why it refused.

- **`gambaprobe hook`** prints one line per row, ending `detoured`,
  `detoured-under`, `shared`, `table-only` or `missing`, then the summary
  `gambaprobe hook: <n> rows, <m> missing, <t> table-only (<d> detoured, <u>
  detoured-under, <s> shared)`, and a `WARNING` line naming every
  `table-only` script row. Every script row's line also ends with its
  `byname=` resolution (below). It also times each phase with
  `QueryPerformanceCounter` (event-table read, by-name resolution, script
  rows, builtin rows, caller-walk table copy) and prints one line
  `gambaprobe hook: took <ms> ms (events <ms>, byname <ms>, scripts <ms>,
  builtins <ms>, table <ms>)`. Live 2 saw `hook` freeze the game about 9 s;
  this line measures the phases and names the cause for the next session
  (`hook-timing`), and a fix is its own issue if the numbers call for one.
  Sent again, it re-arms and starts a new trace window.
- **`gambaprobe trace`**: starts a new trace window (every row's and every
  key's budget), answering `gambaprobe trace: every row's trace budget starts
  over`. The counts continue.
- **`gambaprobe spawn [depth|game|layer|self|scp|stamp]`**: creates one
  `Slot_Machine_01_obj` at the local player, by one of six routes, each a
  different code path (§ Static reading, "The game's own creation call" and
  "Who sets the spawned flag"):
  - `depth` (the default, and the control): `instance_create_depth` at depth
    0, the call Live 1 used, whose machines all died.
  - `game`: the game's own `instance_create` script, called by name with the
    arguments x, y and the machine's object index and with the local player
    as `self` and `other`, the exact call the game's effect case makes. The
    name is the short name of the `HeroSiege::Scripts` constant, resolved
    through `asset_get_index` and called through `script_execute` with
    `CallBuiltinEx`: the route `DungeonChestChat` proved live on 2026-10-03
    (`dungeon-chest-research.md` § Chat route).
  - `layer`: `instance_create_layer` on the local player's own `layer` value,
    read by the instance-handle rule: the game's builtin, our layer choice.
  - `self`: `instance_create_depth` through `CallBuiltinEx` with the player as
    `self` and `other`, which isolates who the caller is.
  - `scp [xyo|oxy]` (`spawn scp`): `sCP` by its SDK constant's short name, `asset_get_index`
    then `script_execute` through `CallBuiltinEx` (`ApCallScript`, as the
    `game` route), `self` = `other` = the local player, arguments `(x, y,
    machine)` (`oxy`: `(machine, x, y)`). The reply adds
    `object=<object_index of the created instance>`, which is 4644 only when
    the order was right (§ Static reading, "Who sets the spawned flag").
  - `stamp` (`spawn stamp`): the `game` route's creation, then in the same command read the
    new instance's `pSpwd` variable by the instance-handle rule
    (`variable_instance_get`), call `GetVariable(key)` by name through
    `CallBuiltinEx` with the player as `self` and `other` (as `sCP` does),
    then `SetVariable(key, true)` the same way, then `GetVariable(key)` again;
    print `pSpwd key=<v> before=<v> after=<v>`. Each refusal names its step
    (no `pSpwd` on the instance, name not resolved, dispatch failed). **Not
    established:** that `GetVariable`/`SetVariable` resolve through YYTK's
    builtin lookup; `pet-relic-collector-research.md` called `GetVariable` by
    name and got `undefined` on every call, which that doc left open. The
    `before=`/`after=` read-back is this route's own control: `after=true`
    with a surviving machine proves it; `undefined` twice means the name did
    not resolve, the finding for phase 2.

  Each answers `gambaprobe spawn: route=<name> id=<n> at <x>,<y>` (`scp`
  adds `object=<object_index>`; `stamp` adds `pSpwd key=<v> before=<v>
  after=<v>`), or a refusal naming the step that failed (no player, name not
  resolved, dispatch failed, result not an instance). It starts a new trace
  window first, so the new machine's `Create_0` is described. The `Create_0`
  and `Alarm_9` rows are its positive control. No route stores an address, a
  layer name or an effect id.
- **`gambaprobe selftest`**: calls `irandom(100)` once through `CallBuiltin`,
  outside the probe's busy guard, and answers `gambaprobe selftest: irandom
  row calls=<before> -> <after>`. The row must move by one; if it does not,
  the builtin rows are blind and nothing they count is evidence.
- **`gambaprobe fnwalk`**: walks the runtime's functions array, by
  validation, never by an address. It takes the routine
  `GetNamedRoutinePointer` returns for a builtin known to be the table's
  first entry (`camera_create`, measured 2026-09-11 in `citrace
  dispatchdump`'s notes in `ModuleMain.cpp`) and for two more
  (`is_undefined`, `instance_create_layer`), scans the main module's
  writable data for an aligned qword equal to the first routine, and accepts
  the 24-byte entry around it as the table base only when its first eight
  entries parse under one layout (a terminated graphic-ASCII name pointer, a
  routine `AddrIsExecutableInModule` places in `Hero_Siege.exe`, an argument
  count 0..16) and the other two names are found by walking with the
  routines `GetNamedRoutinePointer` gave — the rule YYTK patch 0004
  validates with
  (`third_party/yytoolkit/patches/0004-functions-array-validation.patch`).
  It then walks at most 20,000 entries and prints, for each of the probe's
  script rows, every entry named `<short>` or `gml_Script_<short>`:
  `gambaprobe fnwalk: <name> idx=<i> routine=<same|other|not-exe>` (`same` =
  the row's own function; `other` = another executable address in the game
  image; `not-exe` = refused). Every `other` routine is detoured through the
  `GpByNameSlot` machinery 1b built for exactly this (`GpInstallByName`;
  `byname-detoured` feeding the row, `byname-shared` when several rows share
  it), so `status` prints `byname=detoured|shared` where the walk found one.
  A table that does not validate prints `gambaprobe fnwalk: table not found
  (<which check failed>)` and leaves every row as it is; no RVA, nothing
  stored past the command (`citrace dispatchdump`'s RVA is not a seed). Its
  control is the next spawn's `Create_0`: about 28 `InitPV` calls with the
  machine as `self` (`byname-visible`, re-measured: pass if `InitPV
  machine-self>=1` or a `byname-shared` row listing it reads
  `machine-self>=1`). Research build only.
- **`gambaprobe rng <builtin> <value> [count] [args <text>]`**: answers the
  next `count` machine-self calls of one RNG builtin (`irandom`,
  `irandom_range`, `random`, `random_range` or `choose`) with `value`, logs
  what was replaced, then turns itself off (`count` 1 to 50, default 1).
  - It is aimed, because the machine makes RNG calls the prize does not
    depend on: a reel roll each spin, possibly an idle call every frame. An
    unaimed lever would hand its one answer to the first of them. With
    `args`, only a call whose argument text is `<text>` is answered. The text
    is everything after `args`, copied character for character from a trace
    line, which prints each argument with its kind and `std::to_string`
    digits (`a0=real:100.000000 a1=real:5.000000`, never `a0=100 a1=5`), and
    runs of spaces do not matter.
  - Every other machine-self RNG call the armed lever sees, whether another
    builtin's or the target's with other arguments, runs the game's own
    function and is counted as `passed=`, on its row and in the lever's line
    (`passed=<n> (other builtin <a>, other args <b>)`).
  - For `choose`, `value` is the index of the argument to answer with; a
    value that names no argument runs the original and counts as
    out-of-range.
  - It writes a result only when it answers, and then runs no original. A
    lever that is armed but that its target never reached is reported
    `INERT` in `status`, with how many other machine-self RNG calls passed it
    by.
- **`gambaprobe drop`**: builds Goburin's Head through the measured loader
  route `json_parse` -> `InitItemFromJson` -> `LootGroundCreateFromItem`,
  with definition `{"w":1,"a":<seed>,"j":0,"b":98,"c":1}` and key `0-0-1-10`,
  exactly as `sigdrop` and `BuildAngelicPool` build a unique, at the player. It
  logs the built item's `itemInfoStruct` field 27 (its rarity code) and
  answers `gambaprobe drop: built rarity=<code> dropped at <x>,<y>`.
- **`gambaprobe status`**: the probe's line (`gambaprobe: off` while nothing
  is armed), the RNG lever's state, and, once `hook` has run, every row's
  counters and route, with `BUDGET SPENT` on any row that only counts. Every
  script row also shows its `idx=<short>/<gml_Script_>` and
  `byname=<same|detoured|shared|missing>`.
- **`gambaprobe off`** (also `gambaprobe 0`): disarms every lever and answers
  `gambaprobe: off`.

**The caller walk (who removes a machine).** On the `CleanUp_0` and `Alarm_9`
rows, and on the first `Create_0` of a trace window, the detour prints, before
the original runs (while the instance is still readable), one line
`gambaprobe <event>-caller id=<n> object_index=<i> x=<x> y=<y> layer=<l>
depth=<d> alarm9=<a> alarm11=<b> frame=<f>`, every value read from the
instance by the instance-handle rule and `?` when a read fails. Up to 24 stack
frames follow (`RtlCaptureStackBackTrace`), one per line:

- `#<k> gml:<row name>+0x<off>` when the frame lies inside a compiled GML
  function. The row is the compiled-code-table row with the greatest function
  address not above the frame, among the rows whose function
  `AddrIsExecutableInModule` places in `Hero_Siege.exe`. That table, the one
  `FsFindGmlRow` walks, has one row per compiled function (20,925 rows: 14,694
  object events, 6,231 scripts); `hook` reads it once into a sorted copy.
- `#<k> forgepact+0x<off>` inside this plugin; `#<k> exe+0x<rva>` for a runner
  frame in `Hero_Siege.exe` that no row covers; `#<k> <module>+0x<off>` in
  another module; `#<k> ?` otherwise.

Only the first 4 walks per row per trace window print; the row's count carries
the rest. An `object_index` other than the machine's at `CleanUp_0` would mean
`instance_change`; a `layer` the game never uses points at the depth layer.
The lines are written to the log and nothing is kept; they are the finding.

**The by-name resolution (`byname=`).** At the first `hook`, before any script
row is installed, the probe reads `GetNamedRoutineIndex` of both the short name
and `gml_Script_<short name>` for every script row (§ Static reading, "The
by-name route"). The index says what the name names:

- an index of 100000 or more names the script. `GetNamedRoutinePointer` then
  returns its `CScript*`, whose `m_Functions` is the compiled-code table row;
  the script's function is that row's `m_ScriptFunction`, cross-checked
  against the row `FsFindGmlRow` finds for `gml_Script_<short name>`;
- an index below 100000 names an entry of the runtime's functions array.
  `GetNamedRoutinePointer` then returns that entry's routine.

Every row's `hook` and `status` line prints both indices
(`idx=<short>/<gml_Script_>`) and one of:

- `byname=same`: both names resolve to the script, so `GetNamedRoutineIndex`
  prefers the script itself. This shows only which entry the one index per
  name prefers; it does not rule out a same-named functions-array entry,
  because the array is never walked here — `fnwalk` (below) walks it. Until
  `fnwalk` measures it, the store rows are blind (§ Static reading, "The
  by-name route").
- `byname=detoured`: a name resolves to a functions-array routine at another
  executable address inside `Hero_Siege.exe`. That routine is detoured too,
  with the builtin signature `HookBuiltin` uses, as a second attachment feeding
  the same row's counters.
- `byname=shared`: several rows resolve to one such routine. It is detoured
  once, as a `byname-shared` row that logs `argc` and the described arguments
  under the machine-self filter (`InitPV`, `SPV` and `GPV` differ in argument
  count).
- `byname=missing`: no attachment by name, for one of five reasons, printed
  in the row's `[...]` note: the `gml_Script_` name does not resolve; a
  name's functions-array routine has no pointer; that routine is not game
  code; no by-name slot was left; or the routine's `HookBuiltin` detour was
  not installed (not executable in the exe, no longer the routine first read,
  or `HookBuiltin` failed). Only the first means the name does not exist;
  read the note before reading the word.

Its positive control is free: a spawned machine's `Create_0` makes about 28
`InitPV` calls with the machine as `self`, whether or not the machine then
survives (`byname-visible`). Where those calls are counted depends on the
word `hook` printed for `InitPV`: on the `InitPV` row itself for `same` and
`detoured`, and for `shared` only on the `byname-shared <name> (rows
InitPV,...)` row that lists it, never on the `InitPV` row, so `InitPV
machine-self=0` alone proves nothing then. "The counts stay at 0" means the
`InitPV` row and, when `InitPV` prints `byname=shared`, that `byname-shared`
row both read `machine-self=0` while `Create_0` counted. Only then is the
route something else again: `byname-route: blind` is the finding, phase 2
must not lean on the store trace, and the question goes to a consultant, not
to another rebuild.

One per-frame tick returns at once while nothing is armed; nothing else of the
probe is on the frame path.

**The explosion watch (phase 4, our code).** The owner reports that the
machine explodes after roughly 10-14 spins and that the explosion is the only
time Goburin's Head drops, but nothing has observed it, and § Static reading
cannot place it. The watch looks for it from both sides, while the probe is
armed:

- **Machine sprites.** Each tick reads every live machine's `sprite_index`
  through `variable_instance_get` and names it with `sprite_get_name` (a
  value that is not a sprite reads `?`). No sprite is spelled or numbered, so
  it does not matter which sprite the explosion shows. A refresh that threw
  is not polled, so it cannot report every machine gone.
- **Two rings.** Every call of the eight build rows (`CreateDefaultParams`,
  `GetUniqueRepoStruct`, `LootGroundCreate`, `LootGroundCreateFromItem`,
  `CreateLootInFreePos`, `CreateItemNew`, `DropItem`, `DropUniqueItems`),
  whatever its `self`, goes into a ring of the last 64 before the machine-self
  filter, through the script's own function or a by-name slot alike. Every
  `instance_create_layer`, `instance_create_depth` and `instance_destroy` call
  goes into a ring of its own, the last 256, so a burst of effects cannot
  evict a build. A call in the transition's own step runs before the
  end-of-frame tick that notices the transition, so without the rings it
  would be lost.
- **The window.** A sprite change or a machine that is gone opens a window of
  600 frames, as does `gambaprobe window [frames]` by hand (600 by default, at
  most 3600). It first replays both rings' calls from the last 2 frames, in
  call order, then for its span prints a line for every build-row call and
  every instance create/destroy call, for any `self` (named by the probe's own
  self text, so an object other than the machine is named), and after each
  `CreateItemNew` returns, what it built (its `itemType`, its
  `itemDefinitionStruct`'s `j`/`b`/`c`, and its `itemInfoStruct`'s rarity
  `"27"` and name `"28"`, `?` for a value that does not read; Goburin's Head
  is `itemType 10 j=0 b=98 c=1`). Outside a window the trace behaves exactly
  as before. `gambaprobe off` closes an open window.
- **Two caps.** Build lines (calls, replays and `built` lines) spend a cap of
  400 per window that instance lines can never touch. Instance lines spend a
  cap of 800, at most 32 per created object (a create's object argument, or
  the object `instance_destroy` ends), so one effect repeated every frame
  cannot crowd out the rest. Both count what they dropped, the closed line
  reports both, and `status` shows the open window's dropped counts. The first
  time an object reaches its 32 in a window, one `gambaprobe window capped
  <object key>` line names it (and the closed and status lines count the
  capped objects), so a cap that hid, say, the explosion's coins is visible.
  A second transition, or `window`, while one is open extends it rather than
  opening another, and tops both caps up.
- **What a window cannot show.** A `CreateItemNew` that a window replays from
  the ring has no `built` line, because the item was read only for calls
  inside a window; `head-route`'s `GetUniqueRepoStruct` with `10, 0, 98`
  alternative covers that case. The rings count no evictions, so
  `replayed=<n>` cannot tell "nothing happened in the look-back" from "it
  happened and was pushed out of the ring".
- **Holding a window.** While a window is open, `gambaprobe status` prints
  its end, the current frame and the frames that remain (`window=open
  end=<f> frame=<now> remaining=<n>`), so an operator holding one knows when
  to send `window` again.
- **Frames.** Spans are counted in presented frames (the frame callback), so
  above 60 fps a window is shorter in seconds: 3600 frames is a minute at 60
  fps and 30 seconds at 120.
- **The lines**, fixed text that the behavior test pins byte for byte and
  Live procedure 4 reads:

  ```
  gambaprobe machine id=<id> sprite=<name> frame=<f>
  gambaprobe machine id=<id> sprite <old> -> <new> frame=<f>
  gambaprobe machine id=<id> gone frame=<f>
  gambaprobe window open reason=<sprite|gone|command> id=<id or -> frame=<f> replayed=<n> span=<frames>
  gambaprobe window extended reason=<sprite|gone|command> id=<id or -> end=<f> frame=<f>
  gambaprobe window <row> self=<self> argc=<n> <args> frame=<f>
  gambaprobe window replay <row> self=<self> argc=<n> <args> frame=<f>
  gambaprobe window built itemType=<t> j=<j> b=<b> c=<c> rarity=<r> name=<name> self=<self> frame=<f>
  gambaprobe window capped <object key> frame=<f>
  gambaprobe window closed build-lines=<n> build-dropped=<n> instance-lines=<n> instance-dropped=<n> capped=<n> frame=<f>
  gambaprobe watch: machines-seen=<n> transitions=<n> windows=<n> window=<open end=<f> frame=<now> remaining=<n>|closed> ring=<n> instance-ring=<n> build-dropped=<n> instance-dropped=<n> capped=<n>
  ```

  The last is `gambaprobe status`'s new line. `<args>` is printed the way a
  trace line prints a script's arguments. A `gone` line can also mean the
  machine was deactivated or the room changed, so the operator judges it.
- **Positive controls.** `gambaprobe window 1800` near monsters must print
  window lines for `CreateDefaultParams` and `CreateItemNew` with a `self`
  that is not a machine, and at least one `built` line whose `itemType`, `j`,
  `b` and `c` are numbers and whose `rarity` is not `?`; its closed line must
  read `build-dropped=0` (`window-control`). Each machine's first-sight line
  must name a real sprite, not `?` (`sprite-control`). The sprite trigger
  itself has no positive control (Live 3's machine instance outlived its
  last spin), so Live procedure 4 holds a commanded window across the spins
  and does not depend on it.

**Tests (our code).** `tests/test_gamba_probe_contract.py` pins the wiring on
comment-stripped source (research build only, the dispatch, the SDK-derived row
set, the held rows spliced or detoured under and never hooked twice, every
detour behind `AddrIsExecutableInModule`, the trace window, this doc's headings
and decision keys). `tests/test_gamba_probe_behavior.py` with
`tests/gamba_probe_harness.cpp` runs the decision core: lever off, every RNG
answer is the real one; lever on, only a machine-self call of its target is
answered, `count` calls and then off, a machine-self call of another builtin or
with other argument text is left untouched and counted as passed, and the inert
lever is named; one key cannot spend a row's budget, six keys moving every
frame of a spin leave a seventh key's line at the spin's end logged, and a
spent row is named. Since Live 1, the harness also covers the caller walk's
frame text (a pure function of a frame offset and the sorted row list: a GML
row, the plugin, the exe, another module, unknown), the `machine-arg=` rule
and the `byname=` status text; the contract test pins the six spawn routes
(the game scripts `instance_create` and `sCP` by their `HeroSiege::Scripts`
constants, no layer name or effect id literal), the caller walk behind the
same `AddrIsExecutableInModule` rule, the `byname=` resolution printed for
every script row, `fnwalk`'s array walk by validation (no stored address),
the extension-function rows by name, `selftest` outside the busy guard, and
this doc's ten `##` headings in order, with `pending` refused once
`### Live 2 results` exists. Phase 4 adds the explosion watch: the harness
pins the baseline (no window line without a transition or a `window`
command, the trace budgets unchanged, one first-sight line per machine) and
the target (a change or gone line opens one window, which replays its
look-back from both rings in call order, logs forward for its span, keeps
its build and instance caps apart, per object for instance lines, tops both
up when extended and is extended, not doubled), every line byte for byte; the contract test pins the sprite read by
name with no kind check, every build row's ring push before the self filter,
window lines for any `self` apart from the trace budget, the instance ring
and its cap apart from the build cap, the by-name feed for build rows, the
built-item read after `CreateItemNew` returns, the `window` verb and the
`status` line.

## Live procedure 1

The full procedure is in the toolkit workorder `forgepact-goburins-head-pity`,
context file, § "Live procedure 1". That is a local working note, so it is
repeated here whole.

- **Build:** research DLL `ForgePact\plugin_build\BloodPactPlugin_rel.dll`
  from this branch (`build.bat dev`); its SHA-256 is recorded in the session's
  log. The owner is asked before it is installed.
- **Character:** slot 14 "Sorak". The session spends in-game gold: about
  150,000 per machine, 300,000 to 450,000 for the two or three machines below.
  Saves are backed up and restored by the operator at teardown.
- **Control:** `ping` -> a line starting `pong` (seen as `pong (YYTK 4.0.1)` on
  2026-10-02). Marker: `gambaprobe status` -> a line starting `gambaprobe:
  off`.
- **Steps** (each an IPC command unless marked person):
  1. `debuglog` -> `debuglog: ACIK` (game log on). `gambaprobe hook` -> one
     line per row ending `detoured`, `detoured-under`, `shared`, `table-only`
     or `missing`. The five event rows must read `detoured`. No script row
     may read `table-only` or `missing`: the closure (`anon@1474`), `GPV`,
     `SPV`, `InitPV`, `FPV`, `GetGoldAmount`, `GoldOperationPending`,
     `GetGoldCounterHash`, `PickUpGoldCheck`, `LootBlocksUseKey`,
     `CheckUseKey`, `InputPressed`, `NetworkSendClientEffect`, `PlaySound3D`,
     `LootGroundCreate`, `LootGroundCreateFromItem`, `CreateLootInFreePos`,
     `CreateItemNew`, `DropItem`, `DropUniqueItems`, `GetUniqueRepoStruct`,
     `CreateDefaultParams`, `cpr_irandom` and `cpr_rand32`. The three
     prize-route rows the research build holds at startup read
     `detoured-under` (`LootGroundCreateFromItem`, under the item-inspect
     hook) or `shared` / `detoured-under` (`CreateItemNew`, `DropItem`). The
     summary line must read `gambaprobe hook: <n> rows, 0 missing, 0
     table-only`, with no `WARNING` line after it (check `hook-installed`).
  2. `gambaprobe spawn` -> `gambaprobe spawn: id=<n>` and, within a second,
     `gambaprobe status` showing `create=1`, `alarm9=1`, `step>=30`;
     `bp_ipc\gamelog.txt` gains a line containing `Slot Machine Spawned`;
     a screenshot shows a machine beside the player (`spawn-create`,
     `step-fires`). Wait about five seconds with nobody at the machine and
     send `gambaprobe status` again: record any RNG builtin row whose
     `machine-self=` climbed while idle (a per-frame call: its lines at the
     spin may come back `key-capped`), and from its trace lines the argument
     text of each such idle call (step 5 needs it).
  3. `gambaprobe trace`, then person: walk to the machine and use it once (one
     spin; the HUD gold falls by 10,000). Then `gambaprobe status` and the IPC
     log tail: the machine-self rows that fired, in order, with arguments;
     record the `GPV`/`SPV` keys and values that changed and any RNG builtin
     call with its arguments and result (`spin-trace`, `gold-debit`).
  4. Person: keep spinning the same machine until it explodes (expect 10-14
     spins). After each spin `gambaprobe status`, then `gambaprobe trace`
     before the next spin; at the explosion the IPC log tail: which rows fired
     (`instance_destroy`? `Alarm_0`? the closure?), the `GetUniqueRepoStruct`
     arguments and `CreateDefaultParams` count, what placed the prize, and a
     screenshot of what dropped (`explosion-trace`, `prize-trace`,
     `roll-identity`). Record with the checks any row that reads `BUDGET
     SPENT` or `key-capped=` above 0 in the explosion's `status`. For the
     call that decided the prize, record its builtin and its argument text
     exactly as its trace line prints them (`a0=... a1=...`), and whether the
     same builtin with the same text also fired on a spin that did not
     explode. Record too that call's row `repeats=` from the explosion's
     `status`, and whether its key holds repeats: an `unlogged-repeats:`
     entry naming its argument text on that row, or a trace line of that key
     in the exploding spin prefixed `(+<n> repeat(s) ...)`. Repeats on the
     decider's key mean a call of the same shape and result was folded into
     one line, so which of them decided the prize cannot be told from the
     trace: step 5 counts that as the shape firing earlier within the
     exploding spin.
  5. If step 4 showed one RNG builtin call deciding the prize, compare its
     builtin and argument text with the idle calls step 2 recorded. If an
     idle call has the same builtin and the same text, the lever cannot be
     aimed at the prize roll alone: skip the forced roll and record
     `forced-head` as not run, naming that call. Likewise, if step 4's trace
     shows the same builtin with the same argument text firing earlier within
     the exploding spin, before the call that decided the prize, or step 4
     found repeats on the decider's key, the armed lever would answer the
     earlier call: record its ordinal in the spin (or the repeat count) and
     that `forced-head` is not aimable by shape alone (an input to the
     phase-2 Decision), and do not spend the forced roll. Otherwise
     `gambaprobe spawn` a machine; then, before each spin, `gambaprobe trace`
     and `gambaprobe rng <builtin> <value> 1 args <text>` (the builtin and
     text from step 4, the text exactly as its trace line prints it, e.g.
     `args a0=real:100.000000 a1=real:5.000000`, and the value that selects
     base 98), and check that the `gambaprobe rng: ON target=...` echo
     matches the trace line's builtin and argument text character for
     character before the spin; person: one spin; then `gambaprobe status`.
     Repeat until the machine explodes. If step 4
     showed that the explosion comes on a spin the machine's state predicts
     (`explosion-rule` gold or spins), arm the lever only before that spin
     and spin the others unarmed. Record every `gambaprobe rng: answered`
     line with its spin number, and the lever line's `passed=` count after
     each spin: an answer on a spin that did not explode means that call
     shape is not the prize roll alone. An answer on the exploding spin, then
     a trace line of the same shape after it and no Goburin's Head, means an
     earlier call of that shape took the answer: record `forced-head` as not
     aimable by shape alone, not as evidence that the prize roll cannot be
     forced. Expected at the explosion:
     `gambaprobe: rng answered 1` and Goburin's Head on the ground
     (screenshot; `GetUniqueRepoStruct` arguments `10, 0, 98`). If step 4
     showed no such call, send `gambaprobe rng irandom 0 1` anyway and record
     `inert` with its `passed=` count (`forced-head`; a `not-observed` with
     `inert` is the finding, never a defect).
  6. `gambaprobe spawn` a second machine, then `gambaprobe trace`; person: one
     spin on it. Expected: its `gold_spent`-like state key starts from zero,
     not from the first machine's total (`second-machine`).
  7. `gambaprobe drop` -> `gambaprobe drop: built rarity=<code> dropped at
     <x>,<y>` and a Goburin's Head on the ground (screenshot)
     (`fallback-drop`).
  8. `gambaprobe off` -> `gambaprobe: off`. Teardown per the operator's own
     procedure (stop, inspect, restore).
- **Cases:** ordinary = one machine spun to its explosion (steps 3-4);
  outliers = the forced roll (5), a second machine's fresh state (6), the
  loader-route drop (7).
- **Checks**, names verbatim: `dll-hash`, `marker`, `control`,
  `hook-installed`, `spawn-create`, `step-fires`, `spin-trace`, `gold-debit`,
  `explosion-trace`, `prize-trace`, `roll-identity`, `forced-head`,
  `second-machine`, `fallback-drop`. `step-fires` failing while
  `hook-installed` passed means the event detour is blind: report
  `INSTRUMENT-BLIND` and measure nothing further from the event rows (the
  script and builtin rows still count). The research checks (`spin-trace`
  onward) are findings whatever they read.

## Live procedure 2

Written after Live 1 came back INSTRUMENT-BLIND (§ Results). The full
procedure is in the toolkit workorder `forgepact-goburins-head-pity-1b`,
context file, § "Live procedure 2". That is a local working note, so it is
repeated here whole. Steps 1 to 8 spend no gold; the spin half runs only on a
machine that survives.

- **Build:** research DLL `ForgePact\plugin_build\BloodPactPlugin_rel.dll`
  from this branch (`build.bat dev`, with the spawn routes, the caller walk and
  the `byname=` resolution); its SHA-256 is recorded in the session's log. The
  owner is asked before it is installed. The DLL Live 1 left installed
  (`aa5f7d03...`) lacks the spawn routes and the walk: `gambaprobe spawn game`
  answers with the usage line on it, which is how to tell them apart.
- **Character:** slot 14 "Sorak", loaded in the Town of Inoya (where Live 1's
  machines died; no travel needed). Steps 1 to 8 spend no gold. The spin half
  (step 9) needs about 300,000 to 450,000 gold; Sorak had about 299,000 after
  Live 1's restore, so the owner tops the gold up to 1,000,000 through
  HSSaveEditor's `shop.ini` gold field, with the game closed, before the
  session (owner's decision, 2026-10-04).
- **Preconditions:** every ForgePact mod off in the panel (far-sleep in
  particular: it sleeps `Collision_Prop_obj` leaves, the machine's parent),
  `dropmult` at its default x1 for the whole session, and no `citrace`,
  `jumpprobe`, `jumpscenery` or `dungeonprobe` command sent in this launch.
  The game's `ClientCreateEffect` is not called (owner's decision,
  2026-10-04).
- **Control:** `ping` -> a line starting `pong` (`pong (YYTK 4.0.1)` on
  2026-10-04). Marker: `gambaprobe status` -> a line starting `gambaprobe:
  off`.
- **Steps** (each an IPC command unless marked person):
  1. `debuglog` -> `debuglog: ACIK`. `gambaprobe hook` -> every row of Live 1
     step 1 with the same routes, plus builtin rows `instance_change`,
     `layer_destroy_instances`, `instance_deactivate_object` and `room_goto`
     reading `detoured`; every script row's line ends with `byname=same`,
     `byname=detoured`, `byname=shared` or `byname=missing`; the summary
     `gambaprobe hook: <n> rows, 0 missing, 0 table-only`, no `WARNING`
     (`hook-installed`). Record the `byname=` of `InitPV`, `SPV`, `GPV`,
     `FPV` verbatim (`byname-resolve`: pass if all four print one of the
     four words, whatever the word).
  2. `gambaprobe selftest` -> `gambaprobe selftest: irandom row calls=<a> ->
     <b>` with `b = a + 1` (`selftest-rng`). Failing means the builtin rows
     are blind and nothing from them counts: report INSTRUMENT-BLIND.
  3. `gambaprobe spawn` (the `depth` route, the control). Expected, as in
     Live 1: `Create_0`, the spawn line, then within one step `CleanUp_0` and
     `Alarm_9`, and `status` with `step=0`, `machines=0`. Record whether it
     survived instead (`spawn-depth`: pass if `step>=30` and the machine is
     on the screenshot; fail otherwise - a fail is the expected finding).
     Record every `gambaprobe CleanUp_0-caller ...` line and its `#<k>`
     frames verbatim, and the `Alarm_9-caller` lines (`cleanup-caller`: pass
     when at least one `CleanUp_0-caller` line with at least one `gml:` or
     `exe+` frame printed; fail when CleanUp_0 counted and no caller line
     printed). From `status`: `InitPV machine-self=` and `SPV machine-self=`
     and, when step 1 recorded `byname=shared` for `InitPV`, the
     `machine-self=` of the `byname-shared <name> (rows InitPV,...)` row that
     lists it (`byname-visible`: pass if `InitPV machine-self>=1`, or, when
     `InitPV` prints `byname=shared`, that `byname-shared` row reads
     `machine-self>=1`; fail only if both read 0 while `Create_0 calls>=1`).
  4. `gambaprobe spawn game` -> `gambaprobe spawn: route=game id=<n> at
     <x>,<y>` (a refusal line names what failed: record it). Within a second
     `gambaprobe status`: `step>=30`, `machines=1`; screenshot shows the
     machine beside the player (`spawn-game`). If it died, record its
     `CleanUp_0-caller` lines too.
  5. If step 4 did not survive: `gambaprobe spawn layer`, same reading
     (`spawn-layer`); if it survived, mark `spawn-layer` not-observed.
  6. If steps 4 and 5 did not survive: `gambaprobe spawn self`, same reading
     (`spawn-self`); otherwise not-observed.
  7. If no route survived: person: walk to the Town of Inoya portal, enter
     the first combat zone, fight for about 15 seconds; `gambaprobe status`
     before and after: record each RNG row's `calls=` (`rng-rows-live`: pass
     if any of `irandom`, `irandom_range`, `random`, `random_range`,
     `choose`, `cpr_irandom`, `cpr_rand32` rose; fail if all stayed at 0
     through the fight - the finding that the game's rolls do not pass
     through these rows). If a route survived, run this step after the spin
     half instead, only if the owner has the time; otherwise not-observed.
  8. If no route survived: `gambaprobe off` -> `gambaprobe: off`; teardown
     per the operator's own procedure; every spin check below is
     not-observed, and that is the session's result, not a defect.
  9. If a route survived: `gambaprobe trace`, then Live procedure 1's steps
     3 to 7, word for word, with every `gambaprobe spawn` in them replaced by
     `gambaprobe spawn <the surviving route>`, and their checks under the
     same names (`spin-trace`, `gold-debit`, `explosion-trace`,
     `prize-trace`, `roll-identity`, `forced-head`, `second-machine`,
     `fallback-drop`). Before the first spin confirm the HUD gold is at least
     300,000; if not, stop here, mark the spin checks not-observed, and the
     owner tops up as the Character line says, in a later session of this
     same procedure (its own capture `-live-3.md`).
  10. `gambaprobe off` -> `gambaprobe: off`. Teardown per the operator's own
      procedure (stop, inspect, restore).
- **Cases:** ordinary = the `depth` control and the `game` route; outliers =
  `layer` and `self` (only when `game` dies), the combat RNG control.
- **Checks**, names verbatim: `dll-hash`, `marker`, `control`,
  `hook-installed`, `selftest-rng`, `byname-resolve`, `spawn-depth`,
  `cleanup-caller`, `spawn-game`, `spawn-layer`, `spawn-self`,
  `byname-visible`, `rng-rows-live`, `spin-trace`, `gold-debit`,
  `explosion-trace`, `prize-trace`, `roll-identity`, `forced-head`,
  `second-machine`, `fallback-drop`. INSTRUMENT-BLIND is reported only when
  `hook-installed` or `selftest-rng` fails, or when `cleanup-caller` fails
  (CleanUp_0 counted, no caller line). A `spawn-*` fail, a `byname-visible`
  fail and an `rng-rows-live` fail are findings for the Decision, never
  defects.

## Live procedure 3

Written after Live 2 showed no spawn route survives its own `Alarm_9`
(§ Results, `### Live 2 results`). The full procedure is in the toolkit
workorder `forgepact-goburins-head-pity-1c`, context file, § "Live procedure
3". That is a local working note, so it is repeated here whole.

- **build**: research DLL `ForgePact\plugin_build\BloodPactPlugin_rel.dll`
  from this branch after the join's `build.bat dev`; its SHA-256 in `## Log`.
  The owner is asked before it is installed; this plan installs nothing. The
  DLL Live 2 left installed (`eb1fcf09...`) answers `gambaprobe spawn scp`
  with its usage line, which is how to tell them apart.
- **character**: slot 14 "Sorak", loaded in the Town of Inoya. Steps 1-5
  spend no gold; the spin half (step 6) needs 300,000 to 450,000. Before the
  launch, after the backups, with the game closed: set `[gold] gold` to
  1,000,000 in `hs2saves\shop.ini` through HSSaveEditor (decision in force).
- **preconditions**: every ForgePact mod off in the panel (far-sleep in
  particular), `dropmult` at x1, and no `citrace`, `jumpprobe`,
  `jumpscenery` or `dungeonprobe` command sent in this launch.
- **control**: `ping` -> a line starting `pong` (`pong (YYTK 4.0.1)` on
  2026-10-04). Marker: `gambaprobe status` -> a line starting
  `gambaprobe: off`.
- **steps** (each `hs_command` unless marked person):
  1. `debuglog` -> `debuglog: ACIK`. `gambaprobe hook` -> every row of Live
     2's step 1 with the same routes (no `table-only`, no `WARNING`), plus
     the three rows `GetVariable`, `SetVariable`, `SetVariableToUndefined`
     each ending `detoured` or `missing`; the summary `gambaprobe hook: <n>
     rows, <m> missing, 0 table-only` where `m` counts only those three
     (`hook-installed`: pass when no other row is missing or table-only and
     no `WARNING` printed). Record the three rows' words (`ext-rows-hooked`:
     pass if all three read `detoured`; fail otherwise, a finding) and the
     `gambaprobe hook: took <ms> ms (...)` line verbatim (`hook-timing`:
     pass when the line printed, whatever the numbers).
  2. `gambaprobe selftest` -> `gambaprobe selftest: irandom row calls=<a> ->
     <b>` with `b = a + 1` (`selftest-rng`). Failing means the builtin rows
     are blind: report INSTRUMENT-BLIND. Then `gambaprobe fnwalk` -> either
     `gambaprobe fnwalk: table at <n> entries` followed by one
     `gambaprobe fnwalk: <name> idx=<i> routine=<same|other|not-exe>` line
     per matching entry (record every line verbatim, and then `gambaprobe
     status` for each script row's `byname=` word), or `gambaprobe fnwalk:
     table not found (<check>)` (`fnwalk`: pass when the table validated
     and at least one line names `InitPV` or `gml_Script_InitPV`; fail when
     not found or no entry matched - a finding).
  3. `gambaprobe spawn scp` -> `gambaprobe spawn: route=scp id=<n>
     object=4644 at <x>,<y>` (a refusal line names the failed step: record
     it). Within a second `gambaprobe status`: `create=1`, `alarm9=1`,
     `step>=30`, `cleanup=0`, `machines=1`; screenshot shows the machine
     beside the player; `bp_ipc\gamelog.txt` gains `Slot Machine Spawned`;
     no `CleanUp_0-caller` line (`spawn-scp`: pass on `step>=30` and the
     machine on the screenshot). If the reply printed `object=` other than
     4644, or was refused, send `gambaprobe spawn scp oxy` once and read it
     the same way (`spawn-scp-oxy`; not-observed when not needed). From the
     same `status`: the `GetVariable` and `SetVariable` rows' `machine-self=`
     and, with `hs_ipc_tail`, one trace line of each showing a numeric key
     (`state-rows-live`: pass if `GetVariable machine-self>=1` and
     `SetVariable machine-self>=1` with the machine's `Alarm_9` having run;
     fail if both read 0 while `alarm9>=1`). From the same `status`, after
     this spawn's `Create_0` counted: `InitPV machine-self=` and, when a
     `byname-shared` row lists `InitPV`, that row's `machine-self=`
     (`byname-visible`: pass if either reads 1 or more; fail if both read 0
     while `create>=1` - the finding that the walk's detours did not see
     the by-name calls either).
  4. If no `scp` machine survived: `gambaprobe spawn stamp` -> `gambaprobe
     spawn: route=stamp id=<n> object=4644 at <x>,<y> pSpwd key=<v>
     before=<v> after=<v>`; `status` and screenshot read as in step 3
     (`spawn-stamp`). Record `before=`/`after=` verbatim
     (`stamp-readback`: pass if `after=` reads true/1; fail if it reads
     `undefined` or false, a finding). If step 3 survived, run this step as
     the second machine of step 6 instead.
  5. If neither route survived: person: play zones from the Inoya portal
     with `debuglog` on for up to the time `## Needs human judgement` ›
     "How long to farm for a natural machine" allows, checking
     `bp_ipc\gamelog.txt` for `Slot Machine Spawned` and `gambaprobe status`
     for `create=` rising with `cleanup=` not rising. A machine that appears
     and shows `step>=30` is the surviving machine (`natural-machine`: pass
     when one appeared and survived; not-observed when none appeared in the
     time; fail when one appeared and died). If none: `gambaprobe off`,
     teardown, every spin check below not-observed - the session's result,
     not a defect.
  6. On the surviving machine: `gambaprobe trace`, then 1b's Live procedure
     1 steps 3 to 7 word for word (in `ForgePact/docs/gamba-machine-research.md`
     § Live procedure 1), with every `gambaprobe spawn` in them replaced by
     `gambaprobe spawn <the surviving route>` - except step 6's second
     machine, which uses `gambaprobe spawn stamp` when step 4 did not run
     (its `stamp-readback` then reads from here), falling back to the
     surviving route if the stamp machine dies. Their checks keep their
     names (`spin-trace`, `gold-debit`, `explosion-trace`, `prize-trace`,
     `roll-identity`, `forced-head`, `second-machine`, `fallback-drop`). At
     each `status` after a spin also record the `SetVariable` trace lines'
     keys and values (`state-trace`: pass if at least one `SetVariable`
     machine-self line's value changed between two spins; fail if the rows
     counted machine-self calls but no value changed; not-observed when the
     rows read 0). Before the first spin confirm the HUD gold is at least
     300,000; if not, stop, mark the spin checks not-observed, and the owner
     tops up as the decision in force says, for a `-live-4.md` session of
     this same procedure.
  7. `gambaprobe off` -> `gambaprobe: off`. Teardown per the operator's own
     procedure (stop, inspect, restore).
- **cases**: ordinary = the `scp` route and one machine spun to its
  explosion; outliers = the `oxy` order, the `stamp` route, the natural
  fallback, the forced roll, the loader-route drop.
- **checks**, names verbatim: `dll-hash`, `marker`, `control`,
  `hook-installed`, `ext-rows-hooked`, `hook-timing`, `selftest-rng`,
  `fnwalk`, `spawn-scp`, `spawn-scp-oxy`, `spawn-stamp`, `stamp-readback`,
  `natural-machine`, `state-rows-live`, `byname-visible`, `state-trace`, `spin-trace`,
  `gold-debit`, `explosion-trace`, `prize-trace`, `roll-identity`,
  `forced-head`, `second-machine`, `fallback-drop`. INSTRUMENT-BLIND only
  when `hook-installed` or `selftest-rng` fails. Every other fail or
  not-observed is a finding for the Decision, never a defect.
- **relies on**: `Slot_Machine_01_obj` (4644) and its events
  (`gamba-machine-research.md` § Static reading; `RUNTIME_DATA_MODELS.md`
  § 20); `Alarm_9`'s `pSpwd` check and `sCP` (this file, static reading;
  `RUNTIME_DATA_MODELS.md` § 14.3 names `sCP` and `pSpwd`, order
  contradicted above); the `ApCallScript` route (`dungeon-chest-research.md`
  § Chat route, proven live 2026-10-03; Live 2's `game` route created a
  machine through it); `GetVariable` (`pet-relic-collector-research.md`,
  called by name, result open); `DebugLogAddExt` through `debuglog`
  (`gamba-machine-research.md` § Instrument); `PickUpGoldCheck` (§ 13.10),
  `GetUniqueRepoStruct`/`CreateDefaultParams` (§ 13.4) for the spin half;
  `SetVariable`/`SetVariableToUndefined`: not found in the shared references
  or research docs before this reading; the runtime's functions array
  (24-byte entries: name, routine, argument count; YYTK patch 0004's
  validation rule; `citrace dispatchdump`'s notes in `ModuleMain.cpp`,
  measured 2026-09-11, first entry `camera_create`), walked by `fnwalk`.

## Live procedure 4 (explosion watch, research build)

Written after the owner ruled (2026-10-05) that the pity fires on the
machine's explosion, which no session had observed. The full procedure is in
the toolkit workorder `forgepact-goburins-head-pity-4-payout-force`, context
file, § "Live procedure 1 (explosion watch, research build)"; its capture,
written by the live operator, is that workorder's `-live-1.md`. Both are
local working notes under the toolkit's `.claude/workorders/`, which is not
committed, so they may not exist on another machine; the outline is repeated
here.

- **build**: the research DLL from `plugin_build\build.bat dev`
  (`BloodPactPlugin_rel.dll`), installed only after the owner says so.
  `gambapity` stays off all session.
- **character**: slot 14 "Sorak". `shop.ini`'s gold is read, not changed
  (about 25 spins of 10,000 per machine), and
  `forgepact_gamba_pity.json` is recorded before and after (it is the
  successor's starting count; nothing here changes it).
- **control**: `ping` -> `pong (YYTK 4.0.1)`. **marker**: `gambaprobe status`
  -> a line starting `gambaprobe: off`, and a `gambaprobe watch:` line.
- **steps**: `gambaprobe hook` (`hook-rows`: the eight build rows and the
  three instance builtins read `detoured`, `detoured-under` or `shared`);
  `gambaprobe trace`; near monsters, `gambaprobe window 1800` until an item
  drops and the window closes (`window-control`: pass only with a `built`
  line whose `itemType`, `j`, `b` and `c` are numbers and whose `rarity` is
  not `?`, and a closed line reading `build-dropped=0`); `reveal` noted, and
  turned on only if it was off; zones from the Town of Inoya portal until the
  game places a machine (`sprite-control`: `machines=` at least 1 and a real
  sprite name on each first-sight line); from the first spin, the operator
  holds a commanded window: `gambaprobe window 3600` before the first spin,
  then again on a fixed cadence, about every 15 seconds, until the person
  reports the explosion (each re-send prints `gambaprobe window extended ...
  end=<f> frame=<f>`, and `gambaprobe status` shows `window=open end=<f>
  frame=<now> remaining=<n>`; 3600 presented frames is shorter than a minute
  above 60 fps, so the cadence keeps well inside it), so the forward any-self
  lines cover the explosion whatever the sprite trigger does; the person spins one machine until it can
  no longer be used, at most 25 spins or until the HUD gold is under 50,000,
  waits beside it 10 seconds and says how many spins and what they saw;
  `gambaprobe status` and the spun machine's id from its `PickUpGoldCheck`
  lines; a second natural machine, if one appears, the same way;
  `gambaprobe off`, then the saves and `reveal` restored.
- **checks**: `dll-hash`, `marker`, `control`, `hook-rows`, `window-control`
  and `sprite-control` must pass for the session to count. The research
  checks are `explosion-seen` (a change or gone line for the spun machine,
  with its old and new sprite), `spins-to-explode` (machine-self
  `PickUpGoldCheck` calls before the transition, and the person's count),
  `explosion-builds` (every window line around the explosion, from the held
  window and from any transition's window, replayed and forward, whether a
  `Coin_obj` appeared, each closed line's dropped counts and any `capped`
  line), `explosion-self` (the `self`
  of each build in that window) and `head-route` (a `built` line with
  `itemType=10 j=0 b=98`, or `GetUniqueRepoStruct` with `10, 0, 98`;
  `not-observed` unless the game drops the head). Their `fail` or
  `not-observed` is the finding. A negative `explosion-builds` (no build at
  the explosion) counts only when the explosion's frame, from its transition
  line or else from the spun machine's last machine-self `PickUpGoldCheck`
  line, falls inside an open window's frames (between an `open` or
  `extended` line and its end, before its `closed` line); otherwise it is
  `not-observed`.
- **what it decides**: the next phase's design. A build in the window
  around the explosion means the force rewrites the explosion's own build; an
  explosion inside an open window with no build line, no `instance_create_layer`
  of `Loot_Ground_obj` and only `Coin_obj` means the mod drops the head itself
  at the transition. A `Loot_Ground_obj` create with no build line reads as
  "build route not observed", not as "no build" (the item was built by a
  route the window did not log). No machine reaching its explosion, or an
  explosion outside every open window, leaves the route `not-observed`.

## Results

### Live 1 results

**INSTRUMENT-BLIND** (2026-10-04, Town of Inoya and one Hell zone). The capture
is the toolkit's `.claude/workorders/forgepact-goburins-head-pity-live-1.md`, a
local working note not copied here; the full `gambaprobe status` rows are in
that launch's `bp_ipc\out.txt`. No spin was measured, so no § Decision key
moved. What the session did establish:

- **Measured:** three `gambaprobe spawn` calls (the `depth` route; two in town,
  one in a Hell zone) behaved the same. `Create_0` fired inside
  `instance_create_depth`; after the plugin's command returned, in the next
  game step, `CleanUp_0` fired and then `Alarm_9`, in that order; `Step_0`
  never fired; `status` read `machines=0`. The event detours work: those three
  rows counted.
- **Measured:** the `instance_destroy` builtin row counted 0 calls in town over
  the whole window (20 in the zone, none with the machine as `self`). The RNG
  rows (`irandom`, `irandom_range`, `random`, `random_range`, `choose`,
  `cpr_irandom`, `cpr_rand32`) counted 0 in both places. `GetGoldAmount`
  (47 in town, 732 in the zone) and `CreateItemNew` (47) counted calls from
  other selves, so direct calls reach the script detours and the builtin
  detour is installed (`HookBuiltin` is `GetNamedRoutinePointer` plus an inline
  `MmCreateHook`; `dungeon-chest-research.md` counted 3,402 `instance_exists`
  calls through the same kind of row).
- **Measured:** the `InitPV`, `SPV` and `FPV` rows counted 0 calls, and `GPV`
  0 in town and 1 in the zone, while `Create_0` and `CleanUp_0` each ran to
  their end (the event detour prints after the original returns, so no
  exception unwound them).
- **Static reading:** `Create_0` has one exit and makes about 80 calls by
  name to `InitPV`, `SPV` and `GPV`, and `CleanUp_0` calls `FPV` by name
  (§ Static reading, "What `Create_0` runs, and the parent chain"). So the
  zero counts on those rows are the detour missing a call route, not the game
  skipping the calls (§ Static reading, "The by-name route"): not observed by
  the detour.
- **Not established:** who removes the machine. `CleanUp_0` ran with no
  `instance_destroy` call in the window, and `Alarm_9` still ran after it, so
  the removal is not a plain `instance_destroy` from GML. The candidates are a
  runner path (`instance_change`, a layer operation on the runner-managed depth
  layer the `depth` route lands on, room or zone state code), another object's
  Begin Step, or inherited parent code. Live 2's `CleanUp_0-caller` lines name
  the frames.
- **Not established:** whether the RNG rows can see the game's rolls at all.
  Their zero counts came with no machine that ever stepped, so they measured
  nothing about the machine; Live 2's `selftest-rng` and `rng-rows-live` are
  the controls.

Live 2 (below) corrects one reading here: `CleanUp_0` runs nested inside
`Alarm_9`, so the order above is the order the two event lines printed in, not
the order the events began in.

### Live 2 results

**Instrument proven; no machine survived** (2026-10-04, the Town of Inoya,
then the first combat zone outside it; slot 14 "Sorak"; the research DLL built
from this branch, SHA-256 `eb1fcf09...87f2c0`). The capture is the toolkit's
`.claude/workorders/forgepact-goburins-head-pity-1b-live-2.md`, a local working
note not copied here. Each finding names the check it comes from, under the
name § Live procedure 2 gives it.

- **Measured, the instrument:** `gambaprobe hook` installed 41 rows: 0
  missing, 0 table-only (32 detoured, 1 detoured-under, 8 shared), and no
  `WARNING` (`hook-installed`: pass). It took about 9 seconds, and the game was
  frozen for them. `gambaprobe selftest` moved the `irandom` row from 0 calls
  to 1 (`selftest-rng`: pass), so the builtin rows see a call. The caller walk
  named frames by 20,893 compiled-code rows of game code.
- **Measured, by name:** every script row read `byname=same` (24 of 24, with
  `InitPV`, `SPV`, `GPV` and `FPV` among them; `byname-resolve`: pass). Both
  names resolve to the script itself (`byname=same` shows only which entry
  `GetNamedRoutineIndex` — one index per name — prefers; it does not rule out a
  same-named functions-array entry, because the array was never walked here —
  `gambaprobe fnwalk` walks it). Across four `Create_0` runs with a machine as `self`,
  `InitPV`, `SPV`, `GPV` and `FPV` all stayed at `machine-self=0`, and no
  `byname-shared` row existed (`byname-visible`: fail). The by-name store calls
  the static reading puts in `Create_0` are **not observed by the detour**, and
  this instrument cannot see them: the store route is blind. Phase 2 cannot
  lean on a store trace.
- **Measured, the spawn routes:** `depth` (the control), `game`, `layer` and
  `self` each created a machine (`gambaprobe spawn: route=<name> id=<n>`,
  object 4644), and every one was gone before its first `Step_0`. After the
  four spawns, `status` read `create=4 alarm9=4 cleanup=4 step=0` and
  `machines=0`, and the screenshots show no machine (`spawn-depth`,
  `spawn-game`, `spawn-layer`, `spawn-self`: fail). The `game` machine's
  `Create_0` ran under the game's own `instance_create` script (a
  `gml_Script_instance_create` frame in its caller walk). At `Create_0`, the
  `depth` and `self` machines read `layer=-1 depth=0`, the `game` machine a game
  layer (`layer=19940`) and the `layer` machine the layer it was created on
  with the player's `layer` value (`layer=20351`); by `Alarm_9`
  the `depth` machine read a layer and a depth of its own (`layer=20359
  depth=-419`). The route, the layer and the identity of the caller all varied,
  and the outcome did not.
- **Measured, who removes it:** the machine's own `Alarm_9`
  (`cleanup-caller`: pass). On all four spawns, the `CleanUp_0` caller walk
  shows, below the plugin's detour and eight runner frames, a frame in
  `gml_Object_Slot_Machine_01_obj_Alarm_9`: `CleanUp_0` runs from inside the
  machine's `Alarm_9`, through the runner. `Alarm_9`'s own walk shows only
  runner frames under the plugin (the alarm dispatch), at `alarm9=0
  alarm11=2`. Both caller lines carry the same plugin frame number as the
  spawn. Live 1 printed the `CleanUp_0` event line before the `Alarm_9` one
  because an event line prints after its original returns, and `CleanUp_0`,
  nested inside `Alarm_9`, returns first.
- **Measured, no other row saw the machine:** the probe's `machine-self` total
  (3 after one spawn, 12 after four) is the three event rows that fired,
  `Create_0`, `Alarm_9` and `CleanUp_0`, once each per spawn. So whichever
  runner routine `Alarm_9` removes the machine through, no script or builtin
  row (`instance_destroy`, `instance_change`, `layer_destroy_instances` and
  `instance_deactivate_object` among them) counted it with the machine as
  `self`. Which routine it is, is **not established**.
- **Measured, the RNG rows in combat:** about 15 seconds of combat, no mod on
  (`rng-rows-live`: pass). `cpr_irandom` went from 0 calls to 1,308 and
  `cpr_rand32` from 0 to 1,386; `irandom`, `irandom_range`, `random`,
  `random_range` and `choose` did not move (`irandom` kept the self-test's one
  call). Combat's rolls pass through the `cpr_*` script rows, which the detour
  sees; the builtin RNG rows saw none of them. Whether the machine's prize roll
  passes either is **not established**.
- **Not observed:** every spin check (`spin-trace`, `gold-debit`,
  `explosion-trace`, `prize-trace`, `roll-identity`, `forced-head`,
  `second-machine`), since no machine lived to spin; `fallback-drop` was not
  run, since the procedure runs it only once a route survives. No gold was
  spent, and the save was restored from the session's own backup afterwards.
- **Next:** phase 1c, a separate workorder on this branch (owner's decision,
  2026-10-04). It reads, locally, what `Alarm_9` checks before it removes the
  machine, builds a spawn that meets that check, and falls back to a machine the
  game placed itself.

### Live 3 results

**A natural machine spun; no spawn route survived; the state route is blind**
(2026-10-04, the Town of Inoya and the Fields of Battle; slot 14 "Sorak"; the
research DLL built from this branch, SHA-256
`1ee2b7542445ff08477c189eefb802dda6dcd75b81509f09d553211df66f5379`). The
capture is the toolkit's
`.claude/workorders/forgepact-goburins-head-pity-1c-live-3.md`, a local working
note not copied here. Each finding names the check it comes from, under the name
§ Live procedure 3 gives it.

- **Measured, the instrument:** `gambaprobe hook` installed 44 rows: 0
  table-only, and the only 3 missing are the three extension-function builtin
  rows (`hook-installed`: pass), in `8612 ms` (events 424, byname 2, scripts
  2022, builtins 828, table 5334) (`hook-timing`: pass); `selftest` moved the
  `irandom` row 0 -> 1 (`selftest-rng`: pass).
- **Measured, the extension functions do not resolve by name:** `GetVariable`,
  `SetVariable` and `SetVariableToUndefined` all read `(not found by name, st=4)
  missing` (`ext-rows-hooked`: fail). The static reading's state route is not a
  name-resolvable builtin in the table YYTK's lookup reads, so
  `state-route: blind`, and the `stamp` route cannot work either.
- **Measured, `fnwalk`:** `gambaprobe fnwalk: table not found (no aligned qword
  equal to camera_create's routine with eight valid entries)`. The validation
  seed does not locate the functions array, so no `byname=detoured|shared` row
  was made and the walk answered nothing about the store scripts (`fnwalk`:
  fail).
- **Measured, the `scp` spawn:** `spawn scp` created a machine
  (`route=scp id=262068 object=4644 at 912,822`, with the game's own `sCP` frame
  in its caller walk, so the `(x, y, object)` order is the right one), yet the
  machine was gone at its first step (`create=1 alarm9=1 step=1 cleanup=1`,
  `machines=0`, a `CleanUp_0-caller` line) (`spawn-scp`: fail; `spawn-scp-oxy`:
  not-observed, the order was not wrong). The `stamp` route was refused before
  it could read anything: `route=stamp refused - dispatch failed: GetVariable,
  st=4` (`spawn-stamp`: fail, `stamp-readback`: not-observed). So
  `spawn-route: natural` — only a machine the game placed itself survived.
- **Measured, the natural machine:** playing zones from the Inoya portal
  produced a machine that survived and spun: `machines=2 (id 494622,494624)`,
  `step=14219` (`natural-machine`: pass). Over that machine's `Create_0` the
  `InitPV` row still read `machine-self=0` and no `byname-shared` row existed
  (`byname-visible`: fail), so the by-name store route stays blind
  (`byname-route: blind`).
- **Measured, the spin (`spin-trace`, `gold-debit`):** each spin debits 10,000
  gold through `PickUpGoldCheck` with the machine as `self`
  (`a1=real:-10000.000000`, one call per spin; `GetGoldAmount` with the machine
  as `self` read `1001114 -> 991114 -> 981114 -> 971114 -> 1011114 -> ...`).
  Sixteen `PickUpGoldCheck` calls fired over the window, with no
  `instance_destroy` carrying a machine argument.
- **Measured, the payout (`explosion-trace`, `prize-trace`, `roll-identity`):**
  the prize roll is the script `GetUniqueRepoStruct` with the machine as `self`
  (`argc=3 a0=real:1.000000 a1=int64:0 a2=real:72.000000`, not `10, 0, 98`),
  whose randomness goes through the `cpr_irandom` and `cpr_rand32` script rows
  (`scope=machine-event`); a builtin RNG row did not move, which is not-observed. The prize is built by
  `CreateDefaultParams` (`(0,72,true)` then `(0,11,undefined)`) and placed by
  `LootGroundCreate` -> `CreateLootInFreePos` -> `instance_create_layer`
  (`Loot_Ground_obj`, plus `Coin_obj`, `Loot_Pillar_obj`, `Impact_Sound_obj`,
  `Visual_Effect_Simple_obj`). The machine object is not destroyed by a payout:
  `machines=2` (the same two ids) before and after.
- **Measured, the roll is a script:** the `irandom` lever armed
  for the prize roll stayed `INERT` — no machine-self builtin RNG call reached
  it, which is not-observed on that row (the builtin rows are unproven against
  a compiled call), not proof the roll passes no builtin
  (`gambaprobe rng: ... lever=on INERT - armed, but no machine-self irandom call
  has reached it`; `forced-head`: not-observed). `state-trace` is not-observed
  (the `SetVariable` row reads `missing`).
- **Not observed:** a second machine's fresh state (`second-machine`), since no
  `gambaprobe spawn` route survived; and the loader-route drop
  (`fallback-drop`), since `gambaprobe drop` was refused by the auto-mode
  permission classifier before it was sent, and was not worked around.

Live 3 labels the six § Decision keys for the first time (below).

## Decision

roll-route: script

explosion-rule: not-observed

drop-route: LootGroundCreate

counter-route: both

fallback-drop: not-run

pity-design: force-script

Live 3 measured a natural machine's spin and payout (the `scp` and `stamp`
spawn routes both failed, so no spawned machine could be spun). The prize roll
is the script `GetUniqueRepoStruct` with the machine as `self` (arguments `1, 0,
72`), whose randomness goes through the `cpr_irandom`/`cpr_rand32` script rows
(`scope=machine-event`), so `roll-route` is `script`; the `irandom` lever stayed
`INERT` with the machine as `self`, which is not-observed on that row (the
builtin rows are unproven against a compiled call), not proof the roll passes no
builtin. The payout comes on that random roll, and
the machine is not destroyed by it (`machines=2` before and after) — what ends a machine was not observed — so
`explosion-rule` is `not-observed`; the prize is placed by `LootGroundCreate` ->
`CreateLootInFreePos` -> `instance_create_layer`, so `drop-route` is
`LootGroundCreate`. A pity counter can count both events from the machine-self
hooks that fired — spins through `PickUpGoldCheck` (`a1=-10000`, one per spin)
and payouts through `GetUniqueRepoStruct`/`LootGroundCreate` — so
`counter-route` is `both`. `fallback-drop` is `not-run` (the `gambaprobe drop`
command was refused by the auto-mode permission classifier before it was sent,
and was not worked around). With the roll a script and the drop a script, phase
2 forces a named script's result rather than answering a builtin RNG, so
`pity-design` is `force-script`; `drop-ourselves` (the loader route) is the
fallback.

- **`roll-route`** (`roll-identity`): which call, with the machine as `self`,
  decides the prize. `builtin`, `script`, `method` or `not-observed`.
- **`explosion-rule`** (`explosion-trace`): what ends a machine. `gold`,
  `spins`, `random` or `not-observed`.
- **`drop-route`** (`prize-trace`): what places the prize on the ground. The
  name of the script or builtin, or `not-observed`.
- **`counter-route`** (`spin-trace`, `explosion-trace`): which events a pity
  counter can count from the hooks that fired. `spins`, `explosions`, `both`
  or `not-observed`.
- **`fallback-drop`** (`fallback-drop`): `proven`, `failed` or `not-run`.
- **`pity-design`**: phase 2's shape. `answer-roll` (answer the game's own roll
  inside its call; preferred), `force-script` (force a named script's result),
  `drop-ourselves` (drop the item through the loader route when the roll cannot
  be reached), or `blocked` (none of the three is available; this goes to the
  owner).

## Not established

Live 1 was meant to settle these and, INSTRUMENT-BLIND, settled none of them.
Live 2 settled who removes a spawned machine (its own `Alarm_9`, on every
route) and that no spawn route of the four survives. Live 3 measured a natural
machine's spin and payout and answered several of the rest; what stays open:

- **Why a machine `sCP` creates still dies.** `spawn scp` created a machine
  (object 4644, the game's own `sCP` frame in its caller walk) that its own
  `Alarm_9` removed in its first step, so `sCP`'s `SetVariable(key, true)` stamp
  did not take effect — consistent with the extension functions not resolving by
  name. Whether the reading of `sCP`'s stamp is wrong, or the stamp uses a route
  the probe's rows cannot see, is open.
- **How `Alarm_9` reaches the machine's state.** The static reading put it
  through `GetVariable`/`SetVariable`/`SetVariableToUndefined` by name, but none
  of the three resolves by name from the plugin (`state-route: blind`), so the
  real state route is still unobserved (`state-trace`: not-observed).
- **Where the functions array is.** `fnwalk` did not locate it by validation
  (`table not found`), so whether it holds same-named entries for the store
  scripts is still open, and how the by-name store calls reach `InitPV`, `SPV`,
  `GPV` and `FPV` without passing the inline detour at the script's own entry
  (`byname=same`, `machine-self=0`) is still open (`byname-route: blind`).
- **What the three unique picks and seven parameter builds in `Step_0`
  correspond to** (prize tiers?). `GetUniqueRepoStruct` ran with arguments
  `1, 0, 72` and `CreateDefaultParams` built `(0,72,true)` and `(0,11,
  undefined)`, but the mapping to the picks and builds was not read.
- **The machine's state keys** in the protected store (spin count, gold spent,
  threshold): the state rows were blind, so no key was read.
- **The charm's rarity code** (7 Angelic or 10 Unholy, or neither):
  `fallback-drop` did not run.
- **Where the odds live.** The price is measured at 10,000 gold a spin (the
  gold debit through `PickUpGoldCheck`); the 750 (or whichever odds constant)
  was not seen, and neither appears as a literal in the bodies read.
- **The explosion's route and `self` (phase 4).** By the owner's report the
  machine explodes after roughly 10-14 spins, cannot be used afterwards, and
  the explosion is the only time Goburin's Head drops; none of that is
  measured. Which call the explosion makes, with which `self` (the machine,
  one of its events, or another object), whether it builds an item at all,
  how it changes the machine (sprite, `Slot_Machine_01_Destroyed_spr` or
  another, or removal) and how many spins it takes are open, and so is the
  route the head takes when the game drops it. Live 3 cannot answer it: after
  its last spin the machine still existed, and its probe logged only calls
  whose `self` was the machine. § Live procedure 4 measures it.
