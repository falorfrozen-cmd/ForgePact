# Loot announcements (ForgePact #17): research

**Question.** Online, Hero Siege announces a rare drop in the in-game chat.
Offline it shows nothing. Which call, made by name from ForgePact, shows that
line offline for a Heroic, Angelic or Unholy item the game drops: the game's
own announcement path, or one of the chat routes already proven offline?

**Why it matters.** Issue #17 asks for the same announcement offline as
online. The owner prefers the game's own text, colour and sender, which only
the game's own path gives; `AGENTS.md` § "Don't Suspend the Game's Own
Runtime" prefers one call the game already makes over drawing our own line.
So this research tries the game's path first and keeps the proven
`ChatAddServerMessage` line as the floor.

**Posture.** Everything here is measured runtime behaviour, a reading written
in our own words, or our own code. Game objects and scripts are named by their
`hs-game-sdk` names and indices. No game script text, no address and no struct
offset appears. A reading is labelled as a reading, and it is not a fact until
a live session records it.

**Out of scope.** Items below Heroic (Satanic, Mythic and lower), gold, gems,
materials and relics; items the player drops from the bag; a clickable item
link (`UiAChatLobbyShowItemDrop`), sounds or banners; any change to the online
path, the chat feed, `Chat_obj` or the network. No packet is sent on purpose.

## Static search

Every chat, announce and drop name in `hs-game-sdk/cpp/include/hs_game_sdk/scripts.hpp`
(substrings `Chat`, `Announce`, `ItemDrop`, `DropItem`, `Loot_Ground`) and
`objects.hpp` (`Chat`, `Ingame_Chat`). The ones that matter, all present in
`scripts.py` too:

| SDK name | Role (static reading) | `lootannprobe on` |
|---|---|---|
| `gml_Script_anon@1138@gml_Object_Loot_Ground_obj_Create_0` | the rare-drop announcement closure bound on each ground item (hidden-loot research named it by role) | hooked, a 0-argument method |
| `gml_Script_GetRareDropAnnouncement` | "is this drop announced?" for the online path | hooked, its 3 arguments and return recorded |
| `gml_Script_NetworkSendChatMessageIngame` | builds the item line, adds it locally, then sends | hooked |
| `gml_Script_GetItemDropMessage` | the localized "found <item>" text | hooked |
| `gml_Script_ChatAddMessage` | adds a built line to the feed (15 arguments) | hooked (positive control: fires inside `ChatAddServerMessage`) |
| `gml_Script_ChatAddServerMessage` | the proven offline line, red `SERVER:` prefix | hooked |
| `gml_Script_ChatAddIngameMessageFiltered`, `gml_Script_CA_chatIngame` | the receiver side of an online announcement | hooked (expected 0 offline) |
| `gml_Script_PacketSend`, `gml_Script_ChatSendServerMessage`, `gml_Script_ReportClient` | network sends the closure or the sender can reach | hooked, count only (a non-zero is a finding) |
| `gml_Script_LootGroundInit`, `gml_Script_LootGroundDrop`, `gml_Script_LootGroundCreateFromItem` | the attachment points | counted (`LootGroundInit` through the detour it shares with hidden loot sleep, `LootGroundDrop` through its own count-only `fp_lap_lgdrop` detour) |
| `gml_Script_anon@6032@...`, `gml_Script_anon@11081@gml_Object_Loot_Ground_obj_Create_0` | the loot-filter closure and the step dispatcher, same object | hooked, count only (controls) |

Objects: `Loot_Ground_obj` 2513, `Ingame_Chat_obj` 2258, `Chat_obj` 911,
`UI_Ingame_Chat_obj` 5107, `Menu_Controller_obj` 2674. No earlier
`ForgePact/docs/*-research.md` had read the announcement chain. The chat call
route itself is proven (dungeon-chest research, § Chat route).

## Static reading

Our own words, from a local decompile (2026-10-04). Every sentence here is a
reading, not a measurement, and no decompiled line is kept in this repository.

- **Static reading: the announcement is a closure bound on the ground item.**
  `Loot_Ground_obj`'s Create event binds three methods; the one named
  `anon@1138@...` takes no arguments (self and other only). A search for
  direct callers found none for it, while the same search found seven sites
  for the other targets of the same run (the positive control), so it is
  reached as a method value, not by a compiled direct call. Who invokes it,
  and whether anything does offline, is not established.
- **Static reading: it reads the item's rarity and announces it in chat.**
  The closure reads the rarity of its ground item's item (`itemInstance`,
  `kGroundItemInstanceField` in `hs-game-sdk/cpp/include/hs_game_sdk/player.hpp`)
  and announces the drop by sending a chat line through
  `NetworkSendChatMessageIngame`, with a different colour per rarity. The
  rarities it handles are Satanic (6) as well as Angelic (7), Heroic (9) and
  Unholy (10), which is why Live procedure 1 also tries a Satanic item. That
  the key it reads is `"27"` (`docs/RUNTIME_DATA_MODELS.md` § 16.4) is an
  inference from the rarity codes it handles, not a reading; `lootannprobe
  status` after a `place` shows what the closure's hooks saw. It can also
  reach `ChatSendServerMessage` (through `Chat_obj` instances) and
  `ReportClient` (through `Menu_Controller_obj` instances). Measured offline:
  `Chat_obj` 0 and `Menu_Controller_obj` 1 (Live procedure 1, and `Chat_obj`
  0 in dungeon-chest Live 1), and `ChatSendServerMessage`, `ReportClient` and
  `PacketSend` counted 0 in Live procedure 1. The closure itself never ran in
  that session, so those zeros do not say whether its network-facing calls
  run offline; that is not established.
- **Static reading: `GetRareDropAnnouncement(a, b, c)`** answers true for
  Angelic (7) and Unholy (10), and for some material and socketable ids.
  Heroic (9) is not decided there; the closure handles it itself. Whether the
  closure still announces after a false answer depends on a value whose
  meaning is not established. It was never observed firing offline:
  `angelic-roll-hook-research.md` detoured it over 374 game rolls (controls
  passing) and read 0, with no rare drop in those rolls, so that zero does not
  say whether a Heroic drop reaches it. `lootannprobe on` records its
  arguments and its answer.
- **Static reading: `NetworkSendChatMessageIngame`** is called with five
  arguments: a runtime-filled value whose meaning is not established, the real
  18687, the item, a colour and the int64 3 (the calling convention the
  `netsend` sink supplies). With that last argument 3 the text is
  `GetItemDropMessage(item)`; the sender adds the line locally through
  `ChatAddMessage` and sends it over the network under a condition not
  established. Live procedure 1 counts `PacketSend`.
- **Static reading: the receivers.** `CA_chatIngame` calls
  `GetItemDropMessage` and `ChatAddIngameMessageFiltered` (12 arguments),
  which forwards to `ChatAddMessage`. `ChatSendItem` (the player linking an
  item from the inventory grid) calls `NetworkSendChatMessageIngame` too.
- **Static reading: `GetItemDropMessage`** uses `GetLootName` and
  `GetLocalized`: the line is localized and names the item.

Measured before this research, and what the plan stands on:

- `ChatAddServerMessage` called by name (`asset_get_index` of the short name,
  `script_execute` through `CallBuiltinEx`) with the local `Player_obj` as
  `self` and one string shows a red `SERVER: <text>` line; inside it the game
  calls `ChatAddMessage` with 15 arguments (sender `"SERVER"`, the text, two
  reals, two int64s, two reals, a `[hh:mm]` string, six `undefined`).
  `ChatAddMessage` called directly with the sender `"ForgePact"` showed
  `[00:49] ForgePact : <text>`. (`dungeon-chest-research.md` § Chat route.)
- Offline, one `Ingame_Chat_obj` exists in a loaded game; `UI_Ingame_Chat_obj`
  and `Chat_obj` were 0 (2026-10-03).
- `LootGroundInit(instance, item)` is reached by the game's own drops, and its
  argument 0 is the ground item as a reference (hidden-loot Live 3, 1,473 of
  1,473 calls). `LootGroundCreateFromItem` is not on the kill-drop path
  (angelic research: every call was ForgePact's own), so it places a test
  item but cannot be the shipped trigger.

## Instrument

Two halves: the mod's adapter, which ships, and `lootannprobe`, which does not.

### The mod (`lootann`, both builds)

`plugin/include/ForgePact/LootAnnounceMod.hpp` decides; the adapter in
`plugin/ModuleMain.cpp` (from "Loot announcements (LootAnnounceMod.hpp): the
adapter" to "end of the loot announcement adapter") reads and speaks.

- **Hooks**, both by SDK name through `HookOneScript`, installed once per
  session when the switch is on, setup is done and the local player resolves
  through `HhResolveLocalPlayer` (since 2026-10-08, § The switch on at launch;
  before that, on the first `lootann 1` after setup or the first frame after
  setup with the switch on):
  `LootGroundInit`'s one detour, shared with hidden loot sleep (installed by
  `HiddenLootInstall`; a second inline detour on the same script would be
  refused), and `CreateItemNew`'s shared `Hook_CreateItemNew` (hook id
  `fp_lootann_new` when no other feature holds it yet; the Custom Forge, Item
  Truth, signature drops and the research build's item inspection install the
  same hook, and then its route is read from the saved original the way
  signature drops read theirs). Both must be `both`: `LootGroundCreate`
  calls `CreateItemNew` directly, which a table-only hook never sees.
- **The creation guard** (Replan 1, after Live procedure 2 below). An item
  counts only when the game built its item struct through `CreateItemNew` in
  the frame it reached the ground or the frame before. While the switch is
  on, every `CreateItemNew` return the shared hook sees, inner and outermost
  alike, notes two keys into the core's creation window: argument 0 (the item
  instance `LootGroundCreate` hands in) and the returned item. A key is a
  struct's object pointer or a reference's value, compared and never
  followed. A window holds at most `kCreationCap` (4096) keys; a note past
  that counts `create-overflow` and is not kept. A key noted before frame
  T's decisions is recent at T and T+1 and forgotten after T+1; switching off
  clears the window. A bag drop, a re-drop after a pickup, or anything else
  that puts an existing struct on the ground finds no recent note and is held
  (`held-bag-drop`, which keeps its name and its `heldBagDrop` modstate key).
  It replaced a window the mod held open around a count-only
  `LootGroundDrop` detour, which counted 0 while Live procedure 2's bag drop
  reached `LootGroundInit` and was announced. The guard does not need to know
  which script the bag drop runs through. Live procedure 3 measured it hold
  the owner's bag drop and pass 215 of the game's own drops (below); it would
  not hold a bag drop whose ground struct was a copy built through
  `CreateItemNew` in that frame or the one before.
- **Inside `LootGroundInit`'s call** (`LootAnnounceOnInit`, after the game's
  original and after hidden loot's consumer): argument 0 and `self` are
  reduced to durable handles (a number or reference as it is; an instance
  pointer as its own `id`). Nothing is read or said inside the call.
- **At the end of the frame** (`LootAnnounceTick`, before hidden loot's tick,
  which may put a filter-hidden drop to sleep): the first handle that is a
  live `Loot_Ground_obj` is the ground item; its `itemInstance` is the item,
  whose key is derived the same way as at the note (no key counts `no-key`
  and is decided as not recently created); the rarity is the item's
  `itemInfoStruct["27"]`, kept only as a number; its identity is its
  `itemType` and `itemTimeStamp` when the stamp is real (not empty, `0` or
  `undefined`), otherwise the ground id with the stamp. An item whose
  identity could not be read (a real stamp with no numeric `itemType`, or no
  real stamp and no readable ground `id`) counts `no-identity` and is not
  decided, so two unread items never share one identity; that case was not
  observed live. The core then
  decides, in this order: off, not recently created (`held-bag-drop`),
  unread rarity, a rarity not in {9, 7, 10}, an identity already announced
  (`held-duplicate`), or announce. After the batch the creation window ages
  once.
