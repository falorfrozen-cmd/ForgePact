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
scripts are the player market. So the name search found no script a mod could
call to sell an item; a script under some other name was not ruled out.

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

A second branch credits the price through `PickUpGoldCheck` without the third
argument the first branch passes, and writes no gold log. `PickUpGoldCheck`
replaces a missing third argument with 0 (below), so the two credits differ
only in that argument; what the argument means was **not read**. The second
branch's condition was **not read** either, so when a sale takes that branch
is **not established**.

**The price's two factors.** The first is `itemInfoStruct["9"]`. This reading
confirms the key, the multiplication and the rounding up that AFK FARM's
earlier reading recorded ("info value 9 times the stack, rounded up", RDM
§ 13.10). That the second factor is the stack count, the item definition's `o`
field with an absent field counting as 1, is AFK FARM's reading and was
**not re-read** here. `GenerateItemPrice` reads the item's definition and its
rarity code (a base of 15, 30, 50, 225 or 450 by rarity, 5 otherwise) and
writes item info through `SetItemInfo`; which key it writes is **not
established**, though 9 is the natural guess.

**What a sale changes, then**, as far as the reading went (the list is **not
established as complete**): the gold balance (through `PickUpGoldCheck` only,
which on a hash mismatch reports the client and leaves the balance alone, RDM
§ 13.10), the UI gold log on the first branch, and the buyback list. The item
also leaves the bag (measured in Live 1: the sold ring left its bag cell and
showed in the merchant's grid), but how the closure takes it out of the bag and
the player's item map was **not read**.

**The credit's answer** (static reading, 2026-10-24, same build, from the
local decompile of `PickUpGoldCheck` that `tools/decomp_index.py has
PickUpGoldCheck` lists, made for #173). The script starts with its answer
`undefined` and writes an answer in one place only: when the counter hash does
not match, it reports the client and answers the boolean `false`. Every other
path returns with the answer still `undefined`. So a credit that passes the
hash check answers `undefined`, never `true`, and the answer cannot say whether
gold was paid. The hash `GetCounterHash` makes and the check inside
`PickUpGoldCheck` both read the game's global protected store
(`gDataProtected`, the slot named with the decompile index's `slot-name`), not
`self`, so which instance a caller passes as `self` does not decide the hash.
A missing third argument becomes 0. What the rest of the body does after the
hash check (its online calls, the balance write) was **not read**. The game's
own gamba machine calls `GetGoldAmount` after its own `PickUpGoldCheck` debit
(RDM § 20.4, measured as an order of calls); that the balance write lands
inside the call is an inference from that order, **not established**, as RDM
§ 13.10 says: the write could happen inside the call or be deferred behind its
online calls. The mod reads the balance on either side of its own call and
takes the difference as the payment, so it relies on that inference. Live
procedure 2 measures it before the kill window, with one controlled sale and
no coins about (step 4, [write-sync]), and an unpaid count means nothing about
the game until that check has run.

**The types the game will not buy** (static reading, 2026-10-24, same build).
The tooltip line "Item cannot be sold" comes from the translation key
`item_cannot_be_sold` (the game's `bin\translationsAttributes.csv`). Its one
use in the exe is in `DrawInventoryItemV2`, the inventory tooltip script (RDM
§ 16.6); the ghidra MCP project leaves that function unnamed, so the use was
found by a byte scan and read from a local linear listing kept outside any
repository. The tooltip takes the item's sale price from its `GetItemInfo(9)`
answer, and a switch on the item's `itemType` sets that price to 0 for types
12, 13, 14, 15 and 16: `HeroSiege::Items::ItemType::Key`, `Tarot`,
`Material`, `Socketable` and `Relic` in the SDK's `item_type.hpp`. With the
price at 0, under a condition on one of the script's arguments that was not
identified (the merchant view is the natural reading), it draws "Item cannot
be sold". Live 1 agrees on two of the five (§ Live 1 results): a Topaz
(type 15) and a Destiny Shard (type 14) showed the line, and the merchant would
not take the Topaz. Whether the sale closure itself refuses the same five types
was **not read** (the closure times out); the mod follows the tooltip's rule,
which is the owner's "anything not sellable is not sold" applied to what the
game shows the player. `itemType` sits on the item instance struct beside
`itemDefinitionStruct` (RDM § 9.3), the struct the mod reads as
`itemInstance`, and is the item-map fingerprint's last field.

**Sources.** The ghidra MCP answered the name search and decompiled
`GenerateItemPrice`. It timed out on the closure at 240 s, so the sale was read
from a call listing of that closure already on this machine (kept outside any
repository), with its globals named by the decompile index's `slot-name` and
its constants read with `tools/ghidra/DumpNear.java`. The 2026-10-24 readings
reused #173's local decompiles of `PickUpGoldCheck` and `GetCounterHash`
(found through the decompile index) and a local linear listing of the
tooltip's type switch. No decompiled text, listing or address is quoted here.

## The mod

`autosell` (Mods → Quality of Life, **Sell loot your filter hides**, off by
default) cannot call the game's own sale for a ground item (above), so it
performs the sale's three effects itself, every one by name: each script is
resolved through its `hs-game-sdk` constant with `asset_get_index` and run
through `script_execute` with `CallBuiltinEx`, the way `ApCallScript` runs a
script, with the local player as self and other. The credit's hash is read
from the global protected store, not `self` (static reading, above), so that
choice does not decide the hash.

**What it mirrors, from the first branch:**

- **Which items it will sell**: the five types the tooltip prices at 0
  (above) are never sold. The item's `itemType` must read as a number and not
  be `ItemType::Key`, `Tarot`, `Material`, `Socketable` or `Relic` (the SDK
  enum, no literal numbers); an unreadable type is withheld the same way.
  Such an item stays on the ground for hidden loot and counts `unsellable=`
  and `type-unsellable=`. The check lives in the one function the sale and
  `autosell quote` share, so the quote refuses what the sale would.
- **The price**: the ground item's `itemInstance` struct gives
  `itemInfoStruct["9"]` and the definition's `o`; the price is the first
  times the second (a missing, non-positive or non-numeric `o` counts as 1),
  rounded up. Every field is checked for existence before it is read. A
  missing, non-numeric, non-finite or non-positive value makes the item
  unsellable: it stays on the ground and is counted `unsellable=`.
- **The credit**: the balance read through `GetGoldAmount` first (unreadable:
  `gold-unreadable=`, no credit call, the item stays), then a fresh
  `GetCounterHash` immediately before `PickUpGoldCheck`, with the same
  arguments the sale's first branch passes. **The balance decides whether the
  player was paid**, not the answer, since the answer is `undefined` on every
  path but the hash mismatch (static reading, above): the balance is read
  again right after, and the credit is paid when it rose. If that second read
  fails, a call that dispatched and did not answer `false` counts as paid and
  also as `credit-unverified=`. An unpaid credit leaves the item where it is,
  is not remembered, and counts one cause: `credit-call-failed=` (a script did
  not dispatch), `credit-false=` (the call answered `false`, the hash check by
  the static reading) or `credit-unpaid=` (it dispatched, did not answer
  `false`, and the balance did not rise). `credit-refused=` is their sum.
- **The gold log**: `GoldLogAdd` with entry kind 18 and the price, after a
  paid credit, as the sale does; a dispatch failure or a throw there counts
  `log-refused=` and the sale stands. ForgePact's own `tracker` and the HS
  Offline Tracker both count income through that log; whether either one's
  hook sees the mod's call, or a hand vendor sale's, was **not measured**.

**What it does not mirror, and why:**

- **The sell pool** (`sellPoolFingerprintList`): the owner ruled out buyback.
- **The hash check of a bag item**: a fresh ground item is not in the
  player's item map, so there is nothing to check it against.
- **The online inventory** (`ChangeItemOwner`, `InventoryUpdateExt`): the
  recorded reading of the sale does not show either call, and the mod makes
  neither, since a fresh ground item was never in the player's inventory.
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
that is not persistent (town is never sold in), an item struct with a key
(else `no-key=`) not credited before in this room, a type the game will buy,
a price above 0, a local player, a readable balance, a paid credit. A paid
credit counts `exact=` when the balance rose by exactly the price, else
`inexact=` (the 500,000,000 offline cap is one cause, and an unverified
credit counts here too). Then the log, and `instance_destroy` on the item,
the way the game's own pickup callers and ForgePact's pet relic collector
remove a ground item. The credit comes before the destroy so that an unpaid
credit never costs the player the item. Every credit attempt, paid or not,
sets `last=`. A room change forgets every pending note and remembered struct,
with no game call.

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
the outcomes mean"). Live 1 did not run its optional tower step; Live 2's
step 8 repeats it to say which of the two the counters show.

**What it never does:** a game address, a struct layout read, a write to the
verdict, a walk of what is already on the ground, a per-frame or periodic
ground scan, the merchant window, the sell pool, `ChangeItemOwner` or
`InventoryUpdateExt`. Only drops from the moment it is switched on are sold.

**A player's own bag drop.** A bag drop passes `LootGroundInit` too (RDM
§ 16.11), so one the filter hides is sold like any other hidden drop outside
town. Telling the two apart would need Loot announcements' creation guard;
the owner's "the filter is the only gate" decides it for now, and the guide's
Known Limitations say so.

**Rejected alternatives.** Calling the game's own sale (it is taken when an
item is moved into the merchant's grid, takes the item from the drag state,
checks the dragged item's integrity hash, appends its fingerprint to the
buyback list, and its closure's name moves with every patch). Trusting the
credit's answer (it is `undefined` whether or not gold was paid, static
reading; Live 1's build did, and counted every credit refused). Selling
whatever has a price (`autosell quote` priced a socketable and a material the
game will not buy, Live 1). `GetMerchantPrice` or
`GetItemBuyPrice` for the price (neither is the sale price). A second detour
on `LootGroundInit`, or a ground scan (the first is refused, the second is
ruled out by the issue). Destroying first and crediting after (a refused
credit would lose the item). Selling after hidden loot (a slept item can no
longer be found). Dropping the gold log (the sale writes it, and both
trackers count income through it). A price model in `hs-game-sdk` (the
arithmetic is one line; the harness pins it and Live 1 measured it).

## Instrument

Two subcommands read what the mod does without a debugger.

**`autosell stat`** prints one line: `on=` and `route=` (the shared detour's
route: `both`, `table-only`, `none` or `not-installed`), then the counters:
`notes=` (calls noted), `sold=` and `gold=` (items sold and the gold they
brought), `exact=` and `inexact=` (whether the balance rose by exactly the
price; the 500,000,000 offline cap is one cause of an inexact credit),
`visible=` (shown drops, never touched), `no-filter-var=`, `unsellable=` and,
as a part of it, `type-unsellable=` (a type the game will not buy, or an
unreadable type), `skipped-persistent=`, `repeat-held=`, `no-player=`,
`gold-unreadable=` (the balance could not be read before the credit, so no
credit was tried), `credit-refused=` and its three causes
`credit-call-failed=`, `credit-false=` and `credit-unpaid=` (it equals their
sum), `credit-unverified=` (paid by the call's outcome because the balance
could not be read after it), `log-refused=` (the gold log call failed after a
paid credit), `destroy-failed=`, `gone=` (no live instance any more),
`unidentified=`, `no-key=` (an item struct without a key, counted under
`errors=` before Replan 1), `errors=`, `last=` and `gold-now=`. `last=` is
the last credit attempt, paid or not, as price, balance before, balance after
(a number or `unreadable`) and the credit's answer, each after a colon, or
`none`; the answer is `undefined`, `true`, `false`, `number`, `failed` (a
script did not dispatch) or `other`. `gold-now=` is the balance read on demand
through `GetGoldAmount` (or `unreadable`). `autosell 1` answers with the
route; `autosell 0` answers with the same counters. `hiddenloot stat` gains
`sold=`.

`GetGoldAmount` takes no arguments. The game calls it itself (RDM § 20.4);
ForgePact had never called it by name before #196. Live 1 measured it from
the plugin ([gold-read], § Live 1 results): it answered whole numbers that
matched the HUD's gold.

**`autosell quote <fingerprint>`** (research build only) finds an item in the
player's bag by its item-map fingerprint, the way `giveitem bag` finds its
template, and prints its name (info key 28), its `itemType` as `type=`, its
info key 9 value, its count and the price, through the same sellability and
price function the sale uses. A type the game will not buy answers `autosell
quote: refused - item type <n> is not sold by the game`. It changes nothing.
It is how Live 1 compared the mod's price with the merchant's own, and how
Live 2 checks the type rule against items in the bag.

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

## Live 1 results

Capture `forgepact-196-autosell-filtered-live-1` (the hub's
`.claude/workorders/forgepact-196-autosell-filtered-live-1.md`), 2026-10-24,
research build of the branch as it stood before Replan 1, slot 14 "Sorak" in
Fields of Battle (Act 1, Hell). No debugger. The session's saves were restored
at the end, so nothing it paid or sold persisted. Measured unless labelled.

- [dll-hash], [control], [marker]: pass.
- [gold-read]: pass. `gold-now=` read 318545 before the hand sale and 318602
  after it, whole numbers, and the HUD showed 318,602.
- [merchant-parity]: pass. The operator sold a ring (Phasing Bronze Ring of
  Rapidcasting, type 7) to the merchant by hand; the balance rose by 57, the
  `price=` its `autosell quote` gave. Three bag items of type 11 were refused
  by the quote with an info key 9 value not above 0.
- [stack-parity]: not observed. The only stacks in the bag were socketables (a
  Topaz x4 and a Diamond x10); moving the Topaz into the merchant's grid sold
  nothing, and its tooltip, like a Destiny Shard's (a material), read "Item
  cannot be sold". `autosell quote` still priced the Topaz at 692 and the
  Diamond at 1350: the build had no type rule, which Replan 1 added (§ Static
  reading, "The types the game will not buy").
- [route]: pass (`route=both`). [kept-stays]: pass (`lootspawn` made a shown
  set item; `visible=` rose by one, `sold=` did not, its label lay on the
  ground).
- [sold-some]: fail. Over the 120 s kill window `notes=` reached 45: one was
  the step-4 shown item, and the other 44 all counted `credit-refused=`;
  `sold=0`, `gold=0`, and the hidden items stayed on the ground.
  [credit-exact]: not observed (no sale). [clean]: fail on
  `credit-refused=44` alone. [no-hidden-left]: fail (`lootcensus` `hidden=47`
  against a limit of 3).
- [no-runner-errors]: pass (the YYError count did not change).
- [supersedes]: fail, as a consequence: with nothing sold, hidden loot put
  the hidden drops to sleep (`slept=` rose by 22 in the 60 s window) while
  `credit-refused=` kept rising.
- [off-stays]: pass (`sold=` and `notes=` unchanged while off, `hidden=` rose
  from 71 to 95).
- [tower-sold-at]: not observed (step 8 was not run: with every credit
  counted refused the tower counters could show neither path).
  [no-buyback]: not observed (the merchant window showed no buyback view;
  its stock had refreshed and the step-2 ring was no longer listed).
- [restored]: pass.

Route token: `price-route: merchant-parity`. The tower's `fall-instance:` and
`burst:` are both `not-observed`.

**What `credit-refused=` meant in Live 1** (static reading, § Static reading,
"The credit's answer"). Every one of the 44 hidden drops passed every earlier
test and reached the credit. `PickUpGoldCheck` answers `undefined` whenever
the hash check passes, and that build counted only a true bool or a non-zero
number as credited, so `credit-refused=` counted every answer the build could
not read as a payment, whether or not gold was paid; it was never evidence
that the game refused the credit. Whether those credits paid the player is
**not established**: the balance rose by 4,459 over the step-5 window with
`gold=0` and by 314 over step 7's 30 s with the switch off, but coins dropped
throughout and the kill rates differ, so the rise cannot be split between
coins and credits. The rise fits the reading that they paid, and so would a
player who keeps a hidden item they were paid for; Replan 1 decides payment
from the balance around the call (§ The mod) and Live 2 measures it.

## Live procedure 2

Capture: `.claude/workorders/forgepact-196-autosell-filtered-live-2.md` in the
hub (`forgepact-196-autosell-filtered-live-2`). Check names are in brackets.
Written against the state before Live 1: Live 1 restored its backup, so the
bag and the gold are as Live 1 found them.

- **Build**: the research DLL from `plugin_build\build.bat dev` at the branch
  head with Replan 1's credit and type changes, installed only once the owner
  agrees; [dll-hash]: the installed DLL's sha256 equals the built file's.
- **Character**: slot 14 "Sorak", with the owner's own loot filter, never
  changed.
- **Debugger**: none. Every question this session asks is a value, not which
  code runs. The mod decides whether its own credit paid from the balance read
  in-process immediately before and after that call: the plugin calls the
  scripts one after the other from its frame callback, so nothing of the game
  runs between the two reads but the credit itself. That shows the calls run in
  order, not that the balance write happens inside the credit: the write was
  not read and could be deferred, and then the after-read would miss a payment
  that does arrive. Step 4's [write-sync] gives this instrument its positive
  control in the same session, on one sale made while no coin can move the
  balance, before an unpaid result is allowed to mean anything. What the call
  answered is the content of
  its result, which a hardware logpoint at the function's entry cannot see, and
  an item's type is a field the mod reads. A `PickUpGoldCheck` logpoint would
  also fire for every coin picked up in the kill windows. The tower step keeps
  Live 1's reason for having no `LootGroundInit` logpoint (§ Live procedure 1,
  Debugger).
- **Control and marker**: `ping` answers a line starting `pong (YYTK`
  [control]. `autosell stat` answers a line starting `autosell stat: on=0`
  that contains `credit-unpaid=` and `type-unsellable=` [marker], which also
  proves the Replan 1 build is the one installed.
- **Cases**: natural monster drops in an Act 1 outdoor zone (hidden ones sold,
  shown ones kept). Outliers: the five types the game will not buy (two in
  the bag, quoted; any that drop, counted); hidden loot on beside it; a Chaos
  Tower floor (optional). Not run, and recorded not observed live: the hand
  sale and the buyback (Live 1 measured [merchant-parity] and found no buyback
  view), a sellable stack, persistent rooms (harness only), a bag drop, co-op,
  the gold cap, the table-only route.
- **Relies on**: what Live procedure 1 relies on; `GetGoldAmount` by name
  (Live 1's [gold-read]); the credit's answer, the self-free hash and the five
  types (§ Static reading, and RDM § 13.10 and § 16.6); the bag fingerprints
  Live 1 recorded for the Topaz (`0-0-212850037745-15`), the Diamond
  (`0-0-212850154515-15`) and the ring (`0-0-213716543158-7`); a `lootspawn`
  copy reaching the mod's detour like a monster drop (Live 1's [kept-stays]
  counted it `visible=`). Whether the owner's filter hides a copy of the ring
  is not known in advance, which is why step 4 allows three templates.
- **Person block**: reaching a Chaos Tower, if the operator cannot.

Steps:

0. Lease, self-check and a save backup with the game closed; the install
   check [dll-hash]; note the YYError count in `bin\YYToolkit.log`; launch;
   load slot 14.
1. `ping` [control]; `autosell stat` [marker]; `autosell 0` and `hiddenloot 0`
   so both start off whatever the panel pushed.
2. In town, open the bag (C); `menulayout` on the main tab, the socket sub-tab
   and the material sub-tab for fingerprints. `autosell quote` the ring, the
   Topaz, the Diamond and one material item (any; record its key).
   [quote-types]: the ring answers a line with `type=7` and `price=57`; the
   Topaz and the Diamond each answer `autosell quote: refused - item type 15
   is not sold by the game`; the material item answers the same with `item
   type 14`. If a key from Live 1 is absent, use any item of the same sub-tab
   and record its key.
3. Waypoint to an Act 1 outdoor zone (Fields of Battle, as Live 1) and move
   away from the waypoint. Note the YYError count. `autosell 1` answers
   `route=both` [route]; `autosell stat` and `lootcensus` give the baseline
   `gold-now=`, `ground=` and `hidden=`.
4. `lootspawn 1`, then [kept-stays] exactly as Live procedure 1 step 4. Then,
   before any kill, [write-sync] (research, the instrument's own positive
   control). Stand still where no coin lies within reach and none can drop
   (no kill since step 3). Take `autosell stat` twice about 5 s apart: both
   `gold-now=` values must be equal (the balance at rest; if they differ,
   [write-sync] is `not-observed`). Then `lootspawn 1 <fingerprint>` with an
   equipment item from the bag that the filter hides: the ring from step 2
   first, then other equipment items, at most three tries, each template and
   its `hidden-now=`/`visible-now=` recorded. On the first `hidden-now=1`, the
   mod sells that copy at the frame's end: take `autosell stat` at once (S1)
   and again about 5 s later without moving (S2), and record S1's `sold=`,
   `exact=`, `credit-unpaid=` and `last=`, and both `gold-now=` values.
   - `in-call` (the instrument proven): S1's `sold=` and `exact=` each rose
     by one, `last=`'s after-balance minus its before-balance equals its
     price, and S2's `gold-now=` equals S1's and `last=`'s after-balance (no
     second payment arrived later).
   - `deferred`: S1's `credit-unpaid=` rose by one with `last=`'s
     after-balance equal to its before-balance, and S2's `gold-now=` exceeds
     that after-balance by exactly `last=`'s price: the credit paid, after
     the mod's same-frame read.
   - `unpaid`: S1's `credit-unpaid=` rose by one and S2's `gold-now=` still
     equals `last=`'s after-balance: the balance rose neither across the call
     nor in the seconds after it.
   - `not-observed`: no try answered `hidden-now=1`, or the balance moved at
     rest. Any other result is recorded as it is and settles nothing.
