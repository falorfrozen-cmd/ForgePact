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

**Status.** Round 1's live session ran on 2026-09-22 and did **not**
identify the gate. It showed where the gate is not: a refused press in combat
never reaches the Restart activation (`UiAIngameRestart calls=0`), so the
refusal is decided upstream of it, on the button/node side. `wasInCombat` on
the player tracks the refusal, but a command-time write of it was put back by
the game before the press, so whether it *is* the gate stays unmeasured.
Round 2's live session ran the same evening and did **not** identify the gate
either, but moved it: the Restart button's own `enabled` and `manualDisable`
flip with combat (`true` to `false` and `false` to `true`), none of the four
attached UI node-API setters was observed to be called, and a hold of either member written
inside the Restart draw was rewritten before every next call (`entryHeld=0`),
so the draw site could not test them; a draw-time hold of the player's
`wasInCombat` stayed in place and did not unlock the press. The one call that
runs at step time, every frame while the cursor hovers Restart, with the
button as its first argument, is `UiSetFocus`. The owner funded a third
round, a step-time write of the button's own members inside `UiSetFocus`
(§ Instrument, Round 3; § Live procedure, Round 3). Round 3's live session
ran late the same evening and **identified** the gate: with the player still
in combat, writing the Restart button's own `manualDisable` back to `false`
inside `UiSetFocus`, on the button that call is handed, let the press reach
`UiAIngameRestart` and restart the zone; `enabled` alone did not. The game
puts `manualDisable` back every frame, so the write only holds while the
cursor is on Restart, and keyboard navigation did not reach the pause menu's
buttons at all. All three rounds' rows are in § Results, § Decision carries
round 3's lines, and the shipped mod (`restartanytime`) is that write.

## The question

Five things have to be known before any ship code is written, and § Decision
has one line for each of them plus the route the ship code takes:

- **owner** - which scope holds the value the Restart press is refused on:
  `global`, the `Controller_obj` instance, the local `Player_obj`, or the
  `UI_Pause_obj` instance itself.
- **variable** - its name on that owner.
- **readyValue** - the value it holds at the moment the game allows Restart.
- **gate** - where the refusal is decided. Round 1 ruled out the activation
  (`UiAIngameRestart` is never called on a refused press) and recorded
  `upstream`. Round 2 names one of: a node-API call that disables the Restart
  row (`node-row`), a member of the button instance itself
  (`button-member`), the player's `wasInCombat` read by the node
  (`player-var`), a countdown such as `global.tupm[1].in_combat` (`timer`), or
  `upstream (not identified; …)` with the closest finding.
- **override** - whether holding `readyValue` in place, written inside a call
  the game is already making, while the HUD shows the in-combat icon made the
  player's own Restart press go through (`works`), did not (`not observed`,
  only with every control passed and the value found in place at entry), or
  could not be told (`unmeasured`, a failed control or a value the game
  rewrote before use).
- **shipRoute** - which shape the ship code takes: `draw-write` (the value
  written inside the Restart draw), `node-api-arg` (one argument of a node-API
  call), or `none` (stop and ask the owner). Round 3's vocabulary is
  `setfocus-write` (the button's own member written inside `UiSetFocus`, on
  the button that call receives) or `none`.

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

### Round 2: the UI node API

Round 0 searched the SDK for `*Combat*`, `*Fight*`, `*Idle*` and `*Safe*`
names and found no predicate script. That is still true, but it answered a
question round 1 made beside the point: a refused press never reaches
`UiAIngameRestart`, so the refusal is not a combat read inside the
activation - it is the button (a menu node) not dispatching at all. A node's
**enabled** state is not named after combat. It has its own setters, and the
SDK carries the whole UI node API as named scripts, which the combat-only grep
never listed. Round 2 hooks all thirteen in the same build, count-only (self,
argument count, up to three arguments and the return), searched on
2026-09-22 in `hs-game-sdk/cpp/include/hs_game_sdk/scripts.hpp`, each name
present once:

| Candidate | SDK index | Why it is a candidate |
| --- | --- | --- |
| `UiSetRowEnabled` | 4465 | The one script whose name says "enable a row". If the pause menu disables the Restart row every frame, this is the call, and its arguments name the row and the flag - the `node-api-arg` ship route. |
| `SetGlobalUiEnable` | 4462 | Global UI enable. |
| `EnableNav` | 4460 | Navigation enable. |
| `UiSetActivationFunc` | 4463 | Binds an activation to a node; its arguments at menu open show which node gets `UiAIngameRestart`. |
| `UiSetUpdateFunc` | 4464 | Binds a per-frame update function to a node - where a per-frame gate would be evaluated. |
| `UiCreate` | 4467 | Node construction at menu open. |
| `UiCreateNode` | 4469 | As above; its arguments show the node's shape. |
| `UiRemoveNode` | 4461 | Menu close; cross-checks the hold's "menu closed" auto-disarm. |
| `UiSetFocus` | 4476 | Focus on hover or press - a press-path marker that should count on a refused press too. |
| `UiSetRef` | 4487 | Node reference binding at construction. |
| `UiDrawPauseButtonInfo` | 4417 | A pause-menu button draw beside `UiDrawIngameRestart` (4419) in the index; the "wait" text may be drawn here. |
| `UiDrawPauseButtonJournalInfo` | 4418 | The Journal button's draw: a sibling that is *not* gated, for comparison. |
| `UiACloseButton` | 3973 | Candidate activation of the pause menu's Resume/close - the press-path positive control (a sibling press in combat that does reach its activation). That Resume uses it is not known; S3 measures it, and 0 on a Resume press makes that control `unmeasured`. |

