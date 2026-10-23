# #196: selling the loot your filter hides

**Question.** Issue #196 asks for an off-by-default mod that, when an item the
player's loot filter hides drops, sells it as if to a merchant: the item leaves
the ground and the player gets the gold. Three things behind that were not
known on this build. Is there a script that sells an item, which a mod could
call by name? What does a merchant actually pay for an item, and how does the
game credit it? And what else does a sale change that a mod selling ground
items should, or should not, copy?

**The owner's answers** (2026-10-23), which the mod is written to:

- **Which items:** every item the filter hides. The game's own verdict
  (`lootFilterVisible` false on a `Loot_Ground_obj`) is the only gate, so
  uniques, sets and anything else the filter hides are sold. Quest objects are
  not `Loot_Ground_obj` instances and carry no verdict, and Gems of
  Incarnation never get one (hub `docs/RUNTIME_DATA_MODELS.md` § 18.5), so the
  gate never reaches either. Neither is an exception the mod adds.
- **Gold:** exactly what a merchant would pay, and nothing goes to the
  merchant's buyback list.
- **With hidden loot on:** sell supersedes hide. A hidden drop the mod can sell
  is sold; one it cannot sell stays on the ground and hidden loot puts it to
  sleep as before.

**Posture.** Everything here is measured runtime behaviour, a reading written
in our own words, or our own code. Game objects and scripts are named by their
`hs-game-sdk` names. This doc carries no decompiled script text, no address
and no struct offset, and it does not follow any routine call by call. A
reading is labelled as a reading, and it is not a fact until a live session
records it. The shared facts are in the hub's
[`docs/RUNTIME_DATA_MODELS.md`](../../docs/RUNTIME_DATA_MODELS.md) § 13.10,
"The merchant sale"; this doc carries them in short.

## Static reading

**Static reading**, made 2026-10-23 on the local Ghidra project of build
`498d588550d8` (the decompile index's build hash matched the game's exe). Our
own words throughout.

**No named script sells an item.** Every SDK script whose name mentions
merchant, vendor, shop, sell, buyback, price or value was checked, and each
turned out to be something else. `CallbackMerchantSellPoolExit` and
`CallbackMerchantSellPoolResetGold` handle an answer from the online API (they
decrypt and hash it). `GetMerchantPrice` is a fixed table of 174 names to
values, not an item's sale price. `GetItemBuyPrice` belongs to buying from the
merchant: it sits beside the gold checks of a purchase. The `UiAMarket*`
scripts are the player market. So there is nothing a mod could call by name to
sell an item.

**The sale is one branch of a UI closure.** It lives in
`UI_Inventory_Grid_obj`'s `m_MoveItemToGrid` closure, and is taken when an item
is moved into the merchant's grid. The closure's generated name moves with
every patch (hub RDM § 5.3 and § 9.8). Like the stash move
([stash-move-research.md](stash-move-research.md) § Static reading), it takes
the item from the drag state rather than from an argument, so a ground item
cannot be handed to it.

**What the sale does.** In short, and in our own words:

- It checks the item's integrity hash first (`ItemCheckHash`), and reports the
  client (`ReportClient`) when that check fails.
- It records the dragged item's fingerprint in the merchant window's sell pool,
  the list named `sellPoolFingerprintList`. That list is what the merchant
  offers back as buyback.
- It prices the item from the item's own info struct: the value the item
  answers for info key 9 (`GetItemInfo`, which reads `itemInfoStruct`, RDM
  § 23.2), multiplied by a second value and rounded up.
- It credits that price through `PickUpGoldCheck`, with a fresh counter hash
  from `GetCounterHash`, the same way AFK FARM's worker credit does (RDM
  § 13.10, measured there).
- It writes the price to the game's UI gold log through `GoldLogAdd`, with
  entry kind 18 and the price as the amount (RDM § 13.11).
- It makes one more call that looks like a sound; that call was not
  identified.

A second branch credits the price through `PickUpGoldCheck` without the
repeat flag the first branch passes, and writes no gold log. Its condition was
**not read**, so when a sale takes that branch is **not established**.

