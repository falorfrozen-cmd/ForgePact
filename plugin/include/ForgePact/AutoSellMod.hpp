#pragma once

#include "Common.hpp"
#include <hs_game_sdk/item_type.hpp>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <set>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

namespace ForgePact {

// Auto-sell (ForgePact #196): a ground item the player's own loot filter
// hides is sold the frame it drops. The player is credited the price a
// merchant would pay, through the game's own gold credit, the game's gold log
// is written as a hand sale writes it, and the ground item is removed. It is
// never put on the merchant's buyback list (the owner, 2026-10-23: "merchant
// price, no buyback").
//
// Why the sale is mirrored rather than called (docs/auto-sell-research.md,
// static reading): the game has no sell script. A hand sale is a branch of
// the inventory grid's move-item closure, which wants the merchant window
// open, takes the item from the drag state, expects it in the player's item
// map and appends it to the sell pool. So this class performs the sale's
// three effects by name, through calls ModuleMain hands it:
//   1. the price: ceil(itemInfoStruct["9"] x count), count being the item's
//      itemDefinitionStruct.o when that is a positive number, else 1 (the
//      9, the product and the rounding up are a static reading; the count
//      is AFK FARM's earlier reading, not re-read);
//   2. the credit: GetCounterHash() read just before PickUpGoldCheck(hash,
//      price, 1, undefined x3), the sale's first branch. Whether the player
//      was paid is decided by the balance read either side of it, not by
//      its answer: the credit answers undefined on every path but a failed
//      hash check, which answers false (static reading of PickUpGoldCheck,
//      after Live 1 counted every passing credit refused);
//   3. the gold log: GoldLogAdd(18, price), as the sale writes it, and how
//      ForgePact's tracker and HS Offline Tracker count income.
// Not mirrored, by design: the item's hash check (a fresh ground item is in
// no map), the sell pool, the ownership change, the online inventory record,
// and the sound. The hash is read from the game's global protected store,
// not from `self` (static reading); the local player is what is passed.
//
// What the game will not buy is never sold: an item whose itemType is a key,
// a tarot card, a material, a socketable or a relic (the SDK's ItemType
// members; the inventory tooltip prices these 0 and shows "Item cannot be
// sold", static reading, two of them seen by hand in Live 1), or whose
// itemType cannot be read. Such an item stays on the ground for hidden loot
// (the owner: anything not sellable is not sold).
//
// When: ModuleMain's one LootGroundInit detour (shared with hidden loot and
// the other ground-item consumers) hands Note() the call's argument 0 and
// `self`, already reduced to durable handles (a number or a reference) inside
// the call. Nothing is decided there. At the frame's end, after the loot
// announcer and the tracker and before hidden loot sleeps anything, OnFrame()
// decides each note in this order; the first step that fails names the
// counter and the item is left alone:
//   1. a live Loot_Ground_obj among the handles (gone / unidentified);
//   2. the game's verdict `lootFilterVisible`, asked for before it is read:
//      absent (no-filter-var), shown (visible, never touched);
//   3. a room that is readable and not persistent (skipped-persistent);
//   4. its item struct (else unsellable), its key (no-key), not credited
//      before in this room (repeat-held): a Chaos Tower floor may place the
//      same struct on the ground twice;
//   5. sellable: an itemType the game buys (else unsellable and
//      type-unsellable), then a price above 0 (else unsellable); left on the
//      ground either way;
//   6. a local player (no-player);
//   7. the balance read (unreadable: gold-unreadable, no credit call), the
//      credit, the balance read again. Paid when the balance rose; when the
//      second read fails, when the credit dispatched and did not answer false
//      (also counted credit-unverified). Not paid: the item stays and the
//      cause is counted, credit-call-failed (a script did not dispatch),
//      credit-false (answered false) or credit-unpaid (the balance did not
//      rise), credit-refused being their sum; so a refused credit never
//      loses an item;
//   8. paid: after - before == price is exact, anything else inexact (the
//      offline cap is one cause); then the gold log (refused or thrown:
//      log-refused, the sale stands);
//   9. instance_destroy on the ground item, as the game's own pickup callers
//      and the pet relic collector remove one; still there is destroy-failed,
//      else sold, and the price is added to gold.
// Every credit attempt, paid or not, is kept for the stat line's last=: the
// price, the balance either side and the credit's answer.
// The item struct is remembered as credited the moment the payment is seen,
// so a failure after it can never pay for the same struct twice. An
// exception in one item counts errors and leaves that item; the next is
// still decided.
// The instance ids sold this frame are kept for ModuleMain to hand hidden
// loot, which then counts those notes sold instead of unidentified.
//
// Off means off: Note() and OnFrame() return before any call, to the runner
// or to the setters. A room change forgets pending notes and remembered
// structs without a call.
//
// Never here: an address, a struct layout read, the sell pool, the merchant
// window, a ground scan, a write to the verdict.
//
// Header-only; the runner is reached only through g_Yytk->CallBuiltin, and
// every script call goes through a setter, so tests/auto_sell_harness.cpp
// compiles the real class against a controlled game.
class AutoSellMod {
public:
    static constexpr size_t kPendingCap = 8192;        // drop calls held for one frame at most
    static constexpr const char* kVerdict = "lootFilterVisible";
    static constexpr const char* kItem = "itemInstance";
    static constexpr const char* kInfo = "itemInfoStruct";
    static constexpr const char* kDefinition = "itemDefinitionStruct";
    static constexpr const char* kPriceKey = "9";      // the info value a merchant pays per unit
    static constexpr const char* kNameKey = "28";      // the info value holding the display name
    static constexpr const char* kCountKey = "o";      // the stack count, absent counting as 1
    static constexpr const char* kType = "itemType";   // the item's class, on the item struct
    // The item types the game will not buy (the tooltip's "Item cannot be
    // sold" rule, static reading).
    static constexpr HeroSiege::Items::ItemType kNotBought[] = {
        HeroSiege::Items::ItemType::Key, HeroSiege::Items::ItemType::Tarot, HeroSiege::Items::ItemType::Material,
        HeroSiege::Items::ItemType::Socketable, HeroSiege::Items::ItemType::Relic,
    };