Not rows, and why: the `UiFuncs` struct constructors
(`___struct___408@UiCreate@UiFuncs`, `409@UiCreateNode`,
`410@UiCreateContainer`, `411@UiContainerChange`) run at construction, not
per frame; `UiButtonDraws` (4410) and `UiFuncs` (4459) are container names,
not callable scripts. No other `UiAIngame*` exists for Resume, Options or
Journal - those buttons use activations the SDK does not name by menu, which
is why `UiSetFocus` and `UiACloseButton` are the press-path markers. Exit
stays out (owner's decision).

Also reused, not repeated: `menuprobe list UI_Button_obj` (research build)
lists every live button with its `id` and `text`; it listed 13 buttons at the
main menu on 2026-09-21 and is unverified at the pause menu, so S1 runs it as
a hook-free cross-check of `selfIds=`. `event_perform` on a `UI_Button_obj`
was measured not to activate it (`character-select-research.md`), so no
"press the button for the player" route is designed.

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
- **C3 - the Restart draw does not run while the menu is closed** (round 2).
  In town with the menu closed, `restartprobe show` twice, two seconds apart:
  both `control=` values must be equal. This is what makes the hold's
  "disarms when the menu closes" a fact rather than a hope - the draw is the
  hold's default site, so a draw that also ran with the menu closed would
  keep writing there. If it counts, record that, and S4 to S6 are
  `unmeasured`.
- **C4 - the hold's write reaches the value at the point of use** (round 2).
  After S4's first second, `restartprobe hold stat` must show `writes` above
  0 and `readbackOk` equal to `writes`. A hold whose read-back disagrees with
  its write is `unmeasured` for that scope, whatever the press did.
- **C5 - the step-time write reaches the Restart button** (round 3). In
  combat, menu open, Restart greyed, with both `arg0` holds armed at
  `UiSetFocus` and the cursor hovering Restart without pressing,
  `restartprobe hold stat` must show `writes` above 0 and `readbackOk` equal
  to `writes` on every slot used, and the ring's labels must name
  `UI_Button_obj#<id>` with the id T1 read off the button list. A write that
  does not read back, or lands on another instance, makes that slot
  `unmeasured`, whatever the press did. C1 still has to pass in the same
  session: the draw row is also the round-3 hold's menu oracle.

Round 2 reads `override` by these rules. `works` needs C1 above 0, C3
unchanged, C4 `readbackOk` equal to `writes`, and a press during a hold (S4 or
S5) that restarted the zone - the room changed - while the HUD in-combat icon
was on, with `UiAIngameRestart calls=1`. `not observed` needs the same
controls **and** `entryHeld` above 0 for that attempt, meaning the held value
was already in place when a call arrived, so the game could have read it;
it is recorded with what was supplied (site, scope, name, value,
`entryHeld`/`entryOther`). With `entryOther` close to `writes` and
`entryHeld` 0, the game rewrote the value between our calls, so the attempt is
`unmeasured (overwritten before use, <site>)`, not a negative.

Round 1 read `override` as follows (kept for the record): `override: works` needs C1 greater than 0, C2 `wrote=yes changed=yes` both
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
- `restartprobe set <scope> <name> <number> confirm` - the one command-time
  write (round 2 adds scope `path`, below, and the hold, which writes inside a
  hooked call instead). It
  refuses, in this order and before writing anything: an unknown scope, a
  scope with no instance, a variable that is absent, a current value that is
  not a number, a missing `confirm`. Then it writes once, reads the value back
  and prints `wrote=yes|no changed=yes|no before=... after=...`: `wrote`
  compares the read-back with the value written, `changed` with the value
  read before, so a write of the value already held shows `changed=no`. It exists so H-var is
  decided by an experiment rather than inferred from a draw.
- `restartprobe reset` - clears the counters, the call log and `selfIds=`;
  attached rows stay attached, dumps are kept, and an armed hold or `argset`
  stays armed (only `hold off`, `argset clear` or their own auto-disarm end
  them).

Round 2 adds these, all in the same research-build block and none of them on
the per-frame path. Every write and every point-of-use read happens inside a
hooked call's detour, before the game's own body runs, or on the frame that
consumes `cmd.txt`.

- **The thirteen node-API rows** of § Static search, Round 2, attached by the
  same `restartprobe hook` (22 rows in all), count-only. Sampling stays on the
  two `*IngameRestart` rows and the control stays the draw row.
- **`selfIds=`** - `show` prints the distinct instance ids (read by name from
  the self the game handed the call, at most 8, then `,more`) that reached the
  Restart draw. Round 1 printed the caller as `UI_Button_obj#5003`, which is
  the object's index, not an instance; this answers "one button or several"
  without assuming which one.
- **The `path` scope.** `global.tupm[1].in_combat` is a path, not a name.
  `vars` and both sampling rows print it as
  `path:global.tupm[1].in_combat=<kind:value>` or
  `path:…=unresolved: <reason>`, resolved by the same deep reader
  `tgprobe deep get` uses. `set` and `hold` take scope `path` with the full
  path as the name: the parent is resolved the same way, the last segment is
  written with the setter that matches the parent (a struct field, an array
  element, or an instance variable), and the value is read back through the
  full path. Only a `.name` or `[i]` last segment can be written.
- **`restartprobe dump button <label>`** arms a one-shot capture that runs
  inside the Restart draw's next call, on that call's own self. The self is
  accepted by reading a variable through it, never by the kind of handle it
  arrived as. The capture reads `id`, `object_index`, `visible`,
  `sprite_index`, `image_index`, `image_alpha`, `depth`, `x`, `y` and then
  every instance variable the runtime names (up to 512), each printed short.
  It prints `armed; open the pause menu, then: restartprobe dump show
  <label>`, and refuses while the draw row is not attached.
  **`restartprobe dump pause <label>`** captures the first `UI_Pause_obj`
  instance at command time (`no instance` while the menu is closed). Dumps are
  kept by kind and label - one label names a button dump and a pause dump side
  by side - at most 8 per kind, and the oldest is evicted with a line saying
  so. **`restartprobe dump show <label>`** prints
  `<label>: kind=button|pause id=… frame=… names=N` and one `name=value` line
  each (`not captured yet` if the draw has not run since arming).
  **`restartprobe dump diff <a> <b>`** compares each kind the two labels share,
  button first: `~ name: a -> b`, `+ name=b`, `- name (was a)`, then
  `changed= added= removed=`, at most 300 lines.
- **`restartprobe hold <scope> <name> <number> [at <row>] confirm`**, `hold
  off`, `hold stat`. Scopes are `set`'s four plus `button` (the site call's
  own self, checked on every call) and `path`. It refuses, in this order and
  before arming: an unknown scope; a scope with no instance or a path that
  does not resolve; an absent variable; a current value that is not a number
  (for `button` these two are checked on the first call, which disarms if they
  fail); a site row that is unknown or not attached `native`; a missing
  `confirm`; a bad number. Then it prints `hold armed: …; writes start with
  the next call`. The default site is the Restart draw. **On every call of the
  site row, before the game's body runs**, the hold reads the value at entry
  and counts `entryHeld` if it already equals the held value, otherwise
  `entryOther`; writes the held value in the kind it found (a bool stays a
  bool); reads it back and counts `readbackOk` on a match; counts `writes`.
  A value it cannot read counts `unreadable` and is not written; a `button`
  member that is absent or not a number counts `skipped`. It disarms itself,
  inside the call, when a call arrives more than 3 frames after the last
  write (the menu was closed and reopened; `hold: disarmed (menu closed at
  frame …)`) or after 20000 writes (`hold: disarmed (cap)`). The first 20
  calls after arming are logged into the site row's log
  (`entry=… wrote=… readback=…`). `hold stat`, and `show` after `control=`,
  print `armed= site= scope= name= value= writes= readbackOk= entryHeld=
  entryOther= unreadable= skipped= lastWriteFrame=`.
