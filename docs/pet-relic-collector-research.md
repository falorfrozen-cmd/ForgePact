# Pet collects relics (#124)

Issue #124, as the owner settled it on 2026-09-28: a separate Quality of Life
switch, **Pet collects relics** (`petrelic`, off by default), under which the
player's pet walks to relics lying on screen and picks them up one at a time,
raising the owned copy's level exactly as a hand pickup does. A relic the player
already owns at 10/10 is never targeted. It shares the Pet Quest Collector's
targeting code (`PetQuestSelector`), not its collect call: a dropped relic is
not a quest item, so it needs the game's loot pickup instead.

Everything below is labelled. **Static reading** means read from the game's
compiled code (the Sep-17 `Hero_Siege.exe`) in a local decompiler and written
here in our own words; no game code is quoted (`AGENTS.md` § Legal), and objects
and scripts are named by their `hs-game-sdk` names. **Measured** means observed
on the running game. A thing that was looked for and not seen is written "not
observed", never "does not happen". The sections up to [The
mechanism](#the-mechanism) were written before anything about the pickup was
measured; Live 1 of the workorder `forgepact-124-pet-relics` (2026-10-02)
checked the call shape and the ground-relic read on the running game, and
[Live 1 results](#live-1-results-2026-10-02) says which readings it confirmed
and which stay open. Live 2 the same day ran the player build through the
panel ([Live 2 results](#live-2-results-2026-10-02)).

The game facts are also recorded in the hub's `docs/RUNTIME_DATA_MODELS.md`
§ 10.7, the shared record every module reads. § 10.6 there (the companion's own
loot pickup, from [pet-loot-stuck-research.md](pet-loot-stuck-research.md)) is
the reading this one extends.

## Static reading

Read on 2026-10-02 from the scripts `PickupLoot`, `PickupRelic`,
`RelicSetLevel`, `LootGroundRelicStep`, `PickupLootFunc` and
`LootGroundCreateFuncs` (and the ground-item constructor it registers), the
anonymous function `anon@11081@gml_Object_Loot_Ground_obj_Create_0`, the
`Loot_Manager_obj` pickup closure `anon@11556@gml_Object_Loot_Manager_obj_Create_0`,
and the two unnamed event bodies that call `PickupLoot` directly. Variable names
were recovered from the binary's own name-slot table (the technique in
[pet-quest-collector-c-research.md](pet-quest-collector-c-research.md) § 2);
every name below resolved that way, none is a guess.

### A dropped relic is a ground item, not a quest item

A relic drop creates a `Loot_Ground_obj` (item class 16; hub
`docs/models/relic-pick-spec.md`). It is not in the `Quest_Object_Parent_obj`
family and carries no `m_Questpickup`, so the Pet Quest Collector's collect
call does not apply.

The ground item's constructor (registered by `LootGroundCreateFuncs`) creates a
fresh item-instance struct, stores it in the ground instance's **`itemInstance`**
variable, and writes the item class into that struct's **`itemType`**. The item
definition (whose `b` is the relic id) is the struct's **`itemDefinitionStruct`**.
So, for a ground item:

- the item class is `itemInstance.itemType`, and the relic id is
  `itemInstance.itemDefinitionStruct.b`;
- the companion's Step and the ground item's own Create-defined function both
  read the class this way, through `itemInstance`; neither reads an `itemType`
  on the ground instance itself. Whether the ground instance also carries a
  top-level `itemType` copy was not read (`LootGroundInit`, the one constructor
  step left, timed out in the decompiler; see [Not established](#not-established));
- `itemActive`, `lootFilterVisible`, `itemCompanionTimer`, `isPlayerDrop` and
  `itemIsLocal` are variables of the ground instance itself (the companion and
  the `Loot_Manager_obj` closure read them off the instance, not off
  `itemInstance`).

The function `anon@11081@gml_Object_Loot_Ground_obj_Create_0` (defined in the
ground item's Create event) does this, while the ground item is visible: if
`itemInstance.itemType` is 16 it sets the instance's **`isRelic`** to true and
calls `LootGroundRelicStep`; it then advances an image counter by a fraction of
the room-speed factor; and while `itemActive` is false it calls
`LootGroundDeActiveStep`. By that body it is a per-frame update; who calls it
and how often was not read.

### `LootGroundRelicStep` is an animation, not a pickup

It reads two numeric members of the ground item and nudges each by 0.005 per
delta frame while flipping a direction flag: by its body, the relic's floating
animation. The two member names were not resolved. There is no walk-over pickup
for relics: hub `docs/RUNTIME_DATA_MODELS.md` § 18.6 ("No automatic pickup")
holds for them too. The owner's "when the player walks over it" in #124 is the
click/key pickup every ground item has (§ 10.2 there).

### `PickupLoot` is the one pickup script for every ground item

Direct callers (`call rel32` sites): `CA_playerItemPickupAccept` (network), the
`Loot_Manager_obj` closure above (the player's own pickup, hub § 10.2), and two
unnamed event bodies. One of those is **`Companion_obj`'s Step**: it is the body
that lists `Loot_Ground_obj` instances, reads and writes `lootList`,
`lootTarget`, `lootTimer`, `seekSpeed` and `deltaSpeed`, and treats a
`Coin_obj` target separately. The other reads `botZone`, `botItems`, `botState`
and `startBot`; it is some automated-player object's event, not the companion,
and its object was not identified. It calls `PickupLoot` with the same shape as
the companion (below).

What `PickupLoot` does with what it is given:

- **`self`** is the ground item. The script reads the ground instance's own
  variables only on the "not a local item" path (third argument false): its
  `itemInstance`, `droppingAccount`, `itemString` and position.
- **`other`** is never read by the script directly. It is only passed on, as
  `other`, to the scripts and methods `PickupLoot` calls (`PickupRelic`,
  `ItemCheckHash`, `GetVariable`, the item struct's own methods). The same
  holds in `PickupRelic`.
- **`argc` 5** at every direct call site read. The arguments, in order:
  1. the player index `mplr` (it reaches `GetOnlinePlayerItemOwner`, and the
     relic branch hands it to `PickupRelic`);
  2. the item struct: the ground item's `itemInstance`. The script reads the
     class from its `itemType`; an undefined or zero class returns false at
     once;
  3. a flag tested at the top: false sends the item down a path that reads its
     account, region and string fields (by those names, an item from another
     account) before anything else;
  4. a flag that, when true, runs `ItemCheckHash` on the item struct on the
     script's general path (a mismatch calls `ReportClient` and returns
     false);
  5. a value that defaults to false when undefined or missing, and is read on
     the class-13 branch only.
- **The class switch** is a table of 16, 12, 13 and 14. **Class 16 calls
  `PickupRelic(args[0], args[1])` and returns its result**, with nothing after
  it: the hash check, the online branch and `AddToInventory` are not on the
  relic branch. 12, 13 and 14 have their own branches (not followed here);
  the general path runs `ItemCheckHash` (when the fourth argument is true) and
  ends in `AddToInventory`, whose result is the script's.
  Before the switch, the script also sets `inventoryMapChanged` on the
  `Client_obj` instance when one exists.
- **The script never destroys the ground instance.** Neither `PickupLoot` nor
  `PickupRelic` calls the runtime's instance-destroy helper. Every caller does
  it itself after a true return (below).

### The companion's call (`Companion_obj` Step)

Read from the arrival branch the companion runs once it is within twice its
`deltaSpeed` of a ground-item target:

- it lists every `Loot_Ground_obj` within 144 px of the pet
  (`collision_circle_list`), and for each one that passes the type filter
  (hub § 10.6), has `itemActive` true and has `lootFilterVisible` true, it
  enters a `with` block on that item;
- inside it, **`self` is the ground item and `other` is the `Companion_obj`
  instance** (the instance whose Step this is, as `with` leaves it);
- it calls `PickupLoot` with **`argc` 5**:
  1. `global.mplr`, read once at the top of the Step;
  2. the ground item's `itemInstance`;
  3. the literal `true`;
  4. the literal `true`;
  5. the ground item's `isPlayerDrop`, passed through the game's
     protected-value read `GetVariable` (an extension function called by name;
     on two console platforms a stub is called instead), with the game's
     "value not set" sentinel turned into `undefined`;
- **on a true return it destroys the ground item** itself (the runtime's
  instance-destroy helper, called on `self` inside the `with`), and then,
  whatever the result, takes the item out of `lootList`.

It does nothing else after a pickup: no sound, no pickup effect, no inventory
log line.

### The player's own call (`Loot_Manager_obj` closure)

The closure runs a `with` on `playerLootTarget`, so **`self` is the ground
item and `other` is the instance the closure runs on**, the `Loot_Manager_obj`
(a method bound in that object's Create event; hub § 10.2 measured the same
`self`/`other` pair for `m_LootGroundDeActiveStep` on this path). Quest items
take the `m_Questpickup` branch instead (hub § 10.1). For any other ground item
it calls `PickupLoot` with **`argc` 5**:

1. `global.mplr`;
2. the ground item's `itemInstance`;
3. the ground item's `itemIsLocal`, through `GetVariable` and the same
   sentinel-to-`undefined` step;
4. the literal `true`;
5. the ground item's `isPlayerDrop`, through `GetVariable` as above.

On a true return it plays the pickup sound and effect, writes the inventory
log line, removes the item from its own on-screen label array, and **destroys
the ground item** (the same helper, on `self`). On a false return it reads
`itemIsLocal` again and takes a separate path; that path was not followed.

The two callers differ in the third argument only: the companion passes
`true`, the player's pickup passes the item's `itemIsLocal`. The automated-player
event passes `true`, `true` and `undefined` for the last three.

### `PickupRelic(mplr, itemStruct)` and its two 10/10 outcomes

(Read while planning, 2026-10-02.) It finds the owned copy as hub
`docs/RUNTIME_DATA_MODELS.md` § 2 describes (the relic tab cell
`inventoryRelicGrid[b][0][0]` first, then the five equipped slots), compares the
owned relic's id with the dropped one's, reads the owned copy's `o` (its
level), and only while that is below 10 calls `RelicSetLevel(owned, o + 1)` and
`RelicCheckAchievement`. The result depends on where the owned copy lives:

- **an equipped copy at 10/10**: nothing is raised and the script returns
  **false**, so the ground relic stays where it is. This is the "the game will
  not let it be picked up" the owner reports;
- **a relic-tab copy at 10/10**: nothing is raised, but the script returns
  **true**, the same result as a raise. A caller treats that as a pickup and
  destroys the ground relic, which is then consumed for nothing. Not
  established live. The pet mod cannot reach this branch while the maxed scan
  reads the whole relic tab; the plugin refuses to collect when it does not
  (see [The mechanism](#the-mechanism)). Because a true return here raises
  nothing, the plugin also never takes a true return alone as a pickup: it
  checks that the level actually rose before it destroys the relic;
- **a relic the player does not own**: it goes into a new relic-tab entry
  (`GridAddItem`, `AddItemToMap`, `CreateItemSaveStruct`) and the script returns
  true.

### Placing a relic for a test (`forcerelic`, replan 1)

(Read 2026-10-02, after Live 1 session 2.) The research command `forcerelic`
has to put relics on the ground so a live session has something for the pet to
collect. Until replan 1 it called the original `DropRelic` with two arguments,
the player's x and y, and threw the return away. Live 1 session 2 ran it 91
times (**measured**): every call printed success, `petrelic census` read
`ground items=0` each time, and no relic was on screen.

- **Why the two-argument call placed nothing** (static reading). `DropRelic`
  takes up to six arguments: x, y, two more, a fifth that, when true, skips the
  drop's chance roll, and a sixth it hands on to `LootGroundCreate`. Without the
  fifth it compares a roll against its fourth argument, and an absent fourth
  makes that comparison fail every time, so it returns false before it builds
  anything. It returns true only after its `LootGroundCreate` call. `DropRelic`
  is not named in the local decompiler project (the symbol dump ran with
  ForgePact's hook in the script table); it was found as the one caller of both
  `ReturnRandomPlayerRelic` and `GetRelicQuest` outside the Satanic kill
  routines.
- **With the fifth argument true**, #125's Live 1 (2026-09-30,
  `callnum DropRelic <x> <y> 0 0 1 0`) **measured** relics built (class 16) and
  no `Loot_Ground_obj`. `LootGroundCreate` can place an item it is handed
  directly, or queue a definition on `Loot_Manager_obj`'s create pool for a
  later closure (static reading); which branch that call took is not
  established. `forcerelic drop <n>` keeps this call, with the return counted,
  as the same-build fallback.
- **The route `forcerelic` uses now**: build the relic through the game's own
  loader, `InitItemFromJson` with a relic tab entry's fields
  (`{"b":<id>,"a":<seed>,"j":0,"c":0}`) and a key whose last field is the item
  class, 16, then place it with `LootGroundCreateFromItem(x, y, item)`. By the
  reading that script creates a `Loot_Ground_obj` in a free spot, sets its
  `itemInstance` to the item, runs `LootGroundInit` and returns the new
  instance, or a negative number when none exists; it has no create pool, no
  online branch and no zone gate. It is the route `sigdrop` and `angelicdrop`
  use, **measured** on the ground 30/30 and 17/17 on 2026-09-18
  ([angelic-drop-research.md](angelic-drop-research.md)).
- **What was measured for that route, and what was not.** Its measured `self`
  was always the dying enemy; `forcerelic` passes the player, also a real
  instance, because the placement appears to read variables off `self` (which
  ones was not read) and a kill would need the owner. Class 14 from a `-14` key
  is measured (hub `docs/RUNTIME_DATA_MODELS.md` § 16.2); class 16 from `-16` was
  not, before Live 1. So the command reads every placed instance back with
  `HeroSiege::Player::ReadGroundRelic` and counts a relic as placed only when the
  returned instance exists and reads as a relic with the requested id. Live 1
  **measured** both: with the player as `self`, 49 of 49 relics placed this way
  read back as relics with the requested ids, and `forcerelic drop` put 5 of 5
  on the ground ([Live 1 results](#live-1-results-2026-10-02)).
- **It is a test route, not how the game drops relics.** A relic placed this
  way reaches the pet's collect the way a dropped one does, because
  `PickupLoot` sends class 16 to `PickupRelic`, which reads only the item's
  class and id (above). A natural relic drop stays not observed under the pet
  mod.

## Not established

Live 1 settled two entries that stood here: `DropRelic` with the force flag
does leave relics on the ground, and `LootGroundCreateFromItem` does place a
relic with the player as `self` (both **measured**, [Live 1
results](#live-1-results-2026-10-02)). What is still open:

- **Why #125's `DropRelic` call placed nothing.** #125 (`callnum DropRelic <x>
  <y> 0 0 1 0`, six arguments) saw relics built and none on the ground; Live 1's
  `forcerelic drop` (five arguments, the sixth left out, the player as `self`
  and `other`) put every relic on the ground. Which difference matters, the
  sixth argument or the `self`, was not tested.
- **A relic the game dropped by itself, under the pet mod.** Every relic the
  pet collected in Live 1 was placed by `forcerelic`; none was a natural drop.
  The reading says the pickup cannot tell the two apart (`PickupLoot` sends
  class 16 to `PickupRelic`, which reads only the class and the id), but a
  natural drop collected by the pet is not observed live.
- **The relic-tab 10/10 outcome on the running game**: whether a ground relic
  whose owned copy sits at 10/10 in the relic tab really is consumed with
  nothing raised. The mod does not rely on it either way: it refuses to collect
  without a complete maxed scan, and it destroys a relic only when the owned
  level is seen to rise (`true-but-nothing-raised=` counts the true returns
  that raised nothing, with the last reason). Live 1 never called the pickup
  on a maxed relic, so neither 10/10 branch (equipped, false; relic tab, true)
  was exercised.
- **The `other` the game's own companion passes.** By the reading it is the
  `Companion_obj` instance. Live 1's trace saw the plugin's calls with
  `other` = `Companion_obj` succeed, and the player's own pickup pass
  `Loot_Manager_obj`, but the game's companion never picks up a relic (its type
  filter takes classes 11 to 15), so no call of its own was traced.
- **Whether the ground instance also carries a top-level `itemType`.** Both
  readers seen go through `itemInstance`; `LootGroundInit` did not decompile
  (timeout), so a copy it might make was not read. `petrelic census` printed
  its dump of the first ground relic's variable names in Live 1 (`first relic
  vars:` and `its itemInstance (object/struct) vars:`), but the capture did not
  record the names, so this stays open.
- **Why the plugin's fifth argument was `undefined`.** The player's pickup
  passed `real:0` there; the plugin, which reads `isPlayerDrop` and calls
  `GetVariable` by name, passed `undefined` on all 31 of its calls, so either the
  name did not resolve through the builtin call or the variable was absent on
  the instance. The relic branch never reads that argument, and every one of
  those calls raised the owned level.
- **The `held back=1` of Live 1 step 9.** See [Live 1
  results](#live-1-results-2026-10-02).
- **`isRelic`.** It is set by the Create-defined function only while the item
  is visible; whether every ground relic carries it, and from when, was not
  read. The mod identifies a relic by its class (`itemInstance.itemType`), not
  by `isRelic`.
- **What `GetVariable` does with `isPlayerDrop`** (it is the game's
  protected-value read, by its use around `gDataProtected`), and whether the raw
  `isPlayerDrop` differs from what it returns. The relic branch never reads the
  fifth argument, so a relic pickup cannot tell the two apart by the reading.
- **The instance-destroy helper's identity.** It is unnamed in the binary; it
  is read as the runtime's instance destroy because every caller runs it on the
  ground item only after a true return, with nothing after it, and the item is
  gone afterwards (hub § 10.6 records the companion's successful pickups
  destroying the item).
- **What the `Loot_Manager_obj` closure does when `PickupLoot` returns false**,
  and whether its on-screen label array copes with an item another caller
  destroyed (the companion destroys items without touching that array, which
  suggests it does).
- **The automated-player caller's object** and the network caller's shape
  (`CA_playerItemPickupAccept`); neither is on the mod's path.
- **The names of `LootGroundRelicStep`'s two members.** They did not resolve.
- **The other ground-instance variable names on the running game.** Live 1
  read `itemInstance`, its `itemType` and `itemDefinitionStruct.b`, and
  `itemActive` on 42 relics at once. `lootFilterVisible`, `itemCompanionTimer`,
  `isPlayerDrop`, `itemIsLocal` and `isRelic` are still the name table's
  reading, which has matched live reads every time it was checked.

## The mechanism

The plugin collects a relic the way the companion collects its own loot: one
call to `PickupLoot`, by name (`gml_Script_PickupLoot`), with this shape and
no other:

| | value |
|---|---|
| `self` | the ground relic's instance (a `Loot_Ground_obj` whose `itemInstance.itemType` is 16) |
| `other` | the pet: the `Companion_obj` instance |
| `argc` | 5 |
| `args[0]` | `global.mplr` |
| `args[1]` | the ground relic's `itemInstance` (a struct) |
| `args[2]` | `true` |
| `args[3]` | `true` |
| `args[4]` | the ground relic's `isPlayerDrop` through `GetVariable`, called by name the way the runtime calls a builtin; `undefined` when that name does not resolve or the result is the "not set" sentinel |

Immediately before the call the plugin reads the owned level of this relic id
with `HeroSiege::Player::GetOwnedRelicLevels`, passing both scan reports (see
the eligibility paragraph below; an incomplete read refuses the collect). An
owned level already at 10 (or an id `relicfilter testmaxed` marks, research
build) ends the collect there, counted `skipped(maxed)=`, without a call. Then:

- **a true return**: a true return is not evidence that anything happened.
  By the reading `PickupLoot` and `PickupRelic` never destroy the ground relic,
  so it **still exists** after every true return, a successful one included;
  and `PickupRelic` also returns true on the relic-tab 10/10 branch, which
  raises nothing. So the plugin checks the effect the pickup claims, not the
  instance: it reads `GetOwnedRelicLevels` again, both reports complete, and
  the pickup counts as done only when the owned level of this id is exactly one
  higher than before, or the id was not owned before and its level after the
  call is 1 (a relic always drops at level 1, so a new relic-tab entry starts
  there). Only then does the plugin count `collected=`, destroy the ground
  relic itself (`instance_destroy` with the relic as the instance), as the
  companion, the player's pickup and the automated-player caller all do, and
  count `destroyed-by-plugin=`; it then checks `instance_exists` again. A
  relic still there after the destroy is held back and counted
  `destroy-failed=`, with the log line naming it `destroy-failed`. It also
  checks `instance_exists` before destroying, so an item something else
  already removed is not destroyed twice (that collect counts `collected=` but
  not `destroyed-by-plugin=`);
- **a true return with no raise seen** (the level did not rise by exactly
  one, a newly owned id is not at level 1, or the read after the call stopped
  early): nothing is destroyed. The target is held back through the selector
  (the #94 shape) and counted `true-but-nothing-raised=`, whose `(last <why>)`
  names the latest reason: `no-raise(<before>-><after>)` (a level, or `none`
  for an id not owned) or `after-scan-incomplete(<scan>:<stage>)`. The first
  eight such returns also log one line each, `petrelic: pickup returned true,
  relic left on the ground (<why>); held back.`, with the same reason. On a
  working collect this counter stays 0. An after-read that stopped early
  leaves a relic whose raise may
  have happened unseen, which a second pickup would raise again; that is why
  it is held back from the pet and counted rather than destroyed or retried;
- **a false return, or a call that throws**: nothing is destroyed; the target
  is held back through the selector, and the refusal is counted with what was
  supplied;
- the plugin never writes the companion's `lootList` or `lootTarget`. A relic is
  never in them: the companion's type filter tests for classes 11 to 15 only,
  never 16.

A ground item is a relic, for the mod, when its `itemInstance`'s `itemType` is
the relic class (`HeroSiege::Player::kRelicItemClass`) and its definition yields
an id; the owned copy's level comes from the player's relic tab and equipped
slots, never from the dropped relic (it always drops at level 1). Eligibility is
read again at collect time: the instance exists, the class and id read again,
the id is not maxed, and `itemActive` is true when the instance carries that
name.

The maxed set is a struct walk (`GetMaxedRelicIds` over the relic tab's
`inventoryRelicGrid` and the equipped slots), and a walk that stops early does
not fail: it returns a smaller, plausible set. A relic-tab 10/10 id missing
from it is exactly the relic whose pickup returns true and raises nothing. So
every maxed or owned-level read passes both scan reports
(`EquippedSlotScanReport`, `RelicTabScanReport`) and counts as complete only
when both report `stopped == nullptr`:

- **targeting** (the periodic refresh of the maxed set, once per 60 ticks and
  straight after a true return) keeps the last complete set when a refresh
  stops early and tries again 60 ticks later. With no complete set yet it
  targets nothing, and each failed try counts `refused=` with the reason
  `maxed-scan-incomplete(<scan>:<stage>)`, so `petrelic 0` says why the pet
  is idle;
- **the collect-time read** (the one taken just before the call) must itself be
  complete. When it is not, the plugin does not call, holds the target back and
  counts `refused=` with the reason `maxed-scan-incomplete(<scan>:<stage>)`,
  naming the scan (`tab` or `equipped`) and the stage it stopped at (`mplr`,
  `key`, `inventoryData`, `profile`, `grid`, `global`, `exception`, `not-run`
  and so on, as the report spells them).

**Route B** (research build only, `petrelic route b`) calls
`PickupRelic(global.mplr, itemInstance)` by name with the same `self` and
`other`. By the reading it does exactly what route A's class-16 branch does,
minus `PickupLoot`'s top-of-script steps (the class read, the `Client_obj`
`inventoryMapChanged` flag, and the not-local path that `args[2] = true`
skips anyway). Its result goes through the same before/after level check. It
exists so a refused route A costs a command, not a rebuild. Live 1 confirmed
route A (`pickup-route: pickuploot`), so route A is the shipped call and route
B stays research-only and was not exercised.

### What `petrelic 0` and `petrelic stat` count

One line: `petrelic stat: collected=<n> skipped(maxed)=<n> skipped(not relic)=<n>
skipped(gate)=<n> itemActive missing=<n> refused=<n> (last <why>)
true-but-nothing-raised=<n> (last <why>) destroyed-by-plugin=<n>
destroy-failed=<n> target lost=<n> travel timeouts=<n> held back=<n> maxed
scans=<n> maxed ids=<list> route=<a|b> phase=<idle|travel>`. The fields this
mechanism defines:

- `collected=`: true returns whose raise was seen (the owned level exactly one
  higher, or an id not owned before whose level after the call is 1);
- `true-but-nothing-raised=`: true returns with no raise seen, with the last
  reason, `no-raise(<before>-><after>)` or
  `after-scan-incomplete(<scan>:<stage>)`. Nothing is destroyed and the relic
  is held back. It does not count a successful pickup, although the relic is
  still there when `PickupLoot` returns: a seen raise is what decides, not the
  instance;
- `destroyed-by-plugin=`: the plugin's own `instance_destroy` calls, made only
  after a seen raise;
- `destroy-failed=`: collects whose relic was still there after the plugin's
  own destroy; held back;
- `refused=`: no call made or the call refused (no pet, no player, no item, no
  itemInstance, call threw, no callable, returned false,
  `maxed-scan-incomplete(<scan>:<stage>)`), with the last reason;
- `maxed scans=`: complete reads stored in the targeting set; an incomplete
  one is not counted.

Live 1's `collects-relics` check therefore reads `collected=` above 0 together
with `true-but-nothing-raised=0` and `destroy-failed=0`, which a working
collect produces.

## Live 1 results (2026-10-02)

The research build (sha256 `a591ff14a32da319ab3bd4764cb2a423ce1d31d1f40778063ba4caca88239fd6`,
hub `46f6358`, ForgePact `1c9eab7`) on Sorak, slot 14, in Town of Inoya (Hell),
driven through hs-drive; the third sitting of the workorder's Live 1 procedure,
2026-10-02T17:49Z. The capture is the workorder's
`forgepact-124-pet-relics-live-1.md` (kept with the workorder, outside the
repository). Everything in this section is **measured** unless it says
otherwise. All 14 checks passed:

| check | verdict | what it showed |
|---|---|---|
| `dll-hash` | pass | the lease hashed the installed DLL as the build above |
| `marker` | pass | `petrelic stat:` with `route=a`, all counters 0 |
| `control` | pass | `relicfilter` found 41 maxed relics, the same 41 as the second sitting |
| `ground-placed` | pass | `forcerelic ids 1` placed 1/1 through `LootGroundCreateFromItem`, the census read `relic=1 owned=7 maxed=no`, and a screenshot showed the relic ("Demon Sheep") beside the player |
| `pickuploot-shape` | pass | the player's own pickup, traced (below) |
| `census-reads` | pass | 42 relics on screen, `read stages: ok=42` |
| `collects-relics` | pass | `collected=27`, `true-but-nothing-raised=0`, `destroy-failed=0`, ground 42 -> 15 |
| `level-raised` | pass | relics 73, 106 and 131 went 9 -> 10; relic 1 went 7 -> 8 by the hand pickup |
| `pickup-route` | pass | `pickup-route: pickuploot` |
| `real-maxed-untouched` | pass | all 12 maxed relics placed in step 5 still on the ground, `skipped(maxed)=104586` |
| `testmaxed-skipped` | pass | ids 2 and 3 (`relicfilter testmaxed 2,3`) left on the ground, the other four of that batch collected (27 -> 31) |
| `only-maxed-idle` | pass | three stats about 10 s apart: `collected=31 travel timeouts=0 held back=1 phase=idle` each time |
| `droprelic-route` | pass | `forcerelic drop 5` put 5 relics on the ground (below) |
| `stat-line` | pass | every field of [What `petrelic 0` and `petrelic stat` count](#what-petrelic-0-and-petrelic-stat-count) in the `petrelic 0` line |

**Every relic the pet collected was placed by `forcerelic`**, built through
`InitItemFromJson` and put down with `LootGroundCreateFromItem`; none was
dropped by the game. So a natural relic drop collected by the pet is still not
observed live (see [Not established](#not-established)).

### The pickup shape (`pickuploot-shape`)

The player's own pickup, an OS-level click on the relic sent by hs-drive's
input tool with the `petrelic trace` hook installed on both routes
(`hook on PickupLoot -> native detour + table`), logged, verbatim:

```
petrelic trace #1: PickupLoot self=Loot_Ground_obj other=Loot_Manager_obj argc=5 args=[real:1.000000, object/struct, real:1.000000, bool:true, real:0.000000] -> bool:true self-exists-after=yes
```

That confirms the reading of the player's call: `self` the ground item, `other`
the `Loot_Manager_obj`, `argc` 5, `global.mplr` (1, offline) first, the
`itemInstance` struct second, the fourth the literal `true`. The third and
fifth (the reading's `itemIsLocal` and `isPlayerDrop` through `GetVariable`)
arrived as reals, 1 and 0. The return was true and **the ground relic still
existed when `PickupLoot` returned**, as the reading says: the script does not
destroy it, the caller does. Relic 1's owned level went 7 -> 8, read by placing
a second relic 1 and taking the census.

**The route token is `pickup-route: pickuploot`.** The pet's 31 collects
(trace `#2` to `#32`) all took route A, one shape:

```
PickupLoot self=Loot_Ground_obj other=Companion_obj argc=5 args=[real:1.000000, object/struct, bool:true, bool:true, undefined] -> bool:true self-exists-after=yes
```

Step 7 of the procedure (route B, `PickupRelic` directly) was not needed and
not run, so route A ships unchanged and route B stays a research switch. The
fifth argument the plugin passed was `undefined`, where the player's pickup
passed `real:0`; the relic branch does not read it, and every one of those
calls raised a level (see [Not established](#not-established)).

### The census, before and after

- **Before** (step 2): `petrelic census: ground items=0 player=yes owned relics=141 maxed=41`
  and `on screen=0`.
- **One relic** (step 3): `ground items=1`, `relic inst=262120 relic=1 owned=7
  maxed=no itemActive=0`, `read stages: ok=1`. The census printed the
  first relic's variable names (`first relic vars:`, then `its itemInstance
  (object/struct) vars:`); the capture did not record them.
- **42 relics** (step 5, after `forcerelic 40` and `forcerelic ids 0`):
  `ground items=42 ... maxed=41`, `on screen=42 read stages: ok=42`, 30 not
  maxed and 12 maxed (ids 0 twice, 36 twice, 43 twice, 62, 63, 66, 119, 124,
  45), each line `relic inst=<n> relic=<id> owned=<level> maxed=<yes|no>
  itemActive=<0|1>`.
- **After the pet's 45 s** (step 6): `ground items=15 ... maxed=44`, every one
  `maxed=yes itemActive=1`: the 12 maxed relics of step 5 and three copies of
  relic 73, which the pet had raised from 9 to 10 by collecting its fourth copy.
  Relics 106 and 131 also went 9 -> 10, so the maxed total rose 41 -> 44 by
  exactly the three ids that were at 9.
- **After the `testmaxed` round** (step 9): `ground items=17 ... maxed=46`, the
  15 above plus relics 2 and 3, the `testmaxed` ids; 5, 7, 8 and 9 collected.
- **After `forcerelic drop 5`** (step 10): `ground items=22`, five new relics.

`refused=0` and `true-but-nothing-raised=0` held for the whole session, so no
call was refused and there is no refusal to record with what was supplied.

### What the session measured about placing a relic

- **`LootGroundCreateFromItem` with the player as `self`** placed every class-16
  item it was handed: 1/1, 1/1, 40/40, 1/1 and 6/6 (49 of 49), each ground
  count one higher per relic, each read back by `ReadGroundRelic` as a relic
  with the requested id. The items were built by `InitItemFromJson` from a
  relic-tab entry's fields and a key ending in `-16`, so that key builds a
  relic. They lay at the player's position, not spread out (the screenshot
  shows every name label stacked above one spot). The two relics a census read
  straight after placing them showed `itemActive=0`; every relic listed after
  the pet's 45 s showed `itemActive=1`, so the flag turns on some time after
  placement (when was not measured). Both the player's click and the pet's
  `PickupLoot` picked them up and raised the owned level.
- **`DropRelic` with the force flag** (x, y, 0, 0, `true`, the sixth argument
  left out, the player as `self` and `other`): `forcerelic drop: 5 DropRelic
  call(s) with the force flag (x, y, 0, 0, true) at player (969, 822): returned
  true=5 false=0 other=0; ground items 17 -> 22`, and the census listed five new
  relics (ids 134, 118, 31, 116 and 114, all `itemActive=1`). So with the fifth
  argument true it skips the roll, as the static reading says, and it does
  leave the relic on the ground. #125's six-argument call left none; why is
  not established.

### `held back=1`

`held back=` was 0 through step 6 and read 1 from step 9 on, while
`refused=`, `true-but-nothing-raised=` and `travel timeouts=` stayed 0 and all
four non-`testmaxed` relics were collected. In the plugin's selector a travel
is held back when it ends any way but a collect, a lost target or an abandoned
travel, so the one hold that none of those counters accounts for is a collect
the collect-time maxed check stopped (`skipped(maxed)=`, which includes the
`testmaxed` ids). A likely case is a first pick made from the cached maxed set
before its refresh took in ids 2 and 3; that is a reading of our own code, not
a measurement. It did not stop the pet: the screen held only maxed relics after
it, and the pet stayed idle.

### What the screenshot could not separate

Every relic was placed at the player's position, so the step 9 screenshot shows
the pet against the player with the relic labels stacked above the same spot.
That `only-maxed-idle` passed rests on the stat lines (`phase=idle`, nothing
collected, no travel timeout across three reads), not on the picture.

## Live 2 results (2026-10-02)

The player build (`BloodPactPlugin_ship.dll`, sha256
`1db502603bd93432ed5badb027489113c102b0c50a1ec7a03c1054e74c762ee8`, built
from ForgePact `fc797dc`) on Sorak, slot 14, driven through hs-drive,
2026-10-02T18:39Z. The capture is the workorder's
`forgepact-124-pet-relics-live-2.md` (kept with the workorder, outside the
repository). Everything in this section is **measured** unless it says
otherwise. This session checked the switch end to end on the build players
get, through the panel; it did not collect relics again (Live 1 did that, on
the research build, and the pickup code is the same in both builds).

The panel was this branch's `src/forgepact.py`, run headless (its HTTP server
and its watcher, without the window) on `127.0.0.1:8780`. The switch was
turned on and off by posting `{"key":"mod_pet_relic_pickup","value":...}` to
`/api/set`, the same request the Mods-tab row sends; nobody clicked. All 7
checks passed:

| check | verdict | what it showed |
|---|---|---|
| `dll-hash` | pass | the lease hashed the installed DLL as the ship build above, byte-identical to `plugin_build/BloodPactPlugin_ship.dll` |
| `marker` | pass | `petrelic 0` printed `petrelic -> OFF` and then the `petrelic stat:` line, all counters 0, `route=a phase=idle` |
| `control` | pass | `ping` -> `pong (YYTK 4.0.1)`; `relicfilter status: OFF \| skipped 0 maxed relic(s) since armed \| stands down: no` |
| `panel-on` | pass | 3 s after the switch went on, `petrelic -> ON (pet fetches relics on screen and picks them up; a relic owned at 10/10 is left alone)` |
| `research-only` | pass | `petrelic stat` -> `command unavailable in player build: petrelic stat`, and the switch stayed on (no `petrelic -> OFF` followed, the panel still had it on) |
| `panel-off` | pass | `petrelic -> OFF` and the stat line, which read `maxed scans=67` and `pet-seen ticks=4020` from the 31 s the switch was on |
| `panel-launch` | pass | with the plugin switched off by `petrelic 0` and the panel's saved setting on, a restarted panel sent `petrelic 1` (`petrelic -> ON (...)` in the same batch as `petquest -> ON` and `petunstick -> ON`) with no click and no API call |

- **The panel starts with the switch off.** The panel config the session found
  had no `mod_pet_relic_pickup` key, and the watcher's first apply sent no
  `petrelic` command, so the default off holds on a real config, not only in
  `DEFAULTS`.
- **The player build refuses the research subcommands without switching the
  mod off.** Before the fix in ForgePact `fc797dc`, the player build read
  `petrelic stat` as "off". Live 2 shows the refusal line and the switch still
  on.
- **The stat line works in the player build** (`petrelic 0`), with the maxed
  scan running while the pet was out (`maxed scans=67`). No relic was on the
  ground, so `collected=0` here says nothing about the collect.
- **`panel-launch` was a panel restart against the running game**, as the
  procedure words it, not a game relaunch. That a fresh game start with the
  panel already running sends `petrelic 1` is not observed live. By our own
  code (`watcher` in `src/forgepact.py`), a game process the watcher sees start
  goes through the same `apply_all` the restart ran; that is a reading, not a
  measurement.

Saves and the panel config were restored byte-identical after the session.
