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
| `gml_Script_LootGroundInit`, `gml_Script_LootGroundDrop`, `gml_Script_LootGroundCreateFromItem` | the attachment points | counted (`LootGroundInit` through the detour it shares with hidden loot sleep, `LootGroundDrop` through `lootann`'s own) |
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

- **Hooks**, both by SDK name through `HookOneScript`, installed on the first
  `lootann 1` after setup (or the first frame after setup with the switch on):
  `LootGroundInit`'s one detour, shared with hidden loot sleep (installed by
  `HiddenLootInstall`; a second inline detour on the same script would be
  refused), and a count-only `LootGroundDrop` detour that holds the core's
  bag-drop window open around the game's original.
- **Inside `LootGroundInit`'s call** (`LootAnnounceOnInit`, after the game's
  original and after hidden loot's consumer): argument 0 and `self` are
  reduced to durable handles (a number or reference as it is; an instance
  pointer as its own `id`), and whether the bag-drop window is open is noted.
  Nothing is read or said inside the call.
- **At the end of the frame** (`LootAnnounceTick`, before hidden loot's tick,
  which may put a filter-hidden drop to sleep): the first handle that is a
  live `Loot_Ground_obj` is the ground item; its `itemInstance` is the item;
  the rarity is the item's `itemInfoStruct["27"]`, kept only as a number; the
  time stamp its `itemTimeStamp`. The core then decides: off, bag drop,
  unread rarity, a rarity not in {9, 7, 10}, a second sight of the same
  (ground id, time stamp), or announce.
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
  drop-hook=<route>`, plus one warning line for a hook that is not `both`;
  `lootann 0` answers `lootann: off`; `lootann` / `lootann stat` answers
  `lootann: on|off route= seen= announced= held-rarity= held-no-rarity=
  held-duplicate= held-bag-drop= sink-refused= remembered= init-hook=
  drop-hook= bag-drop-calls= unidentified= no-item= queue-full=`. The
  modstate JSON carries `lootAnnounce` with `on`, `route`, `seen`,
  `announced`, `heldRarity`, `heldNoRarity`, `heldDuplicate`, `heldBagDrop`
  and `sinkRefused`.

### `lootannprobe` (research build only)

Under `#ifndef FORGEPACT_RELEASE`, after `dungeonprobe`. Every form:

- **`lootannprobe on`** (after a character is loaded) attaches the sixteen
  rows of the Static search table, once; the hooks stay in. A row attaches by
  the first rule that applies: `via fp_hiddenloot_init (<route>, ...)` for
  `LootGroundInit` and `via fp_lootann_drop (<route>)` for `LootGroundDrop`
  (the mod's own hooks count the call); `via angelicprobe <row>` or `via
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
- **`lootannprobe place heroic|angelic|unholy|satanic|common`** builds one
  item the way `sigdrop` does (`json_parse`, then `InitItemFromJson` with the
  global instance as self; a Heavy Belt base, and `angelic` with `sigdrop`'s
  Headhunter seed), writes `itemInfoStruct["27"]` = 9, 7, 10, 6 or 1 as Custom
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
  `s_lootDrawData`, which resolved even then, is the positive control in
  the same listing. Then it prints `lootannprobe methods: SDK closure
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
  name; Live procedure 2 retests both.
- `NetworkSendChatMessageIngame` and `GetItemDropMessage` refused when
  called by name from ForgePact with the shapes supplied above, and neither
  counted a `PacketSend`. Only one item argument was ever supplied, the
  ground item's `itemInstance` struct; in tries 2 and 3 `GetItemDropMessage`
  counted 1 from inside `NetworkSendChatMessageIngame`'s own body before the
  call failed, so the item argument is the likelier cause than the by-name
  route or `self`. Whether another item shape (an item save struct, as
  `ChatSendItem` passes per the static reading) would be accepted was not
  tried.
- `ChatAddServerMessage` with our own text is the route that shows a line,
  and the player's `name` holds the character's name (`Sorak`).
- Every natural drop seen reached `LootGroundInit` with a ground item
  reference as argument 0, as the hidden-loot research measured. The
  session did not read the rarity of any natural drop, so whether a natural
  Heroic, Angelic or Unholy drop reaches it is still not observed (Live
  procedure 2 tries again).
- `LootGroundDrop` stayed at 0 through the kills: a game drop passing
  through it was not observed. Its hook has not yet counted a call live, so
  this zero has no positive control; Live procedure 2's `bag-drop-silent`
  is the first.

## Route

announce-route: server

Chosen 2026-10-04 from Live procedure 1, in the order `method`, `netsend`,
`chatadd`, `server`: the first three showed no line offline, and `server`
showed the red `SERVER: <character> found <item name>` line with no error and
no `PacketSend`. The owner approved shipping it (2026-10-04), on the recorded
reason that the ground item carries no announcement method. That reason was a
probe artifact (Results, above): `method` was refused because the probe could
not name any `anon@` method, so it is uninterpretable, not failed. `netsend`
and `chatadd` refused with the one item argument supplied, not a call by name
as such. After the round-2 review the owner chose to fix the probe and retest
`method` inside Live procedure 2 (step 2a, `method-found-2` and
`route-method-2`), then choose `method` or `server`; `server` stays shipped
until then. A plain `ChatAddMessage` line without the `SERVER:` prefix is a
possible follow-up, not part of this change. The header's `kShippedSink` and
`tests/test_loot_announce_contract.py`'s `EXPECTED_ROUTE` name the same
route.

## Not established

- Who invokes the announcement closure online, and what binds it on a
  ground item there. Offline it never ran (Live procedure 1: 0 calls over
  a placed Heroic item and about 450 natural drops); that is "not observed
  offline", not "cannot run offline". Whether it is bound on the offline
  ground item is not established either (Results: the probe could not name
  the `anon@` methods it found); 0 calls fits "bound, but nothing invokes
  it offline" as well as "not bound".
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
- Whether a natural Heroic, Angelic or Unholy drop reaches `LootGroundInit`
  the way placed items and ordinary drops do (`raredrop heroic` does not make
  kills drop Heroic items, so the live cases are placed; Live procedure 1
  saw 450 natural drops reach it but read none of their rarities).
- Whether a filter-hidden drop of these rarities should be announced: the
  mod decides before hidden loot sleep puts it to sleep, so it is.
- The `method` sink calls through `InvokeMethodValue`, which also updates the
  Pet Quest Collector's route note (`petquest 0`'s "route used"); a session
  that uses both reads that note with that in mind.
