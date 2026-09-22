# Miner's Helmet

Status (2026-09-23): verified in play and no longer an experiment - see "Live
checks" below. The helmet is built by the existing Custom Forge item
constructor from the sibling Item Editor's Miner template (Item Forge → Forge a
signature item → Miner's Helmet); ForgePact recognises it and runs its
mechanics. It is in no drop pool: ForgePact's Angelic / Unholy die holds only
the Tyrant's Crown and Headhunter. The file keeps its `prototype` name so
existing links resolve.

## Item

- Great Helm base; Unholy display rarity, tier SS.
- 1000 flat Defense (154), 500% Enhanced Defense (29), 20% Movement Speed (25),
  20% All Resistances (173), +5 Light Radius (281).
- These are input stats. The final game tooltip defense still needs live testing.
- Reserved test-item identity: type 0, seed 777003, base 7, c=0, j=0 - what
  the retired panel button created; ForgePact still registers and recognises it.
- Custom Forge can use a different seed: its matching sidecar assigns the
  `miner` mechanic. Only a helmet in the local player's helmet slot qualifies.

## Runtime

The mining adapter still changes only the quantity in an existing native ore
reward. With helmet mode armed, a worn helmet replaces the ore slider with x4;
with the helmet off (or the reward refused for another reason, such as
another player's named dig) the Mining Ore Amount slider applies exactly as it
would had the helmet mechanic never been armed, and the refusal reason names
the slider value. The two never stack. A slider-only reward counts as no helmet
reward: no pulse, no counter, no Vein Resonance. Until 2026-09-23 an armed
helmet forced x1 whenever it was not worn, which is why the slider looked dead
on a character that had ever loaded the helmet. No mining-speed or XP modifier
is introduced.

The equipment read follows `global.equippedItems[mplr][0][0]`, a fingerprint,
through `GetOnlinePlayerItemOwner` and `GetItemFromFingerprint`. This layout and
the resolver arguments were established by private static inspection of the
game's equip/unequip paths and **confirmed live on 2026-09-23** (worn and
removed helmets both recognised on the next reward). It does not
use the permissive created-item registry used by some older mechanics. Missing
equipment data fails to x1.

**Who dug the node (settled 2026-09-23).** The first live run refused every
reward with "Mining player identity unreadable (kind 0)". Private static
inspection of the installed build (`bin\bp_ipc\out.txt` plus the node's step
script and Create event) explains it: the node's `miningPlayer` starts as
`noone`, exactly one path of the step script ever assigns it (a with-loop over
players that the ordinary keyboard dig does not take), and nothing clears it, so
at the ore reward of a normal dig it is still `noone`. The game's own ground-loot
script attributes loot created by this client to the local player (`mplr`). The
reward therefore accepts two ownership rules, and `lastRewardReason` names the
one that applied:

1. `miningPlayer` names a live instance: it must be the local player.
2. `miningPlayer` is not an instance (a plain real such as `noone`, or a real
   below the first GameMaker instance id, which would be an object index): the
   local player must stand within 400 world units of the node. A dig needs the
   player next to the node, so a remote player's node elsewhere in the zone can
   never pass, and one dug beside us is loot this client already owns in vanilla.

The ownership check runs before the item lookup; another player's named dig does
not scan local equipment. Non-string fingerprints are rejected before entering
the game resolver, and definition identifiers must be finite nonnegative whole
numbers. Returning to a menu clears both the equipped flag and its status text.

Permission is checked afresh for every ore reward. A once-per-second read is
only for the panel status. Hooks install lazily after a matching item loads,
not because a built-in template exists.

The golden pulse, a cosmetic extra, draws a gold expanding ring for 550 ms from
a native reward position; it is not yet confirmed on screen, and its final look
is deferred to a later change. It is a bounded visual list (six simultaneous rings) drawn by
the existing HUD callback, with colour/alpha restored, room-change expiry and
one pulse per node. It creates no combat effect object and deals no damage.
Invalid camera dimensions skip drawing; clock rollback or room changes discard
old pulses. Drawing failures restore colour/alpha and never retry a reward.

