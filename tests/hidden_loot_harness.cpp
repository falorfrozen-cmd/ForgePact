// Behavioral regression harness for hidden loot sleep (HiddenLootMod.hpp).
//
// The Python runner injects the REAL ForgePact::HiddenLootMod class below.
// Only the game API, the key state and the foreground test are replaced; no
// game process is touched. The controlled world holds ground items
// (Loot_Ground_obj) carrying the game's filter verdict `lootFilterVisible`, a
// monster (a drop call's `self`), and item structs (a drop call's second
// argument). Instances are active or asleep exactly as the runner keeps them:
// a sleeping instance is invisible to instance_number, instance_find,
// instance_exists and variable_instance_get. object_index answers as a typed
// reference with a flag bit above the kind, and `id` as a reference, as this
// runner answers them.
//
// A drop call's `self` and its instance-pointer arguments live only as long
// as their instances: once one is gone, any builtin handed the pointer would
// read freed memory in the real runtime. This runner counts every such call
// in `danglingPointerCalls` (a positive control proves it fires), and the
// class must leave it at 0 for the whole run.
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
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
    bool isStruct = false;   // VALUE_OBJECT: an item struct rather than an instance
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
enum Obj { LootGround = 50, Monster = 60, Coin = 70 };
struct Instance {
    int64_t id; int object; bool active = true; bool exists = true;
    bool hasVerdict = true; bool verdict = false; bool visible = false;
};
struct World {
    std::vector<Instance> instances;
    std::map<std::string, long> byName;          // calls per builtin
    long calls = 0;
    long callsOnMissing = 0;                     // activate/deactivate/set on an instance that is not there
    long readsOnMissing = 0;                     // variable_instance_get on an instance that is not there
    long danglingPointerCalls = 0;               // a builtin handed an instance pointer whose instance is gone
    std::set<std::string> unexpected;            // builtins the class called that this world does not know
    bool throwOnDeactivate = false;
    bool refuseVisibleWrite = false;
    std::vector<std::pair<int64_t, std::string>> touched;   // (id, builtin) for every call that changes an instance
};
static World world;
static Instance* byId(int64_t id) { for (auto& i : world.instances) if (i.id == id && i.exists) return &i; return nullptr; }
static Instance* anyId(int64_t id) { for (auto& i : world.instances) if (i.id == id) return &i; return nullptr; }
static RValue Ref(int64_t id) { RValue v((double)id); v.m_Kind = VALUE_REF; return v; }
static RValue Plain(int64_t id) { return RValue((double)id); }
static RValue InstancePointer(int64_t id) { RValue v((double)id); v.m_Kind = VALUE_OBJECT; return v; }   // a CInstance* as RValue(S)
static RValue ItemStruct() { RValue v(0.0); v.m_Kind = VALUE_OBJECT; v.isStruct = true; return v; }
static int64_t idOf(const RValue& v) { return static_cast<int64_t>(v.number); }