**The price's two factors.** The first is `itemInfoStruct["9"]`. This reading
confirms the key, the multiplication and the rounding up that AFK FARM's
earlier reading recorded ("info value 9 times the stack, rounded up", RDM
§ 13.10). That the second factor is the stack count, the item definition's `o`
field with an absent field counting as 1, is AFK FARM's reading and was
**not re-read** here. `GenerateItemPrice` reads the item's definition and its
rarity code (a base of 15, 30, 50, 225 or 450 by rarity, 5 otherwise) and
writes item info through `SetItemInfo`; which key it writes is **not
established**, though 9 is the natural guess.

**What a sale changes, then:** the gold balance (through `PickUpGoldCheck`
only, which on a hash mismatch reports the client and leaves the balance
alone, RDM § 13.10), the UI gold log on the first branch, the buyback list, and
the item's place (it leaves the bag).

**Sources.** The ghidra MCP answered the name search and decompiled
`GenerateItemPrice`. It timed out on the closure at 240 s, so the sale was read
from a call listing of that closure already on this machine (kept outside any
repository), with its globals named by the decompile index's `slot-name` and
its constants read with `tools/ghidra/DumpNear.java`. No decompiled text is
quoted here.

## The mod

`autosell` (Mods → Quality of Life, **Sell loot your filter hides**, off by
default) cannot call the game's own sale for a ground item (above), so it
performs the sale's three effects itself, every one by name: each script is
resolved through its `hs-game-sdk` constant with `asset_get_index` and run
through `script_execute` with `CallBuiltinEx`, the way `ApCallScript` runs a
script, with the local player as self and other.

**What it mirrors, from the first branch:**

- **The price**: the ground item's `itemInstance` struct gives
  `itemInfoStruct["9"]` and the definition's `o`; the price is the first
  times the second (a missing, non-positive or non-numeric `o` counts as 1),
  rounded up. Every field is checked for existence before it is read. A
  missing, non-numeric, non-finite or non-positive value makes the item
  unsellable: it stays on the ground and is counted `unsellable=`.
- **The credit**: a fresh `GetCounterHash` read immediately before
  `PickUpGoldCheck`, with the same arguments the sale's first branch passes.
  Only a true answer counts as credited; anything else leaves the item where
  it is and counts `credit-refused=`.
- **The gold log**: `GoldLogAdd` with entry kind 18 and the price, after a
  credit, as the sale does. ForgePact's own `tracker` and the HS Offline
  Tracker both count income through that log, so a sold drop shows up in
  them as a vendor sale would.

**What it does not mirror, and why:**

- **The sell pool** (`sellPoolFingerprintList`): the owner ruled out buyback.
- **The hash check of a bag item**: a fresh ground item is not in the
  player's item map, so there is nothing to check it against.
- **The ownership change and the online inventory record**
  (`InventoryUpdateExt`): both belong to the online inventory, not to an
  offline ground item.
- **The sound**: not identified, and not needed for the sale.

