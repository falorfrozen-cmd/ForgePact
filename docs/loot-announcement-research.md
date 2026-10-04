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

- **Hooks**, both by SDK name through `HookOneScript`, installed on the first
  `lootann 1` after setup (or the first frame after setup with the switch on):
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
  which script the bag drop runs through.
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
  `undefined`), otherwise the ground id with the stamp. The core then
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
  create-hook=<route>` (`both`, `table-only` or `none`), plus one warning
  line for a hook that is not `both` (for `CreateItemNew`: the game's own
  drops may not be seen as new, so nothing would be announced); `lootann 0`
  answers `lootann: off`; `lootann` / `lootann stat` answers
  `lootann: on|off route= seen= announced= held-rarity= held-no-rarity=
  held-duplicate= held-bag-drop= sink-refused= remembered= created=
  create-overflow= init-hook= create-hook= unidentified= no-item= no-key=
  queue-full=`. The
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
  name; Live procedure 3 retests both (Live procedure 2 ended before it got
  there).
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
  Heroic, Angelic or Unholy drop reaches it is still not observed (Live
  procedure 3 tries again).
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
make. The same log shows the game's own drops built this session with a
fresh numeric `itemTimeStamp` through the detoured `CreateItemNew`.

**Why the guard changed.** A window opened by a hook that never sees the bag
drop cannot hold it. "Built by `CreateItemNew` this frame or the last"
separates a new drop from an existing struct put back on the ground without
knowing the bag drop's script, so the mod now asks that (The mod, above),
and Live procedure 3 measures it: `create-hook`, the bag drop again with
`created` noted around the pickup and the drop, and `natural-fresh`, the
guard's positive control on the game's own drops. The owner accepted on
2026-10-04 that a bag drop announcing again is acceptable if research does
not show a way: `bag-drop-silent` and `no-duplicate` are recorded there, not
required, and a fail ships as a Known Limitation.

**The game's exit, as recorded.** About 44 s after `out.txt`'s last line
(the boost commands' own output, nothing after it), the game exited with
code `0x00000001`, not a clean shutdown (`exit.json`). The incident monitor
found no Windows error-reporting record, no faulting module and no exception
code; no `_crash` report was written; `YYToolkit.log` had nothing at that
time; the Application event log had no Hero Siege record. The cause is not
established. The owner reported afterwards that Steam was not running; the
explanation, "steam was not running, it could be the cause", is a
hypothesis, not a finding (the session did not record Steam's state); Live
procedure 3 checks that Steam is running before launch and records it
(`steam-running`).

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
`method` in the next session (`method-found-2` and `route-method-2`), then
choose `method` or `server`; `server` stays shipped until then. Live
procedure 2 ended before its retest step, so Live procedure 3 runs it. A plain `ChatAddMessage` line without the `SERVER:` prefix is a
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
  saw 450 natural drops reach it but read none of their rarities, and Live
  procedure 2 ended before it read one).
- Which script the player's bag drop runs through. Live procedure 2 measured
  that it reaches `LootGroundInit` and that `LootGroundDrop`'s both-route
  detour did not count it; which caller it is, and whether the struct on the
  ground is the one picked up or a copy, are not established (the creation
  guard does not depend on either).
- Whether the creation guard holds a bag drop and passes the game's own
  drops live (`bag-drop-silent`, `natural-fresh`): Live procedure 3.
- Why the game exited in Live procedure 2 (exit code 1, no fault record).
- Whether a filter-hidden drop of these rarities should be announced: the
  mod decides before hidden loot sleep puts it to sleep, so it is.
- The `method` sink calls through `InvokeMethodValue`, which also updates the
  Pet Quest Collector's route note (`petquest 0`'s "route used"); a session
  that uses both reads that note with that in mind.