**Nearby-vein harvesting (Vein Resonance) is implemented and was verified live
on 2026-09-23** (record below). The live dig probe showed how the game digs: the
interact press, or the node's own `miningQue` flag, sends the node through its
level check and straight to the reward within the same step (`miningActive`
true, `range` at `rangeMax`, `stop` 0, `hp` still 1, then `hp` 0), provided the
player stands within the node's `miningActivateDistance` of 16 px. So when a
worn-helmet dig hands out its first ore reward, the plugin lists the
`Mining_Node_obj` instances within 192 px of that node, keeps those that are
alive (`hp` truthy), not active or already queued, and whose protected level
requirement (`GPV(miningReq)`) is at most the character's `GetMiningLevel()`,
and takes the nearest two (`MinerRules::Nearby`). Each gets `miningQue` raised
and its activate distance widened to 4096 for at most 90 frames; its next step
runs the game's own completion - hit effect, ore, mining XP, quests, depletion,
network message - and the ore hook applies 4x under the "vein resonance" rule.
The plugin creates no loot, forces no node health and mines nothing itself. A
queued vein that completes is restored and never starts a chain; one that has
not completed in time is released with its flag lowered. `minerhelm veins 0|1`
switches the feature; `minerhelm status` counts queued, completed (`bonusVeins`)
and released veins. An unreadable mining level queues nothing.

## Creating the helmet

Forge it with the sibling Item Editor → Item Forge → Forge a signature item →
Miner's Helmet. Editor saves use its existing backup/closed-game safeguards. The
editor route creates a fresh seed and a sidecar that assigns the `miner`
mechanic, which ForgePact recognises.

Until 2026-09-23 the panel also had a **Create test helmet** button (Mods →
Items) that queued `minerhelm grant <32-hex-request-id>`, after which the plugin
dropped a helmet with the reserved identity beside the character. The button,
its endpoint and the `grant` command were removed once the helmet was verified
in play; a helmet made that way keeps working. `minerhelm grant` now only prints
the usage line.

`minerhelm status` prints the equipment read, observed reward/pulse counts,
Vein Resonance counters and the last reward decision without changing gear.
`minerhelm probe [seconds]` (default 20, at most 120) prints, every sixth frame
and only on change, the slider state of the mining node nearest the player
(`miningActive`, `miningPlayer`, `stop`, `range`, `rangeMax`, `dir`,
`miningQue`, `hp`, `miningActivateDistance`, distance); the first eight ore
rewards print the same line for their node. It is the live record Vein
Resonance was built on; it reads and changes nothing.

## Live checks

Done in play on 2026-09-23 (details under "Ownership fix" below):

1. A worn helmet was recognised after several game restarts, and taking it off
   was recognised on the next reward.
2. With it worn: `10 -> 40` and `13 -> 52` ore rewards, confirmed in the
   inventory by the user. With it off and the slider at x10: `6 -> 60`.
3. Vein Resonance: two veins 154 and 186 units from the dug node finished
   through the game's own dig with 4x ore; `bonusVeins=2 queued=2 released=0`.

Still open:

1. The item tooltip: name, the five stats and the final defense were not
   recorded.
2. Native mining duration and XP were not measured (the code changes neither).
3. The golden pulse on screen, including camera scaling and room changes.
4. Save/reload per creation path (which path made the recognised helmet was not
   noted).

## Automated checks

`py -3 -m unittest discover -s tests -p 'test_miner_helmet*.py' -v` compiles the
actual equipment-read code against mocked game arrays and fingerprints, checks
reserved identities/nearest-two selection, and checks over real HTTP (temporary
settings) that the retired Create endpoint stays gone. The mining adapter harness also verifies 4x without
stacking, immediate removal, missing equipment, callback exceptions and exactly
one native dispatch.

The sibling editor's `test_custom_item_forge.CustomForgeMechanicTests` covers
the template and sidecar round trip and rejects non-helmet bases. Its broader
suite has a pre-existing missing-file assertion for
`CLAUDE_CUSTOM_FORGE_STAT_DECODE_REQUEST.md`, which is absent from the pinned
repository. No placeholder research document was fabricated to satisfy it.

`tests/miner_helmet_runtime.cpp` includes both complete production adapters,
not a second implementation of their logic. It exercises the equipped item
through a native reward and draw callback, removal between rewards, Custom
Forge's fresh-seed identity, another player's node, failed native dispatch,
renderer restoration, room/clock changes, the retired grant command and bounded
effect storage. It also runs 1,000 idle frames in active and inactive states:
the inactive state installs no mining hooks and reads no equipment; the active
state throttles status reads and produces no ore/effects without mining.

This fake runner cannot reproduce native hook installation, memory lifetime,
or inter-plugin interactions. Its passing result is not a live crash clearance.

## Local verification record (2026-09-21)

- ForgePact full regression: 934 run, 928 passed, 6 skipped.
- Miner-specific checks after panel polish: 7 passed.
- Item Editor mechanic/runtime checks: 43 passed.
- Browser sandbox: template loads all five stats and selects Miner; the panel
  double-click queues one command and shows the mocked plugin acknowledgement.
  No browser errors; panel has no horizontal overflow at 390 px.