    enum class Route { NotInstalled, None, TableOnly, Both };
    // What the credit call gave back. Failed: a script did not dispatch.
    // False: it answered false (the hash check, static reading). The rest
    // name the kind of any other answer; none of them decides payment, the
    // balance does.
    enum class CreditAnswer { Failed, False, True, Undefined, Number, Other };
    struct Room { bool readable = false; bool persistent = false; };
    // An item struct's identity for the room's credited set (ModuleMain's
    // LaItemKey: a kind and a value).
    using ItemKey = std::pair<uint64_t, uint64_t>;

    // The game-facing calls, made by name in ModuleMain. Each answers false
    // when it could not do what it says.
    using LocalPlayer = bool (*)(RValue& player);
    using Credit = CreditAnswer (*)(const RValue& player, double price);   // the counter hash, then the credit; what it answered
    using GoldLog = bool (*)(const RValue& player, double price);  // the game's gold log, type 18
    using GoldRead = bool (*)(const RValue& player, double& gold); // the balance, by the game's own getter
    using KeyOf = bool (*)(const RValue& itemStruct, ItemKey& key);
    using RoomInfo = Room (*)();

    struct Stats {
        uint64_t notes = 0, sold = 0, exact = 0, inexact = 0;
        uint64_t visible = 0, noFilterVar = 0, unsellable = 0, typeUnsellable = 0, skippedPersistent = 0, repeatHeld = 0;
        uint64_t noPlayer = 0, goldUnreadable = 0;
        uint64_t creditRefused = 0;   // the sum of the three causes below
        uint64_t creditCallFailed = 0, creditFalse = 0, creditUnpaid = 0;
        uint64_t creditUnverified = 0;   // paid on the answer, the balance after unreadable (counted inexact too)
        uint64_t logRefused = 0;         // the gold log answered false or threw; the sale stands
        uint64_t destroyFailed = 0, gone = 0, unidentified = 0, noKey = 0, errors = 0;
        double gold = 0;           // the sum of the prices of the items sold
    };
    struct Last {
        bool any = false;
        double price = 0, before = 0, after = 0;
        bool beforeRead = false, afterRead = false;
        CreditAnswer answer = CreditAnswer::Failed;
    };
    // The item's type, the price and its inputs, or why there is none.
    struct PriceParts {
        double type = -1, info9 = 0, count = 1, price = 0;
        bool typeRefused = false;   // the game will not buy the type, or it could not be read
        std::string why;
    };

    static AutoSellMod& Instance() { static AutoSellMod s; return s; }