**When it runs.** The trigger is the shared both-route detour on
`LootGroundInit`, where the filter writes the verdict (see
[hidden-loot-research.md](hidden-loot-research.md) § Static reading). A
second inline detour on that script is refused (#17), so autosell is one more
consumer of the one detour, beside hidden loot, Loot announcements, the flight
charms, Mark 1 and the tracker. While off it returns before any game call.
While on it only notes the call's handles; the decision runs at the end of the
frame, before hidden loot's, because an item hidden loot has put to sleep can
no longer be found. Per noted drop, the first test that fails decides its
counter: a live `Loot_Ground_obj`, a verdict that says hidden, a readable room
that is not persistent (town is never sold in), an item struct not credited
before in this room, a price above 0, a local player, a true credit. Then the
log, a second gold read (the balance must have risen by exactly the price,
`exact=`, else `inexact=`), and `instance_destroy` on the item, the way the
game's own pickup callers and ForgePact's pet relic collector remove a ground
item. The credit comes before the destroy so that a refused credit never
costs the player the item. A room change forgets every pending note and
remembered struct, with no game call.

**Sell supersedes hide.** With hidden loot also on, autosell runs first in
the frame and tells hidden loot which instances it sold, and hidden loot
counts such a note `sold=` instead of `unidentified=`. Everything else in
hidden loot is unchanged: what autosell could not sell is still put to sleep.

**Chaos Tower floors.** A tower floor holds its drops back and places them all
at "Floor Cleared" (RDM § 16.14). The tower branch sits inside
`LootGroundInit` itself (static reading), so every tower drop passes the
detour twice: when it falls, where the branch collects it, and again at the
burst, when the game places it and calls `LootGroundInit` on it a second
time. What is not established is whether the ground instance from the fall
is still alive at the end of that frame, when the mod decides: that the
branch destroys it is an inference. If it is gone, the fall counts `gone=`
and the item is sold at the burst; if it is alive, it is sold at the fall,
and the struct the burst places is the one already credited. So the mod
remembers every item struct it credited in the room and never credits one
twice (`repeat-held=`). That guard stops a second payment only: in the second
case it leaves the placed item where the burst put it, so the player would
keep, hidden, an item they were already paid for (§ Live procedure 1, "What
the outcomes mean"). Live 1's optional tower step says which of the two the
counters show.

**What it never does:** a game address, a struct layout read, a write to the
verdict, a walk of what is already on the ground, a per-frame or periodic
ground scan, the merchant window, the sell pool, `ChangeItemOwner` or
`InventoryUpdateExt`. Only drops from the moment it is switched on are sold.

**A player's own bag drop.** A bag drop passes `LootGroundInit` too (RDM
§ 16.11), so one the filter hides is sold like any other hidden drop outside
town. Telling the two apart would need Loot announcements' creation guard;
the owner's "the filter is the only gate" decides it for now, and the guide's
Known Limitations say so.

**Rejected alternatives.** Calling the game's own sale (it needs the merchant
window open, takes the item from the drag state, expects the item in the
player's map, appends it to the buyback list and sends an online inventory
record, and its closure's name moves with every patch). `GetMerchantPrice` or
`GetItemBuyPrice` for the price (neither is the sale price). A second detour
on `LootGroundInit`, or a ground scan (the first is refused, the second is
ruled out by the issue). Destroying first and crediting after (a refused
credit would lose the item). Selling after hidden loot (a slept item can no
longer be found). Dropping the gold log (the sale writes it, and both
trackers count income through it). A price model in `hs-game-sdk` (the
arithmetic is one line; the harness pins it and Live 1 measures it).

## Instrument

Two subcommands read what the mod does without a debugger.

**`autosell stat`** prints one line: `on=` and `route=` (the shared detour's
route: `both`, `table-only`, `none` or `not-installed`), then the counters:
`notes=` (calls noted), `sold=` and `gold=` (items sold and the gold they
brought), `exact=` and `inexact=` (whether the balance rose by exactly the
price; the 500,000,000 offline cap is one cause of an inexact credit),
`visible=` (shown drops, never touched), `no-filter-var=`, `unsellable=`,
`skipped-persistent=`, `repeat-held=`, `no-player=`, `credit-refused=`,
`destroy-failed=`, `gone=` (no live instance any more), `unidentified=`,
`errors=`, `last=` (the last sale as price, balance before and balance after,
or `none`), and `gold-now=`, the balance read on demand through `GetGoldAmount`
(or `unreadable`). `autosell 1` answers with the route; `autosell 0` answers
with the same counters. `hiddenloot stat` gains `sold=`.

`GetGoldAmount` takes no arguments. The game calls it itself (RDM § 20.4), but
ForgePact has never called it by name before, so `gold-now=` is
**unverified** until Live 1's `gold-read` check.

**`autosell quote <fingerprint>`** (research build only) finds an item in the
player's bag by its item-map fingerprint, the way `giveitem bag` finds its
template, and prints its name (info key 28), its info key 9 value, its count
and the price, through the same price function the sale uses. It changes
nothing. It is how Live 1 compares the mod's price with the merchant's own.

## Live procedure 1

Capture: `.claude/workorders/forgepact-196-autosell-filtered-live-1.md` in the
hub (`forgepact-196-autosell-filtered-live-1`). Check names are in brackets.

- **Build**: the research DLL from `plugin_build\build.bat dev` at the branch
  head, installed only once the owner agrees; [dll-hash]: the installed DLL's
  sha256 equals the built file's.
- **Character**: slot 14 "Sorak", with the owner's own loot filter (strict: it
  hides most drops below S tier), never changed.
