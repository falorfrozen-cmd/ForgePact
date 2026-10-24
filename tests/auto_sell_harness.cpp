// Behavioral regression harness for auto-sell (AutoSellMod.hpp).
//
// The Python runner injects the REAL ForgePact::AutoSellMod class below. Only
// the game is replaced: the runner's builtins, and the game-facing calls the
// class is handed by setters (the local player, the gold credit, the gold
// log, the gold read, the item key and the room's identity), which ModuleMain
// makes by name. No game process is touched.
//
// The controlled world holds ground items (Loot_Ground_obj) carrying the
// game's filter verdict `lootFilterVisible` and an `itemInstance` item struct,
// whose `itemType`, `itemInfoStruct["9"]` and `itemDefinitionStruct.o` are
// the sellability and price inputs; a monster (a drop call's `self`); and a
// coin (not a ground item). object_index answers as a typed reference with a
// flag bit above the kind, and `id` as a reference, as this runner answers
// them. The gold balance moves only through the injected credit, and stops at
// the offline cap. The credit answers `undefined` when it pays, as the game's
// PickUpGoldCheck does (static reading, Live 1), `false` on the hash check,
// and can be made to dispatch nothing or to pay nothing.
//
// Every call the class makes, to the runner or through a setter, is counted
// and kept in order, so a scenario can say both what happened and that
// nothing else did.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <vector>

enum { VALUE_REAL, VALUE_INT32, VALUE_INT64, VALUE_OBJECT, VALUE_REF, VALUE_STRING, VALUE_UNDEFINED, VALUE_BOOL };
static constexpr int kKindFlag = 0x10000000;   // a flag bit above the kind, as object_index carries it
struct RValue {
    int m_Kind = VALUE_UNDEFINED;
    double number = 0;
    std::string text;
    bool isStruct = false;   // VALUE_OBJECT: a struct (number is its key in the world) rather than an instance
    RValue() = default;
    RValue(double n) : m_Kind(VALUE_REAL), number(n) {}
    RValue(bool b) : m_Kind(VALUE_BOOL), number(b ? 1 : 0) {}
    RValue(const char* s) : m_Kind(VALUE_STRING), text(s) {}
    RValue(const std::string& s) : m_Kind(VALUE_STRING), text(s) {}
    double ToDouble() const {
        const int kind = m_Kind & 0x0FFFFFFF;
        if (kind == VALUE_STRING || kind == VALUE_UNDEFINED || kind == VALUE_OBJECT) throw std::runtime_error("not a number");
        return number;
    }
    bool ToBoolean() const { return number != 0; }
    std::string ToString() const { return text; }
};

// The SDK names the class resolves by (hs_game_sdk/objects.hpp).
namespace HeroSiege { namespace Objects {
enum class GameObject { Loot_Ground_obj };
inline std::string_view GetObjectName(GameObject) { return "Loot_Ground_obj"; }
} }

// ---- the controlled world -------------------------------------------------
enum Obj { LootGround = 50, Monster = 60, Coin = 70, Player = 80 };
struct Instance {
    int64_t id; int object; bool exists = true;
    bool hasVerdict = true; bool verdict = false;
    RValue item;   // itemInstance: a struct, or what a ground item without one holds
};
using Fields = std::map<std::string, RValue>;
struct World {
    std::vector<Instance> instances;
    std::map<int64_t, Fields> structs;           // struct key -> its fields
    std::map<std::string, long> byName;          // calls per builtin
    long calls = 0;                              // runner calls
    long game = 0;                               // calls through the setters
    std::vector<std::string> events;             // gold-read, credit, log, destroy, in order
    std::set<std::string> unexpected;            // builtins the class called that this world does not know
    double gold = 1000;
    double goldCap = 500000000;                  // the offline cap
    bool hasPlayer = true;
    bool refuseCredit = false;                   // the credit answers false and pays nothing (the hash check)
    bool creditDispatches = true;                // false: a script did not dispatch
    bool creditPays = true;                      // false: dispatched and answered, the balance did not move
    std::string creditAnswer = "undefined";      // what a dispatched, passing credit answers
    bool goldUnreadableAfterCredit = false;      // the balance read right after a credit fails
    bool refuseLog = false;
    bool keyFails = false;
    bool refuseDestroy = false;
    bool goldUnreadable = false;
    int64_t throwOnItemOf = -1;                  // variable_instance_get(itemInstance) throws for this id
    long credits = 0, logs = 0, goldReads = 0, playerReads = 0, roomReads = 0, keyReads = 0;
    double lastCredit = 0, lastLog = 0;
};
static World world;
static Instance* byId(int64_t id) { for (auto& i : world.instances) if (i.id == id && i.exists) return &i; return nullptr; }
static RValue Ref(int64_t id) { RValue v((double)id); v.m_Kind = VALUE_REF; return v; }
static RValue Plain(int64_t id) { return RValue((double)id); }
static RValue StructRef(int64_t key) { RValue v((double)key); v.m_Kind = VALUE_OBJECT; v.isStruct = true; return v; }
static int64_t idOf(const RValue& v) { return static_cast<int64_t>(v.number); }