    bool Enabled() const { return m_Enabled; }
    Route GetRoute() const { return m_Route; }
    const char* RouteName() const {
        switch (m_Route) {
        case Route::Both: return "both";
        case Route::TableOnly: return "table-only";
        case Route::None: return "none";
        case Route::NotInstalled: return "not-installed";
        }
        return "none";
    }
    const Stats& StatsRef() const { return m_Stats; }
    const Last& LastRef() const { return m_Last; }
    // Instance ids sold by the last OnFrame; empty after a frame that sold
    // nothing, and after every frame while off.
    const std::vector<int64_t>& SoldThisFrame() const { return m_SoldThisFrame; }

    void SetRoute(Route route) { m_Route = route; }
    void SetLocalPlayer(LocalPlayer fn) { m_LocalPlayer = fn; }
    void SetCredit(Credit fn) { m_Credit = fn; }
    void SetGoldLog(GoldLog fn) { m_GoldLog = fn; }
    void SetGoldRead(GoldRead fn) { m_GoldRead = fn; }
    void SetItemKey(KeyOf fn) { m_KeyOf = fn; }
    void SetRoomInfo(RoomInfo fn) { m_RoomInfo = fn; }

    // No call: only drops from now on are sold, nothing already on the ground.
    void Enable() { m_Enabled = true; }
    // No call: pending notes are dropped. Credited structs are kept for the
    // room, so switching off and on cannot pay for one twice.
    void Disable() {
        m_Enabled = false;
        m_Pending.clear();
        m_SoldThisFrame.clear();
    }

    // From the LootGroundInit detour, inside the call: the call's argument 0
    // and `self`, already durable (a number, a reference or undefined). Kept
    // for the frame's end; no call is made here.
    void Note(const RValue& arg0, const RValue& self) {
        if (!m_Enabled) return;
        ++m_Stats.notes;
        if (m_Pending.size() >= kPendingCap) { ++m_Stats.errors; return; }
        m_Pending.push_back({ { arg0, self } });
    }

    // The frame's end. `roomKey` changes on every room change.
    void OnFrame(int64_t roomKey) {
        m_SoldThisFrame.clear();
        if (!m_Enabled) return;
        if (roomKey != m_RoomKey) EnterRoom(roomKey);
        if (m_Pending.empty()) return;
        std::vector<PendingNote> notes;
        notes.swap(m_Pending);
        try {
            if (!LootIndex()) { m_Stats.unidentified += notes.size(); return; }
        } catch (...) { m_Stats.errors += notes.size(); return; }
        m_PlayerAsked = false;
        m_HavePlayer = false;
        std::unordered_set<int64_t> decided;   // the same instance noted twice this frame is decided once
        for (const PendingNote& note : notes) {
            try { Decide(note, decided); } catch (...) { ++m_Stats.errors; }
        }
    }

    // Whether the game buys an item of this type: not one of kNotBought.
    static bool TypeIsBought(double type) {
        for (HeroSiege::Items::ItemType t : kNotBought)
            if (type == static_cast<double>(static_cast<int32_t>(t))) return false;
        return true;
    }

    // Whether the item is sellable and the price a merchant pays for it, the
    // one function both the sale and `autosell quote` use: its itemType
    // readable as a number and one the game buys, then the price above 0.
    // Reads only.
    bool PriceOf(const RValue& item, PriceParts& parts) {
        parts = PriceParts();
        if (Kind(item) != VALUE_OBJECT) { parts.why = "not an item struct"; return false; }
        double type = -1;
        if (!Number(StructField(item, kType), type)) {
            parts.typeRefused = true;
            parts.why = "item type missing or not a number";
            return false;
        }
        parts.type = type;
        if (!TypeIsBought(type)) {
            parts.typeRefused = true;
            parts.why = "item type " + Num(type) + " is not sold by the game";
            return false;
        }
        const RValue info = StructField(item, kInfo);
        if (Kind(info) != VALUE_OBJECT) { parts.why = "no itemInfoStruct"; return false; }
        double info9 = 0;
        if (!Number(StructField(info, kPriceKey), info9)) { parts.why = "info 9 missing or not a number"; return false; }
        parts.info9 = info9;
        if (!(info9 > 0)) { parts.why = "info 9 not above 0"; return false; }
        const RValue definition = StructField(item, kDefinition);
        if (Kind(definition) == VALUE_OBJECT) {
            double count = 0;
            if (Number(StructField(definition, kCountKey), count) && count > 0) parts.count = count;
        }
        parts.price = std::ceil(info9 * parts.count);
        if (!std::isfinite(parts.price) || !(parts.price > 0)) { parts.why = "price not above 0"; return false; }
        return true;
    }

