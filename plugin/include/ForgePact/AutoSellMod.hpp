#pragma once

#include "Common.hpp"
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
//      price, 1, undefined x3), the sale's first branch; only a true answer
//      counts as credited;
//   3. the gold log: GoldLogAdd(18, price), as the sale writes it, and how
//      ForgePact's tracker and HS Offline Tracker count income.
// Not mirrored, by design: the item's hash check (a fresh ground item is in
// no map), the sell pool, the ownership change, the online inventory record,
// and the sound. Which `self` the credit needs is not established (the sale
// passes the grid node); the local player is what is passed, and Live 1's
// exact-gold check settles it.
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
//   4. its item struct not credited before in this room (repeat-held): a
//      Chaos Tower floor may place the same struct on the ground twice;
//   5. a price above 0 (unsellable, left on the ground);
//   6. a local player (no-player);
//   7. the balance read, then the credit; anything but true (credit-refused,
//      the item stays, so a refused credit never loses an item);
//   8. the gold log, the balance read again: after - before == price is
//      exact, anything else inexact (the offline cap is one cause);
//   9. instance_destroy on the ground item, as the game's own pickup callers
//      and the pet relic collector remove one; still there is destroy-failed,
//      else sold, and the price is added to gold.
// The item struct is remembered as credited the moment the credit takes, so
// a failure after it can never pay for the same struct twice. An exception
// in one item counts errors and leaves that item; the next is still decided.
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

    enum class Route { NotInstalled, None, TableOnly, Both };
    struct Room { bool readable = false; bool persistent = false; };
    // An item struct's identity for the room's credited set (ModuleMain's
    // LaItemKey: a kind and a value).
    using ItemKey = std::pair<uint64_t, uint64_t>;

    // The game-facing calls, made by name in ModuleMain. Each answers false
    // when it could not do what it says.
    using LocalPlayer = bool (*)(RValue& player);
    using Credit = bool (*)(const RValue& player, double price);   // the counter hash, then the credit; true only on a true answer
    using GoldLog = bool (*)(const RValue& player, double price);  // the game's gold log, type 18
    using GoldRead = bool (*)(const RValue& player, double& gold); // the balance, by the game's own getter
    using KeyOf = bool (*)(const RValue& itemStruct, ItemKey& key);
    using RoomInfo = Room (*)();

    struct Stats {
        uint64_t notes = 0, sold = 0, exact = 0, inexact = 0;
        uint64_t visible = 0, noFilterVar = 0, unsellable = 0, skippedPersistent = 0, repeatHeld = 0;
        uint64_t noPlayer = 0, creditRefused = 0, destroyFailed = 0, gone = 0, unidentified = 0, errors = 0;
        uint64_t logRefused = 0;   // the gold log answered false or threw (not on the stat line)
        double gold = 0;           // the sum of the prices of the items sold
    };
    struct Last {
        bool any = false;
        double price = 0, before = 0, after = 0;
        bool beforeRead = false, afterRead = false;
    };
    // The price and its inputs, or why there is none.
    struct PriceParts {
        double info9 = 0, count = 1, price = 0;
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

    // The price a merchant pays for an item struct, the one function both the
    // sale and `autosell quote` use. Reads only.
    bool PriceOf(const RValue& item, PriceParts& parts) {
        parts = PriceParts();
        if (Kind(item) != VALUE_OBJECT) { parts.why = "not an item struct"; return false; }
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

    // `autosell quote`'s fields for an item struct: name="..." info9=<v>
    // count=<n> price=<p>, or false with the reason. Reads only.
    bool QuoteFields(const RValue& item, std::string& fields, std::string& why) {
        PriceParts parts;
        try {
            if (!PriceOf(item, parts)) { why = parts.why; return false; }
            std::string name;
            const RValue info = StructField(item, kInfo);
            const RValue text = StructField(info, kNameKey);
            if (Kind(text) == VALUE_STRING) name = text.ToString();
            fields = "name=\"" + name + "\" info9=" + Num(parts.info9) + " count=" + Num(parts.count) + " price=" + Num(parts.price);
            return true;
        } catch (...) { why = "a read threw"; return false; }
    }

    // The last sale's price and the balance either side of its credit:
    // <price>:<before>-><after>, or none.
    std::string LastText() const {
        if (!m_Last.any) return "none";
        return Num(m_Last.price) + ":" + (m_Last.beforeRead ? Num(m_Last.before) : std::string("unreadable"))
            + "->" + (m_Last.afterRead ? Num(m_Last.after) : std::string("unreadable"));
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
        t += " skipped-persistent=" + U(s.skippedPersistent) + " repeat-held=" + U(s.repeatHeld);
        t += " no-player=" + U(s.noPlayer) + " credit-refused=" + U(s.creditRefused) + " destroy-failed=" + U(s.destroyFailed);
        t += " gone=" + U(s.gone) + " unidentified=" + U(s.unidentified) + " errors=" + U(s.errors);
        t += " last=" + LastText();
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
        if (!m_KeyOf || !m_KeyOf(item, key)) { ++m_Stats.errors; return; }
        if (m_Credited.count(key)) { ++m_Stats.repeatHeld; return; }
        // 5.
        PriceParts parts;
        if (!PriceOf(item, parts)) { ++m_Stats.unsellable; return; }
        const double price = parts.price;
        // 6.
        RValue player;
        if (!Player(player)) { ++m_Stats.noPlayer; return; }
        // 7. The balance before, then the credit. A refused credit leaves the
        // item where it is and the struct unremembered.
        Last last;
        last.any = true;
        last.price = price;
        try { last.beforeRead = m_GoldRead && m_GoldRead(player, last.before); } catch (...) { last.beforeRead = false; }
        if (!m_Credit || !m_Credit(player, price)) { ++m_Stats.creditRefused; return; }
        m_Credited.insert(key);
        // 8. The gold log, the balance after.
        try { if (!m_GoldLog || !m_GoldLog(player, price)) ++m_Stats.logRefused; } catch (...) { ++m_Stats.logRefused; }
        try { last.afterRead = m_GoldRead && m_GoldRead(player, last.after); } catch (...) { last.afterRead = false; }
        m_Last = last;
        if (last.beforeRead && last.afterRead && last.after - last.before == price) ++m_Stats.exact;
        else ++m_Stats.inexact;
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