- **The sink**: `LootAnnounceSink(item, lootInst)` runs the body
  `LootAnnounceMod::kShippedSink` names. All four bodies exist in both builds:
  1. `method`: the ground item's method variable whose function name
     (`variable_instance_get_names`, `is_method`, `method_get_index`,
     `script_get_name`) equals the SDK closure's name, invoked with no
     arguments through `InvokeMethodValue`, the ground item as self and other.
  2. `netsend`: `NetworkSendChatMessageIngame` by name, self the ground item,
     arguments (a0, 18687, item, `0xA2FCFF`, int64 3), a0 `undefined` or the
     local player's id (`kLaNetSendA0IsPlayer`).
  3. `chatadd`: `GetItemDropMessage(item)` by name (self the player), then
     `ChatAddMessage` with the measured 15-argument shape, the character's
     name as sender.
  4. `server`: `ChatAddServerMessage` by name, self the player,
     `<character> found <item name>` (`itemInfoStruct["28"]`).
  The character's name is the player's `name` variable when it is text, else
  `You`. A sink that cannot show its line logs one line naming the field that
  failed (`lootann: no line (sink <name>) - <field> ...`, the first 20) and
  counts `sink-refused=`.
- **Lines**: `lootann 1` answers `lootann: on route=<sink> init-hook=<route>
  create-hook=<route> install=<state>` (`both`, `table-only`, `none` or
  `not-installed` for a route; `not-armed`, `waiting-for-character` or
  `installed` for the install, since 2026-10-08), plus one warning
  line for a hook that is not `both` (for `CreateItemNew`: the game's own
  drops may not be seen as new, so nothing would be announced); `lootann 0`
  answers `lootann: off`; `lootann` / `lootann stat` answers
  `lootann: on|off route= seen= announced= held-rarity= held-no-rarity=
  held-duplicate= held-bag-drop= sink-refused= remembered= created=
  create-overflow= init-hook= create-hook= install= unidentified= no-item= no-key=
  no-identity= queue-full=`. The
  modstate JSON carries `lootAnnounce` with `on`, `route`, `seen`,
  `announced`, `heldRarity`, `heldNoRarity`, `heldDuplicate`, `heldBagDrop`
  and `sinkRefused`.

### `lootannprobe` (research build only)

Under `#ifndef FORGEPACT_RELEASE`, after `dungeonprobe`. Every form:

- **`lootannprobe on`** (after a character is loaded) attaches the sixteen
  rows of the Static search table, once; the hooks stay in. A row attaches by
  the first rule that applies: `via fp_hiddenloot_init (<route>, ...)` for
  `LootGroundInit` (the detour the mod shares counts the call); `via
  angelicprobe <row>` or `via
  dungeonprobe chat hook <id>` when that research hook already holds the
  script (its counter is read, nothing is hooked twice, and its arguments are
  not recorded here); `both` when the table entry is the game's own code
  (`HookOneScript`, both routes, a count-only detour), or `TABLE-ONLY (...)`
  when its inline detour failed; `detoured (under table-only
  Hook_LootGroundCreateFromItem)` for the research build's startup table swap
  of `LootGroundCreateFromItem`. Anything else is `blocked: ...` or `not found
  (...)`. Each `both` row prints `HookOneScript`'s `HOOK INSTALLED on
  <script>`; every other row prints `lootannprobe: <script> <route>`. Then it
  prints `status`.
- **Recorded per call**: the count; for the first three calls (every call,
  for rows that are not per-step hot) the `self` object, `argc` and each
  argument's kind with a string's first 40 characters
  (`self=<object> argc=<n> args=<kinds>`), logged as `lootannprobe <script>
  #<n> ...` for the first three. `GetRareDropAnnouncement` also records its
  three arguments' values and its return (`args->ret=a,b,c -> ret`). The two
  `Loot_Ground_obj` controls (`anon@6032`, `anon@11081`) are recorded on their
  first three calls only, then counted.
- **`lootannprobe status`** (or bare `lootannprobe`) prints `lootannprobe:
  on|off rows=16 attached=<n>`, then one `lootannprobe row <script>
  route=<route> calls=<n> last=<what the last recorded call carried>` line per
  row, zero counts included (`calls=n/a` for a row that cannot see its
  script), then `lootannprobe census Loot_Ground_obj=<n> Ingame_Chat_obj=<n>
  Chat_obj=<n> Menu_Controller_obj=<n>`.
- **`lootannprobe place heroic|angelic|unholy|satanic|mythic|common`** builds one
  item the way `sigdrop` does (`json_parse`, then `InitItemFromJson` with the
  global instance as self; a Heavy Belt base, and `angelic` with `sigdrop`'s
  Headhunter seed), writes `itemInfoStruct["27"]` = 9, 7, 10, 6, 5 or 1 as Custom
  Forge writes it, and places it 48 px right of the player through the game's
  own `LootGroundCreateFromItem(x, y, item)`. It prints `lootannprobe place
  <r>: "27"=<n> "28"=<name> itemType=<n> instance=<kind> id=<id> at <x>,<y>`,
  and the item becomes the newest ground item.
- **`lootannprobe methods`** lists the newest ground item's method variables
  (`lootannprobe methods on <which>: <var> -> <script>#<index>, ...`, the
  SDK closure marked `[SDK closure]`). The index is `method_get_index`'s
  value taken as a number (a REAL or a `VALUE_REF`), and the name is
  `script_get_name` of that number, as `CiTryResolveMethod` resolves one; a
  name that does not resolve still prints its index (`<undefined>#<n>`), and
  an index that is not a number prints `#(<kind>)`. Live procedure 1's build
  passed the raw value instead and could name no `anon@` method (Results).
  After the listing it prints the anon control, `lootannprobe methods: anon
  rows resolved: <k> of <n>`: `n` counts the listed method variables named
  `m_LootFilter` or `m_LootGroundDeActiveStep`, and `k` those of them whose
  name resolved to an `anon@<m>@gml_Object_Loot_Ground_obj_Create_0` closure.
  Those two are the control because `Loot_Ground_obj`'s Create event binds
  both to `anon@` closures (static reading, `dev2-bug-batch-research.md`
  "#95 part 1"), and `CiTryResolveMethod` resolved `m_LootGroundDeActiveStep`
  to an `anon@` closure on this runner (`anon@5164`, on a quest item;
  measured, 2026-09-11, `pet-quest-collector-c-research.md`), so a working
  resolver can name one. **The listing is INSTRUMENT-BLIND
  when `k` is 0**: a missing SDK closure row then says nothing about the
  game, and `method-found` is recorded `not-observed (instrument-blind)`,
  never `fail`. A passing control proves the resolver can name the two
  control rows, not every row: the closure can sit on another row
  (`m_AngelicMessage`, say) that still came back unnamed. So the listing
  then prints `lootannprobe methods: unresolved rows: <u> of <total>`,
  where `total` counts every listed row and `u` those with no name
  (`<undefined>#<n>`, `?#?`, `?#(<kind>)`, `(unreadable)`, an empty name,
  or a `(read threw)` entry), followed by those rows in parentheses with
  their indexes. `method-found` is a `fail` only when `k` ≥ 1, `u` is 0
  (every listed row resolved to a name) and no row names `anon@1138`;
  with `k` ≥ 1 and `u` > 0 it is recorded `not-observed (row unresolved:
  <each unresolved row with its #index>)`. `s_lootDrawData` resolves in either case: it is a *named* method,
  so it is the control for named methods only and cannot tell a working
  listing from a blind one, since the failure being fixed touched only
  `anon@` methods. Then it prints `lootannprobe methods: SDK closure
  anon@1138@gml_Object_Loot_Ground_obj_Create_0 found as variable <var>` or
  `not found`. The newest ground item is `place`'s return or `LootGroundInit`'s
  argument 0 while it is still a live `Loot_Ground_obj`, else the room's last
  one.
- **`lootannprobe try <n>`** runs the mod's own sink body n against the newest
  ground item: 1 `method`, 2 `netsend` with a0 `undefined`, 3 `netsend` with
  the player's id, 4 `chatadd`, 5 `server`. It prints the ground item and its
  `"27"`/`"28"`, then `lootannprobe try <n> supplied script=<name>
  self=<what> args=<kinds> -> calling` just before the call (so a crash still
  leaves its line), then `lootannprobe try <n> supplied script=<name>
  self=<what> args=<kinds> -> dispatched=<0|1> ret=<kind>` (with `refused:
  <field>` when it did not dispatch), then `lootannprobe try <n> deltas:
  <script>+<k>, ...` over every row.
- **`lootannprobe say <text>`** calls `ChatAddServerMessage` with that text,
  the proven route, printing the same supplied and dispatched lines.

The positive control for the call route is the existing `dungeonprobe chat
control` (`IsDefined` by name, defined→true, undefined→false). Never run
`dungeonprobe on` or an `angelicprobe` form after `lootannprobe on` in the
same session: their installers do not know these rows.

## Live procedure 1

Research build, route finding, run by `live-operator` (the full procedure is in
the workorder): the call-route control, `lootannprobe on`, `place heroic` and
`status` (does the game run its own announcement for a placed Heroic item
offline?), `methods`, `try 1` to `try 5` with a screenshot each, `place
satanic` with `try 1`, and two minutes of kills to see whether a natural drop
reaches `LootGroundInit` and the closure.

### Results

Session 2026-10-04 14:15-15:03 UTC, research build (sha256
`29ae0c00…eded`, matched the lease), slot 14 Sorak, Town of Inoya, then the
Outskirts of Inoya and Chilling Lake for the kills. The saves were restored
afterwards by the driver from the session's backup, not by the operator
(`hs_saves_inspect` after the restore: 0 changed, added or missing).
Every check, with what was supplied and what was seen:

| Check | Supplied | Seen | Result |
|---|---|---|---|
| `marker` | `lootannprobe status` before `on` | `lootannprobe: off rows=16 attached=0` | pass |
| `control` | `ping` | `pong (YYTK 4.0.1)` | pass |
| `chat-call-control` | `dungeonprobe chat control` | `PASS defined->true undefined->false` | pass |
| (install) | `lootannprobe on` | 16 of 16 rows attached, none `not installed`; `LootGroundInit` counted through `fp_hiddenloot_init` (both routes, shared), `LootGroundDrop` through `fp_lootann_drop` (both), `LootGroundCreateFromItem` detoured under the table-only `Hook_LootGroundCreateFromItem`. Census: `Loot_Ground_obj` 0, `Ingame_Chat_obj` 1, `Chat_obj` 0, `Menu_Controller_obj` 1 | recorded |
| `init-counts-placed` | `lootannprobe place heroic` (`"27"`=9, `"28"`=Heavy Belt of Balance, itemType 8) | `LootGroundInit` 1 (arguments ref, bool), `LootGroundCreateFromItem` 1, the Create-event method `anon@6032` 1 with `self` `Loot_Ground_obj` | pass |
| `closure-fires-offline` | the same placement, then about 450 natural drops | the closure 0, `GetRareDropAnnouncement` 0, `NetworkSendChatMessageIngame` 0; no line in chat | not observed |
| `method-found` | `lootannprobe methods` on the placed item | four method-valued variables (each passed `is_method`): `m_AngelicMessage`, `m_LootFilter` and `m_LootGroundDeActiveStep` printed `<undefined>`, the string `script_get_name` returned for `method_get_index`'s raw, unconverted value; `s_lootDrawData` resolved to `Pickup_Parent_obj`'s Create method. No row matched the SDK closure's name | uninterpretable (instrument) |
| `route-method` | `try 1`: the closure by name, `self` the ground item, no arguments | `refused: method` (no listed name matched), nothing called, no hook moved, no line | uninterpretable (instrument) |
| `route-netsend` | `try 2` / `try 3`: `NetworkSendChatMessageIngame`, `self` the ground item, arguments `undefined` (try 2) or the player reference (try 3), then real 18687, the item struct, a colour real, int64 3 | `script_execute` threw or returned a failure status both times; the script's and `GetItemDropMessage`'s hooks each counted 1 per try, `PacketSend` 0, no line | fail |
| `route-chatadd` | `try 4`: `GetItemDropMessage(item)`, `self` the local `Player_obj` | `script_execute` refused; its hook counted 1, no line | fail |
| `route-server` | `try 5`: `ChatAddServerMessage("Sorak found Heavy Belt of Balance")`, `self` the local `Player_obj` | dispatched, returned `undefined`; the game called `ChatAddMessage` with sender `"SERVER"` and a `[16:17]` stamp; red line `[16:17] SERVER: Sorak found Heavy Belt of Balance` | pass |
| `below-heroic-method` | `place satanic` (`"27"`=6), then `try 1` | the same `refused: method`, no line | not observed |
| `natural-drop-init` | two minutes of kills outside town | `LootGroundInit` 1 → 450 (last `self` `Zombie_Passive_obj`, arguments ref, bool), `Loot_Ground_obj` census 437; the closure, `GetRareDropAnnouncement`, `PacketSend`, `ChatSendServerMessage`, `ReportClient`, `ChatAddIngameMessageFiltered`, `CA_chatIngame` and `LootGroundDrop` all 0 | pass |

