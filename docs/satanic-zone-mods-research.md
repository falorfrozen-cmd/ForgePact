# Satanic Zone mod pool — research note

Status: **live-tested and working (2026-09-10)**, against the user's own
Hero Siege / Steam install with the ForgePact dev build. The original plan
(hook `LoadSatanicZone` and correct its output) turned out to target the
wrong routine — see "Findings" below — and was replaced with a poll-based
mechanism that does not depend on knowing which routine performs the roll.
That mechanism is implemented and confirmed correcting live rolls end to end.

## Goal

Find the routine that **writes** `Controller_obj.satanicZone` /
`satanicZoneBuff` / `satanicZoneDebuff` (which mods get rolled onto a
Satanic Zone, and when), so `plugin/ModuleMain.cpp` can hook it and, when the
user has disabled some mods, steer the roll away from them without forcing
anything the game wouldn't otherwise choose.

## Known so far (static analysis, 2026-09-10)

- `grep` across `plugin/ModuleMain.cpp` for every Satanic-Zone-related script
  name below returns **zero hits** — nothing in ForgePact has ever hooked
  this system. The only existing Satanic-prefixed hook is
  `HookSatanicTier` (`LoadSatanicDropTier`), which is item *rarity tier* by
  monster level — a different, unrelated mechanic.
- `hs-game-sdk/data/scripts.json` (mechanically extracted from the game
  binary — names/indices only, no behavior) lists these routines:
  - `gml_Script_LoadSatanicZone` (index 2289) / `LoadSatanicZone` (2286)
  - `gml_Script_GetSatanicZoneEligible` (2287)
  - `gml_Script_GetSatanicZoneOffline` (2290)
  - `gml_Script_LoadRandomSatanicStat` (2281) / `LoadRandomSatanicStat` (2280)
  - `gml_Script_ReturnRandomSatanic` (3326) / `ReturnRandomSatanic` (3325)
  - `gml_Script_ReturnSatanicZoneBuffs` (3328) / `ReturnSatanicZoneBuffs` (3327)
  - `gml_Script_ReturnSatanicZoneDebuffs` (3330) / `ReturnSatanicZoneDebuffs` (3329)
  - `gml_Script_EnemyKillSatanicZoneRelic` (1511) / `...RelicFeast` (1510)
  - `gml_Script_EnemyDestroySatanicBuffs` (1482)
