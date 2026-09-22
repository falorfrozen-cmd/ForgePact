# Mining Ore Amount — research, 2026-09-21

Status: **verified in play on 2026-09-23** (see "Live verification" at the end);
the adapter described below is unchanged since.
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
node edit was added. x1 installs nothing on a fresh process. After a nondefault
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