What the session established, measured:

- A drop announcement by the game itself was not observed offline: neither a placed
  Heroic item nor about 450 natural drops (Heavy Belt of Balance placed;
  Ymir's Frozen Shroud, Pitfiend's Thorn and others dropped, rarity not read)
  moved the closure's or `GetRareDropAnnouncement`'s count off 0.
- The placed ground item carries four method-valued variables. Three of
  them, `m_AngelicMessage` (the one named for an announcement) among them,
  could not be named by this build's probe: it handed `script_get_name`
  `method_get_index`'s raw value instead of a number, and got `<undefined>`
  back for every `anon@` method, while the named `s_lootDrawData` resolved.
  The variables hold methods; which functions they are was not read. So
  whether `m_AngelicMessage` wraps `anon@1138` is not established, and
  `method-found` and `route-method` measured the probe, not the game (the
  instrument-blindness review of round 2). The probe now converts the index
  first, as `CiTryResolveMethod` does, and prints `#<index>` beside each
  name. Live procedure 3 retested both (Live procedure 2 ended before it got
  there): the fixed probe still named no `anon@` method (below).
- `NetworkSendChatMessageIngame` and `GetItemDropMessage` refused when
  called by name from ForgePact with the shapes supplied above, and neither
  counted a `PacketSend`. Only one item argument was ever supplied, the
  ground item's `itemInstance` struct. In tries 2 and 3 `GetItemDropMessage`'s
  hook counted +1 during the try (measured); its hook does not record its
  caller, so that the call came from inside `NetworkSendChatMessageIngame`'s
  own body is inference, and so is reading the item argument as the likelier
  cause than the by-name route or `self`. Whether another item shape (an
  item save struct, as
  `ChatSendItem` passes per the static reading) would be accepted was not
  tried.
- `ChatAddServerMessage` with our own text is the route that shows a line,
  and the player's `name` holds the character's name (`Sorak`).
- Every natural drop seen reached `LootGroundInit` with a ground item
  reference as argument 0, as the hidden-loot research measured. The
  session did not read the rarity of any natural drop, so whether a natural
  Heroic, Angelic or Unholy drop reaches it was not observed here (Live
  procedure 3 counted one announced, below).
- `LootGroundDrop` stayed at 0 through the kills: a game drop passing
  through it was not observed. Its hook had not counted a call live, so this
  zero had no positive control; Live procedure 2's bag drop, the call it was
  meant to see, left it at 0 too (below).

## Live procedure 2

**LIVE-ABORTED.** Session 2026-10-04 16:25-16:38 UTC, research build sha256
`fcea595b…2b15` (built at ForgePact `f44cc6a`, matched the lease's
`dll_sha256`), slot 14 Sorak, Town of Inoya. It ran the mod through the
panel's switch (served headless from this branch, switched by its own
`/api/set`) with the bag-drop window build, and was meant to end with the
`method` retest. The game exited during step 7, so steps 7 and 8 did not
run. The saves were restored afterwards by the driver from the session's
backup (`hs_saves_inspect` after the restore: 0 changed, added or missing).

| Check | Supplied | Seen | Result |
|---|---|---|---|
| `dll-hash` | the lease's `dll_sha256` against the build's | equal | pass |
| `marker` | `lootann stat` before the switch | `lootann: off route=server seen=0 ... drop-hook=not-installed bag-drop-calls=0 ...` | pass |
| `control` | `ping` | `pong (YYTK 4.0.1)` | pass |
| (switch on) | panel `mod_loot_announce` true | `HOOK INSTALLED` on `LootGroundInit` and `LootGroundDrop`; `lootann: on route=server init-hook=both drop-hook=both`; modstate `lootAnnounce.on` true | recorded |
| `on-announce-heroic` | `lootannprobe place heroic` (`"27"`=9, Heavy Belt of Spellshield, itemType 8, ground id 262176) | red `SERVER: Sorak found Heavy Belt of Spellshield`; `announced=1` | pass |
| `on-announce-angelic` | `lootannprobe place angelic` (`"27"`=7, Headhunter) | red `SERVER: Sorak found Headhunter`; `announced=2` | pass |
| `on-no-announce-satanic` | `lootannprobe place satanic` (`"27"`=6, Cobra Heavy Belt) | no new line; `held-rarity` 0 → 1 | pass |
| `bag-drop-silent` | the owner picked up the placed Heroic belt and dropped it from the bag | `seen` 3 → 4, `announced` 2 → 3, `held-bag-drop` stayed 0, `bag-drop-calls=0` with `drop-hook=both`; the screenshot was taken about six minutes later and the owner was not asked about a line | fail |
| `no-duplicate` | the same steps 2-5 | `announced=3` after the bag drop; the on-screen line count was not captured | fail |
| `off-no-announce` | panel switch off, `lootannprobe place heroic` (Earthworm's Heavy Belt, `"27"`=9) | `lootann: off`; no line; counts unchanged | pass |
| `natural-announce` | panel switch on; Magic Find x100 and `angelic_items` x100 (one extra Angelic or Unholy die per kill, about 1 in 76 kills) at the owner's request | the game exited before any natural drop was read | not observed |
| `anon-control`, `method-found-2`, `route-method-2`, `route-method-angelic-2` | step 8 | not run | not observed |

**The bag drop, measured.** The owner's bag drop reached the shared
`LootGroundInit` detour (`seen` +1) and the item on the ground read an
announced rarity, while the both-route `LootGroundDrop` detour counted 0. So
the mod's assumption that `LootGroundDrop` is the bag drop was false, or at
least not the path this bag drop took; which script the bag drop runs
through is not established. The research build's own `CreateItemNew` log
(`bp_ipc\itemdrops.jsonl`, kept outside any repository) showed one more
build of the same belt between the placements: same definition and
`itemDataHash`, `itemTimeStamp` 0 and `"27"` 1. The item on the ground still
read 9, so it was not that rebuild (a temporary copy made at the pickup or
the drop; the log has no time to say which). Inference: the struct that went
pickup → bag → ground was the placed one, or a copy `CreateItemNew` did not
make. The same log shows the game's own drops built with a fresh numeric
`itemTimeStamp` through the detoured `CreateItemNew`. This session stayed in
town and read no natural drop, so those records are an earlier launch's: the
values read decode to the end of Live procedure 1's kills
(`docs/RUNTIME_DATA_MODELS.md` § 16.11), and that the log is appended to
across launches is inference from that.

**Why the guard changed.** A window opened by a hook that never sees the bag
drop cannot hold it. "Built by `CreateItemNew` this frame or the last" was
chosen to separate a new drop from an existing struct put back on the ground
without knowing the bag drop's script, so the mod now asks that (The mod,
above). It is a design inference, not a measurement: a bag drop would pass
it if the ground received a copy built through `CreateItemNew` in that frame
or the one before, or if a rebuild at the drop were handed the existing
struct as argument 0 (the adapter notes argument 0 as well as the return).
Live procedure 3 measured it (below): `create-hook`, the bag drop again with
`created` noted around the pickup and the drop, and `natural-fresh`, the
guard's positive control on the game's own drops. The owner accepted on
2026-10-04 that a bag drop announcing again is acceptable if research does
not show a way: `bag-drop-silent` and `no-duplicate` were recorded there, not
required, and a fail would have shipped as a Known Limitation.