- **`restartprobe argset <row> a<i> <number> [calls=N] confirm`**, `argset
  clear`, `argset stat`. On the named attached row, for its next N calls
  (default 1), if the call has argument `i` and it is a number, replace it
  with the number in the argument's own kind before the game's body reads it,
  log `before=… after=…` and count `applied`; otherwise count `skipped` (not a
  number) or `skippedArgc` (no such argument) and change nothing. It refuses,
  in order, an unknown or unattached row, an index that is not `a0`…`a15`, a
  bad number, a bad `calls=`, and a missing `confirm`; it clears itself after
  the same 3-frame gap. It writes an argument, never a variable. It exists so
  that if S3 shows a node-API row called every frame with the Restart row's
  enabled flag as an argument, "one value inside the call" can be tested in
  the same session.

`set` and the hold write through one helper, so each of the four write
builtins (global, instance, struct field, array element) appears once in the
block and the helper has exactly those two callers.

Why the hold writes inside the draw, and what `entryHeld`/`entryOther`
decide: a write at the end of the frame lands after every step event, so the
next step can recompute the value before anything reads it - which is what
round 1's R5 showed. The draw runs in the button's own frame, but still after
the step phase. Nothing here measured the step order:
the assumption is that instances step in creation order, and the player
exists long before the pause menu, so if the gate is read in a step event the
order per frame would be the player's step (recompute), the button's step
(read), then the draw (our write). The hold measures that instead of assuming it: the value
at entry to each call is the value after the whole step phase. `entryHeld`
close to `writes` means the write survived a full frame, so a press during
the hold tests the value; `entryOther` close to `writes` means the game
rewrote it in between, and the draw site cannot test it - `at <row>` then
moves the write to a row S3 showed running every frame, and `argset` changes
an argument of such a call directly. Both exist in this build so that one
session can follow the counts instead of needing a third round.

Round 3 extends the hold, in the same block and still with nothing on the
per-frame path; `argset`, `dump`, `set`, `path`, the rows, the 20-line row
log, `reset` (which keeps holds armed) and C1 to C4 are unchanged. Round 2
found the draw site too late - the game recomputes the button's members in
the step phase, before its click check - and found `UiSetFocus` called at
step time, every frame the cursor hovers Restart, with the pause menu as its
self and the Restart button as its first argument. So:

- **Scope `arg0`.** `hold arg0 <member> <number> at UiSetFocus confirm` holds
  a member of the site call's own first argument. It is resolved on every
  call, at the point of use: the call must carry an argument 0, the argument
  must read as an instance (a variable is read through it - it is never
  accepted or refused by the kind of handle it arrived as, since this runner
  hands instances out as references), and then the member is read by name.
  Absent or not a number counts `skipped`, and on the first call disarms, as
  for `button`. The write keeps the kind read at entry, so a bool stays a
  bool. Like `button`, `arg0` cannot be checked when the command is read.
- **A label read off the instance.** Every instance target, `arg0` and
  `button` alike, is named in the output by what it resolved to: the object
  name, `#`, the instance id, `.`, the member - for example
  `UI_Button_obj#262247.enabled`. At a site other than the draw the call's
  self is the pause menu, so a fixed `button.` label would have credited the
  write to the wrong instance.
- **Two slots.** `hold …` arms the first free slot and says which
  (`in hold[0]`); with both armed it refuses `two holds armed; hold off
  first` instead of replacing one. `hold off` disarms both. Each slot has its
  own counters; both apply on every call of their site in slot order, before
  the game's body runs, and both may share a site. `show` prints one stat
  line per slot (`hold[0]: armed= site= scope= name= value= writes=
  readbackOk= entryHeld= entryOther= unreadable= skipped= lastWriteFrame=`).
- **An entry ring per slot.** Each write also records, in a ring of 1024
  entries (about 7 seconds at 144 fps, cleared when the slot is armed), the
  frame, the label, the value found at entry, `held=yes` or `held=no`
  (whether that was already the held value), what was written, and
  `readback=ok` or `mismatch`. `hold stat` prints each slot's stat line and
  then its ring, collapsed into runs - consecutive entries that differ only in
  their frame become one line, `hold[0] ring: frames <first>..<last>
  x<count> <label> entry=… held=… wrote=… readback=…` - newest last, at most
  32 lines. Nobody can type `hold stat` within a tenth of a second of a
  refused press; seven seconds of runs still show what the value was on the
  frames around it, and a single unbroken `held=no` run reads as "rewritten
  between every two writes".