struct Runner {
    RValue CallBuiltin(const char* key, std::vector<RValue> args) {
        ++world.calls;
        const std::string name(key);
        ++world.byName[name];
        if (name == "asset_get_index") {
            if (args[0].text == "Loot_Ground_obj") return RValue((double)LootGround);
            return RValue(-1.0);
        }
        if (name == "instance_exists") {
            if (args[0].isStruct) return RValue(0.0);
            return RValue(byId(idOf(args[0])) ? 1.0 : 0.0);
        }
        if (name == "variable_instance_exists") {
            if (args[0].isStruct) throw std::runtime_error("a struct is not an instance");
            Instance* i = byId(idOf(args[0]));
            if (!i) return RValue(0.0);
            if (args[1].text == "lootFilterVisible") return RValue(i->hasVerdict ? 1.0 : 0.0);
            throw std::runtime_error("unexpected exists " + args[1].text);
        }
        if (name == "variable_instance_get") {
            if (args[0].isStruct) throw std::runtime_error("a struct is not an instance");
            Instance* i = byId(idOf(args[0]));
            if (!i) return RValue();
            if (args[1].text == "object_index") { RValue v((double)i->object); v.m_Kind = VALUE_REF | kKindFlag; return v; }
            if (args[1].text == "id") return Ref(i->id);
            if (args[1].text == "lootFilterVisible") { if (!i->hasVerdict) return RValue(); return RValue(i->verdict); }
            if (args[1].text == "itemInstance") {
                if (i->id == world.throwOnItemOf) throw std::runtime_error("the read threw");
                return i->item;
            }
            throw std::runtime_error("unexpected field " + args[1].text);
        }
        if (name == "variable_struct_exists" || name == "variable_struct_get") {
            if (!args[0].isStruct) throw std::runtime_error("not a struct");
            auto s = world.structs.find(idOf(args[0]));
            if (s == world.structs.end()) throw std::runtime_error("no such struct");
            auto f = s->second.find(args[1].text);
            if (name == "variable_struct_exists") return RValue(f != s->second.end() ? 1.0 : 0.0);
            return f != s->second.end() ? f->second : RValue();
        }
        if (name == "instance_destroy") {
            world.events.push_back("destroy");
            Instance* i = byId(idOf(args[0]));
            if (i && !world.refuseDestroy) i->exists = false;
            return RValue();
        }
        world.unexpected.insert(name);
        throw std::runtime_error("unexpected builtin " + name);
    }
} runner;
static Runner* g_Yytk = &runner;

// PRODUCTION_AUTOSELL

using ForgePact::AutoSellMod;
static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}
static std::string n(long long v) { return std::to_string(v); }

// ---- the game-facing calls ModuleMain makes by name ----------------------------
static const int64_t kPlayerId = 7;
static bool LocalPlayer(RValue& player) {
    ++world.game; ++world.playerReads;
    if (!world.hasPlayer) return false;
    player = Ref(kPlayerId);
    return true;
}
static AutoSellMod::CreditAnswer Credit(const RValue& player, double price) {
    using A = AutoSellMod::CreditAnswer;
    ++world.game; ++world.credits;
    world.events.push_back("credit");
    world.lastCredit = price;
    if (!world.creditDispatches) return A::Failed;
    if (idOf(player) != kPlayerId || world.refuseCredit) return A::False;
    if (world.creditPays) world.gold = std::min(world.gold + price, world.goldCap);
    const std::string& a = world.creditAnswer;
    if (a == "true") return A::True;
    if (a == "false") return A::False;
    if (a == "number") return A::Number;
    if (a == "other") return A::Other;
    return A::Undefined;
}
static bool GoldLog(const RValue&, double price) {
    ++world.game; ++world.logs;
    world.events.push_back("log");
    world.lastLog = price;
    return !world.refuseLog;
}
static bool GoldRead(const RValue&, double& gold) {
    ++world.game; ++world.goldReads;
    const bool afterCredit = !world.events.empty() && world.events.back() == "credit";
    world.events.push_back("gold-read");
    if (world.goldUnreadable || (world.goldUnreadableAfterCredit && afterCredit)) return false;
    gold = world.gold;
    return true;
}
static bool KeyOf(const RValue& item, AutoSellMod::ItemKey& key) {
    ++world.game; ++world.keyReads;
    if (world.keyFails) return false;
    if ((item.m_Kind & 0x0FFFFFFF) != VALUE_OBJECT) return false;
    key = { 1, static_cast<uint64_t>(idOf(item)) };
    return true;
}
static AutoSellMod::Room g_Room{ true, false };
static AutoSellMod::Room RoomInfo() { ++world.game; ++world.roomReads; return g_Room; }