    // `autosell quote`'s fields for an item struct: name="..." type=<t>
    // info9=<v> count=<n> price=<p>, or false with the reason. Reads only.
    bool QuoteFields(const RValue& item, std::string& fields, std::string& why) {
        PriceParts parts;
        try {
            if (!PriceOf(item, parts)) { why = parts.why; return false; }
            std::string name;
            const RValue info = StructField(item, kInfo);
            const RValue text = StructField(info, kNameKey);
            if (Kind(text) == VALUE_STRING) name = text.ToString();
            fields = "name=\"" + name + "\" type=" + Num(parts.type) + " info9=" + Num(parts.info9) + " count=" + Num(parts.count) + " price=" + Num(parts.price);
            return true;
        } catch (...) { why = "a read threw"; return false; }
    }

    // The last credit attempt's price and the balance either side of it:
    // <price>:<before>-><after>, or none. The stat line's last= adds the
    // answer (LastAnswerText).
    std::string LastText() const {
        if (!m_Last.any) return "none";
        return Num(m_Last.price) + ":" + (m_Last.beforeRead ? Num(m_Last.before) : std::string("unreadable"))
            + "->" + (m_Last.afterRead ? Num(m_Last.after) : std::string("unreadable"));
    }
    static const char* AnswerName(CreditAnswer answer) {
        switch (answer) {
        case CreditAnswer::Failed: return "failed";
        case CreditAnswer::False: return "false";
        case CreditAnswer::True: return "true";
        case CreditAnswer::Undefined: return "undefined";
        case CreditAnswer::Number: return "number";
        case CreditAnswer::Other: return "other";
        }
        return "other";
    }
    // The last=: <price>:<before>-><after>:<answer>, or none.
    std::string LastAnswerText() const {
        if (!m_Last.any) return "none";
        return LastText() + ":" + AnswerName(m_Last.answer);
    }
    // The balance now, read on demand through the setters: a number or
    // unreadable.
    std::string GoldNowText() {
        try {
            RValue player;
            double gold = 0;
            if (m_LocalPlayer && m_LocalPlayer(player) && m_GoldRead && m_GoldRead(player, gold)) return Num(gold);
        } catch (...) {}
        return "unreadable";
    }
    // The stat line's fields, from on= to gold-now=, as the live procedure
    // reads them. ModuleMain prefixes `autosell stat: ` or `autosell -> OFF `.
    std::string StatFields() {
        const Stats& s = m_Stats;
        std::string t;
        t += "on=" + std::string(m_Enabled ? "1" : "0");
        t += " route=" + std::string(RouteName());
        t += " notes=" + U(s.notes) + " sold=" + U(s.sold) + " gold=" + Num(s.gold);
        t += " exact=" + U(s.exact) + " inexact=" + U(s.inexact);
        t += " visible=" + U(s.visible) + " no-filter-var=" + U(s.noFilterVar) + " unsellable=" + U(s.unsellable);
        t += " type-unsellable=" + U(s.typeUnsellable);
        t += " skipped-persistent=" + U(s.skippedPersistent) + " repeat-held=" + U(s.repeatHeld);
        t += " no-player=" + U(s.noPlayer) + " gold-unreadable=" + U(s.goldUnreadable);
        t += " credit-refused=" + U(s.creditRefused) + " credit-call-failed=" + U(s.creditCallFailed);
        t += " credit-false=" + U(s.creditFalse) + " credit-unpaid=" + U(s.creditUnpaid);
        t += " credit-unverified=" + U(s.creditUnverified) + " log-refused=" + U(s.logRefused);
        t += " destroy-failed=" + U(s.destroyFailed);
        t += " gone=" + U(s.gone) + " unidentified=" + U(s.unidentified) + " no-key=" + U(s.noKey) + " errors=" + U(s.errors);
        t += " last=" + LastAnswerText();
        t += " gold-now=" + GoldNowText();
        return t;
    }

    // A number as the stat and quote lines print it: whole numbers without a
    // fraction, others to ten significant digits.
    static std::string Num(double v) {
        char buf[64];
        if (std::isfinite(v) && std::fabs(v) < 1e15 && v == std::floor(v)) std::snprintf(buf, sizeof buf, "%.0f", v);
        else std::snprintf(buf, sizeof buf, "%.10g", v);
        return buf;
    }

private:
    struct PendingNote { RValue handles[2]; };