- **Disarm keyed to the menu, not to the site.** At the draw site the rule is
  round 2's (a call more than 3 frames after the last write). At any other
  site a gap in the site's own calls means nothing - `UiSetFocus` stops
  whenever the cursor leaves Restart - so the hold reads the Restart draw's
  calls instead, which C3 showed stop with the menu closed: every row now
  stamps the frame of its latest call and its latest pause of more than 3
  frames, and once a slot has written, a call finding no draw for more than 3
  frames, or a draw pause since its last write, disarms it before writing,
  with `hold: disarmed (menu not drawing since frame …; writes=…)`. The
  20000-write cap still applies. Arming at a site other than the draw refuses
  `needs the Restart draw attached (restartprobe hook first); nothing armed`
  while the draw row is not attached `native`, after the existing refusals
  and before `confirm`.

Coexistence: `zonegenlog` table-hooks `ZoneGenRestart`. If it is on, the probe
detours the game code under it and says
`native (under table-only zonegenlog)`. That works only because
`zonegenlog` installs table-only, so the original it saved is the game's own
code, which the probe can detour. The shipped mod, `restartanytime`, hooks
`UiSetFocus` through both of `HookOneScript`'s routes instead: the original
it saves is the hooking library's trampoline and the table entry is our own
hook, and neither is code inside `Hero_Siege.exe`. So the probe's
`UiSetFocus` row stays unbound (binding it to the mod's original would not
make it attach), and the two install orders go as follows. This is a static
reading of the two installers, not run live: neither order has been tried
in a session. With the mod's
hook in first, a later `restartprobe hook` prints `blocked` for the
`UiSetFocus` row (table entry not code inside the game) and attaches the
other 21 rows as usual. With `restartprobe hook` first, the probe has
detoured the game's code without swapping the table, so the mod's install
asks the hooking library to detour an address that is already patched;
`docs/prospect-window-research.md` says the second hook on an address fails,
measured only in the other order, so for this script that is unverified. If
the second detour succeeds, the log reads `restartanytime: hook installed ->
ON` and both the probe's row and the mod's write run on each call. If
it does fail, the log reads `hook UiSetFocus: TABLE-ONLY (MmCreateHook st=…)`
and then `restartanytime: hook TABLE-ONLY -> OFF`, and the mod stays off for
the session. Players meet neither case: the probe is research-build only.
Run the probe or the mod in a session, not both. `prospectprobe hook` also
detours `UiCreate`, `UiCreateNode`, `UiSetRef` and
`UiRemoveNode` at the same code, so whichever of the two instruments attaches
second reports those four rows `blocked`: run one of them per session. The
two-order live check (mod first, then probe first) remains outstanding — not
scheduled in this workorder.

Also available with no new code: `tgprobe deep snap` and `tgprobe deep diff`
(`docs/toggle-skills-research.md`) snapshot every scalar on the player,
controller and global scopes, so a town snapshot diffed against a fight
snapshot finds every combat-shaped value and its owner.

## Live procedure

### Round 1

Research DLL `plugin_build\BloodPactPlugin_rel.dll`, installed by the owner as
`mods/aurie/BloodPactPlugin.dll`, after an out-of-band copy of the saves.
`hs-drive` order: `hs_selfcheck`, `hs_saves_backup`, `hs_launch`,
`hs_select_character`, then `hs_command` for each step, and at the end
`hs_stop_game`, `hs_saves_inspect`, `hs_saves_restore`. A screenshot is the
oracle for "the pause menu shows the wait" and for the HUD in-combat icon.
This table is kept for the record; its step names are in bold so that a
search for a plain `| R1 |` row finds only the result in § Results.