- Release DLL built and installed with the game closed. SHA-256:
  `B80FEE84BABF2143F4FA80B4B09FF9C6A764790A274048170994D3B54158A2F0`.
  Previous ore-trial DLL and settings copy are in the ignored local folder
  `build/miner-install-backup-20260921-033302`, with a hash-guarded restore script.
- Source panel started on port 8766. No game launch, live helmet creation,
  equipment change, save edit, commit, push or release was performed.

Browser fixtures do not verify native item creation or equipped rewards. The
live acceptance checks above remain outstanding.

## Follow-up verification (2026-09-21)

The full-feature session subsequently recognized the equipped helmet and reported
both mining hooks and drawing ready, but no scaled reward or pulse was recorded;
the user also observed neither effect. Hook installation is therefore not live
feature verification. The ownership check now compares the runner's live instance
ids directly instead of requiring the optional raw-pointer resolver. A new harness
case failed with valid typed ids and an unavailable pointer resolver before that
change and passes afterward, including both the 4x reward and its pulse. This
reproduces a refusal path, not proof that it caused the user's specific run.

The existing mod-state file now separates `stepObserved`, `oreObserved`, and
`lastRewardReason` from hook readiness. Ownership refusals record a bounded log
message; another player's reward and an unavailable/dead owner still stay at x1.
Real ore pickup and on-screen pulse confirmation remain outstanding.

- Full ForgePact regression: 935 tests, 929 passed and 6 skipped.
- Focused mining/helmet tests: 19 passed, including the full runtime harness.
- Item Editor `CustomForgeMechanicTests`: 24 passed.
- The reported menu-status bug, non-string fingerprint dispatch, unnecessary
  remote-miner equipment lookup and pulse clock rollback each had a failing
  test before the corresponding change and pass afterward.
- User comparison: a player build without the miner mechanic worked with the
  created helmet equipped. The exact release and other module/settings parity
  have not been established. This makes the new runtime or its interactions
  candidates; it does not identify a specific faulty hook.
- The updated candidate is built separately. No game DLL, game setting or
  character save is changed by this follow-up; the crash investigation and
  real ore-inventory/pulse verification remain open.

## Ownership fix (2026-09-23)

- The game log of the 2026-09-22 run held one refusal, "Mining player identity
  unreadable (kind 0)", and `modstate.json` showed `stepObserved` true with
  `oreObserved` false for later runs in which nothing was mined. So both native
  hooks fire on the installed build and the ore branch was reached; the reward
  was refused by the ownership check, not missed by the hooks.
- Cause and rules: see "Who dug the node" above. `RewardMultiplier` now applies
  the two rules, refusals carry the distance and the raw `miningPlayer` kind and
  value, and the helpers sit above the equipment reader so the equipment-only
  test build keeps compiling.
- `tests/miner_helmet_runtime.cpp` models the real node (`miningPlayer` a plain
  real, per-instance positions): unnamed miner beside the node grants x4, far
  away is refused, a small real is not treated as an instance, a named remote
  miner is refused even beside the node, a named local miner needs no proximity.
  Focused miner tests: 3 passed (runtime harness 37 scenarios, RESULT OK).
- Live run the same night: the user confirmed 4x ore in the inventory; the log
  shows `miningore: first reward dispatched 10 -> 40` and several more rewards,
  each with "reward from node" state `miningActive=true miningPlayer=-4 stop=0
  range=48 rangeMax=48 dir=1 hp=1` at a distance of 6-37 px, and `hp=0` on the
  node right after. The probe never caught an intermediate slider position: a
  dig completes in the step of the press.
- Vein Resonance was then built on that record (see above). Harness: eight
  scenarios (two nearest eligible veins queued; a queued vein completing is 4x,
  restored, no chain; timeout release; proximity still required for veins nobody
  queued; off switch; unreadable level; zone change; bad switch value).
- Vein Resonance live run (2026-09-23, 01:45): a worn-helmet dig at one node
  (`13 -> 52` ore) logged `vein resonance -> node 267957 (154 px away)` and
  `-> node 267969 (186 px away)`; both then completed through the game's own
  dig while the player stood 155-169 px from them (their reward lines show
  `miningQue=true miningActivateDistance=4096`), and `minerhelm status` read
  `bonusVeins=2 queued=2 released=0`. The user saw the neighbouring veins pop.
  Visual details (the neighbours' hit effects, any level text) are the user's
  observation, not something the log proves.
- The pulse (gold ring) has still not been confirmed visually.
