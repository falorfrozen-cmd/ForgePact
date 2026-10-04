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

- **Phase:** research (phase 1 of 2). Nothing in this phase is player-visible,
  and the player build is free of it.
- **Instrument:** `gambaprobe` (§ Instrument), research build only, on the
  ForgePact branch `134-goburins-head-pity-research`.
- **Live procedure 1:** written (§ Live procedure 1), not yet run. § Results is
  empty and every § Decision key reads `pending` until it has run.
- **Owner's decisions (2026-10-04):** the pity counts explosions, with the gold
  equivalent shown; the session character (slot 14 "Sorak") has enough gold
  for the full procedure; measure first, then plan the mod.

## Static search

Every line is labelled. **Static search** means the name or index is in
`hs-game-sdk` or the game's executable; it says nothing about behaviour.

- **Static search:** object `Slot_Machine_01_obj` (`HeroSiege::Objects`, index
  4644). Sprites `Slot_Machine_01_spr`, `Slot_Machine_01_Destroyed_spr` and
  `Slot_Machine_Icons_spr`; sound `Las_Gambas_snd`.
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
- **Not established**: not known yet. § Not established lists what Live 1 is
  meant to settle.

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
  `instance_create_depth` and `instance_create_layer`. A builtin row's `self`
  is the call's `self`, read by the instance-handle rule. `hook` refuses while
  `citrace`, `jumpprobe` or `jumpscenery` holds one of them, as `jumpprobe
  hook` does.
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
  `table-only` script row. Sent again, it re-arms and starts a new trace
  window.
- **`gambaprobe trace`**: starts a new trace window (every row's and every
  key's budget), answering `gambaprobe trace: every row's trace budget starts
  over`. The counts continue.
- **`gambaprobe spawn`**: one `instance_create_depth` of `Slot_Machine_01_obj`
  at the local player, depth 0, answering `gambaprobe spawn: id=<n>`. It
  starts a new trace window first, so the new machine's `Create_0` is
  described. The `Create_0` and `Alarm_9` rows are its positive control.
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
  counters and route, with `BUDGET SPENT` on any row that only counts.
- **`gambaprobe off`** (also `gambaprobe 0`): disarms every lever and answers
  `gambaprobe: off`.

One per-frame tick returns at once while nothing is armed; nothing else of the
probe is on the frame path.

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
spent row is named.

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

## Results

Live procedure 1 has not run yet. Its results are recorded here, under
`### Live 1 results`, once it has.

## Decision

roll-route: pending

explosion-rule: pending

drop-route: pending

counter-route: pending

fallback-drop: pending

pity-design: pending

Live 1 gives each key exactly one label, backed by the named check; until then
none has one.

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

Live 1 is meant to settle these. Until it has run, none of them is known.

- **The spin's gold debit path.** `PickUpGoldCheck` is the only balance writer
  per `RUNTIME_DATA_MODELS.md` § 13.10, but no event calls it directly.
- **Whether the prize roll is per spin or per explosion.**
- **The explosion threshold**, and whether it is gold, a spin count or random.
- **What the three unique picks and seven parameter builds in `Step_0`
  correspond to** (prize tiers?).
- **The ground placement**: which script or builtin puts the prize on the
  ground.
- **The machine's state keys** in the protected store (spin count, gold spent,
  threshold).
- **Whether a spawned machine behaves as a zone-generated one.**
- **The charm's rarity code** (7 Angelic or 10 Unholy, or neither).
- **Where the price and the odds live.** Neither 10,000 nor 750 appears as a
  literal in the `Create_0`, `Alarm_0` or closure bodies read, so they are in
  the constant tables or the protected store, and only the trace will show
  them.