| Step | Do | Record |
| --- | --- | --- |
| **R1** | In town, no enemies: `restartprobe vars`; `tgprobe deep snap town controller player global`. | Every `<scope>.<name>` line. |
| **R2** | Walk to enemies, get hit, HUD icon visible: `restartprobe vars`; `tgprobe deep snap fight controller player global`; `tgprobe deep diff town fight` filtered by `ombat`, then by `Combat`, then by `lastHit`, then unfiltered. | The changed paths. |
| **C1** | `restartprobe hook`; open the pause menu with Esc, wait two seconds; `restartprobe show`. | `control=` must be greater than 0. If not, every hook row is `unmeasured` and R3 to R5 are void. |
| **R3** | Menu open, in combat: `restartprobe reset` (the draw row's 20-line log fills within a second of C1), then press Restart once (the game is expected to refuse); `restartprobe show`. | `UiAIngameRestart calls=`, its sampled variables and return; the draw row's samples; `ZoneGenRestart calls=` (expected 0). |
| **R4** | Wait out of combat until the game allows it; `restartprobe reset`; press Restart; `restartprobe show`. | The same lines at the moment it worked. The values that differ from R3 decide `variable` and `readyValue`. |
| **C2** | In town, variable at its ready value: `restartprobe set <owner> <variable> <R3 value> confirm`, then `restartprobe set <owner> <variable> <readyValue> confirm`. | Both print `wrote=yes changed=yes`. |
| **R5** | In combat, menu open, wait shown: `restartprobe reset`; `restartprobe set <owner> <variable> <readyValue> confirm`; press Restart within one second; `restartprobe show`. | `wrote=yes changed=yes`; the variable as `UiAIngameRestart` sampled it at the press; whether the zone restarted (screenshot, `ZoneGenRestart calls=`). This is `override`. |
| **R6** | Repeat R5 with the other candidate names if R5 did not restart. | Per name. |
| **R7** | The six `UI_Pause_obj` closure rows: which counted during R3 and R4? | For H-node. |
| **R8** | `hs_stop_game`, `hs_saves_inspect`, `hs_saves_restore`. | `changed` / `missing`. |

The pause menu's Exit is not a step and is never pressed for measurement.

If `override` is `not observed` (both controls passed and the at-press sample
held `readyValue`), H-var is recorded as not observed with a command-time
write, for the names sampled - not as falsified. The next reads are R2's diff unfiltered and the
`UI_Pause_obj` instance's own members; after that the work stops and the
owner decides whether another round is funded. A node state (H-node) is
recorded as `gate: ui-node`.

### Round 2

Research DLL `plugin_build\BloodPactPlugin_rel.dll` built for round 2,
installed by the owner as `mods/aurie/BloodPactPlugin.dll` with their previous
DLL kept beside it, after an out-of-band copy of the saves. `hs-drive` order:
`hs_selfcheck`, `hs_saves_backup`, `hs_launch`, `hs_select_character`, then
`hs_command` for each step, and at the end `hs_stop_game`,
`hs_saves_inspect`, `hs_saves_restore`. Oracles: a screenshot for the HUD
in-combat icon and for how the button looks; the **room name** for
"restarted" (round 1's allowed press went from an `Act_*` room to
`Town_01_rm`, and `ZoneGenRestart` did not count, so that row is not the
oracle); `UiAIngameRestart calls=` for "the press reached the activation".
One ordinary zone is enough, chosen by the owner. Exit is never pressed.
Kept for the record; its step names are in bold now that § Results carries
the plain rows.

| Step | Do | Record |
| --- | --- | --- |
| **C3** | In town, menu closed: `restartprobe hook` (22 rows; expect every row `native`), `restartprobe show`, wait two seconds, `restartprobe show`. | Both `control=` values equal. If the draw counts with the menu closed, the hold's "menu only" claim is false: record it, and S4 to S6 are `unmeasured`. |
| **S1** | In town, no enemies, open the pause menu: `restartprobe reset`; wait a second; `restartprobe show`; `menuprobe list UI_Button_obj`; `restartprobe dump button town`; wait a second; `restartprobe dump show town`; `restartprobe dump pause town`; `restartprobe vars`; `tgprobe deep snap town controller player global`; close the menu. | `control=` above 0 (C1); the draw's `a0` with Restart allowed; `selfIds=`; the button list line whose `text` is Restart (its `id` must be in `selfIds=`); both dumps' `names=`; the `path:` line. |
| **S2** | Walk to enemies, get hit, HUD icon on; open the menu: `restartprobe reset`; wait a second; `restartprobe show`; `restartprobe dump button fight`; wait a second; `restartprobe dump show fight`; `restartprobe dump pause fight`; `restartprobe dump diff town fight` (button, then pause); `restartprobe vars`; `tgprobe deep snap fight controller player global`; `tgprobe deep diff town fight tupm`; `tgprobe deep diff town fight` unfiltered; keep the menu open. | `a0` in combat; every `~`, `+` and `-` line of both dump diffs (the member names that differ are the enabled-flag and timer candidates); whether the `path:global.tupm[1].in_combat` sample changes across the 20 logged frames (does it count down?); the unfiltered diff's `changed= added= removed=` and its `tupm` lines. |
| **S3** | Still in combat, menu open: `restartprobe reset`; press **Restart** once (expect a refusal); `restartprobe show`; then `restartprobe reset`; press **Resume** (closes the menu); reopen it; `restartprobe show`. | Per row, `calls=` after the refused press: which node-API rows count on a press, and their logged arguments (a `UiSetRowEnabled` line names the row and the flag). After Resume: `UiACloseButton calls=` - above 0 means a sibling press does reach its activation in combat; 0 means this control is `unmeasured`, not that presses never dispatch - and which rows count every frame while the menu is open (`UiSetUpdateFunc` or `UiSetRowEnabled` every frame are step-time sites for `at <row>`). |
| **C4** | In combat, menu open, wait shown: `restartprobe reset`; `restartprobe hold player wasInCombat 0 confirm`; wait a second; `restartprobe hold stat`. | `writes` above 0 and `readbackOk` equal to `writes`. |
| **S4** | Continuing C4, hold still armed: `hs_screenshot`; press Restart within a second; `restartprobe show`; `restartprobe hold stat`. | `entryHeld=` against `entryOther=` (decides whether the draw site is usable); the screenshot (does the button stop showing the wait?); `UiAIngameRestart calls=`; the room afterwards (restarted?). This is `override` for the draw site. |
| **S5** | Only if S4 did not restart: `restartprobe hold off`; then one attempt at a time, each followed by `restartprobe hold stat`, a press, `restartprobe show` and `restartprobe hold off`: (a) `restartprobe hold path global.tupm[1].in_combat 0 confirm`, if S2 showed it counting or present in combat only; (b) `restartprobe hold button <member> <value> confirm` for each member S2's button diff named with a number or bool value (the town value); (c) if S3 showed a node-API row called every frame in combat, repeat (a) and (b) with `at <row>`; (d) if S3 showed `UiSetRowEnabled` (or another row) called with a bool or number argument that is false or 0 for the Restart row in combat: `restartprobe argset <row> a<i> 1 calls=2000 confirm`, press, `restartprobe show`, `restartprobe argset clear`. | Per attempt: the `hold stat` or `argset` line, `UiAIngameRestart calls=`, the room afterwards. Each is recorded as `works`, `not observed (site, scope, name, value, entryHeld/entryOther)` or `unmeasured (…)` - what was supplied, every time. |
| **S6** | `restartprobe hold off` (if still armed); close the menu; wait two seconds; reopen; `restartprobe hold stat`; `restartprobe show`. | `armed=no`; if a hold was left armed on purpose for this step, the `hold: disarmed (menu closed …)` line and `writes=` unchanged after the close; `UiRemoveNode calls=` moved on close. |
| **S7** | `hs_stop_game`, `hs_saves_inspect`, `hs_saves_restore`. | `changed` / `missing`. |

Reading the round-2 results into § Decision:

- `gate:` - `node-row` if S3 logged a node-API call naming the Restart row
  with a false or 0 flag in combat; `button-member` if S2's button diff named
  a member that flips and S5(b) worked; `player-var` if S4 worked on
  `wasInCombat`; `timer` if `tupm[1].in_combat` counts down and S5(a) worked;
  otherwise `upstream (not identified; …)` naming the closest finding.
- `owner:`, `variable:`, `readyValue:` - the scope, name (or `a<i>` of the
  row) and value of the attempt that worked, kind included; otherwise round
  1's candidates marked `(not confirmed)`.
- `override:` - `works`, `not observed (…)` or `unmeasured (…)` by the rule in
  § Candidates and controls, naming site, scope, name, value and
  `entryHeld`/`entryOther`.
- `shipRoute:` - `draw-write` (a hold at the draw site worked),
  `node-api-arg` (an `argset` or an `at <row>` hold worked, naming the row and
  argument), or `none`, in which case the work stops and the owner decides
  again.

### Round 3

Research DLL `plugin_build\BloodPactPlugin_rel.dll` built for round 3,
installed by the owner as `mods/aurie/BloodPactPlugin.dll` with their
previous DLL kept beside it, after an out-of-band copy of the saves.
`hs-drive` order: `hs_selfcheck`, `hs_saves_backup`, `hs_launch`,
`hs_select_character`, then `hs_command` for each step, and at the end
`hs_stop_game`, `hs_saves_inspect`, `hs_saves_restore`. Oracles: a
screenshot for the HUD in-combat icon and for how the button looks; the
**room name** for "restarted"; `UiAIngameRestart calls=` for "the press
reached the activation". Combat against the town training dummies is a valid
in-combat state (round 2), but an ordinary `Act_*` zone makes the room change
unambiguous - the owner picks. Exit is never pressed. `prospectprobe hook` is
not run in the same session (it detours four of the same scripts).

| Step | Do | Record |
| --- | --- | --- |
| **C1** | In town, menu closed: `restartprobe hook` (22 rows, expect every row `native`); open the menu; wait two seconds; `restartprobe show`. | `control=` above 0. |
| **T1** | In town, menu open: `restartprobe reset`; `menuprobe list UI_Button_obj`; hover Restart for a second; `restartprobe show`. | Restart's `id` from the list; `UiSetFocus calls=` above 0 with `a0=` a `UI_Button_obj`. This is the site's positive control, and the id the ring must name. |
| **C5** | In combat, HUD icon on, menu open, Restart greyed: `restartprobe reset`; `restartprobe hold arg0 enabled 1 at UiSetFocus confirm`; `restartprobe hold arg0 manualDisable 0 at UiSetFocus confirm`; hover Restart for a second **without pressing**; `restartprobe hold stat`. | Per slot, `writes` above 0 and `readbackOk` equal to `writes`, and ring labels `UI_Button_obj#<T1's id>`; `entryHeld` against `entryOther` per slot (does the write survive to the next hover frame?); `hs_screenshot` (does the button stop looking greyed?). |
| **T2** | Continuing C5, holds armed, still hovering: press Restart; `restartprobe show`; `restartprobe hold stat`; the room name. | `UiAIngameRestart calls=`; both stat lines; the ring's last runs (the entry state on the frames around the press); the room afterwards. This is `override` for the `UiSetFocus` site. |
| **T3** | Only if T2 restarted: `restartprobe hold off`; back in combat with the menu open, one member at a time - `hold arg0 enabled 1 at UiSetFocus confirm`, hover, press, `show`, `hold stat`, `hold off`; then the same with `manualDisable 0`. | Which member or members alone unlock the press: `UiAIngameRestart calls=` and the room, per attempt. |
| **T4** | In combat, menu open, holds armed as in C5: move the cursor off the menu; navigate to Restart with the keyboard or controller only; `restartprobe show`; press with the key or button; `restartprobe show`; the room. | `UiSetFocus calls=` with the cursor away (does keyboard focus call it?); the result of that press. `not run` if the owner has no keyboard or controller path. |
| **T5** | `restartprobe hold off`; arm one hold as in C5; close the menu; wait two seconds; reopen; `restartprobe hold stat`. | `hold: disarmed (menu not drawing since frame …)`; `writes=` unchanged after the close. |
| **T6** | `hs_stop_game`, `hs_saves_inspect`, `hs_saves_restore`. | `changed` / `missing`. |

Reading the round-3 results into § Decision:

- `override: works` - C1 above 0, C5 `readbackOk` equal to `writes` on every
  slot used with the ring naming T1's button id, and T2 (or a T3 attempt)
  restarted the zone - the room changed - with the HUD icon on and
  `UiAIngameRestart calls=1`.
- `override: not observed (UiSetFocus site, arg0, <members>, <values>; ring:
  held=yes on the frames before the press)` - the same controls passed, the
  press was refused, and the ring's last run before the press shows
  `held=yes`: the value was in place when the game could have read it.
- `override: unmeasured (overwritten before use, UiSetFocus site; ring:
  held=no throughout)` - the ring shows the game rewrote the members between
  our step-time writes, so whether the press saw our value is not known.
- `gate:` - `button-member (enabled)`, `(manualDisable)` or `(both)` from T3
  when `works`; otherwise `upstream (not identified; …)` with the closest
  finding. `owner: UI_Button_obj`; `variable:` and `readyValue:` name the
  member or members and the values that worked, kind included.
- `shipRoute:` - `setfocus-write`, or `none`, in which case the work stops
  and the owner decides again.
- T4 is recorded in its row and, if the mod ships, in the module guide's
  Known Limitations; it does not change `override`.

## Results

Round 1's session ran on 2026-09-22 with the research DLL whose sha256 starts
`44c78114`, after a saves backup (`20260922T143603Z_pre-restartprobe`); the
owner pressed the buttons. Each row quotes what the session printed, and
nothing else. Round 2's rows follow round 1's, and round 3's follow round
2's.

| Step | Printed | Reading |
| --- | --- | --- |
| R1 | In `Town_01_rm`: `Player_obj.wasInCombat=bool:false`, `Player_obj.combatRefresh=bool:false`, `global.dpsMeterResetCombat=1`; every other candidate name `absent` on every scope; `UI_Pause_obj: no instance` (menu closed). | Of the seven names, only these three exist; both player values are false in town. |
| R2 | In `Act_01_01` with the HUD crossed-swords icon visible: `wasInCombat` false to true, `combatRefresh` false to true; a new `global.tupm[1].in_combat=real:432` in the fight snapshot; `lastHit` filter `matching=0`. | 432 frames at this game's 144 fps is 3.0 s, the length of the wait; whether it counts down, and whether anything reads it, is not measured. |
| C1 | `hook`: 9 native, 0 blocked, 0 not found. `control=1033` with the menu open; the draw row `self=UI_Button_obj#5003`, `argc=1`, `a0=bool:false` on every logged frame in combat. | Pass: the instrument sees this build's direct calls. `#5003` is the object index, not an instance id. |
| R3 | In combat, Restart pressed and refused: `UiAIngameRestart calls=0`, `ZoneGenRestart calls=0`. | The press never reaches the activation: the gate is upstream of it, on the button/node side. |
| R4 | Out of combat, Restart worked: `UiAIngameRestart calls=1`, `self=UI_Button_obj#5003`, `argc=1 a0=array`; at the call `wasInCombat=false`, `combatRefresh=false`, `tupm[1].in_combat` gone; `ZoneGenRestart calls=0`; room afterwards `Town_01_rm`. | `ZoneGenRestart` was not observed on this path: 0 calls on a row attached `native`, with no positive control on that row, so this is not evidence that a restart never reaches it. The room change is the "restarted" oracle. |
| C2 | `set player wasInCombat 1` / `0` and `set player combatRefresh 1` / `0`: all `wrote=yes changed=yes`. | Pass. The write stored a real where the game's own value is a bool. |
| R5 | In combat, menu open: `combatRefresh` already `bool:false` in combat. `set player wasInCombat 0` `wrote=yes changed=yes` at frame 37710; the draw sample read `real:0` at 37710 and `bool:true` again by frame 37724; draw `a0` stayed `bool:false`; the press was refused, `UiAIngameRestart calls=0`. | `unmeasured (overwritten before use)`: the game recomputes `wasInCombat` while the pause menu is open (the world is not paused), so a command-time write cannot test it. `combatRefresh` is not the gate. |
| R6 | not run - R5 was unmeasured, not a negative, so there was nothing to repeat with other names. | - |
| R7 | `anon@6013` counted once at each menu open (`self=UI_Pause_obj#5206`); the session record names no count for the other five closures. | `anon@6013` runs once per open, not per frame, so it is not where the menu re-decides the button each frame; no per-frame count is recorded for the other five. The per-frame logic is in the object's own events (not hookable on this build) or in scripts round 0 did not list - hence round 2's node-API rows. |
| R8 | not recorded - the session's `hs_saves_inspect` / `hs_saves_restore` output is not in the record. | - |

Round 2's session ran on the evening of 2026-09-22 with the research DLL
whose sha256 starts `91f7282c`, after a saves backup
(`20260922T190508Z_pre-restartprobe-r2`). Combat was against the training
dummies in `Town_01_rm` - the HUD crossed-swords icon on and Restart greyed,
a valid in-combat state; no `Act_*` room was used.

| Step | Printed | Reading |
| --- | --- | --- |
| C3 | `hook`: 22 native / 0 blocked / 0 not found. Menu closed: `control=0` at frame 2610 and again at frame 3960. | Pass: the Restart draw does not run with the menu closed. |
| S1 | In town, menu open: `control=450`; `selfIds=262247`, the id of the `menuprobe list UI_Button_obj` line whose `text="Restart"`; the draw's `a0=bool:false` in town too; button dump `names=85` with `enabled=bool:true`, `manualDisable=bool:false`, `buttonDrawFunc=UiDrawIngameRestart`, `uiNodeCallstack="PauseRestart"`, `updateFunc=undefined`; pause dump `names=64`; `path:global.tupm[1].in_combat` unresolved in town. | C1 pass. One Restart button reaches the draw, instance 262247. The draw's `a0` is false with Restart allowed, so it is not the gate. The button carries its own `enabled` and `manualDisable`. |
| S2 | In combat, menu open: button diff town to fight `~ enabled: true -> false` and `~ manualDisable: false -> true`, plus instance, `masterUi` and parent ids (the menu is rebuilt on open); pause diff: only instance ids and `image_index`; `wasInCombat=true`; `global.tupm[1]` is an unrelated instance (a summon) with no `in_combat`. | The button's own two members flip with combat. The pause instance carries nothing combat-shaped. `global.tupm[1].in_combat` is not a candidate. |
| S3 | Refused press: `UiAIngameRestart calls=0`; `UiSetRowEnabled`, `SetGlobalUiEnable`, `EnableNav` and `UiSetUpdateFunc` `calls=0`; `UiSetFocus` counts every frame while the cursor hovers Restart, with `self=UI_Pause_obj`, `a0` the Restart button and `a1` the pause instance. Resume press: `UiACloseButton calls=1` (`self=UI_Button_obj`, `a0=array`). Reopen: `anon@6013` once, `UiCreateNode` ×10 with `a2=UI_Button_obj`. | Press-path control pass: a sibling press does reach its activation in combat, so a refused Restart press is refused on the button's side. None of the four attached node-API setters was observed to be called (native rows, with `UiSetFocus` counting in the same window as the control). `UiSetFocus` is the one per-frame, step-time call handed the button - round 3's site. |
| C4 | `hold player wasInCombat 0` at the draw: `writes=1260 readbackOk=1260 entryHeld=1250 entryOther=10`. | Pass: the draw-time hold reaches the value, and the value stays in place between draws. |
| S4 | Press with that hold still armed (final `writes=7230 entryHeld=7179 entryOther=51`): refused, `UiAIngameRestart calls=0`, the button still greyed. | `not observed (draw site, player, wasInCombat, false; entryHeld=7179 entryOther=51)` - held in place, and the press still refused. |
| S5 | (b) `hold button enabled 1` at the draw: `writes=5010 readbackOk=5010 entryHeld=0 entryOther=5010`, press refused. (b) `hold button manualDisable 0` at the draw: `writes=10380 readbackOk=10380 entryHeld=0 entryOther=10380`, press refused. (a), (c) and (d) not run. | Both `unmeasured (overwritten before use, draw site)`: every write read back, and every next call found the game's value again, so the press never tested ours. (a) and (c) and (d) not run: no node-API row is called every frame in combat except `UiSetFocus`, on hover only, whose self is the pause menu - so scope `button` there would have written `UI_Pause_obj` - and no `path` root reaches the button. |
| S6 | not recorded - the session record carries no output for this step. | - |
| S7 | not recorded - the session's `hs_saves_inspect` / `hs_saves_restore` output is not in the record. | - |

Round 3's session ran late on the evening of 2026-09-22 with the research
DLL whose sha256 starts `d40a4f25` (built from ForgePact `dab08ce`), after a
saves backup (`20260922T203033Z_pre-restartprobe-r3`); the owner pressed the
buttons. Combat was against the training dummies in `Town_01_rm` again, a
valid in-combat state. C1 passed first: `hook` 22 native / 0 blocked / 0 not
found, and `control=450` with the menu open in town.

| Step | Printed | Reading |
| --- | --- | --- |
| T1 | Hovering Restart: `UiSetFocus calls=450` in the same 450 frames; its `a0` instance 262264, the `menuprobe list UI_Button_obj` line whose `text="Restart"` (262264); `self=UI_Pause_obj`, `a1` the pause instance 262261; `selfIds=262264`. | The site's positive control: `UiSetFocus` is handed the Restart button itself, every frame it is hovered. |
| C5 | In combat (`wasInCombat=bool:true`), cursor moved straight onto Restart, both holds armed (`arg0 enabled 1` and `arg0 manualDisable 0` at `UiSetFocus`): after about a second `writes=450 readbackOk=450` on each slot; ring labels `UI_Button_obj#264101` only, the draw's `selfIds` for that open of the menu; entry `enabled=bool:false` and `manualDisable=bool:true` on every frame (`held=no`); written as bool, the kind found. `hs_screenshot`: Restart drawn lit under the cursor, not greyed. | Pass: every write read back, on the Restart button and nothing else. The game puts both members back every frame, so the ring alone cannot say whether a press sees our value - T2 decides that. |
| T2 | Press with both holds armed: `UiAIngameRestart calls=1` at frame 27419, the slots' `lastWriteFrame`; sampled at that call `Player_obj.wasInCombat=bool:true` and `combatRefresh=bool:true`. At the press `hold[0]` (enabled) and `hold[1]` (manualDisable) each `writes=4560 readbackOk=4560 entryHeld=0 entryOther=4560`, each ring one run `ring: frames 26396..27419 x1024` on `UI_Button_obj#264101`, no other instance written. Afterwards `UI_Pause_obj` gone, room `Town_01_rm`, and the owner saw the zone restart. | `override: works` at the `UiSetFocus` site: the press reached the Restart activation and the zone restarted while the game still counted the player in combat. The fight was in town, so the unchanged room name is not the oracle here; the activation call, the menu closing and the owner's observation are. |
| T3 | (a) `arg0 enabled 1` alone, `wasInCombat=bool:true`: `writes=5130 readbackOk=5130` on `UI_Button_obj#273868` only, entry `false` on every frame; press refused, `UiAIngameRestart calls=0`, Restart still drawn greyed. (b) `arg0 manualDisable 0` alone, `wasInCombat=bool:true`: `writes=3976 readbackOk=3976` on `#273868` only, ring frames 48402 to 49425; Restart drawn lit under the cursor; press gave `UiAIngameRestart calls=1` at frame 49425, the `lastWriteFrame`, with `wasInCombat=bool:true` at the call; `UI_Pause_obj` gone and the owner saw the zone restart. | `gate: button-member (manualDisable)`. `enabled` alone: not observed (UiSetFocus site, arg0, enabled=true; ring `held=no`, put back every frame) - the same write pattern that works for `manualDisable`, so `enabled` need not be touched. |
| T4 | Keyboard navigation does not reach the pause menu's buttons (the owner: "Keyboard doesn't work"); no controller was available. With the mouse off the menu, `UiSetFocus calls=0`. | not run - this setup has no keyboard or controller path to Restart. The mod works while the mouse hovers Restart; that is recorded as a known limitation. |
| T5 | One `arg0 manualDisable` hold at `UiSetFocus`, menu closed and reopened: `hold: disarmed (menu not drawing since frame 93865; writes=784)`, and `writes=784` unchanged after the reopen. The ring also shows `manualDisable=false` written onto other buttons the cursor crossed, `#288234` and `#288243`, whose own value was already false (`held=yes`). | Pass: the draw-keyed disarm stops the hold when the menu closes. `arg0` writes whatever `UiSetFocus` is handed, so the shipped mod must identify the Restart button by its own signal before it writes (§ Decision, `owner`). |
| T6 | `hs_saves_inspect 20260922T203033Z_pre-restartprobe-r3`: changed `herosiege13.hss`, `inventory_order_13.hss` and `shop.ini`; added none; missing none. | The changes are the owner's own play; the saves were not restored. |

One more check, at the owner's request: both members held at the Restart
draw instead (`button enabled 1` and `button manualDisable 0`,
`writes=6360 readbackOk=6360` each on `UI_Button_obj#288236`), with the
cursor off Restart. The owner reported Restart still drawn greyed. So the
greyed look follows the value the game sets at step time, not a write at the
draw: with the mod, Restart stays greyed in combat until the cursor is on
it, lights up under the cursor, and works when clicked.

## Decision

owner: UI_Button_obj (the pause menu's Restart node, told apart from every other node by its own uiNodeCallstack "PauseRestart"; round 3)
variable: manualDisable (round 3 T3 - manualDisable alone unlocks the press; enabled alone did not)
readyValue: manualDisable=false (bool; round 3 - written in the kind read at entry)
gate: button-member (manualDisable)
override: works (UiSetFocus site, arg0, enabled=true and manualDisable=false, bool; round 3 T2 - UiAIngameRestart calls=1 at frame 27419 with wasInCombat=bool:true; manualDisable alone the same at frame 49425)
shipRoute: setfocus-write