    AutoSellMod() = default;

    static std::string U(uint64_t v) { return std::to_string(v); }
    static uint32_t Kind(const RValue& v) { return static_cast<uint32_t>(v.m_Kind) & 0x0FFFFFFFU; }
    // A handle: a number or a typed reference, as its numeric id.
    static bool Id(const RValue& v, double& out) {
        const uint32_t kind = Kind(v);
        if (kind != VALUE_REAL && kind != VALUE_INT32 && kind != VALUE_INT64 && kind != VALUE_REF) return false;
        try { out = v.ToDouble(); } catch (...) { return false; }
        return std::isfinite(out);
    }
    // A price input: a plain finite number. A reference, a string or a bool
    // is not one.
    static bool Number(const RValue& v, double& out) {
        const uint32_t kind = Kind(v);
        if (kind != VALUE_REAL && kind != VALUE_INT32 && kind != VALUE_INT64) return false;
        try { out = v.ToDouble(); } catch (...) { return false; }
        return std::isfinite(out);
    }
    RValue Call(const char* name, std::vector<RValue> args) { return g_Yytk->CallBuiltin(name, args); }
    // A struct's field, asked for before it is read; undefined when absent.
    RValue StructField(const RValue& s, const char* name) {
        if (Kind(s) != VALUE_OBJECT) return RValue();
        if (!Call("variable_struct_exists", { s, RValue(name) }).ToBoolean()) return RValue();
        return Call("variable_struct_get", { s, RValue(name) });
    }

    // Loot_Ground_obj's object index, by its SDK name; asked again until it
    // resolves.
    bool LootIndex() {
        if (m_LootIndex >= 0) return true;
        double v = -1;
        const RValue index = Call("asset_get_index",
            { RValue(std::string(HeroSiege::Objects::GetObjectName(HeroSiege::Objects::GameObject::Loot_Ground_obj))) });
        if (!Id(index, v)) v = -1;
        m_LootIndex = v >= 0 ? static_cast<int64_t>(v) : -1;
        return m_LootIndex >= 0;
    }

    // Which of a note's handles is a live ground item: 1 found, 0 every
    // handle it had is gone, -1 none to look at or none a ground item.
    int Find(const PendingNote& note, RValue& handle, int64_t& id) {
        bool anyHandle = false, anyLive = false;
        for (const RValue& candidate : note.handles) {
            double n = -1;
            if (!Id(candidate, n) || n < 0) continue;   // undefined, or a kind no durable handle has
            anyHandle = true;
            if (!Call("instance_exists", { candidate }).ToBoolean()) continue;
            anyLive = true;
            double object = -1;
            if (!Id(Call("variable_instance_get", { candidate, RValue("object_index") }), object)) continue;
            if (static_cast<int64_t>(object) != m_LootIndex) continue;
            const RValue own = Call("variable_instance_get", { candidate, RValue("id") });
            double ownId = -1;
            if (!Id(own, ownId) || ownId < 0) continue;
            handle = own;
            id = static_cast<int64_t>(ownId);
            return 1;
        }
        return anyHandle && !anyLive ? 0 : -1;
    }

    // A room change forgets the notes and the credited structs. The first
    // room seen after switching on is not a change: the notes taken since
    // then were taken in it.
    void EnterRoom(int64_t roomKey) {
        if (m_RoomKey != INT64_MIN) m_Pending.clear();
        m_RoomKey = roomKey;
        m_RoomChecked = false;
        m_RoomAllows = false;
        m_Credited.clear();
    }
    // Asked once a room: may anything be sold here?
    bool RoomAllows() {
        if (!m_RoomChecked) {
            m_RoomChecked = true;
            const Room room = m_RoomInfo ? m_RoomInfo() : Room();
            m_RoomAllows = room.readable && !room.persistent;
        }
        return m_RoomAllows;
    }
    // The local player, asked once a frame and only when a sale needs it.
    bool Player(RValue& player) {
        if (!m_PlayerAsked) {
            m_PlayerAsked = true;
            m_HavePlayer = m_LocalPlayer && m_LocalPlayer(m_Player);
        }
        if (m_HavePlayer) player = m_Player;
        return m_HavePlayer;
    }

