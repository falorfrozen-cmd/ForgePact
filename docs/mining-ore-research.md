# Mining Ore Amount — research, 2026-09-21

Status: **verified in play on 2026-09-23** (see "Live verification");
the multiplier adapter described below is unchanged since. The extra rolls
added on 2026-09-28 (issue #36) are **not yet confirmed in a live game**; see
"Extra rolls" at the end.
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

Status (2026-09-28, issue #36): **built and covered by the harness, not yet
confirmed in a live game.** The panel row is Mining Ore Extra Rolls
(`drops.mining_ore_rolls`, 1-10, default 1, off by default) and the plugin
command is `miningrolls N`. Live procedure 1 of the workorder
`forgepact-issue-36-extra-ore-rolls` is to be recorded here; until it is, every
statement below about what the game does is a static reading, not a
measurement.

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

### What Live procedure 1 must show

Run on the research DLL, with `miningore stat` reading `nativeReady=1` as the
control and `miningrolls stat` as the marker, digging through `miningrolls dig`
so the loop needs no one at the keyboard:

- `miningrolls stats`: the ten stat queries (692-700, 703). If 693-700 all read
  0, the bonus finds can only be recorded as not observed.
- Rolls 1: no extra run, `hp=0` after (baseline).
- The Experience slider set first (`stat exp 2`), then `miningrolls 3`: the
  rolls still arm (the shared detour), and a dig pays three runs, with
  `extraRunsUnpaid` 0, XP counters grown by exactly one dig's worth, and `hp=0`.
  If a re-run pays nothing, the before/after node snapshots are the finding
  (`rerun: no-reward`), not a defect.
- Multiplier x5 with rolls 3: every stack of every run x5.
- Rolls 10: ten paid runs, the game still answering, `hp=0`.
- Bonus finds over those digs: seen, or not observed (never a failure).
- With a Miner's Helmet worn, rolls 3: each stack x4, one pulse per dig.

Not established until then: whether the completion pays again in the same
frame, whether ten runs in one frame are harmless, and whether any of the
owner's characters has a bonus-find stat above 0.