struct Runner {
    RValue CallBuiltin(const char* key, std::vector<RValue> args) {
        ++world.calls;
        const std::string name(key);
        ++world.byName[name];
        for (const RValue& arg : args) {
            if ((arg.m_Kind & 0x0FFFFFFF) != VALUE_OBJECT || arg.isStruct) continue;
            const Instance* i = anyId(idOf(arg));
            if (!i || !i->exists) ++world.danglingPointerCalls;
        }
        if (name == "asset_get_index") {
            if (args[0].text == "Loot_Ground_obj") return RValue((double)LootGround);
            return RValue(-1.0);
        }
        if (name == "instance_number") {
            long n = 0;
            for (const auto& i : world.instances) if (i.exists && i.active && i.object == (int)args[0].number) ++n;
            return RValue((double)n);
        }
        if (name == "instance_find") {
            int remaining = (int)args[1].number;
            for (const auto& i : world.instances)
                if (i.exists && i.active && i.object == (int)args[0].number && remaining-- == 0) return Ref(i.id);
            return RValue(-4.0);
        }
        if (name == "instance_exists") {
            if (args[0].isStruct) return RValue(0.0);
            Instance* i = byId(idOf(args[0]));
            return RValue(i && i->active ? 1.0 : 0.0);
        }
        if (name == "variable_instance_exists") {
            if (args[0].isStruct) throw std::runtime_error("a struct is not an instance");
            Instance* i = byId(idOf(args[0]));
            if (!i || !i->active) return RValue(0.0);
            return RValue(args[1].text == "lootFilterVisible" && i->hasVerdict ? 1.0 : 0.0);
        }
        if (name == "variable_instance_get") {
            if (args[0].isStruct) throw std::runtime_error("a struct is not an instance");
            Instance* i = byId(idOf(args[0]));
            if (!i) ++world.readsOnMissing;
            if (!i || !i->active) return RValue();   // undefined, as the runner answers for a sleeping or gone instance
            if (args[1].text == "object_index") { RValue v((double)i->object); v.m_Kind = VALUE_REF | kKindFlag; return v; }
            if (args[1].text == "id") return Ref(i->id);
            if (args[1].text == "lootFilterVisible") { if (!i->hasVerdict) return RValue(); return RValue(i->verdict); }
            if (args[1].text == "visible") return RValue(i->visible);
            throw std::runtime_error("unexpected field " + args[1].text);
        }
        if (name == "variable_instance_set") {
            Instance* i = byId(idOf(args[0]));
            if (!i || !i->active) { ++world.callsOnMissing; throw std::runtime_error("no such instance"); }
            world.touched.push_back({ i->id, name });
            if (args[1].text == "lootFilterVisible") { i->verdict = args[2].ToBoolean(); i->hasVerdict = true; return RValue(); }
            if (args[1].text == "visible") {
                if (world.refuseVisibleWrite) throw std::runtime_error("built-in refused");
                i->visible = args[2].ToBoolean();
                return RValue();
            }
            throw std::runtime_error("unexpected write " + args[1].text);
        }
        if (name == "instance_deactivate_object") {
            if (world.throwOnDeactivate) throw std::runtime_error("refused");
            Instance* i = byId(idOf(args[0]));
            if (!i) { ++world.callsOnMissing; return RValue(); }
            world.touched.push_back({ i->id, name });
            i->active = false;
            return RValue();
        }
        if (name == "instance_activate_object") {
            Instance* i = byId(idOf(args[0]));
            if (!i) { ++world.callsOnMissing; return RValue(); }
            world.touched.push_back({ i->id, name });
            i->active = true;
            return RValue();
        }
        world.unexpected.insert(name);
        throw std::runtime_error("unexpected builtin " + name);
    }
} runner;
static Runner* g_Yytk = &runner;

// PRODUCTION_HIDDENLOOT

using ForgePact::HiddenLootMod;
static int failures = 0;
static void check(const std::string& label, bool ok, const std::string& detail = "") {
    std::cout << (ok ? "PASS " : "FAIL ") << label << (detail.empty() ? "" : " " + detail) << std::endl;
    if (!ok) ++failures;
}
static std::string n(long long v) { return std::to_string(v); }

// ---- the key and the window --------------------------------------------------
static bool g_KeyHeld = false;       // the physical key
static int g_HeldVk = 164;           // which key is physically down
static bool g_InFront = true;        // the game's window is the foreground window
static long g_KeyReads = 0, g_FrontReads = 0;
static bool KeyDown(int vk) { ++g_KeyReads; return g_KeyHeld && vk == g_HeldVk; }
static bool GameInFront() { ++g_FrontReads; return g_InFront; }

// ---- rooms ---------------------------------------------------------------------
struct Room { std::string name; bool persistent = false; bool readable = true; };
static Room zone() { return { "Act_01_01", false, true }; }
static Room persistentRoom() { return { "Act_09_01", true, true }; }

static int64_t g_NextId = 100001;
static int64_t addItem(bool hiddenVerdict, bool hasVerdict = true) {
    Instance i{ g_NextId++, LootGround };
    i.hasVerdict = hasVerdict;
    i.verdict = !hiddenVerdict;
    i.visible = !hiddenVerdict;
    world.instances.push_back(i);
    return i.id;
}
static int64_t addMonster() { Instance i{ g_NextId++, Monster }; world.instances.push_back(i); return i.id; }
static int64_t addCoin() { Instance i{ g_NextId++, Coin }; world.instances.push_back(i); return i.id; }

