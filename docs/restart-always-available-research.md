# Restart zone at any time: what gates the pause menu's Restart

**Question.** The pause menu's **Restart** button refuses while the game
counts the player as in combat, and only works once they have been out of
combat for a few seconds (ForgePact issue #8). Which value does that refusal
read, which object owns it, what value means "ready", and can a mod change
that one value for the duration of the button's own call so the player's
press goes through - without driving a restart itself?

**Why it matters.** The mod this research is for ("Restart zone at any time",
off by default) is only worth shipping in the shape every working ForgePact
mod has: the game keeps making its own call, and one value inside that call is
different. That needs the gate named and measured, not guessed. Nothing in
this toolkit has ever hooked, read or documented the restart gate before.

**Posture.** Everything here is measured runtime behaviour and our own code.
Scripts, objects, variables and sprites are named by their `hs-game-sdk` names
and indices, or by identifier names read from the executable's string table;
no decompiled script text appears, and the procedure is written as prose. The
pause menu's Exit is always available in the game and is deliberately not
researched (owner's decision, 2026-09-22).

**Status.** Phase 0 (this document's static search, and the `restartprobe`
instrument in the research build) is written. Nothing below is measured yet:
every row of § Static search is a *candidate*, § Results is empty, and every
line of § Decision reads `pending` until the live session in § Live procedure
has run.

## The question

Five things have to be known before any ship code is written, and § Decision
has one line for each of them plus the route the ship code takes:

- **owner** - which scope holds the value the Restart press is refused on:
  `global`, the `Controller_obj` instance, the local `Player_obj`, or the
  `UI_Pause_obj` instance itself.
- **variable** - its name on that owner.
- **readyValue** - the value it holds at the moment the game allows Restart.
- **gate** - where the refusal is decided: in the activation
  (`UiAIngameRestart`) reading a variable (`activation`), in the draw as well
  (`both`), in a predicate script the activation calls (`predicate`), or in the
  menu node's own enabled state built at Create (`ui-node`).
- **override** - whether writing `readyValue` to that variable while the HUD
  shows the in-combat icon made the player's own Restart press go through
  (`works`), did not (`not observed`, only with both controls passed), or
  could not be told (`unmeasured`, a failed control).
- **shipRoute** - which of the shapes in the plan's ship design follows.

## Static search

Source: `hs-game-sdk`'s script and object tables
(`hs-game-sdk/cpp/include/hs_game_sdk/scripts.hpp` and `objects.hpp`, and the
same names in the Python binding), searched on 2026-09-22 for every name
matching restart, pause, combat, fight, idle and safe; plus an identifier-name
scan of the installed `Hero_Siege.exe` and `data.win` for `combat`, `restart`,
`aggro` and `lastHit`, which reads names out of the string table and nothing
else. Everything in this section is a **candidate**: none of it is measured.

Script candidates (each is an `hs-game-sdk` constant, so the probe names them
without a literal):

| Candidate | SDK index | Why it is a candidate |
| --- | --- | --- |
| `UiAIngameRestart` | 3997 | The pause menu's Restart activation (`UiA*` names are activation functions, as `UiAProspectButton` was in the prospect research). `docs/S10-special-content-notes.md` already records that it clears `Controller_obj.shadowRealmSpawned`, so it is the restart itself, reached through the button. |
| `UiDrawIngameRestart` | 4419 | Its draw function, the only `UiDraw*` with a per-button name; the hypothesis is that the "wait" state is drawn here. Also the instrument's positive control: a draw function of an open menu runs every frame. |
| `ZoneGenRestart` | 3611 | The zone regeneration. A row so this document can say whether a refused press reaches it (the expectation is that it does not). The research-only `zonegenlog` tracer already table-hooks it, as a caller trace, not a gate read. |
| `anon@1402@gml_Object_UI_Pause_obj_Create_0` | 6046 | One of the six closures `UI_Pause_obj` builds at Create; one of them may be the Restart button's activation wrapper. |
| `anon@1714@gml_Object_UI_Pause_obj_Create_0` | 6047 | As above. |
| `anon@1867@gml_Object_UI_Pause_obj_Create_0` | 6048 | As above. |
| `anon@2018@gml_Object_UI_Pause_obj_Create_0` | 6049 | As above. |
| `anon@2582@gml_Object_UI_Pause_obj_Create_0` | 6050 | As above. |
| `anon@6013@gml_Object_UI_Pause_obj_Create_0` | 6051 | As above. |

Objects: `UI_Pause_obj` (5206), `Controller_obj` (984), `Player_obj` (3553),
`DPS_Meter_obj` (1357). No script named with `InCombat`, `Combat` (other than
combat *text*), `Fight`, `Idle` or `Safe` exists in the SDK, so the gate is
not a named predicate script - which is why variables are the first suspects.

Identifier names from the executable's string table (names only; a YYC string
carries no owner, so which object holds each is **not known**):

| Name | Kind (by how it was found) |
| --- | --- |
| `in_combat` | variable name (one hit) |
| `isCombat` | variable name - case-sensitive, so a distinct candidate from `in_combat` |
| `wasInCombat` | variable name |
| `lastHit` | variable name |
| `combatRefresh` | variable name |
| `aggroTimer` | variable name |
| `dpsMeterResetCombat` | variable name; the DPS meter's combat reset, which this mod must not change |
| `Hud_In_Combat_spr` | sprite: the HUD's in-combat icon, the player-visible oracle for "the game thinks I am in combat" |
| `PauseRestart` | UI text key of the Restart label |
| `room_restart`, `game_restart` | builtins; the mod never calls either |

Hypotheses, in the order the live session tests them:

- **H-var (leading).** `UiAIngameRestart` (and `UiDrawIngameRestart`) read a
  combat flag or timer off `global`, `Controller_obj` or `Player_obj` and
  refuse, or draw the wait, while it says in-combat.
- **H-node.** The pause menu builds the button disabled from the same flag at
  Create (one of the six closures), so the activation is never dispatched
  while it is disabled.
- **H-elsewhere.** The flag is read by a script none of the rows covers. This
  is distinguishable from "no call" only by control C1 below.

## Candidates and controls

The controls are named here, before the procedure, so neither can be skipped
and explained away afterwards. A row whose control failed is `unmeasured`, and
`unmeasured` is not a result.

- **C1 - the hook rows can see a call.** With `restartprobe hook` attached and
  the pause menu open for two seconds, `restartprobe show` prints
  `control=<UiDrawIngameRestart calls>`. It must be greater than 0: a draw
  function of an open menu runs every frame. If it is 0 or `n/a`, every hook
  row is `unmeasured` and R3 to R5 are void.
- **C2 - the write route works on this owner.** In town, where the candidate
  holds its ready value, write a value it does *not* hold - the in-combat
  value R3 sampled - with
  `restartprobe set <owner> <variable> <R3 value> confirm`; it must print
  `wrote=yes changed=yes`. Then write the ready value back the same way and
  require `wrote=yes changed=yes` again. Writing the value the variable
  already holds proves nothing: a dropped write reads back the same number,
  which is why `set` prints `changed=` beside `wrote=` and why C2 needs both.
  That proves the write reaches the owner before R5 uses the same write as
  evidence.

`override: works` needs C1 greater than 0, C2 `wrote=yes changed=yes` both
ways, and R5 restarting the zone while the HUD in-combat icon is on. R5 writes
when the command is read, and the press comes up to a second later, so a
step event could put the value back before the Restart activation reads it.
A failed R5 is therefore only read once `restartprobe show` has been checked
for the value the `UiAIngameRestart` row sampled *at the press*: if that
sample is not `readyValue`, the write was overwritten before use and R5 is
`unmeasured` for that name, not a negative. `not observed` means: both
controls passed, the at-press sample held `readyValue`, and the zone still did
not restart - and even then it is recorded as "not observed with a
command-time write" for the names sampled, never as "does not work". With a
failed control it is `unmeasured`.

## Instrument

`restartprobe`, research build only (`plugin_build\build.bat dev`; the player
build answers `command unavailable in player build`). It is dispatched from
its own helper beside `menulayout`'s, never from `FrameCallback`: every read
happens on the frame that consumes `cmd.txt`, and every hook-side read happens
inside the hooked call itself.

- `restartprobe vars` - hook-free. For each scope (`global`, the first
  `Controller_obj`, the first `Player_obj`, the first `UI_Pause_obj`, which is
  `no instance` while the menu is closed) it reads each candidate variable by
  name and prints `<scope>.<name>=<kind:value>`, `absent` or `unreadable`.
  Instances are found by name and accepted by reading a variable through them,
  never by the kind of handle the runtime returned.
- `restartprobe hook` - attaches every row of the candidate table in one
  command, each with its own detour at the script's own code, and prints one
  line per row: `native`, `native (under table-only zonegenlog)`, `blocked`
  with the reason, or `not found`. It resolves each script by its SDK name and
  refuses to detour anything that is not executable code inside
  `Hero_Siege.exe`. A row that could not attach shows `calls=n/a`, never 0.
- `restartprobe show` - per-row call counts, then the call log: the first 20
  calls of each row with the caller's object name, the argument count, up to
  three arguments and the return value. The two `*IngameRestart` rows also
  sample every candidate variable at the call, which is the point-of-use read;
  a read at the frame boundary would answer for the previous frame. The first
  line carries `control=`, C1's number.
- `restartprobe set <scope> <name> <number> confirm` - the one write. It
  refuses, in this order and before writing anything: an unknown scope, a
  scope with no instance, a variable that is absent, a current value that is
  not a number, a missing `confirm`. Then it writes once, reads the value back
  and prints `wrote=yes|no changed=yes|no before=... after=...`: `wrote`
  compares the read-back with the value written, `changed` with the value
  read before, so a write of the value already held shows `changed=no`. It exists so H-var is
  decided by an experiment rather than inferred from a draw.
- `restartprobe reset` - clears the counters and the log; attached rows stay
  attached.

Coexistence: `zonegenlog` table-hooks `ZoneGenRestart`. If it is on, the probe
detours the game code under it and says
`native (under table-only zonegenlog)`. That is the only row that knows
about another ForgePact hook: the two `*IngameRestart` rows carry no
existing-hook pointer today, so if a hook of ours already held either script
they would print `blocked` rather than attach. No such hook exists yet. When
the shipped mod adds one, those two rows have to be bound to its original
pointer first (the same arrangement `ZoneGenRestart` uses with
`zonegenlog`); until that change lands, do not install both in one session.

Also available with no new code: `tgprobe deep snap` and `tgprobe deep diff`
(`docs/toggle-skills-research.md`) snapshot every scalar on the player,
controller and global scopes, so a town snapshot diffed against a fight
snapshot finds every combat-shaped value and its owner.

## Live procedure

Research DLL `plugin_build\BloodPactPlugin_rel.dll`, installed by the owner as
`mods/aurie/BloodPactPlugin.dll`, after an out-of-band copy of the saves.
`hs-drive` order: `hs_selfcheck`, `hs_saves_backup`, `hs_launch`,
`hs_select_character`, then `hs_command` for each step, and at the end
`hs_stop_game`, `hs_saves_inspect`, `hs_saves_restore`. A screenshot is the
oracle for "the pause menu shows the wait" and for the HUD in-combat icon.

| Step | Do | Record |
| --- | --- | --- |
| R1 | In town, no enemies: `restartprobe vars`; `tgprobe deep snap town controller player global`. | Every `<scope>.<name>` line. |
| R2 | Walk to enemies, get hit, HUD icon visible: `restartprobe vars`; `tgprobe deep snap fight controller player global`; `tgprobe deep diff town fight` filtered by `ombat`, then by `Combat`, then by `lastHit`, then unfiltered. | The changed paths. |
| C1 | `restartprobe hook`; open the pause menu with Esc, wait two seconds; `restartprobe show`. | `control=` must be greater than 0. If not, every hook row is `unmeasured` and R3 to R5 are void. |
| R3 | Menu open, in combat: `restartprobe reset` (the draw row's 20-line log fills within a second of C1), then press Restart once (the game is expected to refuse); `restartprobe show`. | `UiAIngameRestart calls=`, its sampled variables and return; the draw row's samples; `ZoneGenRestart calls=` (expected 0). |
| R4 | Wait out of combat until the game allows it; `restartprobe reset`; press Restart; `restartprobe show`. | The same lines at the moment it worked. The values that differ from R3 decide `variable` and `readyValue`. |
| C2 | In town, variable at its ready value: `restartprobe set <owner> <variable> <R3 value> confirm`, then `restartprobe set <owner> <variable> <readyValue> confirm`. | Both print `wrote=yes changed=yes`. |
| R5 | In combat, menu open, wait shown: `restartprobe reset`; `restartprobe set <owner> <variable> <readyValue> confirm`; press Restart within one second; `restartprobe show`. | `wrote=yes changed=yes`; the variable as `UiAIngameRestart` sampled it at the press; whether the zone restarted (screenshot, `ZoneGenRestart calls=`). This is `override`. |
| R6 | Repeat R5 with the other candidate names if R5 did not restart. | Per name. |
| R7 | The six `UI_Pause_obj` closure rows: which counted during R3 and R4? | For H-node. |
| R8 | `hs_stop_game`, `hs_saves_inspect`, `hs_saves_restore`. | `changed` / `missing`. |

The pause menu's Exit is not a step and is never pressed for measurement.

If `override` is `not observed` (both controls passed and the at-press sample
held `readyValue`), H-var is recorded as not observed with a command-time
write, for the names sampled - not as falsified. The next reads are R2's diff unfiltered and the
`UI_Pause_obj` instance's own members; after that the work stops and the
owner decides whether another round is funded. A node state (H-node) is
recorded as `gate: ui-node`.

## Results

Not run yet. One row per step R1 to R8 (and the two controls) is filled from
the lines the session printed, quoted, and nothing else.

| Step | Printed | Reading |
| --- | --- | --- |

## Decision

owner: pending
variable: pending
readyValue: pending
gate: pending
override: pending
shipRoute: pending
