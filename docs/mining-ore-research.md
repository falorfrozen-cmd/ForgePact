# Mining Ore Amount — research, 2026-09-21

Status: **verified in play on 2026-09-23** (see "Live verification");
the multiplier adapter described below is unchanged since. The extra rolls
added on 2026-09-28 (issue #36) **paid out in a live game on the research
build the same day** (Live procedure 1), and Live procedure 2 confirmed the
shipped build through the panel on 2026-10-02. Mining XP and the floating text
per roll, bonus finds under rolls and the Miner's Helmet with rolls are still
not measured. See "Extra rolls" at the end.
Based on ForgePact `eed66427bda39fcd4ea66096934528f5efb9a2b5`. Nothing has been
published, and no EXE version has been changed for this experiment.

## Observed interface

Read-only local static inspection identified a mining reward path that calls
`MiningNodeStepMain` → `LootGroundCreate` directly. The older `dropmult ore`
adapter targets other drop routines; earlier research did not establish that it
changes mining rewards. The new UI therefore uses its own `miningore` command.

At the ground-reward boundary, zero-based argument 2 is the item type and argument
3 is the parameter struct. Material is the SDK's `ItemType::Material` (14).
The struct's `b` is its base definition, **not** its quantity. For the observed
mining path, material bases 27–32 identify Copper, Iron, Gold, Ruby, Jade and
Tarethium ore. The optional `o` carries the stack quantity; absent means one.
The item editor's material schema independently describes that same field.

These are paraphrased interface findings. No game function body or disassembly
is included. Local inspection matched the analyzed game build's `.text`, `.rdata`
and `.data` section bytes to the user's installed copy; this does not establish
compatibility with every game version.

## Adapter boundaries

`plugin/include/ForgePact/MiningOreMod.hpp` is included after `HookOneScript`.
Both scripts use SDK names and must receive real native detours. A table-only
fallback is insufficient on the game's direct compiled call path; if either
native detour is missing, the effective multiplier remains one.

An RAII scope remembers the current mining node only during its original call.
A loot call qualifies only in that scope, with the same caller and the material
type/base whitelist. Non-ore rewards, ore from other callers, gems, XP and
prospecting keep their original path. Re-entrant reward calls cannot multiply
an already scaled quantity again.

For a positive whole-number quantity, a shallow `variable_clone(params, 0)`
preserves the original struct and all fields except the copied `o`. The changed
field is read back before dispatch. The full original argument vector is
forwarded with only the copied struct substituted. The native reward routine is
called exactly once, outside the preparation exception handler: a native failure
cannot trigger a second drop. Invalid parameters or a failed clone/set operation
use the unchanged reward once. A conservative quantity bound prevents overflow.

GameMaker documents depth zero as copying just the outer struct:
[variable_clone](https://manual.gamemaker.io/beta/en/GameMaker_Language/GML_Reference/Variable_Functions/variable_clone.htm).
The built-in is present in the inspected game, but actual runtime invocation and
pickup/stack handling still need the live check below.

No new frame watcher, map enumeration, RNG roll, repeated reward call or saved
node edit was added for the multiplier (the extra rolls below repeat the whole
completion on purpose, behind their own setting). x1 installs nothing on a fresh process. After a nondefault
setting installed the hooks, x1 uses immediate passthrough until process exit.
The existing mod-state writer reports readiness; one-time command/reward/failure
messages aid the trial. Per-call counters are development-build-only.
First-use flags `stepObserved` and `oreObserved` additionally distinguish an
installed hook from an executed mining/reward path, without per-frame counters.
For the helmet, `lastRewardReason` records the authorization outcome separately
from the equipment/readiness status.

## Verification

- Baseline before implementation: default passthrough passed, target scaling
  assertions failed, so the harness can distinguish a no-op adapter.
- `tests/test_mining_ore_behavior.py` compiles the actual adapter with a fake
  game boundary. It checks all six ore types, missing quantity, x1 reset,
  exclusion of other loot/callers, invalid quantities, copy/read/write failures,
  exceptions, re-entry and refusal of table-only hooks.
- `tests/test_mining_ore_panel.py` checks the distinct default/command, limits,
  persistence, explicit live reset, unchanged gold behavior and rejection of
  unknown settings through the HTTP API with game IPC mocked.
- `plugin_build/build.bat release` succeeds with the pinned toolchain. This
  establishes compilation, not actual in-game correctness or performance.
- Full Python regression discovery: 927 tests collected, 921 passed and 6
  skipped in this local checkout. No failures remain.
- An isolated panel server (temporary settings, game IPC mocked) was exercised
  in a browser: type x5, reload and retain x5, reset to x1, maximum x10,
  unchanged Gold, and simulated ready/mismatched/unavailable plugin states.
  No browser errors were reported; the new control had no horizontal overflow
  at 390 px. The shipping panel was subsequently opened for the actual trial.

The experimental Release DLL was installed locally only after verifying the game
was closed and the installed YYToolkit matched the build pin. The previous DLL
and settings were backed up and hashed; original settings remain unchanged.
This installation is test preparation, not a live verification result.

Initial live observation: the game loaded the new plugin, both named native
hooks reported installation, and `modstate.json` confirmed `ready: true`,
`multiplier: 4`, `unavailable: false` after the user selected x4. No mining
reward dispatch had been observed yet at that checkpoint; pickup quantity,
XP and live x1 reset therefore still require confirmation.

For the live trial: use a fresh game session and ordinary mining at x1, then x5,
then x1 again. Check inventory/material-tab totals as well as ground labels and
the one-time `miningore: first reward dispatched N -> M` line in `bp_ipc/out.txt`.
Random ore types can differ between nodes: compare the logged original/scaled
quantity and the actual pickup, not only totals from different random nodes.
Verify XP and non-ore rewards remain normal. With Auto-prospect enabled, also
confirm manually prospecting the mined ore retains normal prospecting behavior.
Do not report the feature as verified until these observations are recorded.

## Live verification (2026-09-23)

- Slider at x10 with the Miner's Helmet off: the helmet's check logged
  `ore bonus skipped - Miner's Helmet is not equipped or unreadable (ore slider
  x10 applies)` and the adapter then logged `miningore: first reward dispatched
  6 -> 60 (one native drop call)`. The user confirmed the amounts in play.
- Helmet worn (x4, replacing the slider): `10 -> 40` and `13 -> 52`.
- Not measured: mining XP, non-ore rewards and manual prospecting of mined ore.
  The adapter changes only the ore stack's `o`, so none of them is expected to
  move.

## Extra rolls

Status (2026-09-28, issue #36): **built, covered by the harness, and paid out
live on the research DLL** (Live procedure 1, below): every re-run of the
completion paid ore, rolls 3 and 10 gave three and ten stacks, the multiplier
scaled every run, and the XP the dig reached counted once. Bonus finds were
**not observed live** (the character's bonus-find stats all read 0), and the
Miner's Helmet case was not run. Live procedure 2 (2026-10-02, below)
confirmed the shipped build through the panel: rolls 3 dropped three stacks and
rolls 1 one. What is still not measured: mining XP and the floating text per
roll (not observed: the character's mining level was at the cap), bonus finds
under rolls, and the Miner's Helmet with rolls. The panel row is Mining Ore Extra Rolls
(`drops.mining_ore_rolls`, 1-10, default 1, off by default) and the plugin
command is `miningrolls N`. The section "What the game does at a dig" is the
static reading; what the sessions measured is under "Live procedure 1
(2026-09-28)" and "Live procedure 2 (2026-10-02)", and where the reading and a
measurement disagree, the measurement is said so there.

### What the game does at a dig (static reading)

Read locally on 2026-09-28 and written down in our own words; the full labelled
reading is the hub's
[`docs/models/mining-reward-spec.md`](../../docs/models/mining-reward-spec.md),
and the facts every module needs are in the hub's
[`docs/RUNTIME_DATA_MODELS.md` § 12](../../docs/RUNTIME_DATA_MODELS.md#12-mining).

- The ore kinds and counts a node pays were fixed when the node was created
  (`Mining_Node_obj`'s Create event, gated by `irandom` and two stat queries;
  `Asgard_Special_Node_obj` with fixed counts). At dig time `MiningNodeStepMain`
  only counts that list and drops each kind once, through one
  `LootGroundCreate` call per kind. So there is no dig-time roll for which ore
  or how much, and the Mining Ore Multiplier and "more copies of the same ore"
  are the same thing.
- The dig's own random part is the bonus finds: several independent rolls,
  each gated by one of the digger's stats (queries 693-700 through
  `ReturnSpecificStat`) and an inclusive `irandom` draw, paying type-15 items,
  one to three `Goblin_Ore_obj`, and some type-14 and type-13 items. These are
  the "special mats" issue #36 asks for. A character whose stats are all 0 never
  sees one.
- After the reward the step calls, directly, `MiningAdd`, `ExperienceUpdate`
  and `GuildExperienceAdd` (on two branches), `CombatText`, up to four
  `quest_exists`/`update_quest` pairs, `PlaySound3D`, a `Mining_Effect_obj`
  and `NetworkSendClient`; then the node's `hp` goes 1 -> 0 (that last part
  measured on 2026-09-23, `miner-helmet-prototype.md` § "Ownership fix").
  Live procedure 1 measured `ExperienceUpdate` and `GuildExperienceAdd` once
  per completion run, but saw no `MiningAdd`, `CombatText` or `update_quest`
  call from a dig through native detours: not observed live, and unexplained
  (see below).

### The mechanism

An extra roll is the game's own completion run again on the same node, in the
same step, from inside the existing `HookStep` detour, after the original
`MiningNodeStepMain` call returned. It runs only when `HookLoot` recognised an
ore stack during that original call, and only while the rolls are above 1.
Before each extra run the plugin sets the node's `hp` back to 1 and
`miningQue` to true (the route Vein Resonance proved live on 2026-09-23) and
calls the step trampoline with the original arguments. During an extra run a
thread-local flag makes five pass-through detours skip the game's call:
`MiningAdd`, `ExperienceUpdate`, `GuildExperienceAdd`, `update_quest`, and the
shared `CombatText` detour. So XP (mining, character, guild), quest progress
and the floating XP text count once per node, while ore, bonus finds, sound and
the hit effect happen once per roll. An extra run that pays no ore ends the
loop, and afterwards the node's `hp` is forced to 0, so a node is never left
diggable twice. The multiplier (or the helmet's x4) scales every stack of every
run; the helmet's pulse and Vein Resonance follow the original run only.

`miningrolls` above 1 installs the step/loot pair, the four pass-through
detours and the shared `CombatText` detour, once a session; all seven must come
up native, or the rolls stay at 1 with one `miningrolls: unavailable - <script>
...` line and the multiplier keeps working. The cap is `kMaxRolls = 10`, refused
in the plugin's parser as well as clamped by the panel.

The research build installs the mining pair before `InstallItemInspectHooks`
(which table-hooks `LootGroundCreate`); before that change the Mining Ore mod
was unavailable in the research DLL, and a live session run on it would have
measured the instrument, not the game.

### Rejected routes

- **Copying the ore stacks N times from `HookLoot`** (extra `LootGroundCreate`
  calls with cloned params). It reproduces only the node's fixed list, so it is
  the quantity multiplier written as separate stacks and can never produce a
  bonus find, which is the point of issue #36.
- **Re-implementing the bonus rolls in the plugin** (calling
  `ReturnSpecificStat` and `irandom` ourselves, then `LootGroundCreate`). It
  would carry every roll site, most of whose caps and bases are computed in code
  not read to the end, and would drift from the game on the next patch. It is
  the fallback only if Live procedure 1 finds that the re-run does not pay
  (`rerun: no-reward`), and that fallback is a new plan with the owner.

Also set aside: editing the node's list before the dig (cannot give a bonus
find, and the list is built in an object event no name can hook), and the
game's Blood Pact extra ore (monster-side `DropOres`/`DropOreMaterials`, not
mining).

### The CombatText decision

`CombatText` already had one native detour, the Experience slider's "N XP"
rescale in `StatsManager.hpp`, and `HookOneScript` puts the inline detour in
only on a script's first install. A second detour from the mining code would
have come up table-only in one install order (rolls refused for anyone with the
Experience slider moved) and blinded the XP text fix in the other. So there is
one detour, in `plugin/include/ForgePact/CombatTextHook.hpp`, installed once by
whichever asks first; it rescales the XP text for the Experience slider and is
silenced during an extra roll. The alternative, letting the floating XP text
repeat per roll, was rejected: the README promises it once per node, and each
roll would show a fresh "N XP" with no XP behind it.

### Live procedure 1 (2026-09-28)

The session the workorder `forgepact-issue-36-extra-ore-rolls` calls Live
procedure 1 (its capture, `forgepact-issue-36-extra-ore-rolls-live-1.md`, stays
on the researcher's machine). Date 2026-09-28, 17:38-18:12 UTC. Build: the
research DLL from `plugin_build\build.bat dev` at ForgePact `98dbf53`, sha256
`e1c5eb9911e37786072d13ba53a5a10fa7a336c663a1d049e97956807ccf414e`, matching
the hash the session's lease read. Character: hero Suh (save slot 2), digging
Copper Veins in The Highland Mines (zone level 33). `drops.mining_ore_rolls` was absent from
`forgepact.json` before launch (so 1), and `stat exp 2` was the first command
to install `CombatText`, so the Experience slider's detour came up first. The
saves were backed up before launch and restored clean afterwards.

Route tokens: **`rerun: pays-out`** and **`bonus: not-observed`**. The
numbers are in the hub's `hs-game-sdk/curated/mining_reward_measurements.json`
(MR4-MR9), each reproduced by `tests/test_mining_reward_model.py` or saying
why not.

| Check | Verdict | Line it rests on |
|---|---|---|
| dll-hash | pass | lease `dll_sha256 e1c5eb99...` equals the build's |
| marker | pass | `miningrolls: rolls=1 rollsReady=0 steps=0 extraRuns=0 extraRunsUnpaid=0 xpPassed=0 xpSilenced=0 MiningAdd=0/0 ExperienceUpdate=0/0 GuildExperienceAdd=0/0 update_quest=0/0 CombatText=0/0 combatTextFirst=none snapshots=0` |
| control | pass | `miningore: steps=0 scopedLoot=0 changed=0 multiplier=1 nativeReady=1` |
| stats-read | pass | `miningrolls stats`: ten lines, `id=692..700,703 value=0` |
| rolls1-baseline | pass | owner-confirmed ore reward; `extraRuns=0`; `scopedLoot=0` is the adapter's own pass-through at x1/rolls 1 |
| rerun-pays-out | pass | `miningrolls: first extra roll paid 1 ore stacks`; `extraRuns=2 extraRunsUnpaid=0` |
| rolls3-stacks | pass | `scopedLoot` 0 -> 3; the owner: "saw 3 drops this time" |
| rolls3-x5-scaled | pass | `miningore: first reward dispatched 5 -> 25 (one native drop call)`; `changed` 0 -> 3; the owner: "3 stacks, much more than usual per stack" |
| rolls10-completes | pass | `extraRuns` 4 -> 13 with `extraRunsUnpaid=0`, `scopedLoot` 6 -> 16, then `pong (YYTK 4.0.1)`; the owner: "10 stacks mined, didnt see anything unusual" |
| node-depleted | pass | every `after extra roll k` snapshot reads `hp=0 miningQue=false` |
| xp-once | pass | `ExperienceUpdate=3/2 GuildExperienceAdd=3/2` after the rolls-3 dig, from `2/0` |
| bonus-finds | not observed | ids 693-700 read 0 at every read; no bonus find seen |
| helmet-rolls3 | not run | `minerhelm status`: helmet not worn |
| xp-slider-shared | pass | `combatTextFirst=experience` at every `miningrolls stat` of the session, and `miningrolls 3` armed (`rollsReady=1`) |

What it established:

- **Re-arming the node and calling the step again pays the completion again,
  in the same Step.** 13 of 13 extra runs paid ore (`extraRunsUnpaid=0`
  throughout). Each one's snapshots read the same shape, for example the
  rolls-3 dig's first:
  `node before extra roll 1: hp=1 miningQue=true miningActive=true stop=0 range=37.4818 miningPlayer=312664r sprite_index=5052r`
  then
  `node after extra roll 1: hp=0 miningQue=false miningActive=true stop=0 range=38.0439 miningPlayer=312664r sprite_index=5052r`.
  Ten runs in one Step (rolls 10) left the game answering `ping`, and the owner
  saw no hitch.
- **The multiplier scales every run.** At x5 with rolls 3, `changed` grew by
  3, one scaled stack per run.
- **`ExperienceUpdate` and `GuildExperienceAdd` are called once per completion
  run.** Their silenced counts grew by exactly the extra runs of each dig (2, 2
  and 9), and the rolls-3 dig let exactly one call of each through (`2/0` ->
  `3/2`). After the x5 and rolls-10 digs the passed counts also carry kill XP
  from nearby combat under the x2 Experience slider, so only that first +1 is a
  clean per-dig count.
- **`MiningAdd`, `CombatText` and `update_quest`: not observed from a dig.**
  The static reading above has the step call all three. Through native detours
  (`rollsReady=1` needs all seven native), `MiningAdd` read `0/0` after all four
  digs; `CombatText`'s silenced count stayed 0 over all 13 extra runs and its
  passed count did not move across the whole rolls-3 dig (149 before and
  after), while the same detour counted kill XP text all session (0, 149, 197,
  215), which is its positive control. `update_quest` read `0/0`, most likely
  because no quest was active, which is not established. Why the dig did not
  reach `MiningAdd` and `CombatText` is not established: a branch of the step
  this character does not take is one untested explanation. The plugin still
  silences all five during an extra run; nothing in the mechanism depends on
  the three being called.
- **The node's `range` is a pulse, not dig progress.** With the character at
  distance 0 and no dig completing, `minerhelm probe` read `range` cycling 0 ->
  about 45-48 -> 0 while `hp` stayed 1; `miningQue` read false at every poll
  (the step consumes it in the frame it is set); `stop` stayed 0.
- **`miningPlayer` read as a reference to the player** (`312664r`) in every
  snapshot of this session, where the 2026-09-23 reward log read `noone` (-4)
  at payout. When the game sets it is not established.
- **The research helper `miningrolls dig` never completed a dig.** About 13
  tries over 5 nodes, including one after `playerwarp` to distance 0 and one
  holding the interact key for 2000 ms, each ended `miningrolls: dig node <id>
  did not complete; released` with `hp` at 1. The owner walked to the mining
  zone and dug all four cases by hand. This is a gap in the research
  automation, not in the extra rolls: the mechanism behaved as above on every
  real dig.

Still not established: a bonus find under extra rolls (no character with a
bonus-find stat above 0 was available, so the bonus sites could not pass); the
Miner's Helmet with rolls above 1 (x4 per stack, one pulse per dig); and
whether an extra run can ever pay nothing (never observed; the plugin's stop
and `hp` reset cover it).

### Live procedure 2 (2026-10-02)

The session the workorder `forgepact-issue-36-extra-ore-rolls` calls Live
procedure 2 (its capture, `forgepact-issue-36-extra-ore-rolls-live-2.md`, stays
on the researcher's machine). Date 2026-10-02, 11:00-11:09 UTC. Build: the
shipped player DLL, the release build of this branch after the 2026-10-02 merge
of main (`552a4b9`), sha256
`58bdd9d5461b80a6cffb6c6d26760fb080b78f82a8869b5f6522278ddb21fee9`, matching
the hash the session's lease read. Character: hero Suh (save slot 2), digging
Copper Veins. `drops.mining_ore_rolls` was absent from `forgepact.json` before
launch (so 1). The panel was this branch's build (`panel/dist`), served
headless on 127.0.0.1, and the rolls were set by sending the panel's own
`/api/set` request, which is what the Loot tab's sliders send; the Loot tab
itself was not clicked, because the session had no way to click the panel
window. The owner dug both nodes by hand. The saves were backed up before
launch and restored clean afterwards.

| Check | Verdict | Line it rests on |
|---|---|---|
| dll-hash | pass | lease `dll_sha256 58bdd9d5...`, `dll_status hashed`, equals the shipped build's |
| marker | pass | `miningrolls 1` -> `miningrolls: x1 (vanilla)` |
| control | pass | `ping` -> `pong (YYTK 4.0.1)` |
| panel-rolls3 | pass | `/api/set` `drops.mining_ore_rolls=3` (multiplier 1) -> `miningrolls: x3 (each dig rolled 3 times)`, `modstate.json` `"rolls":3,"rollsReady":true`; after the dig `miningrolls: first extra roll paid 1 ore stacks`, `extraRuns` 2, `extraRunsUnpaid` 0; the owner saw 3 stacks, copper ore 0 -> 14 on a Copper Vein |
| panel-off | pass | `/api/set` `drops.mining_ore_rolls=1` -> `miningrolls: x1 (vanilla)`, `"rolls":1`; the owner: "1 stack dropped", copper ore 14 -> 17; no new `first extra roll` line, `extraRuns` stayed 2 |
| mining-xp-per-dig | not observed | the character's mining level is at the 5000 cap, so no XP change shows; the player build has no back-end read of mining XP; no floating-text count was given; the owner chose to ship it unmeasured |

What it established:

- **The shipped build, set through the panel, re-runs the dig.** At rolls 3
  both extra runs paid (`extraRuns=2 extraRunsUnpaid=0`) and the dig dropped
  three stacks, against one stack at rolls 1. All seven detours came up native
  on the player build (`rollsReady` true).
- **Back at 1 the panel turns it off.** The next dig dropped one stack and ran
  no extra roll.
- **Ore per stack varied, and why is not measured.** The rolls-3 dig gave 14
  ore in three stacks and the rolls-1 dig 3 ore in one stack, so the stack
  count is exactly three times, but 14 is not three times 3. Each stack's
  quantity was not read.

Not observed in this session: mining XP and the floating text per roll (see
mining-xp-per-dig), bonus finds under extra rolls, and the Miner's Helmet with
rolls above 1.