static HiddenLootMod& mod() { return HiddenLootMod::Instance(); }
static uint64_t frame = 1000;
static void tick(int frames, int64_t room, Room (*probe)() = zone) {
    for (int k = 0; k < frames; ++k) mod().OnFrame(frame++, room, probe);
}
// A drop: the game made the item and ran its filter, then our hook hands the
// class what the call carried, still inside the call, where the class reduces
// it to durable handles.
static void init(const RValue& a0, const RValue& a1, const RValue& self) { mod().OnInit(a0, a1, self); }
static bool touchedBy(int64_t id, const std::string& builtin = "") {
    for (const auto& t : world.touched) if (t.first == id && (builtin.empty() || t.second == builtin)) return true;
    return false;
}
static long countAsleep() { long c = 0; for (const auto& i : world.instances) if (i.exists && !i.active) ++c; return c; }

int main() {
    const int64_t monster = addMonster();
    addCoin();

    // ---- Baseline: off, the shipped default ------------------------------------
    // A hidden drop's init and every tick ask the game nothing, the drop stays
    // awake, and a held key is not even read.
    {
        check("off/starts_off", !mod().Enabled());
        check("key/default_left_alt", mod().Key() == 164 && HiddenLootMod::kDefaultKey == 164, "key=" + n(mod().Key()));
        mod().SetInput(&KeyDown, &GameInFront);
        const int64_t item = addItem(true);
        g_KeyHeld = true;
        init(Ref(item), ItemStruct(), InstancePointer(monster));
        tick(60, 1);
        check("off/init_and_tick_no_calls", world.calls == 0, "calls=" + n(world.calls));
        check("off/key_not_read", g_KeyReads == 0 && g_FrontReads == 0, "keyReads=" + n(g_KeyReads) + " frontReads=" + n(g_FrontReads));
        check("off/drop_stays_awake", byId(item)->active && !touchedBy(item));
        check("off/inits_not_counted", mod().StatsRef().inits == 0);
        g_KeyHeld = false;
        byId(item)->exists = false;
    }

    // ---- Switching on sleeps what the filter already hides on the ground ------
    {
        std::vector<int64_t> hidden, visible;
        for (int k = 0; k < 5; ++k) hidden.push_back(addItem(true));
        for (int k = 0; k < 3; ++k) visible.push_back(addItem(false));
        const int64_t noVar = addItem(true, false);
        const auto walk = mod().Enable(1, zone);
        bool allHiddenAsleep = true, visibleAwake = true;
        for (int64_t id : hidden) allHiddenAsleep = allHiddenAsleep && !anyId(id)->active;
        for (int64_t id : visible) visibleAwake = visibleAwake && anyId(id)->active && !touchedBy(id);
        check("on/walk_sleeps_hidden_ground", mod().Enabled() && walk.slept == 5 && allHiddenAsleep && mod().AsleepNow() == 5,
            "walk-slept=" + n(walk.slept) + " asleep-now=" + n(mod().AsleepNow()));
        check("on/walk_leaves_visible", walk.visible == 3 && walk.noFilterVar == 1 && visibleAwake && anyId(noVar)->active && !touchedBy(noVar),
            "walk-visible=" + n(walk.visible) + " walk-no-filter-var=" + n(walk.noFilterVar));
        check("on/slept_counted", mod().StatsRef().slept == 5, "slept=" + n(mod().StatsRef().slept));
        // Both routes in place: no walk after the switch-on one.
        mod().SetRoute(HiddenLootMod::Route::Both);
        const long finds = world.byName["instance_find"];
        tick(100, 1);
        check("pass/never_with_both_routes", world.byName["instance_find"] == finds && mod().StatsRef().passes == 0,
            "finds=" + n(world.byName["instance_find"] - finds) + " passes=" + n(mod().StatsRef().passes));
    }

    // ---- A hidden drop sleeps at the end of its frame --------------------------
    {
        // The instrument first: handed an instance pointer whose instance is
        // gone, the runner counts it. Then back to 0 for the class's run.
        const int64_t corpse = addMonster();
        byId(corpse)->exists = false;
        runner.CallBuiltin("instance_exists", { InstancePointer(corpse) });
        check("drop/dangling_control_fires", world.danglingPointerCalls == 1, "dangling=" + n(world.danglingPointerCalls));
        world.danglingPointerCalls = 0;

        const int64_t item = addItem(true);
        const HiddenLootMod::Stats st0 = mod().StatsRef();
        const bool noKindsYet = std::string(st0.kinds[0]) == "-" && std::string(st0.kinds[1]) == "-" && std::string(st0.kinds[2]) == "-";
        const long before = world.calls;
        const long existsBefore = world.byName["instance_exists"], getsBefore = world.byName["variable_instance_get"];
        const size_t touchesBefore = world.touched.size();
        const long deactBefore = world.byName["instance_deactivate_object"];
        init(Ref(item), ItemStruct(), InstancePointer(monster));
        // Inside the call: nothing deactivated or written, the item awake.
        check("drop/no_deactivate_or_write_inside_init", world.touched.size() == touchesBefore && byId(item)->active
            && world.byName["instance_deactivate_object"] == deactBefore,
            "touched=" + n((long long)(world.touched.size() - touchesBefore)));
        // Reads only: the reference is kept with no read, the item struct is
        // asked instance_exists (false), the monster's pointer is asked
        // instance_exists and then its `id`.
        const long exists = world.byName["instance_exists"] - existsBefore, gets = world.byName["variable_instance_get"] - getsBefore;
        check("drop/reads_only_inside_init", world.calls - before == 3 && exists == 2 && gets == 1,
            "calls=" + n(world.calls - before) + " instance_exists=" + n(exists) + " variable_instance_get=" + n(gets));
        // The kinds are the call's own, as passed, not what they became.
        const HiddenLootMod::Stats& kinds = mod().StatsRef();
        check("drop/kinds_as_passed", noKindsYet && std::string(kinds.kinds[0]) == "ref" && std::string(kinds.kinds[1]) == "obj"
            && std::string(kinds.kinds[2]) == "obj",
            std::string("kinds=") + kinds.kinds[0] + "/" + kinds.kinds[1] + "/" + kinds.kinds[2]);
        tick(1, 1);
        check("drop/one_deactivate_at_next_tick", !anyId(item)->active && world.byName["instance_deactivate_object"] - deactBefore == 1
            && mod().StatsRef().inits == 1 && mod().StatsRef().slept == 6 && mod().StatsRef().byArg0 - st0.byArg0 == 1,
            "deactivates=" + n(world.byName["instance_deactivate_object"] - deactBefore) + " slept=" + n(mod().StatsRef().slept)
            + " by-arg0=" + n(mod().StatsRef().byArg0 - st0.byArg0));
        tick(5, 1);
        check("drop/never_twice", world.byName["instance_deactivate_object"] - deactBefore == 1);
        // The item found as a plain number, and as `self` (the arguments carry
        // nothing that is a ground item): the kind never decides.
        const HiddenLootMod::Stats st1 = mod().StatsRef();
        const int64_t asNumber = addItem(true);
        init(Plain(asNumber), RValue(), InstancePointer(monster));
        const int64_t asSelf = addItem(true);
        init(RValue(12.0), ItemStruct(), InstancePointer(asSelf));
        tick(1, 1);
        check("drop/identified_from_number", !anyId(asNumber)->active);
        check("drop/identified_from_self", !anyId(asSelf)->active && mod().StatsRef().unidentified == 0
            && mod().StatsRef().bySelf - st1.bySelf == 1 && mod().StatsRef().byArg0 - st1.byArg0 == 1,
            "unidentified=" + n(mod().StatsRef().unidentified) + " by-self=" + n(mod().StatsRef().bySelf - st1.bySelf));
        // Argument 0 as an instance pointer: made its `id` inside the call,
        // and that id identifies it at the tick. The item struct is dropped.
        const HiddenLootMod::Stats st2 = mod().StatsRef();
        const int64_t asPointer = addItem(true);
        init(InstancePointer(asPointer), ItemStruct(), RValue());
        const bool awakeInsideCall = byId(asPointer)->active;
        tick(1, 1);
        check("drop/reduced_inside_call", awakeInsideCall && !anyId(asPointer)->active
            && mod().StatsRef().reduced - st2.reduced == 1 && mod().StatsRef().dropped - st2.dropped == 1
            && mod().StatsRef().byArg0 - st2.byArg0 == 1,
            "reduced=" + n(mod().StatsRef().reduced - st2.reduced) + " dropped=" + n(mod().StatsRef().dropped - st2.dropped)
            + " by-arg0=" + n(mod().StatsRef().byArg0 - st2.byArg0));
        // `self` a monster that dies before the frame's end: its pointer was
        // made an id inside the call, so the tick asks the id, which answers
        // not there, and never hands the runner the dead pointer.
        const int64_t dying = addMonster();
        const HiddenLootMod::Stats st3 = mod().StatsRef();
        const long missing = world.callsOnMissing, missingReads = world.readsOnMissing;
        const size_t touches3 = world.touched.size();
        init(RValue(12.0), ItemStruct(), InstancePointer(dying));
        byId(dying)->exists = false;
        tick(1, 1);
        check("drop/self_gone_before_tick_no_call", mod().StatsRef().unidentified - st3.unidentified == 1
            && world.danglingPointerCalls == 0 && world.callsOnMissing == missing && world.readsOnMissing == missingReads
            && world.touched.size() == touches3,
            "unidentified=" + n(mod().StatsRef().unidentified - st3.unidentified) + " dangling=" + n(world.danglingPointerCalls)
            + " missing=" + n(world.callsOnMissing - missing) + " reads=" + n(world.readsOnMissing - missingReads));
        // The same item handed over twice in one frame is slept once.
        const int64_t twice = addItem(true);
        const long d2 = world.byName["instance_deactivate_object"];
        init(Ref(twice), ItemStruct(), InstancePointer(monster));
        init(Ref(twice), ItemStruct(), InstancePointer(monster));
        tick(1, 1);
        check("drop/same_item_once", world.byName["instance_deactivate_object"] - d2 == 1);
    }

    // ---- A visible drop is never touched ----------------------------------------
    {
        const int64_t item = addItem(false);
        const uint64_t visibleBefore = mod().StatsRef().visible;
        init(Ref(item), ItemStruct(), InstancePointer(monster));
        tick(30, 1);
        check("visible/never_touched", byId(item)->active && !touchedBy(item) && mod().StatsRef().visible == visibleBefore + 1,
            "visible=" + n(mod().StatsRef().visible - visibleBefore));
    }

    // ---- What cannot be read or identified is counted, not acted on -----------
    {
        const int64_t noVar = addItem(true, false);
        init(Ref(noVar), ItemStruct(), InstancePointer(monster));
        tick(1, 1);
        check("nofilter/counted_not_acted", byId(noVar)->active && !touchedBy(noVar) && mod().StatsRef().noFilterVar >= 1,
            "no-filter-var=" + n(mod().StatsRef().noFilterVar));
        // No ground item in the call (about 9% of drop calls make none), and a
        // coin: neither is a Loot_Ground_obj.
        const int64_t coin = addCoin();
        const uint64_t before = mod().StatsRef().unidentified;
        const long touches = (long)world.touched.size();
        init(RValue(), ItemStruct(), InstancePointer(monster));
        init(Ref(coin), ItemStruct(), InstancePointer(monster));
        const int64_t destroyed = addItem(true);
        byId(destroyed)->exists = false;
        init(Ref(destroyed), ItemStruct(), InstancePointer(monster));
        tick(1, 1);
        check("unidentified/counted_not_acted", mod().StatsRef().unidentified == before + 3 && (long)world.touched.size() == touches
            && world.callsOnMissing == 0 && world.readsOnMissing == 0,
            "unidentified=" + n(mod().StatsRef().unidentified - before) + " missing=" + n(world.callsOnMissing));
    }

    // ---- The key: which codes are accepted -------------------------------------
    {
        const int keep = mod().Key();
        check("key/refuses_mouse_buttons", !mod().SetKey(1) && !mod().SetKey(2) && mod().Key() == keep);
        check("key/range", !mod().SetKey(-1) && !mod().SetKey(255) && !mod().SetKey(1000) && mod().SetKey(3) && mod().SetKey(254)
            && mod().Key() == 254);
        // 0 means no key: nothing is polled at all.
        check("key/zero_accepted", mod().SetKey(0) && mod().Key() == 0);
        g_KeyHeld = true;
        const long reads = g_KeyReads, fronts = g_FrontReads;
        tick(30, 1);
        check("key/zero_polls_nothing", g_KeyReads == reads && g_FrontReads == fronts && !mod().Held() && mod().ShownNow() == 0);
        g_KeyHeld = false;
        mod().SetKey(164);
    }

    // ---- The key is ignored while the game is not in front --------------------
    {
        const size_t asleep = mod().AsleepNow();
        g_InFront = false; g_KeyHeld = true;
        const long touches = (long)world.touched.size();
        const long keyReads = g_KeyReads;
        tick(30, 1);
        check("front/key_ignored", !mod().Held() && mod().ShownNow() == 0 && mod().AsleepNow() == asleep
            && (long)world.touched.size() == touches && g_KeyReads == keyReads,
            "shown-now=" + n(mod().ShownNow()) + " keyReads=" + n(g_KeyReads - keyReads));
        g_InFront = true; g_KeyHeld = false;
        tick(1, 1);
    }

    // ---- Holding the key shows, release hides and sleeps again ----------------
    int64_t heldDrop = -1;
    {
        const size_t asleep = mod().AsleepNow();
        check("hold/some_asleep", asleep >= 9, "asleep-now=" + n(asleep));
        g_KeyHeld = true;
        const long activates = world.byName["instance_activate_object"];
        tick(1, 1);
        bool allShown = true;
        for (const auto& i : world.instances)
            if (i.exists && i.object == LootGround && touchedBy(i.id, "instance_deactivate_object"))
                allShown = allShown && i.active && i.verdict && i.visible;
        check("hold/shows", mod().Held() && mod().ShownNow() == asleep && mod().AsleepNow() == 0 && allShown
            && world.byName["instance_activate_object"] - activates == (long)asleep,
            "shown-now=" + n(mod().ShownNow()) + " activates=" + n(world.byName["instance_activate_object"] - activates));
        // Held: nothing more happens while the key stays down.
        const long calls = world.calls;
        tick(20, 1);
        check("hold/steady_while_held", world.calls == calls, "calls=" + n(world.calls - calls));

        // A hidden drop while the key is held is shown instead of slept.
        heldDrop = addItem(true);
        const uint64_t sleptBefore = mod().StatsRef().slept;
        init(Ref(heldDrop), ItemStruct(), InstancePointer(monster));
        tick(1, 1);
        check("held_drop/shown", byId(heldDrop)->active && byId(heldDrop)->verdict && byId(heldDrop)->visible
            && mod().ShownNow() == asleep + 1 && mod().StatsRef().slept == sleptBefore,
            "shown-now=" + n(mod().ShownNow()));

        // One shown item is picked up while the key is held.
        int64_t picked = -1;
        for (auto& i : world.instances)
            if (i.exists && i.object == LootGround && i.active && i.id != heldDrop && touchedBy(i.id, "instance_deactivate_object")) { picked = i.id; break; }
        byId(picked)->exists = false;
        const long missing = world.callsOnMissing, missingReads = world.readsOnMissing;
        const uint64_t goneBefore = mod().StatsRef().gone;
        const size_t touchesBefore = world.touched.size();
        g_KeyHeld = false;
        tick(1, 1);
        bool allHidden = true;
        for (const auto& i : world.instances)
            if (i.exists && i.object == LootGround && touchedBy(i.id, "instance_deactivate_object"))
                allHidden = allHidden && !i.active && !i.verdict && !i.visible;
        bool pickedTouched = false;
        for (size_t k = touchesBefore; k < world.touched.size(); ++k) if (world.touched[k].first == picked) pickedTouched = true;
        check("release/hides_and_sleeps", !mod().Held() && mod().ShownNow() == 0 && mod().AsleepNow() == asleep && allHidden,
            "asleep-now=" + n(mod().AsleepNow()) + " shown-now=" + n(mod().ShownNow()));
        check("release/gone_counted_without_call", mod().StatsRef().gone == goneBefore + 1 && !pickedTouched
            && world.callsOnMissing == missing && world.readsOnMissing == missingReads,
            "gone=" + n(mod().StatsRef().gone - goneBefore) + " missing=" + n(world.callsOnMissing - missing));
        check("held_drop/hidden_on_release", !anyId(heldDrop)->active && !anyId(heldDrop)->verdict
            && mod().StatsRef().slept == sleptBefore + 1, "slept=" + n(mod().StatsRef().slept - sleptBefore));

        // A game that refuses the built-in `visible`: the verdict still moves
        // (the game's own 0.3 s refresh follows it), and it is counted.
        world.refuseVisibleWrite = true;
        g_KeyHeld = true;
        tick(1, 1);
        check("hold/visible_refusal_tolerated", mod().ShownNow() == asleep && mod().StatsRef().visibleUnwritten > 0
            && byId(heldDrop)->verdict && byId(heldDrop)->active,
            "shown-now=" + n(mod().ShownNow()) + " unwritten=" + n(mod().StatsRef().visibleUnwritten));
        g_KeyHeld = false;
        tick(1, 1);
        world.refuseVisibleWrite = false;
        check("hold/refusal_rehides", mod().ShownNow() == 0 && mod().AsleepNow() == asleep);
    }

    // ---- Switching off wakes everything, with the game's verdict in place ----
    {
        // Off with everything asleep: every one is woken, and exists again.
        const size_t asleep = mod().AsleepNow();
        const auto off = mod().Disable();
        check("off/wakes_everything", asleep > 0 && countAsleep() == 0 && off.woken == (long)asleep && off.existAfter == off.woken,
            "woken=" + n(off.woken) + " exist-after=" + n(off.existAfter) + " asleep-before=" + n(asleep) + " inactive=" + n(countAsleep()));
        // On again (the walk sleeps them again), the key held, one more drop
        // shown, then off while all of them are shown: every one ends awake
        // with the game's hidden verdict, as vanilla leaves a hidden item.
        const auto walk = mod().Enable(1, zone);
        g_KeyHeld = true;
        tick(1, 1);
        const int64_t shownAtOff = addItem(true);
        init(Ref(shownAtOff), ItemStruct(), InstancePointer(monster));
        tick(1, 1);
        const size_t shown = mod().ShownNow();
        const auto offHeld = mod().Disable();
        bool allAwakeHidden = true;
        for (const auto& i : world.instances)
            if (i.exists && i.object == LootGround && touchedBy(i.id)) allAwakeHidden = allAwakeHidden && i.active && !i.verdict && !i.visible;
        check("off/verdict_hidden", walk.slept == (long)asleep && shown == asleep + 1 && offHeld.woken == 0 && allAwakeHidden
            && countAsleep() == 0 && byId(shownAtOff)->active && !byId(shownAtOff)->verdict,
            "walk-slept=" + n(walk.slept) + " shown=" + n(shown) + " woken=" + n(offHeld.woken));
        const long calls = world.calls;
        tick(60, 1);
        init(Ref(addItem(true)), ItemStruct(), InstancePointer(monster));
        tick(5, 1);
        check("off/forgotten", !mod().Enabled() && mod().AsleepNow() == 0 && mod().ShownNow() == 0 && !mod().Held()
            && world.calls == calls, "calls=" + n(world.calls - calls));
        g_KeyHeld = false;
    }

    // ---- A room change forgets without a runner call ---------------------------
    {
        for (auto& i : world.instances) if (i.object == LootGround) i.exists = false;
        for (int k = 0; k < 20; ++k) addItem(true);
        const auto walk = mod().Enable(2, zone);
        check("room/walk_in_new_room", walk.slept == 20 && mod().AsleepNow() == 20, "walk-slept=" + n(walk.slept));
        // The zone ends: the room's end destroys every one of them, asleep or not.
        for (auto& i : world.instances) if (i.object == LootGround) i.exists = false;
        const long calls = world.calls;
        tick(1, 3);
        check("room/forgets_without_calls", world.calls == calls && mod().AsleepNow() == 0 && mod().ShownNow() == 0,
            "calls=" + n(world.calls - calls) + " asleep-now=" + n(mod().AsleepNow()));
        // Holding and releasing afterwards addresses none of the old ids.
        g_KeyHeld = true; tick(1, 3); g_KeyHeld = false; tick(1, 3);
        check("room/old_ids_never_addressed", world.callsOnMissing == 0 && world.readsOnMissing == 0,
            "missing=" + n(world.callsOnMissing) + " reads=" + n(world.readsOnMissing));

        // A persistent room: the mod sleeps nothing there, by drop or by walk.
        const int64_t item = addItem(true);
        const uint64_t skipped = mod().StatsRef().skippedPersistent;
        tick(1, 4, persistentRoom);
        init(Ref(item), ItemStruct(), InstancePointer(monster));
        tick(1, 4, persistentRoom);
        const int64_t onGround = addItem(true);
        const auto again = mod().Enable(4, persistentRoom);
        check("room/persistent_sleeps_nothing", byId(item)->active && byId(onGround)->active && again.slept == 0
            && mod().StatsRef().skippedPersistent >= skipped + 2 && countAsleep() == 0,
            "skipped-persistent=" + n(mod().StatsRef().skippedPersistent - skipped) + " inactive=" + n(countAsleep()));
        mod().Disable();
    }

    // ---- The table-only fallback: a pass every 18 frames ------------------------
    {
        for (auto& i : world.instances) if (i.object == LootGround) i.exists = false;
        mod().SetRoute(HiddenLootMod::Route::TableOnly);
        mod().Enable(5, zone);
        const uint64_t passes = mod().StatsRef().passes;
        // Drops the hook never saw (compiled calls bypass a table-only hook).
        std::vector<int64_t> unseen;
        for (int k = 0; k < 7; ++k) unseen.push_back(addItem(true));
        const int64_t shown = addItem(false);
        tick(180, 5);
        bool asleep = true;
        for (int64_t id : unseen) asleep = asleep && !anyId(id)->active;
        const uint64_t ran = mod().StatsRef().passes - passes;
        check("pass/every_18_frames", ran >= 9 && ran <= 11 && asleep && byId(shown)->active && !touchedBy(shown),
            "passes=" + n(ran));
        // A visible item is judged once, not on every pass.
        check("pass/visible_counted_once", mod().StatsRef().visible >= 1);
        const long reads = world.byName["variable_instance_get"];
        tick(36, 5);
        check("pass/judged_items_not_reread", world.byName["variable_instance_get"] == reads,
            "reads=" + n(world.byName["variable_instance_get"] - reads));
        mod().Disable();

        // A failed install (route none) takes the same fallback.
        mod().SetRoute(HiddenLootMod::Route::None);
        mod().Enable(6, zone);
        const int64_t late = addItem(true);
        const uint64_t p2 = mod().StatsRef().passes;
        tick(40, 6);
        check("pass/route_none_uses_it", !anyId(late)->active && mod().StatsRef().passes > p2);
        mod().Disable();
        mod().SetRoute(HiddenLootMod::Route::Both);
    }

    // ---- The switch-on walk is capped like lootcensus ---------------------------
    {
        for (auto& i : world.instances) if (i.object == LootGround) i.exists = false;
        for (int k = 0; k < 9000; ++k) addItem(true);
        const long finds = world.byName["instance_find"];
        const auto walk = mod().Enable(7, zone);
        check("on/walk_capped", walk.slept == HiddenLootMod::kWalkCap && world.byName["instance_find"] - finds <= HiddenLootMod::kWalkCap
            && HiddenLootMod::kWalkCap == 8192,
            "walk-slept=" + n(walk.slept) + " finds=" + n(world.byName["instance_find"] - finds));
        mod().Disable();
        check("on/walk_capped_off_wakes", countAsleep() == 0);
    }

    // ---- A runner that refuses deactivation: counted, nothing left asleep -----
    {
        for (auto& i : world.instances) if (i.object == LootGround) i.exists = false;
        world.throwOnDeactivate = true;
        const int64_t item = addItem(true);
        const uint64_t errors = mod().StatsRef().errors;
        mod().Enable(8, zone);
        init(Ref(addItem(true)), ItemStruct(), InstancePointer(monster));
        tick(2, 8);
        check("errors/counted", mod().StatsRef().errors >= errors + 2 && byId(item)->active && mod().AsleepNow() == 0,
            "errors=" + n(mod().StatsRef().errors - errors));
        world.throwOnDeactivate = false;
        mod().Disable();
    }

    // Every builtin the class called is one this world (and the contract) lists.
    std::string odd;
    for (const auto& u : world.unexpected) odd += u + " ";
    check("calls/only_listed_builtins", world.unexpected.empty(), odd);
    // No builtin the class called, all run, was handed a dead instance pointer.
    check("calls/no_dangling_pointer", world.danglingPointerCalls == 0, "dangling=" + n(world.danglingPointerCalls));

    std::cout << (failures ? "RESULT FAIL" : "RESULT OK") << "\n";
    return failures ? 1 : 0;
}