- **Debugger**: none. Every check reads the plugin's own counters, the game's
  balance through `GetGoldAmount`, `lootcensus`, or a screenshot. A
  `LootGroundInit` logpoint over a tower floor's burst would be the direct
  check for step 8, but it is left out: during #227's tower fight x64dbg
  paused the game on its own first-chance C++ exceptions and had to be
  detached (RDM § 16.14), so step 8 reads the counters and `lootcensus`
  before the fight, before the burst and after it instead, and says only what
  they can show.
- **Control and marker**: `ping` answers a line starting `pong (YYTK`
  [control]; `autosell stat` answers a line starting `autosell stat: on=0`
  [marker].
- **Cases**: natural monster drops in an Act 1 outdoor zone (hidden ones sold,
  shown ones kept). Outliers: hidden loot on beside it; the merchant's own
  price for an equipment item and, if the bag holds one, a stack; a Chaos
  Tower floor's burst (optional). Not run, and recorded not observed live:
  persistent rooms (harness only), a bag drop, co-op, the gold cap, the
  table-only route.
- **Person block**: the hand sale in step 2 if the operator's clicks cannot
  move the item into the merchant's grid; reaching a Chaos Tower.

Steps:

0. Lease, self-check and a save backup with the game closed; the install
   check [dll-hash]; launch; load slot 14.
1. `ping` [control]; `autosell stat` [marker]; `autosell 0` and `hiddenloot 0`
   so both start off whatever the panel pushed.
2. In town at the merchant: `menulayout` for the bag's fingerprints; pick one
   equipment item; `autosell quote` on it (record the line); `autosell stat`
   (the gold, G0); sell that item to the merchant by hand (F opens the
   merchant; move the item from the bag into the merchant's grid);
   `autosell stat` again (G1) and a screenshot of the HUD's gold.
   [gold-read]: G0 and G1 are whole numbers and G1 agrees with the HUD's gold
   to the digits it shows. [merchant-parity] (research): G1 minus G0 equals
   the quote's `price=`. If the bag holds a stack of more than one, the same
   for it [stack-parity] (research; not observed without one).
3. Waypoint to an Act 1 outdoor zone and walk away from the waypoint. Note the
   YYError count in `bin\YYToolkit.log`. `autosell 1` answers `route=both`
   [route]; `lootcensus` gives the baseline `ground=` and `hidden=`.
4. `lootspawn 1`. If it answers `visible-now=1`: `autosell stat` shows
   `visible=` one higher and `sold=` unchanged, `lootcensus` shows `ground=`
   one higher, and a screenshot shows the item's label [kept-stays]. If it
   answers `hidden-now=1`: record that `sold=` rose by one, and take
   [kept-stays] from step 5's `visible=` rise with a screenshot of a kept
   drop's label.
5. Kill for about 120 s (`playerwarp` beside packs, right-click cast), no
   `dropmult`. `autosell stat`: `sold=` at least 1 [sold-some]; `exact=`
   equals the window's `sold=` rise and `inexact=0` [credit-exact];
   `credit-refused=0 destroy-failed=0 no-player=0 unidentified=0 errors=0`
   [clean]. `lootcensus`: `hidden=` no higher than its step-3 value plus the
   window's `unsellable=` rise [no-hidden-left]. The YYError count is
   unchanged since step 3 [no-runner-errors].
6. `hiddenloot 1`, kill about 60 s, then `hiddenloot stat` and `autosell
   stat`: hidden loot's `sold=` rose by as much as autosell's `sold=` over the
   window, and hidden loot's `slept=` rose by no more than autosell's
   `unsellable=` [supersedes]. `hiddenloot 0`.
7. `autosell 0` (record the OFF line); kill about 30 s; `autosell stat`:
   `sold=` unchanged, and `lootcensus` `hidden=` rose [off-stays].
8. Optional, at most 10 minutes, with hidden loot still off: `autosell 1`,
   then a Chaos Tower floor of this zone (walk up the stairs, then F). Take
   `autosell stat` and `lootcensus` together three times: on entering the
   floor (T0), just before the floor's last enemy is killed (T1; if the last
   one cannot be told, the last reading before "Floor Cleared", with how long
   before it was taken), and after "Floor Cleared" once the items have landed
   (T2), with a screenshot of the landed items at T2. Record, separately for
   the fight window (T0 to T1) and the burst window (T1 to T2), the rise of
   `notes=`, `sold=`, `gone=` and `repeat-held=`, and of the census's
   `ground=` and `hidden=`. `notes=` is not a drop count: #227 saw each
   landed item pass the detour twice at the burst, and the mod notes every
   pass. The last enemy's own drop falls inside the burst window, so up to
   that kill's drops may show there as fall-time counts. [tower-sold-at]
   (research) records two things, each only as the counters show it:
   - `fall-instance: gone` when hidden drops fell in the fight window and
     `gone=` rose there while `sold=` did not; `fall-instance: alive` when
     `sold=` rose in the fight window.
   - `burst: sold` when `sold=` rose in the burst window, `repeat-held=` did
     not, and the census's `hidden=` did not rise by the items sold;
     `burst: repeat-held` when `repeat-held=` rose in the burst window and
     the census's `ground=` and `hidden=` rose with it (the paid items were
     placed again and lie there, hidden).
   Each is `not-observed` when no tower is reached, no hidden drop fell on
   the floor, or the readings fit neither. `autosell 0`.
