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
observed", never "does not happen". As of this writing nothing here is measured:
Live 1 of the workorder `forgepact-124-pet-relics` is where the call shape and
the ground-relic read are first checked on the running game.

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

## Not established

- **The relic-tab 10/10 outcome on the running game**: whether a ground relic
  whose owned copy sits at 10/10 in the relic tab really is consumed with
  nothing raised. The mod does not rely on it either way: it refuses to collect
  without a complete maxed scan, and it destroys a relic only when the owned
  level is seen to rise (`true-but-nothing-raised=` counts the true returns
  that raised nothing, with the last reason).
- **The companion's `other` on the running game.** By the reading it is the
  `Companion_obj` instance, and neither `PickupLoot` nor `PickupRelic` reads
  `other` directly, but no trace has shown the value. The research build's
  `petrelic trace` logs `self`, `other`, `argc` and every argument of each
  `PickupLoot` call; the player's own click pickup is its positive control.
- **Whether the ground instance also carries a top-level `itemType`.** Both
  readers seen go through `itemInstance`; `LootGroundInit` did not decompile
  (timeout), so a copy it might make was not read. `petrelic census` (its
  dump of the first ground relic's variable names) settles it.
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
- **Every ground-instance variable name above on the running game.** They are
  read from the binary's name table, which has matched live reads every time it
  was checked, but none of these has been read live yet.

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
exists so a refused route A costs a command, not a rebuild. Both routes are
confirmed or refused by Live 1, through `petrelic trace` on the player's own
click pickup first.

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