**The game's exit, as recorded.** About 44 s after `out.txt`'s last line
(the boost commands' own output, nothing after it), the game exited with
code `0x00000001`, not a clean shutdown (`exit.json`). The incident monitor
found no Windows error-reporting record, no faulting module and no exception
code; no `_crash` report was written; `YYToolkit.log` had nothing at that
time; the Application event log had no Hero Siege record. The cause is not
established. The owner reported afterwards that Steam was not running; the
explanation, "steam was not running, it could be the cause", is a
hypothesis, not a finding (the session did not record Steam's state); Live
procedure 3 checked that Steam was running before launch (`steam-running`),
and that session's game did not exit. One session does not establish the
cause.

## Live procedure 3

The creation guard through the panel, the steps Live procedure 2 never
reached, then the `method` retest with the fixed probe. Run by
`live-operator`; the full procedure is in the workorder.

### Results

Session 2026-10-04 17:43-17:53 UTC, research build sha256 `d1538a0f…1c62`
(built at ForgePact `765026d`, matched the lease's `dll_sha256`), slot 14
Sorak, the Town of Inoya for the placements and the bag drop, then the
Outskirts of Inoya (zone level 243) for the kills and the retest. Steam was
running before launch (`steam.exe` listed). The mod was switched through the
panel (served headless from this branch, switched by its own `/api/set`).
Boosts: none in step 7(a); Magic Find x10 for step 7(b), with the Angelic /
Unholy Drops multiplier left at its saved 1, so any Angelic seen was the
game's own; both restored to the saved values afterwards. The game did not
exit; it was stopped at the end. The saves were restored afterwards from the
session's backup (`hs_saves_inspect` after the restore: 0 changed, added or
missing).

| Check | Supplied | Seen | Result |
|---|---|---|---|
| `dll-hash` | the lease's `dll_sha256` against the build's | equal | pass |
| `marker` | `lootann stat` before the switch | `lootann: off route=server seen=0 ... created=0 create-overflow=0 init-hook=not-installed create-hook=not-installed ...` | pass |
| `control` | `ping` | `pong (YYTK 4.0.1)` | pass |
| `steam-running` | `tasklist` before launch | `steam.exe` listed | pass |
| `create-hook` | panel `mod_loot_announce` true | `HOOK INSTALLED on LootGroundInit`; `lootann: on route=server init-hook=both create-hook=both`; modstate `lootAnnounce.on` true | pass |
| `on-announce-heroic` | `lootannprobe place heroic` (`"27"`=9, Swift Heavy Belt, itemType 8, ground id 262136) | red `[19:44] SERVER: Sorak found Swift Heavy Belt`; `seen=1 announced=1 held-bag-drop=0 created=1` | pass |
| `on-announce-angelic` | `lootannprobe place angelic` (`"27"`=7, Headhunter) | red `SERVER: Sorak found Headhunter`; `announced=2 created=2` | pass |
| `on-no-announce-satanic` | `lootannprobe place satanic` (`"27"`=6, Wanderer Heavy Belt) | no new line; `held-rarity` 0 → 1, `created=3` | pass |
| `bag-drop-silent` | the owner picked up the placed Heroic belt (a), then dropped it from the bag (b), `lootann stat` after each | after (a) nothing moved (`seen=3`, `created=3`); after (b) `seen` 3 → 4, `held-bag-drop` 0 → 1, `announced` stayed 2, `created` 3 → 5; no line in the screenshot (taken when the owner's reply arrived, possibly more than 5 s after the drop) and the owner saw none | pass |
| `no-duplicate` | the same steps 2-5 | `announced=2` after (b); the two lines of steps 2 and 3, none at the bag drop (the owner gave no total count) | pass |
| `off-no-announce` | panel switch off, `lootannprobe place heroic` (Heavy Belt of Swiftcast, `"27"`=9) | `lootann: off`; no line; counts unchanged (`seen=4 announced=2 created=5`) | pass |
| `natural-fresh` | panel switch on; the owner killed monsters in one zone for about five minutes (minimap clock), no boost, no bag drop, no portal | `seen` 4 → 219 (+215), `created` 5 → 220 (+215), `held-bag-drop` 1 → 1, `held-rarity` 1 → 216, `announced` unchanged | pass |
| `natural-announce` | Magic Find x10 through the panel, Angelic / Unholy Drops at its saved 1; the owner played on in the same zone | `announced` 2 → 3 with no placement (`seen` 219 → 446, `held-rarity` 216 → 442, `held-no-rarity`, `held-duplicate` 0, `held-bag-drop` 1); the owner saw a red `SERVER:` line; the screenshot came after the line had gone, so the item and its rarity were not recorded | pass |
| `anon-control` | `lootannprobe on` (16 of 16 rows attached, none table-only or missing; `LootGroundDrop` through its own count-only detour), `lootannprobe place heroic` (Heavy Belt of Wholeness), `lootannprobe methods` | `m_AngelicMessage -> ?#-1, m_LootFilter -> ?#-1, m_LootGroundDeActiveStep -> ?#-1, s_lootDrawData -> s_lootDrawData@gml_Object_Pickup_Parent_obj_Create_0#105134`; `anon rows resolved: 0 of 2`; `unresolved rows: 3 of 4`; `SDK closure ... not found` | fail |
| `method-found-2` | the same listing | no row names `anon@1138` | not observed (instrument-blind: the three `anon@` rows read `?#-1`) |
| `route-method-2` | `lootannprobe try 1` on the placed Heroic, switch off | `supplied script=anon@1138@gml_Object_Loot_Ground_obj_Create_0 self=Loot_Ground_obj args=none -> dispatched=0 ret=none refused: method`; `deltas: none`; no line; no game error | not observed (method not found) |
| `route-method-angelic-2` | `lootannprobe place angelic` (Headhunter), `lootannprobe try 1` | the same refusal, `deltas: none`, no line | not observed (method not found) |

What the session established, measured:

- **The creation guard holds a bag drop.** The owner's bag drop reached
  `LootGroundInit` (`seen` +1) and was held as not recently created
  (`held-bag-drop` +1), with no line. One bag drop of one item was tried;
  dropping several items at once, or dropping in the frame of a pickup, was
  not.
- **The guard passes the game's own drops** (`natural-fresh`, its positive
  control): over 215 natural drops `created` rose by exactly as much as
  `seen`, and none was held as a bag drop. Each placement raised `created`
  by 1 too.
- **The drop from the bag builds through `CreateItemNew`; the pickup does
  not.** `created` stayed at 3 across the pickup and rose by 2 at the drop,
  and the ground item still read as not recently created, so neither key
  noted at the drop was the struct that reached the ground. That this is
  the same rebuild Live procedure 2's `itemdrops.jsonl` showed (same belt,
  `itemTimeStamp` 0) is inference: this session did not read that log.
- **A natural drop of an announced rarity reaches `LootGroundInit` and is
  announced**: `announced` rose by 1 with no placement, and the owner saw
  the red line. Which item it was, and so whether it was Heroic, Angelic or
  Unholy, was not recorded.
- **The fixed probe still names no `anon@` method.** It now takes
  `method_get_index`'s value as a number: for `m_AngelicMessage`,
  `m_LootFilter` and `m_LootGroundDeActiveStep` the conversion returned
  without throwing and gave -1, so `script_get_name` was not asked, while
  the named `s_lootDrawData` gave 105134 and resolved. So the open question
  of the previous round, whether taking a `VALUE_REF` index as a number is
  safe, is answered for safety (no throw, no `(unreadable)` row) but not for
  meaning: the probe does not print the index's kind, so whether -1 is
  `method_get_index`'s own answer for these methods or the conversion's
  reading of a reference is not established. `CiTryResolveMethod` named
  `m_LootGroundDeActiveStep` as an `anon@` closure on a quest item on
  2026-09-11; why the same read gives -1 on this ground item is not
  established. The listing stays instrument-blind, so whether the ground
  item holds `anon@1138` is still not known, and `method` was not called.
- `route-method-2` did not pass, so by the owner's rule `server` stays the
  shipped route (Route, below).

## Route

announce-route: server

Chosen 2026-10-04 from Live procedure 1, in the order `method`, `netsend`,
`chatadd`, `server`: the first three showed no line offline, and `server`
showed the red `SERVER: <character> found <item name>` line with no error and
no `PacketSend`. The owner approved shipping it (2026-10-04), on the recorded
reason that the ground item carries no announcement method. That reason was a
probe artifact (Live procedure 1's Results, above): `method` was refused
because the probe could not name any `anon@` method, so it is
uninterpretable, not failed. `netsend` and `chatadd` refused with the one
item argument supplied, not a call by name as such. After the round-2 review
the owner chose to fix the probe, retest `method`, and then choose: `method`
only if `route-method-2` passed with no `PacketSend` count and no game error
line, else `server`.

Live procedure 3 ran the retest (Live procedure 2 ended before it).
`route-method-2` and `route-method-angelic-2` were not observed: the fixed
probe still resolved no `anon@` method (`anon rows resolved: 0 of 2`), and
`try 1` refused before calling anything. So `server` stays shipped, by the
owner's rule, with nothing left to ask. The `VALUE_REF` finding that goes
with it: taking `method_get_index`'s value as a number did not throw, but
gave -1 for every `anon@` method (Live procedure 3's Results). A plain
`ChatAddMessage` line without the `SERVER:` prefix is a possible follow-up,
not part of this change. The header's `kShippedSink` and
`tests/test_loot_announce_contract.py`'s `EXPECTED_ROUTE` name the same
route.

2026-10-23 (#228): the shipped route is now `chatcolour`, the plain
`ChatAddMessage` line the paragraph above names as a follow-up, with the item
name drawn in the online announcement's colour for its rarity and the rest of
the line in the game's grey chat text (§ Rarity colours (#228)). `server`
stays in both builds as its fallback: when the `chatcolour` call refuses, the
mod logs why and shows the red line instead. `kShippedSink` and
`EXPECTED_ROUTE` now name `chatcolour`.

announce-route: chatcolour

## The switch on at launch

2026-10-08. The owner reported that "loot announcement doesnt work", with no
more detail. The failing session was not captured: `bp_ipc\out.txt` and
`out.prev.txt` on the development machine held no `lootann` lines from
ordinary play (only this research's sessions of 2026-10-04), the installed
plugin was a build from another, unmerged branch, and the owner's
`forgepact.json` by then had the switch off. So **the cause of the owner's
failure is not established**, and nothing below claims to be it.

What was a candidate: every live session above switched the mod on **in
game**, through the panel, with a character loaded. The way a player
normally uses it, with the switch already on as the game starts, was never
measured. Then the panel's launch commands carry `lootann 1`, the plugin
reads commands from the first frame after its setup (while character
selection is still running), and until this change `lootann 1` installed
both hooks at once: `LootGroundInit` and `CreateItemNew` hooked at character
select. That is the hazard the ForgePact guide's Known Limitations item 8
names ("Mods that install a hook must be armed, not hooked, at launch":
hooking `DropRelic` at character select stalled the runner, and the relic
filter waits for `HhResolveLocalPlayer`). The install was tried once and
never again, and a route other than `both` was said only in `out.txt`, so a
failed install would have been silence in play. Hidden loot sleep, which
shares the `LootGroundInit` detour, has an 18-frame pass over ground items
that covers a table-only hook; loot announcements has none, and cannot, since
a pass cannot tell a fresh drop from a bag drop without the creation guard.

What changed: `lootann 1` with no local player turns the switch on and only
arms it. The hooks go in once per session, on the first frame where setup is
done and `HhResolveLocalPlayer` resolves the player, looked for at most once
every 60 frames while armed (`LootAnnounceMod::ShouldInstall`,
`LooksForPlayer`, `kInstallPollFrames`, harness-tested); with a character
loaded `lootann 1` still installs at once. `lootann 1` and `lootann stat`
gain ` install=` after `create-hook=`: `not-armed` (off, never installed),
`waiting-for-character` (on, no player yet) or `installed` (the install ran).
Rejected: retrying a failed install every few frames (it hides a real
refusal, which the route line already reports); a fallback pass like hidden
loot's (above). Tests: `tests/loot_announce_harness.cpp`'s
`baseline/setup_alone_installed_before_this_change`,
`target/waits_for_a_character_before_installing`,
`target/a_switch_on_in_game_installs_at_once` and the negative control
`target/switched_off_never_installs`, through
`tests/test_loot_announce_behavior.py`; the adapter's wiring in
`tests/test_loot_announce_contract.py`.

That it works after the change is measured (Live procedure 4's Results,
2026-10-08: `lootann-armed-at-select`, `lootann-installed-after-load`,
`lootann-heroic-announced` all pass). Whether this path failed before the
change is not established: only the build with the change was run.

## Satanic and Mythic: added and reverted

2026-10-08. Asked which drop they had expected to see announced in the
failing session, the owner answered "Expected Satanic or Mythic", and to
whether this change should add them, "Add Satanic and Mythic". So the
announced set is now five rarities of `itemInfoStruct["27"]`: Heroic 9,
Angelic 7, Unholy 10, Satanic 6 and Mythic 5
(`LootAnnounceMod::kAnnouncedRarities`, with the named constants `kSatanic`
and `kMythic`; codes from `docs/RUNTIME_DATA_MODELS.md`'s rarity table in
the toolkit hub). Common, Superior, Rare and every other code stay held
(`held-rarity`). The game's own online announcement closure also handles
Satanic (§ Static reading: Satanic, Angelic, Heroic and Unholy); Mythic is
the owner's choice, not something the game's online rule was read to do.
Nothing else changes: the sink, the line text, the creation guard and the
once-per-item memory are as before. Tests: the harness's
`baseline/satanic_and_mythic_the_rest_stay_held`,
`baseline/satanic_and_mythic_off_announces_nothing` and
`target/satanic_and_mythic_announced_once` (a fresh Satanic and a fresh
Mythic item each announced once, a second sighting `held-duplicate`), and
the contract test's pin of the five-entry set. `lootannprobe place` gains
`mythic` (code 5) so a session can place one. Live procedures 1 and 3's
results above, where a placed Satanic item was held, stay as recorded: they
measured the set as it was then. Live procedure 4 checks a placed Satanic
and a placed Mythic item (`lootann-satanic-announced`,
`lootann-mythic-announced`).

Reverted the same day, 2026-10-08. During Live procedure 4, back in town
after the placements, the owner reversed the decision: "mythics and satanics
shouldnt show. my mistake". The announced set is three rarities again,
Heroic 9, Angelic 7 and Unholy 10, as it shipped before this change:
`kAnnouncedRarities` is `{ kHeroic, kAngelic, kUnholy }`, `kSatanic` and
`kMythic` are gone from the header, and a Satanic or Mythic item is
`held-rarity` again. The harness's `target/satanic_not_announced` and
`target/mythic_not_announced` are back, the three scenarios above are
removed, and the contract test pins the three-entry set (the tests whose
names carry `satanic_and_mythic_held`). Live procedure 4's
`lootann-satanic-announced` and `lootann-mythic-announced` measured the
five-rarity build before this revert, so they are research results of that
build and say nothing about the shipped set. `lootannprobe place mythic`
stays in the research build, so a later session can place a Mythic item and
see it held. The paragraph above stays as the record of the widening, so it
is not proposed again without the owner.

## Live procedure 4

One session for this change and two other ForgePact fixes of 2026-10-08
(one research build, one launch). Its `lootann-satanic-announced` and
`lootann-mythic-announced` checks are research results of that build, which
announced Satanic and Mythic; the shipped set holds both (§ Satanic and
Mythic: added and reverted). The procedure is the toolkit hub's
workorder `forgepact-moveall-loot-satanic`, its context file's
`### Live procedure 1` (`.claude/workorders/forgepact-moveall-loot-satanic-context.md`
in the hub, a local working file). The loot announcement part: at character
select, `lootann stat` carries ` install=not-armed` (the build's marker),
and `hiddenloot stat` records whether hidden loot sleep is on (`on=`, the
owner's own `forgepact.json` decides it); `lootann 1` answers
`install=waiting-for-character create-hook=not-installed`, and still does 3 s
later (`lootann-armed-at-select`, which rests on those two fields: only loot
announcements' own install sets them). `init-hook=` is not loot
announcements' own: it reports the `LootGroundInit` detour shared with hidden
loot sleep, which hidden loot installs at setup with no player check whenever
its switch is on (the ForgePact guide's Known Limitations item 8). So
`init-hook=not-installed` is expected only with hidden loot off; with it on,
`init-hook=` shows hidden loot's route (the same value as `hiddenloot stat`'s
`route=`), which is that limitation and not a failed arm; after
the character loads, `install=installed init-hook=both create-hook=both`
(`lootann-installed-after-load`); a placed Heroic, Satanic and Mythic item
each raise `announced` by 1 with a red `SERVER:` line and no `held-rarity`
change (`lootann-heroic-announced`, `lootann-satanic-announced`,
`lootann-mythic-announced`); optionally a minute of kills raises `seen` and
`created` (`natural-drops-seen`, research). The session ran on 2026-10-08:
§ Results, below.

### Results

Session 2026-10-08 09:35-09:46 UTC, research build sha256
`87d2c4d5f0322ea35a6e0e9c80c03380603051c3282b5aab4496dd649cb31a04` (the
lease's `dll_sha256`, equal to the built file's, read twice), slot 14 Sorak,
the Town of Inoya, and the Outskirts for the kills. Capture
`.claude/workorders/forgepact-moveall-loot-satanic-live-1.md` in the toolkit
hub, on the owner's machine. The mod was switched by `lootann 1` sent through
the command channel at character select, the moment the panel's launch
commands reach the plugin; the panel itself was not used. The saves were
backed up first and restored afterwards (`hs_saves_inspect` after the
restore: 0 changed, added or missing). This build still announced Satanic
and Mythic: the owner reverted that during this session (§ Satanic and
Mythic: added and reverted), so `lootann-satanic-announced` and
`lootann-mythic-announced` below are **research results of that build, not
the shipped behaviour**, which holds both.

| Check | Supplied | Seen | Result |
|---|---|---|---|
| `control` | `ping` at character select | `pong (YYTK 4.0.1)` | pass |
| `marker` | `lootann stat` at character select, before the switch | `lootann: off route=server seen=0 ... init-hook=not-installed create-hook=not-installed install=not-armed ...` | pass |
| `dll-hash` | the lease's `dll_sha256` against the build's | equal | pass |
| `lootann-armed-at-select` | `hiddenloot stat` (`on=0 route=none`), then `lootann 1` at character select, and `lootann stat` 3 s later | `lootann: on route=server init-hook=not-installed create-hook=not-installed install=waiting-for-character`, and the same three fields 3 s later (hidden loot off, so `init-hook=not-installed` was the expected case) | pass |
| `lootann-installed-after-load` | `hs_select_character` slot 14, `lootann stat` 5 s later | `init-hook=both create-hook=both install=installed`, `created=2268` | pass |
| `lootann-heroic-announced` | `lootannprobe place heroic` (`"27"`=9, Heavy Belt of Energy, ground id 261987); a second placement after step 7 (`"27"`=9, Heavy Belt of Vial) only to catch the line on a screenshot | `announced` 0 → 1; the first screenshot, about 3 s after the placement, showed no line (the red lines fade within a few seconds); the second placement, `announced` 3 → 4, showed red `[11:37] SERVER: Sorak found Heavy Belt of Vial` on a screenshot taken at once | pass |
| `lootann-satanic-announced` (research, pre-revert build) | `lootannprobe place satanic` (`"27"`=6, Heavy Belt of Vitality) | red `SERVER: Sorak found Heavy Belt of Vitality`; `announced` 1 → 2, `held-rarity` 0 → 0 | pass |
| `lootann-mythic-announced` (research, pre-revert build) | `lootannprobe place mythic` (`"27"`=5, Owl Heavy Belt) | red `SERVER: Sorak found Owl Heavy Belt`; `announced` 2 → 3, `held-rarity` 0 → 0 | pass |
| `natural-drops-seen` (research) | the owner killed monsters in the Outskirts for about a minute | `seen` 4 → 280, `created` 2272 → 2548; `announced` 4 → 20 and `held-rarity` 0 → 260 on the pre-revert set, so how many of the 16 were Heroic, Angelic or Unholy is not known | pass |

What the session established, measured:

- **The switch on before a character exists arms and waits.** `lootann 1`
  at character select installed nothing (`create-hook=not-installed`,
  `install=waiting-for-character`) and still had not 3 s later.
- **The hooks go in once the character is loaded.** Within 5 s of slot 14
  loading, both `LootGroundInit` and `CreateItemNew` read `both` and
  `install=installed`; `created` already read 2268, so the game's own item
  builds were being noted.
- **A placed Heroic item is announced in that path**: `announced` rose by 1
  for each placement, with the red line on the screenshot taken at once.
- **The probe's Mythic code reads back as written.** `place mythic` reported
  `"27"`=5 on the placed belt, and the pre-revert build counted it as an
  announced rarity; whether the game's own interface draws it as Mythic was
  not looked at.

Not established by this session: whether the same path failed before the
arming change (only the build with it ran), and the switch on through the
panel's own launch commands rather than `lootann 1` sent at the same moment.
The game's close by `hs_stop_game` without force ended with exit code
`0xC0000409`, after play had ended; it was not investigated.

## Chaos Tower (#227)

### The report

ForgePact #227 (a player report, no capture): with Loot announcements on, a
Heroic, Angelic or Unholy item found in the Chaos Tower gets no chat line,
while the same rarities announce everywhere else. The issue asked whether the
announcement path sees tower drops at all, to be answered with a positive
control outside the tower in the same session.

The answer below is a static reading, made 2026-10-23 on the build the
decompile index labels `498d588550d8`: the path does see the drops, and the
creation guard (§ The mod) rejects them. Live procedure 5 (2026-10-23)
measured its main points on a first floor: items dropped there go into the
floor's list instead of being decided, they land when the floor is cleared,
the rule before this fix holds them as `held-bag-drop` there, and the burst
memory announces them. Each point below says whether it was measured; what
the session did not reach stays in § Not established.

### How it was found

- **The `ghidra` MCP's read tools** answered what each script does:
  `LootGroundInit`, `LootExplosion`, `LootGroundDrop`, `IsChaosTower`,
  `EnemyDestroyChaosTower`, `WormholeGiveReward`, `Controller_obj`'s tower
  reset closure and `Spawn_Next_obj`'s floor-end closure. On this
  `-noanalysis` import the MCP's callers and xrefs come back empty, which is
  "not observed", never "has none" (AGENTS.md § Check for a Named Ghidra
  Project).
- **Direct callers by a byte scan** of the executable's code section for
  `E8`/`E9 rel32` sites aimed at a script's entry. Two positive controls ran
  in the same scan: `DropItemHeroic` gave its two known callers (`VaultUse`
  and the Chaos Tower reward function, the same pair `FindCallers.java` gave
  on 2026-10-20) and `LoadDrops` its one (`DropItem`). The scan cannot see a
  call through a method value, a global pointer or the script table, so a
  caller reached only that way is not observed.
- **Variable and builtin names** in the bodies the decompiler left unnamed
  came from `py -3 tools/decomp_index.py slot-name`, not from guessing.

No decompiled text, address or offset from any of these reads is kept here;
what follows is the mechanism in our own words.

### The loot burst (static reading)

- **A tower floor keeps its drops.** `LootGroundInit`, which every ground
  item passes through, has a branch for the Chaos Tower. It applies when the
  room's name starts with the tower's prefix (the same test `IsChaosTower`
  makes) or a protected value at `gDataProtected[12]` reads true, the room is
  neither `Chaos_Tower_Boss_1` (137) nor `Chaos_Tower_xLobby` (148), and the
  protected flag behind `Controller_obj.lootExplosionGiven` is false. Then the
  new ground item does not stay: the game wraps the ground item's
  `itemInstance` (the item struct itself, not a copy) and its `isPlayerDrop`
  in an `s_LootExplosionData` struct, appends that to
  `Controller_obj.lootExplosionList` (a plain ds_list, read directly), and
  the call that follows is read as destroying the ground instance (an
  inference: the same runtime routine destroys a dialog's instance
  elsewhere). One item-kind test, not read further, can exempt an item.
  Nothing tests `isPlayerDrop` before the append, so a player's own bag drop
  on a floor is collected too.
  **Measured** (Live procedure 5) on `Chaos_Tower_Floor_01_rm`:
  `IsChaosTower` read true, `lootExplosionList` read as a live list (a
  reference), and each Heroic item placed there raised the list's size by 1
  and the mod's `unidentified` by 1 with no decision, which fits the ground
  instance being gone by the frame's end; the floor's own drops from kills
  raised it the same way. That the call after the append is what destroys
  the instance is still the inference above, and the bag drop on a floor
  was not tried (not observed live).
- **The protected keys.** `lootExplosionGiven` and `lootExplosionGold` each
  hold a protected-store key, not the value (the `GPV` pattern,
  `docs/RUNTIME_DATA_MODELS.md` § 13.7 in the hub).
- **The burst.** `LootExplosion` sets `lootExplosionGiven` true, drops the
  held gold, then for every list entry puts the entry's struct back on the
  ground around a given point with `LootGroundDrop` (a new `Loot_Ground_obj`
  whose `itemInstance` is that struct, so no `CreateItemNew` runs for the
  item) and calls `LootGroundInit` on the new ground item with a second
  argument `true`, and finally clears the list. `LootGroundDrop`'s own body
  also calls `LootGroundInit`; whether both calls run for one item is not
  established. **Measured** (Live procedure 5): when the floor was cleared
  the list emptied (`size=0`), `lootExplosionGiven` read true, the held
  items lay on the ground, and the mod decided each placed item twice in
  that burst, which fits two `LootGroundInit` sightings per item; which two
  calls they were was not separated, because the debugger logpoints on
  `LootExplosion` and `LootGroundInit` could not run through the floor.
- **When it comes.** `LootExplosion` has two direct callers by the scan:
  `Spawn_Next_obj`'s closure `anon@1312@gml_Object_Spawn_Next_obj_Create_0`,
  which reads the floor's state (`chaosTowerFloorFinished`,
  `chaosTowerIsLastFloor`, `chaosTowerFloor`, `chaosTowerFloorList`,
  `nextFloorType`, `nextFloorNumber`), and `WormholeGiveReward`. So the
  floor's loot lands when the floor is finished; whether a Wormhole reward
  uses the same list is not established. **Measured** (Live procedure 5):
  the loot landed when the floor was cleared ("Floor Cleared" on screen);
  that `LootExplosion` was the call that put it there is the static reading,
  not observed live.
- **The reset.** `Controller_obj`'s closure `anon@16838` clears
  `lootExplosionList` and resets `lootExplosionGold`, `chaosTowerFloor`,
  `chaosTowerSpawnZone`, `chaosTowerStarted`, `chaosTowerBossState`,
  `lootExplosionGiven` (to false) and `chaosTowerRift`. That it is what
  `m_ChaosTowerReset` holds is plausible, not established (the S10 notes
  named an earlier build's `anon@16294`).
- **Not the cause.** `EnemyDestroyChaosTower` only applies the tower's death
  effects and makes no item call. The tower's reward function (unnamed in the
  import, beside `Chaos_Tower_NPC_obj`'s code) gives its reward through
  `DropItemHeroic` and a drop table, in a room not established.

### What the mod did with it

Measured in Live procedure 5 with the burst memory switched off
(`lootannprobe burst off`, which reproduces the rule before this fix): a
Heroic item placed on the floor got no decide line and raised
`unidentified`, and at the floor's end it was decided `held-bag-drop`
(`via=none`); the owner, watching the game, saw no line for it. The account
of why is the reading and the guard: on a tower floor the item is built through `CreateItemNew` at the
kill, and the noted `LootGroundInit` call's ground instance is gone by the
frame's end, so the mod finds no live `Loot_Ground_obj` among the call's
handles: `unidentified` +1, nothing decided. At the floor's end the same
struct lands again through `LootExplosion`, minutes after it was built and
with no `CreateItemNew` in that frame or the one before, so the creation
guard holds it as `held-bag-drop`. No line either time. The announcement path
sees the drops; the guard rejects them, by its own rule, because the burst
looks exactly like a bag drop.

### The burst memory

The fix teaches the guard about the list, not about the room. The core
(`plugin/include/ForgePact/LootAnnounceMod.hpp`, which knows only keys,
counts and text) keeps a burst memory beside its creation window:

1. **Noting an entry.** Noting a list entry's item key counts `burst-listed`.
   If that key is recently created at that moment (noted by `CreateItemNew`
   this frame or the last), it is kept and counts `burst-fresh`; otherwise it
   is not kept. While the switch is off, noting does nothing. The memory
   holds at most 4096 keys; a note past that is not kept and counts
   `burst-overflow`.
2. **Forgetting.** When the adapter reports that the list shrank or changed,
   the memory forgets every key, after that frame's decisions.
3. **Deciding.** The creation guard passes a ground item when its key is
   recently created or kept in the burst memory. A pass that came only from
   the burst memory counts `burst-landed`. A sighting does not consume the
   key: the burst may initialise one ground item twice in a frame, and the
   identity memory keeps that to one line (the second sighting
   `held-duplicate`). The rarity set and the identity rule are unchanged.
4. **Off.** Switching off forgets the burst memory, as it clears the
   creation window; the counters persist like the others.
5. **The stat line** gains, right after `create-overflow=<n>`,
   ` burst-listed=<n> burst-fresh=<n> burst-landed=<n> burst-overflow=<n>`.

A bag drop on a floor still stays unannounced: its struct was built long
before it was listed, so it is never kept (`burst-fresh` 0) and lands as
`held-bag-drop`. That is the intended rule, pinned by the harness; a bag
drop on a tower floor is not observed live (Live procedure 5 did not reach
it). The adapter in `plugin/ModuleMain.cpp` feeds the memory:

- While the switch is on and the hooks are installed, `LootAnnounceTick`
  reads the list after deciding the frame's noted calls and before ageing the
  creation window, so the burst frame's landings find the memory filled in
  earlier frames, and an entry collected this frame still finds this frame's
  `CreateItemNew` notes.
- Everything by name: the Controller through `FindControllerInstance`,
  `lootExplosionList` through `variable_instance_get`, `ds_exists` with the
  list type, `ds_list_size`, and only the entries past those already read
  through `ds_list_find_value`; each entry's `itemInstance` through the
  adapter's field reader and its key the same way as at a `CreateItemNew`
  note. A list id or Controller handle may be a number or a reference: the
  kind decides how it is read, never whether.
- A list shorter than the count already read, a different list value, or no
  Controller or list makes the core forget, and the count restarts at the
  list's current length. The first read after the switch comes on starts at
  the list's length at that moment.
- No new hook, no address, no struct layout and no write to a game value.
  While off it costs nothing (the tick already returns at once); while on,
  one Controller lookup and one list length per frame, with entries read only
  when the list grew. A read that throws skips that frame's read and says so
  once in `out.txt` (`lootann: burst list unreadable - <what>`).
- The burst reader ships in the player build, since it is the fix. Only the
  instrument below is research-build only.

`tests/loot_announce_harness.cpp` pins the rules against the real header:
the baseline cases (a struct landing long after it was built, with no burst
note, is held; an entry noted while off is not kept) and the target cases
(a fresh entry lands as new with `burst-landed` 1, a stale one stays held, two
sightings in the burst frame give one line, a shrink forgets, the rarity and
identity rules still apply, the cap, off forgets, and the stat fields in
order).

### Instrument additions

Under `#ifndef FORGEPACT_RELEASE`, in `lootannprobe`, for the #227 live
session:

- **`lootannprobe burst`** prints `lootannprobe burst: memory=<on|off>
  tower=<1|0|unread> list=<value|none(<why>)> size=<n|unread>
  given=<value|unread>`, with ` threw=<stage>` appended when a read threw
  (`list`, `ds_list_size` or `given`, the read that was running). `list` is
  `real:<id>`, or `ref:<id>` for a handle that came back a reference (Live
  procedure 5 read `ref:291`); `none(<why>)` says why there is no list:
  `no Controller_obj` when the instrument found nothing to read, or the
  value's kind and the step that refused it when the variable holds no live
  ds_list. `tower` is `gml_Script_IsChaosTower` by name (the
  existing `InChaosTowerCached`); `given` is `gml_Script_GPV` of
  `Controller_obj.lootExplosionGiven`, the route `gatestats` uses.
- **`lootannprobe burst on|off`** sets whether newly listed entries are kept
  (`on` at startup) and answers with the same line. While `off`, entries are
  still read and count `burst-listed` but are not kept, which reproduces the
  rule shipped before #227 for the items listed meanwhile: the negative
  control inside the tower.
- **A decide line** for every decided ground item whose rarity is one the
  mod announces: `lootann decide: "<itemInfoStruct 28>" rarity=<code>
  stamp=<itemTimeStamp> verdict=<VerdictName> via=<recent|burst|none>`,
  where `via` names what passed the guard, `none` when nothing did.
- **`lootannprobe place`** sets `itemInfoStruct["32"]` (the loot filter's
  tier, `docs/RUNTIME_DATA_MODELS.md` § 16.3 in the hub) to 5, SS, before it
  places the item, since the owner's filter hides lower tiers, and its reply
  adds ` "32"=<n> stamp=<itemTimeStamp>`. Measured (Live procedure 5): items
  placed this way showed their labels on the ground under the owner's
  filter, in town and after the tower floor's burst.
- The usage line names `burst [on|off]`.
- **Two stat fields, in the player build** (not behind `#ifndef
  FORGEPACT_RELEASE`, added after Live procedure 5's review): `lootann stat`
  ends with ` burst-list=<state> burst-nokey=<n>`, after `queue-full=`.
  `burst-list` is the last list lookup's answer: `ok`, or `none(<why>)`
  carrying the reason the lookup gave (`no Controller_obj`, or the value's
  kind and the step that refused it), `none(lookup threw)` when the lookup
  threw, and `unread` before the first read. `burst-nokey` counts the list
  entries read whose `itemInstance` gave no key. With `burst-listed=0` alone,
  a renamed `lootExplosionList`, a renamed `itemInstance` and a session that
  never finished a floor read the same; a renamed list shows as a
  `none(...)` state while in the tower, and a renamed entry field as
  `burst-nokey` climbing while `burst-listed` stays 0.

### Rejected routes

1. **Treat every item landing in a `Chaos_Tower_` room as new.** The tower
   collects bag drops too, so a bag drop on a floor would be announced; it
   also misses the `gDataProtected[12]` trigger.
2. **Hook `LootExplosion` or `LootGroundDrop` and open a window.** A new
   hook, reached by a direct call from a closure, and every listed item lands
   inside the window, bag drops included. § The mod records the
   `LootGroundDrop` window the creation guard replaced, which counted 0.
3. **Trust `isPlayerDrop`.** What it holds on a bag drop is not established;
   the creation keys are measured (Live procedure 3).
4. **Widen the creation window.** A floor lasts minutes, and a window that
   long admits an item picked up and dropped again inside it.
5. **Read the item inside the `LootGroundInit` detour before the original.**
   The detour's consumers run after the original by contract
   (`test_the_shared_detour_hands_the_call_on_after_the_original`), and the
   detour is shared with hidden loot sleep.
6. **Announce at the kill instead of at the burst.** Possible from the list,
   but the line would come minutes before the item can be picked up; the
   default kept is the line when the item lands.

## Live procedure 5

The #227 session: one research build (`plugin_build\build.bat dev`) with the
burst memory and the instrument above, slot 14 Sorak. The procedure is the
toolkit hub's workorder `forgepact-227-chaos-tower-announce`, its context
file's `### Live procedure 1` (a local working file in the hub), and the
session's capture is `.claude/workorders/forgepact-227-chaos-tower-announce-live-1.md`
in the hub, on the owner's machine. In order: the control and the build's
marker at load; `lootann 1` outside the tower; a positive control, a Heroic
item placed in town (P); with the switch off, a Heroic item placed in town,
picked up and kept in the bag for a later drop on a floor (C);
`specialrate chaostower 100`, the tower reached in an Act 1 zone and its
first floor entered; a test line through `lootannprobe say`; on the floor a
Heroic item placed with the burst memory off (A, the rule before this fix)
and one with it on (B); C dropped from the bag on the floor; the floor
cleared; at its end, the decide lines for A, B and C and the red line for B.
Two non-breaking debugger logpoints (`docs/tools/x64dbg-mcp.md` in the hub)
were to show `LootGroundInit` as the debugger's control and `LootExplosion`
at the floor's end.

### Results

Session 2026-10-23, about 14:14-14:31 UTC, research build sha256
`a21409cb7320811b8f337cf165f5ec3540cba8ec6a1bc147af68e8f1113a9fa3` (the
built file's and the installed plugin's, equal; ForgePact commit
`ba5e4c8`), slot 14 Sorak, the Town of Inoya, then the Outskirts of Inoya
(Hell) for the tower and `Chaos_Tower_Floor_01_rm`. The session was
aborted at the floor's end (below), so A, B and the burst were read from
`out.txt` and the stat lines; the red line for B and the absence of one for
A were seen by the owner, who was watching the game, and not by the
capture's screenshots. The saves were backed up first and restored
afterwards (`hs_saves_inspect` after the restore: 0 changed, added or
missing).

| Check | Supplied | Seen | Result |
|---|---|---|---|
| `dll-hash` | the installed plugin's sha256 against the build's | equal | pass |
| `control` | `ping` | `pong (YYTK 4.0.1)` | pass |
| `marker` | `lootann stat` at load | `lootann: off route=server ... burst-listed=0 ...` | pass |
| `armed` | `lootann 1` in town | `init-hook=both create-hook=both install=installed` | pass |
| `positive-control` | P: `lootannprobe place heroic` in town (Cobra Heavy Belt, `"27"`=9, `"32"`=5) | `verdict=announce via=recent`; red `SERVER: Sorak found Cobra Heavy Belt` on a screenshot about 2 s later (a first placement's screenshot was missed) | pass |
| `bag-item-ready` | C: `lootann 0`, a Heroic item placed in town (Cold Cloaking Heavy Belt of Glimmer), picked up, `lootann 1` | no decide line; the item in the bag's first Extra tab on a screenshot | pass |
| `in-tower` | `menulayout`, `lootannprobe burst` on the first floor | `room=Chaos_Tower_Floor_01_rm`; `tower=1 list=ref:291 size=0 given=real:0` | pass |
| `tower-chat-visible` | `lootannprobe say tower chat test` on the floor | red `SERVER: tower chat test` on a screenshot taken at once | pass |
| `baseline-collected` | A: `lootannprobe burst off`, a Heroic item placed (Cobra Heavy Belt) | no decide line; `size` 4 → 5 in A's own frame; on the next read `unidentified` and `burst-listed` +2, `burst-fresh` unchanged, `size` +1 more | pass; the second +1 fits one natural drop listed between the reads, not isolated |
| `target-collected` | B: `lootannprobe burst on`, a Heroic item placed (Armorer's Heavy Belt) | no decide line, no red line; `unidentified`, `burst-listed`, `burst-fresh` and `size` each +1 | pass |
| `natural-collected-fresh` | the floor's own kills (the character's own, below) | `burst-listed`/`burst-fresh` 0/0 → 4/4 before A, 7/5 → 8/6 after B | pass |
| `bagdrop-collected` | C dropped from the bag on the floor | not run: the floor ended during the debugger's freeze, before C was dropped | not observed live |
| `burst-announced` | the floor's end | `lootann decide: "Armorer's Heavy Belt" ... verdict=announce via=burst`, `sink-refused` 0; the owner saw the red line for it | pass, owner-observed (no screenshot of the line) |
| `baseline-held` | the floor's end | `lootann decide: "Cobra Heavy Belt" ... verdict=held-bag-drop via=none` (A's stamp, twice); the owner saw no line for it | pass, owner-observed |
| `bagdrop-held` | C at the floor's end | not run (C never dropped) | not observed live |
| `burst-landed` | `lootann stat`, `lootannprobe burst` after the floor | `burst-landed=12`; `size=0 given=real:1` | pass |
| `dbg-control` | a logpoint on `LootGroundInit`, P's placements | one hit per placement; a hold at a placement froze the game and it ran again after release | pass |
| `dbg-lootexplosion` | a logpoint on `LootExplosion` | refused before it was armed (below) | not observed live |
| `dbg-burst-groundinit` | `LootGroundInit` hits right after `LootExplosion`'s | no logpoint armed after the refusal | not observed live |
| `restored` | stop, saves restore and inspect | the operator held them on the unconfirmed detach; the driver then stopped the game cleanly and restored the backup, inspect 0 changed | pass (driver's teardown, not in the capture) |

What the session established, measured:

- **A tower floor collects its drops.** On the first floor, `IsChaosTower`
  read true and `Controller_obj.lootExplosionList` was a live list. Each
  placed Heroic item, and the floor's own drops from kills, raised its size
  and the mod's `unidentified`, and no decide line came at the placement.
- **The collected items land when the floor is cleared.** At "Floor
  Cleared" the list was empty (`size=0`), `lootExplosionGiven` read true,
  and A and B lay on the ground near the character.
- **The rule before this fix holds a tower drop.** A, listed while the
  burst memory was off, was decided `held-bag-drop` (`via=none`) at the
  burst, with no line. That is #227's report, reproduced.
- **The burst memory announces it.** B, listed while it was on, was decided
  `announce` (`via=burst`) at the burst and the owner saw its red line;
  `sink-refused` stayed 0, and `announced` rose by 1 across the burst (B
  only).
- **Each item was decided twice at the burst.** A twice `held-bag-drop`, B
  once `announce` and once `held-duplicate`, so the second sighting gave no
  second line. `burst-landed=12` fits the six kept keys each seen twice.
- **The red line is visible on a tower floor** (`lootannprobe say`).

Not observed live: a bag drop on a floor (C), so the rule that keeps it
unannounced rests on the harness; that `LootExplosion` is the call that
lands the loot, and which two calls give the two sightings (the logpoints
did not run); Angelic and Unholy items in the tower (the same core path,
harness only); the boss room, the lobby, a Wormhole, a floor left
unfinished and co-op.

Practical findings for the next tower session:

- **Entering the tower.** `specialrate chaostower 100` raised the tower in
  the Outskirts of Inoya; `oget Chaos_Tower_obj x`/`y` and `playerwarp` put
  the character at its foot. The F prompt appears only after walking up the
  tower's outside stairs, not at the foot. F enters the Tower Lobby; the run
  starts at the Tower Master ("Let me in!"); the lobby's north stairs lead
  to Floor 1, which opens with a "Choose Your Personal Hell" pick.
- **Sorak kills tower monsters without casting**, so a step that waits for
  "no kills yet" cannot hold with that character: the floor's count fell
  from 125 on entry to 39 by the time B was placed.
- **x64dbg paused on the floor fight's exceptions in this session.** With
  its default exception settings, x64dbg paused on each first-chance C++
  exception (0xE06D7363) the game raised during the floor fight; the game
  froze for 42 s, the `LootExplosion` logpoint was refused as a break storm,
  and the detach was not confirmed. Passing those exceptions through to the
  game was not tried, so whether x64dbg can stay attached through tower
  combat is not established.

## Not established

- Who invokes the announcement closure online, and what binds it on a
  ground item there. Offline it never ran (Live procedure 1: 0 calls over
  a placed Heroic item and about 450 natural drops); that is "not observed
  offline", not "cannot run offline". Whether it is bound on the offline
  ground item is not established either (Live procedure 1's probe could not
  name the `anon@` methods it found, and Live procedure 3's fixed probe read
  each of their indexes as -1); 0 calls fits "bound, but nothing invokes it
  offline" as well as "not bound".
- What `method_get_index` returns for the ground item's `anon@` methods on
  this runner: Live procedure 3's probe took the value as a number and got
  -1 without printing its kind, so whether -1 is the runtime's answer or a
  reference read as a number is not established.
- Which global the closure reads before giving up on a drop
  `GetRareDropAnnouncement` refuses, and whether the closure would refuse a
  Heroic item offline if something did bind it.
- What `NetworkSendChatMessageIngame`'s first argument holds, and whether its
  `PacketSend` block runs offline. Called by name with `undefined` or the
  player reference first, it refused before counting a `PacketSend`; why it
  refused (the argument, `self`, or a state the game sets up online) is not
  established.
- Which `self` and arguments `GetItemDropMessage(item)` wants from a caller
  outside the game's own chat code: with `self` the local `Player_obj` and the
  item struct it refused.
- Which rarity the natural drop Live procedure 3 announced had, and so
  whether each of Heroic, Angelic and Unholy reaches the mod from a kill
  (`raredrop heroic` does not make kills drop Heroic items, so the cases
  read by rarity are placed; Live procedure 3 counted one natural drop
  announced and the owner saw its line, but the item was not recorded).
- Which script the player's bag drop runs through. Live procedure 2 measured
  that it reaches `LootGroundInit` and that `LootGroundDrop`'s both-route
  detour did not count it; Live procedure 3 measured that the drop (not the
  pickup) raises `created` by 2 and that the struct on the ground was not
  one of those keys. Which caller it is, and whether the struct on the
  ground is the one picked up or a copy made outside `CreateItemNew`, are
  not established. The guard depends on the second: a copy built through
  `CreateItemNew` at the drop would be announced again.
- Whether every bag drop is held: Live procedure 3 held one drop of one
  item. Several items dropped at once, a stack, a drop in the frame of a
  pickup, and a co-op peer's drop (built anew on this client, so announced
  by design) were not tried.
- What failed in the owner's session of 2026-10-08 (§ The switch on at
  launch): it was not captured. Whether hooking at character select was the
  cause is not established (the build before the change was not run). The
  armed install with the switch on before a character exists is measured
  (Live procedure 4's Results: armed at character select, installed after
  the load, a placed Heroic item announced); the switch on through the
  panel's own launch commands is not observed. The probe's Mythic code read
  back as 5 (Live procedure 4); a Mythic item held on the shipped set is not
  observed live (harness only).
- Why the game exited in Live procedure 2 (exit code 1, no fault record).
  Live procedure 3, with Steam running, did not exit; one session does not
  settle the owner's Steam hypothesis.
- Whether a filter-hidden drop of these rarities should be announced: the
  mod decides before hidden loot sleep puts it to sleep, so it is.
- The `method` sink calls through `InvokeMethodValue`, which also updates the
  Pet Quest Collector's route note (`petquest 0`'s "route used"); a session
  that uses both reads that note with that in mind.
- Parts of the Chaos Tower's loot burst (§ Chaos Tower (#227)). Live
  procedure 5 measured the collection on a first floor, the landing at the
  floor's end, and the old and new rules' verdicts there; still open:
  - a bag drop on a tower floor: collected and held by the static reading
    and the harness, not observed live;
  - that `LootExplosion` is the call that lands the loot, and which two
    calls give the two sightings per item measured at the burst (the one
    `LootGroundDrop` makes and the one `LootExplosion` makes after it, by
    the reading); the debugger could not stay attached through the floor;
  - what the protected value at `gDataProtected[12]` means, which also sends
    a new ground item to `lootExplosionList` outside a `Chaos_Tower_` room;
  - which items the one item-kind test exempts from the list, and whether an
    exempt item stays on the floor as an ordinary drop;
  - whether a Wormhole reward (`WormholeGiveReward`, the other direct caller
    of `LootExplosion`) bursts the same list, and what reaches it there;
  - which room the tower's reward (`DropItemHeroic` from the reward function
    beside `Chaos_Tower_NPC_obj`) drops in, and so whether the reward is
    collected at all;
  - that the call after the append destroys the ground instance (inferred
    from the same runtime routine destroying a dialog's instance elsewhere).

## Rarity colours (#228)

### The request

ForgePact #228 (QoL): with Loot announcements on, the line the mod shows is
all red, `SERVER: <character> found <item name>`, because the shipped
`server` sink goes through `ChatAddServerMessage` (§ Route). Players asked
for the online look: the item name coloured by its rarity, the rest of the
line neutral. The issue asked two questions: can one chat line carry more
than one colour, and which colour belongs to each rarity.

Both are answered below from a static reading made 2026-10-23 with the
`ghidra` MCP's read tools and a local scan of the executable for constants
and string literals. No decompiled text, address or offset from those reads
is kept here; what follows is the mechanism in our own words, each point
labelled. Colours are GameMaker colour integers (blue, green and red packed
from the low byte up), written in decimal with the `#RRGGBB` they draw as
beside them.

### Static reading

Everything in this subsection is a static reading unless a point says
measured or inference.

**The chat line.**

- `ChatAddMessage` takes fifteen arguments, in this order: the sender name,
  the text, the sender colour, the text colour, the message type, the
  channel, an account id, a language, a time stamp, a character slot, a DLC
  bitmap, a platform, a platform id, a platform name and a region. A text
  colour left out defaults to 11974842 (#BAB8B6), the game's grey chat text.
- Each call builds one `s_ChatMessageData`, whose fields include `name`,
  `msgNameColor`, `messageColor`, `messageType`, `channelType`, `timestamp`,
  `language`, `commandList` and `msgItemData`. The message goes into every
  `UI_Chat_List_obj` list of its channel, except that a message of type 3 is
  left out while the player's `chatShowItemDropMessages` option is off. It
  reaches the in-game feed (`IngameChatFeedAddLatest`) only when the channel
  is -1 or the `enableGeneralChat` option is on.
- When the message updates, emoji codes in the text (`chatEmojiMap`) become
  image commands, and a text that begins with an item link (item data
  encoded in the text) becomes `msgItemData`: that item's `GetItemColor`
  then becomes the text colour, and hovering the line shows the item's
  tooltip.
- `DrawChatMessage`, a method of `s_ChatMessageData` (SDK name
  `gml_Script_DrawChatMessage@anon@13887@s_ChatMessageData@DefineChatMessageData`;
  the anon number moves with each game patch, RDM § 5.3), draws the line
  with a black shadow under each part: the time stamp in 8421504 (#808080),
  the sender in `msgNameColor`, the platform badges, a separator in 7844807
  (#C7B377), and then the text one character at a time in `messageColor`,
  with emoji images the only thing placed between characters. Nothing in the
  text can change its colour. One special case: when `msgNameColor` is
  exactly 6776816 (#F06767), the text is drawn in the default grey instead
  of `messageColor`.
- So a line carries exactly two colours of the caller's choosing, the
  sender's and the text's, beside the fixed stamp and separator. Colouring
  the item name alone means putting everything before it in the sender part.
  Measured 2026-10-23 (§ Session results, below): three `ChatAddMessage`
  lines each drew their sender part in exactly the sender colour passed and
  their text part in exactly the text colour passed.
- `ChatAddServerMessage` takes only the text and a channel (0 by default).
  It passes the sender (measured: `SERVER`), red 255 (#FF0000) as both
  colours, and message type 1. No colour can be passed through it, which is
  why the line the mod shipped with is all red.
- Measured earlier: `ChatAddMessage` called by name with sender `ForgePact`,
  both colours 0, type and channel as int64 0, two reals 0, a `[hh:mm]`
  stamp and six `undefined` showed `[00:49] ForgePact : ForgePact chat test
  8` (`docs/dungeon-chest-research.md` § Chat route, shape 8, Live 1b). The
  colour of that line was not recorded.

**The online announcement's colours.**

- The ground item's announcement closure (`anon@1138`) hands its line to
  `NetworkSendChatMessageIngame` with 18687 and a colour. That script passes
  18687 (#FF4800, orange) as the sender colour, the closure's colour as the
  text colour, message type 3, and a sender name read from the game's
  protected store (which value it reads: not established).
- The text colour by rarity (info `"27"`): Heroic (9) gets the table's
  Heroic entry, `GetRarityColor(9)`, 9764608 (#00FF94, green). Angelic (7,
  or tier `"32"` 6) gets a fixed 10681599 (#FFFCA2, pale yellow), which is
  not the table's Angelic entry (10092542). Unholy (10) gets a fixed 7224024
  (#D83A6E, crimson), the same value as the table's Unholy entry. A Satanic
  item (6) of tier `"32"` 5 (SS) takes the Heroic branch with its own code,
  so the table's Satanic entry, 1447638 (#D61616).
- With type 3 the text comes from `GetItemDropMessage`: a `[Softcore] ` or
  `[Hardcore] ` tag, the finder's name, the localized phrase under the key
  `just_found`, and the item's loot name in square brackets, all in the one
  text colour. Online, then, the whole text is coloured, not only the item
  name; this change follows the issue's wording instead (§ The line
  ForgePact shows).
- Inference: `GetItemDropMessage` splits its argument as a delimited text,
  not as an item struct. That fits Live procedure 1, where it refused the
  item struct (§ Not established, above).

**The rarity colour table.**

- `GetRarityColor` takes a rarity code and two further arguments and
  returns: 1 Common 16777215 (#FFFFFF), or 10982553 (#9994A7) when its
  second argument is non-zero; 2 Superior 12745262 (#2E7AC2); 3 Rare 698851
  (#E3A90A); 4 215270 (#E64803); 5 Mythic 15406513 (#B115EB); 6 Satanic
  1447638 (#D61616); 7 Angelic 10092542 (#FEFF99); 8 1892620 (#0CE11C); 9
  Heroic 9764608 (#00FF94); 10 Unholy 7224024 (#D83A6E); 11 7844807
  (#C7B377); any other code white.
- `GetItemColor` gives an ordinary equipment item's name the table's colour
  for its info `"27"`; other item kinds have branches of their own.

### The line ForgePact shows

- **A new sink, `chatcolour`, is the shipped route** (`route=chatcolour` in
  `lootann stat`, and the modstate's `route`). It calls `ChatAddMessage` by
  its SDK name through the same by-name route the other sinks use, with
  `self` the local `Player_obj` and the fifteen arguments in the kinds shape
  8 measured: the sender part and the text part as text, the two colours as
  reals, the type and the channel as int64 0, two reals 0, the mod's own
  `[hh:mm]` clock stamp and six `undefined`.
- **The split.** The sender part is `<character> found` (the character's
  name, or `You` when it cannot be read) and the text part is the item name
  (`itemInfoStruct["28"]`). The line reads `[hh:mm] Sorak found : <item
  name>`, the stamp and the separator in the game's own colours. Measured
  2026-10-23: the feed showed `[21:58] Sorak found: Cavebat Heavy Belt`, with
  no `SERVER:` (§ Session results; the spacing around the colon is as the
  operator transcribed it from the screenshot).
- **The colours.** The sender part in 11974842 (#BAB8B6, the game's grey
  chat text). The item name in the online announcement's colour: 9764608
  (#00FF94) for Heroic, 10681599 (#FFFCA2) for Angelic, 7224024 (#D83A6E)
  for Unholy, and the grey for any other code. Neither colour is ever 0 or
  6776816 (the value that turns the text grey). The mod's pure core decides
  the parts and the colours, so the harness tests them; the game adapter
  holds no colour value. Measured 2026-10-23 for Heroic, Angelic and Unholy:
  the screenshot pixels of the sender part and of the item name equal these
  values exactly (§ Session results). The grey for other codes is not
  observed live, since only the three announced rarities reach the sink.
- **The red fallback.** When the `chatcolour` call refuses (its resolution
  is refused, or `script_execute` fails), the mod logs one line with the
  existing `lootann: no line (sink chatcolour) - <field>` prefix (the first
  20) saying the red line is shown instead, and then runs the `server` body.
  `sink-refused=` counts every announcement the shipped sink refused,
  whether the red line then showed or not. An item name that is not text
  refuses before any call, as before, with no fallback, since the red line
  needs the name too; so does a missing local player (the `chatcolour` call
  needs `self`, and the red line would need it as well). `server` now takes
  its `<character> found <item name>` text from the core. A call that
  dispatches but adds no line to the feed is not a refusal: by the static
  reading above, a channel-0 line reaches the bottom-left feed only while
  the game's `enableGeneralChat` option is on, so with general chat off the
  call dispatches, `announced=` counts it, and nothing shows, with no
  fallback and nothing in `sink-refused=`. The `server` route before this
  change had the same channel-0 gate, so this is not new; `route=chatcolour`
  and a zero `sink-refused=` do not by themselves show that a line was
  seen. Not observed live: the fallback itself (harness and contract tests
  only) and the general-chat-off case.
- **Research build only** (`#ifndef FORGEPACT_RELEASE`): after each
  `chatcolour` call, one `out.txt` line, `lootann line: sink=chatcolour
  sender="<sender>" body="<text>" sender-colour=<n> body-colour=<n>
  dispatched=<0|1>`, at most 100 per session. The live session reads the
  colours it sent there, beside its screenshots.
- **Placed items are SS.** `lootannprobe place` sets the placed item's tier
  (info `"32"`, RDM § 16.3) to 5 after the rarity and ends its reply with
  ` "32"=<tier>`, so the owner's loot filter, which hides lower tiers, shows
  it. #227 added that write (§ Chaos Tower (#227), Instrument additions);
  this change keeps its one copy.
- **Unchanged:** which rarities are announced (Heroic 9, Angelic 7, Unholy
  10), the creation guard, the install, the stat and modstate field lists,
  the other four sink bodies, `lootannprobe try 1..5` and the panel.

### Rejected routes

1. **Ask `GetRarityColor` by name for each line.** A by-name call per
   announcement, with its own failure path, and the online Angelic colour is
   not in its table. A table in the core is harness-tested, and a game patch
   that recolours a rarity is the kind of change the module guide's
   Maintenance Triggers cover.
2. **Draw our own overlay to colour single words.** It needs a draw hook
   instead of the game's chat, and per-event hooks are disabled on this
   build (AGENTS.md § Don't Suspend the Game's Own Runtime).
3. **Put an item link in the text.** The game's update pass would parse the
   encoding and colour the whole text anyway. A clickable link or tooltip is
   follow-up material (#17 left links out of scope).
4. **Message type 3, or channel -1.** Neither is measured. Type 3 ties the
   line to the player's drop-messages chat option, and what the channel
   table holds for -1 was not read.
5. **Keep `ChatAddServerMessage`.** It hardcodes red for both colours; it
   stays only as the fallback.

Not rejected, but not the default: the online layout, the character's name
in orange 18687 and `found <item>` all in the rarity colour. The default
follows the issue's wording; the owner judges from the live session's
screenshots, and a switch is a change to the one core function that decides
the parts and colours.

### Live session

The #228 session: one research build (`plugin_build\build.bat dev`) of the
change above, slot 14 Sorak, the Town of Inoya. The procedure is the toolkit
hub's workorder `forgepact-228-loot-colors`, its context file's `### Live
procedure 1` (a local working file in the hub), and the session's capture is
`.claude/workorders/forgepact-228-loot-colors-live-1.md` in the hub, on the
owner's machine. In order: the control and the build's marker at load;
`lootann 1`; a feed control, a red line through `lootannprobe say control`
(`ChatAddServerMessage`, channel 0, the same feed gate as the new line)
caught on a screenshot, so that a missing coloured line could not be taken
for a faded or gated feed; a Heroic, an Angelic and an Unholy item placed
with `lootannprobe place`, each followed at once by a screenshot and
`lootann stat`; the colours of the two parts of each line, and of each
item's ground label, compared on the screenshots; a Satanic item placed
(held); the switch off and a Heroic item placed. No debugger was attached:
every check reads the plugin's own `lootann line:` and `lootann stat`
output, or a screenshot.

### Session results

Session 2026-10-23, about 19:57-19:59 UTC, research build sha256
`e053249d326ed1cabd88ea373314e97ed92dcad7190f750da2aac83cc0d77151` (the
built file's and the installed plugin's, equal; ForgePact commit
`9fcb9fd`), slot 14 Sorak, the Town of Inoya. Screenshots were taken at
2560x1440 and the colours read from their pixels as red, green, blue; the
GameMaker integer each equals is beside it. The saves were backed up first
and restored afterwards (`hs_saves_inspect` after the restore: 0 changed,
added or missing).

| Check | Supplied | Seen | Result |
|---|---|---|---|
| `dll-hash` | the installed plugin's sha256 against the build's | equal | pass |
| `control` | `ping` | `pong (YYTK 4.0.1)` | pass |
| `marker` | `lootann stat` at load | `lootann: off route=chatcolour seen=0 ...` | pass |
| `installed` | `lootann 1` | `init-hook=both create-hook=both install=installed` | pass |
| `feed-control` | `lootannprobe say control` (`ChatAddServerMessage`, `self` the local `Player_obj`), screenshot at once | `dispatched=1`; red `[21:58] SERVER: control`, pixels (255,0,0), first try | pass |
| `on-heroic` | `lootannprobe place heroic` (`"27"`=9, Cavebat Heavy Belt, `"32"`=5) | `lootann line: sink=chatcolour sender="Sorak found" body="Cavebat Heavy Belt" sender-colour=11974842 body-colour=9764608 dispatched=1`; `announced` 0 → 1, `sink-refused` 0 | pass |
| `seen-heroic` | the screenshot taken at once | `[21:58] Sorak found: Cavebat Heavy Belt`, no `SERVER:`; `Sorak found` (186,184,182) = 11974842, the item name green (0,255,148) = 9764608 | pass |
| `on-angelic` | `lootannprobe place angelic` (`"27"`=7, Headhunter) | `body-colour=10681599 dispatched=1`; `announced` 1 → 2, `sink-refused` 0 | pass |
| `seen-angelic` | the screenshot taken at once | `Sorak found` (186,184,182); `Headhunter` pale yellow (255,252,162) = 10681599, plainly apart from the grey | pass |
| `on-unholy` | `lootannprobe place unholy` (`"27"`=10, Cobra Heavy Belt) | `body-colour=7224024 dispatched=1`; `announced` 2 → 3, `sink-refused` 0 | pass |
| `seen-unholy` | the screenshot taken at once | `Sorak found` (186,184,182); `Cobra Heavy Belt` crimson (216,58,110) = 7224024 | pass |
| `segments-distinct` | the three lines above | the sender part grey and the item name in its rarity's colour on every line: two colours per line | pass |
| `label-colour` | each placed item's ground label on the same screenshots | the same hue as the chat item name, about 0.85 times as bright: Heroic (0,217,126), Angelic (216,217,130), Unholy (184,49,94) | pass |
| `held-satanic` | `lootannprobe place satanic` (`"27"`=6, Armorer's Heavy Belt) | no `lootann decide` or `lootann line`; no chat line on a screenshot taken at once; `held-rarity` 0 → 1, `announced` stays 3 | pass |
| `off-no-line` | `lootann 0`, then `lootannprobe place heroic` (Accursed Heavy Belt) | `lootann: off`; no line in `out.txt` or on the screenshot; every count unchanged | pass |
| `restored` | `hs_stop_game`, `hs_saves_restore` | exit 0; 105 files restored; inspect after the restore clean | pass |

Route token: `segment-route: two-colours`. The line is drawn as designed:
the sender part in the colour passed as the sender colour, the text part in
the colour passed as the text colour, so `found` stays grey beside the
character's name and only the item name is coloured.

What it settled: the colours `chatcolour` passes are the ones the game
draws, to the pixel, for all three announced rarities; the Angelic pale
yellow reads apart from the grey; a Satanic item and a switched-off mod
still give no line. Not observed live: the red fallback (it runs only when
the `chatcolour` call refuses, which this session never saw; harness and
contract tests cover it), a natural drop from a kill under this build, the
line with general chat off, co-op, and the separator's colour (its pixels
were not sampled). Why the ground labels read darker than the chat text is
not established.

### Not established (#228)

- That the bottom-left feed draws through `DrawChatMessage` (inference: the
  measured lines show a stamp, a sender and a text in exactly the colours
  passed, as it draws them; the session did not show which code drew them).
- The separator's exact characters and colour on a type-0 line (the
  screenshot shows a colon after the sender; neither its spacing nor its
  pixels were recorded).
- What a click on the sender name does in the opened chat window offline:
  the draw code has a menu over the name, and the sender part now carries
  `<character> found`.
- What the second argument of `GetRarityColor` means (non-zero turns Common
  grey), and which items codes 4, 8 and 11 belong to: no item with those
  codes has been observed.
- Which value the online sender name is read from.