// ---- building the world ----------------------------------------------------------
static int64_t g_NextId = 100001;
static int64_t g_NextStruct = 1;
static const double kNoValue = std::numeric_limits<double>::quiet_NaN();
// An item struct: info value 9 (absent when NaN unless given as a string),
// the stack count `o` (absent when NaN), a display name, and the item's
// `itemType` (absent when NaN; 3 is a weapon).
static int64_t addStruct(double info9, double count = kNoValue, const std::string& name = "Rusty Sword",
                         double type = 3) {
    const int64_t key = g_NextStruct++;
    const int64_t info = g_NextStruct++;
    const int64_t def = g_NextStruct++;
    Fields infoFields;
    if (!std::isnan(info9)) infoFields["9"] = RValue(info9);
    infoFields["28"] = RValue(name);
    world.structs[info] = infoFields;
    Fields defFields;
    if (!std::isnan(count)) defFields["o"] = RValue(count);
    world.structs[def] = defFields;
    world.structs[key] = { { "itemInfoStruct", StructRef(info) }, { "itemDefinitionStruct", StructRef(def) } };
    if (!std::isnan(type)) world.structs[key]["itemType"] = RValue(type);
    return key;
}
static int64_t addGround(int64_t structKey, bool hidden = true, bool hasVerdict = true) {
    Instance i{ g_NextId++, LootGround };
    i.hasVerdict = hasVerdict;
    i.verdict = !hidden;
    i.item = structKey > 0 ? StructRef(structKey) : RValue(0.0);
    world.instances.push_back(i);
    return i.id;
}
static int64_t addMonster() { Instance i{ g_NextId++, Monster }; world.instances.push_back(i); return i.id; }
static int64_t addCoin() { Instance i{ g_NextId++, Coin }; world.instances.push_back(i); return i.id; }

static AutoSellMod& mod() { return AutoSellMod::Instance(); }
static int64_t g_RoomKey = 1;
static void tick() { mod().OnFrame(g_RoomKey); }
// A drop call, as the adapter hands it over: argument 0 and `self`, already
// reduced to durable handles inside the call.
static void note(const RValue& a0, const RValue& self) { mod().Note(a0, self); }
static int64_t g_Monster = 0;
static void drop(int64_t ground) { note(Ref(ground), Ref(g_Monster)); }
static bool on(int64_t id) { return byId(id) != nullptr; }
static long callsNow() { return world.calls + world.game; }
static long indexOf(const std::vector<std::string>& v, const std::string& what, size_t from = 0) {
    for (size_t k = from; k < v.size(); ++k) if (v[k] == what) return static_cast<long>(k);
    return -1;
}