    void Decide(const PendingNote& note, std::unordered_set<int64_t>& decided) {
        RValue handle;
        int64_t id = -1;
        const int found = Find(note, handle, id);
        if (found == 0) { ++m_Stats.gone; return; }
        if (found < 0) { ++m_Stats.unidentified; return; }
        if (!decided.insert(id).second) return;
        // 2. The verdict is the game's: asked for, then read; never written.
        if (!Call("variable_instance_exists", { handle, RValue(kVerdict) }).ToBoolean()) { ++m_Stats.noFilterVar; return; }
        if (Call("variable_instance_get", { handle, RValue(kVerdict) }).ToBoolean()) { ++m_Stats.visible; return; }
        // 3.
        if (!RoomAllows()) { ++m_Stats.skippedPersistent; return; }
        // 4. Without an item struct there is nothing a merchant could price.
        const RValue item = Call("variable_instance_get", { handle, RValue(kItem) });
        if (Kind(item) != VALUE_OBJECT) { ++m_Stats.unsellable; return; }
        ItemKey key;
        if (!m_KeyOf || !m_KeyOf(item, key)) { ++m_Stats.noKey; return; }
        if (m_Credited.count(key)) { ++m_Stats.repeatHeld; return; }
        // 5. Sellable: a type the game buys, then a price above 0.
        PriceParts parts;
        if (!PriceOf(item, parts)) {
            ++m_Stats.unsellable;
            if (parts.typeRefused) ++m_Stats.typeUnsellable;
            return;
        }
        const double price = parts.price;
        // 6.
        RValue player;
        if (!Player(player)) { ++m_Stats.noPlayer; return; }
        // 7. The balance before; without it there is no telling whether a
        // credit paid, so no credit is attempted.
        Last last;
        last.price = price;
        try { last.beforeRead = m_GoldRead && m_GoldRead(player, last.before); } catch (...) { last.beforeRead = false; }
        if (!last.beforeRead) { ++m_Stats.goldUnreadable; return; }
        // The credit, then the balance after, which decides the payment. The
        // answer decides only when the balance cannot be read again, and then
        // only a dispatched call that did not answer false counts as paid.
        last.any = true;
        last.answer = m_Credit ? m_Credit(player, price) : CreditAnswer::Failed;
        try { last.afterRead = m_GoldRead && m_GoldRead(player, last.after); } catch (...) { last.afterRead = false; }
        m_Last = last;
        const bool answered = last.answer != CreditAnswer::Failed && last.answer != CreditAnswer::False;
        const bool paid = last.afterRead ? last.after - last.before > 0 : answered;
        if (!paid) {
            // Not paid: the item stays where it is and the struct unremembered.
            ++m_Stats.creditRefused;
            if (last.answer == CreditAnswer::Failed) ++m_Stats.creditCallFailed;
            else if (last.answer == CreditAnswer::False) ++m_Stats.creditFalse;
            else ++m_Stats.creditUnpaid;
            return;
        }
        m_Credited.insert(key);
        if (!last.afterRead) ++m_Stats.creditUnverified;
        // 8. Exact or not, then the gold log; a refused log leaves the sale.
        if (last.afterRead && last.after - last.before == price) ++m_Stats.exact;
        else ++m_Stats.inexact;
        try { if (!m_GoldLog || !m_GoldLog(player, price)) ++m_Stats.logRefused; } catch (...) { ++m_Stats.logRefused; }
        // 9. The ground item goes.
        Call("instance_destroy", { handle });
        if (Call("instance_exists", { handle }).ToBoolean()) { ++m_Stats.destroyFailed; return; }
        ++m_Stats.sold;
        m_Stats.gold += price;
        m_SoldThisFrame.push_back(id);
    }

    bool m_Enabled = false;
    Route m_Route = Route::NotInstalled;
    Stats m_Stats;
    Last m_Last;
    LocalPlayer m_LocalPlayer = nullptr;
    Credit m_Credit = nullptr;
    GoldLog m_GoldLog = nullptr;
    GoldRead m_GoldRead = nullptr;
    KeyOf m_KeyOf = nullptr;
    RoomInfo m_RoomInfo = nullptr;
    int64_t m_LootIndex = -1;
    int64_t m_RoomKey = INT64_MIN;
    bool m_RoomChecked = false;
    bool m_RoomAllows = false;
    bool m_PlayerAsked = false, m_HavePlayer = false;
    RValue m_Player;
    std::vector<PendingNote> m_Pending;
    std::vector<int64_t> m_SoldThisFrame;
    std::set<ItemKey> m_Credited;   // item structs credited in this room
};

}   // namespace ForgePact