5. Kill for about 120 s (`playerwarp` beside packs, right-click cast), no
   `dropmult`. `autosell stat` about every 30 s, then at the end `autosell
   stat` and `lootcensus`:
   - [sold-some]: `sold=` at least 1;
   - [credit-exact]: `exact=` rose by the window's `sold=` rise, with
     `inexact=0` and `credit-unverified=0`;
   - [clean]: `credit-refused=0 credit-call-failed=0 credit-false=0
     credit-unpaid=0 gold-unreadable=0 log-refused=0 destroy-failed=0
     no-player=0 unidentified=0 no-key=0 errors=0`;
   - [gold-rose]: the window's `gold=` rise is above 0, and `gold-now=` rose
     over the window by at least that much (coins only add to it); a window
     with no `gold=` rise fails it rather than passing on a zero;
   - [no-hidden-left]: `lootcensus` `hidden=` no higher than its step-3 value
     plus the window's `unsellable=` rise;
   - [no-runner-errors]: the YYError count is unchanged since step 3;
   - [credit-answer] (research): the answer field of `last=`, after its last
     colon, is `undefined`; recorded whatever it is;
   - [type-seen] (research): `pass` when `type-unsellable=` rose and
     `hidden=` kept those items; `not-observed` when no drop of the five
     types fell;
   - [write-sync-window] (research, only when step 4's [write-sync] was
     `not-observed`): the first time an `autosell stat` shows
     `credit-unpaid=` risen, stop killing at once, step away from any coins
     on the ground, stand still for about 5 s and read `autosell stat` again;
     record `credit-unpaid=`'s rise, `last=`'s price and after-balance, and
     the new `gold-now=`. Coins can confound this one, so it reads `unpaid`
     only when `gold-now=` still equals `last=`'s after-balance, and
     `deferred` only when `credit-unpaid=` rose by one and the gap is exactly
     `last=`'s price; any other gap or rise, or no `credit-unpaid=` rise, is
     `not-observed`.