9. Back in town, open the merchant: a screenshot of its buyback view shows
   the step-2 item and no item the mod sold [no-buyback] (research; not
   observed if no such view is found).
10. Stop the game, inspect and restore the saves, release the lease
    [restored].

**What the outcomes mean.** [merchant-parity] and, if run, [stack-parity]
passing record `price-route: merchant-parity`; either failing records
`price-route: differs`, which means the formula or the count is wrong and the
mod does not ship as it is; no hand sale records `price-route: not-observed`,
and the price then ships as this static reading, labelled unmeasured, in the
notes and the guide. The tower step's two values say only what the counters
saw: whether the falling drop's ground instance was still alive when the mod
decided at the end of that frame, and what the mod did with the item the
burst placed. Neither is a finding about how many times the game calls
`LootGroundInit`, which only a logpoint at the function itself would show.
`fall-instance: gone` with `burst: sold` is the intended path: one sale, at
the burst, and nothing left on the floor. `fall-instance: alive` with
`burst: repeat-held` is **not** a fix: the player was paid at the fall, and
the same item was placed again at the burst, where the guard stopped a second
payment but left the item lying there, hidden by the filter (or put to sleep,
with hidden loot on). The player then keeps an item they were already paid
for. That is not accepted here: it is recorded as a defect for the owner to
decide, either accepting it or changing the mod so that an item already
credited in the room is removed at the burst without a second credit. A
failure of any other check is a defect in the build, and nothing is recorded
as measured from it.

## Not established

- **The second sale branch's condition.** When the game credits a sale
  without the gold log was not read. The mod mirrors the first branch.
- **The stack factor.** That the price's second factor is the definition's
  `o` (absent counting as 1) is AFK FARM's reading, not re-read here; Live 1's
  [stack-parity] measures it if the bag holds a stack.
- **Which key `GenerateItemPrice` writes.** Key 9 is the natural guess.
- **Which `self` the game's credit needs.** The sale passes the grid node and
  the gamba machine passes itself (RDM § 20.4); the mod passes the local
  player. Live 1's [credit-exact] settles whether that credits.
- **`GetGoldAmount` called by name from a plugin.** Unverified until Live 1's
  [gold-read].
- **The price against a real hand sale.** Never measured until Live 1's
  [merchant-parity].
- **A Chaos Tower floor's held drops.** By the static reading in RDM § 16.14
  a hidden tower drop passes the detour both when it falls and at the burst.
  What is not established is whether its ground instance is still alive at
  the end of the frame after the first pass (that the tower branch destroys
  it is an inference), and so whether the mod sells it at the fall or at the
  burst. Live 1's [tower-sold-at] reads which from the counters and the
  census. The `repeat-held=` guard keeps a second credit out either way, but
  if the sale comes at the fall the burst's placed item stays on the floor,
  paid for; whether that is acceptable is the owner's call, not settled here.
  The burst-window `LootGroundInit` logpoint that would show the passes
  directly has never run.
- **The unidentified call after the credit**, which looks like a sound.
- **A player's own bag drop, co-op, the gold cap and the table-only route**
  were not run live.