- `HS-Offline-Tracker` (a sibling, read-only telemetry submodule — not part
  of ForgePact) already verified the **read** side live, in
  `aurie-producer/src/module.cpp` (`ResolveSatanicZone`, `CaptureZone`,
  `TryGetZoneVariable`) and `aurie-producer/README.md`:
  - It hooks `gml_Script_RoomGoto` (after the original returns) and reads,
    off **`Controller_obj`** (instance vars first, global-namespace fallback
    for older builds):
    - `satanicZone` — the resolved room. Produced offline by
      `GetSatanicZoneOffline`/`LoadSatanicZone`, tried against every
      candidate in `global.satanicZone`.
    - `satanicZoneBuff` / `satanicZoneDebuff` — small arrays of the ids
      actually rolled. Live example captured: `buffs=[6,14,21]
      debuffs=[3,12]`.
  - Its own buff/debuff **name/description** table
    (`HS-Offline-Tracker/src/buffs.js`) was hand-verified live on
    2026-09-04 against Hero Siege 7.0.6.0: 25 buffs (ids 1-25), 26 debuffs
    (ids 1-26). That table is now mirrored, unmodified, into
    `hs-game-sdk/curated/satanic_zone.json` (see that file's `$schema_note`)
    and generated into `hs_game_sdk`'s Python/C++/TS bindings by
    `tools/generate_satanic_zone_sdk.py`.
  - This is read-only telemetry: nothing in that module — or anywhere else
    in this toolkit — has ever hooked or influenced the roll itself.

## What Phase 0 needs to answer

1. **Which routine actually performs the roll** — `LoadSatanicZone` is the
   primary candidate (it's the one both the routine table's naming and the
   `HS-Offline-Tracker` README point to as the orchestrator); fall back to
   `GetSatanicZoneOffline` or `LoadRandomSatanicStat` if probing shows
   otherwise.
2. **Cadence** — once per act/session? Once per room load? Only when no
   zone is currently resolved for that act? (Matters for how often a
   toggle change actually takes effect in play.)
3. **Id range / duplicates** — do `satanicZoneBuff`/`satanicZoneDebuff`
   always stay inside 1-25 / 1-26, and can an array contain the same id
   twice?
4. **Is a post-roll rewrite safe** — if the plugin overwrites
   `Controller_obj.satanicZoneBuff`/`Debuff` (via `variable_instance_set`)
   immediately after the roll, do `ReturnSatanicZoneBuffs`/`Debuffs`,
   `EnemyKillSatanicZoneRelic(Feast)`, `EnemyDestroySatanicBuffs`, and the
   zone's own buff-panel UI all pick up the corrected values, or does
   something else read the arrays earlier and cache them?
5. **Minimum viable pool size** — push each polarity down to 1 enabled mod,
   then 0, and watch for a crash, an infinite reroll loop, or a soft-lock.
   `forgepact.py` currently enforces `MIN_ENABLED_SATANIC_BUFFS = 3` and
   `MIN_ENABLED_SATANIC_DEBUFFS = 2` (user-set 2026-09-10) — not yet tested
   against the game's actual behavior at the edge.

## How to run it (dev build only, `#ifndef FORGEPACT_RELEASE`)

No new plugin commands are needed — everything below already exists in
`plugin/ModuleMain.cpp`:

```
ojson Controller_obj satanicZone
ojson Controller_obj satanicZoneBuff
ojson Controller_obj satanicZoneDebuff

pcall LoadSatanicZone
pcall GetSatanicZoneOffline
pcall LoadRandomSatanicStat
pcall ReturnSatanicZoneBuffs
pcall ReturnSatanicZoneDebuffs

naddr LoadSatanicZone
naddr GetSatanicZoneOffline
naddr LoadRandomSatanicStat
```

`ojson` writes to `bp_ipc\ojson_Controller_obj_<var>.json`; `pcall` prints
`Describe(result)` to the log and, for struct/array results, also writes
`bp_ipc\pcall.json`. `naddr` resolves an RVA for Ghidra if a probe result is
ambiguous and static disassembly is needed.

For call-order/cadence, add a temporary logging hook on the top 1-2
candidates using the existing `HookOneScript(...)` pattern — see
`HookSatanicTier` (`plugin/ModuleMain.cpp:7164`) for the exact style — that
logs args/return via `Out(...)` without changing behavior, then walk through
acts/rooms and watch when it fires.

## Findings (2026-09-10, live session)

**`LoadSatanicZone` is not the roll routine — it never fires during normal play.**
`pcall LoadSatanicZone` (0 args) resolves and returns `bool:false` (no
exception), confirming the script name is valid and hookable. But after
hooking it with an unconditional call counter (`g_SatLoadCalls`, separate
from the filtering-only `g_SatModsHits`), the counter stayed at **0** through:
a normal zone load, walking within a room for 30+ seconds, and an explicit
waypoint/portal travel to a different zone — while `Controller_obj`'s
`satanicZoneBuff`/`satanicZoneDebuff` visibly changed on their own during
that same window. Rereading `HS-Offline-Tracker`'s README more carefully:
*it* calls `LoadSatanicZone(room)` itself, as its own diagnostic, "for every
candidate in `global.satanicZone`" — that line describes HS-Offline-Tracker's
own probing code, not something the game calls internally. So hooking
`LoadSatanicZone` only intercepts callers that explicitly invoke it (like that
diagnostic), not the game's own internal roll logic. The actual write site
remains unidentified, and may not even be a single script — nothing else
tried (`GetSatanicZoneOffline`, `LoadRandomSatanicStat`, `ReturnSatanicZoneBuffs`/`Debuffs`)
was confirmed as the source either (see raw probe results below).

**Raw `pcall` results (0 args, live, mid-run):**
```
pcall LoadSatanicZone(0 args)         -> bool:false        (resolves, no crash)
pcall GetSatanicZoneOffline(0 args)   -> undefined          (resolves, no crash)
pcall LoadRandomSatanicStat(0 args)   -> EXCEPTION           (needs args)
pcall ReturnSatanicZoneBuffs(0 args)  -> real:0.000000       (needs args; not a bare array getter)
pcall ReturnSatanicZoneDebuffs(0 args)-> real:0.000000       (same)
```
`LoadRandomSatanicStat` throwing on 0 args is the best remaining lead for a
true "pick one id" routine, but was not pursued further once the poll-based
approach (below) proved sufficient — it needs no routine identification at all.

**Confirmed data shape**, matching `HS-Offline-Tracker`/`buffs.js` exactly:
- `Controller_obj.satanicZone` = a real room-reference number (`162966.0` in
  this session) when a zone is resolved; `Controller_obj.satanicZoneBuff` /
  `satanicZoneDebuff` are small int arrays, ids well inside 1-25 / 1-26.
  Observed rolls: 3 buffs + 2 debuffs both times (not a fixed rule, just what
  was seen twice); ids never repeated within one array.
- The arrays visibly change on their own during a normal play session — not
  a one-time-per-session roll. Cadence/trigger is still unidentified (did not
  correlate cleanly with the explicit room transitions tried), but changes
  happen on the order of tens of seconds to a few minutes apart during
  active play.

## The mechanism that shipped: poll-and-correct (not a routine hook)

Since the write site couldn't be pinned to one routine, `plugin/ModuleMain.cpp`
does not hook anything to filter. `SatanicPollTick()` runs every
`kSatanicPollFrames` (15) frames from the existing `FrameCallback`, reads
`Controller_obj.satanicZoneBuff`/`Debuff`, and whenever the content differs
from what was last seen (proof the game changed it, however it did so),
immediately replaces any disabled id with a random still-enabled id from the
same polarity, then caches the corrected result so the next tick doesn't
mistake its own correction for a new game-driven change. `HookLoadSatanicZone`
is kept only as a best-effort diagnostic (harmless if it never fires).

**Live end-to-end confirmation, same session:**
1. Poll detected the initial roll with nothing disabled yet:
   `POLL: content changed (#1) buffs [] -> [24,22,25] debuffs [] -> [25,7]`.
2. `satmods buff 24` + `satmods debuff 25` sent while those ids were live in
   the array. Next poll tick corrected both in place, no room change needed:
   buffs `[24,22,25] -> [6,22,25]` (only 24 replaced, 22/25 untouched),
   debuffs `[25,7] -> [24,7]` (only 25 replaced, 7 untouched).
   `hits` went from 0 to 2, one per polarity, exactly as expected.
3. A second natural reroll (`POLL: content changed (#2) buffs [6,22,25] ->
   [25,19,4] debuffs [24,7] -> [17,23]`) contained neither disabled id — `hits`
   correctly stayed at 2 (nothing to correct), confirming the mechanism only
   ever touches disabled ids and leaves everything else alone.

## Open questions (not blocking, follow-up only)

- Exact roll cadence/trigger is still unknown (Section "Confirmed data
  shape" above) — irrelevant to correctness since polling doesn't depend on
  it, but would let the poll interval be tuned/relaxed if the true cadence is
  known to be slow.
- `LoadRandomSatanicStat`'s argument shape was never determined (it throws on
  0 args) — worth revisiting only if a future feature needs to *influence*
  the roll's timing/selection algorithm rather than correct its output after
  the fact.
- The verbose `satmods POLL: content changed` trace logging (40 lines,
  `g_SatLoadTraceLeftPoll`) is not gated behind `#ifndef FORGEPACT_RELEASE`,
  so a player build would also print it to `out.txt` for a player's first 40
  observed rolls. Low-priority cleanup before treating this as ship-ready.