int main() {
    g_Monster = addMonster();
    const int64_t coin = addCoin();

    // ---- Baseline: off, the shipped default ------------------------------------
    // A hidden, sellable drop is noted and the frame ends: the class asks the
    // runner nothing and calls none of the game-facing setters. The item stays,
    // no gold moves.
    {
        check("off/starts_off", !mod().Enabled());
        mod().SetLocalPlayer(&LocalPlayer);
        mod().SetCredit(&Credit);
        mod().SetGoldLog(&GoldLog);
        mod().SetGoldRead(&GoldRead);
        mod().SetItemKey(&KeyOf);
        mod().SetRoomInfo(&RoomInfo);
        const int64_t item = addGround(addStruct(37.4, 3));
        const double before = world.gold;
        for (int k = 0; k < 60; ++k) { drop(item); tick(); }
        check("off/no_runner_calls", world.calls == 0, "calls=" + n(world.calls));
        check("off/no_game_calls", world.game == 0, "game=" + n(world.game));
        check("off/drop_stays", on(item) && world.events.empty());
        check("off/no_gold", world.gold == before && world.credits == 0 && world.logs == 0 && world.goldReads == 0);
        check("off/notes_not_counted", mod().StatsRef().notes == 0 && mod().SoldThisFrame().empty());
        byId(item)->exists = false;
    }

    // ---- Target: on, a hidden drop is sold at the end of its frame --------------
    // info 9 is 37.4 and the stack 3: ceil(112.2) = 113 is credited once,
    // logged once, and the ground item is destroyed.
    int64_t soldId = 0;
    {
        mod().Enable();
        check("on/enable_no_calls", callsNow() == 0 && mod().Enabled(), "calls=" + n(callsNow()));
        const int64_t item = addGround(addStruct(37.4, 3));
        soldId = item;
        const double before = world.gold;
        drop(item);
        check("target/nothing_inside_the_call", callsNow() == 0 && on(item), "calls=" + n(callsNow()));
        tick();
        const auto& s = mod().StatsRef();
        check("target/credited_113_once", world.credits == 1 && world.lastCredit == 113 && world.gold == before + 113,
            "credits=" + n(world.credits) + " last=" + std::to_string(world.lastCredit));
        check("target/logged_once", world.logs == 1 && world.lastLog == 113, "logs=" + n(world.logs));
        check("target/destroyed", !on(item) && world.byName["instance_destroy"] == 1);
        check("target/counted", s.notes == 1 && s.sold == 1 && s.gold == 113 && s.exact == 1 && s.inexact == 0,
            "notes=" + n(s.notes) + " sold=" + n(s.sold) + " gold=" + std::to_string(s.gold) + " exact=" + n(s.exact));
        check("target/last", mod().LastText() == "113:1000->1113", mod().LastText());
        check("target/sold_ids", mod().SoldThisFrame().size() == 1 && mod().SoldThisFrame()[0] == item);
        tick();
        check("target/sold_ids_cleared_next_frame", mod().SoldThisFrame().empty());
        check("target/sold_once", world.credits == 1 && world.logs == 1);
    }

    // ---- The call order: gold read, credit, gold read, log, destroy -------------
    // The balance after is read right after the credit, since it decides
    // whether the player was paid; only a paid sale is logged.
    {
        world.events.clear();
        const int64_t item = addGround(addStruct(10));
        drop(item);
        tick();
        const auto& e = world.events;
        const long read = indexOf(e, "gold-read"), credit = indexOf(e, "credit"), log = indexOf(e, "log"),
                   after = indexOf(e, "gold-read", static_cast<size_t>(credit < 0 ? 0 : credit)), destroy = indexOf(e, "destroy");
        check("order/credit_log_destroy", read >= 0 && read < credit && credit < after && after < log && log < destroy,
            "read=" + n(read) + " credit=" + n(credit) + " log=" + n(log) + " after=" + n(after) + " destroy=" + n(destroy));
        check("order/one_of_each", e.size() == 5, "events=" + n(static_cast<long long>(e.size())));
    }

    // ---- No `o`: the count is 1 ---------------------------------------------------
    {
        const int64_t c0 = world.credits;
        int64_t noO = addGround(addStruct(12.2));
        drop(noO); tick();
        const bool plain = world.credits == c0 + 1 && world.lastCredit == 13;
        int64_t zero = addGround(addStruct(12.2, 0));
        drop(zero); tick();
        const bool zeroed = world.credits == c0 + 2 && world.lastCredit == 13;
        check("count/missing_is_one", plain, "last=" + std::to_string(world.lastCredit));
        check("count/non_positive_is_one", zeroed && !on(noO) && !on(zero));
    }

    // ---- A shown drop is never touched --------------------------------------------
    {
        const auto before = mod().StatsRef();
        const int64_t item = addGround(addStruct(50), false);
        world.events.clear();
        drop(item); tick();
        check("visible/never_touched", on(item) && world.events.empty() && mod().StatsRef().visible == before.visible + 1,
            "visible=" + n(mod().StatsRef().visible));
    }

    // ---- No verdict: counted, not touched ------------------------------------------
    {
        const auto before = mod().StatsRef();
        const int64_t item = addGround(addStruct(50), true, false);
        world.events.clear();
        drop(item); tick();
        check("nofilter/counted_not_touched", on(item) && world.events.empty() && mod().StatsRef().noFilterVar == before.noFilterVar + 1,
            "no-filter-var=" + n(mod().StatsRef().noFilterVar));
    }

    // ---- Not a ground item: a coin, a monster, nothing -----------------------------
    {
        const auto before = mod().StatsRef();
        world.events.clear();
        note(Ref(coin), Ref(g_Monster));
        note(RValue(), RValue());
        tick();
        check("unidentified/counted_not_touched", on(coin) && world.events.empty() && mod().StatsRef().unidentified == before.unidentified + 2,
            "unidentified=" + n(mod().StatsRef().unidentified));
    }

    // ---- An instance already gone by the frame's end ----------------------------------
    {
        const auto before = mod().StatsRef();
        const int64_t item = addGround(addStruct(50));
        const int64_t deadSelf = addMonster();
        note(Ref(item), Ref(deadSelf));
        byId(item)->exists = false;      // picked up, or gone with its room
        byId(deadSelf)->exists = false;  // the monster that dropped it, freed
        world.events.clear();
        tick();
        check("gone/counted_no_sale", world.events.empty() && mod().StatsRef().gone == before.gone + 1,
            "gone=" + n(mod().StatsRef().gone));
    }

    // ---- Unsellable: info 9 missing, 0, NaN, a string; no item struct ---------------
    {
        const auto before = mod().StatsRef();
        world.events.clear();
        std::vector<int64_t> items;
        items.push_back(addGround(addStruct(kNoValue)));
        items.push_back(addGround(addStruct(0)));
        const int64_t nanStruct = addStruct(1);
        world.structs[idOf(world.structs[nanStruct]["itemInfoStruct"])]["9"] = RValue(kNoValue);
        items.push_back(addGround(nanStruct));
        const int64_t textStruct = addStruct(1);
        world.structs[idOf(world.structs[textStruct]["itemInfoStruct"])]["9"] = RValue("37");
        items.push_back(addGround(textStruct));
        items.push_back(addGround(0));   // a ground item with no item struct
        for (int64_t id : items) drop(id);
        tick();
        bool all = true;
        for (int64_t id : items) all = all && on(id);
        check("unsellable/left_on_the_ground", all && indexOf(world.events, "credit") < 0 && indexOf(world.events, "destroy") < 0);
        check("unsellable/counted", mod().StatsRef().unsellable == before.unsellable + 5,
            "unsellable=" + n(mod().StatsRef().unsellable - before.unsellable));
    }

    // ---- No local player: no sale --------------------------------------------------
    {
        const auto before = mod().StatsRef();
        world.hasPlayer = false;
        world.events.clear();
        const int64_t item = addGround(addStruct(50));
        drop(item); tick();
        world.hasPlayer = true;
        check("noplayer/no_sale", on(item) && world.events.empty() && mod().StatsRef().noPlayer == before.noPlayer + 1,
            "no-player=" + n(mod().StatsRef().noPlayer));
    }

    // ---- The credit answered false: the item stays, nothing logged, not remembered ---
    // The hash check's answer (static reading): nothing paid, the cause is
    // credit-false, and credit-refused counts it too.
    {
        const auto before = mod().StatsRef();
        world.refuseCredit = true;
        world.events.clear();
        const int64_t item = addGround(addStruct(50));
        drop(item); tick();
        world.refuseCredit = false;
        const auto& s = mod().StatsRef();
        check("refused/item_left", on(item) && indexOf(world.events, "log") < 0 && indexOf(world.events, "destroy") < 0);
        check("refused/counted", s.creditRefused == before.creditRefused + 1 && s.sold == before.sold
            && s.creditFalse == before.creditFalse + 1 && s.creditCallFailed == before.creditCallFailed
            && s.creditUnpaid == before.creditUnpaid,
            "credit-refused=" + n(s.creditRefused) + " credit-false=" + n(s.creditFalse));
        const std::string g = AutoSellMod::Num(world.gold);
        check("refused/last_false", mod().StatFields().find(" last=50:" + g + "->" + g + ":false ") != std::string::npos,
            mod().StatFields());
        // A later drop of the same item, once the credit takes, is sold: a
        // refusal does not mark the struct credited.
        drop(item); tick();
        check("refused/not_remembered", !on(item) && mod().StatsRef().sold == before.sold + 1 && mod().StatsRef().repeatHeld == before.repeatHeld);
    }

    // ---- Replan 1: a credit that answers undefined and pays is a sale ----------------
    // Live 1's failure: the game's credit answers undefined on every path but
    // the hash check (static reading), and the old rule counted that refused
    // while the player was paid. The balance either side decides now.
    {
        const auto before = mod().StatsRef();
        world.creditAnswer = "undefined";
        const double gold = world.gold;
        const int64_t item = addGround(addStruct(40));
        drop(item); tick();
        const auto& s = mod().StatsRef();
        check("credit/undefined_pays", !on(item) && world.gold == gold + 40 && s.sold == before.sold + 1
            && s.exact == before.exact + 1 && s.creditRefused == before.creditRefused,
            "sold=" + n(s.sold - before.sold) + " exact=" + n(s.exact - before.exact)
            + " credit-refused=" + n(s.creditRefused - before.creditRefused));
        const std::string line = mod().StatFields();
        check("credit/undefined_last", line.find(" last=40:" + AutoSellMod::Num(gold) + "->" + AutoSellMod::Num(gold + 40)
            + ":undefined ") != std::string::npos, line);
    }

    // ---- The answer alone never decides: true without payment is unpaid ---------------
    {
        const auto before = mod().StatsRef();
        world.creditAnswer = "true";
        world.creditPays = false;
        world.events.clear();
        const int64_t item = addGround(addStruct(40));
        drop(item); tick();
        world.creditPays = true;
        const auto& s = mod().StatsRef();
        check("credit/true_without_payment_unpaid", on(item) && s.creditUnpaid == before.creditUnpaid + 1
            && s.creditRefused == before.creditRefused + 1 && s.sold == before.sold
            && indexOf(world.events, "log") < 0 && indexOf(world.events, "destroy") < 0,
            "credit-unpaid=" + n(s.creditUnpaid - before.creditUnpaid));
        // Undefined without payment: unpaid as well, and the item is not
        // remembered, so once the credit pays it is sold.
        world.creditAnswer = "undefined";
        world.creditPays = false;
        const int64_t other = addGround(addStruct(40));
        drop(other); tick();
        world.creditPays = true;
        check("credit/unpaid_item_left", on(other) && mod().StatsRef().creditUnpaid == before.creditUnpaid + 2
            && mod().StatFields().find(":undefined ") != std::string::npos, mod().StatFields());
        drop(other); tick();
        check("credit/unpaid_not_remembered", !on(other) && mod().StatsRef().sold == before.sold + 1
            && mod().StatsRef().repeatHeld == before.repeatHeld);
        byId(item)->exists = false;
    }

    // ---- A script that does not dispatch: credit-call-failed, item left ---------------
    {
        const auto before = mod().StatsRef();
        world.creditDispatches = false;
        world.events.clear();
        const int64_t item = addGround(addStruct(40));
        drop(item); tick();
        world.creditDispatches = true;
        const auto& s = mod().StatsRef();
        check("credit/call_failed", on(item) && s.creditCallFailed == before.creditCallFailed + 1
            && s.creditRefused == before.creditRefused + 1 && s.creditFalse == before.creditFalse
            && indexOf(world.events, "log") < 0 && indexOf(world.events, "destroy") < 0,
            "credit-call-failed=" + n(s.creditCallFailed - before.creditCallFailed));
        check("credit/call_failed_last", mod().StatFields().find(":failed ") != std::string::npos, mod().StatFields());
        byId(item)->exists = false;
    }

    // ---- The balance unreadable before: no credit call, the item left ------------------
    {
        const auto before = mod().StatsRef();
        const auto last = mod().LastText();
        world.goldUnreadable = true;
        world.events.clear();
        const long credits = world.credits;
        const int64_t item = addGround(addStruct(40));
        drop(item); tick();
        world.goldUnreadable = false;
        const auto& s = mod().StatsRef();
        check("credit/gold_unreadable_before", on(item) && world.credits == credits && s.goldUnreadable == before.goldUnreadable + 1
            && s.sold == before.sold && s.creditRefused == before.creditRefused && world.events.size() == 1,
            "gold-unreadable=" + n(s.goldUnreadable - before.goldUnreadable) + " events=" + n((long long)world.events.size()));
        check("credit/gold_unreadable_no_last", mod().LastText() == last, mod().LastText());
        byId(item)->exists = false;
    }

    // ---- Unreadable after a false answer or a failed dispatch: not paid ----------------
    {
        const auto before = mod().StatsRef();
        world.goldUnreadableAfterCredit = true;
        world.refuseCredit = true;
        const int64_t a = addGround(addStruct(40));
        drop(a); tick();
        world.refuseCredit = false;
        world.creditDispatches = false;
        const int64_t b = addGround(addStruct(40));
        drop(b); tick();
        world.creditDispatches = true;
        world.goldUnreadableAfterCredit = false;
        const auto& s = mod().StatsRef();
        check("credit/unverified_needs_dispatch_and_not_false", on(a) && on(b) && s.creditFalse == before.creditFalse + 1
            && s.creditCallFailed == before.creditCallFailed + 1 && s.creditUnverified == before.creditUnverified
            && s.sold == before.sold);
        byId(a)->exists = false;
        byId(b)->exists = false;
    }

    // ---- The gold log refused: counted on the stat line, the sale stands ---------------
    {
        const auto before = mod().StatsRef();
        world.refuseLog = true;
        const int64_t item = addGround(addStruct(40));
        drop(item); tick();
        world.refuseLog = false;
        const auto& s = mod().StatsRef();
        check("log/refused_sale_stands", !on(item) && s.logRefused == before.logRefused + 1 && s.sold == before.sold + 1
            && mod().StatFields().find(" log-refused=" + n((long long)s.logRefused) + " ") != std::string::npos, mod().StatFields());
    }

    // ---- The key lookup fails: no-key, not an error -----------------------------------
    {
        const auto before = mod().StatsRef();
        world.keyFails = true;
        world.events.clear();
        const int64_t item = addGround(addStruct(40));
        drop(item); tick();
        world.keyFails = false;
        const auto& s = mod().StatsRef();
        check("nokey/counted_not_error", on(item) && world.events.empty() && s.noKey == before.noKey + 1 && s.errors == before.errors,
            "no-key=" + n(s.noKey - before.noKey) + " errors=" + n(s.errors - before.errors));
        byId(item)->exists = false;
    }

    // ---- The types the game will not buy are never sold --------------------------------
    // Key 12, Tarot 13, Material 14, Socketable 15, Relic 16 (the tooltip's
    // "Item cannot be sold" rule, static reading), and an item with no
    // readable itemType: each unsellable and type-unsellable, no credit and
    // no balance read, left on the ground. A ring (7) beside them sells.
    {
        const auto before = mod().StatsRef();
        const long credits = world.credits, reads = world.goldReads;
        std::vector<int64_t> refused;
        for (double t : { 12.0, 13.0, 14.0, 15.0, 16.0 }) refused.push_back(addGround(addStruct(1350, kNoValue, "Destiny Shard", t)));
        refused.push_back(addGround(addStruct(692, kNoValue, "No Type", kNoValue)));
        const int64_t textType = addStruct(692, kNoValue, "Text Type");
        world.structs[textType]["itemType"] = RValue("15");
        refused.push_back(addGround(textType));
        const int64_t ring = addGround(addStruct(57, kNoValue, "Ring", 7));
        for (int64_t id : refused) drop(id);
        drop(ring);
        tick();
        const auto& s = mod().StatsRef();
        bool left = true;
        for (int64_t id : refused) left = left && on(id);
        check("types/left_on_the_ground", left);
        check("types/counted", s.unsellable == before.unsellable + 7 && s.typeUnsellable == before.typeUnsellable + 7,
            "unsellable=" + n(s.unsellable - before.unsellable) + " type-unsellable=" + n(s.typeUnsellable - before.typeUnsellable));
        check("types/no_credit_no_balance", world.credits == credits + 1 && world.goldReads == reads + 2,
            "credits=" + n(world.credits - credits) + " gold-reads=" + n(world.goldReads - reads));
        check("types/ring_sells", !on(ring) && s.sold == before.sold + 1 && world.lastCredit == 57);
        bool quoted = true;
        std::string seen;
        for (int t = 12; t <= 16; ++t) {
            std::string fields, why;
            const bool ok = mod().QuoteFields(StructRef(addStruct(1350, kNoValue, "Destiny Shard", t)), fields, why);
            quoted = quoted && !ok && fields.empty() && why == "item type " + std::to_string(t) + " is not sold by the game";
            seen += why + "; ";
        }
        check("quote/refuses_unsellable_types", quoted, seen);
        std::string fields, why;
        const bool untyped = !mod().QuoteFields(StructRef(addStruct(692, kNoValue, "No Type", kNoValue)), fields, why);
        check("quote/refuses_unreadable_type", untyped && fields.empty() && why.find("item type") != std::string::npos, why);
        fields.clear(); why.clear();
        const bool priced = mod().QuoteFields(StructRef(addStruct(57, kNoValue, "Ring", 7)), fields, why);
        check("quote/ring_type", priced && fields == "name=\"Ring\" type=7 info9=57 count=1 price=57", fields + " " + why);
        for (int64_t id : refused) byId(id)->exists = false;
    }

    // ---- Inexact: the balance stops at the offline cap -----------------------------
    {
        const auto before = mod().StatsRef();
        world.gold = world.goldCap - 40;
        const int64_t item = addGround(addStruct(100));
        drop(item); tick();
        check("inexact/counted", !on(item) && mod().StatsRef().inexact == before.inexact + 1 && mod().StatsRef().exact == before.exact
            && mod().StatsRef().sold == before.sold + 1, "inexact=" + n(mod().StatsRef().inexact));
        check("inexact/last", mod().LastText() == "100:499999960->500000000", mod().LastText());
        // A balance unreadable right after a credit that dispatched and did
        // not answer false cannot show the credit was exact; the sale itself
        // still happens, counted unverified. (Unreadable before the credit is
        // no sale at all: credit/gold_unreadable_before, below.)
        world.gold = 1000;
        world.goldUnreadableAfterCredit = true;
        const int64_t other = addGround(addStruct(100));
        drop(other); tick();
        world.goldUnreadableAfterCredit = false;
        check("inexact/unreadable_gold", !on(other) && mod().StatsRef().inexact == before.inexact + 2
            && mod().StatsRef().creditUnverified == before.creditUnverified + 1
            && mod().LastText() == "100:1000->unreadable", mod().LastText());
    }

    // ---- Destroy failed: counted, not counted sold, never credited again -------------
    {
        const auto before = mod().StatsRef();
        world.refuseDestroy = true;
        const int64_t item = addGround(addStruct(50));
        const long credits = world.credits;
        drop(item); tick();
        world.refuseDestroy = false;
        check("destroy/failed_counted", on(item) && mod().StatsRef().destroyFailed == before.destroyFailed + 1
            && mod().StatsRef().sold == before.sold && mod().StatsRef().gold == before.gold && mod().SoldThisFrame().empty(),
            "destroy-failed=" + n(mod().StatsRef().destroyFailed));
        drop(item); tick();
        check("destroy/failed_not_credited_twice", world.credits == credits + 1 && mod().StatsRef().repeatHeld == before.repeatHeld + 1);
    }

    // ---- The same item struct noted again in the room: one credit --------------------
    // The Chaos Tower case: the floor's end places the same struct on the
    // ground again, as a new instance.
    {
        const auto before = mod().StatsRef();
        const int64_t shared = addStruct(20, 2);
        const int64_t first = addGround(shared);
        drop(first); tick();
        const int64_t again = addGround(shared);
        const long credits = world.credits;
        drop(again); tick();
        check("repeat/held_one_credit", world.credits == credits && on(again)
            && mod().StatsRef().repeatHeld == before.repeatHeld + 1 && mod().StatsRef().sold == before.sold + 1,
            "repeat-held=" + n(mod().StatsRef().repeatHeld - before.repeatHeld) + " sold=" + n(mod().StatsRef().sold - before.sold));
        byId(again)->exists = false;
    }

    // ---- Persistent or unreadable room: nothing sold, the room asked once ------------
    // Each room's first frame passes before anything drops in it: a note
    // pending across a room change is forgotten (below), not decided.
    {
        const auto before = mod().StatsRef();
        g_Room = { true, true };
        g_RoomKey = 2;
        tick();
        const long roomReads = world.roomReads;
        const int64_t a = addGround(addStruct(50)), b = addGround(addStruct(50));
        world.events.clear();
        drop(a); tick();
        drop(b); tick();
        check("persistent/nothing_sold", on(a) && on(b) && world.events.empty()
            && mod().StatsRef().skippedPersistent == before.skippedPersistent + 2, "skipped-persistent="
            + n(mod().StatsRef().skippedPersistent - before.skippedPersistent));
        check("persistent/room_asked_once", world.roomReads == roomReads + 1, "room-reads=" + n(world.roomReads - roomReads));
        g_Room = { false, false };
        g_RoomKey = 3;
        tick();
        drop(a); tick();
        check("persistent/unreadable_room_sells_nothing", on(a) && mod().StatsRef().skippedPersistent == before.skippedPersistent + 3);
        g_Room = { true, false };
        g_RoomKey = 4;
        tick();
        byId(a)->exists = false;
        byId(b)->exists = false;
    }

    // ---- Room change: pending notes and remembered keys forgotten, no call -----------
    {
        const auto before = mod().StatsRef();
        const int64_t shared = addStruct(30);
        const int64_t first = addGround(shared);
        drop(first); tick();                     // sold in room 4, its struct remembered
        const int64_t stale = addGround(addStruct(30));
        drop(stale);                             // noted, then the room changes
        g_RoomKey = 5;
        const long calls = callsNow();
        tick();
        check("room/forgets_pending_without_calls", callsNow() == calls && on(stale), "calls=" + n(callsNow() - calls));
        const int64_t again = addGround(shared);
        drop(again); tick();
        check("room/forgets_credited_keys", !on(again) && mod().StatsRef().sold == before.sold + 2
            && mod().StatsRef().repeatHeld == before.repeatHeld);
        byId(stale)->exists = false;
    }

    // ---- An exception inside one item does not stop the next -------------------------
    {
        const auto before = mod().StatsRef();
        const int64_t bad = addGround(addStruct(50));
        const int64_t good = addGround(addStruct(50));
        world.throwOnItemOf = bad;
        drop(bad); drop(good); tick();
        world.throwOnItemOf = -1;
        check("errors/one_item_isolated", on(bad) && !on(good) && mod().StatsRef().errors == before.errors + 1
            && mod().StatsRef().sold == before.sold + 1, "errors=" + n(mod().StatsRef().errors - before.errors));
        byId(bad)->exists = false;
    }

    // ---- The same instance noted twice in a frame is decided once --------------------
    {
        const long credits = world.credits;
        const int64_t item = addGround(addStruct(50));
        note(Ref(item), Ref(g_Monster));
        note(Plain(item), RValue());
        tick();
        check("drop/same_instance_once", !on(item) && world.credits == credits + 1);
    }

    // ---- The price function, as `autosell quote` uses it -----------------------------
    {
        std::string fields, why;
        const bool ok = mod().QuoteFields(StructRef(addStruct(37.4, 3, "Rusty Sword")), fields, why);
        check("quote/fields", ok && fields == "name=\"Rusty Sword\" type=3 info9=37.4 count=3 price=113", fields + " " + why);
        fields.clear(); why.clear();
        const bool refused = !mod().QuoteFields(StructRef(addStruct(kNoValue)), fields, why);
        check("quote/refused_with_reason", refused && !why.empty() && fields.empty(), why);
        AutoSellMod::PriceParts parts;
        check("price/ceil_of_product", mod().PriceOf(StructRef(addStruct(0.1, 3)), parts) && parts.price == 1 && parts.count == 3);
    }

    // ---- The stat line names every contract field, in order ---------------------------
    {
        const std::string line = mod().StatFields();
        const char* fields[] = { "on=1", "route=not-installed", "notes=", "sold=", "gold=", "exact=", "inexact=",
            "visible=", "no-filter-var=", "unsellable=", "type-unsellable=", "skipped-persistent=", "repeat-held=",
            "no-player=", "gold-unreadable=", "credit-refused=", "credit-call-failed=", "credit-false=", "credit-unpaid=",
            "credit-unverified=", "log-refused=", "destroy-failed=", "gone=", "unidentified=", "no-key=", "errors=",
            "last=", "gold-now=" };
        size_t at = 0;
        bool ordered = true;
        std::string missing;
        for (const char* f : fields) {
            const size_t p = line.find(std::string(f), at);
            if (p == std::string::npos || (p > 0 && line[p - 1] != ' ')) { ordered = false; missing += std::string(f) + " "; continue; }
            at = p;
        }
        check("stat/fields_in_order", ordered && line.rfind("on=", 0) == 0, missing + "| " + line);
        // Exactly these fields, none beside them.
        size_t count = 0;
        for (size_t p = 0; (p = line.find('=', p)) != std::string::npos; ++p) ++count;
        check("stat/no_other_fields", count == sizeof(fields) / sizeof(fields[0]), "fields=" + n((long long)count) + " | " + line);
        const auto& s = mod().StatsRef();
        check("stat/credit_refused_is_the_sum", s.creditRefused == s.creditCallFailed + s.creditFalse + s.creditUnpaid
            && s.creditCallFailed > 0 && s.creditFalse > 0 && s.creditUnpaid > 0,
            "credit-refused=" + n(s.creditRefused) + " call-failed=" + n(s.creditCallFailed) + " false=" + n(s.creditFalse)
            + " unpaid=" + n(s.creditUnpaid));
        check("stat/type_unsellable_within_unsellable", s.typeUnsellable > 0 && s.typeUnsellable <= s.unsellable);
        check("stat/gold_now_read", line.find("gold-now=" + std::to_string(static_cast<long long>(world.gold))) != std::string::npos, line);
        mod().SetRoute(AutoSellMod::Route::Both);
        check("stat/route_both", mod().StatFields().find(" route=both ") != std::string::npos);
        mod().SetRoute(AutoSellMod::Route::TableOnly);
        check("stat/route_table_only", std::string(mod().RouteName()) == "table-only");
        mod().SetRoute(AutoSellMod::Route::None);
        check("stat/route_none", std::string(mod().RouteName()) == "none");
        world.hasPlayer = false;
        check("stat/gold_now_unreadable", mod().StatFields().find("gold-now=unreadable") != std::string::npos);
        world.hasPlayer = true;
    }

    // ---- Off again: notes dropped, nothing asked ---------------------------------------
    {
        const int64_t item = addGround(addStruct(50));
        drop(item);
        mod().Disable();
        const long calls = callsNow();
        const auto notes = mod().StatsRef().notes;
        for (int k = 0; k < 10; ++k) { drop(item); tick(); }
        check("off/disable_drops_pending_and_asks_nothing", !mod().Enabled() && on(item) && callsNow() == calls
            && mod().StatsRef().notes == notes, "calls=" + n(callsNow() - calls));
    }

    check("calls/only_listed_builtins", world.unexpected.empty(), [] {
        std::string s; for (const auto& u : world.unexpected) s += u + " "; return s; }());
    check("sold/first_target_item_gone", !on(soldId));

    std::cout << (failures ? "RESULT FAIL " + n(failures) : std::string("RESULT OK")) << std::endl;
    return failures ? 1 : 0;
}