6. `hiddenloot 1`, kill about 60 s, then `hiddenloot stat` and `autosell
   stat`: hidden loot's `sold=` rose by as much as autosell's `sold=` over the
   window, and hidden loot's `slept=` rose by no more than autosell's
   `unsellable=` [supersedes]. `hiddenloot 0`.
7. `autosell 0` (record the OFF line); kill about 30 s; `autosell stat`:
   `sold=` unchanged, and `lootcensus` `hidden=` rose [off-stays].
8. Optional, at most 10 minutes, with hidden loot still off: `autosell 1`,
   then Live procedure 1 step 8 word for word (the T0, T1 and T2 readings and
   [tower-sold-at] with its `fall-instance:` and `burst:` values).
   `autosell 0`.
9. Stop the game, inspect and restore the saves, release the lease
   [restored].

**What the outcomes mean.** `credit-route: paid` when [sold-some] and
[credit-exact] pass. A `credit-unpaid=` count says only that the balance did
not rise between the mod's two reads; what it means about the game is decided
by [write-sync] (step 4), or by [write-sync-window] when step 4's was
`not-observed`. `credit-route: deferred` when either reads `deferred`: the
by-name credit pays, but after the mod's same-frame read, so the player is
paid while the mod counts the drop `credit-unpaid=` and leaves it on the
ground. The payment route holds; the mod's payment check is wrong, a defect
in the build for a fix round, not a replan of the payment route.
`credit-route: unpaid` only when either reads `unpaid`: the balance rose
neither across the call nor in the seconds after it, so the by-name credit
pays nothing, the payment route itself is wrong and the mod needs a replan.
With [write-sync] `in-call`, the instrument is proven, so a `credit-unpaid=`
rise in a later window is a [clean] failure like any other, for a fix round.
With neither check reading `deferred`, `unpaid` or `in-call`,
`credit-unpaid=` settles nothing about the game, no `credit-route:` token is
recorded from it, and the session's other results stand. `credit-route:
refused-false` when `credit-false=` rose: the hash check refused the mod's
call, so the self-free reading of the hash is wrong, also a replan.
`credit-route: call-failed` when only `credit-call-failed=` rose: a dispatch
defect in the adapter, for a fix round. [type-seen] and [credit-answer] are
research: their values are recorded, and `not-observed` is a finding, not a
defect. A [credit-answer] other than `undefined` with [credit-exact] passing
means the static reading of the answer is wrong while the payment rule still
holds: it is recorded and the reading corrected, with no fix round. The tower
step's values mean what they mean in Live procedure 1. A failure of any other
check is a defect in the build, and nothing is recorded as measured from it.

## Not established

- **The second sale branch's condition**, and what `PickUpGoldCheck`'s third
  argument means (the second branch leaves it out, which the script turns
  into 0). The mod mirrors the first branch.
- **The rest of `PickUpGoldCheck` after its hash check** (its online calls
  and the balance write): not read. That the write lands inside the call is
  an inference from the gamba machine's order of calls (RDM § 13.10), not
  established, and the mod relies on it, since it decides payment from the
  balance read right before and right after its call in the same frame. Live
  2 measures it on one controlled sale before the kill window ([write-sync],
  `credit-route:`).
- **Whether Live 1's refused credits paid the player.** The balance rose
  while they were counted, but coins dropped throughout (§ Live 1 results).
  Live 2's [credit-exact] and [gold-rose] measure the payment directly.
- **Whether the by-name credit pays at all, from the local player as `self`.**
  The hash does not read `self` (static reading, made after Live 1); that
  the call pays is Live 2's [credit-exact] and `credit-route:` token.
- **The credit's answer in a live call.** `undefined` by the static reading;
  Live 2's [credit-answer] records it.
- **Whether the sale closure refuses the same five types** the tooltip prices
  at 0: not read (the closure times out). The tooltip's own rule is static
  reading, and Live 1 measured it for two types (a socketable and a material);
  Live 2's [quote-types] checks the mod's rule against bag items, and
  [type-seen] whether any such drop fell.
- **The condition under which the tooltip shows "Item cannot be sold"** for a
  price of 0: one of the script's arguments, not identified.
- **The stack factor.** That the price's second factor is the definition's
  `o` (absent counting as 1) is AFK FARM's reading, not re-read here. Live 1
  found no sellable stack ([stack-parity] not observed), and Live 2 does not
  look for one.
- **Which key `GenerateItemPrice` writes.** Key 9 is the natural guess.
- **The buyback list.** The mod never writes the sell pool, but Live 1 found
  no buyback view to look at ([no-buyback] not observed), and Live 2 does not
  look again.
- **Whether a tracker's hook sees `GoldLogAdd`**, from the mod or from a hand
  vendor sale: not measured.
- **A Chaos Tower floor's held drops.** By the static reading in RDM § 16.14
  a hidden tower drop passes the detour both when it falls and at the burst.
  What is not established is whether its ground instance is still alive at
  the end of the frame after the first pass (that the tower branch destroys
  it is an inference), and so whether the mod sells it at the fall or at the
  burst. Live 1 did not reach a tower; Live 2's step 8 reads which from the
  counters and the census ([tower-sold-at]). The `repeat-held=` guard keeps a second credit out either way, but
  if the sale comes at the fall the burst's placed item stays on the floor,
  paid for; whether that is acceptable is the owner's call, not settled here.
  The burst-window `LootGroundInit` logpoint that would show the passes
  directly has never run.
- **The unidentified call after the credit**, which looks like a sound.
- **A player's own bag drop, co-op, the gold cap and the table-only route**
  were not run live.

Settled by Live 1 (measured): `GetGoldAmount` called by name from a plugin
answers the balance the HUD shows ([gold-read]), and the mod's price matches a
real hand sale of an equipment item ([merchant-parity]). Settled after Live 1
by static reading: the credit's hash does not depend on the `self` the mod
passes, and the credit's answer cannot report a payment.
